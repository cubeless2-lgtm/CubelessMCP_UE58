#include "Compatibility/UE58/CubelessAnimInstanceRuntimeStateApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "AlphaBlend.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNode_StateMachine.h"
#include "Animation/AnimStateMachineTypes.h"
#include "Animation/AnimTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Editor.h"
#include "Dom/JsonValue.h"
#include "Misc/CoreMisc.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::AnimInstanceRuntimeState
{
namespace
{
using Variable::FResult;
constexpr int32 MaxChars=1048576,MaxRows=65536,MaxDepth=64,MaxOutputChars=8388608;
constexpr int64 MaxChecks=16777216;
bool InFlight=false;
bool Loaded(const UObject* O)
{return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
FResult Fail(FResult R,const TCHAR* Code,const FString& Error)
{R.bSuccess=false;R.bPostconditionPassed=false;R.ErrorCode=Code;R.Error=Error;R.Data->SetBoolField(TEXT("success"),false);return R;}
void Call(FResult& R,const TCHAR* Key)
{auto J=R.Data->GetObjectField(TEXT("native_call_counts"));double N=0;J->TryGetNumberField(Key,N);J->SetNumberField(Key,N+1);}
bool Bounded(const FString& S,int32 Limit=MaxChars)
{if(S.Len()>Limit){return false;}for(TCHAR C:S){if(C==0){return false;}}return true;}
bool CurvesInput(const FString& Text,TArray<FString>& Names,FString& Error)
{
    if(!Bounded(Text)){Error=TEXT("Curve names carrier exceeds finite UTF16/NUL bounds");return false;}
    int32 Depth=0;bool Quoted=false,Escaped=false;
    for(TCHAR C:Text)
    {
        if(Quoted){if(Escaped){Escaped=false;}else if(C==TCHAR('\\')){Escaped=true;}else if(C==TCHAR('"')){Quoted=false;}continue;}
        if(C==TCHAR('"')){Quoted=true;}else if(C==TCHAR('[')||C==TCHAR('{')){if(++Depth>MaxDepth){return false;}}
        else if(C==TCHAR(']')||C==TCHAR('}')){if(--Depth<0){return false;}}
    }
    if(Quoted||Depth!=0){Error=TEXT("Malformed or over-depth curve carrier");return false;}
    TArray<TSharedPtr<FJsonValue>> Outer;auto Reader=TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]"));
    if(!FJsonSerializer::Deserialize(Reader,Outer,FJsonSerializer::EFlags::None)||Outer.Num()!=1||!Outer[0].IsValid())
    {Error=TEXT("Curve names carrier is not one JSON value");return false;}
    const auto V=Outer[0];if(V->Type==EJson::Null){return true;}
    if(V->Type!=EJson::Array||V->AsArray().Num()>MaxRows){Error=TEXT("curve_names requires a bounded nullable array");return false;}
    for(const auto& Item:V->AsArray())
    {
        FString Name;
        if(Item.IsValid()&&Item->Type==EJson::String){Name=Item->AsString();}
        else if(Item.IsValid()&&Item->Type==EJson::Object)
        {
            // Preserve hidden native GetStringArrayParam descriptor behavior. The public
            // Toolset remains nullable List[str]; descriptor transport is not advertised.
            const auto Object=Item->AsObject();FString Type;
            if(!Object->TryGetStringField(TEXT("name"),Name)){continue;}
            if(Object->TryGetStringField(TEXT("type"),Type)&&!Type.IsEmpty()){Name=Type+TEXT(":")+Name;}
        }
        else{continue;}
        if(!Bounded(Name,NAME_SIZE-1)){Error=TEXT("Curve name exceeds the SDK FName/NUL bound");return false;}
        Names.Add(Name);
    }
    return true;
}
FString WorldType(UWorld* W)
{
    if(!W){return TEXT("None");}
    switch(W->WorldType){case EWorldType::PIE:return TEXT("PIE");case EWorldType::Game:return TEXT("Game");case EWorldType::GamePreview:return TEXT("GamePreview");case EWorldType::Editor:return TEXT("Editor");case EWorldType::EditorPreview:return TEXT("EditorPreview");case EWorldType::Inactive:return TEXT("Inactive");default:return TEXT("Unknown");}
}
bool PlayWorld(UWorld* W)
{return W&&(W->WorldType==EWorldType::PIE||W->WorldType==EWorldType::Game||W->WorldType==EWorldType::GamePreview);}
TArray<UWorld*> Worlds(bool Prefer,bool Require)
{
    TArray<UWorld*> Out;auto Games=[&](){if(GEngine){for(const FWorldContext& C:GEngine->GetWorldContexts()){UWorld* W=C.World();if(PlayWorld(W)){Out.AddUnique(W);}}}};
    if(Prefer){Games();}if(!Require&&GEditor){if(UWorld* W=GEditor->GetEditorWorldContext().World()){Out.AddUnique(W);}}if(!Prefer){Games();}return Out;
}
struct FRead
{
    struct FIdentity{UObject* Object;UClass* Class;UObject* Outer;UPackage* Package;FString Path,PackagePath;};
    explicit FRead(FResult& In):R(In){}
    FResult& R;TArray<TStrongObjectPtr<UObject>> Held;TArray<FIdentity> Identities;TSet<UObject*> Seen;
    UWorld* World=nullptr;EWorldType::Type Type=EWorldType::None;AActor* Actor=nullptr;USkeletalMeshComponent* Component=nullptr;UAnimInstance* Anim=nullptr;
    FString Error;int64 Checks=0;int32 OutputChars=0,Rows=0;
    bool Bad(const FString& E){if(Error.IsEmpty()){Error=E;}R.Data->SetBoolField(TEXT("observation_incomplete"),true);return false;}
    bool Step(){return ++Checks<=MaxChecks?true:Bad(TEXT("Finite runtime observation check budget reached"));}
    bool Hold(UObject* O)
    {
        if(!Loaded(O)||!Loaded(O->GetClass())||O->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists)){return Bad(TEXT("Runtime source object/class is unreadable or reinstanced"));}
        if(!Seen.Contains(O))
        {
            if(Seen.Num()>=MaxRows){return Bad(TEXT("Runtime source object inventory exceeds finite rows"));}
            Seen.Add(O);Held.Emplace(O);UPackage* P=O->GetPackage();if(!Loaded(P)){return Bad(TEXT("Runtime source actual package is unreadable"));}
            Identities.Add({O,O->GetClass(),O->GetOuter(),P,O->GetPathName(),P->GetPathName()});
            if(O->GetClass()!=O&&!Hold(O->GetClass())){return false;}if(P!=O&&!Hold(P)){return false;}
        }
        return true;
    }
    bool Raw()
    {
        if(!Error.IsEmpty()){return false;}
        if(!Step()||!IsEditorGameThread()||IsEngineExitRequested()){return Bad(TEXT("Runtime observation requires a live Editor game thread"));}
        for(const auto& I:Identities)
        {
            if(!Step()){return false;}
            if(!Loaded(I.Object)||!Loaded(I.Class)||!Loaded(I.Package)||I.Class->HasAnyClassFlags(CLASS_NewerVersionExists)||I.Object->GetClass()!=I.Class
                ||I.Object->GetOuter()!=I.Outer||I.Object->GetPackage()!=I.Package||I.Object->GetPathName()!=I.Path||I.Package->GetPathName()!=I.PackagePath)
            {return Bad(TEXT("Runtime object/class/owner/path/actual package changed after callback"));}
        }
        return true;
    }
    bool Current()
    {
        if(!Raw()){return false;}
        if(World)
        {
            bool Present=false;if(GEngine){for(const auto& C:GEngine->GetWorldContexts()){Present|=C.World()==World;}}
            if(GEditor){Present|=GEditor->GetEditorWorldContext().World()==World;}
            if(!Present||World->WorldType!=Type){return Bad(TEXT("Sampled world/context/type is no longer current"));}
        }
        if(Actor&&Actor->GetWorld()!=World){return Bad(TEXT("Actor left the sampled world"));}if(!Raw()){return false;}
        if(Component)
        {
            TArray<USkeletalMeshComponent*> Components;Actor->GetComponents<USkeletalMeshComponent>(Components);
            if(!Raw()||Component->GetOwner()!=Actor||!Components.Contains(Component)){return Bad(TEXT("Skeletal component owner/membership changed"));}
            if(Component->IsRunningParallelEvaluation()){return Bad(TEXT("Parallel animation evaluation is active; no wait/tick/update was dispatched"));}
        }
        if(Anim)
        {
            if(Component->GetAnimInstance()!=Anim||Anim->GetOuter()!=Component||!Raw()){return Bad(TEXT("Component AnimInstance identity/owning component changed"));}
            if(!Anim->IsInitialized()){return Bad(TEXT("AnimInstance is not initialized; observation does not initialize it"));}
            if(Anim->IsRunningParallelEvaluation()){return Bad(TEXT("AnimInstance parallel evaluation is active; no blocking completion requested"));}
        }
        return Raw();
    }
    bool Text(const TSharedRef<FJsonObject>& J,const TCHAR* Key,const FString& S)
    {if(!Bounded(S)||S.Len()>MaxOutputChars-OutputChars){return Bad(TEXT("Runtime output text budget reached"));}OutputChars+=S.Len();J->SetStringField(Key,S);return true;}
    bool Number(const TSharedRef<FJsonObject>& J,const TCHAR* Key,double V)
    {if(!FMath::IsFinite(V)){return Bad(TEXT("Runtime getter returned a non-finite JSON number"));}J->SetNumberField(Key,V);return true;}
    bool Row(){return ++Rows<=MaxRows?true:Bad(TEXT("Runtime output row budget reached; partial prefix retained"));}
};
struct FMetadata
{
    struct FField{FStructProperty* Property;FName Name;UStruct* Owner;UScriptStruct* Type;};
    FRead& C;const IAnimClassInterface* Interface=nullptr;UClass* Class=nullptr;TArray<FField> Fields;TArray<FBakedAnimationStateMachine> Baked;
    explicit FMetadata(FRead& In):C(In){}
    bool Capture()
    {
        Class=C.Anim->GetClass();if(!C.Hold(Class)||!C.Current()){return false;}Interface=IAnimClassInterface::GetFromClass(Class);
        if(!Interface){return true;}const TArray<FStructProperty*>& Properties=Interface->GetAnimNodeProperties();
        if(!C.Current()||Properties.Num()>MaxRows){return C.Bad(TEXT("Animation node property source exceeds finite bounds"));}
        const TArray<FStructProperty*> Copy=Properties;
        for(FStructProperty* P:Copy)
        {
            if(!P){return C.Bad(TEXT("Animation node property source contains null"));}
            const FName Name=P->GetFName();UStruct* Owner=P->GetOwnerStruct();UScriptStruct* Type=P->Struct;
            if(!C.Hold(Owner)||!C.Hold(Type)||FindFProperty<FStructProperty>(Class,Name)!=P){return C.Bad(TEXT("Animation node reflected property ownership/type changed"));}
            Fields.Add({P,Name,Owner,Type});
        }
        const TArray<FBakedAnimationStateMachine>& Source=Interface->GetBakedStateMachines();
        if(!C.Current()||Source.Num()>MaxRows){return C.Bad(TEXT("Baked machine source exceeds finite bounds"));}
        int64 Total=Source.Num();for(const auto& M:Source)
        {Total+=M.States.Num()+M.Transitions.Num();for(const auto& S:M.States){Total+=S.PlayerNodeIndices.Num()+S.LayerNodeIndices.Num()+S.Transitions.Num();if(Total>MaxRows){return C.Bad(TEXT("Baked state/player/transition source exceeds finite rows"));}}if(Total>MaxRows){return C.Bad(TEXT("Baked states/transitions exceed finite source rows"));}}
        Baked=Source;return Current();
    }
    bool Current()
    {
        if(!C.Current()||C.Anim->GetClass()!=Class){return C.Bad(TEXT("AnimInstance class changed during metadata projection"));}
        if(!Interface){return true;}
        if(IAnimClassInterface::GetFromClass(Class)!=Interface){return C.Bad(TEXT("Anim class interface changed"));}
        const auto& Properties=Interface->GetAnimNodeProperties();if(!C.Current()||Properties.Num()!=Fields.Num()){return C.Bad(TEXT("Animation node property count changed"));}
        for(int32 I=0;I<Fields.Num();++I)
        {
            if(!C.Step()){return false;}
            const auto& F=Fields[I];if(Properties[I]!=F.Property||FindFProperty<FStructProperty>(Class,F.Name)!=F.Property
                ||F.Property->GetOwnerStruct()!=F.Owner||F.Property->Struct!=F.Type){return C.Bad(TEXT("Animation node reflected field identity/type changed"));}
        }
        const auto& Source=Interface->GetBakedStateMachines();if(!C.Current()||Source.Num()!=Baked.Num()){return C.Bad(TEXT("Baked machine count changed after callback"));}
        for(int32 M=0;M<Baked.Num();++M)
        {
            if(!C.Step()){return false;}
            const auto& A=Source[M];const auto& B=Baked[M];
            if(A.MachineName!=B.MachineName||A.InitialState!=B.InitialState||A.States.Num()!=B.States.Num()||A.Transitions.Num()!=B.Transitions.Num()){return C.Bad(TEXT("Baked machine metadata shape changed"));}
            for(int32 I=0;I<B.States.Num();++I){if(!C.Step()){return false;}if(A.States[I].StateName!=B.States[I].StateName||A.States[I].bIsAConduit!=B.States[I].bIsAConduit||A.States[I].bAlwaysResetOnEntry!=B.States[I].bAlwaysResetOnEntry||A.States[I].PlayerNodeIndices!=B.States[I].PlayerNodeIndices||A.States[I].LayerNodeIndices!=B.States[I].LayerNodeIndices||A.States[I].Transitions.Num()!=B.States[I].Transitions.Num()){return C.Bad(TEXT("Baked state/player metadata changed"));}}
            for(int32 I=0;I<B.Transitions.Num();++I){if(!C.Step()){return false;}const auto& X=A.Transitions[I];const auto& Y=B.Transitions[I];if(X.StateName!=Y.StateName||X.PreviousState!=Y.PreviousState||X.NextState!=Y.NextState||X.BlendMode!=Y.BlendMode||X.LogicType!=Y.LogicType){return C.Bad(TEXT("Baked transition metadata changed"));}}
        }
        return C.Current();
    }
    // IsInitialized also covers MontageOnly. A fresh lazy proxy has no recorded
    // weight buffers; use its public update counter before the unsafe array getter.
    // This is an observed-update gate, not a private buffer/initialization-epoch proof.
    bool RecordedWeight(int32 NodeIndex,int32 ClassIndex,int32 StateIndex,const TSharedRef<FJsonObject>& J,const TCHAR* Key)
    {
        if(!Machine(NodeIndex,ClassIndex)||!Baked.IsValidIndex(ClassIndex)
            ||(StateIndex!=INDEX_NONE&&!Baked[ClassIndex].States.IsValidIndex(StateIndex)))
        {return C.Bad(TEXT("Recorded weight target has no current runtime node/baked state mapping"));}
        Call(C.R,TEXT("public_update_counter_calls"));
        const bool HasUpdate=C.Anim->GetUpdateCounter().HasEverBeenUpdated();
        C.R.Data->SetBoolField(TEXT("recorded_weight_proxy_update_observed"),HasUpdate);
        C.R.Data->SetBoolField(TEXT("recorded_weight_private_buffer_extents_verified"),false);
        C.R.Data->SetBoolField(TEXT("recorded_weight_current_initialization_epoch_verified"),false);
        if(!C.Raw()||C.Component->GetAnimInstance()!=C.Anim||!C.Anim->IsInitialized()
            ||C.Component->IsRunningParallelEvaluation()||C.Anim->IsRunningParallelEvaluation()||C.Anim->GetClass()!=Class)
        {return C.Bad(TEXT("Runtime owner/class/readiness changed during update-counter query"));}
        const int32 PropertyIndex=Fields.Num()-1-NodeIndex;
        if(!Fields.IsValidIndex(PropertyIndex)){return C.Bad(TEXT("Recorded weight runtime node index is no longer valid"));}
        const FField& Field=Fields[PropertyIndex];
        if(FindFProperty<FStructProperty>(Class,Field.Name)!=Field.Property||Field.Property->GetOwnerStruct()!=Field.Owner||Field.Property->Struct!=Field.Type)
        {return C.Bad(TEXT("Recorded weight reflected property identity/type changed"));}
        if(!HasUpdate)
        {return C.Bad(TEXT("Recorded machine/state weights require an observed public proxy update; fresh/MontageOnly lazy proxy buffers are unverified, no weight getter/tick/initialize dispatched"));}
        // No virtual world/metadata callback occurs between this gate and the getter.
        // Proxy.Uninitialize/MontageOnly can retain old counter AND old buffers. Full
        // Initialize rebuilds buffers then resets the counter. A retained True is not
        // asserted to prove fresh evaluation or current private extents/cache identity.
        Call(C.R,StateIndex==INDEX_NONE?TEXT("recorded_machine_weight_calls"):TEXT("recorded_state_weight_calls"));
        const double Value=StateIndex==INDEX_NONE?C.Anim->GetInstanceMachineWeight(NodeIndex):C.Anim->GetInstanceStateWeight(NodeIndex,StateIndex);
        return C.Current()&&C.Number(J,Key,Value);
    }
    const FAnimNode_StateMachine* Machine(int32 NodeIndex,int32 Expected=INDEX_NONE)
    {
        if(!Current()||!Fields.IsValidIndex(Fields.Num()-1-NodeIndex)){return nullptr;}
        const FField& F=Fields[Fields.Num()-1-NodeIndex];if(!F.Type->IsChildOf(FAnimNode_StateMachine::StaticStruct())){return nullptr;}
        Call(C.R,TEXT("state_machine_instance_calls"));const FAnimNode_StateMachine* Node=C.Anim->GetStateMachineInstance(NodeIndex);
        // Do not call virtual world/metadata accessors while retaining the raw struct
        // pointer. Validate its reflected storage and non-callback owner state here.
        if(!C.Raw()||C.Component->GetAnimInstance()!=C.Anim||!C.Anim->IsInitialized()
            ||C.Component->IsRunningParallelEvaluation()||C.Anim->IsRunningParallelEvaluation()
            ||FindFProperty<FStructProperty>(Class,F.Name)!=F.Property||F.Property->GetOwnerStruct()!=F.Owner||F.Property->Struct!=F.Type)
        {C.Bad(TEXT("Runtime machine storage/owner changed during proxy getter"));return nullptr;}
        if(Node&&Node!=F.Property->ContainerPtrToValuePtr<FAnimNode_StateMachine>(C.Anim))
        {C.Bad(TEXT("Runtime machine pointer does not match captured reflected storage"));return nullptr;}
        if(Node&&Expected!=INDEX_NONE&&Node->StateMachineIndexInClass!=Expected){C.Bad(TEXT("Runtime machine class mapping changed"));return nullptr;}return Node;
    }
};
TSharedPtr<FJsonValue> Enum(const UEnum* E,int64 Value)
{auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("value"),double(Value));if(E){J->SetStringField(TEXT("name"),E->GetNameStringByValue(Value));J->SetStringField(TEXT("enum_type"),E->GetName());}return MakeShared<FJsonValueObject>(J);}
TSharedRef<FJsonObject> Transform(const FTransform& T)
{
    auto J=MakeShared<FJsonObject>();const FVector P=T.GetLocation(),S=T.GetScale3D();const FQuat Q=T.GetRotation();const FRotator R=Q.Rotator();
    J->SetArrayField(TEXT("translation"),{MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)});
    J->SetArrayField(TEXT("rotation"),{MakeShared<FJsonValueNumber>(Q.X),MakeShared<FJsonValueNumber>(Q.Y),MakeShared<FJsonValueNumber>(Q.Z),MakeShared<FJsonValueNumber>(Q.W)});
    J->SetArrayField(TEXT("rotation_euler"),{MakeShared<FJsonValueNumber>(R.Pitch),MakeShared<FJsonValueNumber>(R.Yaw),MakeShared<FJsonValueNumber>(R.Roll)});
    J->SetArrayField(TEXT("scale"),{MakeShared<FJsonValueNumber>(S.X),MakeShared<FJsonValueNumber>(S.Y),MakeShared<FJsonValueNumber>(S.Z)});return J;
}
FString AnimationMode(EAnimationMode::Type M)
{switch(M){case EAnimationMode::AnimationBlueprint:return TEXT("AnimationBlueprint");case EAnimationMode::AnimationSingleNode:return TEXT("AnimationSingleNode");case EAnimationMode::AnimationCustomMode:return TEXT("AnimationCustomMode");default:return TEXT("Unknown");}}
bool Machines(FRead& C,const FString& Filter,bool States,bool Weights,bool Times,bool Transitions,bool Inactive,
    int32 MaxMachines,int32 MaxProbe,int32 MaxStates,int32 MaxTransitions,TArray<TSharedPtr<FJsonValue>>& Warnings)
{
    auto J=MakeShared<FJsonObject>();C.R.Data->SetObjectField(TEXT("state_machines"),J);J->SetBoolField(TEXT("valid"),false);TArray<TSharedPtr<FJsonValue>> Rows;J->SetArrayField(TEXT("machines"),Rows);
    FMetadata Meta(C);if(!Meta.Capture()){return false;}
    if(!Meta.Interface){J->SetStringField(TEXT("error"),TEXT("AnimInstance class does not implement IAnimClassInterface"));return true;}
    const int32 ProbeLimit=FMath::Min(MaxProbe,Meta.Fields.Num());int32 Matching=0,Included=0,Probed=0,Discovered=0;
    J->SetStringField(TEXT("state_machine_filter"),Filter);J->SetNumberField(TEXT("available_count"),Meta.Baked.Num());J->SetNumberField(TEXT("runtime_anim_node_property_count"),Meta.Fields.Num());
    J->SetNumberField(TEXT("max_state_machine_instances_to_probe"),MaxProbe);J->SetNumberField(TEXT("runtime_probe_limit"),ProbeLimit);
    J->SetBoolField(TEXT("runtime_probe_truncated"),Meta.Fields.Num()>ProbeLimit);J->SetBoolField(TEXT("include_state_weights"),Weights);J->SetBoolField(TEXT("include_relevant_anim_times"),Times);
    J->SetBoolField(TEXT("include_transition_progress"),Transitions);J->SetBoolField(TEXT("include_inactive_transition_progress"),Inactive);
    J->SetStringField(TEXT("runtime_index_note"),TEXT("machine_instance_index/runtime_node_index address reverse AnimNodeProperties; machine_index_in_class maps baked metadata only. Runtime getters receive runtime_node_index, correcting the legacy class-index mismatch."));
    for(int32 NodeIndex=0;NodeIndex<ProbeLimit;++NodeIndex)
    {
        ++Probed;J->SetNumberField(TEXT("runtime_nodes_probed"),Probed);const FAnimNode_StateMachine* Node=Meta.Machine(NodeIndex);if(!C.Error.IsEmpty()){return false;}if(!Node){continue;}++Discovered;J->SetNumberField(TEXT("runtime_machines_discovered"),Discovered);
        const int32 ClassIndex=Node->StateMachineIndexInClass;
        if(!Meta.Baked.IsValidIndex(ClassIndex)){Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Skipping state machine runtime instance %d because class index %d does not map to the %d baked state machine(s)"),NodeIndex,ClassIndex,Meta.Baked.Num())));continue;}
        const FBakedAnimationStateMachine& B=Meta.Baked[ClassIndex];const FString Name=B.MachineName.ToString();if(!Filter.IsEmpty()&&!Name.Contains(Filter,ESearchCase::IgnoreCase)){continue;}
        ++Matching;J->SetNumberField(TEXT("matching_count"),Matching);if(Included>=MaxMachines){continue;}if(!C.Row()){return false;}
        auto M=MakeShared<FJsonObject>();Rows.Add(MakeShared<FJsonValueObject>(M));J->SetArrayField(TEXT("machines"),Rows);
        M->SetNumberField(TEXT("machine_instance_index"),NodeIndex);M->SetNumberField(TEXT("runtime_node_index"),NodeIndex);M->SetNumberField(TEXT("machine_index_in_class"),ClassIndex);C.Text(M,TEXT("machine_name"),Name);
#define MACHINE_VALUE(Key,Expr) {const FAnimNode_StateMachine* Live=Meta.Machine(NodeIndex,ClassIndex);if(!Live){return C.Bad(TEXT("Runtime state machine disappeared during row observation"));}const double V=(Expr);if(!C.Current()||!C.Number(M,TEXT(Key),V)){return false;}}
        int32 Current=INDEX_NONE;{const auto* Live=Meta.Machine(NodeIndex,ClassIndex);if(!Live){return C.Bad(TEXT("Runtime state machine disappeared"));}Current=Live->GetCurrentState();}
        M->SetNumberField(TEXT("current_state_index"),Current);C.Text(M,TEXT("current_state_name"),B.States.IsValidIndex(Current)?B.States[Current].StateName.ToString():FName(NAME_None).ToString());
        M->SetBoolField(TEXT("current_state_index_valid"),B.States.IsValidIndex(Current));
        MACHINE_VALUE("current_state_elapsed_time",Live->GetCurrentStateElapsedTime());
        if(Weights&&!Meta.RecordedWeight(NodeIndex,ClassIndex,INDEX_NONE,M,TEXT("machine_weight"))){return false;}
        M->SetBoolField(TEXT("has_baked_state_machine"),true);M->SetNumberField(TEXT("state_count"),B.States.Num());M->SetNumberField(TEXT("transition_count"),B.Transitions.Num());M->SetNumberField(TEXT("initial_state_index"),B.InitialState);
        if(B.States.IsValidIndex(B.InitialState)){C.Text(M,TEXT("initial_state_name"),B.States[B.InitialState].StateName.ToString());}
        if(States)
        {
            TArray<TSharedPtr<FJsonValue>> StateRows;M->SetArrayField(TEXT("states"),StateRows);const int32 Limit=FMath::Min(MaxStates,B.States.Num());
            for(int32 Index=0;Index<Limit;++Index)
            {
                if(!C.Row()){return false;}const auto& S=B.States[Index];auto SJ=MakeShared<FJsonObject>();StateRows.Add(MakeShared<FJsonValueObject>(SJ));M->SetArrayField(TEXT("states"),StateRows);
                SJ->SetNumberField(TEXT("state_index"),Index);C.Text(SJ,TEXT("state_name"),S.StateName.ToString());SJ->SetBoolField(TEXT("is_current"),Index==Current);SJ->SetBoolField(TEXT("is_conduit"),S.bIsAConduit);SJ->SetBoolField(TEXT("always_reset_on_entry"),S.bAlwaysResetOnEntry);
#define STATE_VALUE(Key,Expr) {const auto* Live=Meta.Machine(NodeIndex,ClassIndex);if(!Live){return C.Bad(TEXT("Runtime state machine disappeared during state observation"));}const double V=(Expr);if(!C.Current()||!C.Number(SJ,TEXT(Key),V)){return false;}}
                if(Weights)
                {STATE_VALUE("state_weight",Live->GetStateWeight(Index));if(!Meta.RecordedWeight(NodeIndex,ClassIndex,Index,SJ,TEXT("recorded_state_weight"))){return false;}SJ->SetBoolField(TEXT("has_nonzero_weight"),SJ->GetNumberField(TEXT("state_weight"))>KINDA_SMALL_NUMBER||SJ->GetNumberField(TEXT("recorded_state_weight"))>KINDA_SMALL_NUMBER);}
                if(Times)
                {if(!B.States.IsValidIndex(Current)){return C.Bad(TEXT("Runtime machine has no initialized current state; unsafe relevant-time getter was not dispatched"));}STATE_VALUE("relevant_anim_length",C.Anim->GetRelevantAnimLength(NodeIndex,Index));STATE_VALUE("relevant_anim_time",C.Anim->GetRelevantAnimTime(NodeIndex,Index));STATE_VALUE("relevant_anim_time_fraction",C.Anim->GetRelevantAnimTimeFraction(NodeIndex,Index));STATE_VALUE("relevant_anim_time_remaining",C.Anim->GetRelevantAnimTimeRemaining(NodeIndex,Index));STATE_VALUE("relevant_anim_time_remaining_fraction",C.Anim->GetRelevantAnimTimeRemainingFraction(NodeIndex,Index));SJ->SetBoolField(TEXT("has_relevant_anim"),SJ->GetNumberField(TEXT("relevant_anim_length"))>KINDA_SMALL_NUMBER);}
#undef STATE_VALUE
            }
            M->SetNumberField(TEXT("included_state_count"),StateRows.Num());M->SetBoolField(TEXT("states_truncated"),B.States.Num()>Limit);
        }
        if(Transitions)
        {
            TArray<TSharedPtr<FJsonValue>> TransitionRows;M->SetArrayField(TEXT("transition_progress"),TransitionRows);int32 Active=0;const int32 Limit=FMath::Min(MaxTransitions,B.Transitions.Num());
            for(int32 Index=0;Index<Limit;++Index)
            {
                const auto* Live=Meta.Machine(NodeIndex,ClassIndex);if(!Live){return C.Bad(TEXT("Runtime state machine disappeared during transition observation"));}const bool IsActive=Live->IsTransitionActive(Index);if(!C.Current()){return false;}Active+=IsActive?1:0;if(!Inactive&&!IsActive){continue;}if(!C.Row()){return false;}
                const auto& T=B.Transitions[Index];auto TJ=MakeShared<FJsonObject>();TransitionRows.Add(MakeShared<FJsonValueObject>(TJ));M->SetArrayField(TEXT("transition_progress"),TransitionRows);
                TJ->SetNumberField(TEXT("transition_index"),Index);C.Text(TJ,TEXT("transition_name"),T.StateName.ToString());TJ->SetBoolField(TEXT("is_active"),IsActive);TJ->SetNumberField(TEXT("previous_state_index"),T.PreviousState);TJ->SetNumberField(TEXT("next_state_index"),T.NextState);
                if(B.States.IsValidIndex(T.PreviousState)){C.Text(TJ,TEXT("previous_state_name"),B.States[T.PreviousState].StateName.ToString());}if(B.States.IsValidIndex(T.NextState)){C.Text(TJ,TEXT("next_state_name"),B.States[T.NextState].StateName.ToString());}
                if(!B.States.IsValidIndex(Current)){return C.Bad(TEXT("Runtime machine has no initialized current state; unsafe transition getter was not dispatched"));}
                if(!Meta.Machine(NodeIndex,ClassIndex)){return C.Bad(TEXT("Machine disappeared before transition duration"));}const float Duration=C.Anim->GetInstanceTransitionCrossfadeDuration(NodeIndex,Index);if(!C.Current()){return false;}
                if(!Meta.Machine(NodeIndex,ClassIndex)){return C.Bad(TEXT("Machine disappeared before transition elapsed time"));}const float Elapsed=C.Anim->GetInstanceTransitionTimeElapsed(NodeIndex,Index);if(!C.Current()){return false;}
                if(!C.Number(TJ,TEXT("crossfade_duration"),Duration)||!C.Number(TJ,TEXT("elapsed_time"),Elapsed)||!C.Number(TJ,TEXT("elapsed_fraction"),Duration>KINDA_SMALL_NUMBER?FMath::Clamp(Elapsed/Duration,0.0f,1.0f):(IsActive?1.0f:0.0f))){return false;}
                TJ->SetField(TEXT("blend_mode"),Enum(StaticEnum<EAlphaBlendOption>(),int64(T.BlendMode)));TJ->SetField(TEXT("logic_type"),Enum(StaticEnum<ETransitionLogicType::Type>(),T.LogicType.GetValue()));
            }
            M->SetNumberField(TEXT("included_transition_progress_count"),TransitionRows.Num());M->SetNumberField(TEXT("active_transition_progress_count"),Active);M->SetBoolField(TEXT("transition_progress_includes_inactive"),Inactive);M->SetBoolField(TEXT("transition_progress_truncated"),B.Transitions.Num()>Limit);
        }
#undef MACHINE_VALUE
        J->SetNumberField(TEXT("included_count"),++Included);
    }
    if(!Filter.IsEmpty()&&Matching==0){Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("No state machine matched filter: %s"),*Filter)));}
    J->SetNumberField(TEXT("matching_count"),Matching);J->SetNumberField(TEXT("included_count"),Included);J->SetNumberField(TEXT("runtime_nodes_probed"),Probed);J->SetNumberField(TEXT("runtime_machines_discovered"),Discovered);
    J->SetBoolField(TEXT("truncated"),Matching>Included);J->SetBoolField(TEXT("no_runtime_machine_discovered"),Discovered==0);J->SetBoolField(TEXT("valid"),true);return Meta.Current();
}
bool Montage(FRead& C)
{
    auto J=MakeShared<FJsonObject>();C.R.Data->SetObjectField(TEXT("active_montage"),J);J->SetBoolField(TEXT("valid"),false);
    if(!C.Current()){return false;}Call(C.R,TEXT("current_montage_calls"));UAnimMontage* Active=C.Anim->GetCurrentActiveMontage();
    if(Active&&!C.Hold(Active)){return false;}if(!C.Current()){return false;}
    if(!Active){J->SetStringField(TEXT("note"),TEXT("No active montage"));return true;}
    auto Current=[&]()
    {
        if(!C.Current()){return false;}Call(C.R,TEXT("current_montage_calls"));
        if(C.Anim->GetCurrentActiveMontage()!=Active||!C.Raw()){return C.Bad(TEXT("Current active montage changed during observation"));}return true;
    };
    C.Text(J,TEXT("name"),Active->GetName());C.Text(J,TEXT("path"),Active->GetPathName());
#define MONTAGE_BOOL(Key,Expr) {if(!Current()){return false;}const bool V=(Expr);if(!Current()){return false;}J->SetBoolField(TEXT(Key),V);}
#define MONTAGE_NUMBER(Key,Expr) {if(!Current()){return false;}const double V=(Expr);if(!Current()||!C.Number(J,TEXT(Key),V)){return false;}}
    MONTAGE_BOOL("is_playing",C.Anim->Montage_IsPlaying(Active));MONTAGE_BOOL("is_stopped",C.Anim->Montage_GetIsStopped(Active));
    if(!Current()){return false;}const FString Section=C.Anim->Montage_GetCurrentSection(Active).ToString();if(!Current()||!C.Text(J,TEXT("current_section"),Section)){return false;}
    MONTAGE_NUMBER("position",C.Anim->Montage_GetPosition(Active));MONTAGE_NUMBER("blend_time",C.Anim->Montage_GetBlendTime(Active));
    MONTAGE_NUMBER("play_rate",C.Anim->Montage_GetPlayRate(Active));MONTAGE_NUMBER("effective_play_rate",C.Anim->Montage_GetEffectivePlayRate(Active));
#undef MONTAGE_BOOL
#undef MONTAGE_NUMBER
    J->SetBoolField(TEXT("valid"),true);return Current();
}
bool Curves(FRead& C,const TArray<FString>& Requested,int32 Limit,TArray<TSharedPtr<FJsonValue>>& Warnings)
{
    auto J=MakeShared<FJsonObject>();auto Values=MakeShared<FJsonObject>();C.R.Data->SetObjectField(TEXT("curves"),J);J->SetBoolField(TEXT("valid"),false);J->SetObjectField(TEXT("values"),Values);
    if(!C.Current()){return false;}TArray<FName> Names;
    if(!Requested.IsEmpty()){for(const FString& Text:Requested){if(!Text.IsEmpty()){Names.AddUnique(FName(*Text));}}}
    else{Call(C.R,TEXT("all_curve_names_calls"));C.Anim->GetAllCurveNames(Names);if(!C.Current()){return false;}}
    if(Names.Num()>MaxRows){return C.Bad(TEXT("Curve source names exceed finite rows"));}
    J->SetBoolField(TEXT("requested_only"),!Requested.IsEmpty());J->SetNumberField(TEXT("available_or_requested_count"),Names.Num());int32 Included=0;
    for(const FName& Name:Names)
    {
        if(Included>=Limit){break;}if(!C.Current()||!C.Row()){return false;}float Value=0;
        Call(C.R,TEXT("curve_value_calls"));const bool Found=C.Anim->GetCurveValue(Name,Value);if(!C.Current()){return false;}
        auto Row=MakeShared<FJsonObject>();Values->SetObjectField(Name.ToString(),Row);Row->SetBoolField(TEXT("found"),Found);if(!C.Number(Row,TEXT("value"),Value)){return false;}
        J->SetNumberField(TEXT("included_count"),++Included);
        if(!Found&&!Requested.IsEmpty()){Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Curve not found: %s"),*Name.ToString())));}
    }
    J->SetNumberField(TEXT("included_count"),Included);J->SetBoolField(TEXT("truncated"),Names.Num()>Included);J->SetBoolField(TEXT("valid"),true);return C.Current();
}
}

