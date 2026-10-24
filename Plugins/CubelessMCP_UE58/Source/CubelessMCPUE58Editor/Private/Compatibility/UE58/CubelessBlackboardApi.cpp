// ADK source-only handoff: ai_tools.py1992 add_ai_blackboard_key -> ai_add_blackboard_key ->
// AICommands.cpp1894/HandleAddBlackboardKey2573; ResolveBlackboardBaseClass1365/1383 has no Actor-only restriction.
// Pure UObject-root Object/Class references reuse existing Append/cache/partial/save workers; native/Actor branches remain separate.
// SDK BlueprintGeneratedClass.cpp690/692 tracks generator/class reads: all source roles/exports/first fields precede Authority,
// then original fields/exports/current are reclosed. Shared Object-parent inspection preserves actual skeleton super and nullable CDO/SCS.
// Source checks: public workers/call order unchanged except one scope diagnostic; registration nondoc AST/IO/default/no-import compile pass.
// Build/official behavior/save/reload pending; other native roots, unobserved cold-load transitions and full arbitrary callback effects incomplete.
// Original Blackboard Create/Add/Inspect contracts through public runtime AIModule APIs.
#include "Compatibility/UE58/CubelessBlackboardApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessGeneratedComponentApi.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/World.h"
#include "UObject/UObjectHash.h"
#include "AISystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Class.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Int.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Name.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Rotator.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_String.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "EditorAssetLibrary.h"
#include "Engine/UserDefinedEnum.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::Blackboard
{
namespace
{
constexpr int32 MaxKeys = 512, MaxParents = 32, MaxProperties = 128, MaxLoadedPackages = 4096;
constexpr int32 MaxText = 262144;
constexpr int64 MaxDiskBytes = 16 * 1024 * 1024;
struct FResult
{
    FString Operation, Stage = TEXT("preflight"), Code, Error;
    bool bSuccess = false, bAttempted = false, bKnown = true, bChanged = false, bPost = false;
    bool bSaveRequested = false, bSaveAttempted = false, bSaved = false;
    TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
};
FString Text(const TSharedRef<FJsonObject>& J)
{ FString S; FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&S)); return S; }
bool Fail(FResult& R, const TCHAR* Code, const FString& Error)
{ R.bSuccess = false; R.Code = Code; R.Error = Error; return false; }
FString Encode(const FResult& R)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("operation"), R.Operation);
    J->SetBoolField(TEXT("success"), R.bSuccess); J->SetStringField(TEXT("stage"), R.Stage);
    J->SetStringField(TEXT("error_code"), R.Code); J->SetStringField(TEXT("error"), R.Error);
    J->SetBoolField(TEXT("mutation_attempted"), R.bAttempted); J->SetBoolField(TEXT("change_state_known"), R.bKnown);
    if (R.bKnown) { J->SetBoolField(TEXT("changed"), R.bChanged); }
    else { J->SetField(TEXT("changed"), MakeShared<FJsonValueNull>()); }
    J->SetBoolField(TEXT("postcondition_passed"), R.bPost);
    J->SetBoolField(TEXT("compile_requested"), false); J->SetBoolField(TEXT("compile_attempted"), false);
    J->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    J->SetBoolField(TEXT("save_requested"), R.bSaveRequested); J->SetBoolField(TEXT("save_attempted"), R.bSaveAttempted);
    J->SetBoolField(TEXT("saved"), R.bSaved); J->SetBoolField(TEXT("atomic_rollback_available"), false);
    J->SetBoolField(TEXT("auto_retry_safe"), false); J->SetObjectField(TEXT("data"), R.Data); return Text(J);
}
void Init(FResult& R, const TCHAR* Operation, bool bSave)
{
    R.Operation = Operation; R.bSaveRequested = bSave;
    R.Data->SetBoolField(TEXT("save_asset_requested"), bSave); R.Data->SetBoolField(TEXT("asset_saved"), false);
    R.Data->SetBoolField(TEXT("asset_load_dispatched"), false); R.Data->SetBoolField(TEXT("implicit_postload_possible"), false);
    R.Data->SetNumberField(TEXT("asset_load_count"), 0); R.Data->SetNumberField(TEXT("cache_refresh_count"), 0);
    R.Data->SetBoolField(TEXT("cache_refresh_dispatched"), false); R.Data->SetBoolField(TEXT("key_declaration_attempted"), false);
    R.Data->SetBoolField(TEXT("asset_registry_publication_dispatched"), false);
    R.Data->SetBoolField(TEXT("explicit_post_edit_change_invoked"), false); R.Data->SetBoolField(TEXT("implicit_engine_work_measured"), false);
    R.Data->SetBoolField(TEXT("runtime_blackboard_values_verified"), false); R.Data->SetBoolField(TEXT("behavior_tree_execution_verified"), false);
    R.Data->SetBoolField(TEXT("custom_key_classes_supported"), false); R.Data->SetBoolField(TEXT("custom_base_classes_supported"), false);
    R.Data->SetStringField(TEXT("dependency_tier"), TEXT("EngineAPI: public AIModule/CoreUObject/AssetRegistry and EditorAssetLibrary; no official Blackboard Toolset leaf or ServerRPC"));
    R.Data->SetStringField(TEXT("effect_scope"), TEXT("exact runtime BlackboardData asset and ten native key classes; explicit asset resolution may run PostLoad; cache refresh can add engine persistent keys and broadcasts OnUpdateKeys; no AI world, BehaviorTree/EQS graph, compile, private factory or runtime helper"));
    R.Data->SetStringField(TEXT("changed_observation_scope"), TEXT("selected ordered own/inherited entries, public key IDs/cache flags, bounded reflected data, parent states, native/enum sources and loaded-package dirty states after initial resolution; arbitrary callback/world state and loader work are not fully instrumented"));
}
bool Loaded(const UObject* O)
{ return IsValid(O) && !O->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_BeginDestroyed | RF_FinishDestroyed); }
TSharedRef<FJsonObject> Identity(const UObject* O)
{
    auto J = MakeShared<FJsonObject>();
    J->SetStringField(TEXT("path"), O ? O->GetPathName() : TEXT(""));
    J->SetStringField(TEXT("class"), O ? O->GetClass()->GetPathName() : TEXT(""));
    J->SetStringField(TEXT("outer"), O && O->GetOuter() ? O->GetOuter()->GetPathName() : TEXT(""));
    J->SetStringField(TEXT("object_flags"), O ? FString::Printf(TEXT("%llu"), static_cast<uint64>(O->GetFlags())) : TEXT("0"));
    return J;
}
bool GeneratedReadAdmission();
bool GeneratedBoundary();
bool Properties(UObject* O, const TSet<FName>& Skip, TSharedRef<FJsonObject>& J)
{
    if (!GeneratedReadAdmission() || !Loaded(O)) { return false; }
    int32 Count = 0, Chars = 0;
    for (TFieldIterator<FProperty> It(O->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
    {
        FProperty* P = *It; if (Skip.Contains(P->GetFName())) { continue; }
        if (++Count > MaxProperties || P->ArrayDim < 1 || P->ArrayDim > 64) { return false; }
        TArray<TSharedPtr<FJsonValue>> Values;
        for (int32 I = 0; I < P->ArrayDim; ++I)
        {
            FString S; const void* V = P->ContainerPtrToValuePtr<void>(O, I);
            const bool Exported=P->ExportText_Direct(S,V,V,O,PPF_None); if (!GeneratedReadAdmission() || !Exported) { return false; }
            Chars += S.Len(); if (Chars > MaxText) { return false; } Values.Add(MakeShared<FJsonValueString>(S));
        }
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("owner"), P->GetOwnerStruct()->GetPathName());
        Row->SetStringField(TEXT("class"), P->GetClass()->GetName());
        Row->SetStringField(TEXT("flags"), FString::Printf(TEXT("%llu"), static_cast<uint64>(P->GetPropertyFlags())));
        Row->SetNumberField(TEXT("array_dim"), P->ArrayDim); Row->SetArrayField(TEXT("values"), Values); J->SetObjectField(P->GetName(), Row);
    }
    return true;
}
UClass* KeyClass(const FString& Type)
{
    if (Type == TEXT("bool")) return UBlackboardKeyType_Bool::StaticClass();
    if (Type == TEXT("class")) return UBlackboardKeyType_Class::StaticClass();
    if (Type == TEXT("enum")) return UBlackboardKeyType_Enum::StaticClass();
    if (Type == TEXT("float")) return UBlackboardKeyType_Float::StaticClass();
    if (Type == TEXT("int")) return UBlackboardKeyType_Int::StaticClass();
    if (Type == TEXT("name")) return UBlackboardKeyType_Name::StaticClass();
    if (Type == TEXT("object")) return UBlackboardKeyType_Object::StaticClass();
    if (Type == TEXT("rotator")) return UBlackboardKeyType_Rotator::StaticClass();
    if (Type == TEXT("string")) return UBlackboardKeyType_String::StaticClass();
    if (Type == TEXT("vector")) return UBlackboardKeyType_Vector::StaticClass();
    return nullptr;
}
FString KeyTypeName(UClass* C)
{ FString S = C ? C->GetName() : TEXT(""); S.RemoveFromStart(TEXT("BlackboardKeyType_")); return S; }
bool RuntimeNative(UClass* C, bool bAllowAbstract = false)
{
    if (!Loaded(C) || !C->HasAnyClassFlags(CLASS_Native) || C->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists)
        || (!bAllowAbstract && C->HasAnyClassFlags(CLASS_Abstract)) || C->IsEditorOnly() || !C->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn)
        || C->GetOutermost()->IsDirty()) { return false; }
    const FString P = C->GetOutermost()->GetName();
    return P == TEXT("/Script/CoreUObject") || P == TEXT("/Script/Engine") || P == TEXT("/Script/AIModule");
}
bool Disk(const FString& PackageName, TSharedRef<FJsonObject>& J)
{
    FString Filename;
    if (!FPackageName::TryConvertLongPackageNameToFilename(PackageName, Filename, FPackageName::GetAssetPackageExtension())) { return false; }
    Filename = FPaths::ConvertRelativePathToFull(Filename); const int64 Size = IFileManager::Get().FileSize(*Filename);
    J->SetStringField(TEXT("path"), Filename); J->SetBoolField(TEXT("exists"), Size >= 0); J->SetNumberField(TEXT("bytes"), FMath::Max<int64>(0, Size));
    if (Size < 0) { J->SetStringField(TEXT("sha1"), TEXT("")); return true; }
    TArray<uint8> Bytes;
    if (Size > MaxDiskBytes || !FFileHelper::LoadFileToArray(Bytes, *Filename) || Bytes.Num() != Size) { return false; }
    uint8 Hash[FSHA1::DigestSize]; FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Hash); J->SetStringField(TEXT("sha1"), BytesToHex(Hash, FSHA1::DigestSize)); return true;
}
bool NativeSource(UClass* C, TSharedRef<FJsonObject>& J)
{
    if (!RuntimeNative(C, true))
    {
        if (IsValid(C)) { J = Identity(C); J->SetStringField(TEXT("class_flags"), FString::Printf(TEXT("%llu"), static_cast<uint64>(C->GetClassFlags()))); }
        J->SetBoolField(TEXT("class_loaded"), Loaded(C)); J->SetStringField(TEXT("failed_substage"), TEXT("runtime_native_class")); return false;
    }
    J = Identity(C); J->SetStringField(TEXT("super_class"), C->GetSuperClass() ? C->GetSuperClass()->GetPathName() : TEXT(""));
    J->SetStringField(TEXT("class_flags"), FString::Printf(TEXT("%llu"), static_cast<uint64>(C->GetClassFlags())));
    J->SetBoolField(TEXT("package_dirty"), C->GetOutermost()->IsDirty()); UObject* CDO = C->GetDefaultObject(false);
    J->SetBoolField(TEXT("cdo_exists"), CDO != nullptr);
    if (IsValid(CDO)) { J->SetObjectField(TEXT("existing_cdo"), Identity(CDO)); }
    if (!Loaded(CDO) || CDO->GetClass() != C || CDO->GetOuter() != C->GetOuter() || !CDO->HasAnyFlags(RF_ClassDefaultObject))
    {
        J->SetStringField(TEXT("expected_cdo_outer"), C->GetOuter()->GetPathName()); J->SetBoolField(TEXT("cdo_loaded"), Loaded(CDO));
        J->SetBoolField(TEXT("cdo_class_matches"), IsValid(CDO) && CDO->GetClass() == C);
        J->SetBoolField(TEXT("cdo_outer_matches"), IsValid(CDO) && CDO->GetOuter() == C->GetOuter());
        J->SetBoolField(TEXT("cdo_class_default_object"), IsValid(CDO) && CDO->HasAnyFlags(RF_ClassDefaultObject));
        J->SetStringField(TEXT("failed_substage"), TEXT("existing_cdo_identity")); return false;
    }
    auto P = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("existing_cdo_properties"), P);
    if (!Properties(CDO, {}, P)) { J->SetStringField(TEXT("failed_substage"), TEXT("existing_cdo_properties")); return false; }
    return true;
}
bool Sources(TSharedRef<FJsonObject>& J)
{
    const auto Reject = [&J](const FString& ClassPath, const TSharedRef<FJsonObject>& Source)
    {
        J->SetObjectField(ClassPath, Source); J->SetStringField(TEXT("failed_class"), ClassPath);
        FString Substage; Source->TryGetStringField(TEXT("failed_substage"), Substage);
        J->SetStringField(TEXT("failed_substage"), Substage); J->SetBoolField(TEXT("source_complete"), false); return false;
    };
    const FString Types[] = { TEXT("bool"), TEXT("class"), TEXT("enum"), TEXT("float"), TEXT("int"), TEXT("name"), TEXT("object"), TEXT("rotator"), TEXT("string"), TEXT("vector") };
    for (const auto& T : Types)
    {
        UClass* C = KeyClass(T); auto S = MakeShared<FJsonObject>();
        const FString ClassPath = IsValid(C) ? C->GetPathName() : FString(TEXT("unavailable_key_type:")) + T;
        if (!RuntimeNative(C) || C->GetPathName() != FString(TEXT("/Script/AIModule.BlackboardKeyType_")) + KeyTypeName(C)
            || !C->IsChildOf(UBlackboardKeyType::StaticClass()))
        {
            if (IsValid(C)) { S = Identity(C); S->SetStringField(TEXT("class_flags"), FString::Printf(TEXT("%llu"), static_cast<uint64>(C->GetClassFlags()))); }
            S->SetBoolField(TEXT("class_loaded"), Loaded(C)); S->SetStringField(TEXT("failed_substage"), TEXT("key_class_identity")); return Reject(ClassPath, S);
        }
        if (!NativeSource(C, S)) { return Reject(ClassPath, S); }
        J->SetObjectField(C->GetPathName(), S);
    }
    for (UClass* C : { UBlackboardData::StaticClass(), UAISystem::StaticClass(), AActor::StaticClass(), UObject::StaticClass() })
    { auto S = MakeShared<FJsonObject>(); if (!NativeSource(C, S)) { return Reject(C->GetPathName(), S); } J->SetObjectField(C->GetPathName(), S); }
    const auto* Config = Cast<UAISystem>(UAISystem::StaticClass()->GetDefaultObject(false));
    if (!Config)
    {
        J->SetStringField(TEXT("failed_class"), UAISystem::StaticClass()->GetPathName()); J->SetStringField(TEXT("failed_substage"), TEXT("ai_config_existing_cdo_cast"));
        J->SetBoolField(TEXT("source_complete"), false); return false;
    }
    J->SetBoolField(TEXT("ai_config_add_blackboard_self_key"), Config->bAddBlackboardSelfKey); return true;
}
bool EnumSource(UEnum* E, TSharedRef<FJsonObject>& J)
{
    if (!Loaded(E) || E->IsEditorOnly() || E->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)
        || E->GetOutermost()->IsDirty() || E->NumEnums() < 1 || E->NumEnums() > 1024) { return false; }
    const FString PackageName = E->GetOutermost()->GetName();
    const bool Native = E->GetClass() == UEnum::StaticClass() && E->GetOuter() == E->GetOutermost()
        && E->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn)
        && (PackageName == TEXT("/Script/CoreUObject") || PackageName == TEXT("/Script/Engine") || PackageName == TEXT("/Script/AIModule"));
    const bool Asset = E->GetClass() == UUserDefinedEnum::StaticClass() && E->GetOuter() == E->GetOutermost()
        && PackageName.StartsWith(TEXT("/Game/")) && !E->HasAnyFlags(RF_Transient)
        && !E->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn | PKG_EditorOnly | PKG_PlayInEditor);
    if (!Native && !Asset) { return false; }
    J = Identity(E); TArray<TSharedPtr<FJsonValue>> Rows;
    for (int32 I = 0; I < E->NumEnums(); ++I)
    {
        const FString Name = E->GetNameStringByIndex(I); const bool Hidden = E->HasMetaData(TEXT("Hidden"), I);
        const int64 Value = E->GetValueByIndex(I);
        if (!Hidden && !Name.EndsWith(TEXT("_MAX")) && (Value < 0 || Value > MAX_uint8)) { return false; }
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("name"), Name); Row->SetStringField(TEXT("value"), FString::Printf(TEXT("%lld"), Value));
        Row->SetBoolField(TEXT("hidden"), Hidden); Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    J->SetArrayField(TEXT("entries"), Rows); J->SetBoolField(TEXT("package_dirty"), E->GetOutermost()->IsDirty());
    auto P = MakeShared<FJsonObject>(); if (!Properties(E, {}, P)) { return false; } J->SetObjectField(TEXT("properties"), P);
    if (Asset) { auto D = MakeShared<FJsonObject>(); if (!Disk(PackageName, D) || !D->GetBoolField(TEXT("exists"))) { return false; } J->SetObjectField(TEXT("disk"), D); }
    return true;
}
// JE Repair1: bounded plain-JSON diagnostics; never query UObjects after a guard failure.
bool GeneratedNativeDifference(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B,
    const FString& Path,const TSharedRef<FJsonObject>& Out,int32 Depth,int32& Left)
{
    const auto Set=[&](const TCHAR* Why)
    {
        Out->SetStringField(TEXT("path"),Path.Left(2048));Out->SetStringField(TEXT("reason"),Why);
        const auto Leaf=[](const TSharedPtr<FJsonValue>& V)->FString
        {if(!V.IsValid()){return TEXT("<missing>");}switch(V->Type){case EJson::String:return V->AsString().Left(1024);case EJson::Number:return FString::SanitizeFloat(V->AsNumber());case EJson::Boolean:return V->AsBool()?TEXT("true"):TEXT("false");case EJson::Null:return TEXT("null");case EJson::Object:return TEXT("<object>");case EJson::Array:return TEXT("<array>");default:return TEXT("<none>");}};
        Out->SetStringField(TEXT("expected"),Leaf(A));Out->SetStringField(TEXT("actual"),Leaf(B));
        Out->SetNumberField(TEXT("expected_type"),A.IsValid()?static_cast<int32>(A->Type):-1);Out->SetNumberField(TEXT("actual_type"),B.IsValid()?static_cast<int32>(B->Type):-1);
    };
    if(--Left<0||Depth>32){Set(TEXT("diagnostic_bound"));Out->SetBoolField(TEXT("complete"),false);return true;}
    if(!A.IsValid()||!B.IsValid()||A->Type!=B->Type){Set(TEXT("missing_or_type"));return true;}
    if(A->Type==EJson::Object)
    {
        const auto X=A->AsObject(),Y=B->AsObject();if(!X.IsValid()||!Y.IsValid()){Set(TEXT("invalid_object"));return true;}
        TArray<FString> Keys;for(const auto& P:X->Values){Keys.Add(FString(P.Key.ToView()));}Keys.Sort([](const FString& L,const FString& R){return L.Compare(R,ESearchCase::CaseSensitive)<0;});
        for(const auto& Key:Keys){TSharedPtr<FJsonValue> Expected,Current;for(const auto& P:X->Values){if(FString(P.Key.ToView()).Equals(Key,ESearchCase::CaseSensitive)){Expected=P.Value;break;}}for(const auto& P:Y->Values){if(FString(P.Key.ToView()).Equals(Key,ESearchCase::CaseSensitive)){Current=P.Value;break;}}
            if(GeneratedNativeDifference(Expected,Current,Path+TEXT("/")+Key,Out,Depth+1,Left)){return true;}}
        for(const auto& P:Y->Values){const FString Key(P.Key.ToView());bool Found=false;for(const auto& Q:X->Values){Found|=Key.Equals(FString(Q.Key.ToView()),ESearchCase::CaseSensitive);}if(!Found){return GeneratedNativeDifference(nullptr,P.Value,Path+TEXT("/")+Key,Out,Depth+1,Left);}}
        return false;
    }
    if(A->Type==EJson::Array){const auto& X=A->AsArray();const auto& Y=B->AsArray();if(X.Num()!=Y.Num()){Set(TEXT("array_count"));Out->SetNumberField(TEXT("expected_count"),X.Num());Out->SetNumberField(TEXT("actual_count"),Y.Num());return true;}
        for(int32 I=0;I<X.Num();++I){if(GeneratedNativeDifference(X[I],Y[I],Path+TEXT("/")+FString::FromInt(I),Out,Depth+1,Left)){return true;}}return false;}
    const bool Same=A->Type==EJson::String?A->AsString().Equals(B->AsString(),ESearchCase::CaseSensitive):
        A->Type==EJson::Number?A->AsNumber()==B->AsNumber():A->Type==EJson::Boolean?A->AsBool()==B->AsBool():A->Type==EJson::Null;
    if(!Same){Set(TEXT("leaf_value"));}return !Same;
}
TSharedRef<FJsonObject> GeneratedNativeDiff(const TSharedRef<FJsonObject>& A,const TSharedRef<FJsonObject>& B,const TCHAR* Boundary)
{
    auto D=MakeShared<FJsonObject>();D->SetStringField(TEXT("boundary"),Boundary);D->SetBoolField(TEXT("complete"),true);int32 Left=65536;
    if(!GeneratedNativeDifference(MakeShared<FJsonValueObject>(A),MakeShared<FJsonValueObject>(B),TEXT("$"),D,0,Left))
    {D->SetStringField(TEXT("path"),TEXT("$"));D->SetStringField(TEXT("reason"),TEXT("serialized_order_only"));}
    D->SetStringField(TEXT("scope"),TEXT("First literal snapshot difference only; no new UObject lookup. Values limited to1024 chars; path2048/depth32/65536 visits."));
    return D;
}
// JE: concrete readonly generated-Actor source protection, active only when the
// selected/requested Object/Class reference is non-native. It is not a new executor.
struct FGeneratedBaseScope;
FGeneratedBaseScope* GeneratedCurrent=nullptr;
struct FGeneratedBaseScope
{
    FResult& R;FGeneratedBaseScope* Previous;bool Active=false,Stopped=false,Preparing=false,PlanningRows=false;
    TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;
    struct FIdentity{TStrongObjectPtr<UObject> Object;UClass* Class=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;FString Path;EObjectFlags Flags=RF_NoFlags;};
    struct FRow{TStrongObjectPtr<UBlackboardKeyType> Key;UClass* Class=nullptr;UClass* Base=nullptr;UEnum* Enum=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;FString Name,Path,Fields;bool Synced=false;EObjectFlags Flags=RF_NoFlags;};
    struct FOwner{TStrongObjectPtr<UBlackboardData> Board;UBlackboardData* Parent=nullptr;UPackage* Package=nullptr;FString Path,Fields;EObjectFlags Flags=RF_NoFlags;uint32 PackageFlags=0;TArray<FRow> Own,Inherited;};
    struct FSourceRole{UBlueprint* BP=nullptr;UClass* Generated=nullptr;UClass* Skeleton=nullptr;UClass* Parent=nullptr;UObject* CDO=nullptr;UObject* SkeletonCDO=nullptr;USimpleConstructionScript* SCS=nullptr;UClass* SkeletonParent=nullptr;bool ObjectRoot=false;EBlueprintStatus Status=BS_Unknown;EClassFlags Flags=CLASS_None,SkeletonFlags=CLASS_None;};
    struct FSourcePackage{TStrongObjectPtr<UPackage> Package;TArray<UObject*> Exports;FString DiskText;uint32 Flags=0;};
    struct FProof{TStrongObjectPtr<UClass> Class;TSharedRef<FJsonObject> Receipt=MakeShared<FJsonObject>();};
    TArray<FOwner> Owners;TArray<FIdentity> SourceObjects;TArray<FSourceRole> Roles;TArray<FSourcePackage> Packages;TArray<FProof> Proofs;
    struct FConfigured{UBlackboardKeyType* Key=nullptr;UClass* Base=nullptr;UEnum* Enum=nullptr;FString EnumName;};
    TArray<FConfigured> ConfiguredKeys;TArray<TStrongObjectPtr<UObject>> Allocations;TArray<FIdentity> PendingKeys;TArray<FString> SourceFields;FString NativeFields;TStrongObjectPtr<UBlackboardData> Target;
    TSharedRef<FJsonObject> NativeSnapshot=MakeShared<FJsonObject>();TSharedPtr<FJsonObject> ActorBeforeLoad,ActorAfterLoad;
    explicit FGeneratedBaseScope(FResult& In):R(In),Previous(GeneratedCurrent){GeneratedCurrent=this;}
    ~FGeneratedBaseScope(){GeneratedCurrent=Previous;}
    bool Stop(const TCHAR* Code,const TCHAR* Why)
    {if(!Stopped){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("code"),Code);J->SetStringField(TEXT("stage"),R.Stage);R.Data->SetObjectField(TEXT("generated_base_first_failure"),J);Fail(R,Code,Why);}R.bKnown=false;R.bPost=R.bSuccess=false;R.Data->SetBoolField(TEXT("generated_source_scope_complete"),false);Stopped=true;return false;}
    bool Context()
    {
        if(!Active){return true;}if(Stopped){return false;}
        if(!IsEditorGameThread()||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||!GEditor||GEditor!=Editor.Get()||!Loaded(Editor.Get())||IsPlaySessionActive()
            ||!Loaded(World.Get())||World->WorldType!=EWorldType::Editor||World->bIsTearingDown)
        {return Stop(TEXT("blackboard_generated_context_changed"),TEXT("Captured idle Editor/world must remain exact; no next getter/writer/save/retry."));}
        return GEditor->GetEditorWorldContext().World()==World.Get()||Stop(TEXT("blackboard_generated_world_changed"),TEXT("Editor world changed during generated reference operation."));
    }
    bool PinSource(UObject* O)
    {
        if(!Context()||!Loaded(O)||!Loaded(O->GetClass())||(O->GetOuter()&&!Loaded(O->GetOuter()))||!Loaded(O->GetPackage())){return Stop(TEXT("blackboard_generated_source_invalid"),TEXT("Source identity is not currently readable."));}
        for(const auto& I:SourceObjects){if(I.Object.Get()==O){return true;}}if(SourceObjects.Num()>=2048){return Stop(TEXT("blackboard_generated_source_bound"),TEXT("Bounded source export set exceeded."));}
        FIdentity I;I.Object.Reset(O);I.Class=O->GetClass();I.Outer=O->GetOuter();I.Package=O->GetPackage();I.Path=O->GetPathName();I.Flags=O->GetFlags();SourceObjects.Add(MoveTemp(I));return true;
    }
    // ADK: pure UObject-root sources share existing readonly fields/export/disk
    // proof. Resolve no source, create no CDO/SCS, and hold exact resident roles
    // before calling the shared Object-parent inspector.
    template<class T,class U> static bool SlotSame(const TObjectPtr<T>& Slot,U* Expected)
    {return Slot.IsResolved()&&Slot.GetHandle()==TObjectPtr<T>(Expected).GetHandle();}
    template<class T> bool HoldSlot(const TObjectPtr<T>& Slot,T*& Out)
    {
        Out=nullptr;if(!Slot.IsResolved())return false;if(SlotSame(Slot,static_cast<T*>(nullptr)))return true;
        UObject* Found=StaticFindObject(nullptr,nullptr,*Slot.GetPathName());
        if(!Loaded(Found)||!Found->IsA<T>()||!SlotSame(Slot,static_cast<T*>(Found))||!PinSource(Found))return false;
        Out=static_cast<T*>(Found);return true;
    }
    bool ObjectSuper(UClass* Class,UClass*& Out) const
    {
        Out=nullptr;if(!Loaded(Class))return false;
        const FObjectProperty* P=FindFProperty<FObjectProperty>(UStruct::StaticClass(),TEXT("SuperStruct"));
        if(!P||P->GetClass()!=FObjectProperty::StaticClass()||P->GetOwnerStruct()!=UStruct::StaticClass()||P->GetOwnerProperty()!=P||P->ArrayDim!=1
            ||P->ElementSize!=sizeof(TObjectPtr<UStruct>)||P->GetSize()!=sizeof(TObjectPtr<UStruct>)||P->GetMinAlignment()!=alignof(TObjectPtr<UStruct>)
            ||P->GetOffset_ForInternal()<0||int64(P->GetOffset_ForInternal())+P->GetSize()>UStruct::StaticClass()->GetPropertiesSize()||P->HasSetterOrGetter()||!SlotSame(P->PropertyClass,UStruct::StaticClass()))return false;
        const auto Slot=P->GetObjectPtrPropertyValue(P->ContainerPtrToValuePtr<void>(Class));
        if(!Slot.IsResolved())return false;if(SlotSame(Slot,static_cast<UObject*>(nullptr)))return true;
        auto* Found=StaticFindObject(nullptr,nullptr,*Slot.GetPathName());
        if(!Loaded(Found)||!Found->IsA<UClass>()||!SlotSame(Slot,Found))return false;Out=static_cast<UClass*>(Found);return true;
    }
    bool PackageExportsCurrent(const FSourcePackage& Pack)
    {
        if(!Raw())return false;TArray<UObject*> Now;GetObjectsWithPackage(Pack.Package.Get(),Now,EGetObjectsFlags::IncludeNestedObjects);
        if(!Raw()||Now.Num()!=Pack.Exports.Num())return Stop(TEXT("blackboard_generated_source_exports_changed"),TEXT("Readonly source package export inventory changed across a source SDK callback; no next observer/writer/save."));
        for(UObject* O:Pack.Exports)if(!Now.Contains(O))return Stop(TEXT("blackboard_generated_source_exports_changed"),TEXT("Original strongly held source export left its captured package."));
        return true;
    }
    bool BindObjectRole(UBlueprint* B,UClass* G,FSourceRole& Q)
    {
        Q.BP=B;Q.Generated=G;Q.ObjectRoot=true;UClass* ActualGenerated=nullptr;UClass* ActualParent=nullptr;UObject* SkeletonGenerator=nullptr;
        if(!HoldSlot(B->GeneratedClass.GetGCPtr(),ActualGenerated)||ActualGenerated!=G||!HoldSlot(B->SkeletonGeneratedClass.GetGCPtr(),Q.Skeleton)||!Loaded(Q.Skeleton)
            ||!HoldSlot(B->ParentClass.GetGCPtr(),Q.Parent)||!Loaded(Q.Parent)||!HoldSlot(B->SimpleConstructionScript,Q.SCS)
            ||!HoldSlot(G->ClassDefaultObject,Q.CDO)||!HoldSlot(Q.Skeleton->ClassDefaultObject,Q.SkeletonCDO)||!HoldSlot(Q.Skeleton->ClassGeneratedBy,SkeletonGenerator)||SkeletonGenerator!=B
            ||!ObjectSuper(G,ActualParent)||ActualParent!=Q.Parent||!ObjectSuper(Q.Skeleton,Q.SkeletonParent)||(Q.SkeletonParent&&!PinSource(Q.SkeletonParent)))
            return Stop(TEXT("blackboard_generated_object_roles_pending"),TEXT("Existing resolved generated/skeleton/actual superclass and nullable CDO/SCS roles required; none are created."));
        UPackage* P=G->GetPackage();
        if(Q.Skeleton->GetClass()!=UBlueprintGeneratedClass::StaticClass()||Q.Skeleton->GetOuter()!=P
            ||(Q.CDO&&(Q.CDO->GetClass()!=G||Q.CDO->GetPackage()!=P||!Q.CDO->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject)))
            ||(Q.SkeletonCDO&&(Q.SkeletonCDO->GetClass()!=Q.Skeleton||Q.SkeletonCDO->GetPackage()!=P||!Q.SkeletonCDO->HasAnyFlags(RF_ClassDefaultObject))))
            return Stop(TEXT("blackboard_generated_source_defaults_invalid"),TEXT("Pure Object defaults must be absent or current live native default roles in the source package. Actual Outer is held, not assumed equal to package."));
        Q.Status=B->Status;Q.Flags=G->GetClassFlags();Q.SkeletonFlags=Q.Skeleton->GetClassFlags();return true;
    }
    bool ObjectRoleCurrent(const FSourceRole& Q)
    {
        UClass* Parent=nullptr;UClass* SkeletonParent=nullptr;
        return SlotSame(Q.BP->GeneratedClass.GetGCPtr(),Q.Generated)&&SlotSame(Q.BP->SkeletonGeneratedClass.GetGCPtr(),Q.Skeleton)
            &&SlotSame(Q.BP->ParentClass.GetGCPtr(),Q.Parent)&&SlotSame(Q.BP->SimpleConstructionScript,Q.SCS)&&SlotSame(Q.Generated->ClassGeneratedBy,Q.BP)&&SlotSame(Q.Skeleton->ClassGeneratedBy,Q.BP)
            &&SlotSame(Q.Generated->ClassDefaultObject,Q.CDO)&&SlotSame(Q.Skeleton->ClassDefaultObject,Q.SkeletonCDO)
            &&ObjectSuper(Q.Generated,Parent)&&Parent==Q.Parent&&ObjectSuper(Q.Skeleton,SkeletonParent)&&SkeletonParent==Q.SkeletonParent
            &&Q.BP->Status==Q.Status&&Q.BP->BlueprintType==BPTYPE_Normal&&!Q.BP->bBeingCompiled&&!Q.BP->bQueuedForCompilation&&!Q.BP->bSuppressStructurallyModified&&!Q.BP->bIsRegeneratingOnLoad
            &&Q.Generated->GetClassFlags()==Q.Flags&&Q.Skeleton->GetClassFlags()==Q.SkeletonFlags&&!Q.Generated->GetPackage()->IsDirty();
    }
    bool ReadRows(const TArray<FBlackboardEntry>& Entries,TArray<FRow>& Out)
    {
        if(!Context()||Entries.Num()>MaxKeys){return false;}TArray<FBlackboardEntry> Copy=Entries;
        for(const auto& E:Copy)
        {
            UBlackboardKeyType* K=E.KeyType.Get();if(!Loaded(K)||!Loaded(K->GetClass())||K->GetClass()!=KeyClass(KeyTypeName(K->GetClass()).ToLower())||!Loaded(K->GetOuter())||!Loaded(K->GetPackage())){return Stop(TEXT("blackboard_generated_key_invalid"),TEXT("Current native owned key must be readable before source work."));}
            FRow Q;Q.Key.Reset(K);Q.Class=K->GetClass();Q.Outer=K->GetOuter();Q.Package=K->GetPackage();Q.Path=K->GetPathName();Q.Flags=K->GetFlags();Q.Name=E.EntryName.ToString();Q.Synced=E.bInstanceSynced;
            if(auto* O=Cast<UBlackboardKeyType_Object>(K)){Q.Base=O->BaseClass.Get();}else if(auto* C=Cast<UBlackboardKeyType_Class>(K)){Q.Base=C->BaseClass.Get();}else if(auto* U=Cast<UBlackboardKeyType_Enum>(K)){Q.Enum=U->EnumType.Get();}
            Out.Add(MoveTemp(Q));
        }return true;
    }
    bool RowSame(const FRow& A,const FRow& B)const
    {return A.Key.Get()==B.Key.Get()&&A.Class==B.Class&&A.Base==B.Base&&A.Enum==B.Enum&&A.Outer==B.Outer&&A.Package==B.Package&&A.Path.Equals(B.Path,ESearchCase::CaseSensitive)&&A.Name.Equals(B.Name,ESearchCase::CaseSensitive)&&A.Synced==B.Synced&&A.Flags==B.Flags;}
    bool RowsCurrent(const TArray<FBlackboardEntry>& Entries,const TArray<FRow>& Expected)
    {
        if(Entries.Num()!=Expected.Num()){return false;}
        for(int32 I=0;I<Entries.Num();++I)
        {
            const auto& E=Entries[I];const auto& Q=Expected[I];UBlackboardKeyType* K=Q.Key.Get();
            if(E.KeyType.Get()!=K||!Loaded(K)||K->GetClass()!=Q.Class||K->GetOuter()!=Q.Outer||K->GetPackage()!=Q.Package||K->GetFlags()!=Q.Flags||!K->GetPathName().Equals(Q.Path,ESearchCase::CaseSensitive)||!E.EntryName.ToString().Equals(Q.Name,ESearchCase::CaseSensitive)||static_cast<bool>(E.bInstanceSynced)!=Q.Synced){return false;}
            if(auto* O=Cast<UBlackboardKeyType_Object>(K)){if(O->BaseClass.Get()!=Q.Base){return false;}}else if(auto* C=Cast<UBlackboardKeyType_Class>(K)){if(C->BaseClass.Get()!=Q.Base){return false;}}else if(auto* U=Cast<UBlackboardKeyType_Enum>(K)){if(U->EnumType.Get()!=Q.Enum){return false;}}
        }return true;
    }
    bool Raw()
    {
        if(!Active){return true;}if(!Context()){return false;}
        for(const auto& O:Owners){auto* B=O.Board.Get();if(!Loaded(B)||B->GetClass()!=UBlackboardData::StaticClass()||B->GetOuter()!=O.Package||B->GetPackage()!=O.Package||!Loaded(O.Package)||B->Parent.Get()!=O.Parent||B->GetFlags()!=O.Flags||O.Package->GetPackageFlags()!=O.PackageFlags||!B->GetPathName().Equals(O.Path,ESearchCase::CaseSensitive)||FindObject<UBlackboardData>(nullptr,*O.Path)!=B)
            {return Stop(TEXT("blackboard_generated_target_role_changed"),TEXT("Current selected/parent Blackboard identity or ownership changed."));}
            if(!PlanningRows&&(!RowsCurrent(B->Keys,O.Own)||!RowsCurrent(B->ParentKeys,O.Inherited))){return Stop(TEXT("blackboard_generated_key_role_changed"),TEXT("Current ordered key/reference roles changed before the next SDK boundary."));}}
        // R01: a returned but unappended key is not yet protected by an Own row.
        for(const auto& I:PendingKeys){UObject* O=I.Object.Get();if(!Loaded(O)||!Loaded(I.Class)||(I.Outer&&!Loaded(I.Outer))||!Loaded(I.Package)||O->GetClass()!=I.Class||O->GetOuter()!=I.Outer||O->GetPackage()!=I.Package||O->GetFlags()!=I.Flags||!O->GetPathName().Equals(I.Path,ESearchCase::CaseSensitive))
            {return Stop(TEXT("blackboard_generated_pending_key_changed"),TEXT("Returned unappended key changed identity before direct configuration/append."));}}
        for(const auto& Q:ConfiguredKeys){if(auto* O=Cast<UBlackboardKeyType_Object>(Q.Key)){if(O->BaseClass.Get()!=Q.Base){return Stop(TEXT("blackboard_generated_pending_config_changed"),TEXT("Planned Object BaseClass changed before append."));}}else if(auto* C=Cast<UBlackboardKeyType_Class>(Q.Key)){if(C->BaseClass.Get()!=Q.Base){return Stop(TEXT("blackboard_generated_pending_config_changed"),TEXT("Planned Class BaseClass changed before append."));}}else if(auto* E=Cast<UBlackboardKeyType_Enum>(Q.Key)){if(E->EnumType.Get()!=Q.Enum||!E->EnumName.Equals(Q.EnumName,ESearchCase::CaseSensitive)){return Stop(TEXT("blackboard_generated_pending_config_changed"),TEXT("Planned Enum reference/name changed before append."));}}}
        for(const auto& P:Packages){if(!Loaded(P.Package.Get())||P.Package->GetPackageFlags()!=P.Flags||P.Package->IsDirty()){return Stop(TEXT("blackboard_generated_source_package_changed"),TEXT("Readonly source package flags/dirty role changed."));}}
        for(const auto& I:SourceObjects){UObject* O=I.Object.Get();if(!Loaded(O)||!Loaded(I.Class)||(I.Outer&&!Loaded(I.Outer))||!Loaded(I.Package)||O->GetClass()!=I.Class||O->GetOuter()!=I.Outer||O->GetPackage()!=I.Package||O->GetFlags()!=I.Flags||!O->GetPathName().Equals(I.Path,ESearchCase::CaseSensitive))
            {return Stop(TEXT("blackboard_generated_source_identity_changed"),TEXT("Captured readonly source identity changed; stale objects are not observed."));}}
        for(const auto& Q:Roles)
        {if(Q.ObjectRoot){if(!ObjectRoleCurrent(Q))return Stop(TEXT("blackboard_generated_object_role_changed"),TEXT("Pure Object source generated/skeleton/actual parent/nullable CDO/SCS roles changed; no next SDK or retry."));continue;}if(Q.BP->GeneratedClass!=Q.Generated||Q.BP->SkeletonGeneratedClass!=Q.Skeleton||Q.BP->ParentClass!=Q.Parent||Q.BP->SimpleConstructionScript!=Q.SCS||Q.BP->Status!=Q.Status||Q.BP->BlueprintType!=BPTYPE_Normal||Q.BP->bBeingCompiled||Q.BP->bQueuedForCompilation||Q.BP->bSuppressStructurallyModified||Q.BP->bIsRegeneratingOnLoad||Q.Generated->ClassGeneratedBy!=Q.BP||Q.Generated->GetSuperClass()!=Q.Parent||Q.Generated->GetDefaultObject(false)!=Q.CDO||Q.Generated->GetClassFlags()!=Q.Flags||Q.Skeleton->GetDefaultObject(false)!=Q.SkeletonCDO||Q.Skeleton->GetClassFlags()!=Q.SkeletonFlags||Q.Generated->GetPackage()->IsDirty())
            {return Stop(TEXT("blackboard_generated_class_role_changed"),TEXT("Current source Blueprint/generated/skeleton/CDO/SCS role or idle saved state changed."));}}
        bool HasObjectRoles=false;for(const auto& Q:Roles)HasObjectRoles|=Q.ObjectRoot;
        if(HasObjectRoles)
        {
            if(!Context())return false;
            // Existing Actor role getters above may track reads. Reclose only
            // callback-free identity and Object slots before any next SDK call.
            for(const auto& I:SourceObjects){UObject* O=I.Object.Get();if(!Loaded(O)||!Loaded(I.Class)||(I.Outer&&!Loaded(I.Outer))||!Loaded(I.Package)||O->GetClass()!=I.Class||O->GetOuter()!=I.Outer||O->GetPackage()!=I.Package||O->GetFlags()!=I.Flags||!O->GetPathName().Equals(I.Path,ESearchCase::CaseSensitive))return Stop(TEXT("blackboard_generated_source_identity_changed"),TEXT("Readonly source identity changed during another source-role observer."));}
            for(const auto& Pack:Packages)if(!Loaded(Pack.Package.Get())||Pack.Package->GetPackageFlags()!=Pack.Flags||Pack.Package->IsDirty())return Stop(TEXT("blackboard_generated_source_package_changed"),TEXT("Readonly source package changed during another source-role observer."));
            for(const auto& Q:Roles)if(Q.ObjectRoot&&!ObjectRoleCurrent(Q))return Stop(TEXT("blackboard_generated_object_role_changed"),TEXT("Original pure Object slots changed during another source-role observer; no next SDK or retry."));
        }
        return true;
    }
    bool SnapshotFields(TArray<FString>& Out)
    {
        int64 Size=0;for(const auto& I:SourceObjects){if(!Raw()){return false;}auto J=MakeShared<FJsonObject>();if(!Properties(I.Object.Get(),{},J)){return false;}FString V=Text(J);Size+=V.Len();if(Size>8*1024*1024){return Stop(TEXT("blackboard_generated_source_text_bound"),TEXT("Readonly source field proof exceeded its finite bound."));}Out.Add(MoveTemp(V));}
        return Raw();
    }
    bool FieldsEqual(const TArray<FString>& A,const TArray<FString>& B)const
    {if(A.Num()!=B.Num()){return false;}for(int32 I=0;I<A.Num();++I){if(!A[I].Equals(B[I],ESearchCase::CaseSensitive)){return false;}}return true;}
    bool Full()
    {
        if(!Active){return true;}if(!Raw()){return false;}
        if(!Preparing)
        {
            auto N=MakeShared<FJsonObject>();if(!Sources(N)||!Text(N).Equals(NativeFields,ESearchCase::CaseSensitive)){if(!R.Data->HasField(TEXT("generated_native_first_failure_difference"))){R.Data->SetObjectField(TEXT("generated_native_first_failure_difference"),GeneratedNativeDiff(NativeSnapshot,N,TEXT("NativeFields_full_check")));}return Stop(TEXT("blackboard_generated_native_sources_changed"),TEXT("Existing native defaults/config changed during generated reference work."));}
            TArray<FString> Now;if(!SnapshotFields(Now)||!FieldsEqual(SourceFields,Now)){return Stop(TEXT("blackboard_generated_source_fields_changed"),TEXT("Readonly source fields changed after an explicit SDK; no next mutation/save."));}
            for(const auto& P:Packages){TArray<UObject*> Current;GetObjectsWithPackage(P.Package.Get(),Current,EGetObjectsFlags::IncludeNestedObjects);if(Current.Num()!=P.Exports.Num()){return Stop(TEXT("blackboard_generated_source_exports_changed"),TEXT("Readonly source export set changed."));}for(auto* O:P.Exports){if(!Current.Contains(O)){return Stop(TEXT("blackboard_generated_source_exports_changed"),TEXT("Readonly source export left its captured package."));}}
                auto D=MakeShared<FJsonObject>();if(!Disk(P.Package->GetName(),D)||!Text(D).Equals(P.DiskText,ESearchCase::CaseSensitive)){return Stop(TEXT("blackboard_generated_source_disk_changed"),TEXT("Readonly source package disk changed or became unobservable."));}}
            for(const auto& O:Owners)
            {
                auto V=MakeShared<FJsonObject>();if(!Properties(O.Board.Get(),{TEXT("Keys"),TEXT("ParentKeys"),TEXT("bHasSynchronizedKeys"),TEXT("FirstKeyID")},V)||!Text(V).Equals(O.Fields,ESearchCase::CaseSensitive)){return Stop(TEXT("blackboard_generated_unselected_board_changed"),TEXT("Unselected Blackboard fields changed after callback."));}
                for(const auto* Rows:{&O.Own,&O.Inherited}){for(const auto& Q:*Rows){auto K=MakeShared<FJsonObject>();if(!Properties(Q.Key.Get(),{},K)||!Text(K).Equals(Q.Fields,ESearchCase::CaseSensitive)){return Stop(TEXT("blackboard_generated_unselected_key_changed"),TEXT("Current key fields changed after callback."));}}}
            }
        }const bool OK=Raw();if(OK){R.Data->SetBoolField(TEXT("generated_source_scope_complete"),true);}return OK;
    }
    bool FillRowFields(TArray<FRow>& Rows)
    {for(auto& Q:Rows){auto J=MakeShared<FJsonObject>();if(!Properties(Q.Key.Get(),{},J)){return false;}Q.Fields=Text(J);}return Raw();}
    bool Begin(UBlackboardData* B,bool RequestedGenerated)
    {
        TArray<UBlackboardData*> ChainRows;TSet<UBlackboardData*> Seen;
        for(auto* P=B;P;P=P->Parent.Get()){if(!Loaded(P)||Seen.Contains(P)||Seen.Num()>=MaxParents){return false;}Seen.Add(P);ChainRows.Add(P);for(const auto* Rows:{&P->Keys,&P->ParentKeys}){for(const auto& E:*Rows){if(!Loaded(E.KeyType.Get())){return Stop(TEXT("blackboard_generated_key_invalid"),TEXT("Initial current key pointer is not readable."));}UClass* C=nullptr;if(auto* O=Cast<UBlackboardKeyType_Object>(E.KeyType.Get())){C=O->BaseClass.Get();}else if(auto* K=Cast<UBlackboardKeyType_Class>(E.KeyType.Get())){C=K->BaseClass.Get();}if(C&&!Loaded(C)){return Stop(TEXT("blackboard_generated_source_invalid"),TEXT("Initial BaseClass is not readable."));}if(C&&!C->HasAnyClassFlags(CLASS_Native)){RequestedGenerated=true;}}}}
        if(!RequestedGenerated){return true;}Active=true;
        if(!IsEditorGameThread()||!Loaded(GEditor)||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||IsPlaySessionActive()){return Stop(TEXT("blackboard_generated_editor_required"),TEXT("Idle current Editor context required before generated source work."));}
        Editor=GEditor;World=GEditor->GetEditorWorldContext().World();Target.Reset(B);Preparing=true;R.Data->SetNumberField(TEXT("generated_base_load_count"),0);
        for(auto* P:ChainRows)
        {FOwner O;O.Board.Reset(P);O.Parent=P->Parent.Get();O.Package=P->GetPackage();O.Path=P->GetPathName();O.Flags=P->GetFlags();O.PackageFlags=O.Package->GetPackageFlags();if(!ReadRows(P->Keys,O.Own)||!ReadRows(P->ParentKeys,O.Inherited)){return false;}Owners.Add(MoveTemp(O));}
        if(!Raw()){return false;}
        TArray<UClass*> Bases;for(auto& O:Owners){auto V=MakeShared<FJsonObject>();if(!Properties(O.Board.Get(),{TEXT("Keys"),TEXT("ParentKeys"),TEXT("bHasSynchronizedKeys"),TEXT("FirstKeyID")},V)||!FillRowFields(O.Own)||!FillRowFields(O.Inherited)){return false;}O.Fields=Text(V);for(const auto* Rows:{&O.Own,&O.Inherited}){for(const auto& Q:*Rows){if(Q.Base&&!Q.Base->HasAnyClassFlags(CLASS_Native)){Bases.AddUnique(Q.Base);}}}}
        auto Native=MakeShared<FJsonObject>();if(!Sources(Native)){return Stop(TEXT("blackboard_generated_native_sources_invalid"),TEXT("Existing native defaults/config proof is incomplete."));}NativeSnapshot=Native;NativeFields=Text(Native);
        for(auto* C:Bases){if(!BindSource(C)){return false;}}Preparing=false;
        R.Data->SetStringField(TEXT("generated_base_scope"),TEXT("Exact saved-clean Actor or pure UObject-root Blueprint references; source package exports/fields/disk and generated/skeleton/actual parent/nullable existing CDO/SCS roles stay held. Object-root usage validation is pending; no explicit source compile/save/CDO creation or runtime AI execution."));
        return Full();
    }
    bool AfterBaseLoad(UClass* C,const FString& RequestedPath)
    {
        if(!Active){return true;}if(!Raw()){return false;}
        auto Current=MakeShared<FJsonObject>();const bool Complete=Sources(Current);if(!Raw()){return false;}
        if(Complete&&Text(Current).Equals(NativeFields,ESearchCase::CaseSensitive)){return true;}
        auto Difference=GeneratedNativeDiff(NativeSnapshot,Current,TEXT("direct_generated_base_LoadObject_return"));
        Difference->SetBoolField(TEXT("transition_adopted"),false);R.Data->SetObjectField(TEXT("generated_native_load_difference"),Difference);
        const auto Reject=[&](){if(!R.Data->HasField(TEXT("generated_native_first_failure_difference"))){R.Data->SetObjectField(TEXT("generated_native_first_failure_difference"),Difference);}return Stop(TEXT("blackboard_generated_native_sources_changed"),TEXT("Source load changed native proof beyond the exact Actor replication-setup flag transition; no declaration/save."));};
        // Class.cpp5609-5623/5696: lazy replication setup recurses into SuperClass,
        // changes NetFields/ClassReps and adds this flag. Those caches/global effects
        // are NOT measured here. Only this explicit load-return snapshot leaf is adopted.
        UClass* Actor=AActor::StaticClass();const FString ActorPath=TEXT("/Script/Engine.Actor");
        if(!Complete||!Loaded(C)||C->GetClass()!=UBlueprintGeneratedClass::StaticClass()||C->HasAnyClassFlags(CLASS_Native)||!C->GetPathName().Equals(RequestedPath,ESearchCase::CaseSensitive)
            ||!RequestedPath.StartsWith(TEXT("/Game/"))||!C->IsChildOf(Actor)||!Loaded(Actor)||FindObject<UClass>(nullptr,*ActorPath)!=Actor){return Reject();}
        const TSharedPtr<FJsonObject>* Old=nullptr;const TSharedPtr<FJsonObject>* Now=nullptr;
        if(!NativeSnapshot->TryGetObjectField(ActorPath,Old)||!Current->TryGetObjectField(ActorPath,Now)||!Old||!Now||!Old->IsValid()||!Now->IsValid()){return Reject();}
        FString BeforeFlags,AfterFlags;if(!(*Old)->TryGetStringField(TEXT("class_flags"),BeforeFlags)||!(*Now)->TryGetStringField(TEXT("class_flags"),AfterFlags)){return Reject();}
        const uint64 Before=FCString::Strtoui64(*BeforeFlags,nullptr,10),After=FCString::Strtoui64(*AfterFlags,nullptr,10),Bit=static_cast<uint64>(CLASS_ReplicationDataIsSetUp);
        if(!BeforeFlags.Equals(FString::Printf(TEXT("%llu"),Before),ESearchCase::CaseSensitive)||!AfterFlags.Equals(FString::Printf(TEXT("%llu"),After),ESearchCase::CaseSensitive)
            ||(Before&Bit)!=0||After!=(Before|Bit)||static_cast<uint64>(Actor->GetClassFlags())!=After){return Reject();}
        auto ExpectedActor=MakeShared<FJsonObject>();for(const auto& P:(*Old)->Values){ExpectedActor->SetField(P.Key,P.Value);}ExpectedActor->SetStringField(TEXT("class_flags"),AfterFlags);
        auto Expected=MakeShared<FJsonObject>();for(const auto& P:NativeSnapshot->Values){Expected->SetField(P.Key,P.Value);}Expected->SetObjectField(ActorPath,ExpectedActor);
        if(!Text(Expected).Equals(Text(Current),ESearchCase::CaseSensitive)){return Reject();}
        ActorBeforeLoad=*Old;ActorAfterLoad=ExpectedActor;NativeSnapshot=Expected;NativeFields=Text(Expected);
        Difference->SetBoolField(TEXT("transition_adopted"),true);Difference->SetStringField(TEXT("admitted_bit"),TEXT("CLASS_ReplicationDataIsSetUp=0x800"));
        Difference->SetBoolField(TEXT("replication_cache_contents_verified"),false);R.Data->SetBoolField(TEXT("generated_load_actor_replication_setup_adopted"),true);
        return Full();
    }
    bool BindSource(UClass* C)
    {
        for(const auto& P:Proofs){if(P.Class.Get()==C){return Full();}}
        if(!Raw()||!Loaded(C)||C->GetClass()!=UBlueprintGeneratedClass::StaticClass()){return Stop(TEXT("blackboard_generated_actor_class_required"),TEXT("Only exact current generated Actor or pure UObject-root classes extend BaseClass support."));}
        const TStrongObjectPtr<UClass> SelectedSource(C);const bool ObjectRoot=!C->IsChildOf(AActor::StaticClass());if(!Raw())return false;
        TGuardValue<bool> Enroll(Preparing,true);TSet<UClass*> Seen;UClass* G=C;
        for(;G&&!G->HasAnyClassFlags(CLASS_Native);)
        {
            if(!Raw()||Seen.Contains(G)||Seen.Num()>=16){return Stop(TEXT("blackboard_generated_source_chain_invalid"),TEXT("Bounded current generated Actor ancestry required."));}Seen.Add(G);
            UObject* Generator=nullptr;if(ObjectRoot&&!HoldSlot(G->ClassGeneratedBy,Generator))return Stop(TEXT("blackboard_generated_object_roles_pending"),TEXT("Resolved current generator required before source inspection."));
            UBlueprint* BP=ObjectRoot?Cast<UBlueprint>(Generator):Cast<UBlueprint>(G->ClassGeneratedBy);UPackage* P=G->GetPackage();
            if(!Loaded(BP)||!PinSource(G)||!PinSource(BP)||!PinSource(P)){return false;}FSourceRole Q;FSourcePackage Pack;
            if(ObjectRoot)
            {
                if(!BindObjectRole(BP,G,Q))return false;
                Pack.Package.Reset(P);Pack.Flags=P->GetPackageFlags();GetObjectsWithPackage(P,Pack.Exports,EGetObjectsFlags::IncludeNestedObjects);
                if(Pack.Exports.Num()>1024||!PinSource(P))return false;for(auto* O:Pack.Exports)if(!PinSource(O))return false;
                if(!ObjectRoleCurrent(Q)||!PackageExportsCurrent(Pack))return Stop(TEXT("blackboard_generated_object_role_changed"),TEXT("Original pure Object roles/exports changed before authoritative-class observation."));
            }
            UClass* Authority=ObjectRoot?G:G->GetAuthoritativeClass();if(!Raw()||(ObjectRoot&&(!ObjectRoleCurrent(Q)||!PackageExportsCurrent(Pack)))){return Stop(TEXT("blackboard_generated_object_role_changed"),TEXT("Original pure Object roles changed across the authoritative-class SDK observation."));}
            if(!Loaded(BP)||BP->GetClass()!=UBlueprint::StaticClass()||!Loaded(P)||P==Target->GetPackage()||!P->GetName().StartsWith(TEXT("/Game/"))||P->IsDirty()||P->HasAnyPackageFlags(PKG_CompiledIn|PKG_PlayInEditor|PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly)||BP->GetOuter()!=P||G->GetOuter()!=P||(ObjectRoot?!SlotSame(BP->GeneratedClass.GetGCPtr(),G):BP->GeneratedClass!=G)||G->GetClass()!=UBlueprintGeneratedClass::StaticClass()||Authority!=G
                ||G->HasAnyClassFlags(CLASS_Native|CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)||G->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject)
                ||BP->BlueprintType!=BPTYPE_Normal||(BP->Status!=BS_UpToDate&&BP->Status!=BS_UpToDateWithWarnings)||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bSuppressStructurallyModified||BP->bIsRegeneratingOnLoad)
            {return Stop(TEXT("blackboard_generated_source_not_ready"),TEXT("Exact current saved-clean normal Actor or pure UObject-root Blueprint with no active/queued regeneration required."));}
            bool Already=false;UClass* ExistingParent=nullptr;for(const auto& ExistingRole:Roles){if(ExistingRole.BP==BP){Already=true;ExistingParent=ExistingRole.Parent;}}if(Already){G=ExistingParent;continue;}
            if(ObjectRoot){if(!ObjectRoleCurrent(Q))return Stop(TEXT("blackboard_generated_object_role_changed"),TEXT("Original pure Object roles changed during source admission."));}else
            {
            UClass* S=BP->SkeletonGeneratedClass;UObject* D=G->GetDefaultObject(false);UObject* SD=Loaded(S)?S->GetDefaultObject(false):nullptr;
            if(!Loaded(S)||S->GetClass()!=UBlueprintGeneratedClass::StaticClass()||S->ClassGeneratedBy!=BP||S->GetOuter()!=P||!Loaded(D)||D->GetClass()!=G||D->GetOuter()!=P||!D->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject)||(SD&&(!Loaded(SD)||SD->GetClass()!=S||SD->GetOuter()!=P||!SD->HasAnyFlags(RF_ClassDefaultObject)))||!Loaded(BP->ParentClass)||!Loaded(BP->SimpleConstructionScript))
            {return Stop(TEXT("blackboard_generated_source_defaults_invalid"),TEXT("Existing current generated/skeleton/default/SCS roles required; no default or asset creation."));}
            Q.BP=BP;Q.Generated=G;Q.Skeleton=S;Q.Parent=BP->ParentClass;Q.CDO=D;Q.SkeletonCDO=SD;Q.SCS=BP->SimpleConstructionScript;Q.Status=BP->Status;Q.Flags=G->GetClassFlags();Q.SkeletonFlags=S->GetClassFlags();
            }
            if(!ObjectRoot){Pack.Package.Reset(P);Pack.Flags=P->GetPackageFlags();GetObjectsWithPackage(P,Pack.Exports,EGetObjectsFlags::IncludeNestedObjects);if(Pack.Exports.Num()>1024||!PinSource(P)){return false;}for(auto* O:Pack.Exports){if(!PinSource(O)){return false;}}}
            if(!PinSource(BP)||!PinSource(G)||!PinSource(Q.Skeleton)||(Q.CDO&&!PinSource(Q.CDO))||(Q.SkeletonCDO&&!PinSource(Q.SkeletonCDO))||(Q.SCS&&!PinSource(Q.SCS))||!PinSource(Q.Parent)){return false;}
            auto File=MakeShared<FJsonObject>();if(!Disk(P->GetName(),File)||!File->GetBoolField(TEXT("exists"))){return Stop(TEXT("blackboard_generated_source_not_saved"),TEXT("Existing persisted source package required."));}Pack.DiskText=Text(File);Packages.Add(MoveTemp(Pack));Roles.Add(Q);G=Q.Parent;
        }
        if(ObjectRoot&&G!=UObject::StaticClass())return Stop(TEXT("blackboard_generated_object_native_root_pending"),TEXT("Only generated hierarchy ending at native CoreUObject.Object is admitted; Component/Widget/DataAsset/custom native roots remain incomplete."));
        TArray<FString> Before;if(!SnapshotFields(Before)){return false;}for(int32 I=0;I<SourceFields.Num();++I){if(!Before.IsValidIndex(I)||!SourceFields[I].Equals(Before[I],ESearchCase::CaseSensitive)){return Stop(TEXT("blackboard_generated_prior_source_changed"),TEXT("Previously admitted readonly source changed before another source was enrolled."));}}
        if(ObjectRoot)
        {
            // The first source fields/roles/exports predate every authoritative
            // getter, including getters for generated ancestors. Returned state
            // never becomes a replacement baseline after a tracking callback.
            for(const auto& Role:Roles)if(Role.ObjectRoot)
            {
                if(!Raw())return false;UClass* Authority=Role.Generated->GetAuthoritativeClass();
                if(!Raw()||Authority!=Role.Generated)return Stop(TEXT("blackboard_generated_object_authority_changed"),TEXT("Authoritative-class observer returned outside the original held pure Object roles."));
                for(const auto& Pack:Packages)if(!PackageExportsCurrent(Pack))return false;
                TArray<FString> AfterAuthority;if(!SnapshotFields(AfterAuthority)||!FieldsEqual(Before,AfterAuthority))return Stop(TEXT("blackboard_generated_source_fields_changed"),TEXT("Readonly source fields changed across authoritative-class observation; first pre-call values remain authoritative."));
            }
            for(const auto& Pack:Packages)if(!PackageExportsCurrent(Pack))return false;
        }
        const auto Audit=ObjectRoot?GeneratedComponent::InspectSavedObjectParent(C):GeneratedComponent::InspectSavedActorParent(C);if(!Raw()){return false;}
        if(ObjectRoot){for(const auto& Pack:Packages)if(!PackageExportsCurrent(Pack))return false;}
        TArray<FString> After;if(!Audit->GetBoolField(TEXT("success"))||!SnapshotFields(After)||!FieldsEqual(Before,After)){return Stop(TEXT("blackboard_generated_source_audit_failed"),TEXT("Existing saved Actor/pure Object dependency/source/nullable-default audit failed or changed its source; no declaration/save."));}
        SourceFields=MoveTemp(After);FProof P;P.Class.Reset(C);P.Receipt->SetStringField(TEXT("kind"),ObjectRoot?TEXT("saved_current_object_blueprint"):TEXT("saved_current_actor_blueprint"));P.Receipt->SetStringField(TEXT("class_path"),C->GetPathName());P.Receipt->SetObjectField(ObjectRoot?TEXT("readonly_saved_object_audit"):TEXT("readonly_saved_actor_audit"),Audit);Proofs.Add(MoveTemp(P));R.Data->SetBoolField(TEXT("custom_base_classes_supported"),true);
        auto Summary=MakeShared<FJsonObject>();Summary->SetNumberField(TEXT("held_source_objects"),SourceObjects.Num());Summary->SetNumberField(TEXT("held_source_packages"),Packages.Num());Summary->SetBoolField(TEXT("explicit_source_compile"),false);Summary->SetBoolField(TEXT("explicit_source_save"),false);Summary->SetBoolField(TEXT("explicit_cdo_creation"),false);R.Data->SetObjectField(TEXT("generated_source_protection"),Summary);
        return Raw();
    }
    bool Source(UClass* C,TSharedRef<FJsonObject>& J)
    {if(!Full()){return false;}for(const auto& P:Proofs){if(P.Class.Get()==C){J=P.Receipt;return true;}}return Stop(TEXT("blackboard_generated_reference_not_captured"),TEXT("Generated reference was not admitted from the initial selected keys or explicit requested BaseClass."));}
    bool AdoptRefresh()
    {
        if(!Active){return true;}if(!Context()){return false;}TGuardValue<bool> Plan(PlanningRows,true);
        for(auto& O:Owners)
        {
            if(O.Board.Get()!=Target.Get()){continue;}TArray<FRow> Own,Inherited;if(!ReadRows(O.Board->Keys,Own)||!ReadRows(O.Board->ParentKeys,Inherited)||!FillRowFields(Own)||!FillRowFields(Inherited)){return false;}
            TArray<const FRow*> A,B;for(const auto& Q:O.Own){if(FName(*Q.Name)!=FBlackboard::KeySelf){A.Add(&Q);}}for(const auto& Q:Own){if(FName(*Q.Name)!=FBlackboard::KeySelf){B.Add(&Q);}else if(Q.Class!=UBlackboardKeyType_Object::StaticClass()||Q.Base!=AActor::StaticClass()||Q.Synced||Q.Outer!=Target.Get()){return Stop(TEXT("blackboard_generated_persistent_key_invalid"),TEXT("Only the exact native SelfActor cache transition is planned."));}}
            if(A.Num()!=B.Num()){return Stop(TEXT("blackboard_generated_cache_changed_keys"),TEXT("Cache callback changed nonpersistent key membership."));}for(int32 I=0;I<A.Num();++I){if(!RowSame(*A[I],*B[I])||!A[I]->Fields.Equals(B[I]->Fields,ESearchCase::CaseSensitive)){return Stop(TEXT("blackboard_generated_cache_changed_key"),TEXT("Cache callback changed an existing nonpersistent key."));}}
            for(const auto& Q:O.Own){if(FName(*Q.Name)==FBlackboard::KeySelf){const FRow* Current=nullptr;for(const auto& X:Own){if(FName(*X.Name)==FBlackboard::KeySelf){Current=&X;break;}}if(Current&&(!RowSame(Q,*Current)||!Q.Fields.Equals(Current->Fields,ESearchCase::CaseSensitive))){return Stop(TEXT("blackboard_generated_self_key_changed"),TEXT("An existing persistent key changed identity or fields."));}}}
            for(const auto& Q:Inherited){bool Match=false;for(int32 I=1;I<Owners.Num();++I){for(const auto& X:Owners[I].Own){if(RowSame(Q,X)&&Q.Fields.Equals(X.Fields,ESearchCase::CaseSensitive)){Match=true;}}}if(!Match){return Stop(TEXT("blackboard_generated_inherited_key_changed"),TEXT("Refreshed inherited row must match an exact held parent key."));}}
            O.Own=MoveTemp(Own);O.Inherited=MoveTemp(Inherited);
        }return true;
    }
    bool HoldAllocation(UBlackboardKeyType* Key,UBlackboardData* B,UClass* KC)
    {if(!Active){return true;}if(!Context()||!Loaded(Key)||Key->GetClass()!=KC||Key->GetOuter()!=B||Key->GetPackage()!=B->GetPackage()){return Stop(TEXT("blackboard_generated_key_allocation_invalid"),TEXT("Returned key must have exact current selected ownership before field writes."));}FIdentity I;I.Object.Reset(Key);I.Class=KC;I.Outer=B;I.Package=B->GetPackage();I.Path=Key->GetPathName();I.Flags=Key->GetFlags();PendingKeys.Add(MoveTemp(I));return Full();}
    bool Append(const FBlackboardEntry& Entry,UClass* ExpectedBase,UEnum* ExpectedEnum)
    {if(!Active){return true;}if(!Raw()){return false;}bool Pending=false;for(const auto& I:PendingKeys){Pending|=I.Object.Get()==Entry.KeyType.Get();}if(!Pending){return Stop(TEXT("blackboard_generated_pending_key_missing"),TEXT("Only the exact returned pending key can enter the planned Own row."));}
     FConfigured Q;Q.Key=Entry.KeyType.Get();Q.Base=ExpectedBase;Q.Enum=ExpectedEnum;Q.EnumName=ExpectedEnum?ExpectedEnum->GetName():FString();ConfiguredKeys.Add(Q);
     if(!Full()){return false;}TArray<FBlackboardEntry> E;E.Add(Entry);TArray<FRow> Rows;if(!ReadRows(E,Rows)||!FillRowFields(Rows)){return false;}if(!Raw()||Rows[0].Base!=ExpectedBase||Rows[0].Enum!=ExpectedEnum){return Stop(TEXT("blackboard_generated_pending_config_changed"),TEXT("Current configured reference changed while the pending row was observed."));}Owners[0].Own.Add(MoveTemp(Rows[0]));ConfiguredKeys.Reset();for(int32 I=PendingKeys.Num()-1;I>=0;--I){if(PendingKeys[I].Object.Get()==Entry.KeyType.Get()){PendingKeys.RemoveAt(I);}}return true;}
    bool Remove(int32 Index)
    {if(!Active){return true;}if(!Full()||!Owners[0].Own.IsValidIndex(Index)){return false;}Allocations.Emplace(Owners[0].Own[Index].Key.Get());Owners[0].Own.RemoveAt(Index);return true;}
    bool Saved(bool Saved)
    {if(!Active){return true;}if(!Context()){return false;}if(Saved){const EObjectFlags Bits=RF_WasLoaded|RF_LoadCompleted;Owners[0].Flags=static_cast<EObjectFlags>(Owners[0].Flags|Bits);for(auto& Q:Owners[0].Own){Q.Flags=static_cast<EObjectFlags>(Q.Flags|Bits);}}return Full();}
};
bool GeneratedReadAdmission(){return !GeneratedCurrent||GeneratedCurrent->Raw();}
bool GeneratedBoundary(){return !GeneratedCurrent||GeneratedCurrent->Full();}
bool GeneratedBusy(){return GeneratedCurrent&&GeneratedCurrent->Active;}
bool BaseSource(UClass* C,TSharedRef<FJsonObject>& J)
{return RuntimeNative(C,true)?NativeSource(C,J):(GeneratedCurrent&&GeneratedCurrent->Active&&GeneratedCurrent->Source(C,J));}

