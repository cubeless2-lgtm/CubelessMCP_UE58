#include "Compatibility/UE58/CubelessAnimNodePrePostRuntimePoseApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/Skeleton.h"
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_ControlRig.h"
#include "AnimGraphNode_AssetPlayerBase.h"
#include "AnimGraphNode_LinkedInputPose.h"
#include "AnimGraphNode_LinkedAnimGraph.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_StateMachineBase.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimGraphNode_RigidBody.h"
#include "AnimGraphNode_Trail.h"
#include "AnimGraphNode_ModifyBone.h"
#include "AnimationGraphSchema.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/BlueprintExtension.h"
#include "Components/SkeletalMeshComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/Engine.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Engine/PoseWatch.h"
#include "Engine/PoseWatchRenderData.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/ReferencerFinder.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"
namespace Cubeless::UE58::AnimNodePrePostRuntimePose
{
namespace
{
namespace Sampling
{
using Variable::FResult;
constexpr int32 MaxCarrier=1024*1024,MaxNames=4096,MaxWorlds=64,MaxActors=65536,MaxComponents=4096,MaxBones=65536,MaxHeld=16384;
constexpr int64 MaxChecks=1024*1024;
bool Running=false;
bool Live(const UObject* Object)
{return IsValid(Object)&&!Object->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
bool SafeText(const FString& Text,int32 Limit=MaxCarrier)
{if(Text.Len()>Limit){return false;}for(TCHAR Character:Text){if(Character==0){return false;}}return true;}
bool JsonSyntaxBounded(const FString& Text)
{
    if(Text.IsEmpty()||!SafeText(Text)){return false;}bool Quoted=false,Escape=false;int32 Depth=0,Slots=0;
    for(TCHAR Character:Text)
    {
        if(Quoted){if(Escape){Escape=false;}else if(Character==TEXT('\\')){Escape=true;}else if(Character==TEXT('"')){Quoted=false;}continue;}
        if(Character==TEXT('"')){Quoted=true;}
        else if(Character==TEXT('[')||Character==TEXT('{')){if(++Depth>64||++Slots>65536){return false;}}
        else if(Character==TEXT(']')||Character==TEXT('}')){if(--Depth<0){return false;}}
        else if(Character==TEXT(',')&&++Slots>65536){return false;}
    }
    return !Quoted&&!Escape&&Depth==0;
}
bool Decode(const FString& Text,TSharedPtr<FJsonValue>& Value)
{
    TArray<TSharedPtr<FJsonValue>> Array;
    if(!JsonSyntaxBounded(Text)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]")),Array)||Array.Num()!=1||!Array[0].IsValid()){return false;}
    Value=Array[0];TArray<TSharedPtr<FJsonValue>> Stack={Value};int32 Slots=0;
    while(!Stack.IsEmpty())
    {
        const auto Item=Stack.Pop(EAllowShrinking::No);if(!Item.IsValid()||++Slots>65536){return false;}
        if(Item->Type==EJson::Number&&!FMath::IsFinite(Item->AsNumber())){return false;}
        if(Item->Type==EJson::Array){Stack.Append(Item->AsArray());}
        else if(Item->Type==EJson::Object){for(const auto& Pair:Item->AsObject()->Values){Stack.Add(Pair.Value);}}
        if(Stack.Num()>65536){return false;}
    }
    return true;
}
bool ParseNames(const FString& Json,const TArray<FString>& Defaults,TArray<FString>& Out,FString& Error)
{
    TSharedPtr<FJsonValue> Value;if(!Decode(Json,Value)){Error=TEXT("Name carrier requires one finite JSON value within 1Mi UTF16/depth64/65536 slots/no raw NUL");return false;}
    if(Value->Type!=EJson::Array||Value->AsArray().IsEmpty()){Out=Defaults;return true;}
    if(Value->AsArray().Num()>MaxNames){Error=TEXT("Name input array exceeds4096 rows");return false;}
    for(const auto& Row:Value->AsArray())
    {
        FString Name;
        if(Row->Type==EJson::String){Name=Row->AsString();}
        else if(Row->Type==EJson::Object)
        {
            const auto Object=Row->AsObject();if(!Object->TryGetStringField(TEXT("name"),Name)){continue;}
            FString Type;if(Object->TryGetStringField(TEXT("type"),Type)&&!Type.IsEmpty()){Name=Type+TEXT(":")+Name;}
        }
        else{continue;}
        if(!SafeText(Name,NAME_SIZE-1)){Error=TEXT("Decoded name/type:name exceeds FName/NUL safety bound");return false;}
        Out.Add(Name);
    }
    if(Out.IsEmpty()){Out=Defaults;}return true;
}
FString WorldType(const UWorld* World)
{
    if(!World){return TEXT("None");}
    switch(World->WorldType)
    {
    case EWorldType::Editor:return TEXT("Editor");case EWorldType::PIE:return TEXT("PIE");case EWorldType::Game:return TEXT("Game");
    case EWorldType::GamePreview:return TEXT("GamePreview");case EWorldType::EditorPreview:return TEXT("EditorPreview");case EWorldType::Inactive:return TEXT("Inactive");default:return TEXT("Unknown");
    }
}
bool PlayWorld(const UWorld* World)
{return World&&(World->WorldType==EWorldType::PIE||World->WorldType==EWorldType::Game||World->WorldType==EWorldType::GamePreview);}
bool FiniteTransform(const FTransform& Value)
{return !Value.ContainsNaN()&&Value.GetRotation().IsNormalized();}
struct FRead
{
    struct FIdentity{UObject* Object;UClass* Class;UObject* Outer;UPackage* Package;FString Path,PackagePath;};
    struct FSkin{USkinnedMeshComponent* Component;USkinnedAsset* Asset;USkinnedMeshComponent* Leader;AActor* Owner;UWorld* World;USkeleton* Skeleton;};
    FResult Result;TSharedRef<FJsonObject> Legacy=MakeShared<FJsonObject>(),Calls=MakeShared<FJsonObject>(),BoneSamples=MakeShared<FJsonObject>(),SocketSamples=MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Errors,Warnings;TArray<TStrongObjectPtr<UObject>> Held;TArray<FIdentity> Identities;TSet<UObject*> Seen;
    TArray<FSkin> Skins;TArray<UWorld*> CandidateWorlds;UWorld* World=nullptr;EWorldType::Type Type=EWorldType::None;AActor* Actor=nullptr;USkeletalMeshComponent* Component=nullptr;
    int64 Checks=0;int32 OutputChars=0,ScannedActors=0;
    FRead()
    {
        Result.Data->SetObjectField(TEXT("legacy_result"),Legacy);Result.Data->SetObjectField(TEXT("native_call_counts"),Calls);
        Legacy->SetBoolField(TEXT("success"),false);Legacy->SetBoolField(TEXT("read_only"),true);Legacy->SetBoolField(TEXT("asset_modified"),false);Legacy->SetBoolField(TEXT("starts_sie"),false);
        Legacy->SetStringField(TEXT("runtime_source"),TEXT("active_skeletal_mesh_component"));
        Legacy->SetStringField(TEXT("runtime_note"),TEXT("Samples the current SkeletalMeshComponent pose from an active PIE/SIE/play world when available, otherwise the editor world. It does not start or tick SIE by itself."));
        Legacy->SetObjectField(TEXT("bone_samples"),BoneSamples);Legacy->SetObjectField(TEXT("socket_samples"),SocketSamples);
        Result.Data->SetBoolField(TEXT("legacy_read_only_flags_are_explicit_intent"),true);Result.Data->SetBoolField(TEXT("atomic_frame_snapshot_verified"),false);
        Result.Data->SetBoolField(TEXT("whole_runtime_pose_verified"),false);Result.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);
        Result.Data->SetBoolField(TEXT("public_getter_identity_fallback_possible"),true);
        Result.Data->SetStringField(TEXT("pose_observation_scope"),TEXT("Current public component getters only; missing/uninitialized pose buffers may return SDK identity fallback. No freshness, evaluated-frame or whole-runtime-pose proof is claimed."));
        Result.Data->SetStringField(TEXT("effect_scope"),TEXT("Explicit load/modify/tick/update/initialize/compile/save/start/stop are zero. World, actor-label, component, mesh, socket and pose getter callbacks/cache effects remain total-SDK unknown."));
        for(const TCHAR* Key:{TEXT("asset_load_calls"),TEXT("modify_calls"),TEXT("tick_calls"),TEXT("update_calls"),TEXT("refresh_bone_transforms_calls"),TEXT("initialize_calls"),TEXT("compile_calls"),TEXT("save_calls"),TEXT("start_pie_calls"),TEXT("stop_pie_calls"),TEXT("transaction_calls"),
            TEXT("actor_label_calls"),TEXT("get_components_calls"),TEXT("bone_index_calls"),TEXT("parent_bone_calls"),TEXT("socket_exists_calls"),TEXT("socket_bone_calls"),TEXT("socket_transform_calls")}){Calls->SetNumberField(Key,0);}
    }
    void Count(const TCHAR* Key){double Number=0;Calls->TryGetNumberField(Key,Number);Calls->SetNumberField(Key,Number+1);}
    void SDK(const TCHAR* Key){Result.bChangeStateKnown=false;Count(Key);}
    bool Fail(const TCHAR* Code,const FString& Error)
    {
        if(Result.Error.IsEmpty()){Result.ErrorCode=FString(TEXT("skeletal_bone_sampling_"))+Code;Result.Error=Error;}
        Result.Data->SetBoolField(TEXT("observation_incomplete"),true);return false;
    }
    bool Step(){return ++Checks<=MaxChecks||Fail(TEXT("check_bound"),TEXT("Finite observation check budget exceeded; partial prefix retained"));}
    bool Hold(UObject* Object,int32 Depth=0)
    {
        if(!Object){return true;}if(Depth>64||!Live(Object)||!Live(Object->GetClass())||Object->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists)){return Fail(TEXT("source_unreadable"),TEXT("Source object/class unavailable or reinstanced"));}
        if(Seen.Contains(Object)){return true;}if(Seen.Num()>=MaxHeld){return Fail(TEXT("source_bound"),TEXT("Strong source identity inventory exceeds16384"));}
        UPackage* Package=Object->GetPackage();if(!Live(Package)){return Fail(TEXT("source_unreadable"),TEXT("Source actual package is unavailable"));}
        Seen.Add(Object);Held.Emplace(Object);Identities.Add({Object,Object->GetClass(),Object->GetOuter(),Package,Object->GetPathName(),Package->GetPathName()});
        return Hold(Object->GetClass(),Depth+1)&&Hold(Package,Depth+1)&&Hold(Object->GetOuter(),Depth+1);
    }
    bool Raw()
    {
        if(!Result.Error.IsEmpty()){return false;}
        if(!Step()||!IsEditorGameThread()||IsEngineExitRequested()){return Fail(TEXT("editor_boundary"),TEXT("A live Editor game thread is required; PIE/SIE is allowed"));}
        for(const auto& Id:Identities)
        {
            if(!Step()){return false;}
            if(!Live(Id.Object)||!Live(Id.Class)||!Live(Id.Package)||Id.Class->HasAnyClassFlags(CLASS_NewerVersionExists)
                ||Id.Object->GetClass()!=Id.Class||Id.Object->GetOuter()!=Id.Outer||Id.Object->GetPackage()!=Id.Package
                ||Id.Object->GetPathName()!=Id.Path||Id.Package->GetPathName()!=Id.PackagePath)
            {return Fail(TEXT("source_identity_changed"),TEXT("Object/class/outer/path/actual package changed after a getter callback"));}
        }
        return true;
    }
    bool PoseBuffers(USkinnedMeshComponent* Skin,USkinnedAsset* Asset)
    {
        if(auto* Skeletal=Cast<USkeletalMeshComponent>(Skin))
        {if(Skeletal->IsRunningParallelEvaluation()){return Fail(TEXT("parallel_pending"),TEXT("Parallel animation evaluation is active; no blocking completion or tick was requested"));}}
        if(!Raw()){return false;}
        if(Asset)
        {
            SDK(TEXT("mesh_is_compiling_calls"));const bool Pending=Asset->IsCompiling();if(!Raw()){return false;}
            if(Pending){return Fail(TEXT("mesh_compilation_pending"),TEXT("Mesh compilation is active; no explicit completion was requested"));}
            SDK(TEXT("reference_skeleton_reads"));const auto& Ref=static_cast<const USkinnedAsset*>(Asset)->GetRefSkeleton();
            if(!Raw()){return false;}const auto& Info=Ref.GetRefBoneInfo();const auto& Poses=Ref.GetRefBonePose();
            if(Info.Num()>MaxBones||Poses.Num()<Info.Num()){return Fail(TEXT("reference_skeleton_bound"),TEXT("Reference bone/pose arrays exceed65536 or their shapes do not agree"));}
            for(int32 Index=0;Index<Info.Num();++Index)
            {
                if(!Step()){return false;}
                if((Index==0&&Info[Index].ParentIndex!=INDEX_NONE)||(Index>0&&(Info[Index].ParentIndex<0||Info[Index].ParentIndex>=Index))||!FiniteTransform(Poses[Index]))
                {return Fail(TEXT("reference_skeleton_invalid"),TEXT("Reference parents/pose values cannot safely enter public parent/leader-pose getters"));}
            }
        }
        const auto& Transforms=Skin->GetComponentSpaceTransforms();
        if(Transforms.Num()>MaxBones){return Fail(TEXT("pose_buffer_bound"),TEXT("Current component-space buffer exceeds65536 transforms"));}
        for(const FTransform& PoseTransform:Transforms){if(!Step()||!FiniteTransform(PoseTransform)){return Fail(TEXT("pose_buffer_invalid"),TEXT("Current component-space pose contains nonfinite or unnormalized transforms"));}}
        return Raw();
    }
    bool Current()
    {
        if(!Raw()){return false;}
        if(World)
        {
            bool Present=false;SDK(TEXT("world_membership_queries"));
            if(GEngine){for(const FWorldContext& Context:GEngine->GetWorldContexts()){Present|=Context.World()==World;}}
            if(GEditor){Present|=GEditor->GetEditorWorldContext().World()==World;}
            if(!Present||World->WorldType!=Type){return Fail(TEXT("world_changed"),TEXT("Selected world/context/type is no longer current"));}
        }
        if(Actor)
        {
            SDK(TEXT("actor_world_queries"));UWorld* CurrentWorld=Actor->GetWorld();
            if(!Raw()||CurrentWorld!=World){return Fail(TEXT("actor_world_changed"),TEXT("Matched actor left the selected world"));}
        }
        if(Component)
        {
            TArray<USkeletalMeshComponent*> Components;SDK(TEXT("get_components_calls"));Actor->GetComponents<USkeletalMeshComponent>(Components);
            if(!Raw()||Components.Num()>MaxComponents||Component->GetOwner()!=Actor||!Components.Contains(Component)){return Fail(TEXT("component_changed"),TEXT("Selected component owner/membership changed"));}
            const FTransform ComponentTransform=Component->GetComponentTransform();if(!FiniteTransform(ComponentTransform)){return Fail(TEXT("component_transform_invalid"),TEXT("Current component transform is not finite/normalized"));}
        }
        for(const FSkin& Skin:Skins)
        {
            if(!Raw()||Skin.Component->GetOwner()!=Skin.Owner||Skin.Component->GetSkinnedAsset()!=Skin.Asset||Skin.Component->LeaderPoseComponent.Get()!=Skin.Leader)
            {return Fail(TEXT("pose_source_changed"),TEXT("Captured main/leader component or mesh source changed"));}
            SDK(TEXT("skin_world_queries"));UWorld* SkinWorld=Skin.Component->GetWorld();if(!Raw()||SkinWorld!=Skin.World){return Fail(TEXT("pose_source_changed"),TEXT("Captured pose source world changed"));}
            if(auto* Mesh=Cast<USkeletalMesh>(Skin.Asset))
            {SDK(TEXT("skeleton_identity_queries"));USkeleton* Skeleton=Mesh->GetSkeleton();if(!Raw()||Skeleton!=Skin.Skeleton){return Fail(TEXT("skeleton_changed"),TEXT("Captured mesh skeleton changed"));}}
            if(!PoseBuffers(Skin.Component,Skin.Asset)){return false;}
        }
        // Public mesh/socket callbacks may replace the component's sources without replacing the component itself.
        for(const FSkin& Skin:Skins)
        {
            if(!Raw()||Skin.Component->GetOwner()!=Skin.Owner||Skin.Component->GetSkinnedAsset()!=Skin.Asset||Skin.Component->LeaderPoseComponent.Get()!=Skin.Leader)
            {return Fail(TEXT("pose_source_changed"),TEXT("Captured main/leader source changed during a mesh getter callback"));}
        }
        return Raw();
    }
    bool Text(const TSharedRef<FJsonObject>& Object,const TCHAR* Key,const FString& Value)
    {
        if(!SafeText(Value)||Value.Len()>MaxCarrier-OutputChars){return Fail(TEXT("output_bound"),TEXT("Output text budget exceeded; partial prefix retained"));}
        OutputChars+=Value.Len();Object->SetStringField(Key,Value);return true;
    }
    bool Warning(const FString& Message)
    {if(!SafeText(Message)||Message.Len()>MaxCarrier-OutputChars){return Fail(TEXT("output_bound"),TEXT("Warning text budget exceeded"));}OutputChars+=Message.Len();Warnings.Add(MakeShared<FJsonValueString>(Message));return true;}
    bool Transform(const TSharedRef<FJsonObject>& Object,const TCHAR* Key,const FTransform& Value)
    {
        if(!FiniteTransform(Value)||OutputChars>MaxCarrier-512){return Fail(TEXT("transform_invalid"),TEXT("Sample transform is invalid or exceeds output budget"));}
        OutputChars+=512;auto Json=MakeShared<FJsonObject>();auto Array=[](std::initializer_list<double> Items){TArray<TSharedPtr<FJsonValue>> Values;for(double Item:Items){Values.Add(MakeShared<FJsonValueNumber>(Item));}return Values;};
        const FVector Translation=Value.GetTranslation(),Scale=Value.GetScale3D();const FQuat Rotation=Value.GetRotation();const FRotator Euler=Rotation.Rotator();
        if(Euler.ContainsNaN()){return Fail(TEXT("transform_invalid"),TEXT("Sample Euler conversion is nonfinite"));}
        Json->SetArrayField(TEXT("translation"),Array({Translation.X,Translation.Y,Translation.Z}));Json->SetArrayField(TEXT("rotation"),Array({Rotation.X,Rotation.Y,Rotation.Z,Rotation.W}));
        Json->SetArrayField(TEXT("rotation_euler"),Array({Euler.Pitch,Euler.Yaw,Euler.Roll}));Json->SetArrayField(TEXT("scale"),Array({Scale.X,Scale.Y,Scale.Z}));Object->SetObjectField(Key,Json);return true;
    }
    FResult Finish()
    {
        if(!Result.bSuccess&&Result.Error.IsEmpty()){Fail(TEXT("operation_incomplete"),TEXT("Observation stopped before postconditions; inspect the stage and partial data"));}
        Legacy->SetBoolField(TEXT("success"),Result.bSuccess);if(!Result.Error.IsEmpty()){Legacy->SetStringField(TEXT("error"),Result.Error);Errors.Add(MakeShared<FJsonValueString>(Result.Error));}
        Legacy->SetArrayField(TEXT("errors"),Errors);Legacy->SetArrayField(TEXT("warnings"),Warnings);
        Result.Data->SetNumberField(TEXT("observation_checks"),Checks);Result.Data->SetNumberField(TEXT("held_source_count"),Seen.Num());Result.Data->SetNumberField(TEXT("scanned_actor_count"),ScannedActors);
        Result.Data->SetBoolField(TEXT("explicit_asset_mutation_performed"),false);return Result;
    }
};
bool Worlds(FRead& Read,bool Prefer,bool Require)
{
    auto Add=[&](UWorld* World)
    {
        if(!World||Read.CandidateWorlds.Contains(World)){return true;}
        if(Read.CandidateWorlds.Num()>=MaxWorlds||!Read.Hold(World)){return Read.Fail(TEXT("world_bound"),TEXT("Candidate world inventory exceeds64 or is unavailable"));}
        Read.CandidateWorlds.Add(World);return true;
    };
    auto Games=[&]()
    {
        if(GEngine)
        {for(const FWorldContext& Context:GEngine->GetWorldContexts()){UWorld* World=Context.World();if(PlayWorld(World)&&!Add(World)){return false;}}}
        return true;
    };
    Read.SDK(TEXT("world_context_discovery_calls"));
    if(Prefer&&!Games()){return false;}
    if(!Require&&GEditor&&!Add(GEditor->GetEditorWorldContext().World())){return false;}
    return (Prefer||Games())&&Read.Raw();
}
bool FindTarget(FRead& Read,const FString& Label,const FString& Name,const FString& Path,const FString& ComponentName,bool Require)
{
    for(UWorld* World:Read.CandidateWorlds)
    {
        if(!Read.Raw()){return false;}Read.World=World;Read.Type=World->WorldType;TArray<AActor*> Actors;
        for(TActorIterator<AActor> It(World);It;++It)
        {
            if(++Read.ScannedActors>MaxActors){return Read.Fail(TEXT("actor_bound"),TEXT("Candidate actor scan exceeds65536"));}
            AActor* Candidate=*It;if(!Live(Candidate)){continue;}if(!Read.Hold(Candidate)||!Read.Current()){return false;}
            if(!Label.IsEmpty())
            {Read.SDK(TEXT("actor_label_calls"));const FString CurrentLabel=Candidate->GetActorLabel();if(!Read.Current()){return false;}if(!CurrentLabel.Equals(Label,ESearchCase::IgnoreCase)){continue;}}
            if(!Name.IsEmpty()&&!Candidate->GetName().Equals(Name,ESearchCase::IgnoreCase)){continue;}
            const FString CurrentPath=Candidate->GetPathName();if(!Path.IsEmpty()&&!CurrentPath.Equals(Path,ESearchCase::IgnoreCase)&&!CurrentPath.EndsWith(Path,ESearchCase::IgnoreCase)){continue;}
            TArray<USkeletalMeshComponent*> Components;Read.SDK(TEXT("get_components_calls"));Candidate->GetComponents<USkeletalMeshComponent>(Components);
            if(!Read.Current()||Components.Num()>MaxComponents){return Read.Fail(TEXT("component_bound"),TEXT("Candidate component inventory exceeds4096 or changed after callback"));}
            if(!Components.IsEmpty()){Actors.Add(Candidate);}
        }
        double ActorCount=0;Read.Legacy->TryGetNumberField(TEXT("matched_actor_count"),ActorCount);Read.Legacy->SetNumberField(TEXT("matched_actor_count"),ActorCount+Actors.Num());
        for(AActor* Candidate:Actors)
        {
            if(!Read.Current()){return false;}TArray<USkeletalMeshComponent*> Components;Read.SDK(TEXT("get_components_calls"));Candidate->GetComponents<USkeletalMeshComponent>(Components);
            if(!Read.Current()||Components.Num()>MaxComponents){return Read.Fail(TEXT("component_bound"),TEXT("Current candidate components exceed4096"));}
            USkeletalMeshComponent* First=nullptr;int32 Count=0;
            for(USkeletalMeshComponent* Component:Components)
            {
                if(!Live(Component)){continue;}if(!Read.Hold(Component)){return false;}
                if(!First){First=Component;}
                if(ComponentName.IsEmpty()){++Count;continue;}
                if(Component->GetName().Equals(ComponentName,ESearchCase::IgnoreCase)){++Count;First=Component;break;}
            }
            if((ComponentName.IsEmpty()&&First)||(!ComponentName.IsEmpty()&&Count>0))
            {
                Read.Actor=Candidate;Read.Component=First;Read.Legacy->SetNumberField(TEXT("matched_component_count"),Count);
                return Read.Current();
            }
        }
    }
    return Read.Fail(TEXT("target_missing"),Require
        ?FString::Printf(TEXT("No SkeletalMeshComponent actor matched actor_label='%s', actor_name='%s', actor_path='%s', component_name='%s' in active PIE/SIE/play worlds"),*Label,*Name,*Path,*ComponentName)
        :FString::Printf(TEXT("No SkeletalMeshComponent actor matched actor_label='%s', actor_name='%s', actor_path='%s', component_name='%s' in candidate editor/PIE worlds"),*Label,*Name,*Path,*ComponentName));
}
bool PreparePoseSources(FRead& Read)
{
    USkinnedMeshComponent* Component=Read.Component;TSet<USkinnedMeshComponent*> Seen;
    while(Component)
    {
        if(Seen.Contains(Component)||Seen.Num()>=16){return Read.Fail(TEXT("leader_cycle_or_bound"),TEXT("Leader-pose chain is cyclic or exceeds16"));}
        Seen.Add(Component);if(!Read.Hold(Component)||!Read.Raw()){return false;}
        USkinnedAsset* Asset=Component->GetSkinnedAsset();USkinnedMeshComponent* Leader=Component->LeaderPoseComponent.Get();AActor* Owner=Component->GetOwner();
        Read.SDK(TEXT("skin_world_queries"));UWorld* World=Component->GetWorld();
        if(!Read.Raw()||!Read.Hold(Asset)||!Read.Hold(Leader)||!Read.Hold(Owner)||!Read.Hold(World)){return false;}
        USkeleton* Skeleton=nullptr;
        if(auto* Mesh=Cast<USkeletalMesh>(Asset))
        {
            Read.SDK(TEXT("mesh_is_compiling_calls"));const bool Pending=Mesh->IsCompiling();if(!Read.Raw()||Pending){return Read.Fail(TEXT("mesh_compilation_pending"),TEXT("Mesh compilation is active; no explicit wait"));}
            Read.SDK(TEXT("skeleton_identity_queries"));Skeleton=Mesh->GetSkeleton();if(!Read.Raw()||!Read.Hold(Skeleton)){return false;}
        }
        Read.Skins.Add({Component,Asset,Leader,Owner,World,Skeleton});Component=Leader;
    }
    return Read.Current();
}
bool Metadata(FRead& Read,const FString& ComponentName)
{
    double Actors=0,Components=0;Read.Legacy->TryGetNumberField(TEXT("matched_actor_count"),Actors);Read.Legacy->TryGetNumberField(TEXT("matched_component_count"),Components);
    if(Actors>1&&!Read.Warning(FString::Printf(TEXT("Matched %d actors; sampled the first matching actor in world priority order"),static_cast<int32>(Actors)))){return false;}
    if(ComponentName.IsEmpty()&&Components>1&&!Read.Warning(FString::Printf(TEXT("Actor has %d SkeletalMeshComponents; sampled the first component because component_name was not provided"),static_cast<int32>(Components)))){return false;}
    Read.Legacy->SetBoolField(TEXT("is_play_session_active"),GEditor&&GEditor->GetPIEWorldContext()!=nullptr);
    if(!Read.Text(Read.Legacy,TEXT("sampled_world_type"),WorldType(Read.World))||!Read.Text(Read.Legacy,TEXT("sampled_world_name"),Read.World->GetName())||!Read.Current()){return false;}
    auto Actor=MakeShared<FJsonObject>();Read.Legacy->SetObjectField(TEXT("actor"),Actor);Read.SDK(TEXT("actor_label_calls"));const FString Label=Read.Actor->GetActorLabel();
    if(!Read.Current()||!Read.Text(Actor,TEXT("label"),Label)||!Read.Text(Actor,TEXT("name"),Read.Actor->GetName())||!Read.Text(Actor,TEXT("path"),Read.Actor->GetPathName())||!Read.Text(Actor,TEXT("class"),Read.Actor->GetClass()->GetPathName())){return false;}
    auto Component=MakeShared<FJsonObject>();Read.Legacy->SetObjectField(TEXT("component"),Component);
    if(!Read.Text(Component,TEXT("name"),Read.Component->GetName())||!Read.Text(Component,TEXT("path"),Read.Component->GetPathName())||!Read.Text(Component,TEXT("class"),Read.Component->GetClass()->GetPathName())){return false;}
    Read.SDK(TEXT("mesh_identity_queries"));USkeletalMesh* Mesh=Read.Component->GetSkeletalMeshAsset();Read.SDK(TEXT("anim_class_queries"));UClass* AnimClass=Read.Component->GetAnimClass();
    if(!Read.Raw()||!Read.Hold(Mesh)||!Read.Hold(AnimClass)||!Read.Current()){return false;}
    const auto Mode=Read.Component->GetAnimationMode();const FString ModeName=Mode==EAnimationMode::AnimationBlueprint?TEXT("AnimationBlueprint"):(Mode==EAnimationMode::AnimationSingleNode?TEXT("AnimationSingleNode"):(Mode==EAnimationMode::AnimationCustomMode?TEXT("AnimationCustomMode"):TEXT("Unknown")));
    return Read.Text(Component,TEXT("skeletal_mesh"),Mesh?Mesh->GetPathName():FString())&&Read.Text(Component,TEXT("anim_class"),AnimClass?AnimClass->GetPathName():FString())
        &&Read.Text(Component,TEXT("animation_mode"),ModeName)&&Read.Transform(Component,TEXT("component_transform"),Read.Component->GetComponentTransform())&&Read.Current();
}
bool SampleName(FRead& Read,const FString& Name,bool Socket)
{
    if(Name.IsEmpty()){return true;}if(!Read.Current()){return false;}auto Row=MakeShared<FJsonObject>();
    (Socket?Read.SocketSamples:Read.BoneSamples)->SetObjectField(Name,Row);
    if(!Read.Text(Row,TEXT("name"),Name)||!Read.Text(Row,TEXT("type"),Socket?TEXT("socket"):TEXT("bone"))){return false;}
    const FName Key(*Name);bool Found=false;int32 Index=INDEX_NONE;
    if(Socket){Read.SDK(TEXT("socket_exists_calls"));Found=Read.Component->DoesSocketExist(Key);}
    else{Read.SDK(TEXT("bone_index_calls"));Index=Read.Component->GetBoneIndex(Key);Found=Index!=INDEX_NONE;}
    if(!Read.Current()){return false;}Row->SetBoolField(TEXT("valid"),Found);
    if(!Found)
    {const FString Error=(Socket?TEXT("Socket not found: "):TEXT("Bone not found: "))+Name;return Read.Text(Row,TEXT("error"),Error)&&Read.Warning(Error);}
    FName Parent=NAME_None;
    if(Socket){Read.SDK(TEXT("socket_bone_calls"));Parent=Read.Component->GetSocketBoneName(Key);}
    else{Row->SetNumberField(TEXT("bone_index"),Index);Read.SDK(TEXT("parent_bone_calls"));Parent=Read.Component->GetParentBone(Key);}
    if(!Read.Current()||!Read.Text(Row,Socket?TEXT("socket_bone"):TEXT("parent_bone"),Parent.ToString())){return false;}
    Read.SDK(TEXT("socket_transform_calls"));const FTransform World=Read.Component->GetSocketTransform(Key,RTS_World);
    if(!Read.Current()||!Read.Transform(Row,TEXT("world"),World)){return false;}
    Read.SDK(TEXT("socket_transform_calls"));const FTransform Component=Read.Component->GetSocketTransform(Key,RTS_Component);
    return Read.Current()&&Read.Transform(Row,TEXT("component"),Component);
}
}
// FI-specific execution context. FC observation primitives below remain transport independent.
using Sampling::Live;
using Sampling::SafeText;
using Sampling::Decode;
bool Decode(const FString& Text,TSharedPtr<FJsonValue>& Value,FString& Error)
{if(Sampling::Decode(Text,Value)){return true;}Error=TEXT("Carrier requires one finite JSON value within1Mi UTF16/depth64/65536slots/no raw NUL");return false;}
using Sampling::ParseNames;
using Sampling::FiniteTransform;
using Sampling::WorldType;
using Variable::FResult;
constexpr int32 MaxGraphs=4096,MaxNodes=65536,MaxPins=262144,MaxFields=65536;
bool Running=false;
struct FIdentity
{
    TStrongObjectPtr<UObject> Object,Outer,Package;TStrongObjectPtr<UClass> Class;
    FString Path,PackagePath;
    explicit FIdentity(UObject* O):Object(O),Outer(O?O->GetOuter():nullptr),Package(O?O->GetPackage():nullptr),Class(O?O->GetClass():nullptr),Path(GetPathNameSafe(O)),PackagePath(GetPathNameSafe(O?O->GetPackage():nullptr)){}
    bool Current() const
    {return Live(Object.Get())&&Live(Class.Get())&&Live(Package.Get())&&!Class->HasAnyClassFlags(CLASS_NewerVersionExists)&&Object->GetClass()==Class.Get()&&Object->GetOuter()==Outer.Get()&&Object->GetPackage()==Package.Get()&&Object->GetPathName()==Path&&Package->GetPathName()==PackagePath;}
};
struct FPinInventory
{
    UEdGraphNode* Node=nullptr;TArray<UEdGraphPin*> Pins;
};
struct FWork:Sampling::FRead
{
    UBlueprint* Blueprint=nullptr;UEdGraph* Graph=nullptr;UAnimGraphNode_Base* Node=nullptr;
    UClass* OriginalGeneratedClass=nullptr;
    UAnimInstance* RuntimeAnim=nullptr;UClass* RuntimeClass=nullptr;bool RuntimePost=false;TArray<FStructProperty*> RuntimeProperties;
    TArray<TObjectPtr<UEdGraphNode>> Nodes;
    TArray<FPinInventory> PinInventories;
    TSet<const UEdGraphPin*> KnownPins;
    TArray<TFunction<bool()>> ValueGuards;
    TSharedRef<FJsonObject> Params=MakeShared<FJsonObject>(),TargetJson=MakeShared<FJsonObject>();
    int32 PropertyReads=0;bool ExplicitChanged=false,SourceDirtyBefore=false,SourceDirtyCaptured=false;
    FString SelectedBlueprintName,SourcePath;UPackage* SourcePackage=nullptr;
    FWork()
    {
        Result.Data->Values.Reset();Legacy->Values.Reset();Calls->Values.Reset();
        Result.Data->SetObjectField(TEXT("legacy_result"),Legacy);Result.Data->SetObjectField(TEXT("native_call_counts"),Calls);
        Result.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);Result.Data->SetBoolField(TEXT("atomic_rollback"),false);
        Result.Data->SetBoolField(TEXT("explicit_save_attempted"),false);Result.Data->SetBoolField(TEXT("whole_SDK_purity_verified"),false);
        Result.Data->SetBoolField(TEXT("runtime_pose_capture_freshness_verified"),false);
        Result.Data->SetStringField(TEXT("effect_scope"),TEXT("Explicit debug/graph/temp/tick/refresh/compiler/cleanup receipts are separate from opaque SDK load/callback/cache/implicit-init/implicit-save effects. No request is retried."));
    }
    bool Fail(const TCHAR* Code,const FString& Message)
    {if(Result.Error.IsEmpty()){Result.ErrorCode=FString(TEXT("anim_node_pre_post_"))+Code;Result.Error=Message;}return false;}
    bool Basic(){return Sampling::FRead::Current();}
    bool Idle(const TCHAR* Stage)
    {
        Result.Stage=Stage;
        return (Basic()&&!IsPlaySessionActive())||Fail(TEXT("asset_write_requires_idle"),TEXT("This actual asset writer requires the live idle Editor; no PIE/SIE session is started, stopped or bypassed"));
    }
    void Mutation(const TCHAR* Key){Result.bMutationAttempted=true;Result.bChangeStateKnown=false;Count(Key);}
    void Changed(){ExplicitChanged=true;Result.bChanged=true;Result.bChangeStateKnown=false;}
    bool SourceCurrent()
    {
        if(!Basic()){return false;}
        if(Blueprint&&(!Live(Blueprint)||Blueprint->GeneratedClass!=OriginalGeneratedClass)){return Fail(TEXT("source_class_changed"),TEXT("Selected Blueprint generated-class identity changed during an SDK callback"));}
        if(Graph)
        {
            if(!Live(Graph)||Graph->Nodes!=Nodes){return Fail(TEXT("graph_inventory_changed"),TEXT("Selected source graph node slots changed after callback"));}
            for(const auto& Entry:PinInventories)
            {if(!Live(Entry.Node)||Entry.Node->GetGraph()!=Graph||Entry.Node->Pins!=Entry.Pins){return Fail(TEXT("pin_inventory_changed"),TEXT("Source node/pin ownership or exact pin slots changed"));}}
        }
        if(RuntimeAnim)
        {
            if(!Component||!Live(RuntimeAnim)||RuntimeAnim->GetOuter()!=Component||RuntimeAnim->GetClass()!=RuntimeClass||(RuntimePost?Component->GetPostProcessInstance():Component->GetAnimInstance())!=RuntimeAnim||RuntimeAnim->IsRunningParallelEvaluation()){return Fail(TEXT("selected_instance_changed"),TEXT("Selected main/post-process instance identity or parallel state changed"));}
            if(auto* AC=Cast<UAnimBlueprintGeneratedClass>(RuntimeClass)){if(AC->GetAnimNodeProperties()!=RuntimeProperties){return Fail(TEXT("runtime_property_inventory_changed"),TEXT("Compiled node property slots changed"));}}
        }
        for(const auto& Guard:ValueGuards){if(!Guard()){return Fail(TEXT("value_storage_changed"),TEXT("An enclosing reflected value/array storage changed after callback"));}}
        return Basic();
    }
    bool Current(){return SourceCurrent();}
    bool Pin(const UEdGraphPin* P)
    {
        if(!P){return false;}if(!Current()||!KnownPins.Contains(P)){return Fail(TEXT("pin_unavailable"),TEXT("Pin is outside the captured exact source inventory"));}
        if(P->WasTrashed()){return Fail(TEXT("pin_unavailable"),TEXT("Captured pin was trashed"));}
        auto* Owner=P->GetOwningNodeUnchecked();return (Live(Owner)&&Owner->Pins.Contains(const_cast<UEdGraphPin*>(P))&&Owner->GetGraph()==Graph)||Fail(TEXT("pin_unavailable"),TEXT("Captured pin owner membership changed"));
    }
    bool CaptureGraph(UEdGraph* G)
    {
        Graph=nullptr;Node=nullptr;Nodes.Reset();PinInventories.Reset();KnownPins.Reset();
        if(!Basic()||!Live(G)||!Hold(G)){return false;}const auto Snapshot=G->Nodes;
        if(Snapshot.Num()>MaxNodes){return Fail(TEXT("node_bound"),TEXT("Selected graph exceeds65536 node slots"));}
        int32 Slots=0;
        for(UEdGraphNode* N:Snapshot)
        {
            if(!N){continue;}if(!Live(N)||N->GetGraph()!=G||!Hold(N)){return Fail(TEXT("node_unavailable"),TEXT("Selected graph has an unloaded or foreign node"));}
            if(N->Pins.Num()>MaxPins-Slots){return Fail(TEXT("pin_bound"),TEXT("Selected graph exceeds262144 pin slots"));}Slots+=N->Pins.Num();
            PinInventories.Add({N,N->Pins});for(UEdGraphPin* P:N->Pins){if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N){return Fail(TEXT("pin_unavailable"),TEXT("Invalid root pin slot in selected graph"));}KnownPins.Add(P);}
        }
        Graph=G;Nodes=Snapshot;
        for(const auto& Entry:PinInventories){for(UEdGraphPin* P:Entry.Pins){for(UEdGraphPin* L:P->LinkedTo){if(!KnownPins.Contains(L)){return Fail(TEXT("link_unavailable"),TEXT("Pin link points outside the captured graph inventory"));}}}}
        return Current();
    }
    FResult Finish()
    {
        if(!Result.bSuccess&&Result.Error.IsEmpty()){Fail(TEXT("operation_incomplete"),TEXT("Operation did not close its postconditions; partial receipts retained without retry"));}
        if(!Legacy->HasField(TEXT("success"))){Legacy->SetBoolField(TEXT("success"),Result.bSuccess);}
        Result.Data->SetBoolField(TEXT("standard_postcondition_may_strengthen_legacy_cleanup"),true);
        if(!Result.Error.IsEmpty()){Legacy->SetStringField(TEXT("error"),Result.Error);Errors.Add(MakeShared<FJsonValueString>(Result.Error));}
        if(!Legacy->HasField(TEXT("errors"))){Legacy->SetArrayField(TEXT("errors"),Errors);}if(!Legacy->HasField(TEXT("warnings"))){Legacy->SetArrayField(TEXT("warnings"),Warnings);}
        Result.Data->SetStringField(TEXT("dirty_observation_scope"),TEXT("Captured original source package; identity failure does not prove current replacement package dirty state"));
        Result.Data->SetBoolField(TEXT("observed_explicit_change"),ExplicitChanged);
        Result.Data->SetBoolField(TEXT("legacy_read_only_is_intent_not_purity"),true);
        Result.Data->SetNumberField(TEXT("observation_checks"),Checks);
        if(SourceDirtyCaptured&&Live(SourcePackage)){Legacy->SetBoolField(TEXT("source_dirty_before"),SourceDirtyBefore);Legacy->SetBoolField(TEXT("source_dirty_after"),SourcePackage->IsDirty());}
        return Result;
    }
};
FWork* CurrentWork=nullptr;
bool Current(){return CurrentWork&&CurrentWork->Current();}
void Unsafe(const TCHAR* Message){if(CurrentWork){CurrentWork->Fail(TEXT("observation_incomplete"),Message);}}
bool Layout(const FProperty* P,int32 Size)
{return P&&P->ArrayDim>0&&P->ArrayDim<=4096&&P->GetOffset_ForInternal()>=0&&P->GetSize()>0&&P->GetOffset_ForInternal()<=Size&&P->GetSize()<=Size-P->GetOffset_ForInternal();}
TArray<TSharedPtr<FJsonValue>> Strings(const TArray<FString>& Rows)
{TArray<TSharedPtr<FJsonValue>> Out;for(const FString& Row:Rows){Out.Add(MakeShared<FJsonValueString>(Row));}return Out;}
FString StringParam(const TSharedRef<FJsonObject>& P,const TCHAR* Key,const FString& Default=FString())
{FString Value;return P->TryGetStringField(Key,Value)?Value:Default;}
bool BoolParam(const TSharedRef<FJsonObject>& P,const TCHAR* Key,bool Default)
{return P->HasField(Key)?P->GetBoolField(Key):Default;}
TSharedRef<FJsonObject> TransformJson(const FTransform& T)
{
    auto J=MakeShared<FJsonObject>();const FVector V=T.GetTranslation(),S=T.GetScale3D();const FQuat Q=T.GetRotation();const FRotator E=Q.Rotator();
    auto A=[](std::initializer_list<double> Values){TArray<TSharedPtr<FJsonValue>> O;for(double X:Values){O.Add(MakeShared<FJsonValueNumber>(X));}return O;};
    J->SetArrayField(TEXT("translation"),A({V.X,V.Y,V.Z}));J->SetArrayField(TEXT("rotation"),A({Q.X,Q.Y,Q.Z,Q.W}));J->SetArrayField(TEXT("rotation_euler"),A({E.Pitch,E.Yaw,E.Roll}));J->SetArrayField(TEXT("scale"),A({S.X,S.Y,S.Z}));return J;
}
FString PointerText(const void* P){return FString::Printf(TEXT("%p"),P);}
struct FFieldId
{
    FProperty* Property;TStrongObjectPtr<UStruct> Owner;FName Name,RootName;int32 Offset,Size,Dim;TArray<FProperty*> Chain;UScriptStruct* StructType=nullptr;
    explicit FFieldId(FProperty* P):Property(P),Owner(P?P->GetOwnerStruct():nullptr),Name(P?P->GetFName():NAME_None),Offset(P?P->GetOffset_ForInternal():0),Size(P?P->GetSize():0),Dim(P?P->ArrayDim:0)
    {for(FProperty* At=P;At&&Chain.Num()<64;At=At->GetOwner<FProperty>()){Chain.Insert(At,0);}RootName=Chain.IsEmpty()?NAME_None:Chain[0]->GetFName();if(auto* S=CastField<FStructProperty>(P)){StructType=S->Struct;}}
    bool Current() const
    {
        if(!Live(Owner.Get())||Chain.IsEmpty()||Chain.Num()>=64||Owner->FindPropertyByName(RootName)!=Chain[0]){return false;}
        for(int32 I=1;I<Chain.Num();++I){auto* A=CastField<FArrayProperty>(Chain[I-1]);if(!A||A->Inner!=Chain[I]){return false;}}
        if(auto* S=CastField<FStructProperty>(Property)){if(S->Struct!=StructType){return false;}}
        return Property->GetOwnerStruct()==Owner.Get()&&Property->GetFName()==Name&&Property->GetOffset_ForInternal()==Offset&&Property->GetSize()==Size&&Property->ArrayDim==Dim;
    }
};
TSharedPtr<FJsonValue> PropertyJson(FProperty* P,const void* Value,int32 Depth,int32 MaxDepth);
TSharedRef<FJsonObject> StructJson(UScriptStruct* Type,const void* Value,int32 Depth,int32 MaxDepth)
{
    auto J=MakeShared<FJsonObject>();if(!Current()||!Live(Type)||!Value||!CurrentWork->Hold(Type)){return J;}J->SetStringField(TEXT("_struct_type"),Type->GetName());if(Depth>=MaxDepth){return J;}
    TArray<TUniquePtr<FFieldId>> Fields;for(TFieldIterator<FProperty> It(Type,EFieldIterationFlags::IncludeSuper);It;++It){if(Fields.Num()>=MaxFields){Unsafe(TEXT("Struct field inventory exceeds65536"));return J;}Fields.Add(MakeUnique<FFieldId>(*It));}
    for(const auto& Id:Fields)
    {
        if(!Current()||!Id->Current()||!Layout(Id->Property,Type->GetStructureSize())){Unsafe(TEXT("Struct field/layout changed during property callback"));return J;}
        auto V=PropertyJson(Id->Property,Id->Property->ContainerPtrToValuePtr<void>(Value),Depth+1,MaxDepth);if(!Current()||!Id->Current()){return J;}J->SetField(Id->Name.ToString(),V);
    }return J;
}
TSharedPtr<FJsonValue> PropertyJson(FProperty* P,const void* Value,int32 Depth,int32 MaxDepth)
{
    if(!Current()||!P||!Value||Depth>MaxDepth+1||++CurrentWork->PropertyReads>MaxFields){Unsafe(TEXT("Reflected value/depth budget exceeded"));return MakeShared<FJsonValueNull>();}
    FFieldId Id(P);if(!Id.Current()){Unsafe(TEXT("Reflected descriptor identity changed"));return MakeShared<FJsonValueNull>();}
    if(auto* B=CastField<FBoolProperty>(P)){return MakeShared<FJsonValueBoolean>(B->GetPropertyValue(Value));}
    if(auto* E=CastField<FEnumProperty>(P)){auto J=MakeShared<FJsonObject>();const int64 N=E->GetUnderlyingProperty()->GetSignedIntPropertyValue(Value);J->SetNumberField(TEXT("value"),static_cast<double>(N));J->SetStringField(TEXT("name"),E->GetEnum()->GetNameStringByValue(N));J->SetStringField(TEXT("enum_type"),E->GetEnum()->GetName());return MakeShared<FJsonValueObject>(J);}
    if(auto* B=CastField<FByteProperty>(P)){const uint8 V=B->GetPropertyValue(Value);if(B->Enum){auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("value"),V);J->SetStringField(TEXT("name"),B->Enum->GetNameStringByValue(V));J->SetStringField(TEXT("enum_type"),B->Enum->GetName());return MakeShared<FJsonValueObject>(J);}return MakeShared<FJsonValueNumber>(V);}
    if(auto* N=CastField<FNumericProperty>(P)){const double V=N->IsFloatingPoint()?N->GetFloatingPointPropertyValue(Value):static_cast<double>(N->GetSignedIntPropertyValue(Value));if(!FMath::IsFinite(V)){Unsafe(TEXT("Nonfinite reflected number"));return MakeShared<FJsonValueNull>();}return MakeShared<FJsonValueNumber>(V);}
    if(auto* N=CastField<FNameProperty>(P)){return MakeShared<FJsonValueString>(N->GetPropertyValue(Value).ToString());}
    if(auto* S=CastField<FStrProperty>(P)){const FString V=S->GetPropertyValue(Value);if(!SafeText(V,65536)){Unsafe(TEXT("Reflected string exceeds65536"));return MakeShared<FJsonValueNull>();}return MakeShared<FJsonValueString>(V);}
    if(auto* T=CastField<FTextProperty>(P)){const FString V=T->GetPropertyValue(Value).ToString();if(!Current()||!Id.Current()||!SafeText(V,65536)){Unsafe(TEXT("Reflected localized text changed or exceeds bound"));return MakeShared<FJsonValueNull>();}return MakeShared<FJsonValueString>(V);}
    if(auto* O=CastField<FObjectPropertyBase>(P)){UObject* V=O->GetObjectPropertyValue(Value);if(!V){return MakeShared<FJsonValueNull>();}if(!Live(V)||!CurrentWork->Hold(V)){Unsafe(TEXT("Reflected object is unavailable"));return MakeShared<FJsonValueNull>();}return MakeShared<FJsonValueString>(V->GetPathName());}
    if(auto* S=CastField<FSoftObjectProperty>(P)){return MakeShared<FJsonValueString>(S->GetPropertyValue(Value).ToString());}
    if(auto* S=CastField<FSoftClassProperty>(P)){return MakeShared<FJsonValueString>(S->GetPropertyValue(Value).ToString());}
    if(auto* S=CastField<FStructProperty>(P)){if(Depth<MaxDepth){return MakeShared<FJsonValueObject>(StructJson(S->Struct,Value,Depth,MaxDepth));}}
    if(auto* A=CastField<FArrayProperty>(P))
    {
        TArray<TSharedPtr<FJsonValue>> Out;const auto ExpectedArrayFlags=A->ArrayFlags;FScriptArrayHelper Initial(A,Value);const int32 Num=Initial.Num();const void* Storage=Num?Initial.GetRawPtr(0):nullptr;if(Num<0||Num>MaxFields){Unsafe(TEXT("Settings array exceeds65536"));return MakeShared<FJsonValueArray>(Out);}
        FFieldId Inner(A->Inner);if(CurrentWork->ValueGuards.Num()>=64){Unsafe(TEXT("Reflected value guard nesting exceeds64"));return MakeShared<FJsonValueArray>(Out);}
        CurrentWork->ValueGuards.Add([&](){if(!Id.Current()||!Inner.Current()||A->ArrayFlags!=ExpectedArrayFlags){return false;}FScriptArrayHelper Fresh(A,Value);return Fresh.Num()==Num&&(!Num||Fresh.GetRawPtr(0)==Storage);});ON_SCOPE_EXIT{CurrentWork->ValueGuards.Pop();};
        for(int32 I=0;I<Num;++I){if(!Current()||!Id.Current()||!Inner.Current()||A->ArrayFlags!=ExpectedArrayFlags){Unsafe(TEXT("Array descriptor/allocator flags changed"));break;}FScriptArrayHelper Fresh(A,Value);if(Fresh.Num()!=Num||(Num&&Fresh.GetRawPtr(0)!=Storage)){Unsafe(TEXT("Settings array storage changed during callback"));break;}Out.Add(PropertyJson(A->Inner,Fresh.GetRawPtr(I),Depth+1,MaxDepth));}return MakeShared<FJsonValueArray>(Out);
    }
    FString Out;CurrentWork->SDK(TEXT("property_export_calls"));P->ExportTextItem_Direct(Out,Value,nullptr,nullptr,PPF_None);if(!Current()||!Id.Current()||!SafeText(Out,65536)){Unsafe(TEXT("Export callback changed descriptor/owner or exceeded text bound"));return MakeShared<FJsonValueNull>();}
    if(auto* S=CastField<FStructProperty>(P)){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("_struct_type"),GetNameSafe(S->Struct));J->SetStringField(TEXT("_export_text"),Out);return MakeShared<FJsonValueObject>(J);}return MakeShared<FJsonValueString>(Out);
}
FString NormalizeBlueprintPath(const FString& Raw)
{
    FString Path=FPackageName::ExportTextPathToObjectPath(Raw).TrimStartAndEnd();Path.TrimQuotesInline();if(Path.EndsWith(TEXT("_C"))){Path.LeftChopInline(2);}
    if((Path.StartsWith(TEXT("/Game/"))||Path.StartsWith(TEXT("/Engine/")))&&!Path.Contains(TEXT("."))){Path+=TEXT(".")+FPackageName::GetShortName(Path);}return Path;
}
UBlueprint* LoadBlueprint(const FString& Raw,FWork& W)
{
    const FString Path=NormalizeBlueprintPath(Raw);if(!SafeText(Path,NAME_SIZE-1)){W.Fail(TEXT("blueprint_path_bound"),TEXT("Normalized Blueprint path exceeds FName safety bound"));return nullptr;}
    auto Load=[&](const FString& ObjectPath)->UBlueprint*
    {
        const FString Package=FPackageName::ObjectPathToPackageName(ObjectPath);if(Package.IsEmpty()||!FPackageName::IsValidLongPackageName(Package,true)||ObjectPath.Contains(TEXT("//"))){return nullptr;}
        W.SDK(TEXT("blueprint_load_calls"));UBlueprint* B=LoadObject<UBlueprint>(nullptr,*ObjectPath);W.Count(TEXT("blueprint_load_returns"));if(!W.Basic()){return nullptr;}if(B){return B;}
        W.SDK(TEXT("blueprint_class_load_calls"));UClass* C=LoadObject<UClass>(nullptr,*(ObjectPath.EndsWith(TEXT("_C"))?ObjectPath:ObjectPath+TEXT("_C")));if(!W.Basic()||!Live(C)){return nullptr;}return Cast<UBlueprint>(C->ClassGeneratedBy);
    };
    if(UBlueprint* Direct=Load(Path)){return Direct;}
    if(Raw.Contains(TEXT("/"))||Raw.Contains(TEXT("."))){return nullptr;}
    W.SDK(TEXT("asset_registry_module_acquisition_calls"));auto& Module=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));if(!W.Basic()){return nullptr;}
    TArray<FAssetData> Assets;W.SDK(TEXT("asset_registry_queries"));Module.Get().GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(),Assets,true);if(!W.Basic()){return nullptr;}
    if(Assets.Num()>65536){W.Fail(TEXT("blueprint_search_bound"),TEXT("Blueprint registry result exceeds65536"));return nullptr;}
    const FString Short=FPackageName::GetShortName(Path);
    for(const FAssetData& A:Assets){if(!W.Step()){return nullptr;}if(A.AssetName.ToString().Equals(Raw,ESearchCase::IgnoreCase)||A.AssetName.ToString().Equals(Short,ESearchCase::IgnoreCase)||A.PackageName.ToString().Equals(Raw,ESearchCase::IgnoreCase)||A.GetObjectPathString().Equals(Path,ESearchCase::IgnoreCase)){if(UBlueprint* B=Load(A.GetObjectPathString())){return B;}}}return nullptr;
}
FString GraphTypeName(const UBlueprint* B,const UEdGraph* G)
{
    if(!B||!G){return FString();}if(B->FunctionGraphs.Contains(G)){return TEXT("function");}if(B->MacroGraphs.Contains(G)){return TEXT("macro");}if(B->UbergraphPages.Contains(G)){return TEXT("event");}if(B->DelegateSignatureGraphs.Contains(G)){return TEXT("delegate");}return TEXT("nested");
}
FString NormalizeGraphType(FString Type)
{
    Type=Type.ToLower();Type.ReplaceInline(TEXT(" "),TEXT(""));Type.ReplaceInline(TEXT("-"),TEXT("_"));
    if(Type==TEXT("function_graph")||Type==TEXT("functiongraph")){return TEXT("function");}if(Type==TEXT("event_graph")||Type==TEXT("eventgraph")||Type==TEXT("ubergraph")||Type==TEXT("uber_graph")){return TEXT("event");}if(Type==TEXT("macro_graph")||Type==TEXT("macrograph")){return TEXT("macro");}return Type;
}
TSharedRef<FJsonObject> GraphJson(UBlueprint* B,UEdGraph* G)
{
    auto J=MakeShared<FJsonObject>();if(!G){J->SetBoolField(TEXT("valid"),false);return J;}J->SetStringField(TEXT("name"),G->GetName());J->SetStringField(TEXT("graph_name"),G->GetName());J->SetStringField(TEXT("graph_id"),G->GraphGuid.ToString());J->SetStringField(TEXT("graph_type"),GraphTypeName(B,G));J->SetStringField(TEXT("path"),G->GetPathName());J->SetStringField(TEXT("schema"),GetNameSafe(G->GetSchema()?G->GetSchema()->GetClass():nullptr));J->SetNumberField(TEXT("node_count"),G->Nodes.Num());return J;
}
TSharedRef<FJsonObject> PinJson(UEdGraphPin* P,FWork& W)
{
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("valid"),false);if(!P){return J;}if(!W.Pin(P)){return J;}
    J->SetBoolField(TEXT("valid"),true);J->SetStringField(TEXT("name"),P->PinName.ToString());J->SetStringField(TEXT("pin_id"),P->PinId.ToString());J->SetStringField(TEXT("direction"),P->Direction==EGPD_Input?TEXT("input"):TEXT("output"));J->SetStringField(TEXT("default_value"),P->DefaultValue);J->SetStringField(TEXT("category"),P->PinType.PinCategory.ToString());J->SetStringField(TEXT("subcategory"),P->PinType.PinSubCategory.ToString());J->SetBoolField(TEXT("is_array"),P->PinType.IsArray());if(P->DefaultObject){if(!Live(P->DefaultObject)||!W.Hold(P->DefaultObject)){return J;}J->SetStringField(TEXT("default_object"),P->DefaultObject->GetPathName());}
    auto Type=MakeShared<FJsonObject>();Type->SetStringField(TEXT("category"),P->PinType.PinCategory.ToString());Type->SetStringField(TEXT("subcategory"),P->PinType.PinSubCategory.ToString());Type->SetBoolField(TEXT("is_array"),P->PinType.IsArray());UObject* Sub=P->PinType.PinSubCategoryObject.Get();if(Sub){if(!Live(Sub)||!W.Hold(Sub)){return J;}Type->SetStringField(TEXT("subcategory_object"),Sub->GetPathName());J->SetStringField(TEXT("subcategory_object"),Sub->GetPathName());}Type->SetBoolField(TEXT("is_reference"),P->PinType.bIsReference);Type->SetBoolField(TEXT("is_const"),P->PinType.bIsConst);J->SetObjectField(TEXT("pin_type"),Type);
    TArray<TSharedPtr<FJsonValue>> Links;for(UEdGraphPin* L:P->LinkedTo){if(!W.Pin(L)){return J;}auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("node_id"),L->GetOwningNodeUnchecked()->NodeGuid.ToString());Q->SetStringField(TEXT("node_name"),L->GetOwningNodeUnchecked()->GetName());Q->SetStringField(TEXT("pin_name"),L->PinName.ToString());Q->SetStringField(TEXT("pin_id"),L->PinId.ToString());Links.Add(MakeShared<FJsonValueObject>(Q));}J->SetArrayField(TEXT("linked_to"),Links);return J;
}
TSharedRef<FJsonObject> NodeJson(UEdGraphNode* N,bool Pins,FWork& W)
{
    auto J=MakeShared<FJsonObject>();if(!N){return J;}if(!W.Current()||!Live(N)||!W.Nodes.Contains(N)){W.Fail(TEXT("node_unavailable"),TEXT("Node serializer received a foreign source node"));return J;}
    J->SetStringField(TEXT("id"),N->NodeGuid.ToString());J->SetStringField(TEXT("node_id"),N->NodeGuid.ToString());J->SetStringField(TEXT("name"),N->GetName());J->SetStringField(TEXT("type"),N->GetClass()->GetName());J->SetStringField(TEXT("class"),N->GetClass()->GetName());J->SetStringField(TEXT("class_path"),N->GetClass()->GetPathName());J->SetNumberField(TEXT("x"),N->NodePosX);J->SetNumberField(TEXT("y"),N->NodePosY);J->SetStringField(TEXT("node_comment"),N->NodeComment);J->SetBoolField(TEXT("comment_bubble_visible"),N->bCommentBubbleVisible);J->SetBoolField(TEXT("comment_bubble_pinned"),N->bCommentBubblePinned);J->SetNumberField(TEXT("pos_x"),N->NodePosX);J->SetNumberField(TEXT("pos_y"),N->NodePosY);
    W.SDK(TEXT("node_title_calls"));const FString Title=N->GetNodeTitle(ENodeTitleType::FullTitle).ToString();if(!W.Current()||!SafeText(Title,65536)){W.Fail(TEXT("node_title_changed"),TEXT("Node title callback changed source or exceeded bound"));return J;}J->SetStringField(TEXT("title"),Title);
    if(Pins){TArray<TSharedPtr<FJsonValue>> Rows;const auto Snapshot=N->Pins;for(UEdGraphPin* P:Snapshot){Rows.Add(MakeShared<FJsonValueObject>(PinJson(P,W)));if(!W.Current()||N->Pins!=Snapshot){return J;}}J->SetArrayField(TEXT("pins"),Rows);}return J;
}
TSharedRef<FJsonObject> SettingsJson(UAnimGraphNode_Base* N,int32 Depth,FWork& W)
{
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("has_anim_node_struct"),false);if(!W.Current()){return J;}TArray<TUniquePtr<FFieldId>> Fields;for(TFieldIterator<FProperty> It(N->GetClass(),EFieldIterationFlags::IncludeSuper);It;++It){Fields.Add(MakeUnique<FFieldId>(*It));if(Fields.Num()>MaxFields){W.Fail(TEXT("field_bound"),TEXT("Editor node field inventory exceeds65536"));return J;}}
    for(const auto& Id:Fields){if(!W.Current()||!Id->Current()){return J;}auto* P=CastField<FStructProperty>(Id->Property);if(!P||!Live(P->Struct)||!P->Struct->IsChildOf(FAnimNode_Base::StaticStruct())){continue;}if(!Layout(P,N->GetClass()->GetPropertiesSize())||!W.Hold(P->Struct)){W.Fail(TEXT("node_layout"),TEXT("Editor node struct does not fit current class layout"));return J;}J->SetBoolField(TEXT("has_anim_node_struct"),true);J->SetStringField(TEXT("anim_node_property"),P->GetName());J->SetStringField(TEXT("anim_node_struct_type"),P->Struct->GetName());J->SetObjectField(TEXT("anim_node_properties"),StructJson(P->Struct,P->ContainerPtrToValuePtr<void>(N),0,Depth));return J;}return J;
}
UEdGraphPin* PosePin(UAnimGraphNode_Base* N,EEdGraphPinDirection Direction,FWork& W)
{
    for(bool Component:{true,false})
    {
        const TArray<FString> Preferred=Component?(Direction==EGPD_Input?TArray<FString>{TEXT("ComponentPose")}:TArray<FString>{TEXT("Pose"),TEXT("ComponentPose")}):TArray<FString>{TEXT("Pose"),TEXT("Result")};
        auto Match=[&](UEdGraphPin* P){if(!W.Pin(P)){return false;}auto* T=Cast<UScriptStruct>(P->PinType.PinSubCategoryObject.Get());return P->Direction==Direction&&!P->ParentPin&&P->PinType.PinCategory==UEdGraphSchema_K2::PC_Struct&&T&&(Component?T->IsChildOf(FComponentSpacePoseLink::StaticStruct()):T->IsChildOf(FPoseLink::StaticStruct()));};
        for(const FString& Name:Preferred){for(UEdGraphPin* P:N->Pins){if(Match(P)&&P->PinName.ToString().Equals(Name,ESearchCase::IgnoreCase)){return P;}}}for(UEdGraphPin* P:N->Pins){if(Match(P)){return P;}}
    }return nullptr;
}
struct FAnimNodePoseTransformSample
{
    bool bValid = false;
    FTransform WorldTransform = FTransform::Identity;
    FTransform ComponentTransform = FTransform::Identity;
    FString Error;
};

