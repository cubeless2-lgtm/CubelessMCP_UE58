#include "Compatibility/UE58/CubelessSkeletalBoneSamplingApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
namespace Cubeless::UE58::SkeletalBoneSampling
{
namespace
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
Variable::FResult Sample(const FString& ActorLabel,const FString& ActorName,const FString& ActorPath,const FString& ComponentName,const FString& BonesJson,const FString& SocketsJson,bool bPreferPIEWorld,bool bRequirePIEWorld)
{
    FRead Read;
    if(!IsEditorGameThread()||IsEngineExitRequested()||Running){Read.Fail(TEXT("editor_boundary"),TEXT("Nonreentrant Editor game thread required; PIE/SIE reads are supported"));return Read.Finish();}
    TGuardValue<bool> Guard(Running,true);
    for(const FString* Text:{&ActorLabel,&ActorName,&ActorPath,&ComponentName})
    {if(!SafeText(*Text)){Read.Fail(TEXT("input_bound"),TEXT("Actor/component filter exceeds1Mi UTF16 or contains NUL"));return Read.Finish();}}
    TArray<FString> Bones,Sockets;FString Error;
    if(!ParseNames(BonesJson,{TEXT("root"),TEXT("pelvis"),TEXT("spine_03"),TEXT("head"),TEXT("foot_l"),TEXT("foot_r")},Bones,Error)||!ParseNames(SocketsJson,{},Sockets,Error))
    {Read.Fail(TEXT("names_invalid"),Error);return Read.Finish();}
    auto Strings=[&](const TArray<FString>& Values,TArray<TSharedPtr<FJsonValue>>& Json)
    {for(const FString& Value:Values){if(Value.Len()>MaxCarrier-Read.OutputChars){return Read.Fail(TEXT("output_bound"),TEXT("Requested names exceed output budget"));}Read.OutputChars+=Value.Len();Json.Add(MakeShared<FJsonValueString>(Value));}return true;};
    TArray<TSharedPtr<FJsonValue>> BoneNames,SocketNames;if(!Strings(Bones,BoneNames)||!Strings(Sockets,SocketNames)){return Read.Finish();}
    Read.Legacy->SetArrayField(TEXT("requested_bones"),BoneNames);Read.Legacy->SetArrayField(TEXT("requested_sockets"),SocketNames);
    Read.Legacy->SetBoolField(TEXT("prefer_pie_world"),bPreferPIEWorld);Read.Legacy->SetBoolField(TEXT("require_pie_world"),bRequirePIEWorld);
    if(!Read.Text(Read.Legacy,TEXT("requested_actor_label"),ActorLabel)||!Read.Text(Read.Legacy,TEXT("requested_actor_name"),ActorName)||!Read.Text(Read.Legacy,TEXT("requested_actor_path"),ActorPath)||!Read.Text(Read.Legacy,TEXT("requested_component_name"),ComponentName)){return Read.Finish();}
    Read.Result.Stage=TEXT("locate_runtime_component");
    if(!Worlds(Read,bPreferPIEWorld,bRequirePIEWorld)||!FindTarget(Read,ActorLabel,ActorName,ActorPath,ComponentName,bRequirePIEWorld)){return Read.Finish();}
    Read.Result.Stage=TEXT("prepare_pose_observation");if(!PreparePoseSources(Read)){return Read.Finish();}
    Read.Result.Stage=TEXT("sample_bones");for(const FString& Name:Bones){if(!SampleName(Read,Name,false)){return Read.Finish();}}
    Read.Result.Stage=TEXT("sample_sockets");for(const FString& Name:Sockets){if(!SampleName(Read,Name,true)){return Read.Finish();}}
    Read.Result.Stage=TEXT("sample_metadata");if(!Metadata(Read,ComponentName)){return Read.Finish();}
    if(!Read.Current()){return Read.Finish();}
    Read.Result.bSuccess=Read.Result.bPostconditionPassed=true;Read.Result.Stage=TEXT("complete");return Read.Finish();
}
}