bool Entry(const FBlackboardEntry& E, UBlackboardData* Owner, TSharedRef<FJsonObject>& J)
{
    UBlackboardKeyType* K = E.KeyType.Get(); UClass* C = K ? K->GetClass() : nullptr;
    if (E.EntryName.IsNone() || !Loaded(K) || K->GetOuter() != Owner || C != KeyClass(KeyTypeName(C).ToLower())
        || !RuntimeNative(C) || K->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_Transient) || K->IsEditorOnly()) { return false; }
    J->SetStringField(TEXT("name"), E.EntryName.ToString()); J->SetStringField(TEXT("type"), KeyTypeName(C));
    J->SetStringField(TEXT("key_type_class"), C->GetPathName()); J->SetBoolField(TEXT("instance_synced"), E.bInstanceSynced);
    J->SetObjectField(TEXT("key_type_identity"), Identity(K));
#if WITH_EDITORONLY_DATA
    J->SetStringField(TEXT("entry_description"), E.EntryDescription); J->SetStringField(TEXT("entry_category"), E.EntryCategory.ToString());
#endif
    if (auto* ObjectKey = Cast<UBlackboardKeyType_Object>(K))
    {
        auto S = MakeShared<FJsonObject>(); if (!BaseSource(ObjectKey->BaseClass.Get(), S)) { return false; }
        J->SetStringField(TEXT("base_class"), ObjectKey->BaseClass->GetPathName()); J->SetObjectField(TEXT("base_class_source"), S);
    }
    else if (auto* ClassKey = Cast<UBlackboardKeyType_Class>(K))
    {
        auto S = MakeShared<FJsonObject>(); if (!BaseSource(ClassKey->BaseClass.Get(), S)) { return false; }
        J->SetStringField(TEXT("base_class"), ClassKey->BaseClass->GetPathName()); J->SetObjectField(TEXT("base_class_source"), S);
    }
    else if (auto* EnumKey = Cast<UBlackboardKeyType_Enum>(K))
    {
        auto S = MakeShared<FJsonObject>(); if (!EnumSource(EnumKey->EnumType.Get(), S)) { return false; }
        J->SetStringField(TEXT("enum_path"), EnumKey->EnumType->GetPathName()); J->SetStringField(TEXT("enum_name"), EnumKey->EnumName); J->SetObjectField(TEXT("enum_source"), S);
    }
    auto P = MakeShared<FJsonObject>(); if (!Properties(K, {}, P)) { return false; } J->SetObjectField(TEXT("key_type_properties"), P); return true;
}
bool Chain(UBlackboardData* B, TArray<UBlackboardData*>& Out)
{
    TSet<UBlackboardData*> Seen; int32 Total = 0;
    for (UBlackboardData* P = B; P; P = P->Parent.Get())
    {
        if (!Loaded(P) || Seen.Contains(P) || Out.Num() >= MaxParents || P->GetClass() != UBlackboardData::StaticClass()
            || P->GetOuter() != P->GetOutermost() || P->IsEditorOnly() || P->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_Transient)
            || !(P->GetOutermost()->GetName().StartsWith(TEXT("/Game/")) || P->GetOutermost()->GetName().StartsWith(TEXT("/Engine/")))
            || P->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly | PKG_PlayInEditor | PKG_CompiledIn)
            || P->Keys.Num() > MaxKeys || P->ParentKeys.Num() > MaxKeys
            || static_cast<int32>(P->GetFirstKeyID()) + P->Keys.Num() >= static_cast<int32>(FBlackboard::InvalidKey)) { return false; }
        Seen.Add(P); Out.Add(P); Total += P->Keys.Num(); if (Total > MaxKeys) { return false; }
    }
    // The public GetKey/GetKeyID methods assume valid cached offsets. Never call
    // them against an inconsistent parent chain or an unchecked array index.
    for (int32 I = 0; I < Out.Num(); ++I)
    {
        const int32 Expected = I + 1 < Out.Num() ? Out[I + 1]->GetNumKeys() : 0;
        if (static_cast<int32>(Out[I]->GetFirstKeyID()) != Expected) { return false; }
    }
    return true;
}
bool State(UBlackboardData* B, TSharedRef<FJsonObject>& J)
{
    TArray<UBlackboardData*> Parents; if (!Chain(B, Parents)) { return false; }
    J = Identity(B); J->SetStringField(TEXT("asset_type"), TEXT("blackboard")); J->SetStringField(TEXT("asset_path"), B->GetPathName());
    J->SetStringField(TEXT("parent_blackboard"), B->Parent ? B->Parent->GetPathName() : TEXT(""));
    J->SetNumberField(TEXT("own_key_count"), B->Keys.Num()); J->SetNumberField(TEXT("inherited_key_count"), B->ParentKeys.Num());
    J->SetNumberField(TEXT("total_key_count"), B->Keys.Num() + B->ParentKeys.Num()); J->SetNumberField(TEXT("first_key_id"), static_cast<int32>(B->GetFirstKeyID()));
    J->SetNumberField(TEXT("runtime_total_key_count"), B->GetNumKeys()); J->SetBoolField(TEXT("has_synchronized_keys"), B->HasSynchronizedKeys());
    J->SetBoolField(TEXT("package_dirty"), B->GetOutermost()->IsDirty()); TArray<TSharedPtr<FJsonValue>> Own, Inherited;
    TSet<FName> Names; bool Synced = B->Parent && B->Parent->HasSynchronizedKeys();
    for (int32 I = 0; I < B->Keys.Num(); ++I)
    {
        const auto& E = B->Keys[I]; auto Row = MakeShared<FJsonObject>();
        if (Names.Contains(E.EntryName) || !Entry(E, B, Row)) { return false; } Names.Add(E.EntryName);
        const auto Id = B->GetKeyID(E.EntryName); const int32 ExpectedId = static_cast<int32>(B->GetFirstKeyID()) + I;
        if (static_cast<int32>(Id) != ExpectedId || B->GetKeyName(Id) != E.EntryName || B->GetKeyType(Id).Get() != E.KeyType->GetClass()
            || B->IsKeyInstanceSynced(Id) != static_cast<bool>(E.bInstanceSynced)) { return false; }
        Row->SetNumberField(TEXT("key_id"), static_cast<int32>(Id)); Own.Add(MakeShared<FJsonValueObject>(Row));
        Synced |= E.bInstanceSynced;
    }
    for (const auto& E : B->ParentKeys)
    {
        auto Row = MakeShared<FJsonObject>(); UBlackboardData* Owner = Cast<UBlackboardData>(E.KeyType ? E.KeyType->GetOuter() : nullptr);
        if (!Owner || !Parents.Contains(Owner) || Owner == B || Names.Contains(E.EntryName) || !Entry(E, Owner, Row)) { return false; }
        Names.Add(E.EntryName); const auto Id = B->GetKeyID(E.EntryName);
        if (Id == FBlackboard::InvalidKey || static_cast<int32>(Id) >= static_cast<int32>(B->GetFirstKeyID()) || B->GetKeyName(Id) != E.EntryName
            || B->GetKeyType(Id).Get() != E.KeyType->GetClass()) { return false; }
        Row->SetNumberField(TEXT("key_id"), static_cast<int32>(Id)); Inherited.Add(MakeShared<FJsonValueObject>(Row));
    }
    if (B->HasSynchronizedKeys() != Synced) { return false; }
    J->SetArrayField(TEXT("own_keys"), Own); J->SetArrayField(TEXT("inherited_keys"), Inherited);
    auto P = MakeShared<FJsonObject>(); if (!Properties(B, { TEXT("Keys"), TEXT("ParentKeys"), TEXT("bHasSynchronizedKeys") }, P)) { return false; }
    J->SetObjectField(TEXT("other_properties"), P); return true;
}
struct FObservation
{
    TSharedRef<FJsonObject> State = MakeShared<FJsonObject>(), NativeSources = MakeShared<FJsonObject>(), Disk = MakeShared<FJsonObject>();
    TSharedRef<FJsonObject> ParentStates = MakeShared<FJsonObject>(), OtherDirty = MakeShared<FJsonObject>();
};
// Preserve published before/after_cache_refresh JSON: clone only expectation
// containers and replace only a complete byte-equal old Actor source proof.
TSharedRef<FJsonObject> GeneratedExpectedActorState(const TSharedRef<FJsonObject>& State,const FGeneratedBaseScope& Scope)
{
    auto Out=MakeShared<FJsonObject>();for(const auto& P:State->Values){Out->SetField(P.Key,P.Value);}
    for(const TCHAR* Name:{TEXT("own_keys"),TEXT("inherited_keys")})
    {
        const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;if(!State->TryGetArrayField(Name,Rows)||!Rows){continue;}
        TArray<TSharedPtr<FJsonValue>> Expected;for(const auto& V:*Rows)
        {
            const auto Row=V->AsObject();auto Copy=MakeShared<FJsonObject>();for(const auto& P:Row->Values){Copy->SetField(P.Key,P.Value);}
            const TSharedPtr<FJsonObject>* Source=nullptr;if(Row->TryGetObjectField(TEXT("base_class_source"),Source)&&Source&&Source->IsValid()
                &&Text(Source->ToSharedRef()).Equals(Text(Scope.ActorBeforeLoad.ToSharedRef()),ESearchCase::CaseSensitive))
            {Copy->SetObjectField(TEXT("base_class_source"),Scope.ActorAfterLoad.ToSharedRef());}
            Expected.Add(MakeShared<FJsonValueObject>(Copy));
        }Out->SetArrayField(Name,Expected);
    }return Out;
}
void GeneratedExpectAfterBaseLoad(FObservation& Expected,const FGeneratedBaseScope& Scope)
{
    if(!Scope.ActorBeforeLoad.IsValid()||!Scope.ActorAfterLoad.IsValid()){return;}
    Expected.State=GeneratedExpectedActorState(Expected.State,Scope);
    auto Native=MakeShared<FJsonObject>();for(const auto& P:Expected.NativeSources->Values){Native->SetField(P.Key,P.Value);}
    const TSharedPtr<FJsonObject>* Actor=nullptr;if(Native->TryGetObjectField(TEXT("/Script/Engine.Actor"),Actor)&&Actor&&Actor->IsValid()
        &&Text(Actor->ToSharedRef()).Equals(Text(Scope.ActorBeforeLoad.ToSharedRef()),ESearchCase::CaseSensitive))
    {Native->SetObjectField(TEXT("/Script/Engine.Actor"),Scope.ActorAfterLoad.ToSharedRef());}Expected.NativeSources=Native;
    auto Parents=MakeShared<FJsonObject>();for(const auto& P:Expected.ParentStates->Values)
    {
        auto Row=P.Value->AsObject();auto Copy=MakeShared<FJsonObject>();for(const auto& Q:Row->Values){Copy->SetField(Q.Key,Q.Value);}
        const TSharedPtr<FJsonObject>* State=nullptr;if(Row->TryGetObjectField(TEXT("state"),State)&&State&&State->IsValid()){Copy->SetObjectField(TEXT("state"),GeneratedExpectedActorState(State->ToSharedRef(),Scope));}
        Parents->SetObjectField(P.Key,Copy);
    }Expected.ParentStates=Parents;
}
bool DirtyPackages(const FString& ExcludedPackage, TSharedRef<FJsonObject>& J)
{
    int32 Count = 0;
    for (TObjectIterator<UPackage> It; It; ++It)
    {
        if (!Loaded(*It) || It->GetName() == ExcludedPackage) { continue; }
        if (++Count > MaxLoadedPackages) { return false; } J->SetBoolField(It->GetPathName(), It->IsDirty());
    }
    return true;
}
bool Observe(UBlackboardData* B, FObservation& O)
{
    if (!GeneratedBoundary() || !State(B, O.State) || !Sources(O.NativeSources) || !Disk(B->GetOutermost()->GetName(), O.Disk)) { return false; }
    TArray<UBlackboardData*> Parents; if (!Chain(B, Parents)) { return false; }
    for (int32 I = 1; I < Parents.Num(); ++I)
    {
        auto S = MakeShared<FJsonObject>(), D = MakeShared<FJsonObject>(), Row = MakeShared<FJsonObject>();
        if (!State(Parents[I], S) || !Disk(Parents[I]->GetOutermost()->GetName(), D)) { return false; }
        Row->SetObjectField(TEXT("state"), S); Row->SetObjectField(TEXT("disk"), D); O.ParentStates->SetObjectField(Parents[I]->GetPathName(), Row);
    }
    return DirtyPackages(B->GetOutermost()->GetName(), O.OtherDirty) && GeneratedBoundary();
}
TSharedRef<FJsonObject> Observation(const FObservation& O)
{
    auto J = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("state"), O.State); J->SetObjectField(TEXT("native_sources"), O.NativeSources);
    J->SetObjectField(TEXT("disk"), O.Disk); J->SetObjectField(TEXT("parent_states"), O.ParentStates); J->SetObjectField(TEXT("other_package_dirty"), O.OtherDirty); return J;
}
void PublishState(FResult& R, const FObservation& O, const TCHAR* Label)
{
    R.Data->SetObjectField(Label, Observation(O));
    for (const TCHAR* K : { TEXT("asset_type"), TEXT("asset_path"), TEXT("parent_blackboard") }) { R.Data->SetStringField(K, O.State->GetStringField(K)); }
    for (const TCHAR* K : { TEXT("own_key_count"), TEXT("inherited_key_count"), TEXT("total_key_count") }) { R.Data->SetNumberField(K, O.State->GetNumberField(K)); }
    R.Data->SetArrayField(TEXT("own_keys"), O.State->GetArrayField(TEXT("own_keys")));
    R.Data->SetArrayField(TEXT("inherited_keys"), O.State->GetArrayField(TEXT("inherited_keys")));
    R.Data->SetBoolField(TEXT("readback_confirmed"), true); R.Data->SetBoolField(TEXT("observations_complete"), true);
}
bool EditorState(FResult& R)
{ return (IsEditorGameThread() && !IsPlaySessionActive()) || Fail(R, TEXT("blackboard_editor_state_invalid"), TEXT("Requires the Editor game thread outside PIE/SIE.")); }
bool ExactAssetPath(const FString& Input, bool bMutable, FString& Path, FResult& R)
{
    Path = FPackageName::ExportTextPathToObjectPath(Input).TrimStartAndEnd(); Path.TrimQuotesInline(); Path.ReplaceInline(TEXT("\\"), TEXT("/"));
    if (!Path.Contains(TEXT("."))) { Path += TEXT(".") + FPackageName::GetLongPackageAssetName(Path); }
    FString PackageName, ObjectName;
    if (Path.Len() > 1024 || !FPackageName::IsValidObjectPath(Path)
        || !Path.Split(TEXT("."), &PackageName, &ObjectName, ESearchCase::CaseSensitive, ESearchDir::FromEnd)
        || ObjectName != FPackageName::GetLongPackageAssetName(PackageName)
        || !(PackageName.StartsWith(TEXT("/Game/")) || (!bMutable && PackageName.StartsWith(TEXT("/Engine/")))))
    { return Fail(R, TEXT("blackboard_asset_path_invalid"), TEXT("Use a /Game Blackboard package or exact object path; read-only inspection also permits /Engine. Other mounts/subobjects are unsupported.")); }
    return true;
}
void NoteLoad(FResult& R, const FString& Path)
{
    R.Data->SetBoolField(TEXT("asset_load_dispatched"), true); R.Data->SetBoolField(TEXT("implicit_postload_possible"), true);
    R.Data->SetNumberField(TEXT("asset_load_count"), R.Data->GetNumberField(TEXT("asset_load_count")) + 1);
    R.Data->SetStringField(TEXT("last_explicit_load_path"), Path);
}
UBlackboardData* Resolve(const FString& Path, FResult& R)
{
    UObject* O = FindObject<UObject>(nullptr, *Path);
    if (!O) { NoteLoad(R, Path); O = LoadObject<UObject>(nullptr, *Path); }
    auto* B = Cast<UBlackboardData>(O);
    if (!Loaded(B) || B->GetPathName() != Path || B->GetClass() != UBlackboardData::StaticClass())
    { Fail(R, TEXT("blackboard_native_asset_required"), TEXT("Only exact runtime BlackboardData is implemented; BehaviorTree, EQS and custom Blackboard subclasses remain incomplete.")); return nullptr; }
    return B;
}
void Refresh(UBlackboardData* B, FResult& R)
{
    R.bAttempted = true; R.Data->SetBoolField(TEXT("cache_refresh_dispatched"), true);
    R.Data->SetNumberField(TEXT("cache_refresh_count"), R.Data->GetNumberField(TEXT("cache_refresh_count")) + 1);
    if(!GeneratedCurrent||!GeneratedCurrent->Active){B->UpdateParentKeys(); B->UpdateKeyIDs(); B->UpdateIfHasSynchronizedKeys();return;}
    if(!GeneratedBoundary()){return;} B->UpdateParentKeys();
    if(!GeneratedCurrent->AdoptRefresh()||!GeneratedBoundary()){return;} B->UpdateKeyIDs();
    if(!GeneratedBoundary()){return;} B->UpdateIfHasSynchronizedKeys(); GeneratedBoundary();
}
bool Retained(UBlackboardData* B, const FObservation& Before, FResult& R, FObservation& After)
{
    if (!Observe(B, After))
    { R.bKnown = false; R.bPost = false; R.Data->SetBoolField(TEXT("observations_complete"), false); return Fail(R, TEXT("blackboard_retained_state_unobservable"), TEXT("The retained target/source/cache state is incomplete; do not replay the request.")); }
    PublishState(R, After, TEXT("after")); R.bChanged = Text(Before.State) != Text(After.State) || Text(Before.NativeSources) != Text(After.NativeSources)
        || Text(Before.ParentStates) != Text(After.ParentStates) || Text(Before.OtherDirty) != Text(After.OtherDirty) || Text(Before.Disk) != Text(After.Disk);
    const bool Source = Text(Before.NativeSources) == Text(After.NativeSources), Parents = Text(Before.ParentStates) == Text(After.ParentStates);
    const bool Other = Text(Before.OtherDirty) == Text(After.OtherDirty), PriorDisk = Text(Before.Disk) == Text(After.Disk);
    R.Data->SetBoolField(TEXT("native_sources_preserved"), Source); R.Data->SetBoolField(TEXT("parent_assets_preserved"), Parents);
    R.Data->SetBoolField(TEXT("other_package_dirty_preserved"), Other); R.Data->SetBoolField(TEXT("disk_preserved_before_save"), PriorDisk);
    if (!Source || !Parents || !Other || !PriorDisk)
    { R.bPost = false; return Fail(R, TEXT("blackboard_source_preservation_failed"), TEXT("Retained target exists but native/parent/disk/unrelated dirty-state preservation failed; save is suppressed.")); }
    return true;
}
// SavePackage2 marks serialized exports with these two load-lifecycle flags.
// Predict that exact addition only on the selected Blackboard and its own keys;
// never remove flag differences from the observed target, sources or parents.
bool ExpectedSavedIdentity(const UObject* O, const TSharedRef<FJsonObject>& Before, TSharedRef<FJsonObject>& Out)
{
    if (!Loaded(O) || Before->GetStringField(TEXT("path")) != O->GetPathName()
        || Before->GetStringField(TEXT("class")) != O->GetClass()->GetPathName()
        || Before->GetStringField(TEXT("outer")) != O->GetOuter()->GetPathName()) { return false; }
    const FString Raw = Before->GetStringField(TEXT("object_flags"));
    const uint64 Flags = FCString::Strtoui64(*Raw, nullptr, 10);
    if (Raw != FString::Printf(TEXT("%llu"), Flags)) { return false; }
    Out = MakeShared<FJsonObject>(); for (const auto& P : Before->Values) { Out->SetField(P.Key, P.Value); }
    Out->SetStringField(TEXT("object_flags"), FString::Printf(TEXT("%llu"), Flags | static_cast<uint64>(RF_WasLoaded | RF_LoadCompleted)));
    return true;
}
bool ExpectedSavedState(UBlackboardData* B, const TSharedRef<FJsonObject>& Before, TSharedRef<FJsonObject>& Out)
{
    if (!ExpectedSavedIdentity(B, Before, Out)) { return false; }
    const auto& Rows = Before->GetArrayField(TEXT("own_keys")); if (Rows.Num() != B->Keys.Num()) { return false; }
    TArray<TSharedPtr<FJsonValue>> SavedRows;
    for (int32 I = 0; I < Rows.Num(); ++I)
    {
        UBlackboardKeyType* Key = B->Keys[I].KeyType.Get();
        if (!Loaded(Key) || Key->GetOuter() != B) { return false; }
        const auto Row = Rows[I]->AsObject(); if (!Row.IsValid()) { return false; }
        auto SavedIdentity = MakeShared<FJsonObject>();
        if (!ExpectedSavedIdentity(Key, Row->GetObjectField(TEXT("key_type_identity")).ToSharedRef(), SavedIdentity)) { return false; }
        auto SavedRow = MakeShared<FJsonObject>(); for (const auto& P : Row->Values) { SavedRow->SetField(P.Key, P.Value); }
        SavedRow->SetObjectField(TEXT("key_type_identity"), SavedIdentity); SavedRows.Add(MakeShared<FJsonValueObject>(SavedRow));
    }
    Out->SetArrayField(TEXT("own_keys"), SavedRows); Out->SetBoolField(TEXT("package_dirty"), false);
    return true;
}
bool Save(UBlackboardData* B, const FObservation& Expected, FResult& R)
{
    if (!R.bSaveRequested) { return true; }
    if(!GeneratedBoundary()){return false;} R.Stage = TEXT("save"); R.bSaveAttempted = true; R.bSaved = UEditorAssetLibrary::SaveLoadedAsset(B, false);
    R.Data->SetBoolField(TEXT("asset_saved"), R.bSaved); if(GeneratedCurrent&&!GeneratedCurrent->Saved(R.bSaved)){return false;} FObservation After;
    if (!Observe(B, After)) { R.bKnown = false; R.bPost = false; return Fail(R, TEXT("blackboard_save_state_unobservable"), TEXT("Exact save returned but after-save state is incomplete; retained RAM/disk outcome must not be retried automatically.")); }
    PublishState(R, After, TEXT("after_save"));
    auto E = MakeShared<FJsonObject>(); const bool LifecycleExpected = R.bSaved && ExpectedSavedState(B, Expected.State, E);
    const bool Preserved = LifecycleExpected && Text(E) == Text(After.State) && Text(Expected.NativeSources) == Text(After.NativeSources)
        && Text(Expected.ParentStates) == Text(After.ParentStates) && Text(Expected.OtherDirty) == Text(After.OtherDirty);
    R.Data->SetBoolField(TEXT("save_state_preserved"), Preserved); R.Data->SetBoolField(TEXT("save_disk_outcome_known"), R.bSaved);
    R.Data->SetBoolField(TEXT("save_lifecycle_flag_expectation_applied"), LifecycleExpected);
    R.Data->SetStringField(TEXT("save_state_comparison_scope"), TEXT("Exact target/own-key state with only successful-save RF_WasLoaded|RF_LoadCompleted additions predicted for those identities and package_dirty=false; inherited keys, source/CDO/parent states and unrelated dirty fields remain exact. All raw flags remain in before/after/after_save."));
    if (!R.bSaved)
    { R.bKnown = false; R.bPost = false; return Fail(R, TEXT("blackboard_save_failed"), TEXT("Exact save returned false; RAM prefix is retained and disk transaction outcome is not certified. Do not resend mutation or save automatically.")); }
    if (!Preserved || B->GetOutermost()->IsDirty() || !After.Disk->GetBoolField(TEXT("exists")))
    { R.bPost = false; return Fail(R, TEXT("blackboard_save_postcondition_failed"), TEXT("Exact save returned true but clean bytes/target/source preservation failed.")); }
    return true;
}
bool SimpleName(const FString& S)
{
    if (S.IsEmpty() || S.Len() > 128 || FName(*S).IsNone()) { return false; }
    for (TCHAR C : S) { if (C < 33 || C == 127 || FString(INVALID_OBJECTNAME_CHARACTERS).Contains(FString::Chr(C))) { return false; } }
    return true;
}
}

