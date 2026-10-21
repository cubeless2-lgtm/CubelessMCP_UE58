#include "Compatibility/UE58/CubelessAnimStateMachineVariantApi.h"
#include "Compatibility/UE58/CubelessAnimStateMachineInspectionApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "AnimGraphNode_LinkedAnimGraph.h"

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
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_StateMachineBase.h"
#include "AnimGraphNode_AssetPlayerBase.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimationGraph.h"
#include "AnimationStateGraph.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationStateMachineSchema.h"
#include "AnimationTransitionGraph.h"
#include "AnimStateNode.h"
#include "AnimStateNodeBase.h"
#include "AnimStateEntryNode.h"
#include "AnimStateTransitionNode.h"
#include "Animation/AnimationAsset.h"
#include "Animation/BlendSpace.h"
#include "EdGraphSchema_K2_Actions.h"
#include "K2Node_VariableGet.h"
#include "K2Node_CallFunction.h"
#include "K2Node_EnumLiteral.h"
#include "K2Node_EnumEquality.h"
#include "K2Node_EnumInequality.h"
#include "Kismet/KismetMathLibrary.h"
#include "UObject/UnrealType.h"
#include "Misc/ScopeExit.h"
#include <type_traits>
namespace Cubeless::UE58::AnimStateMachineVariant
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
        Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("command"),TEXT("ensure_anim_state_machine_sample_variant"));
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
        if(Result.Error.IsEmpty()) { Result.ErrorCode=FString(TEXT("anim_state_machine_variant_"))+Code;Result.Error=Message; }
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
        Result.Data->SetArrayField(TEXT("preparation"),Preparation);Legacy->SetArrayField(TEXT("created_links"),Links);Legacy->SetArrayField(TEXT("errors"),Errors);
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
        if(!Work.Current()||!Identity.Current()||!Live(CurrentSchema)||(!CurrentSchema->IsA(UEdGraphSchema_K2::StaticClass())&&!CurrentSchema->IsA(UAnimationStateMachineSchema::StaticClass()))) { return Work.Fail(TEXT("schema_unavailable"),TEXT("Selected graph has no live K2/animation/state-machine schema")); }
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

// FG-specific scalar/parser support; no RPC or Toolset dependency.
FString GetStringParam(const TSharedPtr<FJsonObject>& Params,const FString& Key,const FString& Default=FString())
{ FString Value;return Params&&Params->TryGetStringField(Key,Value)?Value:Default; }
bool GetBoolParam(const TSharedPtr<FJsonObject>& Params,const FString& Key,bool Default)
{ return !Params||!Params->HasField(Key)?Default:Params->GetBoolField(Key); }
int32 GetClampedIntParam(const TSharedPtr<FJsonObject>& Params,const FString& Key,int32 Default,int32 Min,int32 Max)
{ double Number=Default;if(Params) { Params->TryGetNumberField(Key,Number); }if(!FMath::IsFinite(Number)) { return Default; }return FMath::RoundToInt(FMath::Clamp(Number,double(Min),double(Max))); }
FString SanitizeAssetNameSegmentForAnimNodeProbe(FString Value) { return VariantSegment(MoveTemp(Value)); }
FString NormalizeTypeName(FString Value)
{ Value.ToLowerInline();Value.TrimStartAndEndInline();Value.ReplaceInline(TEXT(" "),TEXT(""));Value.ReplaceInline(TEXT("_"),TEXT(""));Value.ReplaceInline(TEXT("-"),TEXT(""));return Value; }
FVector2D Vector2DFromJson(const TSharedPtr<FJsonObject>& Object,const FString& Key)
{
    FVector2D Value(0,0);const TArray<TSharedPtr<FJsonValue>>* Array=nullptr;
    if(Object->TryGetArrayField(Key,Array)&&Array&&Array->Num()>=2) { Value.X=float((*Array)[0]->AsNumber());Value.Y=float((*Array)[1]->AsNumber()); }
    else { FString Text;if(Object->TryGetStringField(Key,Text)) { Text.TrimStartAndEndInline();Text.RemoveFromStart(TEXT("["));Text.RemoveFromEnd(TEXT("]"));Text.RemoveFromStart(TEXT("("));Text.RemoveFromEnd(TEXT(")"));TArray<FString> Parts;Text.ParseIntoArray(Parts,TEXT(","),true);if(Parts.Num()>=2) { Value.X=FCString::Atof(*Parts[0].TrimStartAndEnd());Value.Y=FCString::Atof(*Parts[1].TrimStartAndEnd()); } } }return Value;
}
bool OperationAlias(const FString& Value,std::initializer_list<const TCHAR*> Names)
{ FString Normalized=Value.ToLower().TrimStartAndEnd();Normalized.ReplaceInline(TEXT(" "),TEXT("_"));Normalized.ReplaceInline(TEXT("-"),TEXT("_"));for(const TCHAR* Name:Names) { FString Alias(Name);Alias.ToLowerInline();Alias.ReplaceInline(TEXT(" "),TEXT("_"));Alias.ReplaceInline(TEXT("-"),TEXT("_"));if(Normalized==Alias) { return true; } }return false; }
FName GetCompareFunctionName(const FString& Operation,const FString& Type,FString& Error)
{
    const bool Less=OperationAlias(Operation,{TEXT("less"),TEXT("lt"),TEXT("<")}),Greater=OperationAlias(Operation,{TEXT("greater"),TEXT("gt"),TEXT(">")});
    const bool LE=OperationAlias(Operation,{TEXT("less_equal"),TEXT("lessequal"),TEXT("le"),TEXT("<=")}),GE=OperationAlias(Operation,{TEXT("greater_equal"),TEXT("greaterequal"),TEXT("ge"),TEXT(">=")});
    const bool EQ=OperationAlias(Operation,{TEXT("equal"),TEXT("equals"),TEXT("eq"),TEXT("==")}),NE=OperationAlias(Operation,{TEXT("not_equal"),TEXT("notequal"),TEXT("ne"),TEXT("!=")});
    if(Less||Greater||LE||GE||EQ||NE)
    {
        if(Type==TEXT("byte")&&(EQ||NE)) { return EQ?GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary,EqualEqual_ByteByte):GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary,NotEqual_ByteByte); }
        if(Type==TEXT("float"))
        {
            if(Less) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary,Less_DoubleDouble); }if(Greater) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary,Greater_DoubleDouble); }
            if(LE) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary,LessEqual_DoubleDouble); }if(GE) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary,GreaterEqual_DoubleDouble); }
            return EQ?GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary,EqualEqual_DoubleDouble):GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary,NotEqual_DoubleDouble);
        }
    }
    Error=FString::Printf(TEXT("Unsupported compare type/operation combination: type=%s, operation=%s"),*Type,*Operation);return NAME_None;
}
UEnum* LoadEnumForPin(const FString& Requested,FWork& Work)
{
    FString Name=Requested.TrimStartAndEnd();Name.TrimQuotesInline();if(Name.IsEmpty()||!TextSafe(Name,NAME_SIZE-1)||Forbidden(Name)||!Work.Current()) { return nullptr; }
    const FString Path=ObjectPath(Name,false);Work.SDK(TEXT("enum_load_calls"));UEnum* Enum=LoadObject<UEnum>(nullptr,*Path);
    if(!Work.Current()) { return nullptr; }
    if(!Enum) { Work.SDK(TEXT("enum_find_calls"));Enum=UClass::TryFindTypeSlow<UEnum>(*Name,EFindFirstObjectOptions::ExactClass|EFindFirstObjectOptions::NativeFirst); }
    if(!Work.Current()) { return nullptr; }
    if(!Enum&&Path!=Name) { Work.SDK(TEXT("enum_find_calls"));Enum=UClass::TryFindTypeSlow<UEnum>(*Path,EFindFirstObjectOptions::ExactClass|EFindFirstObjectOptions::NativeFirst); }
    if(!Work.Current()) { return nullptr; }
    if(!Enum&&!Name.Contains(TEXT("/"))&&!Name.Contains(TEXT("."))) { Work.SDK(TEXT("enum_load_calls"));Enum=LoadObject<UEnum>(nullptr,*(TEXT("/Script/Engine.")+Name)); }
    if(!Work.Current()||!Live(Enum)||!Live(Enum->GetPackage())||!ObjectAllowed(Enum,Work)) { return nullptr; }
    if(Enum->NumEnums()>65536) { Work.Fail(TEXT("enum_bound"),TEXT("Enum inventory exceeds65536"));return nullptr; }Work.Inputs.Add(MakeUnique<FIdentity>(Enum));return Enum;
}

FString GetEnumTypeParam(const TSharedPtr<FJsonObject>& Params, const FString& PreferredFieldName = TEXT("enum_type"))
{
    if (!Params.IsValid())
    {
        return FString();
    }

    TArray<FString> FieldNames;
    if (!PreferredFieldName.IsEmpty())
    {
        FieldNames.Add(PreferredFieldName);
    }
    FieldNames.Add(TEXT("enum_type"));
    FieldNames.Add(TEXT("type_object"));
    FieldNames.Add(TEXT("enum_path"));

    TSet<FString> SeenFields;
    for (const FString& FieldName : FieldNames)
    {
        if (FieldName.IsEmpty() || SeenFields.Contains(FieldName))
        {
            continue;
        }
        SeenFields.Add(FieldName);

        FString Value;
        if (Params->TryGetStringField(FieldName, Value))
        {
            Value.TrimStartAndEndInline();
            if (!Value.IsEmpty())
            {
                return Value;
            }
        }
    }

    return FString();
}

const UEnum* GetEnumForPinType(const FEdGraphPinType& PinType)
{
    if (PinType.PinCategory != UEdGraphSchema_K2::PC_Byte || PinType.IsArray())
    {
        return nullptr;
    }

    return Cast<UEnum>(PinType.PinSubCategoryObject.Get());
}

bool IsEnumIndexHiddenOrSpacer(const UEnum* Enum, int32 Index)
{
    return !Enum ||
        !Enum->IsValidEnumValue(Enum->GetValueByIndex(Index)) ||
        Enum->HasMetaData(TEXT("Hidden"), Index) ||
        Enum->HasMetaData(TEXT("Spacer"), Index);
}

int32 FindFirstVisibleEnumIndex(const UEnum* Enum)
{
    if (!Enum)
    {
        return INDEX_NONE;
    }

    for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
    {
        if (!IsEnumIndexHiddenOrSpacer(Enum, Index))
        {
            return Index;
        }
    }

    return INDEX_NONE;
}

bool ResolveEnumValueDefaultString(const TSharedPtr<FJsonValue>& Value, const UEnum* Enum, FString& OutDefaultValue, int32& OutIndex, FString& OutError)
{
    OutDefaultValue.Reset();
    OutIndex = INDEX_NONE;
    OutError.Reset();

    if (!Enum)
    {
        OutError = TEXT("Invalid enum type");
        return false;
    }

    if (!Value.IsValid())
    {
        OutIndex = FindFirstVisibleEnumIndex(Enum);
        if (OutIndex == INDEX_NONE)
        {
            OutError = FString::Printf(TEXT("Enum has no visible values: %s"), *Enum->GetPathName());
            return false;
        }
    }
    else if (Value->Type == EJson::String)
    {
        FString RequestedValue = Value->AsString();if(!TextSafe(RequestedValue,NAME_SIZE-1)) { OutError=TEXT("Enum value name exceeds FName/no-NUL bounds");return false; }
        RequestedValue.TrimStartAndEndInline();
        RequestedValue.TrimQuotesInline();
        if (RequestedValue.IsEmpty())
        {
            OutError = TEXT("Enum value cannot be empty");
            return false;
        }

        OutIndex = Enum->GetIndexByNameString(RequestedValue);
        if (OutIndex == INDEX_NONE && !RequestedValue.Contains(TEXT("::")))
        {
            OutIndex = Enum->GetIndexByNameString(FString::Printf(TEXT("%s::%s"), *Enum->GetName(), *RequestedValue));
        }
        if (OutIndex == INDEX_NONE)
        {
            OutIndex = Enum->GetIndexByNameString(Enum->GenerateFullEnumName(*RequestedValue));
        }
    }
    else if (Value->Type == EJson::Number)
    {
        const double NumberValue = Value->AsNumber();
        if(!FMath::IsFinite(NumberValue)||NumberValue < -9223372036854775808.0||NumberValue >= 9223372036854775808.0) { OutError=TEXT("Enum numeric value is outside int64");return false; }
        const int64 IntegerValue = static_cast<int64>(NumberValue);
        if (!FMath::IsNearlyEqual(NumberValue, static_cast<double>(IntegerValue)))
        {
            OutError = TEXT("Enum numeric default must be an integer value");
            return false;
        }

        OutIndex = Enum->GetIndexByValue(IntegerValue);
    }
    else
    {
        OutError = TEXT("Enum default must be a string enum entry or integer enum value");
        return false;
    }

    if (OutIndex == INDEX_NONE)
    {
        OutError = FString::Printf(TEXT("Enum value not found for %s"), *Enum->GetPathName());
        return false;
    }
    if (IsEnumIndexHiddenOrSpacer(Enum, OutIndex))
    {
        OutError = FString::Printf(TEXT("Enum value is hidden or spacer: %s"), *Enum->GetNameStringByIndex(OutIndex));
        return false;
    }

    OutDefaultValue = Enum->GetNameStringByIndex(OutIndex);
    return true;
}

FString GetAlphaBlendOptionName(EAlphaBlendOption BlendOption)
{
    if (const UEnum* BlendEnum = StaticEnum<EAlphaBlendOption>())
    {
        return BlendEnum->GetNameStringByValue(static_cast<int64>(BlendOption));
    }
    return TEXT("Linear");
}

bool ParseAlphaBlendOption(const FString& RawValue, EAlphaBlendOption& OutBlendOption, FString& OutError)
{
    FString Normalized = RawValue.TrimStartAndEnd();
    if (Normalized.IsEmpty())
    {
        OutBlendOption = EAlphaBlendOption::Linear;
        return true;
    }

    if (const UEnum* BlendEnum = StaticEnum<EAlphaBlendOption>())
    {
        int64 EnumValue = BlendEnum->GetValueByNameString(Normalized);
        if (EnumValue == INDEX_NONE && !Normalized.StartsWith(TEXT("EAlphaBlendOption::")))
        {
            EnumValue = BlendEnum->GetValueByNameString(FString::Printf(TEXT("EAlphaBlendOption::%s"), *Normalized));
        }

        if (EnumValue != INDEX_NONE)
        {
            OutBlendOption = static_cast<EAlphaBlendOption>(EnumValue);
            return true;
        }
    }

    OutError = FString::Printf(TEXT("Unsupported blend_mode '%s'"), *RawValue);
    return false;
}

