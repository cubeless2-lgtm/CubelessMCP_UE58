#include "Compatibility/UE58/CubelessAnimInstanceRuntimePropertyProbeApi.h"
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
#include "Templates/Function.h"

namespace Cubeless::UE58::AnimInstanceRuntimePropertyProbe
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
    explicit FRead(FResult& In):R(In),Output(In.Data){}
    FResult& R;TSharedRef<FJsonObject> Output;bool RequireInitialized=false,UsePostProcess=false;TMap<UPackage*,bool> InitialPackageDirty;TArray<TFunction<bool()>> ProjectionChecks;TArray<TStrongObjectPtr<UObject>> Held;TArray<FIdentity> Identities;TSet<UObject*> Seen;
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
            Identities.Add({O,O->GetClass(),O->GetOuter(),P,O->GetPathName(),P->GetPathName()});if(!InitialPackageDirty.Contains(P)){InitialPackageDirty.Add(P,P->IsDirty());}
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
        // Earlier scopes validate owners before later scopes inspect nested storage.
        for(const auto& Check:ProjectionChecks){if(!Step()||!Check()){return Bad(TEXT("Runtime property descriptor/layout/container or array storage changed during projection callback"));}}
        return true;
    }
    UAnimInstance* SelectedAnim() const {return UsePostProcess?Component->GetPostProcessInstance():Component->GetAnimInstance();}
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
            if(SelectedAnim()!=Anim||Anim->GetOuter()!=Component||!Raw()){return Bad(TEXT("Component AnimInstance identity/owning component changed"));}
            if(RequireInitialized&&!Anim->IsInitialized()){return Bad(TEXT("AnimInstance is not initialized; observation does not initialize it"));}
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
        if(!C.Raw()||C.SelectedAnim()!=C.Anim||!C.Anim->IsInitialized()
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
        if(!C.Raw()||C.SelectedAnim()!=C.Anim||!C.Anim->IsInitialized()
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
    auto J=MakeShared<FJsonObject>();C.Output->SetObjectField(TEXT("state_machines"),J);J->SetBoolField(TEXT("valid"),false);TArray<TSharedPtr<FJsonValue>> Rows;J->SetArrayField(TEXT("machines"),Rows);
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
    auto J=MakeShared<FJsonObject>();C.Output->SetObjectField(TEXT("active_montage"),J);J->SetBoolField(TEXT("valid"),false);
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
    auto J=MakeShared<FJsonObject>();auto Values=MakeShared<FJsonObject>();C.Output->SetObjectField(TEXT("curves"),J);J->SetBoolField(TEXT("valid"),false);J->SetObjectField(TEXT("values"),Values);
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
bool IsFiniteControlRigProbeFloatRange(const double Value)
{
    return FMath::IsFinite(Value) &&
        FMath::Abs(Value) <= static_cast<double>(TNumericLimits<float>::Max());
}

bool TryParseControlRigProbeName(
    const FString& RequestedName,
    const FString& FieldPath,
    FName& OutName,
    FString& OutError,
    const bool bAllowNone = false)
{
    OutError.Reset();
    const FString NormalizedName = RequestedName.TrimStartAndEnd();
    if (NormalizedName.IsEmpty())
    {
        OutError = FString::Printf(TEXT("'%s' must not be empty"), *FieldPath);
        return false;
    }
    if (NormalizedName.Len() >= NAME_SIZE)
    {
        OutError = FString::Printf(
            TEXT("'%s' is too long for an Unreal name (maximum %d characters)"),
            *FieldPath,
            NAME_SIZE - 1);
        return false;
    }

    OutName = FName(*NormalizedName);
    if (!bAllowNone && OutName.IsNone())
    {
        OutError = FString::Printf(
            TEXT("Invalid '%s' name '%s': name resolves to None"),
            *FieldPath,
            *RequestedName);
        return false;
    }
    return true;
}

bool TryGetNumberFromObjectField(const TSharedPtr<FJsonObject>& Object, const FString& LowerName, const FString& UpperName, double& OutValue)
{
    return Object.IsValid() &&
        (Object->TryGetNumberField(LowerName, OutValue) || Object->TryGetNumberField(UpperName, OutValue)) &&
        IsFiniteControlRigProbeFloatRange(OutValue);
}

bool JsonValueToVector(const TSharedPtr<FJsonValue>& Value, FVector& OutVector)
{
    if (!Value.IsValid())
    {
        return false;
    }

    if (Value->Type == EJson::Array)
    {
        const TArray<TSharedPtr<FJsonValue>>& Values = Value->AsArray();
        if (Values.Num() < 3)
        {
            return false;
        }
        double X = 0.0;
        double Y = 0.0;
        double Z = 0.0;
        if (!Values[0].IsValid() || Values[0]->Type != EJson::Number || !Values[0]->TryGetNumber(X) ||
            !Values[1].IsValid() || Values[1]->Type != EJson::Number || !Values[1]->TryGetNumber(Y) ||
            !Values[2].IsValid() || Values[2]->Type != EJson::Number || !Values[2]->TryGetNumber(Z) ||
            !IsFiniteControlRigProbeFloatRange(X) ||
            !IsFiniteControlRigProbeFloatRange(Y) ||
            !IsFiniteControlRigProbeFloatRange(Z))
        {
            return false;
        }
        OutVector = FVector(X, Y, Z);
        return true;
    }

    if (Value->Type == EJson::Object)
    {
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        double X = 0.0;
        double Y = 0.0;
        double Z = 0.0;
        if (!TryGetNumberFromObjectField(Object, TEXT("x"), TEXT("X"), X) ||
            !TryGetNumberFromObjectField(Object, TEXT("y"), TEXT("Y"), Y) ||
            !TryGetNumberFromObjectField(Object, TEXT("z"), TEXT("Z"), Z))
        {
            return false;
        }
        OutVector = FVector(X, Y, Z);
        return true;
    }

    return false;
}

bool JsonValueToRotator(const TSharedPtr<FJsonValue>& Value, FRotator& OutRotator)
{
    FVector Vector;
    if (JsonValueToVector(Value, Vector))
    {
        OutRotator = FRotator(Vector.X, Vector.Y, Vector.Z);
        return true;
    }
    return false;
}

bool JsonValueToTransform(const TSharedPtr<FJsonValue>& Value, FTransform& OutTransform)
{
    if (!Value.IsValid() || Value->Type != EJson::Object)
    {
        return false;
    }

    const TSharedPtr<FJsonObject> Object = Value->AsObject();
    if (!Object.IsValid())
    {
        return false;
    }

    FVector Translation = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    FVector Scale = FVector::OneVector;
    bool bHasAnyField = false;

    const TSharedPtr<FJsonValue> TranslationValue = Object->TryGetField(TEXT("translation")).IsValid()
        ? Object->TryGetField(TEXT("translation"))
        : Object->TryGetField(TEXT("location"));
    if (TranslationValue.IsValid())
    {
        if (!JsonValueToVector(TranslationValue, Translation))
        {
            return false;
        }
        bHasAnyField = true;
    }

    if (const TSharedPtr<FJsonValue> RotationValue = Object->TryGetField(TEXT("rotation")))
    {
        if (!JsonValueToRotator(RotationValue, Rotation))
        {
            return false;
        }
        bHasAnyField = true;
    }

    const TSharedPtr<FJsonValue> ScaleValue = Object->TryGetField(TEXT("scale")).IsValid()
        ? Object->TryGetField(TEXT("scale"))
        : Object->TryGetField(TEXT("scale3d"));
    if (ScaleValue.IsValid())
    {
        if (!JsonValueToVector(ScaleValue, Scale))
        {
            return false;
        }
        bHasAnyField = true;
    }

    if (!bHasAnyField)
    {
        return false;
    }

    OutTransform = FTransform(Rotation, Translation, Scale);
    return true;
}

bool SetUObjectPropertyFromJson(UObject* Target, const FString& PropertyName, const TSharedPtr<FJsonValue>& Value, FString& OutError, bool& Dispatched)
{
    OutError.Reset();
    Dispatched=false;
    if (!Target)
    {
        OutError = TEXT("Invalid target object");
        return false;
    }
    if (!Value.IsValid())
    {
        OutError = FString::Printf(TEXT("Missing value for property '%s'"), *PropertyName);
        return false;
    }

    FName PropertyFName = NAME_None;
    if (!TryParseControlRigProbeName(PropertyName, TEXT("property name"), PropertyFName, OutError))
    {
        return false;
    }

    FProperty* Property = Target->GetClass()->FindPropertyByName(PropertyFName);
    if (!Property)
    {
        OutError = FString::Printf(TEXT("Property '%s' not found on %s"), *PropertyName, *Target->GetClass()->GetName());
        return false;
    }

    if (Property->HasAnyPropertyFlags(CPF_EditorOnly|CPF_Parm) || Property->GetOffset_ForInternal()<0 || Property->ArrayDim<1 || !Property->IsInContainer(Target->GetClass()))
    { OutError=TEXT("Editor-only/parameter or out-of-container runtime property is refused");return false; }
    void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Target);
    if (!ValuePtr)
    {
        OutError = FString::Printf(TEXT("Could not resolve property memory for '%s'"), *PropertyName);
        return false;
    }

    if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
    {
        if (Value->Type != EJson::Boolean)
        {
            OutError = FString::Printf(TEXT("Property '%s' expects a boolean"), *PropertyName);
            return false;
        }
        Dispatched=true; BoolProperty->SetPropertyValue(ValuePtr, Value->AsBool());
        return true;
    }

    if (FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
    {
        if (Value->Type != EJson::Number)
        {
            OutError = FString::Printf(TEXT("Property '%s' expects a number"), *PropertyName);
            return false;
        }
        const double NumericValue = Value->AsNumber();
        if (!IsFiniteControlRigProbeFloatRange(NumericValue))
        {
            OutError = FString::Printf(
                TEXT("Property '%s' requires a finite number within float range"),
                *PropertyName);
            return false;
        }
        if (NumericProperty->IsFloatingPoint())
        {
            Dispatched=true; NumericProperty->SetFloatingPointPropertyValue(ValuePtr, NumericValue);
        }
        else
        {
            // CanHoldValue(double) intentionally requires exact conversion. That
            // is correct for integral properties, but would reject ordinary
            // float inputs such as 0.1 after the finite/range check above.
            if (!NumericProperty->CanHoldValue(NumericValue))
            {
                OutError = FString::Printf(
                    TEXT("Property '%s' cannot represent the requested integral value"),
                    *PropertyName);
                return false;
            }
            if (CastField<FUInt64Property>(NumericProperty) ||
                CastField<FUInt32Property>(NumericProperty) ||
                CastField<FUInt16Property>(NumericProperty))
            {
                Dispatched=true; NumericProperty->SetIntPropertyValue(ValuePtr, static_cast<uint64>(NumericValue));
            }
            else
            {
                Dispatched=true; NumericProperty->SetIntPropertyValue(ValuePtr, static_cast<int64>(NumericValue));
            }
        }
        return true;
    }

    if (FNameProperty* NameProperty = CastField<FNameProperty>(Property))
    {
        if (Value->Type != EJson::String)
        {
            OutError = FString::Printf(TEXT("Property '%s' expects a string/name"), *PropertyName);
            return false;
        }
        const FString NameValue = Value->AsString();
        FName ParsedNameValue = NAME_None;
        FString NameValueError;
        if (!TryParseControlRigProbeName(
                NameValue,
                FString::Printf(TEXT("property '%s' name value"), *PropertyName),
                ParsedNameValue,
                NameValueError))
        {
            OutError = NameValueError;
            return false;
        }
        Dispatched=true; NameProperty->SetPropertyValue(ValuePtr, ParsedNameValue);
        return true;
    }

    if (FStrProperty* StringProperty = CastField<FStrProperty>(Property))
    {
        if (Value->Type != EJson::String)
        {
            OutError = FString::Printf(TEXT("Property '%s' expects a string"), *PropertyName);
            return false;
        }
        Dispatched=true; StringProperty->SetPropertyValue(ValuePtr, Value->AsString());
        return true;
    }

    if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
    {
        if (StructProperty->Struct == TBaseStructure<FVector>::Get())
        {
            FVector VectorValue;
            if (!JsonValueToVector(Value, VectorValue))
            {
                OutError = FString::Printf(TEXT("Property '%s' expects a vector array/object"), *PropertyName);
                return false;
            }
            Dispatched=true; *static_cast<FVector*>(ValuePtr) = VectorValue;
            return true;
        }

        if (StructProperty->Struct == TBaseStructure<FRotator>::Get())
        {
            FRotator RotatorValue;
            if (!JsonValueToRotator(Value, RotatorValue))
            {
                OutError = FString::Printf(TEXT("Property '%s' expects a rotator array/object"), *PropertyName);
                return false;
            }
            Dispatched=true; *static_cast<FRotator*>(ValuePtr) = RotatorValue;
            return true;
        }

        if (StructProperty->Struct == TBaseStructure<FTransform>::Get())
        {
            FTransform TransformValue;
            if (!JsonValueToTransform(Value, TransformValue))
            {
                OutError = FString::Printf(TEXT("Property '%s' expects a transform object"), *PropertyName);
                return false;
            }
            Dispatched=true; *static_cast<FTransform*>(ValuePtr) = TransformValue;
            return true;
        }
    }

    OutError = FString::Printf(TEXT("Unsupported property type for '%s': %s"), *PropertyName, *Property->GetClass()->GetName());
    return false;
}


bool Decode(const FString& Text,TSharedPtr<FJsonValue>& Out,FString& Error)
{
    if(!Bounded(Text)){Error=TEXT("Runtime JSON carrier exceeds 1Mi UTF16 or contains raw NUL");return false;}
    int32 Depth=0;bool Quoted=false,Escaped=false;
    for(TCHAR Ch:Text)
    {
        if(Quoted){if(Escaped){Escaped=false;}else if(Ch==TCHAR('\\')){Escaped=true;}else if(Ch==TCHAR('"')){Quoted=false;}continue;}
        if(Ch==TCHAR('"')){Quoted=true;}else if(Ch==TCHAR('[')||Ch==TCHAR('{')){if(++Depth>MaxDepth){Error=TEXT("Runtime JSON carrier exceeds depth64");return false;}}
        else if(Ch==TCHAR(']')||Ch==TCHAR('}')){if(--Depth<0){Error=TEXT("Malformed runtime JSON carrier");return false;}}
    }
    if(Quoted||Depth){Error=TEXT("Malformed runtime JSON carrier");return false;}
    TArray<TSharedPtr<FJsonValue>> A;auto Reader=TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]"));
    if(!FJsonSerializer::Deserialize(Reader,A,FJsonSerializer::EFlags::None)||A.Num()!=1||!A[0].IsValid()){Error=TEXT("One JSON value is required");return false;}
    int32 Slots=0;TArray<TSharedPtr<FJsonValue>> Work{A[0]};
    while(!Work.IsEmpty())
    {
        auto V=Work.Pop(EAllowShrinking::No);if(!V.IsValid()||++Slots>MaxRows){Error=TEXT("Runtime JSON exceeds finite value slots");return false;}
        if(V->Type==EJson::Number&&!FMath::IsFinite(V->AsNumber())){Error=TEXT("Non-finite JSON number");return false;}
        if(V->Type==EJson::String&&!Bounded(V->AsString())){Error=TEXT("Decoded runtime string exceeds NUL/text bounds");return false;}
        if(V->Type==EJson::Array){Work.Append(V->AsArray());}
        if(V->Type==EJson::Object){for(const auto& KV:V->AsObject()->Values){if(!Bounded(FString(KV.Key.ToView()))){Error=TEXT("Invalid decoded property key");return false;}Work.Add(KV.Value);}}
    }
    Out=A[0];return true;
}
struct FAssignment{FString Name;TSharedPtr<FJsonValue> Value;};
void AddAssignment(TArray<FAssignment>& Out,const FString& Name,const TSharedPtr<FJsonValue>& Value)
{if(!Name.TrimStartAndEnd().IsEmpty()&&Value.IsValid()){Out.Add({Name,Value});}}
bool Assignments(const TSharedPtr<FJsonValue>& Projection,const FString& Name,const TSharedPtr<FJsonValue>& Value,TArray<FAssignment>& Out)
{
    // A fixed internal DTO retains the two original property fields independently.
    // It is not an arbitrary command/transport parameter bag.
    if(Projection->Type!=EJson::Object){return false;}const auto Params=Projection->AsObject();
    for(const auto& KV:Params->Values){if(KV.Key!=TEXT("properties")&&KV.Key!=TEXT("property_values")){return false;}}
    const TSharedPtr<FJsonObject>* Object=nullptr;
    if((Params->TryGetObjectField(TEXT("properties"),Object)||Params->TryGetObjectField(TEXT("property_values"),Object))&&Object)
    {for(const auto& KV:(*Object)->Values){AddAssignment(Out,FString(KV.Key.ToView()),KV.Value);}}
    const TArray<TSharedPtr<FJsonValue>>* Array=nullptr;
    if((Params->TryGetArrayField(TEXT("properties"),Array)||Params->TryGetArrayField(TEXT("property_values"),Array))&&Array)
    {
        for(const auto& V:*Array)
        {if(!V.IsValid()||V->Type!=EJson::Object){continue;}auto J=V->AsObject();FString N;J->TryGetStringField(TEXT("name"),N);if(N.IsEmpty()){J->TryGetStringField(TEXT("property_name"),N);}AddAssignment(Out,N,J->TryGetField(TEXT("value")));}
    }
    if(!Name.IsEmpty()){AddAssignment(Out,Name,Value);}return Out.Num()<=MaxRows;
}

struct FFieldShape
{
    FProperty* Property=nullptr;UStruct* Owner=nullptr;FFieldClass* FieldClass=nullptr;UScriptStruct* Struct=nullptr;
    int32 Offset=0,ElementSize=0,ArrayDim=0,Size=0,OwnerSize=0,StructSize=0;EPropertyFlags Flags=CPF_None;EArrayPropertyFlags ArrayFlags=EArrayPropertyFlags::None;
    TFunction<FProperty*()> Resolve;
    bool Current() const
    {
        // Resolve the captured location before dereferencing a possibly replaced field.
        if(!Property||!Loaded(Owner)||!Resolve||Resolve()!=Property){return false;}
        if(Property->GetClass()!=FieldClass||Property->GetOwnerStruct()!=Owner||Owner->GetPropertiesSize()!=OwnerSize
            ||Property->GetOffset_ForInternal()!=Offset||Property->GetElementSize()!=ElementSize||Property->ArrayDim!=ArrayDim
            ||Property->GetSize()!=Size||Property->PropertyFlags!=Flags||Offset<0||ElementSize<=0||ArrayDim<1||Size<=0){return false;}
        if(const auto ArrayProperty=CastField<FArrayProperty>(Property)){if(ArrayProperty->ArrayFlags!=ArrayFlags){return false;}}
        const auto StructProperty=CastField<FStructProperty>(Property);
        return StructProperty?(StructProperty->Struct==Struct&&Loaded(Struct)&&Struct->GetStructureSize()==StructSize):Struct==nullptr;
    }
};
TSharedRef<FFieldShape> CaptureFieldShape(FRead& C,FProperty* Property,TFunction<FProperty*()> Resolve)
{
    auto S=MakeShared<FFieldShape>();S->Property=Property;S->Resolve=MoveTemp(Resolve);
    if(!Property){return S;}S->Owner=Property->GetOwnerStruct();if(!C.Hold(S->Owner)){return S;}
    S->FieldClass=Property->GetClass();S->Offset=Property->GetOffset_ForInternal();S->ElementSize=Property->GetElementSize();
    S->ArrayDim=Property->ArrayDim;S->Size=Property->GetSize();S->OwnerSize=S->Owner->GetPropertiesSize();S->Flags=Property->PropertyFlags;
    if(auto ArrayProperty=CastField<FArrayProperty>(Property)){S->ArrayFlags=ArrayProperty->ArrayFlags;}
    if(auto StructProperty=CastField<FStructProperty>(Property)){S->Struct=StructProperty->Struct;if(!C.Hold(S->Struct)){return S;}S->StructSize=S->Struct->GetStructureSize();}
    return S;
}
struct FProjectionScope
{
    FRead& Context;int32 Index;
    FProjectionScope(FRead& In,TFunction<bool()> Check):Context(In),Index(In.ProjectionChecks.Num()){Context.ProjectionChecks.Add(MoveTemp(Check));}
    ~FProjectionScope(){Context.ProjectionChecks.SetNum(Index,EAllowShrinking::No);}
};
bool PropertyCurrent(FRead& C,FProperty* P,FName Name,UStruct* Owner)
{return C.Current()&&P&&FindFProperty<FProperty>(C.Anim->GetClass(),Name)==P&&P->GetOwnerStruct()==Owner&&P->GetOffset_ForInternal()>=0&&P->GetSize()>0&&P->ArrayDim>=1&&P->IsInContainer(C.Anim->GetClass());}
int64 Integer(const FNumericProperty* P,const void* Ptr)
{return (CastField<FUInt64Property>(P)||CastField<FUInt32Property>(P)||CastField<FUInt16Property>(P))?int64(P->GetUnsignedIntPropertyValue(Ptr)):P->GetSignedIntPropertyValue(Ptr);}
TSharedPtr<FJsonValue> PropertyJson(FRead& C,const FProperty* P,const void* Ptr,int32 Depth=0)
{
    if(!P||!Ptr){return MakeShared<FJsonValueNull>();}if(!C.Current()||!C.Row()||Depth>MaxDepth){C.Bad(TEXT("Runtime property projection exceeds finite depth/row budget"));return MakeShared<FJsonValueNull>();}
    if(auto B=CastField<FBoolProperty>(P)){return MakeShared<FJsonValueBoolean>(B->GetPropertyValue(Ptr));}
    if(auto E=CastField<FEnumProperty>(P)){return Enum(E->GetEnum(),Integer(E->GetUnderlyingProperty(),Ptr));}
    if(auto B=CastField<FByteProperty>(P)){if(B->Enum){return Enum(B->Enum,B->GetPropertyValue(Ptr));}return MakeShared<FJsonValueNumber>(B->GetPropertyValue(Ptr));}
    if(auto N=CastField<FNumericProperty>(P))
    {double V=N->IsFloatingPoint()?N->GetFloatingPointPropertyValue(Ptr):double(Integer(N,Ptr));if(!FMath::IsFinite(V)){C.Bad(TEXT("Property contains non-finite value"));return MakeShared<FJsonValueNull>();}return MakeShared<FJsonValueNumber>(V);}
    FString Text;
    if(auto N=CastField<FNameProperty>(P)){Text=N->GetPropertyValue(Ptr).ToString();}
    else if(auto StringProperty=CastField<FStrProperty>(P)){Text=StringProperty->GetPropertyValue(Ptr);}
    else if(auto TextProperty=CastField<FTextProperty>(P)){Text=TextProperty->GetPropertyValue(Ptr).ToString();}
    else if(auto ObjectProperty=CastField<FObjectPropertyBase>(P))
    {auto O=ObjectProperty->GetObjectPropertyValue(Ptr);if(!O){return MakeShared<FJsonValueNull>();}if(!C.Hold(O)||!C.Current()){return MakeShared<FJsonValueNull>();}Text=O->GetPathName();}
    else if(auto SoftObjectProperty=CastField<FSoftObjectProperty>(P)){Text=SoftObjectProperty->GetPropertyValue(Ptr).ToString();}
    else if(auto SoftClassProperty=CastField<FSoftClassProperty>(P)){Text=SoftClassProperty->GetPropertyValue(Ptr).ToString();}
    else if(auto StructProperty=CastField<FStructProperty>(P))
    {
        auto J=MakeShared<FJsonObject>();UScriptStruct* T=StructProperty->Struct;if(!T||!C.Hold(T)||!C.Current()){return MakeShared<FJsonValueNull>();}
        if(!C.Text(J,TEXT("_struct_type"),T->GetName())){return MakeShared<FJsonValueObject>(J);}
        if(Depth>=3)
        {P->ExportTextItem_Direct(Text,Ptr,nullptr,nullptr,PPF_None);if(!C.Current()){return MakeShared<FJsonValueObject>(J);}C.Text(J,TEXT("_export_text"),Text);return MakeShared<FJsonValueObject>(J);}
        struct FField{FName Name;TSharedRef<FFieldShape> Shape;};TArray<FField> Fields;
        for(TFieldIterator<FProperty> It(T,EFieldIterationFlags::IncludeSuper);It;++It)
        {FProperty* Field=*It;if(Fields.Num()>=MaxRows){C.Bad(TEXT("Struct fields exceed row bound"));return MakeShared<FJsonValueObject>(J);}const FName FieldName=Field->GetFName();auto Shape=CaptureFieldShape(C,Field,[T,FieldName](){return FindFProperty<FProperty>(T,FieldName);});Fields.Add({FieldName,Shape});if(!C.Current()){return MakeShared<FJsonValueObject>(J);}}
        FProjectionScope StructScope(C,[Fields](){for(const auto& Field:Fields){if(!Field.Shape->Current()){return false;}}return true;});
        for(const auto& F:Fields)
        {if(!C.Current()||!F.Shape->Current()||!F.Shape->Property->IsInContainer(T)){C.Bad(TEXT("Struct projection property identity/layout changed"));break;}auto VJ=PropertyJson(C,F.Shape->Property,F.Shape->Property->ContainerPtrToValuePtr<void>(Ptr),Depth+1);J->SetField(F.Name.ToString(),VJ);if(!C.Current()){break;}}
        return MakeShared<FJsonValueObject>(J);
    }
    else if(auto ArrayProperty=CastField<FArrayProperty>(P))
    {
        TArray<TSharedPtr<FJsonValue>> A;FScriptArrayHelper H(ArrayProperty,Ptr);const int32 Count=H.Num();
        if(Count<0||Count>MaxRows){C.Bad(TEXT("Property array exceeds finite rows"));return MakeShared<FJsonValueArray>(A);}
        auto Inner=CaptureFieldShape(C,ArrayProperty->Inner,[ArrayProperty](){return ArrayProperty->Inner;});
        const void* Data=Count?H.GetRawPtr(0):nullptr;const EArrayPropertyFlags StorageFlags=ArrayProperty->ArrayFlags;
        FProjectionScope ArrayScope(C,[ArrayProperty,Ptr,Inner,Count,Data,StorageFlags](){if(ArrayProperty->ArrayFlags!=StorageFlags||!Inner->Current()){return false;}FScriptArrayHelper Now(ArrayProperty,Ptr);return Now.Num()==Count&&(!Count||Now.GetRawPtr(0)==Data);});
        for(int32 I=0;I<Count;++I){if(!C.Current()){break;}A.Add(PropertyJson(C,Inner->Property,H.GetRawPtr(I),Depth+1));if(!C.Current()){break;}}
        return MakeShared<FJsonValueArray>(A);
    }
    else{P->ExportTextItem_Direct(Text,Ptr,nullptr,nullptr,PPF_None);if(!C.Current()){return MakeShared<FJsonValueNull>();}}
    auto J=MakeShared<FJsonObject>();if(!C.Text(J,TEXT("v"),Text)){return MakeShared<FJsonValueNull>();}return MakeShared<FJsonValueString>(Text);
}
bool SameJson(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B)
{FString X,Y;auto WX=TJsonWriterFactory<>::Create(&X);auto WY=TJsonWriterFactory<>::Create(&Y);FJsonSerializer::Serialize(TArray<TSharedPtr<FJsonValue>>{A},WX);FJsonSerializer::Serialize(TArray<TSharedPtr<FJsonValue>>{B},WY);return X==Y;}
bool TickParticipants(FRead& C)
{
    if(!C.Current()){return false;}const auto* Component=static_cast<const USkeletalMeshComponent*>(C.Component);
    TArray<UAnimInstance*> Participants=Component->GetLinkedAnimInstances();Participants.AddUnique(C.Component->GetAnimInstance());Participants.AddUnique(C.Component->GetPostProcessInstance());
    if(Participants.Num()>MaxRows){return C.Bad(TEXT("Tick participant inventory exceeds finite rows"));}
    for(UAnimInstance* A:Participants)
    {if(!A){continue;}if(!C.Hold(A)||!C.Current()||A->GetOuter()!=C.Component||!A->IsInitialized()||A->IsRunningParallelEvaluation()){return C.Bad(TEXT("Tick participant is uninitialized, parallel, unreadable or has changed owner; no blocking completion attempted"));}}
    return C.Current();
}
bool RefreshCurrentAnim(FRead& C)
{
    C.Anim=nullptr;if(!C.Current()){return false;}Call(C.R,TEXT("anim_instance_calls"));UAnimInstance* A=C.SelectedAnim();
    if(!A||!C.Hold(A)||!C.Current()){return C.Bad(TEXT("Current selected AnimInstance disappeared after component SDK call"));}C.Anim=A;return C.Current();
}
TSharedRef<FJsonObject> AnimJson(UAnimInstance* A)
{auto J=MakeShared<FJsonObject>();if(A){J->SetStringField(TEXT("name"),A->GetName());J->SetStringField(TEXT("path"),A->GetPathName());J->SetStringField(TEXT("class"),A->GetClass()->GetPathName());J->SetStringField(TEXT("outer"),A->GetOuter()?A->GetOuter()->GetPathName():FString());}return J;}
}