FString CreateAsset(const FString& AssetName, const FString& AssetRoot, bool bSaveAsset)
{
    FResult R; Init(R, TEXT("create_ai_blackboard_asset"), bSaveAsset);
    if(GeneratedBusy()){Fail(R,TEXT("blackboard_generated_reentry"),TEXT("A generated BaseClass operation is active; nested operation rejected before load/SDK."));return Encode(R);} FGeneratedBaseScope Generated(R);
    if (!EditorState(R)) { return Encode(R); }
    FString Root = AssetRoot; while (Root.EndsWith(TEXT("/"))) { Root.LeftChopInline(1); }
    if (!SimpleName(AssetName) || !(Root == TEXT("/Game") || Root.StartsWith(TEXT("/Game/"))) || !FPackageName::IsValidLongPackageName(Root))
    { Fail(R, TEXT("blackboard_create_path_invalid"), TEXT("Use a simple asset_name and valid /Game asset_root; existing targets are never overwritten.")); return Encode(R); }
    const FString PackageName = Root + TEXT("/") + AssetName, Path = PackageName + TEXT(".") + AssetName;
    auto SourceBefore = MakeShared<FJsonObject>(), DiskBefore = MakeShared<FJsonObject>(), DirtyBefore = MakeShared<FJsonObject>();
    if (!FPackageName::IsValidObjectPath(Path))
    { R.Data->SetStringField(TEXT("preflight_failed_substage"), TEXT("object_path")); Fail(R, TEXT("blackboard_native_source_unobservable"), TEXT("Destination object path is invalid before creation.")); return Encode(R); }
    R.Data->SetObjectField(TEXT("native_sources_before"), SourceBefore);
    if (!Sources(SourceBefore))
    {
        R.Data->SetStringField(TEXT("preflight_failed_substage"), TEXT("native_sources")); FString FailedClass, FailedSubstage;
        SourceBefore->TryGetStringField(TEXT("failed_class"), FailedClass); SourceBefore->TryGetStringField(TEXT("failed_substage"), FailedSubstage);
        R.Data->SetStringField(TEXT("native_source_failed_class"), FailedClass); R.Data->SetStringField(TEXT("native_source_failed_substage"), FailedSubstage);
        Fail(R, TEXT("blackboard_native_source_unobservable"), TEXT("Clean loaded native Blackboard/key/AISystem classes and existing CDOs were not fully observable; partial source rows and exact failed class/substage are retained.")); return Encode(R);
    }
    R.Data->SetObjectField(TEXT("disk_before"), DiskBefore);
    if (!Disk(PackageName, DiskBefore))
    { R.Data->SetStringField(TEXT("preflight_failed_substage"), TEXT("destination_disk")); Fail(R, TEXT("blackboard_native_source_unobservable"), TEXT("Destination disk state was not fully observable before creation.")); return Encode(R); }
    R.Data->SetObjectField(TEXT("other_package_dirty_before"), DirtyBefore);
    if (!DirtyPackages(PackageName, DirtyBefore))
    { R.Data->SetStringField(TEXT("preflight_failed_substage"), TEXT("other_package_dirty")); Fail(R, TEXT("blackboard_native_source_unobservable"), TEXT("Other-package dirty state was not fully observable before creation.")); return Encode(R); }
    if (FindPackage(nullptr, *PackageName) || FindObject<UObject>(nullptr, *Path) || DiskBefore->GetBoolField(TEXT("exists"))
        || FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(FSoftObjectPath(Path)).IsValid())
    { Fail(R, TEXT("blackboard_target_exists"), TEXT("The destination package/object/registered asset already exists; no replacement is attempted.")); return Encode(R); }
    R.Data->SetObjectField(TEXT("native_sources_before"), SourceBefore); R.Data->SetObjectField(TEXT("disk_before"), DiskBefore);
    R.Data->SetObjectField(TEXT("other_package_dirty_before"), DirtyBefore);
    R.Stage = TEXT("create_asset"); R.bAttempted = true;
    UPackage* Package = CreatePackage(*PackageName); TStrongObjectPtr<UPackage> KeepPackage(Package);
    UBlackboardData* B = Package ? NewObject<UBlackboardData>(Package, UBlackboardData::StaticClass(), *AssetName, RF_Public | RF_Standalone | RF_Transactional) : nullptr;
    TStrongObjectPtr<UBlackboardData> Keep(B); R.bChanged = Package != nullptr;
    if (!Loaded(B)) { R.bKnown = false; Fail(R, TEXT("blackboard_create_failed"), TEXT("Package/object creation did not produce a readable Blackboard; any allocated prefix remains and no rollback/retry is claimed.")); return Encode(R); }
    FObservation Factory;
    if (!Observe(B, Factory)) { R.bKnown = false; Fail(R, TEXT("blackboard_factory_state_unobservable"), TEXT("Created asset exists but its factory/default state is unobservable; no save or retry.")); return Encode(R); }
    PublishState(R, Factory, TEXT("factory_default_state")); R.Data->SetBoolField(TEXT("factory_self_key_config"), SourceBefore->GetBoolField(TEXT("ai_config_add_blackboard_self_key")));
    R.Data->SetBoolField(TEXT("factory_default_keys_observed"), true); R.Data->SetBoolField(TEXT("implicit_persistent_keys_possible"), true);
    Refresh(B, R); B->MarkPackageDirty(); R.Data->SetBoolField(TEXT("asset_registry_publication_dispatched"), true); FAssetRegistryModule::AssetCreated(B);
    FObservation After;
    if (!Observe(B, After)) { R.bKnown = false; Fail(R, TEXT("blackboard_create_postcondition_unobservable"), TEXT("Created/published Blackboard retained but final source/cache readback is incomplete; save suppressed.")); return Encode(R); }
    PublishState(R, After, TEXT("after")); R.bChanged = true;
    const bool SourcePreserved = Text(SourceBefore) == Text(After.NativeSources), DefaultsStable = Text(Factory.NativeSources) == Text(After.NativeSources);
    R.Data->SetBoolField(TEXT("native_sources_preserved"), SourcePreserved); R.Data->SetBoolField(TEXT("factory_default_sources_preserved"), DefaultsStable);
    R.Data->SetBoolField(TEXT("created"), true);
    if (!SourcePreserved || !DefaultsStable || B->Parent || B->GetPathName() != Path || !B->HasAllFlags(RF_Public | RF_Standalone | RF_Transactional)
        || Text(DiskBefore) != Text(After.Disk) || Text(Factory.ParentStates) != Text(After.ParentStates)
        || Text(DirtyBefore) != Text(Factory.OtherDirty) || Text(DirtyBefore) != Text(After.OtherDirty))
    { Fail(R, TEXT("blackboard_create_postcondition_failed"), TEXT("Created asset/source/cache/disk preservation failed; retained prefix remains, save suppressed.")); return Encode(R); }
    R.bPost = true; if (!Save(B, After, R)) { return Encode(R); }
    R.Stage = TEXT("complete"); R.bSuccess = true; return Encode(R);
}

