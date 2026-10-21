#include "Compatibility/UE58/CubelessAnimBPLocomotionDriverApi.h"
#include "Compatibility/UE58/CubelessBlueprintTopologyApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "AnimGraphNode_LinkedAnimGraph.h"
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
#include "Misc/ScopeExit.h"
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
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_IfThenElse.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "KismetAnimationLibrary.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/NavMovementComponent.h"
namespace Cubeless::UE58::AnimBPLocomotionDriver
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
    for(const TCHAR* Prefix:{TEXT("/Game/_MCP_Sample"),TEXT("/Game/_MCP_Temp")})
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
    TArray<TUniquePtr<FIdentity>> Inputs;
    FIdentity* Target=nullptr;bool GraphChanged=false,Created=false,Reused=false;
    explicit FWork(bool CompileRequested,bool SaveRequested,bool Dry)
    {
        Result.bCompileRequested=CompileRequested;Result.bSaveRequested=SaveRequested;
        Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("command"),TEXT("ensure_anim_bp_event_graph_locomotion_driver"));
        Legacy->SetBoolField(TEXT("sample_only"),true);Legacy->SetBoolField(TEXT("dry_run"),Dry);
        for(const TCHAR* Key:{TEXT("source_modified"),TEXT("source_asset_modified"),TEXT("linked_source_modified"),TEXT("linked_source_asset_modified")}) { Legacy->SetBoolField(Key,false); }
        Legacy->SetBoolField(TEXT("compile_requested"),CompileRequested);Legacy->SetBoolField(TEXT("save_requested"),SaveRequested);
        Compile->SetBoolField(TEXT("requested"),CompileRequested);Compile->SetBoolField(TEXT("attempted"),false);
        Save->SetBoolField(TEXT("requested"),SaveRequested);Save->SetBoolField(TEXT("target_saved"),false);
        Legacy->SetObjectField(TEXT("compile_result"),Compile);Legacy->SetArrayField(TEXT("links"),Links);
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
        if(Result.Error.IsEmpty()) { Result.ErrorCode=FString(TEXT("anim_bp_locomotion_driver_"))+Code;Result.Error=Message; }
        Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("error"),Result.Error);return false;
    }
    bool Current(bool CheckTargets=true)
    {
        if(!Result.Error.IsEmpty()) { return false; }
        if(!IsEditorGameThread()||IsPlaySessionActive()) { return Fail(TEXT("editor_changed"),TEXT("Idle Editor game-thread admission changed after callback")); }
        for(const auto& Source:Sources) { if(!Source->Preserved()) { return Fail(TEXT("source_observation_changed"),TEXT("Protected source identity/dirty/file metadata changed; no further writer is dispatched")); } }
        for(const auto& Input:Inputs) { if(!Input->Current()) { return Fail(TEXT("input_identity_changed"),TEXT("Animation/enum input identity changed after callback")); } }
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
        Result.Data->SetArrayField(TEXT("preparation"),Preparation);Legacy->SetArrayField(TEXT("links"),Links);Legacy->SetArrayField(TEXT("errors"),Errors);
        Legacy->SetBoolField(TEXT("target_created"),Created);Legacy->SetBoolField(TEXT("target_reused"),Reused);
        Legacy->SetBoolField(TEXT("target_modified"),Result.bSuccess?GraphChanged:(Created||GraphChanged));Legacy->SetBoolField(TEXT("target_asset_saved"),Result.bSaved);
        if(Target)
        {
            const bool Readable=Target->Current();Legacy->SetStringField(TEXT("target_asset_path"),Target->PackageName);
            Legacy->SetBoolField(TEXT("partial_target_modified"),Created||GraphChanged);Legacy->SetBoolField(TEXT("target_asset_modified"),Created||GraphChanged);
            if(Readable) { Legacy->SetBoolField(TEXT("target_dirty_after"),CastChecked<UPackage>(Target->Package.Get())->IsDirty()); }else { Legacy->SetField(TEXT("target_dirty_after"),MakeShared<FJsonValueNull>()); }
        }
        Legacy->SetBoolField(TEXT("success"),Result.bSuccess);Legacy->SetBoolField(TEXT("original_assets_modified"),!Preserved);return Result;
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

struct FGraphContext
{
    FWork& Work;UAnimBlueprint* Blueprint;UEdGraph* Graph;FIdentity Identity;TArray<TUniquePtr<FIdentity>> Anchors,Members;
    TSet<const UEdGraphPin*> Pins;TStrongObjectPtr<UEdGraphSchema> Schema;TUniquePtr<FIdentity> SchemaIdentity;
    FGraphContext(FWork& InWork,UAnimBlueprint* InBlueprint,UEdGraph* InGraph):Work(InWork),Blueprint(InBlueprint),Graph(InGraph),Identity(InGraph) {}
    bool Refresh()
    {
        if(!Work.Current()||!Identity.Current()||Graph->GetTypedOuter<UBlueprint>()!=Blueprint||Graph->GetPackage()!=Blueprint->GetPackage()) { return Work.Fail(TEXT("graph_identity_changed"),TEXT("Selected graph owner/actual package/class identity changed")); }
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
        if(!Work.Current()||!Identity.Current()||!Live(CurrentSchema)||!CurrentSchema->IsA(UEdGraphSchema_K2::StaticClass())) { return Work.Fail(TEXT("schema_unavailable"),TEXT("Selected graph has no live K2 schema")); }
        if(Schema.Get()&&(Schema.Get()!=CurrentSchema||!SchemaIdentity->Current())) { return Work.Fail(TEXT("schema_changed"),TEXT("Graph schema identity changed after callback")); }
        Schema.Reset(const_cast<UEdGraphSchema*>(CurrentSchema));if(!SchemaIdentity) { SchemaIdentity=MakeUnique<FIdentity>(Schema.Get()); }
        for(const auto& Id:Members) { if(!Id->Current()) { return Work.Fail(TEXT("node_identity_changed"),TEXT("Graph member identity changed during schema observation")); } }
        if(Graph->Nodes!=NodeSlots) { return Work.Fail(TEXT("node_slots_changed"),TEXT("Graph node slots changed during schema callback")); }
        for(const auto& Pair:PinSlots) { if(Pair.Key->Pins!=Pair.Value) { return Work.Fail(TEXT("pin_slots_changed"),TEXT("Node pin slots changed during schema callback; stale pin inventory is discarded")); } }return true;
    }
    bool PinCurrent(const UEdGraphPin* Pin) const
    { return Pin&&Pins.Contains(Pin)&&!Pin->WasTrashed()&&Live(Pin->GetOwningNodeUnchecked())&&Graph->Nodes.Contains(Pin->GetOwningNodeUnchecked())&&Pin->GetOwningNodeUnchecked()->Pins.Contains(const_cast<UEdGraphPin*>(Pin)); }
    void Anchor(UEdGraphNode* Node) { Anchors.Add(MakeUnique<FIdentity>(Node)); }
};
TSharedRef<FJsonObject> PinJson(const UEdGraphPin* Pin,FGraphContext& Context)
{
    auto Row=MakeShared<FJsonObject>();if(!Context.PinCurrent(Pin)) { return Row; }Row->SetStringField(TEXT("name"),Pin->PinName.ToString());Row->SetStringField(TEXT("direction"),Pin->Direction==EGPD_Output?TEXT("output"):TEXT("input"));
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

template<class T> T* ConfiguredNode(FGraphContext& Context,int32 X,int32 Y,TFunction<void(T*)> Configure)
{
    FWork& Work=Context.Work;if(!Context.Refresh()) { return nullptr; }UClass* Class=T::StaticClass();
    if(!Live(Class)||!Live(Class->ClassWithin)||!Context.Graph->IsA(Class->ClassWithin)) { Work.Fail(TEXT("node_class_invalid"),TEXT("Node ClassWithin rejects graph Outer"));return nullptr; }
    Work.Writer(TEXT("node_create_calls"));T* Node=NewObject<T>(Context.Graph);Work.Count(TEXT("node_create_returns"));if(!Work.Current()||!Live(Node)||Node->GetOuter()!=Context.Graph) { return nullptr; }
    FIdentity Id(Node);Node->NodePosX=X;Node->NodePosY=Y;Configure(Node);if(!Id.Current()||!Context.Refresh()) { return nullptr; }
    Work.Writer(TEXT("graph_add_node_calls"));Context.Graph->AddNode(Node,true);Work.GraphChanged=true;Work.Result.bChanged=true;
    if(!Id.Current()||!Context.Refresh()||!Context.Graph->Nodes.Contains(Node)) { Work.Fail(TEXT("node_attach_failed"),TEXT("Node changed after AddNode callback"));return nullptr; }
    Context.Anchor(Node);Work.Writer(TEXT("node_guid_calls"));Node->CreateNewGuid();
    if(!Context.Refresh()) { return nullptr; }Work.Writer(TEXT("post_placed_calls"));static_cast<UEdGraphNode*>(Node)->PostPlacedNewNode();
    if(!Context.Refresh()) { return nullptr; }Work.Writer(TEXT("allocate_pins_calls"));static_cast<UEdGraphNode*>(Node)->AllocateDefaultPins();
    if(!Context.Refresh()) { return nullptr; }Work.Writer(TEXT("reconstruct_calls"));static_cast<UEdGraphNode*>(Node)->ReconstructNode();Work.Count(TEXT("reconstruct_returns"));return Context.Refresh()?Node:nullptr;
}
// FH-specific helpers. Engine graph work is synchronous; no tool RPC or runtime executor.
FString StringParam(const TSharedPtr<FJsonObject>& Object,const FString& Key,const FString& Default=FString())
{ FString Value;return Object&&Object->TryGetStringField(Key,Value)?Value:Default; }
bool PrivateContext(const FString& Text,TSharedPtr<FJsonObject>& Params,FWork& Work)
{
    TSharedPtr<FJsonValue> Value;const TSharedPtr<FJsonObject>* Object=nullptr;
    if(!Decode(Text,Value)||!Value->TryGetObject(Object)||!Object||!Object->IsValid()) { return Work.Fail(TEXT("private_context_invalid"),TEXT("PrivateContextJson requires a bounded JSON object")); }
    Params=*Object;for(const auto& Pair:Params->Values) { if(FString(Pair.Key.ToView())!=TEXT("target_blueprint_path")) { return Work.Fail(TEXT("private_context_key"),TEXT("Private context only permits target_blueprint_path")); } }return true;
}
FString VariableType(const FProperty* Property)
{ if(CastField<FBoolProperty>(Property)) { return TEXT("bool"); }if(CastField<FFloatProperty>(Property)||CastField<FDoubleProperty>(Property)) { return TEXT("float"); }return Property?TEXT("unsupported:")+Property->GetClass()->GetName():FString(); }
bool ExistingType(UAnimBlueprint* Blueprint,const FString& Name,FString& Out,FWork& Work)
{
    if(!Work.Current()) { return false; }if(!TextSafe(Name,NAME_SIZE-1)||Blueprint->NewVariables.Num()>4096) { return Work.Fail(TEXT("variable_inventory_bound"),TEXT("Variable inventory/name cannot be verified; absence is not established")); }
    for(UClass* Class:{Blueprint->GeneratedClass.Get(),Blueprint->SkeletonGeneratedClass.Get(),Blueprint->ParentClass.Get()})
    {
        TSet<UClass*> Seen;for(UClass* Parent=Class;Parent;Parent=Parent->GetSuperClass()) { if(!Live(Parent)||Seen.Contains(Parent)||Seen.Num()>=64) { return Work.Fail(TEXT("variable_class_unreadable"),TEXT("Variable class hierarchy is unreadable, cyclic or deeper than64")); }Seen.Add(Parent); }
        if(Class) { if(const FProperty* Property=FindFProperty<FProperty>(Class,FName(*Name))) { Out=VariableType(Property);return true; } }
    }
    for(const auto& Variable:Blueprint->NewVariables)
    { if(Variable.VarName==FName(*Name)) { const auto Category=Variable.VarType.PinCategory;Out=Category==UEdGraphSchema_K2::PC_Boolean?TEXT("bool"):(Category==UEdGraphSchema_K2::PC_Real||Category==UEdGraphSchema_K2::PC_Float)?TEXT("float"):TEXT("unsupported:")+Category.ToString();return true; } }return false;
}
bool EnsureVariable(UAnimBlueprint* Blueprint,const FString& Name,bool Bool,bool Create,TSharedRef<FJsonObject> Row,FWork& Work)
{
    const FString Expected=Bool?TEXT("bool"):TEXT("float");Row->SetStringField(TEXT("variable_name"),Name);Row->SetStringField(TEXT("expected_type"),Expected);Row->SetBoolField(TEXT("create_if_missing"),Create);Row->SetBoolField(TEXT("created"),false);Row->SetBoolField(TEXT("existed"),false);
    FString Existing;if(ExistingType(Blueprint,Name,Existing,Work))
    { Row->SetBoolField(TEXT("existed"),true);Row->SetStringField(TEXT("existing_type"),Existing);if(Existing==Expected) { return true; }Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("AnimBP variable '")+Name+TEXT("' exists with type '")+Existing+TEXT("', expected ")+Expected));return false; }
    if(!Work.Current()) { return false; }
    if(!Create) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("AnimBP variable does not exist; enable its create_variable_if_missing flag: ")+Name));return false; }
    FEdGraphPinType Type;Type.PinCategory=Bool?UEdGraphSchema_K2::PC_Boolean:UEdGraphSchema_K2::PC_Real;if(!Bool) { Type.PinSubCategory=UEdGraphSchema_K2::PC_Float; }
    const FName VariableName(*Name);const FString Default=Bool?TEXT("false"):TEXT("0.0");
    if(Blueprint->NewVariables.Num()>=4096) { return Work.Fail(TEXT("variable_inventory_bound"),TEXT("Adding a member would exceed4096 variable descriptors")); }
    Work.Writer(TEXT("add_member_variable_calls"));const bool Added=FBlueprintEditorUtils::AddMemberVariable(Blueprint,VariableName,Type,Default);Work.Count(TEXT("add_member_variable_returns"));Work.GraphChanged|=Added;Work.Result.bChanged|=Added;Row->SetBoolField(TEXT("created"),Added);
    if(!Work.Current()) { return false; }if(!Added) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Failed to add AnimBP variable: ")+Name));return false; }
    const FText Category=FText::FromString(TEXT("MCP Locomotion EventGraph"));for(auto& Description:Blueprint->NewVariables) { if(Description.VarName==VariableName) { Description.Category=Category;break; } }
    Work.Writer(TEXT("set_variable_category_calls"));FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint,VariableName,nullptr,Category,true);
    if(!Work.Current()) { return false; }Row->SetStringField(TEXT("created_type"),Expected);Row->SetStringField(TEXT("default_value"),Default);return true;
}
UEdGraphPin* FindNamedPin(FGraphContext& Context,UEdGraphNode* Node,const FString& Name,EEdGraphPinDirection Direction)
{
    if(!Node||!Context.Refresh()||!Context.Graph->Nodes.Contains(Node)) { return nullptr; }for(UEdGraphPin* Pin:Node->Pins) { if(Context.PinCurrent(Pin)&&Pin->Direction==Direction&&Pin->PinName.ToString().Equals(Name,ESearchCase::IgnoreCase)) { return Pin; } }return nullptr;
}
UEdGraphPin* FindDataOutput(FGraphContext& Context,UEdGraphNode* Node)
{ if(!Node||!Context.Refresh()||!Context.Graph->Nodes.Contains(Node)) { return nullptr; }for(UEdGraphPin* Pin:Node->Pins) { if(Context.PinCurrent(Pin)&&Pin->Direction==EGPD_Output&&Pin->PinType.PinCategory!=UEdGraphSchema_K2::PC_Exec) { return Pin; } }return nullptr; }
bool EnsureK2Link(FGraphContext& Context,UEdGraphPin* SourcePin,UEdGraphPin* TargetPin,const FString& Label,bool Replace,TArray<TSharedPtr<FJsonValue>>& Links,TArray<TSharedPtr<FJsonValue>>& Errors,bool& Changed)
{
    auto Row=MakeShared<FJsonObject>();Links.Add(MakeShared<FJsonValueObject>(Row));Context.Work.Legacy->SetArrayField(TEXT("links"),Links);
    Row->SetStringField(TEXT("label"),Label);Row->SetBoolField(TEXT("success"),false);Row->SetBoolField(TEXT("created"),false);Row->SetBoolField(TEXT("already_connected"),false);Row->SetBoolField(TEXT("replaced_existing_links"),false);
    if(!Context.Refresh()) { return false; }
    auto Error=[&](const FString& Message){Row->SetStringField(TEXT("error"),Message);Errors.Add(MakeShared<FJsonValueString>(Message));return false;};
    if(!Context.PinCurrent(SourcePin)||!Context.PinCurrent(TargetPin)) { return Error(TEXT("Cannot create K2 link: missing graph or pin for ")+Label); }
    Row->SetStringField(TEXT("source_node_id"),SourcePin->GetOwningNodeUnchecked()->NodeGuid.ToString());Row->SetStringField(TEXT("source_pin"),SourcePin->PinName.ToString());Row->SetStringField(TEXT("source_pin_type"),SourcePin->PinType.PinCategory.ToString());Row->SetStringField(TEXT("target_node_id"),TargetPin->GetOwningNodeUnchecked()->NodeGuid.ToString());Row->SetStringField(TEXT("target_pin"),TargetPin->PinName.ToString());Row->SetStringField(TEXT("target_pin_type"),TargetPin->PinType.PinCategory.ToString());
    if(TargetPin->LinkedTo.Contains(SourcePin)) { Row->SetBoolField(TEXT("success"),true);Row->SetBoolField(TEXT("already_connected"),true);return true; }
    const bool Exec=SourcePin->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec||TargetPin->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec;bool Replaced=false;
    auto Break=[&](UEdGraphPin* Pin)
    { Context.Work.Writer(TEXT("break_all_pin_links_calls"));Pin->BreakAllPinLinks();Changed=true;Replaced=true;Context.Work.GraphChanged=true;Context.Work.Result.bChanged=true;if(!Context.Refresh()||!Context.PinCurrent(SourcePin)||!Context.PinCurrent(TargetPin)) { return Context.Work.Fail(TEXT("link_pin_changed"),TEXT("Pin changed after break callback; no later link writer is admitted")); }return true; };
    if(!TargetPin->LinkedTo.IsEmpty()) { if(!Replace) { return Error(TEXT("Target pin already has links; enable replace_existing_links for ")+Label); }if(!Break(TargetPin)) { return false; } }
    if(Exec&&!SourcePin->LinkedTo.IsEmpty()) { if(!Replace) { return Error(TEXT("Source exec pin already has links; enable replace_existing_links for ")+Label); }if(!Break(SourcePin)) { return false; } }
    Context.Work.Writer(TEXT("try_create_connection_calls"));const bool Connected=Context.Schema->TryCreateConnection(SourcePin,TargetPin);Context.Work.Count(TEXT("try_create_connection_returns"));
    if(!Context.Refresh()||!Context.PinCurrent(SourcePin)||!Context.PinCurrent(TargetPin)) { return Context.Work.Fail(TEXT("link_pin_changed"),TEXT("Pin changed after schema connection callback")); }
    if(!Connected) { return Error(TEXT("Failed to create K2 link: ")+Label); }Changed=true;Context.Work.GraphChanged=true;Context.Work.Result.bChanged=true;Row->SetBoolField(TEXT("success"),true);Row->SetBoolField(TEXT("created"),true);Row->SetBoolField(TEXT("replaced_existing_links"),Replaced);return true;
}
bool SetThreshold(FGraphContext& Context,UK2Node_CallFunction* Node,UEdGraphPin* Pin,double Value)
{
    if(!Context.Refresh()||!Context.PinCurrent(Pin)||!Live(Node)) { Context.Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Failed to resolve IsAccelerating threshold pin")));return false; }
    if(!Pin->LinkedTo.IsEmpty()) { return true; }const FString Requested=FString::SanitizeFloat(Value);if(Pin->DefaultValue==Requested) { return true; }
    Context.Work.Writer(TEXT("set_threshold_default_calls"));Context.Schema->TrySetDefaultValue(*Pin,Requested);
    if(!Context.Refresh()||!Context.PinCurrent(Pin)) { return Context.Work.Fail(TEXT("threshold_pin_changed"),TEXT("Threshold pin changed after schema default callback")); }
    Context.Work.Writer(TEXT("pin_default_changed_calls"));Node->PinDefaultValueChanged(Pin);Context.Work.GraphChanged=true;Context.Work.Result.bChanged=true;
    if(!Context.Refresh()||!Context.PinCurrent(Pin)) { return Context.Work.Fail(TEXT("threshold_pin_changed"),TEXT("Threshold pin changed after notification")); }return true;
}
} // namespace