Variable::FResult Set(const FString& ActorLabel,const FString& ActorName,const FString& ActorPath,const FString& ComponentName,const FString& PropertiesJson,
    const FString& PropertyName,const FString& ValueJson,bool bTickAfterSet,int64 TickCount,double TickDeltaTime,bool bRefreshBoneTransforms,
    bool bIncludeSnapshotAfter,bool bIncludePreviousValues,const FString& StateMachineName,bool bIncludeStates,bool bIncludeStateWeights,
    bool bIncludeRelevantAnimTimes,bool bIncludeTransitionProgress,bool bIncludeInactiveTransitionProgress,bool bIncludeMontages,bool bIncludeCurves,
    const FString& CurveNamesJson,int64 MaxTransitionsPerMachine,bool bPreferPIEWorld,bool bRequirePIEWorld,const FString& AnimInstanceSource)

{
    FResult R;R.Data->SetStringField(TEXT("command"),TEXT("set_anim_instance_runtime_property_for_probe"));auto Counts=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("native_call_counts"),Counts);
    for(const TCHAR* Key:{TEXT("world_discovery_calls"),TEXT("actor_label_calls"),TEXT("component_inventory_calls"),TEXT("anim_instance_calls"),TEXT("state_machine_instance_calls"),TEXT("current_montage_calls"),TEXT("all_curve_names_calls"),TEXT("curve_value_calls"),TEXT("explicit_property_writer_calls"),TEXT("explicit_tick_calls"),TEXT("returned_tick_calls"),TEXT("explicit_refresh_calls"),TEXT("returned_refresh_calls"),TEXT("explicit_load_calls"),TEXT("explicit_modify_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls"),TEXT("explicit_initialize_calls"),TEXT("starts_pie_sie_calls")}){Counts->SetNumberField(Key,0);}
    for(const TCHAR* Key:{TEXT("read_only"),TEXT("asset_modified"),TEXT("saves_assets"),TEXT("starts_sie"),TEXT("observation_incomplete"),TEXT("automatic_action_retry"),TEXT("atomic_rollback_available"),TEXT("asset_dirty_unchanged_verified"),TEXT("all_SDK_effects_verified_absent"),TEXT("unique_external_pipeline_complete"),TEXT("packaged_runtime_verified"),TEXT("observed_explicit_change")}){R.Data->SetBoolField(Key,false);}
    R.Data->SetBoolField(TEXT("runtime_only"),true);R.Data->SetStringField(TEXT("runtime_source"),TEXT("active_anim_instance"));R.Data->SetStringField(TEXT("runtime_note"),TEXT("Sets only current live AnimInstance reflected runtime storage, then optionally ticks/refreshes its component and snapshots it. No explicit asset Modify/compiler/save/session start is dispatched. Arbitrary SDK/animation callbacks and lazy proxy effects remain unknown; no automatic retry or rollback."));
    R.Data->SetStringField(TEXT("asset_modified_scope"),TEXT("Legacy false means no explicit asset writer, not proof that SDK/custom callbacks kept all assets unchanged"));
    R.Data->SetStringField(TEXT("requested_anim_instance_source"),TEXT("main"));R.Data->SetStringField(TEXT("anim_instance_source"),TEXT("main"));
    R.Data->SetBoolField(TEXT("prefer_pie_world"),bPreferPIEWorld);R.Data->SetBoolField(TEXT("require_pie_world"),bRequirePIEWorld);
    R.Data->SetStringField(TEXT("requested_actor_label"),ActorLabel);R.Data->SetStringField(TEXT("requested_actor_name"),ActorName);R.Data->SetStringField(TEXT("requested_actor_path"),ActorPath);R.Data->SetStringField(TEXT("requested_component_name"),ComponentName);
    R.Data->SetArrayField(TEXT("errors"),{});R.Data->SetArrayField(TEXT("warnings"),{});
    if(!IsEditorGameThread()||IsEngineExitRequested()||InFlight){return Fail(R,TEXT("anim_runtime_property_editor_unavailable"),TEXT("A live Editor game thread is required; PIE/SIE use is allowed, no session is started"));}
    TGuardValue<bool> Busy(InFlight,true);R.Stage=TEXT("validate_anim_runtime_property_inputs");
    for(const FString* S:{&ActorLabel,&ActorName,&ActorPath,&ComponentName,&PropertyName,&StateMachineName,&AnimInstanceSource}){if(!Bounded(*S)){return Fail(R,TEXT("anim_runtime_property_input_bound"),TEXT("Runtime selector/property name exceeds UTF16/NUL bound"));}}
    FString Source=AnimInstanceSource.TrimStartAndEnd();if(Source.IsEmpty()){Source=TEXT("main");}const FString LowerSource=Source.ToLower();
    const bool UseMain=LowerSource==TEXT("main")||LowerSource==TEXT("primary")||LowerSource==TEXT("anim_instance");
    const bool UsePost=LowerSource==TEXT("post_process")||LowerSource==TEXT("postprocess")||LowerSource==TEXT("post_process_anim_bp")||LowerSource==TEXT("postprocess_anim_bp");
    if(!UseMain&&!UsePost){return Fail(R,TEXT("anim_runtime_instance_source_invalid"),FString::Printf(TEXT("Unsupported anim_instance_source='%s'. Expected 'main' or 'post_process'"),*Source));}
    R.Data->SetStringField(TEXT("requested_anim_instance_source"),Source);R.Data->SetStringField(TEXT("anim_instance_source"),UsePost?TEXT("post_process"):TEXT("main"));
    if(!FMath::IsFinite(TickDeltaTime)){return Fail(R,TEXT("anim_runtime_property_delta_invalid"),TEXT("tick_delta_time must be finite"));}
    const double Delta=FMath::Clamp(TickDeltaTime,0.0,1.0);const int32 Ticks=bTickAfterSet?int32(FMath::Clamp<int64>(TickCount,0,240)):0;
    const int32 TransitionLimit=int32(FMath::Clamp<int64>(MaxTransitionsPerMachine,0,4096));
    R.Data->SetStringField(TEXT("received_tick_count_decimal"),LexToString(TickCount));R.Data->SetStringField(TEXT("received_max_transitions_per_machine_decimal"),LexToString(MaxTransitionsPerMachine));R.Data->SetNumberField(TEXT("received_tick_delta_time"),TickDeltaTime);
    TSharedPtr<FJsonValue> Properties,Value;FString ParseError;
    if(!Decode(PropertiesJson,Properties,ParseError)||!Decode(ValueJson,Value,ParseError)){return Fail(R,TEXT("anim_runtime_property_json_invalid"),ParseError);}
    TArray<FAssignment> Work;if(!Assignments(Properties,PropertyName,Value,Work)){return Fail(R,TEXT("anim_runtime_property_assignments_invalid"),TEXT("PropertiesJson requires the fixed properties/property_values projection object"));}
    TArray<FString> RequestedCurves;if(!CurvesInput(CurveNamesJson,RequestedCurves,ParseError)){return Fail(R,TEXT("anim_runtime_curves_invalid"),ParseError);}
    FRead C(R);C.UsePostProcess=UsePost;TArray<TSharedPtr<FJsonValue>> Warnings,Errors;
    auto Prefix=[&](){R.Data->SetArrayField(TEXT("warnings"),Warnings);R.Data->SetArrayField(TEXT("errors"),Errors);R.Data->SetNumberField(TEXT("completed_output_rows"),C.Rows);R.Data->SetBoolField(TEXT("total_change_state_known"),R.bChangeStateKnown);R.Data->SetBoolField(TEXT("observed_explicit_change"),R.bChanged);
        TArray<TSharedPtr<FJsonValue>> Dirty;bool ObservedDirtyChange=false;int32 Count=0;
        for(const auto& KV:C.InitialPackageDirty){if(Count++>=128){break;}auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("before"),KV.Value);if(Loaded(KV.Key)){J->SetStringField(TEXT("captured_actual_package"),KV.Key->GetPathName());const bool After=KV.Key->IsDirty();J->SetBoolField(TEXT("after"),After);ObservedDirtyChange|=After!=KV.Value;}else{J->SetField(TEXT("after"),MakeShared<FJsonValueNull>());}Dirty.Add(MakeShared<FJsonValueObject>(J));}
        R.Data->SetArrayField(TEXT("captured_package_dirty_observations"),Dirty);R.Data->SetBoolField(TEXT("captured_package_dirty_observations_truncated"),C.InitialPackageDirty.Num()>128);R.Data->SetBoolField(TEXT("observed_captured_package_dirty_change"),ObservedDirtyChange);R.Data->SetStringField(TEXT("dirty_observation_scope"),TEXT("At-most128 captured actual package first-sighting flags; no unchanged/atomic/current whole-asset proof"));};
    auto Incomplete=[&](){Prefix();return Fail(R,TEXT("anim_runtime_property_incomplete"),C.Error.IsEmpty()?TEXT("Current target or runtime observation became incomplete; partial prefix retained, no retry"):C.Error);};
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
    if(MatchedActors>1){Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Matched %d actors; set runtime properties on the first matching actor in world priority order"),MatchedActors)));}
    if(ComponentName.IsEmpty()&&MatchedComponents>1){Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Actor has %d SkeletalMeshComponents; set runtime properties on the first component because component_name was not provided"),MatchedComponents)));}
    R.Stage=TEXT("resolve_anim_runtime_property_target");if(!C.Current()){return Incomplete();}Call(R,TEXT("anim_instance_calls"));UAnimInstance* Instance=C.SelectedAnim();
    if(Instance&&!C.Hold(Instance)){return Incomplete();}if(!C.Current()){return Incomplete();}
    if(!Instance)
    {UClass* Class=C.Component->GetAnimClass();if(Class&&!C.Hold(Class)){return Incomplete();}if(!C.Current()){return Incomplete();}Call(R,TEXT("post_process_class_calls"));UClass* PostClass=C.Component->GetPostProcessAnimBPClassToBeUsed().Get();if(PostClass&&!C.Hold(PostClass)){return Incomplete();}if(!C.Current()){return Incomplete();}Prefix();
        if(UsePost){const bool Disabled=C.Component->GetDisablePostProcessBlueprint();if(!C.Current()){return Incomplete();}return Fail(R,TEXT("anim_runtime_instance_missing"),FString::Printf(TEXT("Matched SkeletalMeshComponent '%s' has no post-process AnimInstance. anim_instance_source=post_process animation_mode=%s anim_class=%s post_process_class=%s post_process_disabled=%s"),*C.Component->GetName(),*AnimationMode(C.Component->GetAnimationMode()),Class?*Class->GetPathName():TEXT("<none>"),PostClass?*PostClass->GetPathName():TEXT("<none>"),Disabled?TEXT("true"):TEXT("false")));}
        return Fail(R,TEXT("anim_runtime_instance_missing"),FString::Printf(TEXT("Matched SkeletalMeshComponent '%s' has no AnimInstance. anim_instance_source=main animation_mode=%s anim_class=%s post_process_class=%s"),*C.Component->GetName(),*AnimationMode(C.Component->GetAnimationMode()),Class?*Class->GetPathName():TEXT("<none>"),PostClass?*PostClass->GetPathName():TEXT("<none>")));}
    C.Anim=Instance;if(!C.Current()){return Incomplete();}
    R.Data->SetBoolField(TEXT("is_play_session_active"),IsPlaySessionActive());R.Data->SetStringField(TEXT("sampled_world_type"),WorldType(C.World));R.Data->SetStringField(TEXT("sampled_world_name"),C.World->GetName());R.Data->SetNumberField(TEXT("matched_actor_count"),MatchedActors);R.Data->SetNumberField(TEXT("matched_component_count"),MatchedComponents);
    auto AJ=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("actor"),AJ);Call(R,TEXT("actor_label_calls"));const FString Label=C.Actor->GetActorLabel();if(!C.Current()){return Incomplete();}
    C.Text(AJ,TEXT("label"),Label);C.Text(AJ,TEXT("name"),C.Actor->GetName());C.Text(AJ,TEXT("path"),C.Actor->GetPathName());C.Text(AJ,TEXT("class"),C.Actor->GetClass()->GetPathName());
    auto CJ=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("component"),CJ);USkeletalMesh* Mesh=C.Component->GetSkeletalMeshAsset();if(Mesh&&!C.Hold(Mesh)){return Incomplete();}if(!C.Current()){return Incomplete();}
    UClass* AnimClass=C.Component->GetAnimClass();if(AnimClass&&!C.Hold(AnimClass)){return Incomplete();}if(!C.Current()){return Incomplete();}
    C.Text(CJ,TEXT("name"),C.Component->GetName());C.Text(CJ,TEXT("path"),C.Component->GetPathName());C.Text(CJ,TEXT("class"),C.Component->GetClass()->GetPathName());C.Text(CJ,TEXT("skeletal_mesh"),Mesh?Mesh->GetPathName():FString());C.Text(CJ,TEXT("anim_class"),AnimClass?AnimClass->GetPathName():FString());C.Text(CJ,TEXT("animation_mode"),AnimationMode(C.Component->GetAnimationMode()));
    const FTransform T=C.Component->GetComponentTransform();if(!C.Current()||T.ContainsNaN()){C.Bad(TEXT("Component transform is not a finite current sample"));return Incomplete();}CJ->SetObjectField(TEXT("component_transform"),Transform(T));
    auto IJ=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("anim_instance"),IJ);C.Text(IJ,TEXT("name"),Instance->GetName());C.Text(IJ,TEXT("path"),Instance->GetPathName());C.Text(IJ,TEXT("class"),Instance->GetClass()->GetPathName());C.Text(IJ,TEXT("outer"),Instance->GetOuter()?Instance->GetOuter()->GetPathName():FString());

    if(C.Anim->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject)||C.Anim->IsTemplate())
    {Prefix();return Fail(R,TEXT("anim_runtime_property_template_refused"),TEXT("Only a live component-owned instance can be written; CDO/templates are refused"));}
    if(Work.IsEmpty()){Prefix();return Fail(R,TEXT("anim_runtime_property_assignments_empty"),TEXT("No runtime properties requested. Provide 'properties' as an object/array or 'property_name' plus 'value'."));}
    auto Applied=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("applied_properties"),Applied);TArray<TSharedPtr<FJsonValue>> Rows;int32 Succeeded=0;
    auto AppliedPrefix=[&](){Applied->SetNumberField(TEXT("requested_count"),Work.Num());Applied->SetNumberField(TEXT("success_count"),Succeeded);Applied->SetBoolField(TEXT("all_succeeded"),Succeeded==Work.Num());Applied->SetArrayField(TEXT("assignments"),Rows);Prefix();};
    AppliedPrefix();R.Stage=TEXT("apply_anim_runtime_properties");
    for(const auto& Assignment:Work)
    {
        if(!C.Current()||!C.Row()){AppliedPrefix();return Incomplete();}
        auto Row=MakeShared<FJsonObject>();Rows.Add(MakeShared<FJsonValueObject>(Row));if(!C.Text(Row,TEXT("name"),Assignment.Name)){AppliedPrefix();return Incomplete();}Row->SetField(TEXT("requested_value"),Assignment.Value);AppliedPrefix();
        FName Name;FString Error;FProperty* P=nullptr;UStruct* Owner=nullptr;
        if(TryParseControlRigProbeName(Assignment.Name,TEXT("property name"),Name,Error)){P=FindFProperty<FProperty>(C.Anim->GetClass(),Name);}
        if(P)
        {
            Owner=P->GetOwnerStruct();if(!C.Hold(Owner)||!PropertyCurrent(C,P,Name,Owner)){AppliedPrefix();return Incomplete();}
            Row->SetStringField(TEXT("property_type"),P->GetClass()->GetName());Row->SetNumberField(TEXT("property_flags"),double(P->PropertyFlags));
        }
        TUniquePtr<FProjectionScope> RootPropertyScope;
        if(P)
        {
            auto Shape=CaptureFieldShape(C,P,[Owner,Name](){return FindFProperty<FProperty>(Owner,Name);});
            UAnimInstance* RootInstance=C.Anim;UClass* ContainerClass=RootInstance->GetClass();const int32 ContainerSize=ContainerClass->GetPropertiesSize();
            const void* Address=P->ContainerPtrToValuePtr<void>(RootInstance);
            RootPropertyScope=MakeUnique<FProjectionScope>(C,[Shape,RootInstance,ContainerClass,ContainerSize,Address](){return Shape->Current()&&RootInstance->GetClass()==ContainerClass&&ContainerClass->GetPropertiesSize()==ContainerSize&&Shape->Property->IsInContainer(ContainerClass)&&Shape->Property->ContainerPtrToValuePtr<void>(RootInstance)==Address;});
            if(!C.Current()){AppliedPrefix();return Incomplete();}
        }
        TSharedPtr<FJsonValue> Previous=MakeShared<FJsonValueNull>();
        if(P){Previous=PropertyJson(C,P,P->ContainerPtrToValuePtr<void>(C.Anim));if(!PropertyCurrent(C,P,Name,Owner)){if(bIncludePreviousValues){Row->SetField(TEXT("previous_value"),Previous);}AppliedPrefix();return Incomplete();}}
        if(bIncludePreviousValues){Row->SetField(TEXT("previous_value"),Previous);}
        bool Dispatched=false;
        const bool Success=SetUObjectPropertyFromJson(C.Anim,Assignment.Name,Assignment.Value,Error,Dispatched);
        if(Dispatched){R.bMutationAttempted=true;Call(R,TEXT("explicit_property_writer_calls"));}
        Row->SetBoolField(TEXT("writer_dispatched"),Dispatched);Row->SetBoolField(TEXT("success"),Success);
        if(Success)
        {
            ++Succeeded;AppliedPrefix();if(!PropertyCurrent(C,P,Name,Owner)){return Incomplete();}
            auto Echo=PropertyJson(C,P,P->ContainerPtrToValuePtr<void>(C.Anim));Row->SetField(TEXT("echo_value"),Echo);
            if(!PropertyCurrent(C,P,Name,Owner)){AppliedPrefix();return Incomplete();}const bool Different=!SameJson(Previous,Echo);Row->SetBoolField(TEXT("observed_value_difference"),Different);R.bChanged|=Different;
        }
        else{Row->SetStringField(TEXT("error"),Error);Errors.Add(MakeShared<FJsonValueString>(Error));}
        AppliedPrefix();if(!C.Current()){return Incomplete();}
    }
    if(bTickAfterSet)
    {
        R.Stage=TEXT("tick_anim_runtime_component");auto Tick=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("tick_after_set"),Tick);Tick->SetStringField(TEXT("tick_method"),TEXT("USkeletalMeshComponent::TickAnimation"));Tick->SetNumberField(TEXT("requested_tick_count"),Ticks);Tick->SetNumberField(TEXT("tick_delta_time"),Delta);Tick->SetBoolField(TEXT("refresh_bone_transforms"),bRefreshBoneTransforms);Tick->SetStringField(TEXT("execution_count_scope"),TEXT("Public TickAnimation/RefreshBoneTransforms calls returned; internal update/evaluation/pose completion is uninstrumented"));Tick->SetBoolField(TEXT("synchronous_SDK_call_may_block"),true);Tick->SetNumberField(TEXT("executed_tick_count"),0);Tick->SetNumberField(TEXT("executed_refresh_count"),0);Tick->SetBoolField(TEXT("success"),false);
        for(int32 I=0;I<Ticks;++I)
        {
            if(!TickParticipants(C)){AppliedPrefix();return Incomplete();}R.bMutationAttempted=true;Call(R,TEXT("explicit_tick_calls"));
            // No explicit task completion/wait is requested. This public SDK call
            // may itself update instances/dispatch animation notifies synchronously.
            C.Anim=nullptr;C.Component->TickAnimation(float(Delta),false);Call(R,TEXT("returned_tick_calls"));Tick->SetNumberField(TEXT("executed_tick_count"),I+1);AppliedPrefix();
            if(!RefreshCurrentAnim(C)){return Incomplete();}
            if(bRefreshBoneTransforms)
            {
                if(!TickParticipants(C)){AppliedPrefix();return Incomplete();}Call(R,TEXT("explicit_refresh_calls"));C.Anim=nullptr;C.Component->RefreshBoneTransforms();Call(R,TEXT("returned_refresh_calls"));Tick->SetNumberField(TEXT("executed_refresh_count"),I+1);AppliedPrefix();
                if(!RefreshCurrentAnim(C)){return Incomplete();}
            }
        }
        if(Ticks>0)
        {if(!C.Current()){return Incomplete();}Call(R,TEXT("should_tick_animation_calls"));const bool Should=C.Component->ShouldTickAnimation();if(!C.Current()){return Incomplete();}if(!Should){Warnings.Add(MakeShared<FJsonValueString>(TEXT("SkeletalMeshComponent::ShouldTickAnimation returned false; forced TickAnimation was still called for the probe")));}}
        Tick->SetBoolField(TEXT("success"),true);AppliedPrefix();
    }
    if(bIncludeSnapshotAfter)
    {
        R.Stage=TEXT("snapshot_anim_runtime_properties");C.UsePostProcess=false;R.Data->SetBoolField(TEXT("snapshot_original_main_refresh_semantics"),true);if(!RefreshCurrentAnim(C)){return Incomplete();}auto Snapshot=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("snapshot_after"),Snapshot);Snapshot->SetObjectField(TEXT("anim_instance"),AnimJson(C.Anim));C.Output=Snapshot;C.RequireInitialized=true;
        if(!C.Current()||!Machines(C,StateMachineName,bIncludeStates,bIncludeStateWeights,bIncludeRelevantAnimTimes,bIncludeTransitionProgress,bIncludeInactiveTransitionProgress,32,256,64,TransitionLimit,Warnings)){AppliedPrefix();return Incomplete();}
        if(bIncludeMontages&&!Montage(C)){AppliedPrefix();return Incomplete();}if(bIncludeCurves&&!Curves(C,RequestedCurves,128,Warnings)){AppliedPrefix();return Incomplete();}
    }
    AppliedPrefix();if(!C.Current()){return Incomplete();}R.bSuccess=Errors.IsEmpty();R.bPostconditionPassed=R.bSuccess;R.Stage=R.bSuccess?TEXT("anim_runtime_properties_complete"):TEXT("anim_runtime_properties_partial_failure");R.Data->SetBoolField(TEXT("success"),R.bSuccess);
    if(!R.bSuccess){R.ErrorCode=TEXT("anim_runtime_property_assignment_failed");R.Error=TEXT("One or more runtime properties failed; prior applied values/tick/snapshot remain, no rollback or automatic retry");}return R;
}
}

// FA specific case sampler: one include, same reviewed private helper TU.
#include "Compatibility/UE58/CubelessAnimStateRuntimeResponseImpl.inl"