FString AddKey(const FString& BlackboardPath, const FString& KeyName, const FString& KeyType,
    const FString& BaseClass, const FString& EnumPath, bool bInstanceSynced, bool bSaveAsset)
{
    FResult R; Init(R, TEXT("add_ai_blackboard_key"), bSaveAsset); FString Path;
    if(GeneratedBusy()){Fail(R,TEXT("blackboard_generated_reentry"),TEXT("A generated BaseClass operation is active; nested operation rejected before load/SDK."));return Encode(R);} FGeneratedBaseScope Generated(R);
    if (!EditorState(R) || !ExactAssetPath(BlackboardPath, true, Path, R)) { return Encode(R); }
    if (!SimpleName(KeyName)) { Fail(R, TEXT("blackboard_key_name_invalid"), TEXT("Use a nonempty simple Blackboard key name of at most 128 characters.")); return Encode(R); }
    auto* B = Resolve(Path, R); TStrongObjectPtr<UBlackboardData> Keep(B); if (!B) { return Encode(R); }
    if(!Generated.Begin(B,(KeyType.TrimStartAndEnd().Equals(TEXT("object"),ESearchCase::IgnoreCase)||KeyType.TrimStartAndEnd().Equals(TEXT("class"),ESearchCase::IgnoreCase))&&BaseClass.StartsWith(TEXT("/Game/")))){return Encode(R);}
    FObservation Before;
    if (!Observe(B, Before)) { Fail(R, TEXT("blackboard_preflight_unobservable"), TEXT("Requires bounded native key/parent/cache/source state; custom keys, inconsistent IDs and unsupported base references are rejected before cache mutation.")); return Encode(R); }
    PublishState(R, Before, TEXT("before")); R.Stage = TEXT("refresh_before_duplicate"); Refresh(B, R);
    FObservation Refreshed; if (!Retained(B, Before, R, Refreshed)) { return Encode(R); }
    R.Data->SetBoolField(TEXT("cache_refresh_changed"), R.bChanged); R.Data->SetObjectField(TEXT("after_cache_refresh"), Observation(Refreshed));
    int32 Existing = INDEX_NONE; const FName Name(*KeyName);
    for (int32 I = 0; I < B->Keys.Num(); ++I) { if (B->Keys[I].EntryName == Name) { Existing = I; break; } }
    if (Existing != INDEX_NONE)
    {
        const auto& E = B->Keys[Existing]; auto K = Refreshed.State->GetArrayField(TEXT("own_keys"))[Existing]->AsObject();
        FString RequestedReference, ActualReference;
        if (Cast<UBlackboardKeyType_Object>(E.KeyType)) { RequestedReference = BaseClass.IsEmpty() ? AActor::StaticClass()->GetPathName() : BaseClass; ActualReference = K->GetStringField(TEXT("base_class")); }
        else if (Cast<UBlackboardKeyType_Class>(E.KeyType)) { RequestedReference = BaseClass.IsEmpty() ? UObject::StaticClass()->GetPathName() : BaseClass; ActualReference = K->GetStringField(TEXT("base_class")); }
        else if (Cast<UBlackboardKeyType_Enum>(E.KeyType)) { RequestedReference = EnumPath; ActualReference = K->GetStringField(TEXT("enum_path")); }
        R.Data->SetBoolField(TEXT("created"), false); R.Data->SetBoolField(TEXT("already_exists"), true); R.Data->SetObjectField(TEXT("key"), K.ToSharedRef());
        // Preserve UE56's raw duplicate-type comparison: fresh creation trims,
        // an existing key does not silently accept extra whitespace.
        if (!K->GetStringField(TEXT("type")).Equals(KeyType, ESearchCase::IgnoreCase) || static_cast<bool>(E.bInstanceSynced) != bInstanceSynced
            || (!RequestedReference.IsEmpty() && !ActualReference.Equals(RequestedReference, ESearchCase::IgnoreCase)))
        { Fail(R, TEXT("blackboard_key_conflict"), TEXT("The existing key's raw type, instance_synced or requested reference differs. Cache refresh already ran; no key declaration/save was attempted.")); return Encode(R); }
        R.bPost = true; R.bSuccess = true; R.Stage = TEXT("complete"); return Encode(R);
    }
    if (B->GetKeyID(Name) != FBlackboard::InvalidKey)
    { Fail(R, TEXT("blackboard_key_inherited"), TEXT("The key is inherited/engine-provided and cannot be shadowed; cache refresh already ran, no declaration/save.")); return Encode(R); }
    const FString Type = KeyType.TrimStartAndEnd().ToLower(); UClass* KC = KeyClass(Type);
    if (!KC) { Fail(R, TEXT("blackboard_key_type_unsupported"), TEXT("Supported native key types: bool, class, enum, float, int, name, object, rotator, string, vector.")); return Encode(R); }
    if (B->Keys.Num() + B->ParentKeys.Num() + 1 > MaxKeys || static_cast<int32>(B->GetFirstKeyID()) + B->Keys.Num() + 1 >= static_cast<int32>(FBlackboard::InvalidKey))
    { Fail(R, TEXT("blackboard_key_budget_exceeded"), TEXT("Adding one key exceeds the bounded observation or engine key-ID range.")); return Encode(R); }
    UClass* BC = nullptr; UEnum* Enum = nullptr; TSharedRef<FJsonObject> ReferenceBefore = MakeShared<FJsonObject>();
    if (Type == TEXT("object") || Type == TEXT("class"))
    {
        const FString BasePath = BaseClass.IsEmpty() ? (Type == TEXT("object") ? AActor::StaticClass()->GetPathName() : UObject::StaticClass()->GetPathName()) : BaseClass;
        BC = FindObject<UClass>(nullptr, *BasePath);
        TStrongObjectPtr<UClass> KeepLoadedBase;
        const bool GeneratedPath=BasePath.StartsWith(TEXT("/Game/"))&&BasePath.Len()<NAME_SIZE&&FPackageName::IsValidObjectPath(BasePath)&&!BasePath.Contains(TEXT(":"));
        if(!BC&&GeneratedPath)
        {
            if(!GeneratedBoundary()){return Encode(R);}NoteLoad(R,BasePath);R.Data->SetNumberField(TEXT("generated_base_load_count"),1);R.Data->SetBoolField(TEXT("generated_base_load_attempted"),true);
            BC=LoadObject<UClass>(nullptr,*BasePath);R.Data->SetBoolField(TEXT("generated_base_load_returned"),true);
            if(!Generated.Context()){return Encode(R);}if(Loaded(BC)){KeepLoadedBase.Reset(BC);}
            if(!Generated.AfterBaseLoad(BC,BasePath)){return Encode(R);}GeneratedExpectAfterBaseLoad(Refreshed,Generated);
            if(!GeneratedBoundary()){return Encode(R);}
            // The existing target observation is still native/previously admitted here.
            FObservation LoadedState;if(!Observe(B,LoadedState)){R.bKnown=false;Fail(R,TEXT("blackboard_generated_load_observation_incomplete"),TEXT("Source loader returned without a complete current target baseline; no declaration/save."));return Encode(R);}
            bool DirtyOK=true;for(const auto& Pair:Refreshed.OtherDirty->Values){const auto Now=LoadedState.OtherDirty->TryGetField(Pair.Key.ToView());DirtyOK&=Now&&Now->Type==EJson::Boolean&&Now->AsBool()==Pair.Value->AsBool();}
            for(const auto& Pair:LoadedState.OtherDirty->Values){if(!Refreshed.OtherDirty->HasField(Pair.Key.ToView())){DirtyOK&=Pair.Value->Type==EJson::Boolean&&!Pair.Value->AsBool();}}
            const bool TargetOK=Text(Refreshed.State).Equals(Text(LoadedState.State),ESearchCase::CaseSensitive)&&Text(Refreshed.NativeSources).Equals(Text(LoadedState.NativeSources),ESearchCase::CaseSensitive)&&Text(Refreshed.ParentStates).Equals(Text(LoadedState.ParentStates),ESearchCase::CaseSensitive)&&Text(Refreshed.Disk).Equals(Text(LoadedState.Disk),ESearchCase::CaseSensitive)&&DirtyOK;
            R.Data->SetObjectField(TEXT("after_generated_base_load"),Observation(LoadedState));R.Data->SetBoolField(TEXT("generated_base_load_target_preserved"),TargetOK);
            if(!TargetOK){R.bKnown=false;Fail(R,TEXT("blackboard_generated_load_preservation_failed"),TEXT("Source load changed target/existing source/disk/dirty or introduced a dirty package; no declaration/save."));return Encode(R);}
        }
        if(Loaded(BC)&&!BC->HasAnyClassFlags(CLASS_Native)&&Generated.Active){if(!Generated.BindSource(BC)||!GeneratedBoundary()){return Encode(R);}}
        if (!Loaded(BC) || BC->GetPathName()!=BasePath || !BaseSource(BC, ReferenceBefore))
        { Fail(R, TEXT("blackboard_base_class_unsupported"), TEXT("Requires an exact native BaseClass or admitted saved-clean Actor/pure UObject-root Blueprint class. Other generated native roots remain incomplete. No key allocated; source load effects and earlier refusal ordering differ from UE56.")); return Encode(R); }
    }
    else if (Type == TEXT("enum"))
    {
        const bool Script = EnumPath.StartsWith(TEXT("/Script/CoreUObject.")) || EnumPath.StartsWith(TEXT("/Script/Engine.")) || EnumPath.StartsWith(TEXT("/Script/AIModule."));
        if (!FPackageName::IsValidObjectPath(EnumPath) || (!Script && !EnumPath.StartsWith(TEXT("/Game/"))))
        { Fail(R, TEXT("blackboard_enum_path_invalid"), TEXT("A fresh enum key requires an exact native CoreUObject/Engine/AIModule or saved /Game user-enum path.")); return Encode(R); }
        Enum = FindObject<UEnum>(nullptr, *EnumPath);
        if (!Enum && !Script) { NoteLoad(R, EnumPath); Enum = LoadObject<UEnum>(nullptr, *EnumPath); }
        if (!Enum || Enum->GetPathName() != EnumPath || !EnumSource(Enum, ReferenceBefore))
        {
            FObservation Current;
            if (!Observe(B, Current)) { R.bKnown = false; }
            else { PublishState(R, Current, TEXT("after_reference_failure")); R.bChanged |= Text(Observation(Refreshed)) != Text(Observation(Current)); }
            Fail(R, TEXT("blackboard_enum_unsupported"), TEXT("Requires a clean runtime native or saved /Game user enum with all visible non-MAX values in 0..255. No key allocated; source-load effects are separately observed.")); return Encode(R);
        }
    }
    TStrongObjectPtr<UClass> KeepBase(BC); TStrongObjectPtr<UEnum> KeepEnum(Enum);
    // Reference resolution/possible PostLoad precedes declaration. Re-observe
    // the target and sources so loaders cannot invalidate the mutation plan.
    FObservation AtDispatch;
    if (!Observe(B, AtDispatch)) { R.bKnown = false; Fail(R, TEXT("blackboard_dispatch_unobservable"), TEXT("Source resolution left target/cache state unobservable; no key allocated.")); return Encode(R); }
    PublishState(R, AtDispatch, TEXT("at_dispatch"));
    const bool LoadedPackagesChanged = Text(Refreshed.OtherDirty) != Text(AtDispatch.OtherDirty);
    R.bChanged |= LoadedPackagesChanged; R.Data->SetBoolField(TEXT("reference_resolution_loaded_packages_changed"), LoadedPackagesChanged);
    if (Text(Refreshed.State) != Text(AtDispatch.State) || Text(Refreshed.NativeSources) != Text(AtDispatch.NativeSources)
        || Text(Refreshed.ParentStates) != Text(AtDispatch.ParentStates) || Text(Refreshed.Disk) != Text(AtDispatch.Disk))
    { R.bChanged = true; Fail(R, TEXT("blackboard_dispatch_state_changed"), TEXT("Reference resolution changed the prevalidated target/source; no key declaration/save attempted.")); return Encode(R); }
    R.Data->SetObjectField(TEXT("reference_before"), ReferenceBefore); R.Stage = TEXT("add_key"); if(!GeneratedBoundary()){return Encode(R);} B->Modify();if(!GeneratedBoundary()){return Encode(R);}
    R.Data->SetBoolField(TEXT("key_declaration_attempted"), true);
    if(!GeneratedBoundary()){return Encode(R);}
    UBlackboardKeyType* Key = NewObject<UBlackboardKeyType>(B, KC, NAME_None, RF_Transactional); R.Data->SetBoolField(TEXT("key_allocation_returned"),true);
    if(Generated.Active&&!Generated.HoldAllocation(Key,B,KC)){return Encode(R);} TStrongObjectPtr<UBlackboardKeyType> KeepKey(Key);
    if (!Loaded(Key) || Key->GetClass() != KC || Key->GetOuter() != B)
    { FObservation After; Retained(B, Before, R, After); R.bKnown = false; Fail(R, TEXT("blackboard_key_allocation_failed"), TEXT("Key allocation did not yield the requested owned class; any unappended allocated object prefix is not certified. No rollback or retry.")); return Encode(R); }
    auto Defaults = MakeShared<FJsonObject>(), ClassDefaults = MakeShared<FJsonObject>();
    const TSet<FName> Configured = { TEXT("BaseClass"), TEXT("EnumType"), TEXT("EnumName") };
    if (!Properties(Key, Configured, Defaults) || !Properties(KC->GetDefaultObject(false), Configured, ClassDefaults) || Text(Defaults) != Text(ClassDefaults))
    { R.bKnown = false; Fail(R, TEXT("blackboard_key_default_unobservable"), TEXT("Allocated key remains but its untouched default fields are not observable; no append/save.")); return Encode(R); }
    if(!GeneratedBoundary()){return Encode(R);}
    if (auto* ObjectKey = Cast<UBlackboardKeyType_Object>(Key)) { ObjectKey->BaseClass = BC; }
    else if (auto* ClassKey = Cast<UBlackboardKeyType_Class>(Key)) { ClassKey->BaseClass = BC; }
    else if (auto* EnumKey = Cast<UBlackboardKeyType_Enum>(Key)) { EnumKey->EnumType = Enum; EnumKey->EnumName = Enum->GetName(); }
    FBlackboardEntry E; E.EntryName = Name; E.KeyType = Key; E.bInstanceSynced = bInstanceSynced;
    if(!Generated.Append(E,BC,Enum)){return Encode(R);} B->Keys.Add(E); R.bChanged = true; R.Data->SetBoolField(TEXT("created"), true); R.Data->SetBoolField(TEXT("already_exists"), false);
    Refresh(B, R); if(!GeneratedBoundary()){return Encode(R);} B->MarkPackageDirty(); if(!GeneratedBoundary()){return Encode(R);} FObservation After;
    if (!Retained(B, AtDispatch, R, After)) { return Encode(R); }
    R.bChanged = true;
    const auto& PriorRows = AtDispatch.State->GetArrayField(TEXT("own_keys")); const auto& Rows = After.State->GetArrayField(TEXT("own_keys"));
    bool Shape = Rows.Num() == PriorRows.Num() + 1 && After.State->GetStringField(TEXT("parent_blackboard")) == AtDispatch.State->GetStringField(TEXT("parent_blackboard"))
        && Text(After.State->GetObjectField(TEXT("other_properties")).ToSharedRef()) == Text(AtDispatch.State->GetObjectField(TEXT("other_properties")).ToSharedRef());
    for (int32 I = 0; Shape && I < PriorRows.Num(); ++I) { Shape &= Text(Rows[I]->AsObject().ToSharedRef()) == Text(PriorRows[I]->AsObject().ToSharedRef()); }
    auto KeyAfter = MakeShared<FJsonObject>(), Unchanged = MakeShared<FJsonObject>(), ReferenceAfter = MakeShared<FJsonObject>();
    Shape &= !B->Keys.IsEmpty() && Entry(B->Keys.Last(), B, KeyAfter) && B->Keys.Last().EntryName == Name && B->Keys.Last().KeyType == Key
        && static_cast<bool>(B->Keys.Last().bInstanceSynced) == bInstanceSynced && Properties(Key, Configured, Unchanged) && Text(Defaults) == Text(Unchanged);
    if (BC) { Shape &= BaseSource(BC, ReferenceAfter); }
    if (Enum) { Shape &= EnumSource(Enum, ReferenceAfter); }
    Shape &= Text(ReferenceBefore) == Text(ReferenceAfter);
    if (Shape && BC) { Shape &= KeyAfter->GetStringField(TEXT("base_class")) == BC->GetPathName(); }
    if (Shape && Enum) { Shape &= KeyAfter->GetStringField(TEXT("enum_path")) == Enum->GetPathName() && KeyAfter->GetStringField(TEXT("enum_name")) == Enum->GetName(); }
    R.Data->SetObjectField(TEXT("key"), Rows.IsEmpty() ? KeyAfter : Rows.Last()->AsObject().ToSharedRef());
    R.Data->SetObjectField(TEXT("reference_after"), ReferenceAfter); R.Data->SetBoolField(TEXT("key_append_and_defaults_verified"), Shape);
    if (!Shape) { Fail(R, TEXT("blackboard_key_postcondition_failed"), TEXT("Actual retained key order/defaults/reference/typed ID readback did not match one append; save suppressed, no automatic retry.")); return Encode(R); }
    R.bPost = true; if (!Save(B, After, R)) { return Encode(R); }
    R.Stage = TEXT("complete"); R.bSuccess = true; return Encode(R);
}

