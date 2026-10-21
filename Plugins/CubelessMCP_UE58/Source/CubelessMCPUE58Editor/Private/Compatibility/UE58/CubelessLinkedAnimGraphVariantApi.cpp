#include "Compatibility/UE58/CubelessLinkedAnimGraphVariantApi.h"
#include "Compatibility/UE58/CubelessAnimGraphInspectionApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "AnimGraphNode_LinkedAnimGraph.h"
#include "AnimGraphNode_LinkedInputPose.h"
#include "AnimGraphNode_Root.h"
#include "AnimationGraphSchema.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Blueprint/BlueprintExtension.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PluginDescriptor.h"
#include "Serialization/JsonSerializer.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "UObject/ReferencerFinder.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"
namespace Cubeless::UE58::LinkedAnimGraphVariant
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
    for(const TCHAR* Prefix:{TEXT("/Game/_MCP_Sample"),TEXT("/Game/_MCP_Temp"),TEXT("/Game/CubelessMCPTests")})
    { if(Path.Equals(Prefix,ESearchCase::IgnoreCase)||Path.StartsWith(FString(Prefix)+TEXT("/"),ESearchCase::IgnoreCase)) { return true; } }return false;
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
    FResult Result;
    TSharedRef<FJsonObject> Legacy=MakeShared<FJsonObject>(),Calls=MakeShared<FJsonObject>(),Compile=MakeShared<FJsonObject>(),Save=MakeShared<FJsonObject>();
    TArray<TUniquePtr<FIdentity>> Sources,Targets;TArray<TSharedPtr<FJsonValue>> Preparation,Errors,Links;
    TUniquePtr<FIdentity> AssignedClass,CommonSkeleton;UAnimBlueprint* ClassOwner=nullptr;TSet<FString> TargetPackages;bool TargetSkeletonLocked=false;
    FIdentity* Target=nullptr;bool GraphChanged=false,Created=false,Reused=false;
    explicit FWork(bool CompileRequested,bool SaveRequested,bool Dry)
    {
        Result.bCompileRequested=CompileRequested;Result.bSaveRequested=SaveRequested;
        Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("command"),TEXT("ensure_linked_anim_graph_sample_variant"));
        Legacy->SetBoolField(TEXT("sample_only"),true);Legacy->SetBoolField(TEXT("dry_run"),Dry);
        for(const TCHAR* Key:{TEXT("source_modified"),TEXT("source_asset_modified"),TEXT("linked_source_modified"),TEXT("linked_source_asset_modified")}) { Legacy->SetBoolField(Key,false); }
        Legacy->SetBoolField(TEXT("compile_requested"),CompileRequested);Legacy->SetBoolField(TEXT("save_requested"),SaveRequested);
        Compile->SetBoolField(TEXT("requested"),CompileRequested);Compile->SetBoolField(TEXT("attempted"),false);
        Save->SetBoolField(TEXT("requested"),SaveRequested);Save->SetBoolField(TEXT("target_saved"),false);
        Legacy->SetObjectField(TEXT("compile_result"),Compile);Legacy->SetArrayField(TEXT("created_links"),Links);
        Result.Data->SetObjectField(TEXT("legacy_result"),Legacy);Result.Data->SetObjectField(TEXT("native_call_counts"),Calls);
        Result.Data->SetObjectField(TEXT("compile_receipt"),Compile);Result.Data->SetObjectField(TEXT("save_receipt"),Save);
        for(const TCHAR* Key:{TEXT("asset_delete_calls"),TEXT("asset_delete_returns"),TEXT("asset_duplicate_calls"),TEXT("asset_duplicate_returns"),TEXT("asset_save_calls"),TEXT("asset_save_returns"),TEXT("compile_blueprint_calls"),TEXT("compile_blueprint_returns"),TEXT("node_create_calls"),TEXT("node_create_returns"),TEXT("reconstruct_calls"),TEXT("reconstruct_returns"),TEXT("instance_class_writes"),TEXT("try_create_connection_calls"),TEXT("break_all_pin_links_calls"),TEXT("tick_calls"),TEXT("start_pie_calls"),TEXT("stop_pie_calls")}) { Calls->SetNumberField(Key,0); }
        Result.Data->SetBoolField(TEXT("atomic_rollback_available"),false);Result.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);
        Result.Data->SetBoolField(TEXT("disk_reload_verified"),false);Result.Data->SetBoolField(TEXT("runtime_pose_verified"),false);Result.Data->SetBoolField(TEXT("packaging_verified"),false);
        Result.Data->SetBoolField(TEXT("duplicate_implicit_skeleton_compile_possible"),true);Result.Data->SetBoolField(TEXT("duplicate_source_control_implicit_save_possible"),true);
        Result.Data->SetBoolField(TEXT("implicit_compile_or_save_completion_measured"),false);
        Result.Data->SetStringField(TEXT("source_preservation_scope"),TEXT("No explicit source writes; source object/class/outer/actual package identity, captured dirty state and disk metadata only. Not full RAM or byte immutability."));
        Result.Data->SetStringField(TEXT("total_change_scope"),TEXT("Total SDK load/registry/reconstruction/duplicate/compiler/save effects remain unknown after the first acquisition. Explicit observed changes are reported separately."));
        Result.Data->SetStringField(TEXT("synchronous_SDK_limit"),TEXT("Public duplicate/reconstruction/schema/structural compiler/save calls are synchronous and cannot be preempted by this wrapper."));
    }
    void Count(const TCHAR* Key) { double Value=0;Calls->TryGetNumberField(Key,Value);Calls->SetNumberField(Key,Value+1); }
    void SDK(const TCHAR* Key) { Result.bChangeStateKnown=false;Count(Key); }
    void Writer(const TCHAR* Key) { Result.bMutationAttempted=true;SDK(Key); }
    bool Fail(const TCHAR* Code,const FString& Message)
    {
        if(Result.Error.IsEmpty()) { Result.ErrorCode=FString(TEXT("linked_anim_graph_variant_"))+Code;Result.Error=Message; }
        Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("error"),Result.Error);return false;
    }
    bool Current(bool CheckTargets=true)
    {
        if(!Result.Error.IsEmpty()) { return false; }
        if(!IsEditorGameThread()||IsPlaySessionActive()) { return Fail(TEXT("editor_changed"),TEXT("Idle Editor game-thread admission changed after callback")); }
        for(const auto& Source:Sources) { if(!Source->Preserved()) { return Fail(TEXT("source_observation_changed"),TEXT("Protected source identity/dirty/file metadata changed; no further writer is dispatched")); } }
        if(CommonSkeleton)
        {
            if(!CommonSkeleton->Current()) { return Fail(TEXT("skeleton_identity_changed"),TEXT("Captured common skeleton identity changed after callback")); }
            for(const auto& Source:Sources) { if(CastChecked<UAnimBlueprint>(Source->Object.Get())->TargetSkeleton!=CommonSkeleton->Object.Get()) { return Fail(TEXT("source_skeleton_changed"),TEXT("Source TargetSkeleton binding changed after callback")); } }
        }
        if(CheckTargets) { for(const auto& Id:Targets) { if(!Id->Current()) { return Fail(TEXT("target_identity_changed"),TEXT("Target actual package/outer/class/path changed after callback")); } } }
        if(TargetSkeletonLocked&&Target&&CastChecked<UAnimBlueprint>(Target->Object.Get())->TargetSkeleton!=CommonSkeleton->Object.Get()) { return Fail(TEXT("target_skeleton_changed"),TEXT("Target skeleton binding changed after callback")); }
        if(AssignedClass&&(!AssignedClass->Current()||!Live(ClassOwner)||ClassOwner->GeneratedClass!=AssignedClass->Object.Get()
            ||CastChecked<UClass>(AssignedClass->Object.Get())->ClassGeneratedBy!=ClassOwner||CastChecked<UClass>(AssignedClass->Object.Get())->HasAnyClassFlags(CLASS_NewerVersionExists)))
        { return Fail(TEXT("linked_generated_class_changed"),TEXT("Linked GeneratedClass or its Blueprint owner changed after callback")); }
        return true;
    }
    bool IsSource(UObject* Object) const
    {
        if(!Live(Object)||!Live(Object->GetPackage())) { return true; }
        for(const auto& Source:Sources) { if(Object==Source->Object.Get()||Object->GetPackage()==Source->Package.Get()) { return true; } }return false;
    }
    FIdentity* AddTarget(UObject* Object,const FString& PackageName)
    {
        if(!Live(Object)||!Live(Object->GetClass())||!Live(Object->GetPackage())||IsSource(Object)||!Object->GetPackage()->GetName().Equals(PackageName,ESearchCase::IgnoreCase))
        { Fail(TEXT("target_scope_changed"),TEXT("Target aliases a source, is unreadable, or is outside its declared package"));return nullptr; }
        auto Id=MakeUnique<FIdentity>(Object);FIdentity* Ptr=Id.Get();Targets.Add(MoveTemp(Id));return Ptr;
    }
    FResult Finish()
    {
        if(!Result.bSuccess&&Result.Error.IsEmpty()) { Fail(TEXT("operation_incomplete"),TEXT("Operation stopped before postconditions; inspect actual partial prefix")); }
        bool Preserved=true;TArray<TSharedPtr<FJsonValue>> Observations;
        for(int32 Index=0;Index<Sources.Num();++Index)
        {
            const auto& Id=Sources[Index];Preserved&=Id->Preserved();Observations.Add(MakeShared<FJsonValueObject>(Id->Observation()));
            const TCHAR* Key=Index==0?TEXT("source_dirty_after"):TEXT("linked_source_dirty_after");
            if(Live(Id->Package.Get())) { Legacy->SetBoolField(Key,CastChecked<UPackage>(Id->Package.Get())->IsDirty()); }else { Legacy->SetField(Key,MakeShared<FJsonValueNull>()); }
        }
        if(!Preserved) { Result.bSuccess=false;Result.bPostconditionPassed=false;Fail(TEXT("source_observation_changed"),TEXT("Captured source preservation observation failed")); }
        Result.Data->SetArrayField(TEXT("source_observations"),Observations);Result.Data->SetBoolField(TEXT("captured_source_observations_preserved"),Preserved);
        Result.Data->SetBoolField(TEXT("complete_original_asset_immutability_proof"),false);Result.Data->SetBoolField(TEXT("observed_explicit_change"),Result.bChanged);
        Result.Data->SetArrayField(TEXT("preparation"),Preparation);Legacy->SetArrayField(TEXT("created_links"),Links);Legacy->SetArrayField(TEXT("errors"),Errors);
        Legacy->SetBoolField(TEXT("target_created"),Created);Legacy->SetBoolField(TEXT("target_reused"),Reused);
        Legacy->SetBoolField(TEXT("target_modified"),Result.bSuccess?GraphChanged:(Created||GraphChanged));Legacy->SetBoolField(TEXT("target_asset_saved"),Result.bSaved);
        if(Target)
        {
            const bool Readable=Target->Current();Legacy->SetStringField(TEXT("target_asset_path"),Target->PackageName);
            Legacy->SetBoolField(TEXT("partial_target_modified"),Created||GraphChanged);Legacy->SetBoolField(TEXT("target_asset_modified"),Created||GraphChanged);
            if(Readable) { Legacy->SetBoolField(TEXT("target_dirty_after"),CastChecked<UPackage>(Target->Package.Get())->IsDirty()); }else { Legacy->SetField(TEXT("target_dirty_after"),MakeShared<FJsonValueNull>()); }
        }
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
    Work.SDK(TEXT("blueprint_load_calls"));UBlueprint* Blueprint=LoadObject<UBlueprint>(nullptr,*Path);if(!Work.Current()) { return nullptr; }if(Blueprint) { return Blueprint; }
    FString ClassPath=Path;if(!ClassPath.EndsWith(TEXT("_C"))) { ClassPath+=TEXT("_C"); }
    if(!TextSafe(ClassPath,NAME_SIZE-1)) { return nullptr; }
    Work.SDK(TEXT("blueprint_class_load_calls"));UClass* Class=LoadObject<UClass>(nullptr,*ClassPath);TStrongObjectPtr<UClass> HeldClass(Class);if(!Work.Current()) { return nullptr; }
    if(Class) { return Live(Class)?Cast<UBlueprint>(Class->ClassGeneratedBy):nullptr; }
    return nullptr;
}
UBlueprint* FindBlueprint(const FString& Name,FWork& Work)
{
    const FString Path=ObjectPath(Name,true);if(auto* Direct=LoadBlueprintPath(Path,Work)) { return Direct; }if(!Work.Current()) { return nullptr; }
    // Preserve original CommonUtils level lookup, although the caller then rejects non-AnimBlueprints.
    if(TextSafe(Path,NAME_SIZE-1)&&!Path.IsEmpty())
    {
        Work.SDK(TEXT("world_find_or_load_calls"));auto* World=FindObject<UWorld>(nullptr,*Path);if(!World) { World=LoadObject<UWorld>(nullptr,*Path); }
        TStrongObjectPtr<UWorld> HeldWorld(World);if(!Work.Current()) { return nullptr; }
        if(Live(World)&&Live(World->PersistentLevel))
        { auto* LevelBlueprint=World->PersistentLevel->GetLevelScriptBlueprint(true);if(!Work.Current()) { return nullptr; }if(LevelBlueprint) { return LevelBlueprint; }
          if(auto* ExistingLevelBlueprint=FindObject<ULevelScriptBlueprint>(World->PersistentLevel,*World->GetName())) { return ExistingLevelBlueprint; } }
    }
    if(!Name.Contains(TEXT("/"))&&!Name.Contains(TEXT(".")))
    { const FString Legacy=FString::Printf(TEXT("/Game/Blueprints/%s.%s"),*Name,*Name);if(auto* Blueprint=LoadBlueprintPath(Legacy,Work)) { return Blueprint; } }
    Work.SDK(TEXT("blueprint_registry_acquisition"));auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    TArray<FAssetData> Assets;Work.Count(TEXT("blueprint_registry_queries"));Registry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(),Assets,true);if(!Work.Current()) { return nullptr; }
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
    { if(!AllowedPackage(Referencer.ToString())) { Plan.Receipt->SetStringField(TEXT("protected_saved_referencer"),Referencer.ToString());return Work.Fail(TEXT("overwrite_external_reference"),TEXT("Force deletion could alter a package outside the requested target")); } }
    TArray<UObject*> Referencees;GetObjectsWithPackage(CastChecked<UPackage>(Plan.Existing->Package.Get()),Referencees,EGetObjectsFlags::IncludeNestedObjects,RF_NoFlags,EInternalObjectFlags::Garbage);
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
        { Plan.Receipt->SetStringField(TEXT("protected_memory_referencer"),Referencer->GetPathName());return Work.Fail(TEXT("overwrite_external_reference"),TEXT("A loaded package outside the requested target refers to the asset being overwritten")); }
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
bool Graphs(UAnimBlueprint* Blueprint,TArray<UEdGraph*>& Out,FWork& Work)
{
    if(!Work.Current()||!Live(Blueprint)) { return false; }
    FIdentity Owner(Blueprint);TArray<TUniquePtr<FIdentity>> HeldGraphs;TSet<UEdGraph*> Path;int32 Visits=0;
    const auto Extensions=Blueprint->GetExtensions();if(Extensions.Num()>4096) { return Work.Fail(TEXT("graph_bound"),TEXT("Blueprint extension count exceeds4096")); }
    for(const UBlueprintExtension* Extension:Extensions)
    {
        if(!Extension) { continue; }if(!Live(Extension)) { return Work.Fail(TEXT("graph_extension_unreadable"),TEXT("Unloaded Blueprint extension before SDK graph enumeration")); }
        TSet<UClass*> Seen;
        for(UClass* Type=Extension->GetClass();Type&&Type!=UBlueprintExtension::StaticClass();Type=Type->GetSuperClass())
        {
            if(!Live(Type)||Seen.Contains(Type)||Seen.Num()>=64||!Type->IsNative()||!Live(Type->GetPackage())) { return Work.Fail(TEXT("graph_extension_unsupported"),TEXT("Unaudited Blueprint extension graph producer")); }
            Seen.Add(Type);const FString Package=Type->GetPackage()->GetName();
            if(Package!=TEXT("/Script/Engine")&&Package!=TEXT("/Script/AnimGraph")&&Package!=TEXT("/Script/UMGEditor")) { return Work.Fail(TEXT("graph_extension_unsupported"),TEXT("Blueprint extension is outside the audited native no-op graph producer modules")); }
        }
    }
    TFunction<bool(UEdGraph*,int32)> Visit=[&](UEdGraph* Graph,int32 Depth)
    {
        if(!Live(Graph)||!Live(Graph->GetPackage())||!Live(Graph->GetClass())||Depth>32||++Visits>4096||Path.Contains(Graph)) { return Work.Fail(TEXT("graph_bound"),TEXT("Graph containment is unloaded, cyclic, deeper than32, or exceeds4096 occurrences")); }
        HeldGraphs.Add(MakeUnique<FIdentity>(Graph));Path.Add(Graph);
        if(Graph->SubGraphs.Num()>4096) { return Work.Fail(TEXT("graph_bound"),TEXT("Graph child slots exceed4096")); }
        for(UEdGraph* Child:Graph->SubGraphs) { if(!Visit(Child,Depth+1)) { return false; } }Path.Remove(Graph);return true;
    };
    auto Roots=[&](const TArray<TObjectPtr<UEdGraph>>& Array)
    { if(Array.Num()>4096) { return Work.Fail(TEXT("graph_bound"),TEXT("Graph root slots exceed4096")); }for(UEdGraph* Graph:Array) { if(Graph&&!Visit(Graph,0)) { return false; } }return true; };
    if(!Roots(Blueprint->FunctionGraphs)||!Roots(Blueprint->MacroGraphs)||!Roots(Blueprint->UbergraphPages)||!Roots(Blueprint->DelegateSignatureGraphs)) { return false; }
    if(Blueprint->ImplementedInterfaces.Num()>4096) { return Work.Fail(TEXT("graph_bound"),TEXT("Implemented interface count exceeds4096")); }
    for(const auto& Entry:Blueprint->ImplementedInterfaces) { if(!Roots(Entry.Graphs)) { return false; } }
    Work.SDK(TEXT("get_all_graphs_calls"));Blueprint->GetAllGraphs(Out);
    if(!Work.Current()||!Owner.Current()||Out.Num()>4096) { return Work.Fail(TEXT("graph_changed"),TEXT("Blueprint changed or graph enumeration exceeded4096 after SDK callback")); }
    for(const auto& Id:HeldGraphs) { if(!Id->Current()) { return Work.Fail(TEXT("graph_changed"),TEXT("Graph actual package/owner/class changed during SDK enumeration")); } }
    for(UEdGraph* Graph:Out) { if(!Live(Graph)||!Live(Graph->GetPackage())) { return Work.Fail(TEXT("graph_unreadable"),TEXT("SDK returned an unreadable graph")); } }return true;
}
// Preflight the class graph used by the SDK's protected HasInstanceLoop recursion without calling that protected method.
bool LinkedClosure(UAnimBlueprint* Root,UAnimBlueprint* ProposedLinked,FWork& Work)
{
    if(!Live(Root)) { return Work.Fail(TEXT("linked_closure_unreadable"),TEXT("Linked class owner is unavailable")); }
    TArray<UAnimBlueprint*> Queue={Root};TMap<UAnimBlueprint*,TArray<UAnimBlueprint*>> Edges;TSet<UAnimBlueprint*> Seen;
    TArray<TUniquePtr<FIdentity>> Ids;int32 NodeSlots=0;
    for(int32 Index=0;Index<Queue.Num();++Index)
    {
        if(Queue.Num()>256) { return Work.Fail(TEXT("linked_closure_bound"),TEXT("Linked instance graph exceeds256 Blueprint owners")); }
        UAnimBlueprint* Blueprint=Queue[Index];if(Seen.Contains(Blueprint)) { continue; }Seen.Add(Blueprint);
        if(!Live(Blueprint)||!ObjectAllowed(Blueprint,Work)) { return false; }Ids.Add(MakeUnique<FIdentity>(Blueprint));
        TArray<UEdGraph*> Listed;if(!Graphs(Blueprint,Listed,Work)) { return false; }
        TArray<UAnimBlueprint*>& Next=Edges.FindOrAdd(Blueprint);
        if(Blueprint==Root&&ProposedLinked) { Next.Add(ProposedLinked);Queue.AddUnique(ProposedLinked); }
        for(UEdGraph* Graph:Listed)
        {
            Ids.Add(MakeUnique<FIdentity>(Graph));const TArray<TObjectPtr<UEdGraphNode>> Nodes=Graph->Nodes;
            if(Nodes.Num()>4096||NodeSlots>65536-Nodes.Num()) { return Work.Fail(TEXT("linked_closure_bound"),TEXT("Linked graph node slots exceed the65536 total/4096 per graph bound")); }NodeSlots+=Nodes.Num();
            TMap<UEdGraphNode*,FIdentity*> NodeIdentities;
            for(UEdGraphNode* Node:Nodes)
            {
                if(!Node) { continue; }if(!Live(Node)||!Live(Node->GetPackage())||Node->GetGraph()!=Graph) { return Work.Fail(TEXT("linked_closure_identity"),TEXT("Linked node snapshot is not currently loaded and owned")); }
                auto Id=MakeUnique<FIdentity>(Node);NodeIdentities.Add(Node,Id.Get());Ids.Add(MoveTemp(Id));
            }
            for(UEdGraphNode* Node:Nodes)
            {
                if(!Node) { continue; }FIdentity* NodeId=NodeIdentities.FindChecked(Node);
                if(!NodeId->Current()||Node->GetGraph()!=Graph||Graph->Nodes!=Nodes) { return Work.Fail(TEXT("linked_closure_identity"),TEXT("Linked graph node membership is unavailable")); }
                auto* Linked=Cast<UAnimGraphNode_LinkedAnimGraphBase>(Node);if(!Linked) { continue; }
                Work.SDK(TEXT("linked_target_class_queries"));UClass* Class=Linked->GetTargetClass();
                if(!Work.Current()||!NodeId->Current()||!Live(Graph)||Graph->Nodes!=Nodes) { return Work.Fail(TEXT("linked_closure_identity"),TEXT("Linked target getter changed current node slots or identity")); }
                if(!Class) { continue; }if(!Live(Class)||!ClassAllowed(Class,Work)) { return false; }Ids.Add(MakeUnique<FIdentity>(Class));
                UObject* GeneratedBy=Class->ClassGeneratedBy;
                if(GeneratedBy)
                {
                    if(!Live(GeneratedBy)) { return Work.Fail(TEXT("linked_class_owner_unreadable"),TEXT("Linked target ClassGeneratedBy is not loaded")); }
                    auto* Child=Cast<UAnimBlueprint>(GeneratedBy);if(Child) { Next.Add(Child);Queue.AddUnique(Child); }
                    else { return Work.Fail(TEXT("linked_class_owner_invalid"),TEXT("AnimInstance class owner is not an AnimBlueprint")); }
                }
            }
        }
    }
    TMap<UAnimBlueprint*,int32> Color,Depth;
    struct FFrame { UAnimBlueprint* Owner;int32 Next=0,Longest=1; };
    for(UAnimBlueprint* Start:Queue)
    {
        if(Color.FindRef(Start)==2) { continue; }TArray<FFrame> Stack;Stack.Add({Start});Color.Add(Start,1);
        while(!Stack.IsEmpty())
        {
            FFrame& Frame=Stack.Last();const auto& Children=Edges.FindChecked(Frame.Owner);
            if(Frame.Next<Children.Num())
            {
                UAnimBlueprint* Child=Children[Frame.Next++];const int32 ChildColor=Color.FindRef(Child);
                if(ChildColor==1) { return Work.Fail(TEXT("linked_instance_cycle"),TEXT("Linked AnimGraph class graph contains a cycle before reconstruction/compiler recursion")); }
                if(ChildColor==2) { Frame.Longest=FMath::Max(Frame.Longest,1+Depth.FindChecked(Child));if(Frame.Longest>64) { return Work.Fail(TEXT("linked_closure_depth"),TEXT("Linked instance recursion depth exceeds64")); }continue; }
                Color.Add(Child,1);Stack.Add({Child});if(Stack.Num()>64) { return Work.Fail(TEXT("linked_closure_depth"),TEXT("Linked instance recursion depth exceeds64")); }
            }
            else
            {
                const int32 Finished=Frame.Longest;UAnimBlueprint* Owner=Frame.Owner;Depth.Add(Owner,Finished);Color.Add(Owner,2);Stack.Pop();
                if(!Stack.IsEmpty()) { Stack.Last().Longest=FMath::Max(Stack.Last().Longest,1+Finished);if(Stack.Last().Longest>64) { return Work.Fail(TEXT("linked_closure_depth"),TEXT("Linked instance recursion depth exceeds64")); } }
            }
        }
    }
    for(const auto& Id:Ids) { if(!Id->Current()) { return Work.Fail(TEXT("linked_closure_identity"),TEXT("Linked closure captured identity changed during public getter callbacks")); } }
    auto Receipt=MakeShared<FJsonObject>();Receipt->SetNumberField(TEXT("Blueprints"),Seen.Num());Receipt->SetNumberField(TEXT("node_slots"),NodeSlots);Receipt->SetNumberField(TEXT("depth_bound"),64);
    Receipt->SetBoolField(TEXT("protected_SDK_loop_method_called"),false);Work.Result.Data->SetObjectField(TEXT("linked_instance_closure"),Receipt);return Work.Current();
}
FString GraphType(UBlueprint* Blueprint,UEdGraph* Graph)
{
    if(Blueprint->FunctionGraphs.Contains(Graph)) { return TEXT("function"); }if(Blueprint->MacroGraphs.Contains(Graph)) { return TEXT("macro"); }
    if(Blueprint->UbergraphPages.Contains(Graph)||Blueprint->EventGraphs.Contains(Graph)) { return TEXT("event"); }if(Blueprint->DelegateSignatureGraphs.Contains(Graph)) { return TEXT("delegate"); }return TEXT("unknown");
}
FString NormalizeGraphType(FString Value)
{
    Value.ToLowerInline();Value.ReplaceInline(TEXT(" "),TEXT(""));Value.ReplaceInline(TEXT("-"),TEXT("_"));
    if(Value==TEXT("eventgraph")||Value==TEXT("event_graph")||Value==TEXT("ubergraph")||Value==TEXT("uber_graph")) { return TEXT("event"); }
    if(Value==TEXT("functiongraph")||Value==TEXT("function_graph")) { return TEXT("function"); }if(Value==TEXT("macrograph")||Value==TEXT("macro_graph")) { return TEXT("macro"); }return Value;
}
struct FGraphContext
{
    FWork& Work;UAnimBlueprint* Blueprint;UEdGraph* Graph;FIdentity Identity;TArray<TUniquePtr<FIdentity>> Anchors,Members;
    TSet<const UEdGraphPin*> Pins;TStrongObjectPtr<UEdGraphSchema> Schema;TUniquePtr<FIdentity> SchemaIdentity;
    UScriptStruct* Pose=nullptr;UScriptStruct* ComponentPose=nullptr;TStrongObjectPtr<UScriptStruct> HeldPose,HeldComponentPose;
    FGraphContext(FWork& InWork,UAnimBlueprint* InBlueprint,UEdGraph* InGraph):Work(InWork),Blueprint(InBlueprint),Graph(InGraph),Identity(InGraph) {}
    bool Refresh()
    {
        if(!Work.Current()||!Identity.Current()||Graph->GetOuter()!=Blueprint) { return Work.Fail(TEXT("graph_identity_changed"),TEXT("Selected graph owner/actual package/class identity changed")); }
        for(const auto& Id:Anchors) { if(!Id->Current()||!Graph->Nodes.Contains(CastChecked<UEdGraphNode>(Id->Object.Get()))) { return Work.Fail(TEXT("node_identity_changed"),TEXT("Required node was deleted, moved or replaced by callback")); } }
        TArray<UEdGraph*> Listed;if(!Graphs(Blueprint,Listed,Work)||!Listed.Contains(Graph)) { return Work.Fail(TEXT("graph_membership_changed"),TEXT("Selected graph is no longer a Blueprint member")); }
        if(Graph->Nodes.Num()>4096) { return Work.Fail(TEXT("node_bound"),TEXT("Graph node count exceeds4096")); }
        Members.Reset();Pins.Reset();int32 Slots=0;const auto NodeSlots=Graph->Nodes;TMap<UEdGraphNode*,TArray<UEdGraphPin*>> PinSlots;
        for(UEdGraphNode* Node:Graph->Nodes)
        {
            if(!Node) { continue; }if(!Live(Node)||!Live(Node->GetPackage())||Node->GetGraph()!=Graph||!ObjectAllowed(Node,Work)) { return Work.Fail(TEXT("node_unreadable"),TEXT("Graph node is unreadable or has an unsafe owner/dependency")); }
            Members.Add(MakeUnique<FIdentity>(Node));PinSlots.Add(Node,Node->Pins);
            if(Node->Pins.Num()>32768-Slots) { return Work.Fail(TEXT("pin_bound"),TEXT("Graph pin slots exceed32768")); }Slots+=Node->Pins.Num();
            for(UEdGraphPin* Pin:Node->Pins) { if(!Pin||Pin->WasTrashed()||Pin->GetOwningNodeUnchecked()!=Node) { return Work.Fail(TEXT("pin_unreadable"),TEXT("Graph pin is null, trashed, or outside its owner")); }Pins.Add(Pin); }
        }
        int32 LinkSlots=0;
        for(const UEdGraphPin* Pin:Pins)
        {
            TSet<const UEdGraphPin*> Chain;for(const UEdGraphPin* Parent=Pin;Parent;Parent=Parent->ParentPin)
            { if(!Pins.Contains(Parent)||Chain.Contains(Parent)||Chain.Num()>=64||Parent->GetOwningNodeUnchecked()!=Pin->GetOwningNodeUnchecked()) { return Work.Fail(TEXT("pin_parent_bound"),TEXT("Pin parent chain is cyclic, foreign, or deeper than64")); }Chain.Add(Parent); }
            if(Pin->SubPins.Num()>32768||Pin->LinkedTo.Num()>65536-LinkSlots) { return Work.Fail(TEXT("pin_link_bound"),TEXT("Pin subslot/link projection exceeds finite bounds")); }LinkSlots+=Pin->LinkedTo.Num();
            for(UEdGraphPin* Sub:Pin->SubPins) { if(!Pins.Contains(Sub)||Sub->ParentPin!=Pin) { return Work.Fail(TEXT("pin_subpin_unreadable"),TEXT("Pin subpin is outside current inventory")); } }
            for(UEdGraphPin* Other:Pin->LinkedTo) { if(!Pins.Contains(Other)) { return Work.Fail(TEXT("linked_pin_unreadable"),TEXT("Linked pin is outside current graph inventory")); } }
        }
        Work.SDK(TEXT("get_graph_schema_calls"));const UEdGraphSchema* CurrentSchema=Graph->GetSchema();
        if(!Work.Current()||!Identity.Current()||!Live(CurrentSchema)||!CurrentSchema->IsA(UAnimationGraphSchema::StaticClass())) { return Work.Fail(TEXT("schema_unavailable"),TEXT("Selected graph has no live AnimationGraph schema")); }
        if(Schema.Get()&&(Schema.Get()!=CurrentSchema||!SchemaIdentity->Current())) { return Work.Fail(TEXT("schema_changed"),TEXT("Graph schema identity changed after callback")); }
        Schema.Reset(const_cast<UEdGraphSchema*>(CurrentSchema));if(!SchemaIdentity) { SchemaIdentity=MakeUnique<FIdentity>(Schema.Get()); }
        if(!Pose) { Pose=FindObject<UScriptStruct>(nullptr,TEXT("/Script/Engine.PoseLink"));HeldPose.Reset(Pose); }
        if(!ComponentPose) { ComponentPose=FindObject<UScriptStruct>(nullptr,TEXT("/Script/Engine.ComponentSpacePoseLink"));HeldComponentPose.Reset(ComponentPose); }
        if(!Live(Pose)||!Live(ComponentPose)) { return Work.Fail(TEXT("pose_type_unavailable"),TEXT("Already-loaded stock Engine pose-link types are required")); }
        for(const auto& Id:Members) { if(!Id->Current()) { return Work.Fail(TEXT("node_identity_changed"),TEXT("Graph member identity changed during schema observation")); } }
        if(Graph->Nodes!=NodeSlots) { return Work.Fail(TEXT("node_slots_changed"),TEXT("Graph node slots changed during schema callback")); }
        for(const auto& Pair:PinSlots) { if(Pair.Key->Pins!=Pair.Value) { return Work.Fail(TEXT("pin_slots_changed"),TEXT("Node pin slots changed during schema callback; stale pin inventory is discarded")); } }return true;
    }
    bool PinCurrent(const UEdGraphPin* Pin) const
    { return Pin&&Pins.Contains(Pin)&&!Pin->WasTrashed()&&Live(Pin->GetOwningNodeUnchecked())&&Graph->Nodes.Contains(Pin->GetOwningNodeUnchecked())&&Pin->GetOwningNodeUnchecked()->Pins.Contains(const_cast<UEdGraphPin*>(Pin)); }
    bool IsPose(const UEdGraphPin* Pin,bool IsComponent=false) const
    { return PinCurrent(Pin)&&Pin->PinType.PinCategory==UEdGraphSchema_K2::PC_Struct&&Pin->PinType.PinSubCategoryObject.Get()==(IsComponent?ComponentPose:Pose); }
    void Anchor(UEdGraphNode* Node) { Anchors.Add(MakeUnique<FIdentity>(Node)); }
    UEdGraphPin* FirstPose(UEdGraphNode* Node,EEdGraphPinDirection Direction,const TArray<FString>& Preferred={},bool IsComponent=false) const
    {
        if(!Live(Node)||!Graph->Nodes.Contains(Node)) { return nullptr; }
        for(const FString& Name:Preferred) { for(UEdGraphPin* Pin:Node->Pins) { if(IsPose(Pin,IsComponent)&&Pin->Direction==Direction&&!Pin->ParentPin&&Pin->PinName.ToString().Equals(Name,ESearchCase::IgnoreCase)) { return Pin; } } }
        for(UEdGraphPin* Pin:Node->Pins) { if(IsPose(Pin,IsComponent)&&Pin->Direction==Direction&&!Pin->ParentPin) { return Pin; } }return nullptr;
    }
    UEdGraphPin* OutputPose(UEdGraphNode* Node) const
    { if(auto* Pin=FirstPose(Node,EGPD_Output,{TEXT("Pose"),TEXT("ComponentPose")},true)) { return Pin; }return FirstPose(Node,EGPD_Output,{TEXT("Pose"),TEXT("Result")}); }
};
template<class T> T* FirstNode(FGraphContext& Context)
{ for(UEdGraphNode* Node:Context.Graph->Nodes) { if(auto* Result=Cast<T>(Node)) { return Result; } }return nullptr; }
template<class T> T* AddNode(FGraphContext& Context,int32 X)
{
    FWork& Work=Context.Work;if(!Context.Refresh()) { return nullptr; }
    UClass* Type=T::StaticClass();if(!Live(Type)||!Live(Type->ClassWithin)||!Context.Graph->IsA(Type->ClassWithin)) { Work.Fail(TEXT("node_class_invalid"),TEXT("Stock node ClassWithin is incompatible with selected graph"));return nullptr; }
    if(Type==UAnimGraphNode_LinkedInputPose::StaticClass())
    {
        for(UEdGraph* Graph:Context.Blueprint->FunctionGraphs) { if(!Live(Graph)||!Live(Graph->Schema.Get())) { Work.Fail(TEXT("input_naming_unreadable"),TEXT("LinkedInputPose naming scan requires loaded function graphs and schema classes"));return nullptr; } }
        Work.SDK(TEXT("asset_editor_queries"));auto* Editors=GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
        if(!Editors) { Work.Fail(TEXT("asset_editor_unavailable"),TEXT("AssetEditorSubsystem unavailable before deferred input-node inspector callback"));return nullptr; }
        if(IAssetEditorInstance* Editor=Editors->FindEditorForAsset(Context.Blueprint,false);Editor&&Editor->GetEditorName()!=FName(TEXT("AnimationBlueprintEditor")))
        { Work.Fail(TEXT("asset_editor_unsupported"),TEXT("Existing asset editor would violate the SDK's deferred AnimationBlueprintEditor checked cast"));return nullptr; }
        Work.Result.Data->SetBoolField(TEXT("input_pose_PostPlaced_enqueues_weak_inspector_ticker"),true);
        if(!Context.Refresh()) { return nullptr; }
    }
    Work.Writer(TEXT("node_create_calls"));T* Node=NewObject<T>(Context.Graph);Work.Count(TEXT("node_create_returns"));
    if(!Work.Current()||!Live(Node)||Node->GetOuter()!=Context.Graph||!Live(Node->GetPackage())) { Work.Fail(TEXT("node_create_failed"),TEXT("Stock graph node allocation failed or changed identity"));return nullptr; }
    FIdentity CreatedId(Node);Node->NodePosX=X;Node->NodePosY=0;
    Work.Writer(TEXT("graph_add_node_calls"));Context.Graph->AddNode(Node,true);Work.Result.bChanged=true;Work.GraphChanged=true;
    if(!CreatedId.Current()||!Context.Refresh()||!Context.Graph->Nodes.Contains(Node)) { Work.Fail(TEXT("node_attach_failed"),TEXT("New node changed after AddNode callback"));return nullptr; }
    Context.Anchor(Node);Work.Writer(TEXT("node_guid_calls"));Node->CreateNewGuid();
    if(!Context.Refresh()) { return nullptr; }Work.Writer(TEXT("post_placed_calls"));static_cast<UEdGraphNode*>(Node)->PostPlacedNewNode();
    if(!Context.Refresh()) { return nullptr; }Work.Writer(TEXT("allocate_pins_calls"));static_cast<UEdGraphNode*>(Node)->AllocateDefaultPins();
    if(!Context.Refresh()) { return nullptr; }Work.Writer(TEXT("reconstruct_calls"));static_cast<UEdGraphNode*>(Node)->ReconstructNode();Work.Count(TEXT("reconstruct_returns"));
    if(!Context.Refresh()) { return nullptr; }return Node;
}
FString ResponseName(ECanCreateConnectionResponse Response)
{
    switch(Response)
    {
    case CONNECT_RESPONSE_MAKE:return TEXT("make");case CONNECT_RESPONSE_DISALLOW:return TEXT("disallow");case CONNECT_RESPONSE_BREAK_OTHERS_A:return TEXT("break_others_a");
    case CONNECT_RESPONSE_BREAK_OTHERS_B:return TEXT("break_others_b");case CONNECT_RESPONSE_BREAK_OTHERS_AB:return TEXT("break_others_ab");
    case CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE:return TEXT("make_with_conversion_node");default:return TEXT("unknown");
    }
}
bool Connect(FGraphContext& Context,UEdGraphPin* From,UEdGraphPin* To,bool Replace,bool& Changed,FString& Error)
{
    Changed=false;Error.Reset();FWork& Work=Context.Work;
    if(!Context.Refresh()) { return false; }
    if(!Context.PinCurrent(From)||!Context.PinCurrent(To)) { Error=TEXT("Invalid graph or pins for AnimGraph connection");return false; }
    if(To->LinkedTo.Contains(From)) { return true; }
    if(!To->LinkedTo.IsEmpty())
    {
        if(!Replace) { Error=FString::Printf(TEXT("AnimGraph target pose pin already has a different link: %s"),*To->PinName.ToString());return false; }
        Work.Writer(TEXT("break_all_pin_links_calls"));To->BreakAllPinLinks();Changed=true;Work.GraphChanged=true;Work.Result.bChanged=true;
        if(!Context.Refresh()||!Context.PinCurrent(From)||!Context.PinCurrent(To)) { Work.Fail(TEXT("connection_pin_changed"),TEXT("Pin changed after BreakAllPinLinks callback"));return false; }
    }
    Work.SDK(TEXT("can_create_connection_calls"));const FPinConnectionResponse Response=Context.Schema->CanCreateConnection(From,To);
    if(!Context.Refresh()||!Context.PinCurrent(From)||!Context.PinCurrent(To)) { Work.Fail(TEXT("connection_pin_changed"),TEXT("Pin changed after CanCreateConnection callback"));return false; }
    Work.Result.Data->SetStringField(TEXT("last_schema_response"),ResponseName(Response.Response));Work.Result.Data->SetStringField(TEXT("last_schema_message"),Response.Message.ToString());
    if(!Response.CanSafeConnect()&&(!Replace||Response.Response==CONNECT_RESPONSE_DISALLOW)) { Error=Response.Message.ToString();return false; }
    const TArray<UEdGraphPin*> Before=To->LinkedTo;
    Work.Writer(TEXT("try_create_connection_calls"));const bool Created=Context.Schema->TryCreateConnection(From,To);Work.Count(TEXT("try_create_connection_returns"));
    if(!Context.Refresh()||!Context.PinCurrent(From)||!Context.PinCurrent(To)) { Work.Fail(TEXT("connection_pin_changed"),TEXT("Pin changed after TryCreateConnection callback; further writes stopped"));return false; }
    Changed|=Before!=To->LinkedTo;Work.GraphChanged|=Changed;Work.Result.bChanged|=Changed;
    if(!Created||!To->LinkedTo.Contains(From)||!From->LinkedTo.Contains(To)) { Error=FString::Printf(TEXT("AnimationGraph schema failed to create connection from %s to %s"),*From->PinName.ToString(),*To->PinName.ToString());return false; }
    Changed=true;Work.GraphChanged=true;Work.Result.bChanged=true;return true;
}
void LinkRow(FWork& Work,const TCHAR* Name,bool Success,bool Changed,const FString& Error,bool Skipped=false)
{
    auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),Name);Row->SetBoolField(TEXT("success"),Success);Row->SetBoolField(TEXT("changed"),Changed);
    Row->SetBoolField(TEXT("skipped"),Skipped);Row->SetStringField(TEXT("error"),Error);Work.Links.Add(MakeShared<FJsonValueObject>(Row));Work.Legacy->SetArrayField(TEXT("created_links"),Work.Links);
}
TSharedRef<FJsonObject> PinJson(const UEdGraphPin* Pin,FGraphContext& Context)
{
    auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),Pin?Pin->PinName.ToString():FString());Row->SetStringField(TEXT("direction"),Pin&&Pin->Direction==EGPD_Output?TEXT("output"):TEXT("input"));
    if(!Context.PinCurrent(Pin)) { return Row; }
    if(!TextSafe(Pin->DefaultValue,65536)) { Context.Work.Fail(TEXT("pin_readback_bound"),TEXT("Pin default text exceeds65536/NUL readback bound"));return Row; }
    Row->SetStringField(TEXT("category"),Pin->PinType.PinCategory.ToString());Row->SetStringField(TEXT("subcategory"),Pin->PinType.PinSubCategory.ToString());Row->SetBoolField(TEXT("is_array"),Pin->PinType.IsArray());
    auto Type=MakeShared<FJsonObject>();Type->SetStringField(TEXT("category"),Pin->PinType.PinCategory.ToString());Type->SetStringField(TEXT("subcategory"),Pin->PinType.PinSubCategory.ToString());Type->SetBoolField(TEXT("is_array"),Pin->PinType.IsArray());
    if(UObject* Sub=Pin->PinType.PinSubCategoryObject.Get())
    { if(!Live(Sub)) { Context.Work.Fail(TEXT("pin_type_unreadable"),TEXT("Pin subtype object is not loaded"));return Row; }Row->SetStringField(TEXT("subcategory_object"),Sub->GetPathName());Type->SetStringField(TEXT("subcategory_object"),Sub->GetPathName()); }
    Row->SetObjectField(TEXT("pin_type"),Type);Row->SetStringField(TEXT("default_value"),Pin->DefaultValue);
    if(Pin->DefaultObject) { if(!Live(Pin->DefaultObject)) { Context.Work.Fail(TEXT("pin_default_unreadable"),TEXT("Pin default object is not loaded"));return Row; }Row->SetStringField(TEXT("default_object"),Pin->DefaultObject->GetPathName()); }
    TArray<TSharedPtr<FJsonValue>> Linked;for(UEdGraphPin* Other:Pin->LinkedTo)
    { if(!Context.PinCurrent(Other)) { Context.Work.Fail(TEXT("pin_link_unreadable"),TEXT("Pin link is not current during readback"));break; }auto Link=MakeShared<FJsonObject>();Link->SetStringField(TEXT("node_id"),Other->GetOwningNodeUnchecked()->NodeGuid.ToString());Link->SetStringField(TEXT("node_name"),Other->GetOwningNodeUnchecked()->GetName());Link->SetStringField(TEXT("pin_name"),Other->PinName.ToString());Linked.Add(MakeShared<FJsonValueObject>(Link)); }
    Row->SetArrayField(TEXT("linked_to"),Linked);return Row;
}
TSharedRef<FJsonObject> NodeJson(UEdGraphNode* Node,FGraphContext& Context)
{
    auto Row=MakeShared<FJsonObject>();if(!Node||!Context.Refresh()||!Context.Graph->Nodes.Contains(Node)) { return Row; }FIdentity Id(Node);
    Context.Work.SDK(TEXT("node_title_calls"));const FString Title=Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
    if(!Context.Refresh()||!Id.Current()||!TextSafe(Title,65536)||!TextSafe(Node->NodeComment,65536)) { Context.Work.Fail(TEXT("node_readback_incomplete"),TEXT("Node title callback changed identity or output text exceeded bounds"));return Row; }
    Row->SetStringField(TEXT("node_id"),Node->NodeGuid.ToString());Row->SetStringField(TEXT("name"),Node->GetName());Row->SetStringField(TEXT("class"),Node->GetClass()->GetName());Row->SetStringField(TEXT("title"),Title);
    Row->SetNumberField(TEXT("x"),Node->NodePosX);Row->SetNumberField(TEXT("y"),Node->NodePosY);Row->SetStringField(TEXT("node_comment"),Node->NodeComment);
    Row->SetBoolField(TEXT("comment_bubble_visible"),Node->bCommentBubbleVisible);Row->SetBoolField(TEXT("comment_bubble_pinned"),Node->bCommentBubblePinned);
    TArray<TSharedPtr<FJsonValue>> Values;for(UEdGraphPin* Pin:Node->Pins) { Values.Add(MakeShared<FJsonValueObject>(PinJson(Pin,Context))); }Row->SetArrayField(TEXT("pins"),Values);return Row;
}
void InputSummary(UAnimGraphNode_LinkedInputPose* Input,UAnimGraphNode_LinkedAnimGraph* Linked,bool Attempted,bool Required,FGraphContext& Context)
{
    auto Summary=MakeShared<FJsonObject>();Context.Work.Legacy->SetObjectField(TEXT("linked_input_pose_summary"),Summary);
    Summary->SetBoolField(TEXT("input_link_attempted"),Attempted);Summary->SetBoolField(TEXT("input_link_required"),Required);
    if(!Context.Refresh()) { return; }auto* Pin=Context.FirstPose(Linked,EGPD_Input,{TEXT("InPose"),TEXT("InputPose"),TEXT("Pose"),TEXT("Source")});
    Summary->SetBoolField(TEXT("linked_graph_has_input_pose_pin"),Pin!=nullptr);int32 Count=0;for(UEdGraphPin* Item:Linked->Pins) { if(Context.IsPose(Item)&&Item->Direction==EGPD_Input&&!Item->ParentPin) { ++Count; } }
    Summary->SetNumberField(TEXT("linked_graph_input_pose_pin_count"),Count);Summary->SetObjectField(TEXT("linked_graph_node"),NodeJson(Linked,Context));
    if(!Context.Work.Current()) { return; }if(Input) { Summary->SetObjectField(TEXT("input_pose_node"),NodeJson(Input,Context)); }
    if(!Context.Work.Current()||!Context.Refresh()) { return; }Pin=Context.FirstPose(Linked,EGPD_Input,{TEXT("InPose"),TEXT("InputPose"),TEXT("Pose"),TEXT("Source")});
    if(Pin) { Summary->SetObjectField(TEXT("linked_graph_input_pin"),PinJson(Pin,Context)); }
}
} // namespace
Variable::FResult Ensure(const FString& SourceAnimBPPath,const FString& LinkedAnimBPPath,const FString& TargetAnimBPPath,const FString& TargetRoot,const FString& VariantName,const FString& InsertMode,bool bAllowExistingTargetReuse,bool bOverwriteExisting,bool bReplaceExistingLinks,bool bRequireInputPosePin,bool bCompile,bool bSaveAsset,bool bDryRun,bool bValidateOnly,const FString& PrivateGraphContextJson)
{
    FWork Work(bCompile,bSaveAsset,bDryRun||bValidateOnly);auto& Result=Work.Result;auto& Legacy=Work.Legacy;
    if(!IsEditorGameThread()||IsPlaySessionActive()||Running) { Work.Fail(TEXT("editor_precondition"),TEXT("Idle Editor game thread and no overlapping FE writer required"));return Work.Finish(); }
    TGuardValue<bool> Busy(Running,true);
    for(const FString* Text:{&SourceAnimBPPath,&LinkedAnimBPPath,&TargetAnimBPPath,&TargetRoot,&VariantName,&InsertMode})
    { if(!TextSafe(*Text,CarrierChars)) { Work.Fail(TEXT("argument_bound"),TEXT("String input exceeds1Mi UTF16 units or contains NUL"));return Work.Finish(); } }
    TSharedPtr<FJsonValue> ContextValue;
    if(!Decode(PrivateGraphContextJson,ContextValue)||ContextValue->Type!=EJson::Object) { Work.Fail(TEXT("private_context_invalid"),TEXT("PrivateGraphContextJson must be a bounded object"));return Work.Finish(); }
    const TSharedPtr<FJsonObject> Params=ContextValue->AsObject();
    for(const auto& Pair:Params->Values)
    { const FString Key(Pair.Key.ToView());if(Key!=TEXT("graph_name")&&Key!=TEXT("graph_type")&&Key!=TEXT("target_blueprint_path")&&Key!=TEXT("unsupported_layer_keys")) { Work.Fail(TEXT("private_context_key_invalid"),TEXT("Only graph_name/graph_type/target_blueprint_path/unsupported_layer_keys are permitted"));return Work.Finish(); } }
    const TArray<TSharedPtr<FJsonValue>>* Unsupported=nullptr;
    if(Params->HasField(TEXT("unsupported_layer_keys")))
    {
        if(!Params->TryGetArrayField(TEXT("unsupported_layer_keys"),Unsupported)||Unsupported->Num()>3) { Work.Fail(TEXT("private_context_invalid"),TEXT("Layer presence must be an array of the three confirmed keys"));return Work.Finish(); }
        TSet<FString> Seen;for(const auto& Value:*Unsupported)
        {
            FString Key;if(!Value.IsValid()||Value->Type!=EJson::String||!Value->TryGetString(Key)||Seen.Contains(Key)||(Key!=TEXT("anim_layer_interface_path")&&Key!=TEXT("layer_name")&&Key!=TEXT("linked_layer_instance_bp_path")))
            { Work.Fail(TEXT("private_context_invalid"),TEXT("Layer presence contains a duplicate or unknown key"));return Work.Finish(); }Seen.Add(Key);
        }
    }
    if(SourceAnimBPPath.IsEmpty()) { Work.Fail(TEXT("source_missing"),TEXT("Missing 'source_anim_bp_path' parameter"));return Work.Finish(); }
    if(LinkedAnimBPPath.IsEmpty()) { Work.Fail(TEXT("linked_source_missing"),TEXT("Missing 'linked_anim_bp_path' parameter"));return Work.Finish(); }
    if(Unsupported&&!Unsupported->IsEmpty())
    { Legacy->SetStringField(TEXT("unsupported_feature"),TEXT("linked_anim_layer"));Work.Fail(TEXT("linked_layer_unsupported"),TEXT("Linked Anim Layer authoring is not supported by ensure_linked_anim_graph_sample_variant MVP"));return Work.Finish(); }
    if(!InsertMode.Equals(TEXT("input_pose_to_linked_graph_to_root"),ESearchCase::IgnoreCase))
    { Legacy->SetStringField(TEXT("unsupported_feature"),TEXT("insert_mode"));Legacy->SetStringField(TEXT("supported_insert_mode"),TEXT("input_pose_to_linked_graph_to_root"));Work.Fail(TEXT("insert_mode_unsupported"),TEXT("Unsupported insert_mode for linked graph MVP: ")+InsertMode);return Work.Finish(); }
    Result.Stage=TEXT("load_source_blueprint");auto* Source=Cast<UAnimBlueprint>(FindBlueprint(SourceAnimBPPath,Work));
    if(!Work.Current()||!Live(Source)||!Live(Source->GetPackage())||!ObjectAllowed(Source,Work)) { Work.Fail(TEXT("source_not_found"),TEXT("Source Anim Blueprint not found or invalid: ")+SourceAnimBPPath);return Work.Finish(); }
    Work.Sources.Add(MakeUnique<FIdentity>(Source,true));
    Result.Stage=TEXT("load_linked_blueprint");auto* LinkedBlueprint=Cast<UAnimBlueprint>(FindBlueprint(LinkedAnimBPPath,Work));
    if(!Work.Current()||!Live(LinkedBlueprint)||!Live(LinkedBlueprint->GetPackage())||!ObjectAllowed(LinkedBlueprint,Work)) { Work.Fail(TEXT("linked_source_not_found"),TEXT("Linked Anim Blueprint not found or invalid: ")+LinkedAnimBPPath);return Work.Finish(); }
    Work.Sources.Add(MakeUnique<FIdentity>(LinkedBlueprint,true));
    TStrongObjectPtr<USkeleton> SourceSkeleton(Source->TargetSkeleton),LinkedSkeleton(LinkedBlueprint->TargetSkeleton);
    Legacy->SetStringField(TEXT("source_skeleton"),GetPathNameSafe(SourceSkeleton.Get()));Legacy->SetStringField(TEXT("linked_skeleton"),GetPathNameSafe(LinkedSkeleton.Get()));
    if(!Live(SourceSkeleton.Get())||!Live(LinkedSkeleton.Get())) { Work.Fail(TEXT("skeleton_unavailable"),TEXT("Source and linked AnimBPs must both have TargetSkeleton set for linked graph sample authoring"));return Work.Finish(); }
    if(SourceSkeleton.Get()!=LinkedSkeleton.Get()) { Legacy->SetStringField(TEXT("failure_reason"),TEXT("skeleton_mismatch"));Work.Fail(TEXT("skeleton_mismatch"),TEXT("Source and linked AnimBPs use different TargetSkeleton assets"));return Work.Finish(); }
    Work.CommonSkeleton=MakeUnique<FIdentity>(SourceSkeleton.Get());Work.SDK(TEXT("linked_generated_class_queries"));UClass* LinkedClass=LinkedBlueprint->GetAnimBlueprintGeneratedClass();
    if(!Work.Current()||!Live(LinkedClass)||!LinkedClass->IsChildOf(UAnimInstance::StaticClass())||LinkedClass->ClassGeneratedBy!=LinkedBlueprint
        ||LinkedClass->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)||!ClassAllowed(LinkedClass,Work))
    { Legacy->SetStringField(TEXT("failure_reason"),TEXT("linked_generated_class_unavailable"));Work.Fail(TEXT("linked_generated_class_unavailable"),TEXT("Linked AnimBP does not have a current concrete UAnimInstance generated class"));return Work.Finish(); }
    if(LinkedBlueprint->SkeletonGeneratedClass&&(!Live(LinkedBlueprint->SkeletonGeneratedClass)||!LinkedBlueprint->SkeletonGeneratedClass->IsChildOf(UAnimInstance::StaticClass())||LinkedBlueprint->SkeletonGeneratedClass->ClassGeneratedBy!=LinkedBlueprint))
    { Work.Fail(TEXT("linked_skeleton_class_invalid"),TEXT("Linked skeleton generated class is stale or belongs to another Blueprint"));return Work.Finish(); }
    Work.AssignedClass=MakeUnique<FIdentity>(LinkedClass);Work.ClassOwner=LinkedBlueprint;
    Result.Stage=TEXT("resolve_target_path");const FString Root=NormalizePackage(TargetRoot),Variant=VariantSegment(VariantName);FString TargetPath=NormalizePackage(TargetAnimBPPath);
    if(TargetPath.IsEmpty()) { FString Alias;if(Params->TryGetStringField(TEXT("target_blueprint_path"),Alias)) { TargetPath=NormalizePackage(Alias); } }
    if(TargetPath.IsEmpty()) { TargetPath=FString::Printf(TEXT("%s/%s_%s"),*Root,*VariantSegment(Source->GetName()),*Variant); }
    if(!ValidGamePackage(Root)||!ValidGamePackage(TargetPath)||!SamplePath(TargetPath)) { Work.Fail(TEXT("target_path_refused"),TEXT("Valid /Game target_root and a target below _MCP_Sample/_MCP_Temp/CubelessMCPTests are required"));return Work.Finish(); }
    if(TargetPath.Equals(Source->GetPackage()->GetName(),ESearchCase::IgnoreCase)||TargetPath.Equals(LinkedBlueprint->GetPackage()->GetName(),ESearchCase::IgnoreCase))
    { Work.Fail(TEXT("source_target_alias_refused"),TEXT("Refusing to use the source or linked AnimBP as the target sample AnimBP"));return Work.Finish(); }
    Legacy->SetStringField(TEXT("source_anim_bp_path"),Source->GetPathName());Legacy->SetStringField(TEXT("linked_anim_bp_path"),LinkedBlueprint->GetPathName());Legacy->SetStringField(TEXT("target_anim_bp_path"),TargetPath);
    Legacy->SetStringField(TEXT("target_root"),Root);Legacy->SetStringField(TEXT("variant_name"),Variant);Legacy->SetStringField(TEXT("insert_mode"),TEXT("input_pose_to_linked_graph_to_root"));Legacy->SetStringField(TEXT("linked_instance_class"),LinkedClass->GetPathName());
    Legacy->SetBoolField(TEXT("replace_existing_links"),bReplaceExistingLinks);Legacy->SetBoolField(TEXT("require_input_pose_pin"),bRequireInputPosePin);
    Legacy->SetBoolField(TEXT("source_dirty_before"),Work.Sources[0]->DirtyBefore);Legacy->SetBoolField(TEXT("linked_source_dirty_before"),Work.Sources[1]->DirtyBefore);
    Work.TargetPackages.Add(TargetPath);if(!Work.Current()) { return Work.Finish(); }
    if(bDryRun||bValidateOnly)
    { Legacy->SetStringField(TEXT("runtime_note"),TEXT("Dry run only: no AnimBP was explicitly duplicated, edited, compiled, or saved. Asset loads/SDK callbacks remain unknown."));Result.bSuccess=true;Result.bPostconditionPassed=true;Result.Stage=TEXT("complete");return Work.Finish(); }
    Result.Stage=TEXT("preflight_source_runtime_and_instance_graphs");
    if(!LinkedClosure(Source,nullptr,Work)||!LinkedClosure(LinkedBlueprint,nullptr,Work)||!RuntimePackageGuard(Source,Work,TEXT("source_runtime_projection"))||!RuntimePackageGuard(LinkedBlueprint,Work,TEXT("linked_source_runtime_projection"))
        ||!SavedDependencyGuard({Source->GetPackage()->GetName(),LinkedBlueprint->GetPackage()->GetName()},Work)) { return Work.Finish(); }
    Result.Stage=TEXT("prepare_target_blueprint");FTargetPlan Plan(TargetPath,TEXT("AnimBlueprint"),UAnimBlueprint::StaticClass());
    if(!InspectTarget(Plan,Work)) { return Work.Finish(); }
    if(Plan.Exists&&!bAllowExistingTargetReuse&&!bOverwriteExisting) { Work.Fail(TEXT("target_exists"),TEXT("Temp asset already exists: ")+TargetPath);return Work.Finish(); }
    auto* Target=Cast<UAnimBlueprint>(PrepareTarget(Source,Plan,bOverwriteExisting,Work));
    bool Created=false,Reused=false;Plan.Receipt->TryGetBoolField(TEXT("created"),Created);Plan.Receipt->TryGetBoolField(TEXT("reused"),Reused);Work.Created=Created;Work.Reused=Reused;
    if(!Work.Current()||!Live(Target)) { Work.Fail(TEXT("target_prepare_failed"),TEXT("Failed to prepare target sample AnimBP"));return Work.Finish(); }
    Work.Target=Work.AddTarget(Target,TargetPath);if(!Work.Target||!Work.Current()||!ObjectAllowed(Target,Work)) { return Work.Finish(); }
    Legacy->SetStringField(TEXT("target_skeleton"),GetPathNameSafe(Target->TargetSkeleton));
    if(!Work.CommonSkeleton->Current()||Source->TargetSkeleton!=SourceSkeleton.Get()||LinkedBlueprint->TargetSkeleton!=SourceSkeleton.Get()||Target->TargetSkeleton!=SourceSkeleton.Get())
    { Legacy->SetStringField(TEXT("failure_reason"),TEXT("target_skeleton_mismatch"));Work.Fail(TEXT("target_skeleton_mismatch"),TEXT("Target sample AnimBP uses a different TargetSkeleton than the source AnimBP"));return Work.Finish(); }
    Work.TargetSkeletonLocked=true;
    if(!RuntimePackageGuard(Target,Work,TEXT("target_runtime_projection_before_edit"))) { return Work.Finish(); }
    Result.Stage=TEXT("resolve_target_graph");FString GraphName=TEXT("AnimGraph"),Type=TEXT("function"),Selected;
    if(Params->TryGetStringField(TEXT("graph_name"),Selected)) { GraphName=Selected; }if(Params->TryGetStringField(TEXT("graph_type"),Selected)) { Type=Selected; }
    if(!TextSafe(GraphName,NAME_SIZE-1)||!TextSafe(Type,NAME_SIZE-1)) { Work.Fail(TEXT("graph_selector_bound"),TEXT("Graph selector exceeds safe text bounds"));return Work.Finish(); }
    const FString NormalType=NormalizeGraphType(Type);TArray<UEdGraph*> Listed;if(!Graphs(Target,Listed,Work)) { return Work.Finish(); }UEdGraph* TargetGraph=nullptr;
    for(UEdGraph* Graph:Listed)
    { if((GraphName.IsEmpty()||Graph->GetName().Equals(GraphName,ESearchCase::IgnoreCase))&&(NormalType.IsEmpty()||NormalType==TEXT("any")||GraphType(Target,Graph)==NormalType)) { TargetGraph=Graph;break; } }
    if(GraphName.IsEmpty()&&NormalType.IsEmpty())
    {
        TargetGraph=nullptr;for(UEdGraph* Graph:Target->UbergraphPages) { if(Graph&&Graph->GetName().Contains(TEXT("EventGraph"))) { TargetGraph=Graph;break; } }
        if(!TargetGraph)
        {
            Result.Stage=TEXT("create_default_event_graph");Work.Writer(TEXT("create_default_event_graph_calls"));
            UEdGraph* CreatedGraph=FBlueprintEditorUtils::CreateNewGraph(Target,FName(TEXT("EventGraph")),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
            TStrongObjectPtr<UEdGraph> HeldCreatedGraph(CreatedGraph);
            if(!Work.Current()||!Live(CreatedGraph)||CreatedGraph->GetOuter()!=Target||CreatedGraph->GetPackage()!=Target->GetPackage()) { Work.Fail(TEXT("default_graph_creation_failed"),TEXT("Default EventGraph allocation changed owner or failed"));return Work.Finish(); }
            FIdentity CreatedGraphId(CreatedGraph);Work.Writer(TEXT("add_default_ubergraph_calls"));FBlueprintEditorUtils::AddUbergraphPage(Target,CreatedGraph);
            Work.GraphChanged=true;Result.bChanged=true;
            if(!Work.Current()||!CreatedGraphId.Current()||!Target->UbergraphPages.Contains(CreatedGraph)) { Work.Fail(TEXT("default_graph_creation_failed"),TEXT("Default EventGraph changed during attachment"));return Work.Finish(); }
            TargetGraph=CreatedGraph;Result.Data->SetBoolField(TEXT("default_event_graph_created_before_animation_schema_rejection"),true);
        }
    }
    if(!TargetGraph) { Work.Fail(TEXT("graph_not_found"),FString::Printf(TEXT("Blueprint graph not found. graph_name='%s', graph_type='%s'"),*GraphName,*Type));return Work.Finish(); }
    FGraphContext Context(Work,Target,TargetGraph);if(!Context.Refresh()||!LinkedClosure(Target,LinkedBlueprint,Work)) { return Work.Finish(); }
    Legacy->SetStringField(TEXT("blueprint_name"),Target->GetPathName());Legacy->SetStringField(TEXT("graph_name"),TargetGraph->GetName());Legacy->SetStringField(TEXT("graph_id"),TargetGraph->GraphGuid.ToString());Legacy->SetStringField(TEXT("graph_type"),GraphType(Target,TargetGraph));
    Result.Stage=TEXT("ensure_linked_anim_graph_node");UAnimGraphNode_LinkedAnimGraph* LinkedNode=nullptr;
    for(UEdGraphNode* Node:TargetGraph->Nodes)
    { if(auto* Candidate=Cast<UAnimGraphNode_LinkedAnimGraph>(Node)) { UClass* Class=Candidate->Node.InstanceClass.Get();if(Class&&!Live(Class)) { Work.Fail(TEXT("node_instance_class_unreadable"),TEXT("Existing linked node class is not loaded"));return Work.Finish(); }if(Class&&Class->GetPathName().Equals(LinkedClass->GetPathName(),ESearchCase::IgnoreCase)) { LinkedNode=Candidate;break; } } }
    bool CreatedLinked=false;if(!LinkedNode) { LinkedNode=AddNode<UAnimGraphNode_LinkedAnimGraph>(Context,-240);CreatedLinked=LinkedNode!=nullptr; }else { Context.Anchor(LinkedNode); }
    Legacy->SetBoolField(TEXT("created_linked_anim_graph_node"),CreatedLinked);if(!LinkedNode||!Context.Refresh()) { Work.Fail(TEXT("linked_node_create_failed"),TEXT("Failed to create Linked Anim Graph node"));return Work.Finish(); }
    if(LinkedNode->Node.InstanceClass.Get()!=LinkedClass)
    {
        Work.Writer(TEXT("node_modify_calls"));LinkedNode->Modify();if(!Context.Refresh()) { return Work.Finish(); }
        Work.Writer(TEXT("instance_class_writes"));LinkedNode->Node.InstanceClass=LinkedClass;Work.Result.bChanged=true;Work.GraphChanged=true;
    }
    if(!LinkedClosure(Target,nullptr,Work)||!Context.Refresh()) { return Work.Finish(); }
    // UE58 pose pins resolve the protected FunctionReference. Public InstanceClass notification initializes it.
    Result.Stage=TEXT("initialize_linked_graph_function_reference");
    FProperty* InstanceClassProperty=FindFProperty<FProperty>(FAnimNode_LinkedAnimGraph::StaticStruct(),GET_MEMBER_NAME_CHECKED(FAnimNode_LinkedAnimGraph,InstanceClass));
    if(!InstanceClassProperty) { Work.Fail(TEXT("instance_class_property_missing"),TEXT("UE58 public InstanceClass property is unavailable"));return Work.Finish(); }
    Result.Data->SetBoolField(TEXT("public_instance_class_notification_attempted"),true);
    Result.Data->SetBoolField(TEXT("instance_class_notification_implicit_reconstruct_possible"),true);
    Work.Writer(TEXT("instance_class_post_edit_change_calls"));
    FPropertyChangedEvent InstanceClassChange(InstanceClassProperty,EPropertyChangeType::ValueSet);
    static_cast<UObject*>(LinkedNode)->PostEditChangeProperty(InstanceClassChange);Work.Count(TEXT("instance_class_post_edit_change_returns"));
    if(!Context.Refresh()||LinkedNode->Node.InstanceClass.Get()!=LinkedClass) { Work.Fail(TEXT("linked_node_class_changed"),TEXT("Linked node class or owner did not survive public structural notification"));return Work.Finish(); }
    Work.GraphChanged=true; // This public structural notification requires final structural compile handling even on reuse.
    Result.Stage=TEXT("find_root_and_pose_pins");auto* RootNode=FirstNode<UAnimGraphNode_Root>(Context);
    if(!RootNode) { Work.Fail(TEXT("root_missing"),TEXT("AnimGraph root node not found in graph: ")+TargetGraph->GetName());return Work.Finish(); }Context.Anchor(RootNode);
    auto* InputPin=Context.FirstPose(LinkedNode,EGPD_Input,{TEXT("InPose"),TEXT("InputPose"),TEXT("Pose"),TEXT("Source")});UAnimGraphNode_LinkedInputPose* InputNode=nullptr;
    bool CreatedInput=false,InputAttempted=false;
    if(InputPin)
    {
        Result.Stage=TEXT("ensure_linked_input_pose_node");InputNode=FirstNode<UAnimGraphNode_LinkedInputPose>(Context);
        if(!InputNode) { InputNode=AddNode<UAnimGraphNode_LinkedInputPose>(Context,-480);CreatedInput=InputNode!=nullptr; }else { Context.Anchor(InputNode); }
        Legacy->SetBoolField(TEXT("created_input_pose_node"),CreatedInput);
        if(!InputNode||!Context.Refresh()) { Work.Fail(TEXT("input_node_create_failed"),TEXT("Failed to create LinkedInputPose node"));return Work.Finish(); }
        InputPin=Context.FirstPose(LinkedNode,EGPD_Input,{TEXT("InPose"),TEXT("InputPose"),TEXT("Pose"),TEXT("Source")});
        Result.Stage=TEXT("connect_input_pose_to_linked_graph");bool Changed=false;FString Error;InputAttempted=true;
        const bool Connected=Connect(Context,Context.FirstPose(InputNode,EGPD_Output),InputPin,bReplaceExistingLinks,Changed,Error);LinkRow(Work,TEXT("input_pose_to_linked_graph"),Connected,Changed,Error);
        if(!Work.Current()) { return Work.Finish(); }if(!Connected) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Failed to connect input pose to Linked Anim Graph: ")+Error)); }
    }
    else
    {
        const FString Message=TEXT("Linked Anim Graph node has no exposed input pose pin; output-to-root link will still be authored.");
        LinkRow(Work,TEXT("input_pose_to_linked_graph"),!bRequireInputPosePin,false,bRequireInputPosePin?Message:FString(),true);
        if(bRequireInputPosePin) { Work.Errors.Add(MakeShared<FJsonValueString>(Message)); }
    }
    Legacy->SetBoolField(TEXT("created_input_pose_node"),CreatedInput);
    Result.Stage=TEXT("connect_linked_graph_to_root");if(!Context.Refresh()) { return Work.Finish(); }
    bool ChangedOutput=false;FString OutputError;const bool OutputConnected=Connect(Context,Context.OutputPose(LinkedNode),Context.FirstPose(RootNode,EGPD_Input,{TEXT("Result")}),bReplaceExistingLinks,ChangedOutput,OutputError);
    LinkRow(Work,TEXT("linked_graph_to_root"),OutputConnected,ChangedOutput,OutputError);
    if(!Work.Current()) { return Work.Finish(); }if(!OutputConnected) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Failed to connect Linked Anim Graph to root: ")+OutputError)); }
    InputSummary(InputNode,LinkedNode,InputAttempted,bRequireInputPosePin,Context);if(!Work.Current()) { return Work.Finish(); }
    if(!Work.Errors.IsEmpty()) { Work.Fail(TEXT("graph_authoring_failed"),TEXT("Failed to author Linked Anim Graph sample"));return Work.Finish(); }
    if(Work.GraphChanged)
    {
        Result.Stage=TEXT("mark_structurally_modified");if(!LinkedClosure(Target,nullptr,Work)||!Context.Refresh()) { return Work.Finish(); }
        Work.Writer(TEXT("mark_structurally_modified_calls"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Target);Work.Count(TEXT("mark_structurally_modified_returns"));
        if(!Context.Refresh()) { return Work.Finish(); }Work.Writer(TEXT("notify_graph_changed_calls"));TargetGraph->NotifyGraphChanged();if(!Context.Refresh()) { return Work.Finish(); }
        Result.Data->SetBoolField(TEXT("structural_mark_implicit_skeleton_compile_possible"),true);
    }
    bool CompileOkay=true,SaveOkay=true;
    Result.Stage=TEXT("compile_target_blueprint");
    if(bCompile)
    {
        if(!LinkedClosure(Target,nullptr,Work)||!Context.Refresh()) { return Work.Finish(); }Result.bCompileAttempted=true;Work.Compile->SetBoolField(TEXT("attempted"),true);
        FCompilerResultsLog Log;Work.Writer(TEXT("compile_blueprint_calls"));FKismetEditorUtilities::CompileBlueprint(Target,EBlueprintCompileOptions::None,&Log);Work.Count(TEXT("compile_blueprint_returns"));
        Work.Compile->SetNumberField(TEXT("error_count"),Log.NumErrors);Work.Compile->SetNumberField(TEXT("warning_count"),Log.NumWarnings);
        if(!Context.Refresh()) { return Work.Finish(); }
        const bool LegacyCompileSuccess=Target->GeneratedClass!=nullptr;UClass* Generated=Target->GeneratedClass;
        CompileOkay=Live(Generated)&&Generated->IsChildOf(UAnimInstance::StaticClass())&&Generated->ClassGeneratedBy==Target&&!Generated->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)
            &&Log.NumErrors==0&&(Target->Status==BS_UpToDate||Target->Status==BS_UpToDateWithWarnings);
        Result.Compiled=CompileOkay;Work.Compile->SetBoolField(TEXT("success"),LegacyCompileSuccess);Work.Compile->SetBoolField(TEXT("compiler_status_verified"),CompileOkay);
        Work.Compile->SetNumberField(TEXT("blueprint_status"),static_cast<int32>(Target->Status));Work.Compile->SetStringField(TEXT("blueprint"),Target->GetPathName());Work.Compile->SetStringField(TEXT("generated_class"),GetPathNameSafe(Generated));
        if(!RuntimePackageGuard(Target,Work,TEXT("target_runtime_projection_after_compile"))) { return Work.Finish(); }
    }
    Result.Stage=TEXT("save_target_blueprint");
    if(bSaveAsset) { SaveOkay=SaveOne(*Work.Target,TEXT("target_saved"),Work);Result.bSaved=SaveOkay;if(!Work.Current()) { return Work.Finish(); } }
    TArray<TSharedPtr<FJsonValue>> CompletionErrors;if(!CompileOkay) { CompletionErrors.Add(MakeShared<FJsonValueString>(TEXT("compile_failed"))); }if(!SaveOkay) { CompletionErrors.Add(MakeShared<FJsonValueString>(TEXT("save_failed"))); }
    Legacy->SetBoolField(TEXT("compile_succeeded"),CompileOkay);Legacy->SetBoolField(TEXT("save_succeeded"),SaveOkay);if(!CompletionErrors.IsEmpty()) { Legacy->SetArrayField(TEXT("completion_errors"),CompletionErrors); }
    Result.Stage=TEXT("readback_linked_graph");if(!Context.Refresh()||LinkedNode->Node.InstanceClass.Get()!=LinkedClass) { Work.Fail(TEXT("linked_node_class_changed"),TEXT("Linked node class readback is not the captured generated class"));return Work.Finish(); }
    Legacy->SetStringField(TEXT("created_node_id"),LinkedNode->NodeGuid.ToString());Legacy->SetStringField(TEXT("linked_instance_class_readback"),LinkedClass->GetPathName());
    Legacy->SetObjectField(TEXT("linked_anim_graph_node"),NodeJson(LinkedNode,Context));if(!Work.Current()) { return Work.Finish(); }
    if(InputNode) { Legacy->SetObjectField(TEXT("input_pose_node"),NodeJson(InputNode,Context));if(!Work.Current()) { return Work.Finish(); } }
    Legacy->SetObjectField(TEXT("root_node"),NodeJson(RootNode,Context));if(!Work.Current()) { return Work.Finish(); }
    InputSummary(InputNode,LinkedNode,InputAttempted,bRequireInputPosePin,Context);if(!Work.Current()) { return Work.Finish(); }
    const FString NodeId=LinkedNode->NodeGuid.ToString(),GraphReadbackName=TargetGraph->GetName();
    Work.SDK(TEXT("existing_native_topology_calls"));auto Readback=AnimGraphInspection::Topology(TargetPath,NodeId,TEXT(""),TEXT(""),true,true,true,false,true,4,256,512,GraphReadbackName,TEXT(""),TEXT("function"));Work.Count(TEXT("existing_native_topology_returns"));
    if(const TSharedPtr<FJsonObject>* Inner=nullptr;Readback.Data->TryGetObjectField(TEXT("legacy_result"),Inner)) { Legacy->SetObjectField(TEXT("readback"),*Inner); }else { Legacy->SetObjectField(TEXT("readback"),Readback.Data); }
    if(!Readback.bChangeStateKnown) { Result.bChangeStateKnown=false; }Result.bMutationAttempted|=Readback.bMutationAttempted;Result.bChanged|=Readback.bChanged;
    if(!Context.Refresh()) { return Work.Finish(); }
    if(!Readback.bSuccess||!Readback.bPostconditionPassed) { Work.Fail(TEXT("readback_incomplete"),TEXT("Existing Native topology readback failed; completed explicit writes/compile/save are retained"));return Work.Finish(); }
    if(!CompileOkay||!SaveOkay) { Work.Fail(!CompileOkay?TEXT("compile_failed"):TEXT("save_failed"),TEXT("Requested compiler/save postconditions failed; inspect completion_errors"));return Work.Finish(); }
    Result.bSuccess=true;Result.bPostconditionPassed=true;Result.Stage=TEXT("complete");return Work.Finish();
}
} // namespace Cubeless::UE58::LinkedAnimGraphVariant