FVector2D GetOptionalVector2DField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, const FVector2D& DefaultValue)
{
    if (!Object.IsValid() || !Object->HasField(FieldName))
    {
        return DefaultValue;
    }

    const TSharedPtr<FJsonObject>* PositionObject = nullptr;
    if (Object->TryGetObjectField(FieldName, PositionObject) && PositionObject && PositionObject->IsValid())
    {
        double X = DefaultValue.X;
        double Y = DefaultValue.Y;
        (*PositionObject)->TryGetNumberField(TEXT("x"), X);
        (*PositionObject)->TryGetNumberField(TEXT("y"), Y);
        return FVector2D(X, Y);
    }

    return Vector2DFromJson(Object, FieldName);
}

struct FAnimStateMachineSamplePlayerInputSpec
{
    FString PinName;
    FString VariableName;
    bool bCreateVariableIfMissing = false;
    bool bHasDefaultValue = false;
    double DefaultValue = 0.0;
};

struct FAnimStateMachineSampleStateSpec
{
    FString Name;
    FString AnimationAssetPath;
    FVector2D Position = FVector2D(0.0f, 0.0f);
    bool bAlwaysResetOnEntry = false;
    UAnimationAsset* AnimationAsset = nullptr;
    TArray<FAnimStateMachineSamplePlayerInputSpec> PlayerInputs;
};

struct FAnimStateMachineSampleTransitionSpec
{
    FString FromState;
    FString ToState;
    bool bBidirectional = false;
    int32 PriorityOrder = 1;
    float CrossfadeDuration = 0.2f;
    EAlphaBlendOption BlendMode = EAlphaBlendOption::Linear;
    FString RuleMode = TEXT("always_true");
    float AutomaticRuleTriggerTime = -1.0f;
    FName SyncGroupNameToRequireValidMarkersRule = NAME_None;
    FString RuleVariableName;
    bool bCreateRuleVariableIfMissing = false;
    FString RuleVariableType;
    bool bRuleExpectedValue = true;
    FString RuleCompareOperation = TEXT("greater");
    double RuleCompareValue = 0.0;
    FString RuleEnumType;
    FString RuleEnumDefaultValue;
    int32 RuleEnumIndex = INDEX_NONE;
};

FString NormalizeAnimStateMachinePlayerInputPinName(FString PinName)
{
    PinName.TrimStartAndEndInline();
    PinName.ReplaceInline(TEXT("-"), TEXT("_"));
    if (PinName.Equals(TEXT("x"), ESearchCase::IgnoreCase) ||
        PinName.Equals(TEXT("x_axis"), ESearchCase::IgnoreCase) ||
        PinName.Equals(TEXT("horizontal"), ESearchCase::IgnoreCase))
    {
        return TEXT("X");
    }
    if (PinName.Equals(TEXT("y"), ESearchCase::IgnoreCase) ||
        PinName.Equals(TEXT("y_axis"), ESearchCase::IgnoreCase) ||
        PinName.Equals(TEXT("vertical"), ESearchCase::IgnoreCase))
    {
        return TEXT("Y");
    }
    if (PinName.Equals(TEXT("z"), ESearchCase::IgnoreCase) ||
        PinName.Equals(TEXT("z_axis"), ESearchCase::IgnoreCase))
    {
        return TEXT("Z");
    }
    return PinName;
}

FString GetPlayerInputStringField(const TSharedPtr<FJsonObject>& Object, const TArray<FString>& FieldNames, const FString& DefaultValue = FString())
{
    if (!Object.IsValid())
    {
        return DefaultValue;
    }

    for (const FString& FieldName : FieldNames)
    {
        FString Value;
        if (Object->TryGetStringField(FieldName, Value))
        {
            Value.TrimStartAndEndInline();
            return Value;
        }
    }
    return DefaultValue;
}

bool ParseAnimStateMachineSamplePlayerInputSpecObject(
    const TSharedPtr<FJsonObject>& InputObject,
    const FString& DefaultPinName,
    const FString& StateName,
    FAnimStateMachineSamplePlayerInputSpec& OutSpec,
    TArray<TSharedPtr<FJsonValue>>& OutErrors)
{
    if (!InputObject.IsValid())
    {
        OutErrors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("player_inputs for state '%s' must contain objects, numbers, or variable-name strings"), *StateName)));
        return false;
    }

    OutSpec.PinName = NormalizeAnimStateMachinePlayerInputPinName(GetPlayerInputStringField(
        InputObject,
        { TEXT("pin"), TEXT("pin_name"), TEXT("input"), TEXT("input_pin"), TEXT("axis"), TEXT("name") },
        DefaultPinName));
    OutSpec.VariableName = GetPlayerInputStringField(
        InputObject,
        { TEXT("variable"), TEXT("variable_name"), TEXT("anim_bp_variable"), TEXT("input_variable") });
    OutSpec.bCreateVariableIfMissing =
        GetBoolParam(InputObject, TEXT("create_variable_if_missing"), false) ||
        GetBoolParam(InputObject, TEXT("create_input_variable_if_missing"), false);

    if (InputObject->HasField(TEXT("value")))
    {
        if (!InputObject->TryGetNumberField(TEXT("value"), OutSpec.DefaultValue))
        {
            OutErrors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("player_inputs value for pin '%s' in state '%s' must be numeric"), *OutSpec.PinName, *StateName)));
            return false;
        }
        OutSpec.bHasDefaultValue = true;
    }
    else if (InputObject->HasField(TEXT("default_value")))
    {
        if (!InputObject->TryGetNumberField(TEXT("default_value"), OutSpec.DefaultValue))
        {
            OutErrors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("player_inputs default_value for pin '%s' in state '%s' must be numeric"), *OutSpec.PinName, *StateName)));
            return false;
        }
        OutSpec.bHasDefaultValue = true;
    }

    if (OutSpec.PinName.IsEmpty())
    {
        OutErrors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("player_inputs entry for state '%s' is missing a pin/input/axis name"), *StateName)));
        return false;
    }
    if (OutSpec.VariableName.IsEmpty() && !OutSpec.bHasDefaultValue)
    {
        OutErrors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("player_inputs entry for pin '%s' in state '%s' must specify variable or numeric value/default_value"), *OutSpec.PinName, *StateName)));
        return false;
    }

    return true;
}

bool ParseAnimStateMachineSamplePlayerInputValue(
    const TSharedPtr<FJsonValue>& InputValue,
    const FString& DefaultPinName,
    const FString& StateName,
    FAnimStateMachineSamplePlayerInputSpec& OutSpec,
    TArray<TSharedPtr<FJsonValue>>& OutErrors)
{
    if (!InputValue.IsValid())
    {
        OutErrors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("player_inputs entry for state '%s' is invalid"), *StateName)));
        return false;
    }

    double NumericValue = 0.0;
    if (InputValue->TryGetNumber(NumericValue))
    {
        OutSpec.PinName = NormalizeAnimStateMachinePlayerInputPinName(DefaultPinName);
        OutSpec.bHasDefaultValue = true;
        OutSpec.DefaultValue = NumericValue;
        if (OutSpec.PinName.IsEmpty())
        {
            OutErrors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Numeric player_inputs entry for state '%s' requires an object key pin name"), *StateName)));
            return false;
        }
        return true;
    }

    FString VariableName;
    if (InputValue->TryGetString(VariableName))
    {
        OutSpec.PinName = NormalizeAnimStateMachinePlayerInputPinName(DefaultPinName);
        OutSpec.VariableName = VariableName;
        OutSpec.VariableName.TrimStartAndEndInline();
        if (OutSpec.PinName.IsEmpty() || OutSpec.VariableName.IsEmpty())
        {
            OutErrors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("String player_inputs entry for state '%s' requires an object key pin name and non-empty variable name"), *StateName)));
            return false;
        }
        return true;
    }

    const TSharedPtr<FJsonObject>* ObjectValue = nullptr;
    if (InputValue->TryGetObject(ObjectValue) && ObjectValue && ObjectValue->IsValid())
    {
        return ParseAnimStateMachineSamplePlayerInputSpecObject(*ObjectValue, DefaultPinName, StateName, OutSpec, OutErrors);
    }

    OutErrors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("player_inputs entry for state '%s' must be an object, number, or variable-name string"), *StateName)));
    return false;
}

void ParseAnimStateMachineSamplePlayerInputs(
    const TSharedPtr<FJsonObject>& StateObject,
    FAnimStateMachineSampleStateSpec& Spec,
    TArray<TSharedPtr<FJsonValue>>& OutErrors)
{
    if (!StateObject.IsValid())
    {
        return;
    }

    TSharedPtr<FJsonValue> PlayerInputsValue = StateObject->TryGetField(TEXT("player_inputs"));
    if (!PlayerInputsValue.IsValid())
    {
        PlayerInputsValue = StateObject->TryGetField(TEXT("asset_player_inputs"));
    }
    if (!PlayerInputsValue.IsValid())
    {
        PlayerInputsValue = StateObject->TryGetField(TEXT("blendspace_inputs"));
    }
    if (!PlayerInputsValue.IsValid())
    {
        return;
    }

    const int32 InitialCount = Spec.PlayerInputs.Num();
    const TArray<TSharedPtr<FJsonValue>>* InputArray = nullptr;
    if (PlayerInputsValue->TryGetArray(InputArray) && InputArray)
    {
        for (int32 Index = 0; Index < InputArray->Num(); ++Index)
        {
            FAnimStateMachineSamplePlayerInputSpec InputSpec;
            if (ParseAnimStateMachineSamplePlayerInputValue((*InputArray)[Index], FString(), Spec.Name, InputSpec, OutErrors))
            {
                Spec.PlayerInputs.Add(InputSpec);
            }
        }
    }
    else
    {
        const TSharedPtr<FJsonObject>* InputObject = nullptr;
        if (PlayerInputsValue->TryGetObject(InputObject) && InputObject && InputObject->IsValid())
        {
            for (const auto& Pair : (*InputObject)->Values)
            {
                FAnimStateMachineSamplePlayerInputSpec InputSpec;
                if (ParseAnimStateMachineSamplePlayerInputValue(Pair.Value, FString(Pair.Key.ToView()), Spec.Name, InputSpec, OutErrors))
                {
                    Spec.PlayerInputs.Add(InputSpec);
                }
            }
        }
        else
        {
            OutErrors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("player_inputs for state '%s' must be an array or object"), *Spec.Name)));
        }
    }

    if (Spec.PlayerInputs.Num() - InitialCount > 8)
    {
        OutErrors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("player_inputs for state '%s' has %d entries; max is 8"), *Spec.Name, Spec.PlayerInputs.Num() - InitialCount)));
    }
}

bool ParseAnimStateMachineSampleStateSpecs(
    const TSharedPtr<FJsonObject>& Params,
    int32 MaxStates,
    TArray<FAnimStateMachineSampleStateSpec>& OutSpecs,
    TArray<TSharedPtr<FJsonValue>>& OutErrors)
{
    OutSpecs.Reset();

    const TArray<TSharedPtr<FJsonValue>>* StateValues = nullptr;
    if (Params->HasField(TEXT("states")) && !Params->TryGetArrayField(TEXT("states"), StateValues))
    {
        OutErrors.Add(MakeShared<FJsonValueString>(TEXT("'states' must be an array")));
        return false;
    }

    if (StateValues)
    {
        if (StateValues->Num() == 0)
        {
            OutErrors.Add(MakeShared<FJsonValueString>(TEXT("'states' must contain at least one state")));
            return false;
        }
        if (StateValues->Num() > MaxStates)
        {
            OutErrors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("'states' contains %d entries, max_states=%d"), StateValues->Num(), MaxStates)));
            return false;
        }

        TSet<FString> SeenNames;
        for (int32 Index = 0; Index < StateValues->Num(); ++Index)
        {
            const TSharedPtr<FJsonValue>& StateValue = (*StateValues)[Index];
            const TSharedPtr<FJsonObject>* StateObject = nullptr;
            if (!StateValue.IsValid() || !StateValue->TryGetObject(StateObject) || !StateObject || !StateObject->IsValid())
            {
                OutErrors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("states[%d] must be an object"), Index)));
                continue;
            }

            FAnimStateMachineSampleStateSpec Spec;
            Spec.Name = SanitizeAssetNameSegmentForAnimNodeProbe(GetStringParam(*StateObject, TEXT("name"), FString::Printf(TEXT("State_%d"), Index)));
            Spec.AnimationAssetPath = GetStringParam(*StateObject, TEXT("animation_asset_path"));
            if (Spec.AnimationAssetPath.IsEmpty())
            {
                Spec.AnimationAssetPath = GetStringParam(*StateObject, TEXT("animation_asset"));
            }
            Spec.Position = GetOptionalVector2DField(*StateObject, TEXT("position"), FVector2D(Index * 320.0f, 0.0f));
            Spec.bAlwaysResetOnEntry = GetBoolParam(*StateObject, TEXT("always_reset_on_entry"), false);
            ParseAnimStateMachineSamplePlayerInputs(*StateObject, Spec, OutErrors);

            const FString Kind = GetStringParam(*StateObject, TEXT("kind"), TEXT("asset_player"));
            if (!Kind.Equals(TEXT("asset_player"), ESearchCase::IgnoreCase))
            {
                OutErrors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Unsupported state kind '%s' for state '%s'"), *Kind, *Spec.Name)));
                continue;
            }
            if (Spec.AnimationAssetPath.IsEmpty())
            {
                OutErrors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Missing animation_asset_path for state '%s'"), *Spec.Name)));
                continue;
            }
            if (SeenNames.Contains(Spec.Name.ToLower()))
            {
                OutErrors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Duplicate state name '%s'"), *Spec.Name)));
                continue;
            }
            SeenNames.Add(Spec.Name.ToLower());
            OutSpecs.Add(Spec);
        }
    }
    else
    {
        FAnimStateMachineSampleStateSpec Spec;
        Spec.Name = SanitizeAssetNameSegmentForAnimNodeProbe(GetStringParam(Params, TEXT("state_name"), TEXT("State_0")));
        Spec.AnimationAssetPath = GetStringParam(Params, TEXT("animation_asset_path"));
        if (Spec.AnimationAssetPath.IsEmpty())
        {
            Spec.AnimationAssetPath = GetStringParam(Params, TEXT("animation_asset"));
        }
        Spec.Position = GetOptionalVector2DField(Params, TEXT("position"), FVector2D(0.0f, 0.0f));
        Spec.bAlwaysResetOnEntry = GetBoolParam(Params, TEXT("always_reset_on_entry"), false);
        ParseAnimStateMachineSamplePlayerInputs(Params, Spec, OutErrors);
        if (Spec.AnimationAssetPath.IsEmpty())
        {
            OutErrors.Add(MakeShared<FJsonValueString>(TEXT("Missing 'states' array or top-level 'animation_asset_path'")));
            return false;
        }
        OutSpecs.Add(Spec);
    }

    return OutErrors.Num() == 0 && OutSpecs.Num() > 0;
}