Variable::FResult Inspect(const FString& ActorLabel,const FString& ActorName,const FString& ActorPath,const FString& ComponentName,const FString& StateMachineName,
    bool bIncludeStates,bool bIncludeStateWeights,bool bIncludeRelevantAnimTimes,bool bIncludeTransitionProgress,bool bIncludeInactiveTransitionProgress,
    bool bIncludeMontages,bool bIncludeCurves,const FString& CurveNamesJson,int64 MaxStateMachines,int64 MaxStateMachineInstancesToProbe,
    int64 MaxStatesPerMachine,int64 MaxTransitionsPerMachine,int64 MaxCurves,bool bPreferPIEWorld,bool bRequirePIEWorld)
{
    FResult R;R.Data->SetStringField(TEXT("command"),TEXT("inspect_anim_instance_runtime_state"));auto Counts=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("native_call_counts"),Counts);
    for(const TCHAR* Key:{TEXT("world_discovery_calls"),TEXT("actor_label_calls"),TEXT("component_inventory_calls"),TEXT("anim_instance_calls"),TEXT("state_machine_instance_calls"),TEXT("current_montage_calls"),TEXT("all_curve_names_calls"),TEXT("curve_value_calls"),TEXT("explicit_load_calls"),TEXT("explicit_modify_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls"),TEXT("explicit_tick_calls"),TEXT("explicit_update_calls"),TEXT("explicit_initialize_calls"),TEXT("starts_pie_sie_calls")}){Counts->SetNumberField(Key,0);}
    for(const TCHAR* Key:{TEXT("read_only"),TEXT("explicit_runtime_read_only")}){R.Data->SetBoolField(Key,true);}
    for(const TCHAR* Key:{TEXT("asset_modified"),TEXT("saves_assets"),TEXT("starts_sie"),TEXT("observation_incomplete"),TEXT("atomic_snapshot_verified"),TEXT("all_SDK_effects_verified_absent"),TEXT("automatic_action_retry"),TEXT("atomic_rollback_available"),TEXT("unique_external_pipeline_complete"),TEXT("packaged_runtime_verified")}){R.Data->SetBoolField(Key,false);}
    R.Data->SetStringField(TEXT("runtime_source"),TEXT("active_anim_instance"));R.Data->SetStringField(TEXT("runtime_note"),TEXT("Reads the current SkeletalMeshComponent AnimInstance in ordered PIE/SIE/play or Editor worlds. No explicit session start, initialize, wait, tick, update, execute, compile or save is dispatched. Public SDK proxy getters may lazily create a proxy or handle pending tasks; total effects remain unknown."));
    R.Data->SetStringField(TEXT("change_state_scope"),TEXT("Explicit writer/tick/init/compile/save calls remain zero. World/component/interface/AnimInstance getters and proxy access can have opaque SDK/custom callbacks, so total effects become unknown at world discovery."));
    R.Data->SetBoolField(TEXT("prefer_pie_world"),bPreferPIEWorld);R.Data->SetBoolField(TEXT("require_pie_world"),bRequirePIEWorld);
    R.Data->SetStringField(TEXT("requested_actor_label"),ActorLabel);R.Data->SetStringField(TEXT("requested_actor_name"),ActorName);R.Data->SetStringField(TEXT("requested_actor_path"),ActorPath);R.Data->SetStringField(TEXT("requested_component_name"),ComponentName);
    R.Data->SetArrayField(TEXT("errors"),{});R.Data->SetArrayField(TEXT("warnings"),{});
    if(!IsEditorGameThread()||IsEngineExitRequested()||InFlight){return Fail(R,TEXT("anim_runtime_editor_unavailable"),TEXT("A live Editor game thread is required; PIE/SIE reads are permitted and no session is started"));}
    TGuardValue<bool> Busy(InFlight,true);R.Stage=TEXT("validate_anim_runtime_inputs");
    for(const FString* S:{&ActorLabel,&ActorName,&ActorPath,&ComponentName,&StateMachineName}){if(!Bounded(*S)){return Fail(R,TEXT("anim_runtime_input_bound"),TEXT("Runtime selector exceeds finite text/NUL bounds"));}}
    TArray<FString> RequestedCurves;FString ParseError;if(!CurvesInput(CurveNamesJson,RequestedCurves,ParseError)){return Fail(R,TEXT("anim_runtime_curves_invalid"),ParseError.IsEmpty()?TEXT("Curve carrier exceeds finite bounds"):ParseError);}
    const int32 MachineLimit=int32(FMath::Clamp<int64>(MaxStateMachines,0,1024)),ProbeLimit=int32(FMath::Clamp<int64>(MaxStateMachineInstancesToProbe,0,4096)),StateLimit=int32(FMath::Clamp<int64>(MaxStatesPerMachine,0,2048)),TransitionLimit=int32(FMath::Clamp<int64>(MaxTransitionsPerMachine,0,4096)),CurveLimit=int32(FMath::Clamp<int64>(MaxCurves,0,2048));
    auto Limits=MakeShared<FJsonObject>();Limits->SetStringField(TEXT("max_state_machines"),LexToString(MaxStateMachines));Limits->SetStringField(TEXT("max_state_machine_instances_to_probe"),LexToString(MaxStateMachineInstancesToProbe));Limits->SetStringField(TEXT("max_states_per_machine"),LexToString(MaxStatesPerMachine));Limits->SetStringField(TEXT("max_transitions_per_machine"),LexToString(MaxTransitionsPerMachine));Limits->SetStringField(TEXT("max_curves"),LexToString(MaxCurves));R.Data->SetObjectField(TEXT("received_limits_decimal"),Limits);
    FRead C(R);TArray<TSharedPtr<FJsonValue>> Warnings,Errors;
    auto Prefix=[&](){R.Data->SetArrayField(TEXT("warnings"),Warnings);R.Data->SetArrayField(TEXT("errors"),Errors);R.Data->SetNumberField(TEXT("completed_output_rows"),C.Rows);};
    auto Incomplete=[&]()
    {Prefix();return Fail(R,TEXT("anim_runtime_observation_incomplete"),C.Error.IsEmpty()?TEXT("Runtime observation is incomplete; partial prefix retained, no retry"):C.Error);};
    R.Stage=TEXT("discover_anim_runtime_worlds");R.bChangeStateKnown=false;Call(R,TEXT("world_discovery_calls"));const TArray<UWorld*> Candidates=Worlds(bPreferPIEWorld,bRequirePIEWorld);
    if(Candidates.Num()>MaxRows){C.Bad(TEXT("World source exceeds finite rows"));return Incomplete();}for(UWorld* W:Candidates){if(!C.Hold(W)){return Incomplete();}}
    int32 MatchedActors=0,MatchedComponents=0;
    for(UWorld* World:Candidates)
    {
        C.World=World;C.Type=World->WorldType;if(!C.Current()){return Incomplete();}TArray<AActor*> Actors;
        // Capture first; do not keep a live iterator across label/component callbacks.
        for(TActorIterator<AActor> It(World);It;++It)
        {AActor* A=*It;if(!Loaded(A)){continue;}if(Actors.Num()>=MaxRows||!C.Hold(A)){C.Bad(TEXT("Actor source exceeds finite bounds"));return Incomplete();}Actors.Add(A);}
        TArray<AActor*> Matches;
        for(AActor* A:Actors)
        {
            C.Actor=A;if(!C.Current()){return Incomplete();}
            if(!ActorLabel.IsEmpty())
            {Call(R,TEXT("actor_label_calls"));const FString Label=A->GetActorLabel();if(!C.Current()){return Incomplete();}if(!Label.Equals(ActorLabel,ESearchCase::IgnoreCase)){continue;}}
            if(!ActorName.IsEmpty()&&!A->GetName().Equals(ActorName,ESearchCase::IgnoreCase)){continue;}
            const FString Path=A->GetPathName();if(!ActorPath.IsEmpty()&&!Path.Equals(ActorPath,ESearchCase::IgnoreCase)&&!Path.EndsWith(ActorPath,ESearchCase::IgnoreCase)){continue;}
            TArray<USkeletalMeshComponent*> Components;Call(R,TEXT("component_inventory_calls"));A->GetComponents<USkeletalMeshComponent>(Components);if(!C.Current()){return Incomplete();}
            if(!Components.IsEmpty()){Matches.Add(A);}
        }
        MatchedActors+=Matches.Num();
        for(AActor* A:Matches)
        {
            C.Actor=A;C.Component=nullptr;if(!C.Current()){return Incomplete();}TArray<USkeletalMeshComponent*> Components;Call(R,TEXT("component_inventory_calls"));A->GetComponents<USkeletalMeshComponent>(Components);
            if(!C.Current()||Components.Num()>MaxRows){C.Bad(TEXT("Component source exceeds finite rows"));return Incomplete();}
            USkeletalMeshComponent* First=nullptr;int32 Count=0;
            for(USkeletalMeshComponent* Component:Components)
            {
                if(!Loaded(Component)){continue;}if(!C.Hold(Component)){return Incomplete();}if(!First){First=Component;}
                if(ComponentName.IsEmpty()){++Count;continue;}
                if(Component->GetName().Equals(ComponentName,ESearchCase::IgnoreCase)){First=Component;++Count;break;}
            }
            if(ComponentName.IsEmpty()?First!=nullptr:Count>0){C.Component=First;MatchedComponents=Count;break;}
        }
        if(C.Component){break;}C.Actor=nullptr;
    }
    if(!C.Actor||!C.Component||!C.World)
    {Prefix();return Fail(R,TEXT("anim_runtime_target_missing"),(bRequirePIEWorld?FString::Printf(TEXT("No SkeletalMeshComponent actor matched actor_label='%s', actor_name='%s', actor_path='%s', component_name='%s' in active PIE/SIE/play worlds"),*ActorLabel,*ActorName,*ActorPath,*ComponentName):FString::Printf(TEXT("No SkeletalMeshComponent actor matched actor_label='%s', actor_name='%s', actor_path='%s', component_name='%s' in candidate editor/PIE worlds"),*ActorLabel,*ActorName,*ActorPath,*ComponentName)));}
    if(MatchedActors>1){Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Matched %d actors; inspected the first matching actor in world priority order"),MatchedActors)));}
    if(ComponentName.IsEmpty()&&MatchedComponents>1){Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Actor has %d SkeletalMeshComponents; inspected the first component because component_name was not provided"),MatchedComponents)));}
    R.Stage=TEXT("observe_anim_runtime_target");if(!C.Current()){return Incomplete();}Call(R,TEXT("anim_instance_calls"));UAnimInstance* Instance=C.Component->GetAnimInstance();
    if(Instance&&!C.Hold(Instance)){return Incomplete();}if(!C.Current()){return Incomplete();}
    if(!Instance)
    {UClass* Class=C.Component->GetAnimClass();if(Class&&!C.Hold(Class)){return Incomplete();}if(!C.Current()){return Incomplete();}Prefix();return Fail(R,TEXT("anim_runtime_instance_missing"),FString::Printf(TEXT("Matched SkeletalMeshComponent '%s' has no AnimInstance. animation_mode=%s anim_class=%s"),*C.Component->GetName(),*AnimationMode(C.Component->GetAnimationMode()),Class?*Class->GetPathName():TEXT("<none>")));}
    C.Anim=Instance;if(!C.Current()){return Incomplete();}
    R.Data->SetBoolField(TEXT("is_play_session_active"),IsPlaySessionActive());R.Data->SetStringField(TEXT("sampled_world_type"),WorldType(C.World));R.Data->SetStringField(TEXT("sampled_world_name"),C.World->GetName());R.Data->SetNumberField(TEXT("matched_actor_count"),MatchedActors);R.Data->SetNumberField(TEXT("matched_component_count"),MatchedComponents);
    auto AJ=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("actor"),AJ);Call(R,TEXT("actor_label_calls"));const FString Label=C.Actor->GetActorLabel();if(!C.Current()){return Incomplete();}
    C.Text(AJ,TEXT("label"),Label);C.Text(AJ,TEXT("name"),C.Actor->GetName());C.Text(AJ,TEXT("path"),C.Actor->GetPathName());C.Text(AJ,TEXT("class"),C.Actor->GetClass()->GetPathName());
    auto CJ=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("component"),CJ);USkeletalMesh* Mesh=C.Component->GetSkeletalMeshAsset();if(Mesh&&!C.Hold(Mesh)){return Incomplete();}if(!C.Current()){return Incomplete();}
    UClass* AnimClass=C.Component->GetAnimClass();if(AnimClass&&!C.Hold(AnimClass)){return Incomplete();}if(!C.Current()){return Incomplete();}
    C.Text(CJ,TEXT("name"),C.Component->GetName());C.Text(CJ,TEXT("path"),C.Component->GetPathName());C.Text(CJ,TEXT("class"),C.Component->GetClass()->GetPathName());C.Text(CJ,TEXT("skeletal_mesh"),Mesh?Mesh->GetPathName():FString());C.Text(CJ,TEXT("anim_class"),AnimClass?AnimClass->GetPathName():FString());C.Text(CJ,TEXT("animation_mode"),AnimationMode(C.Component->GetAnimationMode()));
    const FTransform T=C.Component->GetComponentTransform();if(!C.Current()||T.ContainsNaN()){C.Bad(TEXT("Component transform is not a finite current sample"));return Incomplete();}CJ->SetObjectField(TEXT("component_transform"),Transform(T));
    auto IJ=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("anim_instance"),IJ);C.Text(IJ,TEXT("name"),Instance->GetName());C.Text(IJ,TEXT("path"),Instance->GetPathName());C.Text(IJ,TEXT("class"),Instance->GetClass()->GetPathName());C.Text(IJ,TEXT("outer"),Instance->GetOuter()?Instance->GetOuter()->GetPathName():FString());
    R.Stage=TEXT("observe_anim_runtime_state_machines");if(!Machines(C,StateMachineName,bIncludeStates,bIncludeStateWeights,bIncludeRelevantAnimTimes,bIncludeTransitionProgress,bIncludeInactiveTransitionProgress,MachineLimit,ProbeLimit,StateLimit,TransitionLimit,Warnings)){return Incomplete();}
    if(bIncludeMontages){R.Stage=TEXT("observe_anim_runtime_montage");if(!Montage(C)){return Incomplete();}}
    if(bIncludeCurves){R.Stage=TEXT("observe_anim_runtime_curves");if(!Curves(C,RequestedCurves,CurveLimit,Warnings)){return Incomplete();}}
    if(!C.Current()){return Incomplete();}Prefix();R.bSuccess=true;R.bPostconditionPassed=true;R.Stage=TEXT("anim_runtime_observation_complete");R.Data->SetBoolField(TEXT("success"),true);return R;
}
}
