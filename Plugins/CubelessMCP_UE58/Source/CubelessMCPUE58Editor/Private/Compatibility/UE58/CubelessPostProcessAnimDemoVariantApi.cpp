#include "Compatibility/UE58/CubelessPostProcessAnimDemoVariantApi.h"
#include "Compatibility/UE58/CubelessAnimGraphBoneDemoApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PluginDescriptor.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/ReferencerFinder.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"

namespace Cubeless::UE58::PostProcessAnimDemoVariant
{
namespace
{
using Variable::FResult;
constexpr int32 CarrierChars=1024*1024,MaxPackageObjects=4096,MaxReferences=65536;
bool Running=false;
bool Live(const UObject* Object)
{
    return IsValid(Object)&&!Object->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);
}
bool TextSafe(const FString& Text,int32 Max)
{
    if(Text.Len()>Max) { return false; }
    for(TCHAR Character:Text) { if(Character==0) { return false; } }
    return true;
}
bool JsonTextSafe(const FString& Text)
{
    if(Text.IsEmpty()||!TextSafe(Text,CarrierChars)) { return false; }
    bool Quoted=false,Escaped=false;int32 Depth=0,Slots=0;
    for(TCHAR Character:Text)
    {
        if(Quoted) { if(Escaped) { Escaped=false; }else if(Character==TEXT('\\')) { Escaped=true; }else if(Character==TEXT('"')) { Quoted=false; }continue; }
        if(Character==TEXT('"')) { Quoted=true; }
        else if(Character==TEXT('[')||Character==TEXT('{')) { if(++Depth>64||++Slots>MaxReferences) { return false; } }
        else if(Character==TEXT(']')||Character==TEXT('}')) { if(--Depth<0) { return false; } }
        else if(Character==TEXT(',')&&++Slots>MaxReferences) { return false; }
    }
    return !Quoted&&!Escaped&&Depth==0;
}
bool Decode(const FString& Text,TSharedPtr<FJsonValue>& Out)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    if(!JsonTextSafe(Text)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]")),Values)||Values.Num()!=1||!Values[0].IsValid()) { return false; }
    Out=Values[0];return true;
}
FString NormalizePackage(const FString& Raw)
{
    FString Path=FPackageName::ExportTextPathToObjectPath(Raw).TrimStartAndEnd();Path.TrimQuotesInline();Path.ReplaceInline(TEXT("\\"),TEXT("/"));
    int32 Dot=INDEX_NONE;if(Path.FindChar(TEXT('.'),Dot)) { Path.LeftInline(Dot); }
    while(Path.Contains(TEXT("//"))) { Path.ReplaceInline(TEXT("//"),TEXT("/")); }
    while(Path.EndsWith(TEXT("/"))) { Path.LeftChopInline(1); }
    return Path;
}
FString ObjectPath(const FString& Raw,bool bBlueprint)
{
    FString Path=FPackageName::ExportTextPathToObjectPath(Raw).TrimStartAndEnd();Path.TrimQuotesInline();
    if(bBlueprint&&Path.EndsWith(TEXT("_C"))) { Path.LeftChopInline(2); }
    if((Path.StartsWith(TEXT("/Game/"))||Path.StartsWith(TEXT("/Engine/")))&&!Path.Contains(TEXT(".")))
    { const FString Short=FPackageName::GetShortName(Path);Path=FString::Printf(TEXT("%s.%s"),*Path,*Short); }
    return Path;
}
bool ValidGamePackage(const FString& Path)
{
    const FString Object=ObjectPath(Path,false);
    return TextSafe(Path,NAME_SIZE-1)&&TextSafe(Object,NAME_SIZE-1)&&Path.StartsWith(TEXT("/Game/"),ESearchCase::CaseSensitive)
        &&!Path.Contains(TEXT("."))&&FPackageName::IsValidLongPackageName(Path);
}
bool SamplePath(const FString& Path)
{
    return Path.Equals(TEXT("/Game/_MCP_Sample"),ESearchCase::IgnoreCase)||Path.StartsWith(TEXT("/Game/_MCP_Sample/"),ESearchCase::IgnoreCase);
}
FString VariantSegment(FString Name)
{
    for(TCHAR& Character:Name) { if(!FChar::IsAlnum(Character)&&Character!=TEXT('_')) { Character=TEXT('_'); } }
    Name.TrimStartAndEndInline();return Name.IsEmpty()?TEXT("AnimNodeProbe"):Name;
}
struct FFileMeta
{
    FString Filename;bool Exists=false;int64 Size=-1;FDateTime Stamp;
    explicit FFileMeta(const FString& Package)
    {
        if(Package.IsEmpty()) { return; }
        FString Disk;
        if(FPackageName::DoesPackageExist(Package,&Disk))
        { Filename=Disk;Exists=IFileManager::Get().FileExists(*Filename);Size=IFileManager::Get().FileSize(*Filename);Stamp=IFileManager::Get().GetTimeStamp(*Filename); }
    }
    bool Equals(const FFileMeta& Other) const { return Filename==Other.Filename&&Exists==Other.Exists&&Size==Other.Size&&Stamp==Other.Stamp; }
    TSharedRef<FJsonObject> Json() const
    {
        auto Result=MakeShared<FJsonObject>();Result->SetStringField(TEXT("filename"),Filename);Result->SetBoolField(TEXT("exists"),Exists);
        Result->SetStringField(TEXT("size_decimal"),LexToString(Size));Result->SetStringField(TEXT("timestamp"),Stamp.ToIso8601());return Result;
    }
};
struct FIdentity
{
    TStrongObjectPtr<UObject> Object,Class,Outer,Package;FString Path,PackageName;bool DirtyBefore;FFileMeta FileBefore;
    explicit FIdentity(UObject* Value,bool bObserveFile=false):Object(Value),Class(Value->GetClass()),Outer(Value->GetOuter()),Package(Value->GetPackage()),
        Path(Value->GetPathName()),PackageName(Value->GetPackage()->GetName()),DirtyBefore(Value->GetPackage()->IsDirty()),FileBefore(bObserveFile?PackageName:FString()) {}
    bool Current() const
    {
        UObject* Value=Object.Get();
        return Live(Value)&&Live(Class.Get())&&Live(Package.Get())&&(!Outer.Get()||Live(Outer.Get()))
            &&Value->GetClass()==Class.Get()&&Value->GetOuter()==Outer.Get()&&Value->GetPackage()==Package.Get()&&Value->GetPathName()==Path
            &&Package.Get()->GetName()==PackageName;
    }
    bool Preserved() const { return Current()&&CastChecked<UPackage>(Package.Get())->IsDirty()==DirtyBefore&&FileBefore.Equals(FFileMeta(PackageName)); }
    TSharedRef<FJsonObject> Observation() const
    {
        auto Row=MakeShared<FJsonObject>();const bool Readable=Current();Row->SetStringField(TEXT("path"),Path);Row->SetStringField(TEXT("captured_package"),PackageName);
        Row->SetBoolField(TEXT("current_identity_verified"),Readable);Row->SetBoolField(TEXT("dirty_before"),DirtyBefore);Row->SetObjectField(TEXT("file_before"),FileBefore.Json());
        Row->SetObjectField(TEXT("file_after"),FFileMeta(PackageName).Json());
        if(Readable) { Row->SetBoolField(TEXT("dirty_after"),CastChecked<UPackage>(Package.Get())->IsDirty()); }else { Row->SetField(TEXT("dirty_after"),MakeShared<FJsonValueNull>()); }
        return Row;
    }
};
struct FWork
{
    FResult Result;TSharedRef<FJsonObject> Legacy=MakeShared<FJsonObject>(),Calls=MakeShared<FJsonObject>();
    TSharedRef<FJsonObject> Compile=MakeShared<FJsonObject>(),Save=MakeShared<FJsonObject>();
    TArray<TUniquePtr<FIdentity>> Sources;TArray<TSharedPtr<FJsonValue>> Preparation,Errors;
    FIdentity* TargetBlueprint=nullptr;FIdentity* TargetMesh=nullptr;TArray<TUniquePtr<FIdentity>> Targets;
    TUniquePtr<FIdentity> AssignedClass;UAnimBlueprint* ClassOwner=nullptr;TSet<FString> TargetPackages;
    explicit FWork(bool bCompile,bool bSave,bool bDryRun)
    {
        Result.bCompileRequested=bCompile;Result.bSaveRequested=bSave;Legacy->SetBoolField(TEXT("success"),false);Legacy->SetBoolField(TEXT("dry_run"),bDryRun);
        Legacy->SetBoolField(TEXT("original_assets_modified"),false);Result.Data->SetBoolField(TEXT("original_assets_modified_is_legacy_intent_only"),true);
        Result.Data->SetObjectField(TEXT("legacy_result"),Legacy);Result.Data->SetObjectField(TEXT("native_call_counts"),Calls);
        for(const TCHAR* Name:{TEXT("asset_delete_calls"),TEXT("asset_delete_returns"),TEXT("asset_duplicate_calls"),TEXT("asset_duplicate_returns"),
            TEXT("make_directory_calls"),TEXT("EC_ensure_modify_bone_calls"),TEXT("EC_ensure_modify_bone_returns"),TEXT("compile_blueprint_calls"),TEXT("compile_blueprint_returns"),
            TEXT("asset_save_calls"),TEXT("asset_save_returns"),TEXT("mesh_modify_calls"),TEXT("mesh_modify_returns"),TEXT("post_process_class_get_calls"),
            TEXT("set_post_process_class_calls"),TEXT("set_post_process_class_returns"),TEXT("mesh_mark_dirty_calls"),TEXT("mesh_package_mark_dirty_calls"),TEXT("mesh_post_edit_change_calls"),TEXT("mesh_post_edit_change_returns")})
        { Calls->SetNumberField(Name,0); }
        Result.Data->SetBoolField(TEXT("atomic_rollback_available"),false);Result.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);
        Result.Data->SetBoolField(TEXT("disk_reload_verified"),false);Result.Data->SetBoolField(TEXT("runtime_pose_verified"),false);Result.Data->SetBoolField(TEXT("packaging_verified"),false);
        Result.Data->SetBoolField(TEXT("duplicate_implicit_skeleton_compile_possible"),true);Result.Data->SetBoolField(TEXT("duplicate_source_control_implicit_save_possible"),true);
        Result.Data->SetBoolField(TEXT("implicit_compile_or_save_completion_measured"),false);
        Result.Data->SetStringField(TEXT("total_change_scope"),TEXT("SDK load/registry/duplicate/compiler/PostEdit/save callbacks are opaque; after the first SDK acquisition total change remains unknown. Observed explicit writes are reported separately."));
        Result.Data->SetStringField(TEXT("source_preservation_scope"),TEXT("No explicit source writes; captured source object/class/outer/actual package identity, package dirty flag and disk filename/size/timestamp during this operation. This is not a full RAM, byte hash or callback purity proof."));
        Result.Data->SetStringField(TEXT("runtime_dependency_guard_scope"),TEXT("Known MCP/integration namespace, loaded object/class/super/Blueprint parent chains, bounded same-package nontransient object references and saved Package-category dependency projection. Custom serialization, soft references in dirty RAM and complete runtime packaging are not proved."));
        Result.Data->SetStringField(TEXT("synchronous_SDK_limit"),TEXT("DuplicateAsset, CompileBlueprint, PostEditChange and SaveAsset are synchronous public Editor APIs. Mesh IsCompiling is checked before public postprocess getters/setters, but their internal async-property waits cannot be bounded by this API."));
        Compile->SetBoolField(TEXT("requested"),bCompile);Compile->SetBoolField(TEXT("forced_for_generated_class"),false);Compile->SetBoolField(TEXT("attempted"),false);
        Save->SetBoolField(TEXT("requested"),bSave);Save->SetBoolField(TEXT("blueprint_saved"),false);Save->SetBoolField(TEXT("skeletal_mesh_saved"),false);
        Legacy->SetObjectField(TEXT("compile"),Compile);Legacy->SetObjectField(TEXT("save"),Save);
    }
    void Count(const TCHAR* Name) { double Value=0;Calls->TryGetNumberField(Name,Value);Calls->SetNumberField(Name,Value+1); }
    void SDK(const TCHAR* Name) { Result.bChangeStateKnown=false;Count(Name); }
    void Writer(const TCHAR* Name) { Result.bMutationAttempted=true;SDK(Name); }
    bool Fail(const TCHAR* Code,const FString& Message)
    {
        if(Result.Error.IsEmpty()) { Result.ErrorCode=FString(TEXT("postprocess_anim_demo_variant_"))+Code;Result.Error=Message; }
        Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("error"),Result.Error);return false;
    }
    bool Current(bool bCheckTargets=true)
    {
        if(!Result.Error.IsEmpty()) { return false; }
        if(!IsEditorGameThread()||IsPlaySessionActive()) { return Fail(TEXT("editor_changed"),TEXT("Idle Editor game-thread boundary changed after SDK callback")); }
        for(const auto& Source:Sources)
        { if(!Source->Preserved()) { return Fail(TEXT("source_observation_changed"),TEXT("Captured source identity, package dirty state or disk metadata changed; no further explicit writes")); } }
        if(bCheckTargets)
        { for(const auto& Target:Targets) { if(!Target->Current()) { return Fail(TEXT("target_identity_changed"),TEXT("Captured target class/outer/actual package/path changed after SDK callback")); } } }
        if(AssignedClass&&(!AssignedClass->Current()||!Live(ClassOwner)||ClassOwner->GeneratedClass!=AssignedClass->Object.Get()
            ||CastChecked<UClass>(AssignedClass->Object.Get())->ClassGeneratedBy!=ClassOwner||CastChecked<UClass>(AssignedClass->Object.Get())->HasAnyClassFlags(CLASS_NewerVersionExists)))
        { return Fail(TEXT("generated_class_identity_changed"),TEXT("Captured generated class or its Blueprint owner changed after SDK callback")); }
        return true;
    }
    bool IsSource(UObject* Value) const
    {
        if(!Live(Value)||!Live(Value->GetPackage())) { return true; }
        for(const auto& Source:Sources) { if(Value==Source->Object.Get()||Value->GetPackage()==Source->Package.Get()) { return true; } }
        return false;
    }
    FIdentity* AddTarget(UObject* Value,const FString& Expected)
    {
        if(!Live(Value)||!Live(Value->GetClass())||!Live(Value->GetPackage())||IsSource(Value)||!Value->GetPackage()->GetName().Equals(Expected,ESearchCase::IgnoreCase))
        { Fail(TEXT("target_scope_changed"),TEXT("Returned target is unreadable, aliases a source, or does not belong to the declared target package"));return nullptr; }
        auto Id=MakeUnique<FIdentity>(Value);FIdentity* Pointer=Id.Get();Targets.Add(MoveTemp(Id));return Pointer;
    }
    FResult Finish()
    {
        if(!Result.bSuccess&&Result.Error.IsEmpty()) { Fail(TEXT("operation_incomplete"),TEXT("Operation stopped before postconditions; inspect the failed stage and preserved partial prefix")); }
        TArray<TSharedPtr<FJsonValue>> Observations;bool SourcesPreserved=true;
        for(const auto& Source:Sources) { SourcesPreserved&=Source->Preserved();Observations.Add(MakeShared<FJsonValueObject>(Source->Observation())); }
        Result.Data->SetArrayField(TEXT("source_observations"),Observations);Result.Data->SetBoolField(TEXT("captured_source_observations_preserved"),SourcesPreserved);
        Result.Data->SetNumberField(TEXT("source_observation_count"),Observations.Num());Result.Data->SetBoolField(TEXT("complete_original_asset_immutability_proof"),false);
        Result.Data->SetArrayField(TEXT("preparation"),Preparation);Result.Data->SetArrayField(TEXT("errors"),Errors);Legacy->SetArrayField(TEXT("errors"),Errors);
        Result.Data->SetBoolField(TEXT("observed_explicit_change"),Result.bChanged);
        if(!SourcesPreserved) { Result.bSuccess=false;Result.bPostconditionPassed=false;Fail(TEXT("source_observation_changed"),TEXT("Captured source preservation observation failed")); }
        Legacy->SetBoolField(TEXT("success"),Result.bSuccess);return Result;
    }
};
const TArray<FString>& ForbiddenPrefixes()
{
    static const TArray<FString> Prefixes=[]()
    {
        TArray<FString> Values={TEXT("/UnrealMCP/"),TEXT("/MCPUnreal/"),TEXT("/MCP_Unreal/"),TEXT("/CubelessControlRigTools/"),
            TEXT("/Script/UnrealMCP"),TEXT("/Script/MCPUnreal"),TEXT("/Script/MCP_Unreal"),TEXT("/Script/CubelessControlRigTools"),TEXT("/Script/CubelessMCP")};
        for(const TCHAR* Name:{TEXT("CubelessMCP_UE58"),TEXT("ModelContextProtocol"),TEXT("ToolsetRegistry"),TEXT("PythonScriptPlugin"),TEXT("EditorScriptingUtilities")})
        {
            Values.Add(FString::Printf(TEXT("/%s/"),Name));
            if(auto Plugin=IPluginManager::Get().FindPlugin(Name))
            { for(const FModuleDescriptor& Module:Plugin->GetDescriptor().Modules) { Values.Add(FString::Printf(TEXT("/Script/%s"),*Module.Name.ToString())); } }
        }
        return Values;
    }();return Prefixes;
}
bool Forbidden(const FString& Path)
{
    for(const FString& Prefix:ForbiddenPrefixes()) { if(Path.Contains(Prefix,ESearchCase::IgnoreCase)) { return true; } }return false;
}
bool ClassAllowed(UClass* Class,FWork& Work)
{
    TSet<UClass*> Seen;TArray<TStrongObjectPtr<UClass>> Held;
    for(int32 Depth=0;Class;Class=Class->GetSuperClass(),++Depth)
    {
        if(Depth>=256||Seen.Contains(Class)||!Live(Class)||!Live(Class->GetPackage())||Forbidden(Class->GetPathName()))
        { return Work.Fail(TEXT("runtime_dependency_refused"),TEXT("Class lineage is unreadable/cyclic/bounded out or references an MCP/integration module")); }
        Seen.Add(Class);Held.Emplace(Class);
    }
    return true;
}
bool ObjectAllowed(UObject* Object,FWork& Work)
{
    if(!Live(Object)||Forbidden(Object->GetPathName())||!ClassAllowed(Object->GetClass(),Work))
    { return Work.Fail(TEXT("runtime_dependency_refused"),TEXT("Object is unreadable or references a forbidden MCP/integration namespace")); }
    if(auto* ClassObject=Cast<UClass>(Object)) { if(!ClassAllowed(ClassObject,Work)) { return false; } }
    if(auto* BlueprintObject=Cast<UBlueprint>(Object))
    { if(!Live(BlueprintObject->ParentClass)||!ClassAllowed(BlueprintObject->ParentClass,Work)||(BlueprintObject->GeneratedClass&&!ClassAllowed(BlueprintObject->GeneratedClass,Work)))
      { return Work.Fail(TEXT("runtime_dependency_refused"),TEXT("Blueprint parent or generated class is unreadable or introduces a forbidden integration dependency")); } }
    return true;
}
class FBoundedReferenceFinder final:public FReferenceFinder
{
public:
    bool Valid=true;int32 Slots=0;
    explicit FBoundedReferenceFinder(TArray<UObject*>& Values):FReferenceFinder(Values,nullptr,false,true,false,true) {}
    virtual void HandleObjectReference(UObject*& Object,const UObject* ReferencingObject,const FProperty* Property) override
    {
        if(Slots>MaxReferences) { Valid=false;return; }++Slots;
        if(Slots>MaxReferences|| (Object&&!Live(Object))) { Valid=false;return; }
        FReferenceFinder::HandleObjectReference(Object,ReferencingObject,Property);
    }
};
bool RuntimePackageGuard(UObject* Root,FWork& Work,const TCHAR* Receipt)
{
    if(!ObjectAllowed(Root,Work)||!Work.Current()) { return false; }
    TArray<UObject*> Members;Work.SDK(TEXT("runtime_package_inventory_calls"));GetObjectsWithPackage(Root->GetPackage(),Members,EGetObjectsFlags::IncludeNestedObjects,RF_NoFlags,EInternalObjectFlags::Garbage);
    if(!Work.Current()||Members.Num()>MaxPackageObjects) { return Work.Fail(TEXT("runtime_guard_bound"),TEXT("Current package object inventory exceeds4096 or root identity changed")); }
    TArray<TUniquePtr<FIdentity>> Identities;
    for(UObject* Member:Members)
    {
        if(!Live(Member)||!Live(Member->GetPackage())||!ObjectAllowed(Member,Work))
        {
            auto Detail=MakeShared<FJsonObject>();
            Detail->SetStringField(TEXT("object_path"),GetPathNameSafe(Member));
            Detail->SetStringField(TEXT("class_path"),Member?GetPathNameSafe(Member->GetClass()):FString());
            Detail->SetStringField(TEXT("package_path"),Member?GetPathNameSafe(Member->GetPackage()):FString());
            Detail->SetStringField(TEXT("object_flags_decimal"),Member?LexToString(static_cast<uint32>(Member->GetFlags())):FString());
            Detail->SetBoolField(TEXT("member_live"),Live(Member));
            Detail->SetBoolField(TEXT("package_live"),Member&&Live(Member->GetPackage()));
            Work.Result.Data->SetObjectField(TEXT("runtime_guard_failed_member"),Detail);
            // Preserve an earlier precise ObjectAllowed failure instead of replacing its cause.
            if(!Work.Result.Error.IsEmpty()) { return false; }
            return Work.Fail(TEXT("runtime_guard_member_unreadable"),TEXT("Package member identity or runtime dependency admission failed; inspect runtime_guard_failed_member"));
        }
        Identities.Add(MakeUnique<FIdentity>(Member));
    }
    int32 TotalSlots=0;
    for(const auto& Id:Identities)
    {
        if(!Id->Current()||!Work.Current()) { return Work.Fail(TEXT("runtime_guard_identity"),TEXT("Package member changed before reference observation")); }
        TArray<UObject*> References;FBoundedReferenceFinder Finder(References);Work.SDK(TEXT("runtime_reference_projection_calls"));Finder.FindReferences(Id->Object.Get());TotalSlots+=Finder.Slots;
        if(!Finder.Valid||TotalSlots>MaxReferences||!Id->Current()||!Work.Current())
        { return Work.Fail(TEXT("runtime_guard_incomplete"),TEXT("Reference callback changed admitted identities or exceeded65536 total reference slots")); }
        for(UObject* Reference:References) { if(!ObjectAllowed(Reference,Work)) { return false; } }
    }
    for(const auto& Id:Identities) { if(!Id->Current()) { return Work.Fail(TEXT("runtime_guard_identity"),TEXT("Package member changed during reference projection")); } }
    auto Report=MakeShared<FJsonObject>();Report->SetNumberField(TEXT("objects"),Members.Num());Report->SetNumberField(TEXT("reference_slots"),TotalSlots);
    Report->SetBoolField(TEXT("known_forbidden_reference_found"),false);Report->SetBoolField(TEXT("complete_packaging_proof"),false);
    Work.Result.Data->SetObjectField(Receipt,Report);return Work.Current();
}
bool SavedDependencyGuard(const TArray<FString>& Roots,FWork& Work)
{
    Work.SDK(TEXT("saved_dependency_registry_acquisition"));auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    auto Report=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Missing;TArray<FName> Queue;TSet<FName> Seen;
    for(const FString& Root:Roots) { FName Name(*Root);if(!Seen.Contains(Name)) { Seen.Add(Name);Queue.Add(Name); } }
    int32 Index=0,Edges=0;const bool DiscoveryComplete=!Registry.IsLoadingAssets();
    if(DiscoveryComplete)
    {
        while(Index<Queue.Num()&&Index<2048)
        {
            const FName Package=Queue[Index++];TArray<FName> Dependencies;
            Work.SDK(TEXT("saved_package_dependencies_calls"));const bool Found=Registry.GetDependencies(Package,Dependencies,UE::AssetRegistry::EDependencyCategory::Package);
            if(!Work.Current()) { return false; }
            if(!Found) { Missing.Add(MakeShared<FJsonValueString>(Package.ToString()));continue; }
            if(Dependencies.Num()>MaxReferences-Edges) { Work.Result.Data->SetObjectField(TEXT("saved_dependency_projection"),Report);return Work.Fail(TEXT("runtime_guard_bound"),TEXT("Saved dependency edge projection exceeds65536")); }
            Edges+=Dependencies.Num();
            for(FName Dependency:Dependencies)
            {
                const FString Name=Dependency.ToString();if(Forbidden(Name)) { Report->SetStringField(TEXT("forbidden_dependency"),Name);Work.Result.Data->SetObjectField(TEXT("saved_dependency_projection"),Report);return Work.Fail(TEXT("runtime_dependency_refused"),TEXT("Saved package dependency references an MCP/integration plugin")); }
                if(!Name.StartsWith(TEXT("/Script/"))&&!Seen.Contains(Dependency)) { if(Queue.Num()>=2048) { Missing.Add(MakeShared<FJsonValueString>(TEXT("package_bound")));continue; }Seen.Add(Dependency);Queue.Add(Dependency); }
            }
        }
    }
    Report->SetBoolField(TEXT("discovery_complete"),DiscoveryComplete);Report->SetNumberField(TEXT("packages_processed"),Index);Report->SetNumberField(TEXT("edge_count"),Edges);Report->SetArrayField(TEXT("missing_or_bounded"),Missing);
    Report->SetStringField(TEXT("scope"),TEXT("Available saved Package-category hard/soft dependency metadata only; dirty RAM and all management relationships are not represented"));
    Report->SetBoolField(TEXT("complete_packaging_proof"),false);Work.Result.Data->SetObjectField(TEXT("saved_dependency_projection"),Report);return Work.Current();
}
UObject* AssetLoad(const FString& Path,FWork& Work)
{
    if(!TextSafe(ObjectPath(Path,false),NAME_SIZE-1)) { Work.Fail(TEXT("load_path_bound"),TEXT("Composed asset path exceeds FName/NUL safety bound"));return nullptr; }
    Work.SDK(TEXT("asset_load_calls"));UObject* Object=UEditorAssetLibrary::LoadAsset(Path);Work.Count(TEXT("asset_load_returns"));return Object;
}
UBlueprint* LoadBlueprintPath(const FString& Path,FWork& Work)
{
    const FString Package=FPackageName::ObjectPathToPackageName(Path);
    if(Path.IsEmpty()||!TextSafe(Path,NAME_SIZE-1)||!FPackageName::IsValidLongPackageName(Package,true)||Path.Contains(TEXT("//"))) { return nullptr; }
    Work.SDK(TEXT("blueprint_load_calls"));if(auto* Blueprint=LoadObject<UBlueprint>(nullptr,*Path)) { return Blueprint; }
    FString ClassPath=Path;if(!ClassPath.EndsWith(TEXT("_C"))) { ClassPath+=TEXT("_C"); }
    if(!TextSafe(ClassPath,NAME_SIZE-1)) { return nullptr; }
    Work.SDK(TEXT("blueprint_class_load_calls"));if(auto* Class=LoadObject<UClass>(nullptr,*ClassPath))
    { TStrongObjectPtr<UClass> HeldClass(Class);return Live(Class)?Cast<UBlueprint>(Class->ClassGeneratedBy):nullptr; }
    return nullptr;
}
UBlueprint* FindBlueprint(const FString& Name,FWork& Work)
{
    const FString Path=ObjectPath(Name,true);if(auto* Direct=LoadBlueprintPath(Path,Work)) { return Direct; }
    // Preserve original CommonUtils level lookup, although the caller then rejects non-AnimBlueprints.
    if(TextSafe(Path,NAME_SIZE-1)&&!Path.IsEmpty())
    {
        Work.SDK(TEXT("world_find_or_load_calls"));auto* World=FindObject<UWorld>(nullptr,*Path);if(!World) { World=LoadObject<UWorld>(nullptr,*Path); }
        TStrongObjectPtr<UWorld> HeldWorld(World);
        if(Live(World)&&Live(World->PersistentLevel))
        { if(auto* LevelBlueprint=World->PersistentLevel->GetLevelScriptBlueprint(true)) { return LevelBlueprint; }
          if(auto* ExistingLevelBlueprint=FindObject<ULevelScriptBlueprint>(World->PersistentLevel,*World->GetName())) { return ExistingLevelBlueprint; } }
    }
    if(!Name.Contains(TEXT("/"))&&!Name.Contains(TEXT(".")))
    { const FString Legacy=FString::Printf(TEXT("/Game/Blueprints/%s.%s"),*Name,*Name);if(auto* Blueprint=LoadBlueprintPath(Legacy,Work)) { return Blueprint; } }
    Work.SDK(TEXT("blueprint_registry_acquisition"));auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    TArray<FAssetData> Assets;Work.Count(TEXT("blueprint_registry_queries"));Registry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(),Assets,true);
    if(Assets.Num()>4096) { Work.Fail(TEXT("source_lookup_bound"),TEXT("Source Blueprint registry lookup exceeds4096; use a full asset path"));return nullptr; }
    TArray<FString> Candidates;const FString Short=FPackageName::GetShortName(Path);
    for(const FAssetData& Asset:Assets)
    {
        const FString Object=Asset.GetObjectPathString();
        if(Asset.AssetName.ToString().Equals(Name,ESearchCase::IgnoreCase)||Asset.AssetName.ToString().Equals(Short,ESearchCase::IgnoreCase)
            ||Asset.PackageName.ToString().Equals(Name,ESearchCase::IgnoreCase)||Object.Equals(Path,ESearchCase::IgnoreCase)) { Candidates.Add(Object); }
    }
    if(Candidates.Num()==1) { return LoadBlueprintPath(Candidates[0],Work); }
    if(Candidates.Num()>1) { Work.Fail(TEXT("source_ambiguous"),TEXT("Ambiguous source Blueprint name; provide a full asset path")); }
    return nullptr;
}
struct FTargetPlan
{
    FString Path,Label;UClass* Expected;bool Exists=false;TUniquePtr<FIdentity> Existing;
    TSharedRef<FJsonObject> Receipt=MakeShared<FJsonObject>();
    FTargetPlan(const FString& InPath,const TCHAR* InLabel,UClass* InExpected):Path(InPath),Label(InLabel),Expected(InExpected)
    { Receipt->SetStringField(TEXT("target"),Path);Receipt->SetStringField(TEXT("label"),Label);Receipt->SetBoolField(TEXT("created"),false);Receipt->SetBoolField(TEXT("reused"),false); }
};
bool InspectTarget(FTargetPlan& Plan,FWork& Work)
{
    Work.Preparation.Add(MakeShared<FJsonValueObject>(Plan.Receipt));
    Work.SDK(TEXT("asset_exists_calls"));Plan.Exists=UEditorAssetLibrary::DoesAssetExist(Plan.Path);Plan.Receipt->SetBoolField(TEXT("existed_before"),Plan.Exists);
    if(!Work.Current()) { return false; }
    if(Plan.Exists)
    {
        UObject* Existing=AssetLoad(Plan.Path,Work);
        if(!Work.Current()) { return false; }
        if(Existing)
        {
            if(!Live(Existing)||!Live(Existing->GetClass())||!Live(Existing->GetPackage())||Work.IsSource(Existing)||!Existing->GetPackage()->GetName().Equals(Plan.Path,ESearchCase::IgnoreCase))
            { return Work.Fail(TEXT("target_alias_refused"),TEXT("Existing target redirects outside its declared package or aliases a protected source")); }
            Plan.Existing=MakeUnique<FIdentity>(Existing);Plan.Receipt->SetStringField(TEXT("existing_class"),Existing->GetClass()->GetPathName());
        }
    }
    return true;
}
bool OverwriteReferencesSafe(FTargetPlan& Plan,FWork& Work)
{
    if(!Plan.Exists) { return true; }
    if(!Plan.Existing||!Plan.Existing->Current()||!Work.Current())
    { return Work.Fail(TEXT("overwrite_unobservable"),TEXT("Overwrite requires the complete existing target identity before force deletion")); }
    for(const auto& Source:Work.Sources)
    { if(Source->DirtyBefore) { return Work.Fail(TEXT("overwrite_source_dirty"),TEXT("Overwrite refuses dirty source packages: their unsaved soft-reference dependencies cannot be protected by saved metadata")); } }
    auto AllowedPackage=[&](const FString& Package)
    { for(const FString& Target:Work.TargetPackages) { if(Package.Equals(Target,ESearchCase::IgnoreCase)) { return true; } }return false; };
    Work.SDK(TEXT("overwrite_referencer_registry_acquisition"));auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    if(Registry.IsLoadingAssets()) { return Work.Fail(TEXT("overwrite_registry_pending"),TEXT("Overwrite requires completed Asset Registry discovery; no automatic retry")); }
    TArray<FName> SavedReferencers;Work.Count(TEXT("overwrite_saved_referencer_queries"));
    if(!Registry.GetReferencers(FName(*Plan.Path),SavedReferencers,UE::AssetRegistry::EDependencyCategory::Package)||SavedReferencers.Num()>MaxReferences)
    { return Work.Fail(TEXT("overwrite_referencers_incomplete"),TEXT("Saved target referencer projection is unavailable or exceeds65536")); }
    if(!Work.Current()||!Plan.Existing->Current()) { return false; }
    for(FName Referencer:SavedReferencers)
    { if(!AllowedPackage(Referencer.ToString())) { Plan.Receipt->SetStringField(TEXT("protected_saved_referencer"),Referencer.ToString());return Work.Fail(TEXT("overwrite_external_reference"),TEXT("Force deletion could alter a package outside the two requested targets")); } }
    TArray<UObject*> Referencees;GetObjectsWithPackage(CastChecked<UPackage>(Plan.Existing->Package.Get()),Referencees,true);
    if(Referencees.Num()>MaxPackageObjects) { return Work.Fail(TEXT("overwrite_referencers_bound"),TEXT("Target member inventory exceeds4096 before overwrite")); }
    TArray<TStrongObjectPtr<UObject>> Held;TArray<TUniquePtr<FIdentity>> MemberIds;
    Referencees.AddUnique(Plan.Existing->Object.Get());
    for(UObject* Object:Referencees)
    { if(!Live(Object)||!Live(Object->GetPackage())||!Live(Object->GetClass())) { return Work.Fail(TEXT("overwrite_member_unreadable"),TEXT("Target member unavailable before referencer observation")); }Held.Emplace(Object);MemberIds.Add(MakeUnique<FIdentity>(Object)); }
    Work.SDK(TEXT("overwrite_memory_referencer_queries"));const TArray<UObject*> Referencers=FReferencerFinder::GetAllReferencers(Referencees,nullptr,EReferencerFinderFlags::SkipWeakReferences);
    if(Referencers.Num()>MaxReferences||!Work.Current()||!Plan.Existing->Current()) { return Work.Fail(TEXT("overwrite_referencers_incomplete"),TEXT("Memory referencer observation exceeded its bound or changed admitted identities")); }
    for(const auto& Id:MemberIds) { if(!Id->Current()) { return Work.Fail(TEXT("overwrite_member_changed"),TEXT("Target member changed during referencer observation")); } }
    for(UObject* Referencer:Referencers)
    {
        if(!Live(Referencer)||!Live(Referencer->GetPackage())) { return Work.Fail(TEXT("overwrite_referencer_unreadable"),TEXT("Unreadable memory referencer before force deletion")); }
        // Public force-delete may clear transient editor/undo references; user/source packages are excluded.
        if(Referencer->GetPackage()!=GetTransientPackage()&&!AllowedPackage(Referencer->GetPackage()->GetName()))
        { Plan.Receipt->SetStringField(TEXT("protected_memory_referencer"),Referencer->GetPathName());return Work.Fail(TEXT("overwrite_external_reference"),TEXT("A loaded package outside the two requested targets refers to the asset being overwritten")); }
    }
    Plan.Receipt->SetBoolField(TEXT("overwrite_external_package_referencer_projection_passed"),true);
    Plan.Receipt->SetBoolField(TEXT("force_delete_can_clear_transient_editor_undo_references"),true);return Work.Current();
}
UObject* PrepareTarget(UObject* Source,FTargetPlan& Plan,bool bOverwrite,FWork& Work)
{
    if(!Work.Current()) { return nullptr; }
    if(Plan.Exists)
    {
        if(Plan.Existing&&!Plan.Existing->Current()) { Work.Fail(TEXT("target_identity_changed"),TEXT("Existing target changed after the complete source/target admission"));return nullptr; }
        if(Plan.Existing&&!bOverwrite&&Plan.Existing->Object->IsA(Plan.Expected))
        { Plan.Receipt->SetBoolField(TEXT("reused"),true);return Plan.Existing->Object.Get(); }
        if(!bOverwrite)
        { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Existing target ")+Plan.Label+TEXT(" is unloadable or has an unexpected class: ")+Plan.Path));return nullptr; }
        if(!OverwriteReferencesSafe(Plan,Work)) { return nullptr; }
        Work.Writer(TEXT("asset_delete_calls"));Plan.Receipt->SetBoolField(TEXT("delete_attempted"),true);
        const bool Deleted=UEditorAssetLibrary::DeleteAsset(Plan.Path);Work.Count(TEXT("asset_delete_returns"));Plan.Receipt->SetBoolField(TEXT("delete_returned"),Deleted);Work.Result.bChanged|=Deleted;
        Plan.Existing.Reset();
        if(!Work.Current()) { return nullptr; }
        if(!Deleted) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Failed to delete existing target ")+Plan.Label+TEXT(": ")+Plan.Path));return nullptr; }
    }
    const FString Directory=FPackageName::GetLongPackagePath(Plan.Path);
    if(!Directory.IsEmpty())
    { Work.Writer(TEXT("make_directory_calls"));Plan.Receipt->SetBoolField(TEXT("make_directory_returned"),UEditorAssetLibrary::MakeDirectory(Directory));if(!Work.Current()) { return nullptr; } }
    const FString SourcePackage=Source->GetPackage()->GetName();Work.Writer(TEXT("asset_duplicate_calls"));Plan.Receipt->SetBoolField(TEXT("duplicate_attempted"),true);
    UObject* Duplicated=UEditorAssetLibrary::DuplicateAsset(SourcePackage,Plan.Path);Work.Count(TEXT("asset_duplicate_returns"));
    Plan.Receipt->SetBoolField(TEXT("duplicate_returned_object"),Duplicated!=nullptr);
    if(Duplicated&&Live(Duplicated)) { Work.Result.bChanged=true;Plan.Receipt->SetStringField(TEXT("duplicate_return_path"),Duplicated->GetPathName()); }
    if(!Work.Current()||!Live(Duplicated)||!Duplicated->IsA(Plan.Expected))
    { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Failed to duplicate ")+Plan.Label+TEXT(" '")+SourcePackage+TEXT("' to '")+Plan.Path+TEXT("'")));return nullptr; }
    Plan.Receipt->SetBoolField(TEXT("created"),true);return Duplicated;
}
bool SaveOne(FIdentity& Id,const TCHAR* Key,FWork& Work)
{
    if(!Work.Current()) { return false; }
    UObject* ByPath=AssetLoad(Id.PackageName,Work);
    if(!Work.Current()||ByPath!=Id.Object.Get()) { return Work.Fail(TEXT("save_identity_changed"),TEXT("Save target path no longer resolves to the captured object")); }
    const FFileMeta Before(Id.PackageName);auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("package"),Id.PackageName);Receipt->SetObjectField(TEXT("file_before"),Before.Json());
    Work.Save->SetObjectField(FString(Key)+TEXT("_receipt"),Receipt);Work.Result.bSaveAttempted=true;Work.Writer(TEXT("asset_save_calls"));Receipt->SetBoolField(TEXT("attempted"),true);
    const bool Saved=UEditorAssetLibrary::SaveAsset(Id.PackageName,false);Work.Count(TEXT("asset_save_returns"));Receipt->SetBoolField(TEXT("SDK_returned"),Saved);
    const bool Current=Id.Current();const FFileMeta After(Id.PackageName);Receipt->SetObjectField(TEXT("file_after"),After.Json());Receipt->SetBoolField(TEXT("target_identity_verified"),Current);
    const bool FileChanged=!Before.Equals(After);Receipt->SetBoolField(TEXT("file_metadata_changed"),FileChanged);Work.Result.bChanged|=FileChanged;
    const bool Clean=Current&&!CastChecked<UPackage>(Id.Package.Get())->IsDirty();Receipt->SetBoolField(TEXT("package_clean"),Clean);Receipt->SetBoolField(TEXT("independent_byte_content_or_reload_verified"),false);
    const bool Verified=Saved&&Current&&Clean&&After.Exists&&After.Size>0;Work.Save->SetBoolField(Key,Verified);
    return Work.Current()&&Verified;
}
TSharedRef<FJsonObject> GraphOutcome(const FResult& Result)
{
    auto Json=MakeShared<FJsonObject>();Json->SetBoolField(TEXT("success"),Result.bSuccess);Json->SetBoolField(TEXT("mutation_attempted"),Result.bMutationAttempted);
    Json->SetBoolField(TEXT("observed_changed"),Result.bChanged);Json->SetBoolField(TEXT("change_state_known"),Result.bChangeStateKnown);Json->SetBoolField(TEXT("postcondition_passed"),Result.bPostconditionPassed);
    Json->SetBoolField(TEXT("compile_requested"),Result.bCompileRequested);Json->SetBoolField(TEXT("compile_attempted"),Result.bCompileAttempted);
    if(Result.Compiled.IsSet()) { Json->SetBoolField(TEXT("compiled"),Result.Compiled.GetValue()); }else { Json->SetField(TEXT("compiled"),MakeShared<FJsonValueNull>()); }
    Json->SetBoolField(TEXT("save_requested"),Result.bSaveRequested);Json->SetBoolField(TEXT("save_attempted"),Result.bSaveAttempted);Json->SetBoolField(TEXT("saved"),Result.bSaved);
    Json->SetStringField(TEXT("stage"),Result.Stage);Json->SetStringField(TEXT("error_code"),Result.ErrorCode);Json->SetStringField(TEXT("error"),Result.Error);Json->SetObjectField(TEXT("data"),Result.Data);return Json;
}
bool MeshReady(USkeletalMesh* Mesh,FWork& Work)
{
    if(!Work.Current()||!Live(Mesh)) { return false; }
    Work.SDK(TEXT("mesh_is_compiling_queries"));const bool Pending=Mesh->IsCompiling();
    if(!Work.Current()) { return false; }
    return !Pending||Work.Fail(TEXT("mesh_compilation_pending"),TEXT("Mesh asynchronous compilation is pending; no explicit wait or automatic writer retry is performed"));
}
}
Variable::FResult Ensure(const FString& SourceBlueprintName,const FString& SourceSkeletalMesh,const FString& VariantName,const FString& TargetRoot,const FString& TargetBlueprintNameJson,const FString& TargetSkeletalMeshJson,const FString& BoneName,const FString& RotationJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bReplaceExisting,bool bOverwriteExisting,bool bCompile,bool bSave,bool bDryRun,bool bAllowNonSample)
{
    FWork Work(bCompile,bSave,bDryRun);auto& Result=Work.Result;
    if(!IsEditorGameThread()||IsPlaySessionActive()||Running)
    { Work.Fail(TEXT("editor_invalid"),TEXT("An idle, nonreentrant Editor game thread is required"));return Work.Finish(); }
    TGuardValue<bool> Guard(Running,true);
    TSharedPtr<FJsonValue> TargetBlueprintValue,TargetMeshValue,RotationValue;
    if(!Decode(TargetBlueprintNameJson,TargetBlueprintValue)||!Decode(TargetSkeletalMeshJson,TargetMeshValue)||!Decode(RotationJson,RotationValue)
        ||(TargetBlueprintValue->Type!=EJson::Null&&TargetBlueprintValue->Type!=EJson::String)||(TargetMeshValue->Type!=EJson::Null&&TargetMeshValue->Type!=EJson::String))
    { Work.Fail(TEXT("input_json_invalid"),TEXT("Target names require nullable string JSON; rotation requires one bounded JSON value (1Mi UTF16/depth64/65536 slots/no raw NUL)"));return Work.Finish(); }
    if((TargetBlueprintValue->Type==EJson::String&&!TextSafe(TargetBlueprintValue->AsString(),NAME_SIZE-1))
        ||(TargetMeshValue->Type==EJson::String&&!TextSafe(TargetMeshValue->AsString(),NAME_SIZE-1)))
    { Work.Fail(TEXT("target_text_bound"),TEXT("Decoded target name exceeds FName/NUL safety bound"));return Work.Finish(); }
    // Reused EC has a narrower input text budget; reject before creating/deleting targets.
    if(RotationJson.Len()>131072) { Work.Fail(TEXT("rotation_bound"),TEXT("Reused EC ModifyBone rotation carrier exceeds131072 UTF16 units"));return Work.Finish(); }
    for(const FString* Text:{&SourceBlueprintName,&SourceSkeletalMesh,&VariantName,&TargetRoot,&BoneName,&GraphName,&GraphId,&GraphType})
    { if(!TextSafe(*Text,NAME_SIZE-1)) { Work.Fail(TEXT("input_text_bound"),TEXT("Ordinary input exceeds FName/NUL safety bound"));return Work.Finish(); } }
    if(SourceSkeletalMesh.TrimStartAndEnd().IsEmpty()) { Work.Fail(TEXT("source_missing"),TEXT("Missing 'source_skeletal_mesh' parameter"));return Work.Finish(); }
    FString Root=NormalizePackage(TargetRoot.IsEmpty()?TEXT("/Game/_MCP_Sample/AnimStudy"):TargetRoot);if(Root.IsEmpty()) { Root=TEXT("/Game/_MCP_Sample/AnimStudy"); }
    const FString Segment=VariantSegment(VariantName);
    const FString BlueprintPath=NormalizePackage(TargetBlueprintValue->Type==EJson::Null?FString::Printf(TEXT("%s/ABP_PostProcess_Demo_%s"),*Root,*Segment):TargetBlueprintValue->AsString());
    const FString MeshPath=NormalizePackage(TargetMeshValue->Type==EJson::Null?FString::Printf(TEXT("%s/SKM_PostProcess_Demo_%s"),*Root,*Segment):TargetMeshValue->AsString());
    Result.Stage=TEXT("validate_targets");
    if(!ValidGamePackage(Root)||!ValidGamePackage(BlueprintPath)||!ValidGamePackage(MeshPath))
    { Work.Fail(TEXT("target_path_invalid"),TEXT("Invalid target_root, target_blueprint_name or target_skeletal_mesh; bounded valid /Game package paths required"));return Work.Finish(); }
    if(BlueprintPath.Equals(MeshPath,ESearchCase::IgnoreCase)) { Work.Fail(TEXT("target_alias_refused"),TEXT("Blueprint and SkeletalMesh targets must be different packages"));return Work.Finish(); }
    if(!bAllowNonSample&&(!SamplePath(BlueprintPath)||!SamplePath(MeshPath)))
    { Work.Fail(TEXT("target_scope_refused"),TEXT("Refusing to create Post Process demo targets outside /Game/_MCP_Sample; intentional isolated non-sample targets require allow_non_sample=true"));return Work.Finish(); }
    Work.Legacy->SetStringField(TEXT("variant_name"),Segment);Work.Legacy->SetStringField(TEXT("target_root"),Root);Work.Legacy->SetStringField(TEXT("target_blueprint"),BlueprintPath);Work.Legacy->SetStringField(TEXT("target_skeletal_mesh"),MeshPath);
    Work.Legacy->SetBoolField(TEXT("overwrite_existing"),bOverwriteExisting);Work.Legacy->SetBoolField(TEXT("replace_existing"),bReplaceExisting);
    Result.Stage=TEXT("load_sources");UBlueprint* SourceBlueprint=FindBlueprint(SourceBlueprintName,Work);
    if(!Live(SourceBlueprint)||!Live(SourceBlueprint->GetClass())||!Live(SourceBlueprint->GetPackage())||!Cast<UAnimBlueprint>(SourceBlueprint))
    { Work.Fail(TEXT("source_blueprint_invalid"),TEXT("Source Anim Blueprint not found or is not an Anim Blueprint: ")+SourceBlueprintName);return Work.Finish(); }
    Work.Sources.Add(MakeUnique<FIdentity>(SourceBlueprint,true));
    const FString SourceMeshPackage=NormalizePackage(SourceSkeletalMesh);UObject* MeshObject=AssetLoad(SourceMeshPackage,Work);
    if(!MeshObject)
    {
        const FString Path=ObjectPath(SourceSkeletalMesh,false);if(TextSafe(Path,NAME_SIZE-1)) { Work.SDK(TEXT("source_mesh_static_load_calls"));MeshObject=LoadObject<UObject>(nullptr,*Path); }
    }
    auto* SourceMesh=Live(MeshObject)&&Live(MeshObject->GetClass())?Cast<USkeletalMesh>(MeshObject):nullptr;
    if(!Work.Current()||!Live(SourceMesh)||!Live(SourceMesh->GetPackage()))
    { Work.Fail(TEXT("source_mesh_invalid"),TEXT("Source SkeletalMesh not found: ")+SourceSkeletalMesh);return Work.Finish(); }
    Work.Sources.Add(MakeUnique<FIdentity>(SourceMesh,true));
    for(const auto& Source:Work.Sources)
    { if(Source->PackageName.Equals(BlueprintPath,ESearchCase::IgnoreCase)||Source->PackageName.Equals(MeshPath,ESearchCase::IgnoreCase))
      { Work.Fail(TEXT("source_target_alias_refused"),TEXT("Either target aliases a protected source package; cross-kind aliases are also refused"));return Work.Finish(); } }
    Work.Legacy->SetStringField(TEXT("source_blueprint"),SourceBlueprint->GetPathName());Work.Legacy->SetStringField(TEXT("source_skeletal_mesh"),SourceMesh->GetPathName());
    Work.TargetPackages.Add(BlueprintPath);Work.TargetPackages.Add(MeshPath);
    if(!Work.Current()) { return Work.Finish(); }
    if(bDryRun)
    {
        Result.bSuccess=Result.bPostconditionPassed=true;Result.Stage=TEXT("complete");
        Work.Legacy->SetStringField(TEXT("runtime_note"),TEXT("Dry run only: no assets were duplicated, edited, compiled, or saved."));return Work.Finish();
    }
    Result.Stage=TEXT("runtime_dependency_preflight");
    if(!RuntimePackageGuard(SourceBlueprint,Work,TEXT("source_blueprint_runtime_projection"))||!RuntimePackageGuard(SourceMesh,Work,TEXT("source_mesh_runtime_projection"))
        ||!SavedDependencyGuard({SourceBlueprint->GetPackage()->GetName(),SourceMesh->GetPackage()->GetName()},Work)) { return Work.Finish(); }
    Result.Stage=TEXT("inspect_targets");FTargetPlan BlueprintPlan(BlueprintPath,TEXT("Anim Blueprint"),UAnimBlueprint::StaticClass()),MeshPlan(MeshPath,TEXT("SkeletalMesh"),USkeletalMesh::StaticClass());
    if(!InspectTarget(BlueprintPlan,Work)||!InspectTarget(MeshPlan,Work)) { return Work.Finish(); }
    if(bOverwriteExisting&&(!OverwriteReferencesSafe(BlueprintPlan,Work)||!OverwriteReferencesSafe(MeshPlan,Work))) { return Work.Finish(); }
    // Both target identities are admitted before any overwrite/duplicate can run.
    Result.Stage=TEXT("prepare_target_blueprint");auto* TargetBlueprint=Cast<UAnimBlueprint>(PrepareTarget(SourceBlueprint,BlueprintPlan,bOverwriteExisting,Work));
    if(TargetBlueprint) { Work.TargetBlueprint=Work.AddTarget(TargetBlueprint,BlueprintPath); }
    Work.Legacy->SetBoolField(TEXT("target_blueprint_created"),BlueprintPlan.Receipt->GetBoolField(TEXT("created")));
    Work.Legacy->SetBoolField(TEXT("target_blueprint_reused"),BlueprintPlan.Receipt->GetBoolField(TEXT("reused")));
    // Identity/scope failures stop before a later stage can overwrite the first failure.
    // Ordinary preparation errors in Work.Errors still preserve the original second-target attempt.
    if(!Result.Error.IsEmpty()) { return Work.Finish(); }
    Result.Stage=TEXT("prepare_target_skeletal_mesh");auto* TargetMesh=Cast<USkeletalMesh>(PrepareTarget(SourceMesh,MeshPlan,bOverwriteExisting,Work));
    if(TargetMesh) { Work.TargetMesh=Work.AddTarget(TargetMesh,MeshPath); }
    for(const auto& Pair:{TPair<const TCHAR*,FTargetPlan*>(TEXT("target_blueprint"),&BlueprintPlan),TPair<const TCHAR*,FTargetPlan*>(TEXT("target_skeletal_mesh"),&MeshPlan)})
    {
        Work.Legacy->SetBoolField(FString(Pair.Key)+TEXT("_created"),Pair.Value->Receipt->GetBoolField(TEXT("created")));
        Work.Legacy->SetBoolField(FString(Pair.Key)+TEXT("_reused"),Pair.Value->Receipt->GetBoolField(TEXT("reused")));
    }
    if(!TargetBlueprint||!TargetMesh||!Work.TargetBlueprint||!Work.TargetMesh||!Work.Errors.IsEmpty()||!Result.Error.IsEmpty())
    { Work.Fail(TEXT("prepare_targets_failed"),TEXT("Failed to prepare Post Process demo target assets"));return Work.Finish(); }
    if(!RuntimePackageGuard(TargetBlueprint,Work,TEXT("target_blueprint_runtime_projection_before_graph"))||!RuntimePackageGuard(TargetMesh,Work,TEXT("target_mesh_runtime_projection_before_assignment"))) { return Work.Finish(); }
    Result.Stage=TEXT("configure_anim_graph");Work.Count(TEXT("EC_ensure_modify_bone_calls"));
    FResult GraphResult=AnimGraphBoneDemo::EnsureModifyBoneDemo(BlueprintPath,BoneName,RotationJson,GraphName,GraphId,GraphType,bReplaceExisting,bAllowNonSample);
    Work.Count(TEXT("EC_ensure_modify_bone_returns"));Result.bMutationAttempted|=GraphResult.bMutationAttempted;Result.bChanged|=GraphResult.bChanged;
    Result.Data->SetObjectField(TEXT("graph_native_outcome"),GraphOutcome(GraphResult));
    Result.Data->SetObjectField(TEXT("graph_native_data"),GraphResult.Data);Result.Data->SetStringField(TEXT("graph_native_stage"),GraphResult.Stage);Result.Data->SetStringField(TEXT("graph_native_error"),GraphResult.Error);
    auto GraphLegacy=GraphResult.Data->TryGetField(TEXT("legacy_result"));
    if(GraphLegacy.IsValid()&&GraphLegacy->Type==EJson::Object) { Work.Legacy->SetObjectField(TEXT("graph_result"),GraphLegacy->AsObject()); }
    else { Work.Legacy->SetObjectField(TEXT("graph_result"),GraphResult.Data); }
    if(!Work.Current()||!GraphResult.bSuccess||!GraphResult.bPostconditionPassed)
    { Work.Fail(TEXT("graph_failed"),TEXT("Failed to configure target Post Process AnimGraph"));return Work.Finish(); }
    Result.Stage=TEXT("compile_target_blueprint");
    const bool Forced=!bCompile&&!TargetBlueprint->GeneratedClass;Work.Compile->SetBoolField(TEXT("forced_for_generated_class"),Forced);
    if(bCompile||!TargetBlueprint->GeneratedClass)
    {
        Result.bCompileRequested=true;Work.Compile->SetBoolField(TEXT("requested"),true);
        if(!Work.Current()) { return Work.Finish(); }
        Result.bCompileAttempted=true;Work.Compile->SetBoolField(TEXT("attempted"),true);
        FCompilerResultsLog Log;Work.Writer(TEXT("compile_blueprint_calls"));FKismetEditorUtilities::CompileBlueprint(TargetBlueprint,EBlueprintCompileOptions::None,&Log);Work.Count(TEXT("compile_blueprint_returns"));
        if(!Work.Current()) { return Work.Finish(); }
        const bool LegacySuccess=TargetBlueprint->GeneratedClass!=nullptr;
        const bool Healthy=Live(TargetBlueprint->GeneratedClass)&&Log.NumErrors==0&&(TargetBlueprint->Status==BS_UpToDate||TargetBlueprint->Status==BS_UpToDateWithWarnings);
        Result.Compiled=Healthy;Work.Compile->SetBoolField(TEXT("success"),LegacySuccess);Work.Compile->SetBoolField(TEXT("compiler_status_verified"),Healthy);
        Work.Compile->SetNumberField(TEXT("error_count"),Log.NumErrors);Work.Compile->SetNumberField(TEXT("warning_count"),Log.NumWarnings);Work.Compile->SetNumberField(TEXT("blueprint_status"),static_cast<int32>(TargetBlueprint->Status));
        if(!Healthy) { Work.Fail(TEXT("compile_failed"),TEXT("Target compile reported errors, stale status or missing GeneratedClass; old GeneratedClass alone is not completion evidence"));return Work.Finish(); }
    }
    else { Work.Compile->SetBoolField(TEXT("success"),Live(TargetBlueprint->GeneratedClass));Work.Compile->SetBoolField(TEXT("compiler_status_verified"),false); }
    Work.Compile->SetStringField(TEXT("blueprint"),TargetBlueprint->GetPathName());
    UClass* TargetClass=TargetBlueprint->GeneratedClass;TStrongObjectPtr<UClass> HeldTargetClass(TargetClass);
    if(!Live(TargetClass)||!TargetClass->IsChildOf(UAnimInstance::StaticClass())||TargetClass->HasAnyClassFlags(CLASS_Abstract|CLASS_NewerVersionExists)
        ||!ClassAllowed(TargetClass,Work)||TargetClass->ClassGeneratedBy!=TargetBlueprint)
    { Work.Fail(TEXT("generated_class_invalid"),TEXT("Target Anim Blueprint generated class is missing, stale, abstract, foreign or not an AnimInstance class"));return Work.Finish(); }
    Work.Compile->SetStringField(TEXT("generated_class"),TargetClass->GetPathName());
    Work.AssignedClass=MakeUnique<FIdentity>(TargetClass);Work.ClassOwner=TargetBlueprint;
    if(!RuntimePackageGuard(TargetBlueprint,Work,TEXT("target_blueprint_runtime_projection_after_compile"))) { return Work.Finish(); }
    Result.Stage=TEXT("assign_post_process_class");
    if(!MeshReady(TargetMesh,Work)) { return Work.Finish(); }
    Work.SDK(TEXT("post_process_class_get_calls"));UClass* Previous=TargetMesh->GetPostProcessAnimBlueprint().Get();TStrongObjectPtr<UClass> HeldPrevious(Previous);
    if(!Work.Current()||(Previous&&!Live(Previous))||!Live(TargetClass)||TargetBlueprint->GeneratedClass!=TargetClass)
    { Work.Fail(TEXT("assignment_identity_changed"),TEXT("PostProcess getter callback changed the admitted class or target identity"));return Work.Finish(); }
    const bool Changed=Previous!=TargetClass;Work.Legacy->SetBoolField(TEXT("post_process_anim_blueprint_changed"),Changed);
    Work.Legacy->SetStringField(TEXT("previous_post_process_anim_blueprint_class"),Previous?Previous->GetPathName():FString());Work.Legacy->SetStringField(TEXT("post_process_anim_blueprint_class"),TargetClass->GetPathName());
    if(Changed)
    {
        Work.Writer(TEXT("mesh_modify_calls"));TargetMesh->Modify();Work.Count(TEXT("mesh_modify_returns"));
        if(!MeshReady(TargetMesh,Work)||!Live(TargetClass)||TargetBlueprint->GeneratedClass!=TargetClass)
        { Work.Fail(TEXT("assignment_identity_changed"),TEXT("Modify callback changed target/class or started mesh compilation before assignment"));return Work.Finish(); }
        Work.Writer(TEXT("set_post_process_class_calls"));TargetMesh->SetPostProcessAnimBlueprint(TSubclassOf<UAnimInstance>(TargetClass));Work.Count(TEXT("set_post_process_class_returns"));Result.bChanged=true;
        if(!Work.Current()) { return Work.Finish(); }
        Work.Writer(TEXT("mesh_mark_dirty_calls"));TargetMesh->MarkPackageDirty();if(!Work.Current()) { return Work.Finish(); }
        Work.Writer(TEXT("mesh_package_mark_dirty_calls"));CastChecked<UPackage>(Work.TargetMesh->Package.Get())->MarkPackageDirty();if(!Work.Current()) { return Work.Finish(); }
        Work.Writer(TEXT("mesh_post_edit_change_calls"));TargetMesh->PostEditChange();Work.Count(TEXT("mesh_post_edit_change_returns"));if(!Work.Current()) { return Work.Finish(); }
    }
    Result.Stage=TEXT("save_targets");
    if(bSave)
    {
        const bool BlueprintSaved=SaveOne(*Work.TargetBlueprint,TEXT("blueprint_saved"),Work);
        // Preserve ordered two-save attempts when the first SDK save returns false but identities remain safe.
        const bool MeshSaved=Work.Current()?SaveOne(*Work.TargetMesh,TEXT("skeletal_mesh_saved"),Work):false;
        Result.bSaved=BlueprintSaved&&MeshSaved;
        if(!Result.bSaved) { Work.Fail(TEXT("save_failed"),TEXT("Failed to save one or more Post Process demo target assets"));return Work.Finish(); }
    }
    Result.Stage=TEXT("verify_assignment");
    if(!MeshReady(TargetMesh,Work)||!Live(TargetClass)||TargetBlueprint->GeneratedClass!=TargetClass)
    { Work.Fail(TEXT("assignment_readback_pending"),TEXT("Current class changed or mesh async compilation prevents safe assignment readback; preserve partial results"));return Work.Finish(); }
    Work.SDK(TEXT("post_process_class_get_calls"));UClass* Readback=TargetMesh->GetPostProcessAnimBlueprint().Get();
    if(!Work.Current()||Readback!=TargetClass||TargetBlueprint->GeneratedClass!=TargetClass||!Live(TargetClass))
    { Work.Fail(TEXT("assignment_readback_failed"),TEXT("PostProcess class readback differs from the admitted generated class"));return Work.Finish(); }
    Work.Legacy->SetStringField(TEXT("target_blueprint_object_path"),TargetBlueprint->GetPathName());Work.Legacy->SetStringField(TEXT("target_skeletal_mesh_object_path"),TargetMesh->GetPathName());
    Work.Legacy->SetStringField(TEXT("runtime_note"),TEXT("The target SkeletalMesh points to the generated Post Process AnimBlueprint class. Runtime pre/post pose proof requires a separate component or sampling flow."));
    Result.Data->SetBoolField(TEXT("assignment_readback_verified"),true);Result.bSuccess=Result.bPostconditionPassed=true;Result.Stage=TEXT("complete");return Work.Finish();
}
}