TSharedPtr<FJsonObject> AnimStateMachinePlayerInputSpecToJson(const FAnimStateMachineSamplePlayerInputSpec& Spec)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("pin"), Spec.PinName);
    if (!Spec.VariableName.IsEmpty())
    {
        Object->SetStringField(TEXT("variable"), Spec.VariableName);
        Object->SetBoolField(TEXT("create_variable_if_missing"), Spec.bCreateVariableIfMissing);
    }
    if (Spec.bHasDefaultValue)
    {
        Object->SetNumberField(TEXT("default_value"), Spec.DefaultValue);
    }
    return Object;
}

bool ParseAnimStateMachineSampleTransitionSpecs(
    FWork& Work, const TSharedPtr<FJsonObject>& Params,
    int32 MaxTransitions,
    const TSet<FString>& StateNames,
    TArray<FAnimStateMachineSampleTransitionSpec>& OutSpecs,
    TArray<TSharedPtr<FJsonValue>>& OutErrors)
{
    OutSpecs.Reset();

    const TArray<TSharedPtr<FJsonValue>>* TransitionValues = nullptr;
    if (!Params->HasField(TEXT("transitions")))
    {
        return true;
    }
    if (!Params->TryGetArrayField(TEXT("transitions"), TransitionValues))
    {
        OutErrors.Add(MakeShared<FJsonValueString>(TEXT("'transitions' must be an array")));
        return false;
    }
    if (!TransitionValues)
    {
        return true;
    }
    if (TransitionValues->Num() > MaxTransitions)
    {
        OutErrors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("'transitions' contains %d entries, max_transitions=%d"), TransitionValues->Num(), MaxTransitions)));
        return false;
    }

    for (int32 Index = 0; Index < TransitionValues->Num(); ++Index)
    {
        const TSharedPtr<FJsonValue>& TransitionValue = (*TransitionValues)[Index];
        const TSharedPtr<FJsonObject>* TransitionObject = nullptr;
        if (!TransitionValue.IsValid() || !TransitionValue->TryGetObject(TransitionObject) || !TransitionObject || !TransitionObject->IsValid())
        {
            OutErrors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("transitions[%d] must be an object"), Index)));
            continue;
        }

        FAnimStateMachineSampleTransitionSpec Spec;
        Spec.FromState = SanitizeAssetNameSegmentForAnimNodeProbe(GetStringParam(*TransitionObject, TEXT("from_state")));
        Spec.ToState = SanitizeAssetNameSegmentForAnimNodeProbe(GetStringParam(*TransitionObject, TEXT("to_state")));
        Spec.bBidirectional = GetBoolParam(*TransitionObject, TEXT("bidirectional"), false);
        Spec.PriorityOrder = FMath::Max(0, GetClampedIntParam(*TransitionObject, TEXT("priority_order"), 1, 0, 100000));
        Spec.CrossfadeDuration = FMath::Max(0.0f, static_cast<float>((*TransitionObject)->HasField(TEXT("crossfade_duration")) ? (*TransitionObject)->GetNumberField(TEXT("crossfade_duration")) : 0.2));
        Spec.RuleMode = GetStringParam(*TransitionObject, TEXT("rule_mode"), TEXT("always_true")).ToLower();
        Spec.RuleMode.TrimStartAndEndInline();
        Spec.RuleMode.ReplaceInline(TEXT("-"), TEXT("_"));
        if (Spec.RuleMode.Equals(TEXT("boolean_variable"), ESearchCase::IgnoreCase))
        {
            Spec.RuleMode = TEXT("bool_variable");
        }
        else if (Spec.RuleMode.Equals(TEXT("numeric_compare"), ESearchCase::IgnoreCase) ||
                 Spec.RuleMode.Equals(TEXT("float_variable"), ESearchCase::IgnoreCase) ||
                 Spec.RuleMode.Equals(TEXT("numeric_variable"), ESearchCase::IgnoreCase))
        {
            Spec.RuleMode = TEXT("float_compare");
        }
        else if (Spec.RuleMode.Equals(TEXT("enum_variable"), ESearchCase::IgnoreCase) ||
                 Spec.RuleMode.Equals(TEXT("enum_equals"), ESearchCase::IgnoreCase) ||
                 Spec.RuleMode.Equals(TEXT("byte_enum_compare"), ESearchCase::IgnoreCase))
        {
            Spec.RuleMode = TEXT("enum_compare");
        }
        Spec.AutomaticRuleTriggerTime = static_cast<float>((*TransitionObject)->HasField(TEXT("automatic_rule_trigger_time")) ? (*TransitionObject)->GetNumberField(TEXT("automatic_rule_trigger_time")) : -1.0);
        const FString SyncGroup = GetStringParam(*TransitionObject, TEXT("sync_group_name_to_require_valid_markers_rule"));
        if (!SyncGroup.IsEmpty())
        {
            if(!TextSafe(SyncGroup,NAME_SIZE-1)) { OutErrors.Add(MakeShared<FJsonValueString>(TEXT("Sync group name exceeds safe FName bounds")));continue; }
            Spec.SyncGroupNameToRequireValidMarkersRule = FName(*SyncGroup);
        }
        Spec.RuleVariableName = GetStringParam(*TransitionObject, TEXT("rule_variable"));
        if (Spec.RuleVariableName.IsEmpty())
        {
            Spec.RuleVariableName = GetStringParam(*TransitionObject, TEXT("variable_name"));
        }
        if (Spec.RuleVariableName.IsEmpty())
        {
            Spec.RuleVariableName = GetStringParam(*TransitionObject, TEXT("variable"));
        }
        Spec.RuleVariableName.TrimStartAndEndInline();
        Spec.bCreateRuleVariableIfMissing =
            GetBoolParam(*TransitionObject, TEXT("create_rule_variable_if_missing"),
                GetBoolParam(*TransitionObject, TEXT("create_variable_if_missing"), false));
        Spec.RuleVariableType = GetStringParam(*TransitionObject, TEXT("rule_variable_type"));
        Spec.RuleVariableType.TrimStartAndEndInline();
        Spec.bRuleExpectedValue =
            GetBoolParam(*TransitionObject, TEXT("expected_value"),
                GetBoolParam(*TransitionObject, TEXT("rule_expected_value"), true));
        Spec.RuleCompareOperation = GetStringParam(*TransitionObject, TEXT("compare"));
        if (Spec.RuleCompareOperation.IsEmpty())
        {
            Spec.RuleCompareOperation = GetStringParam(*TransitionObject, TEXT("operation"));
        }
        if (Spec.RuleCompareOperation.IsEmpty())
        {
            Spec.RuleCompareOperation = Spec.RuleMode.Equals(TEXT("float_compare"), ESearchCase::IgnoreCase)
                ? TEXT("greater")
                : TEXT("equal");
        }
        Spec.RuleCompareOperation.TrimStartAndEndInline();
        if (Spec.RuleMode.Equals(TEXT("float_compare"), ESearchCase::IgnoreCase))
        {
            if ((*TransitionObject)->HasField(TEXT("compare_value")))
            {
                Spec.RuleCompareValue = (*TransitionObject)->GetNumberField(TEXT("compare_value"));
            }
            else if ((*TransitionObject)->HasField(TEXT("threshold")))
            {
                Spec.RuleCompareValue = (*TransitionObject)->GetNumberField(TEXT("threshold"));
            }
            else if ((*TransitionObject)->HasField(TEXT("value")))
            {
                Spec.RuleCompareValue = (*TransitionObject)->GetNumberField(TEXT("value"));
            }
        }

        FString BlendError;
        if (!ParseAlphaBlendOption(GetStringParam(*TransitionObject, TEXT("blend_mode"), TEXT("Linear")), Spec.BlendMode, BlendError))
        {
            OutErrors.Add(MakeShared<FJsonValueString>(BlendError));
            continue;
        }

        if (!StateNames.Contains(Spec.FromState.ToLower()) || !StateNames.Contains(Spec.ToState.ToLower()))
        {
            OutErrors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Transition references unknown state: '%s' -> '%s'"), *Spec.FromState, *Spec.ToState)));
            continue;
        }
        if (Spec.FromState.Equals(Spec.ToState, ESearchCase::IgnoreCase))
        {
            OutErrors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Self transitions are not supported in v1: '%s'"), *Spec.FromState)));
            continue;
        }
        if (!Spec.RuleMode.Equals(TEXT("always_true"), ESearchCase::IgnoreCase) &&
            !Spec.RuleMode.Equals(TEXT("always_false"), ESearchCase::IgnoreCase) &&
            !Spec.RuleMode.Equals(TEXT("automatic_remaining_time"), ESearchCase::IgnoreCase) &&
            !Spec.RuleMode.Equals(TEXT("bool_variable"), ESearchCase::IgnoreCase) &&
            !Spec.RuleMode.Equals(TEXT("float_compare"), ESearchCase::IgnoreCase) &&
            !Spec.RuleMode.Equals(TEXT("enum_compare"), ESearchCase::IgnoreCase))
        {
            OutErrors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Unsupported rule_mode '%s'"), *Spec.RuleMode)));
            continue;
        }
        if (Spec.RuleMode.Equals(TEXT("bool_variable"), ESearchCase::IgnoreCase) ||
            Spec.RuleMode.Equals(TEXT("float_compare"), ESearchCase::IgnoreCase) ||
            Spec.RuleMode.Equals(TEXT("enum_compare"), ESearchCase::IgnoreCase))
        {
            if (Spec.RuleVariableName.IsEmpty())
            {
                OutErrors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Missing rule_variable for transition '%s' -> '%s'"), *Spec.FromState, *Spec.ToState)));
                continue;
            }

            FString ExpectedType = TEXT("float");
            if (Spec.RuleMode.Equals(TEXT("bool_variable"), ESearchCase::IgnoreCase))
            {
                ExpectedType = TEXT("bool");
            }
            else if (Spec.RuleMode.Equals(TEXT("enum_compare"), ESearchCase::IgnoreCase))
            {
                ExpectedType = TEXT("enum");
            }
            if (Spec.RuleVariableType.IsEmpty())
            {
                Spec.RuleVariableType = ExpectedType;
            }
            const FString NormalizedRuleVariableType = NormalizeTypeName(Spec.RuleVariableType);
            const bool bTypeMatches =
                (ExpectedType == TEXT("bool") && (NormalizedRuleVariableType == TEXT("bool") || NormalizedRuleVariableType == TEXT("boolean"))) ||
                (ExpectedType == TEXT("float") && (NormalizedRuleVariableType == TEXT("float") || NormalizedRuleVariableType == TEXT("double") || NormalizedRuleVariableType == TEXT("real"))) ||
                (ExpectedType == TEXT("enum") && (NormalizedRuleVariableType == TEXT("enum") || NormalizedRuleVariableType == TEXT("byteenum")));
            if (!bTypeMatches)
            {
                OutErrors.Add(MakeShared<FJsonValueString>(FString::Printf(
                    TEXT("rule_variable_type '%s' does not match rule_mode '%s' for transition '%s' -> '%s'"),
                    *Spec.RuleVariableType,
                    *Spec.RuleMode,
                    *Spec.FromState,
                    *Spec.ToState)));
                continue;
            }

            if (Spec.RuleMode.Equals(TEXT("float_compare"), ESearchCase::IgnoreCase))
            {
                FString CompareError;
                if (GetCompareFunctionName(Spec.RuleCompareOperation, TEXT("float"), CompareError).IsNone())
                {
                    OutErrors.Add(MakeShared<FJsonValueString>(CompareError));
                    continue;
                }
            }
            else if (Spec.RuleMode.Equals(TEXT("enum_compare"), ESearchCase::IgnoreCase))
            {
                Spec.RuleEnumType = GetEnumTypeParam(*TransitionObject, TEXT("rule_enum_type"));
                if (Spec.RuleEnumType.IsEmpty())
                {
                    OutErrors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Missing enum_type for enum_compare transition '%s' -> '%s'"), *Spec.FromState, *Spec.ToState)));
                    continue;
                }
                if (Forbidden(Spec.RuleEnumType))
                {
                    OutErrors.Add(MakeShared<FJsonValueString>(
                        FString(TEXT("MCP enum dependency is forbidden: "))+Spec.RuleEnumType));
                    continue;
                }

                UEnum* Enum = LoadEnumForPin(Spec.RuleEnumType,Work);
                if (!Enum)
                {
                    OutErrors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Enum not found for enum_compare transition '%s' -> '%s': %s"), *Spec.FromState, *Spec.ToState, *Spec.RuleEnumType)));
                    continue;
                }

                FString CompareError;
                if (GetCompareFunctionName(Spec.RuleCompareOperation, TEXT("byte"), CompareError).IsNone())
                {
                    OutErrors.Add(MakeShared<FJsonValueString>(CompareError));
                    continue;
                }

                const TSharedPtr<FJsonValue> EnumValueJson =
                    (*TransitionObject)->HasField(TEXT("enum_value")) ? (*TransitionObject)->TryGetField(TEXT("enum_value")) :
                    ((*TransitionObject)->HasField(TEXT("expected_enum_value")) ? (*TransitionObject)->TryGetField(TEXT("expected_enum_value")) :
                    ((*TransitionObject)->HasField(TEXT("compare_value")) ? (*TransitionObject)->TryGetField(TEXT("compare_value")) :
                    (*TransitionObject)->TryGetField(TEXT("value"))));
                FString EnumDefaultError;
                if (!ResolveEnumValueDefaultString(EnumValueJson, Enum, Spec.RuleEnumDefaultValue, Spec.RuleEnumIndex, EnumDefaultError))
                {
                    OutErrors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Invalid enum_compare value for transition '%s' -> '%s': %s"), *Spec.FromState, *Spec.ToState, *EnumDefaultError)));
                    continue;
                }
                Spec.RuleEnumType = Enum->GetPathName();
            }
        }

        OutSpecs.Add(Spec);
    }

    return OutErrors.Num() == 0;
}

TSharedPtr<FJsonObject> AnimStateMachineSampleStateSpecToJson(const FAnimStateMachineSampleStateSpec& Spec)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("name"), Spec.Name);
    Object->SetStringField(TEXT("animation_asset_path"), Spec.AnimationAssetPath);
    Object->SetStringField(TEXT("animation_asset_class"), Spec.AnimationAsset && Spec.AnimationAsset->GetClass() ? Spec.AnimationAsset->GetClass()->GetPathName() : FString());
    Object->SetNumberField(TEXT("position_x"), Spec.Position.X);
    Object->SetNumberField(TEXT("position_y"), Spec.Position.Y);
    Object->SetBoolField(TEXT("always_reset_on_entry"), Spec.bAlwaysResetOnEntry);
    if (Spec.PlayerInputs.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> PlayerInputValues;
        for (const FAnimStateMachineSamplePlayerInputSpec& PlayerInput : Spec.PlayerInputs)
        {
            PlayerInputValues.Add(MakeShared<FJsonValueObject>(AnimStateMachinePlayerInputSpecToJson(PlayerInput)));
        }
        Object->SetArrayField(TEXT("player_inputs"), PlayerInputValues);
    }
    return Object;
}

