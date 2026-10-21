#include "Compatibility/UE58/CubelessDataAssetApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "AssetToolsModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/PreviewMeshCollection.h"
#include "Animation/Skeleton.h"
#include "Animation/MeshDeformerCollection.h"
#include "Animation/MeshDeformer.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/DataAsset.h"
#include "StructUtils/UserDefinedStruct.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Factories/DataAssetFactory.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "InputCoreTypes.h"
#include "JsonObjectConverter.h"
#include "JsonObjectStructInterface.h"
#include "UObject/MetaData.h"
#include "Internationalization/Text.h"
#include "Editor/TransBuffer.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::DataAsset
{
namespace
{
constexpr const TCHAR* ClassPath = TEXT("/Script/Engine.MeshDeformerCollection");
constexpr const TCHAR* PreviewClassPath = TEXT("/Script/Engine.PreviewMeshCollection");
constexpr const TCHAR* InputActionClassPath = TEXT("/Script/EnhancedInput.InputAction");
bool InputActionActive = false;
constexpr const TCHAR* MappingContextClassPath = TEXT("/Script/EnhancedInput.InputMappingContext");
constexpr const TCHAR* PlayerSettingsClassPath = TEXT("/Script/EnhancedInput.PlayerMappableKeySettings");
bool MappingContextActive = false;
struct FResult
{
    FString Op, Stage = TEXT("preflight"), Code, Error;
    bool Success = false, Attempted = false, Known = true, Changed = false, Post = false, SaveRequested = false, SaveAttempted = false, Saved = false;
    TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
};
void Count(FResult& R, const TCHAR* K) { R.Data->SetNumberField(K, R.Data->GetNumberField(K) + 1); }
bool Fail(FResult& R, const TCHAR* K, const FString& E, bool Unknown = false)
{ R.Success = R.Post = false; R.Code = K; R.Error = E; if (Unknown) { R.Known = false; } return false; }
FString Text(const TSharedRef<FJsonObject>& J) { FString S; FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&S)); return S; }
FString Encode(FResult& R)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("operation"), R.Op); J->SetBoolField(TEXT("success"), R.Success); J->SetStringField(TEXT("stage"), R.Stage); J->SetStringField(TEXT("error_code"), R.Code); J->SetStringField(TEXT("error"), R.Error);
    J->SetBoolField(TEXT("mutation_attempted"), R.Attempted); J->SetBoolField(TEXT("change_state_known"), R.Known); J->SetBoolField(TEXT("outcome_unknown"), !R.Known);
    if (R.Known) { J->SetBoolField(TEXT("changed"), R.Changed); } else { J->SetField(TEXT("changed"), MakeShared<FJsonValueNull>()); }
    J->SetBoolField(TEXT("postcondition_passed"), R.Post); J->SetBoolField(TEXT("compile_requested"), false); J->SetBoolField(TEXT("compile_attempted"), false); J->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    J->SetBoolField(TEXT("save_requested"), R.SaveRequested); J->SetBoolField(TEXT("save_attempted"), R.SaveAttempted); J->SetBoolField(TEXT("saved"), R.Saved); J->SetObjectField(TEXT("data"), R.Data); return Text(J);
}
void Init(FResult& R, const TCHAR* Op, bool Save = false)
{
    R.Op = Op; R.SaveRequested = Save;
    for (const TCHAR* K : { TEXT("asset_load_count"), TEXT("class_load_count"), TEXT("legacy_get_calls"), TEXT("observation_capture_calls"), TEXT("json_export_calls"), TEXT("json_conversion_calls"), TEXT("property_storage_copy_calls"), TEXT("post_edit_change_property_calls"), TEXT("outer_transaction_calls"), TEXT("explicit_modify_calls"), TEXT("mark_package_dirty_calls"), TEXT("factory_allocation_attempts"), TEXT("asset_tools_create_attempts"), TEXT("asset_tools_get_calls"), TEXT("asset_exists_query_calls"), TEXT("save_attempt_count"), TEXT("explicit_compile_calls"), TEXT("explicit_CDO_create_calls") }) { R.Data->SetNumberField(K, 0); }
    for (const TCHAR* K : { TEXT("observations_complete"), TEXT("source_preserved"), TEXT("references_preserved"), TEXT("unselected_state_preserved"), TEXT("other_package_dirty_preserved"), TEXT("persistence_verified"), TEXT("runtime_verified"), TEXT("full_original_data_asset_parity"), TEXT("implicit_engine_effects_measured") }) { R.Data->SetBoolField(K, false); }
    for (const TCHAR* K : { TEXT("asset_load_calls"), TEXT("class_load_calls"), TEXT("save_calls"), TEXT("property_attempts"), TEXT("property_notifications"), TEXT("applied"), TEXT("failed") }) { R.Data->SetArrayField(K, {}); }
    R.Data->SetBoolField(TEXT("save_requested"), Save); R.Data->SetBoolField(TEXT("saved"), false); R.Data->SetStringField(TEXT("supported_class"), ClassPath); R.Data->SetStringField(TEXT("asset_type"), TEXT("data_asset"));
    R.Data->SetStringField(TEXT("admission_scope"), TEXT("Native MeshDeformerCollection lifecycle and Description writes; exact native PreviewMeshCollection get/create only. Preview entry setter, arbitrary native/generated classes and other array/object/config/noneditable writes remain incomplete."));
    R.Data->SetStringField(TEXT("effect_scope"), TEXT("Original load routes, transaction/Modify, conversion, notifications and forced save. Inherited loader/serialization/delegate/cache/default work is not measured zero. Read preservation starts after the first original load. Global package dirty maps are retained separately from selected runtime-reference admission."));
    R.Data->SetStringField(TEXT("description_cook_scope"), TEXT("Description is Editor-only and stripped in cooked content; native class/runtime arrays are preserved, not a nonempty deformer runtime-use proof."));
    R.Data->SetStringField(TEXT("change_accounting_scope"), TEXT("Original terminal per-property updated count/fresh asset creation, independent from real Modify/callback/dirty/save work and full raw state drift. Unknown incomplete phases remain changed=null."));
    R.Data->SetBoolField(TEXT("atomic_rollback_available"), false); R.Data->SetBoolField(TEXT("auto_retry_safe"), false);
}
bool Loaded(const UObject* O) { return IsValid(O) && !O->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_BeginDestroyed | RF_FinishDestroyed); }
bool HasNul(const FString& S) { for (int32 I = 0; I < S.Len(); ++I) { if (S[I] == TCHAR('\0')) { return true; } } return false; }
bool Clean(const FString& S)
{ for (const TCHAR* K : { TEXT("/Script/CubelessMCP"), TEXT("/Script/UnrealMCP"), TEXT("/Script/ModelContextProtocol"), TEXT("/Script/ToolsetRegistry"), TEXT("/Engine/PythonTypes"), TEXT("/CubelessMCP_UE58/"), TEXT("/UnrealMCP/") }) { if (S.Contains(K, ESearchCase::IgnoreCase)) { return false; } } return true; }
TSharedRef<FJsonObject> Identity(const UObject* O)
{
    auto J = MakeShared<FJsonObject>(); J->SetBoolField(TEXT("valid"), Loaded(O)); J->SetStringField(TEXT("path"), Loaded(O) ? O->GetPathName() : FString()); J->SetStringField(TEXT("class"), Loaded(O) ? O->GetClass()->GetPathName() : FString());
    J->SetStringField(TEXT("outer"), Loaded(O) && O->GetOuter() ? O->GetOuter()->GetPathName() : FString()); J->SetStringField(TEXT("object_flags"), Loaded(O) ? LexToString(static_cast<uint64>(O->GetFlags())) : FString()); return J;
}
bool Equal(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B, int32 Depth = 0)
{
    if (!A || !B || A->Type != B->Type || Depth > 32) { return false; }
    if (A->Type == EJson::Object) { const auto X = A->AsObject(), Y = B->AsObject(); if (!X.IsValid() || !Y.IsValid() || X->Values.Num() != Y->Values.Num()) { return false; } for (const auto& P : X->Values) { if (!Equal(P.Value, Y->TryGetField(P.Key.ToView()), Depth + 1)) { return false; } } return true; }
    if (A->Type == EJson::Array) { const auto& X = A->AsArray(); const auto& Y = B->AsArray(); if (X.Num() != Y.Num()) { return false; } for (int32 I = 0; I < X.Num(); ++I) { if (!Equal(X[I], Y[I], Depth + 1)) { return false; } } return true; }
    return A->Type == EJson::String ? A->AsString().Equals(B->AsString(), ESearchCase::CaseSensitive) : FJsonValue::CompareEqual(*A, *B);
}
bool Equal(const TSharedRef<FJsonObject>& A, const TSharedRef<FJsonObject>& B) { return Equal(MakeShared<FJsonValueObject>(A), MakeShared<FJsonValueObject>(B)); }
bool Bounded(const TSharedPtr<FJsonValue>& V, int32& N, int32& Chars, int32 Depth = 0)
{
    if (!V || ++N > 16384 || Depth > 32) { return false; }
    if (V->Type == EJson::Number && !FMath::IsFinite(V->AsNumber())) { return false; }
    if (V->Type == EJson::String) { const int32 S = V->AsString().Len(); if (S > 1048576 || Chars > 1048576 - S) { return false; } Chars += S; }
    if (V->Type == EJson::Object) { if (!V->AsObject().IsValid()) { return false; } for (const auto& P : V->AsObject()->Values) { if (!Bounded(P.Value, N, Chars, Depth + 1)) { return false; } } }
    if (V->Type == EJson::Array) { if (V->AsArray().Num() > 4096) { return false; } for (const auto& X : V->AsArray()) { if (!Bounded(X, N, Chars, Depth + 1)) { return false; } } } return V->Type != EJson::None;
}
struct FPreviewCollectionScope;
struct FDeformerCollectionScope;
struct FInputActionScope;
struct FInputOwnedReadScope;
bool OwnedReadAdmit(FInputOwnedReadScope* Scope);
struct FMappingContextScope;
bool MappingAdmit(FMappingContextScope* Scope);
void MappingExportDiagnostic(FMappingContextScope* Scope,UObject* Object,FProperty* Property,bool Before,bool Performed=false,int32 Length=0,bool CurrentOK=true,bool LimitOK=true);
bool InputAdmit(FInputActionScope* Scope);
bool PreviewAdmit(FPreviewCollectionScope* Scope);
bool DeformerAdmit(FDeformerCollectionScope* Scope);
bool Fields(UObject* O, TSharedRef<FJsonObject>& J, FPreviewCollectionScope* Scope=nullptr,FDeformerCollectionScope* Deformer=nullptr,FInputActionScope* Input=nullptr,FMappingContextScope* Mapping=nullptr,FInputOwnedReadScope* OwnedRead=nullptr)
{
    int32 N = 0, Chars = 0;if(!OwnedReadAdmit(OwnedRead)||!PreviewAdmit(Scope)||!DeformerAdmit(Deformer)||!InputAdmit(Input)||!MappingAdmit(Mapping)||!Loaded(O)){return false;}
    for (TFieldIterator<FProperty> It(O->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
    {
        FProperty* P = *It; if (++N > 256 || P->ArrayDim != 1) { return false; } auto Row = MakeShared<FJsonObject>(); J->SetObjectField(P->GetName(), Row);
        Row->SetStringField(TEXT("owner"), P->GetOwnerStruct()->GetPathName()); Row->SetStringField(TEXT("property_class"), P->GetClass()->GetName()); Row->SetStringField(TEXT("flags"), LexToString(static_cast<uint64>(P->GetPropertyFlags())));
        MappingExportDiagnostic(Mapping,O,P,true);FString S; const void* A = P->ContainerPtrToValuePtr<void>(O); const bool Exported=P->ExportText_Direct(S,A,A,O,PPF_None);const bool CurrentOK=OwnedReadAdmit(OwnedRead)&&PreviewAdmit(Scope)&&DeformerAdmit(Deformer)&&InputAdmit(Input)&&MappingAdmit(Mapping);const bool LimitOK=S.Len()<=1048576&&Chars<=1048576-S.Len();MappingExportDiagnostic(Mapping,O,P,false,Exported,S.Len(),CurrentOK,LimitOK);if(!CurrentOK||!Exported||!LimitOK){return false;} Chars += S.Len(); Row->SetStringField(TEXT("export_text"), S);
    } return true;
}
bool Sources(UClass* Class, UClass* Factory, TSharedRef<FJsonObject>& J, bool RequireDefaults = false, FPreviewCollectionScope* Scope=nullptr,FDeformerCollectionScope* Deformer=nullptr,FInputActionScope* Input=nullptr,FMappingContextScope* Mapping=nullptr)
{
    for (UClass* T : { Class, Factory })
    {
        if (!T) { continue; } if (!Loaded(T) || !T->HasAnyClassFlags(CLASS_Native) || T->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists) || !Clean(T->GetPathName())) { return false; }
        auto Row = Identity(T); J->SetObjectField(T->GetPathName(), Row); Row->SetStringField(TEXT("class_flags"), LexToString(static_cast<uint64>(T->GetClassFlags()))); Row->SetBoolField(TEXT("package_dirty"), T->GetOutermost()->IsDirty());
        UObject* CDO = T->GetDefaultObject(false); Row->SetBoolField(TEXT("CDO_exists"), CDO != nullptr); if (!CDO) { if (RequireDefaults) { return false; } continue; } if (!Loaded(CDO)) { return false; }
        Row->SetObjectField(TEXT("CDO_identity"), Identity(CDO)); auto P = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("CDO_fields"), P); if (!Fields(CDO, P, Scope,Deformer,Input,Mapping)) { return false; }
    } return true;
}
bool Disk(const FString& Package, TSharedRef<FJsonObject>& J)
{
    FString F; if (!FPackageName::TryConvertLongPackageNameToFilename(Package, F, FPackageName::GetAssetPackageExtension())) { return false; } F = FPaths::ConvertRelativePathToFull(F); const int64 N = IFileManager::Get().FileSize(*F);
    J->SetStringField(TEXT("path"), F); J->SetBoolField(TEXT("exists"), N >= 0); J->SetNumberField(TEXT("bytes"), FMath::Max<int64>(N, 0)); J->SetStringField(TEXT("sha1"), FString()); if (N < 0) { return true; }
    TArray<uint8> B; if (N > 64 * 1024 * 1024 || !FFileHelper::LoadFileToArray(B, *F) || B.Num() != N) { return false; } uint8 H[FSHA1::DigestSize]; FSHA1::HashBuffer(B.GetData(), B.Num(), H); J->SetStringField(TEXT("sha1"), BytesToHex(H, FSHA1::DigestSize)); return true;
}
bool Dirty(const FString& Package, TSharedRef<FJsonObject>& J)
{ int32 N = 0; for (TObjectIterator<UPackage> It; It; ++It) { if (!Loaded(*It) || It->GetName() == Package) { continue; } if (++N > 8192) { return false; } J->SetBoolField(It->GetName(), It->IsDirty()); } return true; }
// IX: concrete exact-native get/create protection. GetPreviewSkeletalMeshes is not called;
// that separate SDK method synchronously loads the soft meshes and AnimBlueprints.
struct FPreviewCollectionScope
{
    FResult& R;bool Stopped=false,Bound=false;TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;
    struct FIdentity{TStrongObjectPtr<UObject> Object;UClass* Class=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;FString Path;};
    struct FSource{TStrongObjectPtr<UClass> Class;UObject* Default=nullptr;EClassFlags Flags=CLASS_None;};
    TArray<FIdentity> Identities;TArray<FSource> SourceRoles;TStrongObjectPtr<UPreviewMeshCollection> Target;TStrongObjectPtr<USkeleton> Skeleton;
    TArray<UObject*> Owned;TArray<FString> SoftPaths;FString TargetPath;
    explicit FPreviewCollectionScope(FResult& In):R(In){}
    bool Stop(const TCHAR* Code,const TCHAR* Error)
    {if(!Stopped){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("code"),Code);J->SetStringField(TEXT("stage"),R.Stage);R.Data->SetObjectField(TEXT("preview_first_failure"),J);Fail(R,Code,Error,true);}Stopped=true;return false;}
    bool Context()
    {
        if(Stopped){return false;}
        if(!IsEditorGameThread()||!GEditor||GEditor!=Editor.Get()||!Loaded(Editor.Get())||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||IsPlaySessionActive()
            ||!Loaded(World.Get())||World->WorldType!=EWorldType::Editor||World->bIsTearingDown)
        {return Stop(TEXT("data_asset_preview_context_changed"),TEXT("Captured idle Editor/world must remain exact; no next SDK/read/save or retry."));}
        if(GEditor->GetEditorWorldContext().World()!=World.Get()){return Stop(TEXT("data_asset_preview_world_changed"),TEXT("Current Editor world changed during Preview collection get/create."));}return true;
    }
    bool Start()
    {
        if(!IsEditorGameThread()||!Loaded(GEditor)||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||IsPlaySessionActive())
        {return Stop(TEXT("data_asset_preview_context_required"),TEXT("Idle Editor context required before original loader/factory work."));}
        Editor=GEditor;World=GEditor->GetEditorWorldContext().World();return Context();
    }
    bool Pin(UObject* O)
    {
        if(!O){return true;}if(!Context()||!Loaded(O)||!Loaded(O->GetClass())||(O->GetOuter()&&!Loaded(O->GetOuter()))||!Loaded(O->GetPackage())||!Clean(O->GetPathName())||!Clean(O->GetClass()->GetPathName()))
        {return Stop(TEXT("data_asset_preview_identity_invalid"),TEXT("Exact live target/source/owned identity required; no new loader for references."));}
        for(const auto& I:Identities){if(I.Object.Get()==O){return true;}}if(Identities.Num()>=512){return Stop(TEXT("data_asset_preview_identity_budget"),TEXT("Bounded Preview identity set exceeded."));}
        FIdentity I;I.Object.Reset(O);I.Class=O->GetClass();I.Outer=O->GetOuter();I.Package=O->GetPackage();I.Path=O->GetPathName();Identities.Add(MoveTemp(I));return true;
    }
    bool BindSource(UClass* Class,bool Factory=false)
    {
        if(!Context()||!Loaded(Class)||Class!=(Factory?UDataAssetFactory::StaticClass():UPreviewMeshCollection::StaticClass())||!Class->HasAnyClassFlags(CLASS_Native)
            ||Class->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)||Class->ClassGeneratedBy||Class->GetOuter()!=Class->GetPackage()
            ||!Class->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||Class->GetPackage()->IsDirty()
            ||!Class->GetPathName().Equals(Factory?TEXT("/Script/UnrealEd.DataAssetFactory"):PreviewClassPath,ESearchCase::CaseSensitive))
        {return Stop(TEXT("data_asset_preview_source_class_invalid"),TEXT("Exact compiled-in Preview collection/factory class required; no generic class expansion."));}
        for(const auto& S:SourceRoles){if(S.Class.Get()==Class){return Admit();}}
        UObject* Default=Class->GetDefaultObject(false);if(Default&&(!Loaded(Default)||Default->GetClass()!=Class||Default->GetOuter()!=Class->GetPackage()||!Default->HasAnyFlags(RF_ClassDefaultObject)))
        {return Stop(TEXT("data_asset_preview_default_invalid"),TEXT("Only the existing exact native default may be observed; no CDO creation."));}
        if(!Pin(Class)||!Pin(Class->GetPackage())||!Pin(Default)||!Pin(Class->GetSuperClass())){return false;}FSource S;S.Class.Reset(Class);S.Default=Default;S.Flags=Class->GetClassFlags();SourceRoles.Add(MoveTemp(S));return Admit();
    }
    bool BindTarget(UDataAsset* Asset,const FString& Path)
    {
        if(!Admit()||!Loaded(Asset)||Asset->GetClass()!=UPreviewMeshCollection::StaticClass()||Asset->GetOuter()!=Asset->GetPackage()||!Asset->GetPathName().Equals(Path,ESearchCase::IgnoreCase)
            ||Asset->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_Transient)||Asset->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn|PKG_PlayInEditor|PKG_EditorOnly))
        {return Stop(TEXT("data_asset_preview_target_invalid"),TEXT("Exact current ordinary Preview collection required; returned allocation prefix is retained."));}
        if(Bound){return Asset==Target.Get()||Stop(TEXT("data_asset_preview_target_replaced"),TEXT("Final original relookup returned another target; no replay."));}
        Target.Reset(static_cast<UPreviewMeshCollection*>(Asset));TargetPath=Asset->GetPathName();Skeleton.Reset(Target->Skeleton.Get());
        if(Skeleton.IsValid()&&(!Loaded(Skeleton.Get())||Skeleton->GetClass()!=USkeleton::StaticClass())){return Stop(TEXT("data_asset_preview_skeleton_invalid"),TEXT("Optional native Skeleton must be already loaded; no source load or modification."));}
        if(!Pin(Asset)||!Pin(Asset->GetPackage())||!Pin(Skeleton.Get())){return false;}
        GetObjectsWithOuter(Asset,Owned,EGetObjectsFlags::IncludeNestedObjects);if(Owned.Num()>256||Target->SkeletalMeshes.Num()>1024){return Stop(TEXT("data_asset_preview_owned_budget"),TEXT("Bounded owned export/entry set required."));}
        for(UObject* O:Owned){if(!Loaded(O)||!O->IsIn(Asset)||!O->GetClass()->HasAnyClassFlags(CLASS_Native)||O->GetClass()->ClassGeneratedBy||!Pin(O)){return Stop(TEXT("data_asset_preview_owned_invalid"),TEXT("Current native owned exports must remain exact."));}}
        for(const auto& E:Target->SkeletalMeshes){const FString Mesh=E.SkeletalMesh.ToSoftObjectPath().ToString(),Anim=E.AnimBlueprint.ToSoftObjectPath().ToString();if(!Clean(Mesh)||!Clean(Anim)){return Stop(TEXT("data_asset_preview_soft_dependency_rejected"),TEXT("Existing soft paths cannot name editor tooling; no soft reference is loaded."));}SoftPaths.Add(Mesh);SoftPaths.Add(Anim);}
        Bound=true;R.Data->SetStringField(TEXT("supported_class"),PreviewClassPath);R.Data->SetStringField(TEXT("description_cook_scope"),TEXT("PreviewMeshCollection has no Description setter in this scope; native source class and existing payload paths are retained, not runtime/cook/preview execution proof."));R.Data->SetStringField(TEXT("preview_scope"),TEXT("Exact native get/create only; reflected target/source/default/reference/disk/dirty proof after initial load. No explicit soft mesh/AnimBlueprint loader or preview evaluation is called. Initial asset load/PostLoad, entry setter, preview runtime and global callback effects remain unverified."));return Admit();
    }
    // JD: only the selected direct converter may replace this plain routing expectation.
    // Its actual partial output is retained even when the stock converter returns false.
    bool AdoptConvertedEntries()
    {
        if(!Admit(false)){return false;}if(Target->SkeletalMeshes.Num()>1024){return Stop(TEXT("data_asset_preview_entry_budget"),TEXT("Direct conversion prefix exceeded the existing bounded entry observation; no save/retry."));}
        TArray<FString> Current;for(const auto& E:Target->SkeletalMeshes)
        {const FString Mesh=E.SkeletalMesh.ToSoftObjectPath().ToString(),Anim=E.AnimBlueprint.ToSoftObjectPath().ToString();if(!Clean(Mesh)||!Clean(Anim)){return Stop(TEXT("data_asset_preview_soft_dependency_rejected"),TEXT("Actual imported paths contain editor tooling; partial conversion is retained, no save/retry."));}Current.Add(Mesh);Current.Add(Anim);}
        SoftPaths=MoveTemp(Current);return Admit();
    }
    bool Admit(bool Entries = true)
    {
        if(!Context()){return false;}
        for(const auto& I:Identities){UObject* O=I.Object.Get();if(!Loaded(O)||!Loaded(I.Class)||(I.Outer&&!Loaded(I.Outer))||!Loaded(I.Package)||O->GetClass()!=I.Class||O->GetOuter()!=I.Outer||O->GetPackage()!=I.Package||!O->GetPathName().Equals(I.Path,ESearchCase::CaseSensitive))
            {return Stop(TEXT("data_asset_preview_identity_changed"),TEXT("Captured identity changed after callback; no stale object or next SDK."));}}
        for(const auto& S:SourceRoles){if(!Loaded(S.Class.Get())||S.Class->ClassGeneratedBy||!S.Class->HasAnyClassFlags(CLASS_Native)||S.Class->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)||S.Class->GetClassFlags()!=S.Flags||S.Class->GetDefaultObject(false)!=S.Default||(S.Default&&!S.Default->HasAnyFlags(RF_ClassDefaultObject)))
            {return Stop(TEXT("data_asset_preview_source_changed"),TEXT("Current native class/existing default role changed."));}}
        if(!Bound){return true;}if(Target->GetClass()!=UPreviewMeshCollection::StaticClass()||FindObject<UDataAsset>(nullptr,*TargetPath)!=Target.Get()||Target->Skeleton.Get()!=Skeleton.Get()||(Entries&&Target->SkeletalMeshes.Num()*2!=SoftPaths.Num()))
            {return Stop(TEXT("data_asset_preview_role_changed"),TEXT("Current target/Skeleton/entry routing changed; no next observation or save."));}
        int32 I=0;if(Entries){for(const auto& E:Target->SkeletalMeshes){if(!E.SkeletalMesh.ToSoftObjectPath().ToString().Equals(SoftPaths[I++],ESearchCase::CaseSensitive)||!E.AnimBlueprint.ToSoftObjectPath().ToString().Equals(SoftPaths[I++],ESearchCase::CaseSensitive))
            {return Stop(TEXT("data_asset_preview_entries_changed"),TEXT("Current ordered soft paths changed; not reloaded or rebound."));}}}
        TArray<UObject*> Current;GetObjectsWithOuter(Target.Get(),Current,EGetObjectsFlags::IncludeNestedObjects);if(Current.Num()!=Owned.Num()){return Stop(TEXT("data_asset_preview_owned_changed"),TEXT("Owned export set changed during Preview work."));}
        for(UObject* O:Owned){if(!Current.Contains(O)){return Stop(TEXT("data_asset_preview_owned_changed"),TEXT("Captured export left target ownership."));}}return true;
    }
};
bool PreviewAdmit(FPreviewCollectionScope* Scope){return !Scope||Scope->Admit();}
// JO: concrete runtime collection arrays only. The SDK consumer GetMeshDeformers
// is deliberately not called: authoring these paths is not deformer execution.
struct FDeformerCollectionScope
{
    FResult& R;bool Stopped=false,Bound=false;TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;
    struct FIdentity{TStrongObjectPtr<UObject> Hold;TStrongObjectPtr<UPackage> Package;UClass* Class=nullptr;UObject* Outer=nullptr;FString Path;EObjectFlags Flags=RF_NoFlags;};
    struct FSource{TStrongObjectPtr<UMeshDeformerCollection> Hold;FString Description;TArray<FString> Soft;TArray<UMeshDeformerCollection*> Hard;uint32 PackageFlags=0;};
    TArray<FIdentity> Identities;TArray<FSource> Collections;TStrongObjectPtr<UMeshDeformerCollection> Target;TStrongObjectPtr<UClass> Class;TStrongObjectPtr<UObject> Default;
    TMap<UPackage*,TArray<UObject*>> PackageExports;
    FArrayProperty* SoftProperty=nullptr;FArrayProperty* HardProperty=nullptr;FStrProperty* DescriptionProperty=nullptr;TArray<FString> Soft;TArray<UMeshDeformerCollection*> Hard;FString Description;TSharedPtr<FJsonObject> SourceBefore;
    EClassFlags ClassFlags=CLASS_None;FString TargetPath;int32 SourceSlots=0;
    explicit FDeformerCollectionScope(FResult& In):R(In){}
    bool Stop(const TCHAR* Code,const TCHAR* Why)
    {if(!Stopped){Fail(R,Code,Why,true);R.Data->SetBoolField(TEXT("automatic_retry_safe"),false);R.Data->SetStringField(TEXT("deformer_stop_stage"),R.Stage);}Stopped=true;return false;}
    bool Reject(const TCHAR* Code,const TCHAR* Why){return Fail(R,Code,Why);}
    bool Context()
    {
        if(Stopped){return false;}
        if(!IsEditorGameThread()||!Loaded(GEditor)||GEditor!=Editor.Get()||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||IsPlaySessionActive()
            ||!Loaded(World.Get())||World->WorldType!=EWorldType::Editor||World->bIsTearingDown)
        {return Stop(TEXT("data_asset_deformer_context_changed"),TEXT("Captured Editor/world/GC/exit/save admission changed; no next SDK or save."));}
        return GEditor->GetEditorWorldContext().World()==World.Get()||Stop(TEXT("data_asset_deformer_world_changed"),TEXT("Current Editor world changed; actual partial output retained."));
    }
    bool Start()
    {
        if(!IsEditorGameThread()||!Loaded(GEditor)||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||IsPlaySessionActive())
        {return Reject(TEXT("data_asset_deformer_context_required"),TEXT("Idle Editor context required before collection array work."));}
        Editor=GEditor;World=GEditor->GetEditorWorldContext().World();return Context();
    }
    bool Pin(UObject* O)
    {
        if(!O){return true;}if(!Context()||!Loaded(O)||!Loaded(O->GetClass())||!Loaded(O->GetPackage())||(O->GetOuter()&&!Loaded(O->GetOuter()))||!Clean(O->GetPathName())||!Clean(O->GetClass()->GetPathName()))
        {return Stop(TEXT("data_asset_deformer_identity_invalid"),TEXT("Current live exact source/target identity required before strong adoption."));}
        for(const auto& I:Identities){if(I.Hold.Get()==O){return true;}}if(Identities.Num()>=128){return Stop(TEXT("data_asset_deformer_identity_bound"),TEXT("Finite collection identity budget exceeded."));}
        FIdentity I;I.Hold.Reset(O);I.Package.Reset(O->GetPackage());I.Class=O->GetClass();I.Outer=O->GetOuter();I.Path=O->GetPathName();I.Flags=O->GetFlags();Identities.Add(MoveTemp(I));return true;
    }
    bool Paths(UMeshDeformerCollection* A,TArray<FString>& Out)
    {
        if(A->MeshDeformers.Num()>1024){return false;}for(const auto& P:A->MeshDeformers){const FString S=P.ToSoftObjectPath().ToString();if(S.Len()>2048||HasNul(S)||!Clean(S)){return false;}Out.Add(S);}return true;
    }
    bool BindPackage(UPackage* P,UMeshDeformerCollection* Primary)
    {
        if(PackageExports.Contains(P)){return true;}TArray<UObject*> Objects;GetObjectsWithPackage(P,Objects);
        if(!Context()||Objects.Num()>64){return Stop(TEXT("data_asset_deformer_package_bound"),TEXT("Finite exact package export inventory required."));}
        for(UObject* O:Objects)
        {if(!Loaded(O)||(O!=Primary&&O->GetClass()->GetPathName()!=TEXT("/Script/CoreUObject.MetaData"))){return Reject(TEXT("data_asset_deformer_foreign_package_export"),TEXT("Only the selected native collection and existing native package metadata exports are admitted; siblings/custom exports remain unsupported."));}if(!Pin(O)){return false;}}
        PackageExports.Add(P,MoveTemp(Objects));return true;
    }
    bool BindTarget(UDataAsset* A,const FString& Path)
    {
        if(!Context()||!Loaded(A)||A->GetClass()!=UMeshDeformerCollection::StaticClass()||A->GetOuter()!=A->GetPackage()||A->GetPathName()!=Path||A->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject))
        {return Stop(TEXT("data_asset_deformer_target_invalid"),TEXT("Exact ordinary native MeshDeformerCollection target required."));}
        if(Bound){return A==Target.Get()?Admit():Stop(TEXT("data_asset_deformer_target_replaced"),TEXT("Original relookup returned another selected asset; no adoption or replay."));}
        // No source getter/export before the exact returned target is held.
        Target.Reset(static_cast<UMeshDeformerCollection*>(A));TargetPath=Path;Class.Reset(A->GetClass());ClassFlags=Class->GetClassFlags();
        if(Class.Get()!=UMeshDeformerCollection::StaticClass()||Class->GetClass()!=UClass::StaticClass()||Class->ClassGeneratedBy||!Class->HasAnyClassFlags(CLASS_Native)
            ||!Class->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||Class->GetPackage()->IsDirty()||Class->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists))
        {return Stop(TEXT("data_asset_deformer_class_invalid"),TEXT("Exact native compiled-in collection class required."));}
        if(!Pin(A)||!Pin(Class.Get())||!Pin(Class->GetPackage())){return false;}UObject* D=Class->GetDefaultObject(false);
        if(D&&(!Loaded(D)||D->GetClass()!=Class.Get()||!D->HasAnyFlags(RF_ClassDefaultObject)||!Pin(D))){return Stop(TEXT("data_asset_deformer_default_invalid"),TEXT("Existing native default is incomplete; no CDO warming."));}Default.Reset(D);
        SoftProperty=FindFProperty<FArrayProperty>(Class.Get(),TEXT("MeshDeformers"));HardProperty=FindFProperty<FArrayProperty>(Class.Get(),TEXT("MeshDeformerCollections"));DescriptionProperty=FindFProperty<FStrProperty>(Class.Get(),TEXT("Description"));
        auto* S=SoftProperty?CastField<FSoftObjectProperty>(SoftProperty->Inner):nullptr;auto* H=HardProperty?CastField<FObjectProperty>(HardProperty->Inner):nullptr;
        for(FArrayProperty* P:{SoftProperty,HardProperty})
        {if(!P||P->GetOwnerStruct()!=Class.Get()||P->ArrayDim!=1||!P->HasAllPropertyFlags(CPF_Edit)||P->HasAnyPropertyFlags(CPF_EditConst|CPF_Config|CPF_GlobalConfig|CPF_Transient|CPF_DisableEditOnInstance|CPF_ContainsInstancedReference))
            {return Stop(TEXT("data_asset_deformer_schema_invalid"),TEXT("Exact editable noninstanced native collection array schemas required."));}}
        if(!DescriptionProperty||DescriptionProperty->GetOwnerStruct()!=Class.Get()||DescriptionProperty->ArrayDim!=1||!DescriptionProperty->HasAllPropertyFlags(CPF_Edit|CPF_EditorOnly)||DescriptionProperty->HasAnyPropertyFlags(CPF_EditConst|CPF_Config|CPF_GlobalConfig|CPF_Transient|CPF_DisableEditOnInstance))
        {return Stop(TEXT("data_asset_deformer_schema_invalid"),TEXT("Existing exact editable Editor-only Description FString required."));}
        if(!S||S->PropertyClass!=UMeshDeformer::StaticClass()||!H||H->PropertyClass!=Class.Get()||!Pin(S->PropertyClass))
        {return Stop(TEXT("data_asset_deformer_schema_invalid"),TEXT("Exact soft MeshDeformer and hard MeshDeformerCollection element classes required."));}
        TArray<UObject*> Owned;GetObjectsWithOuter(A,Owned,EGetObjectsFlags::IncludeNestedObjects);if(!Context()||!Owned.IsEmpty()||A->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn|PKG_PlayInEditor|PKG_EditorOnly))
        {return Stop(TEXT("data_asset_deformer_owned_unsupported"),TEXT("Noninstanced top-level collection without owned exports required."));}
        if(!Paths(Target.Get(),Soft)||Target->MeshDeformerCollections.Num()>1024){return Stop(TEXT("data_asset_deformer_array_bound"),TEXT("Current collection arrays exceed finite observation scope."));}
        for(UMeshDeformerCollection* P:Target->MeshDeformerCollections){Hard.Add(P);}Description=Target->Description;Bound=true;if(!BindPackage(A->GetPackage(),Target.Get())){return false;}return Admit();
    }
    bool Admit(FProperty* Mutable=nullptr)
    {
        if(!Context()){return false;}
        for(const auto& I:Identities)
        {auto* O=I.Hold.Get();if(!Loaded(O)||O->GetClass()!=I.Class||O->GetOuter()!=I.Outer||O->GetPackage()!=I.Package.Get()||O->GetPathName()!=I.Path||(O!=Target.Get()&&O->GetFlags()!=I.Flags))
            {return Stop(TEXT("data_asset_deformer_identity_changed"),TEXT("Captured source/target identity changed; no stale object or next SDK."));}}
        for(const auto& Pair:PackageExports)
        {TArray<UObject*> Now;GetObjectsWithPackage(Pair.Key,Now);if(Now.Num()!=Pair.Value.Num()){return Stop(TEXT("data_asset_deformer_package_exports_changed"),TEXT("Captured package gained/lost an export; no next SDK or save."));}for(UObject* O:Pair.Value){if(!Now.Contains(O)){return Stop(TEXT("data_asset_deformer_package_exports_changed"),TEXT("Captured package export identity changed."));}}}
        if(!Bound){return true;}
        if(Target->GetClass()!=Class.Get()||FindObject<UDataAsset>(nullptr,*TargetPath)!=Target.Get()||Class->ClassGeneratedBy||Class->GetClassFlags()!=ClassFlags||Class->GetDefaultObject(false)!=Default.Get()
            ||FindFProperty<FArrayProperty>(Class.Get(),TEXT("MeshDeformers"))!=SoftProperty||FindFProperty<FArrayProperty>(Class.Get(),TEXT("MeshDeformerCollections"))!=HardProperty||FindFProperty<FStrProperty>(Class.Get(),TEXT("Description"))!=DescriptionProperty||Class->GetPackage()->IsDirty())
        {return Stop(TEXT("data_asset_deformer_role_changed"),TEXT("Current target/native class/default/property role changed."));}
        if(Mutable!=SoftProperty){TArray<FString> Now;if(!Paths(Target.Get(),Now)||Now!=Soft){return Stop(TEXT("data_asset_deformer_array_changed"),TEXT("Current soft paths changed outside selected direct import."));}}
        if(Mutable!=HardProperty)
        {if(Target->MeshDeformerCollections.Num()!=Hard.Num()){return Stop(TEXT("data_asset_deformer_array_changed"),TEXT("Current hard array changed outside selected direct import."));}
         for(int32 I=0;I<Hard.Num();++I){if(Target->MeshDeformerCollections[I].Get()!=Hard[I]){return Stop(TEXT("data_asset_deformer_array_changed"),TEXT("Current ordered hard collection pointers changed outside selected direct import."));}}}
        if(Mutable!=DescriptionProperty&&Target->Description!=Description){return Stop(TEXT("data_asset_deformer_description_changed"),TEXT("Current Description changed outside its selected import."));}
        for(const auto& S:Collections)
        {auto* A=S.Hold.Get();TArray<FString> Current;if(!Loaded(A)||A->GetClass()!=Class.Get()||A->GetPackage()->IsDirty()||uint32(A->GetPackage()->GetPackageFlags())!=S.PackageFlags||A->Description!=S.Description||!Paths(A,Current)||Current!=S.Soft||A->MeshDeformerCollections.Num()!=S.Hard.Num())
            {return Stop(TEXT("data_asset_deformer_source_changed"),TEXT("Read-only source collection payload/package changed."));}
         for(int32 I=0;I<S.Hard.Num();++I){if(A->MeshDeformerCollections[I].Get()!=S.Hard[I]){return Stop(TEXT("data_asset_deformer_source_changed"),TEXT("Read-only source collection routing changed."));}}}
        return true;
    }
    bool Gather(UMeshDeformerCollection* A,TSet<UMeshDeformerCollection*>& Visiting,int32 Depth)
    {
        if(!A){return true;}if(!Admit()){return false;}
        if(A==Target.Get()||Visiting.Contains(A)){return Reject(TEXT("data_asset_deformer_cycle_unsupported"),TEXT("Target/self/cyclic collection references are refused before Modify/import."));}
        for(const auto& S:Collections){if(S.Hold.Get()==A){return true;}}
        if(Depth>8||Collections.Num()>=16||!Loaded(A)||A->GetClass()!=Class.Get()||A->GetOuter()!=A->GetPackage()||A->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject)
            ||A->GetPackage()->IsDirty()||!A->GetPackage()->IsFullyLoaded()||!A->GetPackage()->GetName().StartsWith(TEXT("/Game/"))||A->GetPackage()==Target->GetPackage()||!Clean(A->GetPathName())||A->MeshDeformerCollections.Num()>1024)
        {return Reject(TEXT("data_asset_deformer_source_unsupported"),TEXT("Finite already loaded exact saved-clean /Game collection sources in distinct packages required; no source loader or repair."));}
        // Strong adoption precedes export/disk/metadata SDK boundaries.
        FSource S;S.Hold.Reset(A);S.Description=A->Description;S.PackageFlags=uint32(A->GetPackage()->GetPackageFlags());
        if(!Pin(A)||!Pin(A->GetPackage())||!BindPackage(A->GetPackage(),A)){return false;}if(!Paths(A,S.Soft)){return Reject(TEXT("data_asset_deformer_source_paths_unsupported"),TEXT("Readonly soft paths exceed finite source scope."));}for(UMeshDeformerCollection* P:A->MeshDeformerCollections){S.Hard.Add(P);}
        SourceSlots+=S.Soft.Num()+S.Hard.Num();if(SourceSlots>4096){return Reject(TEXT("data_asset_deformer_source_bound"),TEXT("Aggregate source collection slots exceed4096."));}
        TArray<UObject*> Owned;GetObjectsWithOuter(A,Owned,EGetObjectsFlags::IncludeNestedObjects);if(!Admit()){return false;}if(!Owned.IsEmpty()){return Reject(TEXT("data_asset_deformer_source_owned_unsupported"),TEXT("Source collections must have no owned object exports."));}
        Visiting.Add(A);const auto Children=S.Hard;Collections.Add(MoveTemp(S));
        for(auto* Child:Children){if(!Gather(Child,Visiting,Depth+1)){return false;}}Visiting.Remove(A);return Admit();
    }
    bool Prepare(const TSharedRef<FJsonObject>& Requested)
    {
        if(!Admit()){return false;}TSet<UMeshDeformerCollection*> Visiting;for(auto* P:Hard){if(!Gather(P,Visiting,0)){return false;}}
        for(const auto& Pair:Requested->Values)
        {
            const FString Name(Pair.Key.ToView());if(Name.Len()>=NAME_SIZE||Pair.Value->Type!=EJson::Array){continue;}
            if(FName(*Name)==FName(TEXT("MeshDeformers"))&&Pair.Value->AsArray().Num()>1024){return Reject(TEXT("data_asset_deformer_array_bound"),TEXT("Soft deformer request exceeds1024 entries."));}
            if(FName(*Name)!=FName(TEXT("MeshDeformerCollections"))){continue;}
            if(Pair.Value->AsArray().Num()>1024){return Reject(TEXT("data_asset_deformer_array_bound"),TEXT("Hard collection request exceeds1024 entries."));}
            for(const auto& V:Pair.Value->AsArray())
            {
                if(!V.IsValid()||V->Type==EJson::Null){continue;}if(V->Type!=EJson::String){return Reject(TEXT("data_asset_deformer_reference_shape_unsupported"),TEXT("Hard collection entries require path strings/null; inline object construction is not admitted."));}
                FString P=FPackageName::ExportTextPathToObjectPath(V->AsString()).TrimStartAndEnd();P.TrimQuotesInline();if(P.IsEmpty()||P==TEXT("None")){continue;}
                if(P.Len()>=NAME_SIZE||!FPackageName::IsValidObjectPath(P)||!Clean(P)){return Reject(TEXT("data_asset_deformer_reference_invalid"),TEXT("Bounded exact hard source object path required."));}
                auto* O=FindObject<UObject>(nullptr,*P);if(!Loaded(O)||O->GetClass()!=Class.Get()||O->GetPathName()!=P){return Reject(TEXT("data_asset_deformer_reference_unavailable"),TEXT("Hard collection source must already resolve to the exact loaded native asset; no loader/alias fallback."));}
                if(!Gather(static_cast<UMeshDeformerCollection*>(O),Visiting,0)){return false;}
            }
        }
        auto J=MakeShared<FJsonObject>();if(!ReadonlySnapshot(J)){return false;}SourceBefore=J;R.Data->SetObjectField(TEXT("deformer_readonly_sources_before"),J);return Admit();
    }
    bool ReadonlySnapshot(TSharedRef<FJsonObject>& Out)
    {
        if(!Admit()){return false;}int64 Bytes=0;
        for(const auto& S:Collections)
        {auto Row=Identity(S.Hold.Get());auto P=MakeShared<FJsonObject>(),D=MakeShared<FJsonObject>();Row->SetObjectField(TEXT("properties"),P);Row->SetObjectField(TEXT("disk"),D);Row->SetNumberField(TEXT("package_flags"),S.PackageFlags);Row->SetBoolField(TEXT("dirty"),S.Hold->GetPackage()->IsDirty());
         FString Filename;if(!FPackageName::TryConvertLongPackageNameToFilename(S.Hold->GetPackage()->GetName(),Filename,FPackageName::GetAssetPackageExtension())){return Stop(TEXT("data_asset_deformer_source_disk_incomplete"),TEXT("Source package filename is unavailable."));}const int64 Size=IFileManager::Get().FileSize(*Filename);
         if(Size<0||Size>16*1024*1024-Bytes||!Fields(S.Hold.Get(),P,nullptr,this)||!Disk(S.Hold->GetPackage()->GetName(),D)||!Admit()||!D->GetBoolField(TEXT("exists"))||int64(D->GetNumberField(TEXT("bytes")))!=Size)
            {return Stop(TEXT("data_asset_deformer_source_unobservable"),TEXT("Complete readonly source fields/disk exceed finite scope or are unavailable; no writer/save."));}Bytes+=Size;Out->SetObjectField(S.Hold->GetPathName(),Row);}
        if(Text(Out).Len()>1048576){return Stop(TEXT("data_asset_deformer_source_bound"),TEXT("Readonly source witness exceeds1Mi UTF16; no writer/save."));}return Admit();
    }
    bool CheckReadonly()
    {if(!Admit()){return false;}auto J=MakeShared<FJsonObject>();if(!ReadonlySnapshot(J)){return false;}if(!SourceBefore||!Equal(SourceBefore.ToSharedRef(),J)){return Stop(TEXT("data_asset_deformer_readonly_preservation_failed"),TEXT("Read-only source fields/disk changed after SDK; no next writer/save."));}R.Data->SetBoolField(TEXT("deformer_readonly_sources_preserved"),true);return Admit();}
    bool AdoptActual(FProperty* Written)
    {
        if(!Admit(Written)){return false;}TArray<FString> Now;if(!Paths(Target.Get(),Now)||Target->MeshDeformerCollections.Num()>1024){return Stop(TEXT("data_asset_deformer_partial_bound"),TEXT("Actual partial arrays exceed finite observation; no save/replay."));}
        TArray<UMeshDeformerCollection*> Current;for(UMeshDeformerCollection* P:Target->MeshDeformerCollections)
        {if(P){bool Held=false;for(const auto& S:Collections){Held|=S.Hold.Get()==P;}if(!Held||P==Target.Get()){return Stop(TEXT("data_asset_deformer_unplanned_reference"),TEXT("Converter introduced an unpinned/self hard reference; actual partial output retained, no next SDK."));}}Current.Add(P);}
        if(Written==SoftProperty){Soft=MoveTemp(Now);}if(Written==HardProperty){Hard=MoveTemp(Current);}if(Written==DescriptionProperty){Description=Target->Description;}return Admit();
    }
};
bool DeformerAdmit(FDeformerCollectionScope* Scope){return !Scope||Scope->Admit();}
// KA: get_data_asset-only observation of five exact native owned input classes.
// No instance authoring, notification, evaluation, source loader or caller-issued token.
struct FInputOwnedReadScope
{
    FResult& R;bool Stopped=false,Bound=false,MappingSet=false,ActionSet=false,MappingCopyInFlight=false,ReferenceClear=false;bool TargetModifyEntered=false,TargetModifyCanAddTransactional=false,TargetPackageTransactionalObserved=false,PlayerSettingsHost=false;UObject* Allocating=nullptr;UClass* AllocatingClass=nullptr;TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;
    TStrongObjectPtr<UDataAsset> Target;TArray<UObject*> Owned,PackageExports;TSharedRef<FJsonObject> Before=MakeShared<FJsonObject>();
    struct FIdentity{TStrongObjectPtr<UObject> Hold;UClass* Class=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;FString Path;EObjectFlags Flags=RF_NoFlags;};TArray<FIdentity> Held;
    struct FLayout{FProperty* P=nullptr;UStruct* Owner=nullptr;FName Name;EPropertyFlags Flags=CPF_None;int32 Offset=0,Size=0;FProperty* Inner=nullptr;EPropertyFlags InnerFlags=CPF_None;int32 InnerSize=0;UScriptStruct* InnerStruct=nullptr;int32 InnerStructSize=0;UScriptStruct* Struct=nullptr;int32 StructSize=0;FObjectPropertyBase* Object=nullptr;UClass* ObjectClass=nullptr;UEnum* Enum=nullptr;FNumericProperty* Underlying=nullptr;};TArray<FLayout> Layouts;
    struct FNative{UClass* Class=nullptr;UObject* Default=nullptr;UClass* Super=nullptr;EClassFlags Flags=CLASS_None;uint32 PackageFlags=0;};TArray<FNative> Native;
    struct FCurve{UObject* Object=nullptr;UPackage* Package=nullptr;uint32 Flags=0;FObjectPropertyBase* ImportProperty=nullptr;UObject* ImportData=nullptr;TArray<UObject*> Exports;};TArray<FCurve> Curves;
    struct FCurveSlot{UObject* Owner=nullptr;FObjectPropertyBase* Property=nullptr;UObject* Curve=nullptr;};TArray<FCurveSlot> CurveSlots;
    TArray<UObject*> PlayerSettingsObjects;TMap<UObject*,FObjectProperty*> PlayerSettingsMetadata;FObjectProperty* ActionSettings=nullptr;FObjectProperty* RowSettings=nullptr;
    TArray<UObject*> NewResponseObjects;UObject* PendingCurveOwner=nullptr;FObjectPropertyBase* PendingCurveProperty=nullptr;UObject* PendingCurveValue=nullptr;
    FArrayProperty* ActionTriggers=nullptr;FArrayProperty* ActionModifiers=nullptr;FStructProperty* Defaults=nullptr;FArrayProperty* Rows=nullptr;FArrayProperty* RowTriggers=nullptr;FArrayProperty* RowModifiers=nullptr;TArray<FString> Links,ClearedOwnedPaths,ClearedActionPaths;TMap<FString,const void*> ArrayAddresses;bool LinksFrozen=false;
    explicit FInputOwnedReadScope(FResult& In,bool MappingWriter=false,bool ActionWriter=false,bool ClearWriter=false):R(In),MappingSet(MappingWriter),ActionSet(ActionWriter),ReferenceClear(ClearWriter){Editor=GEditor;if(Loaded(GEditor)){World=GEditor->GetEditorWorldContext().World();}}
    bool Stop(const TCHAR* Code,const TCHAR* Message)
    {if(!Stopped){Fail(R,Code,Message,true);R.Stage=TEXT("verification");R.Data->SetStringField(TEXT("owned_input_read_first_failure"),Code);}Stopped=true;return false;}
    bool Context()
    {
        if(Stopped){return false;}if((R.Op!=TEXT("get_data_asset")&&!((MappingSet||ActionSet)&&R.Op==TEXT("set_data_asset_properties")))||(MappingSet&&Target.IsValid()&&Target->GetClass()->GetPathName()!=MappingContextClassPath)||(ActionSet&&Target.IsValid()&&Target->GetClass()->GetPathName()!=InputActionClassPath)||!IsEditorGameThread()||!Loaded(GEditor)||GEditor!=Editor.Get()||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||IsPlaySessionActive()||!Loaded(World.Get())||World->WorldType!=EWorldType::Editor||World->bIsTearingDown||GEditor->GetEditorWorldContext().World()!=World.Get())
        {return Stop(TEXT("data_asset_owned_read_context_changed"),TEXT("Captured owned Get/finite Context-set observer changed; no further explicit read/write/retry."));}return true;
    }
    bool Pin(UObject* O)
    {
        if(!Context()||!Loaded(O)||!Loaded(O->GetClass())||!Loaded(O->GetPackage())||!Clean(O->GetPathName())||!Clean(O->GetClass()->GetPathName())){return Stop(TEXT("data_asset_owned_read_identity_invalid"),TEXT("Current clean native/owned/source identity required."));}
        for(const auto& I:Held){if(I.Hold.Get()==O){return true;}}if(Held.Num()>=512){return Stop(TEXT("data_asset_owned_read_identity_bound"),TEXT("Finite owned-input read identity budget exceeded."));}
        FIdentity I;I.Hold.Reset(O);I.Class=O->GetClass();I.Outer=O->GetOuter();I.Package=O->GetPackage();I.Path=O->GetPathName();I.Flags=O->GetFlags();Held.Add(MoveTemp(I));return true;
    }
    int32 Kind(UClass* C)const
    {
        if(!Loaded(C)){return INDEX_NONE;}const FString P=C->GetPathName();
        const TCHAR* Paths[]={TEXT("/Script/EnhancedInput.InputModifierDeadZone"),TEXT("/Script/EnhancedInput.InputModifierNegate"),TEXT("/Script/EnhancedInput.InputModifierResponseCurveUser"),TEXT("/Script/EnhancedInput.InputTriggerDown"),TEXT("/Script/EnhancedInput.InputTriggerReleased"),TEXT("/Script/EnhancedInput.InputModifierScalar"),TEXT("/Script/EnhancedInput.InputModifierSwizzleAxis"),TEXT("/Script/EnhancedInput.InputTriggerPressed"),TEXT("/Script/EnhancedInput.InputModifierResponseCurveExponential"),TEXT("/Script/EnhancedInput.InputModifierFOVScaling"),TEXT("/Script/EnhancedInput.InputModifierScaleByDeltaTime"),PlayerSettingsClassPath};
        for(int32 I=0;I<UE_ARRAY_COUNT(Paths);++I){if(P.Equals(Paths[I],ESearchCase::CaseSensitive)&&FindObject<UClass>(nullptr,Paths[I])==C){return I;}}return INDEX_NONE;
    }
    bool TriggerKind(int32 K)const{return K==3||K==4||K==7;}
    bool WriterKind(int32 K)const{return K>=0&&K<=10;}
    // PB: pure request classification precedes every mixed-slot class/CDO query.
    bool MathModifierPath(const FString& Path)const
    {return Path.Equals(TEXT("/Script/EnhancedInput.InputModifierResponseCurveExponential"),ESearchCase::CaseSensitive)||Path.Equals(TEXT("/Script/EnhancedInput.InputModifierFOVScaling"),ESearchCase::CaseSensitive)||Path.Equals(TEXT("/Script/EnhancedInput.InputModifierScaleByDeltaTime"),ESearchCase::CaseSensitive);}
    bool KeepMathModifierTarget(UDataAsset* ReturnedTarget)
    {
        if(Bound||!Context()||!Loaded(ReturnedTarget)||(Target.IsValid()&&Target.Get()!=ReturnedTarget)){return Stop(TEXT("data_asset_math_modifier_owner_unavailable"),TEXT("Exact returned target must be held before math modifier source planning."));}
        Target.Reset(ReturnedTarget);if(!Pin(ReturnedTarget)||!Pin(ReturnedTarget->GetPackage())||!Pin(ReturnedTarget->GetClass())||!Current()){return false;}
        R.Data->SetBoolField(TEXT("native_input_math_modifier_literal"),true);R.Data->SetBoolField(TEXT("math_modifier_runtime_evaluation_verified"),false);return true;
    }
    bool Layout(FProperty* P)
    {
        if(!P||!Loaded(P->GetOwnerStruct())||P->ArrayDim!=1||P->ElementSize<=0){return false;}for(const auto& L:Layouts){if(L.P==P){return true;}}
        if(Layouts.Num()>=256||!Pin(P->GetOwnerStruct())){return false;}FLayout L;L.P=P;L.Owner=P->GetOwnerStruct();L.Name=P->GetFName();L.Flags=P->GetPropertyFlags();L.Offset=P->GetOffset_ForInternal();L.Size=P->ElementSize;
        if(FArrayProperty* A=CastField<FArrayProperty>(P)){if(!A->Inner||A->Inner->ArrayDim!=1||A->Inner->ElementSize<=0){return false;}L.Inner=A->Inner;L.InnerFlags=A->Inner->GetPropertyFlags();L.InnerSize=A->Inner->ElementSize;if(FStructProperty* S=CastField<FStructProperty>(A->Inner)){if(!Loaded(S->Struct)||!Pin(S->Struct)){return false;}L.InnerStruct=S->Struct;L.InnerStructSize=S->Struct->GetStructureSize();}}
        if(FStructProperty* S=CastField<FStructProperty>(P)){if(!Loaded(S->Struct)||!Pin(S->Struct)){return false;}L.Struct=S->Struct;L.StructSize=S->Struct->GetStructureSize();}
        L.Object=CastField<FObjectPropertyBase>(L.Inner?L.Inner:P);if(L.Object){if(!Loaded(L.Object->PropertyClass)||!Pin(L.Object->PropertyClass)){return false;}L.ObjectClass=L.Object->PropertyClass;}
        if(FEnumProperty* E=CastField<FEnumProperty>(P)){if(!Loaded(E->GetEnum())||!E->GetUnderlyingProperty()||!Pin(E->GetEnum())){return false;}L.Enum=E->GetEnum();L.Underlying=E->GetUnderlyingProperty();}
        Layouts.Add(L);return true;
    }
    bool FieldsLayout(UClass* C)
    {int32 N=0;for(TFieldIterator<FProperty> It(C,EFieldIteratorFlags::IncludeSuper);It;++It){if(++N>64||!Layout(*It)){return false;}}return true;}
    bool NativeClass(UClass* C,const TCHAR* Package,const TCHAR* SuperPath)
    {
        if(!Loaded(C)||C->GetClass()!=UClass::StaticClass()||C->ClassGeneratedBy||!C->HasAnyClassFlags(CLASS_Native)||C->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)||C->IsEditorOnly()||C->GetPackage()->GetName()!=Package||!C->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||C->GetPackage()->HasAnyPackageFlags(PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly)||C->GetPackage()->IsDirty()||!Loaded(C->GetSuperClass())||C->GetSuperClass()->GetPathName()!=SuperPath){return false;}
        for(const auto& N:Native){if(N.Class==C){return true;}}if(!Pin(C)||!Pin(C->GetPackage())||!Pin(C->GetSuperClass())||!FieldsLayout(C)){return false;}
        UObject* D=C->GetDefaultObject(false);if(D&&(!Loaded(D)||D->GetClass()!=C||D->GetOuter()!=C->GetPackage()||!D->HasAnyFlags(RF_ClassDefaultObject)||!Pin(D))){return false;}FNative N;N.Class=C;N.Default=D;N.Super=C->GetSuperClass();N.Flags=C->GetClassFlags();N.PackageFlags=uint32(C->GetPackage()->GetPackageFlags());Native.Add(N);return true;
    }
    bool Curve(UObject* O)
    {
        if(!O){return true;}for(const auto& C:Curves){if(C.Object==O){return true;}}if(Curves.Num()>=16||!Loaded(O)||O->GetClass()!=FindObject<UClass>(nullptr,TEXT("/Script/Engine.CurveFloat"))||O->GetOuter()!=O->GetPackage()||O->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_Transient)||!FPackageName::IsValidObjectPath(O->GetPathName())||!(O->GetPackage()->GetName().StartsWith(TEXT("/Game/"))||O->GetPackage()->GetName().StartsWith(TEXT("/Engine/")))||O->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn|PKG_PlayInEditor|PKG_EditorOnly)||O->GetPackage()->IsDirty()||!O->GetPackage()->IsFullyLoaded()||!NativeClass(O->GetClass(),TEXT("/Script/Engine"),TEXT("/Script/Engine.CurveBase"))||!Pin(O)||!Pin(O->GetPackage())){return false;}
        FCurve C;C.Object=O;C.Package=O->GetPackage();C.Flags=uint32(C.Package->GetPackageFlags());C.ImportProperty=FindFProperty<FObjectPropertyBase>(O->GetClass(),TEXT("AssetImportData"));if(!C.ImportProperty||C.ImportProperty->GetOwnerStruct()->GetPathName()!=TEXT("/Script/Engine.CurveBase")||!Loaded(C.ImportProperty->PropertyClass)||C.ImportProperty->PropertyClass->GetPathName()!=TEXT("/Script/Engine.AssetImportData")||!Layout(C.ImportProperty)){return false;}C.ImportData=C.ImportProperty->GetObjectPropertyValue_InContainer(O);
        // CurveBase::PostInitProperties creates this stock editor-only owned source object.
        if(C.ImportData&&(!Loaded(C.ImportData)||C.ImportData->GetOuter()!=O||C.ImportData->GetClass()!=C.ImportProperty->PropertyClass.Get()||!NativeClass(C.ImportData->GetClass(),TEXT("/Script/Engine"),TEXT("/Script/CoreUObject.Object"))||!Pin(C.ImportData))){return false;}
        GetObjectsWithOuter(C.Package,C.Exports,EGetObjectsFlags::IncludeNestedObjects);if(C.Exports.Num()>16){return false;}for(UObject* X:C.Exports){if(!Loaded(X)||(X!=O&&X!=C.ImportData&&X->GetClass()->GetPathName()!=TEXT("/Script/CoreUObject.MetaData"))||!Pin(X)){return false;}}
        auto File=MakeShared<FJsonObject>();if(!Disk(C.Package->GetName(),File)||!File->GetBoolField(TEXT("exists"))||File->GetNumberField(TEXT("bytes"))<=0){return false;}Curves.Add(MoveTemp(C));return true;
    }

    // OY: bind only exact preloaded CurveFloat references before the first owned/source baseline.
    // JSON null is the original recursive converter's skip; "None" is an explicit null assignment.
    bool CurveReference(const TSharedPtr<FJsonValue>& Value,UObject*& Resolved,bool Enroll)
    {
        Resolved=nullptr;if(!Current()||!Value){return false;}if(Value->Type==EJson::Null){return true;}
        if(Value->Type!=EJson::String){return Stop(TEXT("data_asset_response_curve_value_unsupported"),TEXT("Response axes require JSON null (keep native default), None, or an already loaded exact CurveFloat reference; no inline curve/number/coercion."));}
        const FString Requested=Value->AsString();if(Requested.Equals(TEXT("None"),ESearchCase::CaseSensitive)){return true;}
        const FString CurvePath=FPackageName::ExportTextPathToObjectPath(Requested);
        const bool ExactSpelling=Requested.Equals(CurvePath,ESearchCase::CaseSensitive)||Requested.Equals(TEXT("/Script/Engine.CurveFloat'")+CurvePath+TEXT("'"),ESearchCase::CaseSensitive)||Requested.Equals(TEXT("CurveFloat'")+CurvePath+TEXT("'"),ESearchCase::CaseSensitive);
        if(!ExactSpelling||CurvePath.Len()>1024||!Clean(CurvePath)||!FPackageName::IsValidObjectPath(CurvePath)||CurvePath.Contains(TEXT(":"))||!(CurvePath.StartsWith(TEXT("/Game/"))||CurvePath.StartsWith(TEXT("/Engine/"))))
        {return Stop(TEXT("data_asset_response_curve_path_unsupported"),TEXT("Exact full Game/Engine object path or CurveFloat class-export required; aliases/custom/inline/loading remain incomplete."));}
        UObject* ReturnedCurve=FindObject<UObject>(nullptr,*CurvePath);TStrongObjectPtr<UObject> ReturnedCurveHold(ReturnedCurve);
        if(!Current()){return false;}if(!Loaded(ReturnedCurve)||!ReturnedCurve->GetPathName().Equals(CurvePath,ESearchCase::CaseSensitive)||ReturnedCurve->GetClass()!=FindObject<UClass>(nullptr,TEXT("/Script/Engine.CurveFloat")))
        {return Stop(TEXT("data_asset_response_curve_not_loaded"),TEXT("Exact CurveFloat must already be loaded; no source load or retry was dispatched."));}
        if(Enroll){if(Bound||!Curve(ReturnedCurve)){return Stop(TEXT("data_asset_response_curve_source_unsupported"),TEXT("Saved-clean exact CurveFloat source could not be enrolled before the baseline."));}}
        else{bool HeldCurve=false;for(const auto& SourceCurve:Curves){HeldCurve|=SourceCurve.Object==ReturnedCurve;}if(!HeldCurve){return Stop(TEXT("data_asset_response_curve_source_unplanned"),TEXT("The requested curve was not held before the original owned/source baseline."));}}
        Resolved=ReturnedCurve;return Current();
    }
    bool PrepareCurveLiteral(UDataAsset* Owner,const TSharedRef<FJsonObject>& Literal)
    {
        FString RequestedClass;if(!Literal->TryGetStringField(TEXT("_ClassName"),RequestedClass)||RequestedClass!=TEXT("/Script/EnhancedInput.InputModifierResponseCurveUser")){return true;}
        if(Bound||!Current()||!Loaded(Owner)||(Target.IsValid()&&Target.Get()!=Owner)){return Stop(TEXT("data_asset_response_curve_owner_unavailable"),TEXT("The returned target must be held before response source planning."));}
        Target.Reset(Owner);if(!Pin(Owner)||!Pin(Owner->GetPackage())||!Pin(Owner->GetClass())||!Current()){return false;}
        if(Literal->Values.Num()!=4){return Stop(TEXT("data_asset_response_curve_literal_incomplete"),TEXT("Complete _ClassName/responseX/responseY/responseZ literal required; partial/arbitrary fields remain incomplete."));}
        for(const TCHAR* Axis:{TEXT("responseX"),TEXT("responseY"),TEXT("responseZ")})
        {const auto AxisValue=Literal->TryGetField(Axis);UObject* PlannedCurve=nullptr;if(!AxisValue||!CurveReference(AxisValue,PlannedCurve,true)){return false;}}
        R.Data->SetBoolField(TEXT("native_input_response_curve_literal"),true);
        R.Data->SetBoolField(TEXT("response_curve_source_loader_dispatched"),false);
        R.Data->SetStringField(TEXT("response_curve_null_semantics"),TEXT("JSON null skips the field and keeps the held native CDO default in a new instance; string None explicitly copies nullptr. Curves are exact preloaded saved-clean sources; runtime response evaluation is not performed."));
        return Current();
    }
    bool ResponseDefaults(UObject* Object,UClass* ResponseClass)
    {
        if(!Current()){return false;}UObject* ResponseDefault=ResponseClass->GetDefaultObject(false);
        if(!Loaded(ResponseDefault)){return Stop(TEXT("data_asset_response_curve_default_unavailable"),TEXT("Existing held native default required; no CDO creation."));}
        for(const TCHAR* Axis:{TEXT("ResponseX"),TEXT("ResponseY"),TEXT("ResponseZ")})
        {
            auto* AxisProperty=FindFProperty<FObjectPropertyBase>(ResponseClass,Axis);if(!AxisProperty){return false;}
            const auto ActualHandle=AxisProperty->GetObjectPtrPropertyValue_InContainer(Object),DefaultHandle=AxisProperty->GetObjectPtrPropertyValue_InContainer(ResponseDefault);
            if(!ActualHandle.IsResolved()||!DefaultHandle.IsResolved()||ActualHandle.Get()!=DefaultHandle.Get()){return Stop(TEXT("data_asset_response_curve_constructor_default_changed"),TEXT("New exact modifier must start with the held native CDO axis references; null is not assumed."));}
        }return Current();
    }
    bool BeginCurveField(UObject* Object,FProperty* Property,const void* PlannedStorage)
    {
        if(Kind(Object->GetClass())!=2){return true;}
        if(!NewResponseObjects.Contains(Object)||PendingCurveOwner||!Current()){return Stop(TEXT("data_asset_response_curve_field_not_new"),TEXT("Only one planned field of this call's new exact ResponseCurveUser may change."));}
        auto* AxisProperty=CastField<FObjectPropertyBase>(Property);if(!AxisProperty){return false;}
        const auto PlannedHandle=AxisProperty->GetObjectPtrPropertyValue(PlannedStorage);if(!PlannedHandle.IsResolved()){return false;}UObject* ExpectedCurve=PlannedHandle.Get();
        bool PlannedHeld=ExpectedCurve==nullptr;for(const auto& SourceCurve:Curves){PlannedHeld|=SourceCurve.Object==ExpectedCurve;}
        bool SlotFound=false;for(const auto& Slot:CurveSlots){SlotFound|=Slot.Owner==Object&&Slot.Property==AxisProperty;}
        if(!PlannedHeld||!SlotFound){return Stop(TEXT("data_asset_response_curve_field_unplanned"),TEXT("Exact new axis and prebound curve or nullptr required before copy."));}
        PendingCurveOwner=Object;PendingCurveProperty=AxisProperty;PendingCurveValue=ExpectedCurve;return true;
    }
    bool OwnedSchema(UObject* O,bool Instance=true)
    {
        if(!Loaded(O)||(Instance&&(O->GetOuter()!=Target.Get()||O->GetPackage()!=Target->GetPackage()||O->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_Transient)))){return false;}
        UClass* C=O->GetClass();const int32 K=Kind(C);const bool Trigger=TriggerKind(K);
        if(K==INDEX_NONE||!NativeClass(C,TEXT("/Script/EnhancedInput"),K==11?TEXT("/Script/CoreUObject.Object"):Trigger?TEXT("/Script/EnhancedInput.InputTrigger"):TEXT("/Script/EnhancedInput.InputModifier"))||!Pin(O)){return false;}
        if(K==11)return PlayerSettingsSchema(O);
        const TArray<FString> Names=K==8?TArray<FString>{TEXT("CurveExponent")}:K==9?TArray<FString>{TEXT("FOVScale"),TEXT("FOVScalingType")}:K==10?TArray<FString>{}:K==0?TArray<FString>{TEXT("LowerThreshold"),TEXT("UpperThreshold"),TEXT("Type")}:K==1?TArray<FString>{TEXT("bX"),TEXT("bY"),TEXT("bZ")}:K==2?TArray<FString>{TEXT("ResponseX"),TEXT("ResponseY"),TEXT("ResponseZ")}:K==5?TArray<FString>{TEXT("Scalar")}:K==6?TArray<FString>{TEXT("Order")}:TArray<FString>{TEXT("ActuationThreshold"),TEXT("bShouldAlwaysTick"),TEXT("LastValue")};
        int32 Counted=0;for(TFieldIterator<FProperty> It(C,EFieldIteratorFlags::IncludeSuper);It;++It){if(!Names.Contains(It->GetName())){return false;}++Counted;}if(Counted!=Names.Num()){return false;}
        for(int32 I=0;I<Names.Num();++I)
        {
            FProperty* P=C->FindPropertyByName(*Names[I]);if(!P||P->GetOwnerStruct()!=(Trigger?C->GetSuperClass():C)||!Layout(P)){return false;}
            if((K==0&&I<2)||(K==9&&I==0)||(Trigger&&I==0)){if(!CastField<FFloatProperty>(P)){return false;}}
            else if(K==0||K==6||K==9){FEnumProperty* E=CastField<FEnumProperty>(P);if(!E||!Loaded(E->GetEnum())||E->GetEnum()->GetPathName()!=(K==0?TEXT("/Script/EnhancedInput.EDeadZoneType"):K==9?TEXT("/Script/EnhancedInput.EFOVScalingType"):TEXT("/Script/EnhancedInput.EInputAxisSwizzle"))||!E->GetUnderlyingProperty()->IsA<FByteProperty>()){return false;}}
            else if(K==1||(Trigger&&I==1)){if(!CastField<FBoolProperty>(P)){return false;}}
            else if(K==2){FObjectPropertyBase* F=CastField<FObjectPropertyBase>(P);if(!F||!Loaded(F->PropertyClass)||F->PropertyClass->GetPathName()!=TEXT("/Script/Engine.CurveFloat")){return false;}UObject* V=nullptr;if(MappingSet||ActionSet){const auto CurveHandle=F->GetObjectPtrPropertyValue_InContainer(O);if(!CurveHandle.IsResolved()){return Stop(TEXT("data_asset_response_curve_handle_unresolved"),TEXT("Only already resolved response references are admitted; no lazy load."));}V=CurveHandle.Get();}else{V=F->GetObjectPropertyValue_InContainer(O);}if(!Curve(V)){return false;}FCurveSlot Slot;Slot.Owner=O;Slot.Property=F;Slot.Curve=V;CurveSlots.Add(Slot);}
            else if(K==5||K==8){FStructProperty* F=CastField<FStructProperty>(P);if(!F||F->Struct!=TBaseStructure<FVector>::Get()||F->Struct->GetStructureSize()!=sizeof(FVector)){return false;}}
            else{FStructProperty* F=CastField<FStructProperty>(P);if(!F||!Loaded(F->Struct)||F->Struct->GetPathName()!=TEXT("/Script/EnhancedInput.InputActionValue")){return false;}}
        }return true;
    }
    // RF: existing native player settings are immutable owned observations, never writer literals.
    bool PlayerSettingsSchema(UObject* Object)
    {
        UClass* C=Object->GetClass();const TCHAR* Names[]={TEXT("Metadata"),TEXT("Name"),TEXT("DisplayName"),TEXT("DisplayCategory"),TEXT("SupportedKeyProfiles"),TEXT("SupportedKeyProfileIds")};
        int32 Counted=0;for(TFieldIterator<FProperty> It(C,EFieldIterationFlags::IncludeDeprecated);It;++It){if(++Counted>6||!Layout(*It))return false;bool Known=false;for(const TCHAR* Name:Names)Known|=It->GetFName()==FName(Name);if(!Known)return false;}if(Counted!=6)return false;
        for(const TCHAR* Name:Names)
        {
            FProperty* P=FindFProperty<FProperty>(C,Name);if(!P||P->GetOwnerStruct()!=C||P->ArrayDim!=1)return false;
            if(P->GetFName()==TEXT("Metadata")){auto* F=CastField<FObjectProperty>(P);if(!F||!F->PropertyClass.IsResolved()||F->PropertyClass.Get()!=UObject::StaticClass())return false;const auto Value=F->GetObjectPtrPropertyValue_InContainer(Object);if(!Value.IsResolved()||Value.Get()!=nullptr)return Stop(TEXT("data_asset_player_settings_metadata_pending"),TEXT("First player settings source requires resolved null Metadata; arbitrary icon/asset references remain incomplete."));PlayerSettingsMetadata.Add(Object,F);}
            else if(P->GetFName()==TEXT("Name")){if(P->GetClass()!=FNameProperty::StaticClass())return false;}
            else if(P->GetFName()==TEXT("DisplayName")||P->GetFName()==TEXT("DisplayCategory")){if(P->GetClass()!=FTextProperty::StaticClass())return false;}
            else if(P->GetFName()==TEXT("SupportedKeyProfiles")){auto* F=CastField<FStructProperty>(P);if(!F||!F->Struct.IsResolved()||!Loaded(F->Struct.Get())||F->Struct->GetPathName()!=TEXT("/Script/GameplayTags.GameplayTagContainer"))return false;}
            else{auto* A=CastField<FArrayProperty>(P);if(!A||!A->Inner||A->Inner->GetOwnerVariant().ToField()!=A||A->Inner->GetClass()!=FStrProperty::StaticClass()||A->Inner->ArrayDim!=1||A->Inner->GetSize()!=sizeof(FString))return false;FScriptArrayHelper Values(A,A->ContainerPtrToValuePtr<void>(Object));if(Values.Num()>64)return false;for(int32 I=0;I<Values.Num();++I){const FString Value=static_cast<FStrProperty*>(A->Inner)->GetPropertyValue(Values.GetRawPtr(I));if(Value.Len()>1024||HasNul(Value)||!Clean(Value))return false;}}
        }
        PlayerSettingsObjects.AddUnique(Object);PlayerSettingsHost=true;R.Data->SetBoolField(TEXT("native_player_mappable_settings"),true);R.Data->SetBoolField(TEXT("player_settings_inline_allocation_supported"),false);R.Data->SetStringField(TEXT("player_settings_scope"),TEXT("Exact preexisting same-owner native PlayerMappableKeySettings with null Metadata; Name/FText/profile payloads and linked/unlinked object identities remain immutable. Populated Get, existing Action scalars, and Context OverrideSettings class-export references only; no input evaluation/rebinding or inline settings construction."));return true;
    }
    bool IsPlayerSettingsObject(UObject* Object)const
    {return !Object||(Loaded(Object)&&Object->GetOuter()==Target.Get()&&Object->GetPackage()==Target->GetPackage()&&Owned.Contains(Object)&&PlayerSettingsObjects.Contains(Object)&&Kind(Object->GetClass())==11);}
    bool PlayerSettingsLink(FObjectProperty* Property,void* Container,const FString& At,TArray<FString>& Out)
    {
        if(!Property||!Container)return false;const auto Value=Property->GetObjectPtrPropertyValue_InContainer(Container);if(!Value.IsResolved()||!IsPlayerSettingsObject(Value.Get()))return false;
        Out.Add(At+TEXT("=")+(Value.Get()?Value->GetPathName():TEXT("None")));return true;
    }
    UObject* ReusePlayerSettings(const FString& Export)
    {
        if(!Check())return nullptr;for(UObject* Object:PlayerSettingsObjects){if(IsPlayerSettingsObject(Object)&&Export.Equals(FString(PlayerSettingsClassPath)+TEXT("'")+Object->GetPathName()+TEXT("'"),ESearchCase::CaseSensitive))return Object;}return nullptr;
    }
    bool Array(FArrayProperty* P,void* Container,bool Trigger,const FString& At,TArray<FString>& Out)
    {
        if(!P||!Container){return false;}FObjectPropertyBase* Inner=CastField<FObjectPropertyBase>(P->Inner);if(!Inner||!Loaded(Inner->PropertyClass)||Inner->PropertyClass->GetPathName()!=(Trigger?TEXT("/Script/EnhancedInput.InputTrigger"):TEXT("/Script/EnhancedInput.InputModifier"))){return false;}
        FScriptArrayHelper A(P,P->ContainerPtrToValuePtr<void>(Container));if(A.Num()>64){return false;}const void* Address=A.Num()?A.GetRawPtr(0):nullptr;if(LinksFrozen){const void* const* Saved=ArrayAddresses.Find(At);if(!Saved||*Saved!=Address){return false;}}else{ArrayAddresses.Add(At,Address);}Out.Add(At+TEXT("#")+LexToString(A.Num()));
        for(int32 I=0;I<A.Num();++I){UObject* O=Inner->GetObjectPropertyValue(A.GetRawPtr(I));if(!Loaded(O)||O->GetOuter()!=Target.Get()||!Owned.Contains(O)){return false;}const int32 K=Kind(O->GetClass());if(K==INDEX_NONE||K==11||(Trigger?!TriggerKind(K):TriggerKind(K))){return false;}Out.Add(At+TEXT("/")+LexToString(I)+TEXT("=")+O->GetPathName());}return true;
    }
    bool ReadLinks(TArray<FString>& Out)
    {
        UDataAsset* A=Target.Get();if(ActionTriggers){if(PlayerSettingsHost&&!PlayerSettingsLink(ActionSettings,A,TEXT("PlayerSettings"),Out))return false;return Array(ActionTriggers,A,true,TEXT("Triggers"),Out)&&Array(ActionModifiers,A,false,TEXT("Modifiers"),Out);}
        if(!Defaults||!Rows){return false;}FScriptArrayHelper Rw(Rows,Rows->ContainerPtrToValuePtr<void>(Defaults->ContainerPtrToValuePtr<void>(A)));if(Rw.Num()>64){return false;}const void* Address=Rw.Num()?Rw.GetRawPtr(0):nullptr;if(LinksFrozen){const void* const* Saved=ArrayAddresses.Find(TEXT("Mappings"));if(!Saved||*Saved!=Address){return false;}}else{ArrayAddresses.Add(TEXT("Mappings"),Address);}Out.Add(TEXT("rows#")+LexToString(Rw.Num()));
        for(int32 I=0;I<Rw.Num();++I){if(!Array(RowTriggers,Rw.GetRawPtr(I),true,TEXT("Mappings/")+LexToString(I)+TEXT("/Triggers"),Out)||!Array(RowModifiers,Rw.GetRawPtr(I),false,TEXT("Mappings/")+LexToString(I)+TEXT("/Modifiers"),Out)){return false;}if(PlayerSettingsHost&&!PlayerSettingsLink(RowSettings,Rw.GetRawPtr(I),TEXT("Mappings/")+LexToString(I)+TEXT("/PlayerSettings"),Out))return false;}return true;
    }
    bool BeginTargetModify()
    {
        if(!Current()){return false;}
        TargetModifyEntered=true;TargetModifyCanAddTransactional=false;
        if(Bound&&(MappingSet||ActionSet)&&R.Op==TEXT("set_data_asset_properties")&&Target.IsValid())
        {
            UPackage* P=Target->GetPackage();
            TargetModifyCanAddTransactional=P&&P!=GetTransientPackage()&&!P->IsDirty()&&GUndo!=nullptr
                &&!P->HasAnyPackageFlags(PKG_PlayInEditor|PKG_ContainsScript|PKG_CompiledIn);
        }
        return true;
    }
    bool Current()
    {
        if(!Context()){return false;}for(const auto& I:Held)
        {
            UObject* O=I.Hold.Get();FString Property,Expected,Actual;bool SavedTargetPackageFlags=false,ModifiedTargetPackageFlags=false;
            auto Pointer=[](const void* P){return FString::Printf(TEXT("%p"),P);};
            if(!Loaded(O)){Property=TEXT("loaded");Expected=TEXT("true");Actual=TEXT("false");}
            else if(UClass* ActualClass=O->GetClass();ActualClass!=I.Class){Property=TEXT("class_pointer");Expected=Pointer(I.Class);Actual=Pointer(ActualClass);}
            else if(UObject* ActualOuter=O->GetOuter();ActualOuter!=I.Outer){Property=TEXT("outer_pointer");Expected=Pointer(I.Outer);Actual=Pointer(ActualOuter);}
            else if(UPackage* ActualPackage=O->GetPackage();ActualPackage!=I.Package){Property=TEXT("package_pointer");Expected=Pointer(I.Package);Actual=Pointer(ActualPackage);}
            else if(const FString ActualPath=O->GetPathName();ActualPath!=I.Path){Property=TEXT("path");Expected=I.Path;Actual=ActualPath;}
            else if(const EObjectFlags ActualFlags=O->GetFlags();ActualFlags!=I.Flags&&!((MappingSet||ActionSet)&&R.Saved&&Target.IsValid()&&ActualPackage==Target->GetPackage()&&ActualFlags==(I.Flags|RF_WasLoaded|RF_LoadCompleted))
                &&!(ModifiedTargetPackageFlags=Bound&&(MappingSet||ActionSet)&&R.Op==TEXT("set_data_asset_properties")&&TargetModifyEntered&&TargetModifyCanAddTransactional
                    &&Target.IsValid()&&O==ActualPackage&&ActualPackage==Target->GetPackage()&&I.Class==UPackage::StaticClass()&&I.Outer==nullptr&&I.Package==ActualPackage
                    &&!EnumHasAnyFlags(I.Flags,RF_Transactional)&&ActualFlags==(I.Flags|RF_Transactional)&&(TargetPackageTransactionalObserved||ActualPackage->IsDirty()))
                &&!(SavedTargetPackageFlags=Bound&&(MappingSet||ActionSet)&&R.Op==TEXT("set_data_asset_properties")&&R.SaveRequested&&R.SaveAttempted&&R.Saved
                    &&Target.IsValid()&&O==ActualPackage&&ActualPackage==Target->GetPackage()&&I.Class==UPackage::StaticClass()&&I.Outer==nullptr&&I.Package==ActualPackage
                    &&(I.Flags==RF_Public||TargetPackageTransactionalObserved)&&ActualFlags==(I.Flags|RF_Transactional|RF_WasLoaded|RF_LoadCompleted)))
            {Property=TEXT("object_flags");Expected=LexToString(uint32(I.Flags));Actual=LexToString(uint32(ActualFlags));}
            else if(UObject* Found=FindObject<UObject>(nullptr,*I.Path);Found!=O){Property=TEXT("find_object_pointer");Expected=Pointer(O);Actual=Pointer(Found);}
            else if(ModifiedTargetPackageFlags)
            {
                TargetPackageTransactionalObserved=true;
                if(!R.Data->HasField(TEXT("owned_input_read_target_package_transactional_transition")))
                {
                    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("object_path"),I.Path);J->SetStringField(TEXT("expected"),LexToString(uint32(I.Flags)));J->SetStringField(TEXT("actual"),LexToString(uint32(ActualFlags)));
                    J->SetBoolField(TEXT("exact_target_package_identity_verified"),true);J->SetBoolField(TEXT("expected_baseline_changed"),false);J->SetBoolField(TEXT("GUndo_present_before_Modify"),true);J->SetBoolField(TEXT("dirty_before_Modify"),false);
                    J->SetStringField(TEXT("scope"),TEXT("UPackage::SetDirtyFlag: exact held target package only, clean before this explicit Modify with GUndo, exact RF_Transactional addition; retained flags after unwind and existing saved loaded-bit boundary only. No source/owned/other bits or baseline adoption."));
                    R.Data->SetObjectField(TEXT("owned_input_read_target_package_transactional_transition"),J);
                }
            }
            else if(SavedTargetPackageFlags&&!R.Data->HasField(TEXT("owned_input_read_saved_target_package_flags_transition")))
            {
                auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("object_path"),I.Path);J->SetStringField(TEXT("property"),TEXT("UPackage EObjectFlags"));
                J->SetStringField(TEXT("expected"),LexToString(uint32(I.Flags)));J->SetStringField(TEXT("actual"),LexToString(uint32(ActualFlags)));
                J->SetBoolField(TEXT("saved"),R.Saved);J->SetBoolField(TEXT("exact_target_package_identity_verified"),true);J->SetBoolField(TEXT("expected_baseline_changed"),false);
                J->SetStringField(TEXT("scope"),TEXT("Finite owned writer only: saved exact target UPackage from original RF_Public or the separately observed explicit-Modify transactional transition to exact baseline|Transactional|WasLoaded|LoadCompleted; class/outer/package/path/refind passed, all later guards continue. No source/owned/other flags or baseline adoption."));
                R.Data->SetObjectField(TEXT("owned_input_read_saved_target_package_flags_transition"),J);
            }
            if(!Property.IsEmpty())
            {
                if(!R.Data->HasField(TEXT("owned_input_read_first_identity_mismatch")))
                {auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("object_path"),I.Path);J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);J->SetBoolField(TEXT("saved"),R.Saved);R.Data->SetObjectField(TEXT("owned_input_read_first_identity_mismatch"),J);}
                return Stop(TEXT("data_asset_owned_read_identity_changed"),TEXT("Held owner/instance/class/curve identity changed; no adoption or retry."));
            }
        }
        for(const auto& N:Native){if(N.Class->GetClassFlags()!=N.Flags||N.Class->GetSuperClass()!=N.Super||N.Class->GetDefaultObject(false)!=N.Default||N.Class->ClassGeneratedBy||N.Class->GetPackage()->IsDirty()||uint32(N.Class->GetPackage()->GetPackageFlags())!=N.PackageFlags){return Stop(TEXT("data_asset_owned_read_native_changed"),TEXT("Native class/ancestry/default/source changed."));}}
        for(const auto& L:Layouts)
        {
            if(L.Owner->FindPropertyByName(L.Name)!=L.P||L.P->GetOwnerStruct()!=L.Owner||L.P->ArrayDim!=1||L.P->GetPropertyFlags()!=L.Flags||L.P->GetOffset_ForInternal()!=L.Offset||L.P->ElementSize!=L.Size){return Stop(TEXT("data_asset_owned_read_layout_changed"),TEXT("Current exact reflected field layout changed before storage access."));}
            if(L.Inner){FArrayProperty* A=CastField<FArrayProperty>(L.P);if(!A||A->Inner!=L.Inner||L.Inner->ArrayDim!=1||L.Inner->GetPropertyFlags()!=L.InnerFlags||L.Inner->ElementSize!=L.InnerSize){return Stop(TEXT("data_asset_owned_read_inner_changed"),TEXT("Current array inner layout changed."));}}
            if(L.InnerStruct&&(!CastField<FStructProperty>(L.Inner)||CastField<FStructProperty>(L.Inner)->Struct!=L.InnerStruct||L.InnerStruct->GetStructureSize()!=L.InnerStructSize)){return Stop(TEXT("data_asset_owned_read_row_struct_changed"),TEXT("Current array struct identity/layout changed."));}
            if(L.Struct&&(CastField<FStructProperty>(L.P)->Struct!=L.Struct||L.Struct->GetStructureSize()!=L.StructSize)){return Stop(TEXT("data_asset_owned_read_struct_changed"),TEXT("Current native struct layout changed."));}
            if(L.Object&&L.Object->PropertyClass.Get()!=L.ObjectClass){return Stop(TEXT("data_asset_owned_read_object_class_changed"),TEXT("Current object property class changed."));}
            if(L.Enum){FEnumProperty* E=CastField<FEnumProperty>(L.P);if(!E||E->GetEnum()!=L.Enum||E->GetUnderlyingProperty()!=L.Underlying){return Stop(TEXT("data_asset_owned_read_enum_changed"),TEXT("Current enum declaration changed."));}}
        }
        for(const auto& Pair:PlayerSettingsMetadata){if(Pair.Value->GetClass()!=FObjectProperty::StaticClass()||Pair.Value->GetOwnerStruct()!=Pair.Key->GetClass()||!Pair.Value->PropertyClass.IsResolved()||Pair.Value->PropertyClass.Get()!=UObject::StaticClass())return Stop(TEXT("data_asset_player_settings_descriptor_changed"),TEXT("Metadata exact native descriptor/binding changed."));const auto Value=Pair.Value->GetObjectPtrPropertyValue_InContainer(Pair.Key);if(!Value.IsResolved()||Value.Get()!=nullptr)return Stop(TEXT("data_asset_player_settings_metadata_changed"),TEXT("Captured null Metadata changed or became unresolved; no next export/writer/save."));}
        if(!Bound){return true;}TArray<UObject*> Exports;GetObjectsWithOuter(Target->GetPackage(),Exports,EGetObjectsFlags::IncludeNestedObjects);if(Exports.Num()!=PackageExports.Num()+(Allocating?1:0)){return Stop(TEXT("data_asset_owned_read_package_changed"),TEXT("Selected asset package export set changed."));}for(UObject* O:Exports){if(!PackageExports.Contains(O)&&O!=Allocating){return Stop(TEXT("data_asset_owned_read_package_changed"),TEXT("Selected package export identity changed."));}}TArray<UObject*> Now;GetObjectsWithOuter(Target.Get(),Now,EGetObjectsFlags::IncludeNestedObjects);if(Now.Num()!=Owned.Num()+(Allocating?1:0)){return Stop(TEXT("data_asset_owned_read_set_changed"),TEXT("Owned object set changed across read."));}for(UObject* O:Now){if((!Owned.Contains(O)&&O!=Allocating)||!Loaded(O)||O->GetOuter()!=Target.Get()||Kind(O->GetClass())==INDEX_NONE||(O==Allocating&&(!AllocatingClass||O->GetClass()!=AllocatingClass||O->GetPackage()!=Target->GetPackage()||O->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_Transient)))){return Stop(TEXT("data_asset_owned_read_set_changed"),TEXT("Owned set/direct outer/native class changed."));}}
        TArray<FString> NowLinks;if(!MappingCopyInFlight&&(!ReadLinks(NowLinks)||NowLinks!=Links)){return Stop(TEXT("data_asset_owned_read_order_changed"),TEXT("Stored array order/duplicates/membership changed."));}
        for(const auto& S:CurveSlots){UObject* ActualCurve=nullptr;if(MappingSet||ActionSet){const auto CurveHandle=S.Property->GetObjectPtrPropertyValue_InContainer(S.Owner);if(!CurveHandle.IsResolved()){return Stop(TEXT("data_asset_response_curve_handle_unresolved"),TEXT("Captured response handle became unresolved; no resolution or next writer."));}ActualCurve=CurveHandle.Get();}else{ActualCurve=S.Property->GetObjectPropertyValue_InContainer(S.Owner);}if(ActualCurve!=S.Curve&&!(S.Owner==PendingCurveOwner&&S.Property==PendingCurveProperty&&ActualCurve==PendingCurveValue)){return Stop(TEXT("data_asset_owned_read_curve_changed"),TEXT("Stored response reference differs from old or the one exact planned new-object field."));}}
        for(const auto& C:Curves){if(C.ImportProperty->GetObjectPropertyValue_InContainer(C.Object)!=C.ImportData||C.Object->GetPackage()!=C.Package||C.Package->IsDirty()||!C.Package->IsFullyLoaded()||uint32(C.Package->GetPackageFlags())!=C.Flags){return Stop(TEXT("data_asset_owned_read_curve_source_changed"),TEXT("Saved-clean curve source changed."));}TArray<UObject*> X;GetObjectsWithOuter(C.Package,X,EGetObjectsFlags::IncludeNestedObjects);if(X.Num()!=C.Exports.Num()){return Stop(TEXT("data_asset_owned_read_curve_export_changed"),TEXT("Curve source export set changed."));}for(UObject* O:X){if(!C.Exports.Contains(O)){return Stop(TEXT("data_asset_owned_read_curve_export_changed"),TEXT("Curve source export identity changed."));}}}return true;
    }
    bool Snapshot(TSharedRef<FJsonObject>& J)
    {
        if(!Current()){return false;}auto Objects=MakeShared<FJsonObject>();J->SetObjectField(TEXT("owned_exports"),Objects);TArray<TSharedPtr<FJsonValue>> Order;for(const auto& S:Links){Order.Add(MakeShared<FJsonValueString>(S));}J->SetArrayField(TEXT("ordered_slots"),Order);
        for(UObject* O:Owned){auto V=Identity(O);auto F=MakeShared<FJsonObject>();V->SetObjectField(TEXT("fields"),F);Objects->SetObjectField(O->GetPathName(),V);if(!Fields(O,F,nullptr,nullptr,nullptr,nullptr,this)){return false;}}
        auto Source=MakeShared<FJsonObject>();J->SetObjectField(TEXT("native_sources"),Source);for(const auto& N:Native){auto V=Identity(N.Class);V->SetStringField(TEXT("class_flags"),LexToString(uint64(N.Flags)));V->SetNumberField(TEXT("package_flags"),N.PackageFlags);V->SetBoolField(TEXT("CDO_exists"),N.Default!=nullptr);Source->SetObjectField(N.Class->GetPathName(),V);if(N.Default){V->SetObjectField(TEXT("CDO_identity"),Identity(N.Default));auto F=MakeShared<FJsonObject>();V->SetObjectField(TEXT("CDO_fields"),F);if(!Fields(N.Default,F,nullptr,nullptr,nullptr,nullptr,this)){return false;}}}
        auto EnumSource=MakeShared<FJsonObject>();J->SetObjectField(TEXT("enum_sources"),EnumSource);for(const auto& L:Layouts){if(!L.Enum){continue;}UEnum* E=L.Enum;if(E->NumEnums()<1||E->NumEnums()>8){return false;}TArray<TSharedPtr<FJsonValue>> Values;for(int32 I=0;I<E->NumEnums();++I){auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("name"),E->GetNameByIndex(I).ToString());V->SetStringField(TEXT("value"),LexToString(E->GetValueByIndex(I)));V->SetStringField(TEXT("authored_name"),E->GetAuthoredNameStringByValue(E->GetValueByIndex(I)));Values.Add(MakeShared<FJsonValueObject>(V));}EnumSource->SetArrayField(E->GetPathName(),Values);}if(!Current()){return false;}
        auto Sources=MakeShared<FJsonObject>();J->SetObjectField(TEXT("curve_sources"),Sources);int64 Bytes=0;
        for(const auto& C:Curves){auto V=Identity(C.Object);auto F=MakeShared<FJsonObject>(),File=MakeShared<FJsonObject>(),Exports=MakeShared<FJsonObject>();V->SetObjectField(TEXT("fields"),F);V->SetObjectField(TEXT("disk"),File);V->SetObjectField(TEXT("exports"),Exports);V->SetNumberField(TEXT("package_flags"),C.Flags);V->SetBoolField(TEXT("package_dirty"),C.Package->IsDirty());Sources->SetObjectField(C.Object->GetPathName(),V);
            if(!Fields(C.Object,F,nullptr,nullptr,nullptr,nullptr,this)||!Disk(C.Package->GetName(),File)||!Current()||!File->GetBoolField(TEXT("exists"))||File->GetNumberField(TEXT("bytes"))<=0){return false;}Bytes+=int64(File->GetNumberField(TEXT("bytes")));if(Bytes>64*1024*1024){return false;}for(UObject* O:C.Exports){auto E=Identity(O);auto P=MakeShared<FJsonObject>();E->SetObjectField(TEXT("fields"),P);Exports->SetObjectField(O->GetPathName(),E);if(!Fields(O,P,nullptr,nullptr,nullptr,nullptr,this)){return false;}}}
        int32 N=0,Chars=0;return Current()&&Bounded(MakeShared<FJsonValueObject>(J),N,Chars)&&Clean(Text(J));
    }
    bool Bind(UDataAsset* A)
    {
        if(Bound){return Target.Get()==A&&Current();}if(!Context()||!Loaded(A)||A->GetOuter()!=A->GetPackage()||A->GetClass()!=FindObject<UClass>(nullptr,*A->GetClass()->GetPathName())||!Pin(A)||!Pin(A->GetPackage())||!Pin(A->GetClass())||!FieldsLayout(A->GetClass())){return Stop(TEXT("data_asset_owned_read_target_invalid"),TEXT("Exact already returned native owner and current layout required."));}Target.Reset(A);
        if(A->GetClass()->GetPathName()==InputActionClassPath){ActionTriggers=FindFProperty<FArrayProperty>(A->GetClass(),TEXT("Triggers"));ActionModifiers=FindFProperty<FArrayProperty>(A->GetClass(),TEXT("Modifiers"));ActionSettings=FindFProperty<FObjectProperty>(A->GetClass(),TEXT("PlayerMappableKeySettings"));if(!ActionTriggers||!ActionModifiers||!ActionSettings||!Layout(ActionSettings)){return Stop(TEXT("data_asset_owned_read_array_schema"),TEXT("Exact native Action arrays required."));}}
        else if(A->GetClass()->GetPathName()==MappingContextClassPath){Defaults=FindFProperty<FStructProperty>(A->GetClass(),TEXT("DefaultKeyMappings"));Rows=Defaults?FindFProperty<FArrayProperty>(Defaults->Struct,TEXT("Mappings")):nullptr;FStructProperty* Inner=Rows?CastField<FStructProperty>(Rows->Inner):nullptr;if(!Defaults||!Rows||!Inner||!Loaded(Inner->Struct)||!Pin(Inner->Struct)||!Layout(Rows)){return Stop(TEXT("data_asset_owned_read_row_schema"),TEXT("Actual native DefaultKeyMappings.Mappings layout required."));}for(TFieldIterator<FProperty> It(Inner->Struct);It;++It){if(!Layout(*It)){return false;}}RowTriggers=FindFProperty<FArrayProperty>(Inner->Struct,TEXT("Triggers"));RowModifiers=FindFProperty<FArrayProperty>(Inner->Struct,TEXT("Modifiers"));RowSettings=FindFProperty<FObjectProperty>(Inner->Struct,TEXT("PlayerMappableKeySettings"));if(!RowSettings||!Layout(RowSettings))return false;}
        else{return Stop(TEXT("data_asset_owned_read_owner_unsupported"),TEXT("Owned read supports exact native InputAction/InputMappingContext only."));}
        GetObjectsWithOuter(A,Owned,EGetObjectsFlags::IncludeNestedObjects);if(Owned.Num()>64){return Stop(TEXT("data_asset_owned_read_object_bound"),TEXT("At most64 direct owned input instances may be observed."));}for(UObject* O:Owned){if(!OwnedSchema(O)){return Stop(TEXT("data_asset_owned_read_instance_unsupported"),TEXT("Direct asset-owned exact DeadZone/Negate/ResponseCurveUser/Down/Released/Scalar/SwizzleAxis/Pressed fields and loaded saved-clean CurveFloat sources required."));}}
        if(!ReadLinks(Links)){return Stop(TEXT("data_asset_owned_read_slots_unsupported"),TEXT("Finite ordered modifier/trigger slots must reference direct owned instances of the matching supported family; null/foreign/custom entries remain unsupported."));}LinksFrozen=true;GetObjectsWithOuter(A->GetPackage(),PackageExports,EGetObjectsFlags::IncludeNestedObjects);if(PackageExports.Num()>80){return Stop(TEXT("data_asset_owned_read_package_bound"),TEXT("Finite selected owner/owned/native metadata export set required."));}for(UObject* O:PackageExports){if(!Loaded(O)||(O!=A&&!Owned.Contains(O)&&O->GetClass()->GetPathName()!=TEXT("/Script/CoreUObject.MetaData"))||!Pin(O)){return Stop(TEXT("data_asset_owned_read_foreign_export"),TEXT("Only selected owner, supported owned instances and native package metadata may share the package."));}}Bound=true;
        R.Data->SetObjectField(TEXT("owned_input_read_before"),Before);if(!Snapshot(Before)){return Stop(TEXT("data_asset_owned_read_baseline_incomplete"),TEXT("Complete bounded owned/native/curve read baseline required."));}
        R.Data->SetStringField(TEXT("owned_input_read_scope"),MappingSet?TEXT("Internal exact Context-set owned observer: current strong identities/native fields/curves/slots are preserved. Only this call's returned direct-owned allocation, exact planned field export and exact complete mapping slot copy are locally adopted; no caller snapshot/token or arbitrary class/in-place old object mutation authority. SavedTrue permits only unchanged or exact loaded flag transition on held target identities. Private trigger payload, global callbacks, Blueprint consumers, unlinked cold persistence and runtime input evaluation remain unverified."):ActionSet?TEXT("Internal exact InputAction owned lifecycle observer: only new finite literals receive field copies; old held identities are immutable and selected ordered arrays adopt only planned pointers. Nonselected fields/native sources/other array backing remain frozen; no caller token, cleanup/retry or future delegate/tick completion proof."):TEXT("Get-only exact eleven native owned classes; stored array order/duplicates, direct asset outer, linked and unlinked owned exports, reflected fields and loaded CurveFloat sources are observed. Null/distinct curve slots and partial authored states are read as stored. Trigger LastValue private payload and non-reflected curve import-info, input evaluation, delegates/tracking sets/deferred ticks, Blueprint consumers, writer recovery and cold/cook/runtime behavior remain unverified. InputAction scalar-only setter/Create keep empty inline arrays/no target owned exports; separate ActionSet admits the finite exact native modifier/trigger literal classes, and Context owned-set support remains separately bounded."));if(ReferenceClear){R.Data->SetStringField(TEXT("owned_input_read_scope"),R.Data->GetStringField(TEXT("owned_reference_clear_scope")));}R.Data->SetBoolField(TEXT("owned_input_private_runtime_state_verified"),false);return Current();
    }
    TSharedRef<FJsonObject> CopySnapshot(const TSharedRef<FJsonObject>& From)
    {TSharedPtr<FJsonObject> To;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text(From)),To);return To.IsValid()?To.ToSharedRef():MakeShared<FJsonObject>();}
    TSharedRef<FJsonObject> ComparisonBaseline()
    {
        if((!MappingSet&&!ActionSet)||!R.Saved){return Before;}auto Expected=CopySnapshot(Before);
        for(UObject* O:Owned){if(Expected->GetObjectField(TEXT("owned_exports"))->HasTypedField<EJson::Object>(O->GetPathName())){Expected->GetObjectField(TEXT("owned_exports"))->GetObjectField(O->GetPathName())->SetStringField(TEXT("object_flags"),LexToString(uint64(O->GetFlags())));}}return Expected;
    }
    bool PrepareWriteClasses(const TSharedRef<FJsonObject>& Requested,UDataAsset* ReturnedTarget)
    {
        if(!MappingSet||Bound||!Current()){return false;}
        for(const auto& Top:Requested->Values)
        {
            if(FName(*FString(Top.Key.ToView()))!=TEXT("DefaultKeyMappings")||Top.Value->Type!=EJson::Object||Top.Value->AsObject()->Values.Num()!=1){continue;}
            const TArray<TSharedPtr<FJsonValue>>* RequestedMappingRows=nullptr;for(const auto& W:Top.Value->AsObject()->Values){if(FName(*FString(W.Key.ToView()))==TEXT("Mappings")&&W.Value->Type==EJson::Array){RequestedMappingRows=&W.Value->AsArray();}}
            if(!RequestedMappingRows||RequestedMappingRows->Num()>64){continue;}bool Shape=true;TArray<TPair<FString,bool>> Classes;TArray<TSharedRef<FJsonObject>> ResponseLiterals;
            for(const auto& Row:*RequestedMappingRows)
            {
                if(Row->Type!=EJson::Object){Shape=false;break;}TSet<FName> Seen;bool HasAction=false,HasKey=false;
                for(const auto& Field:Row->AsObject()->Values)
                {
                    const FName N(*FString(Field.Key.ToView()));if(Seen.Contains(N)){Shape=false;break;}Seen.Add(N);HasAction|=N==TEXT("Action");HasKey|=N==TEXT("Key");
                    if(N==TEXT("Triggers")||N==TEXT("Modifiers"))
                    {if(Field.Value->Type!=EJson::Array||Field.Value->AsArray().Num()>64){Shape=false;break;}for(const auto& Slot:Field.Value->AsArray()){if(Slot->Type==EJson::Object){FString Path;if(Slot->AsObject()->TryGetStringField(TEXT("_ClassName"),Path)){Classes.Add({Path,N==TEXT("Triggers")});if(N==TEXT("Modifiers")&&Path==TEXT("/Script/EnhancedInput.InputModifierResponseCurveUser")){ResponseLiterals.Add(Slot->AsObject().ToSharedRef());}}}}}
                    else if(N!=TEXT("Action")&&N!=TEXT("Key")&&N!=TEXT("SettingBehavior")&&N!=TEXT("PlayerMappableKeySettings")){Shape=false;break;}
                }
                if(!Shape||!HasAction||!HasKey){Shape=false;break;}
            }
            // Unknown top-level/row payloads remain ordinary property-loop failures, not class/CDO authority.
            if(!Shape){continue;}
            if(Classes.ContainsByPredicate([&](const TPair<FString,bool>& Entry){return !Entry.Value&&MathModifierPath(Entry.Key);})&&!KeepMathModifierTarget(ReturnedTarget)){return false;}
            for(const auto& ResponseLiteral:ResponseLiterals){if(!PrepareCurveLiteral(ReturnedTarget,ResponseLiteral)){return false;}}
            for(const auto& Entry:Classes)
            {UClass* C=FindObject<UClass>(nullptr,*Entry.Key);const int32 K=Kind(C);if(WriterKind(K)&&TriggerKind(K)==Entry.Value&&Entry.Key.Equals(C->GetPathName(),ESearchCase::CaseSensitive)){UObject* D=C->GetDefaultObject(false);if(!Loaded(D)||!OwnedSchema(D,false)){return Stop(TEXT("data_asset_mapping_owned_native_unavailable"),TEXT("Exact loaded writer class/CDO/schema required only for declared mapping slots; no warming."));}}}
        }return Current();
    }
    // MS: only the already admitted, held direct-owned inventory is an identity authority.
    UObject* ReuseOwned(const FString& Export,bool Trigger)
    {
        if(!Check()){return nullptr;}
        for(UObject* O:Owned)
        {
            const int32 K=Kind(O->GetClass());if(K==INDEX_NONE||K==11||TriggerKind(K)!=Trigger){continue;}
            const FString Canonical=O->GetClass()->GetPathName()+TEXT("'")+O->GetPathName()+TEXT("'");
            if(Export.Equals(Canonical,ESearchCase::CaseSensitive)){return O;}
        }
        if(!R.Data->HasField(TEXT("owned_lifecycle_first_mismatch")))
        {auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("object"),Export);J->SetStringField(TEXT("property"),TEXT("owned_slot_reference"));J->SetStringField(TEXT("expected"),TEXT("exact class-export identity in the held same-target matching-family inventory"));J->SetStringField(TEXT("actual"),TEXT("no matching admitted identity"));R.Data->SetObjectField(TEXT("owned_lifecycle_first_mismatch"),J);}
        return nullptr;
    }
    bool ActionWriterKind(int32 K)const{return K>=0&&K<=10;}
    bool PrepareActionWriteClasses(const TSharedRef<FJsonObject>& Requested,UDataAsset* ReturnedTarget)
    {
        if(!ActionSet||Bound||!Current()){return false;}
        bool MathRequested=false;for(const auto& RequestedTop:Requested->Values)
        {if(FName(*FString(RequestedTop.Key.ToView()))!=TEXT("Modifiers")||RequestedTop.Value->Type!=EJson::Array||RequestedTop.Value->AsArray().Num()>64){continue;}for(const auto& RequestedSlot:RequestedTop.Value->AsArray()){if(RequestedSlot->Type!=EJson::Object){continue;}FString RequestedClass;if(RequestedSlot->AsObject()->TryGetStringField(TEXT("_ClassName"),RequestedClass)&&MathModifierPath(RequestedClass)){MathRequested=true;}}}
        if(MathRequested&&!KeepMathModifierTarget(ReturnedTarget)){return false;}
        // Hold the returned target and bind every requested curve before any mixed-slot class/CDO query.
        for(const auto& RequestedTop:Requested->Values)
        {
            if(FName(*FString(RequestedTop.Key.ToView()))!=TEXT("Modifiers")||RequestedTop.Value->Type!=EJson::Array||RequestedTop.Value->AsArray().Num()>64){continue;}
            for(const auto& RequestedSlot:RequestedTop.Value->AsArray())
            {if(RequestedSlot->Type==EJson::Object){FString ResponsePath;if(RequestedSlot->AsObject()->TryGetStringField(TEXT("_ClassName"),ResponsePath)&&ResponsePath.Equals(TEXT("/Script/EnhancedInput.InputModifierResponseCurveUser"),ESearchCase::CaseSensitive)&&!PrepareCurveLiteral(ReturnedTarget,RequestedSlot->AsObject().ToSharedRef())){return false;}}}
        }
        for(const auto& Top:Requested->Values)
        {
            const FName Field(*FString(Top.Key.ToView()));if((Field!=TEXT("Triggers")&&Field!=TEXT("Modifiers"))||Top.Value->Type!=EJson::Array||Top.Value->AsArray().Num()>64){continue;}
            for(const auto& Slot:Top.Value->AsArray())
            {
                if(Slot->Type!=EJson::Object){continue;}FString Path;if(!Slot->AsObject()->TryGetStringField(TEXT("_ClassName"),Path)){continue;}
                UClass* NativeClassObject=FindObject<UClass>(nullptr,*Path);const int32 K=Kind(NativeClassObject);
                if(ActionWriterKind(K)&&TriggerKind(K)==(Field==TEXT("Triggers"))&&Path.Equals(NativeClassObject->GetPathName(),ESearchCase::CaseSensitive))
                {UObject* D=NativeClassObject->GetDefaultObject(false);if(!Loaded(D)||!OwnedSchema(D,false)){return Stop(TEXT("data_asset_action_owned_native_unavailable"),TEXT("Exact loaded Action writer class/CDO/schema required only for declared array slots; no warming."));}}
            }
        }return Current();
    }
    bool AcceptActionCopy(FArrayProperty* Selected,const TArray<FString>& ExpectedLinks)
    {
        if(!ActionSet||!MappingCopyInFlight||!Current()||(Selected!=ActionTriggers&&Selected!=ActionModifiers)){return false;}
        const FString At=Selected==ActionTriggers?TEXT("Triggers"):TEXT("Modifiers");FScriptArrayHelper SelectedArray(Selected,Selected->ContainerPtrToValuePtr<void>(Target.Get()));
        // Only the selected array backing may be replaced; ReadLinks still checks the other backing.
        ArrayAddresses.Add(At,SelectedArray.Num()?SelectedArray.GetRawPtr(0):nullptr);TArray<FString> NowLinks;
        if(!ReadLinks(NowLinks)||NowLinks!=ExpectedLinks){return Stop(TEXT("data_asset_action_owned_copy_slots_changed"),TEXT("Selected ordered array or preserved other-array backing differs from exact planned pointers."));}
        Links=NowLinks;MappingCopyInFlight=false;auto Expected=CopySnapshot(Before),Now=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Slots;for(const auto& L:Links){Slots.Add(MakeShared<FJsonValueString>(L));}Expected->SetArrayField(TEXT("ordered_slots"),Slots);
        if(!Snapshot(Now)||!Equal(Expected,Now)){return Stop(TEXT("data_asset_action_owned_copy_state_changed"),TEXT("Array copy changed another held owned/native/curve field."));}Before=Now;return true;
    }
    bool AcceptAllocation(UObject* O,UClass* C)
    {
        Allocating=O;AllocatingClass=C;if((!MappingSet&&!ActionSet)||!O||!(ActionSet?ActionWriterKind(Kind(C)):WriterKind(Kind(C)))||!Pin(O)||!Check()){return Stop(TEXT("data_asset_mapping_owned_allocation_unknown"),TEXT("Allocation return or preserved preexisting state unavailable; prefix retained, no cleanup/retry."));}
        Owned.Add(O);PackageExports.Add(O);Allocating=nullptr;AllocatingClass=nullptr;
        if(!Pin(O)||!OwnedSchema(O)){return Stop(TEXT("data_asset_mapping_owned_allocation_schema"),TEXT("Allocation is not the exact direct context-owned native instance."));}
        if(Kind(C)==2&&!ResponseDefaults(O,C)){return false;}auto Now=MakeShared<FJsonObject>();if(!Snapshot(Now)){return Stop(TEXT("data_asset_mapping_owned_allocation_unobservable"),TEXT("Allocated prefix is held but endpoint unavailable."));}
        auto Expected=CopySnapshot(Before);Expected->GetObjectField(TEXT("owned_exports"))->SetObjectField(O->GetPathName(),Now->GetObjectField(TEXT("owned_exports"))->GetObjectField(O->GetPathName()));if(!Equal(Expected,Now)){return Stop(Kind(C)==2?TEXT("data_asset_response_curve_allocation_preservation_failed"):TEXT("data_asset_owned_allocation_preservation_failed"),TEXT("Only the new owned row may be added; existing owned/native/curve baseline was not readopted."));}Before=Expected;if(Kind(C)==2){NewResponseObjects.Add(O);}return Current();
    }
    bool AcceptField(UObject* O,FProperty* P,const FString& Canonical)
    {
        if((!MappingSet&&!ActionSet)||!Owned.Contains(O)||!Current()){return false;}auto Expected=CopySnapshot(Before),Now=MakeShared<FJsonObject>();
        const auto Exports=Expected->GetObjectField(TEXT("owned_exports"));if(!Exports->HasTypedField<EJson::Object>(O->GetPathName())){return false;}
        const auto Fields=Exports->GetObjectField(O->GetPathName())->GetObjectField(TEXT("fields"));if(!Fields->HasTypedField<EJson::Object>(P->GetName())){return false;}
        Fields->GetObjectField(P->GetName())->SetStringField(TEXT("export_text"),Canonical);
        if(!Snapshot(Now)||!Equal(Expected,Now)){return Stop(TEXT("data_asset_mapping_owned_field_changed"),TEXT("Actual finite field write or another frozen source/owner state differs; prefix retained."));}
        if(Kind(O->GetClass())==2){if(PendingCurveOwner!=O||PendingCurveProperty!=P){return Stop(TEXT("data_asset_response_curve_field_expectation_missing"),TEXT("Exact pre-copy axis expectation required."));}for(auto& Slot:CurveSlots){if(Slot.Owner==O&&Slot.Property==P){Slot.Curve=PendingCurveValue;}}PendingCurveOwner=nullptr;PendingCurveProperty=nullptr;PendingCurveValue=nullptr;Before=Expected;return Current();}Before=Now;return true;
    }
    bool AcceptMappingCopy(const TArray<FString>& ExpectedLinks)
    {
        if(!MappingSet||!MappingCopyInFlight||!Current()){return false;}LinksFrozen=false;ArrayAddresses.Reset();TArray<FString> NowLinks;
        if(!ReadLinks(NowLinks)||NowLinks!=ExpectedLinks){return Stop(TEXT("data_asset_mapping_owned_copy_slots_changed"),TEXT("Copied ordered slots differ from exact planned pointers."));}
        Links=NowLinks;LinksFrozen=true;MappingCopyInFlight=false;auto Expected=CopySnapshot(Before),Now=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Slots;for(const auto& L:Links){Slots.Add(MakeShared<FJsonValueString>(L));}Expected->SetArrayField(TEXT("ordered_slots"),Slots);
        if(!Snapshot(Now)||!Equal(Expected,Now)){return Stop(TEXT("data_asset_mapping_owned_copy_state_changed"),TEXT("Mapping copy changed another held owned/native/curve field."));}Before=Now;return true;
    }
    // LW: exact selected reference removal only; every old owned instance remains held and frozen.
    void DescribeReferenceClear()
    {
        R.Data->SetBoolField(TEXT("native_input_owned_reference_clear"),true);
        R.Data->SetBoolField(TEXT("owned_reference_clear_is_replacement"),false);
        R.Data->SetBoolField(TEXT("owned_reference_clear_destruction_requested"),false);
        R.Data->SetBoolField(TEXT("owned_reference_clear_rename_requested"),false);
        R.Data->SetBoolField(TEXT("owned_reference_clear_gc_requested"),false);
        R.Data->SetNumberField(TEXT("owned_reference_clear_calls"),0);
        R.Data->SetArrayField(TEXT("owned_reference_clear_prefix"),{});
        R.Data->SetStringField(TEXT("owned_reference_clear_scope"),TEXT("Explicit empty Action Modifiers/Triggers or Context DefaultKeyMappings.Mappings only. Removes selected array references; all old linked/unlinked owned objects remain strong-held/current, never renamed, destroyed or collected here. Existing property notifications, mixed failures and actual save receipts remain; no replacement/fresh-state recovery, unlinked cold lifetime or future input/tick proof."));
    }
    bool ClearReferences(FArrayProperty* Selected,FJsonObject& Attempt)
    {
        if(!ReferenceClear||!Check()||!Bound||!((ActionSet&&!MappingSet&&(Selected==ActionTriggers||Selected==ActionModifiers))||(MappingSet&&!ActionSet&&Selected==Rows)))
        {return Stop(TEXT("data_asset_owned_clear_scope_changed"),TEXT("Current exact selected finite reference-clear role required; no next writer/save/retry."));}
        const FString At=MappingSet?TEXT("Mappings"):Selected==ActionTriggers?TEXT("Triggers"):TEXT("Modifiers");
        TArray<FString> ExpectedLinks;int32 RemovedOwnedSlots=0;
        if(MappingSet){ExpectedLinks.Add(TEXT("rows#0"));for(const auto& L:Links){RemovedOwnedSlots+=L.Contains(TEXT("="))?1:0;}}
        else{for(const auto& L:Links){if(L.StartsWith(At+TEXT("#"))){ExpectedLinks.Add(At+TEXT("#0"));}else if(L.StartsWith(At+TEXT("/"))){++RemovedOwnedSlots;}else{ExpectedLinks.Add(L);}}}
        TArray<FString> RemovedPaths;
        for(const auto& L:Links)
        {
            const int32 Equals=L.Find(TEXT("="));if(Equals==INDEX_NONE||(!MappingSet&&!L.StartsWith(At+TEXT("/")))){continue;}
            const FString OldPath=L.Mid(Equals+1);const bool HeldOwned=Held.ContainsByPredicate([&](const FIdentity& I){return I.Path.Equals(OldPath,ESearchCase::CaseSensitive)&&Owned.Contains(I.Hold.Get());});
            if(!HeldOwned){return Stop(TEXT("data_asset_owned_clear_slot_identity_missing"),TEXT("Selected old slot has no exact held owned identity; no clear or retry."));}if(!RemovedPaths.ContainsByPredicate([&](const FString& ExistingPath){return ExistingPath.Equals(OldPath,ESearchCase::CaseSensitive);})){RemovedPaths.Add(OldPath);}
        }
        void* Container=MappingSet?Defaults->ContainerPtrToValuePtr<void>(Target.Get()):static_cast<void*>(Target.Get());
        FScriptArrayHelper Initial(Selected,Selected->ContainerPtrToValuePtr<void>(Container));const int32 BeforeCount=Initial.Num();
        auto Receipt=MakeShared<FJsonObject>();auto Prefix=R.Data->GetArrayField(TEXT("owned_reference_clear_prefix"));Prefix.Add(MakeShared<FJsonValueObject>(Receipt));R.Data->SetArrayField(TEXT("owned_reference_clear_prefix"),Prefix);
        Receipt->SetStringField(TEXT("property"),At);Receipt->SetStringField(TEXT("route"),TEXT("FScriptArrayHelper::EmptyValues"));Receipt->SetNumberField(TEXT("array_elements_before"),BeforeCount);Receipt->SetNumberField(TEXT("old_owned_objects_held"),Owned.Num());Receipt->SetNumberField(TEXT("planned_removed_owned_reference_slots"),RemovedOwnedSlots);
        Receipt->SetBoolField(TEXT("entered"),true);Receipt->SetBoolField(TEXT("returned"),false);Receipt->SetBoolField(TEXT("after_count_observed"),false);Receipt->SetBoolField(TEXT("old_owned_inventory_current_verified"),false);
        Attempt.SetBoolField(TEXT("direct_target_storage"),true);Attempt.SetStringField(TEXT("storage_write_route"),TEXT("FScriptArrayHelper::EmptyValues"));Attempt.SetBoolField(TEXT("reference_clear_entered"),true);
        MappingCopyInFlight=true;R.Known=false;Count(R,TEXT("owned_reference_clear_calls"));Initial.EmptyValues();
        // Plain return receipt precedes any broad/current observation; no raw storage is reused after that boundary.
        Receipt->SetBoolField(TEXT("returned"),true);Attempt.SetBoolField(TEXT("reference_clear_returned"),true);Attempt.SetBoolField(TEXT("storage_write_returned"),true);
        if(!Current()){return false;}
        Container=MappingSet?Defaults->ContainerPtrToValuePtr<void>(Target.Get()):static_cast<void*>(Target.Get());
        FScriptArrayHelper Actual(Selected,Selected->ContainerPtrToValuePtr<void>(Container));const int32 AfterCount=Actual.Num();Receipt->SetBoolField(TEXT("after_count_observed"),true);Receipt->SetNumberField(TEXT("array_elements_after"),AfterCount);
        if(AfterCount!=0){return Stop(TEXT("data_asset_owned_clear_nonempty_endpoint"),TEXT("Selected clear returned without an empty array; actual prefix retained."));}
        const bool Accepted=MappingSet?AcceptMappingCopy(ExpectedLinks):AcceptActionCopy(Selected,ExpectedLinks);
        if(!Accepted||!Check()){return false;}for(const auto& OldPath:RemovedPaths){if(!ClearedOwnedPaths.ContainsByPredicate([&](const FString& ExistingPath){return ExistingPath.Equals(OldPath,ESearchCase::CaseSensitive);})){ClearedOwnedPaths.Add(OldPath);}}Receipt->SetBoolField(TEXT("old_owned_inventory_current_verified"),true);Receipt->SetNumberField(TEXT("removed_array_elements"),BeforeCount);Receipt->SetNumberField(TEXT("removed_owned_reference_slots"),RemovedOwnedSlots);return true;
    }
    bool Check()
    {if(!Bound){return Current();}auto Now=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("owned_input_read_after"),Now);if(!Snapshot(Now)||!Equal(ComparisonBaseline(),Now)){return Stop(TEXT("data_asset_owned_read_preservation_failed"),TEXT("Owned instance/native/curve source state changed or became incomplete during read; no replay or implicit recovery."));}R.Data->SetBoolField(TEXT("owned_input_read_preserved"),true);return true;}
};
bool OwnedReadAdmit(FInputOwnedReadScope* Scope){return !Scope||Scope->Current();}
// JX: exact loaded InputAction only. Public reflection avoids an EnhancedInput
// link dependency; SDK tracking sets and deferred Editor ticks are not observed.
struct FInputActionScope
{
    FResult& R;FInputOwnedReadScope* OwnedRead=nullptr; bool Stopped=false, Started=false, Bound=false,RequireFactory=false; TWeakObjectPtr<UEditorEngine> Editor; TWeakObjectPtr<UWorld> World;
    struct FIdentity { TStrongObjectPtr<UObject> Hold; UClass* Class=nullptr; UObject* Outer=nullptr; UPackage* Package=nullptr; FString Path; };
    TArray<FIdentity> Held; TStrongObjectPtr<UClass> Class; TStrongObjectPtr<UDataAsset> Target; TStrongObjectPtr<UObject> Default;
    FArrayProperty* Triggers=nullptr; FArrayProperty* Modifiers=nullptr; FObjectPropertyBase* Settings=nullptr;
    TMap<FName,FProperty*> Scalars; TMap<FName,TArray<FString>> EnumTokens;
    TSharedRef<FJsonObject> SourceBefore=MakeShared<FJsonObject>(), FieldsBefore=MakeShared<FJsonObject>();
    FString TargetPath; bool CheckConsumers=false;
    struct FActionFieldPlan{FProperty* Property=nullptr;TUniquePtr<FDefaultConstructedPropertyElement> Value;FString Canonical;};
    struct FActionSlotPlan{UClass* Class=nullptr;UObject* Object=nullptr;TArray<FActionFieldPlan> Fields;};
    TArray<FActionSlotPlan> ActionPlan;TArray<UObject*> CreatedOwned;TArray<TSharedPtr<FJsonValue>> CreationPrefix;TSet<FArrayProperty*> WrittenArrays;TMap<FName,TArray<FString>> VerifiedArrayLinks;UDataAsset* FreshAction=nullptr;
    bool OwnedWriter()const{return OwnedRead&&OwnedRead->ActionSet;}
    explicit FInputActionScope(FResult& In,bool Factory=false,FInputOwnedReadScope* Read=nullptr):R(In),OwnedRead(Read),RequireFactory(Factory){}
    ~FInputActionScope(){if(Started){InputActionActive=false;}}
    bool Stop(const TCHAR* Code,const TCHAR* Message)
    {
        if(!Stopped){R.Stage=TEXT("verification");Fail(R,Code,Message,true);auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("code"),Code);R.Data->SetObjectField(TEXT("input_action_first_failure"),J);}
        Stopped=true;return false;
    }
    bool Context()
    {
        if(Stopped){return false;}
        if(!IsEditorGameThread()||!Loaded(GEditor)||GEditor!=Editor.Get()||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||IsPlaySessionActive()
            ||!Loaded(World.Get())||World->WorldType!=EWorldType::Editor||World->bIsTearingDown||GEditor->GetEditorWorldContext().World()!=World.Get())
        {return Stop(TEXT("data_asset_input_action_context_changed"),TEXT("The captured idle Editor/world changed; no further explicit writer/read/save or retry; mandatory transaction unwind may still run."));}return true;
    }
    bool Pin(UObject* O)
    {
        if(!O){return true;}if(!Context()||!Loaded(O)||!Loaded(O->GetClass())||!Loaded(O->GetPackage())||!Clean(O->GetPathName())){return Stop(TEXT("data_asset_input_action_identity_invalid"),TEXT("Current exact native/target identity required."));}
        for(const auto& I:Held){if(I.Hold.Get()==O){return true;}}if(Held.Num()>=128){return Stop(TEXT("data_asset_input_action_identity_budget"),TEXT("Finite InputAction identity budget exceeded."));}
        FIdentity I;I.Hold.Reset(O);I.Class=O->GetClass();I.Outer=O->GetOuter();I.Package=O->GetPackage();I.Path=O->GetPathName();Held.Add(MoveTemp(I));return true;
    }
    bool Source(TSharedRef<FJsonObject>& J)
    {
        if(!Sources(Class.Get(),RequireFactory?UDataAssetFactory::StaticClass():nullptr,J,true,nullptr,nullptr,this)){return false;}
        auto Schema=MakeShared<FJsonObject>();J->SetObjectField(TEXT("scalar_schema"),Schema);
        for(const auto& Pair:Scalars)
        {
            FProperty* P=Pair.Value;auto Row=MakeShared<FJsonObject>();Schema->SetObjectField(P->GetName(),Row);
            Row->SetStringField(TEXT("owner"),P->GetOwnerStruct()->GetPathName());Row->SetStringField(TEXT("field_class"),P->GetClass()->GetName());Row->SetStringField(TEXT("cpp_type"),P->GetCPPType());Row->SetStringField(TEXT("flags"),LexToString(uint64(P->GetPropertyFlags())));
            if(const FEnumProperty* E=CastField<FEnumProperty>(P))
            {
                UEnum* Enum=E->GetEnum();if(!Loaded(Enum)||Enum->NumEnums()<1||Enum->NumEnums()>8){return false;}TArray<TSharedPtr<FJsonValue>> Rows;
                for(int32 Index=0;Index<Enum->NumEnums();++Index){auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("name"),Enum->GetNameByIndex(Index).ToString());V->SetStringField(TEXT("value"),LexToString(Enum->GetValueByIndex(Index)));V->SetStringField(TEXT("authored_name"),Enum->GetAuthoredNameStringByValue(Enum->GetValueByIndex(Index)));Rows.Add(MakeShared<FJsonValueObject>(V));}Row->SetArrayField(TEXT("enum_values"),Rows);
            }
        }
        auto Protected=MakeShared<FJsonObject>();J->SetObjectField(TEXT("protected_schema"),Protected);
        for(FProperty* P:TArray<FProperty*>{Triggers,Modifiers,Settings})
        {
            auto Row=MakeShared<FJsonObject>();Protected->SetObjectField(P->GetName(),Row);Row->SetStringField(TEXT("field_class"),P->GetClass()->GetName());Row->SetStringField(TEXT("flags"),LexToString(uint64(P->GetPropertyFlags())));Row->SetNumberField(TEXT("array_dim"),P->ArrayDim);
            if(const FArrayProperty* A=CastField<FArrayProperty>(P)){const FObjectPropertyBase* Inner=CastField<FObjectPropertyBase>(A->Inner);Row->SetStringField(TEXT("inner_flags"),LexToString(uint64(A->Inner->GetPropertyFlags())));Row->SetStringField(TEXT("object_class"),Inner->PropertyClass->GetPathName());}
            else{Row->SetStringField(TEXT("object_class"),Settings->PropertyClass->GetPathName());}
        }return true;
    }
    bool Start()
    {
        if(InputActionActive){return Fail(R,TEXT("data_asset_input_action_reentry"),TEXT("An InputAction operation is active; no nested load or writer."));}
        if(!IsEditorGameThread()||!Loaded(GEditor)||IsPlaySessionActive()){return Fail(R,TEXT("data_asset_input_action_editor_required"),TEXT("Idle Editor required before InputAction work."));}
        Editor=GEditor;World=GEditor->GetEditorWorldContext().World();if(!Context()){return false;}
        if(!FModuleManager::Get().IsModuleLoaded(TEXT("EnhancedInput"))){return Fail(R,TEXT("data_asset_input_action_module_not_loaded"),TEXT("EnhancedInput must already be loaded; no module loading/registration change is requested."));}
        UClass* C=FindObject<UClass>(nullptr,InputActionClassPath);
        if(!Loaded(C)||C->GetClass()!=UClass::StaticClass()||C->ClassGeneratedBy||C->GetSuperClass()!=UDataAsset::StaticClass()||!C->HasAnyClassFlags(CLASS_Native)
            ||C->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)||C->IsEditorOnly()||C->GetPackage()->GetName()!=TEXT("/Script/EnhancedInput")
            ||!C->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||C->GetPackage()->HasAnyPackageFlags(PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly)||C->GetPackage()->IsDirty())
        {return Fail(R,TEXT("data_asset_input_action_source_not_loaded"),TEXT("Exact already-loaded native runtime InputAction class required; no module/class/CDO warming was requested."));}
        Class.Reset(C);UObject* D=C->GetDefaultObject(false);if(!Loaded(D)||D->GetClass()!=C||D->GetOuter()!=C->GetPackage()||!D->HasAnyFlags(RF_ClassDefaultObject))
        {return Fail(R,TEXT("data_asset_input_action_default_unavailable"),TEXT("Existing exact native InputAction CDO required; it is never created here."));}Default.Reset(D);
        InputActionActive=true;Started=true;
        if(!Pin(C)||!Pin(C->GetPackage())||!Pin(D)||!Pin(C->GetSuperClass())){return false;}
        if(RequireFactory&&(!Pin(UDataAssetFactory::StaticClass())||!Pin(UDataAssetFactory::StaticClass()->GetDefaultObject(false)))){return false;}
        for(const TCHAR* Name:{TEXT("bTriggerWhenPaused"),TEXT("bConsumeInput"),TEXT("bConsumesActionAndAxisMappings"),TEXT("bReserveAllMappings"),TEXT("ValueType"),TEXT("AccumulationBehavior")})
        {
            FProperty* P=C->FindPropertyByName(Name);if(!P||P->GetOwnerStruct()!=C||P->ArrayDim!=1||!P->HasAllPropertyFlags(CPF_Edit)||P->HasAnyPropertyFlags(CPF_EditConst|CPF_EditorOnly|CPF_Transient|CPF_Config|CPF_GlobalConfig|CPF_DisableEditOnInstance))
            {return Stop(TEXT("data_asset_input_action_schema_mismatch"),TEXT("Exact six public editable runtime scalar declarations required."));}
            if(P->GetFName()==TEXT("ValueType")||P->GetFName()==TEXT("AccumulationBehavior"))
            {
                const bool Value=P->GetFName()==TEXT("ValueType");FEnumProperty* E=CastField<FEnumProperty>(P);UEnum* Enum=E?E->GetEnum():nullptr;
                const FString Path=Value?TEXT("/Script/EnhancedInput.EInputActionValueType"):TEXT("/Script/EnhancedInput.EInputActionAccumulationBehavior");
                if(!Loaded(Enum)||Enum->GetPathName()!=Path||Enum->GetPackage()!=C->GetPackage()||!E->GetUnderlyingProperty()||!E->GetUnderlyingProperty()->IsA<FByteProperty>()||!Pin(Enum))
                {return Stop(TEXT("data_asset_input_action_enum_mismatch"),TEXT("Exact public uint8 native value/accumulation enum required."));}
                TArray<FString> Tokens=Value?TArray<FString>{TEXT("Boolean"),TEXT("Axis1D"),TEXT("Axis2D"),TEXT("Axis3D")}:TArray<FString>{TEXT("TakeHighestAbsoluteValue"),TEXT("Cumulative")};
                for(const auto& Token:Tokens){const int64 V=Enum->GetValueByNameString(Token);if(V==INDEX_NONE||!Enum->GetAuthoredNameStringByValue(V).Equals(Token,ESearchCase::CaseSensitive)){return Stop(TEXT("data_asset_input_action_enum_token_mismatch"),TEXT("Current authored enum tokens differ from installed SDK declarations."));}}EnumTokens.Add(P->GetFName(),MoveTemp(Tokens));
            }
            else if(!CastField<FBoolProperty>(P)||P->GetCPPType()!=TEXT("bool")){return Stop(TEXT("data_asset_input_action_schema_mismatch"),TEXT("Exact native bool field required."));}
            Scalars.Add(P->GetFName(),P);
        }
        Triggers=FindFProperty<FArrayProperty>(C,TEXT("Triggers"));Modifiers=FindFProperty<FArrayProperty>(C,TEXT("Modifiers"));Settings=FindFProperty<FObjectPropertyBase>(C,TEXT("PlayerMappableKeySettings"));
        if(!Triggers||!Modifiers||!Settings||Triggers->GetOwnerStruct()!=C||Modifiers->GetOwnerStruct()!=C||Settings->GetOwnerStruct()!=C){return Stop(TEXT("data_asset_input_action_instanced_schema_mismatch"),TEXT("Exact protected instanced declarations required."));}
        for(const auto& Pair:TArray<TPair<FArrayProperty*,FString>>{{Triggers,TEXT("/Script/EnhancedInput.InputTrigger")},{Modifiers,TEXT("/Script/EnhancedInput.InputModifier")}})
        {
            FObjectPropertyBase* Inner=CastField<FObjectPropertyBase>(Pair.Key->Inner);
            if(Pair.Key->ArrayDim!=1||!Pair.Key->HasAllPropertyFlags(CPF_Edit|CPF_BlueprintVisible)||!Inner||!Inner->HasAnyPropertyFlags(CPF_InstancedReference|CPF_ExportObject)||!Loaded(Inner->PropertyClass)||Inner->PropertyClass->GetPathName()!=Pair.Value||!Pin(Inner->PropertyClass))
            {return Stop(TEXT("data_asset_input_action_instanced_schema_mismatch"),TEXT("Exact native Trigger/Modifier array element classes required."));}
        }
        if(Settings->ArrayDim!=1||!Settings->HasAllPropertyFlags(CPF_Edit|CPF_BlueprintVisible|CPF_InstancedReference)||!Loaded(Settings->PropertyClass)||Settings->PropertyClass->GetPathName()!=TEXT("/Script/EnhancedInput.PlayerMappableKeySettings")||!Pin(Settings->PropertyClass))
        {return Stop(TEXT("data_asset_input_action_instanced_schema_mismatch"),TEXT("Exact native player-settings object property required."));}
        if(!Source(SourceBefore)){return Stop(TEXT("data_asset_input_action_source_unobservable"),TEXT("Bounded source/CDO/scalar/enum observations incomplete."));}
        R.Data->SetObjectField(TEXT("input_action_source_before"),SourceBefore);R.Data->SetStringField(TEXT("supported_class"),InputActionClassPath);
        R.Data->SetStringField(TEXT("admission_scope"),OwnedRead?TEXT("Exact native InputAction Get with eleven finite direct owned modifier/trigger classes and loaded CurveFloat source observations; null player settings. Generic setter/Create keep empty-array/no-owned-target restrictions."):TEXT("Exact already-loaded native InputAction lifecycle and six editable scalar fields; empty Triggers/Modifiers, null PlayerMappableKeySettings and no owned exports. Other classes, instanced inputs, mappings/keys and runtime evaluation remain incomplete."));
        R.Data->SetStringField(TEXT("description_cook_scope"),TEXT("InputAction scalar fields are native runtime asset payload; actual input processing, Blueprint consumer updates, deferred ticks, cook and vehicle runtime are unverified."));
        R.Data->SetBoolField(TEXT("runtime_input_action_verified"),false);R.Data->SetBoolField(TEXT("input_action_factory_source_required"),RequireFactory);
        R.Data->SetBoolField(TEXT("input_action_prior_runtime_capture_available"),false);R.Data->SetBoolField(TEXT("input_action_tracking_sets_observed"),false);R.Data->SetBoolField(TEXT("input_action_deferred_tick_verified"),false);
        R.Data->SetStringField(TEXT("input_action_callback_scope"),TEXT("SDK non-null scalar MemberProperty notifications: ValueType adds this action to a public static set for later Editor Tick; this exact override does not call Super or scalar Triggers/Modifiers delegates. Tracking sets/future ticks/new referencers and global purity are unobserved; no tracking-set reset or editor registration repair."));return Current();
    }
    bool Current()
    {
        if(!Context()||(OwnedRead&&!OwnedRead->Current())){return false;}for(const auto& I:Held){UObject* O=I.Hold.Get();if(!Loaded(O)||O->GetClass()!=I.Class||O->GetOuter()!=I.Outer||O->GetPackage()!=I.Package||O->GetPathName()!=I.Path||FindObject<UObject>(nullptr,*I.Path)!=O){return Stop(TEXT("data_asset_input_action_identity_changed"),TEXT("Held source/target identity changed; no adoption or further explicit work; mandatory transaction unwind is retained."));}}
        if(!FModuleManager::Get().IsModuleLoaded(TEXT("EnhancedInput"))||Class.Get()!=FindObject<UClass>(nullptr,InputActionClassPath)||Class->GetDefaultObject(false)!=Default.Get()||Class->GetPackage()->IsDirty()){return Stop(TEXT("data_asset_input_action_source_changed"),TEXT("Native class/default/source package changed."));}
        if(Class->GetClass()!=UClass::StaticClass()||Class->ClassGeneratedBy||Class->GetSuperClass()!=UDataAsset::StaticClass()||!Class->HasAnyClassFlags(CLASS_Native)||Class->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)||Class->IsEditorOnly())
        {return Stop(TEXT("data_asset_input_action_source_changed"),TEXT("Native class ownership/ancestry/admission changed."));}
        for(const auto& Pair:Scalars){if(Class->FindPropertyByName(Pair.Key)!=Pair.Value||Pair.Value->GetOwnerStruct()!=Class.Get()||Pair.Value->ArrayDim!=1){return Stop(TEXT("data_asset_input_action_property_changed"),TEXT("Current native scalar property identity/layout changed."));}}
        if(FindFProperty<FArrayProperty>(Class.Get(),TEXT("Triggers"))!=Triggers||FindFProperty<FArrayProperty>(Class.Get(),TEXT("Modifiers"))!=Modifiers||FindFProperty<FObjectPropertyBase>(Class.Get(),TEXT("PlayerMappableKeySettings"))!=Settings
            ||!Triggers||!Modifiers||!Settings||Triggers->GetOwnerStruct()!=Class.Get()||Modifiers->GetOwnerStruct()!=Class.Get()||Settings->GetOwnerStruct()!=Class.Get()
            ||Triggers->ArrayDim!=1||Modifiers->ArrayDim!=1||Settings->ArrayDim!=1)
        {return Stop(TEXT("data_asset_input_action_protected_layout_changed"),TEXT("Protected native property layout changed before storage observation."));}
        const FObjectPropertyBase* TInner=CastField<FObjectPropertyBase>(Triggers->Inner);const FObjectPropertyBase* MInner=CastField<FObjectPropertyBase>(Modifiers->Inner);
        if(!TInner||!MInner||!Loaded(TInner->PropertyClass)||!Loaded(MInner->PropertyClass)||!Loaded(Settings->PropertyClass)
            ||TInner->PropertyClass->GetPathName()!=TEXT("/Script/EnhancedInput.InputTrigger")||MInner->PropertyClass->GetPathName()!=TEXT("/Script/EnhancedInput.InputModifier")
            ||Settings->PropertyClass->GetPathName()!=TEXT("/Script/EnhancedInput.PlayerMappableKeySettings"))
        {return Stop(TEXT("data_asset_input_action_protected_layout_changed"),TEXT("Protected array/settings object classes changed."));}
        if(!Bound){return true;}UDataAsset* A=Target.Get();
        if(!Loaded(A)||A->GetClass()!=Class.Get()||A->GetOuter()!=A->GetPackage()||A->GetPathName()!=TargetPath||FindObject<UDataAsset>(nullptr,*TargetPath)!=A
            ||A->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject)||A->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn|PKG_PlayInEditor|PKG_EditorOnly)
            ||FindFProperty<FArrayProperty>(Class.Get(),TEXT("Triggers"))!=Triggers||FindFProperty<FArrayProperty>(Class.Get(),TEXT("Modifiers"))!=Modifiers||FindFProperty<FObjectPropertyBase>(Class.Get(),TEXT("PlayerMappableKeySettings"))!=Settings)
        {return Stop(TEXT("data_asset_input_action_target_changed"),TEXT("Current exact ordinary InputAction and protected property roles required."));}
        FScriptArrayHelper T(Triggers,Triggers->ContainerPtrToValuePtr<void>(A)),M(Modifiers,Modifiers->ContainerPtrToValuePtr<void>(A));
        const auto SettingsValue=Settings->GetObjectPtrPropertyValue_InContainer(A);if(!SettingsValue.IsResolved()||(!OwnedRead&&(T.Num()!=0||M.Num()!=0))||(SettingsValue.Get()!=nullptr&&(!OwnedRead||!OwnedRead->IsPlayerSettingsObject(SettingsValue.Get())))){return Stop(TEXT("data_asset_input_action_instanced_state_unsupported"),TEXT("Only empty Triggers/Modifiers and null PlayerMappableKeySettings are admitted; no inline/owned input authoring."));}
        TArray<UObject*> Owned;GetObjectsWithOuter(A,Owned,EGetObjectsFlags::IncludeNestedObjects);if((!OwnedRead&&!Owned.IsEmpty())||(OwnedRead&&!OwnedRead->Current())){return Stop(TEXT("data_asset_input_action_owned_export_unsupported"),TEXT("InputAction owned exports must remain absent."));}
        TArray<UObject*> PackageExports;GetObjectsWithOuter(A->GetPackage(),PackageExports,EGetObjectsFlags::IncludeNestedObjects);
        if(PackageExports.Num()>(OwnedRead?80:16)){return Stop(TEXT("data_asset_input_action_package_export_budget"),TEXT("Finite selected action/package-metadata export set required."));}
        for(UObject* O:PackageExports)
        {
            if(!Loaded(O)){return Stop(TEXT("data_asset_input_action_foreign_package_export"),TEXT("A foreign package export appeared; no package save."));}
            const bool PendingActionOwned=OwnedWriter()&&OwnedRead->Bound&&OwnedRead->Target.Get()==A&&O==OwnedRead->Allocating&&CreatedOwned.Contains(O)
                &&OwnedRead->ActionWriterKind(OwnedRead->Kind(OwnedRead->AllocatingClass))&&O->GetClass()==OwnedRead->AllocatingClass&&O->GetOuter()==A&&O->GetPackage()==A->GetPackage()
                &&!O->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_Transient)&&OwnedRead->Held.ContainsByPredicate([O,A](const FInputOwnedReadScope::FIdentity& I)
                {return I.Hold.Get()==O&&I.Class==O->GetClass()&&I.Outer==A&&I.Package==A->GetPackage()&&I.Path==O->GetPathName()&&I.Flags==O->GetFlags();});
            if(O!=A&&!(OwnedRead&&OwnedRead->Owned.Contains(O))&&!PendingActionOwned&&O->GetClass()->GetPathName()!=TEXT("/Script/CoreUObject.MetaData")){return Stop(TEXT("data_asset_input_action_foreign_package_export"),TEXT("A foreign package export appeared; no package save."));}
        }
        return true;
    }
    bool Consumers()
    {
        UDataAsset* A=Target.Get();
        UClass* EventClass=FindObject<UClass>(nullptr,TEXT("/Script/InputBlueprintNodes.K2Node_EnhancedInputAction"));
        UClass* ValueClass=FindObject<UClass>(nullptr,TEXT("/Script/InputBlueprintNodes.K2Node_GetInputActionValue"));
            int32 Counted=0;for(TObjectIterator<UObject> It;It;++It)
            {
                UObject* O=*It;if(++Counted>131072){return Stop(TEXT("data_asset_input_action_referencer_budget"),TEXT("Loaded node referencer observation exceeds its finite budget."));}
                if(!Loaded(O)||!Loaded(O->GetClass())){continue;}
                UClass* Base=Loaded(EventClass)&&O->IsA(EventClass)?EventClass:Loaded(ValueClass)&&O->IsA(ValueClass)?ValueClass:nullptr;if(!Base){continue;}
                FObjectPropertyBase* P=FindFProperty<FObjectPropertyBase>(Base,TEXT("InputAction"));if(!P||P->GetOwnerStruct()!=Base||P->PropertyClass.Get()!=Class.Get()){return Stop(TEXT("data_asset_input_action_referencer_unobservable"),TEXT("Current stock InputAction node referencer is unobservable."));}
                if(P->GetObjectPropertyValue_InContainer(O)==A){return Stop(TEXT("data_asset_input_action_loaded_node_referencer"),TEXT("ValueType/Triggers notification could reconstruct a loaded Blueprint node on a later tick; disconnect that owned reference separately before this bounded setter."));}
            }
        R.Data->SetBoolField(TEXT("input_action_loaded_node_referencers_absent"),true);
        R.Data->SetNumberField(TEXT("input_action_referencer_objects_observed"),Counted);
        return true;
    }
    bool Check(FProperty* Mutable=nullptr)
    {
        if(!Current()||(OwnedWriter()&&!OwnedRead->Check())||(Bound&&CheckConsumers&&!Consumers())){return false;}auto SourceNow=MakeShared<FJsonObject>();const bool Observed=Source(SourceNow);R.Data->SetObjectField(TEXT("input_action_source_after"),SourceNow);if(!Observed||!Equal(SourceBefore,SourceNow)){return Stop(TEXT("data_asset_input_action_source_preservation_failed"),TEXT("Current reflected native source/CDO/enum state differs."));}
        if(!Bound){return true;}auto Now=MakeShared<FJsonObject>();if(!Fields(Target.Get(),Now,nullptr,nullptr,this)||!Current()||Now->Values.Num()!=FieldsBefore->Values.Num()){return Stop(TEXT("data_asset_input_action_fields_unobservable"),TEXT("Complete current InputAction field observation required."));}
        for(const auto& Pair:FieldsBefore->Values){if(!Mutable||FName(*FString(Pair.Key.ToView()))!=Mutable->GetFName()){if(!Equal(Pair.Value,Now->TryGetField(Pair.Key.ToView()))){return Stop(TEXT("data_asset_input_action_unselected_field_changed"),TEXT("A nonselected field changed across the scalar/notification boundary."));}}}
        if(Mutable){FieldsBefore->SetField(Mutable->GetName(),Now->TryGetField(Mutable->GetName()));}return true;
    }
    bool FreshTarget(UDataAsset* A)
    {
        if(!OwnedWriter()){return true;}if(Bound){return A==Target.Get()&&Current();}
        if(!Current()||!Loaded(A)||A->GetClass()!=Class.Get()||A->GetOuter()!=A->GetPackage()||!Pin(A)||!Pin(A->GetPackage())){return Stop(TEXT("data_asset_action_owned_target_unavailable"),TEXT("Current exact native Action identity/layout required before fresh-array admission."));}
        if(OwnedRead->ReferenceClear){return Current();} // LW: Bind/observer preserves the existing inventory; no fresh-authoring authority.
        if(Settings->GetObjectPropertyValue_InContainer(A)!=nullptr&&!OwnedRead->PlayerSettingsHost)
        {return Fail(R,TEXT("data_asset_action_owned_settings_unsupported"),TEXT("Null PlayerMappableKeySettings required; populated arrays are admitted only by the following complete owned inventory Bind."));}
        FreshAction=A;return Current();
    }
    bool PlanArray(FArrayProperty* Selected,const TSharedPtr<FJsonValue>& Value,FString& Error)
    {
        ActionPlan.Reset();if(!OwnedWriter()||FreshAction!=Target.Get()||!Check()){return false;}
        if((Selected!=Triggers&&Selected!=Modifiers)||WrittenArrays.Contains(Selected)||!Value||Value->Type!=EJson::Array||Value->AsArray().Num()>64)
        {Error=TEXT("At most64 slots; each known array may be written once in this call");return false;}
        auto CurrentJson=MakeShared<FJsonObject>();Count(R,TEXT("owned_matching_json_export_calls"));const bool ExportedCurrent=FJsonObjectConverter::UStructToJsonObject(Class.Get(),Target.Get(),CurrentJson,0,0);if(!Check()){return false;}
        if(!ExportedCurrent){Error=TEXT("Current canonical Action JSON unavailable before planning");return false;}
        const TCHAR* JsonKey=Selected==Triggers?TEXT("triggers"):TEXT("modifiers");const bool Matching=Equal(Value,CurrentJson->TryGetField(JsonKey));
        FObjectPropertyBase* Inner=CastField<FObjectPropertyBase>(Selected->Inner);if(!Inner){return false;}
        int32 SlotIndex=0,NewCount=0;
        for(const auto& V:Value->AsArray())
        {
            if(Matching)
            {if(!Check()){return false;}FScriptArrayHelper Existing(Selected,Selected->ContainerPtrToValuePtr<void>(Target.Get()));if(SlotIndex>=Existing.Num()){Error=TEXT("Unchanged Action slot count mismatch");return false;}FActionSlotPlan Plan;Plan.Object=Inner->GetObjectPropertyValue(Existing.GetRawPtr(SlotIndex++));if(!Check()){return false;}if(!Loaded(Plan.Object)||!OwnedRead->Owned.Contains(Plan.Object)||OwnedRead->TriggerKind(OwnedRead->Kind(Plan.Object->GetClass()))!=(Selected==Triggers)){Error=TEXT("Unchanged Action must retain exact admitted slot identities");return false;}Plan.Class=Plan.Object->GetClass();ActionPlan.Add(MoveTemp(Plan));continue;}
            ++SlotIndex;
            if(V->Type==EJson::String)
            {FActionSlotPlan Plan;Plan.Object=OwnedRead->ReuseOwned(V->AsString(),Selected==Triggers);if(!Check()){return false;}if(!Plan.Object){Error=TEXT("Exact held same-target class-export reference required; no foreign/plain/forward path");return false;}Plan.Class=Plan.Object->GetClass();ActionPlan.Add(MoveTemp(Plan));continue;}
            if(V->Type!=EJson::Object){Error=TEXT("Complete native literal or exact held same-target class-export required; null/foreign/custom entries unsupported");return false;}
            if(OwnedRead->Owned.Num()+(++NewCount)>64){Error=TEXT("Old held objects plus planned new literals exceed64; no allocation or cleanup");return false;}
            const auto Literal=V->AsObject();FString Path;if(!Literal->TryGetStringField(TEXT("_ClassName"),Path)){Error=TEXT("Exact _ClassName required");return false;}
            UClass* NativeClassObject=FindObject<UClass>(nullptr,*Path);const int32 K=OwnedRead->Kind(NativeClassObject);
            if(!OwnedRead->ActionWriterKind(K)||OwnedRead->TriggerKind(K)!=(Selected==Triggers)||!Path.Equals(NativeClassObject->GetPathName(),ESearchCase::CaseSensitive)){Error=TEXT("Only exact DeadZone/Negate/ResponseCurveUser/Scalar/SwizzleAxis/ResponseCurveExponential/FOVScaling/ScaleByDeltaTime modifiers or Down/Released/Pressed triggers are writable");return false;}
            UObject* D=NativeClassObject->GetDefaultObject(false);auto Defaults=MakeShared<FJsonObject>();if(!Loaded(D)){Error=TEXT("Existing native CDO required");return false;}
            const bool Exported=FJsonObjectConverter::UStructToJsonObject(NativeClassObject,D,Defaults,0,0);if(!Check()){return false;}
            if(!Exported||Literal->Values.Num()!=Defaults->Values.Num()+1){Error=TEXT("Complete canonical lowerCamel owned fields required");return false;}FActionSlotPlan Plan;Plan.Class=NativeClassObject;
            for(const auto& Field:Defaults->Values)
            {
                const FString Name(Field.Key.ToView());const auto Supplied=Literal->TryGetField(Name);FProperty* P=NativeClassObject->FindPropertyByName(FName(*Name));if(!Supplied||!P){Error=TEXT("Exact canonical native field keys required");return false;}
                if(P->GetFName()==TEXT("LastValue")){if(Supplied->Type!=EJson::Object||!Supplied->AsObject()->Values.IsEmpty()||!Equal(Supplied,Field.Value)){Error=TEXT("Retain reflected empty LastValue; private runtime payload is not written or verified");return false;}continue;}
                if(P->GetFName()==TEXT("bShouldAlwaysTick")){if(Supplied->Type!=EJson::Boolean||Supplied->AsBool()||Field.Value->Type!=EJson::Boolean||Field.Value->AsBool()){Error=TEXT("Retain exact readonly native false bookkeeping default");return false;}continue;}
                if(K==2&&Supplied->Type==EJson::Null){continue;}UObject* PlannedCurveObject=nullptr;
                bool Allowed=false;if(K==2){auto* ResponseProperty=CastField<FObjectPropertyBase>(P);Allowed=ResponseProperty&&ResponseProperty->PropertyClass.Get()==FindObject<UClass>(nullptr,TEXT("/Script/Engine.CurveFloat"))&&OwnedRead->CurveReference(Supplied,PlannedCurveObject,false);}
                else if(P->IsA<FFloatProperty>()){Allowed=Supplied->Type==EJson::Number&&FMath::IsFinite(Supplied->AsNumber())&&FMath::Abs(Supplied->AsNumber())<=double(MAX_flt);}
                else if(P->IsA<FBoolProperty>()){Allowed=Supplied->Type==EJson::Boolean;}
                else if(FEnumProperty* E=CastField<FEnumProperty>(P)){if((K==0||K==6||K==9)&&Supplied->Type==EJson::String){for(int32 EnumIndex=0;EnumIndex<E->GetEnum()->NumEnums();++EnumIndex){const FString Token=E->GetEnum()->GetAuthoredNameStringByValue(E->GetEnum()->GetValueByIndex(EnumIndex));const bool Known=K==0?(Token==TEXT("Axial")||Token==TEXT("Radial")):K==9?(Token==TEXT("Standard")||Token==TEXT("UE4_BackCompat")):(Token==TEXT("YXZ")||Token==TEXT("ZYX")||Token==TEXT("XZY")||Token==TEXT("YZX")||Token==TEXT("ZXY"));Allowed|=Known&&Supplied->AsString().Equals(Token,ESearchCase::CaseSensitive);}}}
                else if(FStructProperty* VectorField=CastField<FStructProperty>(P)){if((K==5||K==8)&&VectorField->Struct==TBaseStructure<FVector>::Get()&&Supplied->Type==EJson::Object&&Supplied->AsObject()->Values.Num()==3){Allowed=true;for(const TCHAR* Axis:{TEXT("x"),TEXT("y"),TEXT("z")}){const auto AxisValue=Supplied->AsObject()->TryGetField(Axis);Allowed&=AxisValue&&AxisValue->Type==EJson::Number&&FMath::IsFinite(AxisValue->AsNumber());}}}
                if(!Allowed||!P->HasAnyPropertyFlags(CPF_Edit)){Error=TEXT("Finite declared writable scalar/vector/authored-enum field required");return false;}
                FActionFieldPlan FieldPlan;FieldPlan.Property=P;FieldPlan.Value=MakeUnique<FDefaultConstructedPropertyElement>(P);Count(R,TEXT("owned_field_plan_conversion_calls"));
                bool Converted=false;if(K==2){auto* ResponseProperty=CastField<FObjectPropertyBase>(P);ResponseProperty->SetObjectPropertyValue(FieldPlan.Value->GetObjAddress(),PlannedCurveObject);Converted=ResponseProperty->GetObjectPropertyValue(FieldPlan.Value->GetObjAddress())==PlannedCurveObject;}else{Converted=FJsonObjectConverter::JsonValueToUProperty(Supplied,P,FieldPlan.Value->GetObjAddress(),0,0);}if(!Check()){return false;}
                if(!Converted){Error=TEXT("Owned scalar conversion failed before allocation");return false;}const void* Address=FieldPlan.Value->GetObjAddress();const bool ExportedField=P->ExportText_Direct(FieldPlan.Canonical,Address,Address,Target.Get(),PPF_None);if(!Check()){return false;}
                if(!ExportedField||FieldPlan.Canonical.Len()>65536){Error=TEXT("Finite canonical field export unavailable before allocation");return false;}Plan.Fields.Add(MoveTemp(FieldPlan));
            }ActionPlan.Add(MoveTemp(Plan));
        }return Check();
    }
    bool ConfigureArray(FArrayProperty* Selected,FJsonObject& Attempt)
    {
        for(int32 SlotIndex=0;SlotIndex<ActionPlan.Num();++SlotIndex)
        {
            if(!Check()){return false;}auto& Plan=ActionPlan[SlotIndex];if(Plan.Object){continue;}auto Receipt=MakeShared<FJsonObject>();CreationPrefix.Add(MakeShared<FJsonValueObject>(Receipt));R.Data->SetArrayField(TEXT("input_action_owned_creation_prefix"),CreationPrefix);
            Receipt->SetStringField(TEXT("property"),Selected->GetName());Receipt->SetStringField(TEXT("class"),Plan.Class->GetPathName());Receipt->SetNumberField(TEXT("slot"),SlotIndex);Receipt->SetBoolField(TEXT("allocation_entered"),true);
            R.Known=false;Count(R,TEXT("owned_object_allocation_calls"));UObject* O=NewObject<UObject>(Target.Get(),Plan.Class,NAME_None,RF_Transactional);Receipt->SetBoolField(TEXT("allocation_returned"),true);Receipt->SetBoolField(TEXT("returned_object_is_null"),O==nullptr);Receipt->SetBoolField(TEXT("returned_identity_observed"),false);
            OwnedRead->Allocating=O;OwnedRead->AllocatingClass=Plan.Class;if(!O||!OwnedRead->Pin(O)){return Stop(TEXT("data_asset_action_owned_allocation_unknown"),TEXT("Allocation identity unavailable; return bool/prior observed prefix retained, no next explicit writer/save."));}
            Plan.Object=O;CreatedOwned.Add(O);Receipt->SetObjectField(TEXT("identity"),Identity(O));Receipt->SetBoolField(TEXT("returned_identity_observed"),true);R.Data->SetNumberField(TEXT("owned_objects_created_count"),CreatedOwned.Num());
            if(!Current()||!Check()||!OwnedRead->AcceptAllocation(O,Plan.Class)){return Stop(TEXT("data_asset_action_owned_allocation_unknown"),TEXT("Complete allocation endpoint unavailable; actual returned identity/prefix retained."));}
            if(OwnedRead->TriggerKind(OwnedRead->Kind(Plan.Class))){FBoolProperty* Tick=FindFProperty<FBoolProperty>(Plan.Class,TEXT("bShouldAlwaysTick"));if(!Check()||!Tick||Tick->GetPropertyValue_InContainer(O)){return Stop(TEXT("data_asset_action_owned_trigger_default_changed"),TEXT("Returned trigger bookkeeping differs from planned native false default."));}}
            TArray<TSharedPtr<FJsonValue>> Fields;
            for(const auto& F:Plan.Fields)
            {
                if(!Check()||!OwnedRead->BeginCurveField(O,F.Property,F.Value->GetObjAddress())){return false;}auto Written=MakeShared<FJsonObject>();Fields.Add(MakeShared<FJsonValueObject>(Written));Receipt->SetArrayField(TEXT("field_copy_prefix"),Fields);Written->SetStringField(TEXT("field"),F.Property->GetName());Written->SetStringField(TEXT("canonical"),F.Canonical);Written->SetBoolField(TEXT("entered"),true);Count(R,TEXT("owned_field_storage_copy_calls"));
                F.Property->CopyCompleteValue(F.Property->ContainerPtrToValuePtr<void>(O),F.Value->GetObjAddress());Written->SetBoolField(TEXT("returned"),true);if(!OwnedRead->AcceptField(O,F.Property,F.Canonical)||!Check()){return Stop(TEXT("data_asset_action_owned_field_unknown"),TEXT("Actual field-copy prefix unavailable or another frozen field changed; no next explicit writer/save."));}
            }Receipt->SetBoolField(TEXT("configured"),true);
        }
        Attempt.SetNumberField(TEXT("owned_objects_created"),CreatedOwned.Num());return Check();
    }
    bool ScratchArray(FArrayProperty* Selected,void* Scratch,TArray<FString>& ExpectedLinks)
    {
        if(!Check()){return false;}FObjectPropertyBase* Inner=CastField<FObjectPropertyBase>(Selected->Inner);if(!Inner){return false;}FScriptArrayHelper Slots(Selected,Scratch);Slots.AddValues(ActionPlan.Num());
        for(int32 SlotIndex=0;SlotIndex<ActionPlan.Num();++SlotIndex){if(!Check()){return false;}Inner->SetObjectPropertyValue(Slots.GetRawPtr(SlotIndex),ActionPlan[SlotIndex].Object);}
        const FString At=Selected==Triggers?TEXT("Triggers"):TEXT("Modifiers");for(const auto& L:OwnedRead->Links)
        {if(L.StartsWith(At+TEXT("#"))){ExpectedLinks.Add(At+TEXT("#")+LexToString(ActionPlan.Num()));for(int32 SlotIndex=0;SlotIndex<ActionPlan.Num();++SlotIndex){ExpectedLinks.Add(At+TEXT("/")+LexToString(SlotIndex)+TEXT("=")+ActionPlan[SlotIndex].Object->GetPathName());}}else if(!L.StartsWith(At+TEXT("/"))){ExpectedLinks.Add(L);}}
        return Check();
    }
    bool Bind(UDataAsset* A,const FString& Path)
    {
        if(!Check()||!Loaded(A)||A->GetClass()!=Class.Get()||A->GetPathName()!=Path||A->GetOuter()!=A->GetPackage()){return Stop(TEXT("data_asset_input_action_target_invalid"),TEXT("Exact returned InputAction target required."));}
        if(Bound){return (A==Target.Get()&&Current())||Stop(TEXT("data_asset_input_action_target_replaced"),TEXT("Original asset relookup returned a different current action; no adoption or retry."));}Target.Reset(A);TargetPath=Path;Bound=true;if(!Pin(A)||!Pin(A->GetPackage())||!Current()||!Fields(A,FieldsBefore,nullptr,nullptr,this)){return Stop(TEXT("data_asset_input_action_target_unobservable"),TEXT("Current action field baseline is incomplete; no next explicit writer or save."));}
        TArray<UObject*> Exports;GetObjectsWithOuter(A->GetPackage(),Exports,EGetObjectsFlags::IncludeNestedObjects);for(UObject* O:Exports){if(!Loaded(O)||(O!=A&&!(OwnedRead&&OwnedRead->Owned.Contains(O))&&O->GetClass()->GetPathName()!=TEXT("/Script/CoreUObject.MetaData"))||!Pin(O)){return Stop(TEXT("data_asset_input_action_foreign_package_export"),TEXT("Only the exact selected action and native package metadata may share its save package."));}}return Check();
    }
};
bool InputAdmit(FInputActionScope* Scope){return !Scope||Scope->Current();}
// JY: exact native Context and its effective default mapping payload. No MapKey/rebuild/config writer.
struct FMappingContextScope
{
    FResult& R;FInputOwnedReadScope* OwnedRead=nullptr;bool Started=false,Stopped=false,Bound=false,RequireFactory=false,Ready=false;TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;
    TStrongObjectPtr<UClass> Class,ActionClass;TStrongObjectPtr<UObject> Default;TStrongObjectPtr<UDataAsset> Target;FString TargetPath;
    struct FIdentity{TStrongObjectPtr<UObject> Hold;UClass* Class=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;FString Path;};TArray<FIdentity> Held;
    struct FLayout{FProperty* Property=nullptr;UStruct* Owner=nullptr;FName Name;EPropertyFlags Flags=CPF_None;int32 Offset=0,Size=0;};TArray<FLayout> Layouts;TMap<FProperty*,TPair<uint64,int32>> Nested;UEnum* SettingEnum=nullptr;FNumericProperty* BehaviorNumeric=nullptr;
    struct FActionSource{UDataAsset* Object=nullptr;FString Path,Package;TSharedRef<FJsonObject> Before=MakeShared<FJsonObject>();};TArray<FActionSource> Actions;
    UScriptStruct* DataStruct=nullptr;UScriptStruct* RowStruct=nullptr;UScriptStruct* KeyStruct=nullptr;int32 DataSize=0,RowSize=0;FProperty* RowsInner=nullptr;FProperty* LegacyInner=nullptr;FProperty* ProfilesKey=nullptr;FProperty* ProfilesValue=nullptr;FProperty* TriggersInner=nullptr;FProperty* ModifiersInner=nullptr;FStructProperty* Defaults=nullptr;FArrayProperty* Rows=nullptr;FArrayProperty* Legacy=nullptr;FMapProperty* Profiles=nullptr;
    FObjectPropertyBase* Action=nullptr;FStructProperty* Key=nullptr;FArrayProperty* Triggers=nullptr;FArrayProperty* Modifiers=nullptr;FEnumProperty* Behavior=nullptr;FObjectPropertyBase* Settings=nullptr;FBoolProperty* Ignored=nullptr;FBoolProperty* Ticking=nullptr;
    TSharedRef<FJsonObject> SourceBefore=MakeShared<FJsonObject>(),FieldsBefore=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> RowsBefore;
    struct FOwnedFieldPlan{FProperty* Property=nullptr;TUniquePtr<FDefaultConstructedPropertyElement> Value;FString Canonical;};
    struct FOwnedSlotPlan{UClass* Class=nullptr;UObject* Object=nullptr;int32 Row=0,Slot=0;bool Trigger=false;TArray<FOwnedFieldPlan> Fields;};
    TArray<FOwnedSlotPlan> OwnedPlan;TArray<UObject*> CreatedOwned;TSharedPtr<FJsonValue> PlainMapping;TArray<FString> VerifiedMappingLinks;bool MatchingOwned=false,MappingRequestVerified=false;
    bool OwnedWriter()const{return OwnedRead&&OwnedRead->MappingSet;}
    FString ObservationPhase;TSharedPtr<FJsonObject> ExportTrace;TSharedRef<FJsonObject> SourceDiagnostic=MakeShared<FJsonObject>();
    void BeginPhase(const TCHAR* Phase)
    {ObservationPhase=Phase;ExportTrace.Reset();auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("phase"),ObservationPhase);J->SetBoolField(TEXT("returned"),false);SourceDiagnostic->SetObjectField(TEXT("current_phase"),J);R.Data->SetObjectField(TEXT("input_mapping_source_diagnostic"),SourceDiagnostic);}
    void ObservationFailed(const TCHAR* Reason)
    {
        if(R.Data->HasField(TEXT("input_mapping_observation_first_failure"))){return;}auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("phase"),ObservationPhase);J->SetStringField(TEXT("reason"),Reason);if(ExportTrace.IsValid()){J->SetObjectField(TEXT("field"),ExportTrace.ToSharedRef());}R.Data->SetObjectField(TEXT("input_mapping_observation_first_failure"),J);
    }
    bool EndPhase(bool OK)
    {const auto J=SourceDiagnostic->GetObjectField(TEXT("current_phase"));J->SetBoolField(TEXT("returned"),true);J->SetBoolField(TEXT("complete"),OK);if(!OK){ObservationFailed(TEXT("source_phase_incomplete"));}return OK;}
    void BeginExport(UObject* O,FProperty* P,const TCHAR* Route)
    {
        // Metadata is captured while the original pre-export current barrier still holds.
        ExportTrace=MakeShared<FJsonObject>();ExportTrace->SetStringField(TEXT("phase"),ObservationPhase);ExportTrace->SetStringField(TEXT("route"),Route);ExportTrace->SetStringField(TEXT("object"),O->GetPathName());ExportTrace->SetStringField(TEXT("object_class"),O->GetClass()->GetPathName());ExportTrace->SetStringField(TEXT("property"),P->GetName());ExportTrace->SetStringField(TEXT("owner"),P->GetOwnerStruct()->GetPathName());ExportTrace->SetStringField(TEXT("field_class"),P->GetClass()->GetName());const FStructProperty* S=CastField<FStructProperty>(P);ExportTrace->SetStringField(TEXT("struct_type"),S&&S->Struct?S->Struct->GetPathName():FString());ExportTrace->SetBoolField(TEXT("data_is_delta"),true);ExportTrace->SetBoolField(TEXT("return_known"),false);SourceDiagnostic->SetObjectField(TEXT("last_export"),ExportTrace.ToSharedRef());
    }
    void EndExport(bool Performed,int32 Length,bool CurrentOK,bool LimitOK)
    {
        // No UObject/FProperty access after the export or a failed current barrier.
        if(!ExportTrace.IsValid()){return;}ExportTrace->SetBoolField(TEXT("return_known"),true);ExportTrace->SetBoolField(TEXT("export_performed"),Performed);ExportTrace->SetNumberField(TEXT("text_length"),Length);ExportTrace->SetBoolField(TEXT("current_after"),CurrentOK);ExportTrace->SetBoolField(TEXT("within_limit"),LimitOK);if(!Performed||!CurrentOK||!LimitOK){ObservationFailed(TEXT("field_observation_incomplete"));}
    }
    explicit FMappingContextScope(FResult& In,bool Factory=false,FInputOwnedReadScope* Read=nullptr):R(In),OwnedRead(Read),RequireFactory(Factory){}
    ~FMappingContextScope(){if(Started){MappingContextActive=false;}}
    bool Stop(const TCHAR* Code,const TCHAR* Message)
    {if(!Stopped){ObservationFailed(Code);Fail(R,Code,Message,true);R.Stage=TEXT("verification");auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("code"),Code);R.Data->SetObjectField(TEXT("input_mapping_first_failure"),J);}Stopped=true;return false;}
    bool Context()
    {
        if(Stopped){return false;}if(!IsEditorGameThread()||!Loaded(GEditor)||GEditor!=Editor.Get()||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||IsPlaySessionActive()
            ||!Loaded(World.Get())||World->WorldType!=EWorldType::Editor||World->bIsTearingDown||GEditor->GetEditorWorldContext().World()!=World.Get())
        {return Stop(TEXT("data_asset_mapping_context_changed"),TEXT("Captured idle Editor/world changed; no further explicit read/write/save/retry; mandatory transaction unwind may still run."));}return true;
    }
    bool Pin(UObject* O)
    {
        if(!O){return true;}if(!Context()||!Loaded(O)||!Loaded(O->GetClass())||!Loaded(O->GetPackage())||!Clean(O->GetPathName())||!Clean(O->GetClass()->GetPathName())){return Stop(TEXT("data_asset_mapping_identity_invalid"),TEXT("Exact current clean native/source/target identity required."));}
        for(const auto& I:Held){if(I.Hold.Get()==O){return true;}}if(Held.Num()>=1024){return Stop(TEXT("data_asset_mapping_identity_budget"),TEXT("Finite Context/source identity budget exceeded."));}
        FIdentity I;I.Hold.Reset(O);I.Class=O->GetClass();I.Outer=O->GetOuter();I.Package=O->GetPackage();I.Path=O->GetPathName();Held.Add(MoveTemp(I));return true;
    }
    bool Layout(FProperty* P,UStruct* Owner)
    {
        if(!P||P->GetOwnerStruct()!=Owner||P->ArrayDim!=1||P->ElementSize<=0||!Pin(Owner)){return Stop(TEXT("data_asset_mapping_schema_mismatch"),TEXT("Exact native owner/dimension/layout required."));}
        FLayout L;L.Property=P;L.Owner=Owner;L.Name=P->GetFName();L.Flags=P->GetPropertyFlags();L.Offset=P->GetOffset_ForInternal();L.Size=P->ElementSize;Layouts.Add(L);return true;
    }
    bool NativeClass(UClass* C,const TCHAR* Path)
    {return Loaded(C)&&C==FindObject<UClass>(nullptr,Path)&&C->GetClass()==UClass::StaticClass()&&!C->ClassGeneratedBy&&C->GetSuperClass()==UDataAsset::StaticClass()&&C->HasAnyClassFlags(CLASS_Native)&&!C->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)&&!C->IsEditorOnly()&&C->GetPackage()->GetName()==TEXT("/Script/EnhancedInput")&&C->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)&&!C->GetPackage()->HasAnyPackageFlags(PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly)&&!C->GetPackage()->IsDirty();}
    bool Current()
    {
        if(!Context()||(OwnedRead&&!OwnedRead->Current())){return false;}for(const auto& I:Held){UObject* O=I.Hold.Get();if(!Loaded(O)||O->GetClass()!=I.Class||O->GetOuter()!=I.Outer||O->GetPackage()!=I.Package||O->GetPathName()!=I.Path||FindObject<UObject>(nullptr,*I.Path)!=O){return Stop(TEXT("data_asset_mapping_identity_changed"),TEXT("Held current identity changed; no adoption or retry."));}}
        if(!FModuleManager::Get().IsModuleLoaded(TEXT("EnhancedInput"))||!NativeClass(Class.Get(),MappingContextClassPath)||!NativeClass(ActionClass.Get(),InputActionClassPath)||Class->GetDefaultObject(false)!=Default.Get()){return Stop(TEXT("data_asset_mapping_native_changed"),TEXT("Exact native Context/Action source or existing CDO changed."));}
        for(const auto& L:Layouts){if(L.Owner->FindPropertyByName(L.Name)!=L.Property||L.Property->GetOwnerStruct()!=L.Owner||L.Property->ArrayDim!=1||L.Property->GetPropertyFlags()!=L.Flags||L.Property->GetOffset_ForInternal()!=L.Offset||L.Property->ElementSize!=L.Size){return Stop(TEXT("data_asset_mapping_layout_changed"),TEXT("Current exact property layout changed before memory access."));}}
        if(Ready&&(Defaults->Struct!=DataStruct||DataStruct->GetStructureSize()!=DataSize||RowStruct->GetStructureSize()!=RowSize||Key->Struct!=KeyStruct||Key->ElementSize!=sizeof(FKey)
            ||Rows->Inner!=RowsInner||Legacy->Inner!=LegacyInner||Profiles->KeyProp!=ProfilesKey||Profiles->ValueProp!=ProfilesValue||Triggers->Inner!=TriggersInner||Modifiers->Inner!=ModifiersInner
            ||!CastField<FStructProperty>(RowsInner)||CastField<FStructProperty>(RowsInner)->Struct!=RowStruct||RowsInner->ElementSize!=RowSize||RowsInner->ArrayDim!=1
            ||!CastField<FStructProperty>(LegacyInner)||CastField<FStructProperty>(LegacyInner)->Struct!=RowStruct||LegacyInner->ElementSize!=RowSize||LegacyInner->ArrayDim!=1
            ||!CastField<FStrProperty>(ProfilesKey)||!CastField<FStructProperty>(ProfilesValue)||CastField<FStructProperty>(ProfilesValue)->Struct!=DataStruct
            ||!CastField<FObjectPropertyBase>(TriggersInner)||!Loaded(CastField<FObjectPropertyBase>(TriggersInner)->PropertyClass)||CastField<FObjectPropertyBase>(TriggersInner)->PropertyClass->GetPathName()!=TEXT("/Script/EnhancedInput.InputTrigger")
            ||!CastField<FObjectPropertyBase>(ModifiersInner)||!Loaded(CastField<FObjectPropertyBase>(ModifiersInner)->PropertyClass)||CastField<FObjectPropertyBase>(ModifiersInner)->PropertyClass->GetPathName()!=TEXT("/Script/EnhancedInput.InputModifier")
            ||Action->PropertyClass.Get()!=ActionClass.Get()||Behavior->GetEnum()!=SettingEnum||Behavior->GetUnderlyingProperty()!=BehaviorNumeric||!Loaded(Settings->PropertyClass)||Settings->PropertyClass->GetPathName()!=TEXT("/Script/EnhancedInput.PlayerMappableKeySettings")||!Loaded(Behavior->GetEnum())||Behavior->GetEnum()->GetPathName()!=TEXT("/Script/EnhancedInput.EPlayerMappableKeySettingBehaviors")))
        {return Stop(TEXT("data_asset_mapping_nested_layout_changed"),TEXT("Exact nested struct/array/map/object layout changed before memory access."));}
        if(Ready)
        {
            for(const auto& Pair:Nested){if(Pair.Key->ArrayDim!=1||uint64(Pair.Key->GetPropertyFlags())!=Pair.Value.Key||Pair.Key->ElementSize!=Pair.Value.Value){return Stop(TEXT("data_asset_mapping_nested_flags_changed"),TEXT("Nested native flags/dimension/element layout changed."));}}
            FScriptArrayHelper D(Rows,Rows->ContainerPtrToValuePtr<void>(Defaults->ContainerPtrToValuePtr<void>(Default.Get()))),L(Legacy,Legacy->ContainerPtrToValuePtr<void>(Default.Get()));FScriptMapHelper P(Profiles,Profiles->ContainerPtrToValuePtr<void>(Default.Get()));
            if(D.Num()!=0||L.Num()!=0||P.Num()!=0){return Stop(TEXT("data_asset_mapping_native_default_changed"),TEXT("Native Context CDO default/deprecated/profile arrays must remain empty."));}
        }
        for(const auto& S:Actions){if(!Loaded(S.Object)||S.Object->GetClass()!=ActionClass.Get()||S.Object->GetPathName()!=S.Path||S.Object->GetOuter()!=S.Object->GetPackage()||S.Object->GetPackage()->IsDirty()||!S.Object->GetPackage()->IsFullyLoaded()){return Stop(TEXT("data_asset_mapping_action_changed"),TEXT("Saved-clean exact Game InputAction source changed."));}}
        if(!Bound){return true;}UDataAsset* A=Target.Get();if(!Loaded(A)||A->GetClass()!=Class.Get()||A->GetOuter()!=A->GetPackage()||A->GetPathName()!=TargetPath||FindObject<UDataAsset>(nullptr,*TargetPath)!=A||A->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject)||A->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn|PKG_PlayInEditor|PKG_EditorOnly)){return Stop(TEXT("data_asset_mapping_target_changed"),TEXT("Exact ordinary native Context target required."));}
        FScriptArrayHelper Old(Legacy,Legacy->ContainerPtrToValuePtr<void>(A));FScriptMapHelper Profile(Profiles,Profiles->ContainerPtrToValuePtr<void>(A));if(Old.Num()!=0||Profile.Num()!=0){return Stop(TEXT("data_asset_mapping_profile_unsupported"),TEXT("Deprecated Mappings and MappingProfileOverrides must remain empty; actual default mappings are separate."));}
        TArray<UObject*> Owned;GetObjectsWithOuter(A,Owned,EGetObjectsFlags::IncludeNestedObjects);if((!OwnedRead&&!Owned.IsEmpty())||(OwnedRead&&!OwnedRead->Current())){return Stop(TEXT("data_asset_mapping_owned_unsupported"),TEXT("Context owned inline objects remain unsupported."));}
        TArray<UObject*> Exports;GetObjectsWithOuter(A->GetPackage(),Exports,EGetObjectsFlags::IncludeNestedObjects);if(Exports.Num()>(OwnedRead?80:16)){return Stop(TEXT("data_asset_mapping_export_budget"),TEXT("Finite selected package export set required."));}
        for(UObject* O:Exports)
        {
            if(!Loaded(O)){return Stop(TEXT("data_asset_mapping_foreign_export"),TEXT("Only selected Context and native package metadata may share the save package."));}
            // KH: bridge only this call's held return until AcceptAllocation commits it to Owned.
            const bool PendingOwned=OwnedWriter()&&OwnedRead->Bound&&OwnedRead->Target.Get()==A&&O==OwnedRead->Allocating
                &&OwnedRead->WriterKind(OwnedRead->Kind(OwnedRead->AllocatingClass))&&O->GetClass()==OwnedRead->AllocatingClass
                &&O->GetOuter()==A&&O->GetPackage()==A->GetPackage()&&!O->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_Transient)&&CreatedOwned.Contains(O)
                &&OwnedRead->Held.ContainsByPredicate([O,A](const FInputOwnedReadScope::FIdentity& I)
                {return I.Hold.Get()==O&&I.Class==O->GetClass()&&I.Outer==A&&I.Package==A->GetPackage()&&I.Path==O->GetPathName()&&I.Flags==O->GetFlags();});
            if(O!=A&&!(OwnedRead&&OwnedRead->Owned.Contains(O))&&!PendingOwned&&O->GetClass()->GetPathName()!=TEXT("/Script/CoreUObject.MetaData"))
            {return Stop(TEXT("data_asset_mapping_foreign_export"),TEXT("Only selected Context and native package metadata may share the save package."));}
        }return true;
    }
    bool Raw(UObject* O,TSharedRef<FJsonObject>& J)
    {
        if(!Current()||!Loaded(O)){return false;}int32 Counted=0,Chars=0;
        for(TFieldIterator<FProperty> It(O->GetClass(),EFieldIteratorFlags::IncludeSuper);It;++It)
        {
            FProperty* P=*It;if(++Counted>256||P->ArrayDim!=1){return false;}auto F=MakeShared<FJsonObject>();J->SetObjectField(P->GetName(),F);F->SetStringField(TEXT("owner"),P->GetOwnerStruct()->GetPathName());F->SetStringField(TEXT("class"),P->GetClass()->GetName());F->SetStringField(TEXT("flags"),LexToString(uint64(P->GetPropertyFlags())));
            BeginExport(O,P,TEXT("raw_field"));FString TextValue;const void* Address=P->ContainerPtrToValuePtr<void>(O);const bool Exported=P->ExportText_Direct(TextValue,Address,Address,O,PPF_None);const bool CurrentOK=Current();const bool LimitOK=TextValue.Len()<=1048576&&Chars<=1048576-TextValue.Len();EndExport(Exported,TextValue.Len(),CurrentOK,LimitOK);if(!CurrentOK||!Exported||!LimitOK){return false;}Chars+=TextValue.Len();F->SetStringField(TEXT("value"),TextValue);
        }return true;
    }
    bool ActionSnapshot(UDataAsset* A,TSharedRef<FJsonObject>& J)
    {
        if(!Current()||!Loaded(A)||A->GetPackage()->IsDirty()||!A->GetPackage()->IsFullyLoaded()){return false;}J->SetObjectField(TEXT("identity"),Identity(A));auto File=MakeShared<FJsonObject>();J->SetObjectField(TEXT("disk"),File);if(!Disk(A->GetPackage()->GetName(),File)||!File->GetBoolField(TEXT("exists"))||File->GetNumberField(TEXT("bytes"))<=0){return false;}
        TArray<UObject*> Exports;GetObjectsWithOuter(A->GetPackage(),Exports,EGetObjectsFlags::IncludeNestedObjects);if(Exports.Num()>32){return false;}auto Objects=MakeShared<FJsonObject>();J->SetObjectField(TEXT("exports"),Objects);
        for(UObject* O:Exports)
        {
            if(!Loaded(O)||!O->GetClass()->HasAnyClassFlags(CLASS_Native)||O->GetClass()->ClassGeneratedBy||(O->IsEditorOnly()&&O->GetClass()->GetPathName()!=TEXT("/Script/CoreUObject.MetaData"))||(O!=A&&!O->IsIn(A)&&O->GetClass()->GetPathName()!=TEXT("/Script/CoreUObject.MetaData"))||!Pin(O)||!Pin(O->GetClass())){return false;}
            auto Row=MakeShared<FJsonObject>();Objects->SetObjectField(O->GetPathName(),Row);Row->SetObjectField(TEXT("identity"),Identity(O));auto Values=MakeShared<FJsonObject>();Row->SetObjectField(TEXT("raw_fields"),Values);if(!Raw(O,Values)){return false;}
        }return Current()&&!A->GetPackage()->IsDirty()&&A->GetPackage()->IsFullyLoaded();
    }
    bool AdmitAction(UDataAsset* A,FString& Error)
    {
        for(const auto& S:Actions){if(S.Object==A){return Current();}}if(Actions.Num()>=32){Error=TEXT("At most32 distinct already-loaded InputAction sources");return false;}
        if(!Current()||!Loaded(A)||A->GetClass()!=ActionClass.Get()||A->GetOuter()!=A->GetPackage()||!A->GetPackage()->GetName().StartsWith(TEXT("/Game/"))||!FPackageName::IsValidObjectPath(A->GetPathName())||A->GetPathName().Contains(TEXT(":"))||!Clean(A->GetPathName())||A->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject)||A->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn|PKG_PlayInEditor|PKG_EditorOnly)||A->GetPackage()->IsDirty()||!A->GetPackage()->IsFullyLoaded()){Error=TEXT("Already-loaded saved-clean exact native Game InputAction reference required; no source loader");return false;}
        auto File=MakeShared<FJsonObject>();if(!Disk(A->GetPackage()->GetName(),File)||!File->GetBoolField(TEXT("exists"))||File->GetNumberField(TEXT("bytes"))<=0){Error=TEXT("InputAction source must already have a saved nonempty native asset file");return false;}
        if(!Pin(A)||!Pin(A->GetPackage())){return false;}FActionSource S;S.Object=A;S.Path=A->GetPathName();S.Package=A->GetPackage()->GetName();if(!ActionSnapshot(A,S.Before)){Error=TEXT("Complete saved source fields/owned exports/disk observation required");return Stop(TEXT("data_asset_mapping_action_unobservable"),TEXT("Action baseline became incomplete across observation; actual prefix retained, no further explicit writer/save."));}auto Before=R.Data->HasTypedField<EJson::Object>(TEXT("input_mapping_action_sources_before"))?R.Data->GetObjectField(TEXT("input_mapping_action_sources_before")).ToSharedRef():MakeShared<FJsonObject>();Before->SetObjectField(S.Path,S.Before);R.Data->SetObjectField(TEXT("input_mapping_action_sources_before"),Before);Actions.Add(MoveTemp(S));return Current();
    }
    bool KeyAllowed(const FKey& K)
    {
        const FString Name=K.ToString();bool Listed=false;for(const TCHAR* Token:{TEXT("Gamepad_FaceButton_Right"),TEXT("U"),TEXT("Gamepad_LeftX"),TEXT("W"),TEXT("Gamepad_LeftTriggerAxis"),TEXT("S"),TEXT("A"),TEXT("D"),TEXT("Gamepad_RightTriggerAxis")}){Listed|=Name.Equals(Token,ESearchCase::CaseSensitive);}return Listed&&K.IsValid();
    }
    bool ObserveRows(void* Data,bool Adopt,TArray<TSharedPtr<FJsonValue>>& Result,FString& Error)
    {
        if(!Current()){return false;}FScriptArrayHelper Array(Rows,Rows->ContainerPtrToValuePtr<void>(Data));if(Array.Num()>64){Error=TEXT("At most64 ordered mapping rows");return false;}
        for(int32 Index=0;Index<Array.Num();++Index)
        {
            void* Row=Array.GetRawPtr(Index);UObject* Reference=Action->GetObjectPropertyValue_InContainer(Row);UDataAsset* A=Loaded(Reference)?Cast<UDataAsset>(Reference):nullptr;
            if(Adopt){if(!AdmitAction(A,Error)){return false;}}else{bool Found=false;for(const auto& S:Actions){Found|=S.Object==A;}if(!Found||!Loaded(A)||A->GetClass()!=ActionClass.Get()){Error=TEXT("Unadmitted Action reference in current mapping row");return false;}}
            const FKey* K=Key->ContainerPtrToValuePtr<FKey>(Row);FScriptArrayHelper T(Triggers,Triggers->ContainerPtrToValuePtr<void>(Row)),M(Modifiers,Modifiers->ContainerPtrToValuePtr<void>(Row));
            const uint64 B=Behavior->GetUnderlyingProperty()->GetUnsignedIntPropertyValue(Behavior->ContainerPtrToValuePtr<void>(Row));
            const auto SettingsValue=Settings->GetObjectPtrPropertyValue_InContainer(Row);const FString BehaviorToken=Behavior->GetEnum()->GetAuthoredNameStringByValue(B);const bool PlayerStored=OwnedRead&&OwnedRead->PlayerSettingsHost;
            if(!KeyAllowed(*K)||(!OwnedRead&&(T.Num()!=0||M.Num()!=0))||!SettingsValue.IsResolved()||(SettingsValue.Get()!=nullptr&&(!PlayerStored||!OwnedRead->IsPlayerSettingsObject(SettingsValue.Get())))||!(BehaviorToken==TEXT("InheritSettingsFromAction")||(PlayerStored&&(BehaviorToken==TEXT("OverrideSettings")||BehaviorToken==TEXT("IgnoreSettings"))))||Ignored->GetPropertyValue_InContainer(Row)||Ticking->GetPropertyValue_InContainer(Row))
            {Error=TEXT("Finite valid digital/axis keys, empty Triggers/Modifiers, inherited/null settings and false transient flags required");return false;}
            if(OwnedWriter())
            {
                for(const auto& Pair:TArray<TPair<FArrayProperty*,bool>>{{Triggers,true},{Modifiers,false}})
                {FObjectPropertyBase* Inner=CastField<FObjectPropertyBase>(Pair.Key->Inner);FScriptArrayHelper Slots(Pair.Key,Pair.Key->ContainerPtrToValuePtr<void>(Row));if(!Inner||Slots.Num()>64){Error=TEXT("Finite owned slots required");return false;}for(int32 I=0;I<Slots.Num();++I){UObject* O=Inner->GetObjectPropertyValue(Slots.GetRawPtr(I));if(!Loaded(O)||!OwnedRead->Owned.Contains(O)||O->GetOuter()!=Target.Get()||OwnedRead->TriggerKind(OwnedRead->Kind(O->GetClass()))!=Pair.Value){Error=TEXT("Exact current direct-owned matching modifier/trigger pointer required");return false;}}}
            }
            auto RawRow=MakeShared<FJsonObject>();Result.Add(MakeShared<FJsonValueObject>(RawRow));
            for(TFieldIterator<FProperty> It(RowStruct,EFieldIteratorFlags::ExcludeSuper);It;++It)
            {
                FProperty* P=*It;BeginExport(Target.Get(),P,TEXT("raw_row_field"));ExportTrace->SetNumberField(TEXT("row_index"),Index);FString Value;const void* Address=P->ContainerPtrToValuePtr<void>(Row);const bool Exported=P->ExportText_Direct(Value,Address,Address,Target.Get(),PPF_None);const bool CurrentOK=Current();const bool LimitOK=Value.Len()<=65536;EndExport(Exported,Value.Len(),CurrentOK,LimitOK);if(!CurrentOK||!Exported||!LimitOK){return false;}RawRow->SetStringField(P->GetName(),Value);
            }

        }return true;
    }
    bool Source(TSharedRef<FJsonObject>& J)
    {
        R.Data->SetObjectField(TEXT("input_mapping_source_partial"),J);BeginPhase(TEXT("context_native"));if(!EndPhase(Sources(Class.Get(),nullptr,J,true,nullptr,nullptr,nullptr,this))){return false;}
        if(RequireFactory){BeginPhase(TEXT("factory_native"));if(!EndPhase(Sources(UDataAssetFactory::StaticClass(),nullptr,J,true,nullptr,nullptr,nullptr,this))){return false;}}
        auto ActionSource=MakeShared<FJsonObject>();J->SetObjectField(TEXT("action_native_source"),ActionSource);BeginPhase(TEXT("action_native"));if(!EndPhase(Sources(ActionClass.Get(),nullptr,ActionSource,true,nullptr,nullptr,nullptr,this))){return false;}
        auto ContextDefault=MakeShared<FJsonObject>(),ActionDefault=MakeShared<FJsonObject>();J->SetObjectField(TEXT("raw_context_default"),ContextDefault);J->SetObjectField(TEXT("raw_action_default"),ActionDefault);
        BeginPhase(TEXT("raw_context_default"));if(!EndPhase(Raw(Default.Get(),ContextDefault))){return false;}BeginPhase(TEXT("raw_action_default"));if(!EndPhase(Raw(ActionClass->GetDefaultObject(false),ActionDefault))){return false;}
        BeginPhase(TEXT("schema_enum"));auto Schema=MakeShared<FJsonObject>();J->SetObjectField(TEXT("mapping_schema"),Schema);
        for(const auto& L:Layouts){auto Row=MakeShared<FJsonObject>();Schema->SetObjectField(L.Owner->GetPathName()+TEXT(".")+L.Property->GetName(),Row);Row->SetStringField(TEXT("flags"),LexToString(uint64(L.Property->GetPropertyFlags())));Row->SetStringField(TEXT("cpp_type"),L.Property->GetCPPType());Row->SetNumberField(TEXT("offset"),L.Property->GetOffset_ForInternal());Row->SetNumberField(TEXT("element_size"),L.Property->ElementSize);}
        auto InnerSchema=MakeShared<FJsonObject>();J->SetObjectField(TEXT("nested_schema"),InnerSchema);for(const auto& Pair:Nested){auto V=MakeShared<FJsonObject>();InnerSchema->SetObjectField(Pair.Key->GetPathName(),V);V->SetStringField(TEXT("flags"),LexToString(uint64(Pair.Key->GetPropertyFlags())));V->SetStringField(TEXT("cpp_type"),Pair.Key->GetCPPType());V->SetNumberField(TEXT("element_size"),Pair.Key->ElementSize);V->SetNumberField(TEXT("array_dim"),Pair.Key->ArrayDim);}
        J->SetStringField(TEXT("legacy_deprecated_metadata"),Legacy->GetMetaData(TEXT("DeprecatedProperty")));J->SetStringField(TEXT("legacy_deprecation_message"),Legacy->GetMetaData(TEXT("DeprecationMessage")));
        J->SetStringField(TEXT("row_struct_flags"),LexToString(uint64(RowStruct->StructFlags)));J->SetNumberField(TEXT("row_struct_size"),RowStruct->GetStructureSize());J->SetStringField(TEXT("data_struct_flags"),LexToString(uint64(DataStruct->StructFlags)));J->SetNumberField(TEXT("data_struct_size"),DataStruct->GetStructureSize());
        TArray<TSharedPtr<FJsonValue>> EnumValues;UEnum* E=Behavior->GetEnum();if(E->NumEnums()>8){EndPhase(false);return false;}for(int32 I=0;I<E->NumEnums();++I){auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),E->GetNameByIndex(I).ToString());Row->SetStringField(TEXT("value"),LexToString(E->GetValueByIndex(I)));Row->SetStringField(TEXT("authored_name"),E->GetAuthoredNameStringByValue(E->GetValueByIndex(I)));EnumValues.Add(MakeShared<FJsonValueObject>(Row));}J->SetArrayField(TEXT("setting_enum"),EnumValues);EndPhase(true);BeginPhase(TEXT("current"));return EndPhase(Current());
    }
    bool Start()
    {
        if(MappingContextActive||InputActionActive){return Fail(R,TEXT("data_asset_mapping_reentry"),TEXT("Concrete input operation callback reentry rejected before load."));}if(!IsEditorGameThread()||!Loaded(GEditor)||IsPlaySessionActive()){return Fail(R,TEXT("data_asset_mapping_editor_required"),TEXT("Idle Editor required."));}
        Editor=GEditor;World=GEditor->GetEditorWorldContext().World();if(!Context()){return false;}if(!FModuleManager::Get().IsModuleLoaded(TEXT("EnhancedInput"))){return Fail(R,TEXT("data_asset_mapping_module_unavailable"),TEXT("EnhancedInput must already be loaded; no module/CDO warming."));}
        UClass* C=FindObject<UClass>(nullptr,MappingContextClassPath);UClass* AC=FindObject<UClass>(nullptr,InputActionClassPath);if(!NativeClass(C,MappingContextClassPath)||!NativeClass(AC,InputActionClassPath)){return Fail(R,TEXT("data_asset_mapping_native_unavailable"),TEXT("Exact already-loaded native Context and InputAction classes required."));}Class.Reset(C);ActionClass.Reset(AC);Default.Reset(C->GetDefaultObject(false));
        if(!Loaded(Default.Get())||Default->GetClass()!=C||!Loaded(AC->GetDefaultObject(false))){return Fail(R,TEXT("data_asset_mapping_default_unavailable"),TEXT("Existing Context/Action CDOs required; no default creation."));}MappingContextActive=true;Started=true;
        if(!Pin(C)||!Pin(AC)||!Pin(C->GetPackage())||!Pin(C->GetSuperClass())||!Pin(Default.Get())||!Pin(AC->GetDefaultObject(false))){return false;}if(RequireFactory&&(!Pin(UDataAssetFactory::StaticClass())||!Pin(UDataAssetFactory::StaticClass()->GetDefaultObject(false)))){return false;}
        DataStruct=FindObject<UScriptStruct>(nullptr,TEXT("/Script/EnhancedInput.InputMappingContextMappingData"));RowStruct=FindObject<UScriptStruct>(nullptr,TEXT("/Script/EnhancedInput.EnhancedActionKeyMapping"));KeyStruct=FindObject<UScriptStruct>(nullptr,TEXT("/Script/InputCore.Key"));
        if(!Loaded(DataStruct)||!Loaded(RowStruct)||!(DataStruct->StructFlags&STRUCT_Native)||!(RowStruct->StructFlags&STRUCT_Native)||!Loaded(KeyStruct)||!(KeyStruct->StructFlags&STRUCT_Native)||KeyStruct->GetStructureSize()!=sizeof(FKey)||KeyStruct->GetPathName()!=TEXT("/Script/InputCore.Key")||!Pin(DataStruct)||!Pin(RowStruct)||!Pin(KeyStruct)){return Stop(TEXT("data_asset_mapping_struct_mismatch"),TEXT("Exact native public mapping/data/FKey script structs required."));}
        Defaults=FindFProperty<FStructProperty>(C,TEXT("DefaultKeyMappings"));Legacy=FindFProperty<FArrayProperty>(C,TEXT("Mappings"));Profiles=FindFProperty<FMapProperty>(C,TEXT("MappingProfileOverrides"));Rows=FindFProperty<FArrayProperty>(DataStruct,TEXT("Mappings"));
        if(!Layout(Defaults,C)||!Layout(Legacy,C)||!Layout(Profiles,C)||!Layout(Rows,DataStruct)||Defaults->Struct!=DataStruct||!Defaults->HasAllPropertyFlags(CPF_Edit|CPF_Config)||!Legacy->HasAllPropertyFlags(CPF_Config|CPF_BlueprintVisible|CPF_BlueprintReadOnly)||Legacy->HasAnyPropertyFlags(CPF_Edit)||!Legacy->HasMetaData(TEXT("DeprecatedProperty"))||!Profiles->HasAllPropertyFlags(CPF_Edit|CPF_Config)||!CastField<FStructProperty>(Rows->Inner)||CastField<FStructProperty>(Rows->Inner)->Struct!=RowStruct||!CastField<FStructProperty>(Legacy->Inner)||CastField<FStructProperty>(Legacy->Inner)->Struct!=RowStruct||!CastField<FStrProperty>(Profiles->KeyProp)||!CastField<FStructProperty>(Profiles->ValueProp)||CastField<FStructProperty>(Profiles->ValueProp)->Struct!=DataStruct){return Stop(TEXT("data_asset_mapping_schema_mismatch"),TEXT("Exact actual default/deprecated/profile storage declarations required."));}
        Action=FindFProperty<FObjectPropertyBase>(RowStruct,TEXT("Action"));Key=FindFProperty<FStructProperty>(RowStruct,TEXT("Key"));Triggers=FindFProperty<FArrayProperty>(RowStruct,TEXT("Triggers"));Modifiers=FindFProperty<FArrayProperty>(RowStruct,TEXT("Modifiers"));Behavior=FindFProperty<FEnumProperty>(RowStruct,TEXT("SettingBehavior"));Settings=FindFProperty<FObjectPropertyBase>(RowStruct,TEXT("PlayerMappableKeySettings"));Ignored=FindFProperty<FBoolProperty>(RowStruct,TEXT("bShouldBeIgnored"));Ticking=FindFProperty<FBoolProperty>(RowStruct,TEXT("bHasAlwaysTickTrigger"));
        int32 RowFields=0;for(TFieldIterator<FProperty> It(RowStruct,EFieldIteratorFlags::ExcludeSuper);It;++It){++RowFields;if(!Layout(*It,RowStruct)){return false;}}
        if(RowFields!=8||!Action||Action->PropertyClass.Get()!=AC||!Action->HasAllPropertyFlags(CPF_Edit|CPF_BlueprintVisible)||!Key||Key->Struct!=KeyStruct||!Key->HasAllPropertyFlags(CPF_Edit|CPF_BlueprintVisible)||!Triggers||!Modifiers||!Behavior||!Behavior->HasAllPropertyFlags(CPF_Edit)||!Settings||!Ignored||!Ticking||!Ignored->HasAllPropertyFlags(CPF_Transient)||!Ticking->HasAllPropertyFlags(CPF_Transient)||!Settings->HasAllPropertyFlags(CPF_Edit|CPF_InstancedReference)||!Loaded(Settings->PropertyClass)||Settings->PropertyClass->GetPathName()!=TEXT("/Script/EnhancedInput.PlayerMappableKeySettings")||!Behavior->GetUnderlyingProperty()||!Behavior->GetUnderlyingProperty()->IsA<FByteProperty>()||!Loaded(Behavior->GetEnum())||Behavior->GetEnum()->GetPathName()!=TEXT("/Script/EnhancedInput.EPlayerMappableKeySettingBehaviors")||!Pin(Behavior->GetEnum())||!Pin(Settings->PropertyClass))
        {return Stop(TEXT("data_asset_mapping_row_schema_mismatch"),TEXT("Exact eight public stored mapping row fields/settings/transient bits required."));}
        for(const auto& Pair:TArray<TPair<FArrayProperty*,FString>>{{Triggers,TEXT("/Script/EnhancedInput.InputTrigger")},{Modifiers,TEXT("/Script/EnhancedInput.InputModifier")}}){FObjectPropertyBase* Inner=CastField<FObjectPropertyBase>(Pair.Key->Inner);if(!Pair.Key->HasAllPropertyFlags(CPF_Edit|CPF_BlueprintVisible)||!Inner||!Inner->HasAnyPropertyFlags(CPF_InstancedReference|CPF_ExportObject)||!Loaded(Inner->PropertyClass)||Inner->PropertyClass->GetPathName()!=Pair.Value||!Pin(Inner->PropertyClass)){return Stop(TEXT("data_asset_mapping_row_schema_mismatch"),TEXT("Exact protected native inline array classes required."));}}
        RowsInner=Rows->Inner;LegacyInner=Legacy->Inner;ProfilesKey=Profiles->KeyProp;ProfilesValue=Profiles->ValueProp;TriggersInner=Triggers->Inner;ModifiersInner=Modifiers->Inner;DataSize=DataStruct->GetStructureSize();RowSize=RowStruct->GetStructureSize();SettingEnum=Behavior->GetEnum();BehaviorNumeric=Behavior->GetUnderlyingProperty();for(FProperty* P:TArray<FProperty*>{RowsInner,LegacyInner,ProfilesKey,ProfilesValue,TriggersInner,ModifiersInner}){Nested.Add(P,TPair<uint64,int32>(uint64(P->GetPropertyFlags()),P->ElementSize));}Ready=true;
        R.Data->SetObjectField(TEXT("input_mapping_source_before"),SourceBefore);if(!Source(SourceBefore)){return Stop(TEXT("data_asset_mapping_source_unobservable"),TEXT("Complete native source/schema/CDO baseline required; earliest phase/field diagnostic and partial source retained."));}R.Data->SetStringField(TEXT("supported_class"),MappingContextClassPath);
        R.Data->SetStringField(TEXT("admission_scope"),OwnedRead?TEXT("Exact native Context Get of ordered default Action/FKey mappings with eleven finite direct asset-owned modifier/trigger classes and loaded CurveFloat sources; existing keys/action-source/deprecated/profile/settings boundaries remain. InputAction setter/Create keep empty inline arrays/no target owned exports; finite Context owned writing is separate."):TEXT("Exact native Context lifecycle and actual DefaultKeyMappings.Mappings ordered Action/FKey rows; finite saved-clean native Game actions, nine source-backed digital/axis keys, empty inline arrays/profiles/deprecated storage and inherited/null settings."));
        R.Data->SetStringField(TEXT("description_cook_scope"),TEXT("Native EnhancedInput asset payload; input dispatch, control mapping rebuild, runtime/vehicle execution and cook are unverified."));R.Data->SetBoolField(TEXT("input_mapping_factory_source_required"),RequireFactory);R.Data->SetBoolField(TEXT("input_mapping_prior_runtime_capture_available"),false);R.Data->SetBoolField(TEXT("input_mapping_global_effects_verified"),false);
        R.Data->SetStringField(TEXT("input_mapping_partial_contract"),TEXT("Existing converter is applied to a scratch property after finite row/key/reference admission, then the whole accepted default property is copied. This does not reproduce original direct-converter resize/earlier-row partial failures or provide atomic rollback. Mixed top-level property/save, actual copy/notification prefixes and unknown outcomes are reported separately."));
        R.Data->SetStringField(TEXT("input_mapping_reference_text_scope"),TEXT("Canonical native Action class-export strings are used for strict expected-value success. Plain object-path import is admitted but may copy/save successfully and then fail the original literal expected-string predicate against a class-export readback; actual save/partial receipts are retained."));
        R.Data->SetStringField(TEXT("input_mapping_callback_scope"),TEXT("Context inherits UObject non-null property-change global broadcast; current source/target/action fields and transaction/save boundaries are checked. Unknown delegates, source load/PostLoad effects, FKey lookup cache and global purity are unmeasured. No MapKey/rebuild/SaveConfig/registration repair."));return Current();
    }
    bool Check(FProperty* Mutable=nullptr)
    {
        if(!Current()||(OwnedWriter()&&!OwnedRead->Check())){return false;}auto SourceNow=MakeShared<FJsonObject>();if(!Source(SourceNow)||!Equal(SourceBefore,SourceNow)){return Stop(TEXT("data_asset_mapping_source_changed"),TEXT("Native source/schema/CDO changed across callback boundary."));}R.Data->SetObjectField(TEXT("input_mapping_source_after"),SourceNow);
        auto ActionAfter=MakeShared<FJsonObject>();for(const auto& A:Actions){auto Now=MakeShared<FJsonObject>();ActionAfter->SetObjectField(A.Path,Now);R.Data->SetObjectField(TEXT("input_mapping_action_sources_after"),ActionAfter);if(!ActionSnapshot(A.Object,Now)||!Equal(A.Before,Now)){return Stop(TEXT("data_asset_mapping_action_preservation_failed"),TEXT("Admitted action source fields/owned exports/disk changed; no next explicit writer/save/retry."));}}
        if(!Bound){return true;}auto Now=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> RawRows;FString Error;
        if(!Raw(Target.Get(),Now)||!ObserveRows(Defaults->ContainerPtrToValuePtr<void>(Target.Get()),false,RawRows,Error)||!Current()||Now->Values.Num()!=FieldsBefore->Values.Num()){return Stop(TEXT("data_asset_mapping_state_unobservable"),TEXT("Complete current mapping raw state required."));}
        for(const auto& Pair:FieldsBefore->Values){if(!Mutable||FName(*FString(Pair.Key.ToView()))!=Mutable->GetFName()){if(!Equal(Pair.Value,Now->TryGetField(Pair.Key.ToView()))){return Stop(TEXT("data_asset_mapping_unselected_changed"),TEXT("A nonselected Context field changed, including deprecated/profile/settings state."));}}}
        if(Mutable){FieldsBefore=Now;RowsBefore=RawRows;}else if(!Equal(MakeShared<FJsonValueArray>(RowsBefore),MakeShared<FJsonValueArray>(RawRows))){return Stop(TEXT("data_asset_mapping_row_changed"),TEXT("Raw row order/settings/keys/flags drifted after its stored-value freeze."));}
        R.Data->SetObjectField(TEXT("input_mapping_raw_fields"),Now);R.Data->SetArrayField(TEXT("input_mapping_raw_rows"),RawRows);return true;
    }
    bool Bind(UDataAsset* A,const FString& Path)
    {
        if(!Check()||!Loaded(A)||A->GetClass()!=Class.Get()||A->GetPathName()!=Path||A->GetOuter()!=A->GetPackage()){return Stop(TEXT("data_asset_mapping_target_invalid"),TEXT("Exact returned native Context target required."));}if(Bound){return (A==Target.Get()&&Current())||Stop(TEXT("data_asset_mapping_target_replaced"),TEXT("Original relookup returned a different object; no adoption."));}
        Target.Reset(A);TargetPath=Path;Bound=true;if(!Pin(A)||!Pin(A->GetPackage())||!Current()){return false;}TArray<TSharedPtr<FJsonValue>> RawRows;FString Error;if(!ObserveRows(Defaults->ContainerPtrToValuePtr<void>(A),true,RawRows,Error)||!Raw(A,FieldsBefore)){return Stop(TEXT("data_asset_mapping_target_unsupported"),TEXT("Initial mapping rows/source references/protected state are unsupported or unobservable."));}RowsBefore=RawRows;TArray<UObject*> Exports;GetObjectsWithOuter(A->GetPackage(),Exports,EGetObjectsFlags::IncludeNestedObjects);for(UObject* O:Exports){if(!Pin(O)){return false;}}return Check();
    }
    bool PlanOwned(const TSharedPtr<FJsonValue>& Value,FString& Error)
    {
        OwnedPlan.Reset();PlainMapping.Reset();MatchingOwned=false;if(!OwnedWriter()||!Check()){return false;}
        const TArray<TSharedPtr<FJsonValue>>* Entries=nullptr;for(const auto& P:Value->AsObject()->Values){Entries=&P.Value->AsArray();}if(!Entries){return false;}
        FScriptArrayHelper Stored(Rows,Rows->ContainerPtrToValuePtr<void>(Defaults->ContainerPtrToValuePtr<void>(Target.Get())));
        const bool Fresh=Stored.Num()==0&&OwnedRead->Owned.IsEmpty();MatchingOwned=!Fresh;
        if(!Fresh)
        {
            auto Now=MakeShared<FJsonObject>();Count(R,TEXT("owned_matching_json_export_calls"));const bool Exported=FJsonObjectConverter::UStructToJsonObject(Class.Get(),Target.Get(),Now,0,0);if(!Check()){return false;}
            const auto Actual=Now->TryGetField(TEXT("defaultKeyMappings"));if(!Exported){Error=TEXT("Current canonical Context JSON unavailable before planning");return false;}MatchingOwned=Equal(Value,Actual)&&Stored.Num()==Entries->Num();
        }
        TSet<UObject*> Seen;int32 Total=0,NewCount=0;auto Plain=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> PlainRows;
        for(int32 RowIndex=0;RowIndex<Entries->Num();++RowIndex)
        {
            const auto Input=(*Entries)[RowIndex]->AsObject();auto PlainRow=MakeShared<FJsonObject>();
            for(const auto& P:Input->Values){const FName N(*FString(P.Key.ToView()));if(N==TEXT("Triggers")||N==TEXT("Modifiers")){PlainRow->SetArrayField(FString(P.Key.ToView()),{});}else{PlainRow->SetField(FString(P.Key.ToView()),P.Value);}}
            PlainRows.Add(MakeShared<FJsonValueObject>(PlainRow));
            for(const auto& Family:TArray<TPair<FArrayProperty*,bool>>{{Triggers,true},{Modifiers,false}})
            {
                const TArray<TSharedPtr<FJsonValue>>* Items=nullptr;for(const auto& P:Input->Values){if(FName(*FString(P.Key.ToView()))==Family.Key->GetFName()){Items=&P.Value->AsArray();}}
                if(!Items){continue;}FObjectPropertyBase* Inner=CastField<FObjectPropertyBase>(Family.Key->Inner);
                for(int32 Slot=0;Slot<Items->Num();++Slot)
                {
                    if(++Total>64||!Current()){Error=TEXT("At most64 total owned slots/literals in this finite property");return false;}const auto V=(*Items)[Slot];FOwnedSlotPlan Plan;Plan.Row=RowIndex;Plan.Slot=Slot;Plan.Trigger=Family.Value;
                    if(MatchingOwned){FScriptArrayHelper Existing(Family.Key,Family.Key->ContainerPtrToValuePtr<void>(Stored.GetRawPtr(RowIndex)));if(Slot>=Existing.Num()){Error=TEXT("Existing slot mismatch");return false;}Plan.Object=Inner->GetObjectPropertyValue(Existing.GetRawPtr(Slot));if(!Loaded(Plan.Object)||!OwnedRead->Owned.Contains(Plan.Object)){Error=TEXT("Current direct-owned slot required");return false;}Plan.Class=Plan.Object->GetClass();}
                    if(V->Type==EJson::String)
                    {
                        UObject* Reused=OwnedRead->ReuseOwned(V->AsString(),Family.Value);if(!Check()){return false;}
                        if(!Reused||(MatchingOwned&&Reused!=Plan.Object)){Error=TEXT("Exact held same-target matching-family class-export reference required; foreign/plain/forward paths rejected");return false;}Plan.Object=Reused;Plan.Class=Reused->GetClass();
                    }
                    else if(V->Type==EJson::Object)
                    {
                        const auto Literal=V->AsObject();FString Path;if(!Literal->TryGetStringField(TEXT("_ClassName"),Path)){Error=TEXT("Exact _ClassName required");return false;}UClass* C=FindObject<UClass>(nullptr,*Path);const int32 K=OwnedRead->Kind(C);
                        if(!Loaded(C)||K==INDEX_NONE||!Path.Equals(C->GetPathName(),ESearchCase::CaseSensitive)||OwnedRead->TriggerKind(K)!=Family.Value||(!MatchingOwned&&!OwnedRead->WriterKind(K))||(MatchingOwned&&(C!=Plan.Class||Seen.Contains(Plan.Object)))){Error=TEXT("Finite native class/family or first-literal identity mismatch");return false;}Plan.Class=C;
                        if(!MatchingOwned&&OwnedRead->Owned.Num()+(++NewCount)>64){Error=TEXT("Old held objects plus planned new literals exceed64; no allocation or cleanup");return false;}
                        auto ActualFields=MakeShared<FJsonObject>();UObject* Source=MatchingOwned?Plan.Object:C->GetDefaultObject(false);
                        if(!Loaded(Source)){Error=TEXT("Existing exact native source required");return false;}const bool Exported=FJsonObjectConverter::UStructToJsonObject(C,Source,ActualFields,0,0);if(!Check()){return false;}
                        if(!Exported||Literal->Values.Num()!=ActualFields->Values.Num()+1){Error=TEXT("Complete canonical lowerCamel owned fields required");return false;}
                        for(const auto& Field:ActualFields->Values)
                        {
                            const FString Name(Field.Key.ToView());const auto Supplied=Literal->TryGetField(Name);if(!Supplied){Error=TEXT("Exact canonical field key required");return false;}
                            if(MatchingOwned){if(!Equal(Supplied,Field.Value)){Error=TEXT("Existing owned fields are immutable in matching reuse");return false;}continue;}
                            FProperty* P=C->FindPropertyByName(FName(*Name));if(!P){Error=TEXT("Native field unavailable");return false;}
                            if(P->GetFName()==TEXT("LastValue")){if(Supplied->Type!=EJson::Object||!Supplied->AsObject()->Values.IsEmpty()||!Equal(Supplied,Field.Value)){Error=TEXT("Trigger LastValue must retain the native reflected empty object; private payload is not reset or verified");return false;}continue;}
                            if(OwnedRead->TriggerKind(K)&&P->GetFName()==TEXT("bShouldAlwaysTick")){if(Supplied->Type!=EJson::Boolean||Supplied->AsBool()||Field.Value->Type!=EJson::Boolean||Field.Value->AsBool()){Error=TEXT("Trigger bShouldAlwaysTick must retain its exact native false bookkeeping default");return false;}continue;}
                            if(K==2&&Supplied->Type==EJson::Null){continue;}UObject* PlannedCurveObject=nullptr;
                            bool Allowed=false;
                            if(K==2){auto* ResponseProperty=CastField<FObjectPropertyBase>(P);Allowed=ResponseProperty&&ResponseProperty->PropertyClass.Get()==FindObject<UClass>(nullptr,TEXT("/Script/Engine.CurveFloat"))&&OwnedRead->CurveReference(Supplied,PlannedCurveObject,false);}
                            else if(P->IsA<FFloatProperty>()){Allowed=Supplied->Type==EJson::Number&&FMath::IsFinite(Supplied->AsNumber())&&FMath::Abs(Supplied->AsNumber())<=double(MAX_flt);}
                            else if(P->IsA<FBoolProperty>()){Allowed=Supplied->Type==EJson::Boolean&&!(K==7&&Supplied->AsBool());}
                            else if(FEnumProperty* E=CastField<FEnumProperty>(P)){if(Supplied->Type==EJson::String){for(int32 J=0;J<E->GetEnum()->NumEnums();++J){const FString Token=E->GetEnum()->GetAuthoredNameStringByValue(E->GetEnum()->GetValueByIndex(J));const bool Known=K==0?(Token==TEXT("Axial")||Token==TEXT("Radial")):K==9?(Token==TEXT("Standard")||Token==TEXT("UE4_BackCompat")):(Token==TEXT("YXZ")||Token==TEXT("ZYX")||Token==TEXT("XZY")||Token==TEXT("YZX")||Token==TEXT("ZXY"));Allowed|=Known&&Supplied->AsString().Equals(Token,ESearchCase::CaseSensitive);}}}
                            else if(FStructProperty* F=CastField<FStructProperty>(P)){if((K==5||K==8)&&F->Struct==TBaseStructure<FVector>::Get()&&Supplied->Type==EJson::Object&&Supplied->AsObject()->Values.Num()==3){Allowed=true;for(const TCHAR* Axis:{TEXT("x"),TEXT("y"),TEXT("z")}){const auto Q=Supplied->AsObject()->TryGetField(Axis);Allowed&=Q&&Q->Type==EJson::Number&&FMath::IsFinite(Q->AsNumber());}}}
                            if(!Allowed||!P->HasAnyPropertyFlags(CPF_Edit)){Error=TEXT("Finite declared writable scalar/vector/authored-enum field required");return false;}
                            FOwnedFieldPlan FP;FP.Property=P;FP.Value=MakeUnique<FDefaultConstructedPropertyElement>(P);Count(R,TEXT("owned_field_plan_conversion_calls"));bool Converted=false;if(K==2){auto* ResponseProperty=CastField<FObjectPropertyBase>(P);ResponseProperty->SetObjectPropertyValue(FP.Value->GetObjAddress(),PlannedCurveObject);Converted=ResponseProperty->GetObjectPropertyValue(FP.Value->GetObjAddress())==PlannedCurveObject;}else{Converted=FJsonObjectConverter::JsonValueToUProperty(Supplied,P,FP.Value->GetObjAddress(),0,0);}if(!Check()){return false;}
                            const bool ExportOK=Converted&&P->ExportText_Direct(FP.Canonical,FP.Value->GetObjAddress(),FP.Value->GetObjAddress(),nullptr,PPF_None);if(!Check()){return false;}if(!ExportOK||FP.Canonical.Len()>65536){Error=TEXT("Finite native field conversion/observation failed before allocation");return false;}Plan.Fields.Add(MoveTemp(FP));
                        }
                        if(MatchingOwned){Seen.Add(Plan.Object);}
                    }
                    else{Error=TEXT("Owned slot requires exact literal or same-target earlier class-export path");return false;}
                    OwnedPlan.Add(MoveTemp(Plan));
                }
            }
        }
        Plain->SetArrayField(TEXT("mappings"),PlainRows);PlainMapping=MakeShared<FJsonValueObject>(Plain);R.Data->SetBoolField(TEXT("input_mapping_owned_matching_only"),MatchingOwned);R.Data->SetNumberField(TEXT("input_mapping_owned_planned_slots"),Total);return Check();
    }
    bool ConfigureOwned(FJsonObject& Attempt)
    {
        if(!OwnedWriter()||MatchingOwned){return true;}TArray<TSharedPtr<FJsonValue>> Prefix;
        for(auto& Plan:OwnedPlan)
        {
            if(!Check()){return false;}if(Plan.Object){continue;}auto Receipt=MakeShared<FJsonObject>();Prefix.Add(MakeShared<FJsonValueObject>(Receipt));R.Data->SetArrayField(TEXT("input_mapping_owned_creation_prefix"),Prefix);Receipt->SetStringField(TEXT("class"),Plan.Class->GetPathName());Receipt->SetNumberField(TEXT("row"),Plan.Row);Receipt->SetNumberField(TEXT("slot"),Plan.Slot);Receipt->SetBoolField(TEXT("trigger"),Plan.Trigger);Receipt->SetBoolField(TEXT("allocation_entered"),true);
            R.Known=false;Count(R,TEXT("owned_object_allocation_calls"));UObject* O=NewObject<UObject>(Target.Get(),Plan.Class,NAME_None,RF_Transactional);Receipt->SetBoolField(TEXT("allocation_returned"),true);
            Receipt->SetBoolField(TEXT("returned_object_is_null"),O==nullptr);Receipt->SetBoolField(TEXT("returned_identity_observed"),false);
            OwnedRead->Allocating=O;OwnedRead->AllocatingClass=Plan.Class;
            if(!O||!OwnedRead->Pin(O)){R.Data->SetBoolField(TEXT("owned_allocation_endpoint_complete"),false);return Stop(TEXT("data_asset_mapping_owned_allocation_unknown"),TEXT("Allocation identity unavailable; null/non-null return receipt and prior observed identities retained, no next explicit writer/save."));}
            Plan.Object=O;CreatedOwned.Add(O);Receipt->SetObjectField(TEXT("identity"),Identity(O));Receipt->SetBoolField(TEXT("returned_identity_observed"),true);R.Data->SetNumberField(TEXT("owned_objects_created_count"),CreatedOwned.Num());
            if(!Current()||!Check()||!OwnedRead->AcceptAllocation(O,Plan.Class)){return Stop(TEXT("data_asset_mapping_owned_allocation_unknown"),TEXT("Complete source/target allocation endpoint unavailable; actual returned identity/prefix retained, no next explicit writer/save."));}
            if(!Check()){return false;}
            if(OwnedRead->TriggerKind(OwnedRead->Kind(Plan.Class)))
            {FBoolProperty* Tick=FindFProperty<FBoolProperty>(Plan.Class,TEXT("bShouldAlwaysTick"));if(!Tick||Tick->GetPropertyValue_InContainer(O)){return Stop(TEXT("data_asset_mapping_owned_trigger_default_changed"),TEXT("Returned trigger bookkeeping differs from planned false native default; allocation prefix retained."));}}
            TArray<TSharedPtr<FJsonValue>> Fields;
            for(const auto& F:Plan.Fields)
            {
                if(!Check()||!OwnedRead->BeginCurveField(O,F.Property,F.Value->GetObjAddress())){return false;}auto Written=MakeShared<FJsonObject>();Fields.Add(MakeShared<FJsonValueObject>(Written));Receipt->SetArrayField(TEXT("field_copy_prefix"),Fields);Written->SetStringField(TEXT("field"),F.Property->GetName());Written->SetStringField(TEXT("canonical"),F.Canonical);Written->SetBoolField(TEXT("entered"),true);Count(R,TEXT("owned_field_storage_copy_calls"));
                F.Property->CopyCompleteValue(F.Property->ContainerPtrToValuePtr<void>(O),F.Value->GetObjAddress());Written->SetBoolField(TEXT("returned"),true);
                if(!OwnedRead->AcceptField(O,F.Property,F.Canonical)||!Check()){return Stop(TEXT("data_asset_mapping_owned_field_unknown"),TEXT("Actual field-copy prefix unavailable or altered another source/owned field; no next explicit writer/save."));}
            }
            Receipt->SetBoolField(TEXT("configured"),true);
        }
        Attempt.SetNumberField(TEXT("owned_objects_created"),CreatedOwned.Num());return Check();
    }
    bool AttachOwned(void* Data,TArray<FString>& Links,FString& Error)
    {
        if(!Check()){return false;}FScriptArrayHelper Array(Rows,Rows->ContainerPtrToValuePtr<void>(Data));
        for(const auto& Plan:OwnedPlan)
        {
            FArrayProperty* P=Plan.Trigger?Triggers:Modifiers;FObjectPropertyBase* Inner=CastField<FObjectPropertyBase>(P->Inner);if(Plan.Row>=Array.Num()||!Inner||!Loaded(Plan.Object)){Error=TEXT("Owned scratch row/pointer unavailable");return false;}
            FScriptArrayHelper Slots(P,P->ContainerPtrToValuePtr<void>(Array.GetRawPtr(Plan.Row)));if(Plan.Slot!=Slots.Num()){Error=TEXT("Owned scratch slot order mismatch");return false;}Slots.AddValue();Inner->SetObjectPropertyValue(Slots.GetRawPtr(Plan.Slot),Plan.Object);if(!Check()){return false;}
        }
        Links.Add(TEXT("rows#")+LexToString(Array.Num()));
        for(int32 I=0;I<Array.Num();++I){for(const auto& Family:TArray<TPair<FArrayProperty*,bool>>{{Triggers,true},{Modifiers,false}}){FScriptArrayHelper Slots(Family.Key,Family.Key->ContainerPtrToValuePtr<void>(Array.GetRawPtr(I)));FObjectPropertyBase* Inner=CastField<FObjectPropertyBase>(Family.Key->Inner);const FString At=TEXT("Mappings/")+LexToString(I)+(Family.Value?TEXT("/Triggers"):TEXT("/Modifiers"));Links.Add(At+TEXT("#")+LexToString(Slots.Num()));for(int32 J=0;J<Slots.Num();++J){Links.Add(At+TEXT("/")+LexToString(J)+TEXT("=")+Inner->GetObjectPropertyValue(Slots.GetRawPtr(J))->GetPathName());}}if(OwnedRead&&OwnedRead->PlayerSettingsHost&&!OwnedRead->PlayerSettingsLink(CastField<FObjectProperty>(Settings),Array.GetRawPtr(I),TEXT("Mappings/")+LexToString(I)+TEXT("/PlayerSettings"),Links))return false;}return Check();
    }
    bool Request(const TSharedPtr<FJsonValue>& Value,FString& Error)
    {
        if(!Value.IsValid()||Value->Type!=EJson::Object){Error=TEXT("DefaultKeyMappings requires its native JSON object wrapper");return false;}const auto Wrapper=Value->AsObject();const TArray<TSharedPtr<FJsonValue>>* Entries=nullptr;
        if(Wrapper->Values.Num()!=1){Error=TEXT("DefaultKeyMappings supports only its Mappings array");return false;}for(const auto& Pair:Wrapper->Values){if(FName(*FString(Pair.Key.ToView()))!=TEXT("Mappings")||Pair.Value->Type!=EJson::Array){Error=TEXT("Use the actual DefaultKeyMappings.Mappings native array");return false;}Entries=&Pair.Value->AsArray();}if(!Entries||Entries->Num()>64){Error=TEXT("At most64 mapping rows");return false;}
        if(OwnedRead&&OwnedRead->ReferenceClear){if(!Entries->IsEmpty()){Error=TEXT("Reference-clear mode accepts only an explicit empty Mappings array; no replacement");return false;}return Check();}
        for(const auto& Entry:*Entries)
        {
            if(!Current()){return false;}if(!Entry.IsValid()||Entry->Type!=EJson::Object){Error=TEXT("Each mapping row must be a native JSON object");return false;}TSet<FName> Seen;bool HasAction=false,HasKey=false;
            for(const auto& Pair:Entry->AsObject()->Values)
            {
                const FString Name(Pair.Key.ToView());if(Name.Len()>=NAME_SIZE||HasNul(Name)||!Pair.Value.IsValid()){Error=TEXT("Bounded known row fields required");return false;}const FName N(*Name);if(Seen.Contains(N)){Error=TEXT("Duplicate row field aliases unsupported");return false;}Seen.Add(N);
                if(N==TEXT("Action"))
                {
                    HasAction=true;if(Pair.Value->Type!=EJson::String){Error=TEXT("Action requires an already-loaded native object path/export string");return false;}const FString TextValue=Pair.Value->AsString(),Path=FPackageName::ExportTextPathToObjectPath(TextValue);if(Path.Len()>=1024||!Path.StartsWith(TEXT("/Game/"))||!FPackageName::IsValidObjectPath(Path)||Path.Contains(TEXT(":"))||!Clean(Path)||(TextValue!=Path&&TextValue!=FString::Printf(TEXT("/Script/EnhancedInput.InputAction'%s'"),*Path))){Error=TEXT("Exact native Game InputAction object reference required");return false;}UObject* O=FindObject<UObject>(nullptr,*Path);if(!Loaded(O)||!AdmitAction(Cast<UDataAsset>(O),Error)){if(Error.IsEmpty()){Error=TEXT("Action source is not currently available");}return false;}
                }
                else if(N==TEXT("Key")){HasKey=true;if(Pair.Value->Type!=EJson::String||!KeyAllowed(FKey(*Pair.Value->AsString()))||!Pair.Value->AsString().Equals(FKey(*Pair.Value->AsString()).ToString(),ESearchCase::CaseSensitive)){Error=TEXT("Use one of the nine source-backed registered digital/axis key names");return false;}}
                else if(N==TEXT("Triggers")||N==TEXT("Modifiers")){if(Pair.Value->Type!=EJson::Array||Pair.Value->AsArray().Num()>64||(!OwnedWriter()&&!Pair.Value->AsArray().IsEmpty())){Error=TEXT("Empty arrays or the finite Context-owned writer slot contract required");return false;}}
                else if(N==TEXT("SettingBehavior")){if(Pair.Value->Type!=EJson::String||!(Pair.Value->AsString()==TEXT("InheritSettingsFromAction")||(OwnedRead&&OwnedRead->PlayerSettingsHost&&Pair.Value->AsString()==TEXT("OverrideSettings")))){Error=TEXT("Inherited settings, or OverrideSettings with an exact held same-owner reference, required");return false;}}
                else if(N==TEXT("PlayerMappableKeySettings")){if(Pair.Value->Type!=EJson::Null&&(Pair.Value->Type!=EJson::String||(Pair.Value->AsString()!=TEXT("None")&&(!OwnedRead||!OwnedRead->PlayerSettingsHost||!OwnedRead->ReusePlayerSettings(Pair.Value->AsString()))))){Error=TEXT("Null or exact held same-owner native settings class-export required; inline/foreign references remain incomplete");return false;}}
                else{Error=TEXT("Unknown/transient/custom mapping row field unsupported");return false;}
            }if(!HasAction||!HasKey){Error=TEXT("Each mapping row requires Action and Key");return false;}
            if(OwnedRead&&OwnedRead->PlayerSettingsHost)
            {
                FString RequestedBehavior=TEXT("InheritSettingsFromAction");TSharedPtr<FJsonValue> RequestedSettings;
                for(const auto& Pair:Entry->AsObject()->Values){const FName Name(*FString(Pair.Key.ToView()));if(Name==TEXT("SettingBehavior"))RequestedBehavior=Pair.Value->AsString();else if(Name==TEXT("PlayerMappableKeySettings"))RequestedSettings=Pair.Value;}
                const bool NonNull=RequestedSettings&&RequestedSettings->Type==EJson::String&&RequestedSettings->AsString()!=TEXT("None");
                if((RequestedBehavior==TEXT("OverrideSettings"))!=NonNull){Error=TEXT("First explicit settings writer requires OverrideSettings with one nonnull same-owner settings reference, or inherited/null settings");return false;}
            }
        }return (!OwnedWriter()||PlanOwned(Value,Error))&&Check();
    }
};
bool MappingAdmit(FMappingContextScope* Scope){return !Scope||Scope->Current();}
void MappingExportDiagnostic(FMappingContextScope* Scope,UObject* Object,FProperty* Property,bool Before,bool Performed,int32 Length,bool CurrentOK,bool LimitOK)
{if(!Scope){return;}if(Before){Scope->BeginExport(Object,Property,TEXT("native_source_field"));}else{Scope->EndExport(Performed,Length,CurrentOK,LimitOK);}}

struct FContext { TStrongObjectPtr<UDataAsset> Asset; UClass* Class = nullptr; UClass* Factory = nullptr; FString Path, Package;bool PermitPreview=false,ActionReferenceClear=false,MappingReferenceClear=false,SingleActionArrayNull=false,GeneratedLoaded=false,NativeLoaded=false;TStrongObjectPtr<UEditorEngine> LoadEditor;TStrongObjectPtr<UWorld> LoadWorld;TStrongObjectPtr<UClass> LoadClass;TStrongObjectPtr<UPackage> LoadPackage;TSharedPtr<FJsonObject> MappingOwnedRequest,ActionOwnedRequest;TUniquePtr<FInputOwnedReadScope> OwnedRead;TUniquePtr<FPreviewCollectionScope> Preview;TUniquePtr<FDeformerCollectionScope> Deformer;TUniquePtr<FInputActionScope> Input;TUniquePtr<FMappingContextScope> Mapping; };
struct FObservation
{
    bool Complete = false;
    TMap<FString,TSet<FString>> ReferenceContributors; // ReferenceFix1: plain provenance from the existing per-object finder only.
    TSharedRef<FJsonObject> State = MakeShared<FJsonObject>(), Source = MakeShared<FJsonObject>(), Refs = MakeShared<FJsonObject>(), File = MakeShared<FJsonObject>(), Other = MakeShared<FJsonObject>();
};
TSharedRef<FJsonObject> Observation(const FObservation& O)
{ auto J = MakeShared<FJsonObject>(); J->SetBoolField(TEXT("complete"), O.Complete); J->SetObjectField(TEXT("state"), O.State); J->SetObjectField(TEXT("native_sources"), O.Source); J->SetObjectField(TEXT("references"), O.Refs); J->SetObjectField(TEXT("disk"), O.File); J->SetObjectField(TEXT("other_package_dirty"), O.Other); return J; }
bool Capture(FContext& C, FObservation& O, FResult& R, const TCHAR* Name)
{
    O = FObservation{}; R.Data->SetObjectField(Name, Observation(O)); Count(R, TEXT("observation_capture_calls"));if(!PreviewAdmit(C.Preview.Get())||!DeformerAdmit(C.Deformer.Get())||!InputAdmit(C.Input.Get())||!MappingAdmit(C.Mapping.Get())){return false;} UDataAsset* A = C.Asset.Get();
    if (!Loaded(A) || !Loaded(C.Class) || A->GetClass() != C.Class || (C.Class->GetPathName()!=ClassPath&&!(C.PermitPreview&&C.Class==UPreviewMeshCollection::StaticClass())&&!(C.Input&&C.Class==C.Input->Class.Get())&&!(C.Mapping&&C.Class==C.Mapping->Class.Get())) || A->GetPathName() != C.Path || FindObject<UDataAsset>(nullptr, *C.Path) != A || A->GetOuter() != A->GetOutermost() || A->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_Transient)) { return false; }
    O.State->SetObjectField(TEXT("identity"), Identity(A)); O.State->SetBoolField(TEXT("package_dirty"), A->GetOutermost()->IsDirty());
    auto P = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("properties"), P); auto V = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("values"), V);
    if (!Fields(A, P,C.Preview.Get(),C.Deformer.Get(),C.Input.Get(),C.Mapping.Get())) { return false; } Count(R, TEXT("json_export_calls"));const bool Exported=FJsonObjectConverter::UStructToJsonObject(C.Class,A,V,0,0);if(!PreviewAdmit(C.Preview.Get())||!DeformerAdmit(C.Deformer.Get())||!InputAdmit(C.Input.Get())||!MappingAdmit(C.Mapping.Get())||!Exported){return false;}
    int32 N = 0, Chars = 0; if (!Bounded(MakeShared<FJsonValueObject>(V), N, Chars) || !Clean(Text(O.State)) || !Sources(C.Class, C.Factory, O.Source,false,C.Preview.Get(),C.Deformer.Get(),C.Input.Get(),C.Mapping.Get())) { return false; }
    TArray<UObject*> Owned; GetObjectsWithOuter(A, Owned, EGetObjectsFlags::IncludeNestedObjects); Owned.Insert(A, 0); if (Owned.Num() > 256) { return false; } auto Exports = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("owned_exports"), Exports); int32 CountRefs = 0;
    for (UObject* X : Owned)
    {
        if (!Loaded(X) || !Clean(X->GetPathName()) || !Clean(X->GetClass()->GetPathName())) { return false; }
        if (X != A) { auto Row = MakeShared<FJsonObject>(); Exports->SetObjectField(X->GetPathName(), Row); Row->SetObjectField(TEXT("identity"), Identity(X)); auto XP = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("properties"), XP); if (!Fields(X, XP,C.Preview.Get(),C.Deformer.Get(),C.Input.Get(),C.Mapping.Get())) { return false; } }
        TArray<UObject*> Refs; FReferenceFinder Finder(Refs, nullptr, false, true, false, true); Finder.FindReferences(X);if(!PreviewAdmit(C.Preview.Get())||!DeformerAdmit(C.Deformer.Get())||!InputAdmit(C.Input.Get())||!MappingAdmit(C.Mapping.Get())){return false;}
        for (UObject* Ref : Refs) { if (++CountRefs > 16384 || !Loaded(Ref) || !Clean(Ref->GetPathName()) || !Clean(Ref->GetClass()->GetPathName())) { return false; } O.Refs->SetObjectField(Ref->GetPathName(), Identity(Ref));if(C.OwnedRead&&(C.OwnedRead->ReferenceClear||C.OwnedRead->ActionSet||C.OwnedRead->MappingSet)){O.ReferenceContributors.FindOrAdd(Ref->GetPathName()).Add(X->GetPathName());} }
    }
    if (!Disk(C.Package, O.File) || !Dirty(C.Package, O.Other)||!PreviewAdmit(C.Preview.Get())||!DeformerAdmit(C.Deformer.Get())||!InputAdmit(C.Input.Get())||!MappingAdmit(C.Mapping.Get())||(C.Input&&!C.Input->Check())||(C.Mapping&&!C.Mapping->Check())||(C.OwnedRead&&!C.OwnedRead->Check())) { return false; } O.Complete = true; R.Data->SetObjectField(Name, Observation(O)); return true;
}
bool Path(const FString& Requested, bool Mutable, FContext& C, FResult& R)
{
    R.Data->SetStringField(TEXT("requested_asset_path"), Requested);
    if (!IsEditorGameThread() || IsPlaySessionActive()) { return Fail(R, TEXT("data_asset_editor_context_required"), TEXT("Editor game thread without PIE/SIE required.")); }
    if (Requested.IsEmpty() || Requested.Len() > 2048 || HasNul(Requested)) { return Fail(R, TEXT("data_asset_path_invalid"), TEXT("Bounded original asset path required.")); }
    C.Path = FPackageName::ExportTextPathToObjectPath(Requested).TrimStartAndEnd(); C.Path.TrimQuotesInline(); C.Path.ReplaceInline(TEXT("\\"), TEXT("/"));
    if (!C.Path.Contains(TEXT("."))) { C.Path += TEXT(".") + FPackageName::GetShortName(C.Path); } C.Package = FPackageName::ObjectPathToPackageName(C.Path);
    if (C.Path.Len() >= 1024 || !FPackageName::IsValidObjectPath(C.Path) || C.Path.Contains(TEXT(":")) || !Clean(C.Path) || !(C.Package.StartsWith(TEXT("/Game/")) || (!Mutable && C.Package.StartsWith(TEXT("/Engine/")))))
    { return Fail(R, TEXT("data_asset_path_invalid"), TEXT("Exact Game/Engine object identity, Game-only writes, and no tool dependency path required.")); }
    R.Data->SetStringField(TEXT("data_asset"), C.Path); return true;
}
// RL: only the original primary LoadAsset may publish this generated handoff.
bool GeneratedLoadContextCurrent(const FContext& C)
{
    return IsEditorGameThread()&&Loaded(C.LoadEditor.Get())&&GEditor==C.LoadEditor.Get()&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&!IsPlaySessionActive()&&Loaded(C.LoadWorld.Get())&&GEditor->GetEditorWorldContext().World()==C.LoadWorld.Get()&&C.LoadWorld->WorldType==EWorldType::Editor&&!C.LoadWorld->bIsTearingDown;
}
bool Load(FContext& C, FResult& R, const TCHAR* Stage,bool PermitGenerated=false)
{
    if(!DeformerAdmit(C.Deformer.Get())||!InputAdmit(C.Input.Get())||!MappingAdmit(C.Mapping.Get())){return false;}
    if(C.PermitPreview&&!C.Preview&&!C.Input&&!C.Mapping){C.Preview=MakeUnique<FPreviewCollectionScope>(R);if(!C.Preview->Start()){return false;}}
    if(PermitGenerated)
    {
        if(!IsEditorGameThread()||!Loaded(GEditor)||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||IsPlaySessionActive())return Fail(R,TEXT("data_asset_load_editor_required"),TEXT("Idle Editor required before the original LoadAsset call."));
        C.LoadEditor.Reset(GEditor);C.LoadWorld.Reset(GEditor->GetEditorWorldContext().World());if(!GeneratedLoadContextCurrent(C))return Fail(R,TEXT("data_asset_load_world_required"),TEXT("Current live Editor world required before LoadAsset."));
    }
    R.Stage = Stage; auto J = MakeShared<FJsonObject>(); auto Calls = R.Data->GetArrayField(TEXT("asset_load_calls")); Calls.Add(MakeShared<FJsonValueObject>(J)); R.Data->SetArrayField(TEXT("asset_load_calls"), Calls);
    J->SetStringField(TEXT("route"), TEXT("UEditorAssetLibrary::LoadAsset")); J->SetStringField(TEXT("stage"), Stage); J->SetStringField(TEXT("asset_path"), C.Path); J->SetBoolField(TEXT("attempted"), true); Count(R, TEXT("asset_load_count"));
    UObject* Object = UEditorAssetLibrary::LoadAsset(C.Path);TStrongObjectPtr<UObject> ReturnedAssetHold(Object);J->SetBoolField(TEXT("returned"),true);J->SetBoolField(TEXT("returned_nonnull"),Object!=nullptr);
    if(PermitGenerated&&!GeneratedLoadContextCurrent(C))return Fail(R,TEXT("data_asset_load_context_changed"),TEXT("Original LoadAsset returned outside its held Editor/world boundary; no returned-object getter or next SDK."),true);
    if(!PreviewAdmit(C.Preview.Get())||!DeformerAdmit(C.Deformer.Get())||!InputAdmit(C.Input.Get())||!MappingAdmit(C.Mapping.Get())){return false;}
    TStrongObjectPtr<UClass> ReturnedClassHold;
    if(PermitGenerated&&Object)
    {
        if(!Loaded(Object))return Fail(R,TEXT("data_asset_load_target_unavailable"),TEXT("Returned asset is not a complete live object after LoadAsset; no retry."),true);
        ReturnedClassHold.Reset(Object->GetClass());if(!Loaded(ReturnedClassHold.Get()))return Fail(R,TEXT("data_asset_load_class_unavailable"),TEXT("Returned asset class is not current after LoadAsset; no retry."),true);
    }
    UDataAsset* A = Cast<UDataAsset>(Object);
    if(PermitGenerated&&A&&!ReturnedClassHold->HasAnyClassFlags(CLASS_Native))
    {
        // No new loader or baseline before PostLoad is claimed. The existing generated work binds fresh source/default/payload roles next.
        TStrongObjectPtr<UObject> ReturnedOuterHold(A->GetOuter());if(!Loaded(ReturnedOuterHold.Get())||!Loaded(ReturnedOuterHold->GetClass()))return Fail(R,TEXT("data_asset_generated_load_outer_unavailable"),TEXT("Returned top-level owner must be live before its package role is read."),true);C.LoadPackage.Reset(Cast<UPackage>(ReturnedOuterHold.Get()));
        if(!Loaded(C.LoadPackage.Get())||A->GetOuter()!=C.LoadPackage.Get()||A->GetPackage()!=C.LoadPackage.Get()||C.LoadPackage->GetName()!=C.Package||C.LoadPackage->HasAnyPackageFlags(PKG_CompiledIn|PKG_PlayInEditor|PKG_EditorOnly)||!A->GetPathName().Equals(C.Path,ESearchCase::CaseSensitive)||FindObject<UDataAsset>(nullptr,*C.Path)!=A)
            return Fail(R,TEXT("data_asset_generated_load_identity_changed"),TEXT("Original LoadAsset result must still be the exact top-level object/package selected by this request; no retry or result substitution."),true);
        if(C.Asset.IsValid()&&C.Asset.Get()!=A)return Fail(R,TEXT("data_asset_target_relocated"),TEXT("Original LoadAsset route returned another current target; no retry."),true);
        J->SetObjectField(TEXT("returned_object"),Identity(A));C.Asset.Reset(A);C.LoadClass.Reset(ReturnedClassHold.Get());C.Class=C.LoadClass.Get();C.GeneratedLoaded=true;
        R.Data->SetBoolField(TEXT("generated_target_load_handoff"),true);R.Data->SetStringField(TEXT("generated_target_load_route"),TEXT("original_LoadAsset_return_then_current_generated_source_target_binding"));
        C.Preview.Reset();return true;
    }
    if(PermitGenerated&&Loaded(A)&&ReturnedClassHold->HasAnyClassFlags(CLASS_Native)&&ReturnedClassHold->GetPathName()!=ClassPath&&ReturnedClassHold->GetPathName()!=PreviewClassPath&&ReturnedClassHold->GetPathName()!=InputActionClassPath&&ReturnedClassHold->GetPathName()!=MappingContextClassPath)
    {
        TStrongObjectPtr<UObject> ReturnedOuter(A->GetOuter());if(!Loaded(ReturnedOuter.Get())||!Loaded(ReturnedOuter->GetClass()))return Fail(R,TEXT("data_asset_native_load_outer_unavailable"),TEXT("Native primary result owner unavailable; no next getter."),true);C.LoadPackage.Reset(Cast<UPackage>(ReturnedOuter.Get()));
        if(!Loaded(C.LoadPackage.Get())||A->GetOuter()!=C.LoadPackage.Get()||A->GetPackage()!=C.LoadPackage.Get()||C.LoadPackage->GetName()!=C.Package||C.LoadPackage->HasAnyPackageFlags(PKG_CompiledIn|PKG_PlayInEditor|PKG_EditorOnly)||!A->GetPathName().Equals(C.Path,ESearchCase::CaseSensitive)||FindObject<UDataAsset>(nullptr,*C.Path)!=A)return Fail(R,TEXT("data_asset_native_load_identity_changed"),TEXT("Original primary result must retain exact native class/outer/package/path; no replacement or retry."),true);
        if(C.Asset.IsValid()&&C.Asset.Get()!=A)return Fail(R,TEXT("data_asset_target_relocated"),TEXT("Primary result differs from prior retained target."),true);
        J->SetObjectField(TEXT("returned_object"),Identity(A));C.Asset.Reset(A);C.LoadClass.Reset(ReturnedClassHold.Get());C.Class=C.LoadClass.Get();C.NativeLoaded=true;R.Data->SetBoolField(TEXT("generic_native_target_load_handoff"),true);C.Preview.Reset();C.Deformer.Reset();C.Input.Reset();C.Mapping.Reset();C.OwnedRead.Reset();return true;
    }
    if(C.Deformer&&Loaded(A)){if(A->GetClass()==UMeshDeformerCollection::StaticClass()){if(!C.Deformer->BindTarget(A,C.Path)){J->SetBoolField(TEXT("returned"),true);return false;}}else if(A->GetClass()==UPreviewMeshCollection::StaticClass()){C.Deformer.Reset();}}
    if(Loaded(A)&&A->GetClass()->GetPathName()==InputActionClassPath)
    {
        if(C.SingleActionArrayNull){J->SetBoolField(TEXT("returned"),true);R.Stage=TEXT("preflight");R.Data->SetBoolField(TEXT("native_input_owned_reference_clear_rejected_null"),true);return Fail(R,TEXT("data_asset_owned_clear_array_invalid"),TEXT("A sole exact Action Modifiers/Triggers null request is not an explicit empty array; no Modify/transaction/save or normalization."));}
        C.Preview.Reset();C.Deformer.Reset();if(!C.Input){bool PlayerHost=false;if(R.Op==TEXT("set_data_asset_properties")){auto* P=FindFProperty<FObjectProperty>(A->GetClass(),TEXT("PlayerMappableKeySettings"));if(P&&P->GetOwnerStruct()==A->GetClass()&&P->ArrayDim==1&&P->PropertyClass.IsResolved()&&Loaded(P->PropertyClass.Get())&&P->PropertyClass->GetPathName()==PlayerSettingsClassPath){const auto V=P->GetObjectPtrPropertyValue_InContainer(A);if(!V.IsResolved()){J->SetBoolField(TEXT("returned"),true);return Fail(R,TEXT("data_asset_player_settings_unresolved"),TEXT("Populated settings handle must already be resolved; no lazy load."));}PlayerHost=V.Get()!=nullptr;}}if(R.Op==TEXT("get_data_asset")||C.ActionOwnedRequest.IsValid()||PlayerHost){C.OwnedRead=MakeUnique<FInputOwnedReadScope>(R,false,C.ActionOwnedRequest.IsValid()||PlayerHost,C.ActionReferenceClear);C.OwnedRead->PlayerSettingsHost=PlayerHost;if(C.ActionReferenceClear){C.OwnedRead->DescribeReferenceClear();}if(C.ActionOwnedRequest.IsValid()&&!C.OwnedRead->PrepareActionWriteClasses(C.ActionOwnedRequest.ToSharedRef(),A)){J->SetBoolField(TEXT("returned"),true);return false;}}C.Input=MakeUnique<FInputActionScope>(R,false,C.OwnedRead.Get());if(!C.Input->Start()){J->SetBoolField(TEXT("returned"),true);return false;}}
        if(!C.Input->FreshTarget(A)||(C.OwnedRead&&!C.OwnedRead->Bind(A))||!C.Input->Bind(A,C.Path)){J->SetBoolField(TEXT("returned"),true);return false;}
    }
    if(Loaded(A)&&A->GetClass()->GetPathName()==MappingContextClassPath)
    {
        C.Preview.Reset();C.Deformer.Reset();if(!C.Mapping){if(R.Op==TEXT("get_data_asset")||C.MappingOwnedRequest.IsValid()){C.OwnedRead=MakeUnique<FInputOwnedReadScope>(R,C.MappingOwnedRequest.IsValid(),false,C.MappingReferenceClear);if(C.MappingReferenceClear){C.OwnedRead->DescribeReferenceClear();}if(C.MappingOwnedRequest.IsValid()&&!C.OwnedRead->PrepareWriteClasses(C.MappingOwnedRequest.ToSharedRef(),A)){J->SetBoolField(TEXT("returned"),true);return false;}}C.Mapping=MakeUnique<FMappingContextScope>(R,false,C.OwnedRead.Get());if(!C.Mapping->Start()){J->SetBoolField(TEXT("returned"),true);return false;}}
        if((C.OwnedRead&&!C.OwnedRead->Bind(A))||!C.Mapping->Bind(A,C.Path)){J->SetBoolField(TEXT("returned"),true);return false;}
    }
    if(C.OwnedRead&&C.OwnedRead->PlayerSettingsHost){R.Data->SetStringField(TEXT("admission_scope"),TEXT("Exact native Action/Context with immutable preexisting direct-owned native PlayerMappableKeySettings and null Metadata: populated Get, existing Action scalar writes with unchanged arrays, and Context inherited/null or OverrideSettings/exact same-owner class-export reference. Existing finite modifier/trigger paths remain separate."));R.Data->SetStringField(TEXT("player_settings_limits"),TEXT("No inline settings allocation, foreign/forward references, settings-field writes, IgnoreSettings writer, arbitrary Metadata, runtime evaluation or full legacy parity. PostLoad completes before the first baseline; deprecated tag migration and cold persistence of unlinked objects are not proven."));}
    J->SetBoolField(TEXT("returned"), true); J->SetObjectField(TEXT("returned_object"), Identity(Object));
    if (!Loaded(A) || A->GetPathName() != C.Path || (A->GetClass()->GetPathName()!=ClassPath&&!(C.PermitPreview&&A->GetClass()==UPreviewMeshCollection::StaticClass())&&!(C.Input&&A->GetClass()==C.Input->Class.Get())&&!(C.Mapping&&A->GetClass()==C.Mapping->Class.Get())) || A->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn | PKG_PlayInEditor | PKG_EditorOnly))
    { return Fail(R, TEXT("data_asset_type_unsupported"), TEXT("Exact supported Deformer/Preview/InputAction or finite native InputMappingContext required; arbitrary classes remain incomplete."), R.Attempted); }
    if (C.Asset.IsValid() && C.Asset.Get() != A) { return Fail(R, TEXT("data_asset_target_relocated"), TEXT("Original LoadAsset route returned another current target; no retry."), true); }
    if(A->GetClass()==UPreviewMeshCollection::StaticClass()){if(!C.Preview||!C.Preview->BindSource(A->GetClass())||!C.Preview->BindTarget(A,C.Path)){return false;}}else{C.Preview.Reset();}
    C.Asset.Reset(A); C.Class = A->GetClass(); return true;
}
TSharedRef<FJsonObject> LegacyGet(FContext& C, const FObservation& O)
{ auto J = MakeShared<FJsonObject>(); J->SetBoolField(TEXT("success"), O.Complete); J->SetStringField(TEXT("data_asset"), C.Path); J->SetStringField(TEXT("class"), C.Class->GetPathName()); J->SetObjectField(TEXT("values"), O.State->GetObjectField(TEXT("values")).ToSharedRef()); return J; }
TSharedPtr<FJsonValue> ValueByName(const TSharedRef<FJsonObject>& J, const FString& Name)
{ for (const auto& P : J->Values) { if (FName(*FString(P.Key.ToView())) == FName(*Name)) { return P.Value; } } return nullptr; }
void ClearFinal(FResult& R)
{ for (const TCHAR* K : { TEXT("observations_complete"), TEXT("source_preserved"), TEXT("references_preserved"), TEXT("unselected_state_preserved"), TEXT("other_package_dirty_preserved"), TEXT("persistence_verified") }) { R.Data->SetBoolField(K, false); } }
bool Fences(const FObservation& Before, const FObservation& After, bool Description, FResult& R,const TSet<FName>* Selected=nullptr)
{
    if (!Before.Complete || !After.Complete) { return Fail(R, TEXT("data_asset_observation_incomplete"), TEXT("Bounded actual prefix incomplete; no next writer/save."), true); }
    const bool Source = Equal(Before.Source, After.Source), Refs = Equal(Before.Refs, After.Refs), Other = Equal(Before.Other, After.Other), File = Equal(Before.File, After.File);
    bool State = Before.State->Values.Num() == After.State->Values.Num();
    for (const auto& P : Before.State->Values)
    {
        const FString K(P.Key.ToView()); if (K == TEXT("package_dirty")) { continue; }
        if ((Description||Selected) && (K == TEXT("values") || K == TEXT("properties")))
        { const auto X = P.Value->AsObject(), Y = After.State->GetObjectField(K); State &= X->Values.Num() == Y->Values.Num(); for (const auto& F : X->Values) { if (!(Description&&FName(*FString(F.Key.ToView()))==FName(TEXT("Description")))&&!(Selected&&Selected->Contains(FName(*FString(F.Key.ToView()))))) { State &= Equal(F.Value, Y->TryGetField(F.Key.ToView())); } } }
        else { State &= Equal(P.Value, After.State->TryGetField(P.Key.ToView())); }
    }
    R.Data->SetBoolField(TEXT("observations_complete"), true); R.Data->SetBoolField(TEXT("source_preserved"), Source); R.Data->SetBoolField(TEXT("references_preserved"), Refs); R.Data->SetBoolField(TEXT("unselected_state_preserved"), State); R.Data->SetBoolField(TEXT("other_package_dirty_preserved"), Other); R.Data->SetBoolField(TEXT("disk_preserved_before_save"), File);
    return (Source && Refs && Other && File && State) || Fail(R, TEXT("data_asset_preservation_failed"), TEXT("Complete callback prefix changed unselected fields/source/references/disk/other dirty state; save suppressed, no rollback."));
}
bool SavedEqual(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B, const FString& Path, const FString& At = FString())
{
    if (!A || !B || A->Type != B->Type) { return false; } if (At == TEXT("/package_dirty")) { return B->Type == EJson::Boolean && !B->AsBool(); }
    if (A->Type == EJson::Object)
    {
        const auto X = A->AsObject(), Y = B->AsObject(); if (X->Values.Num() != Y->Values.Num()) { return false; } FString Owner; const bool Own = X->TryGetStringField(TEXT("path"), Owner) && (Owner == Path || Owner.StartsWith(Path + TEXT(":")));
        for (const auto& P : X->Values)
        {
            const auto Other = Y->TryGetField(P.Key.ToView()); if (!Other) { return false; }
            if (Own && P.Key.ToView() == TEXT("object_flags")) { if (P.Value->Type != EJson::String || Other->Type != EJson::String) { return false; } const uint64 F = FCString::Strtoui64(*P.Value->AsString(), nullptr, 10); if (LexToString(F) != P.Value->AsString() || Other->AsString() != LexToString(F | static_cast<uint64>(RF_WasLoaded | RF_LoadCompleted))) { return false; } }
            else if (!SavedEqual(P.Value, Other, Path, At + TEXT("/") + FString(P.Key.ToView()))) { return false; }
        } return true;
    }
    if (A->Type == EJson::Array) { const auto& X = A->AsArray(); const auto& Y = B->AsArray(); if (X.Num() != Y.Num()) { return false; } for (int32 I = 0; I < X.Num(); ++I) { if (!SavedEqual(X[I], Y[I], Path, At + TEXT("/") + LexToString(I))) { return false; } } return true; } return Equal(A, B);
}
bool Save(FContext& C, const FObservation& Prepared, FResult& R)
{
    if (!R.SaveRequested) { return true; }
    R.Data->SetObjectField(TEXT("before_save_observation"), Observation(Prepared)); R.Data->SetBoolField(TEXT("before_save_observations_complete"), Prepared.Complete); R.Data->SetBoolField(TEXT("before_save_source_preserved"), R.Data->GetBoolField(TEXT("source_preserved"))); R.Data->SetBoolField(TEXT("before_save_unselected_state_preserved"), R.Data->GetBoolField(TEXT("unselected_state_preserved"))); ClearFinal(R);
    if (!PreviewAdmit(C.Preview.Get())||!DeformerAdmit(C.Deformer.Get())||!InputAdmit(C.Input.Get())||!MappingAdmit(C.Mapping.Get())){return false;}
    if (!Prepared.Complete || !C.Asset->GetOutermost()->IsFullyLoaded()) { return Fail(R, TEXT("data_asset_save_not_ready"), TEXT("Complete current instance and already fully loaded package required."), R.Attempted); }
    R.Stage = TEXT("save_asset"); R.Known = false; R.SaveAttempted = true; Count(R, TEXT("save_attempt_count")); auto J = MakeShared<FJsonObject>();
    J->SetStringField(TEXT("asset_path"), C.Path); J->SetStringField(TEXT("route"), TEXT("UEditorAssetLibrary::SaveLoadedAsset")); J->SetBoolField(TEXT("only_if_dirty"), false); J->SetBoolField(TEXT("attempted"), true); R.Data->SetArrayField(TEXT("save_calls"), { MakeShared<FJsonValueObject>(J) });
    R.Saved = UEditorAssetLibrary::SaveLoadedAsset(C.Asset.Get(), false); J->SetBoolField(TEXT("returned"), true); J->SetBoolField(TEXT("saved"), R.Saved); R.Data->SetBoolField(TEXT("saved"), R.Saved); return PreviewAdmit(C.Preview.Get())&&DeformerAdmit(C.Deformer.Get())&&InputAdmit(C.Input.Get())&&MappingAdmit(C.Mapping.Get())&&(!C.Input||C.Input->Check())&&(!C.Mapping||C.Mapping->Check());
}
bool Final(FContext& C, const FObservation& Prepared, FObservation& After, FResult& R)
{
    ClearFinal(R); R.Known = false; Count(R, TEXT("legacy_get_calls"));
    if (!Load(C, R, TEXT("legacy_get_after_load")) || !Capture(C, After, R, R.SaveAttempted ? TEXT("after_save_observation") : TEXT("after_observation")))
    { return Fail(R, TEXT("data_asset_final_readback_incomplete"), TEXT("Original final load/serialization incomplete; real conversion/callback/save prefix retained."), true); }
    R.Data->SetObjectField(TEXT("after"), LegacyGet(C, After));
    auto SavedStateView=After.State,SavedRefsView=After.Refs;
    if(R.Saved&&C.Mapping&&C.Mapping->OwnedWriter()&&!C.OwnedRead->Owned.IsEmpty())
    {
        if(!C.Mapping->Check()){return false;}SavedStateView=C.OwnedRead->CopySnapshot(After.State);SavedRefsView=C.OwnedRead->CopySnapshot(After.Refs);
        for(UObject* O:C.OwnedRead->Owned)
        {
            const FString Path=O->GetPathName();const auto Exports=SavedStateView->GetObjectField(TEXT("owned_exports"));
            if(!Exports->HasTypedField<EJson::Object>(Path)||!Exports->GetObjectField(Path)->HasTypedField<EJson::Object>(TEXT("identity"))){return C.Mapping->Stop(TEXT("data_asset_mapping_owned_saved_identity_missing"),TEXT("Saved admitted-owned identity absent; real save prefix retained."));}
            // SavePackages does not promise load flags on a newly allocated inline object.
            // Current already allows only unchanged or exact load-completed bits on held target identities.
            const FString Flags=LexToString(uint64(O->GetFlags()|RF_WasLoaded|RF_LoadCompleted));
            Exports->GetObjectField(Path)->GetObjectField(TEXT("identity"))->SetStringField(TEXT("object_flags"),Flags);
            if(SavedRefsView->HasTypedField<EJson::Object>(Path)){SavedRefsView->GetObjectField(Path)->SetStringField(TEXT("object_flags"),Flags);}
        }
        R.Data->SetStringField(TEXT("input_mapping_saved_owned_flag_comparison"),TEXT("Comparison views normalize only this Context writer's held admitted-owned identities after SavedTrue; actual after/ref flags remain recorded, unchanged or exact WasLoaded|LoadCompleted transition only. No other flag/reference/state allowance."));
    }
    if(R.Saved&&C.Input&&C.Input->OwnedWriter()&&!C.OwnedRead->Owned.IsEmpty())
    {
        if(!C.Input->Check()){return false;}SavedStateView=C.OwnedRead->CopySnapshot(After.State);SavedRefsView=C.OwnedRead->CopySnapshot(After.Refs);
        for(UObject* O:C.OwnedRead->Owned)
        {
            const FString Path=O->GetPathName();const auto Exports=SavedStateView->GetObjectField(TEXT("owned_exports"));if(!Exports->HasTypedField<EJson::Object>(Path)||!Exports->GetObjectField(Path)->HasTypedField<EJson::Object>(TEXT("identity"))){return C.Input->Stop(TEXT("data_asset_action_owned_saved_identity_missing"),TEXT("Saved admitted-owned identity absent; actual save prefix retained."));}
            const FString Flags=LexToString(uint64(O->GetFlags()|RF_WasLoaded|RF_LoadCompleted));Exports->GetObjectField(Path)->GetObjectField(TEXT("identity"))->SetStringField(TEXT("object_flags"),Flags);if(SavedRefsView->HasTypedField<EJson::Object>(Path)){SavedRefsView->GetObjectField(Path)->SetStringField(TEXT("object_flags"),Flags);}
        }R.Data->SetStringField(TEXT("input_action_saved_owned_flag_comparison"),TEXT("After SavedTrue only exact held Action-owned identity load bits use comparison views; actual after/ref evidence is unchanged. Current permits unchanged or exact WasLoaded|LoadCompleted transition only."));
    }
    const bool State = R.Saved ? SavedEqual(MakeShared<FJsonValueObject>(Prepared.State), MakeShared<FJsonValueObject>(SavedStateView), C.Path) : Equal(Prepared.State, After.State);
    const bool Source = Equal(Prepared.Source, After.Source), Refs = R.Saved ? SavedEqual(MakeShared<FJsonValueObject>(Prepared.Refs), MakeShared<FJsonValueObject>(SavedRefsView), C.Path) : Equal(Prepared.Refs, After.Refs), Other = Equal(Prepared.Other, After.Other);
    const bool File = R.Saved ? After.File->GetBoolField(TEXT("exists")) && After.File->GetNumberField(TEXT("bytes")) > 0 && !C.Asset->GetOutermost()->IsDirty() : Equal(Prepared.File, After.File);
    R.Known = true; R.Data->SetBoolField(TEXT("observations_complete"), true); R.Data->SetBoolField(TEXT("source_preserved"), Source); R.Data->SetBoolField(TEXT("references_preserved"), Refs); R.Data->SetBoolField(TEXT("unselected_state_preserved"), State); R.Data->SetBoolField(TEXT("other_package_dirty_preserved"), Other); R.Data->SetBoolField(TEXT("save_state_preserved"), State); R.Data->SetBoolField(TEXT("persistence_verified"), R.Saved && File);
    return (State && Source && Refs && Other && File) || Fail(R, TEXT("data_asset_final_preservation_failed"), TEXT("Complete final values/source/references/disk/other dirty fence failed; real save receipt retained, no replay."));
}
void Contract(FResult& R, const TSharedRef<FJsonObject>& Before, const TSharedRef<FJsonObject>& After, const TSharedRef<FJsonObject>& Delta, bool Performed, bool Changed, bool LegacySuccess, bool Post, const TCHAR* Method, const TArray<FString>& Checks)
{
    auto C = MakeShared<FJsonObject>(); C->SetStringField(TEXT("contract_version"), TEXT("mutation_result_v1")); C->SetStringField(TEXT("mode"), TEXT("apply")); C->SetBoolField(TEXT("dry_run"), false); C->SetBoolField(TEXT("DRY_RUN"), false); C->SetBoolField(TEXT("changed"), Changed); C->SetBoolField(TEXT("mutation_performed"), Performed);
    C->SetBoolField(TEXT("success"), LegacySuccess && Post); C->SetBoolField(TEXT("verified_success"), LegacySuccess && Post); C->SetBoolField(TEXT("postcondition_verified"), Post); C->SetObjectField(TEXT("before"), Before); C->SetObjectField(TEXT("after"), After);
    for (const TCHAR* K : { TEXT("affected_count"), TEXT("created_count"), TEXT("updated_count"), TEXT("removed_count"), TEXT("skipped_count"), TEXT("failed_count") }) { if (!Delta->HasField(K)) { Delta->SetField(K, MakeShared<FJsonValueNull>()); } }
    C->SetObjectField(TEXT("delta"), Delta); auto Mutation = MakeShared<FJsonObject>(); Mutation->SetBoolField(TEXT("attempted"), true); Mutation->SetBoolField(TEXT("performed"), Performed); Mutation->SetStringField(TEXT("state"), Performed ? TEXT("performed") : TEXT("not_performed")); C->SetObjectField(TEXT("mutation"), Mutation);
    auto V = MakeShared<FJsonObject>(); V->SetStringField(TEXT("status"), Post ? TEXT("passed") : TEXT("failed")); V->SetStringField(TEXT("method"), Method); V->SetBoolField(TEXT("before_observed"), true); V->SetBoolField(TEXT("after_observed"), true); V->SetBoolField(TEXT("strict"), true); TArray<TSharedPtr<FJsonValue>> Rows; for (const FString& Check : Checks) { Rows.Add(MakeShared<FJsonValueString>(Check)); } V->SetArrayField(TEXT("checks"), Rows); C->SetObjectField(TEXT("verification"), V);
    const FString Semantics = LegacySuccess && Post ? TEXT("verified_postcondition") : (LegacySuccess ? TEXT("postcondition_failed") : TEXT("failed_execution")); C->SetStringField(TEXT("success_semantics"), Semantics);
    if (LegacySuccess && !Post) { C->SetBoolField(TEXT("legacy_success"), true); C->SetStringField(TEXT("error_code"), TEXT("mutation_postcondition_failed")); C->SetStringField(TEXT("error"), TEXT("Mutation postcondition failed independent readback")); }
    R.Data->SetObjectField(TEXT("legacy_mutation_contract"), C); R.Data->SetStringField(TEXT("contract_version"), TEXT("mutation_result_v1")); R.Data->SetBoolField(TEXT("verified_success"), LegacySuccess && Post); R.Data->SetBoolField(TEXT("postcondition_verified"), Post); R.Data->SetObjectField(TEXT("verification"), V); R.Data->SetObjectField(TEXT("delta"), Delta); R.Data->SetBoolField(TEXT("legacy_mutation_performed"), Performed); R.Data->SetStringField(TEXT("success_semantics"), Semantics);
}

// JD: literal comparisons are local to the new Preview setter. Do not change the
// existing ordinary DataAsset/Description policies or use entry operator== (it omits AnimBlueprint).
bool PreviewLiteral(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B,int32 Depth=0,bool Saved=false,const FString& TargetPath=FString(),const FString& At=FString())
{
    if(!A||!B||A->Type!=B->Type||Depth>32){return false;}
    if(Saved&&At==TEXT("/package_dirty")){return B->Type==EJson::Boolean&&!B->AsBool();}
    if(A->Type==EJson::Object)
    {
        const auto X=A->AsObject(),Y=B->AsObject();if(!X.IsValid()||!Y.IsValid()||X->Values.Num()!=Y->Values.Num()){return false;}
        FString Owner;const bool Own=Saved&&X->TryGetStringField(TEXT("path"),Owner)&&(Owner.Equals(TargetPath,ESearchCase::CaseSensitive)||Owner.StartsWith(TargetPath+TEXT(":"),ESearchCase::CaseSensitive));
        for(const auto& P:X->Values)
        {
            TSharedPtr<FJsonValue> Z;for(const auto& Q:Y->Values){if(FString(P.Key.ToView()).Equals(FString(Q.Key.ToView()),ESearchCase::CaseSensitive)){Z=Q.Value;break;}}if(!Z){return false;}
            const FString K(P.Key.ToView());
            if(Own&&K.Equals(TEXT("object_flags"),ESearchCase::CaseSensitive))
            {if(P.Value->Type!=EJson::String||Z->Type!=EJson::String){return false;}const uint64 F=FCString::Strtoui64(*P.Value->AsString(),nullptr,10);if(!LexToString(F).Equals(P.Value->AsString(),ESearchCase::CaseSensitive)||!Z->AsString().Equals(LexToString(F|static_cast<uint64>(RF_WasLoaded|RF_LoadCompleted)),ESearchCase::CaseSensitive)){return false;}}
            else if(!PreviewLiteral(P.Value,Z,Depth+1,Saved,TargetPath,At+TEXT("/")+K)){return false;}
        }return true;
    }
    if(A->Type==EJson::Array){const auto& X=A->AsArray();const auto& Y=B->AsArray();if(X.Num()!=Y.Num()){return false;}for(int32 I=0;I<X.Num();++I){if(!PreviewLiteral(X[I],Y[I],Depth+1,Saved,TargetPath,At+TEXT("/")+LexToString(I))){return false;}}return true;}
    return A->Type==EJson::String?A->AsString().Equals(B->AsString(),ESearchCase::CaseSensitive):FJsonValue::CompareEqual(*A,*B);
}
bool PreviewLiteral(const TSharedRef<FJsonObject>& A,const TSharedRef<FJsonObject>& B)
{return PreviewLiteral(MakeShared<FJsonValueObject>(A),MakeShared<FJsonValueObject>(B));}
bool PreviewSetFences(const FObservation& Before,const FObservation& After,bool SelectedMayChange,FResult& R)
{
    bool State=Before.Complete&&After.Complete&&Before.State->Values.Num()==After.State->Values.Num();
    for(const auto& P:Before.State->Values)
    {
        const FString K(P.Key.ToView());if(K.Equals(TEXT("package_dirty"),ESearchCase::CaseSensitive)){continue;}
        if(SelectedMayChange&&(K.Equals(TEXT("values"),ESearchCase::CaseSensitive)||K.Equals(TEXT("properties"),ESearchCase::CaseSensitive)))
        {
            const auto X=P.Value->AsObject(),Y=After.State->GetObjectField(K);State&=X->Values.Num()==Y->Values.Num();
            for(const auto& F:X->Values){if(FName(*FString(F.Key.ToView()))!=FName(TEXT("SkeletalMeshes"))){State&=PreviewLiteral(F.Value,Y->TryGetField(F.Key.ToView()));}}
        }
        else{State&=PreviewLiteral(P.Value,After.State->TryGetField(P.Key.ToView()));}
    }
    const bool Source=PreviewLiteral(Before.Source,After.Source),Refs=PreviewLiteral(Before.Refs,After.Refs),Other=PreviewLiteral(Before.Other,After.Other),File=PreviewLiteral(Before.File,After.File);
    R.Data->SetBoolField(TEXT("observations_complete"),Before.Complete&&After.Complete);R.Data->SetBoolField(TEXT("source_preserved"),Source);R.Data->SetBoolField(TEXT("references_preserved"),Refs);R.Data->SetBoolField(TEXT("unselected_state_preserved"),State);R.Data->SetBoolField(TEXT("other_package_dirty_preserved"),Other);R.Data->SetBoolField(TEXT("disk_preserved_before_save"),File);
    return State&&Source&&Refs&&Other&&File;
}
// Original JsonValueMatchesExpected contract: object subset/key aliases and string
// case-insensitive matching are legacy result semantics, never the preservation proof.
bool PreviewMatchesExpected(const TSharedPtr<FJsonValue>& Expected,const TSharedPtr<FJsonValue>& Actual,int32 Depth=0)
{
    if(!Expected||!Actual||Expected->Type!=Actual->Type||Depth>32){return false;}
    if(Expected->Type==EJson::Object)
    {const auto X=Expected->AsObject(),Y=Actual->AsObject();if(!X.IsValid()||!Y.IsValid()){return false;}for(const auto& P:X->Values){TSharedPtr<FJsonValue> Z;for(const auto& Q:Y->Values){if(FString(P.Key.ToView()).Equals(FString(Q.Key.ToView()),ESearchCase::IgnoreCase)){Z=Q.Value;break;}}if(!PreviewMatchesExpected(P.Value,Z,Depth+1)){return false;}}return true;}
    if(Expected->Type==EJson::Array)
    {const auto& X=Expected->AsArray();const auto& Y=Actual->AsArray();if(X.Num()!=Y.Num()){return false;}for(int32 I=0;I<X.Num();++I){if(!PreviewMatchesExpected(X[I],Y[I],Depth+1)){return false;}}return true;}
    if(Expected->Type==EJson::String){return Expected->AsString().Equals(Actual->AsString(),ESearchCase::IgnoreCase);}
    if(Expected->Type==EJson::Number){return FMath::IsNearlyEqual(Expected->AsNumber(),Actual->AsNumber());}
    if(Expected->Type==EJson::Boolean){return Expected->AsBool()==Actual->AsBool();}
    return Expected->Type==EJson::None||Expected->Type==EJson::Null;
}
bool PreviewSetterActive=false;
void PreviewInvalidateVerification(FResult& R)
{
    R.Success=R.Post=false;R.Data->SetBoolField(TEXT("verified_success"),false);R.Data->SetBoolField(TEXT("postcondition_verified"),false);
    R.Data->SetStringField(TEXT("success_semantics"),TEXT("postcondition_failed"));
    if(R.Data->HasTypedField<EJson::Object>(TEXT("verification"))){R.Data->GetObjectField(TEXT("verification"))->SetStringField(TEXT("status"),TEXT("failed"));}
    if(R.Data->HasTypedField<EJson::Object>(TEXT("legacy_mutation_contract")))
    {const auto J=R.Data->GetObjectField(TEXT("legacy_mutation_contract"));J->SetBoolField(TEXT("success"),false);J->SetBoolField(TEXT("verified_success"),false);J->SetBoolField(TEXT("postcondition_verified"),false);J->SetStringField(TEXT("success_semantics"),TEXT("postcondition_failed"));}
}
FString SetPreview(FContext& C,FResult& R,const TSharedRef<FJsonObject>& Requested,bool bSave)
{
    TGuardValue<bool> Active(PreviewSetterActive,true);FPreviewCollectionScope& Scope=*C.Preview;
    R.Data->SetStringField(TEXT("admission_scope"),TEXT("Exact native PreviewMeshCollection.SkeletalMeshes direct array import; Skeleton and unselected fields remain protected. Other properties/classes remain per-property failures."));
    R.Data->SetStringField(TEXT("preview_scope"),TEXT("Original direct array Resize/item import may partially mutate on failure; current ordered paths and full authored state are observed after each explicit SDK boundary. No consumer getter, hard-load/type/exists query for soft targets, rollback or shader/preview execution proof."));
    R.Data->SetStringField(TEXT("description_cook_scope"),TEXT("Native Preview entry payload contains runtime Engine soft references; current stored paths are not proof that meshes/AnimBlueprints load, compile, cook or preview successfully."));
    R.Data->SetBoolField(TEXT("direct_property_import"),true);R.Data->SetBoolField(TEXT("scratch_atomic_conversion"),false);
    UDataAsset* A=C.Asset.Get();FObservation Before,Configured,After;
    if(bSave&&!A->GetPackage()->IsFullyLoaded()){Fail(R,TEXT("data_asset_save_not_ready"),TEXT("Already fully loaded package required before requested save."));return Encode(R);}
    Count(R,TEXT("legacy_get_calls"));if(!Load(C,R,TEXT("legacy_get_before_load"))||!Capture(C,Before,R,TEXT("before_observation"))){Fail(R,TEXT("data_asset_before_unobservable"),TEXT("Complete original before-load/serialization baseline required before Modify."),true);return Encode(R);}
    R.Data->SetObjectField(TEXT("before"),LegacyGet(C,Before));R.Data->SetStringField(TEXT("class"),C.Class->GetPathName());
    FArrayProperty* Entries=CastField<FArrayProperty>(C.Class->FindPropertyByName(FName(TEXT("SkeletalMeshes"))));
    FStructProperty* Entry=Entries?CastField<FStructProperty>(Entries->Inner):nullptr;
    if(!Scope.Admit()||!Entries||Entries->GetOwnerStruct()!=C.Class||Entries->ArrayDim!=1||!Entries->HasAnyPropertyFlags(CPF_Edit)||Entries->HasAnyPropertyFlags(CPF_EditConst|CPF_Config|CPF_GlobalConfig|CPF_Transient|CPF_DisableEditOnInstance)||!Entry||Entry->Struct!=FPreviewMeshCollectionEntry::StaticStruct())
    {Fail(R,TEXT("data_asset_preview_schema_changed"),TEXT("Exact native editable Preview entry array schema required; no generic object/struct setter."),true);return Encode(R);}
    if(!Scope.Admit()||!Scope.Pin(Entry->Struct)){return Encode(R);}
    auto Point=[&](FObservation& O,const TCHAR* Stage,bool SelectedMayChange)->bool
    {R.Stage=Stage;if(!Scope.Admit()||!Capture(C,O,R,Stage)){return Scope.Stop(TEXT("data_asset_preview_set_observation_incomplete"),TEXT("Actual SDK prefix could not be observed; no next SDK/save/retry."));}
     if(!PreviewSetFences(Before,O,SelectedMayChange,R)){return Scope.Stop(TEXT("data_asset_preview_set_preservation_failed"),TEXT("Unselected state/native source/reference/disk/other dirty changed after explicit SDK; actual partial output retained."));}return true;};
    FObservation Expected=Before;TArray<TSharedPtr<FJsonValue>> Applied,Failed,Attempts,Notifications;TArray<FString> NotifyNames;TSet<FString> AppliedNames;bool Imported=false;
    auto Run=[&]()
    {
        R.Stage=TEXT("apply_properties");R.Attempted=true;R.Known=false;
        if(!Point(Expected,TEXT("after_transaction_begin"),false)){return;}
        Count(R,TEXT("explicit_modify_calls"));A->Modify();if(!Point(Expected,TEXT("after_modify"),false)){return;}
        for(const auto& Pair:Requested->Values)
        {
            if(!Scope.Admit()){return;}const FString Name(Pair.Key.ToView());auto Row=MakeShared<FJsonObject>();Attempts.Add(MakeShared<FJsonValueObject>(Row));R.Data->SetArrayField(TEXT("property_attempts"),Attempts);
            Row->SetStringField(TEXT("property"),Name);Row->SetBoolField(TEXT("conversion_attempted"),false);Row->SetBoolField(TEXT("storage_copy_returned"),false);Row->SetBoolField(TEXT("direct_target_storage"),true);
            FProperty* P=Name.Len()<NAME_SIZE&&!HasNul(Name)?C.Class->FindPropertyByName(FName(*Name)):nullptr;FString Error;
            if(!P){Error=TEXT("Property not found");}
            else if(P!=Entries){Error=TEXT("Only exact editable SkeletalMeshes array is admitted; Skeleton and other property variants remain incomplete");}
            else if(!Clean(Text([&](){auto J=MakeShared<FJsonObject>();J->SetField(TEXT("value"),Pair.Value);return J;}())))
            {Error=TEXT("Editor tooling dependency references are not admitted");}
            else
            {
                Row->SetBoolField(TEXT("conversion_attempted"),true);Count(R,TEXT("json_conversion_calls"));Imported=true;
                // Original target storage, not scratch: Resize and prior items survive a false result.
                const bool Converted=FJsonObjectConverter::JsonValueToUProperty(Pair.Value,P,P->ContainerPtrToValuePtr<void>(A),0,0);
                Row->SetBoolField(TEXT("conversion_returned"),true);Row->SetBoolField(TEXT("conversion_succeeded"),Converted);
                if(!Scope.AdoptConvertedEntries()||!Point(Expected,TEXT("after_direct_property_conversion"),true)){return;}
                Row->SetNumberField(TEXT("actual_entry_count"),Scope.Target->SkeletalMeshes.Num());
                if(!Converted){Error=TEXT("Value conversion failed (check type)");Row->SetBoolField(TEXT("partial_output_observed"),true);}
                else{Applied.Add(MakeShared<FJsonValueString>(Name));NotifyNames.Add(Name);AppliedNames.Add(Name);}
            }
            if(!Error.IsEmpty()){auto F=MakeShared<FJsonObject>();F->SetStringField(TEXT("property"),Name);F->SetStringField(TEXT("error"),Error);Failed.Add(MakeShared<FJsonValueObject>(F));Row->SetStringField(TEXT("error"),Error);}
            R.Data->SetArrayField(TEXT("applied"),Applied);R.Data->SetArrayField(TEXT("failed"),Failed);
        }
        for(const FString& Name:NotifyNames)
        {
            if(!Scope.Admit()||C.Class->FindPropertyByName(FName(*Name))!=Entries){Scope.Stop(TEXT("data_asset_preview_property_role_changed"),TEXT("Current selected property role changed before notification."));return;}
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("property"),Name);J->SetBoolField(TEXT("attempted"),true);Notifications.Add(MakeShared<FJsonValueObject>(J));R.Data->SetArrayField(TEXT("property_notifications"),Notifications);
            Count(R,TEXT("post_edit_change_property_calls"));FPropertyChangedEvent Event(Entries);A->PostEditChangeProperty(Event);J->SetBoolField(TEXT("returned"),true);
            FObservation Current;if(!Point(Current,TEXT("after_property_notification"),Imported)||!PreviewLiteral(Expected.State->GetObjectField(TEXT("values")).ToSharedRef(),Current.State->GetObjectField(TEXT("values")).ToSharedRef())){if(!Scope.Stopped){Scope.Stop(TEXT("data_asset_preview_notification_changed_values"),TEXT("Notification changed the imported current values; no next writer/save."));}return;}Expected=Current;
        }
        if(Applied.Num()>0){if(!Scope.Admit()){return;}Count(R,TEXT("mark_package_dirty_calls"));A->MarkPackageDirty();if(!Point(Expected,TEXT("after_mark_dirty"),Imported)){return;}}
        if(!Point(Configured,TEXT("after_configuration"),Imported)){return;}R.Known=true;
        const bool ActualChanged=!PreviewLiteral(Before.State->GetObjectField(TEXT("values")).ToSharedRef(),Configured.State->GetObjectField(TEXT("values")).ToSharedRef());
        R.Changed=ActualChanged;R.Data->SetBoolField(TEXT("observed_state_changed"),!PreviewLiteral(Before.State,Configured.State));R.Data->SetBoolField(TEXT("observed_property_values_changed"),ActualChanged);
        R.Data->SetBoolField(TEXT("partial_conversion_state_changed"),ActualChanged&&Failed.Num()>0);R.Data->SetBoolField(TEXT("after_configuration_source_preserved"),true);R.Data->SetBoolField(TEXT("after_configuration_unselected_state_preserved"),true);
        if(!Save(C,Configured,R)){return;}
        const bool FinalOK=Final(C,Configured,After,R);R.Data->SetBoolField(TEXT("final_preservation_passed"),FinalOK);if(!After.Complete||Scope.Stopped){return;}
        const bool LiteralFinal=PreviewLiteral(MakeShared<FJsonValueObject>(Configured.State),MakeShared<FJsonValueObject>(After.State),0,R.Saved,C.Path)&&PreviewLiteral(Configured.Source,After.Source)&&PreviewLiteral(MakeShared<FJsonValueObject>(Configured.Refs),MakeShared<FJsonValueObject>(After.Refs),0,R.Saved,C.Path)&&PreviewLiteral(Configured.Other,After.Other);
        if(!LiteralFinal){Scope.Stop(TEXT("data_asset_preview_final_literal_mismatch"),TEXT("Final original relookup changed literal prepared state/source/reference/other dirty; actual save receipt retained."));return;}
        TArray<TSharedPtr<FJsonValue>> Results;int32 Updated=0,Skipped=0,PropertyFailure=0;
        for(const auto& Pair:Requested->Values)
        {
            const FString Name(Pair.Key.ToView());const auto B=ValueByName(Before.State->GetObjectField(TEXT("values")).ToSharedRef(),Name),Z=ValueByName(After.State->GetObjectField(TEXT("values")).ToSharedRef(),Name);
            const bool Was=PreviewMatchesExpected(Pair.Value,B),Is=PreviewMatchesExpected(Pair.Value,Z),Did=AppliedNames.Contains(Name);FString Outcome;
            if(!Did){Outcome=TEXT("conversion_failed");++PropertyFailure;}else if(!Is){Outcome=TEXT("readback_failed");++PropertyFailure;}else if(Was){Outcome=TEXT("skipped_unchanged");++Skipped;}else{Outcome=TEXT("updated");++Updated;}
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("property"),Name);J->SetBoolField(TEXT("applied"),Did);J->SetBoolField(TEXT("before_matched_expected"),Was);J->SetBoolField(TEXT("after_matched_expected"),Is);J->SetStringField(TEXT("outcome"),Outcome);Results.Add(MakeShared<FJsonValueObject>(J));
        }
        const bool SaveOK=!bSave||R.Saved,Specific=PropertyFailure==0&&SaveOK;const int32 CompletionFailure=SaveOK?0:1,FailedCount=SaveOK?PropertyFailure:FMath::Max(PropertyFailure,Requested->Values.Num());
        R.Data->SetArrayField(TEXT("property_results"),Results);R.Data->SetNumberField(TEXT("updated_property_count"),Updated);R.Data->SetNumberField(TEXT("skipped_property_count"),Skipped);R.Data->SetNumberField(TEXT("property_failure_count"),PropertyFailure);R.Data->SetNumberField(TEXT("completion_failure_count"),CompletionFailure);
        auto Delta=MakeShared<FJsonObject>();Delta->SetNumberField(TEXT("affected_count"),Updated);Delta->SetNumberField(TEXT("created_count"),0);Delta->SetNumberField(TEXT("updated_count"),Updated);Delta->SetNumberField(TEXT("removed_count"),0);Delta->SetNumberField(TEXT("skipped_count"),Skipped);Delta->SetNumberField(TEXT("property_failure_count"),PropertyFailure);Delta->SetNumberField(TEXT("completion_failure_count"),CompletionFailure);Delta->SetNumberField(TEXT("failed_count"),FailedCount);
        Contract(R,R.Data->GetObjectField(TEXT("before")).ToSharedRef(),R.Data->GetObjectField(TEXT("after")).ToSharedRef(),Delta,Applied.Num()>0,Updated>0,Failed.Num()==0,Specific,TEXT("data_asset_property_serialization_readback"),{TEXT("asset_resolved_after"),TEXT("requested_properties_match"),TEXT("save_gate")});
        R.Known=true;R.Changed=ActualChanged;R.Data->SetStringField(TEXT("legacy_verification_scope"),TEXT("Original applied/updated property counts remain separate from actual stored-value drift, including failed direct imports. Original expected-object subset/key aliases/string matching are legacy predicates; preservation uses separate literal comparisons. Null/coerced input may convert yet fail original expected-value readback. No atomic rollback or preview consumption is claimed."));
        if(!FinalOK){PreviewInvalidateVerification(R);return;}if(!Specific||Failed.Num()>0){Fail(R,Failed.Num()==0?TEXT("mutation_postcondition_failed"):TEXT("data_asset_properties_failed"),TEXT("Actual terminal partial property/readback/save failure retained; no retry."));return;}R.Success=R.Post=true;R.Stage=TEXT("complete");
    };
    {
        R.Attempted=true;R.Known=false;Count(R,TEXT("outer_transaction_calls"));const FScopedTransaction Transaction(NSLOCTEXT("CubelessDataAsset","Set","Set DataAsset properties"));Run();
    } // EndTransaction can broadcast; publish only after its normal return and a full current fence.
    if(Scope.Stopped){PreviewInvalidateVerification(R);return Encode(R);}
    if(After.Complete)
    {
        FObservation Fence;const FString TerminalStage=R.Stage;R.Stage=TEXT("after_transaction_end");
        if(!Scope.Admit()||!Capture(C,Fence,R,TEXT("after_transaction_observation"))){Scope.Stop(TEXT("data_asset_preview_transaction_observation_incomplete"),TEXT("Normal transaction unwind returned without complete current observation; actual prefix retained."));PreviewInvalidateVerification(R);return Encode(R);}
        const bool OK=PreviewLiteral(After.State,Fence.State)&&PreviewLiteral(After.Source,Fence.Source)&&PreviewLiteral(After.Refs,Fence.Refs)&&PreviewLiteral(After.File,Fence.File)&&PreviewLiteral(After.Other,Fence.Other);
        R.Data->SetBoolField(TEXT("transaction_fence_passed"),OK);
        if(!OK){Scope.Stop(TEXT("data_asset_preview_transaction_preservation_failed"),TEXT("Transaction finalized callback changed the final state/source/reference/disk/dirty; actual save receipt retained."));PreviewInvalidateVerification(R);}else{R.Stage=TerminalStage;}
    }
    return Encode(R);
}

bool DeformerSetterActive=false;
bool DeformerSetFences(const FObservation& Before,const FObservation& After,const TSet<FName>& Imported,FDeformerCollectionScope& Scope,FResult& R)
{
    if(!Scope.Admit()){return false;}bool State=Before.Complete&&After.Complete&&Before.State->Values.Num()==After.State->Values.Num();
    for(const auto& Pair:Before.State->Values)
    {
        const FString Key(Pair.Key.ToView());if(Key==TEXT("package_dirty")){continue;}
        if(Key==TEXT("values")||Key==TEXT("properties"))
        {const auto A=Pair.Value->AsObject(),B=After.State->GetObjectField(Key);State&=A->Values.Num()==B->Values.Num();for(const auto& Field:A->Values){if(!Imported.Contains(FName(*FString(Field.Key.ToView())))){State&=PreviewLiteral(Field.Value,B->TryGetField(Field.Key.ToView()));}}}
        else{State&=PreviewLiteral(Pair.Value,After.State->TryGetField(Pair.Key.ToView()));}
    }
    // The existing collector is direct/nonrecursive (UObjectGlobals.h3197),
    // so only actual hard-array source asset rows may change. Order/duplicates
    // are separately fixed by the typed current array, not this reference set.
    auto Expected=MakeShared<FJsonObject>();
    for(const auto& Pair:Before.Refs->Values)
    {bool ArraySource=false;for(const auto& S:Scope.Collections){ArraySource|=FString(Pair.Key.ToView())==S.Hold->GetPathName();}if(!ArraySource){Expected->SetField(FString(Pair.Key.ToView()),Pair.Value);}}
    for(UMeshDeformerCollection* P:Scope.Hard){if(P){Expected->SetObjectField(P->GetPathName(),Identity(P));}}
    const bool Source=PreviewLiteral(Before.Source,After.Source),Refs=PreviewLiteral(Expected,After.Refs),Other=PreviewLiteral(Before.Other,After.Other),File=PreviewLiteral(Before.File,After.File);
    R.Data->SetBoolField(TEXT("observations_complete"),Before.Complete&&After.Complete);R.Data->SetBoolField(TEXT("source_preserved"),Source);R.Data->SetBoolField(TEXT("references_preserved"),Refs);R.Data->SetBoolField(TEXT("unselected_state_preserved"),State);R.Data->SetBoolField(TEXT("other_package_dirty_preserved"),Other);R.Data->SetBoolField(TEXT("disk_preserved_before_save"),File);
    return State&&Source&&Refs&&Other&&File&&Scope.Admit();
}
FString SetDeformer(FContext& C,FResult& R,const TSharedRef<FJsonObject>& Requested,bool bSave)
{
    TGuardValue<bool> Active(DeformerSetterActive,true);FDeformerCollectionScope& Scope=*C.Deformer;
    R.Data->SetStringField(TEXT("admission_scope"),TEXT("Exact native MeshDeformerCollection MeshDeformers soft array and MeshDeformerCollections closed loaded hard array; original Description remains supported in mixed requests."));
    R.Data->SetStringField(TEXT("deformer_scope"),TEXT("Direct target array import retains Resize/prior-element failure prefix. Loaded exact saved-clean collection source closure is strongly retained/read-only; target/self/cycles/inline hard object construction are prewriter refused. No soft target loader, GetMeshDeformers consumer, deformer execution, CDO warming or runtime/cook proof."));
    R.Data->SetStringField(TEXT("description_cook_scope"),TEXT("Description is Editor-only; collection arrays are runtime payload authoring, not executed deformation."));
    R.Data->SetBoolField(TEXT("direct_property_import"),true);R.Data->SetBoolField(TEXT("scratch_atomic_conversion"),false);
    UDataAsset* A=C.Asset.Get();FObservation Before,Configured,After;
    if(bSave&&!A->GetPackage()->IsFullyLoaded()){Fail(R,TEXT("data_asset_save_not_ready"),TEXT("Already fully loaded package required before requested save."));return Encode(R);}
    if(!Scope.Prepare(Requested)){return Encode(R);}
    Count(R,TEXT("legacy_get_calls"));if(!Load(C,R,TEXT("legacy_get_before_load"))||!Capture(C,Before,R,TEXT("before_observation"))){Fail(R,TEXT("data_asset_before_unobservable"),TEXT("Complete original before-load/serialization baseline required before Modify."),true);return Encode(R);}
    R.Data->SetObjectField(TEXT("before"),LegacyGet(C,Before));R.Data->SetStringField(TEXT("class"),C.Class->GetPathName());
    TSet<FName> ImportedNames;
    auto Point=[&](FObservation& O,const TCHAR* Stage,bool SelectedMayChange)->bool
    {R.Stage=Stage;if(!Scope.CheckReadonly()||!Capture(C,O,R,Stage)){return Scope.Stop(TEXT("data_asset_deformer_set_observation_incomplete"),TEXT("Actual SDK prefix could not be observed; no next SDK/save/retry."));}
     if(!DeformerSetFences(Before,O,ImportedNames,Scope,R)){return Scope.Stop(TEXT("data_asset_deformer_set_preservation_failed"),TEXT("Unselected state/native source/reference/disk/other dirty changed after explicit SDK; actual partial output retained."));}return true;};
    FObservation Expected=Before;TArray<TSharedPtr<FJsonValue>> Applied,Failed,Attempts,Notifications;TArray<FString> NotifyNames;TSet<FString> AppliedNames;bool Imported=false;
    auto Run=[&]()
    {
        R.Stage=TEXT("apply_properties");R.Attempted=true;R.Known=false;
        if(!Point(Expected,TEXT("after_transaction_begin"),false)){return;}
        Count(R,TEXT("explicit_modify_calls"));A->Modify();if(!Point(Expected,TEXT("after_modify"),false)){return;}
        for(const auto& Pair:Requested->Values)
        {
            if(!Scope.Admit()){return;}const FString Name(Pair.Key.ToView());auto Row=MakeShared<FJsonObject>();Attempts.Add(MakeShared<FJsonValueObject>(Row));R.Data->SetArrayField(TEXT("property_attempts"),Attempts);
            Row->SetStringField(TEXT("property"),Name);Row->SetBoolField(TEXT("conversion_attempted"),false);Row->SetBoolField(TEXT("storage_copy_returned"),false);Row->SetBoolField(TEXT("direct_target_storage"),true);
            FProperty* P=Name.Len()<NAME_SIZE&&!HasNul(Name)?C.Class->FindPropertyByName(FName(*Name)):nullptr;FString Error;
            if(!P){Error=TEXT("Property not found");}
            else if(P!=Scope.SoftProperty&&P!=Scope.HardProperty&&P!=Scope.DescriptionProperty){Error=TEXT("Only exact editable MeshDeformers/MeshDeformerCollections arrays and existing Description are supported");}
            else if(P==Scope.DescriptionProperty&&(!Pair.Value.IsValid()||Pair.Value->Type!=EJson::String||Pair.Value->AsString().Len()>16384||HasNul(Pair.Value->AsString())||!Clean(Pair.Value->AsString()))){Error=TEXT("Description requires bounded literal string without tool dependency; original null/type coercions remain incomplete");}
            else if(!Clean(Text([&](){auto J=MakeShared<FJsonObject>();J->SetField(TEXT("value"),Pair.Value);return J;}())))
            {Error=TEXT("Editor tooling dependency references are not admitted");}
            else
            {
                Row->SetBoolField(TEXT("conversion_attempted"),true);Count(R,TEXT("json_conversion_calls"));Imported=true;
                // Original target storage, not scratch: Resize and prior items survive a false result.
                const bool Converted=FJsonObjectConverter::JsonValueToUProperty(Pair.Value,P,P->ContainerPtrToValuePtr<void>(A),0,0);
                Row->SetBoolField(TEXT("conversion_returned"),true);Row->SetBoolField(TEXT("conversion_succeeded"),Converted);
                ImportedNames.Add(P->GetFName());if(!Scope.AdoptActual(P)||!Point(Expected,TEXT("after_direct_property_conversion"),true)){return;}
                if(P==Scope.SoftProperty){Row->SetNumberField(TEXT("actual_entry_count"),Scope.Target->MeshDeformers.Num());}else if(P==Scope.HardProperty){Row->SetNumberField(TEXT("actual_entry_count"),Scope.Target->MeshDeformerCollections.Num());}
                if(!Converted){Error=TEXT("Value conversion failed (check type)");Row->SetBoolField(TEXT("partial_output_observed"),true);}
                else{Applied.Add(MakeShared<FJsonValueString>(Name));NotifyNames.Add(Name);AppliedNames.Add(Name);}
            }
            if(!Error.IsEmpty()){auto F=MakeShared<FJsonObject>();F->SetStringField(TEXT("property"),Name);F->SetStringField(TEXT("error"),Error);Failed.Add(MakeShared<FJsonValueObject>(F));Row->SetStringField(TEXT("error"),Error);}
            R.Data->SetArrayField(TEXT("applied"),Applied);R.Data->SetArrayField(TEXT("failed"),Failed);
        }
        for(const FString& Name:NotifyNames)
        {
            FProperty* CurrentProperty=Name.Len()<NAME_SIZE?C.Class->FindPropertyByName(FName(*Name)):nullptr;
            if(!Scope.Admit()||(CurrentProperty!=Scope.SoftProperty&&CurrentProperty!=Scope.HardProperty&&CurrentProperty!=Scope.DescriptionProperty)){Scope.Stop(TEXT("data_asset_deformer_property_role_changed"),TEXT("Current selected property role changed before notification."));return;}
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("property"),Name);J->SetBoolField(TEXT("attempted"),true);Notifications.Add(MakeShared<FJsonValueObject>(J));R.Data->SetArrayField(TEXT("property_notifications"),Notifications);
            Count(R,TEXT("post_edit_change_property_calls"));FPropertyChangedEvent Event(CurrentProperty);A->PostEditChangeProperty(Event);J->SetBoolField(TEXT("returned"),true);
            FObservation Current;if(!Point(Current,TEXT("after_property_notification"),Imported)||!PreviewLiteral(Expected.State->GetObjectField(TEXT("values")).ToSharedRef(),Current.State->GetObjectField(TEXT("values")).ToSharedRef())){if(!Scope.Stopped){Scope.Stop(TEXT("data_asset_deformer_notification_changed_values"),TEXT("Notification changed the imported current values; no next writer/save."));}return;}Expected=Current;
        }
        if(Applied.Num()>0){if(!Scope.Admit()){return;}Count(R,TEXT("mark_package_dirty_calls"));A->MarkPackageDirty();if(!Point(Expected,TEXT("after_mark_dirty"),Imported)){return;}}
        if(!Point(Configured,TEXT("after_configuration"),Imported)){return;}R.Known=true;
        const bool ActualChanged=!PreviewLiteral(Before.State->GetObjectField(TEXT("values")).ToSharedRef(),Configured.State->GetObjectField(TEXT("values")).ToSharedRef());
        R.Changed=ActualChanged;R.Data->SetBoolField(TEXT("observed_state_changed"),!PreviewLiteral(Before.State,Configured.State));R.Data->SetBoolField(TEXT("observed_property_values_changed"),ActualChanged);
        R.Data->SetBoolField(TEXT("partial_conversion_state_changed"),ActualChanged&&Failed.Num()>0);R.Data->SetBoolField(TEXT("after_configuration_source_preserved"),true);R.Data->SetBoolField(TEXT("after_configuration_unselected_state_preserved"),true);
        if(!Scope.CheckReadonly()||!Save(C,Configured,R)||!Scope.CheckReadonly()){return;}
        const bool FinalOK=Final(C,Configured,After,R);R.Data->SetBoolField(TEXT("final_preservation_passed"),FinalOK);if(!After.Complete||Scope.Stopped){return;}
        const bool LiteralFinal=PreviewLiteral(MakeShared<FJsonValueObject>(Configured.State),MakeShared<FJsonValueObject>(After.State),0,R.Saved,C.Path)&&PreviewLiteral(Configured.Source,After.Source)&&PreviewLiteral(MakeShared<FJsonValueObject>(Configured.Refs),MakeShared<FJsonValueObject>(After.Refs),0,R.Saved,C.Path)&&PreviewLiteral(Configured.Other,After.Other);
        if(!LiteralFinal){Scope.Stop(TEXT("data_asset_deformer_final_literal_mismatch"),TEXT("Final original relookup changed literal prepared state/source/reference/other dirty; actual save receipt retained."));return;}
        TArray<TSharedPtr<FJsonValue>> Results;int32 Updated=0,Skipped=0,PropertyFailure=0;
        for(const auto& Pair:Requested->Values)
        {
            const FString Name(Pair.Key.ToView());const auto B=ValueByName(Before.State->GetObjectField(TEXT("values")).ToSharedRef(),Name),Z=ValueByName(After.State->GetObjectField(TEXT("values")).ToSharedRef(),Name);
            const bool Was=PreviewMatchesExpected(Pair.Value,B),Is=PreviewMatchesExpected(Pair.Value,Z),Did=AppliedNames.Contains(Name);FString Outcome;
            if(!Did){Outcome=TEXT("conversion_failed");++PropertyFailure;}else if(!Is){Outcome=TEXT("readback_failed");++PropertyFailure;}else if(Was){Outcome=TEXT("skipped_unchanged");++Skipped;}else{Outcome=TEXT("updated");++Updated;}
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("property"),Name);J->SetBoolField(TEXT("applied"),Did);J->SetBoolField(TEXT("before_matched_expected"),Was);J->SetBoolField(TEXT("after_matched_expected"),Is);J->SetStringField(TEXT("outcome"),Outcome);Results.Add(MakeShared<FJsonValueObject>(J));
        }
        const bool SaveOK=!bSave||R.Saved,Specific=PropertyFailure==0&&SaveOK;const int32 CompletionFailure=SaveOK?0:1,FailedCount=SaveOK?PropertyFailure:FMath::Max(PropertyFailure,Requested->Values.Num());
        R.Data->SetArrayField(TEXT("property_results"),Results);R.Data->SetNumberField(TEXT("updated_property_count"),Updated);R.Data->SetNumberField(TEXT("skipped_property_count"),Skipped);R.Data->SetNumberField(TEXT("property_failure_count"),PropertyFailure);R.Data->SetNumberField(TEXT("completion_failure_count"),CompletionFailure);
        auto Delta=MakeShared<FJsonObject>();Delta->SetNumberField(TEXT("affected_count"),Updated);Delta->SetNumberField(TEXT("created_count"),0);Delta->SetNumberField(TEXT("updated_count"),Updated);Delta->SetNumberField(TEXT("removed_count"),0);Delta->SetNumberField(TEXT("skipped_count"),Skipped);Delta->SetNumberField(TEXT("property_failure_count"),PropertyFailure);Delta->SetNumberField(TEXT("completion_failure_count"),CompletionFailure);Delta->SetNumberField(TEXT("failed_count"),FailedCount);
        Contract(R,R.Data->GetObjectField(TEXT("before")).ToSharedRef(),R.Data->GetObjectField(TEXT("after")).ToSharedRef(),Delta,Applied.Num()>0,Updated>0,Failed.Num()==0,Specific,TEXT("data_asset_property_serialization_readback"),{TEXT("asset_resolved_after"),TEXT("requested_properties_match"),TEXT("save_gate")});
        R.Known=true;R.Changed=ActualChanged;R.Data->SetStringField(TEXT("legacy_verification_scope"),TEXT("Original applied/updated property counts remain separate from actual stored-value drift, including failed direct imports. Original expected-object subset/key aliases/string matching are legacy predicates; preservation uses separate literal comparisons. Null/coerced input may convert yet fail original expected-value readback. No atomic rollback or deformer consumption is claimed."));
        if(!FinalOK){PreviewInvalidateVerification(R);return;}if(!Specific||Failed.Num()>0){Fail(R,Failed.Num()==0?TEXT("mutation_postcondition_failed"):TEXT("data_asset_properties_failed"),TEXT("Actual terminal partial property/readback/save failure retained; no retry."));return;}R.Success=R.Post=true;R.Stage=TEXT("complete");
    };
    {
        R.Attempted=true;R.Known=false;Count(R,TEXT("outer_transaction_calls"));const FScopedTransaction Transaction(NSLOCTEXT("CubelessDataAsset","Set","Set DataAsset properties"));Run();
    } // EndTransaction can broadcast; publish only after its normal return and a full current fence.
    if(Scope.Stopped){PreviewInvalidateVerification(R);return Encode(R);}
    if(After.Complete)
    {
        FObservation Fence;const FString TerminalStage=R.Stage;R.Stage=TEXT("after_transaction_end");
        if(!Scope.CheckReadonly()||!Capture(C,Fence,R,TEXT("after_transaction_observation"))){Scope.Stop(TEXT("data_asset_deformer_transaction_observation_incomplete"),TEXT("Normal transaction unwind returned without complete current observation; actual prefix retained."));PreviewInvalidateVerification(R);return Encode(R);}
        const bool OK=PreviewLiteral(After.State,Fence.State)&&PreviewLiteral(After.Source,Fence.Source)&&PreviewLiteral(After.Refs,Fence.Refs)&&PreviewLiteral(After.File,Fence.File)&&PreviewLiteral(After.Other,Fence.Other);
        R.Data->SetBoolField(TEXT("transaction_fence_passed"),OK);
        if(!OK){Scope.Stop(TEXT("data_asset_deformer_transaction_preservation_failed"),TEXT("Transaction finalized callback changed the final state/source/reference/disk/dirty; actual save receipt retained."));PreviewInvalidateVerification(R);}else{R.Stage=TerminalStage;}
    }
    return Encode(R);
}

// ReferenceFix1: only actual selected old slots may remove their root-only reference contribution.
// Old owned instances are still serialized individually, so their Curve/other direct contributions stay strict.
bool ReferenceClearExpectedReferences(const FObservation& Before,const FObservation& After,FInputOwnedReadScope& OwnedScope,FMappingContextScope* Mapping,FResult& R,TSharedRef<FJsonObject>& Expected)
{
    if(!OwnedScope.ReferenceClear||!Before.Complete||!After.Complete||!OwnedScope.Check())
    {return OwnedScope.Stop(TEXT("data_asset_owned_clear_reference_scope_changed"),TEXT("Complete current owned/source endpoints required before selected reference comparison."));}
    FString TargetPath;for(const auto& I:OwnedScope.Held){if(I.Hold.Get()==OwnedScope.Target.Get()){TargetPath=I.Path;break;}}
    if(TargetPath.IsEmpty()){return OwnedScope.Stop(TEXT("data_asset_owned_clear_reference_owner_missing"),TEXT("Held selected owner path unavailable; no next writer/save."));}
    Expected=OwnedScope.CopySnapshot(Before.Refs);TArray<TSharedPtr<FJsonValue>> Rows;
    R.Data->SetArrayField(TEXT("owned_reference_clear_reference_comparison"),Rows);
    R.Data->SetStringField(TEXT("owned_reference_clear_reference_policy"),TEXT("Only actually cleared old-owned slots and separately admitted Context row.Action sources may lose an exclusively target-owned contribution. Remaining slots/other owned contributors and every other reference stay literal-strict; no After reference baseline adoption."));
    TArray<FString> Candidates=OwnedScope.ClearedOwnedPaths;for(const auto& Path:OwnedScope.ClearedActionPaths){if(!Candidates.ContainsByPredicate([&](const FString& ExistingPath){return ExistingPath.Equals(Path,ESearchCase::CaseSensitive);})){Candidates.Add(Path);}}
    for(const auto& Path:Candidates)
    {
        const bool ActionSource=OwnedScope.ClearedActionPaths.ContainsByPredicate([&](const FString& ExistingPath){return ExistingPath.Equals(Path,ESearchCase::CaseSensitive);});const auto Prior=Before.Refs->TryGetField(Path);const auto Actual=After.Refs->TryGetField(Path);const TSet<FString>* Contributors=Before.ReferenceContributors.Find(Path);
        TSharedPtr<FJsonValue> HeldIdentity;
        if(ActionSource)
        {
            const FMappingContextScope::FActionSource* Source=Mapping?Mapping->Actions.FindByPredicate([&](const FMappingContextScope::FActionSource& A){return A.Path.Equals(Path,ESearchCase::CaseSensitive);}):nullptr;
            if(!Source||!Mapping->RowsBefore.IsEmpty()){return OwnedScope.Stop(TEXT("data_asset_owned_clear_action_reference_unproven"),TEXT("Cleared row.Action must be an admitted immutable source and current rows must be empty."));}
            HeldIdentity=MakeShared<FJsonValueObject>(Source->Before->GetObjectField(TEXT("identity")));
        }
        else
        {
            const bool HeldOwned=OwnedScope.Held.ContainsByPredicate([&](const FInputOwnedReadScope::FIdentity& I){return I.Path.Equals(Path,ESearchCase::CaseSensitive)&&OwnedScope.Owned.Contains(I.Hold.Get());});
            const auto Exports=Before.State->GetObjectField(TEXT("owned_exports"));
            if(!HeldOwned||!Exports->HasTypedField<EJson::Object>(Path)){return OwnedScope.Stop(TEXT("data_asset_owned_clear_reference_identity_missing"),TEXT("Removed reference must match the preserved old owned inventory, not an arbitrary source path."));}
            HeldIdentity=MakeShared<FJsonValueObject>(Exports->GetObjectField(Path)->GetObjectField(TEXT("identity")));
        }
        if(!Prior||!Equal(Prior,HeldIdentity)||!Contributors||Contributors->IsEmpty())
        {return OwnedScope.Stop(TEXT("data_asset_owned_clear_reference_provenance_missing"),TEXT("Before identity/contributor provenance is incomplete; reference equality is not relaxed."));}
        const bool RemainingSlot=OwnedScope.Links.ContainsByPredicate([&](const FString& L){const int32 Equals=L.Find(TEXT("="));return Equals!=INDEX_NONE&&L.Mid(Equals+1).Equals(Path,ESearchCase::CaseSensitive);});
        bool RootOnly=false;if(Contributors->Num()==1){for(const auto& Owner:*Contributors){RootOnly=Owner.Equals(TargetPath,ESearchCase::CaseSensitive);}}const bool Remove=RootOnly&&!RemainingSlot;
        if(Remove){Expected->RemoveField(Path);}
        auto Row=MakeShared<FJsonObject>();Rows.Add(MakeShared<FJsonValueObject>(Row));R.Data->SetArrayField(TEXT("owned_reference_clear_reference_comparison"),Rows);
        Row->SetStringField(TEXT("path"),Path);Row->SetStringField(TEXT("selected_role"),ActionSource?TEXT("admitted_row_action"):TEXT("old_owned_slot"));TArray<TSharedPtr<FJsonValue>> Owners;for(const auto& Owner:*Contributors){Owners.Add(MakeShared<FJsonValueString>(Owner));}Row->SetArrayField(TEXT("before_contributors"),Owners);
        Row->SetBoolField(TEXT("remaining_slot"),RemainingSlot);Row->SetBoolField(TEXT("root_only"),RootOnly);Row->SetBoolField(TEXT("expected_reference_present"),!Remove);Row->SetBoolField(TEXT("actual_reference_present"),Actual.IsValid());Row->SetField(TEXT("before_identity"),Prior);Row->SetField(TEXT("actual_identity"),Actual.IsValid()?Actual:MakeShared<FJsonValueNull>());
    }
    const bool Same=Equal(Expected,After.Refs);R.Data->SetBoolField(TEXT("owned_reference_clear_references_match_expected"),Same);
    // Plain captured JSON only: no UObject/SDK lookup after a mismatching endpoint.
    auto First=[&](const FString& Path,const TSharedPtr<FJsonValue>& E,const TSharedPtr<FJsonValue>& A)
    {if(!R.Data->HasField(TEXT("owned_reference_clear_reference_first_mismatch"))){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),Path);J->SetBoolField(TEXT("expected_present"),E.IsValid());J->SetBoolField(TEXT("actual_present"),A.IsValid());J->SetField(TEXT("expected_identity"),E.IsValid()?E:MakeShared<FJsonValueNull>());J->SetField(TEXT("actual_identity"),A.IsValid()?A:MakeShared<FJsonValueNull>());R.Data->SetObjectField(TEXT("owned_reference_clear_reference_first_mismatch"),J);}};
    if(!Same){for(const auto& P:Expected->Values){const FString Path(P.Key.ToView());const auto Actual=After.Refs->TryGetField(Path);if(!Equal(P.Value,Actual)){First(Path,P.Value,Actual);break;}}if(!R.Data->HasField(TEXT("owned_reference_clear_reference_first_mismatch"))){for(const auto& P:After.Refs->Values){const FString Path(P.Key.ToView());if(!Expected->HasField(Path)){First(Path,nullptr,P.Value);break;}}}}
    return true; // The unchanged whole Fences comparison remains the authority for success/failure.
}
// MS: old direct-owned objects stay alive and frozen even when their target array reference leaves.
bool OwnedLifecycleReferences(const FObservation& Before,FInputOwnedReadScope& Scope,FResult& R,TSharedRef<FJsonObject>& Expected)
{
    if(!Before.Complete||!Scope.Check()){return false;}Expected=Scope.CopySnapshot(Before.Refs);FString TargetPath;
    for(const auto& I:Scope.Held){if(I.Hold.Get()==Scope.Target.Get()){TargetPath=I.Path;break;}}
    if(TargetPath.IsEmpty()){return Scope.Stop(TEXT("data_asset_owned_lifecycle_owner_missing"),TEXT("Held target path unavailable; no next SDK."));}
    TArray<TSharedPtr<FJsonValue>> Rows;const auto Old=Before.State->GetObjectField(TEXT("owned_exports"));
    for(const auto& Pair:Old->Values)
    {
        const FString Path(Pair.Key.ToView());const auto IdentityBefore=MakeShared<FJsonValueObject>(Pair.Value->AsObject()->GetObjectField(TEXT("identity")));
        const bool Held=Scope.Held.ContainsByPredicate([&](const FInputOwnedReadScope::FIdentity& I){return I.Path.Equals(Path,ESearchCase::CaseSensitive)&&Scope.Owned.Contains(I.Hold.Get());});
        const auto Prior=Before.Refs->TryGetField(Path);const TSet<FString>* Contributors=Before.ReferenceContributors.Find(Path);bool OtherContributor=false;
        if(!Held||(Prior&&(!Equal(Prior,IdentityBefore)||!Contributors||Contributors->IsEmpty()))||(!Prior&&Contributors))
        {auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("object"),Path);J->SetStringField(TEXT("property"),TEXT("before_reference_provenance"));J->SetStringField(TEXT("expected"),TEXT("held old identity with complete contributor provenance when referenced"));J->SetStringField(TEXT("actual"),TEXT("identity or contributor mismatch"));R.Data->SetObjectField(TEXT("owned_lifecycle_first_mismatch"),J);return Scope.Stop(TEXT("data_asset_owned_lifecycle_reference_unproven"),TEXT("Cannot derive old-owned reference closure from current held before evidence."));}
        if(Contributors){for(const auto& Owner:*Contributors){OtherContributor|=!Owner.Equals(TargetPath,ESearchCase::CaseSensitive);}}
        const bool Slot=Scope.Links.ContainsByPredicate([&](const FString& L){const int32 At=L.Find(TEXT("="));return At!=INDEX_NONE&&L.Mid(At+1).Equals(Path,ESearchCase::CaseSensitive);});
        if(OtherContributor||Slot){Expected->SetField(Path,IdentityBefore);}else{Expected->RemoveField(Path);}
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("path"),Path);Row->SetBoolField(TEXT("other_unchanged_contributor"),OtherContributor);Row->SetBoolField(TEXT("current_selected_or_preserved_slot"),Slot);Row->SetBoolField(TEXT("expected_reference_present"),OtherContributor||Slot);Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    // OY: new modifier exports contribute only their exact accepted axis references, including native-default null skips.
    // The held source identity, not After.Refs, is the authority for this additive reference delta.
    for(const auto& Slot:Scope.CurveSlots)
    {
        if(!Scope.NewResponseObjects.Contains(Slot.Owner)||!Slot.Curve){continue;}
        bool BoundSource=false;for(const auto& SourceCurve:Scope.Curves){BoundSource|=SourceCurve.Object==Slot.Curve;}
        if(!BoundSource){return Scope.Stop(TEXT("data_asset_response_curve_reference_unplanned"),TEXT("New axis reference lacks a prebound saved-clean CurveFloat source."));}
        const FString SourcePath=Slot.Curve->GetPathName();if(!Before.Refs->HasField(SourcePath)){Expected->SetObjectField(SourcePath,Identity(Slot.Curve));}
        auto Axis=MakeShared<FJsonObject>();Axis->SetStringField(TEXT("new_owned_object"),Slot.Owner->GetPathName());Axis->SetStringField(TEXT("field"),Slot.Property->GetName());Axis->SetStringField(TEXT("path"),SourcePath);Axis->SetBoolField(TEXT("expected_reference_present"),true);Axis->SetBoolField(TEXT("source_prebound"),true);Rows.Add(MakeShared<FJsonValueObject>(Axis));
    }
    R.Data->SetArrayField(TEXT("owned_lifecycle_reference_expectation"),Rows);R.Data->SetStringField(TEXT("owned_lifecycle_reference_policy"),TEXT("Old immutable owned exports remain serialized individually. Root contribution follows accepted ordered slots; only accepted new Response axis slots add their prebound CurveFloat identities. Other contributors/native/source/unrelated references stay strict. No After reference baseline adoption."));return Scope.Check();
}
bool ActionOwnedSetFences(const FObservation& Before,const FObservation& After,FInputActionScope& Scope,FResult& R,const TSet<FName>& Selected)
{
    if(Scope.OwnedRead&&Scope.OwnedRead->ReferenceClear)
    {
        FObservation Left=Before;if(!ReferenceClearExpectedReferences(Before,After,*Scope.OwnedRead,nullptr,R,Left.Refs)){return false;}
        return Fences(Left,After,false,R,&Selected)&&Scope.Check();
    }
    if(!Scope.OwnedWriter()){return Fences(Before,After,false,R,&Selected);}FObservation Left=Before,Right=After;if(!OwnedLifecycleReferences(Before,*Scope.OwnedRead,R,Left.Refs)){return false;}Right.State=Scope.OwnedRead->CopySnapshot(After.State);Right.Refs=Scope.OwnedRead->CopySnapshot(After.Refs);
    for(UObject* O:Scope.CreatedOwned)
    {
        if(!Scope.Check()||Before.State->GetObjectField(TEXT("owned_exports"))->HasField(O->GetPathName())||!Right.State->GetObjectField(TEXT("owned_exports"))->HasField(O->GetPathName())){return Scope.Stop(TEXT("data_asset_action_owned_export_prefix_changed"),TEXT("Created direct Action-owned export prefix differs from observed endpoint."));}
        Right.State->GetObjectField(TEXT("owned_exports"))->RemoveField(O->GetPathName());for(UObject* Ref:TArray<UObject*>{O,O->GetClass(),O->GetClass()->GetDefaultObject(false)}){if(Ref&&!Before.Refs->HasField(Ref->GetPathName())){Right.Refs->RemoveField(Ref->GetPathName());}}
    }return Fences(Left,Right,false,R,&Selected)&&Scope.Check();
}

FString SetInputAction(FContext& C,FResult& R,const TSharedRef<FJsonObject>& Requested,bool bSave)
{
    FInputActionScope& Scope=*C.Input;UDataAsset* A=C.Asset.Get();FObservation Before,Configured,After;const TArray<FString> BeforeOwnedLinks=Scope.OwnedWriter()?Scope.OwnedRead->Links:TArray<FString>{};
    for(const auto& Pair:Requested->Values){const FString Name(Pair.Key.ToView());if(Name.Len()<NAME_SIZE&&!HasNul(Name)&&(FName(*Name)==TEXT("ValueType")||(Scope.OwnedWriter()&&FName(*Name)==TEXT("Triggers")))){Scope.CheckConsumers=true;}}
    if(!Scope.Check()){return Encode(R);}
    if(Scope.OwnedWriter()){for(const TCHAR* Counter:{TEXT("owned_object_allocation_calls"),TEXT("owned_field_plan_conversion_calls"),TEXT("owned_field_storage_copy_calls"),TEXT("owned_matching_json_export_calls")}){R.Data->SetNumberField(Counter,0);}R.Data->SetNumberField(TEXT("owned_objects_created_count"),0);R.Data->SetBoolField(TEXT("native_input_owned_lifecycle"),!Scope.OwnedRead->ReferenceClear);R.Data->SetStringField(TEXT("input_action_owned_scope"),TEXT("Exact native Action owned lifecycle: unchanged complete Get arrays reuse current identities; exact held same-target class-export paths reorder/reuse; DeadZone/Negate/ResponseCurveUser/Scalar/SwizzleAxis/ResponseCurveExponential/FOVScaling/ScaleByDeltaTime modifier and Down/Released/Pressed trigger complete literals allocate replacements. Old linked/unlinked instances remain immutable and held. No in-place old-object field writes, destruction, rename, GC, caller snapshot authority or cleanup/retry."));R.Data->SetStringField(TEXT("input_action_callback_scope"),TEXT("Non-null Action property event preserves per-Action modifier/trigger delegates and static trigger tracking. Loaded stock node referencers are rejected; private listeners/tracking sets and next-tick reconstruction completion are not verified. Incomplete current endpoint stops explicit writer/save; mandatory transaction unwind may still run."));}
    R.Data->SetStringField(TEXT("admission_scope"),Scope.OwnedWriter()?TEXT("Exact native InputAction: six existing scalars and two ordered owned arrays; DeadZone/Negate/ResponseCurveUser/Scalar/SwizzleAxis/ResponseCurveExponential/FOVScaling/ScaleByDeltaTime modifier and Down/Released/Pressed trigger literals allocate finite replacements, held exact same-target matching-family class-export identities may be reused/reordered, unchanged Get arrays preserve identity. Null settings, old owned immutability and loaded stock ValueType/Triggers consumer guards remain. No caller token, runtime input, cold persistence of unlinked old objects or future tick completion proof."):TEXT("Exact native InputAction lifecycle plus ValueType/AccumulationBehavior and four scalar bool fields; empty Triggers/Modifiers/null PlayerMappableKeySettings and no owned exports. Loaded stock action node referencers reject ValueType notification before Modify; no input mapping/context/key/runtime expansion."));
    if(Scope.OwnedRead&&Scope.OwnedRead->ReferenceClear)
    {
        R.Data->SetStringField(TEXT("admission_scope"),TEXT("Exact native Action explicit empty Modifiers/Triggers reference clear and existing six scalars. Old owned inventory remains held/current, settings null, loaded stock ValueType/Triggers consumer guard preserved; no allocation/replacement/recovery."));
        R.Data->SetStringField(TEXT("input_action_owned_scope"),R.Data->GetStringField(TEXT("owned_reference_clear_scope")));
    }
    if(Scope.OwnedRead&&Scope.OwnedRead->PlayerSettingsHost)R.Data->SetStringField(TEXT("admission_scope"),TEXT("Existing Action scalars with immutable preexisting same-owner native PlayerMappableKeySettings and unchanged arrays; no settings/inline object writer or runtime rebinding."));
    R.Data->SetStringField(TEXT("description_cook_scope"),TEXT("InputAction scalar fields are native runtime asset payload; actual input processing, Blueprint consumer updates and vehicle runtime are not executed or verified."));
    if (bSave && !A->GetOutermost()->IsFullyLoaded()) { Fail(R, TEXT("data_asset_save_not_ready"), TEXT("Already fully loaded package required before a requested-save setter.")); return Encode(R); }
    Count(R, TEXT("legacy_get_calls"));
    if (!Load(C, R, TEXT("legacy_get_before_load")) || !Capture(C, Before, R, TEXT("before_observation")))
    { Fail(R, TEXT("data_asset_before_unobservable"), TEXT("Complete original before-load/serialization baseline required before Modify."), true); return Encode(R); }
    R.Data->SetObjectField(TEXT("before"), LegacyGet(C, Before)); R.Data->SetStringField(TEXT("class"), C.Class->GetPathName());
    auto Run=[&]()
    {
    R.Stage = TEXT("apply_properties");
    if(Scope.OwnedRead&&!Scope.OwnedRead->BeginTargetModify()){return;}
    Count(R, TEXT("explicit_modify_calls")); A->Modify();if(!Scope.Check()){return;}
    TArray<TSharedPtr<FJsonValue>> Applied, Failed, Attempts, Notifications; TArray<FProperty*> Properties; TSet<FString> AppliedNames;TSet<FName> Selected;
    for (const auto& Pair : Requested->Values)
    {
        if(!Scope.Check()){return;}
        const FString Name(Pair.Key.ToView()); auto Row = MakeShared<FJsonObject>(); Attempts.Add(MakeShared<FJsonValueObject>(Row)); R.Data->SetArrayField(TEXT("property_attempts"), Attempts);
        Row->SetStringField(TEXT("property"), Name); Row->SetBoolField(TEXT("conversion_attempted"), false); Row->SetBoolField(TEXT("storage_copy_returned"), false);
        FProperty* P = Name.Len() < NAME_SIZE && !HasNul(Name) ? C.Class->FindPropertyByName(FName(*Name)) : nullptr; FString Error;
        if (!P) { Error = TEXT("Property not found"); }
        else if(Scope.OwnedRead&&Scope.OwnedRead->PlayerSettingsHost&&(P==Scope.Triggers||P==Scope.Modifiers)){Error=TEXT("Populated player settings first writer preserves arrays and settings; only existing Action scalars are writable");}
        else if(Scope.OwnedRead&&Scope.OwnedRead->ReferenceClear&&(P==Scope.Triggers||P==Scope.Modifiers))
        {
            FArrayProperty* Array=CastField<FArrayProperty>(P);
            if(!Pair.Value.IsValid()||Pair.Value->Type!=EJson::Array||!Pair.Value->AsArray().IsEmpty()||Scope.WrittenArrays.Contains(Array)){Error=TEXT("Explicit empty selected array once per call required; null/nonempty/replacement/duplicate aliases are unsupported");}
            else
            {
                if(!Scope.OwnedRead->ClearReferences(Array,*Row)){return;}
                Applied.Add(MakeShared<FJsonValueString>(Name));Properties.Add(P);AppliedNames.Add(Name);Selected.Add(P->GetFName());Scope.WrittenArrays.Add(Array);R.Data->SetArrayField(TEXT("applied"),Applied);
                if(!Scope.Check(P)){return;}
            }
        }
        else if(Scope.OwnedWriter()&&(P==Scope.Triggers||P==Scope.Modifiers))
        {
            FArrayProperty* Array=CastField<FArrayProperty>(P);
            if(!Scope.PlanArray(Array,Pair.Value,Error)){if(Scope.Stopped){return;}if(Error.IsEmpty()){Error=TEXT("Finite Action lifecycle array admission failed");}}
            else
            {
                if(!Scope.ConfigureArray(Array,*Row)){return;}FDefaultConstructedPropertyElement Scratch(P);TArray<FString> ExpectedLinks;
                if(!Scope.ScratchArray(Array,Scratch.GetObjAddress(),ExpectedLinks)){return;}Row->SetBoolField(TEXT("conversion_attempted"),true);Row->SetBoolField(TEXT("conversion_returned"),true);Row->SetBoolField(TEXT("converted"),true);
                Scope.OwnedRead->MappingCopyInFlight=true;Count(R,TEXT("property_storage_copy_calls"));P->CopyCompleteValue(P->ContainerPtrToValuePtr<void>(A),Scratch.GetObjAddress());Row->SetBoolField(TEXT("storage_copy_returned"),true);
                Applied.Add(MakeShared<FJsonValueString>(Name));Properties.Add(P);AppliedNames.Add(Name);Selected.Add(P->GetFName());R.Data->SetArrayField(TEXT("applied"),Applied);Scope.WrittenArrays.Add(Array);
                if(!Scope.OwnedRead->AcceptActionCopy(Array,ExpectedLinks)||!Scope.Check(P)){Scope.Stop(TEXT("data_asset_action_owned_array_unknown"),TEXT("Complete selected array-copy endpoint unavailable; actual created/copy prefix retained."));return;}
                TArray<FString> SelectedLinks;const FString At=Array==Scope.Triggers?TEXT("Triggers"):TEXT("Modifiers");for(const auto& L:ExpectedLinks){if(L.StartsWith(At+TEXT("#"))||L.StartsWith(At+TEXT("/"))){SelectedLinks.Add(L);}}Scope.VerifiedArrayLinks.Add(P->GetFName(),MoveTemp(SelectedLinks));
            }
        }
        else if(Scope.Scalars.FindRef(P->GetFName())!=P){Error=TEXT("Only exact InputAction ValueType/AccumulationBehavior and the four native bool fields are writable");}
        else if(!Pair.Value.IsValid()){Error=TEXT("Missing scalar value");}
        else if(CastField<FBoolProperty>(P)&&Pair.Value->Type!=EJson::Boolean){Error=TEXT("InputAction bool requires JSON true or false; original coercion variants remain incomplete");}
        else if(CastField<FEnumProperty>(P)&&(Pair.Value->Type!=EJson::String||!Scope.EnumTokens.FindChecked(P->GetFName()).ContainsByPredicate([&](const FString& Token){return Token.Equals(Pair.Value->AsString(),ESearchCase::CaseSensitive);})))
        {Error=TEXT("Use an exact currently verified authored native enum token; numbers, MAX and unknown tokens are unsupported");}
        else
        {
            FDefaultConstructedPropertyElement Scratch(P); Row->SetBoolField(TEXT("conversion_attempted"), true); Count(R, TEXT("json_conversion_calls"));
            const bool Converted=FJsonObjectConverter::JsonValueToUProperty(Pair.Value,P,Scratch.GetObjAddress(),0,0);
            if(!Scope.Check()){return;}
            if (!Converted) { Error = TEXT("Value conversion failed (check type)"); }
            else { Count(R, TEXT("property_storage_copy_calls")); P->CopyCompleteValue(P->ContainerPtrToValuePtr<void>(A), Scratch.GetObjAddress()); Row->SetBoolField(TEXT("storage_copy_returned"), true); Applied.Add(MakeShared<FJsonValueString>(Name)); Properties.Add(P); AppliedNames.Add(Name);Selected.Add(P->GetFName());R.Data->SetArrayField(TEXT("applied"),Applied);if(!Scope.Check(P)){return;} }
        }
        if (!Error.IsEmpty()) { auto F = MakeShared<FJsonObject>(); F->SetStringField(TEXT("property"), Name); F->SetStringField(TEXT("error"), Error); Failed.Add(MakeShared<FJsonValueObject>(F)); Row->SetStringField(TEXT("error"), Error); }
        R.Data->SetArrayField(TEXT("applied"), Applied); R.Data->SetArrayField(TEXT("failed"), Failed);
    }
    for (FProperty* P : Properties)
    {
        if(!Scope.Check()){return;}
        auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("property"), P->GetName()); J->SetBoolField(TEXT("attempted"), true); Notifications.Add(MakeShared<FJsonValueObject>(J)); R.Data->SetArrayField(TEXT("property_notifications"), Notifications);
        Count(R, TEXT("post_edit_change_property_calls")); FPropertyChangedEvent Event(P); A->PostEditChangeProperty(Event); J->SetBoolField(TEXT("returned"), true);if(!Scope.Check()){return;}
    }
    if (Applied.Num() > 0) { Count(R, TEXT("mark_package_dirty_calls")); A->MarkPackageDirty();if(!Scope.Check()){return;} }
    if (!Capture(C, Configured, R, TEXT("after_configuration"))) { Fail(R, TEXT("data_asset_configuration_unobservable"), TEXT("Actual conversion/callback prefix incomplete; save suppressed."), true); return; }
    R.Known = true; R.Changed = !Scope.CreatedOwned.IsEmpty()||!Equal(Before.State->GetObjectField(TEXT("values")).ToSharedRef(), Configured.State->GetObjectField(TEXT("values")).ToSharedRef())||(Scope.OwnedWriter()&&BeforeOwnedLinks!=Scope.OwnedRead->Links); R.Data->SetBoolField(TEXT("observed_state_changed"), !Equal(Before.State, Configured.State));
    if (!ActionOwnedSetFences(Before,Configured,Scope,R,Selected)) { return; } R.Data->SetBoolField(TEXT("after_configuration_source_preserved"), true); R.Data->SetBoolField(TEXT("after_configuration_unselected_state_preserved"), true);
    if (!Save(C, Configured, R)) { return; }
    const bool FinalOK = Final(C, Configured, After, R); R.Data->SetBoolField(TEXT("final_preservation_passed"), FinalOK); if (!After.Complete) { return; }
    TArray<TSharedPtr<FJsonValue>> Results; int32 Updated = 0, Skipped = 0, PropertyFailure = 0;
    for (const auto& Pair : Requested->Values)
    {
        const FString Name(Pair.Key.ToView()); const auto B = ValueByName(Before.State->GetObjectField(TEXT("values")).ToSharedRef(), Name), Z = ValueByName(After.State->GetObjectField(TEXT("values")).ToSharedRef(), Name);
        const bool LegacyWas=PreviewMatchesExpected(Pair.Value,B), LegacyIs=PreviewMatchesExpected(Pair.Value,Z), Did=AppliedNames.Contains(Name);bool NativeIs=false,NativeWas=false;
            if(Did&&Scope.OwnedWriter()&&!Scope.OwnedRead->ReferenceClear){const auto* Expected=Scope.VerifiedArrayLinks.Find(FName(*Name));TArray<FString> ActualLinks,PriorLinks;const FString At=FName(*Name)==TEXT("Triggers")?TEXT("Triggers"):TEXT("Modifiers");for(const auto& L:Scope.OwnedRead->Links){if(L.StartsWith(At+TEXT("#"))||L.StartsWith(At+TEXT("/"))){ActualLinks.Add(L);}}for(const auto& L:BeforeOwnedLinks){if(L.StartsWith(At+TEXT("#"))||L.StartsWith(At+TEXT("/"))){PriorLinks.Add(L);}}NativeIs=Expected&&*Expected==ActualLinks;NativeWas=Expected&&*Expected==PriorLinks;}const bool Is=LegacyIs||NativeIs,Was=LegacyWas||NativeWas;FString Outcome;
        if (!Did) { Outcome = TEXT("conversion_failed"); ++PropertyFailure; } else if (!Is) { Outcome = TEXT("readback_failed"); ++PropertyFailure; } else if (Was) { Outcome = TEXT("skipped_unchanged"); ++Skipped; } else { Outcome = TEXT("updated"); ++Updated; }
        auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("property"), Name); J->SetBoolField(TEXT("applied"), Did); J->SetBoolField(TEXT("before_matched_expected"), Was); J->SetBoolField(TEXT("after_matched_expected"), Is);J->SetBoolField(TEXT("legacy_before_matched_expected"),LegacyWas);J->SetBoolField(TEXT("legacy_after_matched_expected"),LegacyIs);J->SetBoolField(TEXT("owned_identity_plan_verified"),NativeIs);J->SetBoolField(TEXT("owned_identity_plan_already_matched"),NativeWas); J->SetStringField(TEXT("outcome"), Outcome); Results.Add(MakeShared<FJsonValueObject>(J));
    }
    const bool SaveOK = !bSave || R.Saved, Specific = PropertyFailure == 0 && SaveOK; const int32 CompletionFailure = SaveOK ? 0 : 1, FailedCount = SaveOK ? PropertyFailure : FMath::Max(PropertyFailure, Requested->Values.Num());
    R.Data->SetArrayField(TEXT("property_results"), Results); R.Data->SetNumberField(TEXT("updated_property_count"), Updated); R.Data->SetNumberField(TEXT("skipped_property_count"), Skipped); R.Data->SetNumberField(TEXT("property_failure_count"), PropertyFailure); R.Data->SetNumberField(TEXT("completion_failure_count"), CompletionFailure);
    auto Delta = MakeShared<FJsonObject>(); Delta->SetNumberField(TEXT("affected_count"), Updated); Delta->SetNumberField(TEXT("created_count"), 0); Delta->SetNumberField(TEXT("updated_count"), Updated); Delta->SetNumberField(TEXT("removed_count"), 0); Delta->SetNumberField(TEXT("skipped_count"), Skipped); Delta->SetNumberField(TEXT("property_failure_count"), PropertyFailure); Delta->SetNumberField(TEXT("completion_failure_count"), CompletionFailure); Delta->SetNumberField(TEXT("failed_count"), FailedCount);
    Contract(R, R.Data->GetObjectField(TEXT("before")).ToSharedRef(), R.Data->GetObjectField(TEXT("after")).ToSharedRef(), Delta, Applied.Num() > 0||!Scope.CreatedOwned.IsEmpty(), R.Changed, Failed.Num() == 0, Specific, TEXT("data_asset_property_serialization_readback"), { TEXT("asset_resolved_after"), TEXT("requested_properties_match"), TEXT("save_gate") });
    R.Known = true; R.Changed = !Scope.CreatedOwned.IsEmpty()||!Equal(Before.State->GetObjectField(TEXT("values")).ToSharedRef(),Configured.State->GetObjectField(TEXT("values")).ToSharedRef())||(Scope.OwnedWriter()&&BeforeOwnedLinks!=Scope.OwnedRead->Links); R.Data->SetStringField(TEXT("legacy_verification_scope"), TEXT("Original selected property/count/save predicate, independent from extra native whole-state/source preservation. Performed and changed differ on matching/empty setter; actual work counts remain authoritative."));
    if (!FinalOK) { PreviewInvalidateVerification(R);return; }
    if (!Specific || Failed.Num() > 0) { Fail(R, Failed.Num() == 0 ? TEXT("mutation_postcondition_failed") : TEXT("data_asset_properties_failed"), TEXT("Actual terminal partial property/readback/save failure retained; no retry.")); return; }
    R.Success = R.Post = true; R.Stage = TEXT("complete"); return;
    };
    {
        R.Attempted=true;R.Known=false;Count(R,TEXT("outer_transaction_calls"));const FScopedTransaction Transaction(NSLOCTEXT("CubelessDataAsset","Set","Set DataAsset properties"));
        if(Scope.Check()){Run();}
    }
    if(Scope.OwnedWriter()&&!After.Complete){R.Known=false;PreviewInvalidateVerification(R);R.Data->SetBoolField(TEXT("transaction_fence_passed"),false);R.Data->SetStringField(TEXT("input_action_owned_transaction_scope"),TEXT("No safe complete terminal endpoint after mandatory transaction unwind; actual allocation/copy/save prefix retained with unknown change state and no follow-up read/retry."));}
    if(Scope.Stopped){if(Scope.OwnedWriter()){R.Known=false;}PreviewInvalidateVerification(R);return Encode(R);}
    if(After.Complete)
    {
        FObservation Fence;const FString Stage=R.Stage;R.Stage=TEXT("after_transaction_end");
        const bool Observed=Scope.Check()&&Capture(C,Fence,R,TEXT("input_action_after_transaction_observation"));
        const bool Same=Observed&&Equal(After.State,Fence.State)&&Equal(After.Source,Fence.Source)&&Equal(After.Refs,Fence.Refs)&&Equal(After.File,Fence.File)&&Equal(After.Other,Fence.Other);
        R.Data->SetBoolField(TEXT("transaction_fence_passed"),Same);
        if(!Same){Scope.Stop(TEXT("data_asset_input_action_transaction_changed"),TEXT("Transaction-end callback changed prepared final state; actual save receipt/prefix retained."));PreviewInvalidateVerification(R);}else{R.Stage=Stage;}
    }
    return Encode(R);
}

bool MappingSetFences(const FObservation& Before,const FObservation& After,FMappingContextScope& Scope,FResult& R,const TSet<FName>& Selected)
{
    if(Scope.OwnedRead&&Scope.OwnedRead->ReferenceClear)
    {
        FObservation Expected=Before;if(!ReferenceClearExpectedReferences(Before,After,*Scope.OwnedRead,&Scope,R,Expected.Refs)){return false;}
        R.Data->SetStringField(TEXT("references_scope"),TEXT("Exact cleared owned slots and admitted row.Action root-only contributions may leave; old owned Curve/other contributions and immutable Action identity/fields/disk remain protected. All unrelated references are literal-strict."));
        return Fences(Expected,After,false,R,&Selected)&&Scope.Check();
    }
    FObservation Left=Before,Right=After;TSharedRef<FJsonObject> ExpectedRefs=Before.Refs;if(Scope.OwnedWriter()&&!OwnedLifecycleReferences(Before,*Scope.OwnedRead,R,ExpectedRefs)){return false;}Left.Refs=MakeShared<FJsonObject>();Right.Refs=MakeShared<FJsonObject>();
    // Only admitted immutable action identities may enter/leave the selected mapping references.
    for(const auto& Pair:ExpectedRefs->Values){bool Action=false;for(const auto& S:Scope.Actions){Action|=FString(Pair.Key.ToView()).Equals(S.Path,ESearchCase::CaseSensitive);}if(!Action){Left.Refs->SetField(FString(Pair.Key.ToView()),Pair.Value);}}
    for(const auto& Pair:After.Refs->Values){bool Action=false;for(const auto& S:Scope.Actions){Action|=FString(Pair.Key.ToView()).Equals(S.Path,ESearchCase::CaseSensitive);}if(!Action){Right.Refs->SetField(FString(Pair.Key.ToView()),Pair.Value);}}
    if(Scope.OwnedWriter()&&!Scope.CreatedOwned.IsEmpty())
    {
        Right.State=Scope.OwnedRead->CopySnapshot(After.State);
        for(UObject* O:Scope.CreatedOwned)
        {
            if(!Scope.Check()||Before.State->GetObjectField(TEXT("owned_exports"))->HasField(O->GetPathName())||!Right.State->GetObjectField(TEXT("owned_exports"))->HasField(O->GetPathName())){return Scope.Stop(TEXT("data_asset_mapping_owned_export_prefix_changed"),TEXT("Created direct-owned export prefix differs from the inspected endpoint."));}
            Right.State->GetObjectField(TEXT("owned_exports"))->RemoveField(O->GetPathName());
            for(UObject* Ref:TArray<UObject*>{O,O->GetClass(),O->GetClass()->GetDefaultObject(false)})
            {if(Ref){const FString Path=Ref->GetPathName();if(!Before.Refs->HasField(Path)){Right.Refs->RemoveField(Path);}}}
        }
    }
    const bool Same=Fences(Left,Right,false,R,&Selected);R.Data->SetStringField(TEXT("references_scope"),TEXT("Selected default-mapping Action references and exact old-owned root contributions derived from accepted slots may change; every admitted Action identity/fields/owned exports/disk and old owned field remains frozen. All remaining captured references are literal-equal."));return Same&&Scope.Check();
}

FString SetMappingContext(FContext& C,FResult& R,const TSharedRef<FJsonObject>& Requested,bool bSave)
{
    FMappingContextScope& Scope=*C.Mapping;UDataAsset* A=C.Asset.Get();FObservation Before,Configured,After;const TArray<FString> BeforeOwnedLinks=Scope.OwnedWriter()?Scope.OwnedRead->Links:TArray<FString>{};const TArray<TSharedPtr<FJsonValue>> BeforeRawRows=Scope.RowsBefore;
    if(Scope.OwnedWriter())
    {
        R.Data->SetBoolField(TEXT("native_input_owned_lifecycle"),!Scope.OwnedRead->ReferenceClear);R.Data->SetStringField(TEXT("admission_scope"),TEXT("Exact native Context populated lifecycle: default rows/keys/Action references and selected owned slots can be replaced/reordered; eleven finite complete native literal classes allocate new instances and exact held same-target matching-family class-export identities reuse any of the eleven read classes. Exact unchanged Get JSON keeps its no-op path. Old objects remain immutable/held; no general class, curve literal, profile/settings, destruction/rename/GC or runtime expansion."));
        R.Data->SetStringField(TEXT("input_mapping_partial_contract"),TEXT("All finite rows/classes/fields/keys/references are planned before allocation. Each NewObject return/field-copy and complete mapping-copy prefix is retained. Direct context-owned allocations can remain unlinked if later scratch/copy fails. This is not original direct-converter array-prefix parity, atomic rollback, automatic cleanup/retry or proof unlinked objects persist after cold load."));
        R.Data->SetNumberField(TEXT("owned_objects_created_count"),0);
    }
    if(Scope.OwnedRead&&Scope.OwnedRead->ReferenceClear)
    {
        R.Data->SetStringField(TEXT("admission_scope"),TEXT("Exact native Context explicit empty DefaultKeyMappings.Mappings reference clear only. All old linked/unlinked owned instances and Action/Curve sources remain held/current. No UnmapAll/rebuild, allocation/replacement/profile/deprecated write or recovery."));
        R.Data->SetStringField(TEXT("input_mapping_partial_contract"),R.Data->GetStringField(TEXT("owned_reference_clear_scope")));
    }
    if(!Scope.Check()){return Encode(R);}
    if (bSave && !A->GetOutermost()->IsFullyLoaded()) { Fail(R, TEXT("data_asset_save_not_ready"), TEXT("Already fully loaded package required before a requested-save setter.")); return Encode(R); }
    Count(R, TEXT("legacy_get_calls"));
    if (!Load(C, R, TEXT("legacy_get_before_load")) || !Capture(C, Before, R, TEXT("before_observation")))
    { Fail(R, TEXT("data_asset_before_unobservable"), TEXT("Complete original before-load/serialization baseline required before Modify."), true); return Encode(R); }
    R.Data->SetObjectField(TEXT("before"), LegacyGet(C, Before)); R.Data->SetStringField(TEXT("class"), C.Class->GetPathName());
    auto Run=[&]()
    {
    R.Stage = TEXT("apply_properties");
    if(Scope.OwnedRead&&!Scope.OwnedRead->BeginTargetModify()){return;}
    Count(R, TEXT("explicit_modify_calls")); A->Modify();if(!Scope.Check()){return;}
    TArray<TSharedPtr<FJsonValue>> Applied, Failed, Attempts, Notifications; TArray<FProperty*> Properties; TSet<FString> AppliedNames;TSet<FName> Selected;
    for (const auto& Pair : Requested->Values)
    {
        if(!Scope.Check()){return;}
        const FString Name(Pair.Key.ToView()); auto Row = MakeShared<FJsonObject>(); Attempts.Add(MakeShared<FJsonValueObject>(Row)); R.Data->SetArrayField(TEXT("property_attempts"), Attempts);
        Row->SetStringField(TEXT("property"), Name); Row->SetBoolField(TEXT("conversion_attempted"), false); Row->SetBoolField(TEXT("storage_copy_returned"), false);
        FProperty* P = Name.Len() < NAME_SIZE && !HasNul(Name) ? C.Class->FindPropertyByName(FName(*Name)) : nullptr; FString Error;
        if (!P) { Error = TEXT("Property not found"); }
        else if(P!=Scope.Defaults){Error=TEXT("Only actual DefaultKeyMappings native struct is writable; deprecated Mappings/profiles/other fields remain protected");}
        else if(Scope.OwnedWriter()&&Selected.Contains(P->GetFName())){Error=TEXT("The selected default mapping property may be written once per call; duplicate aliases are unsupported");}
        else if(!Scope.Request(Pair.Value,Error)){if(Scope.Stopped){return;}if(Error.IsEmpty()){Error=TEXT("Finite mapping row admission failed");}}
        else if(Scope.OwnedRead&&Scope.OwnedRead->ReferenceClear)
        {
            TArray<FString> RemovedActions;
            for(const auto& BeforeRow:Scope.RowsBefore)
            {
                const FString Path=FPackageName::ExportTextPathToObjectPath(BeforeRow->AsObject()->GetStringField(TEXT("Action")));
                if(!Scope.Actions.ContainsByPredicate([&](const FMappingContextScope::FActionSource& Source){return Source.Path.Equals(Path,ESearchCase::CaseSensitive);})){Scope.Stop(TEXT("data_asset_mapping_clear_action_unproven"),TEXT("Selected old row.Action is not an admitted held source; no clear/save/retry."));return;}if(!RemovedActions.ContainsByPredicate([&](const FString& ExistingPath){return ExistingPath.Equals(Path,ESearchCase::CaseSensitive);})){RemovedActions.Add(Path);}
            }
            if(!Scope.OwnedRead->ClearReferences(Scope.Rows,*Row)){return;}for(const auto& Path:RemovedActions){if(!Scope.OwnedRead->ClearedActionPaths.ContainsByPredicate([&](const FString& ExistingPath){return ExistingPath.Equals(Path,ESearchCase::CaseSensitive);})){Scope.OwnedRead->ClearedActionPaths.Add(Path);}}
            Applied.Add(MakeShared<FJsonValueString>(Name));Properties.Add(P);AppliedNames.Add(Name);Selected.Add(P->GetFName());R.Data->SetArrayField(TEXT("applied"),Applied);
            if(!Scope.Check(P)){return;}Row->SetArrayField(TEXT("stored_rows_after_clear"),Scope.RowsBefore);
        }
        else if(Scope.OwnedWriter()&&Scope.MatchingOwned)
        {
            // Reuse only the internally inspected same-slot pointers; no object/default/array replacement.
            Row->SetBoolField(TEXT("existing_owned_roundtrip_matched"),true);Row->SetNumberField(TEXT("existing_owned_reused_slots"),Scope.OwnedPlan.Num());
            Applied.Add(MakeShared<FJsonValueString>(Name));Properties.Add(P);AppliedNames.Add(Name);Selected.Add(P->GetFName());if(!Scope.Check()){return;}Scope.VerifiedMappingLinks=Scope.OwnedRead->Links;Scope.MappingRequestVerified=true;
        }
        else
        {
            if(Scope.OwnedWriter()&&!Scope.ConfigureOwned(*Row)){return;}
            FDefaultConstructedPropertyElement Scratch(P); Row->SetBoolField(TEXT("conversion_attempted"), true); Count(R, TEXT("json_conversion_calls"));
            const bool Converted=FJsonObjectConverter::JsonValueToUProperty(Scope.OwnedWriter()?Scope.PlainMapping:Pair.Value,P,Scratch.GetObjAddress(),0,0);
            if(!Scope.Check()){return;}
            Row->SetBoolField(TEXT("conversion_returned"),true);Row->SetBoolField(TEXT("converted"),Converted);
            if (!Converted) { Error = TEXT("Scratch row conversion failed; allocated direct-owned prefix retained, target resize/earlier-row partial import was not attempted"); }
            else
            {
                TArray<FString> PlannedLinks;if(Scope.OwnedWriter()&&!Scope.AttachOwned(Scratch.GetObjAddress(),PlannedLinks,Error)){if(Scope.Stopped||!Scope.Check()){return;}}
                TArray<TSharedPtr<FJsonValue>> ScratchRows;const bool Accepted=Error.IsEmpty()&&Scope.ObserveRows(Scratch.GetObjAddress(),false,ScratchRows,Error);Row->SetArrayField(TEXT("scratch_rows"),ScratchRows);
                if(Scope.Stopped||!Scope.Check()){return;}
                if(Accepted)
                {
                    if(Scope.OwnedWriter()){Scope.OwnedRead->MappingCopyInFlight=true;}
                    Count(R,TEXT("property_storage_copy_calls"));P->CopyCompleteValue(P->ContainerPtrToValuePtr<void>(A),Scratch.GetObjAddress());Row->SetBoolField(TEXT("storage_copy_returned"),true);
                    Applied.Add(MakeShared<FJsonValueString>(Name));Properties.Add(P);AppliedNames.Add(Name);Selected.Add(P->GetFName());R.Data->SetArrayField(TEXT("applied"),Applied);
                    if(Scope.OwnedWriter()&&!Scope.OwnedRead->AcceptMappingCopy(PlannedLinks)){Scope.Stop(TEXT("data_asset_mapping_owned_copy_unknown"),TEXT("Copied owned slot endpoint unavailable; prefix retained, no next explicit writer/save."));return;}
                    if(!Scope.Check(P)){return;}Row->SetArrayField(TEXT("stored_rows_after_copy"),Scope.RowsBefore);
                    if(Scope.OwnedWriter()&&!Equal(MakeShared<FJsonValueArray>(ScratchRows),MakeShared<FJsonValueArray>(Scope.RowsBefore))){Scope.Stop(TEXT("data_asset_mapping_owned_raw_copy_changed"),TEXT("Stored raw eight-field rows differ from the complete planned scratch rows."));return;}
                    if(Scope.OwnedWriter()){Scope.VerifiedMappingLinks=PlannedLinks;Scope.MappingRequestVerified=true;}
                }else if(Error.IsEmpty()){Error=TEXT("Converted mapping row storage is unsupported");}
            }
        }
        if (!Error.IsEmpty()) { auto F = MakeShared<FJsonObject>(); F->SetStringField(TEXT("property"), Name); F->SetStringField(TEXT("error"), Error); Failed.Add(MakeShared<FJsonValueObject>(F)); Row->SetStringField(TEXT("error"), Error); }
        R.Data->SetArrayField(TEXT("applied"), Applied); R.Data->SetArrayField(TEXT("failed"), Failed);
    }
    for (FProperty* P : Properties)
    {
        if(!Scope.Check()){return;}
        auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("property"), P->GetName()); J->SetBoolField(TEXT("attempted"), true); Notifications.Add(MakeShared<FJsonValueObject>(J)); R.Data->SetArrayField(TEXT("property_notifications"), Notifications);
        Count(R, TEXT("post_edit_change_property_calls")); FPropertyChangedEvent Event(P); A->PostEditChangeProperty(Event); J->SetBoolField(TEXT("returned"), true);if(!Scope.Check()){return;}
    }
    if (Applied.Num() > 0) { Count(R, TEXT("mark_package_dirty_calls")); A->MarkPackageDirty();if(!Scope.Check()){return;} }
    if (!Capture(C, Configured, R, TEXT("after_configuration"))) { Fail(R, TEXT("data_asset_configuration_unobservable"), TEXT("Actual conversion/callback prefix incomplete; save suppressed."), true); return; }
    R.Known = true; R.Changed = !Scope.CreatedOwned.IsEmpty()||!Equal(Before.State->GetObjectField(TEXT("values")).ToSharedRef(), Configured.State->GetObjectField(TEXT("values")).ToSharedRef())||(Scope.OwnedWriter()&&BeforeOwnedLinks!=Scope.OwnedRead->Links); R.Data->SetBoolField(TEXT("observed_state_changed"), !Equal(Before.State, Configured.State));
    if (!MappingSetFences(Before,Configured,Scope,R,Selected)) { return; } R.Data->SetBoolField(TEXT("after_configuration_source_preserved"), true); R.Data->SetBoolField(TEXT("after_configuration_unselected_state_preserved"), true);
    if (!Save(C, Configured, R)) { return; }
    const bool FinalOK = Final(C, Configured, After, R); R.Data->SetBoolField(TEXT("final_preservation_passed"), FinalOK); if (!After.Complete) { return; }
    TArray<TSharedPtr<FJsonValue>> Results; int32 Updated = 0, Skipped = 0, PropertyFailure = 0;
    for (const auto& Pair : Requested->Values)
    {
        const FString Name(Pair.Key.ToView()); const auto B = ValueByName(Before.State->GetObjectField(TEXT("values")).ToSharedRef(), Name), Z = ValueByName(After.State->GetObjectField(TEXT("values")).ToSharedRef(), Name);
        const bool LegacyWas=PreviewMatchesExpected(Pair.Value,B), LegacyIs=PreviewMatchesExpected(Pair.Value,Z), Did=AppliedNames.Contains(Name);bool NativeIs=false,NativeWas=false;
            if(Did&&Scope.OwnedWriter()&&!Scope.OwnedRead->ReferenceClear){NativeIs=Scope.MappingRequestVerified&&FName(*Name)==TEXT("DefaultKeyMappings")&&Scope.VerifiedMappingLinks==Scope.OwnedRead->Links;NativeWas=NativeIs&&Scope.VerifiedMappingLinks==BeforeOwnedLinks&&Equal(MakeShared<FJsonValueArray>(BeforeRawRows),MakeShared<FJsonValueArray>(Scope.RowsBefore));}const bool Is=LegacyIs||NativeIs,Was=LegacyWas||NativeWas;FString Outcome;
        if (!Did) { Outcome = TEXT("conversion_failed"); ++PropertyFailure; } else if (!Is) { Outcome = TEXT("readback_failed"); ++PropertyFailure; } else if (Was) { Outcome = TEXT("skipped_unchanged"); ++Skipped; } else { Outcome = TEXT("updated"); ++Updated; }
        auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("property"), Name); J->SetBoolField(TEXT("applied"), Did); J->SetBoolField(TEXT("before_matched_expected"), Was); J->SetBoolField(TEXT("after_matched_expected"), Is);J->SetBoolField(TEXT("legacy_before_matched_expected"),LegacyWas);J->SetBoolField(TEXT("legacy_after_matched_expected"),LegacyIs);J->SetBoolField(TEXT("owned_identity_plan_verified"),NativeIs);J->SetBoolField(TEXT("owned_identity_plan_already_matched"),NativeWas); J->SetStringField(TEXT("outcome"), Outcome); Results.Add(MakeShared<FJsonValueObject>(J));
    }
    const bool SaveOK = !bSave || R.Saved, Specific = PropertyFailure == 0 && SaveOK; const int32 CompletionFailure = SaveOK ? 0 : 1, FailedCount = SaveOK ? PropertyFailure : FMath::Max(PropertyFailure, Requested->Values.Num());
    R.Data->SetArrayField(TEXT("property_results"), Results); R.Data->SetNumberField(TEXT("updated_property_count"), Updated); R.Data->SetNumberField(TEXT("skipped_property_count"), Skipped); R.Data->SetNumberField(TEXT("property_failure_count"), PropertyFailure); R.Data->SetNumberField(TEXT("completion_failure_count"), CompletionFailure);
    auto Delta = MakeShared<FJsonObject>(); Delta->SetNumberField(TEXT("affected_count"), Updated); Delta->SetNumberField(TEXT("created_count"), 0); Delta->SetNumberField(TEXT("updated_count"), Updated); Delta->SetNumberField(TEXT("removed_count"), 0); Delta->SetNumberField(TEXT("skipped_count"), Skipped); Delta->SetNumberField(TEXT("property_failure_count"), PropertyFailure); Delta->SetNumberField(TEXT("completion_failure_count"), CompletionFailure); Delta->SetNumberField(TEXT("failed_count"), FailedCount);
    Contract(R, R.Data->GetObjectField(TEXT("before")).ToSharedRef(), R.Data->GetObjectField(TEXT("after")).ToSharedRef(), Delta, Applied.Num() > 0||!Scope.CreatedOwned.IsEmpty(), R.Changed, Failed.Num() == 0, Specific, TEXT("data_asset_property_serialization_readback"), { TEXT("asset_resolved_after"), TEXT("requested_properties_match"), TEXT("save_gate") });
    R.Known = true; R.Changed = !Scope.CreatedOwned.IsEmpty()||!Equal(Before.State->GetObjectField(TEXT("values")).ToSharedRef(),Configured.State->GetObjectField(TEXT("values")).ToSharedRef())||(Scope.OwnedWriter()&&BeforeOwnedLinks!=Scope.OwnedRead->Links); R.Data->SetStringField(TEXT("legacy_verification_scope"), TEXT("Selected native DefaultKeyMappings property/count/save predicate; scratch conversion differs from original direct-array partial writes. Actual target copy/notification/save prefixes, matching/empty Modify work and semantic changes remain separate."));
    if (!FinalOK) { PreviewInvalidateVerification(R);return; }
    if (!Specific || Failed.Num() > 0) { Fail(R, Failed.Num() == 0 ? TEXT("mutation_postcondition_failed") : TEXT("data_asset_properties_failed"), TEXT("Actual terminal partial property/readback/save failure retained; no retry.")); return; }
    R.Success = R.Post = true; R.Stage = TEXT("complete"); return;
    };
    {
        R.Attempted=true;R.Known=false;Count(R,TEXT("outer_transaction_calls"));const FScopedTransaction Transaction(NSLOCTEXT("CubelessDataAsset","Set","Set DataAsset properties"));
        if(Scope.Check()){Run();}
    }
    if(Scope.Stopped){PreviewInvalidateVerification(R);return Encode(R);}
    if(Scope.OwnedWriter()&&!After.Complete)
    {R.Known=false;PreviewInvalidateVerification(R);R.Data->SetBoolField(TEXT("transaction_fence_passed"),false);R.Data->SetStringField(TEXT("input_mapping_transaction_scope"),TEXT("No safe complete terminal endpoint; mandatory outer transaction unwind may invoke unknown callbacks. Created/field/copy/save prefix retained and no followup read/retry."));}
    if(After.Complete)
    {
        FObservation Fence;const FString Stage=R.Stage;R.Stage=TEXT("after_transaction_end");
        const bool Observed=Scope.Check()&&Capture(C,Fence,R,TEXT("input_mapping_after_transaction_observation"));
        const bool Same=Observed&&Equal(After.State,Fence.State)&&Equal(After.Source,Fence.Source)&&Equal(After.Refs,Fence.Refs)&&Equal(After.File,Fence.File)&&Equal(After.Other,Fence.Other);
        R.Data->SetBoolField(TEXT("transaction_fence_passed"),Same);
        if(!Same){Scope.Stop(TEXT("data_asset_mapping_transaction_changed"),TEXT("Transaction-end callback changed prepared final state; actual save receipt/prefix retained."));PreviewInvalidateVerification(R);}else{R.Stage=Stage;}
    }
    return Encode(R);
}

// QC: concrete generated PrimaryDataAsset leaf; the existing native four-class leaves remain separate.
bool QCGeneratedTarget(const FString& Requested)
{
    if(Requested.Len()>2048||HasNul(Requested)||!Clean(Requested))return false;
    FString P=FPackageName::ExportTextPathToObjectPath(Requested).TrimStartAndEnd();P.TrimQuotesInline();P.ReplaceInline(TEXT("\\"),TEXT("/"));if(!P.Contains(TEXT(".")))P+=TEXT(".")+FPackageName::GetShortName(P);
    UDataAsset* A=FindObject<UDataAsset>(nullptr,*P);return Loaded(A)&&Loaded(A->GetClass())&&!A->GetClass()->HasAnyClassFlags(CLASS_Native);
}
bool QCActive=false;
struct FGeneratedDataAssetWork
{
    FResult& R;bool Stopped=false,Bound=false,TargetBound=false,Creating=false,SavedExports=false,SaveEntered=false;int32 Mutable=INDEX_NONE;
    TStrongObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;TStrongObjectPtr<UBlueprint> Blueprint;
    TStrongObjectPtr<UBlueprintGeneratedClass> Class,Skeleton;TStrongObjectPtr<UObject> Default,SkeletonDefault;
    // VE: the shared generated work retains the actual direct native base.
    // AssetBundleData/PreSave belongs to UPrimaryDataAsset, not UDataAsset.
    TStrongObjectPtr<UClass> NativeBase;EClassFlags NativeBaseFlags=CLASS_None;int32 NativeBaseSize=0,NativeBaseAlign=0;
    TStrongObjectPtr<UPackage> SourcePackage,TargetPackage;TStrongObjectPtr<UDataAsset> Target;TStrongObjectPtr<UDataAssetFactory> Factory,FactoryDefault;
    TArray<TStrongObjectPtr<UObject>> Holds;FString PathName,PackageName,SourceFileHash;EObjectFlags TargetFlags=RF_NoFlags;bool InitialDirty=false;
    EClassFlags ClassFlags=CLASS_None,SkeletonFlags=CLASS_None;EObjectFlags BlueprintFlags=RF_NoFlags,DefaultFlags=RF_NoFlags,SkeletonDefaultFlags=RF_NoFlags;int32 ClassSize=0,SkeletonSize=0,ClassAlign=0,SkeletonAlign=0;uint32 SourcePackageFlags=0;
    TSharedRef<FJsonObject> SourceBefore=MakeShared<FJsonObject>(),OtherBefore=MakeShared<FJsonObject>(),FileBefore=MakeShared<FJsonObject>();
    TArray<FBPVariableDescription> Declarations;
    struct FGraphRole{TStrongObjectPtr<UEdGraph> G;FGuid Id;FString Path;};TArray<FGraphRole> Graphs;
    TArray<TObjectPtr<UEdGraph>> Uber,EventStubs;
    struct FValue{int64 Integer=0;uint64 Bits=0;bool Bit=false;FString String;FName Name;int32 Shape=0;TArray<FValue> Children;};
    struct FField{FProperty* P=nullptr;FFieldClass* Kind=nullptr;FName Name;UStruct* Owner=nullptr;uint64 Flags=0;int32 Size=0,Offset=0,Dim=0;bool Frame=false;UScriptStruct* Struct=nullptr;};
    TArray<FField> Fields,SkeletonFields,FactoryFields;TArray<FValue> DefaultValues,SkeletonDefaultValues,Expected;
    FStructProperty* BundleProperty=nullptr;TStrongObjectPtr<UScriptStruct> BundleStruct,FrameStruct;
    FStructProperty* GenFrame=nullptr;FStructProperty* SkelFrame=nullptr;TStrongObjectPtr<UFunction> GenUber,SkelUber;
    UClass* FactoryDefaultClass=nullptr;TSharedRef<FJsonObject> FactoryBefore=MakeShared<FJsonObject>();
    explicit FGeneratedDataAssetWork(FResult& In):R(In){}
    bool Failure(const TCHAR* Property,const FString& ExpectedValue,const FString& ActualValue,bool Unknown=true,const FString& Object=FString())
    {
        if(!Stopped){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("object"),Object.IsEmpty()?PathName:Object);J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),ExpectedValue);J->SetStringField(TEXT("actual"),ActualValue);J->SetBoolField(TEXT("operands_observed"),false);J->SetStringField(TEXT("operand_scope"),TEXT("Ordered current comparison reason; scalar value/member-count cells contain copied operands. Generic role reasons do not claim every role operand was captured."));R.Data->SetObjectField(TEXT("generated_first_mismatch"),J);Fail(R,TEXT("data_asset_generated_current_failed"),TEXT("Actual generated asset/source prefix retained; no next SDK, rollback or automatic retry."),Unknown);}Stopped=true;ClearFinal(R);return false;
    }
    bool Reject(const TCHAR* Code,const FString& Why){return Fail(R,Code,Why);}
    bool Context()
    {
        return !Stopped&&IsEditorGameThread()&&Loaded(Editor.Get())&&GEditor==Editor.Get()&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&!IsPlaySessionActive()&&Loaded(World.Get())&&GEditor->GetEditorWorldContext().World()==World.Get()&&World->WorldType==EWorldType::Editor&&!World->bIsTearingDown;
    }
    static bool Scalar(FProperty* P)
    {return P&&(P->GetClass()==FBoolProperty::StaticClass()||P->GetClass()==FIntProperty::StaticClass()||P->GetClass()==FInt64Property::StaticClass()||P->GetClass()==FFloatProperty::StaticClass()||P->GetClass()==FDoubleProperty::StaticClass()||P->GetClass()==FStrProperty::StaticClass()||P->GetClass()==FNameProperty::StaticClass());}
    static FValue Read(FProperty* P,const void* Address)
    {
        FValue V;if(auto* S=CastField<FStrProperty>(P))V.String=S->GetPropertyValue(Address);else if(auto* N=CastField<FNameProperty>(P)){V.Name=N->GetPropertyValue(Address);V.String=V.Name.ToString();}else if(auto* B=CastField<FBoolProperty>(P))V.Bit=B->GetPropertyValue(Address);else if(P->GetClass()==FFloatProperty::StaticClass())FMemory::Memcpy(&V.Bits,Address,sizeof(float));else if(P->GetClass()==FDoubleProperty::StaticClass())FMemory::Memcpy(&V.Bits,Address,sizeof(double));else V.Integer=CastFieldChecked<FNumericProperty>(P)->GetSignedIntPropertyValue(Address);return V;
    }
    static bool Same(const FValue& A,const FValue& B)
    {if(A.Shape!=B.Shape||A.Children.Num()!=B.Children.Num()||A.Integer!=B.Integer||A.Bits!=B.Bits||A.Bit!=B.Bit||A.Name!=B.Name||!A.String.Equals(B.String,ESearchCase::CaseSensitive))return false;for(int32 I=0;I<A.Children.Num();++I)if(!Same(A.Children[I],B.Children[I]))return false;return true;}
    static FString ValueText(const FValue& V)
    {FString S=TEXT("shape=")+LexToString(V.Shape)+TEXT(";int=")+LexToString(V.Integer)+TEXT(";bits=")+LexToString(V.Bits)+TEXT(";bool=")+LexToString(V.Bit)+TEXT(";string=")+V.String+TEXT(";name=")+V.Name.ToString();for(int32 I=0;I<V.Children.Num()&&S.Len()<131072;++I)S+=TEXT(";[")+LexToString(I)+TEXT("]{")+ValueText(V.Children[I])+TEXT("}");return S;}
    // RM: canonicalize copied slots only. Never sort, deduplicate or rehash authored storage here.
    static bool ValueLess(const FValue& A,const FValue& B)
    {
        if(A.Shape!=B.Shape)return A.Shape<B.Shape;if(A.Integer!=B.Integer)return A.Integer<B.Integer;if(A.Bits!=B.Bits)return A.Bits<B.Bits;if(A.Bit!=B.Bit)return A.Bit<B.Bit;
        const int32 TextOrder=A.String.Compare(B.String,ESearchCase::CaseSensitive);if(TextOrder)return TextOrder<0;const int32 NameOrder=A.Name.Compare(B.Name);if(NameOrder)return NameOrder<0;
        for(int32 I=0;I<FMath::Min(A.Children.Num(),B.Children.Num());++I){if(ValueLess(A.Children[I],B.Children[I]))return true;if(ValueLess(B.Children[I],A.Children[I]))return false;}return A.Children.Num()<B.Children.Num();
    }
    static bool HasHashValue(const FValue& V)
    {if(V.Shape==3||V.Shape==4)return true;for(const auto& C:V.Children)if(HasHashValue(C))return true;return false;}
    static bool HashScalar(FProperty* P)
    {return Scalar(P)&&P->HasAllPropertyFlags(CPF_HasGetValueTypeHash);}
    static bool SetLayoutSame(const FScriptSetLayout& A,const FScriptSetLayout& B)
    {
#if UE_USE_COMPACT_SET_AS_DEFAULT
        return A.Size==B.Size&&A.Alignment==B.Alignment;
#else
        return A.Size==B.Size&&A.HashNextIdOffset==B.HashNextIdOffset&&A.HashIndexOffset==B.HashIndexOffset&&A.SparseArrayLayout.Size==B.SparseArrayLayout.Size&&A.SparseArrayLayout.Alignment==B.SparseArrayLayout.Alignment;
#endif
    }
    static bool MapLayoutSame(const FScriptMapLayout& A,const FScriptMapLayout& B)
    {return A.ValueOffset==B.ValueOffset&&SetLayoutSame(A.SetLayout,B.SetLayout);}
    // Source snapshots retain their actual stock JSON. Only the comparison copy substitutes
    // hash-containing authored roots whose exact typed values were already checked by Current.
    TSharedRef<FJsonObject> SourceComparable(TSharedRef<FJsonObject> Snapshot)
    {
        auto Copy=MakeShared<FJsonObject>(*Snapshot);
        for(const TCHAR* Which:{TEXT("generated_default"),TEXT("skeleton_default")})
        {
            const auto Row=Snapshot->TryGetField(Which);if(!Row.IsValid()||Row->Type!=EJson::Object)continue;
            auto OldRow=Row->AsObject();const TSharedPtr<FJsonObject>* OldValues=nullptr;if(!OldRow->TryGetObjectField(TEXT("values"),OldValues)||!OldValues||!OldValues->IsValid())continue;
            const bool Generated=FCString::Strcmp(Which,TEXT("generated_default"))==0;const auto& Typed=Generated?DefaultValues:SkeletonDefaultValues;const auto& FieldRoles=Generated?Fields:SkeletonFields;
            auto NewRow=MakeShared<FJsonObject>(*OldRow),NewValues=MakeShared<FJsonObject>(**OldValues);
            for(auto& Pair:NewValues->Values)
            {
                const FName Name(*FString(Pair.Key.ToView()));for(int32 I=0;I<FieldRoles.Num()&&I<Typed.Num();++I)if(!FieldRoles[I].Frame&&FieldRoles[I].Name==Name&&HasHashValue(Typed[I])){Pair.Value=MakeShared<FJsonValueString>(ValueText(Typed[I]));break;}
            }
            NewRow->SetObjectField(TEXT("values"),NewValues);Copy->SetObjectField(Which,NewRow);
        }return Copy;
    }

    // RI extends this concrete generated asset lease; no second converter or writer is introduced.
    struct FPayloadField
    {
        FProperty* P=nullptr;FProperty* Parent=nullptr;UStruct* Owner=nullptr;FFieldClass* Kind=nullptr;
        FName Name;FGuid Guid;FString Authored;uint64 Flags=0;int32 Size=0,Offset=0,Dim=0,Align=0;
        FProperty* Inner=nullptr;FProperty* Key=nullptr;FProperty* Value=nullptr;FScriptSetLayout SetLayout{};FScriptMapLayout MapLayout{};EMapPropertyFlags MapFlags=EMapPropertyFlags::None;EArrayPropertyFlags ArrayFlags=EArrayPropertyFlags::None;UUserDefinedStruct* Struct=nullptr;
    };
    struct FPayloadStruct
    {
        TStrongObjectPtr<UUserDefinedStruct> Type;TStrongObjectPtr<UPackage> Package;TStrongObjectPtr<UObject> Data;
        UClass* DataClass=nullptr;EObjectFlags Flags=RF_NoFlags,DataFlags=RF_NoFlags;EStructFlags StructFlags=STRUCT_NoFlags;
        FGuid Guid;FString Path,DataPath;int32 Size=0,Align=0;uint32 PackageFlags=0;const uint8* Memory=nullptr;
        TArray<FProperty*> Members;FValue Defaults;TSharedRef<FJsonObject> FileBefore=MakeShared<FJsonObject>();bool Ready=false;
    };
    TArray<FPayloadField> PayloadFields;TArray<TUniquePtr<FPayloadStruct>> PayloadStructs;bool PayloadDefaultsReady=false,CompositePayload=false,HashedPayload=false;
    FPayloadStruct* PayloadType(UUserDefinedStruct* S)
    {for(const auto& E:PayloadStructs)if(E->Type.Get()==S)return E.Get();return nullptr;}
    bool BindPayload(FProperty* P,FProperty* Parent=nullptr,int32 Depth=0)
    {
        if(PayloadFields.ContainsByPredicate([&](const FPayloadField& E){return E.P==P;}))return true;
        if(!P||Depth>8||PayloadFields.Num()>=256||P->ArrayDim!=1||P->HasSetterOrGetter()||P->GetSize()<=0||P->GetMinAlignment()<=0
            ||P->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated|CPF_EditorOnly|CPF_InstancedReference|CPF_ContainsInstancedReference|CPF_Config|CPF_GlobalConfig))
            return Reject(TEXT("data_asset_generated_payload_layout_pending"),TEXT("Bounded scalar/primitive-array/user-struct fields without accessors, instanced or transient/config roles required; wider original fields remain incomplete."));
        UStruct* Owner=P->GetOwnerStruct();if(!Loaded(Owner)||P->GetOwner<FProperty>()!=Parent||P->GetOwnerProperty()!=(Parent?Parent->GetOwnerProperty():P))
            return Reject(TEXT("data_asset_generated_payload_owner_pending"),TEXT("Actual direct reflected member or Array.Inner owner required."));
        FPayloadField E;E.P=P;E.Parent=Parent;E.Owner=Owner;E.Kind=P->GetClass();E.Name=P->GetFName();E.Flags=uint64(P->GetPropertyFlags());E.Size=P->GetSize();E.Offset=P->GetOffset_ForInternal();E.Dim=P->ArrayDim;E.Align=P->GetMinAlignment();
        if(!Parent&&(E.Offset<0||int64(E.Offset)+E.Size>Owner->GetPropertiesSize()))return Reject(TEXT("data_asset_generated_payload_member_bounds"),TEXT("Member storage must fit its actual owning struct/class."));
        if(!Parent&&Owner->GetClass()==UUserDefinedStruct::StaticClass())
        {
            auto* U=static_cast<UUserDefinedStruct*>(Owner);E.Guid=U->FindPropertyGuidFromName(E.Name);E.Authored=U->GetAuthoredNameForField(P);
            if(!Context()||!E.Guid.IsValid()||E.Authored.IsEmpty())return Reject(TEXT("data_asset_generated_payload_authored_mapping"),TEXT("Compiled UDS members require actual authored names and property GUIDs."));
        }
        if(Scalar(P)){PayloadFields.Add(E);return true;}
        if(P->GetClass()==FArrayProperty::StaticClass())
        {
            auto* A=static_cast<FArrayProperty*>(P);E.Inner=A->Inner;E.ArrayFlags=A->ArrayFlags;
            if(!E.Inner||!Scalar(E.Inner)||E.ArrayFlags!=EArrayPropertyFlags::None||P->GetSize()!=sizeof(FScriptArray)||P->GetMinAlignment()!=alignof(FScriptArray))return Reject(TEXT("data_asset_generated_payload_array_pending"),TEXT("Actual default-allocator array with scalar7 primitive Inner required; struct arrays, nested containers, object/enum/Map/Set remain incomplete."));
            PayloadFields.Add(E);CompositePayload=true;return BindPayload(E.Inner,P,Depth+1);
        }
        if(P->GetClass()==FSetProperty::StaticClass())
        {
            auto* S=static_cast<FSetProperty*>(P);E.Inner=S->ElementProp;E.SetLayout=S->SetLayout;
            if(!E.Inner||!HashScalar(E.Inner)||P->GetSize()!=sizeof(FScriptSet)||P->GetMinAlignment()!=alignof(FScriptSet)||!SetLayoutSame(E.SetLayout,FScriptSet::GetScriptLayout(E.Inner->GetSize(),E.Inner->GetMinAlignment())))return Reject(TEXT("data_asset_generated_payload_set_pending"),TEXT("Actual default-allocator Set with hashable native scalar7 ElementProp and SDK-linked layout required; struct/object/enum elements remain incomplete."));
            PayloadFields.Add(E);CompositePayload=HashedPayload=true;return BindPayload(E.Inner,P,Depth+1);
        }
        if(P->GetClass()==FMapProperty::StaticClass())
        {
            auto* M=static_cast<FMapProperty*>(P);E.Key=M->KeyProp;E.Value=M->ValueProp;E.MapLayout=M->MapLayout;E.MapFlags=M->MapFlags;
            if(!E.Key||!E.Value||!HashScalar(E.Key)||E.MapFlags!=EMapPropertyFlags::None||P->GetSize()!=sizeof(FScriptMap)||P->GetMinAlignment()!=alignof(FScriptMap)||!(Scalar(E.Value)||E.Value->GetClass()==FArrayProperty::StaticClass()||E.Value->GetClass()==FStructProperty::StaticClass())||!MapLayoutSame(E.MapLayout,FScriptMap::GetScriptLayout(E.Key->GetSize(),E.Key->GetMinAlignment(),E.Value->GetSize(),E.Value->GetMinAlignment())))return Reject(TEXT("data_asset_generated_payload_map_pending"),TEXT("Actual default-allocator Map with hashable scalar7 key and existing finite scalar/array/UDS value payload required; other key/value operators and allocators remain incomplete."));
            PayloadFields.Add(E);CompositePayload=HashedPayload=true;return BindPayload(E.Key,P,Depth+1)&&BindPayload(E.Value,P,Depth+1);
        }
        if(P->GetClass()!=FStructProperty::StaticClass())return Reject(TEXT("data_asset_generated_payload_kind_pending"),TEXT("Reflected payload admits scalar7, their arrays/Sets, scalar-key Maps and compiled user-defined structs; other original types remain incomplete."));
        auto* F=static_cast<FStructProperty*>(P);if(!F->Struct.IsResolved())return Reject(TEXT("data_asset_generated_payload_struct_unresolved"),TEXT("Existing resolved UDS source required; no source loader or compiler."));
        UScriptStruct* S=F->Struct.Get();TStrongObjectPtr<UScriptStruct> Immediate(S);
        if(!Context()||!Loaded(S)||S->GetClass()!=UUserDefinedStruct::StaticClass()||P->ElementSize!=S->GetStructureSize())return Reject(TEXT("data_asset_generated_payload_struct_pending"),TEXT("Exact loaded compiled user-defined struct with matching storage required; custom/native struct operators remain incomplete."));
        E.Struct=static_cast<UUserDefinedStruct*>(S);PayloadFields.Add(E);CompositePayload=true;
        if(auto* Existing=PayloadType(E.Struct))return Existing->Ready||Reject(TEXT("data_asset_generated_payload_struct_cycle"),TEXT("Recursive source type is outside the finite payload tree."));
        if(PayloadStructs.Num()>=16)return Reject(TEXT("data_asset_generated_payload_source_bound"),TEXT("At most16 distinct nested UDS sources required."));
        auto Entry=MakeUnique<FPayloadStruct>();auto* Role=Entry.Get();Role->Type.Reset(E.Struct);Role->Package.Reset(E.Struct->GetPackage());
        auto* U=E.Struct;auto* K=Role->Package.Get();
        if(!Loaded(K)||U->GetOuter()!=K||!U->GetPathName().StartsWith(TEXT("/Game/"))||!Clean(U->GetPathName())||K->IsDirty()||U->Status!=UDSS_UpToDate||!U->Guid.IsValid()||U->PrimaryStruct.IsStale()||U->PrimaryStruct.Get()||U->GetSuperStruct()||!U->EditorData.IsResolved())
            return Reject(TEXT("data_asset_generated_payload_source_pending"),TEXT("Saved-clean top-level own compiled Game UDS with resolved existing editor data and no parent/primary duplicate required."));
        UObject* Data=U->EditorData.Get();Role->Data.Reset(Data);if(!Loaded(Data)||Data->GetOuter()!=U||Data->GetClass()!=UUserDefinedStructEditorData::StaticClass())return Reject(TEXT("data_asset_generated_payload_editor_data_pending"),TEXT("Held exact native UDS-owned UserDefinedStructEditorData required for the SDK read-only authored-name implementation; custom editor data remains incomplete."));
        Holds.Emplace(Data->GetClass());Role->DataClass=Data->GetClass();Role->DataFlags=Data->GetFlags();Role->DataPath=Data->GetPathName();Role->Flags=U->GetFlags();Role->StructFlags=U->StructFlags;Role->Guid=U->Guid;Role->Size=U->GetStructureSize();Role->Align=U->GetMinAlignment();Role->Path=U->GetPathName();Role->PackageFlags=uint32(K->GetPackageFlags());
        Role->Memory=U->GetDefaultInstance();if(!Context()||!Role->Memory||Role->Size<=0||Role->Size>65536||Role->Align<=0)return Reject(TEXT("data_asset_generated_payload_default_pending"),TEXT("Existing bounded compiled UDS default instance required; no default allocation/recreation."));
        for(TFieldIterator<FProperty> It(U,EFieldIterationFlags::None);It;++It){if(Role->Members.Num()>=64)return Reject(TEXT("data_asset_generated_payload_members_bound"),TEXT("At most64 own fields per UDS source."));Role->Members.Add(*It);}
        if(Role->Members.IsEmpty())return Reject(TEXT("data_asset_generated_payload_empty_struct_pending"),TEXT("Nonempty compiled reflected UDS source required."));
        PayloadStructs.Add(MoveTemp(Entry));TSet<FGuid> Guids;TSet<FString> AuthoredNames;
        for(FProperty* Member:Role->Members)
        {
            if(Member->GetOwnerStruct()!=U||!BindPayload(Member,nullptr,Depth+1))return false;
            const auto* Captured=PayloadFields.FindByPredicate([&](const FPayloadField& Z){return Z.P==Member;});
            if(!Captured||Guids.Contains(Captured->Guid)||AuthoredNames.Contains(Captured->Authored))return Reject(TEXT("data_asset_generated_payload_duplicate_authored_mapping"),TEXT("Unique actual UDS member names/GUIDs required."));Guids.Add(Captured->Guid);AuthoredNames.Add(Captured->Authored);
        }
        if(!Disk(K->GetName(),Role->FileBefore)||!Context()||!Role->FileBefore->GetBoolField(TEXT("exists"))||Role->FileBefore->GetNumberField(TEXT("bytes"))<=0)return Reject(TEXT("data_asset_generated_payload_source_file_pending"),TEXT("Existing saved source UDS file required."));
        Role->Ready=true;return true;
    }
    bool PayloadCurrent()
    {
        // Validate owning objects and fresh member inventories before any retained field is read.
        for(const auto& Entry:PayloadStructs)
        {
            const auto& E=*Entry;auto* U=E.Type.Get();auto* K=E.Package.Get();auto* D=E.Data.Get();
            if(!Loaded(U)||U->GetClass()!=UUserDefinedStruct::StaticClass()||!Loaded(K)||U->GetOuter()!=K||U->GetFlags()!=E.Flags||!U->GetPathName().Equals(E.Path,ESearchCase::CaseSensitive)||K->IsDirty()||uint32(K->GetPackageFlags())!=E.PackageFlags||U->Status!=UDSS_UpToDate||U->Guid!=E.Guid||U->PrimaryStruct.IsStale()||U->PrimaryStruct.Get()||U->GetSuperStruct()||U->StructFlags!=E.StructFlags||U->GetStructureSize()!=E.Size||U->GetMinAlignment()!=E.Align||!U->EditorData.IsResolved()||U->EditorData.Get()!=D)
                return Failure(TEXT("payload_source_role"),E.Path,TEXT("source identity/status/layout/slots changed"));
            if(!Loaded(D)||D->GetClass()!=E.DataClass||D->GetOuter()!=U||D->GetFlags()!=E.DataFlags||!D->GetPathName().Equals(E.DataPath,ESearchCase::CaseSensitive))return Failure(TEXT("payload_editor_data"),E.DataPath,TEXT("held editor-data role changed"));
            TArray<FProperty*> Now;for(TFieldIterator<FProperty> It(U,EFieldIterationFlags::None);It;++It){if(Now.Num()>=64)return Failure(TEXT("payload_member_count"),LexToString(E.Members.Num()),TEXT(">64"));Now.Add(*It);}if(Now!=E.Members)return Failure(TEXT("payload_member_inventory"),E.Path,TEXT("direct field membership/order changed; stale fields not read"));
        }
        for(const auto& E:PayloadFields)
        {
            if(E.Parent)
            {
                bool Member=false;if(E.Parent->GetClass()==FArrayProperty::StaticClass())Member=static_cast<FArrayProperty*>(E.Parent)->Inner==E.P;else if(E.Parent->GetClass()==FSetProperty::StaticClass())Member=static_cast<FSetProperty*>(E.Parent)->ElementProp==E.P;else if(E.Parent->GetClass()==FMapProperty::StaticClass()){auto* M=static_cast<FMapProperty*>(E.Parent);Member=M->KeyProp==E.P||M->ValueProp==E.P;}
                if(!Member)return Failure(TEXT("payload_inner_slot"),E.Name.ToString(),TEXT("fresh parent Array.Inner/Set.Element/Map.Key-or-Value changed; old nested field not read"));
            }
            auto* P=E.P;if(P->GetClass()!=E.Kind||P->GetOwnerStruct()!=E.Owner||P->GetOwner<FProperty>()!=E.Parent||P->GetOwnerProperty()!=(E.Parent?E.Parent->GetOwnerProperty():P)||P->GetFName()!=E.Name||P->ArrayDim!=E.Dim||P->GetSize()!=E.Size||P->GetOffset_ForInternal()!=E.Offset||P->GetMinAlignment()!=E.Align||uint64(P->GetPropertyFlags())!=E.Flags||P->HasSetterOrGetter())return Failure(TEXT("payload_field_layout"),E.Name.ToString(),TEXT("direct descriptor/owner/layout/flags changed"));
            if(E.Inner&&E.Kind==FArrayProperty::StaticClass()){auto* A=static_cast<FArrayProperty*>(P);if(A->Inner!=E.Inner||A->ArrayFlags!=E.ArrayFlags)return Failure(TEXT("payload_array_binding"),E.Name.ToString(),TEXT("Inner/allocator flags changed"));}
            if(E.Kind==FSetProperty::StaticClass()){auto* S=static_cast<FSetProperty*>(P);if(S->ElementProp!=E.Inner||!SetLayoutSame(S->SetLayout,E.SetLayout))return Failure(TEXT("payload_set_binding"),E.Name.ToString(),TEXT("ElementProp or SDK-linked layout changed"));}
            if(E.Kind==FMapProperty::StaticClass()){auto* M=static_cast<FMapProperty*>(P);if(M->KeyProp!=E.Key||M->ValueProp!=E.Value||M->MapFlags!=E.MapFlags||!MapLayoutSame(M->MapLayout,E.MapLayout))return Failure(TEXT("payload_map_binding"),E.Name.ToString(),TEXT("Key/Value/allocator/layout changed"));}
            if(E.Struct){auto* S=static_cast<FStructProperty*>(P);if(!S->Struct.IsResolved()||S->Struct.Get()!=E.Struct)return Failure(TEXT("payload_struct_binding"),E.Name.ToString(),TEXT("resolved held struct slot changed"));}
            if(!E.Parent&&E.Owner->GetClass()==UUserDefinedStruct::StaticClass())
            {auto* U=static_cast<UUserDefinedStruct*>(E.Owner);const FGuid Guid=U->FindPropertyGuidFromName(E.Name);const FString Authored=U->GetAuthoredNameForField(P);if(!Context()||Guid!=E.Guid||!Authored.Equals(E.Authored,ESearchCase::CaseSensitive))return Failure(TEXT("payload_authored_mapping"),E.Authored+TEXT(":")+E.Guid.ToString(),Authored+TEXT(":")+Guid.ToString());}
        }
        for(const auto& Entry:PayloadStructs)
        {
            const auto& E=*Entry;const uint8* Memory=E.Type->GetDefaultInstance();if(!Context()||Memory!=E.Memory)return Failure(TEXT("payload_default_instance"),E.Path,TEXT("compiled default storage changed"));
            if(PayloadDefaultsReady){FValue V;int32 Cells=0,Chars=0;if(!ReadStruct(E,Memory,V,Cells,Chars,0)||!Same(V,E.Defaults))return Failure(TEXT("payload_source_default"),ValueText(E.Defaults),ValueText(V),true,E.Path);}
        }
        return true;
    }
    bool ReadStruct(const FPayloadStruct& S,const void* Address,FValue& Out,int32& Cells,int32& Chars,int32 Depth)
    {
        if(!Address||Depth>8)return Failure(TEXT("payload_struct_value_bound"),TEXT("current struct memory/depth<=8"),TEXT("not observed"));Out.Shape=1;
        for(FProperty* P:S.Members){FValue V;if(!ReadPayload(P,P->ContainerPtrToValuePtr<void>(Address),V,Cells,Chars,Depth+1))return false;Out.Children.Add(MoveTemp(V));}return true;
    }
    bool ReadPayload(FProperty* P,const void* Address,FValue& Out,int32& Cells,int32& Chars,int32 Depth)
    {
        if(!Address||Depth>8||++Cells>16384)return Failure(TEXT("payload_value_bound"),TEXT("depth<=8 and <=16384 cells"),TEXT("exceeded"));
        if(Scalar(P)){Out=Read(P,Address);if((P->GetClass()==FStrProperty::StaticClass()||P->GetClass()==FNameProperty::StaticClass())&&(Chars+=Out.String.Len())>65536)return Failure(TEXT("payload_text_bound"),TEXT("<=65536 copied characters"),LexToString(Chars));return true;}
        if(P->GetClass()==FArrayProperty::StaticClass())
        {
            auto* A=static_cast<FArrayProperty*>(P);const auto* Storage=static_cast<const FScriptArray*>(Address);const int32 Num=Storage->Num(),Max=Storage->Max();const void* Data=Storage->GetData();
            if(Num<0||Num>4096||Max<Num||Max>65536||(Max>0&&!Data)||(Data&&UPTRINT(Data)%A->Inner->GetMinAlignment()!=0))return Failure(TEXT("payload_array_storage"),TEXT("bounded current default-allocator storage with aligned data"),LexToString(Num)+TEXT("/")+LexToString(Max));
            FScriptArrayHelper Values(A,Address);Out.Shape=2;for(int32 I=0;I<Num;++I){FValue V;if(!ReadPayload(A->Inner,Values.GetRawPtr(I),V,Cells,Chars,Depth+1))return false;Out.Children.Add(MoveTemp(V));}return true;
        }
        if(P->GetClass()==FSetProperty::StaticClass())
        {
            auto* S=static_cast<FSetProperty*>(P);const auto* Storage=static_cast<const FScriptSet*>(Address);const int32 Num=Storage->NumUnchecked(),Max=Storage->GetMaxIndex();if(Num<0||Num>4096||Max<Num||Max>65536)return Failure(TEXT("payload_set_storage"),TEXT("bounded current Num/max valid-index storage"),LexToString(Num)+TEXT("/")+LexToString(Max));
            FScriptSetHelper Values(S,Address);Out.Shape=3;int32 Observed=0;for(int32 I=0;I<Max;++I)if(Values.IsValidIndex(I)){const void* At=Values.GetElementPtr(I);if(!At||UPTRINT(At)%S->ElementProp->GetMinAlignment()!=0)return Failure(TEXT("payload_set_alignment"),TEXT("current aligned native element storage"),TEXT("unavailable"));FValue V;if(!ReadPayload(S->ElementProp,At,V,Cells,Chars,Depth+1))return false;Out.Children.Add(MoveTemp(V));++Observed;}if(Observed!=Num)return Failure(TEXT("payload_set_membership"),LexToString(Num),LexToString(Observed));Out.Children.Sort([](const FValue& A,const FValue& Z){return ValueLess(A,Z);});return true;
        }
        if(P->GetClass()==FMapProperty::StaticClass())
        {
            auto* M=static_cast<FMapProperty*>(P);const auto* Storage=static_cast<const FScriptMap*>(Address);const int32 Num=Storage->NumUnchecked(),Max=Storage->GetMaxIndex();if(Num<0||Num>4096||Max<Num||Max>65536)return Failure(TEXT("payload_map_storage"),TEXT("bounded current Num/max valid-index storage"),LexToString(Num)+TEXT("/")+LexToString(Max));
            FScriptMapHelper Values(M,Address);Out.Shape=4;int32 Observed=0;for(int32 I=0;I<Max;++I)if(Values.IsValidIndex(I)){const void* Key=Values.GetKeyPtr(I);const void* Value=Values.GetValuePtr(I);if(!Key||!Value||UPTRINT(Key)%M->KeyProp->GetMinAlignment()!=0||UPTRINT(Value)%M->ValueProp->GetMinAlignment()!=0)return Failure(TEXT("payload_map_alignment"),TEXT("current aligned key/value storage"),TEXT("unavailable"));FValue Pair,K,V;Pair.Shape=5;if(!ReadPayload(M->KeyProp,Key,K,Cells,Chars,Depth+1)||!ReadPayload(M->ValueProp,Value,V,Cells,Chars,Depth+1))return false;Pair.Children.Add(MoveTemp(K));Pair.Children.Add(MoveTemp(V));Out.Children.Add(MoveTemp(Pair));++Observed;}if(Observed!=Num)return Failure(TEXT("payload_map_membership"),LexToString(Num),LexToString(Observed));Out.Children.Sort([](const FValue& A,const FValue& Z){return ValueLess(A,Z);});return true;
        }
        auto* S=static_cast<FStructProperty*>(P);auto* Role=PayloadType(static_cast<UUserDefinedStruct*>(S->Struct.Get()));return Role&&ReadStruct(*Role,Address,Out,Cells,Chars,Depth);
    }
    bool ReadCell(FProperty* P,const void* Address,FValue& Out)
    {int32 Cells=0,Chars=0;return ReadPayload(P,Address,Out,Cells,Chars,0);}
    bool CapturePayloadDefaults()
    {
        if(!PayloadCurrent())return false;for(auto& Entry:PayloadStructs){int32 Cells=0,Chars=0;if(!ReadStruct(*Entry,Entry->Memory,Entry->Defaults,Cells,Chars,0))return false;}PayloadDefaultsReady=true;return PayloadCurrent();
    }
    bool PayloadInputSafe(FProperty* P,const TSharedPtr<FJsonValue>& Value,int32 Depth=0)
    {
        if(!Value.IsValid()||Depth>8)return false;if(Value->Type==EJson::Null)return true;
        if(Scalar(P))return !((P->GetClass()==FIntProperty::StaticClass()||P->GetClass()==FInt64Property::StaticClass())&&Value->Type==EJson::Number&&(Value->AsNumber()>=9223372036854775808.0||Value->AsNumber()< -9223372036854775808.0));
        if(P->GetClass()==FArrayProperty::StaticClass())
        {if(Value->Type!=EJson::Array)return true;const auto& Values=Value->AsArray();if(Values.Num()>4096)return false;for(const auto& V:Values)if(!PayloadInputSafe(static_cast<FArrayProperty*>(P)->Inner,V,Depth+1))return false;return true;}
        if(P->GetClass()==FSetProperty::StaticClass())
        {if(Value->Type!=EJson::Array)return true;const auto& Values=Value->AsArray();if(Values.Num()>4096)return false;for(const auto& V:Values)if(!PayloadInputSafe(static_cast<FSetProperty*>(P)->ElementProp,V,Depth+1))return false;return true;}
        if(P->GetClass()==FMapProperty::StaticClass())
        {if(Value->Type!=EJson::Object)return true;const auto Object=Value->AsObject();if(!Object.IsValid()||Object->Values.Num()>4096)return false;auto* M=static_cast<FMapProperty*>(P);for(const auto& Pair:Object->Values){const auto Key=MakeShared<FJsonValueString>(FString(Pair.Key.ToView()));if(!PayloadInputSafe(M->KeyProp,Key,Depth+1)||!PayloadInputSafe(M->ValueProp,Pair.Value,Depth+1))return false;}return true;}
        if(Value->Type!=EJson::Object)return true;const auto* Role=PayloadType(static_cast<UUserDefinedStruct*>(static_cast<FStructProperty*>(P)->Struct.Get()));if(!Role)return false;
        const auto Object=Value->AsObject();if(!Object.IsValid())return false;
        for(FProperty* Member:Role->Members)
        {
            const auto* Field=PayloadFields.FindByPredicate([&](const FPayloadField& E){return E.P==Member;});if(!Field)return false;
            // Match the same UDS authored name used by JsonAttributesToUStructWithContainer.
            auto V=Object->TryGetField(Field->Authored);if(V.IsValid()&&!PayloadInputSafe(Member,V,Depth+1))return false;
        }return true;
    }
    bool PayloadSourceSnapshot(TSharedRef<FJsonObject> Out)
    {
        TArray<TSharedPtr<FJsonValue>> Sources;for(const auto& Entry:PayloadStructs)
        {
            if(!Current())return false;const auto& E=*Entry;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),E.Path);J->SetStringField(TEXT("guid"),E.Guid.ToString());J->SetNumberField(TEXT("size"),E.Size);J->SetNumberField(TEXT("alignment"),E.Align);J->SetStringField(TEXT("default_values"),ValueText(E.Defaults));J->SetStringField(TEXT("editor_data"),E.DataPath);
            auto File=MakeShared<FJsonObject>();if(!Disk(E.Package->GetName(),File)||!Current()||!Equal(E.FileBefore,File))return Failure(TEXT("payload_source_file"),Text(E.FileBefore),Text(File),true,E.Path);J->SetObjectField(TEXT("file"),File);Sources.Add(MakeShared<FJsonValueObject>(J));
        }Out->SetArrayField(TEXT("payload_struct_sources"),Sources);return Current();
    }

    static bool SamePin(const FEdGraphPinType& A,const FEdGraphPinType& B)
    {return A==B&&A.bIsUObjectWrapper==B.bIsUObjectWrapper&&A.bSerializeAsSinglePrecisionFloat==B.bSerializeAsSinglePrecisionFloat&&A.PinValueType.bTerminalIsUObjectWrapper==B.PinValueType.bTerminalIsUObjectWrapper;}
    bool Layout(UBlueprintGeneratedClass* C,TArray<FField>& Out)
    {
        const FStructProperty* Public=C->UberGraphFramePointerProperty;auto Fn=C->UberGraphFunction;if(!Fn.IsResolved())return Reject(TEXT("data_asset_generated_compiler_slot_unresolved"),TEXT("Compiler function slot must already be resolved."));if(Fn.Get())Holds.Emplace(Fn.Get());
        int32 Authored=0,Frames=0;for(TFieldIterator<FProperty> It(C,EFieldIterationFlags::None);It;++It)
        {
            FProperty* P=*It;if(Out.Num()>=33||P->GetOwnerStruct()!=C||P->GetOwnerProperty()!=P||P->ArrayDim!=1||P->HasSetterOrGetter()||P->GetSize()<=0||P->GetOffset_ForInternal()<0||int64(P->GetOffset_ForInternal())+P->GetSize()>C->GetPropertiesSize())return Reject(TEXT("data_asset_generated_layout_pending"),TEXT("Bounded direct own field membership/layout required."));
            FField E;E.P=P;E.Kind=P->GetClass();E.Name=P->GetFName();E.Owner=C;E.Flags=uint64(P->GetPropertyFlags());E.Size=P->GetSize();E.Offset=P->GetOffset_ForInternal();E.Dim=P->ArrayDim;
            const bool Declared=Declarations.ContainsByPredicate([&](const FBPVariableDescription& D){return D.VarName==E.Name;});
            if(!Declared&&P->GetFName()==UBlueprintGeneratedClass::GetUberGraphFrameName())
            {
                auto* S=CastField<FStructProperty>(P);if(++Frames>1||!S||!S->Struct.IsResolved()||!Loaded(S->Struct.Get())||!S->Struct->GetPathName().Equals(TEXT("/Script/Engine.PointerToUberGraphFrame"),ESearchCase::CaseSensitive)||!P->HasAllPropertyFlags(CPF_Transient|CPF_DuplicateTransient)||P->HasAnyPropertyFlags(CPF_BlueprintVisible|CPF_Parm)||!(Public==P||(!Public&&!Fn.Get())))return Reject(TEXT("data_asset_generated_compiler_frame_pending"),TEXT("Only actual exact compiler frame with bound pointer or null function/null public pointer is separate from authored fields."));
                UScriptStruct* Native=FindObject<UScriptStruct>(nullptr,TEXT("/Script/Engine.PointerToUberGraphFrame"));TStrongObjectPtr<UScriptStruct> Immediate(Native);
                if(!Loaded(Native)||Native!=S->Struct.Get()||Native->GetClass()!=UScriptStruct::StaticClass()||(Native->StructFlags&STRUCT_Native)==0||Native->GetOuter()!=Native->GetPackage()||!Native->GetPackage()->GetName().Equals(TEXT("/Script/Engine"),ESearchCase::CaseSensitive)||Native->GetStructureSize()!=sizeof(FPointerToUberGraphFrame)||Native->GetMinAlignment()!=alignof(FPointerToUberGraphFrame))return Reject(TEXT("data_asset_generated_frame_binding_pending"),TEXT("Exact loaded native compiler frame identity/size/alignment required; no frame default/runtime value read."));
                E.Frame=true;E.Struct=S->Struct.Get();Holds.Emplace(E.Struct);if(FrameStruct.IsValid()&&FrameStruct.Get()!=E.Struct)return Reject(TEXT("data_asset_generated_frame_source_changed"),TEXT("Same exact native compiler frame struct required."));FrameStruct.Reset(E.Struct);
            }
            else {if(!Declared||++Authored>32)return Reject(TEXT("data_asset_generated_declared_payload_scope_pending"),TEXT("At most32 directly declared own source fields required; inherited and wider generated hierarchies remain incomplete."));if(!BindPayload(P))return false;}
            Out.Add(E);
        }
        if(Authored!=Declarations.Num())return Reject(TEXT("data_asset_generated_declared_layout_mismatch"),TEXT("Every own variable must have one directly owned authored compiled field; physical compiler frame is counted separately."));return true;
    }
    bool LayoutCurrent(UBlueprintGeneratedClass* C,const TArray<FField>& Rows)
    {
        TArray<FProperty*> Now;for(TFieldIterator<FProperty> It(C,EFieldIterationFlags::None);It;++It){if(Now.Num()>=33)return Failure(TEXT("field_count"),LexToString(Rows.Num()),TEXT(">33"));Now.Add(*It);}if(Now.Num()!=Rows.Num())return Failure(TEXT("field_count"),LexToString(Rows.Num()),LexToString(Now.Num()),true,C->GetPathName());
        for(int32 I=0;I<Now.Num();++I){const auto& E=Rows[I];if(Now[I]!=E.P)return Failure(TEXT("field_membership"),LexToString(UPTRINT(E.P)),LexToString(UPTRINT(Now[I])));auto* P=E.P;if(P->GetClass()!=E.Kind||P->GetFName()!=E.Name||P->GetOwnerStruct()!=C||P->GetOwnerProperty()!=P||P->ArrayDim!=E.Dim||uint64(P->GetPropertyFlags())!=E.Flags||P->GetSize()!=E.Size||P->GetOffset_ForInternal()!=E.Offset||P->HasSetterOrGetter())return Failure(TEXT("field_layout"),E.Name.ToString(),TEXT("changed"),true,C->GetPathName());if(E.Frame){auto* S=CastField<FStructProperty>(P);if(!S||!S->Struct.IsResolved()||S->Struct.Get()!=E.Struct||!Loaded(E.Struct)||E.Struct!=FrameStruct.Get()||E.Struct->GetClass()!=UScriptStruct::StaticClass()||(E.Struct->StructFlags&STRUCT_Native)==0||!E.Struct->GetPathName().Equals(TEXT("/Script/Engine.PointerToUberGraphFrame"),ESearchCase::CaseSensitive)||E.Struct->GetStructureSize()!=sizeof(FPointerToUberGraphFrame)||E.Struct->GetMinAlignment()!=alignof(FPointerToUberGraphFrame))return Failure(TEXT("frame_binding"),TEXT("held native frame"),TEXT("changed"));}}return true;
    }
    bool DefaultsCurrent(UObject* D,const TArray<FField>& Rows,const TArray<FValue>& Values)
    {
        if(!D)return Values.IsEmpty();if(Values.Num()!=Rows.Num())return false;for(int32 I=0;I<Rows.Num();++I)if(!Rows[I].Frame){FValue Actual;if(!ReadCell(Rows[I].P,Rows[I].P->ContainerPtrToValuePtr<void>(D),Actual))return false;if(!Same(Actual,Values[I]))return Failure(TEXT("source_default"),ValueText(Values[I]),ValueText(Actual),true,D->GetPathName()+TEXT(".")+Rows[I].Name.ToString());}return true;
    }
    bool NativeBaseCurrent()
    {
        auto* Base=NativeBase.Get();
        if(!Loaded(Base)||(Base!=UDataAsset::StaticClass()&&Base!=UPrimaryDataAsset::StaticClass())||Base->GetClass()!=UClass::StaticClass()||Base->GetClassFlags()!=NativeBaseFlags||Base->GetPropertiesSize()!=NativeBaseSize||Base->GetMinAlignment()!=NativeBaseAlign||!Base->HasAnyClassFlags(CLASS_Native)||Base->HasAnyClassFlags(CLASS_NewerVersionExists)||!Base->ClassGeneratedBy.IsResolved()||Base->ClassGeneratedBy.Get()!=nullptr)
            return Failure(TEXT("native_base_role"),TEXT("held direct Engine DataAsset or PrimaryDataAsset class/layout"),TEXT("changed"));
        return true;
    }
    bool HasAssetBundles()const{return NativeBase.Get()==UPrimaryDataAsset::StaticClass();}
    bool ClassRole(UBlueprintGeneratedClass* C,UObject* D,EClassFlags Flags,const FString& Suffix)
    {
        if(!NativeBaseCurrent())return false;
        if(!Loaded(C)||C->GetClass()!=UBlueprintGeneratedClass::StaticClass()||C->GetOuter()!=SourcePackage.Get()||C->GetSuperClass()!=NativeBase.Get()||C->GetClassFlags()!=Flags||C->GetPropertiesSize()!=(C==Class.Get()?ClassSize:SkeletonSize)||C->GetMinAlignment()!=(C==Class.Get()?ClassAlign:SkeletonAlign)||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy.Get()!=Blueprint.Get()||!C->GetName().Equals(Suffix,ESearchCase::CaseSensitive)||!C->ClassDefaultObject.IsResolved()||C->ClassDefaultObject.Get()!=D)return Failure(TEXT("compiled_class_role"),Suffix,TEXT("class/parent/source/flags/CDO slot changed"));
        if(D&&(!Loaded(D)||D->GetFlags()!=(C==Class.Get()?DefaultFlags:SkeletonDefaultFlags)||D->GetClass()!=C||D->GetOuter()!=SourcePackage.Get()||!D->HasAnyFlags(RF_ClassDefaultObject)||D->GetFName()!=FName(*(TEXT("Default__")+C->GetName()))))return Failure(TEXT("source_cdo_role"),TEXT("own existing exact default object"),TEXT("changed"));return true;
    }
    bool BundlesEmpty(UObject* O)
    {return !O||(!HasAssetBundles()&&!BundleProperty)||(HasAssetBundles()&&BundleProperty&&BundleProperty->ContainerPtrToValuePtr<FAssetBundleData>(O)->Bundles.IsEmpty());}
    bool Current()
    {
        if(!Context())return Failure(TEXT("editor_context"),TEXT("same idle Editor/world"),TEXT("changed"));if(!Bound)return true;if(!NativeBaseCurrent())return false;
        auto* B=Blueprint.Get();if(!Loaded(SourcePackage.Get())||!Loaded(B)||B->GetClass()!=UBlueprint::StaticClass()||B->GetOuter()!=SourcePackage.Get()||B->GetFlags()!=BlueprintFlags||B->Status!=BS_UpToDate||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||B->bSuppressStructurallyModified||B->BlueprintType!=BPTYPE_Normal||!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.GetGCPtr().Get()!=NativeBase.Get()||!B->GeneratedClass.GetGCPtr().IsResolved()||B->GeneratedClass.GetGCPtr().Get()!=Class.Get()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||B->SkeletonGeneratedClass.GetGCPtr().Get()!=Skeleton.Get()||SourcePackage->IsDirty()||SourcePackage->GetPackageFlags()!=SourcePackageFlags)return Failure(TEXT("source_blueprint_role"),TEXT("original saved-clean source binding/status/flags"),TEXT("changed"));
        if(!ClassRole(Class.Get(),Default.Get(),ClassFlags,Blueprint->GetName()+TEXT("_C"))||!ClassRole(Skeleton.Get(),SkeletonDefault.Get(),SkeletonFlags,TEXT("SKEL_")+Blueprint->GetName()+TEXT("_C"))||!LayoutCurrent(Class.Get(),Fields)||!LayoutCurrent(Skeleton.Get(),SkeletonFields))return false;
        if(!Class->UberGraphFunction.IsResolved()||Class->UberGraphFunction.Get()!=GenUber.Get()||Class->UberGraphFramePointerProperty!=GenFrame||!Skeleton->UberGraphFunction.IsResolved()||Skeleton->UberGraphFunction.Get()!=SkelUber.Get()||Skeleton->UberGraphFramePointerProperty!=SkelFrame)return Failure(TEXT("compiler_roles"),TEXT("initial nullable UberGraphFunction/public frame slots"),TEXT("changed"));
        if(!B->SimpleConstructionScript.IsResolved()||B->SimpleConstructionScript.Get()!=nullptr||!B->ImplementedInterfaces.IsEmpty()||!B->FunctionGraphs.IsEmpty()||!B->MacroGraphs.IsEmpty()||!B->DelegateSignatureGraphs.IsEmpty()||B->UbergraphPages!=Uber||B->EventGraphs!=EventStubs||!Class->DynamicBindingObjects.IsEmpty()||!Class->ComponentTemplates.IsEmpty()||!Class->Timelines.IsEmpty()||!Skeleton->DynamicBindingObjects.IsEmpty()||!Skeleton->ComponentTemplates.IsEmpty()||!Skeleton->Timelines.IsEmpty()||B->NewVariables.Num()!=Declarations.Num())return Failure(TEXT("source_authored_inventory"),TEXT("original data-only Blueprint"),TEXT("changed"));
        for(int32 I=0;I<Declarations.Num();++I){const auto& A=Declarations[I];const auto& Z=B->NewVariables[I];if(A.VarName!=Z.VarName||A.VarGuid!=Z.VarGuid||!SamePin(A.VarType,Z.VarType)||!A.DefaultValue.Equals(Z.DefaultValue,ESearchCase::CaseSensitive)||A.PropertyFlags!=Z.PropertyFlags||!A.FriendlyName.Equals(Z.FriendlyName,ESearchCase::CaseSensitive)||A.RepNotifyFunc!=Z.RepNotifyFunc||A.ReplicationCondition!=Z.ReplicationCondition||A.MetaDataArray.Num()!=Z.MetaDataArray.Num()||FTextInspector::GetSharedDataId(A.Category)!=FTextInspector::GetSharedDataId(Z.Category)||FTextInspector::GetFlags(A.Category)!=FTextInspector::GetFlags(Z.Category))return Failure(TEXT("source_declaration"),A.VarName.ToString(),TEXT("changed"));for(int32 K=0;K<A.MetaDataArray.Num();++K)if(A.MetaDataArray[K].DataKey!=Z.MetaDataArray[K].DataKey||!A.MetaDataArray[K].DataValue.Equals(Z.MetaDataArray[K].DataValue,ESearchCase::CaseSensitive))return Failure(TEXT("source_metadata"),A.VarName.ToString(),TEXT("changed"));}
        for(const auto& G:Graphs){if(!Loaded(G.G.Get())||G.G->GetClass()!=UEdGraph::StaticClass()||G.G->GetOuter()!=B||G.G->GraphGuid!=G.Id||!G.G->GetPathName().Equals(G.Path,ESearchCase::CaseSensitive)||!G.G->Schema.GetGCPtr().IsResolved()||G.G->Schema.GetGCPtr().Get()!=UEdGraphSchema_K2::StaticClass()||!G.G->Nodes.IsEmpty()||!G.G->SubGraphs.IsEmpty())return Failure(TEXT("source_graph"),G.Path,TEXT("changed"));}
        if(HasAssetBundles())
        {
            if(FindFProperty<FStructProperty>(UPrimaryDataAsset::StaticClass(),TEXT("AssetBundleData"))!=BundleProperty||!BundleProperty||BundleProperty->GetOwnerStruct()!=UPrimaryDataAsset::StaticClass()||!BundleProperty->Struct.IsResolved()||BundleProperty->Struct.Get()!=BundleStruct.Get()||!Loaded(BundleStruct.Get())||BundleStruct->GetClass()!=UScriptStruct::StaticClass()||(BundleStruct->StructFlags&STRUCT_Native)==0||BundleStruct->GetStructureSize()!=sizeof(FAssetBundleData)||BundleStruct->GetMinAlignment()!=alignof(FAssetBundleData)||BundleProperty->ArrayDim!=1||BundleProperty->GetSize()!=sizeof(FAssetBundleData)||!BundlesEmpty(Default.Get())||!BundlesEmpty(SkeletonDefault.Get()))return Failure(TEXT("source_asset_bundle"),TEXT("empty native source default bundle"),TEXT("changed"));
        }
        else if(BundleProperty||BundleStruct.IsValid()||FindFProperty<FStructProperty>(UDataAsset::StaticClass(),TEXT("AssetBundleData")))return Failure(TEXT("source_asset_bundle_role"),TEXT("plain UDataAsset base has no inherited bundle property"),TEXT("changed"));
        if(!PayloadCurrent()||!DefaultsCurrent(Default.Get(),Fields,DefaultValues)||!DefaultsCurrent(SkeletonDefault.Get(),SkeletonFields,SkeletonDefaultValues))return false;
        if(FactoryDefault.IsValid())
        {
            TArray<FProperty*> Actual;for(TFieldIterator<FProperty> It(UDataAssetFactory::StaticClass(),EFieldIterationFlags::IncludeSuper);It;++It){if(Actual.Num()>=128)return Failure(TEXT("factory_field_count"),TEXT("<=128"),TEXT("exceeded"));Actual.Add(*It);}if(Actual.Num()!=FactoryFields.Num())return Failure(TEXT("factory_field_inventory"),LexToString(FactoryFields.Num()),LexToString(Actual.Num()));
            for(int32 I=0;I<Actual.Num();++I){const auto& E=FactoryFields[I];if(Actual[I]!=E.P)return Failure(TEXT("factory_field_membership"),LexToString(UPTRINT(E.P)),LexToString(UPTRINT(Actual[I])));auto* P=E.P;if(P->GetClass()!=E.Kind||P->GetOwnerStruct()!=E.Owner||P->GetFName()!=E.Name||P->ArrayDim!=E.Dim||P->GetSize()!=E.Size||P->GetOffset_ForInternal()!=E.Offset||uint64(P->GetPropertyFlags())!=E.Flags)return Failure(TEXT("factory_field_layout"),E.Name.ToString(),TEXT("changed"));}
        }
        if(FactoryDefault.IsValid()){auto* FC=UDataAssetFactory::StaticClass();auto Value=FactoryDefault->DataAssetClass;if(!Loaded(FactoryDefault.Get())||FactoryDefault->GetClass()!=FC||FactoryDefault->GetOuter()!=FC->GetOuter()||!FC->ClassDefaultObject.IsResolved()||FC->ClassDefaultObject.Get()!=FactoryDefault.Get()||!Value.GetGCPtr().IsResolved()||Value.GetGCPtr().Get()!=FactoryDefaultClass)return Failure(TEXT("factory_cdo"),TEXT("held native factory CDO/class slot"),TEXT("changed"));}
        if(Factory.IsValid()){auto V=Factory->DataAssetClass;if(!Loaded(Factory.Get())||Factory->GetClass()!=UDataAssetFactory::StaticClass()||Factory->GetOuter()!=GetTransientPackage()||!V.GetGCPtr().IsResolved()||V.GetGCPtr().Get()!=Class.Get())return Failure(TEXT("factory_instance"),TEXT("held native factory with requested class"),TEXT("changed"));}
        if(TargetBound)
        {
            const EObjectFlags Planned=SavedExports?EObjectFlags(TargetFlags|RF_WasLoaded|RF_LoadCompleted):TargetFlags;
            if(!Loaded(TargetPackage.Get())||!Loaded(Target.Get())||Target->GetClass()!=Class.Get()||Target->GetOuter()!=TargetPackage.Get()||!Target->GetPathName().Equals(PathName,ESearchCase::CaseSensitive)||Target->GetFlags()!=Planned||TargetPackage->HasAnyPackageFlags(PKG_CompiledIn|PKG_PlayInEditor|PKG_EditorOnly))return Failure(TEXT("target_identity"),PathName+TEXT(";flags=")+LexToString(uint64(Planned)),TEXT("role/flags changed"));
            for(int32 I=0;I<Fields.Num();++I)if(!Fields[I].Frame&&I!=Mutable){FValue V;if(!ReadCell(Fields[I].P,Fields[I].P->ContainerPtrToValuePtr<void>(Target.Get()),V))return false;if(!Same(V,Expected[I]))return Failure(TEXT("target_value"),ValueText(Expected[I]),ValueText(V),true,PathName+TEXT(".")+Fields[I].Name.ToString());}
            if(!SaveEntered&&!BundlesEmpty(Target.Get()))return Failure(TEXT("target_initial_bundle"),TEXT("empty inherited derived bundle"),TEXT("changed before save"));
        }return true;
    }
    bool File(const FString& Package,TSharedRef<FJsonObject> Out)
    {if(!Current())return false;const bool Observed=Disk(Package,Out);if(!Current())return false;if(!Observed)return Failure(TEXT("file_observation"),TEXT("bounded complete disk observation"),TEXT("failed"));return true;}
    bool SourceSnapshot(TSharedRef<FJsonObject> Out)
    {
        if(!Current())return false;Out->SetStringField(TEXT("direct_native_base"),NativeBase->GetPathName());Out->SetBoolField(TEXT("inherited_asset_bundle_data_present"),HasAssetBundles());Out->SetStringField(TEXT("blueprint"),Blueprint->GetPathName());Out->SetStringField(TEXT("class"),Class->GetPathName());Out->SetStringField(TEXT("skeleton"),Skeleton->GetPathName());Out->SetStringField(TEXT("class_flags"),LexToString(uint64(ClassFlags)));Out->SetStringField(TEXT("skeleton_flags"),LexToString(uint64(SkeletonFlags)));Out->SetStringField(TEXT("generated_cdo_flags"),LexToString(uint64(DefaultFlags)));Out->SetStringField(TEXT("skeleton_cdo_flags"),LexToString(uint64(SkeletonDefaultFlags)));Out->SetNumberField(TEXT("authored_fields"),Declarations.Num());Out->SetNumberField(TEXT("generated_physical_fields"),Fields.Num());Out->SetNumberField(TEXT("skeleton_physical_fields"),SkeletonFields.Num());Out->SetBoolField(TEXT("compiler_frame_value_read"),false);
        TArray<TSharedPtr<FJsonValue>> FieldRows;for(const auto& E:Fields){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),E.Name.ToString());J->SetStringField(TEXT("property_class"),E.Kind->GetName());J->SetStringField(TEXT("owner"),E.Owner->GetPathName());J->SetStringField(TEXT("flags"),LexToString(E.Flags));J->SetNumberField(TEXT("array_dim"),E.Dim);J->SetNumberField(TEXT("offset"),E.Offset);J->SetNumberField(TEXT("size"),E.Size);J->SetBoolField(TEXT("compiler_frame"),E.Frame);FieldRows.Add(MakeShared<FJsonValueObject>(J));}Out->SetArrayField(TEXT("generated_fields"),FieldRows);
        for(const auto& Pair:TArray<TPair<UObject*,const TCHAR*>>{{Default.Get(),TEXT("generated_default")},{SkeletonDefault.Get(),TEXT("skeleton_default")}})
        {auto J=MakeShared<FJsonObject>();Out->SetObjectField(Pair.Value,J);J->SetBoolField(TEXT("exists"),Pair.Key!=nullptr);if(!Pair.Key)continue;J->SetStringField(TEXT("path"),Pair.Key->GetPathName());TSharedRef<FJsonObject> Values=MakeShared<FJsonObject>();const bool OK=FJsonObjectConverter::UStructToJsonObject(Pair.Key->GetClass(),Pair.Key,Values,0,0);Count(R,TEXT("json_export_calls"));if(!Current())return false;if(!OK)return Failure(TEXT("source_default_export"),TEXT("complete native JSON export"),TEXT("failed"));int32 Slots=0,Chars=0;if(!Bounded(MakeShared<FJsonValueObject>(Values),Slots,Chars))return Failure(TEXT("source_default_json_bounds"),TEXT("finite bounded JSON values"),TEXT("not observed"));J->SetObjectField(TEXT("values"),Values);}
        auto F=MakeShared<FJsonObject>();if(!File(SourcePackage->GetName(),F))return false;Out->SetObjectField(TEXT("file"),F);return PayloadSourceSnapshot(Out);
    }
    bool BindSource(UClass* Requested)
    {
        R.Data->SetBoolField(TEXT("native_generated_data_asset"),true);R.Data->SetStringField(TEXT("admission_scope"),TEXT("Loaded saved-clean direct DataAsset or PrimaryDataAsset Blueprint class with own fields0..32: scalar7, primitive arrays/Sets, scalar-key Maps with the existing finite payload values, and compiled user-defined structs containing those fields. Fresh or already-loaded existing targets share one converter/current path. Native class branches remain unchanged."));R.Data->SetStringField(TEXT("description_cook_scope"),TEXT("This generated branch does not special-case native Deformer Description; own property flags are observed independently from editor variable descriptors."));R.Data->SetStringField(TEXT("effect_scope"),TEXT("Loaded-only source/target; public native factory, transaction/Modify, original per-property conversion, notifications and optional force-save. Composite conversion directly preserves actual known partial values on false; opaque callbacks/AssetManager derived cache/runtime are not measured zero."));R.Data->SetStringField(TEXT("generated_source_scope"),TEXT("Loaded saved-clean direct DataAsset or PrimaryDataAsset Blueprint and saved-clean compiled UDS sources. Own fields0..32; nested UDS/member GUID/default/file, array Inner, Set Element and Map Key/Value/current allocation roles are captured. Copied slot bags preserve duplicate/default entries without fixing physical/hash order. Only PrimaryDataAsset has inherited SDK-derived AssetBundleData; plain DataAsset has no inherited bundle slot."));
        Editor.Reset(GEditor);if(!Loaded(Editor.Get())||!IsEditorGameThread()||IsPlaySessionActive()||IsGarbageCollecting()||UE::IsSavingPackage())return Reject(TEXT("data_asset_generated_editor_required"),TEXT("Idle Editor required before source lookup."));World.Reset(GEditor->GetEditorWorldContext().World());if(!Current())return false;
        if(!Loaded(Requested)||Requested->GetClass()!=UBlueprintGeneratedClass::StaticClass()||Requested->HasAnyClassFlags(CLASS_Native|CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)||(Requested->GetSuperClass()!=UPrimaryDataAsset::StaticClass()&&Requested->GetSuperClass()!=UDataAsset::StaticClass())||!Requested->ClassGeneratedBy.IsResolved())return Reject(TEXT("data_asset_generated_source_pending"),TEXT("Already-loaded concrete direct UDataAsset or UPrimaryDataAsset Blueprint class required; inherited generated hierarchies remain incomplete."));
        NativeBase.Reset(Requested->GetSuperClass());NativeBaseFlags=NativeBase->GetClassFlags();NativeBaseSize=NativeBase->GetPropertiesSize();NativeBaseAlign=NativeBase->GetMinAlignment();if(!NativeBaseCurrent())return false;
        Class.Reset(static_cast<UBlueprintGeneratedClass*>(Requested));UObject* Source=Requested->ClassGeneratedBy.Get();Holds.Emplace(Source);Blueprint.Reset(Cast<UBlueprint>(Source));if(!Loaded(Blueprint.Get())||Blueprint->GetClass()!=UBlueprint::StaticClass())return Reject(TEXT("data_asset_generated_blueprint_pending"),TEXT("Exact ordinary source Blueprint required."));auto* B=Blueprint.Get();SourcePackage.Reset(B->GetPackage());
        if(!B->GetPathName().StartsWith(TEXT("/Game/"))||!Clean(B->GetPathName())||B->GetOuter()!=SourcePackage.Get()||SourcePackage->IsDirty()||B->Status!=BS_UpToDate||B->BlueprintType!=BPTYPE_Normal||!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.GetGCPtr().Get()!=NativeBase.Get()||!B->GeneratedClass.GetGCPtr().IsResolved()||B->GeneratedClass.GetGCPtr().Get()!=Class.Get()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||!B->SimpleConstructionScript.IsResolved()||B->SimpleConstructionScript.Get()||!B->ImplementedInterfaces.IsEmpty()||!B->FunctionGraphs.IsEmpty()||!B->MacroGraphs.IsEmpty()||!B->DelegateSignatureGraphs.IsEmpty()||B->NewVariables.Num()>32||B->UbergraphPages.Num()>1)return Reject(TEXT("data_asset_generated_source_blueprint_pending"),TEXT("Saved-clean direct data-only source with own payload variables0..32 and at most one empty EventGraph required."));
        Skeleton.Reset(Cast<UBlueprintGeneratedClass>(B->SkeletonGeneratedClass.GetGCPtr().Get()));if(!Loaded(Skeleton.Get())||Skeleton->GetClass()!=UBlueprintGeneratedClass::StaticClass()||!Class->ClassDefaultObject.IsResolved()||!Skeleton->ClassDefaultObject.IsResolved())return Reject(TEXT("data_asset_generated_classes_unavailable"),TEXT("Existing resolved generated/skeleton/default roles required; no CDO creation or compile."));Default.Reset(Class->ClassDefaultObject.Get());SkeletonDefault.Reset(Skeleton->ClassDefaultObject.Get());if(!Loaded(Default.Get()))return Reject(TEXT("data_asset_generated_default_unavailable"),TEXT("Existing generated CDO required for actual factory defaults; skeleton CDO may be null."));
        Declarations=B->NewVariables;Uber=B->UbergraphPages;EventStubs=B->EventGraphs;BlueprintFlags=B->GetFlags();ClassFlags=Class->GetClassFlags();SkeletonFlags=Skeleton->GetClassFlags();SourcePackageFlags=SourcePackage->GetPackageFlags();DefaultFlags=Default->GetFlags();SkeletonDefaultFlags=SkeletonDefault.IsValid()?SkeletonDefault->GetFlags():RF_NoFlags;ClassSize=Class->GetPropertiesSize();SkeletonSize=Skeleton->GetPropertiesSize();ClassAlign=Class->GetMinAlignment();SkeletonAlign=Skeleton->GetMinAlignment();
        for(const auto& V:Declarations)
        {
            const auto Container=V.VarType.ContainerType;const bool Map=Container==EPinContainerType::Map;
            if(!V.VarGuid.IsValid()||(Container!=EPinContainerType::None&&Container!=EPinContainerType::Array&&Container!=EPinContainerType::Set&&!Map)||V.VarType.PinSubCategoryObject.IsStale()||V.VarType.PinValueType.TerminalSubCategoryObject.IsStale()||(!Map&&V.VarType.PinValueType.TerminalSubCategoryObject.Get())||!Clean(V.DefaultValue))return Reject(TEXT("data_asset_generated_variable_pending"),TEXT("Finite scalar/array/Set/Map/UDS declarations with valid GUID and resolved subtypes required; wider original types remain incomplete."));
            UObject* Type=V.VarType.PinSubCategoryObject.Get();if(Type){Holds.Emplace(Type);if(!Context()||!Loaded(Type)||Type->GetClass()!=UUserDefinedStruct::StaticClass()||Container!=EPinContainerType::None)return Reject(TEXT("data_asset_generated_variable_subtype_pending"),TEXT("Only existing UDS scalar subtype is admitted; Map keys and Set elements must be native primitives."));}
            UObject* ValueType=Map?V.VarType.PinValueType.TerminalSubCategoryObject.Get():nullptr;if(ValueType){Holds.Emplace(ValueType);if(!Context()||!Loaded(ValueType)||ValueType->GetClass()!=UUserDefinedStruct::StaticClass())return Reject(TEXT("data_asset_generated_map_terminal_pending"),TEXT("Existing held UDS Map value subtype or primitive terminal required; object/enum values remain incomplete."));}
        }
        // Validate each held CDO class/outer/default role before any field offset/value read.
        if(!ClassRole(Class.Get(),Default.Get(),ClassFlags,B->GetName()+TEXT("_C"))||!ClassRole(Skeleton.Get(),SkeletonDefault.Get(),SkeletonFlags,TEXT("SKEL_")+B->GetName()+TEXT("_C")))return false;
        if(!Layout(Class.Get(),Fields)||!Layout(Skeleton.Get(),SkeletonFields))return false;
        if(!CapturePayloadDefaults())return false;auto CaptureDefaults=[&](UObject* D,const TArray<FField>& Rows,TArray<FValue>& Values)->bool{if(!D)return true;for(const auto& E:Rows){FValue V;if(!E.Frame&&!ReadCell(E.P,E.P->ContainerPtrToValuePtr<void>(D),V))return false;Values.Add(MoveTemp(V));}return true;};if(!CaptureDefaults(Default.Get(),Fields,DefaultValues)||!CaptureDefaults(SkeletonDefault.Get(),SkeletonFields,SkeletonDefaultValues))return false;R.Data->SetBoolField(TEXT("generated_reflected_payload"),CompositePayload);R.Data->SetNumberField(TEXT("generated_payload_struct_sources"),PayloadStructs.Num());R.Data->SetBoolField(TEXT("payload_array_addresses_held_across_callbacks"),false);R.Data->SetBoolField(TEXT("generated_map_set_payload"),HashedPayload);if(HashedPayload){R.Data->SetBoolField(TEXT("typed_container_comparison_ignores_physical_order"),true);R.Data->SetBoolField(TEXT("typed_container_comparison_preserves_duplicate_slots"),true);R.Data->SetBoolField(TEXT("hash_bucket_contents_verified"),false);R.Data->SetBoolField(TEXT("container_normalization_overrides_original_success"),false);}
        GenFrame=Class->UberGraphFramePointerProperty;SkelFrame=Skeleton->UberGraphFramePointerProperty;GenUber.Reset(Class->UberGraphFunction.Get());SkelUber.Reset(Skeleton->UberGraphFunction.Get());
        if(HasAssetBundles())
        {
            BundleProperty=FindFProperty<FStructProperty>(UPrimaryDataAsset::StaticClass(),TEXT("AssetBundleData"));if(!BundleProperty||!BundleProperty->Struct.IsResolved())return Reject(TEXT("data_asset_generated_bundle_layout_pending"),TEXT("Public reflected native AssetBundleData binding required."));BundleStruct.Reset(BundleProperty->Struct.Get());if(!Loaded(BundleStruct.Get())||BundleStruct.Get()!=FAssetBundleData::StaticStruct()||!BundlesEmpty(Default.Get())||!BundlesEmpty(SkeletonDefault.Get()))return Reject(TEXT("data_asset_generated_bundle_source_pending"),TEXT("First source native default bundle must be empty; no bundle import/runtime asset manager proof."));
        }
        else if(FindFProperty<FStructProperty>(UDataAsset::StaticClass(),TEXT("AssetBundleData")))return Reject(TEXT("data_asset_generated_plain_base_layout_pending"),TEXT("Plain native UDataAsset must not expose an inherited AssetBundleData field."));
        R.Data->SetStringField(TEXT("generated_direct_native_base"),NativeBase->GetPathName());R.Data->SetBoolField(TEXT("inherited_asset_bundle_data_present"),HasAssetBundles());R.Data->SetNumberField(TEXT("generated_own_variable_count"),Declarations.Num());
        for(const auto& H:Uber){if(!H.IsResolved()||!Loaded(H.Get())||H->GetClass()!=UEdGraph::StaticClass()||H->GetOuter()!=B||!H->Nodes.IsEmpty()||!H->SubGraphs.IsEmpty())return Reject(TEXT("data_asset_generated_source_graph_pending"),TEXT("First source EventGraph must be empty; no user graph execution is assumed."));FGraphRole G;G.G.Reset(H.Get());G.Id=H->GraphGuid;G.Path=H->GetPathName();Graphs.Add(MoveTemp(G));}
        Bound=true;if(!Current())return false;R.Stage=TEXT("generated_source_snapshot");if(!SourceSnapshot(SourceBefore))return false;R.Data->SetObjectField(TEXT("class_source_before"),SourceBefore);R.Data->SetStringField(TEXT("supported_class"),Class->GetPathName());if(!Clean(Text(SourceBefore)))return Reject(TEXT("data_asset_generated_source_dependency_rejected"),TEXT("Captured source defaults/metadata must not reference tooling runtime dependencies."));return true;
    }
    bool BindTarget(UDataAsset* A,bool Fresh)
    {
        TStrongObjectPtr<UDataAsset> Immediate(A);if(!Current())return false;if(!Loaded(A)||A->GetClass()!=Class.Get()||A->GetOuter()!=A->GetPackage()||!A->GetPathName().Equals(PathName,ESearchCase::CaseSensitive)||A->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_Transient)||!A->HasAllFlags(RF_Public|RF_Standalone))return Failure(TEXT("returned_target_role"),PathName,TEXT("not an exact live public standalone generated asset"));
        Target.Reset(A);TargetPackage.Reset(A->GetPackage());TargetFlags=A->GetFlags();InitialDirty=TargetPackage->IsDirty();for(int32 I=0;I<Fields.Num();++I){FValue V;if(!Fields[I].Frame&&!ReadCell(Fields[I].P,Fields[I].P->ContainerPtrToValuePtr<void>(A),V))return false;if(Fresh&&!Fields[I].Frame&&!Same(V,DefaultValues[I]))return Failure(TEXT("factory_payload_defaults"),ValueText(DefaultValues[I]),ValueText(V));Expected.Add(MoveTemp(V));}TargetBound=true;return Current();
    }
    bool Snapshot(TSharedRef<FJsonObject> Out)
    {
        if(!Current())return false;Out->SetStringField(TEXT("data_asset"),PathName);Out->SetStringField(TEXT("class"),Class->GetPathName());Out->SetBoolField(TEXT("success"),true);Out->SetBoolField(TEXT("package_dirty"),TargetPackage->IsDirty());Out->SetStringField(TEXT("object_flags"),LexToString(uint64(Target->GetFlags())));auto Values=MakeShared<FJsonObject>();Count(R,TEXT("json_export_calls"));const bool OK=FJsonObjectConverter::UStructToJsonObject(Class.Get(),Target.Get(),Values,0,0);if(!Current()||!OK)return Failure(TEXT("target_json_export"),TEXT("complete native JSON export"),TEXT("failed"));int32 N=0,Chars=0;if(!Bounded(MakeShared<FJsonValueObject>(Values),N,Chars)||!Clean(Text(Values)))return Failure(TEXT("target_json_bounds_or_reference"),TEXT("bounded safe scalar/derived JSON"),TEXT("unobservable/forbidden"));Out->SetObjectField(TEXT("values"),Values);if(HashedPayload){auto Typed=MakeShared<FJsonObject>();for(int32 I=0;I<Expected.Num();++I)if(!Fields[I].Frame&&HasHashValue(Expected[I]))Typed->SetStringField(Fields[I].Name.ToString(),ValueText(Expected[I]));Out->SetObjectField(TEXT("typed_unordered_slot_values"),Typed);Out->SetStringField(TEXT("typed_container_value_scope"),TEXT("Copied unordered typed slots/pairs preserve multiplicity and actual default/partial entries. Physical slot order, addresses, hash buckets and uniqueness are not asserted. Stock JSON values and original final subset predicate remain separate."));}Out->SetBoolField(TEXT("asset_bundle_data_is_sdk_derived"),HasAssetBundles());Out->SetStringField(TEXT("direct_native_base"),NativeBase->GetPathName());Out->SetBoolField(TEXT("inherited_asset_bundle_data_present"),HasAssetBundles());return true;
    }
    bool References(TSharedRef<FJsonObject> Out)
    {
        if(!Current())return false;TArray<UObject*> Refs;FReferenceFinder Finder(Refs,nullptr,false,true,false,true);Finder.FindReferences(Target.Get());for(UObject* O:Refs)if(O)Holds.Emplace(O);if(!Current())return false;if(Refs.Num()>1024)return Failure(TEXT("direct_reference_count"),TEXT("<=1024"),LexToString(Refs.Num()));
        for(UObject* O:Refs){if(!Loaded(O)||!Clean(O->GetPathName())||!Clean(O->GetClass()->GetPathName()))return Failure(TEXT("direct_hard_reference"),TEXT("loaded non-tooling references"),TEXT("invalid/forbidden"));Out->SetObjectField(O->GetPathName(),Identity(O));}return Current();
    }
    bool FinalSource()
    {auto Now=MakeShared<FJsonObject>();if(!SourceSnapshot(Now))return false;R.Data->SetObjectField(TEXT("class_source_after"),Now);if(!(HashedPayload?Equal(SourceComparable(SourceBefore),SourceComparable(Now)):Equal(SourceBefore,Now)))return Failure(TEXT("source_snapshot"),Text(SourceBefore),Text(Now));auto Other=MakeShared<FJsonObject>();if(!Dirty(PackageName,Other)||!Current()||!Equal(OtherBefore,Other))return Failure(TEXT("other_package_dirty"),Text(OtherBefore),Text(Other));R.Data->SetBoolField(TEXT("source_preserved"),true);R.Data->SetBoolField(TEXT("other_package_dirty_preserved"),true);return true;}
    bool Save()
    {
        if(!R.SaveRequested)return true;if(!Current())return false;if(!TargetPackage->IsFullyLoaded())return Failure(TEXT("target_save_ready"),TEXT("already fully loaded package"),TEXT("not fully loaded"));
        R.Stage=TEXT("save_generated_asset");R.Known=false;R.SaveAttempted=true;SaveEntered=true;Count(R,TEXT("save_attempt_count"));auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("route"),TEXT("UEditorAssetLibrary::SaveLoadedAsset"));Receipt->SetBoolField(TEXT("only_if_dirty"),false);Receipt->SetBoolField(TEXT("attempted"),true);R.Data->SetArrayField(TEXT("save_calls"),{MakeShared<FJsonValueObject>(Receipt)});
        const bool Saved=UEditorAssetLibrary::SaveLoadedAsset(Target.Get(),false);R.Saved=Saved;R.Data->SetBoolField(TEXT("saved"),Saved);Receipt->SetBoolField(TEXT("returned"),true);Receipt->SetBoolField(TEXT("saved"),Saved);if(!Saved)return Failure(TEXT("save_return"),TEXT("saved true"),TEXT("false; actual write outcome not proven"));SavedExports=true;if(!Current())return false;auto FileAfter=MakeShared<FJsonObject>();if(!File(PackageName,FileAfter)||!FileAfter->GetBoolField(TEXT("exists"))||FileAfter->GetNumberField(TEXT("bytes"))<=0||TargetPackage->IsDirty())return Failure(TEXT("saved_file"),TEXT("existing nonempty file and clean target"),TEXT("not observed"));R.Data->SetObjectField(TEXT("after_save_file"),FileAfter);R.Data->SetBoolField(TEXT("persistence_verified"),true);return true;
    }
    bool Setup(const FString& RequestedPath,bool Create,UClass* RequestedClass=nullptr,const FContext* LoadedContext=nullptr)
    {
        if(LoadedContext&&(!LoadedContext->GeneratedLoaded||!R.Known||!GeneratedLoadContextCurrent(*LoadedContext)))return Failure(TEXT("load_handoff_context"),TEXT("Known original LoadAsset return in the same held Editor/world"),TEXT("not current; no target lookup or next SDK"));
        FContext C;if(!Path(RequestedPath,Create||R.Op!=TEXT("get_data_asset"),C,R))return false;PathName=C.Path;PackageName=C.Package;Creating=Create;
        if(Create){if(!BindSource(RequestedClass))return false;}
        else{TStrongObjectPtr<UDataAsset> A(FindObject<UDataAsset>(nullptr,*PathName));if(!Loaded(A.Get())){if(LoadedContext)return Failure(TEXT("load_handoff_target"),TEXT("Exact held LoadAsset result remains loaded"),TEXT("not current"));return Reject(TEXT("data_asset_generated_target_not_loaded"),TEXT("Already-loaded exact generated asset or the held original LoadAsset return required; no added loader."));}
            if(LoadedContext&&(!Loaded(LoadedContext->Asset.Get())||!Loaded(LoadedContext->LoadClass.Get())||!Loaded(LoadedContext->LoadPackage.Get())||A.Get()!=LoadedContext->Asset.Get()||A->GetClass()!=LoadedContext->LoadClass.Get()||A->GetOuter()!=LoadedContext->LoadPackage.Get()||PathName!=LoadedContext->Path||PackageName!=LoadedContext->Package))return Failure(TEXT("load_handoff_identity"),TEXT("Fresh target lookup equals the held original result/class/package/path"),TEXT("changed; no source observer or next SDK"));
            if(!BindSource(A->GetClass())||!BindTarget(A.Get(),false))return false;}
        if(!Dirty(PackageName,OtherBefore)||!Current()||!File(PackageName,FileBefore))return Failure(TEXT("initial_file_or_dirty"),TEXT("complete source/target state"),TEXT("unobservable"));R.Data->SetObjectField(TEXT("target_file_before"),FileBefore);return true;
    }
};


// Selected primitive conversion scratch has C++ storage lifetime independent of FProperty.
// No property-driven destructor is invoked if an SDK callback retires a class/field.
struct FQCScalarScratch
{
    alignas(16) uint8 Bytes[16]{};FString String;FName Name;
    void* Address(FProperty* P){if(P->GetClass()==FStrProperty::StaticClass())return &String;if(P->GetClass()==FNameProperty::StaticClass())return &Name;return Bytes;}
};
bool QCFactorySnapshot(FGeneratedDataAssetWork& W,TSharedRef<FJsonObject> Out)
{
    if(!W.Current()||!Loaded(W.FactoryDefault.Get()))return false;const auto Fields=W.FactoryFields;
    for(const auto& E:Fields){if(!W.Current())return false;FProperty* P=E.P;FString Value;const bool Exported=P->ExportText_Direct(Value,P->ContainerPtrToValuePtr<void>(W.FactoryDefault.Get()),P->ContainerPtrToValuePtr<void>(W.FactoryDefault.Get()),W.FactoryDefault.Get(),PPF_None);if(!W.Current()||!Exported||Value.Len()>65536)return W.Failure(TEXT("factory_default_export"),TEXT("bounded complete native export"),TEXT("failed"));Out->SetStringField(P->GetPathName(),Value);}return W.Current();
}
bool QCFinish(FGeneratedDataAssetWork& W,TSharedRef<FJsonObject> Out,TSharedRef<FJsonObject> ReferencesBefore,bool ReadOnly=false)
{
    if(!W.Current()||!W.Snapshot(Out)||!W.FinalSource())return false;auto Refs=MakeShared<FJsonObject>();if(!W.References(Refs))return false;W.R.Data->SetObjectField(TEXT("references_after"),Refs);if(!Equal(ReferencesBefore,Refs))return W.Failure(TEXT("direct_references"),Text(ReferencesBefore),Text(Refs));W.R.Data->SetBoolField(TEXT("references_preserved"),true);
    if(W.FactoryDefault.IsValid()){auto Now=MakeShared<FJsonObject>();if(!QCFactorySnapshot(W,Now))return false;W.R.Data->SetObjectField(TEXT("factory_default_after"),Now);if(!Equal(W.FactoryBefore,Now))return W.Failure(TEXT("factory_default_snapshot"),Text(W.FactoryBefore),Text(Now));}
    if(!W.R.Saved){auto File=MakeShared<FJsonObject>();if(!W.File(W.PackageName,File))return false;W.R.Data->SetObjectField(TEXT("target_file_after"),File);if(!Equal(W.FileBefore,File))return W.Failure(TEXT("unsaved_target_file"),Text(W.FileBefore),Text(File));}
    if(ReadOnly&&W.TargetPackage->IsDirty()!=W.InitialDirty)return W.Failure(TEXT("read_package_dirty"),LexToString(W.InitialDirty),LexToString(W.TargetPackage->IsDirty()));
    W.R.Data->SetBoolField(TEXT("observations_complete"),true);W.R.Data->SetBoolField(TEXT("unselected_state_preserved"),true);W.R.Data->SetStringField(TEXT("generated_preservation_scope"),TEXT("Held source/direct and nested field/default/file roles and exact target authored scalar/UDS/primitive-array/Map/Set values. Hash-containing roots use unordered typed slot bags/pairs with multiplicity; source raw JSON remains reported and its comparison uses the already-verified typed roots. Array storage is freshly validated/reacquired, not required to keep an allocation address across the converter. Actual inherited AssetBundleData after save remains SDK-derived; AssetManager cache/runtime/full transitive proof is not claimed."));return W.Current();
}
FString QCGet(const FString& AssetPath,const FResult* LoadPrefix=nullptr,const FContext* LoadedContext=nullptr)
{
    FResult R;Init(R,TEXT("get_data_asset"));if(LoadPrefix)R=*LoadPrefix;if(QCActive||InputActionActive||MappingContextActive)return Fail(R,TEXT("data_asset_generated_reentry"),TEXT("Asset operation already active.")),Encode(R);TGuardValue<bool> Active(QCActive,true);FGeneratedDataAssetWork W(R);
    if(!W.Setup(AssetPath,false,nullptr,LoadedContext))return Encode(R);auto Before=MakeShared<FJsonObject>(),After=MakeShared<FJsonObject>(),Refs=MakeShared<FJsonObject>();R.Stage=TEXT("generated_read_before");if(!W.Snapshot(Before)||!W.References(Refs))return Encode(R);R.Data->SetObjectField(TEXT("before_observation"),Before);R.Data->SetObjectField(TEXT("references_before"),Refs);
    R.Stage=TEXT("generated_read_after");if(!QCFinish(W,After,Refs,true))return Encode(R);R.Data->SetObjectField(TEXT("after_observation"),After);if(!Equal(Before,After))return W.Failure(TEXT("read_snapshot"),Text(Before),Text(After)),Encode(R);R.Data->SetStringField(TEXT("class"),W.Class->GetPathName());R.Data->SetObjectField(TEXT("values"),After->GetObjectField(TEXT("values")).ToSharedRef());R.Known=true;R.Changed=false;R.Success=R.Post=true;R.Stage=TEXT("complete");return Encode(R);
}
FString QCSet(const FString& AssetPath,const FString& PropertiesJson,bool Save,const FResult* LoadPrefix=nullptr,const FContext* LoadedContext=nullptr)
{
    FResult R;Init(R,TEXT("set_data_asset_properties"),Save);if(LoadPrefix)R=*LoadPrefix;if(QCActive||InputActionActive||MappingContextActive)return Fail(R,TEXT("data_asset_generated_reentry"),TEXT("Asset operation already active.")),Encode(R);TGuardValue<bool> Active(QCActive,true);
    TSharedPtr<FJsonObject> Requested;int32 N=0,Chars=0;if(PropertiesJson.Len()>65536||HasNul(PropertiesJson)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(PropertiesJson),Requested)||!Requested.IsValid()||Requested->Values.Num()>32||!Bounded(MakeShared<FJsonValueObject>(Requested),N,Chars))return Fail(R,TEXT("data_asset_properties_json_invalid"),TEXT("Bounded finite properties JSON object with at most32 fields required.")),Encode(R);
    R.Data->SetObjectField(TEXT("requested_properties"),Requested.ToSharedRef());FGeneratedDataAssetWork W(R);if(!W.Setup(AssetPath,false,nullptr,LoadedContext))return Encode(R);const TArray<FGeneratedDataAssetWork::FValue> Original=W.Expected;
    auto Before=MakeShared<FJsonObject>(),After=MakeShared<FJsonObject>(),Refs=MakeShared<FJsonObject>();if(!W.Snapshot(Before)||!W.References(Refs))return Encode(R);R.Data->SetObjectField(TEXT("before"),Before);R.Data->SetObjectField(TEXT("references_before"),Refs);R.Data->SetStringField(TEXT("class"),W.Class->GetPathName());
    TArray<TSharedPtr<FJsonValue>> Applied,Failed,Attempts,Notifications;TArray<int32> AppliedIndices;TSet<FName> AppliedNames;TUniquePtr<FScopedTransaction> Transaction;
    auto Run=[&]() -> bool
    {
        R.Stage=TEXT("generated_apply_properties");R.Attempted=true;R.Known=false;Count(R,TEXT("outer_transaction_calls"));Transaction=MakeUnique<FScopedTransaction>(NSLOCTEXT("CubelessDataAsset","GeneratedSet","Set generated DataAsset properties"));if(!W.Current())return false;Count(R,TEXT("explicit_modify_calls"));W.Target->Modify();if(!W.Current())return false;
        for(const auto& Pair:Requested->Values)
        {
            if(!W.Current())return false;const FString Name(Pair.Key.ToView());auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("property"),Name);Receipt->SetBoolField(TEXT("conversion_attempted"),false);Attempts.Add(MakeShared<FJsonValueObject>(Receipt));R.Data->SetArrayField(TEXT("property_attempts"),Attempts);FString Error;int32 Index=INDEX_NONE;
            if(Name.Len()<NAME_SIZE&&!HasNul(Name))for(int32 I=0;I<W.Fields.Num();++I)if(!W.Fields[I].Frame&&W.Fields[I].Name==FName(*Name)){Index=I;break;}
            if(Index==INDEX_NONE)Error=TEXT("Property not found or outside own reflected source fields");
            else
            {
                auto* P=W.Fields[Index].P;const auto Value=Pair.Value;
                if(!Value.IsValid()||!Clean(Text([&](){auto J=MakeShared<FJsonObject>();J->SetField(TEXT("value"),Value);return J;}())))Error=TEXT("Invalid or tooling-dependent property value");
                else if((P->GetClass()==FIntProperty::StaticClass()||P->GetClass()==FInt64Property::StaticClass())&&Value->Type==EJson::Number&&(Value->AsNumber()>=9223372036854775808.0||Value->AsNumber()< -9223372036854775808.0))Error=TEXT("Numeric JSON would overflow the SDK int64 intermediate; use a bounded string or supported numeric value");
                else if(!FGeneratedDataAssetWork::Scalar(P))
                {
                    if(!W.PayloadInputSafe(P,Value))Error=TEXT("Composite input exceeds finite array/depth or safe numeric conversion bounds");
                    else
                    {
                        // The original converter may resize/write a prefix and return false. No property-driven scratch destructor spans this call.
                        const auto Previous=W.Expected[Index];W.Mutable=Index;Receipt->SetBoolField(TEXT("conversion_attempted"),true);Receipt->SetBoolField(TEXT("direct_target_conversion"),true);Count(R,TEXT("json_conversion_calls"));
                        const bool Converted=FJsonObjectConverter::JsonValueToUProperty(Value,P,P->ContainerPtrToValuePtr<void>(W.Target.Get()),0,0);Receipt->SetBoolField(TEXT("conversion_returned"),true);Receipt->SetBoolField(TEXT("converted"),Converted);
                        if(!W.Current())return false;FGeneratedDataAssetWork::FValue Actual;if(!W.ReadCell(P,P->ContainerPtrToValuePtr<void>(W.Target.Get()),Actual))return false;
                        const bool Changed=!FGeneratedDataAssetWork::Same(Previous,Actual);W.Expected[Index]=MoveTemp(Actual);W.Mutable=INDEX_NONE;Receipt->SetBoolField(TEXT("actual_selected_value_changed"),Changed);Receipt->SetBoolField(TEXT("known_partial_value_retained"),!Converted&&Changed);if(!W.Current())return false;
                        if(!Converted)Error=TEXT("Value conversion failed (check type); actual converted prefix is retained");
                        else {Applied.Add(MakeShared<FJsonValueString>(Name));AppliedIndices.Add(Index);AppliedNames.Add(FName(*Name));}
                    }
                }
                else
                {
                    FQCScalarScratch Scratch;Receipt->SetBoolField(TEXT("conversion_attempted"),true);Count(R,TEXT("json_conversion_calls"));const bool Converted=FJsonObjectConverter::JsonValueToUProperty(Value,P,Scratch.Address(P),0,0);Receipt->SetBoolField(TEXT("conversion_returned"),true);Receipt->SetBoolField(TEXT("converted"),Converted);if(!W.Current())return false;
                    if(!Converted)Error=TEXT("Value conversion failed (check type)");
                    else
                    {
                        const auto Planned=FGeneratedDataAssetWork::Read(P,Scratch.Address(P));W.Mutable=Index;Count(R,TEXT("property_storage_copy_calls"));P->CopyCompleteValue(P->ContainerPtrToValuePtr<void>(W.Target.Get()),Scratch.Address(P));Receipt->SetBoolField(TEXT("storage_copy_returned"),true);if(!W.Current())return false;
                        const auto Actual=FGeneratedDataAssetWork::Read(P,P->ContainerPtrToValuePtr<void>(W.Target.Get()));if(!FGeneratedDataAssetWork::Same(Actual,Planned))return W.Failure(TEXT("selected_primitive_copy"),FGeneratedDataAssetWork::ValueText(Planned),FGeneratedDataAssetWork::ValueText(Actual));W.Expected[Index]=Planned;W.Mutable=INDEX_NONE;if(!W.Current())return false;Applied.Add(MakeShared<FJsonValueString>(Name));AppliedIndices.Add(Index);AppliedNames.Add(FName(*Name));
                    }
                }
            }
            if(!Error.IsEmpty()){auto F=MakeShared<FJsonObject>();F->SetStringField(TEXT("property"),Name);F->SetStringField(TEXT("error"),Error);Failed.Add(MakeShared<FJsonValueObject>(F));Receipt->SetStringField(TEXT("error"),Error);}R.Data->SetArrayField(TEXT("applied"),Applied);R.Data->SetArrayField(TEXT("failed"),Failed);
        }
        for(int32 I:AppliedIndices){if(!W.Current())return false;auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("property"),W.Fields[I].Name.ToString());Receipt->SetBoolField(TEXT("attempted"),true);Notifications.Add(MakeShared<FJsonValueObject>(Receipt));R.Data->SetArrayField(TEXT("property_notifications"),Notifications);FPropertyChangedEvent Event(W.Fields[I].P);Count(R,TEXT("post_edit_change_property_calls"));W.Target->PostEditChangeProperty(Event);Receipt->SetBoolField(TEXT("returned"),true);if(!W.Current())return false;}
        if(!Applied.IsEmpty()){Count(R,TEXT("mark_package_dirty_calls"));W.Target->MarkPackageDirty();if(!W.Current())return false;}auto Configured=MakeShared<FJsonObject>();if(!W.Snapshot(Configured)||!W.FinalSource())return false;R.Data->SetObjectField(TEXT("after_configuration"),Configured);
        // Original saves after known individual failures as well as success.
        return W.Save();
    };
    const bool Completed=Run();Transaction.Reset();if(W.Stopped)return Encode(R);if(!Completed||!W.Current())return Encode(R);R.Stage=TEXT("generated_terminal_observation");if(!QCFinish(W,After,Refs))return Encode(R);R.Data->SetObjectField(TEXT("after"),After);
    int32 Updated=0,Skipped=0,PropertyFailure=0;TArray<TSharedPtr<FJsonValue>> Results;
    for(const auto& Pair:Requested->Values){const FString Name(Pair.Key.ToView());const auto B=ValueByName(Before->GetObjectField(TEXT("values")).ToSharedRef(),Name),Z=ValueByName(After->GetObjectField(TEXT("values")).ToSharedRef(),Name);const bool Did=AppliedNames.Contains(FName(*Name)),Was=PreviewMatchesExpected(Pair.Value,B),Is=PreviewMatchesExpected(Pair.Value,Z);FString Outcome;if(!Did){Outcome=TEXT("conversion_failed");++PropertyFailure;}else if(!Is){Outcome=TEXT("readback_failed");++PropertyFailure;}else if(Was){Outcome=TEXT("skipped_unchanged");++Skipped;}else{Outcome=TEXT("updated");++Updated;}auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("property"),Name);Row->SetStringField(TEXT("outcome"),Outcome);Row->SetBoolField(TEXT("applied"),Did);Row->SetBoolField(TEXT("before_matched_expected"),Was);Row->SetBoolField(TEXT("after_matched_expected"),Is);Results.Add(MakeShared<FJsonValueObject>(Row));}
    bool ActualChanged=false;for(int32 I=0;I<Original.Num();++I)if(!W.Fields[I].Frame&&!FGeneratedDataAssetWork::Same(Original[I],W.Expected[I]))ActualChanged=true;const bool Specific=PropertyFailure==0&&(!Save||R.Saved);auto Delta=MakeShared<FJsonObject>();Delta->SetNumberField(TEXT("affected_count"),Updated);Delta->SetNumberField(TEXT("created_count"),0);Delta->SetNumberField(TEXT("updated_count"),Updated);Delta->SetNumberField(TEXT("removed_count"),0);Delta->SetNumberField(TEXT("skipped_count"),Skipped);Delta->SetNumberField(TEXT("failed_count"),PropertyFailure);Delta->SetNumberField(TEXT("property_failure_count"),PropertyFailure);Delta->SetNumberField(TEXT("completion_failure_count"),Save&&!R.Saved?1:0);
    R.Data->SetArrayField(TEXT("property_results"),Results);R.Data->SetNumberField(TEXT("updated_property_count"),Updated);R.Data->SetNumberField(TEXT("skipped_property_count"),Skipped);R.Data->SetNumberField(TEXT("property_failure_count"),PropertyFailure);R.Data->SetNumberField(TEXT("completion_failure_count"),Save&&!R.Saved?1:0);R.Data->SetBoolField(TEXT("actual_authored_value_changed"),ActualChanged);Contract(R,Before,After,Delta,!Applied.IsEmpty(),Updated>0,Failed.IsEmpty(),Specific,TEXT("generated_data_asset_reflected_readback"),{TEXT("class_source_current"),TEXT("own_reflected_values"),TEXT("save_gate")});
    R.Known=true;R.Changed=ActualChanged;if(!Specific||!Failed.IsEmpty()){Fail(R,TEXT("data_asset_properties_failed"),TEXT("Known terminal property failure; applied/failed, actual saved prefix and exact retained values preserved."));R.Stage=TEXT("complete_with_property_failures");return Encode(R);}R.Success=R.Post=true;R.Stage=TEXT("complete");return Encode(R);
}
FString QCCreate(const FString& AssetPath,const FString& ClassPathRequest,bool Save)
{
    FResult R;Init(R,TEXT("create_data_asset"),Save);if(QCActive||InputActionActive||MappingContextActive)return Fail(R,TEXT("data_asset_generated_reentry"),TEXT("Asset operation already active.")),Encode(R);TGuardValue<bool> Active(QCActive,true);
    if(ClassPathRequest.Len()>1024||HasNul(ClassPathRequest)||!Clean(ClassPathRequest)||!FPackageName::IsValidObjectPath(ClassPathRequest))return Fail(R,TEXT("data_asset_generated_class_path_invalid"),TEXT("Exact loaded safe Game generated class object path required.")),Encode(R);TStrongObjectPtr<UClass> Requested(FindObject<UClass>(nullptr,*ClassPathRequest));FGeneratedDataAssetWork W(R);if(!W.Setup(AssetPath,true,Requested.Get()))return Encode(R);
    auto Absence=[&](const TCHAR* Key)->bool
    {
        if(!W.Current())return false;auto J=MakeShared<FJsonObject>();R.Data->SetObjectField(Key,J);Count(R,TEXT("asset_exists_query_calls"));const bool Exists=UEditorAssetLibrary::DoesAssetExist(W.PathName);J->SetBoolField(TEXT("registry_returned"),true);if(!W.Current())return false;J->SetBoolField(TEXT("registry_exists"),Exists);const bool ObjectExists=FindObject<UObject>(nullptr,*W.PathName)!=nullptr,PackageExists=FindObject<UPackage>(nullptr,*W.PackageName)!=nullptr;J->SetBoolField(TEXT("memory_object_exists"),ObjectExists);J->SetBoolField(TEXT("memory_package_exists"),PackageExists);auto File=MakeShared<FJsonObject>();if(!W.File(W.PackageName,File))return false;J->SetObjectField(TEXT("disk"),File);if(Exists||ObjectExists||PackageExists||File->GetBoolField(TEXT("exists")))return W.Reject(TEXT("data_asset_already_exists_or_unobservable"),TEXT("Fresh absent object/package/registry/file required; no replacement/retry."));return true;
    };
    if(!Absence(TEXT("target_before_creation")))return Encode(R);auto* FC=UDataAssetFactory::StaticClass();if(!FC->ClassDefaultObject.IsResolved()||!Loaded(FC->ClassDefaultObject.Get())||FC->ClassDefaultObject->GetClass()!=FC||!FModuleManager::Get().IsModuleLoaded(TEXT("AssetTools")))return Fail(R,TEXT("data_asset_generated_factory_not_ready"),TEXT("Already existing exact native factory CDO and loaded AssetTools module required; no CDO/module warming.")),Encode(R);
    W.FactoryDefault.Reset(static_cast<UDataAssetFactory*>(FC->ClassDefaultObject.Get()));auto CDOClass=W.FactoryDefault->DataAssetClass;if(!CDOClass.GetGCPtr().IsResolved())return Fail(R,TEXT("data_asset_generated_factory_class_unresolved"),TEXT("Factory CDO class slot must already be resolved.")),Encode(R);W.FactoryDefaultClass=CDOClass.GetGCPtr().Get();if(W.FactoryDefaultClass)W.Holds.Emplace(W.FactoryDefaultClass);
    for(TFieldIterator<FProperty> It(FC,EFieldIterationFlags::IncludeSuper);It;++It){FProperty* P=*It;if(W.FactoryFields.Num()>=128)return W.Reject(TEXT("data_asset_generated_factory_field_bound"),TEXT("Native factory field inventory exceeds128.")),Encode(R);FGeneratedDataAssetWork::FField E;E.P=P;E.Kind=P->GetClass();E.Name=P->GetFName();E.Owner=P->GetOwnerStruct();E.Flags=uint64(P->GetPropertyFlags());E.Size=P->GetSize();E.Offset=P->GetOffset_ForInternal();E.Dim=P->ArrayDim;W.FactoryFields.Add(E);}
    if(!QCFactorySnapshot(W,W.FactoryBefore))return Encode(R);R.Data->SetObjectField(TEXT("factory_default_before"),W.FactoryBefore);
    auto Before=MakeShared<FJsonObject>(),After=MakeShared<FJsonObject>(),Refs=MakeShared<FJsonObject>();Before->SetBoolField(TEXT("exists"),false);Before->SetStringField(TEXT("asset_path"),W.PathName);R.Data->SetObjectField(TEXT("before"),Before);TUniquePtr<FScopedTransaction> Transaction;
    auto Run=[&]() -> bool
    {
        R.Stage=TEXT("create_generated_data_asset");R.Attempted=true;R.Known=false;Count(R,TEXT("outer_transaction_calls"));Transaction=MakeUnique<FScopedTransaction>(NSLOCTEXT("CubelessDataAsset","GeneratedCreate","Create generated DataAsset"));if(!W.Current())return false;
        Count(R,TEXT("factory_allocation_attempts"));TStrongObjectPtr<UDataAssetFactory> ReturnedFactory(NewObject<UDataAssetFactory>());if(!W.Current())return false;if(!Loaded(ReturnedFactory.Get())||ReturnedFactory->GetClass()!=FC||ReturnedFactory->GetOuter()!=GetTransientPackage())return W.Failure(TEXT("returned_factory"),TEXT("live exact transient native factory"),TEXT("not observed"));ReturnedFactory->DataAssetClass=W.Class.Get();W.Factory.Reset(ReturnedFactory.Get());if(!W.Current())return false;R.Data->SetObjectField(TEXT("created_factory"),Identity(W.Factory.Get()));
        Count(R,TEXT("asset_tools_get_calls"));IAssetTools& Tools=FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();if(!W.Current()||!Absence(TEXT("precreate_absence")))return false;Count(R,TEXT("asset_tools_create_attempts"));TStrongObjectPtr<UObject> Returned(Tools.CreateAsset(FPackageName::GetShortName(W.PackageName),FPackageName::GetLongPackagePath(W.PackageName),W.Class.Get(),W.Factory.Get()));R.Data->SetBoolField(TEXT("asset_tools_create_returned"),true);if(!W.Current())return false;
        if(!Loaded(Returned.Get())||Returned->GetClass()!=W.Class.Get())return W.Failure(TEXT("create_return"),TEXT("exact generated asset"),TEXT("null/wrong returned class"));if(!W.BindTarget(static_cast<UDataAsset*>(Returned.Get()),true))return false;R.Data->SetObjectField(TEXT("created_asset"),Identity(W.Target.Get()));R.Data->SetStringField(TEXT("class"),W.Class->GetPathName());Count(R,TEXT("mark_package_dirty_calls"));W.Target->MarkPackageDirty();if(!W.Current()||!W.References(Refs)||!W.FinalSource())return false;R.Data->SetObjectField(TEXT("references_before"),Refs);return W.Save();
    };
    const bool Completed=Run();Transaction.Reset();if(W.Stopped||!Completed||!W.Current())return Encode(R);R.Stage=TEXT("generated_creation_terminal");if(!QCFinish(W,After,Refs))return Encode(R);Count(R,TEXT("asset_exists_query_calls"));const bool Exists=UEditorAssetLibrary::DoesAssetExist(W.PathName);if(!W.Current())return Encode(R);After->SetBoolField(TEXT("exists"),Exists);R.Data->SetObjectField(TEXT("after"),After);R.Data->SetBoolField(TEXT("asset_observed"),Exists);auto Delta=MakeShared<FJsonObject>();Delta->SetNumberField(TEXT("affected_count"),1);Delta->SetNumberField(TEXT("created_count"),1);Delta->SetNumberField(TEXT("updated_count"),0);Delta->SetNumberField(TEXT("removed_count"),0);Delta->SetNumberField(TEXT("skipped_count"),0);Delta->SetNumberField(TEXT("failed_count"),Exists?0:1);Contract(R,Before,After,Delta,true,true,true,Exists&&(!Save||R.Saved),TEXT("generated_data_asset_factory_current"),{TEXT("fresh_target"),TEXT("typed_generated_class"),TEXT("source_cdo_defaults"),TEXT("save_gate")});R.Known=true;R.Changed=true;if(!Exists)return Fail(R,TEXT("mutation_postcondition_failed"),TEXT("Actual created asset prefix retained; registry existence was not observed.")),Encode(R);R.Success=R.Post=true;R.Stage=TEXT("complete");return Encode(R);
}

}
namespace
{
// US: concrete native DataAsset lifecycle, retaining the original public3/SDK operations.
// No native default scratch or per-row-type/class whitelist is introduced here.
bool USNativeActive=false;
struct FUSNativeDataAssetWork
{
    static constexpr int32 MaxFields=2048,MaxObjects=4096;
    FResult& R;bool Stopped=false,Bound=false,SourceBindingsReady=false,SourceMetadataReady=false,DefaultsReady=false,TargetOwnedReady=false;
    TStrongObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;TStrongObjectPtr<UClass> Type;TStrongObjectPtr<UDataAsset> Asset;FString Path,PackagePath;
    struct FObjectRole{TStrongObjectPtr<UObject> Hold;UClass* Kind=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;FString Path;EObjectFlags Flags=RF_NoFlags;uint32 PackageFlags=0;bool PackageDirty=false;};
    struct FFieldRole{FProperty* P=nullptr;FFieldClass* Kind=nullptr;UStruct* Owner=nullptr;FProperty* Parent=nullptr;FName Name;int32 Offset=0,Size=0,Element=0,Dim=0,Align=0;uint64 Flags=0;TMap<FName,FString> Metadata;UScriptStruct* Struct=nullptr;UClass* ObjectClass=nullptr;EClassFlags ObjectClassFlags=CLASS_None;UEnum* Enum=nullptr;FNumericProperty* Underlying=nullptr;FProperty* Inner=nullptr;FProperty* Key=nullptr;FProperty* Value=nullptr;uint8 BoolSize=0,BoolOffset=0,BoolMask=0,BoolFieldMask=0;FScriptSetLayout SetLayout;FScriptMapLayout MapLayout;};
    struct FSourceRole{TStrongObjectPtr<UStruct> Hold;UClass* Kind=nullptr;UStruct* Super=nullptr;uint64 Flags=0;int32 Size=0,Align=0;TArray<FProperty*> Members;TMap<FName,FString> Metadata;bool Ready=false,CustomJsonConverter=false;};
    struct FEnumRole{TStrongObjectPtr<UEnum> Hold;UEnum::ECppForm Form=UEnum::ECppForm::Regular;TArray<FName> Names;TArray<int64> Values;};
    struct FDefaultRole{TStrongObjectPtr<UClass> Class;TStrongObjectPtr<UObject> Object;TSharedRef<FJsonObject> Values=MakeShared<FJsonObject>();bool OwnedReady=false;};
    TArray<FObjectRole> Objects;TArray<TUniquePtr<FSourceRole>> Structs;TArray<FFieldRole> Fields;TArray<FEnumRole> Enums;TArray<UStruct*> Active;TArray<FDefaultRole> Defaults;
    TSharedRef<FJsonObject> Expected=MakeShared<FJsonObject>(),SourceBaseline=MakeShared<FJsonObject>(),DiskBaseline=MakeShared<FJsonObject>(),OtherBaseline=MakeShared<FJsonObject>();
    TStrongObjectPtr<UTransBuffer> Transactor;const FTransaction* OwnTransaction=nullptr;FGuid TransactionId,OperationId;FString TransactionContext;int32 TransactionIndex=INDEX_NONE,TransactionDepth=0;bool TransactionReady=false;
    explicit FUSNativeDataAssetWork(FResult& In):R(In){if(IsInGameThread()&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage()){Editor.Reset(GEditor);World.Reset(GWorld);}}
    bool Bad(const TCHAR* Property,const FString& Wanted=TEXT("captured native source/target role"),const FString& Actual=TEXT("changed or unavailable"),bool Unknown=true)
    {if(Stopped)return false;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("object"),Path);J->SetStringField(TEXT("expected"),Wanted);J->SetStringField(TEXT("actual"),Actual);R.Data->SetObjectField(TEXT("native_asset_first_mismatch"),J);Stopped=true;return Fail(R,TEXT("data_asset_native_current_changed"),TEXT("Actual native operation prefix retained; no next SDK/save/retry after current loss."),Unknown);}
    static uint64 SourceFlags(const UStruct* S){return S->GetClass()==UClass::StaticClass()?uint64(static_cast<const UClass*>(S)->ClassFlags):uint64(static_cast<const UScriptStruct*>(S)->StructFlags);}

    bool Context()
    {
        if(Stopped||!IsInGameThread()||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||GIsImportingT3D)return false;
        if(!Loaded(Editor.Get())||GEditor!=Editor.Get()||!Loaded(World.Get())||GWorld!=World.Get()||World->WorldType!=EWorldType::Editor||World->bIsTearingDown)return false;
        if(!Editor->PlayWorld.IsResolved()||Editor->PlayWorld.Get()!=nullptr)return false;
        return !IsPlaySessionActive()&&Editor->GetEditorWorldContext().World()==World.Get();
    }
    bool Pin(UObject* O)
    {
        if(!Context()||!Loaded(O)||!Clean(O->GetPathName())||!Clean(O->GetClass()->GetPathName()))return Bad(TEXT("held_return"));
        for(const auto& E:Objects)if(E.Hold.Get()==O)return true;if(Objects.Num()>=MaxObjects)return Bad(TEXT("held_bound"));
        FObjectRole E;E.Hold.Reset(O);E.Kind=O->GetClass();E.Outer=O->GetOuter();E.Package=O->GetPackage();E.Path=O->GetPathName();E.Flags=O->GetFlags();E.PackageFlags=uint32(E.Package->GetPackageFlags());E.PackageDirty=E.Package->IsDirty();Objects.Add(MoveTemp(E));return true;
    }
    bool OwnedEmpty(UObject* Object)
    {TArray<UObject*> Owned;GetObjectsWithOuter(Object,Owned,EGetObjectsFlags::IncludeNestedObjects);if(!Context())return Bad(TEXT("owned_inventory_context"));return Owned.IsEmpty()||Bad(TEXT("native_owned_export_scope_changed"),TEXT("captured empty owned inventory before any recursive exporter"),LexToString(Owned.Num()));}
    bool RootCurrent(bool CheckDefaultSlots=true)
    {
        if(!Context()||(TransactionReady&&!TransactionCurrent()))return Bad(TEXT("editor_context"));
        for(const auto& E:Objects){auto* O=E.Hold.Get();if(!Loaded(O)||!Loaded(E.Kind)||(E.Outer&&!Loaded(E.Outer))||!Loaded(E.Package)||O->GetClass()!=E.Kind||O->GetOuter()!=E.Outer||O->GetPackage()!=E.Package||O->GetFlags()!=E.Flags||!O->GetPathName().Equals(E.Path,ESearchCase::CaseSensitive))return Bad(TEXT("held_identity"),E.Path);if((!Bound||E.Package!=Asset->GetPackage())&&(uint32(E.Package->GetPackageFlags())!=E.PackageFlags||E.Package->IsDirty()!=E.PackageDirty))return Bad(TEXT("source_package_state"),E.Path);}
        for(const auto& Entry:Structs){const auto& E=*Entry;auto* S=E.Hold.Get();if(!Loaded(S)||S->GetClass()!=E.Kind||SourceFlags(S)!=E.Flags||S->GetSuperStruct()!=E.Super||S->GetStructureSize()!=E.Size||S->GetMinAlignment()!=E.Align)return Bad(TEXT("native_struct_layout"));TArray<FProperty*> Now;for(TFieldIterator<FProperty> It(S,EFieldIterationFlags::IncludeDeprecated);It;++It){if(Now.Num()>=MaxFields)return Bad(TEXT("native_field_bound"));Now.Add(*It);}if(Now!=E.Members)return Bad(TEXT("native_member_inventory"));}
        for(const auto& E:Fields){auto* P=E.P;if(E.Parent){const auto* Parent=Fields.FindByPredicate([&](const FFieldRole& X){return X.P==E.Parent;});if(!Parent)return Bad(TEXT("inner_parent"));bool Found=false;if(Parent->Kind==FArrayProperty::StaticClass())Found=static_cast<FArrayProperty*>(E.Parent)->Inner==P;else if(Parent->Kind==FSetProperty::StaticClass())Found=static_cast<FSetProperty*>(E.Parent)->ElementProp==P;else if(Parent->Kind==FMapProperty::StaticClass())Found=static_cast<FMapProperty*>(E.Parent)->KeyProp==P||static_cast<FMapProperty*>(E.Parent)->ValueProp==P;else if(Parent->Kind==FEnumProperty::StaticClass())Found=static_cast<FEnumProperty*>(E.Parent)->GetUnderlyingProperty()==P;if(!Found)return Bad(TEXT("inner_membership"));}if(P->GetClass()!=E.Kind||P->GetOwnerStruct()!=E.Owner||P->GetOwner<FProperty>()!=E.Parent||P->GetFName()!=E.Name||P->GetOffset_ForInternal()!=E.Offset||P->GetSize()!=E.Size||P->ElementSize!=E.Element||P->ArrayDim!=E.Dim||P->GetMinAlignment()!=E.Align||uint64(P->GetPropertyFlags())!=E.Flags||P->HasSetterOrGetter())return Bad(TEXT("native_field_layout"),E.Name.ToString());if(E.Kind==FBoolProperty::StaticClass()){auto* B=static_cast<FBoolProperty*>(P);if(B->GetBoolFieldSize()!=E.BoolSize||B->GetByteOffset()!=E.BoolOffset||B->GetByteMask()!=E.BoolMask||B->GetFieldMask()!=E.BoolFieldMask)return Bad(TEXT("native_bool_layout"));}const auto* M=P->GetMetaDataMap();if((M?M->Num():0)!=E.Metadata.Num())return Bad(TEXT("native_field_metadata"));for(const auto& Pair:E.Metadata){const auto* V=M?M->Find(Pair.Key):nullptr;if(!V||!V->Equals(Pair.Value,ESearchCase::CaseSensitive))return Bad(TEXT("native_field_metadata"));}}
        if(SourceMetadataReady)for(const auto& Entry:Structs){const auto& E=*Entry;const auto* M=FMetaData::GetMapForObject(E.Hold.Get());if((M?M->Num():0)!=E.Metadata.Num())return Bad(TEXT("native_struct_metadata"));for(const auto& Pair:E.Metadata){const auto* V=M?M->Find(Pair.Key):nullptr;if(!V||!V->Equals(Pair.Value,ESearchCase::CaseSensitive))return Bad(TEXT("native_struct_metadata"));}if((E.Kind==UScriptStruct::StaticClass()&&FJsonObjectStructInterfaceRegistry::HasStructConverterRegistered(static_cast<UScriptStruct*>(E.Hold.Get())))!=E.CustomJsonConverter)return Bad(TEXT("native_json_converter_role"));}
        if(Bound&&(!Loaded(Asset.Get())||Asset->GetClass()!=Type.Get()||Asset->GetOuter()!=Asset->GetPackage()||!Asset->GetPathName().Equals(Path,ESearchCase::CaseSensitive)||!Asset->GetPackage()->GetName().Equals(PackagePath,ESearchCase::CaseSensitive)||FindObject<UDataAsset>(nullptr,*Path)!=Asset.Get()))return Bad(TEXT("native_asset_target_role"));
        if(Bound&&TargetOwnedReady&&!OwnedEmpty(Asset.Get()))return false;
        if(CheckDefaultSlots)for(const auto& D:Defaults){if(!D.Class->ClassDefaultObject.IsResolved()||D.Class->ClassDefaultObject.Get()!=D.Object.Get()||(D.Object.IsValid()&&(!Loaded(D.Object.Get())||D.Object->GetClass()!=D.Class.Get())))return Bad(TEXT("native_default_slot"));if(D.OwnedReady&&D.Object.IsValid()&&!OwnedEmpty(D.Object.Get()))return false;}
        return true;
    }
    bool BindEnum(UEnum* E)
    {
        if(!Loaded(E)||!Pin(E)||!Pin(E->GetPackage())||!RootCurrent())return false;for(const auto& Old:Enums)if(Old.Hold.Get()==E)return true;
        if(E->GetClass()!=UEnum::StaticClass()||E->NumEnums()>1024)return Bad(TEXT("native_enum_role"));FEnumRole Role;Role.Hold.Reset(E);Role.Form=E->GetCppForm();for(int32 I=0;I<E->NumEnums();++I){Role.Names.Add(E->GetNameByIndex(I));Role.Values.Add(E->GetValueByIndex(I));}Enums.Add(MoveTemp(Role));return RootCurrent();
    }
    static bool Kind(FFieldClass* K)
    {return K==FBoolProperty::StaticClass()||K==FInt8Property::StaticClass()||K==FInt16Property::StaticClass()||K==FIntProperty::StaticClass()||K==FInt64Property::StaticClass()||K==FByteProperty::StaticClass()||K==FUInt16Property::StaticClass()||K==FUInt32Property::StaticClass()||K==FUInt64Property::StaticClass()||K==FFloatProperty::StaticClass()||K==FDoubleProperty::StaticClass()||K==FNameProperty::StaticClass()||K==FStrProperty::StaticClass()||K==FTextProperty::StaticClass()||K==FEnumProperty::StaticClass()||K==FStructProperty::StaticClass()||K==FObjectProperty::StaticClass()||K==FArrayProperty::StaticClass()||K==FSetProperty::StaticClass()||K==FMapProperty::StaticClass();}
    bool BindField(FProperty* P,FProperty* Parent=nullptr,int32 Depth=0)
    {
        if(!RootCurrent())return false;if(!P||!Kind(P->GetClass()))return Bad(TEXT("native_field_class_pending"),TEXT("exact native scalar/enum/struct/object/array/set/map descriptor"),P?P->GetClass()->GetName():TEXT("null"),false);
        for(const auto& E:Fields)if(E.P==P)return true;
        if(Depth>16||Fields.Num()>=MaxFields||P->ArrayDim<1||P->ArrayDim>256||P->GetSize()<=0||P->GetMinAlignment()<=0||P->HasSetterOrGetter()||P->HasAnyPropertyFlags(CPF_InstancedReference|CPF_ContainsInstancedReference|CPF_PersistentInstance))return Bad(TEXT("native_field_scope_pending"),TEXT("bounded linked field without accessors or instanced object serialization"),P->GetName(),false);
        FFieldRole E;E.P=P;E.Kind=P->GetClass();E.Owner=P->GetOwnerStruct();E.Parent=P->GetOwner<FProperty>();E.Name=P->GetFName();E.Offset=P->GetOffset_ForInternal();E.Size=P->GetSize();E.Element=P->ElementSize;E.Dim=P->ArrayDim;E.Align=P->GetMinAlignment();E.Flags=uint64(P->GetPropertyFlags());if(const auto* M=P->GetMetaDataMap())E.Metadata=*M;
        if(E.Parent!=Parent||!E.Owner||(!Parent&&(E.Offset<0||int64(E.Offset)+E.Size>E.Owner->GetPropertiesSize())))return Bad(TEXT("native_field_owner_bounds"));
        if(E.Kind==FBoolProperty::StaticClass()){auto* B=static_cast<FBoolProperty*>(P);E.BoolSize=B->GetBoolFieldSize();E.BoolOffset=B->GetByteOffset();E.BoolMask=B->GetByteMask();E.BoolFieldMask=B->GetFieldMask();}
        Fields.Add(E);const int32 Index=Fields.Num()-1;
        if(E.Kind==FStructProperty::StaticClass()){auto* S=static_cast<FStructProperty*>(P);if(!S->Struct.IsResolved())return Bad(TEXT("nested_struct_unresolved"));auto* Returned=S->Struct.Get();TStrongObjectPtr<UScriptStruct> Hold(Returned);if(!RootCurrent()||!Loaded(Returned))return Bad(TEXT("nested_struct_return"));Fields[Index].Struct=Returned;if(!BindStruct(Returned,Depth+1))return false;}
        else if(E.Kind==FObjectProperty::StaticClass()){auto* O=static_cast<FObjectProperty*>(P);if(!O->PropertyClass.IsResolved())return Bad(TEXT("object_class_unresolved"));auto* Returned=O->PropertyClass.Get();TStrongObjectPtr<UClass> Hold(Returned);if(!RootCurrent()||!Loaded(Returned)||!Pin(Returned)||!Pin(Returned->GetPackage()))return false;if(!Returned->HasAnyClassFlags(CLASS_Native)||!Returned->ClassGeneratedBy.IsResolved()||Returned->ClassGeneratedBy.Get()!=nullptr||Returned->HasAnyClassFlags(CLASS_DefaultToInstanced))return Bad(TEXT("object_class_pending"),TEXT("loaded native noninstanced reference class"),Returned->GetPathName(),false);Fields[Index].ObjectClass=Returned;Fields[Index].ObjectClassFlags=Returned->ClassFlags;}
        else if(E.Kind==FEnumProperty::StaticClass()){auto* F=static_cast<FEnumProperty*>(P);auto* Inner=F->GetUnderlyingProperty();if(!Inner||!Kind(Inner->GetClass())||!CastField<FNumericProperty>(Inner))return Bad(TEXT("enum_underlying"));Fields[Index].Underlying=Inner;if(!BindField(Inner,P,Depth+1))return false;}
        else if(E.Kind==FByteProperty::StaticClass()){auto* B=static_cast<FByteProperty*>(P);if(!B->Enum.IsResolved())return Bad(TEXT("byte_enum_unresolved"));Fields[Index].Enum=B->Enum.Get();}
        else if(E.Kind==FArrayProperty::StaticClass()){auto* A=static_cast<FArrayProperty*>(P);if(A->ArrayFlags!=EArrayPropertyFlags::None||P->ElementSize!=sizeof(FScriptArray))return Bad(TEXT("array_allocator_pending"),TEXT("native default allocator"),P->GetName(),false);Fields[Index].Inner=A->Inner;if(!BindField(A->Inner,P,Depth+1))return false;}
        else if(E.Kind==FSetProperty::StaticClass()){auto* S=static_cast<FSetProperty*>(P);Fields[Index].Inner=S->ElementProp;Fields[Index].SetLayout=S->SetLayout;if(P->ElementSize!=sizeof(FScriptSet)||!BindField(S->ElementProp,P,Depth+1))return Bad(TEXT("set_layout_pending"));}
        else if(E.Kind==FMapProperty::StaticClass()){auto* M=static_cast<FMapProperty*>(P);if(M->MapFlags!=EMapPropertyFlags::None||P->ElementSize!=sizeof(FScriptMap))return Bad(TEXT("map_allocator_pending"));Fields[Index].Key=M->KeyProp;Fields[Index].Value=M->ValueProp;Fields[Index].MapLayout=M->MapLayout;if(!BindField(M->KeyProp,P,Depth+1)||!BindField(M->ValueProp,P,Depth+1))return false;}
        return RootCurrent();
    }
    bool BindStruct(UStruct* S,int32 Depth=0)
    {
        if(!RootCurrent())return false;if(Active.Contains(S))return Bad(TEXT("native_recursive_source_pending"),TEXT("finite acyclic reflected source"),TEXT("active cycle"),false);for(const auto& E:Structs)if(E->Hold.Get()==S)return E->Ready;
        if(Depth>16||Structs.Num()>=128||!Loaded(S)||(S->GetClass()!=UClass::StaticClass()&&S->GetClass()!=UScriptStruct::StaticClass())||S->GetOuter()!=S->GetPackage()||!S->GetPackage()->GetName().StartsWith(TEXT("/Script/"))||!S->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||S->GetStructureSize()<=0||S->GetStructureSize()>1048576||S->GetMinAlignment()<=0||!Clean(S->GetPathName()))return Bad(TEXT("native_source_scope_pending"),TEXT("exact loaded compiled-in finite class/scriptstruct"),Loaded(S)?S->GetPathName():TEXT("unavailable"),false);
        if(S->GetClass()==UClass::StaticClass()&&(!static_cast<UClass*>(S)->HasAnyClassFlags(CLASS_Native)||!static_cast<UClass*>(S)->ClassGeneratedBy.IsResolved()||static_cast<UClass*>(S)->ClassGeneratedBy.Get()!=nullptr))return Bad(TEXT("native_class_source_pending"),TEXT("native current declaring class"),S->GetPathName(),false);
        if(!Pin(S)||!Pin(S->GetPackage()))return false;auto Entry=MakeUnique<FSourceRole>();auto* Role=Entry.Get();Role->Hold.Reset(S);Role->Kind=S->GetClass();Role->Super=S->GetSuperStruct();Role->Flags=SourceFlags(S);Role->Size=S->GetStructureSize();Role->Align=S->GetMinAlignment();for(TFieldIterator<FProperty> It(S,EFieldIterationFlags::IncludeDeprecated);It;++It)Role->Members.Add(*It);Structs.Add(MoveTemp(Entry));Active.Add(S);
        if(Role->Super&&!BindStruct(Role->Super,Depth+1))return false;
        if(S->GetClass()==UScriptStruct::StaticClass()&&Role->Members.IsEmpty()&&!Role->Super)return Bad(TEXT("native_opaque_payload_pending"),TEXT("bounded reflected native value payload"),S->GetPathName(),false);const auto Members=Role->Members;for(auto* P:Members)if(!BindField(P,nullptr,Depth+1))return false;Active.Remove(S);Role->Ready=true;return RootCurrent();
    }
    bool Current(bool CheckTarget=true,bool CheckValues=true)
    {
        if(!RootCurrent())return false;
        for(const auto& E:Fields){auto* P=E.P;if(E.Struct){auto* F=static_cast<FStructProperty*>(P);if(!F->Struct.IsResolved()||F->Struct.Get()!=E.Struct)return Bad(TEXT("struct_binding"));}if(E.ObjectClass){auto* F=static_cast<FObjectProperty*>(P);if(!F->PropertyClass.IsResolved()||F->PropertyClass.Get()!=E.ObjectClass||E.ObjectClass->ClassFlags!=E.ObjectClassFlags||!E.ObjectClass->ClassGeneratedBy.IsResolved()||E.ObjectClass->ClassGeneratedBy.Get()!=nullptr)return Bad(TEXT("object_class_binding"));}if(SourceBindingsReady&&E.Kind==FEnumProperty::StaticClass()){auto* F=static_cast<FEnumProperty*>(P);auto* Returned=F->GetEnum();TStrongObjectPtr<UEnum> Hold(Returned);if(!RootCurrent())return false;if(Returned!=E.Enum||F->GetUnderlyingProperty()!=E.Underlying)return Bad(TEXT("enum_binding"));}if(E.Kind==FByteProperty::StaticClass()){auto* F=static_cast<FByteProperty*>(P);if(!F->Enum.IsResolved()||F->Enum.Get()!=E.Enum)return Bad(TEXT("byte_enum_binding"));}if(E.Kind==FArrayProperty::StaticClass()&&(static_cast<FArrayProperty*>(P)->Inner!=E.Inner||static_cast<FArrayProperty*>(P)->ArrayFlags!=EArrayPropertyFlags::None))return Bad(TEXT("array_inner"));if(E.Kind==FSetProperty::StaticClass()&&(static_cast<FSetProperty*>(P)->ElementProp!=E.Inner||!SetLayoutSame(static_cast<FSetProperty*>(P)->SetLayout,E.SetLayout)))return Bad(TEXT("set_element"));if(E.Kind==FMapProperty::StaticClass()&&(static_cast<FMapProperty*>(P)->KeyProp!=E.Key||static_cast<FMapProperty*>(P)->ValueProp!=E.Value||static_cast<FMapProperty*>(P)->MapFlags!=EMapPropertyFlags::None||!MapLayoutSame(static_cast<FMapProperty*>(P)->MapLayout,E.MapLayout)))return Bad(TEXT("map_key_value"));}
        for(const auto& E:Enums){auto* U=E.Hold.Get();if(!Loaded(U)||U->GetCppForm()!=E.Form||U->NumEnums()!=E.Names.Num())return Bad(TEXT("enum_inventory"));for(int32 I=0;I<E.Names.Num();++I)if(U->GetNameByIndex(I)!=E.Names[I]||U->GetValueByIndex(I)!=E.Values[I])return Bad(TEXT("enum_value"));}
        if(CheckValues&&DefaultsReady)for(const auto& D:Defaults)if(D.Object.IsValid()){auto Now=MakeShared<FJsonObject>();if(!TypedObject(D.Object.Get(),D.Class.Get(),Now)||!Equal(D.Values,Now))return Bad(TEXT("native_default_authored_value"),D.Class->GetPathName());}
        if(CheckTarget&&Bound){auto Now=MakeShared<FJsonObject>();if(!TypedObject(Asset.Get(),Type.Get(),Now)||!Equal(Expected,Now))return Bad(TEXT("native_target_authored_value"));}return true;
    }
    bool TypedValue(FProperty* P,const void* At,TSharedPtr<FJsonValue>& Out,int32& Cells,int32 Depth=0)
    {
        if(!At||Depth>16||++Cells>65536)return Bad(TEXT("typed_value_bound"));const auto* E=Fields.FindByPredicate([&](const FFieldRole& V){return V.P==P;});if(!E)return Bad(TEXT("typed_descriptor"));
        if(P->ArrayDim>1){TArray<TSharedPtr<FJsonValue>> A;for(int32 I=0;I<P->ArrayDim;++I){TSharedPtr<FJsonValue> V;if(!TypedScalar(*E,static_cast<const uint8*>(At)+I*P->ElementSize,V,Cells,Depth+1))return false;A.Add(V);}Out=MakeShared<FJsonValueArray>(A);return true;}return TypedScalar(*E,At,Out,Cells,Depth);
    }
    bool TypedScalar(const FFieldRole& E,const void* At,TSharedPtr<FJsonValue>& Out,int32& Cells,int32 Depth)
    {
        if(Depth>16||++Cells>65536)return Bad(TEXT("typed_value_bound"));auto* P=E.P;
        if(E.Kind==FBoolProperty::StaticClass())Out=MakeShared<FJsonValueBoolean>(static_cast<FBoolProperty*>(P)->GetPropertyValue(At));
        else if(E.Kind==FNameProperty::StaticClass())Out=MakeShared<FJsonValueString>(static_cast<FNameProperty*>(P)->GetPropertyValue(At).ToString());
        else if(E.Kind==FStrProperty::StaticClass())Out=MakeShared<FJsonValueString>(static_cast<FStrProperty*>(P)->GetPropertyValue(At));
        else if(E.Kind==FTextProperty::StaticClass()){const FText Text=static_cast<FTextProperty*>(P)->GetPropertyValue(At);Out=MakeShared<FJsonValueString>(LexToString(FTextInspector::GetFlags(Text))+TEXT("|")+LexToString(UPTRINT(FTextInspector::GetSharedDataId(Text))));}
        else if(E.Kind==FEnumProperty::StaticClass())Out=MakeShared<FJsonValueString>(LexToString(E.Underlying->GetSignedIntPropertyValue(At)));
        else if(auto* N=CastField<FNumericProperty>(P)){if(N->IsFloatingPoint()){const double V=N->GetFloatingPointPropertyValue(At);if(!FMath::IsFinite(V))return Bad(TEXT("native_nonfinite_value"));Out=MakeShared<FJsonValueNumber>(V);}else{const bool Unsigned=E.Kind==FByteProperty::StaticClass()||E.Kind==FUInt16Property::StaticClass()||E.Kind==FUInt32Property::StaticClass()||E.Kind==FUInt64Property::StaticClass();Out=MakeShared<FJsonValueString>(Unsigned?LexToString(N->GetUnsignedIntPropertyValue(At)):LexToString(N->GetSignedIntPropertyValue(At)));}}
        else if(E.Kind==FObjectProperty::StaticClass()){const TObjectPtr<UObject> Slot=static_cast<FObjectProperty*>(P)->GetObjectPtrPropertyValue(At);if(!Slot.IsResolved())return Bad(TEXT("typed_object_unresolved"));auto* Ref=Slot.Get();TStrongObjectPtr<UObject> Hold(Ref);if(Ref&&(!Pin(Ref)||!Pin(Ref->GetPackage())))return false;Out=Ref?TSharedPtr<FJsonValue>(MakeShared<FJsonValueString>(Ref->GetPathName()+TEXT("|")+LexToString(UPTRINT(Ref)))):TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>());}
        else if(E.Struct){auto J=MakeShared<FJsonObject>();for(TFieldIterator<FProperty> It(E.Struct);It;++It){if(It->HasAnyPropertyFlags(CPF_Transient))continue;TSharedPtr<FJsonValue> V;if(!TypedValue(*It,It->ContainerPtrToValuePtr<void>(At),V,Cells,Depth+1))return false;J->SetField(It->GetName(),V);}Out=MakeShared<FJsonValueObject>(J);}
        else if(E.Kind==FArrayProperty::StaticClass()){FScriptArrayHelper H(static_cast<FArrayProperty*>(P),At);if(H.Num()<0||H.Num()>4096)return Bad(TEXT("typed_array_bound"));TArray<TSharedPtr<FJsonValue>> A;for(int32 I=0;I<H.Num();++I){TSharedPtr<FJsonValue> V;if(!TypedValue(E.Inner,H.GetRawPtr(I),V,Cells,Depth+1))return false;A.Add(V);}Out=MakeShared<FJsonValueArray>(A);}
        else if(E.Kind==FSetProperty::StaticClass()||E.Kind==FMapProperty::StaticClass()){TArray<FString> Bag;if(E.Kind==FSetProperty::StaticClass()){FScriptSetHelper H(static_cast<FSetProperty*>(P),At);if(H.Num()<0||H.Num()>4096||H.GetMaxIndex()>65536)return Bad(TEXT("typed_set_bound"));for(int32 I=0;I<H.GetMaxIndex();++I)if(H.IsValidIndex(I)){TSharedPtr<FJsonValue> V;if(!TypedValue(E.Inner,H.GetElementPtr(I),V,Cells,Depth+1))return false;FString Text;FJsonSerializer::Serialize(V,TEXT(""),TJsonWriterFactory<>::Create(&Text));Bag.Add(Text);}}else{FScriptMapHelper H(static_cast<FMapProperty*>(P),At);if(H.Num()<0||H.Num()>4096||H.GetMaxIndex()>65536)return Bad(TEXT("typed_map_bound"));for(int32 I=0;I<H.GetMaxIndex();++I)if(H.IsValidIndex(I)){TSharedPtr<FJsonValue> K,V;if(!TypedValue(E.Key,H.GetKeyPtr(I),K,Cells,Depth+1)||!TypedValue(E.Value,H.GetValuePtr(I),V,Cells,Depth+1))return false;FString Text;FJsonSerializer::Serialize(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{K,V}),TEXT(""),TJsonWriterFactory<>::Create(&Text));Bag.Add(Text);}}Bag.Sort();TArray<TSharedPtr<FJsonValue>> A;for(const auto& Text:Bag)A.Add(MakeShared<FJsonValueString>(Text));Out=MakeShared<FJsonValueArray>(A);}
        else return Bad(TEXT("typed_value_kind"));return true;
    }
    bool TypedObject(const void* At,UStruct* ObjectType,const TSharedRef<FJsonObject>& Out)
    {if(!RootCurrent()||!At)return false;int32 Cells=0;for(TFieldIterator<FProperty> It(ObjectType);It;++It){if(It->HasAnyPropertyFlags(CPF_Transient))continue;TSharedPtr<FJsonValue> V;if(!TypedValue(*It,It->ContainerPtrToValuePtr<void>(At),V,Cells))return false;Out->SetField(It->GetName(),V);}return RootCurrent();}
    bool TargetFlagsAfter(bool Saving=false)
    {
        if(!Context())return Bad(TEXT("returned_context"));for(auto& E:Objects){auto* O=E.Hold.Get();if(!Loaded(O))return Bad(TEXT("returned_object"));if(O->GetFlags()==E.Flags)continue;const uint32 A=uint32(E.Flags),B=uint32(O->GetFlags()),LoadedBits=uint32(RF_WasLoaded|RF_LoadCompleted);const bool Owned=Bound&&O->GetPackage()==Asset->GetPackage();const bool PackageTransaction=Owned&&O==Asset->GetPackage()&&GUndo&&O->GetPackage()->IsDirty()&&!(A&uint32(RF_Transactional))&&B==(A|uint32(RF_Transactional));const bool Saved=Saving&&Owned&&!(A&~B)&&!((B&~A)&~LoadedBits);if(!PackageTransaction&&!Saved)return Bad(TEXT("unexpected_object_flags"),LexToString(A),LexToString(B));E.Flags=O->GetFlags();}return Current(false);
    }
    bool TransactionCurrent()
    {if(!TransactionReady)return true;if(!Context()||!Loaded(Transactor.Get())||!Editor->Trans.IsResolved()||Editor->Trans.Get()!=Transactor.Get()||Transactor->GetClass()!=UTransBuffer::StaticClass()||Transactor->ActiveCount!=TransactionDepth||Transactor->UndoCount||Transactor->UndoBuffer.IsEmpty()||&Transactor->UndoBuffer.Last().Get()!=OwnTransaction)return false;const auto C=OwnTransaction->GetContext();return C.TransactionId==TransactionId&&C.OperationId==OperationId&&C.Context==TransactionContext;}
    bool BeginTransaction()
    {
        if(!Current())return false;const bool Can=Editor->CanTransact();if(!Current())return false;if(!Can||GIsTransacting)return true;if(!Editor->Trans.IsResolved())return Bad(TEXT("transaction_slot"));auto* T=Editor->Trans.Get();TStrongObjectPtr<UTransactor> Hold(T);if(!Current()||!Loaded(T)||T->GetClass()!=UTransBuffer::StaticClass())return Bad(TEXT("transaction_class_pending"),TEXT("native UTransBuffer"),TEXT("custom or unavailable"),false);Transactor.Reset(static_cast<UTransBuffer*>(T));const int32 Before=Transactor->ActiveCount;if(Before<0||Before==MAX_int32||(Before&&(Transactor->UndoCount||Transactor->UndoBuffer.IsEmpty())))return Bad(TEXT("transaction_depth"));const FTransaction* Prior=Before?&Transactor->UndoBuffer.Last().Get():nullptr;FGuid PriorId,PriorOperation;if(Prior){const auto C=Prior->GetContext();TransactionContext=C.Context;PriorId=C.TransactionId;PriorOperation=C.OperationId;}else TransactionContext=TEXT("CubelessUS_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
        Count(R,TEXT("outer_transaction_calls"));TransactionIndex=Editor->BeginTransaction(*TransactionContext,FText::FromString(TEXT("DataAsset native operation")),Asset.Get());R.Data->SetNumberField(TEXT("native_asset_transaction_begin_index"),TransactionIndex);
        if(!Context()||!Editor->Trans.IsResolved()||Editor->Trans.Get()!=Transactor.Get()||!Loaded(Transactor.Get())||TransactionIndex!=Before||Transactor->ActiveCount!=Before+1||Transactor->UndoCount||Transactor->UndoBuffer.IsEmpty())return Bad(TEXT("transaction_begin_return"));OwnTransaction=&Transactor->UndoBuffer.Last().Get();const auto C=OwnTransaction->GetContext();if(C.Context!=TransactionContext||(Prior&&(OwnTransaction!=Prior||C.TransactionId!=PriorId||C.OperationId!=PriorOperation)))return Bad(TEXT("transaction_begin_identity"));TransactionId=C.TransactionId;OperationId=C.OperationId;TransactionDepth=Before+1;TransactionReady=true;return Current();
    }
    bool EndTransaction(bool ReadEndpoint=true)
    {
        if(TransactionIndex==INDEX_NONE)return !Stopped;if(Stopped||!TransactionCurrent()){R.Data->SetBoolField(TEXT("native_asset_transaction_end_skipped_unknown"),true);return Bad(TEXT("transaction_end_current"));}const int32 Before=TransactionIndex;const int32 Returned=Editor->EndTransaction();R.Data->SetNumberField(TEXT("native_asset_transaction_end_depth"),Returned);TransactionIndex=INDEX_NONE;TransactionReady=false;if(!Context()||!Loaded(Transactor.Get())||!Editor->Trans.IsResolved()||Editor->Trans.Get()!=Transactor.Get()||Returned!=Before+1||Transactor->ActiveCount!=Before)return Bad(TEXT("transaction_end_return"));if(Before){if(Transactor->UndoBuffer.IsEmpty()||&Transactor->UndoBuffer.Last().Get()!=OwnTransaction)return Bad(TEXT("transaction_parent_identity"));const auto C=OwnTransaction->GetContext();if(C.TransactionId!=TransactionId||C.OperationId!=OperationId||C.Context!=TransactionContext)return Bad(TEXT("transaction_parent_context"));}return !ReadEndpoint||Current(true,true);
    }
    static bool SetLayoutSame(const FScriptSetLayout& A,const FScriptSetLayout& B)
    {
#if UE_USE_COMPACT_SET_AS_DEFAULT
        return A.Size==B.Size&&A.Alignment==B.Alignment;
#else
        return A.Size==B.Size&&A.HashNextIdOffset==B.HashNextIdOffset&&A.HashIndexOffset==B.HashIndexOffset&&A.SparseArrayLayout.Size==B.SparseArrayLayout.Size&&A.SparseArrayLayout.Alignment==B.SparseArrayLayout.Alignment;
#endif
    }
    static bool MapLayoutSame(const FScriptMapLayout& A,const FScriptMapLayout& B)
    {return A.ValueOffset==B.ValueOffset&&SetLayoutSame(A.SetLayout,B.SetLayout);}
    bool AfterAssetFactoryReturn()
    {
        // NewObject may create a previously-null native CDO. This one boundary
        // admits only that exact compiled-in class slot; prior CDO values remain strict.
        if(!RootCurrent(false))return false;TArray<int32> Born;
        for(int32 I=0;I<Defaults.Num();++I){auto& D=Defaults[I];if(!D.Class->ClassDefaultObject.IsResolved())return Bad(TEXT("native_factory_default_unresolved"));UObject* Returned=D.Class->ClassDefaultObject.Get();TStrongObjectPtr<UObject> Hold(Returned);if(!RootCurrent(false))return false;if(D.Object.IsValid()){if(Returned!=D.Object.Get())return Bad(TEXT("native_factory_prior_default_replaced"));}else if(Returned){if(!Loaded(Returned)||Returned->GetClass()!=D.Class.Get()||Returned->GetOuter()!=D.Class->GetPackage()||!Returned->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject))return Bad(TEXT("native_factory_new_default_role"));D.Object.Reset(Returned);if(!Pin(Returned))return false;Born.Add(I);}}
        TArray<TSharedPtr<FJsonValue>> Created;for(int32 I:Born){auto& D=Defaults[I];TArray<UObject*> Owned;GetObjectsWithOuter(D.Object.Get(),Owned,EGetObjectsFlags::IncludeNestedObjects);if(!RootCurrent())return false;if(!Owned.IsEmpty())return Bad(TEXT("native_factory_default_owned_pending"));D.OwnedReady=true;D.Values=MakeShared<FJsonObject>();if(!TypedObject(D.Object.Get(),D.Class.Get(),D.Values))return false;Created.Add(MakeShared<FJsonValueString>(D.Object->GetPathName()));}R.Data->SetArrayField(TEXT("native_factory_created_default_cache_roles"),Created);return Current();
    }
    bool BindAsset(UDataAsset* A,const FString& InPath)
    {
        if(!RootCurrent()||!Loaded(A)||A->GetClass()!=Type.Get()||A->GetOuter()!=A->GetPackage()||!A->GetPathName().Equals(InPath,ESearchCase::CaseSensitive))return Bad(TEXT("native_asset_binding"));
        Asset.Reset(A);Path=InPath;PackagePath=A->GetPackage()->GetName();Bound=true;if(!Pin(A)||!Pin(A->GetPackage()))return false;
        TArray<UObject*> Children;GetObjectsWithOuter(A,Children,EGetObjectsFlags::IncludeNestedObjects);if(!RootCurrent())return false;if(!Children.IsEmpty())return Bad(TEXT("native_asset_owned_scope_pending"),TEXT("noninstanced value/reference payload; owned object authoring remains pending"),LexToString(Children.Num()),false);
        TargetOwnedReady=true;Expected=MakeShared<FJsonObject>();if(!TypedObject(A,Type.Get(),Expected))return false;return Current();
    }
    bool Start(UClass* K,UDataAsset* Existing=nullptr,const FString& InPath=FString())
    {
        Type.Reset(K);if(!Context()||!Loaded(K)||K->GetClass()!=UClass::StaticClass()||!K->HasAnyClassFlags(CLASS_Native)||!K->ClassGeneratedBy.IsResolved()||K->ClassGeneratedBy.Get()!=nullptr||K->HasAnyClassFlags(CLASS_Abstract)||!K->IsChildOf(UDataAsset::StaticClass()))return Bad(TEXT("native_data_asset_class_pending"),TEXT("concrete native UDataAsset subclass"),Loaded(K)?K->GetPathName():TEXT("unavailable"),false);
        if(!BindStruct(K))return false;
        // Fix target authored values before any source metadata/enum observer can resolve.
        if(Existing&&!BindAsset(Existing,InPath))return false;
        for(const auto& Source:Structs)if(Source->Kind==UClass::StaticClass())
        {
            auto* Class=static_cast<UClass*>(Source->Hold.Get());if(!Class->ClassDefaultObject.IsResolved())return Bad(TEXT("native_default_unresolved"));UObject* Returned=Class->ClassDefaultObject.Get();TStrongObjectPtr<UObject> Hold(Returned);if(!RootCurrent())return false;
            FDefaultRole D;D.Class.Reset(Class);D.Object.Reset(Returned);if(Returned){if(!Loaded(Returned)||Returned->GetClass()!=Class||!Returned->HasAnyFlags(RF_ClassDefaultObject)||!Pin(Returned))return Bad(TEXT("native_default_role"));TArray<UObject*> Owned;GetObjectsWithOuter(Returned,Owned,EGetObjectsFlags::IncludeNestedObjects);if(!RootCurrent())return false;if(!Owned.IsEmpty())return Bad(TEXT("native_default_owned_scope_pending"),TEXT("no opaque default-owned subobjects in this value profile"),LexToString(Owned.Num()),false);D.OwnedReady=true;if(!TypedObject(Returned,Class,D.Values))return false;}Defaults.Add(MoveTemp(D));
        }
        DefaultsReady=true;if(!Current())return false;
        for(auto& Source:Structs){if(const auto* M=FMetaData::GetMapForObject(Source->Hold.Get()))Source->Metadata=*M;if(!RootCurrent())return false;Source->CustomJsonConverter=Source->Kind==UScriptStruct::StaticClass()&&FJsonObjectStructInterfaceRegistry::HasStructConverterRegistered(static_cast<UScriptStruct*>(Source->Hold.Get()));if(!RootCurrent())return false;}SourceMetadataReady=true;if(!Current())return false;
        for(int32 I=0;I<Fields.Num();++I){const auto FieldKind=Fields[I].Kind;if(FieldKind==FEnumProperty::StaticClass()){UEnum* Returned=static_cast<FEnumProperty*>(Fields[I].P)->GetEnum();TStrongObjectPtr<UEnum> Hold(Returned);if(!RootCurrent())return false;Fields[I].Enum=Returned;if(!BindEnum(Returned))return false;}else if(FieldKind==FByteProperty::StaticClass()&&Fields[I].Enum&&!BindEnum(Fields[I].Enum))return false;if(!Current())return false;}SourceBindingsReady=true;if(!Current())return false;
        R.Data->SetBoolField(TEXT("generic_native_data_asset_profile"),true);R.Data->SetStringField(TEXT("supported_class"),K->GetPathName());R.Data->SetStringField(TEXT("description_cook_scope"),TEXT("Actual field flags govern Editor-only versus runtime authored values; no blanket Description-only or cooking/runtime proof for generic native classes."));R.Data->SetStringField(TEXT("admission_scope"),TEXT("Concrete native UDataAsset with actual declaring layouts, finite reflected native value/container fields, nullable loaded native references and existing nullable native CDOs. Original custom native constructor/import/export/PostEdit/PreSave remain whole SDK boundaries; no default scratch, arbitrary owned object writer, global-purity or runtime completion proof."));return Source(SourceBaseline);
    }
    bool Source(const TSharedRef<FJsonObject>& Out)
    {
        if(!Current())return false;Out->SetObjectField(TEXT("class"),Identity(Type.Get()));TArray<TSharedPtr<FJsonValue>> DefaultRows;for(const auto& D:Defaults){auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("class"),D.Class->GetPathName());Row->SetObjectField(TEXT("existing_default"),Identity(D.Object.Get()));Row->SetObjectField(TEXT("typed_authored_values"),D.Values);DefaultRows.Add(MakeShared<FJsonValueObject>(Row));}Out->SetArrayField(TEXT("default_roles"),DefaultRows);
        TArray<TSharedPtr<FJsonValue>> Descriptors;for(const auto& F:Fields){auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),F.Name.ToString());Row->SetStringField(TEXT("owner"),F.Owner->GetPathName());Row->SetStringField(TEXT("kind"),F.Kind->GetName());Row->SetNumberField(TEXT("offset"),F.Offset);Row->SetNumberField(TEXT("size"),F.Size);Row->SetStringField(TEXT("flags"),LexToString(F.Flags));Descriptors.Add(MakeShared<FJsonValueObject>(Row));}Out->SetArrayField(TEXT("linked_descriptors"),Descriptors);return Current();
    }
    bool FullJson(const TSharedRef<FJsonObject>& Out)
    {
        if(!Current())return false;Count(R,TEXT("json_export_calls"));const bool Returned=FJsonObjectConverter::UStructToJsonObject(Type.Get(),Asset.Get(),Out,0,0);R.Data->SetBoolField(TEXT("native_asset_last_export_returned"),Returned);if(!Current())return false;int32 CountValues=0,Chars=0;if(!Returned||!Bounded(MakeShared<FJsonValueObject>(Out),CountValues,Chars))return Bad(TEXT("native_asset_export_failed"));return true;
    }
    bool Snapshot(const TSharedRef<FJsonObject>& Out,bool Export=true)
    {
        if(!Current())return false;Count(R,TEXT("observation_capture_calls"));Out->SetObjectField(TEXT("identity"),Identity(Asset.Get()));Out->SetBoolField(TEXT("package_dirty"),Asset->GetPackage()->IsDirty());auto Values=MakeShared<FJsonObject>();Out->SetObjectField(TEXT("values"),Values);if(Export&&!FullJson(Values))return false;Out->SetObjectField(TEXT("typed_authored_values"),Expected);return Current();
    }
    bool AcceptSelected(FName Selected)
    {
        // Only the just-returned converter/notification's selected authored field may change.
        if(!Current(false))return false;auto Actual=MakeShared<FJsonObject>();if(!TypedObject(Asset.Get(),Type.Get(),Actual)||!Current(false))return false;if(Expected->Values.Num()!=Actual->Values.Num())return Bad(TEXT("native_target_value_inventory"));
        for(const auto& Pair:Expected->Values){if(FName(*FString(Pair.Key.ToView()))==Selected)continue;if(!Equal(Pair.Value,Actual->TryGetField(Pair.Key.ToView())))return Bad(TEXT("native_target_unselected_value"),FString(Pair.Key.ToView()));}Expected=Actual;R.Data->SetObjectField(TEXT("native_actual_typed_prefix"),Expected);return Current();
    }
    bool CaptureFiles()
    {if(!Current())return false;return Disk(PackagePath,DiskBaseline)&&Current()&&Dirty(PackagePath,OtherBaseline)&&Current();}
    bool Files(bool Saved=false)
    {
        if(!Current())return false;auto File=MakeShared<FJsonObject>(),Other=MakeShared<FJsonObject>();if(!Disk(PackagePath,File)||!Current()||!Dirty(PackagePath,Other)||!Current())return Bad(TEXT("native_asset_file_observation"));R.Data->SetObjectField(TEXT("native_asset_disk_after"),File);R.Data->SetObjectField(TEXT("native_asset_other_dirty_after"),Other);const bool Pass=(Saved?(File->GetBoolField(TEXT("exists"))&&File->GetNumberField(TEXT("bytes"))>0&&!Asset->GetPackage()->IsDirty()):Equal(DiskBaseline,File))&&Equal(OtherBaseline,Other);R.Data->SetBoolField(TEXT("other_package_dirty_preserved"),Equal(OtherBaseline,Other));return Pass||Bad(TEXT("native_asset_disk_or_other_dirty"));
    }
    bool Save()
    {
        R.Data->SetBoolField(TEXT("save_requested"),R.SaveRequested);if(!R.SaveRequested)return Files();if(!Current()||!Files())return false;
        if(!Asset->GetPackage()->IsFullyLoaded())return Bad(TEXT("native_asset_save_pending"),TEXT("fully loaded current target package"),TEXT("not fully loaded"),false);
        TArray<UObject*> PackageObjects;GetObjectsWithPackage(Asset->GetPackage(),PackageObjects,false);if(!Current())return false;for(UObject* O:PackageObjects)if(O!=Asset.Get()&&(!Loaded(O)||O->GetClass()->GetPathName()!=TEXT("/Script/CoreUObject.MetaData")))return Bad(TEXT("native_asset_foreign_save_export_pending"));
        R.SaveAttempted=true;Count(R,TEXT("save_attempt_count"));auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("asset_path"),Path);Receipt->SetBoolField(TEXT("only_if_dirty"),false);R.Data->SetArrayField(TEXT("save_calls"),{MakeShared<FJsonValueObject>(Receipt)});const bool Returned=UEditorAssetLibrary::SaveLoadedAsset(Asset.Get(),false);Receipt->SetBoolField(TEXT("returned"),true);Receipt->SetBoolField(TEXT("success"),Returned);R.Saved=Returned;R.Data->SetBoolField(TEXT("saved"),Returned);
        if(!Returned){R.Data->SetBoolField(TEXT("native_save_false_getters_suppressed"),true);return Fail(R,TEXT("data_asset_save_failed"),TEXT("Actual save false prefix retained; no asset/source getter, dirty clear or retry. Only own sealed transaction cleanup may run."),true);}if(!TargetFlagsAfter(true)||!Current()||!Files(true))return false;R.Data->SetBoolField(TEXT("persistence_verified"),true);return true;
    }
    bool InputBounded(const TSharedPtr<FJsonObject>& Requested)
    {
        if(!Current()||!Requested)return false;int32 Cells=0;
        for(const auto& Pair:Requested->Values){const FString Name(Pair.Key.ToView());if(Name.Len()>=NAME_SIZE||HasNul(Name))continue;FProperty* P=Type->FindPropertyByName(FName(*Name));if(!Current())return false;if(!P)continue;const auto* E=Fields.FindByPredicate([&](const FFieldRole& F){return F.P==P;});if(!E)return Bad(TEXT("native_input_descriptor"));if(P->HasAnyPropertyFlags(CPF_Transient))return Bad(TEXT("native_transient_selected_scope_pending"),TEXT("selected authored value field; transient cache writes remain valid pending"),Name,false);if(!ImportValue(*E,Pair.Value,Cells,0))return false;}return Current();
    }
    bool ImportFields(UStruct* S,const TSharedPtr<FJsonObject>& Requested,int32& Cells,int32 Depth=0)
    {
        if(!Requested||Depth>16||++Cells>16384)return Bad(TEXT("native_input_bound"));for(TFieldIterator<FProperty> It(S);It;++It){const auto* E=Fields.FindByPredicate([&](const FFieldRole& V){return V.P==*It;});if(!E)return Bad(TEXT("native_input_descriptor"));const FString Name=It->GetAuthoredName();if(!Current())return false;const auto V=Requested->TryGetField(Name);if(!V||V->Type==EJson::Null)continue;if(!ImportValue(*E,V,Cells,Depth+1))return false;}return true;
    }
    bool ImportValue(const FFieldRole& E,const TSharedPtr<FJsonValue>& V,int32& Cells,int32 Depth)
    {
        if(!V||Depth>16||Cells>16384)return Bad(TEXT("native_input_bound"));
        const bool Container=(E.Kind==FArrayProperty::StaticClass()||E.Kind==FSetProperty::StaticClass())&&E.Dim==1;
        if(V->Type==EJson::Array&&!Container){const auto& Items=V->AsArray();const int32 ToRead=FMath::Clamp(Items.Num(),0,E.Dim);for(int32 I=0;I<ToRead;++I)if(!ImportScalar(E,Items[I],Cells,Depth+1))return false;return true;}
        return ImportScalar(E,V,Cells,Depth);
    }
    bool ImportScalar(const FFieldRole& E,const TSharedPtr<FJsonValue>& V,int32& Cells,int32 Depth)
    {
        if(!V||Depth>16||++Cells>16384)return Bad(TEXT("native_input_bound"));if((E.Kind==FNameProperty::StaticClass()||E.Enum)&&V->Type==EJson::String){const FString Text=V->AsString();bool Nul=false;for(TCHAR C:Text)Nul|=C==TCHAR('\0');if(Text.Len()>=NAME_SIZE||Nul)return Bad(TEXT("native_name_input_bound"),TEXT("safe FName/enum text"),TEXT("too long or nul"),false);}if(E.Kind==FObjectProperty::StaticClass()&&V->Type==EJson::String&&!Clean(V->AsString()))return Bad(TEXT("native_reference_forbidden"),TEXT("runtime-independent native asset reference"),V->AsString(),false);
        if(E.Struct&&V->Type==EJson::Object)return ImportFields(E.Struct,V->AsObject(),Cells,Depth+1);if((E.Kind==FArrayProperty::StaticClass()||E.Kind==FSetProperty::StaticClass())&&V->Type==EJson::Array){const auto* Inner=Fields.FindByPredicate([&](const FFieldRole& F){return F.P==E.Inner;});if(!Inner||V->AsArray().Num()>4096)return Bad(TEXT("native_input_array_bound"));for(const auto& Child:V->AsArray())if(!ImportValue(*Inner,Child,Cells,Depth+1))return false;}if(E.Kind==FMapProperty::StaticClass()&&V->Type==EJson::Object){const auto* Key=Fields.FindByPredicate([&](const FFieldRole& F){return F.P==E.Key;});const auto* Value=Fields.FindByPredicate([&](const FFieldRole& F){return F.P==E.Value;});if(!Key||!Value||V->AsObject()->Values.Num()>4096)return Bad(TEXT("native_input_map_bound"));for(const auto& Child:V->AsObject()->Values){if(!ImportValue(*Key,MakeShared<FJsonValueString>(FString(Child.Key.ToView())),Cells,Depth+1)||!ImportValue(*Value,Child.Value,Cells,Depth+1))return false;}}return true;
    }
};
bool USNativePrimaryClass(const FString& Requested)
{return Requested.StartsWith(TEXT("/Script/"))&&Requested!=ClassPath&&!Requested.Equals(PreviewClassPath,ESearchCase::IgnoreCase)&&Requested!=InputActionClassPath&&Requested!=MappingContextClassPath;}
FString USFinish(FUSNativeDataAssetWork& W)
{
    const bool KnownEndpoint=W.R.Known&&!W.Stopped;auto Endpoint=MakeShared<FJsonObject>();bool BeforeClosed=true;if(KnownEndpoint)BeforeClosed=W.Snapshot(Endpoint,false)&&W.Files(W.R.Saved);
    bool Closed=BeforeClosed;if(W.TransactionIndex!=INDEX_NONE)Closed=W.EndTransaction(KnownEndpoint&&BeforeClosed)&&Closed;
    if(KnownEndpoint&&Closed){auto After=MakeShared<FJsonObject>();Closed=W.Snapshot(After,false)&&W.Files(W.R.Saved)&&Equal(Endpoint,After);if(!Closed&&!W.Stopped)W.Bad(TEXT("native_transaction_endpoint_changed"));}
    if(!Closed||W.Stopped){W.R.Success=W.R.Post=false;if(W.R.Attempted)W.R.Known=false;for(const TCHAR* K:{TEXT("source_preserved"),TEXT("references_preserved"),TEXT("unselected_state_preserved"),TEXT("persistence_verified")})W.R.Data->SetBoolField(K,false);}W.R.Data->SetBoolField(TEXT("native_transaction_endpoint_verified"),KnownEndpoint&&Closed&&!W.Stopped);return Encode(W.R);
}
bool USReload(FUSNativeDataAssetWork& W)
{
    if(!W.Current())return false;auto Call=MakeShared<FJsonObject>();Call->SetStringField(TEXT("asset_path"),W.Path);Call->SetStringField(TEXT("stage"),TEXT("legacy_get_before_load"));Call->SetStringField(TEXT("route"),TEXT("UEditorAssetLibrary::LoadAsset"));Call->SetBoolField(TEXT("attempted"),true);auto Calls=W.R.Data->GetArrayField(TEXT("asset_load_calls"));Calls.Add(MakeShared<FJsonValueObject>(Call));W.R.Data->SetArrayField(TEXT("asset_load_calls"),Calls);Count(W.R,TEXT("asset_load_count"));UObject* Returned=UEditorAssetLibrary::LoadAsset(W.Path);TStrongObjectPtr<UObject> Hold(Returned);Call->SetBoolField(TEXT("returned"),true);Call->SetBoolField(TEXT("returned_nonnull"),Returned!=nullptr);if(!W.Current())return false;return Returned==W.Asset.Get()||W.Bad(TEXT("native_asset_original_reload_changed"));
}
TSharedRef<FJsonObject> USLegacyGet(FUSNativeDataAssetWork& W,const TSharedRef<FJsonObject>& Snapshot)
{auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),true);J->SetStringField(TEXT("data_asset"),W.Path);J->SetStringField(TEXT("class"),W.Type->GetPathName());J->SetObjectField(TEXT("values"),Snapshot->GetObjectField(TEXT("values")).ToSharedRef());return J;}
FString USGet(FResult& R,FContext& C)
{
    TGuardValue<bool> Active(USNativeActive,true);FUSNativeDataAssetWork W(R);if(!GeneratedLoadContextCurrent(C)||!W.Start(C.Class,C.Asset.Get(),C.Path)||!W.CaptureFiles())return Encode(R);R.Data->SetObjectField(TEXT("native_source_before"),W.SourceBaseline);auto Before=MakeShared<FJsonObject>(),After=MakeShared<FJsonObject>();Count(R,TEXT("legacy_get_calls"));if(!W.Snapshot(Before)||!W.Snapshot(After)||!W.Files())return Encode(R);
    R.Data->SetObjectField(TEXT("before_observation"),Before);R.Data->SetObjectField(TEXT("after_observation"),After);R.Data->SetStringField(TEXT("class"),W.Type->GetPathName());R.Data->SetObjectField(TEXT("values"),Before->GetObjectField(TEXT("values")).ToSharedRef());if(!Equal(Before,After)){W.Bad(TEXT("native_asset_read_changed"));return Encode(R);}R.Known=true;R.Changed=false;R.Success=R.Post=true;R.Stage=TEXT("complete");for(const TCHAR* K:{TEXT("observations_complete"),TEXT("source_preserved"),TEXT("references_preserved"),TEXT("unselected_state_preserved")})R.Data->SetBoolField(K,true);return Encode(R);
}
FString USSet(FResult& R,FContext& C,const TSharedRef<FJsonObject>& Requested)
{
    TGuardValue<bool> Active(USNativeActive,true);FUSNativeDataAssetWork W(R);if(!GeneratedLoadContextCurrent(C)||!W.Start(C.Class,C.Asset.Get(),C.Path)||!W.InputBounded(Requested)||!W.CaptureFiles())return Encode(R);R.Data->SetObjectField(TEXT("native_source_before"),W.SourceBaseline);const auto OriginalTyped=W.Expected;auto Before=MakeShared<FJsonObject>(),After=MakeShared<FJsonObject>();Count(R,TEXT("legacy_get_calls"));if(!USReload(W)||!W.Snapshot(Before))return Encode(R);R.Data->SetObjectField(TEXT("before"),USLegacyGet(W,Before));R.Data->SetObjectField(TEXT("before_observation"),Before);R.Data->SetStringField(TEXT("class"),W.Type->GetPathName());
    TArray<TSharedPtr<FJsonValue>> Applied,Failed,Attempts,Notifications;TArray<FName> NotifyNames;TSet<FString> AppliedNames;TArray<TPair<FString,TSharedPtr<FJsonValue>>> Inputs;for(const auto& Pair:Requested->Values)Inputs.Emplace(FString(Pair.Key.ToView()),Pair.Value);
    auto Run=[&]()
    {
        R.Attempted=true;R.Known=false;R.Stage=TEXT("apply_properties");if(!W.BeginTransaction()||!W.Current())return;Count(R,TEXT("explicit_modify_calls"));const bool Modified=W.Asset->Modify();R.Data->SetBoolField(TEXT("native_modify_returned"),Modified);if(!W.TargetFlagsAfter()||!W.Current())return;
        for(const auto& Pair:Inputs)
        {
            if(!W.Current())return;const FString Name=Pair.Key;auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("property"),Name);Row->SetBoolField(TEXT("conversion_attempted"),false);Attempts.Add(MakeShared<FJsonValueObject>(Row));R.Data->SetArrayField(TEXT("property_attempts"),Attempts);FProperty* P=Name.Len()<NAME_SIZE&&!HasNul(Name)?W.Type->FindPropertyByName(FName(*Name)):nullptr;if(!W.Current())return;FString Error;
            if(!P)Error=TEXT("Property not found");else
            {
                if(!W.Fields.ContainsByPredicate([&](const FUSNativeDataAssetWork::FFieldRole& F){return F.P==P;})){W.Bad(TEXT("native_selected_descriptor"));return;}const FName Selected=P->GetFName();void* Storage=P->ContainerPtrToValuePtr<void>(W.Asset.Get());Row->SetBoolField(TEXT("conversion_attempted"),true);Count(R,TEXT("json_conversion_calls"));const bool Converted=FJsonObjectConverter::JsonValueToUProperty(Pair.Value,P,Storage,0,0);Row->SetBoolField(TEXT("conversion_returned"),Converted);
                // The converter writes the actual field, including any prefix on false.
                if(!W.AcceptSelected(Selected))return;if(Converted){Applied.Add(MakeShared<FJsonValueString>(Name));AppliedNames.Add(Name);NotifyNames.Add(Selected);}else Error=TEXT("Value conversion failed (check type)");
            }
            if(!Error.IsEmpty()){auto Failure=MakeShared<FJsonObject>();Failure->SetStringField(TEXT("property"),Name);Failure->SetStringField(TEXT("error"),Error);Failed.Add(MakeShared<FJsonValueObject>(Failure));Row->SetStringField(TEXT("error"),Error);}R.Data->SetArrayField(TEXT("applied"),Applied);R.Data->SetArrayField(TEXT("failed"),Failed);
        }
        for(FName Selected:NotifyNames)
        {
            if(!W.Current())return;FProperty* P=W.Type->FindPropertyByName(Selected);if(!W.Current())return;if(!P||!W.Fields.ContainsByPredicate([&](const FUSNativeDataAssetWork::FFieldRole& F){return F.P==P;})){W.Bad(TEXT("native_notification_descriptor"));return;}auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("property"),Selected.ToString());Receipt->SetBoolField(TEXT("attempted"),true);Notifications.Add(MakeShared<FJsonValueObject>(Receipt));R.Data->SetArrayField(TEXT("property_notifications"),Notifications);FPropertyChangedEvent Event(P);Count(R,TEXT("post_edit_change_property_calls"));W.Asset->PostEditChangeProperty(Event);Receipt->SetBoolField(TEXT("returned"),true);if(!W.AcceptSelected(Selected))return;
        }
        if(!Applied.IsEmpty()){if(!W.Current())return;Count(R,TEXT("mark_package_dirty_calls"));const bool Marked=W.Asset->MarkPackageDirty();R.Data->SetBoolField(TEXT("native_mark_dirty_returned"),Marked);if(!W.TargetFlagsAfter()||!W.Current())return;}
        auto Configured=MakeShared<FJsonObject>();if(!W.Snapshot(Configured)||!W.Files())return;R.Data->SetObjectField(TEXT("after_configuration"),Configured);
        // Preserve original known individual-failure continuation and requested save.
        if(!W.Save())return;if(!W.Snapshot(After)||!W.Files(R.Saved))return;R.Data->SetObjectField(TEXT("after_observation"),After);R.Data->SetObjectField(TEXT("after"),USLegacyGet(W,After));
        int32 Updated=0,Skipped=0,PropertyFailure=0;TArray<TSharedPtr<FJsonValue>> Results;
        for(const auto& Pair:Inputs){const FString Name=Pair.Key;const auto BeforeValue=ValueByName(Before->GetObjectField(TEXT("values")).ToSharedRef(),Name),AfterValue=ValueByName(After->GetObjectField(TEXT("values")).ToSharedRef(),Name);const bool Did=AppliedNames.Contains(Name),Was=PreviewMatchesExpected(Pair.Value,BeforeValue),Is=PreviewMatchesExpected(Pair.Value,AfterValue);FString Outcome;if(!Did){Outcome=TEXT("conversion_failed");++PropertyFailure;}else if(!Is){Outcome=TEXT("readback_failed");++PropertyFailure;}else if(Was){Outcome=TEXT("skipped_unchanged");++Skipped;}else{Outcome=TEXT("updated");++Updated;}auto Result=MakeShared<FJsonObject>();Result->SetStringField(TEXT("property"),Name);Result->SetStringField(TEXT("outcome"),Outcome);Result->SetBoolField(TEXT("applied"),Did);Result->SetBoolField(TEXT("before_matched_expected"),Was);Result->SetBoolField(TEXT("after_matched_expected"),Is);Results.Add(MakeShared<FJsonValueObject>(Result));}
        const bool SaveOK=!R.SaveRequested||R.Saved,Specific=PropertyFailure==0&&SaveOK;const int32 CompletionFailure=SaveOK?0:1;auto Delta=MakeShared<FJsonObject>();Delta->SetNumberField(TEXT("affected_count"),Updated);Delta->SetNumberField(TEXT("created_count"),0);Delta->SetNumberField(TEXT("updated_count"),Updated);Delta->SetNumberField(TEXT("removed_count"),0);Delta->SetNumberField(TEXT("skipped_count"),Skipped);Delta->SetNumberField(TEXT("property_failure_count"),PropertyFailure);Delta->SetNumberField(TEXT("completion_failure_count"),CompletionFailure);Delta->SetNumberField(TEXT("failed_count"),SaveOK?PropertyFailure:FMath::Max(PropertyFailure,Inputs.Num()));R.Data->SetArrayField(TEXT("property_results"),Results);R.Data->SetNumberField(TEXT("updated_property_count"),Updated);R.Data->SetNumberField(TEXT("skipped_property_count"),Skipped);R.Data->SetNumberField(TEXT("property_failure_count"),PropertyFailure);R.Data->SetNumberField(TEXT("completion_failure_count"),CompletionFailure);R.Data->SetBoolField(TEXT("actual_authored_value_changed"),!Equal(OriginalTyped,W.Expected));Contract(R,R.Data->GetObjectField(TEXT("before")).ToSharedRef(),R.Data->GetObjectField(TEXT("after")).ToSharedRef(),Delta,!Applied.IsEmpty(),Updated>0,Failed.IsEmpty(),Specific,TEXT("native_data_asset_direct_property_readback"),{TEXT("native_source_current"),TEXT("actual_partial_cell_prefix"),TEXT("save_gate")});
        R.Known=true;R.Changed=Updated>0;for(const TCHAR* K:{TEXT("observations_complete"),TEXT("source_preserved"),TEXT("references_preserved"),TEXT("unselected_state_preserved")})R.Data->SetBoolField(K,true);if(!Specific||!Failed.IsEmpty()){Fail(R,TEXT("data_asset_properties_failed"),TEXT("Known terminal individual failures preserve actual applied/failed cells, notifications and requested-save prefix; no retry."));R.Stage=TEXT("complete_with_property_failures");return;}R.Success=R.Post=true;R.Stage=TEXT("complete");
    };
    Run();return USFinish(W);
}
FString USCreate(const FString& AssetPath,const FString& RequestedClass,bool SaveRequested)
{
    FResult R;Init(R,TEXT("create_data_asset"),SaveRequested);R.Data->SetStringField(TEXT("requested_class_path"),RequestedClass);if(USNativeActive||PreviewSetterActive||DeformerSetterActive||InputActionActive||MappingContextActive||QCActive){Fail(R,TEXT("data_asset_native_reentry"),TEXT("Existing authoring operation active; no load/factory."));return Encode(R);}TGuardValue<bool> Active(USNativeActive,true);FContext C;if(!Path(AssetPath,true,C,R))return Encode(R);FUSNativeDataAssetWork W(R);W.Path=C.Path;W.PackagePath=C.Package;if(!W.Context())return Fail(R,TEXT("data_asset_editor_context_required"),TEXT("Idle captured Editor GT required.")),Encode(R);
    if(RequestedClass.Len()>1024||HasNul(RequestedClass)||!Clean(RequestedClass)||!FPackageName::IsValidObjectPath(RequestedClass)){Fail(R,TEXT("data_asset_class_path_invalid"),TEXT("Bounded native class object path required."));return Encode(R);}auto Call=MakeShared<FJsonObject>();Call->SetStringField(TEXT("class_path"),RequestedClass);Call->SetBoolField(TEXT("attempted"),true);R.Data->SetArrayField(TEXT("class_load_calls"),{MakeShared<FJsonValueObject>(Call)});Count(R,TEXT("class_load_count"));UClass* ReturnedClass=LoadObject<UClass>(nullptr,*RequestedClass);TStrongObjectPtr<UClass> KeepClass(ReturnedClass);Call->SetBoolField(TEXT("returned"),true);Call->SetBoolField(TEXT("returned_nonnull"),ReturnedClass!=nullptr);if(!W.Context())return W.Bad(TEXT("native_class_load_context")),Encode(R);if(!Loaded(ReturnedClass)||!ReturnedClass->GetPathName().Equals(RequestedClass,ESearchCase::CaseSensitive))return Fail(R,TEXT("data_asset_class_unsupported"),TEXT("Actual class identity differs or is unavailable.")),Encode(R);if(!W.Start(ReturnedClass))return Encode(R);R.Data->SetObjectField(TEXT("native_source_before"),W.SourceBaseline);if(!W.CaptureFiles())return Encode(R);
    auto Fresh=[&]()->bool{if(!W.Current())return false;Count(R,TEXT("asset_exists_query_calls"));const bool Exists=UEditorAssetLibrary::DoesAssetExist(C.Path);R.Data->SetBoolField(TEXT("native_create_exists_query_returned"),true);if(!W.Current())return false;auto File=MakeShared<FJsonObject>();if(!Disk(C.Package,File)||!W.Current())return false;return (!Exists&&!File->GetBoolField(TEXT("exists"))&&!FindObject<UObject>(nullptr,*C.Path)&&!FindPackage(nullptr,*C.Package))||Fail(R,TEXT("data_asset_already_exists"),TEXT("Fresh absent registry/object/package/file slot required; no overwrite or retry."));};if(!Fresh())return Encode(R);auto Before=MakeShared<FJsonObject>();Before->SetBoolField(TEXT("exists"),false);Before->SetStringField(TEXT("asset_path"),C.Path);R.Data->SetObjectField(TEXT("before"),Before);TStrongObjectPtr<UDataAssetFactory> Factory;
    auto Run=[&]()
    {
        R.Attempted=true;R.Known=false;R.Stage=TEXT("create_native_data_asset");if(!W.BeginTransaction()||!Fresh())return;Count(R,TEXT("factory_allocation_attempts"));UDataAssetFactory* ReturnedFactory=NewObject<UDataAssetFactory>();Factory.Reset(ReturnedFactory);R.Data->SetBoolField(TEXT("native_factory_allocation_returned"),ReturnedFactory!=nullptr);if(!W.Current())return;if(!Loaded(ReturnedFactory)||ReturnedFactory->GetClass()!=UDataAssetFactory::StaticClass()||ReturnedFactory->GetOuter()!=GetTransientPackage()||!W.Pin(ReturnedFactory))return (void)W.Bad(TEXT("native_factory_role"));ReturnedFactory->DataAssetClass=ReturnedClass;if(!W.Current())return;
        Count(R,TEXT("asset_tools_get_calls"));IAssetTools& Tools=FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();if(!W.Current()||!Fresh())return;if(!Factory->DataAssetClass.GetGCPtr().IsResolved()||Factory->DataAssetClass.GetGCPtr().Get()!=ReturnedClass)return (void)W.Bad(TEXT("native_factory_assigned_class"));Count(R,TEXT("asset_tools_create_attempts"));UObject* Returned=Tools.CreateAsset(FPackageName::GetShortName(C.Package),FPackageName::GetLongPackagePath(C.Package),ReturnedClass,Factory.Get());TStrongObjectPtr<UObject> Keep(Returned);R.Data->SetBoolField(TEXT("asset_tools_create_returned"),true);R.Data->SetBoolField(TEXT("asset_tools_create_nonnull"),Returned!=nullptr);if(!W.AfterAssetFactoryReturn())return;if(!Loaded(Returned)||Returned->GetClass()!=ReturnedClass||!Returned->GetPathName().Equals(C.Path,ESearchCase::CaseSensitive))return (void)W.Bad(TEXT("native_factory_created_asset"));auto* New=static_cast<UDataAsset*>(Returned);if(!W.BindAsset(New,C.Path))return;R.Data->SetObjectField(TEXT("created_asset"),Identity(New));if(!W.Current())return;Count(R,TEXT("mark_package_dirty_calls"));const bool Marked=New->MarkPackageDirty();R.Data->SetBoolField(TEXT("native_mark_dirty_returned"),Marked);if(!W.TargetFlagsAfter()||!W.Current())return;
        auto Allocated=MakeShared<FJsonObject>();if(!W.Snapshot(Allocated)||!W.Files())return;R.Data->SetObjectField(TEXT("after_creation_observation"),Allocated);if(!W.Save())return;auto AfterSnapshot=MakeShared<FJsonObject>();if(!W.Snapshot(AfterSnapshot)||!W.Files(R.Saved))return;Count(R,TEXT("legacy_get_calls"));auto After=USLegacyGet(W,AfterSnapshot);After->SetBoolField(TEXT("exists"),true);R.Data->SetObjectField(TEXT("after"),After);R.Data->SetObjectField(TEXT("after_observation"),AfterSnapshot);R.Data->SetStringField(TEXT("data_asset"),C.Path);R.Data->SetStringField(TEXT("class"),ReturnedClass->GetPathName());const bool Pass=Loaded(W.Asset.Get())&&W.Asset->GetClass()==ReturnedClass&&(!R.SaveRequested||R.Saved);auto Delta=MakeShared<FJsonObject>();for(const TCHAR* K:{TEXT("affected_count"),TEXT("created_count")})Delta->SetNumberField(K,1);for(const TCHAR* K:{TEXT("updated_count"),TEXT("removed_count"),TEXT("skipped_count"),TEXT("property_failure_count"),TEXT("completion_failure_count")})Delta->SetNumberField(K,0);Delta->SetNumberField(TEXT("failed_count"),Pass?0:1);Contract(R,Before,After,Delta,true,true,true,Pass,TEXT("native_data_asset_factory_lifecycle"),{TEXT("class_match"),TEXT("current_created_asset"),TEXT("save_gate")});R.Known=true;R.Changed=true;R.Success=R.Post=Pass;R.Stage=TEXT("complete");for(const TCHAR* K:{TEXT("observations_complete"),TEXT("source_preserved"),TEXT("references_preserved"),TEXT("unselected_state_preserved")})R.Data->SetBoolField(K,true);
    };
    Run();return USFinish(W);
}

}
FString Get(const FString& AssetPath)
{
    if(USNativeActive){FResult Busy;Init(Busy,TEXT("get_data_asset"));Fail(Busy,TEXT("data_asset_native_reentry"),TEXT("Native asset operation active; no nested loader/observer."));return Encode(Busy);}
    if(QCGeneratedTarget(AssetPath))return QCGet(AssetPath);
    FResult R; Init(R, TEXT("get_data_asset"));if(MappingContextActive){Fail(R,TEXT("data_asset_mapping_reentry"),TEXT("Context callback reentry rejected before load."));return Encode(R);} if(InputActionActive){Fail(R,TEXT("data_asset_input_action_reentry"),TEXT("InputAction callback reentry rejected before load."));return Encode(R);} FContext C;C.PermitPreview=true; FObservation Before, After;
    if (!Path(AssetPath, false, C, R) || !Load(C, R, TEXT("get_data_asset_load"),true)) { return Encode(R); } if(C.GeneratedLoaded)return QCGet(AssetPath,&R,&C);if(C.NativeLoaded)return USGet(R,C);Count(R, TEXT("legacy_get_calls"));
    if (!Capture(C, Before, R, TEXT("before_observation"))) { Fail(R, TEXT("data_asset_read_unobservable"), TEXT("Bounded reader/export prefix incomplete; maps retained."), true); return Encode(R); }
    R.Data->SetStringField(TEXT("class"), C.Class->GetPathName()); R.Data->SetObjectField(TEXT("values"), Before.State->GetObjectField(TEXT("values")).ToSharedRef()); R.Data->SetStringField(TEXT("values_scope"), TEXT("Original serialization in complete before_observation; successful final preservation proves equality."));
    if (!Capture(C, After, R, TEXT("after_observation"))) { Fail(R, TEXT("data_asset_read_unobservable"), TEXT("Final reader prefix incomplete."), true); return Encode(R); }
    const bool State = Equal(Before.State, After.State), Source = Equal(Before.Source, After.Source), Refs = Equal(Before.Refs, After.Refs), File = Equal(Before.File, After.File), Other = Equal(Before.Other, After.Other);
    R.Known = true; R.Changed = !(State && Source && Refs && File && Other); R.Data->SetBoolField(TEXT("observations_complete"), true); R.Data->SetBoolField(TEXT("source_preserved"), Source); R.Data->SetBoolField(TEXT("references_preserved"), Refs); R.Data->SetBoolField(TEXT("unselected_state_preserved"), State); R.Data->SetBoolField(TEXT("other_package_dirty_preserved"), Other); R.Data->SetBoolField(TEXT("disk_preserved"), File);
    if (R.Changed) { Fail(R, TEXT("data_asset_read_preservation_failed"), TEXT("Complete bounded reader scope changed; explicit writer/compile/save remained zero.")); return Encode(R); } R.Success = R.Post = true; R.Stage = TEXT("complete"); return Encode(R);
}
FString Set(const FString& AssetPath, const FString& PropertiesJson, bool bSave)
{
    if(USNativeActive){FResult Busy;Init(Busy,TEXT("set_data_asset_properties"),bSave);Fail(Busy,TEXT("data_asset_native_reentry"),TEXT("Native asset operation active; no nested loader/writer."));return Encode(Busy);}
    if(QCGeneratedTarget(AssetPath))return QCSet(AssetPath,PropertiesJson,bSave);
    FResult R; Init(R, TEXT("set_data_asset_properties"), bSave); FContext C; FObservation Before, Configured, After;
    if(PreviewSetterActive||DeformerSetterActive||InputActionActive||MappingContextActive){Fail(R,TEXT("data_asset_preview_set_reentry"),TEXT("A concrete Preview setter is already active; nested setter rejected before load/SDK."));return Encode(R);}C.PermitPreview=true;
    if (!Path(AssetPath, true, C, R)) { return Encode(R); }
    TSharedPtr<FJsonObject> Requested;
    if (PropertiesJson.Len() > 65536 || HasNul(PropertiesJson) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(PropertiesJson), Requested) || !Requested.IsValid() || Requested->Values.Num() > 32)
    { Fail(R, TEXT("data_asset_properties_json_invalid"), TEXT("Original properties object requires bounded JSON and at most32 fields.")); return Encode(R); }
    int32 N = 0, Chars = 0; if (!Bounded(MakeShared<FJsonValueObject>(Requested), N, Chars)) { Fail(R, TEXT("data_asset_properties_json_invalid"), TEXT("Finite bounded requested values required.")); return Encode(R); }
    R.Data->SetObjectField(TEXT("requested_properties"), Requested.ToSharedRef());
    bool ArrayRequest=false;for(const auto& Pair:Requested->Values){const FString K(Pair.Key.ToView());if(K.Len()<NAME_SIZE&&(FName(*K)==FName(TEXT("MeshDeformers"))||FName(*K)==FName(TEXT("MeshDeformerCollections")))){ArrayRequest=true;}}
    TGuardValue<bool> DeformerEntry(DeformerSetterActive,ArrayRequest);if(ArrayRequest){C.Deformer=MakeUnique<FDeformerCollectionScope>(R);if(!C.Deformer->Start()){return Encode(R);}}
    for(const auto& Top:Requested->Values)
    {
        if(FName(*FString(Top.Key.ToView()))!=TEXT("DefaultKeyMappings")||Top.Value->Type!=EJson::Object){continue;}
        for(const auto& Wrap:Top.Value->AsObject()->Values){if(FName(*FString(Wrap.Key.ToView()))!=TEXT("Mappings")||Wrap.Value->Type!=EJson::Array){continue;}for(const auto& Row:Wrap.Value->AsArray()){if(Row->Type!=EJson::Object){continue;}for(const auto& Field:Row->AsObject()->Values){const FName MappingFieldName(*FString(Field.Key.ToView()));if((MappingFieldName==TEXT("Triggers")||MappingFieldName==TEXT("Modifiers"))&&Field.Value->Type==EJson::Array&&!Field.Value->AsArray().IsEmpty()){C.MappingOwnedRequest=Requested;}}}}
    }
    for(const auto& Top:Requested->Values){const FName ActionArrayName(*FString(Top.Key.ToView()));if((ActionArrayName==TEXT("Triggers")||ActionArrayName==TEXT("Modifiers"))&&Top.Value->Type==EJson::Array&&!Top.Value->AsArray().IsEmpty()){C.ActionOwnedRequest=Requested;}}
    // LW sole-null refusal is intentionally not applied to mixed scalar/unknown siblings.
    if(Requested->Values.Num()==1){for(const auto& Top:Requested->Values){const FString K(Top.Key.ToView());C.SingleActionArrayNull=(K.Equals(TEXT("Modifiers"),ESearchCase::IgnoreCase)||K.Equals(TEXT("Triggers"),ESearchCase::IgnoreCase))&&Top.Value->Type==EJson::Null;}}
    // MS: every actual nonempty Mappings request uses the finite owned lifecycle, even when all row arrays are empty.
    if(!C.MappingOwnedRequest.IsValid())
    {for(const auto& Top:Requested->Values){if(FName(*FString(Top.Key.ToView()))!=TEXT("DefaultKeyMappings")||Top.Value->Type!=EJson::Object||Top.Value->AsObject()->Values.Num()!=1){continue;}for(const auto& Wrap:Top.Value->AsObject()->Values){if(FName(*FString(Wrap.Key.ToView()))==TEXT("Mappings")&&Wrap.Value->Type==EJson::Array&&!Wrap.Value->AsArray().IsEmpty()){C.MappingOwnedRequest=Requested;}}}}
    // Explicit empty-only requests retain the LW clear path and its original receipts.
    if(!C.ActionOwnedRequest.IsValid())
    {
        for(const auto& Top:Requested->Values){const FString K(Top.Key.ToView());if((K.Equals(TEXT("Modifiers"),ESearchCase::IgnoreCase)||K.Equals(TEXT("Triggers"),ESearchCase::IgnoreCase))&&Top.Value->Type==EJson::Array&&Top.Value->AsArray().IsEmpty()){C.ActionReferenceClear=true;C.ActionOwnedRequest=Requested;}}
    }
    if(!C.MappingOwnedRequest.IsValid())
    {
        for(const auto& Top:Requested->Values){if(!FString(Top.Key.ToView()).Equals(TEXT("DefaultKeyMappings"),ESearchCase::IgnoreCase)||Top.Value->Type!=EJson::Object||Top.Value->AsObject()->Values.Num()!=1){continue;}for(const auto& W:Top.Value->AsObject()->Values){if(FString(W.Key.ToView()).Equals(TEXT("Mappings"),ESearchCase::IgnoreCase)&&W.Value->Type==EJson::Array&&W.Value->AsArray().IsEmpty()){C.MappingReferenceClear=true;C.MappingOwnedRequest=Requested;}}}
    }
    if (!Load(C, R, TEXT("set_data_asset_primary_load"),true)) { return Encode(R); } if(C.GeneratedLoaded)return QCSet(AssetPath,PropertiesJson,bSave,&R,&C);if(C.NativeLoaded)return USSet(R,C,Requested.ToSharedRef());UDataAsset* A = C.Asset.Get();
    if(C.Mapping){return SetMappingContext(C,R,Requested.ToSharedRef(),bSave);}
    if(C.Input){return SetInputAction(C,R,Requested.ToSharedRef(),bSave);}
    if(C.Preview){return SetPreview(C,R,Requested.ToSharedRef(),bSave);}
    if(C.Deformer){return SetDeformer(C,R,Requested.ToSharedRef(),bSave);}
    if (bSave && !A->GetOutermost()->IsFullyLoaded()) { Fail(R, TEXT("data_asset_save_not_ready"), TEXT("Already fully loaded package required before a requested-save setter.")); return Encode(R); }
    Count(R, TEXT("legacy_get_calls"));
    if (!Load(C, R, TEXT("legacy_get_before_load")) || !Capture(C, Before, R, TEXT("before_observation")))
    { Fail(R, TEXT("data_asset_before_unobservable"), TEXT("Complete original before-load/serialization baseline required before Modify."), true); return Encode(R); }
    R.Data->SetObjectField(TEXT("before"), LegacyGet(C, Before)); R.Data->SetStringField(TEXT("class"), C.Class->GetPathName());
    R.Stage = TEXT("apply_properties"); R.Attempted = true; R.Known = false; Count(R, TEXT("outer_transaction_calls")); const FScopedTransaction Transaction(NSLOCTEXT("CubelessDataAsset", "Set", "Set DataAsset properties"));
    Count(R, TEXT("explicit_modify_calls")); A->Modify();
    TArray<TSharedPtr<FJsonValue>> Applied, Failed, Attempts, Notifications; TArray<FProperty*> Properties; TSet<FString> AppliedNames;
    for (const auto& Pair : Requested->Values)
    {
        const FString Name(Pair.Key.ToView()); auto Row = MakeShared<FJsonObject>(); Attempts.Add(MakeShared<FJsonValueObject>(Row)); R.Data->SetArrayField(TEXT("property_attempts"), Attempts);
        Row->SetStringField(TEXT("property"), Name); Row->SetBoolField(TEXT("conversion_attempted"), false); Row->SetBoolField(TEXT("storage_copy_returned"), false);
        FProperty* P = Name.Len() < NAME_SIZE && !HasNul(Name) ? C.Class->FindPropertyByName(FName(*Name)) : nullptr; FString Error;
        if (!P) { Error = TEXT("Property not found"); }
        else if (P->GetOwnerStruct() != C.Class || P->GetFName() != FName(TEXT("Description")) || !CastField<FStrProperty>(P) || P->ArrayDim != 1 || !P->HasAllPropertyFlags(CPF_Edit | CPF_EditorOnly) || P->HasAnyPropertyFlags(CPF_Config | CPF_GlobalConfig | CPF_Transient | CPF_EditConst | CPF_DisableEditOnInstance))
        { Error = TEXT("Only exact editable Editor-only Description is supported; original other-property variants remain incomplete"); }
        else if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::String || Pair.Value->AsString().Len() > 16384 || HasNul(Pair.Value->AsString()) || !Clean(Pair.Value->AsString()))
        { Error = TEXT("Description requires bounded literal string without tool dependency; original null/type coercions remain incomplete"); }
        else
        {
            FDefaultConstructedPropertyElement Scratch(P); Row->SetBoolField(TEXT("conversion_attempted"), true); Count(R, TEXT("json_conversion_calls"));
            if (!FJsonObjectConverter::JsonValueToUProperty(Pair.Value, P, Scratch.GetObjAddress(), 0, 0)) { Error = TEXT("Value conversion failed (check type)"); }
            else { Count(R, TEXT("property_storage_copy_calls")); P->CopyCompleteValue(P->ContainerPtrToValuePtr<void>(A), Scratch.GetObjAddress()); Row->SetBoolField(TEXT("storage_copy_returned"), true); Applied.Add(MakeShared<FJsonValueString>(Name)); Properties.Add(P); AppliedNames.Add(Name); }
        }
        if (!Error.IsEmpty()) { auto F = MakeShared<FJsonObject>(); F->SetStringField(TEXT("property"), Name); F->SetStringField(TEXT("error"), Error); Failed.Add(MakeShared<FJsonValueObject>(F)); Row->SetStringField(TEXT("error"), Error); }
        R.Data->SetArrayField(TEXT("applied"), Applied); R.Data->SetArrayField(TEXT("failed"), Failed);
    }
    for (FProperty* P : Properties)
    {
        auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("property"), P->GetName()); J->SetBoolField(TEXT("attempted"), true); Notifications.Add(MakeShared<FJsonValueObject>(J)); R.Data->SetArrayField(TEXT("property_notifications"), Notifications);
        Count(R, TEXT("post_edit_change_property_calls")); FPropertyChangedEvent Event(P); A->PostEditChangeProperty(Event); J->SetBoolField(TEXT("returned"), true);
    }
    if (Applied.Num() > 0) { Count(R, TEXT("mark_package_dirty_calls")); A->MarkPackageDirty(); }
    if (!Capture(C, Configured, R, TEXT("after_configuration"))) { Fail(R, TEXT("data_asset_configuration_unobservable"), TEXT("Actual conversion/callback prefix incomplete; save suppressed."), true); return Encode(R); }
    R.Known = true; R.Changed = !Equal(Before.State->GetObjectField(TEXT("values")).ToSharedRef(), Configured.State->GetObjectField(TEXT("values")).ToSharedRef()); R.Data->SetBoolField(TEXT("observed_state_changed"), !Equal(Before.State, Configured.State));
    if (!Fences(Before, Configured, Applied.Num() > 0, R)) { return Encode(R); } R.Data->SetBoolField(TEXT("after_configuration_source_preserved"), true); R.Data->SetBoolField(TEXT("after_configuration_unselected_state_preserved"), true);
    if (!Save(C, Configured, R)) { return Encode(R); }
    const bool FinalOK = Final(C, Configured, After, R); R.Data->SetBoolField(TEXT("final_preservation_passed"), FinalOK); if (!After.Complete) { return Encode(R); }
    TArray<TSharedPtr<FJsonValue>> Results; int32 Updated = 0, Skipped = 0, PropertyFailure = 0;
    for (const auto& Pair : Requested->Values)
    {
        const FString Name(Pair.Key.ToView()); const auto B = ValueByName(Before.State->GetObjectField(TEXT("values")).ToSharedRef(), Name), Z = ValueByName(After.State->GetObjectField(TEXT("values")).ToSharedRef(), Name);
        const bool Was = Equal(Pair.Value, B), Is = Equal(Pair.Value, Z), Did = AppliedNames.Contains(Name); FString Outcome;
        if (!Did) { Outcome = TEXT("conversion_failed"); ++PropertyFailure; } else if (!Is) { Outcome = TEXT("readback_failed"); ++PropertyFailure; } else if (Was) { Outcome = TEXT("skipped_unchanged"); ++Skipped; } else { Outcome = TEXT("updated"); ++Updated; }
        auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("property"), Name); J->SetBoolField(TEXT("applied"), Did); J->SetBoolField(TEXT("before_matched_expected"), Was); J->SetBoolField(TEXT("after_matched_expected"), Is); J->SetStringField(TEXT("outcome"), Outcome); Results.Add(MakeShared<FJsonValueObject>(J));
    }
    const bool SaveOK = !bSave || R.Saved, Specific = PropertyFailure == 0 && SaveOK; const int32 CompletionFailure = SaveOK ? 0 : 1, FailedCount = SaveOK ? PropertyFailure : FMath::Max(PropertyFailure, Requested->Values.Num());
    R.Data->SetArrayField(TEXT("property_results"), Results); R.Data->SetNumberField(TEXT("updated_property_count"), Updated); R.Data->SetNumberField(TEXT("skipped_property_count"), Skipped); R.Data->SetNumberField(TEXT("property_failure_count"), PropertyFailure); R.Data->SetNumberField(TEXT("completion_failure_count"), CompletionFailure);
    auto Delta = MakeShared<FJsonObject>(); Delta->SetNumberField(TEXT("affected_count"), Updated); Delta->SetNumberField(TEXT("created_count"), 0); Delta->SetNumberField(TEXT("updated_count"), Updated); Delta->SetNumberField(TEXT("removed_count"), 0); Delta->SetNumberField(TEXT("skipped_count"), Skipped); Delta->SetNumberField(TEXT("property_failure_count"), PropertyFailure); Delta->SetNumberField(TEXT("completion_failure_count"), CompletionFailure); Delta->SetNumberField(TEXT("failed_count"), FailedCount);
    Contract(R, R.Data->GetObjectField(TEXT("before")).ToSharedRef(), R.Data->GetObjectField(TEXT("after")).ToSharedRef(), Delta, Applied.Num() > 0, Updated > 0, Failed.Num() == 0, Specific, TEXT("data_asset_property_serialization_readback"), { TEXT("asset_resolved_after"), TEXT("requested_properties_match"), TEXT("save_gate") });
    R.Known = true; R.Changed = Updated > 0; R.Data->SetStringField(TEXT("legacy_verification_scope"), TEXT("Original selected property/count/save predicate, independent from extra native whole-state/source preservation. Performed and changed differ on matching/empty setter; actual work counts remain authoritative."));
    if (!FinalOK) { return Encode(R); }
    if (!Specific || Failed.Num() > 0) { Fail(R, Failed.Num() == 0 ? TEXT("mutation_postcondition_failed") : TEXT("data_asset_properties_failed"), TEXT("Actual terminal partial property/readback/save failure retained; no retry.")); return Encode(R); }
    R.Success = R.Post = true; R.Stage = TEXT("complete"); return Encode(R);
}
FString Create(const FString& AssetPath, const FString& RequestedClass, bool bSave)
{
    if(USNativeActive){FResult Busy;Init(Busy,TEXT("create_data_asset"),bSave);Fail(Busy,TEXT("data_asset_native_reentry"),TEXT("Native asset operation active; no nested class loader/factory."));return Encode(Busy);}
    if(USNativePrimaryClass(RequestedClass))return USCreate(AssetPath,RequestedClass,bSave);
    if(RequestedClass.StartsWith(TEXT("/Game/")))return QCCreate(AssetPath,RequestedClass,bSave);
    FResult R; Init(R, TEXT("create_data_asset"), bSave); FContext C;TUniquePtr<FScopedTransaction> Transaction;FObservation InputTerminal,MappingTerminal;
    auto Finish=[&]() -> FString
    {
        if(C.Mapping&&Transaction)
        {
            Transaction.Reset();if(C.Mapping->Stopped||!C.Mapping->Check()){PreviewInvalidateVerification(R);return Encode(R);}
            if(MappingTerminal.Complete)
            {
                FObservation Fence;const FString Stage=R.Stage;R.Stage=TEXT("after_transaction_end");const bool Observed=Capture(C,Fence,R,TEXT("input_mapping_after_transaction_observation"));
                const bool Same=Observed&&Equal(MappingTerminal.State,Fence.State)&&Equal(MappingTerminal.Source,Fence.Source)&&Equal(MappingTerminal.Refs,Fence.Refs)&&Equal(MappingTerminal.File,Fence.File)&&Equal(MappingTerminal.Other,Fence.Other);R.Data->SetBoolField(TEXT("transaction_fence_passed"),Same);
                if(!Same){C.Mapping->Stop(TEXT("data_asset_mapping_transaction_changed"),TEXT("Transaction-end global callback changed final state; actual SavedTrue/prefix retained."));PreviewInvalidateVerification(R);}else{R.Stage=Stage;}
            }
        }
        if(C.Input&&Transaction)
        {
            Transaction.Reset();
            if(C.Input->Stopped||!C.Input->Check()){PreviewInvalidateVerification(R);return Encode(R);}
            if(InputTerminal.Complete)
            {
                FObservation Fence;const FString Stage=R.Stage;R.Stage=TEXT("after_transaction_end");
                const bool Observed=Capture(C,Fence,R,TEXT("input_action_after_transaction_observation"));
                const bool Same=Observed&&Equal(InputTerminal.State,Fence.State)&&Equal(InputTerminal.Source,Fence.Source)&&Equal(InputTerminal.Refs,Fence.Refs)&&Equal(InputTerminal.File,Fence.File)&&Equal(InputTerminal.Other,Fence.Other);
                R.Data->SetBoolField(TEXT("transaction_fence_passed"),Same);
                if(!Same){C.Input->Stop(TEXT("data_asset_input_action_transaction_changed"),TEXT("Transaction-end callback changed final observed state; saved receipt/prefix retained, no replay."));PreviewInvalidateVerification(R);}else{R.Stage=Stage;}
            }
        }return Encode(R);
    };
    if(MappingContextActive){Fail(R,TEXT("data_asset_mapping_reentry"),TEXT("Context callback reentry rejected before class/asset load."));return Finish();}
    if(InputActionActive){Fail(R,TEXT("data_asset_input_action_reentry"),TEXT("InputAction callback reentry rejected before class/asset load."));return Finish();}
    if (!Path(AssetPath, true, C, R)) { return Finish(); } R.Data->SetStringField(TEXT("requested_class_path"), RequestedClass);
    if (RequestedClass.Len() > 1024 || HasNul(RequestedClass) || (RequestedClass!=ClassPath&&!RequestedClass.Equals(PreviewClassPath,ESearchCase::IgnoreCase)&&RequestedClass!=InputActionClassPath&&RequestedClass!=MappingContextClassPath)) { Fail(R, TEXT("data_asset_class_unsupported"), TEXT("Create admits exact native MeshDeformerCollection, PreviewMeshCollection, InputAction or InputMappingContext; arbitrary concrete native/generated classes remain incomplete, not excluded merely because a base is abstract.")); return Finish(); }
    if(RequestedClass==MappingContextClassPath){C.Mapping=MakeUnique<FMappingContextScope>(R,true);if(!C.Mapping->Start()){return Finish();}}
    if(RequestedClass==InputActionClassPath){C.Input=MakeUnique<FInputActionScope>(R,true);if(!C.Input->Start()){return Finish();}}
    C.PermitPreview=RequestedClass.Equals(PreviewClassPath,ESearchCase::IgnoreCase);if(C.PermitPreview){C.Preview=MakeUnique<FPreviewCollectionScope>(R);if(!C.Preview->Start()){return Finish();}}
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("class_path"), RequestedClass); J->SetBoolField(TEXT("attempted"), true); R.Data->SetArrayField(TEXT("class_load_calls"), { MakeShared<FJsonValueObject>(J) }); Count(R, TEXT("class_load_count"));
    C.Class = LoadObject<UClass>(nullptr, *RequestedClass);if(!PreviewAdmit(C.Preview.Get())||!InputAdmit(C.Input.Get())||!MappingAdmit(C.Mapping.Get())){J->SetBoolField(TEXT("returned"),true);return Finish();} J->SetBoolField(TEXT("returned"), true); J->SetObjectField(TEXT("returned_class"), Identity(C.Class));
    if (!Loaded(C.Class) || !C.Class->IsChildOf(UDataAsset::StaticClass()) || !C.Class->HasAnyClassFlags(CLASS_Native) || C.Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) { Fail(R, TEXT("data_asset_class_unsupported"), TEXT("Loaded stable nonabstract native supported class required.")); return Finish(); }
    if(C.Preview&&!C.Preview->BindSource(C.Class)){return Finish();}
    Count(R, TEXT("asset_exists_query_calls")); const bool Exists = UEditorAssetLibrary::DoesAssetExist(C.Path);if(!PreviewAdmit(C.Preview.Get())||!InputAdmit(C.Input.Get())||!MappingAdmit(C.Mapping.Get())){return Finish();} auto Absent = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("target_before_creation"), Absent); Absent->SetBoolField(TEXT("asset_registry_exists"), Exists); Absent->SetObjectField(TEXT("memory_object"), Identity(FindObject<UObject>(nullptr, *C.Path))); auto File = MakeShared<FJsonObject>(); Absent->SetObjectField(TEXT("disk"), File);
    if (!Disk(C.Package, File) || Exists || FindObject<UObject>(nullptr, *C.Path) || File->GetBoolField(TEXT("exists")) || FindObject<UPackage>(nullptr, *C.Package)) { Fail(R, TEXT("data_asset_already_exists_or_unobservable"), TEXT("Fresh absent package/object/registry/disk required; original existing target is error, not no-op.")); return Finish(); }
    C.Factory = UDataAssetFactory::StaticClass();if(C.Preview&&!C.Preview->BindSource(C.Factory,true)){return Finish();} auto Source = MakeShared<FJsonObject>(), Other = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("before_creation_native_sources"), Source); R.Data->SetObjectField(TEXT("before_creation_other_package_dirty"), Other);
    if (!Sources(C.Class, C.Factory, Source, true,C.Preview.Get(),nullptr,C.Input.Get(),C.Mapping.Get()) || !Dirty(C.Package, Other) || !FModuleManager::Get().IsModuleLoaded(TEXT("AssetTools"))) { Fail(R, TEXT("data_asset_factory_source_not_ready"), TEXT("Loaded existing native/factory CDOs and AssetTools module required; no explicit warming.")); return Finish(); }
    auto Before = MakeShared<FJsonObject>(); Before->SetBoolField(TEXT("exists"), false); Before->SetStringField(TEXT("asset_path"), C.Path); R.Data->SetObjectField(TEXT("before"), Before);
    R.Stage = TEXT("create_asset"); R.Attempted = true; R.Known = false; Count(R, TEXT("outer_transaction_calls")); Transaction=MakeUnique<FScopedTransaction>(NSLOCTEXT("CubelessDataAsset", "Create", "Create DataAsset"));if(!PreviewAdmit(C.Preview.Get())||!InputAdmit(C.Input.Get())||!MappingAdmit(C.Mapping.Get())){return Finish();}
    Count(R, TEXT("factory_allocation_attempts")); TStrongObjectPtr<UDataAssetFactory> Factory(NewObject<UDataAssetFactory>());if(!PreviewAdmit(C.Preview.Get())||!InputAdmit(C.Input.Get())||!MappingAdmit(C.Mapping.Get())){return Finish();}if(C.Preview&&(!Loaded(Factory.Get())||Factory->GetClass()!=UDataAssetFactory::StaticClass()||Factory->GetOuter()!=GetTransientPackage()||!C.Preview->Pin(Factory.Get()))){Fail(R,TEXT("data_asset_preview_factory_invalid"),TEXT("Returned transient factory must remain exact before its field is assigned."),true);return Finish();} R.Data->SetObjectField(TEXT("created_factory"), Identity(Factory.Get()));
    if (!Loaded(Factory.Get())) { Fail(R, TEXT("data_asset_factory_creation_failed"), TEXT("Factory allocation prefix retained; no retry."), true); return Finish(); }
    if(C.Input&&(Factory->GetClass()!=UDataAssetFactory::StaticClass()||Factory->GetOuter()!=GetTransientPackage()||!C.Input->Pin(Factory.Get()))){Fail(R,TEXT("data_asset_input_action_factory_invalid"),TEXT("Current exact native transient factory required before class assignment."),true);return Finish();}
    if(C.Mapping&&(Factory->GetClass()!=UDataAssetFactory::StaticClass()||Factory->GetOuter()!=GetTransientPackage()||!C.Mapping->Pin(Factory.Get()))){Fail(R,TEXT("data_asset_mapping_factory_invalid"),TEXT("Exact transient native factory required before assigning class."),true);return Finish();}
    Factory->DataAssetClass = C.Class;
    Count(R, TEXT("asset_tools_get_calls")); IAssetTools& Tools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get(); if(!PreviewAdmit(C.Preview.Get())||!InputAdmit(C.Input.Get())||!MappingAdmit(C.Mapping.Get())){return Finish();}
    if(C.Preview||C.Input||C.Mapping)
    {
        // Sources/transaction/factory callbacks follow the first absent check. Recheck at
        // the last explicit SDK boundary so that AssetTools cannot enter its overwrite prompt.
        auto Fresh=MakeShared<FJsonObject>();R.Data->SetObjectField(C.Mapping?TEXT("input_mapping_precreate_absence"):C.Input?TEXT("input_action_precreate_absence"):TEXT("preview_precreate_absence"),Fresh);Count(R,TEXT("asset_exists_query_calls"));
        R.Data->SetNumberField(C.Mapping?TEXT("input_mapping_precreate_absence_query_calls"):C.Input?TEXT("input_action_precreate_absence_query_calls"):TEXT("preview_precreate_absence_query_calls"),1);
        const bool RegistryExists=UEditorAssetLibrary::DoesAssetExist(C.Path);Fresh->SetBoolField(TEXT("registry_query_returned"),true);
        if(!PreviewAdmit(C.Preview.Get())||!InputAdmit(C.Input.Get())||!MappingAdmit(C.Mapping.Get())){return Finish();}
        Fresh->SetBoolField(TEXT("registry_exists"),RegistryExists);const bool MemoryExists=FindObject<UObject>(nullptr,*C.Path)!=nullptr,PackageExists=FindObject<UPackage>(nullptr,*C.Package)!=nullptr;
        Fresh->SetBoolField(TEXT("memory_object_exists"),MemoryExists);Fresh->SetBoolField(TEXT("memory_package_exists"),PackageExists);auto CurrentFile=MakeShared<FJsonObject>();Fresh->SetObjectField(TEXT("disk"),CurrentFile);
        const bool FileObserved=Disk(C.Package,CurrentFile);if(!PreviewAdmit(C.Preview.Get())||!InputAdmit(C.Input.Get())||!MappingAdmit(C.Mapping.Get())){return Finish();}Fresh->SetBoolField(TEXT("complete"),FileObserved);
        if(!FileObserved||RegistryExists||MemoryExists||PackageExists||CurrentFile->GetBoolField(TEXT("exists")))
        {Fail(R,C.Mapping?TEXT("data_asset_mapping_target_no_longer_absent"):C.Input?TEXT("data_asset_input_action_target_no_longer_absent"):TEXT("data_asset_preview_target_no_longer_absent"),TEXT("Fresh-only target changed after preparatory callbacks; factory allocation prefix retained, no CreateAsset/overwrite/modal/save/retry."),true);return Finish();}
    }
    if(C.Input&&(!C.Input->Check()||!Loaded(Factory.Get())||Factory->DataAssetClass.Get()!=C.Class)){Fail(R,TEXT("data_asset_input_action_factory_changed"),TEXT("Current factory class assignment/source changed before CreateAsset; no replay."),true);return Finish();}
    if(C.Mapping&&(!C.Mapping->Check()||!Loaded(Factory.Get())||Factory->DataAssetClass.Get()!=C.Class)){Fail(R,TEXT("data_asset_mapping_factory_changed"),TEXT("Factory/source changed before CreateAsset; no retry."),true);return Finish();}
    Count(R,TEXT("asset_tools_create_attempts"));
    UObject* Object = Tools.CreateAsset(FPackageName::GetShortName(C.Package), FPackageName::GetLongPackagePath(C.Package), C.Class, Factory.Get()); R.Data->SetBoolField(TEXT("asset_tools_create_returned"), true);if(!PreviewAdmit(C.Preview.Get())||!InputAdmit(C.Input.Get())||!MappingAdmit(C.Mapping.Get())){return Finish();} R.Data->SetObjectField(TEXT("created_asset"), Identity(Object)); C.Asset.Reset(Loaded(Object)?Cast<UDataAsset>(Object):nullptr);
    if (!Loaded(C.Asset.Get()) || C.Asset->GetPathName() != C.Path || C.Asset->GetClass() != C.Class) { Fail(R, TEXT("data_asset_creation_failed"), TEXT("Actual factory/CreateAsset prefix retained; target not observed, no retry."), true); return Finish(); }
    if(C.Preview&&!C.Preview->BindTarget(C.Asset.Get(),C.Path)){return Finish();}
    if(C.Input&&!C.Input->Bind(C.Asset.Get(),C.Path)){return Finish();}
    if(C.Mapping&&!C.Mapping->Bind(C.Asset.Get(),C.Path)){return Finish();}
    R.Data->SetStringField(TEXT("class"), C.Asset->GetClass()->GetPathName());
    Count(R, TEXT("mark_package_dirty_calls")); C.Asset->MarkPackageDirty();if(!PreviewAdmit(C.Preview.Get())||!InputAdmit(C.Input.Get())||!MappingAdmit(C.Mapping.Get())){return Finish();} FObservation Created, After;
    if (!Capture(C, Created, R, TEXT("after_creation"))) { Fail(R, TEXT("data_asset_creation_unobservable"), TEXT("Created target raw prefix incomplete; no save."), true); return Finish(); }
    if(C.Input){InputTerminal=Created;}if(C.Mapping){MappingTerminal=Created;}
    const bool SourceOK = Equal(Source, Created.Source), OtherOK = Equal(Other, Created.Other), FileOK = Equal(File, Created.File); R.Known = true; R.Changed = true; R.Data->SetBoolField(TEXT("observations_complete"), true); R.Data->SetBoolField(TEXT("source_preserved"), SourceOK); R.Data->SetBoolField(TEXT("references_preserved"), true); R.Data->SetBoolField(TEXT("unselected_state_preserved"), true); R.Data->SetBoolField(TEXT("other_package_dirty_preserved"), OtherOK); R.Data->SetBoolField(TEXT("disk_preserved_before_save"), FileOK);
    if (!(SourceOK && OtherOK && FileOK)) { Fail(R, TEXT("data_asset_creation_preservation_failed"), TEXT("Created asset retained; source/disk/other dirty changed, save suppressed.")); return Finish(); }
    if (!Save(C, Created, R)) { return Finish(); } const bool FinalOK = Final(C, Created, After, R); if(C.Input&&After.Complete){InputTerminal=After;}if(C.Mapping&&After.Complete){MappingTerminal=After;} R.Data->SetBoolField(TEXT("final_preservation_passed"), FinalOK); if (!After.Complete) { return Finish(); }
    Count(R, TEXT("asset_exists_query_calls")); const bool ExistsAfter = UEditorAssetLibrary::DoesAssetExist(C.Path);if(!PreviewAdmit(C.Preview.Get())||!InputAdmit(C.Input.Get())||!MappingAdmit(C.Mapping.Get())){return Finish();} R.Data->GetObjectField(TEXT("after"))->SetBoolField(TEXT("exists"), ExistsAfter);
    const bool Observed = ExistsAfter && C.Asset->GetClass() == C.Class && After.Complete, SaveOK = !bSave || R.Saved, Specific = Observed && SaveOK; R.Data->SetBoolField(TEXT("asset_observed"), Observed);
    auto Delta = MakeShared<FJsonObject>(); Delta->SetNumberField(TEXT("affected_count"), Observed ? 1 : 0); Delta->SetNumberField(TEXT("created_count"), Observed ? 1 : 0); Delta->SetNumberField(TEXT("updated_count"), 0); Delta->SetNumberField(TEXT("removed_count"), 0); Delta->SetNumberField(TEXT("skipped_count"), 0); Delta->SetNumberField(TEXT("completion_failure_count"), SaveOK ? 0 : 1); Delta->SetNumberField(TEXT("failed_count"), (Observed ? 0 : 1) + (SaveOK ? 0 : 1));
    Contract(R, Before, R.Data->GetObjectField(TEXT("after")).ToSharedRef(), Delta, true, true, true, Specific, TEXT("asset_library_data_asset_relookup"), { TEXT("asset_exists_after"), TEXT("class_matches"), TEXT("save_gate") });
    R.Known = true; R.Changed = true; R.Data->SetStringField(TEXT("legacy_verification_scope"), TEXT("Actual original AssetLibrary relookup/existence/class/save predicate; extra whole-target/source preservation is separate."));
    if (!FinalOK) { return Finish(); } if (!Specific) { Fail(R, TEXT("mutation_postcondition_failed"), TEXT("Created/saved prefix retained; original final existence/class/save gate failed.")); return Finish(); }
    R.Success = R.Post = true; R.Stage = TEXT("complete"); return Finish();
}
}