FString RemoveKey(const FString& BlackboardPath, const FString& KeyName, bool bSaveAsset)
{
    FResult R; Init(R, TEXT("remove_ai_blackboard_key"), bSaveAsset); FString Path;
    if(GeneratedBusy()){Fail(R,TEXT("blackboard_generated_reentry"),TEXT("A generated BaseClass operation is active; nested operation rejected before load/SDK."));return Encode(R);} FGeneratedBaseScope Generated(R);
    R.Data->SetBoolField(TEXT("removed"), false); R.Data->SetBoolField(TEXT("already_absent"), false);
    R.Data->SetBoolField(TEXT("key_removal_attempted"), false); R.Data->SetNumberField(TEXT("key_removal_count"), 0);
    R.Data->SetNumberField(TEXT("modify_calls"), 0); R.Data->SetNumberField(TEXT("package_dirty_mark_calls"), 0);
    R.Data->SetNumberField(TEXT("save_attempt_count"), 0); R.Data->SetArrayField(TEXT("save_calls"), {});
    R.Data->SetBoolField(TEXT("removed_key_garbage_destroy_attempted"), false);
    R.Data->SetBoolField(TEXT("behavior_tree_reference_rewrite_attempted"), false);
    R.Data->SetBoolField(TEXT("behavior_tree_consumer_state_independently_observed"), false);
    R.Data->SetBoolField(TEXT("remove_order_and_cache_verified"), false);
    const auto ClearFinalProofs = [&R]()
    {
        for (const TCHAR* K : { TEXT("readback_confirmed"), TEXT("observations_complete"), TEXT("native_sources_preserved"),
            TEXT("parent_assets_preserved"), TEXT("other_package_dirty_preserved"), TEXT("disk_preserved_before_save") }) { R.Data->SetBoolField(K, false); }
    };
    if (!EditorState(R) || !ExactAssetPath(BlackboardPath, true, Path, R)) { return Encode(R); }
    if (!SimpleName(KeyName)) { Fail(R, TEXT("blackboard_key_name_invalid"), TEXT("Use a nonempty simple Blackboard key name of at most 128 characters.")); return Encode(R); }
    auto* B = Resolve(Path, R); TStrongObjectPtr<UBlackboardData> Keep(B); if (!B) { return Encode(R); }
    if(!Generated.Begin(B,false)){return Encode(R);}
    FObservation Before;
    if (!Observe(B, Before)) { Fail(R, TEXT("blackboard_preflight_unobservable"), TEXT("Requires the existing bounded native target/key/parent/cache/source/disk observation before cache refresh or removal.")); return Encode(R); }
    PublishState(R, Before, TEXT("before")); R.Stage = TEXT("refresh_before_removal"); Refresh(B, R);
    FObservation Refreshed; const bool CacheOK = Retained(B, Before, R, Refreshed);
    const auto CachePrefix = Observation(Refreshed); CachePrefix->SetBoolField(TEXT("complete"), R.bKnown);
    CachePrefix->SetBoolField(TEXT("protected_prefix_preserved"), CacheOK);
    if (R.bKnown)
    {
        for (const TCHAR* K : { TEXT("native_sources_preserved"), TEXT("parent_assets_preserved"), TEXT("other_package_dirty_preserved"),
            TEXT("disk_preserved_before_save") }) { CachePrefix->SetBoolField(K, R.Data->GetBoolField(K)); }
        R.Data->SetStringField(TEXT("legacy_state_projection_scope"), TEXT("after_cache_refresh"));
    }
    if (R.bKnown) { R.Data->SetBoolField(TEXT("cache_refresh_changed"), R.bChanged); }
    else { R.Data->SetField(TEXT("cache_refresh_changed"), MakeShared<FJsonValueNull>()); }
    R.Data->SetObjectField(TEXT("after_cache_refresh"), CachePrefix);
    if (!CacheOK)
    {
        if (!R.bKnown) { R.Data->SetObjectField(TEXT("after"), CachePrefix); ClearFinalProofs(); R.Data->SetStringField(TEXT("legacy_state_projection_scope"), TEXT("before (last complete prefix; cache observation incomplete)")); }
        return Encode(R);
    }
    const FName Name(*KeyName); int32 Existing = INDEX_NONE;
    for (int32 I = 0; I < B->Keys.Num(); ++I) { if (B->Keys[I].EntryName == Name) { Existing = I; break; } }
    // The original public contract forbids engine-provided keys. Guard the
    // actual reserved name even when UpdatePersistentKey stores it locally;
    // deleting it and having the public refresh recreate it is not success.
    if (Name == FBlackboard::KeySelf)
    { Fail(R, TEXT("blackboard_key_engine_provided"), TEXT("The engine-provided SelfActor key cannot be removed; cache refresh already ran, no declaration deletion/save.")); return Encode(R); }
    if (Existing == INDEX_NONE)
    {
        if (B->GetKeyID(Name) != FBlackboard::InvalidKey)
        { Fail(R, TEXT("blackboard_key_inherited"), TEXT("The key is inherited and cannot be removed from this asset; cache refresh already ran, no deletion/save.")); return Encode(R); }
        R.Data->SetBoolField(TEXT("already_absent"), true); R.bPost = true; R.bSuccess = true; R.Stage = TEXT("complete"); return Encode(R);
    }
    const FBlackboardEntry Removed = B->Keys[Existing]; TStrongObjectPtr<UBlackboardKeyType> KeepRemoved(Removed.KeyType.Get());
    const auto& PriorRows = Refreshed.State->GetArrayField(TEXT("own_keys"));
    R.Data->SetObjectField(TEXT("removed_key"), PriorRows[Existing]->AsObject());
    auto RemovedExpected = MakeShared<FJsonObject>();
    for (const auto& Pair : PriorRows[Existing]->AsObject()->Values) { if (FString(*Pair.Key) != TEXT("key_id")) { RemovedExpected->SetField(Pair.Key, Pair.Value); } }
    // The complete cache snapshot stays explicitly scoped; the next actual
    // mutation has not been observed and must not inherit its final proof.
    ClearFinalProofs(); R.Data->RemoveField(TEXT("after"));
    R.Stage = TEXT("remove_key"); if(!GeneratedBoundary()){return Encode(R);} B->Modify(); R.Data->SetNumberField(TEXT("modify_calls"), 1);if(!GeneratedBoundary()){return Encode(R);}
    if(!Generated.Remove(Existing)){return Encode(R);} R.Data->SetBoolField(TEXT("key_removal_attempted"), true); B->Keys.RemoveAt(Existing); R.Data->SetNumberField(TEXT("key_removal_count"), 1);
    R.Data->SetBoolField(TEXT("removed"), true);
    Refresh(B, R); if(!GeneratedBoundary()){return Encode(R);} B->MarkPackageDirty(); R.Data->SetNumberField(TEXT("package_dirty_mark_calls"), 1);if(!GeneratedBoundary()){return Encode(R);}
    FObservation After; if (!Retained(B, Refreshed, R, After))
    {
        if (!R.bKnown)
        {
            const auto Partial = Observation(After); Partial->SetBoolField(TEXT("complete"), false); R.Data->SetObjectField(TEXT("after"), Partial);
            ClearFinalProofs(); R.Data->SetStringField(TEXT("legacy_state_projection_scope"), TEXT("after_cache_refresh (last complete prefix; removal observation incomplete)"));
        }
        else { R.Data->SetStringField(TEXT("legacy_state_projection_scope"), TEXT("after (complete retained removal; preservation failed)")); }
        return Encode(R);
    }
    R.Data->SetStringField(TEXT("legacy_state_projection_scope"), TEXT("after")); R.bChanged = true;
    auto Expected = MakeShared<FJsonObject>(); for (const auto& Pair : Refreshed.State->Values) { Expected->SetField(Pair.Key, Pair.Value); }
    TArray<TSharedPtr<FJsonValue>> ExpectedRows;
    for (int32 I = 0; I < PriorRows.Num(); ++I)
    {
        if (I == Existing) { continue; }
        auto Row = MakeShared<FJsonObject>(); for (const auto& Pair : PriorRows[I]->AsObject()->Values) { Row->SetField(Pair.Key, Pair.Value); }
        Row->SetNumberField(TEXT("key_id"), PriorRows[I]->AsObject()->GetNumberField(TEXT("key_id")) - (I > Existing ? 1 : 0));
        ExpectedRows.Add(MakeShared<FJsonValueObject>(Row));
    }
    bool Synced = B->Parent && B->Parent->HasSynchronizedKeys(); for (const auto& E : B->Keys) { Synced |= E.bInstanceSynced; }
    Expected->SetArrayField(TEXT("own_keys"), ExpectedRows); Expected->SetNumberField(TEXT("own_key_count"), ExpectedRows.Num());
    Expected->SetNumberField(TEXT("total_key_count"), Refreshed.State->GetNumberField(TEXT("total_key_count")) - 1);
    Expected->SetNumberField(TEXT("runtime_total_key_count"), Refreshed.State->GetNumberField(TEXT("runtime_total_key_count")) - 1);
    Expected->SetBoolField(TEXT("has_synchronized_keys"), Synced); Expected->SetBoolField(TEXT("package_dirty"), true);
    auto RemovedAfter = MakeShared<FJsonObject>(); const bool RemovedObject = Entry(Removed, B, RemovedAfter) && Text(RemovedExpected) == Text(RemovedAfter);
    bool Shape = Text(Expected) == Text(After.State) && B->GetKeyID(Name) == FBlackboard::InvalidKey && RemovedObject;
    R.Data->SetObjectField(TEXT("removed_key_object_after"), RemovedAfter); R.Data->SetBoolField(TEXT("removed_key_object_preserved_before_save"), RemovedObject);
    R.Data->SetBoolField(TEXT("remove_order_and_cache_verified"), Shape);
    if (!Shape) { R.bPost = false; Fail(R, TEXT("blackboard_remove_postcondition_failed"), TEXT("Removal prefix is retained but exact one-entry deletion/order/remaining identities/defaults/typed-ID/sync/removed-object proof failed; save suppressed, no replay.")); return Encode(R); }
    R.Data->SetStringField(TEXT("reference_warning"), TEXT("Behavior Tree nodes that referenced the removed key are not rewritten"));
    R.bPost = true;
    if (R.bSaveRequested)
    {
        const auto Prefix = R.Data->GetObjectField(TEXT("after")); Prefix->SetBoolField(TEXT("complete"), true); Prefix->SetBoolField(TEXT("protected_prefix_preserved"), true);
        for (const TCHAR* K : { TEXT("readback_confirmed"), TEXT("observations_complete"), TEXT("native_sources_preserved"),
            TEXT("parent_assets_preserved"), TEXT("other_package_dirty_preserved"), TEXT("disk_preserved_before_save") }) { Prefix->SetBoolField(K, R.Data->GetBoolField(K)); }
        Prefix->SetBoolField(TEXT("remove_order_and_cache_verified"), true); ClearFinalProofs(); R.Data->RemoveField(TEXT("disk_preserved_before_save"));
    }
    const bool DirtyBeforeSave = B->GetOutermost()->IsDirty(); const bool SavedOK = Save(B, After, R);
    if (R.bSaveAttempted)
    {
        auto Receipt = MakeShared<FJsonObject>(); Receipt->SetNumberField(TEXT("attempt_index"), 1); Receipt->SetStringField(TEXT("asset_path"), Path);
        Receipt->SetBoolField(TEXT("package_dirty_before"), DirtyBeforeSave); Receipt->SetBoolField(TEXT("success"), R.bSaved); if(!Generated.Stopped){Receipt->SetBoolField(TEXT("package_dirty_after"), B->GetOutermost()->IsDirty());}else{Receipt->SetField(TEXT("package_dirty_after"),MakeShared<FJsonValueNull>());}
        R.Data->SetNumberField(TEXT("save_attempt_count"), 1); R.Data->SetArrayField(TEXT("save_calls"), { MakeShared<FJsonValueObject>(Receipt) });
    }
    if (!SavedOK)
    {
        if (!R.Data->HasField(TEXT("after_save"))) { ClearFinalProofs(); R.Data->SetStringField(TEXT("legacy_state_projection_scope"), TEXT("after (last complete removal prefix; save observation incomplete)")); }
        else { R.Data->SetStringField(TEXT("legacy_state_projection_scope"), TEXT("after_save (complete observation; save outcome/postcondition failed)")); }
        return Encode(R);
    }
    if (R.bSaveAttempted)
    {
        // Successful existing Save already proved the exact selected state,
        // native sources, parents and other dirty fields. Final summaries now
        // describe that actual saved phase, not the retained pre-save prefix.
        for (const TCHAR* K : { TEXT("readback_confirmed"), TEXT("observations_complete"), TEXT("native_sources_preserved"),
            TEXT("parent_assets_preserved"), TEXT("other_package_dirty_preserved") }) { R.Data->SetBoolField(K, true); }
        R.Data->SetStringField(TEXT("legacy_state_projection_scope"), TEXT("after_save"));
    }
    R.Stage = TEXT("complete"); R.bSuccess = true; return Encode(R);
}