TSharedPtr<FJsonObject> AnimStateMachineSampleTransitionSpecToJson(const FAnimStateMachineSampleTransitionSpec& Spec)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("from_state"), Spec.FromState);
    Object->SetStringField(TEXT("to_state"), Spec.ToState);
    Object->SetBoolField(TEXT("bidirectional"), Spec.bBidirectional);
    Object->SetNumberField(TEXT("priority_order"), Spec.PriorityOrder);
    Object->SetNumberField(TEXT("crossfade_duration"), Spec.CrossfadeDuration);
    Object->SetStringField(TEXT("blend_mode"), GetAlphaBlendOptionName(Spec.BlendMode));
    Object->SetStringField(TEXT("rule_mode"), Spec.RuleMode);
    Object->SetNumberField(TEXT("automatic_rule_trigger_time"), Spec.AutomaticRuleTriggerTime);
    Object->SetStringField(TEXT("sync_group_name_to_require_valid_markers_rule"), Spec.SyncGroupNameToRequireValidMarkersRule.ToString());
    if (!Spec.RuleVariableName.IsEmpty())
    {
        Object->SetStringField(TEXT("rule_variable"), Spec.RuleVariableName);
        Object->SetBoolField(TEXT("create_rule_variable_if_missing"), Spec.bCreateRuleVariableIfMissing);
        Object->SetStringField(TEXT("rule_variable_type"), Spec.RuleVariableType);
        Object->SetBoolField(TEXT("expected_value"), Spec.bRuleExpectedValue);
        Object->SetStringField(TEXT("compare"), Spec.RuleCompareOperation);
        Object->SetNumberField(TEXT("compare_value"), Spec.RuleCompareValue);
        if (!Spec.RuleEnumType.IsEmpty())
        {
            Object->SetStringField(TEXT("enum_type"), Spec.RuleEnumType);
            Object->SetStringField(TEXT("enum_value"), Spec.RuleEnumDefaultValue);
            Object->SetNumberField(TEXT("enum_index"), Spec.RuleEnumIndex);
        }
    }
    return Object;
}