struct FAnimNodePoseCapture
{
    TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
    TMap<FString, FAnimNodePoseTransformSample> BoneSamples;
    TMap<FString, FAnimNodePoseTransformSample> SocketSamples;
};

TArray<FString> GetDefaultAnimNodePrePostSampleBones()
{
    return {
        TEXT("Head_02"),
        TEXT("TailEnd"),
        TEXT("R_Stalk_04"),
        TEXT("L_Stalk_04")
    };
}

struct FTransientAnimPoseWatchRegistration
{
    FString Role;
    int32 NodeID = INDEX_NONE;
    UPoseWatch* PoseWatch = nullptr;
    UPoseWatchPoseElement* PoseElement = nullptr;
    bool bRegistered = false;
    bool bPreExisting = false;
    bool bCreatedTransient = false;
};
struct FRuntimePoseLinkSelection
{
    int32 Index = INDEX_NONE;
    int32 LinkID = INDEX_NONE;
    FString FieldPath;
    TSharedPtr<FJsonObject> LinkObject;
};
// Runtime operations are direct public Engine calls; no Toolset/RPC is involved.
TSharedPtr<FJsonObject> ErrorResponse(const FString& Message)
{auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),false);J->SetStringField(TEXT("error"),Message);return J;}
FString GetStringParam(const TSharedPtr<FJsonObject>& P,const FString& Key,const FString& Default=FString())
{FString V;return P.IsValid()&&P->TryGetStringField(Key,V)?V:Default;}
bool GetBoolParam(const TSharedPtr<FJsonObject>& P,const FString& Key,bool Default)
{return P.IsValid()&&P->HasField(Key)?P->GetBoolField(Key):Default;}
int32 GetClampedIntParam(const TSharedPtr<FJsonObject>& P,const FString& Key,int32 Default,int32 Min,int32 Max)
{double V=Default;if(P){P->TryGetNumberField(Key,V);}return FMath::IsFinite(V)?static_cast<int32>(FMath::Clamp(V,static_cast<double>(Min),static_cast<double>(Max))):Default;}
double GetClampedNumberParam(const TSharedPtr<FJsonObject>& P,const FString& Key,double Default,double Min,double Max)
{double V=Default;if(P){P->TryGetNumberField(Key,V);}return FMath::IsFinite(V)?FMath::Clamp(V,Min,Max):Default;}
bool GetJsonBoolFieldOrDefault(const TSharedPtr<FJsonObject>& P,const FString& Key,bool Default)
{bool V=Default;return P&&P->TryGetBoolField(Key,V)?V:Default;}
FString GetJsonStringFieldOrDefault(const TSharedPtr<FJsonObject>& P,const FString& Key,const FString& Default)
{return GetStringParam(P,Key,Default);}
TArray<TSharedPtr<FJsonValue>> StringArrayToJsonValues(const TArray<FString>& V){return Strings(V);}
TArray<TSharedPtr<FJsonValue>> VectorToJsonArray(const FVector& V)
{return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)};}
TSharedPtr<FJsonObject> TransformToJsonObject(const FTransform& V){return TransformJson(V);}
FString PointerToJsonString(const void* P){return PointerText(P);}
TSharedPtr<FJsonObject> NodeToJson(UEdGraphNode* N,bool Pins)
{return N&&Current()?NodeJson(N,Pins,*CurrentWork):MakeShared<FJsonObject>();}
TSharedPtr<FJsonObject> PinToJson(UEdGraphPin* P)
{return P&&Current()?PinJson(P,*CurrentWork):MakeShared<FJsonObject>();}
void AddGraphField(const TSharedPtr<FJsonObject>& J,UBlueprint* B,UEdGraph* G)
{if(J&&Current()){J->SetObjectField(TEXT("graph"),GraphJson(B,G));}}
FString GetAnimNodePrePostMVPKind(UAnimGraphNode_Base* Node)
{
    if (Cast<UAnimGraphNode_RigidBody>(Node))
    {
        return TEXT("rigidbody");
    }
    if (Cast<UAnimGraphNode_Trail>(Node))
    {
        return TEXT("trail");
    }
    if (Cast<UAnimGraphNode_ControlRig>(Node))
    {
        return TEXT("controlrig");
    }
    return TEXT("unsupported");
}
FString GetAnimNodePrePostSupportNote(UAnimGraphNode_Base* Node)
{
    if (Cast<UAnimGraphNode_RigidBody>(Node))
    {
        return TEXT("MVP target for isolated source-vs-output sampling with temporary probe assets.");
    }
    if (Cast<UAnimGraphNode_Trail>(Node))
    {
        return TEXT("MVP target for isolated source-vs-output sampling with safe _MCP_Temp or _MCP_Sample probe assets.");
    }
    if (Cast<UAnimGraphNode_ControlRig>(Node))
    {
        return TEXT("Direct transient ControlRig pre/post probing already exists; compiled AnimGraph-internal ControlRig source-vs-output attribution remains a later instrumentation mode.");
    }
    return TEXT("Dry-run resolver can identify this AnimGraph node, but the isolated runtime sampler MVP does not support it yet.");
}
bool IsOutputOnlyAnimPoseSourceNode(UAnimGraphNode_Base* Node)
{
    return Cast<UAnimGraphNode_AssetPlayerBase>(Node)
        || Cast<UAnimGraphNode_LinkedInputPose>(Node);
}
bool IsUnsupportedAnimPrePostBoundaryNode(UAnimGraphNode_Base* Node)
{
    return Cast<UAnimGraphNode_Root>(Node)
        || Cast<UAnimGraphNode_StateMachineBase>(Node)
        || Cast<UAnimGraphNode_TransitionResult>(Node)
        || Cast<UAnimGraphNode_LinkedAnimGraph>(Node);
}
void CollectRuntimePoseLinkSupportFields(
    const TSharedPtr<FJsonObject>& RuntimePoseLinksObject,
    int32& OutValidPoseLinkCount,
    bool& bOutHasComponentSpaceLink,
    TArray<FString>& OutFieldPaths)
{
    OutValidPoseLinkCount = 0;
    bOutHasComponentSpaceLink = false;
    OutFieldPaths.Reset();

    const TArray<TSharedPtr<FJsonValue>>* PoseLinkValues = nullptr;
    if (!RuntimePoseLinksObject.IsValid() || !RuntimePoseLinksObject->TryGetArrayField(TEXT("pose_links"), PoseLinkValues) || !PoseLinkValues)
    {
        return;
    }

    for (const TSharedPtr<FJsonValue>& PoseLinkValue : *PoseLinkValues)
    {
        if (!PoseLinkValue.IsValid() || PoseLinkValue->Type != EJson::Object)
        {
            continue;
        }

        const TSharedPtr<FJsonObject> PoseLinkObject = PoseLinkValue->AsObject();
        if (!PoseLinkObject.IsValid())
        {
            continue;
        }

        double LinkIDNumber = INDEX_NONE;
        bool bValid = false;
        PoseLinkObject->TryGetBoolField(TEXT("valid"), bValid);
        if (!bValid || !PoseLinkObject->TryGetNumberField(TEXT("link_id"), LinkIDNumber) || static_cast<int32>(LinkIDNumber) == INDEX_NONE)
        {
            continue;
        }

        ++OutValidPoseLinkCount;
        FString FieldPath;
        PoseLinkObject->TryGetStringField(TEXT("field_path"), FieldPath);
        if (!FieldPath.IsEmpty())
        {
            OutFieldPaths.Add(FieldPath);
        }

        FString PoseLinkStruct;
        PoseLinkObject->TryGetStringField(TEXT("pose_link_struct"), PoseLinkStruct);
        bOutHasComponentSpaceLink |= PoseLinkStruct.Contains(TEXT("ComponentSpacePoseLink"), ESearchCase::IgnoreCase);
    }
}
TSharedPtr<FJsonObject> BuildAnimNodeRuntimeMappingSupportJson(
    UAnimGraphNode_Base* Node,
    FStructProperty* RuntimeAnimNodeProperty,
    const TSharedPtr<FJsonObject>& RuntimePoseLinksObject,
    bool bRuntimeNodeMapped,
    bool bFindDebugMapped,
    const FString& EvidenceTierOverride = FString())
{
    TSharedPtr<FJsonObject> SupportObject = MakeShared<FJsonObject>();

    int32 ValidPoseLinkCount = 0;
    bool bHasComponentSpaceLink = false;
    TArray<FString> FieldPaths;
    CollectRuntimePoseLinkSupportFields(RuntimePoseLinksObject, ValidPoseLinkCount, bHasComponentSpaceLink, FieldPaths);

    const bool bNodeMapped = bRuntimeNodeMapped || bFindDebugMapped;
    const bool bRuntimePoseLinksChecked = RuntimePoseLinksObject.IsValid() && RuntimePoseLinksObject->HasField(TEXT("pose_links"));
    const bool bOutputOnlySourceNode = IsOutputOnlyAnimPoseSourceNode(Node);
    const bool bUnsupportedBoundaryNode = IsUnsupportedAnimPrePostBoundaryNode(Node);

    FString SupportClass = TEXT("static_only");
    FString UnsupportedReason;
    FString RecommendedMode = TEXT("compiled_graph_mapping");
    FString RecommendedInputPoseMode;
    FString EvidenceTier = EvidenceTierOverride.IsEmpty() ? TEXT("static_topology") : EvidenceTierOverride;
    bool bPoseWatchSupported = false;

    if (bUnsupportedBoundaryNode)
    {
        SupportClass = TEXT("unsupported_for_prepost");
        UnsupportedReason = TEXT("The selected node is a graph boundary or sink node where source-vs-output PoseWatch attribution is not stable.");
        EvidenceTier = EvidenceTierOverride.IsEmpty() && bNodeMapped ? TEXT("compiled_node_mapping") : EvidenceTier;
    }
    else if (bOutputOnlySourceNode)
    {
        SupportClass = TEXT("output_only_source_node");
        UnsupportedReason = TEXT("The selected node produces an output pose without a meaningful upstream input pose for pre/post attribution.");
        EvidenceTier = EvidenceTierOverride.IsEmpty() && bNodeMapped ? TEXT("compiled_node_mapping") : EvidenceTier;
    }
    else if (ValidPoseLinkCount > 0 && bNodeMapped)
    {
        if (ValidPoseLinkCount > 1)
        {
            SupportClass = TEXT("multi_input_pose_node");
            RecommendedInputPoseMode = TEXT("all");
        }
        else
        {
            SupportClass = bHasComponentSpaceLink ? TEXT("component_space_pose_node") : TEXT("single_input_pose_node");
            RecommendedInputPoseMode = TEXT("first");
        }

        bPoseWatchSupported = true;
        RecommendedMode = TEXT("pose_watch_capture");
        EvidenceTier = EvidenceTierOverride.IsEmpty() ? TEXT("runtime_pose_link_inventory") : EvidenceTierOverride;
    }
    else if (bNodeMapped)
    {
        SupportClass = TEXT("compiled_mapping_only");
        UnsupportedReason = bRuntimePoseLinksChecked
            ? TEXT("The compiled/live node maps, but no valid runtime FPoseLink or FComponentSpacePoseLink input was found.")
            : TEXT("The compiled/live node maps, but runtime pose-link inventory was not available in this response.");
        EvidenceTier = EvidenceTierOverride.IsEmpty() ? TEXT("compiled_node_mapping") : EvidenceTierOverride;
    }
    else if (bRuntimePoseLinksChecked)
    {
        SupportClass = TEXT("compiled_mapping_only");
        UnsupportedReason = TEXT("Runtime pose-link inventory was checked, but the selected node did not map to a live same-instance node.");
        EvidenceTier = EvidenceTierOverride.IsEmpty() ? TEXT("runtime_pose_link_inventory") : EvidenceTierOverride;
    }
    else
    {
        UnsupportedReason = TEXT("Only static editor topology is available. Run compiled_graph_mapping against a compatible live AnimInstance before claiming runtime attribution.");
    }

    SupportObject->SetStringField(TEXT("node_class"), Node && Node->GetClass() ? Node->GetClass()->GetPathName() : FString());
    SupportObject->SetStringField(TEXT("runtime_struct"), RuntimeAnimNodeProperty && RuntimeAnimNodeProperty->Struct ? RuntimeAnimNodeProperty->Struct->GetPathName() : FString());
    SupportObject->SetStringField(TEXT("support_class"), SupportClass);
    SupportObject->SetBoolField(TEXT("pose_watch_supported"), bPoseWatchSupported);
    SupportObject->SetStringField(TEXT("recommended_mode"), RecommendedMode);
    SupportObject->SetStringField(TEXT("recommended_input_pose_mode"), RecommendedInputPoseMode);
    SupportObject->SetArrayField(TEXT("known_good_field_paths"), StringArrayToJsonValues(FieldPaths));
    SupportObject->SetStringField(TEXT("unsupported_reason"), UnsupportedReason);
    SupportObject->SetStringField(TEXT("evidence_tier"), EvidenceTier);
    SupportObject->SetNumberField(TEXT("valid_runtime_pose_link_count"), ValidPoseLinkCount);
    SupportObject->SetBoolField(TEXT("runtime_pose_links_checked"), bRuntimePoseLinksChecked);
    SupportObject->SetBoolField(TEXT("runtime_node_instance_mapped"), bRuntimeNodeMapped);
    SupportObject->SetBoolField(TEXT("find_debug_anim_node_mapped"), bFindDebugMapped);
    SupportObject->SetBoolField(TEXT("same_anim_instance_node_mapping"), bNodeMapped);
    return SupportObject;
}
TSharedPtr<FJsonObject> TransformDeltaToJson(
    const FAnimNodePoseTransformSample& Before,
    const FAnimNodePoseTransformSample& After)
{
    TSharedPtr<FJsonObject> DeltaObject = MakeShared<FJsonObject>();
    if (!Before.bValid || !After.bValid)
    {
        DeltaObject->SetBoolField(TEXT("valid"), false);
        if (!Before.Error.IsEmpty())
        {
            DeltaObject->SetStringField(TEXT("pre_error"), Before.Error);
        }
        if (!After.Error.IsEmpty())
        {
            DeltaObject->SetStringField(TEXT("post_error"), After.Error);
        }
        return DeltaObject;
    }

    const FVector WorldDelta = After.WorldTransform.GetLocation() - Before.WorldTransform.GetLocation();
    const FVector ComponentDelta = After.ComponentTransform.GetLocation() - Before.ComponentTransform.GetLocation();
    const double WorldRotationDeltaDegrees = FMath::RadiansToDegrees(Before.WorldTransform.GetRotation().AngularDistance(After.WorldTransform.GetRotation()));
    const double ComponentRotationDeltaDegrees = FMath::RadiansToDegrees(Before.ComponentTransform.GetRotation().AngularDistance(After.ComponentTransform.GetRotation()));

    DeltaObject->SetBoolField(TEXT("valid"), true);
    DeltaObject->SetArrayField(TEXT("world_translation_delta"), VectorToJsonArray(WorldDelta));
    DeltaObject->SetNumberField(TEXT("world_translation_distance"), WorldDelta.Size());
    DeltaObject->SetNumberField(TEXT("world_rotation_angle_degrees"), WorldRotationDeltaDegrees);
    DeltaObject->SetArrayField(TEXT("component_translation_delta"), VectorToJsonArray(ComponentDelta));
    DeltaObject->SetNumberField(TEXT("component_translation_distance"), ComponentDelta.Size());
    DeltaObject->SetNumberField(TEXT("component_rotation_angle_degrees"), ComponentRotationDeltaDegrees);
    return DeltaObject;
}
TSharedPtr<FJsonObject> BuildAnimNodePoseDeltas(
    const FAnimNodePoseCapture& Before,
    const FAnimNodePoseCapture& After,
    const TArray<FString>& SampleBones,
    const TArray<FString>& SampleSockets)
{
    TSharedPtr<FJsonObject> DeltaObject = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> BoneDeltasObject = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> SocketDeltasObject = MakeShared<FJsonObject>();

    for (const FString& BoneName : SampleBones)
    {
        if (BoneName.IsEmpty())
        {
            continue;
        }

        const FAnimNodePoseTransformSample* BeforeSample = Before.BoneSamples.Find(BoneName);
        const FAnimNodePoseTransformSample* AfterSample = After.BoneSamples.Find(BoneName);
        if (BeforeSample && AfterSample)
        {
            BoneDeltasObject->SetObjectField(BoneName, TransformDeltaToJson(*BeforeSample, *AfterSample));
        }
    }

    for (const FString& SocketName : SampleSockets)
    {
        if (SocketName.IsEmpty())
        {
            continue;
        }

        const FAnimNodePoseTransformSample* BeforeSample = Before.SocketSamples.Find(SocketName);
        const FAnimNodePoseTransformSample* AfterSample = After.SocketSamples.Find(SocketName);
        if (BeforeSample && AfterSample)
        {
            SocketDeltasObject->SetObjectField(SocketName, TransformDeltaToJson(*BeforeSample, *AfterSample));
        }
    }

    DeltaObject->SetObjectField(TEXT("bone_deltas"), BoneDeltasObject);
    DeltaObject->SetObjectField(TEXT("socket_deltas"), SocketDeltasObject);
    return DeltaObject;
}
TSharedPtr<FJsonObject> RuntimePoseLinkSelectionToJson(const FRuntimePoseLinkSelection& Selection)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("index"), Selection.Index);
    Object->SetNumberField(TEXT("link_id"), Selection.LinkID);
    Object->SetStringField(TEXT("field_path"), Selection.FieldPath);
    Object->SetObjectField(TEXT("runtime_pose_link"), Selection.LinkObject);
    return Object;
}
TArray<FRuntimePoseLinkSelection> GetValidRuntimePoseLinkSelections(const TSharedPtr<FJsonObject>& RuntimePoseLinksObject)
{
    TArray<FRuntimePoseLinkSelection> Selections;

    const TArray<TSharedPtr<FJsonValue>>* RuntimePoseLinkValues = nullptr;
    if (!RuntimePoseLinksObject.IsValid() || !RuntimePoseLinksObject->TryGetArrayField(TEXT("pose_links"), RuntimePoseLinkValues) || !RuntimePoseLinkValues)
    {
        return Selections;
    }

    int32 ValidIndex = 0;
    for (const TSharedPtr<FJsonValue>& LinkValue : *RuntimePoseLinkValues)
    {
        if (!LinkValue.IsValid() || LinkValue->Type != EJson::Object)
        {
            continue;
        }

        const TSharedPtr<FJsonObject> LinkObject = LinkValue->AsObject();
        if (!LinkObject.IsValid())
        {
            continue;
        }

        double LinkIDNumber = INDEX_NONE;
        const bool bValid = GetJsonBoolFieldOrDefault(LinkObject, TEXT("valid"), false);
        if (bValid && LinkObject->TryGetNumberField(TEXT("link_id"), LinkIDNumber) && static_cast<int32>(LinkIDNumber) != INDEX_NONE)
        {
            FRuntimePoseLinkSelection Selection;
            Selection.Index = ValidIndex++;
            Selection.LinkID = static_cast<int32>(LinkIDNumber);
            Selection.FieldPath = GetJsonStringFieldOrDefault(LinkObject, TEXT("field_path"), FString());
            Selection.LinkObject = LinkObject;
            Selections.Add(Selection);
        }
    }

    return Selections;
}
bool SelectRuntimePoseLinksForPoseWatch(
    const TSharedPtr<FJsonObject>& Params,
    const TSharedPtr<FJsonObject>& RuntimePoseLinksObject,
    TArray<FRuntimePoseLinkSelection>& OutSelections,
    FString& OutMode,
    TArray<TSharedPtr<FJsonValue>>& ErrorValues)
{
    OutSelections.Reset();
    OutMode = GetStringParam(Params, TEXT("input_pose_mode"), TEXT("first"));
    OutMode.TrimStartAndEndInline();
    if (OutMode.IsEmpty())
    {
        OutMode = TEXT("first");
    }

    const TArray<FRuntimePoseLinkSelection> ValidSelections = GetValidRuntimePoseLinkSelections(RuntimePoseLinksObject);
    if (ValidSelections.Num() == 0)
    {
        ErrorValues.Add(MakeShared<FJsonValueString>(TEXT("No runtime FPoseLink/FComponentSpacePoseLink input link was found on the selected node; cannot capture pre-node input pose.")));
        return false;
    }

    FString RequestedFieldPath = GetStringParam(Params, TEXT("input_pose_field_path"));
    RequestedFieldPath.TrimStartAndEndInline();
    const bool bHasRequestedFieldPath = !RequestedFieldPath.IsEmpty();

    double RequestedIndexNumber = -1.0;
    const bool bHasRequestedIndex = Params.IsValid() && Params->TryGetNumberField(TEXT("input_pose_index"), RequestedIndexNumber);
    if(bHasRequestedIndex&&(!FMath::IsFinite(RequestedIndexNumber)||RequestedIndexNumber<static_cast<double>(MIN_int32)+1||RequestedIndexNumber>static_cast<double>(MAX_int32)-1)){ErrorValues.Add(MakeShared<FJsonValueString>(TEXT("input_pose_index is outside safe int32 selection range")));return false;}
    const int32 RequestedIndex = bHasRequestedIndex?FMath::RoundToInt(RequestedIndexNumber):INDEX_NONE;

    if (bHasRequestedFieldPath)
    {
        OutMode = TEXT("field_path");
        for (const FRuntimePoseLinkSelection& Selection : ValidSelections)
        {
            if (Selection.FieldPath.Equals(RequestedFieldPath, ESearchCase::IgnoreCase))
            {
                OutSelections.Add(Selection);
                return true;
            }
        }

        ErrorValues.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("input_pose_field_path did not match a valid runtime pose link: %s"), *RequestedFieldPath)));
        return false;
    }

    if (bHasRequestedIndex)
    {
        OutMode = TEXT("index");
        if (!ValidSelections.IsValidIndex(RequestedIndex))
        {
            ErrorValues.Add(MakeShared<FJsonValueString>(FString::Printf(
                TEXT("input_pose_index %d is out of range for %d valid runtime pose links"),
                RequestedIndex,
                ValidSelections.Num())));
            return false;
        }

        OutSelections.Add(ValidSelections[RequestedIndex]);
        return true;
    }

    if (OutMode.Equals(TEXT("all"), ESearchCase::IgnoreCase))
    {
        OutSelections = ValidSelections;
        return OutSelections.Num() > 0;
    }

    if (!OutMode.Equals(TEXT("first"), ESearchCase::IgnoreCase)
        && !OutMode.Equals(TEXT("preferred"), ESearchCase::IgnoreCase)
        && !OutMode.Equals(TEXT("default"), ESearchCase::IgnoreCase))
    {
        ErrorValues.Add(MakeShared<FJsonValueString>(FString::Printf(
            TEXT("Unsupported input_pose_mode '%s'. Use first, all, input_pose_field_path, or input_pose_index."),
            *OutMode)));
        return false;
    }

    OutMode = TEXT("first");
    OutSelections.Add(ValidSelections[0]);
    return true;
}
FString SanitizeAssetNameSegmentForAnimNodeProbe(const FString& RawName)
{
    FString Result = RawName;
    for (TCHAR& Character : Result)
    {
        if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
        {
            Character = TEXT('_');
        }
    }
    Result.TrimStartAndEndInline();
    return Result.IsEmpty() ? TEXT("AnimNodeProbe") : Result;
}
FString NormalizeTempRootForAnimNodeProbe(const FString& RawTempRoot)
{
    FString TempRoot = RawTempRoot.IsEmpty() ? TEXT("/Game/_MCP_Temp/AnimNodePrePost") : RawTempRoot;
    TempRoot.ReplaceInline(TEXT("\\"), TEXT("/"));
    TempRoot.TrimEndInline();
    while (TempRoot.EndsWith(TEXT("/")))
    {
        TempRoot.LeftChopInline(1);
    }
    return TempRoot.IsEmpty() ? TEXT("/Game/_MCP_Temp/AnimNodePrePost") : TempRoot;
}
bool IsAllowedAnimNodeProbeTempRoot(const FString& TempRoot)
{
    return TempRoot.Equals(TEXT("/Game/_MCP_Temp"), ESearchCase::IgnoreCase)
        || TempRoot.StartsWith(TEXT("/Game/_MCP_Temp/"), ESearchCase::IgnoreCase);
}
bool Graphs(UBlueprint* Blueprint,TArray<UEdGraph*>& Out,FWork& Work)
{
    if(!Work.Basic()||!Live(Blueprint)) { return false; }
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
    if(!Work.Basic()||!Owner.Current()||Out.Num()>4096) { return Work.Fail(TEXT("graph_changed"),TEXT("Blueprint changed or graph enumeration exceeded4096 after SDK callback")); }
    for(const auto& Id:HeldGraphs) { if(!Id->Current()) { return Work.Fail(TEXT("graph_changed"),TEXT("Graph actual package/owner/class changed during SDK enumeration")); } }
    for(UEdGraph* Graph:Out) { if(!Live(Graph)||!Live(Graph->GetPackage())) { return Work.Fail(TEXT("graph_unreadable"),TEXT("SDK returned an unreadable graph")); } }return true;
}
bool GraphSelector(UEdGraph* G,UBlueprint* B,const FString& Id,const FString& Name,const FString& Type)
{
    if(!G){return false;}if(!Id.IsEmpty()){FGuid Parsed;if(FGuid::Parse(Id,Parsed)){if(G->GraphGuid!=Parsed){return false;}}else if(G->GraphGuid.ToString()!=Id){return false;}}
    return (Name.IsEmpty()||G->GetName().Equals(Name,ESearchCase::IgnoreCase))&&(Type.IsEmpty()||Type==TEXT("any")||GraphTypeName(B,G)==Type);
}
UEdGraph* ResolveGraph(UBlueprint* B,const TSharedPtr<FJsonObject>& P,FWork& W)
{
    const FString Id=GetStringParam(P,TEXT("graph_id")),Name=GetStringParam(P,TEXT("graph_name")),Type=NormalizeGraphType(GetStringParam(P,TEXT("graph_type")));TArray<UEdGraph*> All;if(!Graphs(B,All,W)){return nullptr;}
    if(Id.IsEmpty()&&Name.IsEmpty()&&Type.IsEmpty())
    {
        for(UEdGraph* G:B->UbergraphPages){if(Live(G)){return G;}}
        // The original resolver creates an EventGraph even with bCreateIfMissing=false. Preserve that path explicitly.
        if(!W.Idle(TEXT("resolver_create_event_graph"))){return nullptr;}W.Mutation(TEXT("resolver_create_event_graph_calls"));UEdGraph* G=FBlueprintEditorUtils::CreateNewGraph(B,TEXT("EventGraph"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());if(!W.Basic()||!Live(G)||!W.Hold(G)){return nullptr;}W.Changed();W.Mutation(TEXT("resolver_add_ubergraph_page_calls"));FBlueprintEditorUtils::AddUbergraphPage(B,G);if(!W.Basic()){return nullptr;}return G;
    }
    for(UEdGraph* G:All){if(!W.Basic()){return nullptr;}if(GraphSelector(G,B,Id,Name,Type)){return G;}}
    W.Fail(TEXT("graph_not_found"),FString::Printf(TEXT("Blueprint graph not found. graph_name='%s', graph_id='%s', graph_type='%s'"),*Name,*Id,*Type));return nullptr;
}
struct FAnimInstanceRuntimeTarget
{UWorld* World=nullptr;AActor* Actor=nullptr;USkeletalMeshComponent* Component=nullptr;UAnimInstance* AnimInstance=nullptr;FString AnimInstanceSource;};
bool RuntimeParticipants(FWork& W,bool Initialized)
{
    if(!W.Current()||!W.Component){return false;}auto* C=W.Component;
    const auto Linked=static_cast<const USkeletalMeshComponent*>(C)->GetLinkedAnimInstances();if(Linked.Num()>4096){return W.Fail(TEXT("linked_instance_bound"),TEXT("Linked AnimInstance inventory exceeds4096"));}
    TArray<UAnimInstance*> Participants=Linked;Participants.AddUnique(C->GetAnimInstance());Participants.AddUnique(C->GetPostProcessInstance());
    for(UAnimInstance* A:Participants)
    {if(!A){continue;}if(!W.Hold(A)||!W.Current()||A->GetOuter()!=C||A->IsRunningParallelEvaluation()||(Initialized&&!A->IsInitialized())){return W.Fail(TEXT("runtime_not_ready"),TEXT("Animation participant is uninitialized, parallel, foreign or unavailable; no wait/initialize is performed"));}}
    return W.Current()&&static_cast<const USkeletalMeshComponent*>(C)->GetLinkedAnimInstances()==Linked;
}
bool FindAnimInstanceRuntimeTarget(const TSharedPtr<FJsonObject>& P,bool Prefer,bool Require,const FString&,FAnimInstanceRuntimeTarget& T,TArray<TSharedPtr<FJsonValue>>&,FString& Error)
{
    auto& W=*CurrentWork;W.Result.Stage=TEXT("resolve_runtime_target");
    if(!Sampling::Worlds(W,Prefer,Require)||!Sampling::FindTarget(W,GetStringParam(P,TEXT("actor_label")),GetStringParam(P,TEXT("actor_name")),GetStringParam(P,TEXT("actor_path")),GetStringParam(P,TEXT("component_name")),Require)||!Sampling::PreparePoseSources(W)||!W.Current()){Error=W.Result.Error;return false;}
    FString Source=GetStringParam(P,TEXT("anim_instance_source"),TEXT("main")).TrimStartAndEnd().ToLower();if(Source.IsEmpty()){Source=TEXT("main");}
    const bool Post=Source==TEXT("post_process")||Source==TEXT("postprocess")||Source==TEXT("post_process_anim_bp")||Source==TEXT("postprocess_anim_bp");
    if(!Post&&Source!=TEXT("main")&&Source!=TEXT("primary")&&Source!=TEXT("anim_instance")){Error=TEXT("Unknown anim_instance_source: ")+Source;return false;}
    W.SDK(TEXT("anim_instance_selection_queries"));UAnimInstance* A=Post?W.Component->GetPostProcessInstance():W.Component->GetAnimInstance();
    if(!W.Current()||!Live(A)||!W.Hold(A)||A->GetOuter()!=W.Component||!W.Hold(A->GetClass())||A->IsRunningParallelEvaluation()){Error=TEXT("Selected AnimInstance is unavailable or parallel");return false;}
    W.RuntimeAnim=A;W.RuntimePost=Post;W.RuntimeClass=A->GetClass();W.RuntimeProperties.Reset();
    if(auto* AC=Cast<UAnimBlueprintGeneratedClass>(A->GetClass())){W.RuntimeProperties=AC->GetAnimNodeProperties();if(W.RuntimeProperties.Num()>65536){Error=TEXT("Anim node property inventory exceeds65536");return false;}}
    T.World=W.World;T.Actor=W.Actor;T.Component=W.Component;T.AnimInstance=A;T.AnimInstanceSource=Post?TEXT("post_process"):TEXT("main");
    if(!W.Current()){Error=W.Result.Error;return false;}return true;
}
TSharedPtr<FJsonObject> AnimInstanceToRuntimeJson(UAnimInstance* A)
{auto J=MakeShared<FJsonObject>();if(!A||!Current()||!CurrentWork->Hold(A)){return J;}J->SetStringField(TEXT("name"),A->GetName());J->SetStringField(TEXT("path"),A->GetPathName());J->SetStringField(TEXT("class"),A->GetClass()->GetPathName());J->SetStringField(TEXT("outer"),GetPathNameSafe(A->GetOuter()));J->SetBoolField(TEXT("initialized"),A->IsInitialized());return J;}
void PopulateAnimInstanceRuntimeTargetFields(const TSharedPtr<FJsonObject>& J,const FAnimInstanceRuntimeTarget& T,bool Prefer,bool Require)
{
    auto& W=*CurrentWork;if(!Current()){return;}Sampling::Metadata(W,GetStringParam(W.Params,TEXT("component_name")));
    for(const TCHAR* Key:{TEXT("actor"),TEXT("component"),TEXT("sampled_world_type"),TEXT("sampled_world_name"),TEXT("matched_actor_count"),TEXT("matched_component_count")}){if(W.Legacy->HasField(Key)){J->SetField(Key,W.Legacy->TryGetField(Key));}}
    J->SetStringField(TEXT("requested_actor_label"),GetStringParam(W.Params,TEXT("actor_label")));J->SetStringField(TEXT("requested_actor_name"),GetStringParam(W.Params,TEXT("actor_name")));J->SetStringField(TEXT("requested_actor_path"),GetStringParam(W.Params,TEXT("actor_path")));J->SetStringField(TEXT("requested_component_name"),GetStringParam(W.Params,TEXT("component_name")));FString RequestedSource=GetStringParam(W.Params,TEXT("anim_instance_source"),TEXT("main")).TrimStartAndEnd();if(RequestedSource.IsEmpty()){RequestedSource=TEXT("main");}J->SetStringField(TEXT("requested_anim_instance_source"),RequestedSource);J->SetBoolField(TEXT("is_play_session_active"),IsPlaySessionActive());
    J->SetObjectField(TEXT("anim_instance"),AnimInstanceToRuntimeJson(T.AnimInstance));J->SetStringField(TEXT("anim_instance_source"),T.AnimInstanceSource);J->SetBoolField(TEXT("prefer_pie_world"),Prefer);J->SetBoolField(TEXT("require_pie_world"),Require);
}
TSharedPtr<FJsonObject> TickSkeletalComponentForAnimRuntimeProbe(USkeletalMeshComponent* C,int32 Count,double Delta,bool Refresh,TArray<TSharedPtr<FJsonValue>>&)
{
    auto J=MakeShared<FJsonObject>();auto& W=*CurrentWork;J->SetNumberField(TEXT("requested_tick_count"),Count);J->SetNumberField(TEXT("tick_delta_time"),Delta);J->SetBoolField(TEXT("refresh_bone_transforms"),Refresh);J->SetNumberField(TEXT("executed_tick_count"),0);J->SetNumberField(TEXT("executed_refresh_count"),0);J->SetBoolField(TEXT("success"),false);
    J->SetStringField(TEXT("execution_count_scope"),TEXT("Returned explicit synchronous public calls, not internal evaluation/initialization completion; an SDK call cannot be preempted"));
    if(C!=W.Component||Count<0||Count>240||!FMath::IsFinite(Delta)||!RuntimeParticipants(W,Count>0)){return J;}
    for(int32 I=0;I<Count;++I)
    {
        if(!RuntimeParticipants(W,true)){return J;}W.Result.Stage=TEXT("tick_runtime_component");W.Mutation(TEXT("explicit_tick_calls"));C->TickAnimation(static_cast<float>(Delta),false);W.Count(TEXT("returned_tick_calls"));W.Changed();J->SetNumberField(TEXT("executed_tick_count"),I+1);if(!W.Current()){return J;}
        if(Refresh){if(!RuntimeParticipants(W,true)){return J;}W.Mutation(TEXT("explicit_refresh_calls"));C->RefreshBoneTransforms();W.Count(TEXT("returned_refresh_calls"));W.Changed();J->SetNumberField(TEXT("executed_refresh_count"),I+1);if(!W.Current()){return J;}}
    }J->SetBoolField(TEXT("success"),W.Current());return J;
}
FAnimNodePoseCapture CaptureAnimNodeProbePose(const USkeletalMeshComponent* C,const TArray<FString>& Bones,const TArray<FString>& Sockets,bool Allow,TArray<TSharedPtr<FJsonValue>>& Errors,TArray<TSharedPtr<FJsonValue>>& Warnings)
{
    FAnimNodePoseCapture Out;auto& W=*CurrentWork;auto BJ=MakeShared<FJsonObject>(),SJ=MakeShared<FJsonObject>();Out.Json->SetObjectField(TEXT("bone_samples"),BJ);Out.Json->SetObjectField(TEXT("socket_samples"),SJ);
    if(C!=W.Component||!W.Current()){return Out;}
    for(bool Socket:{false,true}){for(const FString& Name:Socket?Sockets:Bones)
    {
        if(Name.IsEmpty()){continue;}if(!SafeText(Name,NAME_SIZE-1)||!W.Current()){W.Fail(TEXT("sample_name_bound"),TEXT("Decoded sample name is invalid for FName"));return Out;}
        if(!Sampling::SampleName(W,Name,Socket)||!W.Current()){return Out;}const auto Row=(Socket?W.SocketSamples:W.BoneSamples)->GetObjectField(Name);(Socket?SJ:BJ)->SetObjectField(Name,Row);
        FAnimNodePoseTransformSample Sample;Sample.bValid=Row->GetBoolField(TEXT("valid"));if(Sample.bValid)
        {const FName Key(*Name);W.SDK(TEXT("socket_transform_calls"));Sample.WorldTransform=C->GetSocketTransform(Key,RTS_World);if(!W.Current()){return Out;}W.SDK(TEXT("socket_transform_calls"));Sample.ComponentTransform=C->GetSocketTransform(Key,RTS_Component);if(!W.Current()||!FiniteTransform(Sample.WorldTransform)||!FiniteTransform(Sample.ComponentTransform)){W.Fail(TEXT("sample_nonfinite"),TEXT("Sample transform is invalid"));return Out;}}
        else{Sample.Error=GetStringParam(Row,TEXT("error"));(Allow?Warnings:Errors).Add(MakeShared<FJsonValueString>(Sample.Error));}(Socket?Out.SocketSamples:Out.BoneSamples).Add(Name,Sample);
    }}return Out;
}
bool RuntimeProperty(UAnimBlueprintGeneratedClass* AC,FStructProperty* P,UAnimInstance* A)
{
    if(!Current()||!Live(AC)||!CurrentWork->Hold(AC)||!P||!A||AC!=A->GetClass()||!AC->GetAnimNodeProperties().Contains(P)){return false;}
    FFieldId Id(P);return Id.Current()&&Live(P->Struct)&&P->Struct->IsChildOf(FAnimNode_Base::StaticStruct())&&P->ArrayDim==1&&Layout(P,AC->GetPropertiesSize())&&CurrentWork->Hold(P->Struct);
}
TSharedPtr<FJsonObject> AnimGraphCompiledNodeMappingToJson(UBlueprint* B,UAnimGraphNode_Base* N,UAnimInstance* A,USkeletalMeshComponent* C,bool* Runtime,bool* Debug)
{
    if(Runtime){*Runtime=false;}if(Debug){*Debug=false;}auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("valid"),false);J->SetStringField(TEXT("mapping_kind"),TEXT("editor_guid_to_compiled_anim_node_property"));
    if(!Current()||!N){return J;}auto* AC=Cast<UAnimBlueprintGeneratedClass>(A?A->GetClass():(B?B->GeneratedClass.Get():nullptr));
    J->SetStringField(TEXT("node_guid"),N->NodeGuid.ToString(EGuidFormats::Digits));J->SetStringField(TEXT("node_object_name"),N->GetName());J->SetStringField(TEXT("node_class"),N->GetClass()->GetPathName());J->SetStringField(TEXT("blueprint_path"),GetPathNameSafe(B));J->SetStringField(TEXT("blueprint_generated_class"),GetPathNameSafe(B?B->GeneratedClass:nullptr));J->SetBoolField(TEXT("runtime_instance_requested"),A!=nullptr);J->SetStringField(TEXT("runtime_instance_class"),GetPathNameSafe(A?A->GetClass():nullptr));
    const bool Compatible=!A||(B&&B->GeneratedClass&&A->IsA(B->GeneratedClass));J->SetBoolField(TEXT("runtime_instance_compatible"),Compatible);J->SetBoolField(TEXT("has_anim_blueprint_generated_class"),AC!=nullptr);J->SetStringField(TEXT("anim_class"),GetPathNameSafe(AC));
    if(!Live(AC)||!CurrentWork->Hold(AC)){return J;}const auto Properties=AC->GetAnimNodeProperties();if(Properties.Num()>65536){Unsafe(TEXT("Compiled property inventory exceeds65536"));return J;}
    CurrentWork->SDK(TEXT("guid_to_property_index_queries"));const int32 Index=AC->GetNodeIndexFromGuid(N->NodeGuid,EPropertySearchMode::Hierarchy);if(!Current()||Properties!=AC->GetAnimNodeProperties()){return J;}FStructProperty* P=Properties.IsValidIndex(Index)?Properties[Index]:nullptr;
    J->SetNumberField(TEXT("node_index"),Index);J->SetNumberField(TEXT("anim_node_property_count"),Properties.Num());J->SetBoolField(TEXT("node_index_found"),P!=nullptr);bool Valid=false;
    if(P){FFieldId Id(P);Valid=Id.Current()&&Live(P->Struct)&&P->Struct->IsChildOf(FAnimNode_Base::StaticStruct())&&P->ArrayDim==1&&Layout(P,AC->GetPropertiesSize());if(Valid){J->SetStringField(TEXT("anim_node_property_name"),P->GetName());J->SetStringField(TEXT("anim_node_property_path"),P->GetPathName());J->SetStringField(TEXT("anim_node_property_cpp_type"),P->GetCPPType(nullptr, 0));J->SetStringField(TEXT("anim_node_property_struct"),P->Struct->GetPathName());J->SetNumberField(TEXT("property_offset"),P->GetOffset_ForInternal());}}
    FAnimNode_Base* R=A&&Compatible&&Valid?P->ContainerPtrToValuePtr<FAnimNode_Base>(A):nullptr;FAnimNode_Base* D=nullptr;
    if(C&&RuntimeParticipants(*CurrentWork,false)&&Current()){CurrentWork->SDK(TEXT("find_debug_anim_node_calls"));D=N->FindDebugAnimNode(C);if(!Current()||AC->GetAnimNodeProperties()!=Properties){return J;}}
    J->SetBoolField(TEXT("anim_node_property_valid"),Valid);J->SetBoolField(TEXT("runtime_node_instance_mapped"),R!=nullptr);J->SetStringField(TEXT("runtime_node_pointer"),PointerText(R));J->SetBoolField(TEXT("find_debug_anim_node_mapped"),D!=nullptr);J->SetStringField(TEXT("find_debug_anim_node_pointer"),PointerText(D));J->SetBoolField(TEXT("pointer_match"),R&&D&&R==D);J->SetBoolField(TEXT("valid"),Valid&&(!A||R||D));J->SetStringField(TEXT("index_semantics"),TEXT("GetNodeIndexFromGuid already returns remapped property-array index; no proxy runtime-index reversal"));if(Runtime){*Runtime=R!=nullptr;}if(Debug){*Debug=D!=nullptr;}return J;
}
TSharedPtr<FJsonObject> RuntimePoseLinkToJson(const FString& Path,const FStructProperty* P,const void* Value,UAnimBlueprintGeneratedClass* AC,UAnimInstance* A)
{
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("valid"),false);J->SetStringField(TEXT("field_path"),Path);if(!Current()||!P||!Value){return J;}FFieldId Id(const_cast<FStructProperty*>(P));if(!Id.Current()||P->GetElementSize()<sizeof(FPoseLinkBase)){return J;}
    const auto* Link=static_cast<const FPoseLinkBase*>(Value);const int32 Index=Link->LinkID,Source=Link->SourceLinkID;
    const auto Properties=AC->GetAnimNodeProperties();FStructProperty* LP=Properties.IsValidIndex(Index)?Properties[Index]:nullptr;FAnimNode_Base* R=RuntimeProperty(AC,LP,A)?LP->ContainerPtrToValuePtr<FAnimNode_Base>(A):nullptr;
    auto* Mutable=const_cast<FPoseLinkBase*>(Link);CurrentWork->SDK(TEXT("pose_link_cached_pointer_queries"));auto* Cached=Mutable->GetLinkNode();if(!Current()||!Id.Current()||AC->GetAnimNodeProperties()!=Properties){return J;}
    J->SetBoolField(TEXT("valid"),true);J->SetStringField(TEXT("field_name"),P->GetName());J->SetStringField(TEXT("pose_link_struct"),P->Struct->GetPathName());J->SetNumberField(TEXT("link_id"),Index);J->SetNumberField(TEXT("source_link_id"),Source);J->SetBoolField(TEXT("link_id_property_valid"),R!=nullptr);J->SetStringField(TEXT("linked_property_name"),R?LP->GetName():FString());J->SetStringField(TEXT("linked_property_path"),R?LP->GetPathName():FString());J->SetStringField(TEXT("linked_property_struct"),R?LP->Struct->GetPathName():FString());J->SetBoolField(TEXT("has_cached_linked_node"),Cached!=nullptr);J->SetStringField(TEXT("cached_linked_node_pointer"),PointerText(Cached));J->SetBoolField(TEXT("link_id_node_mapped"),R!=nullptr);J->SetStringField(TEXT("link_id_node_pointer"),PointerText(R));J->SetBoolField(TEXT("linked_pointer_match"),Cached&&R&&Cached==R);
    for(bool Up:{false,true}){const int32 NodeIndex=Up?Source:Index;UEdGraphNode* Visual=nullptr;if(NodeIndex>=0&&Properties.IsValidIndex(NodeIndex)){CurrentWork->SDK(TEXT("property_index_to_visual_node_queries"));Visual=const_cast<UEdGraphNode*>(AC->GetVisualNodeFromNodePropertyIndex(NodeIndex,EPropertySearchMode::Hierarchy));if(!Current()){return J;}}J->SetObjectField(Up?TEXT("source_visual_node"):TEXT("linked_visual_node"),Visual&&CurrentWork->Nodes.Contains(Visual)?NodeToJson(Visual,false):MakeShared<FJsonObject>());}return J;
}
void CollectRuntimePoseLinksFromStruct(const UScriptStruct* Type,const void* Value,const FString& Path,UAnimBlueprintGeneratedClass* AC,UAnimInstance* A,TArray<TSharedPtr<FJsonValue>>& Out,int32 Depth,int32 Limit)
{
    if(!Current()||!Live(Type)||!Value||Depth>Limit||!CurrentWork->Hold(const_cast<UScriptStruct*>(Type))){return;}
    TArray<TUniquePtr<FFieldId>> Fields;for(TFieldIterator<FProperty> It(Type,EFieldIterationFlags::IncludeSuper);It;++It){if(Fields.Num()>=MaxFields){Unsafe(TEXT("Pose-link field inventory exceeds65536"));return;}Fields.Add(MakeUnique<FFieldId>(*It));}
    for(const auto& Id:Fields)
    {
        if(!Current()||!Id->Current()||!Layout(Id->Property,Type->GetStructureSize())||++CurrentWork->PropertyReads>MaxFields){Unsafe(TEXT("Pose-link descriptor/layout/budget changed"));return;}
        FProperty* P=Id->Property;const FString Child=Path.IsEmpty()?P->GetName():Path+TEXT(".")+P->GetName();const void* V=P->ContainerPtrToValuePtr<void>(Value);
        if(auto* S=CastField<FStructProperty>(P)){if(!Live(S->Struct)){continue;}if(S->Struct->IsChildOf(FPoseLinkBase::StaticStruct())){Out.Add(MakeShared<FJsonValueObject>(RuntimePoseLinkToJson(Child,S,V,AC,A)));}else{CollectRuntimePoseLinksFromStruct(S->Struct,V,Child,AC,A,Out,Depth+1,Limit);}}
        else if(auto* Array=CastField<FArrayProperty>(P))
        {
            auto* Inner=CastField<FStructProperty>(Array->Inner);if(!Inner||!Live(Inner->Struct)){continue;}FFieldId InnerId(Inner);const auto ExpectedArrayFlags=Array->ArrayFlags;FScriptArrayHelper First(Array,V);const int32 Num=First.Num();const void* Storage=Num?First.GetRawPtr(0):nullptr;if(Num<0||Num>65536){Unsafe(TEXT("Pose-link array exceeds65536"));return;}
            if(CurrentWork->ValueGuards.Num()>=64){Unsafe(TEXT("Pose-link storage guard nesting exceeds64"));return;}CurrentWork->ValueGuards.Add([&](){if(!Id->Current()||!InnerId.Current()||Array->ArrayFlags!=ExpectedArrayFlags){return false;}FScriptArrayHelper Fresh(Array,V);return Fresh.Num()==Num&&(!Num||Fresh.GetRawPtr(0)==Storage);});ON_SCOPE_EXIT{CurrentWork->ValueGuards.Pop();};
            for(int32 I=0;I<Num;++I){if(!Current()||!Id->Current()||!InnerId.Current()||Array->ArrayFlags!=ExpectedArrayFlags){return;}FScriptArrayHelper Fresh(Array,V);if(Fresh.Num()!=Num||(Num&&Fresh.GetRawPtr(0)!=Storage)){Unsafe(TEXT("Pose-link array storage changed"));return;}const FString Element=FString::Printf(TEXT("%s[%d]"),*Child,I);if(Inner->Struct->IsChildOf(FPoseLinkBase::StaticStruct())){Out.Add(MakeShared<FJsonValueObject>(RuntimePoseLinkToJson(Element,Inner,Fresh.GetRawPtr(I),AC,A)));}else{CollectRuntimePoseLinksFromStruct(Inner->Struct,Fresh.GetRawPtr(I),Element,AC,A,Out,Depth+1,Limit);}}
        }
    }
}
TSharedPtr<FJsonObject> RuntimeAnimNodePoseLinksToJson(UAnimBlueprintGeneratedClass* AC,FStructProperty* P,UAnimInstance* A,int32 Depth)
{
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("valid"),false);J->SetStringField(TEXT("mapping_kind"),TEXT("runtime_anim_node_pose_links"));TArray<TSharedPtr<FJsonValue>> Rows;
    if(RuntimeProperty(AC,P,A)){CollectRuntimePoseLinksFromStruct(P->Struct,P->ContainerPtrToValuePtr<void>(A),FString(),AC,A,Rows,0,Depth);if(Current()){J->SetBoolField(TEXT("valid"),true);J->SetStringField(TEXT("anim_class"),AC->GetPathName());J->SetStringField(TEXT("anim_node_property_name"),P->GetName());J->SetStringField(TEXT("anim_node_property_struct"),P->Struct->GetPathName());}}
    J->SetNumberField(TEXT("max_depth"),Depth);J->SetNumberField(TEXT("pose_link_count"),Rows.Num());J->SetArrayField(TEXT("pose_links"),Rows);return J;
}
struct FWatchSession
{
    UAnimBlueprint* Blueprint=nullptr;UClass* Generated=nullptr;FAnimBlueprintDebugData* Data=nullptr;TUniquePtr<FIdentity> Identity;
    TStrongObjectPtr<UObject> Previous;UAnimInstance* Selected=nullptr;bool ChangedDebug=false,Closed=false,Clean=true;
    TArray<FTransientAnimPoseWatchRegistration> Registrations;UClass* ExpectedParent=nullptr;TArray<TPair<UClass*,UClass*>> ParentChain;
    bool Stable() const
    {if(!Identity||!Identity->Current()||Blueprint->GeneratedClass!=Generated||Blueprint->ParentClass!=ExpectedParent){return false;}for(const auto& Link:ParentChain){if(!Live(Link.Key)||Link.Key->GetSuperClass()!=Link.Value){return false;}}return Blueprint->GetDebugData()==Data;}
    bool Start(UAnimBlueprint* B,UAnimInstance* A)
    {
        if(!Current()||!Live(B)||!CurrentWork->Hold(B)||!CurrentWork->Hold(B->GeneratedClass)){return false;}Blueprint=B;Generated=B->GeneratedClass;Identity=MakeUnique<FIdentity>(B);ExpectedParent=B->ParentClass;TSet<UClass*> Seen;for(UClass* C=ExpectedParent;C;C=C->GetSuperClass()){if(!Live(C)||Seen.Contains(C)||Seen.Num()>=64||!CurrentWork->Hold(C)){return false;}Seen.Add(C);ParentChain.Add({C,C->GetSuperClass()});}Data=B->GetDebugData();Selected=A;Previous.Reset(B->GetObjectBeingDebugged());
        if(!Data||!CurrentWork->Hold(Previous.Get())){return false;}
        if(Previous.Get()!=A){CurrentWork->Mutation(TEXT("debug_object_set_calls"));B->SetObjectBeingDebugged(A);ChangedDebug=true;CurrentWork->Changed();}
        return Current()&&Stable()&&B->GetObjectBeingDebugged()==A;
    }
    FAnimNodePoseWatch* Find(int32 Index) const
    {if(!Stable()||Data->AnimNodePoseWatch.Num()>65536){return nullptr;}for(auto& Row:Data->AnimNodePoseWatch){if(Row.NodeID==Index){return &Row;}}return nullptr;}
    FTransientAnimPoseWatchRegistration Register(int32 Index,const FString& Role,TArray<TSharedPtr<FJsonValue>>& Errors)
    {
        FTransientAnimPoseWatchRegistration R;R.NodeID=Index;R.Role=Role;
        auto Bad=[&](const TCHAR* Message){Errors.Add(MakeShared<FJsonValueString>(Message));return R;};
        if(!Current()||!Stable()||Index<0||Data->AnimNodePoseWatch.Num()>=65536){return Bad(TEXT("PoseWatch registration boundary unavailable"));}
        if(auto* Existing=Find(Index))
        {R.PoseWatch=Existing->PoseWatch;R.PoseElement=Existing->PoseWatchPoseElement;R.bPreExisting=true;R.bRegistered=Live(R.PoseWatch)&&Live(R.PoseElement)&&CurrentWork->Hold(R.PoseWatch)&&CurrentWork->Hold(R.PoseElement);if(!R.bRegistered){Errors.Add(MakeShared<FJsonValueString>(TEXT("Existing pose watch has unavailable pointers")));}Registrations.Add(R);return R;}
        CurrentWork->Mutation(TEXT("transient_pose_watch_new_calls"));R.PoseWatch=NewObject<UPoseWatch>(Blueprint,NAME_None,RF_Transient);R.bCreatedTransient=R.PoseWatch!=nullptr;Registrations.Add(R);
        if(!Current()||!Stable()||!Live(R.PoseWatch)||R.PoseWatch->GetOuter()!=Blueprint||!CurrentWork->Hold(R.PoseWatch)){return Bad(TEXT("Transient pose watch creation changed owner"));}
        CurrentWork->Mutation(TEXT("pose_watch_visibility_calls"));R.PoseWatch->SetIsVisible(false);if(!Current()||!Stable()){return Bad(TEXT("PoseWatch visibility callback changed owner"));}
        CurrentWork->Mutation(TEXT("pose_watch_add_element_calls"));R.PoseElement=R.PoseWatch->AddElement<UPoseWatchPoseElement>(FText::FromString(Role),FName(TEXT("AnimGraph.PoseWatch.Icon"))).Get();Registrations.Last()=R;
        if(!Current()||!Stable()||!Live(R.PoseElement)||!CurrentWork->Hold(R.PoseElement)){return Bad(TEXT("Transient pose element unavailable"));}
        R.PoseElement->SetColor(Role.Contains(TEXT("pre"))?FColor::Cyan:FColor::Orange);if(!Current()||!Stable()){return Bad(TEXT("PoseWatch element color callback changed owner"));}R.PoseElement->SetIsVisible(false);if(!Current()||!Stable()){return Bad(TEXT("PoseWatch element visibility callback changed owner"));}
        if(Find(Index)){return Bad(TEXT("Another pose watch occupied the requested NodeID during callback"));}CurrentWork->Mutation(TEXT("debug_data_add_pose_watch_calls"));Data->AddPoseWatch(Index,R.PoseElement);CurrentWork->Changed();
        auto* Added=Find(Index);R.bRegistered=Added&&Added->PoseWatch==R.PoseWatch&&Added->PoseWatchPoseElement==R.PoseElement;Registrations.Last()=R;if(!R.bRegistered){Errors.Add(MakeShared<FJsonValueString>(TEXT("Transient PoseWatch exact registration could not be verified")));}return R;
    }
    bool Close()
    {
        if(Closed){return Clean;}Closed=true;TSet<int32> Removed;
        // Cleanup deliberately does not depend on Work.Error being empty. It still requires exact ownership.
        for(const auto& R:Registrations)
        {
            if(!R.bCreatedTransient||Removed.Contains(R.NodeID)){continue;}Removed.Add(R.NodeID);auto* Found=Find(R.NodeID);
            if(!Stable()){Clean=false;continue;}
            if(Found&&Found->PoseWatch==R.PoseWatch&&Found->PoseWatchPoseElement==R.PoseElement)
            {CurrentWork->Mutation(TEXT("debug_data_remove_pose_watch_calls"));Data->RemovePoseWatch(R.NodeID);if(!Stable()||Find(R.NodeID)){Clean=false;}}
            else if(Found){Clean=false;}
        }
        if(ChangedDebug)
        {
            if(Stable()&&Blueprint->GetObjectBeingDebugged()==Selected&&(!Previous.Get()||Live(Previous.Get())))
            {CurrentWork->Mutation(TEXT("debug_object_restore_calls"));Blueprint->SetObjectBeingDebugged(Previous.Get());Clean=Stable()&&Blueprint->GetObjectBeingDebugged()==Previous.Get()&&Clean;}
            else{Clean=false;}
        }
        CurrentWork->Result.Data->SetBoolField(TEXT("root_debug_object_restored"),Stable()&&Blueprint->GetObjectBeingDebugged()==Previous.Get());
        CurrentWork->Result.Data->SetBoolField(TEXT("linked_debug_state_restoration_verified"),false);
        CurrentWork->Result.Data->SetBoolField(TEXT("owned_pose_watch_cleanup_verified"),Clean);
        return Clean;
    }
    ~FWatchSession(){if(!Closed){Close();}}
};
TSharedPtr<FJsonObject> PoseWatchRegistrationToJson(const FTransientAnimPoseWatchRegistration& R)
{auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("role"),R.Role);J->SetNumberField(TEXT("node_id"),R.NodeID);J->SetBoolField(TEXT("registered"),R.bRegistered);J->SetBoolField(TEXT("pre_existing"),R.bPreExisting);J->SetBoolField(TEXT("created_transient"),R.bCreatedTransient);J->SetStringField(TEXT("pose_watch"),Live(R.PoseWatch)?R.PoseWatch->GetPathName():FString());J->SetStringField(TEXT("pose_element"),Live(R.PoseElement)?R.PoseElement->GetPathName():FString());return J;}
FAnimNodePoseCapture CaptureWatch(FWatchSession& Session,int32 Index,const FString& Role,UAnimInstance* Expected,const TArray<FString>& Bones,bool Allow,TArray<TSharedPtr<FJsonValue>>& Errors,TArray<TSharedPtr<FJsonValue>>& Warnings)
{
    FAnimNodePoseCapture Out;auto& W=*CurrentWork;auto J=Out.Json;J->SetStringField(TEXT("role"),Role);J->SetNumberField(TEXT("node_id"),Index);J->SetBoolField(TEXT("valid"),false);
    auto Bad=[&](const FString& Message){J->SetStringField(TEXT("error"),Message);Errors.Add(MakeShared<FJsonValueString>(Message));return Out;};
    if(!W.Current()||!Session.Stable()){return Bad(TEXT("PoseWatch owner is unavailable after tick"));}auto* Watch=Session.Find(Index);if(!Watch){return Bad(TEXT("PoseWatch capture not found after tick"));}
    if(Watch->Object!=Expected){return Bad(TEXT("PoseWatch capture came from a different AnimInstance"));}
    const auto Required=Watch->GetRequiredBones();const auto Transforms=Watch->GetBoneTransforms();const FTransform World=Watch->GetWorldTransform();
    if(Required.IsEmpty()||Transforms.IsEmpty()){return Bad(TEXT("PoseWatch did not receive transforms; link may not have evaluated"));}
    if(Required.Num()>65536||Transforms.Num()>65536||!FiniteTransform(World)){return Bad(TEXT("PoseWatch captured arrays or world transform exceed safe bounds"));}
    for(const auto& T:Transforms){if(!FiniteTransform(T)){return Bad(TEXT("PoseWatch contains nonfinite transforms"));}}
    if(!W.Current()){return Bad(TEXT("PoseWatch source changed"));}auto* Mesh=W.Component->GetSkeletalMeshAsset();if(!Live(Mesh)||!W.Hold(Mesh)){return Bad(TEXT("PoseWatch component mesh unavailable"));}
    W.SDK(TEXT("reference_skeleton_reads"));const auto& Ref=Mesh->GetRefSkeleton();if(!W.Current()){return Bad(TEXT("Reference skeleton source changed"));}const auto Info=Ref.GetRefBoneInfo();if(Info.Num()>65536){return Bad(TEXT("Reference skeleton exceeds65536"));}
    for(int32 I=0;I<Info.Num();++I){if((I==0&&Info[I].ParentIndex!=INDEX_NONE)||(I>0&&(Info[I].ParentIndex<0||Info[I].ParentIndex>=I))){return Bad(TEXT("Reference skeleton parent chain is invalid"));}}
    int32 MaxRequired=INDEX_NONE;for(FBoneIndexType B:Required){MaxRequired=FMath::Max(MaxRequired,static_cast<int32>(B));}const bool MeshIndexed=Transforms.Num()>MaxRequired;
    auto Local=[&](int32 B,FTransform& T){const int32 Position=Required.Find(static_cast<FBoneIndexType>(B));const int32 At=MeshIndexed&&Position!=INDEX_NONE?B:Position;if(!Transforms.IsValidIndex(At)){return false;}T=Transforms[At];return true;};
    auto Rows=MakeShared<FJsonObject>();J->SetObjectField(TEXT("bone_samples"),Rows);
    for(const FString& Name:Bones)
    {
        if(Name.IsEmpty()){continue;}if(!SafeText(Name,NAME_SIZE-1)||!W.Current()){return Bad(TEXT("PoseWatch sample name/source unavailable"));}W.SDK(TEXT("bone_index_calls"));const int32 Bone=W.Component->GetBoneIndex(FName(*Name));if(!W.Current()){return Bad(TEXT("Bone-index callback changed source"));}
        FAnimNodePoseTransformSample Sample;auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("bone"),Name);FTransform T;
        if(Info.IsValidIndex(Bone)&&Local(Bone,T))
        {Sample.ComponentTransform=T;int32 Parent=Info[Bone].ParentIndex;int32 Depth=0;while(Parent!=INDEX_NONE){if(++Depth>65536||!Info.IsValidIndex(Parent)||!Local(Parent,T)){break;}Sample.ComponentTransform=Sample.ComponentTransform*T;Parent=Info[Parent].ParentIndex;}Sample.bValid=Parent==INDEX_NONE&&FiniteTransform(Sample.ComponentTransform);Sample.WorldTransform=Sample.ComponentTransform*World;Sample.bValid&=FiniteTransform(Sample.WorldTransform);}
        Row->SetBoolField(TEXT("valid"),Sample.bValid);if(Sample.bValid){Row->SetObjectField(TEXT("world"),TransformJson(Sample.WorldTransform));Row->SetObjectField(TEXT("component"),TransformJson(Sample.ComponentTransform));}else{Sample.Error=TEXT("PoseWatch did not capture requested bone/parents: ")+Name;Row->SetStringField(TEXT("error"),Sample.Error);(Allow?Warnings:Errors).Add(MakeShared<FJsonValueString>(Sample.Error));}Rows->SetObjectField(Name,Row);Out.BoneSamples.Add(Name,Sample);
    }
    J->SetStringField(TEXT("captured_object"),Expected->GetPathName());J->SetBoolField(TEXT("captured_expected_anim_instance"),true);J->SetBoolField(TEXT("pose_watch_valid"),true);J->SetNumberField(TEXT("required_bone_count"),Required.Num());J->SetNumberField(TEXT("bone_transform_count"),Transforms.Num());J->SetObjectField(TEXT("world_transform"),TransformJson(World));J->SetBoolField(TEXT("valid"),W.Current());J->SetBoolField(TEXT("fresh_this_request_verified"),false);return Out;
}
TSharedPtr<FJsonObject> PoseWatchMode(const TSharedPtr<FJsonObject>& P,const FString& BlueprintName,UBlueprint* B,UEdGraph* G,UAnimGraphNode_Base* N,const TSharedPtr<FJsonObject>& Target,const TArray<FString>& Bones,const TArray<FString>& Sockets)
{
    auto& W=*CurrentWork;TArray<TSharedPtr<FJsonValue>> Errors,Warnings;FAnimInstanceRuntimeTarget T;FString Error;
    if(!FindAnimInstanceRuntimeTarget(P,GetBoolParam(P,TEXT("prefer_pie_world"),true),GetBoolParam(P,TEXT("require_pie_world"),false),TEXT("pose-watched"),T,Warnings,Error)){return ErrorResponse(Error);}
    if(!B->GeneratedClass||!T.AnimInstance->IsA(B->GeneratedClass)){return ErrorResponse(TEXT("Matched AnimInstance class is not compatible with resolved blueprint"));}
    bool Runtime=false,Debug=false;auto Mapping=AnimGraphCompiledNodeMappingToJson(B,N,T.AnimInstance,T.Component,&Runtime,&Debug);if(!Current()){return ErrorResponse(W.Result.Error);}
    auto* AC=Cast<UAnimBlueprintGeneratedClass>(T.AnimInstance->GetClass());if(!AC){return ErrorResponse(TEXT("Selected instance is not an AnimBlueprint generated class"));}
    const int32 PostIndex=AC->GetNodeIndexFromGuid(N->NodeGuid,EPropertySearchMode::Hierarchy);const auto Properties=AC->GetAnimNodeProperties();FStructProperty* Property=Properties.IsValidIndex(PostIndex)?Properties[PostIndex]:nullptr;
    auto Links=RuntimeAnimNodePoseLinksToJson(AC,Property,T.AnimInstance,GetClampedIntParam(P,TEXT("pose_link_max_depth"),4,0,8));if(!Current()){return ErrorResponse(W.Result.Error);}auto Support=BuildAnimNodeRuntimeMappingSupportJson(N,Property,Links,Runtime,Debug);Target->SetObjectField(TEXT("mapping_support"),Support);
    TArray<FRuntimePoseLinkSelection> Selections;FString InputMode;const bool HasInput=SelectRuntimePoseLinksForPoseWatch(P,Links,Selections,InputMode,Errors);
    auto J=MakeShared<FJsonObject>();J->SetObjectField(TEXT("runtime_node_mapping"),Mapping);J->SetObjectField(TEXT("runtime_pose_links"),Links);J->SetObjectField(TEXT("mapping_support"),Support);J->SetObjectField(TEXT("target_node"),Target);J->SetStringField(TEXT("mode"),TEXT("pose_watch_capture"));J->SetBoolField(TEXT("runtime_graph_prepost"),false);J->SetBoolField(TEXT("same_instance_prepost"),false);
    if(!Current()||!RuntimeProperty(AC,Property,T.AnimInstance)||!HasInput){J->SetBoolField(TEXT("success"),false);J->SetArrayField(TEXT("errors"),Errors);return J;}
    auto* RuntimeBP=Cast<UAnimBlueprint>(AC->ClassGeneratedBy);if(!Live(RuntimeBP)||!W.Hold(RuntimeBP)){return ErrorResponse(TEXT("Runtime AnimBlueprint unavailable"));}
    // Public root lookup follows ParentClass. Bound its chain before entering the SDK recursion.
    TSet<UClass*> Parents;for(UClass* C=RuntimeBP->ParentClass;C;C=C->GetSuperClass()){if(!Live(C)||Parents.Contains(C)||Parents.Num()>=64||!W.Hold(C)){return ErrorResponse(TEXT("AnimBlueprint parent chain exceeds safe root-lookup boundary"));}Parents.Add(C);}
    W.SDK(TEXT("root_anim_blueprint_queries"));UAnimBlueprint* Root=UAnimBlueprint::FindRootAnimBlueprint(RuntimeBP);if(!Current()){return ErrorResponse(W.Result.Error);}FWatchSession Session;
    if(!Session.Start(Root?Root:RuntimeBP,T.AnimInstance)){Session.Close();return ErrorResponse(TEXT("Could not establish exact runtime debug target"));}
    auto Post=Session.Register(PostIndex,TEXT("post_node_output"),Errors);TArray<TSharedPtr<FJsonValue>> SelectedRows,Registered;
    for(const auto& Selection:Selections){SelectedRows.Add(MakeShared<FJsonValueObject>(RuntimePoseLinkSelectionToJson(Selection)));if(Selection.LinkID!=PostIndex){Session.Register(Selection.LinkID,FString::Printf(TEXT("pre_input_pose[%d]"),Selection.Index),Errors);}else{Warnings.Add(MakeShared<FJsonValueString>(TEXT("Pre input link matched post link; shared one watch")));}}
    for(const auto& R:Session.Registrations){Registered.Add(MakeShared<FJsonValueObject>(PoseWatchRegistrationToJson(R)));}
    const bool Allow=GetBoolParam(P,TEXT("allow_missing_bones"),false)||GetBoolParam(P,TEXT("allow_missing_samples"),false);const double Delta=GetClampedNumberParam(P,TEXT("tick_delta_time"),1.0/30.0,0,1);const bool Refresh=GetBoolParam(P,TEXT("refresh_bone_transforms"),true);
    auto Settle=TickSkeletalComponentForAnimRuntimeProbe(T.Component,GetClampedIntParam(P,TEXT("settle_tick_count"),0,0,240),Delta,Refresh,Warnings);auto Tick=TickSkeletalComponentForAnimRuntimeProbe(T.Component,GetClampedIntParam(P,TEXT("tick_count"),1,0,240),Delta,Refresh,Warnings);
    TArray<FAnimNodePoseCapture> Pre;TArray<TSharedPtr<FJsonValue>> PreRows,Deltas;bool Valid=Errors.IsEmpty();
    for(const auto& S:Selections){auto Capture=CaptureWatch(Session,S.LinkID,FString::Printf(TEXT("pre_input_pose[%d]"),S.Index),T.AnimInstance,Bones,Allow,Errors,Warnings);Capture.Json->SetNumberField(TEXT("input_pose_index"),S.Index);Capture.Json->SetNumberField(TEXT("input_pose_link_id"),S.LinkID);Capture.Json->SetStringField(TEXT("input_pose_field_path"),S.FieldPath);Capture.Json->SetObjectField(TEXT("runtime_pose_link"),S.LinkObject);Valid&=GetJsonBoolFieldOrDefault(Capture.Json,TEXT("valid"),false);PreRows.Add(MakeShared<FJsonValueObject>(Capture.Json));Pre.Add(MoveTemp(Capture));}
    auto PostPose=CaptureWatch(Session,PostIndex,TEXT("post_node_output"),T.AnimInstance,Bones,Allow,Errors,Warnings);Valid&=GetJsonBoolFieldOrDefault(PostPose.Json,TEXT("valid"),false)&&Errors.IsEmpty()&&Current();const bool Closed=Session.Close();
    for(int32 I=0;I<Pre.Num();++I){auto D=MakeShared<FJsonObject>();D->SetNumberField(TEXT("input_pose_index"),Selections[I].Index);D->SetNumberField(TEXT("input_pose_link_id"),Selections[I].LinkID);D->SetStringField(TEXT("input_pose_field_path"),Selections[I].FieldPath);D->SetObjectField(TEXT("deltas"),BuildAnimNodePoseDeltas(Pre[I],PostPose,Bones,{}));Deltas.Add(MakeShared<FJsonValueObject>(D));}
    J->SetBoolField(TEXT("success"),Valid);J->SetBoolField(TEXT("read_only"),false);J->SetBoolField(TEXT("runtime_only"),true);J->SetBoolField(TEXT("asset_modified"),false);J->SetBoolField(TEXT("saves_assets"),false);J->SetBoolField(TEXT("dry_run"),false);J->SetBoolField(TEXT("runtime_graph_prepost"),Valid);J->SetBoolField(TEXT("same_instance_prepost"),Valid);J->SetBoolField(TEXT("same_anim_instance_node_mapping"),Runtime||Debug);J->SetBoolField(TEXT("runtime_node_instance_mapped"),Runtime);J->SetBoolField(TEXT("find_debug_anim_node_mapped"),Debug);J->SetBoolField(TEXT("original_assets_modified"),false);J->SetBoolField(TEXT("temp_assets_created"),false);J->SetBoolField(TEXT("transient_pose_watches"),true);J->SetBoolField(TEXT("debug_object_was_already_target"),!Session.ChangedDebug);J->SetBoolField(TEXT("debug_object_restored"),Closed);J->SetStringField(TEXT("comparison_kind"),TEXT("runtime_pose_watch_prepost"));J->SetStringField(TEXT("previous_debug_object"),Live(Session.Previous.Get())?Session.Previous->GetPathName():FString());J->SetStringField(TEXT("debug_anim_blueprint"),Session.Identity?Session.Identity->Path:FString());J->SetStringField(TEXT("blueprint_name"),BlueprintName);J->SetStringField(TEXT("blueprint_path"),W.SourcePath);J->SetNumberField(TEXT("post_node_link_id"),PostIndex);J->SetNumberField(TEXT("pre_input_link_id"),Selections.IsEmpty()?INDEX_NONE:Selections[0].LinkID);J->SetStringField(TEXT("pre_input_field_path"),Selections.IsEmpty()?FString():Selections[0].FieldPath);J->SetStringField(TEXT("input_pose_mode"),InputMode);J->SetNumberField(TEXT("input_pose_selection_count"),Selections.Num());J->SetArrayField(TEXT("pre_input_pose_links"),SelectedRows);J->SetArrayField(TEXT("pose_watch_registrations"),Registered);J->SetArrayField(TEXT("requested_sample_bones"),Strings(Bones));J->SetArrayField(TEXT("requested_sample_sockets"),Strings(Sockets));J->SetBoolField(TEXT("allow_missing_bones"),Allow);J->SetObjectField(TEXT("settle_tick"),Settle);J->SetObjectField(TEXT("tick"),Tick);if(!Pre.IsEmpty()){J->SetObjectField(TEXT("pre_pose"),Pre[0].Json);J->SetObjectField(TEXT("deltas"),BuildAnimNodePoseDeltas(Pre[0],PostPose,Bones,{}));}J->SetArrayField(TEXT("pre_input_poses"),PreRows);J->SetObjectField(TEXT("post_pose"),PostPose.Json);J->SetArrayField(TEXT("deltas_by_input"),Deltas);if(!Sockets.IsEmpty()){Warnings.Add(MakeShared<FJsonValueString>(TEXT("PoseWatch captures internal bones; requested sockets unsupported")));}J->SetArrayField(TEXT("warnings"),Warnings);J->SetArrayField(TEXT("errors"),Errors);PopulateAnimInstanceRuntimeTargetFields(J,T,GetBoolParam(P,TEXT("prefer_pie_world"),true),GetBoolParam(P,TEXT("require_pie_world"),false));AddGraphField(J,B,G);
    if(!Closed){W.Fail(TEXT("debug_cleanup_unverified"),TEXT("Owned PoseWatch or root debug cleanup could not be confirmed; legacy result retained without atomic-rollback claim"));}return J;
}
struct FTempAsset
{FString Path;TStrongObjectPtr<UBlueprint> Blueprint;TUniquePtr<FIdentity> Identity;bool Created=false,DeletedExisting=false;};
bool TempPath(const FString& Path)
{return Path.StartsWith(TEXT("/Game/_MCP_Temp/"),ESearchCase::IgnoreCase)&&SafeText(Path,NAME_SIZE-1)&&!Path.Contains(TEXT("//"))&&FPackageName::IsValidLongPackageName(Path,true);}
bool DeleteProjection(const FString& Path,UObject* Object,FWork& W)
{
    if(!Live(Object)||!Live(Object->GetPackage())||!W.Current()){return false;}FIdentity Identity(Object);const FString Package=Object->GetPackage()->GetName();if(!TempPath(Package)||Package==W.Blueprint->GetPackage()->GetName()){return W.Fail(TEXT("delete_source_alias"),TEXT("Temp deletion may not alias the source package"));}
    W.SDK(TEXT("delete_referencer_registry_acquisition"));auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();if(!W.Current()||Registry.IsLoadingAssets()){return W.Fail(TEXT("delete_discovery_pending"),TEXT("Safe temp deletion requires completed registry discovery; no retry"));}
    TArray<FName> Saved;W.SDK(TEXT("delete_saved_referencer_queries"));if(!Registry.GetReferencers(FName(*Package),Saved,UE::AssetRegistry::EDependencyCategory::Package)||Saved.Num()>65536||!W.Current()||!Identity.Current()){return W.Fail(TEXT("delete_referencers_unknown"),TEXT("Saved referencer projection unavailable or owner changed"));}
    for(FName R:Saved){if(R.ToString()!=Package){return W.Fail(TEXT("delete_external_reference"),TEXT("Temp asset is referenced by another saved package; no force delete"));}}
    TArray<UObject*> Members;GetObjectsWithPackage(Object->GetPackage(),Members,EGetObjectsFlags::IncludeNestedObjects,RF_NoFlags,EInternalObjectFlags::Garbage);if(Members.Num()>4096){return W.Fail(TEXT("delete_member_bound"),TEXT("Delete package exceeds4096 members"));}
    TArray<TUniquePtr<FIdentity>> Ids;for(UObject* Member:Members){if(!Live(Member)){return W.Fail(TEXT("delete_member_unavailable"),TEXT("Delete package member unavailable"));}Ids.Add(MakeUnique<FIdentity>(Member));}
    W.SDK(TEXT("delete_loaded_referencer_queries"));const auto Referencers=FReferencerFinder::GetAllReferencers(Members,nullptr,EReferencerFinderFlags::SkipWeakReferences);if(Referencers.Num()>65536||!W.Current()||!Identity.Current()){return false;}for(const auto& Id:Ids){if(!Id->Current()){return W.Fail(TEXT("delete_member_changed"),TEXT("Member changed during reference query"));}}
    for(UObject* R:Referencers){if(!Live(R)||!Live(R->GetPackage())||(R->GetPackage()!=GetTransientPackage()&&R->GetPackage()->GetName()!=Package)){return W.Fail(TEXT("delete_external_reference"),TEXT("A loaded package outside the temp target refers to it; force-delete refused"));}}
    W.Result.Data->SetBoolField(TEXT("delete_can_clear_transient_editor_undo_references"),true);W.Result.Data->SetBoolField(TEXT("delete_all_soft_RAM_references_verified"),false);return Identity.Current()&&W.Current()&&TempPath(Path);
}
bool DuplicateTemp(FTempAsset& T,bool Overwrite,FWork& W,TArray<TSharedPtr<FJsonValue>>& Errors)
{
    if(!W.Idle(TEXT("prepare_isolated_temp_asset"))||!TempPath(T.Path)||T.Path==W.Blueprint->GetPackage()->GetName()){Errors.Add(MakeShared<FJsonValueString>(TEXT("Invalid or source-alias temp asset path")));return false;}
    W.SDK(TEXT("temp_asset_exists_queries"));const bool Exists=UEditorAssetLibrary::DoesAssetExist(T.Path);if(!W.Current()){return false;}
    if(Exists)
    {
        if(!Overwrite){Errors.Add(MakeShared<FJsonValueString>(TEXT("Temp asset already exists: ")+T.Path));return false;}if(W.Blueprint->GetPackage()->IsDirty()){return W.Fail(TEXT("overwrite_source_dirty"),TEXT("Dirty source cannot be protected by saved soft-reference metadata during force delete"));}
        W.SDK(TEXT("temp_existing_asset_loads"));UObject* Existing=UEditorAssetLibrary::LoadAsset(T.Path);if(!W.Current()||!DeleteProjection(T.Path,Existing,W)){return false;}
        if(!W.Idle(TEXT("delete_existing_temp_asset"))){return false;}W.Mutation(TEXT("delete_existing_temp_calls"));const bool Deleted=UEditorAssetLibrary::DeleteAsset(T.Path);W.Count(TEXT("delete_existing_temp_returns"));T.DeletedExisting=Deleted;if(Deleted){W.Changed();}if(!W.Current()||!Deleted){Errors.Add(MakeShared<FJsonValueString>(TEXT("Failed to delete existing temp asset: ")+T.Path));return false;}
    }
    if(!W.Idle(TEXT("duplicate_isolated_temp_blueprint"))){return false;}W.Mutation(TEXT("duplicate_temp_calls"));UObject* Duplicated=UEditorAssetLibrary::DuplicateAsset(W.Blueprint->GetPackage()->GetName(),T.Path);W.Count(TEXT("duplicate_temp_returns"));T.Blueprint.Reset(Cast<UBlueprint>(Duplicated));T.Created=Duplicated!=nullptr;if(T.Created){W.Changed();}
    if(!W.Current()||!Live(T.Blueprint.Get())||!T.Blueprint->IsA<UAnimBlueprint>()||T.Blueprint->GetClass()!=W.Blueprint->GetClass()||T.Blueprint->GetPackage()->GetName()!=T.Path||Duplicated==W.Blueprint){Errors.Add(MakeShared<FJsonValueString>(TEXT("Duplicated AnimBlueprint identity could not be verified")));return false;}T.Identity=MakeUnique<FIdentity>(T.Blueprint.Get());return T.Identity->Current();
}
TSharedPtr<FJsonObject> CompileTemp(FTempAsset& T,FWork& W)
{
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("requested"),true);J->SetBoolField(TEXT("success"),false);
    W.Result.bCompileRequested=true;if(!T.Identity||!T.Identity->Current()||!W.Idle(TEXT("compile_isolated_temp_blueprint"))){return J;}
    FCompilerResultsLog Log;W.Result.bCompileAttempted=true;W.Mutation(TEXT("explicit_compile_calls"));FKismetEditorUtilities::CompileBlueprint(T.Blueprint.Get(),EBlueprintCompileOptions::None,&Log);W.Count(TEXT("explicit_compile_returns"));W.Changed();
    if(!W.Current()||!T.Identity->Current()){return J;}UClass* Generated=T.Blueprint->GeneratedClass;const bool OK=Log.NumErrors==0&&T.Blueprint->Status!=BS_Error&&Live(Generated)&&!Generated->HasAnyClassFlags(CLASS_Abstract|CLASS_NewerVersionExists)&&Generated->IsChildOf(UAnimInstance::StaticClass())&&Generated->ClassGeneratedBy==T.Blueprint.Get();
    W.Result.Compiled=W.Result.Compiled.IsSet()?W.Result.Compiled.GetValue()&&OK:OK;J->SetBoolField(TEXT("success"),OK);J->SetStringField(TEXT("blueprint"),T.Blueprint->GetPathName());J->SetStringField(TEXT("generated_class"),GetPathNameSafe(Generated));J->SetNumberField(TEXT("compiler_errors"),Log.NumErrors);J->SetNumberField(TEXT("compiler_warnings"),Log.NumWarnings);return J;
}
bool BypassTemp(FTempAsset& T,const TSharedPtr<FJsonObject>& Resolve,FWork& W,TSharedPtr<FJsonObject>& Out,TArray<TSharedPtr<FJsonValue>>& Errors)
{
    Out=MakeShared<FJsonObject>();if(!T.Identity||!T.Identity->Current()||!W.Idle(TEXT("bypass_isolated_temp_node"))){return false;}FWork Local;Local.Calls=W.Calls;Local.Blueprint=T.Blueprint.Get();Local.SourcePath=T.Blueprint->GetPathName();Local.OriginalGeneratedClass=T.Blueprint->GeneratedClass;if(!Local.Hold(T.Blueprint.Get())){return false;}FWork* Saved=CurrentWork;CurrentWork=&Local;ON_SCOPE_EXIT{CurrentWork=Saved;W.Result.bChangeStateKnown=false;W.Result.bMutationAttempted|=Local.Result.bMutationAttempted;W.Result.bChanged|=Local.Result.bChanged;W.ExplicitChanged|=Local.ExplicitChanged;};
    UEdGraph* Graph=ResolveGraph(Local.Blueprint,Resolve,Local);if(!Graph||!Local.CaptureGraph(Graph)){Errors.Add(MakeShared<FJsonValueString>(Local.Result.Error));return false;}
    UAnimGraphNode_Base* Found=nullptr;TArray<UAnimGraphNode_Base*> SameClass;UAnimGraphNode_Base* NameMatch=nullptr;
    for(UEdGraphNode* N:Local.Nodes){auto* A=Cast<UAnimGraphNode_Base>(N);if(!A){continue;}if(A->NodeGuid==W.Node->NodeGuid){Found=A;break;}if(A->GetClass()==W.Node->GetClass()){SameClass.Add(A);if(A->GetName().Equals(W.Node->GetName(),ESearchCase::IgnoreCase)){NameMatch=A;}}}
    if(!Found){Found=NameMatch?NameMatch:(SameClass.Num()==1?SameClass[0]:nullptr);}if(!Found){Errors.Add(MakeShared<FJsonValueString>(TEXT("Target node not found in temp AnimBlueprint")));return false;}
    UEdGraphPin* Input=PosePin(Found,EGPD_Input,Local);UEdGraphPin* Output=PosePin(Found,EGPD_Output,Local);if(!Input||!Output||Input->LinkedTo.Num()!=1||Output->LinkedTo.IsEmpty()){Errors.Add(MakeShared<FJsonValueString>(TEXT("Bypass needs exactly one upstream pose and at least one downstream pose")));return false;}
    UEdGraphPin* Upstream=Input->LinkedTo[0];const auto Downstream=Output->LinkedTo;const UEdGraphSchema* Schema=Graph->GetSchema();if(!Live(Schema)||!Local.Hold(const_cast<UEdGraphSchema*>(Schema))){return false;}TArray<TSharedPtr<FJsonValue>> Reconnected;bool Changed=false;
    for(UEdGraphPin* Pin:Downstream)
    {
        if(!W.Current()||!Local.Pin(Upstream)||!Local.Pin(Pin)||!Local.Idle(TEXT("reconnect_temp_pose"))){return false;}const FPinConnectionResponse Can=Schema->CanCreateConnection(Upstream,Pin);if(!Local.Current()){return false;}if(Can.Response==CONNECT_RESPONSE_DISALLOW||Can.Response==CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE){Errors.Add(MakeShared<FJsonValueString>(Can.Message.ToString()));return false;}
        Local.Mutation(TEXT("temp_pose_connection_calls"));const bool Connected=Schema->TryCreateConnection(Upstream,Pin);if(!Local.CaptureGraph(Graph)||!Local.Pin(Upstream)||!Local.Pin(Pin)){return false;}Changed|=Connected;Local.Changed();Reconnected.Add(MakeShared<FJsonValueObject>(PinJson(Pin,Local)));if(!Connected||!Upstream->LinkedTo.Contains(Pin)){Errors.Add(MakeShared<FJsonValueString>(TEXT("Temp pose bypass connection failed; earlier links remain")));return false;}
    }
    if(Changed)
    {
        if(!Local.Current()||!W.Current()){return false;}Local.Mutation(TEXT("temp_mark_blueprint_modified_calls"));FBlueprintEditorUtils::MarkBlueprintAsModified(Local.Blueprint);if(!Local.CaptureGraph(Graph)||!Local.Nodes.Contains(Found)){return false;}Local.Mutation(TEXT("temp_reconstruct_node_calls"));Found->ReconstructNode();if(!Local.CaptureGraph(Graph)){return false;}Local.Mutation(TEXT("temp_notify_graph_calls"));Graph->NotifyGraphChanged();if(!Local.CaptureGraph(Graph)){return false;}
    }
    Out->SetBoolField(TEXT("success"),true);Out->SetBoolField(TEXT("graph_changed"),Changed);Out->SetStringField(TEXT("blueprint"),T.Identity->Path);Out->SetObjectField(TEXT("bypassed_node"),NodeJson(Found,false,Local));Out->SetArrayField(TEXT("reconnected_downstream_pins"),Reconnected);return T.Identity->Current()&&W.Current();
}
struct FProbe
{TUniquePtr<FIdentity> Actor,Component;TUniquePtr<FWork> Context;FString ActorPath,ComponentPath;bool Spawned=false,Destroyed=false;};
bool SpawnProbe(FProbe& P,UWorld* World,USkeletalMesh* Mesh,UClass* Class,const FString& Label,const FVector& Location,FWork& W)
{
    if(!W.Current()||!Live(World)||!Live(Mesh)||!Live(Class)||!Class->IsChildOf(UAnimInstance::StaticClass())||Class->HasAnyClassFlags(CLASS_Abstract|CLASS_NewerVersionExists)){return false;}FIdentity WorldId(World),MeshId(Mesh),ClassId(Class);FActorSpawnParameters Params;Params.Name=MakeUniqueObjectName(World,AActor::StaticClass(),FName(*Label));Params.ObjectFlags|=RF_Transient;
    W.Mutation(TEXT("spawn_probe_actor_calls"));AActor* Actor=World->SpawnActor<AActor>(AActor::StaticClass(),FTransform(FRotator::ZeroRotator,Location),Params);P.Spawned=Actor!=nullptr;if(!W.Current()||!WorldId.Current()||!Live(Actor)||Actor->GetWorld()!=World){return false;}W.Changed();P.Actor=MakeUnique<FIdentity>(Actor);P.ActorPath=Actor->GetPathName();
    auto CurrentOwned=[&](){return W.Current()&&WorldId.Current()&&MeshId.Current()&&ClassId.Current()&&P.Actor->Current()&&Actor->GetWorld()==World&&(!P.Component||(P.Component->Current()&&Cast<USkeletalMeshComponent>(P.Component->Object.Get())->GetOwner()==Actor));};
    W.Mutation(TEXT("probe_set_actor_label_calls"));Actor->SetActorLabel(Label);if(!CurrentOwned()){return false;}W.Mutation(TEXT("probe_new_component_calls"));auto* C=NewObject<USkeletalMeshComponent>(Actor,TEXT("AnimNodeProbeSkeletalMeshComponent"),RF_Transient);if(!CurrentOwned()||!Live(C)){return false;}P.Component=MakeUnique<FIdentity>(C);P.ComponentPath=C->GetPathName();
    W.Mutation(TEXT("probe_set_mesh_calls"));C->SetSkeletalMesh(Mesh);if(!CurrentOwned()){return false;}W.Mutation(TEXT("probe_set_animation_mode_calls"));C->SetAnimationMode(EAnimationMode::AnimationBlueprint);if(!CurrentOwned()){return false;}W.Mutation(TEXT("probe_set_anim_class_calls"));C->SetAnimInstanceClass(Class);if(!CurrentOwned()){return false;}W.Mutation(TEXT("probe_set_root_component_calls"));Actor->SetRootComponent(C);if(!CurrentOwned()){return false;}W.Mutation(TEXT("probe_add_instance_component_calls"));Actor->AddInstanceComponent(C);if(!CurrentOwned()){return false;}W.Mutation(TEXT("probe_register_component_calls"));C->RegisterComponent();if(!CurrentOwned()){return false;}
    P.Context=MakeUnique<FWork>();auto& R=*P.Context;R.Calls=W.Calls;R.World=World;R.Type=World->WorldType;R.Actor=Actor;R.Component=C;if(!R.Hold(World)||!R.Hold(Actor)||!R.Hold(C)||!Sampling::PreparePoseSources(R)||!RuntimeParticipants(R,true)){return false;}W.Mutation(TEXT("probe_initial_refresh_calls"));C->RefreshBoneTransforms();W.Count(TEXT("probe_initial_refresh_returns"));return CurrentOwned()&&R.Current();
}
void CleanupProbe(FProbe& P,UWorld* World,const FString& Label,FWork& W,TArray<TSharedPtr<FJsonValue>>& Rows)
{
    auto J=MakeShared<FJsonObject>();Rows.Add(MakeShared<FJsonValueObject>(J));J->SetStringField(TEXT("label"),Label);J->SetBoolField(TEXT("had_actor"),P.Spawned);J->SetStringField(TEXT("actor_name"),P.ActorPath);J->SetBoolField(TEXT("destroyed"),false);if(!P.Spawned){return;}
    if(!IsEditorGameThread()||!Live(World)||!P.Actor||!P.Actor->Current()||!P.Actor->Object->HasAnyFlags(RF_Transient)||Cast<AActor>(P.Actor->Object.Get())->GetWorld()!=World){W.Fail(TEXT("actor_cleanup_unverified"),TEXT("Owned probe actor identity changed; no foreign cleanup attempted"));return;}
    W.Mutation(TEXT("destroy_probe_actor_calls"));P.Destroyed=World->DestroyActor(Cast<AActor>(P.Actor->Object.Get()));W.Count(TEXT("destroy_probe_actor_returns"));J->SetBoolField(TEXT("destroyed"),P.Destroyed);if(!P.Destroyed){W.Fail(TEXT("actor_cleanup_failed"),TEXT("Public DestroyActor returned false; exact owned actor remains"));}
}
void CleanupTemp(FTempAsset& T,const FString& Label,FWork& W,TArray<TSharedPtr<FJsonValue>>& Rows)
{
    auto J=MakeShared<FJsonObject>();Rows.Add(MakeShared<FJsonValueObject>(J));J->SetStringField(TEXT("label"),Label);J->SetStringField(TEXT("asset_path"),T.Path);J->SetBoolField(TEXT("existed"),T.Created);J->SetBoolField(TEXT("deleted"),false);if(!T.Created){return;}
    // A pre-existing path which failed duplication is never claimed as cleanup ownership.
    if(!IsEditorGameThread()||IsPlaySessionActive()||!T.Identity||!T.Identity->Current()||!TempPath(T.Path)){W.Fail(TEXT("asset_cleanup_unverified"),TEXT("Owned temp identity or idle boundary changed; no blind path deletion"));return;}
    // Preserve an earlier operation error while checking exact cleanup referencers in a fresh local read context.
    FWork Guard;Guard.Blueprint=W.Blueprint;Guard.OriginalGeneratedClass=W.OriginalGeneratedClass;Guard.SourcePath=W.SourcePath;Guard.Calls=W.Calls;Guard.Hold(W.Blueprint);if(!DeleteProjection(T.Path,T.Blueprint.Get(),Guard)){W.Fail(TEXT("asset_cleanup_unverified"),TEXT("Temp cleanup referencer projection did not pass; exact owned asset retained"));return;}
    W.Mutation(TEXT("delete_owned_temp_calls"));const bool Deleted=UEditorAssetLibrary::DeleteAsset(T.Path);W.Count(TEXT("delete_owned_temp_returns"));J->SetBoolField(TEXT("deleted"),Deleted);if(!Deleted){W.Fail(TEXT("asset_cleanup_failed"),TEXT("Owned temp asset deletion returned false; no retry"));}
}
TSharedPtr<FJsonObject> IsolatedMode(const TSharedPtr<FJsonObject>& P,const FString& BlueprintName,UBlueprint* B,UEdGraph* G,UAnimGraphNode_Base* N,const TSharedPtr<FJsonObject>& Resolve,const TSharedPtr<FJsonObject>& Target,const TArray<FString>& Bones,const TArray<FString>& Sockets,bool Supported)
{
    auto& W=*CurrentWork;auto J=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Errors,Warnings,Cleanup;FTempAsset Source,Post;FProbe Before,After;UWorld* World=nullptr;TStrongObjectPtr<USkeletalMesh> Mesh;
    J->SetBoolField(TEXT("success"),false);J->SetStringField(TEXT("mode"),TEXT("isolated_temp_components"));J->SetStringField(TEXT("comparison_kind"),TEXT("isolated_temp_components"));J->SetBoolField(TEXT("runtime_graph_prepost"),false);J->SetBoolField(TEXT("same_instance_prepost"),false);J->SetBoolField(TEXT("original_assets_modified"),false);J->SetObjectField(TEXT("target_node"),Target);
    if(!Supported){return ErrorResponse(TEXT("isolated_temp_components currently supports only RigidBody and Trail nodes"));}const FString MeshPath=GetStringParam(P,TEXT("skeletal_mesh")).TrimStartAndEnd();if(MeshPath.IsEmpty()){return ErrorResponse(TEXT("isolated_temp_components requires skeletal_mesh"));}
    W.SDK(TEXT("isolated_mesh_asset_loads"));Mesh.Reset(Cast<USkeletalMesh>(UEditorAssetLibrary::LoadAsset(MeshPath)));if(!W.Current()||!Live(Mesh.Get())||!W.Hold(Mesh.Get())){return ErrorResponse(TEXT("Isolated SkeletalMesh not found or changed"));}
    if(!Sampling::Worlds(W,GetBoolParam(P,TEXT("prefer_pie_world"),true),GetBoolParam(P,TEXT("require_pie_world"),false))||W.CandidateWorlds.IsEmpty()){return ErrorResponse(TEXT("No candidate world available for isolated probe"));}World=W.CandidateWorlds[0];W.World=World;W.Type=World->WorldType;
    if(!W.Idle(TEXT("prepare_isolated_temp_assets"))){return ErrorResponse(W.Result.Error);}const FString Root=NormalizeTempRootForAnimNodeProbe(GetStringParam(P,TEXT("temp_root"),TEXT("/Game/_MCP_Temp/AnimNodePrePost")));if(!IsAllowedAnimNodeProbeTempRoot(Root)||!SafeText(Root,NAME_SIZE-1)){return ErrorResponse(TEXT("Refusing temp_root outside /Game/_MCP_Temp"));}
    const FString Run=SanitizeAssetNameSegmentForAnimNodeProbe(GetStringParam(P,TEXT("run_id"),FGuid::NewGuid().ToString(EGuidFormats::Digits)));const FString Prefix=Root+TEXT("/")+SanitizeAssetNameSegmentForAnimNodeProbe(B->GetName())+TEXT("_")+SanitizeAssetNameSegmentForAnimNodeProbe(N->GetName());Source.Path=Prefix+TEXT("_SourceBypass_")+Run;Post.Path=Prefix+TEXT("_PostNode_")+Run;
    if(!TempPath(Source.Path)||!TempPath(Post.Path)||Source.Path==Post.Path){return ErrorResponse(TEXT("Composed temp paths are invalid or collide"));}auto Prep=MakeShared<FJsonObject>();J->SetObjectField(TEXT("prepare_temp_assets"),Prep);Prep->SetStringField(TEXT("temp_root"),Root);Prep->SetStringField(TEXT("source_bypass_path"),Source.Path);Prep->SetStringField(TEXT("post_node_path"),Post.Path);const bool Overwrite=GetBoolParam(P,TEXT("overwrite_existing_temp_assets"),true);Prep->SetBoolField(TEXT("overwrite_existing_temp_assets"),Overwrite);
    W.Mutation(TEXT("make_temp_directory_calls"));const bool Directory=UEditorAssetLibrary::MakeDirectory(Root);if(!W.Current()||!Directory){Errors.Add(MakeShared<FJsonValueString>(TEXT("Failed to create isolated temp directory")));}
    bool Prepared=false;TSharedPtr<FJsonObject> Bypass=MakeShared<FJsonObject>();
    if(Errors.IsEmpty())
    {
        const bool SourceOK=DuplicateTemp(Source,Overwrite,W,Errors);const bool PostOK=W.Current()?DuplicateTemp(Post,Overwrite,W,Errors):false;
        if(SourceOK&&PostOK&&BypassTemp(Source,Resolve,W,Bypass,Errors)){auto A=CompileTemp(Source,W);auto C=CompileTemp(Post,W);Prep->SetObjectField(TEXT("source_compile"),A);Prep->SetObjectField(TEXT("post_compile"),C);Prepared=GetJsonBoolFieldOrDefault(A,TEXT("success"),false)&&GetJsonBoolFieldOrDefault(C,TEXT("success"),false);if(!Prepared){Errors.Add(MakeShared<FJsonValueString>(TEXT("Failed to compile isolated temp blueprints")));}}
    }
    Prep->SetObjectField(TEXT("source_bypass"),Bypass);FAnimNodePoseCapture PrePose,PostPose;const bool Allow=GetBoolParam(P,TEXT("allow_missing_bones"),false)||GetBoolParam(P,TEXT("allow_missing_samples"),false);const double Delta=GetClampedNumberParam(P,TEXT("tick_delta_time"),1.0/30.0,0,1);const int32 Settle=GetClampedIntParam(P,TEXT("settle_tick_count"),0,0,240),Ticks=GetClampedIntParam(P,TEXT("tick_count"),1,0,240);const bool Refresh=GetBoolParam(P,TEXT("refresh_bone_transforms"),true);
    if(Prepared&&W.Current())
    {
        const bool SpawnA=SpawnProbe(Before,World,Mesh.Get(),Source.Blueprint->GeneratedClass,TEXT("MCP_AnimNode_Source_")+Run,FVector(0,-80,120),W);const bool SpawnB=W.Current()?SpawnProbe(After,World,Mesh.Get(),Post.Blueprint->GeneratedClass,TEXT("MCP_AnimNode_Post_")+Run,FVector(0,80,120),W):false;
        if(SpawnA&&SpawnB)
        {
            for(int32 I=0;I<2;++I){auto& Probe=I==0?Before:After;auto& Context=*Probe.Context;FWork* Saved=CurrentWork;CurrentWork=&Context;auto SettleJ=TickSkeletalComponentForAnimRuntimeProbe(Context.Component,Settle,Delta,Refresh,Warnings);auto TickJ=TickSkeletalComponentForAnimRuntimeProbe(Context.Component,Ticks,Delta,Refresh,Warnings);auto Pose=CaptureAnimNodeProbePose(Context.Component,Bones,Sockets,Allow,Errors,Warnings);CurrentWork=Saved;W.Result.bMutationAttempted|=Context.Result.bMutationAttempted;W.ExplicitChanged|=Context.ExplicitChanged;W.Result.bChanged|=Context.Result.bChanged;W.Result.bChangeStateKnown=false;J->SetObjectField(I==0?TEXT("source_settle_tick"):TEXT("post_settle_tick"),SettleJ);J->SetObjectField(I==0?TEXT("source_tick"):TEXT("post_tick"),TickJ);auto Info=MakeShared<FJsonObject>();Info->SetStringField(TEXT("actor_path"),Probe.ActorPath);Info->SetStringField(TEXT("component_path"),Probe.ComponentPath);J->SetObjectField(I==0?TEXT("source_bypass_probe"):TEXT("post_node_probe"),Info);if(I==0){PrePose=MoveTemp(Pose);}else{PostPose=MoveTemp(Pose);}if(!Context.Result.Error.IsEmpty()){Errors.Add(MakeShared<FJsonValueString>(Context.Result.Error));break;}if(!W.Current()){break;}}
        }else{Errors.Add(MakeShared<FJsonValueString>(TEXT("Failed to spawn/configure isolated probe actors/components")));}
    }
    const bool LegacySuccess=Prepared&&Errors.IsEmpty()&&W.Result.Error.IsEmpty();const bool DoCleanup=GetBoolParam(P,TEXT("cleanup"),true);if(DoCleanup){CleanupProbe(Before,World,TEXT("source_bypass_actor"),W,Cleanup);CleanupProbe(After,World,TEXT("post_node_actor"),W,Cleanup);Before.Context.Reset();After.Context.Reset();CleanupTemp(Source,TEXT("source_bypass_asset"),W,Cleanup);CleanupTemp(Post,TEXT("post_node_asset"),W,Cleanup);}
    J->SetBoolField(TEXT("success"),LegacySuccess);J->SetBoolField(TEXT("read_only"),false);J->SetBoolField(TEXT("runtime_only"),false);J->SetBoolField(TEXT("asset_modified"),true);J->SetBoolField(TEXT("saves_assets"),false);J->SetBoolField(TEXT("dry_run"),false);J->SetBoolField(TEXT("temp_assets_created"),Source.Created||Post.Created);J->SetBoolField(TEXT("cleanup"),DoCleanup);J->SetStringField(TEXT("runtime_note"),TEXT("Separate source-bypass and selected-node transient components; not same-instance instrumentation. Duplicate SDK may implicitly compile/save; cleanup is exact-owned and non-atomic."));J->SetStringField(TEXT("blueprint_name"),BlueprintName);J->SetStringField(TEXT("blueprint_path"),W.SourcePath);J->SetStringField(TEXT("skeletal_mesh"),MeshPath);J->SetStringField(TEXT("sampled_world_type"),WorldType(World));J->SetStringField(TEXT("sampled_world_name"),Live(World)?World->GetName():FString());J->SetArrayField(TEXT("requested_sample_bones"),Strings(Bones));J->SetArrayField(TEXT("requested_sample_sockets"),Strings(Sockets));J->SetBoolField(TEXT("allow_missing_bones"),Allow);J->SetObjectField(TEXT("pre_pose"),PrePose.Json);J->SetObjectField(TEXT("post_pose"),PostPose.Json);J->SetObjectField(TEXT("deltas"),BuildAnimNodePoseDeltas(PrePose,PostPose,Bones,Sockets));J->SetArrayField(TEXT("cleanup_results"),Cleanup);J->SetArrayField(TEXT("errors"),Errors);J->SetArrayField(TEXT("warnings"),Warnings);AddGraphField(J,B,G);return J;
}
TSharedPtr<FJsonObject> BuildAnimNodePrePostRuntimeTickDeltaResponse(
    const TSharedPtr<FJsonObject>& Params,
    const FString& BlueprintName,
    UBlueprint* Blueprint,
    UEdGraph* TargetGraph,
    UAnimGraphNode_Base* TargetNode,
    const TSharedPtr<FJsonObject>& TargetNodeObject,
    const TArray<FString>& SampleBones,
    const TArray<FString>& SampleSockets,
    bool bIsolatedSamplerSupported)
{
    FString Mode = GetStringParam(Params, TEXT("mode"), TEXT("active_component_tick_delta"));
    Mode.TrimStartAndEndInline();
    if (Mode.IsEmpty())
    {
        Mode = TEXT("active_component_tick_delta");
    }

    if (!Mode.Equals(TEXT("active_component_tick_delta"), ESearchCase::IgnoreCase)
        && !Mode.Equals(TEXT("live_component_tick_delta"), ESearchCase::IgnoreCase))
    {
        TSharedPtr<FJsonObject> ErrorObj = ErrorResponse(FString::Printf(
            TEXT("Runtime mode '%s' is not implemented yet. Use mode='active_component_tick_delta' or dry_run=true."),
            *Mode));
        ErrorObj->SetStringField(TEXT("requested_mode"), Mode);
        ErrorObj->SetStringField(TEXT("implemented_mode"), TEXT("active_component_tick_delta"));
        ErrorObj->SetBoolField(TEXT("runtime_graph_prepost"), false);
        ErrorObj->SetBoolField(TEXT("same_instance_prepost"), false);
        ErrorObj->SetBoolField(TEXT("original_assets_modified"), false);
        ErrorObj->SetObjectField(TEXT("target_node"), TargetNodeObject);
        AddGraphField(ErrorObj, Blueprint, TargetGraph);
        return ErrorObj;
    }

    bool bPreferPIEWorld = true;
    Params->TryGetBoolField(TEXT("prefer_pie_world"), bPreferPIEWorld);
    const bool bRequirePIEWorld = GetBoolParam(Params, TEXT("require_pie_world"), false);
    const bool bAllowMissingSamples = GetBoolParam(Params, TEXT("allow_missing_bones"), false)
        || GetBoolParam(Params, TEXT("allow_missing_samples"), false);
    const int32 SettleTickCount = GetClampedIntParam(Params, TEXT("settle_tick_count"), 0, 0, 240);
    const int32 TickCount = GetClampedIntParam(Params, TEXT("tick_count"), 1, 0, 240);
    const double TickDeltaTime = GetClampedNumberParam(Params, TEXT("tick_delta_time"), 1.0 / 30.0, 0.0, 1.0);
    const bool bRefreshBoneTransforms = GetBoolParam(Params, TEXT("refresh_bone_transforms"), true);

    TArray<TSharedPtr<FJsonValue>> ErrorValues;
    TArray<TSharedPtr<FJsonValue>> WarningValues;

    if (!bIsolatedSamplerSupported)
    {
        WarningValues.Add(MakeShared<FJsonValueString>(TEXT("Selected node type is not supported by the later isolated sampler MVP; active_component_tick_delta can still sample final component pose deltas.")));
    }

    if (Params->HasField(TEXT("properties")) || Params->HasField(TEXT("property_name")) || Params->HasField(TEXT("curve_values")))
    {
        WarningValues.Add(MakeShared<FJsonValueString>(TEXT("active_component_tick_delta does not apply driver properties or curve values yet; use existing runtime property tools before this call when needed.")));
    }

    FAnimInstanceRuntimeTarget Target;
    FString TargetError;
    if (!FindAnimInstanceRuntimeTarget(Params, bPreferPIEWorld, bRequirePIEWorld, TEXT("sampled"), Target, WarningValues, TargetError))
    {
        TSharedPtr<FJsonObject> ErrorObj = ErrorResponse(TargetError);
        ErrorObj->SetStringField(TEXT("mode"), TEXT("active_component_tick_delta"));
        ErrorObj->SetBoolField(TEXT("runtime_graph_prepost"), false);
        ErrorObj->SetBoolField(TEXT("same_instance_prepost"), false);
        ErrorObj->SetBoolField(TEXT("original_assets_modified"), false);
        ErrorObj->SetObjectField(TEXT("target_node"), TargetNodeObject);
        ErrorObj->SetArrayField(TEXT("warnings"), WarningValues);
        AddGraphField(ErrorObj, Blueprint, TargetGraph);
        return ErrorObj;
    }

    if (Blueprint && Blueprint->GeneratedClass && Target.AnimInstance && !Target.AnimInstance->GetClass()->IsChildOf(Blueprint->GeneratedClass))
    {
        TSharedPtr<FJsonObject> ErrorObj = ErrorResponse(FString::Printf(
            TEXT("Matched AnimInstance class '%s' is not compatible with resolved blueprint '%s'. Pass an actor using this AnimBP."),
            Target.AnimInstance->GetClass() ? *Target.AnimInstance->GetClass()->GetPathName() : TEXT("<none>"),
            *CurrentWork->SourcePath));
        ErrorObj->SetStringField(TEXT("mode"), TEXT("active_component_tick_delta"));
        ErrorObj->SetStringField(TEXT("matched_anim_class"), Target.AnimInstance->GetClass() ? Target.AnimInstance->GetClass()->GetPathName() : FString());
        ErrorObj->SetStringField(TEXT("resolved_blueprint_class"), Blueprint->GeneratedClass->GetPathName());
        ErrorObj->SetBoolField(TEXT("runtime_graph_prepost"), false);
        ErrorObj->SetBoolField(TEXT("same_instance_prepost"), false);
        ErrorObj->SetBoolField(TEXT("original_assets_modified"), false);
        ErrorObj->SetObjectField(TEXT("target_node"), TargetNodeObject);
        AddGraphField(ErrorObj, Blueprint, TargetGraph);
        return ErrorObj;
    }

    TSharedPtr<FJsonObject> SettleTickObject = TickSkeletalComponentForAnimRuntimeProbe(
        Target.Component,
        SettleTickCount,
        TickDeltaTime,
        bRefreshBoneTransforms,
        WarningValues);

    const int32 ErrorCountBeforePreSample = ErrorValues.Num();
    FAnimNodePoseCapture PreTickPose = CaptureAnimNodeProbePose(
        Target.Component,
        SampleBones,
        SampleSockets,
        bAllowMissingSamples,
        ErrorValues,
        WarningValues);
    if (ErrorValues.Num() > ErrorCountBeforePreSample)
    {
        TSharedPtr<FJsonObject> ErrorObj = ErrorResponse(TEXT("One or more requested sample bones/sockets were missing; pass allow_missing_bones=true to return partial samples."));
        ErrorObj->SetStringField(TEXT("mode"), TEXT("active_component_tick_delta"));
        ErrorObj->SetBoolField(TEXT("runtime_graph_prepost"), false);
        ErrorObj->SetBoolField(TEXT("same_instance_prepost"), false);
        ErrorObj->SetBoolField(TEXT("original_assets_modified"), false);
        ErrorObj->SetObjectField(TEXT("target_node"), TargetNodeObject);
        ErrorObj->SetObjectField(TEXT("settle_tick"), SettleTickObject);
        ErrorObj->SetObjectField(TEXT("pre_tick_pose"), PreTickPose.Json);
        ErrorObj->SetArrayField(TEXT("errors"), ErrorValues);
        ErrorObj->SetArrayField(TEXT("warnings"), WarningValues);
        PopulateAnimInstanceRuntimeTargetFields(ErrorObj, Target, bPreferPIEWorld, bRequirePIEWorld);
        AddGraphField(ErrorObj, Blueprint, TargetGraph);
        return ErrorObj;
    }

    TSharedPtr<FJsonObject> TickObject = TickSkeletalComponentForAnimRuntimeProbe(
        Target.Component,
        TickCount,
        TickDeltaTime,
        bRefreshBoneTransforms,
        WarningValues);

    FAnimNodePoseCapture PostTickPose = CaptureAnimNodeProbePose(
        Target.Component,
        SampleBones,
        SampleSockets,
        bAllowMissingSamples,
        ErrorValues,
        WarningValues);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), ErrorValues.Num() == 0);
    ResultObj->SetBoolField(TEXT("read_only"), false);
    ResultObj->SetBoolField(TEXT("runtime_only"), true);
    ResultObj->SetBoolField(TEXT("asset_modified"), false);
    ResultObj->SetBoolField(TEXT("saves_assets"), false);
    ResultObj->SetBoolField(TEXT("dry_run"), false);
    ResultObj->SetStringField(TEXT("mode"), TEXT("active_component_tick_delta"));
    ResultObj->SetStringField(TEXT("comparison_kind"), TEXT("active_component_tick_delta"));
    ResultObj->SetBoolField(TEXT("runtime_graph_prepost"), false);
    ResultObj->SetBoolField(TEXT("same_instance_prepost"), false);
    ResultObj->SetBoolField(TEXT("original_assets_modified"), false);
    ResultObj->SetBoolField(TEXT("temp_assets_created"), false);
    ResultObj->SetStringField(TEXT("runtime_note"), TEXT("Samples final SkeletalMeshComponent pose before and after forced ticks on an active component. This is not true internal AnimGraph node source-vs-output instrumentation."));
    ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
    ResultObj->SetStringField(TEXT("blueprint_path"), CurrentWork->SourcePath);
    ResultObj->SetObjectField(TEXT("target_node"), TargetNodeObject);
    ResultObj->SetArrayField(TEXT("requested_sample_bones"), StringArrayToJsonValues(SampleBones));
    ResultObj->SetArrayField(TEXT("requested_sample_sockets"), StringArrayToJsonValues(SampleSockets));
    ResultObj->SetBoolField(TEXT("allow_missing_bones"), bAllowMissingSamples);
    ResultObj->SetObjectField(TEXT("settle_tick"), SettleTickObject);
    ResultObj->SetObjectField(TEXT("tick"), TickObject);
    ResultObj->SetObjectField(TEXT("pre_tick_pose"), PreTickPose.Json);
    ResultObj->SetObjectField(TEXT("post_tick_pose"), PostTickPose.Json);
    ResultObj->SetObjectField(TEXT("deltas"), BuildAnimNodePoseDeltas(PreTickPose, PostTickPose, SampleBones, SampleSockets));
    PopulateAnimInstanceRuntimeTargetFields(ResultObj, Target, bPreferPIEWorld, bRequirePIEWorld);
    AddGraphField(ResultObj, Blueprint, TargetGraph);
    ResultObj->SetArrayField(TEXT("errors"), ErrorValues);
    ResultObj->SetArrayField(TEXT("warnings"), WarningValues);
    return ResultObj;
}
TSharedPtr<FJsonObject> BuildAnimNodePrePostCompiledGraphMappingResponse(
    const TSharedPtr<FJsonObject>& Params,
    const FString& BlueprintName,
    UBlueprint* Blueprint,
    UEdGraph* TargetGraph,
    UAnimGraphNode_Base* TargetNode,
    const TSharedPtr<FJsonObject>& TargetNodeObject)
{
    bool bPreferPIEWorld = true;
    Params->TryGetBoolField(TEXT("prefer_pie_world"), bPreferPIEWorld);
    const bool bRequirePIEWorld = GetBoolParam(Params, TEXT("require_pie_world"), false);

    TArray<TSharedPtr<FJsonValue>> WarningValues;
    FAnimInstanceRuntimeTarget Target;
    FString TargetError;
    if (!FindAnimInstanceRuntimeTarget(Params, bPreferPIEWorld, bRequirePIEWorld, TEXT("mapped"), Target, WarningValues, TargetError))
    {
        TSharedPtr<FJsonObject> ErrorObj = ErrorResponse(TargetError);
        ErrorObj->SetStringField(TEXT("mode"), TEXT("compiled_graph_mapping"));
        ErrorObj->SetBoolField(TEXT("read_only"), true);
        ErrorObj->SetBoolField(TEXT("runtime_only"), true);
        ErrorObj->SetBoolField(TEXT("asset_modified"), false);
        ErrorObj->SetBoolField(TEXT("saves_assets"), false);
        ErrorObj->SetBoolField(TEXT("dry_run"), false);
        ErrorObj->SetBoolField(TEXT("runtime_graph_prepost"), false);
        ErrorObj->SetBoolField(TEXT("same_instance_prepost"), false);
        ErrorObj->SetBoolField(TEXT("same_anim_instance_node_mapping"), false);
        ErrorObj->SetBoolField(TEXT("instrumentation_preflight"), true);
        ErrorObj->SetBoolField(TEXT("original_assets_modified"), false);
        ErrorObj->SetBoolField(TEXT("temp_assets_created"), false);
        ErrorObj->SetObjectField(TEXT("target_node"), TargetNodeObject);
        ErrorObj->SetArrayField(TEXT("warnings"), WarningValues);
        AddGraphField(ErrorObj, Blueprint, TargetGraph);
        return ErrorObj;
    }

    if (Blueprint && Blueprint->GeneratedClass && Target.AnimInstance && !Target.AnimInstance->GetClass()->IsChildOf(Blueprint->GeneratedClass))
    {
        TSharedPtr<FJsonObject> ErrorObj = ErrorResponse(FString::Printf(
            TEXT("Matched AnimInstance class '%s' is not compatible with resolved blueprint '%s'. Pass an actor using this AnimBP."),
            Target.AnimInstance->GetClass() ? *Target.AnimInstance->GetClass()->GetPathName() : TEXT("<none>"),
            *CurrentWork->SourcePath));
        ErrorObj->SetStringField(TEXT("mode"), TEXT("compiled_graph_mapping"));
        ErrorObj->SetStringField(TEXT("matched_anim_class"), Target.AnimInstance->GetClass() ? Target.AnimInstance->GetClass()->GetPathName() : FString());
        ErrorObj->SetStringField(TEXT("resolved_blueprint_class"), Blueprint->GeneratedClass->GetPathName());
        ErrorObj->SetBoolField(TEXT("read_only"), true);
        ErrorObj->SetBoolField(TEXT("runtime_only"), true);
        ErrorObj->SetBoolField(TEXT("asset_modified"), false);
        ErrorObj->SetBoolField(TEXT("saves_assets"), false);
        ErrorObj->SetBoolField(TEXT("dry_run"), false);
        ErrorObj->SetBoolField(TEXT("runtime_graph_prepost"), false);
        ErrorObj->SetBoolField(TEXT("same_instance_prepost"), false);
        ErrorObj->SetBoolField(TEXT("same_anim_instance_node_mapping"), false);
        ErrorObj->SetBoolField(TEXT("instrumentation_preflight"), true);
        ErrorObj->SetBoolField(TEXT("original_assets_modified"), false);
        ErrorObj->SetBoolField(TEXT("temp_assets_created"), false);
        ErrorObj->SetObjectField(TEXT("target_node"), TargetNodeObject);
        ErrorObj->SetArrayField(TEXT("warnings"), WarningValues);
        AddGraphField(ErrorObj, Blueprint, TargetGraph);
        return ErrorObj;
    }

    bool bRuntimeNodeMapped = false;
    bool bFindDebugMapped = false;
    TSharedPtr<FJsonObject> MappingObject = AnimGraphCompiledNodeMappingToJson(
        Blueprint,
        TargetNode,
        Target.AnimInstance,
        Target.Component,
        &bRuntimeNodeMapped,
        &bFindDebugMapped);

    if(!Current()){return ErrorResponse(CurrentWork->Result.Error);}
    UAnimBlueprintGeneratedClass* RuntimeAnimClass = Target.AnimInstance ? Cast<UAnimBlueprintGeneratedClass>(Target.AnimInstance->GetClass()) : nullptr;
    const int32 RuntimeNodeIndex = (RuntimeAnimClass && TargetNode) ? RuntimeAnimClass->GetNodeIndexFromGuid(TargetNode->NodeGuid, EPropertySearchMode::Hierarchy) : INDEX_NONE;
    FStructProperty* RuntimeAnimNodeProperty = (RuntimeAnimClass && RuntimeAnimClass->GetAnimNodeProperties().IsValidIndex(RuntimeNodeIndex))
        ? RuntimeAnimClass->GetAnimNodeProperties()[RuntimeNodeIndex]
        : nullptr;
    const int32 PoseLinkMaxDepth = GetClampedIntParam(Params, TEXT("pose_link_max_depth"), 4, 0, 8);
    TSharedPtr<FJsonObject> RuntimePoseLinksObject = RuntimeAnimNodePoseLinksToJson(
        RuntimeAnimClass,
        RuntimeAnimNodeProperty,
        Target.AnimInstance,
        PoseLinkMaxDepth);
    const TArray<TSharedPtr<FJsonValue>>* RuntimePoseLinkValues = nullptr;
    const bool bRuntimePoseLinksAvailable = RuntimePoseLinksObject->TryGetArrayField(TEXT("pose_links"), RuntimePoseLinkValues)
        && RuntimePoseLinkValues
        && RuntimePoseLinkValues->Num() > 0;

    const bool bMapped = bRuntimeNodeMapped || bFindDebugMapped;
    if(!Current()){return ErrorResponse(CurrentWork->Result.Error);}
    TSharedPtr<FJsonObject> MappingSupportObject = BuildAnimNodeRuntimeMappingSupportJson(
        TargetNode,
        RuntimeAnimNodeProperty,
        RuntimePoseLinksObject,
        bRuntimeNodeMapped,
        bFindDebugMapped);
    TargetNodeObject->SetObjectField(TEXT("mapping_support"), MappingSupportObject);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), bMapped);
    ResultObj->SetBoolField(TEXT("read_only"), true);
    ResultObj->SetBoolField(TEXT("runtime_only"), true);
    ResultObj->SetBoolField(TEXT("asset_modified"), false);
    ResultObj->SetBoolField(TEXT("saves_assets"), false);
    ResultObj->SetBoolField(TEXT("dry_run"), false);
    ResultObj->SetStringField(TEXT("mode"), TEXT("compiled_graph_mapping"));
    ResultObj->SetStringField(TEXT("comparison_kind"), TEXT("compiled_graph_node_mapping"));
    ResultObj->SetBoolField(TEXT("runtime_graph_prepost"), false);
    ResultObj->SetBoolField(TEXT("same_instance_prepost"), false);
    ResultObj->SetBoolField(TEXT("same_anim_instance_node_mapping"), bMapped);
    ResultObj->SetBoolField(TEXT("runtime_node_instance_mapped"), bRuntimeNodeMapped);
    ResultObj->SetBoolField(TEXT("find_debug_anim_node_mapped"), bFindDebugMapped);
    ResultObj->SetBoolField(TEXT("runtime_pose_links_available"), bRuntimePoseLinksAvailable);
    ResultObj->SetBoolField(TEXT("instrumentation_preflight"), true);
    ResultObj->SetBoolField(TEXT("original_assets_modified"), false);
    ResultObj->SetBoolField(TEXT("temp_assets_created"), false);
    ResultObj->SetStringField(TEXT("runtime_note"), TEXT("Maps the selected editor AnimGraph node GUID to the compiled/live FAnimNode instance on the matched AnimInstance. This proves a same-instance node address can be found, but it does not sample node input/output pose data."));
    ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
    ResultObj->SetStringField(TEXT("blueprint_path"), CurrentWork->SourcePath);
    ResultObj->SetObjectField(TEXT("target_node"), TargetNodeObject);
    ResultObj->SetObjectField(TEXT("runtime_node_mapping"), MappingObject);
    ResultObj->SetObjectField(TEXT("runtime_pose_links"), RuntimePoseLinksObject);
    ResultObj->SetObjectField(TEXT("mapping_support"), MappingSupportObject);
    PopulateAnimInstanceRuntimeTargetFields(ResultObj, Target, bPreferPIEWorld, bRequirePIEWorld);
    AddGraphField(ResultObj, Blueprint, TargetGraph);
    ResultObj->SetArrayField(TEXT("warnings"), WarningValues);
    return ResultObj;
}
bool ContextParams(const FString& Text,FWork& W)
{
    TSharedPtr<FJsonValue> Value;FString Error;if(!Decode(Text,Value,Error)||Value->Type!=EJson::Object){return W.Fail(TEXT("private_context_invalid"),Error.IsEmpty()?TEXT("Private context must be an object"):Error);}auto Context=Value->AsObject();
    const TSet<FString> Allowed={TEXT("anim_blueprint_path"),TEXT("allow_missing_samples"),TEXT("graph_selector"),TEXT("input_pose_index"),TEXT("ignored_driver_keys")};
    for(const auto& Pair:Context->Values){if(!Allowed.Contains(FString(Pair.Key.ToView()))){return W.Fail(TEXT("private_context_key"),TEXT("Unknown private context key"));}}
    for(const TCHAR* Key:{TEXT("anim_blueprint_path"),TEXT("allow_missing_samples"),TEXT("input_pose_index")}){if(Context->HasField(Key)){W.Params->SetField(Key,Context->TryGetField(Key));}}
    if(Context->HasField(TEXT("graph_selector")))
    {
        const auto Raw=Context->TryGetField(TEXT("graph_selector"));if(!Raw||Raw->Type!=EJson::Object){return W.Fail(TEXT("graph_context_invalid"),TEXT("graph_selector must be an object projection"));}const auto Selector=Raw->AsObject();for(const auto& Pair:Selector->Values){const FString Key(Pair.Key.ToView());if(Key!=TEXT("graph_name")&&Key!=TEXT("graph_id")&&Key!=TEXT("graph_type")){return W.Fail(TEXT("graph_context_key"),TEXT("Unknown graph selector key"));}}
        for(const TCHAR* Key:{TEXT("graph_name"),TEXT("graph_id"),TEXT("graph_type")}){W.Params->RemoveField(Key);if(Selector->HasField(Key)){W.Params->SetField(Key,Selector->TryGetField(Key));}}
    }
    if(Context->HasField(TEXT("ignored_driver_keys")))
    {
        const TArray<TSharedPtr<FJsonValue>>* Keys=nullptr;if(!Context->TryGetArrayField(TEXT("ignored_driver_keys"),Keys)||Keys->Num()>3){return W.Fail(TEXT("ignored_keys_invalid"),TEXT("ignored_driver_keys must contain only three confirmed key names"));}TSet<FString> Seen;
        for(const auto& Row:*Keys){FString Key;if(!Row||!Row->TryGetString(Key)||(Key!=TEXT("properties")&&Key!=TEXT("property_name")&&Key!=TEXT("curve_values"))||Seen.Contains(Key)){return W.Fail(TEXT("ignored_keys_invalid"),TEXT("Unknown or duplicate ignored driver key"));}Seen.Add(Key);W.Params->SetField(Key,MakeShared<FJsonValueNull>());}
    }
    if(!W.Params->HasField(TEXT("graph_name"))&&!W.Params->HasField(TEXT("graph_id"))&&!W.Params->HasField(TEXT("graph_type"))){W.Params->SetStringField(TEXT("graph_name"),TEXT("AnimGraph"));W.Params->SetStringField(TEXT("graph_type"),TEXT("function"));}return true;
}
bool ResolveTarget(FWork& W,bool IncludePins,int32 Depth)
{
    FString Name=GetStringParam(W.Params,TEXT("blueprint_name"));if(Name.IsEmpty()){Name=GetStringParam(W.Params,TEXT("anim_blueprint"));}if(Name.IsEmpty()){Name=GetStringParam(W.Params,TEXT("anim_blueprint_path"));}if(Name.IsEmpty()){return W.Fail(TEXT("blueprint_missing"),TEXT("Missing blueprint_name or anim_blueprint parameter"));}
    W.SelectedBlueprintName=Name;W.Result.Stage=TEXT("resolve_anim_blueprint");W.Blueprint=LoadBlueprint(Name,W);if(!W.Basic()||!Live(W.Blueprint)||!W.Hold(W.Blueprint)){return W.Fail(TEXT("blueprint_not_found"),TEXT("Blueprint not found: ")+Name);}W.OriginalGeneratedClass=W.Blueprint->GeneratedClass;W.SourcePath=W.Blueprint->GetPathName();W.SourcePackage=W.Blueprint->GetPackage();W.SourceDirtyBefore=W.SourcePackage->IsDirty();W.SourceDirtyCaptured=true;
    W.Result.Stage=TEXT("resolve_anim_graph");UEdGraph* G=ResolveGraph(W.Blueprint,W.Params,W);if(!G||!W.CaptureGraph(G)){return false;}const UEdGraphSchema* Schema=G->GetSchema();if(!W.Current()||!Live(Schema)||!W.Hold(const_cast<UEdGraphSchema*>(Schema))||!Schema->IsA<UAnimationGraphSchema>()){return W.Fail(TEXT("not_animation_graph"),TEXT("Target graph is not an AnimationGraph"));}
    const FString Id=GetStringParam(W.Params,TEXT("node_id")),Type=GetStringParam(W.Params,TEXT("node_type")),TitleContains=GetStringParam(W.Params,TEXT("title_contains"));TArray<UAnimGraphNode_Base*> Candidates;TArray<TSharedPtr<FJsonValue>> Rows;bool NonAnim=false;
    for(UEdGraphNode* N:W.Nodes)
    {
        if(!N){continue;}if(!W.Current()){return false;}if(!Id.IsEmpty()&&!N->NodeGuid.ToString().Equals(Id,ESearchCase::IgnoreCase)){continue;}auto* A=Cast<UAnimGraphNode_Base>(N);if(!A){NonAnim|=!Id.IsEmpty();continue;}
        if(!Type.IsEmpty()||!TitleContains.IsEmpty()){W.SDK(TEXT("node_filter_title_calls"));const FString Title=A->GetNodeTitle(ENodeTitleType::FullTitle).ToString();if(!W.Current()||!SafeText(Title,65536)){return W.Fail(TEXT("title_callback"),TEXT("Node title callback changed source or exceeded bound"));}if((!Type.IsEmpty()&&!A->GetClass()->GetName().Contains(Type)&&!Title.Contains(Type))||(!TitleContains.IsEmpty()&&!Title.Contains(TitleContains))){continue;}}
        Candidates.Add(A);auto Row=NodeJson(A,false,W);Row->SetStringField(TEXT("mvp_kind"),GetAnimNodePrePostMVPKind(A));Row->SetBoolField(TEXT("isolated_sampler_mvp_supported"),Cast<UAnimGraphNode_RigidBody>(A)||Cast<UAnimGraphNode_Trail>(A));Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    if(Candidates.Num()!=1){W.Legacy->SetStringField(TEXT("blueprint_name"),Name);W.Legacy->SetStringField(TEXT("blueprint_path"),W.SourcePath);W.Legacy->SetObjectField(TEXT("graph"),GraphJson(W.Blueprint,G));W.Legacy->SetArrayField(TEXT("candidates"),Rows);return W.Fail(TEXT("node_selector"),NonAnim?TEXT("node_id matched a non-AnimGraph node"):FString::Printf(TEXT("AnimGraph selector matched %d nodes; provide one node_id"),Candidates.Num()));}
    W.Node=Candidates[0];W.Result.Stage=TEXT("serialize_anim_node_target");auto J=NodeJson(W.Node,IncludePins,W);W.TargetJson->Values=J->Values;W.TargetJson->SetStringField(TEXT("mvp_kind"),GetAnimNodePrePostMVPKind(W.Node));W.TargetJson->SetBoolField(TEXT("isolated_sampler_mvp_supported"),Cast<UAnimGraphNode_RigidBody>(W.Node)||Cast<UAnimGraphNode_Trail>(W.Node));W.TargetJson->SetStringField(TEXT("support_note"),GetAnimNodePrePostSupportNote(W.Node));W.TargetJson->SetObjectField(TEXT("settings"),SettingsJson(W.Node,Depth,W));UEdGraphPin* Input=PosePin(W.Node,EGPD_Input,W);UEdGraphPin* Output=PosePin(W.Node,EGPD_Output,W);W.TargetJson->SetObjectField(TEXT("preferred_input_pose_pin"),Input?PinJson(Input,W):MakeShared<FJsonObject>());W.TargetJson->SetObjectField(TEXT("preferred_output_pose_pin"),Output?PinJson(Output,W):MakeShared<FJsonObject>());
    for(bool Up:{true,false}){TArray<TSharedPtr<FJsonValue>> Links;UEdGraphPin* Pin=Up?Input:Output;if(Pin&&W.Pin(Pin)){const auto Linked=Pin->LinkedTo;for(UEdGraphPin* L:Linked){if(!W.Pin(L)){return false;}Links.Add(MakeShared<FJsonValueObject>(PinJson(L,W)));}}W.TargetJson->SetArrayField(Up?TEXT("upstream_pose_links"):TEXT("downstream_pose_links"),Links);}
    W.TargetJson->SetObjectField(TEXT("compiled_node_mapping"),AnimGraphCompiledNodeMappingToJson(W.Blueprint,W.Node,nullptr,nullptr,nullptr,nullptr));W.TargetJson->SetObjectField(TEXT("mapping_support"),BuildAnimNodeRuntimeMappingSupportJson(W.Node,nullptr,nullptr,false,false));return W.Current();
}
}
Variable::FResult Sample(const FString& BlueprintName,const FString& AnimBlueprint,const FString& GraphName,const FString& GraphId,const FString& GraphType,const FString& NodeId,const FString& NodeType,const FString& TitleContains,const FString& SampleBonesJson,const FString& SampleSocketsJson,const FString& Mode,const FString& SkeletalMesh,const FString& TempRoot,bool bCleanup,bool bOverwriteExistingTempAssets,const FString& RunId,const FString& ActorLabel,const FString& ActorName,const FString& ActorPath,const FString& ComponentName,const FString& AnimInstanceSource,bool bPreferPieWorld,bool bRequirePieWorld,int64 TickCount,double TickDeltaTime,int64 SettleTickCount,bool bRefreshBoneTransforms,bool bAllowMissingBones,int64 PoseLinkMaxDepth,const FString& InputPoseMode,const FString& InputPoseFieldPath,int64 InputPoseIndex,bool bIncludePins,int64 MaxDepth,bool bDryRun,const FString& PrivateContextJson)
{
    FWork W;if(!IsEditorGameThread()||IsEngineExitRequested()||Running){W.Fail(TEXT("editor_boundary"),TEXT("Requires a nonreentrant live Editor game thread; PIE/SIE is permitted for runtime modes"));return W.Finish();}TGuardValue<bool> Guard(Running,true);CurrentWork=&W;ON_SCOPE_EXIT{CurrentWork=nullptr;};
    for(const FString* Text:{&BlueprintName,&AnimBlueprint,&GraphName,&GraphId,&GraphType,&NodeId,&NodeType,&TitleContains,&Mode,&SkeletalMesh,&TempRoot,&RunId,&ActorLabel,&ActorName,&ActorPath,&ComponentName,&AnimInstanceSource,&InputPoseMode,&InputPoseFieldPath}){if(!SafeText(*Text)){W.Fail(TEXT("text_bound"),TEXT("Ordinary text exceeds1Mi UTF16 units or contains NUL"));return W.Finish();}}
    if(!FMath::IsFinite(TickDeltaTime)){W.Fail(TEXT("nonfinite_delta"),TEXT("tick_delta_time must be finite"));return W.Finish();}auto P=W.Params;
    const TArray<TPair<FString,const FString*>> Texts={{TEXT("blueprint_name"),&BlueprintName},{TEXT("anim_blueprint"),&AnimBlueprint},{TEXT("node_id"),&NodeId},{TEXT("node_type"),&NodeType},{TEXT("title_contains"),&TitleContains},{TEXT("skeletal_mesh"),&SkeletalMesh},{TEXT("run_id"),&RunId},{TEXT("actor_label"),&ActorLabel},{TEXT("actor_name"),&ActorName},{TEXT("actor_path"),&ActorPath},{TEXT("component_name"),&ComponentName},{TEXT("input_pose_field_path"),&InputPoseFieldPath},{TEXT("graph_id"),&GraphId}};
    for(const auto& Pair:Texts){if(!Pair.Value->IsEmpty()){P->SetStringField(Pair.Key,*Pair.Value);}}
    P->SetStringField(TEXT("graph_name"),GraphName);P->SetStringField(TEXT("graph_type"),GraphType);P->SetStringField(TEXT("mode"),Mode);P->SetStringField(TEXT("temp_root"),TempRoot);P->SetStringField(TEXT("anim_instance_source"),AnimInstanceSource);P->SetStringField(TEXT("input_pose_mode"),InputPoseMode);
    const TArray<TPair<FString,bool>> Flags={{TEXT("cleanup"),bCleanup},{TEXT("overwrite_existing_temp_assets"),bOverwriteExistingTempAssets},{TEXT("prefer_pie_world"),bPreferPieWorld},{TEXT("require_pie_world"),bRequirePieWorld},{TEXT("refresh_bone_transforms"),bRefreshBoneTransforms},{TEXT("allow_missing_bones"),bAllowMissingBones},{TEXT("include_pins"),bIncludePins},{TEXT("dry_run"),bDryRun}};for(const auto& F:Flags){P->SetBoolField(F.Key,F.Value);}
    P->SetNumberField(TEXT("tick_count"),static_cast<int32>(FMath::Clamp<int64>(TickCount,0,240)));P->SetNumberField(TEXT("settle_tick_count"),static_cast<int32>(FMath::Clamp<int64>(SettleTickCount,0,240)));P->SetNumberField(TEXT("pose_link_max_depth"),static_cast<int32>(FMath::Clamp<int64>(PoseLinkMaxDepth,0,8)));P->SetNumberField(TEXT("max_depth"),static_cast<int32>(FMath::Clamp<int64>(MaxDepth,1,8)));P->SetNumberField(TEXT("tick_delta_time"),FMath::Clamp(TickDeltaTime,0.0,1.0));if(InputPoseIndex>=0){P->SetStringField(TEXT("input_pose_index_decimal"),LexToString(InputPoseIndex));if(InputPoseIndex<=MAX_int32){P->SetNumberField(TEXT("input_pose_index"),static_cast<int32>(InputPoseIndex));}else{P->SetNumberField(TEXT("input_pose_index"),static_cast<double>(MAX_int32));}}
    auto Limits=MakeShared<FJsonObject>();Limits->SetStringField(TEXT("tick_count"),LexToString(TickCount));Limits->SetStringField(TEXT("settle_tick_count"),LexToString(SettleTickCount));Limits->SetStringField(TEXT("pose_link_max_depth"),LexToString(PoseLinkMaxDepth));Limits->SetStringField(TEXT("input_pose_index"),LexToString(InputPoseIndex));Limits->SetStringField(TEXT("max_depth"),LexToString(MaxDepth));W.Result.Data->SetObjectField(TEXT("received_numeric_int64_decimal"),Limits);
    if(!ContextParams(PrivateContextJson,W)){return W.Finish();}TSharedPtr<FJsonValue> BonesValue,SocketsValue;FString Error;if(!Decode(SampleBonesJson,BonesValue,Error)||!Decode(SampleSocketsJson,SocketsValue,Error)){W.Fail(TEXT("samples_json_invalid"),Error);return W.Finish();}P->SetField(TEXT("sample_bones"),BonesValue);P->SetField(TEXT("sample_sockets"),SocketsValue);
    TArray<FString> Bones,Sockets;if(!ParseNames(SampleBonesJson,bDryRun?TArray<FString>():GetDefaultAnimNodePrePostSampleBones(),Bones,Error)||!ParseNames(SampleSocketsJson,{},Sockets,Error)){W.Fail(TEXT("sample_names_invalid"),Error);return W.Finish();}
    if(!ResolveTarget(W,bIncludePins,static_cast<int32>(FMath::Clamp<int64>(MaxDepth,1,8)))){return W.Finish();}TSharedPtr<FJsonObject> Response;
    if(bDryRun)
    {
        Response=MakeShared<FJsonObject>();Response->SetBoolField(TEXT("success"),true);Response->SetBoolField(TEXT("read_only"),true);Response->SetBoolField(TEXT("asset_modified"),false);Response->SetBoolField(TEXT("runtime_only"),false);Response->SetBoolField(TEXT("dry_run"),true);Response->SetStringField(TEXT("mode"),TEXT("dry_run_target_resolver"));Response->SetBoolField(TEXT("runtime_graph_prepost"),false);Response->SetBoolField(TEXT("same_instance_prepost"),false);Response->SetBoolField(TEXT("original_assets_modified"),false);Response->SetBoolField(TEXT("temp_assets_created"),false);Response->SetStringField(TEXT("next_implementation_mode"),TEXT("pose_watch_capture"));Response->SetStringField(TEXT("runtime_note"),TEXT("Resolves one AnimGraph node and reports feasibility; no explicit tick or pose capture. Legacy resolver can create an EventGraph only in idle Editor."));Response->SetStringField(TEXT("blueprint_name"),W.SelectedBlueprintName);Response->SetStringField(TEXT("blueprint_path"),W.SourcePath);Response->SetNumberField(TEXT("max_depth"),FMath::Clamp<int64>(MaxDepth,1,8));Response->SetObjectField(TEXT("target_node"),W.TargetJson);Response->SetArrayField(TEXT("available_runtime_modes"),Strings({TEXT("compiled_graph_mapping"),TEXT("pose_watch_capture"),TEXT("active_component_tick_delta"),TEXT("isolated_temp_components")}));Response->SetArrayField(TEXT("requested_sample_bones"),Strings(Bones));Response->SetArrayField(TEXT("requested_sample_sockets"),Strings(Sockets));AddGraphField(Response,W.Blueprint,W.Graph);
    }
    else if(Mode.Equals(TEXT("compiled_graph_mapping"),ESearchCase::IgnoreCase)||Mode.Equals(TEXT("runtime_node_mapping"),ESearchCase::IgnoreCase)||Mode.Equals(TEXT("compiled_runtime_mapping"),ESearchCase::IgnoreCase)){Response=BuildAnimNodePrePostCompiledGraphMappingResponse(P,W.SelectedBlueprintName,W.Blueprint,W.Graph,W.Node,W.TargetJson);}
    else if(Mode.Equals(TEXT("pose_watch_capture"),ESearchCase::IgnoreCase)||Mode.Equals(TEXT("runtime_pose_watch"),ESearchCase::IgnoreCase)||Mode.Equals(TEXT("compiled_graph_pose_watch"),ESearchCase::IgnoreCase)){Response=PoseWatchMode(P,W.SelectedBlueprintName,W.Blueprint,W.Graph,W.Node,W.TargetJson,Bones,Sockets);}
    else if(Mode.Equals(TEXT("isolated_temp_components"),ESearchCase::IgnoreCase)){Response=IsolatedMode(P,W.SelectedBlueprintName,W.Blueprint,W.Graph,W.Node,P,W.TargetJson,Bones,Sockets,Cast<UAnimGraphNode_RigidBody>(W.Node)||Cast<UAnimGraphNode_Trail>(W.Node));}
    else{Response=BuildAnimNodePrePostRuntimeTickDeltaResponse(P,W.SelectedBlueprintName,W.Blueprint,W.Graph,W.Node,W.TargetJson,Bones,Sockets,Cast<UAnimGraphNode_RigidBody>(W.Node)||Cast<UAnimGraphNode_Trail>(W.Node));}
    if(Response){W.Legacy->Values=Response->Values;}const bool LegacyOK=GetJsonBoolFieldOrDefault(Response,TEXT("success"),false);W.Result.Data->SetBoolField(TEXT("legacy_success"),LegacyOK);W.Result.bSuccess=LegacyOK&&W.Result.Error.IsEmpty();W.Result.bPostconditionPassed=W.Result.bSuccess;if(!W.Result.bSuccess&&W.Result.Error.IsEmpty()){W.Fail(TEXT("mode_failed"),GetStringParam(Response,TEXT("error"),TEXT("Requested mode did not meet its postconditions; inspect legacy result and partial receipts")));}W.Result.Stage=W.Result.bSuccess?TEXT("completed_anim_node_pre_post_runtime_pose"):W.Result.Stage;return W.Finish();
}
}