Variable::FResult Ensure(const FString& InSourceAnimBPPath,const FString& InTargetAnimBPPath,const FString& InTargetRoot,const FString& InVariantName,const FString& InSpeedVariable,bool bCreateVariableIfMissing,bool bIncludeDirection,const FString& InDirectionVariable,bool bCreateDirectionVariableIfMissing,bool bIncludeIsInAir,const FString& InIsInAirVariable,bool bCreateIsInAirVariableIfMissing,bool bIncludeAcceleration,const FString& InAccelerationVariable,bool bCreateAccelerationVariableIfMissing,bool bIncludeIsAccelerating,const FString& InIsAcceleratingVariable,bool bCreateIsAcceleratingVariableIfMissing,double InIsAcceleratingThreshold,bool bGuardPawnOwner,bool bReplaceExistingLinks,bool bAllowExistingTargetReuse,bool bOverwriteExisting,bool bCompile,bool bSaveAsset,bool bDryRun,bool bValidateOnly,const FString& InPrivateContextJson)
{
    const bool Dry=bDryRun||bValidateOnly,bSave=bSaveAsset;FWork Work(bCompile,bSave,Dry);auto Legacy=Work.Legacy;
    if(Running) { Work.Fail(TEXT("busy"),TEXT("A locomotion-driver operation is active; no retry"));return Work.Finish(); }Running=true;ON_SCOPE_EXIT { Running=false; };
    if(!IsEditorGameThread()||IsPlaySessionActive()) { Work.Fail(TEXT("editor_not_idle"),TEXT("Idle Editor game thread required"));return Work.Finish(); }
    TSharedPtr<FJsonObject> Params;if(!PrivateContext(InPrivateContextJson,Params,Work)) { return Work.Finish(); }
    for(const FString* Text:{&InSourceAnimBPPath,&InTargetAnimBPPath,&InTargetRoot,&InVariantName,&InSpeedVariable,&InDirectionVariable,&InIsInAirVariable,&InAccelerationVariable,&InIsAcceleratingVariable}) { if(!TextSafe(*Text,NAME_SIZE-1)) { Work.Fail(TEXT("scalar_bound"),TEXT("Scalar name/path exceeds FName/no-NUL bounds"));return Work.Finish(); } }
    if(!FMath::IsFinite(InIsAcceleratingThreshold)) { Work.Fail(TEXT("threshold_invalid"),TEXT("is_accelerating_threshold must be finite"));return Work.Finish(); }
    const FString Source=InSourceAnimBPPath.TrimStartAndEnd();if(Source.IsEmpty()) { Work.Fail(TEXT("source_missing"),TEXT("Missing 'source_anim_bp_path' parameter"));return Work.Finish(); }
    Work.Result.Stage=TEXT("load_source_anim_blueprint");UAnimBlueprint* SourceAnimBlueprint=Cast<UAnimBlueprint>(FindBlueprint(Source,Work));
    if(!Work.Current()||!Live(SourceAnimBlueprint)||!Live(SourceAnimBlueprint->GetPackage())||!ObjectAllowed(SourceAnimBlueprint,Work)) { Work.Fail(TEXT("source_not_found"),TEXT("Source Anim Blueprint not found: ")+Source);return Work.Finish(); }
    Work.Sources.Add(MakeUnique<FIdentity>(SourceAnimBlueprint,true));if(SourceAnimBlueprint->TargetSkeleton) { if(!Live(SourceAnimBlueprint->TargetSkeleton)) { Work.Fail(TEXT("source_skeleton_unreadable"),TEXT("Source TargetSkeleton is not loaded"));return Work.Finish(); }Work.CommonSkeleton=MakeUnique<FIdentity>(SourceAnimBlueprint->TargetSkeleton); }
    Legacy->SetStringField(TEXT("source_anim_bp_path"),SourceAnimBlueprint->GetPathName());Legacy->SetStringField(TEXT("source_package"),SourceAnimBlueprint->GetPackage()->GetName());
    if(!RuntimePackageGuard(SourceAnimBlueprint,Work,TEXT("source_dependency_projection"))) { return Work.Finish(); }
    const FString SpeedVariable=InSpeedVariable.TrimStartAndEnd(),DirectionVariable=InDirectionVariable.TrimStartAndEnd(),IsInAirVariable=InIsInAirVariable.TrimStartAndEnd(),AccelerationVariable=InAccelerationVariable.TrimStartAndEnd(),IsAcceleratingVariable=InIsAcceleratingVariable.TrimStartAndEnd();
    if(SpeedVariable.IsEmpty()||(bIncludeDirection&&DirectionVariable.IsEmpty())||(bIncludeIsInAir&&IsInAirVariable.IsEmpty())||(bIncludeAcceleration&&AccelerationVariable.IsEmpty())||(bIncludeIsAccelerating&&IsAcceleratingVariable.IsEmpty())) { Work.Fail(TEXT("variable_name_empty"),TEXT("An enabled locomotion variable name is empty"));return Work.Finish(); }
    const double IsAcceleratingThreshold=FMath::Clamp(InIsAcceleratingThreshold,0.0,1000000.0);const FString TargetRoot=NormalizePackage(InTargetRoot),VariantName=VariantSegment(InVariantName);
    FString TargetAnimBPPath=NormalizePackage(InTargetAnimBPPath);if(TargetAnimBPPath.IsEmpty()) { TargetAnimBPPath=NormalizePackage(StringParam(Params,TEXT("target_blueprint_path"))); }
    if(TargetAnimBPPath.IsEmpty()) { TargetAnimBPPath=TargetRoot+TEXT("/")+VariantSegment(SourceAnimBlueprint->GetName())+TEXT("_")+VariantName; }
    Legacy->SetStringField(TEXT("target_anim_bp_path"),TargetAnimBPPath);Legacy->SetStringField(TEXT("target_root"),TargetRoot);Legacy->SetStringField(TEXT("variant_name"),VariantName);
    Legacy->SetStringField(TEXT("speed_variable"),SpeedVariable);Legacy->SetStringField(TEXT("direction_variable"),DirectionVariable);Legacy->SetStringField(TEXT("is_in_air_variable"),IsInAirVariable);Legacy->SetStringField(TEXT("acceleration_variable"),AccelerationVariable);Legacy->SetStringField(TEXT("is_accelerating_variable"),IsAcceleratingVariable);
    Legacy->SetBoolField(TEXT("create_variable_if_missing"),bCreateVariableIfMissing);Legacy->SetBoolField(TEXT("include_direction"),bIncludeDirection);Legacy->SetBoolField(TEXT("create_direction_variable_if_missing"),bCreateDirectionVariableIfMissing);Legacy->SetBoolField(TEXT("include_is_in_air"),bIncludeIsInAir);Legacy->SetBoolField(TEXT("create_is_in_air_variable_if_missing"),bCreateIsInAirVariableIfMissing);Legacy->SetBoolField(TEXT("include_acceleration"),bIncludeAcceleration);Legacy->SetBoolField(TEXT("create_acceleration_variable_if_missing"),bCreateAccelerationVariableIfMissing);Legacy->SetBoolField(TEXT("include_is_accelerating"),bIncludeIsAccelerating);Legacy->SetBoolField(TEXT("create_is_accelerating_variable_if_missing"),bCreateIsAcceleratingVariableIfMissing);Legacy->SetNumberField(TEXT("is_accelerating_threshold"),IsAcceleratingThreshold);Legacy->SetBoolField(TEXT("guard_pawn_owner"),bGuardPawnOwner);Legacy->SetBoolField(TEXT("replace_existing_links"),bReplaceExistingLinks);
    if(!ValidGamePackage(TargetRoot)||!ValidGamePackage(TargetAnimBPPath)||!SamplePath(TargetAnimBPPath)||TargetAnimBPPath.Equals(SourceAnimBlueprint->GetPackage()->GetName(),ESearchCase::IgnoreCase)) { Work.Fail(TEXT("target_scope_invalid"),TEXT("Target must be a distinct valid sample /Game package"));return Work.Finish(); }
    if(Dry) { Work.Result.Stage=TEXT("dry_run_complete");Work.Result.bSuccess=true;Work.Result.bPostconditionPassed=true;Legacy->SetStringField(TEXT("runtime_note"),TEXT("Dry run: no explicit duplicate/graph/compiler/save; SDK source-load effects remain unknown"));return Work.Finish(); }
    Work.Result.Stage=TEXT("prepare_target_anim_blueprint");Work.TargetPackages.Add(TargetAnimBPPath);FTargetPlan Plan(TargetAnimBPPath,TEXT("AnimBlueprint"),UAnimBlueprint::StaticClass());
    if(!InspectTarget(Plan,Work)) { return Work.Finish(); }if(Plan.Exists&&!bOverwriteExisting&&!bAllowExistingTargetReuse) { Work.Fail(TEXT("target_exists"),TEXT("Target exists and reuse is disabled"));return Work.Finish(); }
    UAnimBlueprint* TargetAnimBlueprint=Cast<UAnimBlueprint>(PrepareTarget(SourceAnimBlueprint,Plan,bOverwriteExisting,Work));if(!Work.Current()||!TargetAnimBlueprint||!Work.Errors.IsEmpty()) { Work.Fail(TEXT("target_prepare_failed"),TEXT("Failed to prepare target sample AnimBP"));return Work.Finish(); }
    Work.Target=Work.AddTarget(TargetAnimBlueprint,TargetAnimBPPath);if(!Work.Target) { return Work.Finish(); }Plan.Receipt->TryGetBoolField(TEXT("created"),Work.Created);Plan.Receipt->TryGetBoolField(TEXT("reused"),Work.Reused);Work.TargetSkeletonLocked=Work.CommonSkeleton.IsValid();
    Legacy->SetBoolField(TEXT("target_exists_before"),Plan.Exists);Legacy->SetStringField(TEXT("target_class"),TargetAnimBlueprint->GetClass()->GetPathName());
    if(!RuntimePackageGuard(TargetAnimBlueprint,Work,TEXT("target_dependency_projection"))) { return Work.Finish(); }
    auto& ErrorValues=Work.Errors;TArray<TSharedPtr<FJsonValue>> WarningValues,VariableValues;bool& bGraphChanged=Work.GraphChanged;
    auto MakeEventGraphPartialError=[&](const FString& Message,bool Changed)->FResult { Work.GraphChanged|=Changed;Work.Fail(TEXT("authoring_failed"),Message);return Work.Finish(); };
    Work.Result.Stage=TEXT("ensure_locomotion_variables");
    struct FRequest { const FString* Name;bool Enabled,Bool,Create; };
    const FRequest Requests[]={{&SpeedVariable,true,false,bCreateVariableIfMissing},{&DirectionVariable,bIncludeDirection,false,bCreateDirectionVariableIfMissing},{&IsInAirVariable,bIncludeIsInAir,true,bCreateIsInAirVariableIfMissing},{&AccelerationVariable,bIncludeAcceleration,false,bCreateAccelerationVariableIfMissing},{&IsAcceleratingVariable,bIncludeIsAccelerating,true,bCreateIsAcceleratingVariableIfMissing}};
    for(const auto& Request:Requests)
    {
        if(!Request.Enabled) { continue; }auto Row=MakeShared<FJsonObject>();VariableValues.Add(MakeShared<FJsonValueObject>(Row));Legacy->SetArrayField(TEXT("variables"),VariableValues);
        if(!EnsureVariable(TargetAnimBlueprint,*Request.Name,Request.Bool,Request.Create,Row,Work)) { return MakeEventGraphPartialError(TEXT("Invalid AnimBP locomotion EventGraph variables"),bGraphChanged); }
    }
    Work.Result.Stage=TEXT("resolve_or_create_event_graph");TArray<UEdGraph*> AllGraphs;if(!Graphs(TargetAnimBlueprint,AllGraphs,Work)) { return Work.Finish(); }UEdGraph* EventGraph=nullptr;
    for(UEdGraph* Graph:TargetAnimBlueprint->UbergraphPages) { if(Live(Graph)&&Graph->GetName().Contains(TEXT("EventGraph"))) { EventGraph=Graph;break; } }
    if(!EventGraph)
    {
        Work.Writer(TEXT("create_event_graph_calls"));EventGraph=FBlueprintEditorUtils::CreateNewGraph(TargetAnimBlueprint,FName(TEXT("EventGraph")),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
        if(!Work.Current()||!Live(EventGraph)||EventGraph->GetOuter()!=TargetAnimBlueprint) { return MakeEventGraphPartialError(TEXT("Failed to create EventGraph"),bGraphChanged); }FIdentity Id(EventGraph);
        Work.Writer(TEXT("add_ubergraph_page_calls"));FBlueprintEditorUtils::AddUbergraphPage(TargetAnimBlueprint,EventGraph);Work.Result.bChanged=true;Work.GraphChanged=true;
        if(!Work.Current()||!Id.Current()) { return MakeEventGraphPartialError(TEXT("EventGraph changed after attachment"),bGraphChanged); }
    }
    const FString EventGraphName=EventGraph->GetName();FGraphContext Context(Work,TargetAnimBlueprint,EventGraph);if(!Context.Refresh()) { return Work.Finish(); }
    auto ResolveFunction=[&](UClass* Class,FName Name)->UFunction*
    {
        if(!Work.Current()||!Live(Class)||!ClassAllowed(Class,Work)) { return nullptr; }FIdentity Id(Class);Work.SDK(TEXT("resolve_function_calls"));UFunction* Function=Class->FindFunctionByName(Name);
        if(!Work.Current()||!Id.Current()||!Live(Function)) { return nullptr; }Work.Inputs.Add(MakeUnique<FIdentity>(Function));return Function;
    };
    Work.Result.Stage=TEXT("resolve_engine_functions");
    UFunction* UpdateFunction=ResolveFunction(UAnimInstance::StaticClass(),GET_FUNCTION_NAME_CHECKED(UAnimInstance,BlueprintUpdateAnimation));
    UFunction* TryGetPawnOwnerFunction=ResolveFunction(UAnimInstance::StaticClass(),GET_FUNCTION_NAME_CHECKED(UAnimInstance,TryGetPawnOwner));
    UFunction* GetVelocityFunction=ResolveFunction(AActor::StaticClass(),GET_FUNCTION_NAME_CHECKED(AActor,GetVelocity));
    UFunction* VSizeXYFunction=ResolveFunction(UKismetMathLibrary::StaticClass(),GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary,VSizeXY));
    UFunction* IsValidFunction=bGuardPawnOwner?ResolveFunction(UKismetSystemLibrary::StaticClass(),GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary,IsValid)):nullptr;
    UFunction* GetActorRotationFunction=bIncludeDirection?ResolveFunction(AActor::StaticClass(),GET_FUNCTION_NAME_CHECKED(AActor,K2_GetActorRotation)):nullptr;
    UFunction* CalculateDirectionFunction=bIncludeDirection?ResolveFunction(UKismetAnimationLibrary::StaticClass(),GET_FUNCTION_NAME_CHECKED(UKismetAnimationLibrary,CalculateDirection)):nullptr;
    const bool bNeedsMovementComponent=bIncludeIsInAir||bIncludeAcceleration||bIncludeIsAccelerating,bNeedsCharacterMovement=bIncludeAcceleration||bIncludeIsAccelerating;
    UFunction* GetMovementComponentFunction=bNeedsMovementComponent?ResolveFunction(APawn::StaticClass(),GET_FUNCTION_NAME_CHECKED(APawn,GetMovementComponent)):nullptr;
    UFunction* IsFallingFunction=bIncludeIsInAir?ResolveFunction(UNavMovementComponent::StaticClass(),GET_FUNCTION_NAME_CHECKED(UNavMovementComponent,IsFalling)):nullptr;
    UFunction* GetCurrentAccelerationFunction=bNeedsCharacterMovement?ResolveFunction(UCharacterMovementComponent::StaticClass(),GET_FUNCTION_NAME_CHECKED(UCharacterMovementComponent,GetCurrentAcceleration)):nullptr;
    UFunction* IsAcceleratingGreaterFunction=bIncludeIsAccelerating?ResolveFunction(UKismetMathLibrary::StaticClass(),GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary,Greater_DoubleDouble)):nullptr;
    if(!UpdateFunction||!TryGetPawnOwnerFunction||!GetVelocityFunction||!VSizeXYFunction||(bGuardPawnOwner&&!IsValidFunction)||(bIncludeDirection&&(!GetActorRotationFunction||!CalculateDirectionFunction))||(bNeedsMovementComponent&&!GetMovementComponentFunction)||(bIncludeIsInAir&&!IsFallingFunction)||(bNeedsCharacterMovement&&!GetCurrentAccelerationFunction)||(bIncludeIsAccelerating&&!IsAcceleratingGreaterFunction)) { return MakeEventGraphPartialError(TEXT("Failed to resolve required engine functions"),bGraphChanged); }
    auto FindPin=[&](UEdGraphNode* Node,const FString& Name,EEdGraphPinDirection Direction) { return FindNamedPin(Context,Node,Name,Direction); };
    auto FindInputPinByNames=[&](UEdGraphNode* Node,std::initializer_list<const TCHAR*> Names)->UEdGraphPin* { for(const TCHAR* Name:Names) { if(auto* Pin=FindPin(Node,Name,EGPD_Input)) { return Pin; } }return nullptr; };
    auto FindOutputPinByNames=[&](UEdGraphNode* Node,std::initializer_list<const TCHAR*> Names)->UEdGraphPin* { for(const TCHAR* Name:Names) { if(auto* Pin=FindPin(Node,Name,EGPD_Output)) { return Pin; } }return nullptr; };
    auto FindFirstDataOutputPin=[&](UEdGraphNode* Node) { return FindDataOutput(Context,Node); };
    auto FindFirstPinByCategory=[&](UEdGraphNode* Node,EEdGraphPinDirection Direction,FName Category)->UEdGraphPin* { if(!Node||!Context.Refresh()||!EventGraph->Nodes.Contains(Node)) { return nullptr; }for(UEdGraphPin* Pin:Node->Pins) { if(Context.PinCurrent(Pin)&&Pin->Direction==Direction&&Pin->PinType.PinCategory==Category) { return Pin; } }return nullptr; };
    auto FindEventNodeByFunction=[&](UEdGraph*,UFunction* Function)->UK2Node_Event* { if(!Function||!Context.Refresh()) { return nullptr; }for(UEdGraphNode* Node:EventGraph->Nodes) { if(auto* Event=Cast<UK2Node_Event>(Node);Event&&Event->EventReference.GetMemberName()==Function->GetFName()) { Context.Anchor(Event);return Event; } }return nullptr; };
    auto FindFunctionCallNodeByFunction=[&](UEdGraph*,UFunction* Function)->UK2Node_CallFunction*
    {
        if(!Function||!Context.Refresh()) { return nullptr; }const auto Slots=EventGraph->Nodes;
        for(UEdGraphNode* Node:Slots) { if(auto* Call=Cast<UK2Node_CallFunction>(Node)) { Work.SDK(TEXT("target_function_queries"));UFunction* Target=Call->GetTargetFunction();if(!Context.Refresh()) { return nullptr; }if(Target==Function||Call->FunctionReference.GetMemberName()==Function->GetFName()) { Context.Anchor(Call);return Call; } } }return nullptr;
    };
    auto FindFunctionCallNodeByFunctionWithInputSource=[&](UEdGraph*,UFunction* Function,UEdGraphPin* SourcePin)->UK2Node_CallFunction*
    {
        if(!Function||!Context.Refresh()||!Context.PinCurrent(SourcePin)) { return nullptr; }const auto Slots=EventGraph->Nodes;
        for(UEdGraphNode* Node:Slots) { if(auto* Call=Cast<UK2Node_CallFunction>(Node)) { Work.SDK(TEXT("target_function_queries"));UFunction* Target=Call->GetTargetFunction();if(!Context.Refresh()||!Context.PinCurrent(SourcePin)) { return nullptr; }if(Target!=Function&&Call->FunctionReference.GetMemberName()!=Function->GetFName()) { continue; }for(UEdGraphPin* Pin:Call->Pins) { if(Context.PinCurrent(Pin)&&Pin->Direction==EGPD_Input&&Pin->LinkedTo.Contains(SourcePin)) { Context.Anchor(Call);return Call; } } } }return nullptr;
    };
    auto CreateFunctionCallNode=[&](UEdGraph*,UFunction* Function,const FVector2D& Position)->UK2Node_CallFunction*
    { if(!Live(Function)||!Context.Refresh()) { return nullptr; }return ConfiguredNode<UK2Node_CallFunction>(Context,int32(Position.X),int32(Position.Y),[&](UK2Node_CallFunction* Node){Node->SetFromFunction(Function);}); };
    auto CreateEventNode=[&](UEdGraph*,const FString& Name,const FVector2D& Position)->UK2Node_Event*
    {
        if(!Context.Refresh()||!TextSafe(Name,NAME_SIZE-1)) { return nullptr; }const FName EventName(*Name);UFunction* Function=nullptr;
        if(TargetAnimBlueprint->ParentClass) { Function=ResolveFunction(TargetAnimBlueprint->ParentClass,EventName); }
        if(!Function&&Work.Current()) { bool Invalid=false;Work.SDK(TEXT("interface_event_function_queries"));Function=FBlueprintEditorUtils::FindFunctionInImplementedInterfaces(TargetAnimBlueprint,EventName,&Invalid,true);if(Live(Function)) { Work.Inputs.Add(MakeUnique<FIdentity>(Function)); } }
        if(!Context.Refresh()) { return nullptr; }if(!Function&&TargetAnimBlueprint->GeneratedClass) { Function=ResolveFunction(TargetAnimBlueprint->GeneratedClass,EventName); }
        if(!Live(Function)||!Context.Refresh()) { return nullptr; }UClass* Owner=Cast<UClass>(Function->GetOuter());if(!Live(Owner)) { return nullptr; }FIdentity OwnerId(Owner);
        Work.SDK(TEXT("authoritative_class_queries"));Owner=Owner->GetAuthoritativeClass();if(!Context.Refresh()||!OwnerId.Current()||!Live(Owner)||!ClassAllowed(Owner,Work)) { return nullptr; }Work.Inputs.Add(MakeUnique<FIdentity>(Owner));
        Work.SDK(TEXT("find_override_calls"));UK2Node_Event* Override=FBlueprintEditorUtils::FindOverrideForFunction(TargetAnimBlueprint,Owner,EventName);
        if(!Context.Refresh()) { return nullptr; }if(Override) { if(!Live(Override)||!EventGraph->Nodes.Contains(Override)) { Work.Fail(TEXT("event_override_foreign"),TEXT("Existing event override is outside the selected EventGraph"));return nullptr; }Context.Anchor(Override);return Override; }
        return ConfiguredNode<UK2Node_Event>(Context,int32(Position.X),int32(Position.Y),[&](UK2Node_Event* Node){Node->EventReference.SetExternalMember(EventName,Owner);Node->bOverrideFunction=true;});
    };
    auto FindVariableSetNodeByName=[&](UEdGraph*,const FString& Name)->UK2Node_VariableSet* { if(!Context.Refresh()) { return nullptr; }for(UEdGraphNode* Node:EventGraph->Nodes) { if(auto* Setter=Cast<UK2Node_VariableSet>(Node);Setter&&Setter->VariableReference.GetMemberName()==FName(*Name)) { Context.Anchor(Setter);return Setter; } }return nullptr; };
    auto CreateSelfVariableSetNode=[&](UEdGraph*,const FString& Name,const FVector2D& Position) { return ConfiguredNode<UK2Node_VariableSet>(Context,int32(Position.X),int32(Position.Y),[&](UK2Node_VariableSet* Node){Node->VariableReference.SetSelfMember(FName(*Name));}); };
    auto FindDynamicCastNodeByTargetClass=[&](UEdGraph*,UClass* Class)->UK2Node_DynamicCast* { if(!Context.Refresh()) { return nullptr; }for(UEdGraphNode* Node:EventGraph->Nodes) { if(auto* CastNode=Cast<UK2Node_DynamicCast>(Node);CastNode&&CastNode->TargetType==Class) { Context.Anchor(CastNode);return CastNode; } }return nullptr; };
    auto CreateDynamicCastNodeForAnimEventGraph=[&](UEdGraph*,UClass* Class,const FVector2D& Position)->UK2Node_DynamicCast* { if(!Live(Class)||!ClassAllowed(Class,Work)) { return nullptr; }auto* Node=ConfiguredNode<UK2Node_DynamicCast>(Context,int32(Position.X),int32(Position.Y),[&](UK2Node_DynamicCast* NewNode){NewNode->TargetType=Class;});if(!Node||!Context.Refresh()) { return nullptr; }Work.Writer(TEXT("set_cast_purity_calls"));Node->SetPurity(false);if(!Context.Refresh()) { return nullptr; }Work.Writer(TEXT("reconstruct_calls"));Node->ReconstructNode();Work.Count(TEXT("reconstruct_returns"));return Context.Refresh()?Node:nullptr; };
    auto CreateBranchNodeForAnimEventGraph=[&](UEdGraph*,const FVector2D& Position) { return ConfiguredNode<UK2Node_IfThenElse>(Context,int32(Position.X),int32(Position.Y),[](UK2Node_IfThenElse*){}); };
    auto FindLocomotionDriverBranchNode=[&](UEdGraph*,UK2Node_CallFunction* ValidNode,UK2Node_VariableSet* Setter)->UK2Node_IfThenElse* { auto* Output=FindFirstDataOutputPin(ValidNode);auto* Execute=FindPin(Setter,UEdGraphSchema_K2::PN_Execute.ToString(),EGPD_Input);if(!Context.Refresh()) { return nullptr; }for(UEdGraphNode* Node:EventGraph->Nodes) { if(auto* Branch=Cast<UK2Node_IfThenElse>(Node)) { auto* Condition=FindInputPinByNames(Branch,{TEXT("Condition"),TEXT("bCondition")});auto* Then=FindOutputPinByNames(Branch,{TEXT("then"),TEXT("True")});if((Condition&&Output&&Context.PinCurrent(Output)&&Condition->LinkedTo.Contains(Output))||(Then&&Execute&&Context.PinCurrent(Execute)&&Then->LinkedTo.Contains(Execute))) { Context.Anchor(Branch);return Branch; } } }return nullptr; };
    auto FindVariableSetValueInputPin=[&](UK2Node_VariableSet* Node,const FString& Name)->UEdGraphPin* { if(auto* Pin=FindPin(Node,Name,EGPD_Input)) { return Pin; }if(!Node||!Context.Refresh()) { return nullptr; }for(UEdGraphPin* Pin:Node->Pins) { if(Context.PinCurrent(Pin)&&Pin->Direction==EGPD_Input&&Pin->PinType.PinCategory!=UEdGraphSchema_K2::PC_Exec) { return Pin; } }return nullptr; };
    auto FindCastResult=[&](UK2Node_DynamicCast* Node)->UEdGraphPin* { if(!Node||!Context.Refresh()) { return nullptr; }for(UEdGraphPin* Pin:Node->Pins) { if(Context.PinCurrent(Pin)&&Pin->Direction==EGPD_Output&&Pin->PinType.PinSubCategoryObject==Node->TargetType.Get()&&Pin->PinName.ToString().StartsWith(UEdGraphSchema_K2::PN_CastedValuePrefix)) { return Pin; } }return nullptr; };
    auto EnsureK2GraphConnection=[&](UEdGraph*,UEdGraphPin* From,UEdGraphPin* To,const FString& Label,bool Replace,TArray<TSharedPtr<FJsonValue>>& Links,TArray<TSharedPtr<FJsonValue>>& Errors,bool& Changed) { return EnsureK2Link(Context,From,To,Label,Replace,Links,Errors,Changed); };
    auto NodeToJson=[&](UEdGraphNode* Node,bool) { return NodeJson(Node,Context); };
    Work.Result.Stage=TEXT("ensure_locomotion_nodes");
    bool bUpdateEventCreated = false;
    UK2Node_Event* UpdateEventNode = FindEventNodeByFunction(EventGraph, UpdateFunction);
    if (!UpdateEventNode)
    {
        UpdateEventNode = CreateEventNode(EventGraph, UpdateFunction->GetName(), FVector2D(-760.0f, -160.0f));
        bUpdateEventCreated = UpdateEventNode != nullptr;
        bGraphChanged = bGraphChanged || bUpdateEventCreated;
    }
    if (!UpdateEventNode)
    {
        return MakeEventGraphPartialError(TEXT("Failed to create Event Blueprint Update Animation node"), bGraphChanged);
    }

    bool bTryGetPawnOwnerCreated = false;
    UK2Node_CallFunction* TryGetPawnOwnerNode = FindFunctionCallNodeByFunction(EventGraph, TryGetPawnOwnerFunction);
    if (!TryGetPawnOwnerNode)
    {
        TryGetPawnOwnerNode = CreateFunctionCallNode(EventGraph, TryGetPawnOwnerFunction, FVector2D(-520.0f, -160.0f));
        bTryGetPawnOwnerCreated = TryGetPawnOwnerNode != nullptr;
        bGraphChanged = bGraphChanged || bTryGetPawnOwnerCreated;
    }

    bool bIsValidCreated = false;
    UK2Node_CallFunction* IsValidNode = nullptr;
    if (bGuardPawnOwner)
    {
        IsValidNode = FindFunctionCallNodeByFunction(EventGraph, IsValidFunction);
        if (!IsValidNode)
        {
            IsValidNode = CreateFunctionCallNode(EventGraph, IsValidFunction, FVector2D(-300.0f, -260.0f));
            bIsValidCreated = IsValidNode != nullptr;
            bGraphChanged = bGraphChanged || bIsValidCreated;
        }
    }

    bool bGetVelocityCreated = false;
    UK2Node_CallFunction* GetVelocityNode = FindFunctionCallNodeByFunction(EventGraph, GetVelocityFunction);
    if (!GetVelocityNode)
    {
        GetVelocityNode = CreateFunctionCallNode(EventGraph, GetVelocityFunction, FVector2D(-300.0f, -20.0f));
        bGetVelocityCreated = GetVelocityNode != nullptr;
        bGraphChanged = bGraphChanged || bGetVelocityCreated;
    }

    bool bVSizeXYCreated = false;
    UK2Node_CallFunction* VSizeXYNode = FindFunctionCallNodeByFunction(EventGraph, VSizeXYFunction);
    if (!VSizeXYNode)
    {
        VSizeXYNode = CreateFunctionCallNode(EventGraph, VSizeXYFunction, FVector2D(-40.0f, -20.0f));
        bVSizeXYCreated = VSizeXYNode != nullptr;
        bGraphChanged = bGraphChanged || bVSizeXYCreated;
    }

    bool bSetSpeedCreated = false;
    UK2Node_VariableSet* SetSpeedNode = FindVariableSetNodeByName(EventGraph, SpeedVariable);
    if (!SetSpeedNode)
    {
        SetSpeedNode = CreateSelfVariableSetNode(EventGraph, SpeedVariable, FVector2D(220.0f, -80.0f));
        bSetSpeedCreated = SetSpeedNode != nullptr;
        bGraphChanged = bGraphChanged || bSetSpeedCreated;
    }

    bool bGetActorRotationCreated = false;
    UK2Node_CallFunction* GetActorRotationNode = nullptr;
    bool bCalculateDirectionCreated = false;
    UK2Node_CallFunction* CalculateDirectionNode = nullptr;
    bool bSetDirectionCreated = false;
    UK2Node_VariableSet* SetDirectionNode = nullptr;
    if (bIncludeDirection)
    {
        GetActorRotationNode = FindFunctionCallNodeByFunction(EventGraph, GetActorRotationFunction);
        if (!GetActorRotationNode)
        {
            GetActorRotationNode = CreateFunctionCallNode(EventGraph, GetActorRotationFunction, FVector2D(-300.0f, 140.0f));
            bGetActorRotationCreated = GetActorRotationNode != nullptr;
            bGraphChanged = bGraphChanged || bGetActorRotationCreated;
        }

        CalculateDirectionNode = FindFunctionCallNodeByFunction(EventGraph, CalculateDirectionFunction);
        if (!CalculateDirectionNode)
        {
            CalculateDirectionNode = CreateFunctionCallNode(EventGraph, CalculateDirectionFunction, FVector2D(-40.0f, 140.0f));
            bCalculateDirectionCreated = CalculateDirectionNode != nullptr;
            bGraphChanged = bGraphChanged || bCalculateDirectionCreated;
        }

        SetDirectionNode = FindVariableSetNodeByName(EventGraph, DirectionVariable);
        if (!SetDirectionNode)
        {
            SetDirectionNode = CreateSelfVariableSetNode(EventGraph, DirectionVariable, FVector2D(480.0f, 80.0f));
            bSetDirectionCreated = SetDirectionNode != nullptr;
            bGraphChanged = bGraphChanged || bSetDirectionCreated;
        }
    }

    bool bGetMovementComponentCreated = false;
    UK2Node_CallFunction* GetMovementComponentNode = nullptr;
    bool bIsFallingCreated = false;
    UK2Node_CallFunction* IsFallingNode = nullptr;
    bool bSetIsInAirCreated = false;
    UK2Node_VariableSet* SetIsInAirNode = nullptr;
    if (bNeedsMovementComponent)
    {
        GetMovementComponentNode = FindFunctionCallNodeByFunction(EventGraph, GetMovementComponentFunction);
        if (!GetMovementComponentNode)
        {
            GetMovementComponentNode = CreateFunctionCallNode(EventGraph, GetMovementComponentFunction, FVector2D(-300.0f, 340.0f));
            bGetMovementComponentCreated = GetMovementComponentNode != nullptr;
            bGraphChanged = bGraphChanged || bGetMovementComponentCreated;
        }
    }

    if (bIncludeIsInAir)
    {
        IsFallingNode = FindFunctionCallNodeByFunction(EventGraph, IsFallingFunction);
        if (!IsFallingNode)
        {
            IsFallingNode = CreateFunctionCallNode(EventGraph, IsFallingFunction, FVector2D(-40.0f, 340.0f));
            bIsFallingCreated = IsFallingNode != nullptr;
            bGraphChanged = bGraphChanged || bIsFallingCreated;
        }

        SetIsInAirNode = FindVariableSetNodeByName(EventGraph, IsInAirVariable);
        if (!SetIsInAirNode)
        {
            SetIsInAirNode = CreateSelfVariableSetNode(EventGraph, IsInAirVariable, FVector2D(480.0f, 300.0f));
            bSetIsInAirCreated = SetIsInAirNode != nullptr;
            bGraphChanged = bGraphChanged || bSetIsInAirCreated;
        }
    }

    bool bCastCharacterMovementCreated = false;
    UK2Node_DynamicCast* CastCharacterMovementNode = nullptr;
    bool bGetCurrentAccelerationCreated = false;
    UK2Node_CallFunction* GetCurrentAccelerationNode = nullptr;
    bool bAccelerationVSizeXYCreated = false;
    UK2Node_CallFunction* AccelerationVSizeXYNode = nullptr;
    bool bIsAcceleratingGreaterCreated = false;
    UK2Node_CallFunction* IsAcceleratingGreaterNode = nullptr;
    bool bSetAccelerationCreated = false;
    UK2Node_VariableSet* SetAccelerationNode = nullptr;
    bool bSetIsAcceleratingCreated = false;
    UK2Node_VariableSet* SetIsAcceleratingNode = nullptr;
    if (bNeedsCharacterMovement)
    {
        CastCharacterMovementNode = FindDynamicCastNodeByTargetClass(EventGraph, UCharacterMovementComponent::StaticClass());
        if (!CastCharacterMovementNode)
        {
            CastCharacterMovementNode = CreateDynamicCastNodeForAnimEventGraph(EventGraph, UCharacterMovementComponent::StaticClass(), FVector2D(220.0f, 500.0f));
            bCastCharacterMovementCreated = CastCharacterMovementNode != nullptr;
            bGraphChanged = bGraphChanged || bCastCharacterMovementCreated;
        }

        GetCurrentAccelerationNode = FindFunctionCallNodeByFunction(EventGraph, GetCurrentAccelerationFunction);
        if (!GetCurrentAccelerationNode)
        {
            GetCurrentAccelerationNode = CreateFunctionCallNode(EventGraph, GetCurrentAccelerationFunction, FVector2D(480.0f, 500.0f));
            bGetCurrentAccelerationCreated = GetCurrentAccelerationNode != nullptr;
            bGraphChanged = bGraphChanged || bGetCurrentAccelerationCreated;
        }

        UEdGraphPin* CurrentAccelerationReturnPinForFind = FindFirstDataOutputPin(GetCurrentAccelerationNode);
        AccelerationVSizeXYNode = FindFunctionCallNodeByFunctionWithInputSource(EventGraph, VSizeXYFunction, CurrentAccelerationReturnPinForFind);
        if (!AccelerationVSizeXYNode)
        {
            AccelerationVSizeXYNode = CreateFunctionCallNode(EventGraph, VSizeXYFunction, FVector2D(740.0f, 500.0f));
            bAccelerationVSizeXYCreated = AccelerationVSizeXYNode != nullptr;
            bGraphChanged = bGraphChanged || bAccelerationVSizeXYCreated;
        }
    }

    if (bIncludeIsAccelerating)
    {
        IsAcceleratingGreaterNode = FindFunctionCallNodeByFunction(EventGraph, IsAcceleratingGreaterFunction);
        if (!IsAcceleratingGreaterNode)
        {
            IsAcceleratingGreaterNode = CreateFunctionCallNode(EventGraph, IsAcceleratingGreaterFunction, FVector2D(1000.0f, 560.0f));
            bIsAcceleratingGreaterCreated = IsAcceleratingGreaterNode != nullptr;
            bGraphChanged = bGraphChanged || bIsAcceleratingGreaterCreated;
        }
    }

    if (bIncludeAcceleration)
    {
        SetAccelerationNode = FindVariableSetNodeByName(EventGraph, AccelerationVariable);
        if (!SetAccelerationNode)
        {
            SetAccelerationNode = CreateSelfVariableSetNode(EventGraph, AccelerationVariable, FVector2D(1260.0f, 460.0f));
            bSetAccelerationCreated = SetAccelerationNode != nullptr;
            bGraphChanged = bGraphChanged || bSetAccelerationCreated;
        }
    }

    if (bIncludeIsAccelerating)
    {
        SetIsAcceleratingNode = FindVariableSetNodeByName(EventGraph, IsAcceleratingVariable);
        if (!SetIsAcceleratingNode)
        {
            SetIsAcceleratingNode = CreateSelfVariableSetNode(EventGraph, IsAcceleratingVariable, FVector2D(1260.0f, 620.0f));
            bSetIsAcceleratingCreated = SetIsAcceleratingNode != nullptr;
            bGraphChanged = bGraphChanged || bSetIsAcceleratingCreated;
        }
    }

    bool bBranchCreated = false;
    UK2Node_IfThenElse* BranchNode = nullptr;
    if (bGuardPawnOwner)
    {
        BranchNode = FindLocomotionDriverBranchNode(EventGraph, IsValidNode, SetSpeedNode);
        if (!BranchNode)
        {
            BranchNode = CreateBranchNodeForAnimEventGraph(EventGraph, FVector2D(-120.0f, -160.0f));
            bBranchCreated = BranchNode != nullptr;
            bGraphChanged = bGraphChanged || bBranchCreated;
        }
    }

    if (!TryGetPawnOwnerNode || !GetVelocityNode || !VSizeXYNode || !SetSpeedNode ||
        (bGuardPawnOwner && (!IsValidNode || !BranchNode)) ||
        (bIncludeDirection && (!GetActorRotationNode || !CalculateDirectionNode || !SetDirectionNode)) ||
        (bNeedsMovementComponent && !GetMovementComponentNode) ||
        (bIncludeIsInAir && (!IsFallingNode || !SetIsInAirNode)) ||
        (bNeedsCharacterMovement && (!CastCharacterMovementNode || !GetCurrentAccelerationNode || !AccelerationVSizeXYNode)) ||
        (bIncludeAcceleration && !SetAccelerationNode) ||
        (bIncludeIsAccelerating && (!IsAcceleratingGreaterNode || !SetIsAcceleratingNode)))
    {
        return MakeEventGraphPartialError(TEXT("Failed to create one or more AnimBP locomotion EventGraph nodes"), bGraphChanged);
    }


    Work.Result.Stage=TEXT("capture_driver_pins");
    UEdGraphPin* UpdateThenPin = FindOutputPinByNames(UpdateEventNode, { TEXT("then") });
    UEdGraphPin* TryGetPawnExecPin = FindFirstPinByCategory(TryGetPawnOwnerNode, EGPD_Input, UEdGraphSchema_K2::PC_Exec);
    UEdGraphPin* TryGetPawnThenPin = FindFirstPinByCategory(TryGetPawnOwnerNode, EGPD_Output, UEdGraphSchema_K2::PC_Exec);
    UEdGraphPin* TryGetPawnReturnPin = FindFirstDataOutputPin(TryGetPawnOwnerNode);
    UEdGraphPin* GetVelocityTargetPin = FindInputPinByNames(GetVelocityNode, { TEXT("self"), TEXT("Target"), TEXT("Actor") });
    UEdGraphPin* GetVelocityExecPin = FindFirstPinByCategory(GetVelocityNode, EGPD_Input, UEdGraphSchema_K2::PC_Exec);
    UEdGraphPin* GetVelocityThenPin = FindFirstPinByCategory(GetVelocityNode, EGPD_Output, UEdGraphSchema_K2::PC_Exec);
    UEdGraphPin* GetVelocityReturnPin = FindFirstDataOutputPin(GetVelocityNode);
    UEdGraphPin* VSizeXYInputPin = FindInputPinByNames(VSizeXYNode, { TEXT("A"), TEXT("Vector") });
    UEdGraphPin* VSizeXYReturnPin = FindFirstDataOutputPin(VSizeXYNode);
    UEdGraphPin* SetSpeedExecPin = FindPin(SetSpeedNode, UEdGraphSchema_K2::PN_Execute.ToString(), EGPD_Input);
    UEdGraphPin* SetSpeedValuePin = FindVariableSetValueInputPin(SetSpeedNode, SpeedVariable);
    UEdGraphPin* SetSpeedThenPin = FindOutputPinByNames(SetSpeedNode, { TEXT("then") });

    UEdGraphPin* GetActorRotationTargetPin = nullptr;
    UEdGraphPin* GetActorRotationExecPin = nullptr;
    UEdGraphPin* GetActorRotationThenPin = nullptr;
    UEdGraphPin* GetActorRotationReturnPin = nullptr;
    UEdGraphPin* CalculateDirectionExecPin = nullptr;
    UEdGraphPin* CalculateDirectionThenPin = nullptr;
    UEdGraphPin* CalculateDirectionVelocityPin = nullptr;
    UEdGraphPin* CalculateDirectionBaseRotationPin = nullptr;
    UEdGraphPin* CalculateDirectionReturnPin = nullptr;
    UEdGraphPin* SetDirectionExecPin = nullptr;
    UEdGraphPin* SetDirectionThenPin = nullptr;
    UEdGraphPin* SetDirectionValuePin = nullptr;
    if (bIncludeDirection)
    {
        GetActorRotationTargetPin = FindInputPinByNames(GetActorRotationNode, { TEXT("self"), TEXT("Target"), TEXT("Actor") });
        GetActorRotationExecPin = FindFirstPinByCategory(GetActorRotationNode, EGPD_Input, UEdGraphSchema_K2::PC_Exec);
        GetActorRotationThenPin = FindFirstPinByCategory(GetActorRotationNode, EGPD_Output, UEdGraphSchema_K2::PC_Exec);
        GetActorRotationReturnPin = FindFirstDataOutputPin(GetActorRotationNode);
        CalculateDirectionExecPin = FindFirstPinByCategory(CalculateDirectionNode, EGPD_Input, UEdGraphSchema_K2::PC_Exec);
        CalculateDirectionThenPin = FindFirstPinByCategory(CalculateDirectionNode, EGPD_Output, UEdGraphSchema_K2::PC_Exec);
        CalculateDirectionVelocityPin = FindInputPinByNames(CalculateDirectionNode, { TEXT("Velocity") });
        CalculateDirectionBaseRotationPin = FindInputPinByNames(CalculateDirectionNode, { TEXT("BaseRotation"), TEXT("Rotation") });
        CalculateDirectionReturnPin = FindFirstDataOutputPin(CalculateDirectionNode);
        SetDirectionExecPin = FindPin(SetDirectionNode, UEdGraphSchema_K2::PN_Execute.ToString(), EGPD_Input);
        SetDirectionThenPin = FindOutputPinByNames(SetDirectionNode, { TEXT("then") });
        SetDirectionValuePin = FindVariableSetValueInputPin(SetDirectionNode, DirectionVariable);
    }

    UEdGraphPin* GetMovementComponentTargetPin = nullptr;
    UEdGraphPin* GetMovementComponentExecPin = nullptr;
    UEdGraphPin* GetMovementComponentThenPin = nullptr;
    UEdGraphPin* GetMovementComponentReturnPin = nullptr;
    UEdGraphPin* IsFallingTargetPin = nullptr;
    UEdGraphPin* IsFallingExecPin = nullptr;
    UEdGraphPin* IsFallingThenPin = nullptr;
    UEdGraphPin* IsFallingReturnPin = nullptr;
    UEdGraphPin* SetIsInAirExecPin = nullptr;
    UEdGraphPin* SetIsInAirThenPin = nullptr;
    UEdGraphPin* SetIsInAirValuePin = nullptr;
    if (bNeedsMovementComponent)
    {
        GetMovementComponentTargetPin = FindInputPinByNames(GetMovementComponentNode, { TEXT("self"), TEXT("Target"), TEXT("Pawn") });
        GetMovementComponentExecPin = FindFirstPinByCategory(GetMovementComponentNode, EGPD_Input, UEdGraphSchema_K2::PC_Exec);
        GetMovementComponentThenPin = FindFirstPinByCategory(GetMovementComponentNode, EGPD_Output, UEdGraphSchema_K2::PC_Exec);
        GetMovementComponentReturnPin = FindFirstDataOutputPin(GetMovementComponentNode);
    }
    if (bIncludeIsInAir)
    {
        IsFallingTargetPin = FindInputPinByNames(IsFallingNode, { TEXT("self"), TEXT("Target"), TEXT("MovementComponent") });
        IsFallingExecPin = FindFirstPinByCategory(IsFallingNode, EGPD_Input, UEdGraphSchema_K2::PC_Exec);
        IsFallingThenPin = FindFirstPinByCategory(IsFallingNode, EGPD_Output, UEdGraphSchema_K2::PC_Exec);
        IsFallingReturnPin = FindFirstDataOutputPin(IsFallingNode);
        SetIsInAirExecPin = FindPin(SetIsInAirNode, UEdGraphSchema_K2::PN_Execute.ToString(), EGPD_Input);
        SetIsInAirThenPin = FindOutputPinByNames(SetIsInAirNode, { TEXT("then") });
        SetIsInAirValuePin = FindVariableSetValueInputPin(SetIsInAirNode, IsInAirVariable);
    }

    UEdGraphPin* CastCharacterMovementExecPin = nullptr;
    UEdGraphPin* CastCharacterMovementThenPin = nullptr;
    UEdGraphPin* CastCharacterMovementObjectPin = nullptr;
    UEdGraphPin* CastCharacterMovementResultPin = nullptr;
    UEdGraphPin* GetCurrentAccelerationTargetPin = nullptr;
    UEdGraphPin* GetCurrentAccelerationExecPin = nullptr;
    UEdGraphPin* GetCurrentAccelerationThenPin = nullptr;
    UEdGraphPin* GetCurrentAccelerationReturnPin = nullptr;
    UEdGraphPin* AccelerationVSizeXYInputPin = nullptr;
    UEdGraphPin* AccelerationVSizeXYReturnPin = nullptr;
    UEdGraphPin* IsAcceleratingGreaterAInputPin = nullptr;
    UEdGraphPin* IsAcceleratingGreaterBInputPin = nullptr;
    UEdGraphPin* IsAcceleratingGreaterReturnPin = nullptr;
    UEdGraphPin* SetAccelerationExecPin = nullptr;
    UEdGraphPin* SetAccelerationThenPin = nullptr;
    UEdGraphPin* SetAccelerationValuePin = nullptr;
    UEdGraphPin* SetIsAcceleratingExecPin = nullptr;
    UEdGraphPin* SetIsAcceleratingThenPin = nullptr;
    UEdGraphPin* SetIsAcceleratingValuePin = nullptr;
    if (bNeedsCharacterMovement)
    {
        CastCharacterMovementExecPin = FindFirstPinByCategory(CastCharacterMovementNode, EGPD_Input, UEdGraphSchema_K2::PC_Exec);
        CastCharacterMovementThenPin = FindPin(CastCharacterMovementNode,UEdGraphSchema_K2::PN_CastSucceeded.ToString(),EGPD_Output);
        CastCharacterMovementObjectPin = FindPin(CastCharacterMovementNode,UEdGraphSchema_K2::PN_ObjectToCast.ToString(),EGPD_Input);
        CastCharacterMovementResultPin = FindCastResult(CastCharacterMovementNode);
        GetCurrentAccelerationTargetPin = FindInputPinByNames(GetCurrentAccelerationNode, { TEXT("self"), TEXT("Target"), TEXT("CharacterMovementComponent") });
        GetCurrentAccelerationExecPin = FindFirstPinByCategory(GetCurrentAccelerationNode, EGPD_Input, UEdGraphSchema_K2::PC_Exec);
        GetCurrentAccelerationThenPin = FindFirstPinByCategory(GetCurrentAccelerationNode, EGPD_Output, UEdGraphSchema_K2::PC_Exec);
        GetCurrentAccelerationReturnPin = FindFirstDataOutputPin(GetCurrentAccelerationNode);
        AccelerationVSizeXYInputPin = FindInputPinByNames(AccelerationVSizeXYNode, { TEXT("A"), TEXT("Vector") });
        AccelerationVSizeXYReturnPin = FindFirstDataOutputPin(AccelerationVSizeXYNode);
    }
    if (bIncludeIsAccelerating)
    {
        IsAcceleratingGreaterAInputPin = FindInputPinByNames(IsAcceleratingGreaterNode, { TEXT("A") });
        IsAcceleratingGreaterBInputPin = FindInputPinByNames(IsAcceleratingGreaterNode, { TEXT("B") });
        IsAcceleratingGreaterReturnPin = FindFirstDataOutputPin(IsAcceleratingGreaterNode);
        SetIsAcceleratingExecPin = FindPin(SetIsAcceleratingNode, UEdGraphSchema_K2::PN_Execute.ToString(), EGPD_Input);
        SetIsAcceleratingThenPin = FindOutputPinByNames(SetIsAcceleratingNode, { TEXT("then") });
        SetIsAcceleratingValuePin = FindVariableSetValueInputPin(SetIsAcceleratingNode, IsAcceleratingVariable);
    }
    if (bIncludeAcceleration)
    {
        SetAccelerationExecPin = FindPin(SetAccelerationNode, UEdGraphSchema_K2::PN_Execute.ToString(), EGPD_Input);
        SetAccelerationThenPin = FindOutputPinByNames(SetAccelerationNode, { TEXT("then") });
        SetAccelerationValuePin = FindVariableSetValueInputPin(SetAccelerationNode, AccelerationVariable);
    }

    UEdGraphPin* IsValidInputPin = nullptr;
    UEdGraphPin* IsValidReturnPin = nullptr;
    UEdGraphPin* BranchExecPin = nullptr;
    UEdGraphPin* BranchThenPin = nullptr;
    UEdGraphPin* BranchConditionPin = nullptr;
    if (bGuardPawnOwner)
    {
        IsValidInputPin = FindInputPinByNames(IsValidNode, { TEXT("Object"), TEXT("InputObject") });
        IsValidReturnPin = FindFirstDataOutputPin(IsValidNode);
        BranchExecPin = FindPin(BranchNode, UEdGraphSchema_K2::PN_Execute.ToString(), EGPD_Input);
        BranchThenPin = FindOutputPinByNames(BranchNode, { TEXT("then"), TEXT("True") });
        BranchConditionPin = FindInputPinByNames(BranchNode, { TEXT("Condition"), TEXT("bCondition") });
    }


    Work.Result.Stage=TEXT("connect_locomotion_data_and_exec");
    TArray<TSharedPtr<FJsonValue>>& LinkValues=Work.Links;
    bool bLinkChanged = false;
    const bool bTryGetPawnHasExecPins = TryGetPawnExecPin && TryGetPawnThenPin;
    UEdGraphPin* LocomotionExecSourcePin = UpdateThenPin;
    if (bTryGetPawnHasExecPins)
    {
        EnsureK2GraphConnection(EventGraph, UpdateThenPin, TryGetPawnExecPin, TEXT("update_animation_to_try_get_pawn_owner"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);
        LocomotionExecSourcePin = TryGetPawnThenPin;
    }
    else
    {
        WarningValues.Add(MakeShared<FJsonValueString>(TEXT("TryGetPawnOwner has no exec pins; treating it as a pure data node and connecting UpdateAnimation exec directly to the guarded setter chain")));
    }

    if (bGuardPawnOwner)
    {
        EnsureK2GraphConnection(
            EventGraph,
            LocomotionExecSourcePin,
            BranchExecPin,
            bTryGetPawnHasExecPins ? TEXT("try_get_pawn_owner_to_valid_branch") : TEXT("update_animation_to_valid_branch"),
            bReplaceExistingLinks,
            LinkValues,
            ErrorValues,
            bLinkChanged);
        EnsureK2GraphConnection(EventGraph, TryGetPawnReturnPin, IsValidInputPin, TEXT("pawn_owner_to_is_valid"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);
        EnsureK2GraphConnection(EventGraph, IsValidReturnPin, BranchConditionPin, TEXT("is_valid_to_branch_condition"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);
        if (GetVelocityExecPin && GetVelocityThenPin)
        {
            EnsureK2GraphConnection(EventGraph, BranchThenPin, GetVelocityExecPin, TEXT("valid_branch_to_get_velocity"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);
            EnsureK2GraphConnection(EventGraph, GetVelocityThenPin, SetSpeedExecPin, TEXT("get_velocity_to_set_speed_exec"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);
        }
        else
        {
            EnsureK2GraphConnection(EventGraph, BranchThenPin, SetSpeedExecPin, TEXT("valid_branch_to_set_speed_exec"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);
        }
    }
    else if (GetVelocityExecPin && GetVelocityThenPin)
    {
        EnsureK2GraphConnection(
            EventGraph,
            LocomotionExecSourcePin,
            GetVelocityExecPin,
            bTryGetPawnHasExecPins ? TEXT("try_get_pawn_owner_to_get_velocity") : TEXT("update_animation_to_get_velocity"),
            bReplaceExistingLinks,
            LinkValues,
            ErrorValues,
            bLinkChanged);
        EnsureK2GraphConnection(EventGraph, GetVelocityThenPin, SetSpeedExecPin, TEXT("get_velocity_to_set_speed_exec"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);
    }
    else
    {
        EnsureK2GraphConnection(
            EventGraph,
            LocomotionExecSourcePin,
            SetSpeedExecPin,
            bTryGetPawnHasExecPins ? TEXT("try_get_pawn_owner_to_set_speed_exec") : TEXT("update_animation_to_set_speed_exec"),
            bReplaceExistingLinks,
            LinkValues,
            ErrorValues,
            bLinkChanged);
    }
    EnsureK2GraphConnection(EventGraph, TryGetPawnReturnPin, GetVelocityTargetPin, TEXT("pawn_owner_to_get_velocity_target"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);
    EnsureK2GraphConnection(EventGraph, GetVelocityReturnPin, VSizeXYInputPin, TEXT("velocity_to_vsize_xy"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);
    EnsureK2GraphConnection(EventGraph, VSizeXYReturnPin, SetSpeedValuePin, TEXT("speed_value_to_set_speed"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);

    UEdGraphPin* LocomotionPostSetExecPin = SetSpeedThenPin;
    if (bIncludeDirection)
    {
        EnsureK2GraphConnection(EventGraph, TryGetPawnReturnPin, GetActorRotationTargetPin, TEXT("pawn_owner_to_get_actor_rotation_target"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);
        EnsureK2GraphConnection(EventGraph, GetVelocityReturnPin, CalculateDirectionVelocityPin, TEXT("velocity_to_calculate_direction"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);
        EnsureK2GraphConnection(EventGraph, GetActorRotationReturnPin, CalculateDirectionBaseRotationPin, TEXT("actor_rotation_to_calculate_direction_base_rotation"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);
        EnsureK2GraphConnection(EventGraph, CalculateDirectionReturnPin, SetDirectionValuePin, TEXT("direction_value_to_set_direction"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);

        if (GetActorRotationExecPin && GetActorRotationThenPin)
        {
            EnsureK2GraphConnection(EventGraph, LocomotionPostSetExecPin, GetActorRotationExecPin, TEXT("set_speed_to_get_actor_rotation_exec"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);
            LocomotionPostSetExecPin = GetActorRotationThenPin;
        }
        if (CalculateDirectionExecPin && CalculateDirectionThenPin)
        {
            EnsureK2GraphConnection(
                EventGraph,
                LocomotionPostSetExecPin,
                CalculateDirectionExecPin,
                GetActorRotationExecPin ? TEXT("get_actor_rotation_to_calculate_direction_exec") : TEXT("set_speed_to_calculate_direction_exec"),
                bReplaceExistingLinks,
                LinkValues,
                ErrorValues,
                bLinkChanged);
            LocomotionPostSetExecPin = CalculateDirectionThenPin;
        }
        EnsureK2GraphConnection(
            EventGraph,
            LocomotionPostSetExecPin,
            SetDirectionExecPin,
            CalculateDirectionExecPin ? TEXT("calculate_direction_to_set_direction_exec") :
                (GetActorRotationExecPin ? TEXT("get_actor_rotation_to_set_direction_exec") : TEXT("set_speed_to_set_direction_exec")),
            bReplaceExistingLinks,
            LinkValues,
            ErrorValues,
            bLinkChanged);
        LocomotionPostSetExecPin = SetDirectionThenPin;
    }

    if (bNeedsMovementComponent)
    {
        EnsureK2GraphConnection(EventGraph, TryGetPawnReturnPin, GetMovementComponentTargetPin, TEXT("pawn_owner_to_get_movement_component_target"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);

        if (GetMovementComponentExecPin && GetMovementComponentThenPin)
        {
            EnsureK2GraphConnection(EventGraph, LocomotionPostSetExecPin, GetMovementComponentExecPin, TEXT("locomotion_setters_to_get_movement_component_exec"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);
            LocomotionPostSetExecPin = GetMovementComponentThenPin;
        }
    }

    if (bIncludeIsInAir)
    {
        EnsureK2GraphConnection(EventGraph, GetMovementComponentReturnPin, IsFallingTargetPin, TEXT("movement_component_to_is_falling_target"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);
        EnsureK2GraphConnection(EventGraph, IsFallingReturnPin, SetIsInAirValuePin, TEXT("is_falling_to_set_is_in_air"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);

        if (IsFallingExecPin && IsFallingThenPin)
        {
            EnsureK2GraphConnection(
                EventGraph,
                LocomotionPostSetExecPin,
                IsFallingExecPin,
                GetMovementComponentExecPin ? TEXT("get_movement_component_to_is_falling_exec") : TEXT("locomotion_setters_to_is_falling_exec"),
                bReplaceExistingLinks,
                LinkValues,
                ErrorValues,
                bLinkChanged);
            LocomotionPostSetExecPin = IsFallingThenPin;
        }
        EnsureK2GraphConnection(
            EventGraph,
            LocomotionPostSetExecPin,
            SetIsInAirExecPin,
            IsFallingExecPin ? TEXT("is_falling_to_set_is_in_air_exec") :
                (GetMovementComponentExecPin ? TEXT("get_movement_component_to_set_is_in_air_exec") : TEXT("locomotion_setters_to_set_is_in_air_exec")),
            bReplaceExistingLinks,
            LinkValues,
            ErrorValues,
            bLinkChanged);
        LocomotionPostSetExecPin = SetIsInAirThenPin;
    }

    if (bNeedsCharacterMovement)
    {
        EnsureK2GraphConnection(EventGraph, GetMovementComponentReturnPin, CastCharacterMovementObjectPin, TEXT("movement_component_to_character_movement_cast"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);
        EnsureK2GraphConnection(EventGraph, CastCharacterMovementResultPin, GetCurrentAccelerationTargetPin, TEXT("character_movement_to_get_current_acceleration_target"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);
        EnsureK2GraphConnection(EventGraph, GetCurrentAccelerationReturnPin, AccelerationVSizeXYInputPin, TEXT("current_acceleration_to_vsize_xy"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);

        if (bIncludeAcceleration)
        {
            EnsureK2GraphConnection(EventGraph, AccelerationVSizeXYReturnPin, SetAccelerationValuePin, TEXT("acceleration_value_to_set_acceleration"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);
        }
        if (bIncludeIsAccelerating)
        {
            EnsureK2GraphConnection(EventGraph, AccelerationVSizeXYReturnPin, IsAcceleratingGreaterAInputPin, TEXT("acceleration_value_to_is_accelerating_compare"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);
            EnsureK2GraphConnection(EventGraph, IsAcceleratingGreaterReturnPin, SetIsAcceleratingValuePin, TEXT("is_accelerating_compare_to_set_is_accelerating"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);

            if(!SetThreshold(Context,IsAcceleratingGreaterNode,IsAcceleratingGreaterBInputPin,IsAcceleratingThreshold)) { if(!Work.Current()) { return Work.Finish(); } }

        }

        EnsureK2GraphConnection(EventGraph, LocomotionPostSetExecPin, CastCharacterMovementExecPin, TEXT("locomotion_setters_to_character_movement_cast_exec"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);
        LocomotionPostSetExecPin = CastCharacterMovementThenPin;
        if (GetCurrentAccelerationExecPin && GetCurrentAccelerationThenPin)
        {
            EnsureK2GraphConnection(EventGraph, LocomotionPostSetExecPin, GetCurrentAccelerationExecPin, TEXT("character_movement_cast_to_get_current_acceleration_exec"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);
            LocomotionPostSetExecPin = GetCurrentAccelerationThenPin;
        }
        if (bIncludeAcceleration)
        {
            EnsureK2GraphConnection(EventGraph, LocomotionPostSetExecPin, SetAccelerationExecPin, TEXT("acceleration_chain_to_set_acceleration_exec"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);
            LocomotionPostSetExecPin = SetAccelerationThenPin;
        }
        if (bIncludeIsAccelerating)
        {
            EnsureK2GraphConnection(EventGraph, LocomotionPostSetExecPin, SetIsAcceleratingExecPin, TEXT("acceleration_chain_to_set_is_accelerating_exec"), bReplaceExistingLinks, LinkValues, ErrorValues, bLinkChanged);
            LocomotionPostSetExecPin = SetIsAcceleratingThenPin;
        }
    }
    bGraphChanged = bGraphChanged || bLinkChanged;


    TSharedPtr<FJsonObject> NodesObject = MakeShared<FJsonObject>();
    NodesObject->SetObjectField(TEXT("update_animation_event"), NodeToJson(UpdateEventNode, true));
    NodesObject->SetObjectField(TEXT("try_get_pawn_owner"), NodeToJson(TryGetPawnOwnerNode, true));
    NodesObject->SetObjectField(TEXT("get_velocity"), NodeToJson(GetVelocityNode, true));
    NodesObject->SetObjectField(TEXT("vsize_xy"), NodeToJson(VSizeXYNode, true));
    NodesObject->SetObjectField(TEXT("set_speed"), NodeToJson(SetSpeedNode, true));
    if (bIncludeDirection)
    {
        NodesObject->SetObjectField(TEXT("get_actor_rotation"), NodeToJson(GetActorRotationNode, true));
        NodesObject->SetObjectField(TEXT("calculate_direction"), NodeToJson(CalculateDirectionNode, true));
        NodesObject->SetObjectField(TEXT("set_direction"), NodeToJson(SetDirectionNode, true));
    }
    if (bIncludeIsInAir)
    {
        NodesObject->SetObjectField(TEXT("get_movement_component"), NodeToJson(GetMovementComponentNode, true));
        NodesObject->SetObjectField(TEXT("is_falling"), NodeToJson(IsFallingNode, true));
        NodesObject->SetObjectField(TEXT("set_is_in_air"), NodeToJson(SetIsInAirNode, true));
    }
    else if (bNeedsMovementComponent)
    {
        NodesObject->SetObjectField(TEXT("get_movement_component"), NodeToJson(GetMovementComponentNode, true));
    }
    if (bNeedsCharacterMovement)
    {
        NodesObject->SetObjectField(TEXT("cast_character_movement"), NodeToJson(CastCharacterMovementNode, true));
        NodesObject->SetObjectField(TEXT("get_current_acceleration"), NodeToJson(GetCurrentAccelerationNode, true));
        NodesObject->SetObjectField(TEXT("acceleration_vsize_xy"), NodeToJson(AccelerationVSizeXYNode, true));
    }
    if (bIncludeAcceleration)
    {
        NodesObject->SetObjectField(TEXT("set_acceleration"), NodeToJson(SetAccelerationNode, true));
    }
    if (bIncludeIsAccelerating)
    {
        NodesObject->SetObjectField(TEXT("is_accelerating_compare"), NodeToJson(IsAcceleratingGreaterNode, true));
        NodesObject->SetObjectField(TEXT("set_is_accelerating"), NodeToJson(SetIsAcceleratingNode, true));
    }
    if (bGuardPawnOwner)
    {
        NodesObject->SetObjectField(TEXT("is_valid"), NodeToJson(IsValidNode, true));
        NodesObject->SetObjectField(TEXT("valid_branch"), NodeToJson(BranchNode, true));
    }

    TSharedPtr<FJsonObject> CreatedNodesObject = MakeShared<FJsonObject>();
    CreatedNodesObject->SetBoolField(TEXT("update_animation_event"), bUpdateEventCreated);
    CreatedNodesObject->SetBoolField(TEXT("try_get_pawn_owner"), bTryGetPawnOwnerCreated);
    CreatedNodesObject->SetBoolField(TEXT("get_velocity"), bGetVelocityCreated);
    CreatedNodesObject->SetBoolField(TEXT("vsize_xy"), bVSizeXYCreated);
    CreatedNodesObject->SetBoolField(TEXT("set_speed"), bSetSpeedCreated);
    CreatedNodesObject->SetBoolField(TEXT("is_valid"), bIsValidCreated);
    CreatedNodesObject->SetBoolField(TEXT("valid_branch"), bBranchCreated);
    CreatedNodesObject->SetBoolField(TEXT("get_actor_rotation"), bGetActorRotationCreated);
    CreatedNodesObject->SetBoolField(TEXT("calculate_direction"), bCalculateDirectionCreated);
    CreatedNodesObject->SetBoolField(TEXT("set_direction"), bSetDirectionCreated);
    CreatedNodesObject->SetBoolField(TEXT("get_movement_component"), bGetMovementComponentCreated);
    CreatedNodesObject->SetBoolField(TEXT("is_falling"), bIsFallingCreated);
    CreatedNodesObject->SetBoolField(TEXT("set_is_in_air"), bSetIsInAirCreated);
    CreatedNodesObject->SetBoolField(TEXT("cast_character_movement"), bCastCharacterMovementCreated);
    CreatedNodesObject->SetBoolField(TEXT("get_current_acceleration"), bGetCurrentAccelerationCreated);
    CreatedNodesObject->SetBoolField(TEXT("acceleration_vsize_xy"), bAccelerationVSizeXYCreated);
    CreatedNodesObject->SetBoolField(TEXT("is_accelerating_compare"), bIsAcceleratingGreaterCreated);
    CreatedNodesObject->SetBoolField(TEXT("set_acceleration"), bSetAccelerationCreated);
    CreatedNodesObject->SetBoolField(TEXT("set_is_accelerating"), bSetIsAcceleratingCreated);


    Legacy->SetObjectField(TEXT("nodes"),NodesObject);Legacy->SetObjectField(TEXT("created_nodes"),CreatedNodesObject);Legacy->SetArrayField(TEXT("variables"),VariableValues);Legacy->SetArrayField(TEXT("links"),LinkValues);Legacy->SetArrayField(TEXT("warnings"),WarningValues);
    if(!Work.Current()) { return Work.Finish(); }if(!ErrorValues.IsEmpty()) { return MakeEventGraphPartialError(TEXT("Failed to author AnimBP locomotion EventGraph driver"),bGraphChanged); }
    Work.Result.Stage=TEXT("notify_structural_change");if(bGraphChanged) { Work.Writer(TEXT("mark_structurally_modified_calls"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(TargetAnimBlueprint);if(!Context.Refresh()) { return Work.Finish(); }Work.Writer(TEXT("notify_graph_changed_calls"));EventGraph->NotifyGraphChanged();if(!Context.Refresh()) { return Work.Finish(); } }
    bool CompileOK=true,SaveOK=true;Work.Result.Stage=TEXT("compile_target_anim_blueprint");
    if(bCompile)
    {
        if(!LinkedClosure(TargetAnimBlueprint,nullptr,Work)||!Context.Refresh()||!RuntimePackageGuard(TargetAnimBlueprint,Work,TEXT("precompile_dependency_projection"))) { return Work.Finish(); }
        FCompilerResultsLog Log;Work.Result.bCompileAttempted=true;Work.Compile->SetBoolField(TEXT("attempted"),true);Work.Writer(TEXT("compile_blueprint_calls"));FKismetEditorUtilities::CompileBlueprint(TargetAnimBlueprint,EBlueprintCompileOptions::None,&Log);Work.Count(TEXT("compile_blueprint_returns"));
        if(!Work.Current()) { return Work.Finish(); }UClass* Generated=TargetAnimBlueprint->GeneratedClass;const bool LegacySuccess=Generated!=nullptr;
        CompileOK=Live(Generated)&&Generated->IsChildOf(UAnimInstance::StaticClass())&&Generated->ClassGeneratedBy==TargetAnimBlueprint&&!Generated->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)&&Log.NumErrors==0&&(TargetAnimBlueprint->Status==BS_UpToDate||TargetAnimBlueprint->Status==BS_UpToDateWithWarnings);
        Work.Result.Compiled=CompileOK;Work.Compile->SetBoolField(TEXT("success"),LegacySuccess);Work.Compile->SetBoolField(TEXT("compiler_status_verified"),CompileOK);Work.Compile->SetNumberField(TEXT("error_count"),Log.NumErrors);Work.Compile->SetNumberField(TEXT("warning_count"),Log.NumWarnings);Work.Compile->SetNumberField(TEXT("blueprint_status"),int32(TargetAnimBlueprint->Status));Work.Compile->SetStringField(TEXT("blueprint"),TargetAnimBlueprint->GetPathName());Work.Compile->SetStringField(TEXT("generated_class"),Live(Generated)?Generated->GetPathName():FString());
        if(!RuntimePackageGuard(TargetAnimBlueprint,Work,TEXT("postcompile_dependency_projection"))) { return Work.Finish(); }
    }
    Legacy->SetObjectField(TEXT("compile"),Work.Compile);Work.Result.Stage=TEXT("save_target_anim_blueprint");
    if(bSave) { SaveOK=SaveOne(*Work.Target,TEXT("target_saved"),Work);Work.Result.bSaved=SaveOK;if(!Work.Current()) { return Work.Finish(); } }
    Work.Result.Stage=TEXT("readback_event_graph");Work.SDK(TEXT("blueprint_topology_inspection_calls"));const auto Readback=BlueprintTopology::Inspect(TargetAnimBPPath,EventGraphName,TEXT(""),TEXT("event"),TEXT(""),TEXT(""),TEXT(""),TEXT(""),true,true,true,64,128,256,64);Work.Count(TEXT("blueprint_topology_inspection_returns"));
    if(const TSharedPtr<FJsonObject>* Inner=nullptr;Readback.Data->TryGetObjectField(TEXT("legacy_result"),Inner)) { Legacy->SetObjectField(TEXT("readback"),*Inner); }else { Legacy->SetObjectField(TEXT("readback"),Readback.Data); }
    Work.Result.bMutationAttempted|=Readback.bMutationAttempted;Work.Result.bChanged|=Readback.bChanged;Work.Result.Data->SetBoolField(TEXT("readback_success"),Readback.bSuccess);Work.Result.Data->SetStringField(TEXT("readback_error"),Readback.Error);Work.Result.Data->SetStringField(TEXT("readback_stage"),Readback.Stage);
    if(!Work.Current()) { return Work.Finish(); }
    Legacy->SetBoolField(TEXT("compile_succeeded"),CompileOK);Legacy->SetBoolField(TEXT("save_succeeded"),SaveOK);TArray<TSharedPtr<FJsonValue>> CompletionErrors;if(!CompileOK) { CompletionErrors.Add(MakeShared<FJsonValueString>(TEXT("compile_failed"))); }if(!SaveOK) { CompletionErrors.Add(MakeShared<FJsonValueString>(TEXT("save_failed"))); }if(!CompletionErrors.IsEmpty()) { Legacy->SetArrayField(TEXT("completion_errors"),CompletionErrors); }
    // Reacquire a current graph after compiler/readback; no old pin pointer is used.
    TArray<UEdGraph*> FinalGraphs;if(!Graphs(TargetAnimBlueprint,FinalGraphs,Work)) { return Work.Finish(); }bool GraphFound=false;
    for(UEdGraph* Graph:FinalGraphs) { if(Graph->GetName()==EventGraphName) { FGraphContext Final(Work,TargetAnimBlueprint,Graph);if(!Final.Refresh()) { return Work.Finish(); }auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("graph_id"),Graph->GraphGuid.ToString());Row->SetStringField(TEXT("graph_name"),Graph->GetName());Row->SetStringField(TEXT("graph_type"),TEXT("event"));Row->SetStringField(TEXT("schema"),Final.Schema->GetClass()->GetName());Row->SetNumberField(TEXT("node_count"),Graph->Nodes.Num());Legacy->SetObjectField(TEXT("graph"),Row);GraphFound=true;break; } }
    if(!GraphFound) { Work.Fail(TEXT("event_graph_missing_after_readback"),TEXT("Authored EventGraph is unavailable after compiler/readback callbacks"));return Work.Finish(); }
    if(!CompileOK) { Work.Fail(TEXT("compile_failed"),TEXT("Driver authored; compiler postcondition failed, inspect completion_errors and saved prefix")); }
    else if(!SaveOK) { Work.Fail(TEXT("save_failed"),TEXT("Driver authored but requested save failed")); }
    else if(!Readback.bSuccess||!Readback.bPostconditionPassed) { Work.Fail(TEXT("readback_failed"),TEXT("Driver authored but topology readback failed: ")+Readback.Error); }
    else { Work.Result.Stage=TEXT("complete");Work.Result.bSuccess=true;Work.Result.bPostconditionPassed=true; }return Work.Finish();
}
} // namespace Cubeless::UE58::AnimBPLocomotionDriver