FString InspectAsset(const FString& AssetPath)
{
    FResult R; Init(R, TEXT("inspect_ai_asset_structure"), false); FString Path;
    if(GeneratedBusy()){Fail(R,TEXT("blackboard_generated_reentry"),TEXT("A generated BaseClass operation is active; nested operation rejected before load/SDK."));return Encode(R);} FGeneratedBaseScope Generated(R);
    if (!EditorState(R) || !ExactAssetPath(AssetPath, false, Path, R)) { return Encode(R); }
    auto* B = Resolve(Path, R); TStrongObjectPtr<UBlackboardData> Keep(B); if (!B) { return Encode(R); }
    if(!Generated.Begin(B,false)){return Encode(R);}
    FObservation Before, After;
    if (!Observe(B, Before)) { Fail(R, TEXT("blackboard_inspection_unobservable"), TEXT("Requires bounded native Blackboard/key/parent/reference/cache state; BT/EQS/custom key and base-class variants remain pending.")); return Encode(R); }
    PublishState(R, Before, TEXT("before"));
    if (!Retained(B, Before, R, After)) { return Encode(R); }
    if (R.bChanged) { Fail(R, TEXT("blackboard_inspection_state_changed"), TEXT("Repeated current Blackboard/source observations differ after initial asset resolution; no cache refresh/write/save was called.")); return Encode(R); }
    R.Data->SetBoolField(TEXT("read_only_after_initial_resolution"), true); R.bPost = true; R.bSuccess = true; R.Stage = TEXT("complete"); return Encode(R);
}
}