// Source-derived rule-variable type projection; no property pointer survives a callback.
bool VariableDriven(const FString& Mode) { return Mode==TEXT("bool_variable")||Mode==TEXT("float_compare")||Mode==TEXT("enum_compare"); }
FString PropertyType(const FProperty* Property)
{
    if(CastField<FBoolProperty>(Property)) { return TEXT("bool"); }if(CastField<FFloatProperty>(Property)||CastField<FDoubleProperty>(Property)) { return TEXT("float"); }
    if(const auto* EnumProperty=CastField<FEnumProperty>(Property)) { return EnumProperty->GetEnum()?TEXT("enum:")+EnumProperty->GetEnum()->GetPathName():TEXT("unsupported:enum"); }
    if(const auto* ByteProperty=CastField<FByteProperty>(Property)) { return ByteProperty->Enum?TEXT("enum:")+ByteProperty->Enum->GetPathName():TEXT("byte"); }
    return Property?TEXT("unsupported:")+Property->GetClass()->GetName():FString();
}
FString PinTypeName(const FEdGraphPinType& Type)
{
    if(Type.PinCategory==UEdGraphSchema_K2::PC_Boolean) { return TEXT("bool"); }
    if(Type.PinCategory==UEdGraphSchema_K2::PC_Real||Type.PinCategory==UEdGraphSchema_K2::PC_Float) { return TEXT("float"); }
    if(const UEnum* Enum=GetEnumForPinType(Type)) { return TEXT("enum:")+Enum->GetPathName(); }
    return Type.PinCategory==UEdGraphSchema_K2::PC_Byte?TEXT("byte"):TEXT("unsupported:")+Type.PinCategory.ToString();
}
bool TypesCompatible(const FString& ExpectedRaw,const FString& ActualRaw)
{
    const FString Expected=NormalizeTypeName(ExpectedRaw),Actual=NormalizeTypeName(ActualRaw);
    if(Expected.StartsWith(TEXT("enum:"))) { return Actual.StartsWith(TEXT("enum:"))&&Expected.Equals(Actual,ESearchCase::IgnoreCase); }
    if(Expected==TEXT("enum")||Expected==TEXT("byteenum")) { return Actual.StartsWith(TEXT("enum:")); }
    if(Expected==TEXT("bool")||Expected==TEXT("boolean")) { return Actual==TEXT("bool")||Actual==TEXT("boolean"); }
    if(Expected==TEXT("float")||Expected==TEXT("double")||Expected==TEXT("real")) { return Actual==TEXT("float")||Actual==TEXT("double")||Actual==TEXT("real"); }return Expected==Actual;
}
bool ExistingVariableType(UAnimBlueprint* Blueprint,const FString& Name,FString& Out,FWork& Work)
{
    if(!Work.Current()) { return false; }
    if(!TextSafe(Name,NAME_SIZE-1)) { return Work.Fail(TEXT("variable_name_bound"),TEXT("Variable name exceeds FName/no-NUL bounds; absence was not established")); }
    if(Blueprint->NewVariables.Num()>4096) { return Work.Fail(TEXT("variable_inventory_bound"),TEXT("Variable inventory exceeds4096; absence was not established and no variable writer is admitted")); }
    for(UClass* Class:{Blueprint->GeneratedClass.Get(),Blueprint->SkeletonGeneratedClass.Get(),Blueprint->ParentClass.Get()})
    {
        TSet<UClass*> Seen;for(UClass* Parent=Class;Parent;Parent=Parent->GetSuperClass())
        { if(!Live(Parent)||Seen.Contains(Parent)||Seen.Num()>=64) { Work.Fail(TEXT("variable_class_unreadable"),TEXT("Variable class hierarchy is unreadable/cyclic/deeper than64"));return false; }Seen.Add(Parent); }
        if(Class) { if(const FProperty* Property=FindFProperty<FProperty>(Class,FName(*Name))) { Out=PropertyType(Property);return true; } }
    }
    for(const FBPVariableDescription& Description:Blueprint->NewVariables) { if(Description.VarName==FName(*Name)) { Out=PinTypeName(Description.VarType);return true; } }return false;
}
struct FVariableRequest { FString Name,Type,Default;bool Create=false,HasDefault=false; };
bool EnsureVariables(UAnimBlueprint* Blueprint,const TArray<FAnimStateMachineSampleTransitionSpec>& Transitions,const TArray<FAnimStateMachineSampleStateSpec>& States,bool Rule,TArray<TSharedPtr<FJsonValue>>& Rows,FWork& Work)
{
    const int32 Initial=Work.Errors.Num();TMap<FString,FVariableRequest> Requests;
    if(Rule)
    {
        for(const auto& Spec:Transitions)
        {
            if(!VariableDriven(Spec.RuleMode)) { continue; }const FString Type=Spec.RuleMode==TEXT("bool_variable")?TEXT("bool"):Spec.RuleMode==TEXT("enum_compare")?TEXT("enum:")+Spec.RuleEnumType:TEXT("float");
            const FString Key=Spec.RuleVariableName.ToLower();FVariableRequest* Existing=Requests.Find(Key);
            if(Existing) { if(!TypesCompatible(Existing->Type,Type)) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Rule variable requested with conflicting types: ")+Spec.RuleVariableName)); }Existing->Create|=Spec.bCreateRuleVariableIfMissing; }
            else { FVariableRequest Request;Request.Name=Spec.RuleVariableName;Request.Type=Type;Request.Create=Spec.bCreateRuleVariableIfMissing;Request.Default=Type==TEXT("bool")?TEXT("false"):Type.StartsWith(TEXT("enum:"))?Spec.RuleEnumDefaultValue:TEXT("0.0");Request.HasDefault=true;Requests.Add(Key,MoveTemp(Request)); }
        }
    }
    else
    {
        for(const auto& State:States) { for(const auto& Input:State.PlayerInputs)
        {
            if(Input.VariableName.IsEmpty()) { continue; }FVariableRequest& Request=Requests.FindOrAdd(Input.VariableName.ToLower());
            if(Request.Name.IsEmpty()) { Request.Name=Input.VariableName;Request.Type=TEXT("float"); }
            Request.Create|=Input.bCreateVariableIfMissing;if(Input.bHasDefaultValue&&!Request.HasDefault) { Request.HasDefault=true;Request.Default=FString::SanitizeFloat(Input.DefaultValue); }
        } }
    }
    for(const auto& Pair:Requests)
    {
        if(!Work.Current()) { return false; }const FVariableRequest& Request=Pair.Value;auto Row=MakeShared<FJsonObject>();Rows.Add(MakeShared<FJsonValueObject>(Row));
        Row->SetStringField(TEXT("variable_name"),Request.Name);Row->SetStringField(TEXT("expected_type"),Request.Type);Row->SetBoolField(TEXT("create_if_missing"),Request.Create);Row->SetBoolField(TEXT("created"),false);Row->SetBoolField(TEXT("existed"),false);
        FString Existing;if(ExistingVariableType(Blueprint,Request.Name,Existing,Work))
        { Row->SetBoolField(TEXT("existed"),true);Row->SetStringField(TEXT("existing_type"),Existing);if(!TypesCompatible(Request.Type,Existing)) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Variable type mismatch for ")+Request.Name+TEXT(": expected ")+Request.Type+TEXT(", actual ")+Existing)); }continue; }
        if(!Work.Current()) { return false; }
        if(!Request.Create) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Missing variable; enable create_variable_if_missing: ")+Request.Name));continue; }
        FEdGraphPinType Type;
        if(Request.Type==TEXT("bool")) { Type.PinCategory=UEdGraphSchema_K2::PC_Boolean; }
        else if(Request.Type.StartsWith(TEXT("enum:"))) { UEnum* Enum=LoadEnumForPin(Request.Type.Mid(5),Work);if(!Enum) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Unable to resolve variable enum: ")+Request.Name));continue; }Type.PinCategory=UEdGraphSchema_K2::PC_Byte;Type.PinSubCategoryObject=Enum; }
        else { Type.PinCategory=UEdGraphSchema_K2::PC_Real;Type.PinSubCategory=UEdGraphSchema_K2::PC_Float; }
        const FString Default=Request.HasDefault?Request.Default:TEXT("0.0");const FName Name(*Request.Name);
        if(Blueprint->NewVariables.Num()>=4096) { return Work.Fail(TEXT("variable_inventory_bound"),TEXT("Adding a member would exceed the4096 variable inventory bound")); }
        Work.Writer(TEXT("add_member_variable_calls"));const bool Added=FBlueprintEditorUtils::AddMemberVariable(Blueprint,Name,Type,Default);Work.Count(TEXT("add_member_variable_returns"));
        Work.Result.bChanged|=Added;Work.GraphChanged|=Added;Row->SetBoolField(TEXT("created"),Added);
        if(!Work.Current()) { return false; }if(!Added) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Failed to create variable: ")+Request.Name));continue; }
        const FText Category=FText::FromString(Rule?TEXT("MCP State Machine Rules"):TEXT("MCP State Machine Player Inputs"));
        for(FBPVariableDescription& Description:Blueprint->NewVariables) { if(Description.VarName==Name) { Description.Category=Category;break; } }
        Work.Writer(TEXT("set_variable_category_calls"));FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint,Name,nullptr,Category,true);
        if(!Work.Current()) { return false; }Row->SetStringField(TEXT("created_type"),Request.Type);Row->SetStringField(TEXT("default_value"),Default);
    }
    return Work.Errors.Num()==Initial;
}
UEdGraphPin* PinByName(FGraphContext& Context,UEdGraphNode* Node,const TArray<FString>& Names,EEdGraphPinDirection Direction)
{
    if(!Node||!Context.Refresh()||!Context.Graph->Nodes.Contains(Node)) { return nullptr; }
    for(const FString& Name:Names) { for(UEdGraphPin* Pin:Node->Pins) { if(Context.PinCurrent(Pin)&&Pin->Direction==Direction&&Pin->PinName.ToString().Equals(Name,ESearchCase::IgnoreCase)) { return Pin; } } }return nullptr;
}
UEdGraphPin* DataOutput(FGraphContext& Context,UEdGraphNode* Node)
{ if(!Node||!Context.Refresh()||!Context.Graph->Nodes.Contains(Node)) { return nullptr; }for(UEdGraphPin* Pin:Node->Pins) { if(Context.PinCurrent(Pin)&&Pin->Direction==EGPD_Output&&Pin->PinType.PinCategory!=UEdGraphSchema_K2::PC_Exec) { return Pin; } }return nullptr; }
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
UK2Node_VariableGet* VariableGet(FGraphContext& Context,const FString& Name,int32 X,int32 Y)
{ return ConfiguredNode<UK2Node_VariableGet>(Context,X,Y,[&](UK2Node_VariableGet* Node){Node->VariableReference.SetSelfMember(FName(*Name));}); }
UK2Node_CallFunction* FunctionNode(FGraphContext& Context,FName Name,int32 X,int32 Y)
{
    UFunction* Function=UKismetMathLibrary::StaticClass()->FindFunctionByName(Name);if(!Live(Function)) { return nullptr; }FIdentity Id(Function);
    UK2Node_CallFunction* Node=ConfiguredNode<UK2Node_CallFunction>(Context,X,Y,[&](UK2Node_CallFunction* NewNode){NewNode->SetFromFunction(Function);});
    if(!Id.Current()) { Context.Work.Fail(TEXT("function_changed"),TEXT("Kismet function identity changed during node creation"));return nullptr; }return Node;
}
bool SetDefault(FGraphContext& Context,UEdGraphPin* Pin,const FString& Value,bool Notify=true)
{
    if(!Context.Refresh()||!Context.PinCurrent(Pin)) { return false; }FWork& Work=Context.Work;UEdGraphNode* Owner=Pin->GetOwningNodeUnchecked();
    Work.Writer(TEXT("set_pin_default_calls"));Context.Schema->TrySetDefaultValue(*Pin,Value);Work.GraphChanged=true;Work.Result.bChanged=true;
    if(!Context.Refresh()||!Context.PinCurrent(Pin)) { return Work.Fail(TEXT("default_pin_changed"),TEXT("Pin was replaced by default-setting callback")); }
    if(Notify) { Work.Writer(TEXT("pin_default_changed_calls"));Owner->PinDefaultValueChanged(Pin);if(!Context.Refresh()||!Context.PinCurrent(Pin)) { return Work.Fail(TEXT("default_pin_changed"),TEXT("Pin changed after default notification")); } }return true;
}
bool BreakLinks(FGraphContext& Context,UEdGraphPin* Pin)
{
    if(!Context.Refresh()||!Context.PinCurrent(Pin)) { return false; }if(Pin->LinkedTo.IsEmpty()) { return true; }
    Context.Work.Writer(TEXT("break_all_pin_links_calls"));Pin->BreakAllPinLinks();Context.Work.GraphChanged=true;Context.Work.Result.bChanged=true;
    if(!Context.Refresh()||!Context.PinCurrent(Pin)) { return Context.Work.Fail(TEXT("link_pin_changed"),TEXT("Pin changed after BreakAllPinLinks callback")); }return true;
}
bool Notify(FGraphContext& Context)
{ if(!Context.Refresh()) { return false; }Context.Work.Writer(TEXT("notify_graph_changed_calls"));Context.Graph->NotifyGraphChanged();return Context.Refresh(); }
UAnimGraphNode_AssetPlayerBase* SpawnPlayer(FGraphContext& Context,UAnimationAsset* Asset,const FVector2D& Position)
{
    FWork& Work=Context.Work;if(!Context.Refresh()||!Live(Asset)) { return nullptr; }FIdentity AssetId(Asset);const auto Before=Context.Graph->Nodes;
    Work.SDK(TEXT("get_player_node_class_calls"));UClass* Class=GetNodeClassForAsset(Asset->GetClass());
    if(!Context.Refresh()||!AssetId.Current()||!Live(Class)||!Class->IsChildOf(UAnimGraphNode_AssetPlayerBase::StaticClass())||Class==UAnimGraphNode_AssetPlayerBase::StaticClass()||Class->HasAnyClassFlags(CLASS_Abstract|CLASS_NewerVersionExists)||!Live(Class->ClassWithin)||!GetTransientPackage()->IsA(Class->ClassWithin)) { return nullptr; }
    if(!ClassAllowed(Class,Work)) { return nullptr; }FIdentity ClassId(Class);
    Work.Writer(TEXT("player_template_create_calls"));UAnimGraphNode_AssetPlayerBase* Template=NewObject<UAnimGraphNode_AssetPlayerBase>(GetTransientPackage(),Class);TStrongObjectPtr<UAnimGraphNode_AssetPlayerBase> Held(Template);
    if(!Context.Refresh()||!AssetId.Current()||!ClassId.Current()||!Live(Template)) { return nullptr; }FIdentity TemplateId(Template);
    Work.Writer(TEXT("set_animation_asset_calls"));Template->SetAnimationAsset(Asset);
    if(!Context.Refresh()||!AssetId.Current()||!ClassId.Current()||!TemplateId.Current()) { return nullptr; }
    Work.Writer(TEXT("copy_animation_settings_calls"));Template->CopySettingsFromAnimationAsset(Asset);
    if(!Context.Refresh()||!AssetId.Current()||!TemplateId.Current()) { return nullptr; }
    FEdGraphSchemaAction_K2NewNode Action;Action.NodeTemplate=Template;Work.Writer(TEXT("player_spawn_action_calls"));Action.PerformAction(Context.Graph,nullptr,FVector2f(float(Position.X),float(Position.Y)),false);Work.Count(TEXT("player_spawn_action_returns"));
    Work.GraphChanged=true;Work.Result.bChanged=true;if(!Context.Refresh()||!AssetId.Current()) { return nullptr; }
    for(UEdGraphNode* Node:Context.Graph->Nodes) { if(!Before.Contains(Node)) { if(auto* Player=Cast<UAnimGraphNode_AssetPlayerBase>(Node)) { Context.Anchor(Player);return Player; } } }return nullptr;
}
UAnimStateNode* SpawnState(FGraphContext& Context,const FAnimStateMachineSampleStateSpec& Spec)
{
    if(!Context.Refresh()) { return nullptr; }FWork& Work=Context.Work;
    Work.Writer(TEXT("state_template_create_calls"));UAnimStateNode* Template=NewObject<UAnimStateNode>();TStrongObjectPtr<UAnimStateNode> Held(Template);
    if(!Context.Refresh()||!Live(Template)) { return nullptr; }
    FEdGraphSchemaAction_NewStateNode Action;Action.NodeTemplate=Template;Work.Writer(TEXT("state_spawn_action_calls"));
    UEdGraphNode* Spawned=Action.PerformAction(Context.Graph,nullptr,FVector2f(float(Spec.Position.X),float(Spec.Position.Y)),false);Work.Count(TEXT("state_spawn_action_returns"));Work.GraphChanged=true;Work.Result.bChanged=true;
    if(!Context.Refresh()||!Live(Spawned)||!Context.Graph->Nodes.Contains(Spawned)) { return nullptr; }UAnimStateNode* State=Cast<UAnimStateNode>(Spawned);if(!State) { return nullptr; }Context.Anchor(State);
    Work.Writer(TEXT("state_rename_calls"));static_cast<UEdGraphNode*>(State)->OnRenameNode(Spec.Name);
    if(!Context.Refresh()) { return nullptr; }Work.Writer(TEXT("modify_state_calls"));State->Modify();if(!Context.Refresh()) { return nullptr; }State->bAlwaysResetOnEntry=Spec.bAlwaysResetOnEntry;return State;
}
UEdGraphPin* PoseSink(FGraphContext& Context,UAnimStateNode* State)
{
    if(!Live(State)||!Live(State->GetGraph())||!State->GetGraph()->Nodes.Contains(State)) { return nullptr; }FIdentity StateId(State);
    if(!Context.Refresh()||!StateId.Current()||State->BoundGraph!=Context.Graph) { return nullptr; }
    UAnimationStateGraph* Graph=Cast<UAnimationStateGraph>(Context.Graph);if(!Graph||!Live(Graph->MyResultNode)||!Graph->Nodes.Contains(Graph->MyResultNode)) { return nullptr; }
    FIdentity ResultId(Graph->MyResultNode);Context.Work.SDK(TEXT("state_pose_sink_calls"));UEdGraphPin* Pin=State->GetPoseSinkPinInsideState();
    if(!Context.Refresh()||!StateId.Current()||!ResultId.Current()||State->BoundGraph!=Graph||!State->GetGraph()->Nodes.Contains(State)) { Context.Work.Fail(TEXT("state_pose_sink_changed"),TEXT("State/result identity or membership changed during pose-sink query"));return nullptr; }
    return Context.PinCurrent(Pin)?Pin:nullptr;
}
template<class T> T* NamedNode(FGraphContext& Context,const FString& Name)
{
    if(!Context.Refresh()) { return nullptr; }const auto Slots=Context.Graph->Nodes;
    for(UEdGraphNode* Node:Slots) { if(auto* Typed=Cast<T>(Node))
    {
        Context.Work.SDK(TEXT("node_name_calls"));FString CurrentName;
        if constexpr(std::is_same_v<T,UAnimStateNode>)
        { if(Typed->BoundGraph&&(!Live(Typed->BoundGraph)||Typed->BoundGraph->GetOuter()!=Typed||Typed->BoundGraph->GetPackage()!=Context.Graph->GetPackage())) { Context.Work.Fail(TEXT("state_bound_graph_invalid"),TEXT("State bound graph is unreadable or foreign"));return nullptr; }CurrentName=static_cast<UAnimStateNodeBase*>(Typed)->GetStateName(); }
        else { if(Typed->EditorStateMachineGraph&&(!Live(Typed->EditorStateMachineGraph)||Typed->EditorStateMachineGraph->GetOuter()!=Typed||Typed->EditorStateMachineGraph->GetPackage()!=Context.Graph->GetPackage())) { Context.Work.Fail(TEXT("machine_bound_graph_invalid"),TEXT("Machine bound graph is unreadable or foreign"));return nullptr; }CurrentName=Typed->GetStateMachineName(); }
        if(!Context.Refresh()) { return nullptr; }if(CurrentName.Equals(Name,ESearchCase::IgnoreCase)) { Context.Anchor(Typed);return Typed; }
    } }return nullptr;
}
UAnimStateTransitionNode* FindTransition(FGraphContext& Context,UAnimStateNode* From,UAnimStateNode* To)
{
    if(!From||!To||!Context.Refresh()) { return nullptr; }const auto Slots=Context.Graph->Nodes;
    for(UEdGraphNode* Node:Slots) { if(auto* Transition=Cast<UAnimStateTransitionNode>(Node))
    {
        // Public getters index Pins[0/1]; reject malformed transition slots first.
        if(Transition->Pins.Num()<2||!Context.PinCurrent(Transition->Pins[0])||!Context.PinCurrent(Transition->Pins[1])) { Context.Work.Fail(TEXT("transition_pins_unreadable"),TEXT("Transition requires two live pins before endpoint getters"));return nullptr; }
        Context.Work.SDK(TEXT("transition_endpoint_queries"));auto* Previous=Transition->GetPreviousState();if(!Context.Refresh()) { return nullptr; }auto* Next=Transition->GetNextState();if(!Context.Refresh()) { return nullptr; }
        if(Previous==From&&Next==To) { Context.Anchor(Transition);return Transition; }
    } }return nullptr;
}
bool MachineConnection(FGraphContext& Context,UEdGraphPin* From,UEdGraphPin* To)
{
    if(!Context.Refresh()||!Context.PinCurrent(From)||!Context.PinCurrent(To)) { return false; }
    Context.Work.Writer(TEXT("try_create_connection_calls"));const bool Connected=Context.Schema->TryCreateConnection(From,To);Context.Work.Count(TEXT("try_create_connection_returns"));Context.Work.GraphChanged|=Connected;Context.Work.Result.bChanged|=Connected;
    return Context.Refresh()&&Connected;
}
UEdGraphPin* DirectionPin(FGraphContext& Context,UEdGraphNode* Node,EEdGraphPinDirection Direction)
{ if(!Node||!Context.Refresh()||!Context.Graph->Nodes.Contains(Node)) { return nullptr; }for(UEdGraphPin* Pin:Node->Pins) { if(Context.PinCurrent(Pin)&&Pin->Direction==Direction) { return Pin; } }return nullptr; }
bool ApplyPlayerInputs(FGraphContext& Context,UAnimStateNode* State,UAnimGraphNode_AssetPlayerBase*& Player,const FAnimStateMachineSampleStateSpec& Spec,TArray<TSharedPtr<FJsonValue>>& Rows)
{
    FWork& Work=Context.Work;const int32 Initial=Work.Errors.Num();if(Spec.PlayerInputs.IsEmpty()) { return true; }
    if(!Cast<UBlendSpace>(Spec.AnimationAsset)) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("player_inputs are supported only for BlendSpace-backed states: ")+Spec.Name));return false; }
    if(!Context.Refresh()) { return false; }
    if(!Player)
    {
        UEdGraphPin* Sink=PoseSink(Context,State);if(!Context.Refresh()) { return false; }
        if(Context.PinCurrent(Sink)) { for(UEdGraphPin* Linked:Sink->LinkedTo) { if(Context.PinCurrent(Linked)) { if(auto* Found=Cast<UAnimGraphNode_AssetPlayerBase>(Linked->GetOwningNodeUnchecked())) { Player=Found;break; } } } }
        if(!Player) { Player=FirstNode<UAnimGraphNode_AssetPlayerBase>(Context); }
    }
    if(!Player) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("State has no asset player for player_inputs: ")+Spec.Name));return false; }Context.Anchor(Player);
    for(int32 Index=0;Index<Spec.PlayerInputs.Num();++Index)
    {
        if(!Context.Refresh()) { return false; }const auto& Input=Spec.PlayerInputs[Index];auto Row=AnimStateMachinePlayerInputSpecToJson(Input);Rows.Add(MakeShared<FJsonValueObject>(Row));Row->SetBoolField(TEXT("applied"),false);Row->SetStringField(TEXT("asset_player_node_id"),Player->NodeGuid.ToString());
        UEdGraphPin* Pin=nullptr;const auto PinSlots=Player->Pins;
        for(UEdGraphPin* Candidate:PinSlots)
        {
            if(!Context.PinCurrent(Candidate)||Candidate->Direction!=EGPD_Input||Candidate->ParentPin) { continue; }
            Work.SDK(TEXT("pin_display_name_calls"));const FString Display=Candidate->GetDisplayName().ToString();if(!Context.Refresh()||!Context.PinCurrent(Candidate)) { return false; }
            if(Candidate->PinName.ToString().Equals(Input.PinName,ESearchCase::IgnoreCase)||Candidate->PinFriendlyName.ToString().Equals(Input.PinName,ESearchCase::IgnoreCase)||Display.Equals(Input.PinName,ESearchCase::IgnoreCase)) { Pin=Candidate;break; }
        }
        if(!Pin) { TArray<TSharedPtr<FJsonValue>> Available;for(UEdGraphPin* Current:Player->Pins) { if(Context.PinCurrent(Current)&&Current->Direction==EGPD_Input&&!Current->ParentPin) { Available.Add(MakeShared<FJsonValueObject>(PinJson(Current,Context))); } }Row->SetArrayField(TEXT("available_input_pins"),Available);Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Asset player input pin not found: ")+Spec.Name+TEXT(".")+Input.PinName));continue; }
        Row->SetObjectField(TEXT("input_pin"),PinJson(Pin,Context));
        if(Pin->PinType.PinCategory!=UEdGraphSchema_K2::PC_Real&&Pin->PinType.PinCategory!=UEdGraphSchema_K2::PC_Float) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Player input is not float/real: ")+Input.PinName));continue; }
        if(!Input.VariableName.IsEmpty())
        {
            UK2Node_VariableGet* Existing=nullptr;for(UEdGraphPin* Linked:Pin->LinkedTo) { if(Context.PinCurrent(Linked)) { if(auto* Get=Cast<UK2Node_VariableGet>(Linked->GetOwningNodeUnchecked())) { if(Get->GetVarNameString().Equals(Input.VariableName,ESearchCase::CaseSensitive)) { Existing=Get;break; } } } }
            if(Existing) { Row->SetStringField(TEXT("variable_get_node_id"),Existing->NodeGuid.ToString());Row->SetBoolField(TEXT("reused_variable_get"),true);Row->SetBoolField(TEXT("linked_to_input"),true);Row->SetBoolField(TEXT("applied"),true);continue; }
            if(!BreakLinks(Context,Pin)) { return false; }auto* Get=VariableGet(Context,Input.VariableName,Player->NodePosX-260,Player->NodePosY+120+Index*110);UEdGraphPin* Output=DataOutput(Context,Get);
            FString Error;bool Changed=false;if(!Get||!Connect(Context,Output,Pin,false,Changed,Error)) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Failed to bind player input: ")+Input.PinName+TEXT(" ")+Error));if(!Work.Current()) { return false; }continue; }
            Row->SetStringField(TEXT("variable_get_node_id"),Get->NodeGuid.ToString());Row->SetBoolField(TEXT("reused_variable_get"),false);Row->SetBoolField(TEXT("linked_to_input"),true);
        }
        else
        {
            if(!BreakLinks(Context,Pin)||!SetDefault(Context,Pin,FString::SanitizeFloat(Input.DefaultValue))) { return false; }
            Row->SetStringField(TEXT("requested_default_value"),FString::SanitizeFloat(Input.DefaultValue));Row->SetStringField(TEXT("applied_default_value"),Pin->DefaultValue);Row->SetBoolField(TEXT("linked_to_input"),false);
        }
        Row->SetObjectField(TEXT("input_pin"),PinJson(Pin,Context));Row->SetBoolField(TEXT("applied"),true);
    }
    if(Work.GraphChanged&&!Notify(Context)) { return false; }return Work.Errors.Num()==Initial;
}
bool RuleLink(FGraphContext& Context,UEdGraphPin* From,UEdGraphPin* To,const TCHAR* Label)
{
    FString Error;bool Changed=false;if(Connect(Context,From,To,false,Changed,Error)) { return true; }
    Context.Work.Errors.Add(MakeShared<FJsonValueString>(FString(TEXT("Failed to link transition rule '"))+Label+TEXT("': ")+Error));return false;
}
bool DestroyCreated(FGraphContext& Context,UEdGraphNode* Node)
{
    if(!Node||!Context.Refresh()||!Context.Graph->Nodes.Contains(Node)) { return false; }
    // Remove this particular expected-deletion anchor before public DestroyNode.
    Context.Anchors.RemoveAll([&](const TUniquePtr<FIdentity>& Id){return Id->Object.Get()==Node;});
    Context.Work.Writer(TEXT("destroy_failed_rule_node_calls"));Node->DestroyNode();Context.Work.GraphChanged=true;Context.Work.Result.bChanged=true;return Context.Refresh();
}
bool ApplyRule(UAnimBlueprint* Blueprint,UAnimStateTransitionNode* Transition,const FAnimStateMachineSampleTransitionSpec& Spec,TSharedRef<FJsonObject> Row,FWork& Work)
{
    Row->SetStringField(TEXT("rule_mode"),Spec.RuleMode);Row->SetBoolField(TEXT("applied"),false);
    UAnimationTransitionGraph* Graph=Cast<UAnimationTransitionGraph>(Transition->BoundGraph);if(!Live(Graph)) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Transition has no rule graph")));return false; }
    FGraphContext Context(Work,Blueprint,Graph);if(!Context.Refresh()) { return false; }
    Work.SDK(TEXT("transition_result_queries"));UAnimGraphNode_TransitionResult* Result=Graph->GetResultNode();
    if(!Context.Refresh()||!Live(Result)||!Graph->Nodes.Contains(Result)) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Transition has no result node")));return false; }Context.Anchor(Result);
    UEdGraphPin* CanEnter=PinByName(Context,Result,{TEXT("bCanEnterTransition")},EGPD_Input);
    if(!CanEnter||!BreakLinks(Context,CanEnter)) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Transition has no usable result pin")));return false; }
    Work.Writer(TEXT("modify_rule_graph_calls"));Graph->Modify();if(!Context.Refresh()) { return false; }
    Work.Writer(TEXT("modify_rule_result_calls"));Result->Modify();if(!Context.Refresh()||!Context.PinCurrent(CanEnter)) { return false; }
    Work.Writer(TEXT("modify_rule_pin_calls"));CanEnter->Modify();if(!Context.Refresh()||!Context.PinCurrent(CanEnter)) { return false; }
    const bool Constant=Spec.RuleMode!=TEXT("always_false");Result->Node.bCanEnterTransition=Constant;CanEnter->DefaultValue=Constant?TEXT("true"):TEXT("false");Work.GraphChanged=true;Work.Result.bChanged=true;
    Row->SetStringField(TEXT("result_node_id"),Result->NodeGuid.ToString());Row->SetStringField(TEXT("result_pin"),TEXT("bCanEnterTransition"));Row->SetBoolField(TEXT("linked_to_result"),false);
    if(!VariableDriven(Spec.RuleMode)) { Row->SetBoolField(TEXT("constant_result"),Constant);Row->SetBoolField(TEXT("applied"),true);return Notify(Context); }
    UK2Node_VariableGet* Get=VariableGet(Context,Spec.RuleVariableName,-480,0);UEdGraphPin* VariableOut=DataOutput(Context,Get);
    if(!Get||!VariableOut) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Failed to create rule variable get: ")+Spec.RuleVariableName));return false; }
    Row->SetStringField(TEXT("rule_variable"),Spec.RuleVariableName);Row->SetStringField(TEXT("variable_get_node_id"),Get->NodeGuid.ToString());
    if(Spec.RuleMode==TEXT("bool_variable"))
    {
        Row->SetBoolField(TEXT("expected_value"),Spec.bRuleExpectedValue);
        if(Spec.bRuleExpectedValue) { if(!RuleLink(Context,VariableOut,CanEnter,TEXT("bool_variable"))) { return false; } }
        else
        {
            auto* Not=FunctionNode(Context,GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary,Not_PreBool),-240,0);
            auto* Input=PinByName(Context,Not,{TEXT("A"),TEXT("bA"),TEXT("Value")},EGPD_Input);auto* Output=DataOutput(Context,Not);
            if(!Not||!Input||!Output||!RuleLink(Context,VariableOut,Input,TEXT("bool_variable_not_input"))||!RuleLink(Context,Output,CanEnter,TEXT("bool_variable_not_result"))) { return false; }
            Row->SetStringField(TEXT("not_node_id"),Not->NodeGuid.ToString());
        }
    }
    else if(Spec.RuleMode==TEXT("enum_compare"))
    {
        UEnum* Enum=LoadEnumForPin(Spec.RuleEnumType,Work);if(!Enum||!Context.Refresh()) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Enum unavailable while authoring rule")));return false; }
        auto* Literal=ConfiguredNode<UK2Node_EnumLiteral>(Context,-480,180,[&](UK2Node_EnumLiteral* Node){Node->Enum=Enum;});
        auto* EnumInput=PinByName(Context,Literal,{UK2Node_EnumLiteral::GetEnumInputPinName().ToString()},EGPD_Input);auto* EnumOut=DataOutput(Context,Literal);
        if(!Literal||!EnumInput||!EnumOut) { DestroyCreated(Context,Literal);Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Enum literal has no usable pins")));return false; }
        if(!SetDefault(Context,EnumInput,Spec.RuleEnumDefaultValue,false)) { return false; }
        Work.SDK(TEXT("current_pin_default_validation_calls"));const FString DefaultError=Context.Schema->IsCurrentPinDefaultValid(EnumInput);
        if(!Context.Refresh()||!Context.PinCurrent(EnumInput)||!Work.Current()) { return false; }
        if(!DefaultError.IsEmpty()||Enum->GetIndexByNameString(EnumInput->DefaultValue)!=Spec.RuleEnumIndex)
        { DestroyCreated(Context,Literal);Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Enum literal default was not accepted: ")+DefaultError));return false; }
        Work.Writer(TEXT("pin_default_changed_calls"));Literal->PinDefaultValueChanged(EnumInput);if(!Context.Refresh()) { return false; }
        FString Error;const FName Function=GetCompareFunctionName(Spec.RuleCompareOperation,TEXT("byte"),Error);UK2Node_EnumEquality* Compare=nullptr;
        if(Function==GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary,NotEqual_ByteByte)) { Compare=ConfiguredNode<UK2Node_EnumInequality>(Context,-240,90,[](UK2Node_EnumInequality*){}); }
        else { Compare=ConfiguredNode<UK2Node_EnumEquality>(Context,-240,90,[](UK2Node_EnumEquality*){}); }
        auto* A=PinByName(Context,Compare,{TEXT("A")},EGPD_Input);auto* B=PinByName(Context,Compare,{TEXT("B")},EGPD_Input);auto* Output=DataOutput(Context,Compare);
        if(!Compare||!A||!B||!Output) { DestroyCreated(Context,Compare);Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Enum compare has no usable pins")));return false; }
        if(!RuleLink(Context,VariableOut,A,TEXT("enum_compare_variable"))||!RuleLink(Context,EnumOut,B,TEXT("enum_compare_literal"))||!RuleLink(Context,Output,CanEnter,TEXT("enum_compare_result"))) { return false; }
        Row->SetStringField(TEXT("enum_type"),Enum->GetPathName());Row->SetStringField(TEXT("enum_value"),Spec.RuleEnumDefaultValue);Row->SetNumberField(TEXT("enum_index"),Spec.RuleEnumIndex);Row->SetStringField(TEXT("enum_literal_node_id"),Literal->NodeGuid.ToString());Row->SetStringField(TEXT("compare_node_id"),Compare->NodeGuid.ToString());Row->SetStringField(TEXT("compare"),Spec.RuleCompareOperation);
    }
    else
    {
        FString Error;const FName Function=GetCompareFunctionName(Spec.RuleCompareOperation,TEXT("float"),Error);auto* Compare=FunctionNode(Context,Function,-240,0);
        auto* A=PinByName(Context,Compare,{TEXT("A"),TEXT("Float"),TEXT("Double"),TEXT("Value")},EGPD_Input);auto* B=PinByName(Context,Compare,{TEXT("B"),TEXT("Threshold"),TEXT("CompareValue")},EGPD_Input);auto* Output=DataOutput(Context,Compare);
        if(!Compare||!A||!B||!Output) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Failed to create float compare node: ")+Error));return false; }
        if(!SetDefault(Context,B,FString::SanitizeFloat(Spec.RuleCompareValue),false)||!RuleLink(Context,VariableOut,A,TEXT("float_compare_input"))||!RuleLink(Context,Output,CanEnter,TEXT("float_compare_result"))) { return false; }
        Row->SetStringField(TEXT("compare_node_id"),Compare->NodeGuid.ToString());Row->SetStringField(TEXT("compare"),Spec.RuleCompareOperation);Row->SetNumberField(TEXT("compare_value"),Spec.RuleCompareValue);
    }
    Row->SetBoolField(TEXT("linked_to_result"),true);Row->SetBoolField(TEXT("applied"),true);return Notify(Context);
}
bool DecodeProjection(const FString& Text,const TArray<FString>& Keys,TSharedPtr<FJsonObject> Params,FWork& Work)
{
    TSharedPtr<FJsonValue> Value;const TSharedPtr<FJsonObject>* Object=nullptr;
    if(!Decode(Text,Value)||!Value->TryGetObject(Object)||!Object||!Object->IsValid()) { return Work.Fail(TEXT("private_projection_invalid"),TEXT("Private projection must be a bounded JSON object")); }
    for(const auto& Pair:(*Object)->Values) { const FString Key(Pair.Key.ToView());if(!Keys.Contains(Key)) { return Work.Fail(TEXT("private_projection_key"),TEXT("Unsupported private projection key: ")+Key); }Params->SetField(Key,Pair.Value); }return true;
}
bool NamesAndNumbers(const TArray<FAnimStateMachineSampleStateSpec>& States,const TArray<FAnimStateMachineSampleTransitionSpec>& Transitions,FWork& Work)
{
    for(const auto& State:States)
    {
        if(!TextSafe(State.Name,NAME_SIZE-1)||!TextSafe(State.AnimationAssetPath,NAME_SIZE-1)||!FMath::IsFinite(State.Position.X)||!FMath::IsFinite(State.Position.Y)||FMath::Abs(State.Position.X)>double(MAX_int32)||FMath::Abs(State.Position.Y)>double(MAX_int32)) { return Work.Fail(TEXT("state_safe_bounds"),TEXT("State name/path/position exceeds finite name/int32 placement bounds")); }
        for(const auto& Input:State.PlayerInputs) { if(!TextSafe(Input.PinName,NAME_SIZE-1)||!TextSafe(Input.VariableName,NAME_SIZE-1)||!FMath::IsFinite(Input.DefaultValue)||FMath::Abs(Input.DefaultValue)>double(MAX_flt)) { return Work.Fail(TEXT("player_input_safe_bounds"),TEXT("Player input name/value exceeds FName/finite float bounds")); } }
    }
    for(const auto& Transition:Transitions)
    {
        if(!TextSafe(Transition.RuleVariableName,NAME_SIZE-1)||!TextSafe(Transition.RuleEnumType,NAME_SIZE-1)||!TextSafe(Transition.RuleEnumDefaultValue,NAME_SIZE-1)||!FMath::IsFinite(Transition.CrossfadeDuration)||!FMath::IsFinite(Transition.AutomaticRuleTriggerTime)||!FMath::IsFinite(Transition.RuleCompareValue)) { return Work.Fail(TEXT("transition_safe_bounds"),TEXT("Transition name/numeric input exceeds finite SDK bounds")); }
    }return true;
}
} // namespace

Variable::FResult Ensure(const FString& SourceAnimBPPath,const FString& StatesProjectionJson,const FString& TransitionsProjectionJson,const FString& TargetAnimBPPath,const FString& TargetRoot,const FString& VariantName,const FString& StateMachineName,const FString& EntryState,const FString& AnimationAssetPath,bool bConnectDemoMachineToOutput,bool bReplaceExistingRootLink,bool bReplaceExistingEntryLink,bool bAllowExistingTargetReuse,bool bOverwriteExisting,bool bCompile,bool bSaveAsset,bool bDryRun,bool bValidateOnly,int64 MaxStates,int64 MaxTransitions,const FString& PrivateContextJson)
{
    FWork Work(bCompile,bSaveAsset,bDryRun||bValidateOnly);Work.Legacy->SetBoolField(TEXT("original_assets_modified"),false);
    if(Running) { Work.Fail(TEXT("busy"),TEXT("A state-machine variant operation is already active; no retry"));return Work.Finish(); }Running=true;ON_SCOPE_EXIT { Running=false; };
    if(!IsEditorGameThread()||IsPlaySessionActive()) { Work.Fail(TEXT("editor_not_idle"),TEXT("Operation requires the idle Editor game thread"));return Work.Finish(); }
    TSharedPtr<FJsonObject> Params=MakeShared<FJsonObject>();
    if(!DecodeProjection(StatesProjectionJson,{TEXT("states")},Params,Work)||!DecodeProjection(TransitionsProjectionJson,{TEXT("transitions")},Params,Work)) { return Work.Finish(); }
    if(!EntryState.IsEmpty()) { Params->SetStringField(TEXT("entry_state"),EntryState); }if(!AnimationAssetPath.IsEmpty()) { Params->SetStringField(TEXT("animation_asset_path"),AnimationAssetPath); }
    if(!DecodeProjection(PrivateContextJson,{TEXT("graph_name"),TEXT("graph_type"),TEXT("target_blueprint_path"),TEXT("entry_state"),TEXT("replace_existing_sample_machine"),TEXT("state_name"),TEXT("animation_asset"),TEXT("position"),TEXT("always_reset_on_entry"),TEXT("player_inputs"),TEXT("asset_player_inputs"),TEXT("blendspace_inputs")},Params,Work)) { return Work.Finish(); }
    for(const FString* Text:{&SourceAnimBPPath,&TargetAnimBPPath,&TargetRoot,&VariantName,&StateMachineName,&EntryState,&AnimationAssetPath}) { if(!TextSafe(*Text,NAME_SIZE-1)) { Work.Fail(TEXT("scalar_bound"),TEXT("Scalar name/path exceeds NAME_SIZE/no-NUL bound"));return Work.Finish(); } }
    const FString Source=SourceAnimBPPath.TrimStartAndEnd();if(Source.IsEmpty()) { Work.Fail(TEXT("source_missing"),TEXT("Missing 'source_anim_bp_path' parameter"));return Work.Finish(); }
    Work.Result.Stage=TEXT("load_source_anim_blueprint");UAnimBlueprint* Blueprint=Cast<UAnimBlueprint>(FindBlueprint(Source,Work));
    if(!Work.Current()||!Live(Blueprint)||!Live(Blueprint->GetPackage())||!ObjectAllowed(Blueprint,Work)) { Work.Fail(TEXT("source_not_found"),TEXT("Source Anim Blueprint not found: ")+Source);return Work.Finish(); }
    Work.Sources.Add(MakeUnique<FIdentity>(Blueprint,true));Work.Legacy->SetStringField(TEXT("source_anim_bp_path"),Blueprint->GetPathName());Work.Legacy->SetStringField(TEXT("source_package"),Blueprint->GetPackage()->GetName());
    if(Blueprint->TargetSkeleton) { if(!Live(Blueprint->TargetSkeleton)) { Work.Fail(TEXT("source_skeleton_unreadable"),TEXT("Source TargetSkeleton is not loaded"));return Work.Finish(); }Work.CommonSkeleton=MakeUnique<FIdentity>(Blueprint->TargetSkeleton); }
    if(!RuntimePackageGuard(Blueprint,Work,TEXT("source_dependency_projection"))) { return Work.Finish(); }
    const FString Root=NormalizePackage(TargetRoot);FString Target=NormalizePackage(TargetAnimBPPath);if(Target.IsEmpty()) { Target=NormalizePackage(GetStringParam(Params,TEXT("target_blueprint_path"))); }
    const FString Variant=VariantSegment(VariantName);if(Target.IsEmpty()) { Target=Root+TEXT("/")+VariantSegment(Blueprint->GetName())+TEXT("_")+Variant; }
    Work.Legacy->SetStringField(TEXT("target_anim_bp_path"),Target);Work.Legacy->SetStringField(TEXT("target_root"),Root);Work.Legacy->SetStringField(TEXT("variant_name"),Variant);
    if(!ValidGamePackage(Root)||!ValidGamePackage(Target)||!SamplePath(Target)||Target.Equals(Blueprint->GetPackage()->GetName(),ESearchCase::IgnoreCase)) { Work.Fail(TEXT("target_scope_invalid"),TEXT("Target must be a distinct valid sample /Game package; source overwrite is refused"));return Work.Finish(); }
    if(GetBoolParam(Params,TEXT("replace_existing_sample_machine"),false)) { Work.Fail(TEXT("replace_machine_unsupported"),TEXT("replace_existing_sample_machine is not supported in v1; use a new state_machine_name or overwrite the whole target sample asset"));return Work.Finish(); }
    const int32 StateLimit=int32(FMath::Clamp<int64>(MaxStates,1,64)),TransitionLimit=int32(FMath::Clamp<int64>(MaxTransitions,0,128));
    TArray<FAnimStateMachineSampleStateSpec> States;TArray<FAnimStateMachineSampleTransitionSpec> Transitions;
    Work.Result.Stage=TEXT("parse_state_specs");if(!ParseAnimStateMachineSampleStateSpecs(Params,StateLimit,States,Work.Errors)) { Work.Fail(TEXT("state_specs_invalid"),TEXT("Invalid state specs"));return Work.Finish(); }
    if(!NamesAndNumbers(States,{},Work)) { return Work.Finish(); }TSet<FString> StateNames;
    Work.Result.Stage=TEXT("load_state_animation_assets");
    for(auto& State:States)
    {
        StateNames.Add(State.Name.ToLower());State.AnimationAsset=Cast<UAnimationAsset>(AssetLoad(ObjectPath(State.AnimationAssetPath,false),Work));
        if(!Work.Current()) { return Work.Finish(); }
        if(!Live(State.AnimationAsset)||!Live(State.AnimationAsset->GetPackage())||!ObjectAllowed(State.AnimationAsset,Work)) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Animation asset not found or not UAnimationAsset for state '")+State.Name+TEXT("': ")+State.AnimationAssetPath)); }
        else { Work.Inputs.Add(MakeUnique<FIdentity>(State.AnimationAsset)); }
    }
    Work.Result.Stage=TEXT("parse_transition_specs");ParseAnimStateMachineSampleTransitionSpecs(Work,Params,TransitionLimit,StateNames,Transitions,Work.Errors);
    if(!Work.Current()) { return Work.Finish(); }if(!Work.Errors.IsEmpty()) { Work.Fail(TEXT("request_invalid"),TEXT("Invalid state-machine sample request"));return Work.Finish(); }
    if(!NamesAndNumbers(States,Transitions,Work)) { return Work.Finish(); }
    const FString Entry=VariantSegment(GetStringParam(Params,TEXT("entry_state"),States[0].Name)),MachineName=VariantSegment(StateMachineName);
    if(!TextSafe(Entry,NAME_SIZE-1)||!TextSafe(MachineName,NAME_SIZE-1)||!StateNames.Contains(Entry.ToLower())) { Work.Fail(TEXT("entry_invalid"),TEXT("entry_state is not in states: ")+Entry);return Work.Finish(); }
    Work.Legacy->SetStringField(TEXT("entry_state"),Entry);Work.Legacy->SetStringField(TEXT("state_machine_name"),MachineName);Work.Legacy->SetBoolField(TEXT("connect_demo_machine_to_output"),bConnectDemoMachineToOutput);
    TArray<TSharedPtr<FJsonValue>> PlannedStates,PlannedTransitions;for(const auto& State:States) { PlannedStates.Add(MakeShared<FJsonValueObject>(AnimStateMachineSampleStateSpecToJson(State))); }for(const auto& Transition:Transitions) { PlannedTransitions.Add(MakeShared<FJsonValueObject>(AnimStateMachineSampleTransitionSpecToJson(Transition))); }
    Work.Legacy->SetArrayField(TEXT("planned_states"),PlannedStates);Work.Legacy->SetArrayField(TEXT("planned_transitions"),PlannedTransitions);
    if(bDryRun||bValidateOnly) { Work.Result.Stage=TEXT("dry_run_complete");Work.Result.bSuccess=true;Work.Result.bPostconditionPassed=true;Work.Legacy->SetStringField(TEXT("runtime_note"),TEXT("Dry run: no explicit duplicate/editor writer/compiler/save; SDK load effects remain unknown"));return Work.Finish(); }
    Work.Result.Stage=TEXT("prepare_target_anim_blueprint");Work.TargetPackages.Add(Target);FTargetPlan Plan(Target,TEXT("AnimBlueprint"),UAnimBlueprint::StaticClass());
    if(!InspectTarget(Plan,Work)) { return Work.Finish(); }
    if(Plan.Exists&&!bOverwriteExisting&&!bAllowExistingTargetReuse) { Work.Fail(TEXT("target_exists"),TEXT("Target already exists and reuse is disabled"));return Work.Finish(); }
    UAnimBlueprint* TargetBP=Cast<UAnimBlueprint>(PrepareTarget(Blueprint,Plan,bOverwriteExisting,Work));if(!Work.Current()||!TargetBP||!Work.Errors.IsEmpty()) { Work.Fail(TEXT("target_prepare_failed"),TEXT("Failed to prepare target sample AnimBP"));return Work.Finish(); }
    Work.Target=Work.AddTarget(TargetBP,Target);if(!Work.Target) { return Work.Finish(); }Plan.Receipt->TryGetBoolField(TEXT("created"),Work.Created);Plan.Receipt->TryGetBoolField(TEXT("reused"),Work.Reused);
    Work.TargetSkeletonLocked=Work.CommonSkeleton.IsValid();if(!Work.Current()) { return Work.Finish(); }
    Work.Legacy->SetBoolField(TEXT("target_exists_before"),Plan.Exists);Work.Legacy->SetStringField(TEXT("target_class"),TargetBP->GetClass()->GetPathName());
    if(!RuntimePackageGuard(TargetBP,Work,TEXT("target_dependency_projection"))) { return Work.Finish(); }
    TArray<TSharedPtr<FJsonValue>> RuleVariables,InputVariables,StateRows,TransitionRows;
    Work.Legacy->SetArrayField(TEXT("rule_variables"),RuleVariables);Work.Legacy->SetArrayField(TEXT("state_input_variables"),InputVariables);
    Work.Result.Stage=TEXT("ensure_rule_variables");const bool RuleOK=EnsureVariables(TargetBP,Transitions,States,true,RuleVariables,Work);Work.Legacy->SetArrayField(TEXT("rule_variables"),RuleVariables);
    if(!RuleOK) { Work.Fail(TEXT("rule_variables_invalid"),TEXT("Invalid state-machine transition rule variables"));return Work.Finish(); }
    Work.Result.Stage=TEXT("ensure_player_input_variables");const bool InputsOK=EnsureVariables(TargetBP,Transitions,States,false,InputVariables,Work);Work.Legacy->SetArrayField(TEXT("state_input_variables"),InputVariables);
    if(!InputsOK) { Work.Fail(TEXT("input_variables_invalid"),TEXT("Invalid state-machine player input variables"));return Work.Finish(); }
    Work.Result.Stage=TEXT("resolve_target_anim_graph");TArray<UEdGraph*> AllGraphs;if(!Graphs(TargetBP,AllGraphs,Work)) { return Work.Finish(); }
    const FString GraphName=GetStringParam(Params,TEXT("graph_name"),TEXT("AnimGraph")),GraphKind=NormalizeGraphType(GetStringParam(Params,TEXT("graph_type"),TEXT("function")));UEdGraph* AnimGraph=nullptr;
    if(GraphName.IsEmpty()&&GraphKind.IsEmpty())
    {
        for(UEdGraph* Graph:TargetBP->UbergraphPages) { if(Live(Graph)&&Graph->GetName().Contains(TEXT("EventGraph"))) { AnimGraph=Graph;break; } }
        if(!AnimGraph)
        {
            Work.Writer(TEXT("create_default_event_graph_calls"));AnimGraph=FBlueprintEditorUtils::CreateNewGraph(TargetBP,FName(TEXT("EventGraph")),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
            if(!Work.Current()||!Live(AnimGraph)||AnimGraph->GetOuter()!=TargetBP) { Work.Fail(TEXT("default_event_graph_failed"),TEXT("Default graph creation failed"));return Work.Finish(); }FIdentity Id(AnimGraph);
            Work.Writer(TEXT("add_ubergraph_page_calls"));FBlueprintEditorUtils::AddUbergraphPage(TargetBP,AnimGraph);Work.GraphChanged=true;Work.Result.bChanged=true;if(!Work.Current()||!Id.Current()) { return Work.Finish(); }
        }
    }
    else { for(UEdGraph* Graph:AllGraphs) { if((GraphName.IsEmpty()||Graph->GetName().Equals(GraphName,ESearchCase::IgnoreCase))&&(GraphKind.IsEmpty()||GraphKind==TEXT("any")||GraphType(TargetBP,Graph)==GraphKind)) { AnimGraph=Graph;break; } } }
    if(!Live(AnimGraph)||!AnimGraph->Schema||!AnimGraph->Schema->IsChildOf(UAnimationGraphSchema::StaticClass())) { Work.Fail(TEXT("graph_not_found"),TEXT("Target graph is not a matching AnimationGraph: ")+GraphName);return Work.Finish(); }
    const FString SelectedGraphName=AnimGraph->GetName();FGraphContext AnimContext(Work,TargetBP,AnimGraph);if(!AnimContext.Refresh()) { return Work.Finish(); }
    Work.Result.Stage=TEXT("ensure_state_machine_node");UAnimGraphNode_StateMachineBase* Machine=NamedNode<UAnimGraphNode_StateMachineBase>(AnimContext,MachineName);bool CreatedMachine=false;
    if(!Machine)
    {
        if(!Work.Current()) { return Work.Finish(); }Machine=ConfiguredNode<UAnimGraphNode_StateMachine>(AnimContext,-360,0,[](UAnimGraphNode_StateMachine*){});
        if(!Machine||!AnimContext.Refresh()) { Work.Fail(TEXT("machine_create_failed"),TEXT("Failed to create state-machine AnimGraph node"));return Work.Finish(); }
        Work.Writer(TEXT("state_machine_rename_calls"));static_cast<UEdGraphNode*>(Machine)->OnRenameNode(MachineName);CreatedMachine=true;if(!AnimContext.Refresh()) { return Work.Finish(); }
    }
    UAnimationStateMachineGraph* MachineGraph=Machine->EditorStateMachineGraph;
    if(!Live(MachineGraph)) { Work.Fail(TEXT("machine_graph_missing"),TEXT("State-machine node has no EditorStateMachineGraph"));return Work.Finish(); }
    FGraphContext MachineContext(Work,TargetBP,MachineGraph);if(!MachineContext.Refresh()) { return Work.Finish(); }
    TMap<FString,UAnimStateNode*> StateNodes;
    Work.Result.Stage=TEXT("ensure_states_and_players");
    for(const auto& Spec:States)
    {
        if(!AnimContext.Refresh()||!MachineContext.Refresh()) { return Work.Finish(); }
        bool CreatedState=false,CreatedPlayer=false,PoseCreated=false;UAnimStateNode* State=NamedNode<UAnimStateNode>(MachineContext,Spec.Name);
        if(!State) { if(!Work.Current()) { return Work.Finish(); }State=SpawnState(MachineContext,Spec);CreatedState=State!=nullptr; }
        if(!State) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Failed to create state: ")+Spec.Name));if(!Work.Current()) { return Work.Finish(); }continue; }
        StateNodes.Add(Spec.Name.ToLower(),State);auto Row=AnimStateMachineSampleStateSpecToJson(Spec);StateRows.Add(MakeShared<FJsonValueObject>(Row));Work.Legacy->SetArrayField(TEXT("states"),StateRows);
        Row->SetBoolField(TEXT("created"),CreatedState);Row->SetBoolField(TEXT("reused"),!CreatedState);Row->SetStringField(TEXT("node_id"),State->NodeGuid.ToString());
        UAnimationStateGraph* StateGraph=Cast<UAnimationStateGraph>(State->BoundGraph);UAnimGraphNode_AssetPlayerBase* Player=nullptr;TArray<TSharedPtr<FJsonValue>> InputRows;
        if(StateGraph)
        {
            FGraphContext StateContext(Work,TargetBP,StateGraph);if(!StateContext.Refresh()) { return Work.Finish(); }Row->SetStringField(TEXT("graph_name"),StateGraph->GetName());
            UEdGraphPin* Sink=PoseSink(StateContext,State);if(!StateContext.Refresh()||!MachineContext.Refresh()) { return Work.Finish(); }
            const bool HasPose=StateContext.PinCurrent(Sink)&&!Sink->LinkedTo.IsEmpty();
            if(!HasPose)
            {
                Player=SpawnPlayer(StateContext,Spec.AnimationAsset,FVector2D(-240,0));CreatedPlayer=Player!=nullptr;
                if(!Player) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Failed to spawn asset player; check skeleton compatibility for state: ")+Spec.Name)); }
                else
                {
                    if(!MachineContext.Refresh()||!StateContext.Refresh()) { return Work.Finish(); }Sink=PoseSink(StateContext,State);
                    if(!StateContext.Refresh()||!MachineContext.Refresh()) { return Work.Finish(); }UEdGraphPin* Output=StateContext.OutputPose(Player);FString Error;
                    if(!Connect(StateContext,Output,Sink,false,PoseCreated,Error)) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Failed to connect state player: ")+Spec.Name+TEXT(" ")+Error)); }
                }
            }
            if(!Work.Current()) { return Work.Finish(); }
            ApplyPlayerInputs(StateContext,State,Player,Spec,InputRows);Row->SetArrayField(TEXT("player_input_authoring"),InputRows);
            if(!MachineContext.Refresh()||!StateContext.Refresh()) { return Work.Finish(); }if(Player) { Row->SetObjectField(TEXT("asset_player_node"),NodeJson(Player,StateContext)); }
        }
        else { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("State has no UAnimationStateGraph: ")+Spec.Name)); }
        Row->SetBoolField(TEXT("asset_player_created"),CreatedPlayer);Row->SetBoolField(TEXT("pose_link_created"),PoseCreated);if(!Work.Current()) { return Work.Finish(); }
    }
    bool EntryConnected=false,EntryAlready=false;Work.Result.Stage=TEXT("connect_entry_state");
    if(auto** EntryPointer=StateNodes.Find(Entry.ToLower()))
    {
        if(!MachineContext.Refresh()) { return Work.Finish(); }UAnimStateEntryNode* EntryNode=MachineGraph->EntryNode;UEdGraphPin* From=DirectionPin(MachineContext,EntryNode,EGPD_Output);UEdGraphPin* To=DirectionPin(MachineContext,*EntryPointer,EGPD_Input);
        if(From&&To)
        {
            EntryAlready=To->LinkedTo.Contains(From);
            if(!EntryAlready&&!From->LinkedTo.IsEmpty()&&!bReplaceExistingEntryLink) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Entry node already links to another state; pass replace_existing_entry_link=true"))); }
            else if(EntryAlready||MachineConnection(MachineContext,From,To)) { EntryConnected=true; }
            else { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Failed to connect entry node to state: ")+Entry)); }
        }
    }
    Work.Result.Stage=TEXT("ensure_transitions_and_rules");
    for(const auto& Spec:Transitions)
    {
        if(!AnimContext.Refresh()||!MachineContext.Refresh()) { return Work.Finish(); }UAnimStateNode* From=StateNodes.FindRef(Spec.FromState.ToLower());UAnimStateNode* To=StateNodes.FindRef(Spec.ToState.ToLower());bool Created=false;
        UAnimStateTransitionNode* Transition=FindTransition(MachineContext,From,To);
        if(!Transition&&From&&To)
        { auto* Output=DirectionPin(MachineContext,From,EGPD_Output);auto* Input=DirectionPin(MachineContext,To,EGPD_Input);if(Output&&Input&&MachineConnection(MachineContext,Output,Input)) { Transition=FindTransition(MachineContext,From,To);Created=Transition!=nullptr; } }
        if(!Work.Current()) { return Work.Finish(); }if(!Transition) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Failed to create transition: ")+Spec.FromState+TEXT(" -> ")+Spec.ToState));continue; }
        auto Row=AnimStateMachineSampleTransitionSpecToJson(Spec);TransitionRows.Add(MakeShared<FJsonValueObject>(Row));Work.Legacy->SetArrayField(TEXT("transitions"),TransitionRows);
        Work.Writer(TEXT("modify_transition_calls"));Transition->Modify();if(!MachineContext.Refresh()) { return Work.Finish(); }
        Transition->Bidirectional=Spec.bBidirectional;Transition->PriorityOrder=Spec.PriorityOrder;Transition->CrossfadeDuration=Spec.CrossfadeDuration;Transition->BlendMode=Spec.BlendMode;Transition->LogicType=ETransitionLogicType::TLT_StandardBlend;
        Transition->SyncGroupNameToRequireValidMarkersRule=Spec.SyncGroupNameToRequireValidMarkersRule;Transition->bAutomaticRuleBasedOnSequencePlayerInState=Spec.RuleMode==TEXT("automatic_remaining_time");Transition->AutomaticRuleTriggerTime=Spec.AutomaticRuleTriggerTime;Work.GraphChanged=true;Work.Result.bChanged=true;
        auto Rule=MakeShared<FJsonObject>();const int32 BeforeErrors=Work.Errors.Num();const bool Applied=ApplyRule(TargetBP,Transition,Spec,Rule,Work);if(!Applied&&Work.Errors.Num()==BeforeErrors&&Work.Current()) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Failed to apply transition rule: ")+Spec.FromState+TEXT(" -> ")+Spec.ToState)); }
        Row->SetObjectField(TEXT("rule_graph_authoring"),Rule);Row->SetBoolField(TEXT("created"),Created);Row->SetBoolField(TEXT("reused"),!Created);
        if(!AnimContext.Refresh()||!MachineContext.Refresh()) { return Work.Finish(); }
        Row->SetStringField(TEXT("transition_id"),Transition->NodeGuid.ToString());Row->SetStringField(TEXT("rule_graph_name"),Transition->BoundGraph?Transition->BoundGraph->GetName():FString());Row->SetBoolField(TEXT("automatic_rule_based_on_sequence_player_in_state"),Transition->bAutomaticRuleBasedOnSequencePlayerInState);
    }
    Work.Result.Stage=TEXT("connect_machine_to_output");bool ConnectedOutput=false,AlreadyOutput=false;
    if(bConnectDemoMachineToOutput)
    {
        if(!AnimContext.Refresh()) { return Work.Finish(); }auto* RootNode=FirstNode<UAnimGraphNode_Root>(AnimContext);auto* Output=AnimContext.OutputPose(Machine);auto* Input=AnimContext.FirstPose(RootNode,EGPD_Input,{TEXT("Result")});
        AlreadyOutput=AnimContext.PinCurrent(Input)&&AnimContext.PinCurrent(Output)&&Input->LinkedTo.Contains(Output);FString Error;bool Changed=false;ConnectedOutput=Connect(AnimContext,Output,Input,bReplaceExistingRootLink,Changed,Error);
        if(!ConnectedOutput) { Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Failed to connect state machine to AnimGraph output: ")+Error)); }
    }
    if(!Work.Current()) { return Work.Finish(); }
    if(!Work.Errors.IsEmpty()) { Work.Fail(TEXT("authoring_failed"),TEXT("Failed to author state-machine sample; partial target writes are retained"));return Work.Finish(); }
    Work.Result.Stage=TEXT("notify_structural_change");
    if(Work.GraphChanged)
    {
        Work.Writer(TEXT("mark_structurally_modified_calls"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(TargetBP);
        if(!AnimContext.Refresh()||!MachineContext.Refresh()||!Notify(AnimContext)||!Notify(MachineContext)) { return Work.Finish(); }
    }
    auto MachineRow=MakeShared<FJsonObject>();Work.Legacy->SetObjectField(TEXT("state_machine"),MachineRow);
    if(!AnimContext.Refresh()||!MachineContext.Refresh()) { return Work.Finish(); }
    Work.SDK(TEXT("node_name_calls"));const FString ActualName=Machine->GetStateMachineName();if(!AnimContext.Refresh()||!MachineContext.Refresh()) { return Work.Finish(); }
    MachineRow->SetStringField(TEXT("name"),ActualName);MachineRow->SetStringField(TEXT("node_id"),Machine->NodeGuid.ToString());MachineRow->SetStringField(TEXT("graph_name"),MachineGraph->GetName());MachineRow->SetBoolField(TEXT("created"),CreatedMachine);MachineRow->SetBoolField(TEXT("reused"),!CreatedMachine);MachineRow->SetBoolField(TEXT("connected_to_output_pose"),ConnectedOutput);MachineRow->SetBoolField(TEXT("already_connected_to_output_pose"),AlreadyOutput);MachineRow->SetBoolField(TEXT("entry_connected"),EntryConnected);MachineRow->SetBoolField(TEXT("entry_already_connected"),EntryAlready);MachineRow->SetStringField(TEXT("entry_state"),Entry);MachineRow->SetNumberField(TEXT("state_count"),StateRows.Num());MachineRow->SetNumberField(TEXT("transition_count"),TransitionRows.Num());
    Work.Result.Stage=TEXT("compile_target_anim_blueprint");bool CompileOK=true;
    if(bCompile)
    {
        if(!Work.Current()||!LinkedClosure(TargetBP,nullptr,Work)||!RuntimePackageGuard(TargetBP,Work,TEXT("precompile_dependency_projection"))) { return Work.Finish(); }
        FCompilerResultsLog Log;Work.Result.bCompileAttempted=true;Work.Compile->SetBoolField(TEXT("attempted"),true);Work.Writer(TEXT("compile_blueprint_calls"));FKismetEditorUtilities::CompileBlueprint(TargetBP,EBlueprintCompileOptions::None,&Log);Work.Count(TEXT("compile_blueprint_returns"));
        if(!Work.Current()) { return Work.Finish(); }CompileOK=Log.NumErrors==0&&(TargetBP->Status==BS_UpToDate||TargetBP->Status==BS_UpToDateWithWarnings)&&Live(TargetBP->GeneratedClass)&&TargetBP->GeneratedClass->IsChildOf(UAnimInstance::StaticClass())&&TargetBP->GeneratedClass->ClassGeneratedBy==TargetBP&&!TargetBP->GeneratedClass->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists);
        Work.Compile->SetStringField(TEXT("blueprint"),TargetBP->GetPathName());Work.Compile->SetStringField(TEXT("generated_class"),Live(TargetBP->GeneratedClass)?TargetBP->GeneratedClass->GetPathName():FString());
        Work.Result.Compiled=CompileOK;Work.Compile->SetBoolField(TEXT("success"),CompileOK);Work.Compile->SetBoolField(TEXT("generated_class_present"),Live(TargetBP->GeneratedClass));Work.Compile->SetNumberField(TEXT("error_count"),Log.NumErrors);Work.Compile->SetNumberField(TEXT("warning_count"),Log.NumWarnings);Work.Compile->SetNumberField(TEXT("blueprint_status"),int32(TargetBP->Status));
    }
    Work.Legacy->SetObjectField(TEXT("compile"),Work.Compile);Work.Compile->SetBoolField(TEXT("legacy_generated_class_only_success"),Live(TargetBP->GeneratedClass));
    // Preserve original normal-return compiler-failure -> requested save -> readback order.
    Work.Result.Stage=TEXT("save_target_anim_blueprint");bool SaveOK=true;
    if(bSaveAsset) { if(!RuntimePackageGuard(TargetBP,Work,TEXT("presave_dependency_projection"))) { return Work.Finish(); }SaveOK=SaveOne(*Work.Target,TEXT("target_saved"),Work);Work.Result.bSaved=SaveOK;if(!Work.Current()) { return Work.Finish(); } }
    Work.Result.Stage=TEXT("readback_state_machine");Work.SDK(TEXT("state_machine_inspection_calls"));const auto Readback=AnimStateMachineInspection::Inspect(Target,MachineName,true,true,true,256);Work.Count(TEXT("state_machine_inspection_returns"));
    if(const TSharedPtr<FJsonObject>* Inner=nullptr;Readback.Data->TryGetObjectField(TEXT("legacy_result"),Inner)) { Work.Legacy->SetObjectField(TEXT("readback"),*Inner); }else { Work.Legacy->SetObjectField(TEXT("readback"),Readback.Data); }
    Work.Result.bMutationAttempted|=Readback.bMutationAttempted;Work.Result.bChanged|=Readback.bChanged;Work.Result.Data->SetBoolField(TEXT("readback_success"),Readback.bSuccess);Work.Result.Data->SetStringField(TEXT("readback_error"),Readback.Error);Work.Result.Data->SetStringField(TEXT("readback_stage"),Readback.Stage);
    if(!Work.Current()) { return Work.Finish(); }
    Work.Legacy->SetBoolField(TEXT("target_asset_modified"),Work.GraphChanged);Work.Legacy->SetBoolField(TEXT("target_asset_saved"),Work.Result.bSaved);Work.Legacy->SetArrayField(TEXT("warnings"),{});
    Work.Legacy->SetBoolField(TEXT("compile_succeeded"),CompileOK);Work.Legacy->SetBoolField(TEXT("save_succeeded"),SaveOK);
    TArray<TSharedPtr<FJsonValue>> CompletionErrors;if(!CompileOK) { CompletionErrors.Add(MakeShared<FJsonValueString>(TEXT("compile_failed"))); }if(!SaveOK) { CompletionErrors.Add(MakeShared<FJsonValueString>(TEXT("save_failed"))); }if(!CompletionErrors.IsEmpty()) { Work.Legacy->SetArrayField(TEXT("completion_errors"),CompletionErrors); }
    // Reacquire graph metadata after compiler/readback callbacks; do not reuse old pin/node pointers.
    TArray<UEdGraph*> FinalGraphs;if(!Graphs(TargetBP,FinalGraphs,Work)) { return Work.Finish(); }
    for(UEdGraph* Graph:FinalGraphs)
    {
        if(Graph->GetName().Equals(SelectedGraphName,ESearchCase::CaseSensitive))
        {
            FGraphContext FinalContext(Work,TargetBP,Graph);if(!FinalContext.Refresh()) { return Work.Finish(); }auto GraphRow=MakeShared<FJsonObject>();GraphRow->SetStringField(TEXT("graph_id"),Graph->GraphGuid.ToString());GraphRow->SetStringField(TEXT("graph_name"),Graph->GetName());GraphRow->SetStringField(TEXT("graph_type"),GraphType(TargetBP,Graph));GraphRow->SetStringField(TEXT("schema"),FinalContext.Schema->GetClass()->GetName());GraphRow->SetNumberField(TEXT("node_count"),Graph->Nodes.Num());Work.Legacy->SetObjectField(TEXT("graph"),GraphRow);break;
        }
    }
    if(!CompileOK) { Work.Fail(TEXT("compile_failed"),TEXT("State-machine sample written; requested compiler returned errors or an invalid generated class")); }
    else if(bSaveAsset&&!SaveOK) { Work.Fail(TEXT("save_failed"),TEXT("State-machine sample written but requested save postcondition failed")); }
    else if(!Readback.bSuccess) { Work.Fail(TEXT("readback_failed"),TEXT("State-machine sample written but inspection failed: ")+Readback.Error); }
    else { Work.Result.Stage=TEXT("complete");Work.Result.bSuccess=true;Work.Result.bPostconditionPassed=true; }
    return Work.Finish();
}
} // namespace Cubeless::UE58::AnimStateMachineVariant
