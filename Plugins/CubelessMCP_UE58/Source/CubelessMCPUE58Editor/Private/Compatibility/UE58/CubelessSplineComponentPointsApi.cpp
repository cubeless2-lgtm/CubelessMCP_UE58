// UE56 PCGCommands::HandleSetSplineComponentPoints, using public UE5.8 APIs only.
// Registration, transport, Python selection and durable operation journals remain outside this adapter.
#include "Compatibility/UE58/CubelessSplineComponentPointsApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Components/SplineComponent.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "Misc/Crc.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/WeakObjectPtr.h"

namespace Cubeless::UE58::SplineComponentPoints
{
namespace
{
using Variable::FResult;
constexpr int32 MaxJsonChars=1024*1024,MaxPoints=65536,MaxActors=65536,MaxComponents=4096;
bool Running=false;

bool Live(const UObject* Object)
{
    return IsValid(Object)&&!Object->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);
}
bool SafeText(const FString& Text,int32 Limit)
{
    if(Text.Len()>Limit) { return false; }
    for(TCHAR C:Text) { if(C==0) { return false; } }
    return true;
}
// JSON keys are case-sensitive; default FString set equality/hash is not.
struct FCaseSensitiveJsonKeyFuncs : DefaultKeyFuncs<FString>
{
    static bool Matches(const FString& A,const FString& B) { return A.Equals(B,ESearchCase::CaseSensitive); }
    static uint32 GetKeyHash(const FString& Key) { return FCrc::StrCrc32(*Key); }
};
bool Parse(const FString& Text,TSharedPtr<FJsonValue>& Value)
{
    if(Text.IsEmpty()||!SafeText(Text,MaxJsonChars)) { return false; }
    auto Reader=TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]"));
    TArray<TSet<FString,FCaseSensitiveJsonKeyFuncs>> Keys;TArray<bool> Objects;EJsonNotation Token;int32 Slots=0;
    while(Reader->ReadNext(Token))
    {
        if(Token==EJsonNotation::Error||++Slots>MaxPoints*8+1024) { return false; }
        if(Token!=EJsonNotation::ObjectEnd&&Token!=EJsonNotation::ArrayEnd&&!Objects.IsEmpty()&&Objects.Last())
        {
            const FString Key(Reader->GetIdentifier());
            if(Keys.Last().Contains(Key)) { return false; }
            Keys.Last().Add(Key);
        }
        if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart)
        { Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();if(Objects.Num()>16) { return false; } }
        else if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd)
        { if(Objects.IsEmpty()) { return false; }Objects.Pop();Keys.Pop(); }
    }
    TArray<TSharedPtr<FJsonValue>> Values;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]")),Values)||Values.Num()!=1||!Values[0].IsValid()) { return false; }
    Value=Values[0];return true;
}
TArray<TSharedPtr<FJsonValue>> VectorJson(const FVector& V)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    for(double N:{V.X,V.Y,V.Z}) { if(FMath::IsFinite(N)) { Values.Add(MakeShared<FJsonValueNumber>(N)); }else { Values.Add(MakeShared<FJsonValueNull>()); } }
    return Values;
}
FString JsonText(const TSharedRef<FJsonObject>& Object)
{
    FString Text;FJsonSerializer::Serialize(Object,TJsonWriterFactory<>::Create(&Text));return Text;
}
TArray<TSharedPtr<FJsonValue>> NamesJson(const TArray<FName>& Names)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    for(const FName& Name:Names) { Values.Add(MakeShared<FJsonValueString>(Name.ToString())); }
    return Values;
}
TSharedRef<FJsonObject> ActorJson(const AActor* Actor)
{
    auto J=MakeShared<FJsonObject>();
    if(!Live(Actor)) { return J; }
    J->SetStringField(TEXT("label"),Actor->GetActorLabel());J->SetStringField(TEXT("name"),Actor->GetName());
    J->SetStringField(TEXT("path"),Actor->GetPathName());J->SetArrayField(TEXT("tags"),NamesJson(Actor->Tags));return J;
}
TSharedRef<FJsonObject> SplineJson(USplineComponent* Spline)
{
    auto J=MakeShared<FJsonObject>();
    if(!Live(Spline)) { return J; }
    J->SetStringField(TEXT("name"),Spline->GetName());J->SetStringField(TEXT("path"),Spline->GetPathName());
    J->SetBoolField(TEXT("is_trash_component"),Spline->GetName().StartsWith(TEXT("TRASH_")));
    J->SetNumberField(TEXT("point_count"),Spline->GetNumberOfSplinePoints());
    const double Length=Spline->GetSplineLength();
    if(FMath::IsFinite(Length)) { J->SetNumberField(TEXT("spline_length"),Length); }else { J->SetField(TEXT("spline_length"),MakeShared<FJsonValueNull>()); }
    J->SetBoolField(TEXT("closed_loop"),Spline->IsClosedLoop());J->SetArrayField(TEXT("tags"),NamesJson(Spline->ComponentTags));return J;
}
TSharedRef<FJsonObject> Observation(AActor* Actor,USplineComponent* Spline,ESplineCoordinateSpace::Type Space)
{
    auto J=MakeShared<FJsonObject>();const bool ActorLive=Live(Actor),SplineLive=Live(Spline);
    J->SetBoolField(TEXT("actor_valid"),ActorLive);J->SetBoolField(TEXT("component_valid"),SplineLive);
    if(ActorLive)
    {
        J->SetObjectField(TEXT("actor"),ActorJson(Actor));J->SetStringField(TEXT("actor_class"),Actor->GetClass()->GetPathName());
        J->SetStringField(TEXT("world"),Actor->GetWorld()?Actor->GetWorld()->GetPathName():FString());
        J->SetBoolField(TEXT("actor_package_dirty"),Actor->GetPackage()->IsDirty());
    }
    if(SplineLive)
    {
        J->SetObjectField(TEXT("spline_component"),SplineJson(Spline));J->SetStringField(TEXT("component_class"),Spline->GetClass()->GetPathName());
        J->SetStringField(TEXT("owner_path"),Live(Spline->GetOwner())?Spline->GetOwner()->GetPathName():FString());
        J->SetBoolField(TEXT("component_package_dirty"),Spline->GetPackage()->IsDirty());
        J->SetBoolField(TEXT("input_to_construction_script"),Spline->bInputSplinePointsToConstructionScript);
        J->SetBoolField(TEXT("spline_has_been_edited"),Spline->bSplineHasBeenEdited);
        const int32 Count=Spline->GetNumberOfSplinePoints();J->SetBoolField(TEXT("readback_truncated"),Count>MaxPoints);
        TArray<TSharedPtr<FJsonValue>> Points,Types;
        for(int32 I=0;I<FMath::Min(Count,MaxPoints);++I)
        { Points.Add(MakeShared<FJsonValueArray>(VectorJson(Spline->GetLocationAtSplinePoint(I,Space))));Types.Add(MakeShared<FJsonValueNumber>(static_cast<int32>(Spline->GetSplinePointType(I)))); }
        J->SetArrayField(TEXT("points"),Points);J->SetArrayField(TEXT("point_types_enum"),Types);
    }
    return J;
}

struct FWork
{
    FResult Result;
    TSharedRef<FJsonObject> Legacy=MakeShared<FJsonObject>(),Calls=MakeShared<FJsonObject>(),Validation=MakeShared<FJsonObject>();
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<AActor> Actor;TWeakObjectPtr<USplineComponent> Original,Current;
    TSharedPtr<FJsonObject> Before;ESplineCoordinateSpace::Type Space=ESplineCoordinateSpace::World;
    bool HasTarget=false,ReconstructionDispatched=false;
    FWork()
    {
        Result.Compiled=false;
        Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("command"),TEXT("set_spline_component_points"));
        Result.Data->SetObjectField(TEXT("legacy_result"),Legacy);Result.Data->SetObjectField(TEXT("native_call_counts"),Calls);
        Result.Data->SetObjectField(TEXT("postconditions"),Validation);
        Result.Data->SetBoolField(TEXT("atomic_rollback_available"),false);Result.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);
        Result.Data->SetBoolField(TEXT("explicit_compile_requested"),false);Result.Data->SetBoolField(TEXT("explicit_save_requested"),false);
        Result.Data->SetBoolField(TEXT("disk_reload_verified"),false);Result.Data->SetBoolField(TEXT("PCG_generation_waited"),false);
        Result.Data->SetBoolField(TEXT("implicit_Engine_writes_measured"),false);
        auto Limits=MakeShared<FJsonObject>();Limits->SetNumberField(TEXT("JSON_UTF16_characters"),MaxJsonChars);Limits->SetNumberField(TEXT("points"),MaxPoints);
        Limits->SetNumberField(TEXT("scanned_actors"),MaxActors);Limits->SetNumberField(TEXT("spline_components"),MaxComponents);Result.Data->SetObjectField(TEXT("limits"),Limits);
        Result.Data->SetStringField(TEXT("retry_risk"),TEXT("Replaying setters can rebroadcast delegates, dirty packages and rerun arbitrary construction scripts; an uncertain request must be inspected rather than automatically replayed."));
        Result.Data->SetStringField(TEXT("total_change_scope"),TEXT("Selected actor/component public projection and explicit wrapper calls only; delegate/UserConstructionScript/transaction/render/PCG side effects are not a complete global change inventory."));
        Result.Data->SetStringField(TEXT("synchronous_SDK_limit"),TEXT("Spline setters and RerunConstructionScripts are public synchronous game-thread operations; arbitrary callbacks cannot be preempted by this adapter."));
        Result.Data->SetStringField(TEXT("mark_dirty_option_scope"),TEXT("False omits the two explicit MarkPackageDirty calls; Modify() defaults to bAlwaysMarkDirty=true and can still dirty packages."));
        for(const TCHAR* Key:{TEXT("actor_modify_calls"),TEXT("actor_modify_returns"),TEXT("component_modify_calls"),TEXT("component_modify_returns"),TEXT("input_flag_writes"),TEXT("override_set_calls"),TEXT("override_set_returns"),TEXT("set_points_calls"),TEXT("set_points_returns"),TEXT("set_point_type_calls"),TEXT("set_point_type_returns"),TEXT("closed_loop_set_calls"),TEXT("closed_loop_set_returns"),TEXT("update_spline_calls"),TEXT("update_spline_returns"),TEXT("mark_render_state_dirty_calls"),TEXT("mark_render_state_dirty_returns"),TEXT("mark_package_dirty_calls"),TEXT("mark_package_dirty_returns"),TEXT("rerun_construction_scripts_calls"),TEXT("rerun_construction_scripts_returns"),TEXT("compile_blueprint_calls"),TEXT("asset_save_calls"),TEXT("map_save_calls"),TEXT("start_pie_calls"),TEXT("stop_pie_calls")}) { Calls->SetNumberField(Key,0); }
    }
    void Count(const TCHAR* Key)
    { double N=0;Calls->TryGetNumberField(Key,N);Calls->SetNumberField(Key,N+1); }
    bool Fail(const TCHAR* Code,const FString& Message)
    {
        if(Result.Error.IsEmpty()) { Result.ErrorCode=FString(TEXT("spline_component_points_"))+Code;Result.Error=Message; }
        Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("error"),Result.Error);return false;
    }
    bool Admit(bool WithComponent=true)
    {
        if(!IsEditorGameThread()||IsPlaySessionActive()||!GEditor||!Live(World.Get())||GEditor->GetEditorWorldContext().World()!=World.Get())
        { return Fail(TEXT("editor_changed"),TEXT("Idle editor game-thread/world admission changed; no further writer was dispatched")); }
        if(!Live(Actor.Get())||Actor->GetWorld()!=World.Get())
        { return Fail(TEXT("actor_changed"),TEXT("Selected actor is no longer valid in the captured Editor world")); }
        if(WithComponent&&(!Live(Current.Get())||Current->GetOwner()!=Actor.Get()||Current->GetWorld()!=World.Get()))
        { return Fail(TEXT("component_changed"),TEXT("Selected spline was destroyed/reparented during an engine callback; no automatic replacement or mutation replay")); }
        return true;
    }
    bool Writer(const TCHAR* Stage,const TCHAR* Counter,bool WithComponent=true)
    {
        Result.Stage=Stage;if(!Admit(WithComponent)) { return false; }
        Result.bMutationAttempted=true;Result.bChangeStateKnown=false;Count(Counter);return true;
    }
    FResult Finish()
    {
        if(HasTarget)
        {
            const auto After=Observation(Actor.Get(),Current.Get(),Space);
            Result.Data->SetObjectField(TEXT("target_after"),After);
            Result.Data->SetBoolField(TEXT("original_component_valid_after"),Live(Original.Get()));
            Result.Data->SetBoolField(TEXT("component_instance_replaced"),Live(Current.Get())&&Original.Get()!=Current.Get());
            Result.Data->SetBoolField(TEXT("original_component_identity_lost"),!Live(Original.Get()));
            if(Before.IsValid()&&Live(Actor.Get())&&Live(Current.Get()))
            {
                const bool Changed=JsonText(Before.ToSharedRef())!=JsonText(After)||Original.Get()!=Current.Get();
                Result.bChanged=Changed;Result.Data->SetBoolField(TEXT("observed_projection_changed"),Changed);
            }
            else { Result.Data->SetField(TEXT("observed_projection_changed"),MakeShared<FJsonValueNull>()); }
        }
        Result.Data->SetBoolField(TEXT("construction_scripts_dispatched"),ReconstructionDispatched);
        Result.Data->SetBoolField(TEXT("total_change_state_unknown"),!Result.bChangeStateKnown);
        Result.Data->SetBoolField(TEXT("explicit_compile_attempted"),Result.bCompileAttempted);Result.Data->SetBoolField(TEXT("explicit_save_attempted"),Result.bSaveAttempted);
        return Result;
    }
};

bool ContextString(FWork& W,const TSharedRef<FJsonObject>& J,const TCHAR* Key,FString& Out)
{
    if(!J->HasField(Key)) { return true; }
    const auto Value=J->TryGetField(Key);
    if(Value.IsValid()&&Value->Type==EJson::Null) { Out.Empty();return true; }
    if(!Value.IsValid()||Value->Type!=EJson::String||!J->TryGetStringField(Key,Out)||!SafeText(Out,65536))
    { return W.Fail(TEXT("context_invalid"),FString::Printf(TEXT("Private '%s' must be a bounded string or null"),Key)); }
    return true;
}
bool ContextBool(FWork& W,const TSharedRef<FJsonObject>& J,const TCHAR* Key,bool& Out)
{
    if(!J->HasField(Key)) { return true; }
    if(!J->HasTypedField<EJson::Boolean>(Key)||!J->TryGetBoolField(Key,Out))
    { return W.Fail(TEXT("context_invalid"),FString::Printf(TEXT("Private '%s' must be an exact JSON Boolean"),Key)); }
    return true;
}
bool Point(FWork& W,const TSharedPtr<FJsonValue>& Value,int32 Index,FVector& Out)
{
    double XYZ[3]={0,0,0};
    if(Value.IsValid()&&Value->Type==EJson::Array)
    {
        const auto& A=Value->AsArray();if(A.Num()<3) { return W.Fail(TEXT("points_invalid"),FString::Printf(TEXT("Point %d must contain at least 3 numeric values"),Index)); }
        for(int32 C=0;C<3;++C)
        { if(!A[C].IsValid()||A[C]->Type!=EJson::Number||!A[C]->TryGetNumber(XYZ[C])) { return W.Fail(TEXT("points_invalid"),FString::Printf(TEXT("Point %d first three components must be exact JSON numbers"),Index)); } }
    }
    else if(Value.IsValid()&&Value->Type==EJson::Object)
    {
        const auto J=Value->AsObject();const TCHAR* Lower[3]={TEXT("x"),TEXT("y"),TEXT("z")};const TCHAR* Upper[3]={TEXT("X"),TEXT("Y"),TEXT("Z")};
        for(int32 C=0;C<3;++C)
        {
            auto V=J->TryGetField(Lower[C]);if(!V.IsValid()||V->Type!=EJson::Number) { V=J->TryGetField(Upper[C]); }
            if(!V.IsValid()||V->Type!=EJson::Number||!V->TryGetNumber(XYZ[C])) { return W.Fail(TEXT("points_invalid"),FString::Printf(TEXT("Point %d object must contain x/y/z numeric fields"),Index)); }
        }
    }
    else { return W.Fail(TEXT("points_invalid"),FString::Printf(TEXT("Point %d must be an array or object"),Index)); }
    // The UE56 native command rounded each component to float before constructing FVector.
    const float X=static_cast<float>(XYZ[0]),Y=static_cast<float>(XYZ[1]),Z=static_cast<float>(XYZ[2]);
    if(!FMath::IsFinite(XYZ[0])||!FMath::IsFinite(XYZ[1])||!FMath::IsFinite(XYZ[2])||!FMath::IsFinite(X)||!FMath::IsFinite(Y)||!FMath::IsFinite(Z))
    { return W.Fail(TEXT("points_invalid"),FString::Printf(TEXT("Point %d must remain finite after legacy float32 conversion"),Index)); }
    Out=FVector(X,Y,Z);return true;
}
bool ActorMatches(AActor* A,const FString& Label,const FString& Name,const FString& Path,const FString& Tag,const FString& Prefix)
{
    return Live(A)&&(Label.IsEmpty()||A->GetActorLabel().Equals(Label,ESearchCase::IgnoreCase))
        &&(Name.IsEmpty()||A->GetName().Equals(Name,ESearchCase::IgnoreCase))
        &&(Path.IsEmpty()||A->GetPathName().Equals(Path,ESearchCase::IgnoreCase))
        &&(Tag.IsEmpty()||A->Tags.Contains(FName(*Tag)))
        &&(Prefix.IsEmpty()||A->GetActorLabel().StartsWith(Prefix,ESearchCase::IgnoreCase));
}
USplineComponent* FindSpline(AActor* Actor,const FString& Name,const FString& Tag,TArray<TWeakObjectPtr<USplineComponent>>& Candidates,bool& TooMany)
{
    Candidates.Reset();TooMany=false;if(!Live(Actor)) { return nullptr; }
    TArray<USplineComponent*> Components;Actor->GetComponents<USplineComponent>(Components);
    if(Components.Num()>MaxComponents) { TooMany=true;return nullptr; }
    USplineComponent* First=nullptr;USplineComponent* Trash=nullptr;
    for(USplineComponent* C:Components)
    {
        if(!Live(C)) { continue; }Candidates.Add(TWeakObjectPtr<USplineComponent>(C));
        if((Name.IsEmpty()||C->GetName().Equals(Name,ESearchCase::IgnoreCase))&&(Tag.IsEmpty()||C->ComponentTags.Contains(FName(*Tag))))
        {
            if(!C->GetName().StartsWith(TEXT("TRASH_"))) { if(!First) { First=C; } }
            else if(!Trash) { Trash=C; }
        }
    }
    return First?First:Trash;
}
}

Variable::FResult Set(const FString& PointsJson,const FString& ActorLabel,const FString& ActorName,const FString& ActorTag,const FString& ActorLabelPrefix,const FString& ComponentName,const FString& ComponentTag,bool bSelectedOnly,bool bClosedLoop,const FString& PrivateContextJson)
{
    FWork W;
    if(!IsEditorGameThread()||!GEditor||IsPlaySessionActive())
    { W.Fail(TEXT("editor_required"),TEXT("Requires an idle Editor game thread; PIE/SIE is not an editor-world spline mutation target"));return W.Finish(); }
    if(Running) { W.Fail(TEXT("reentrant"),TEXT("A spline point operation is already active on this game thread"));return W.Finish(); }
    Running=true;ON_SCOPE_EXIT { Running=false; };
    W.World=GEditor->GetEditorWorldContext().World();
    if(!Live(W.World.Get())||W.World->WorldType!=EWorldType::Editor)
    { W.Fail(TEXT("world_missing"),TEXT("Failed to get editor world"));return W.Finish(); }
    for(const FString* Text:{&ActorLabel,&ActorName,&ActorTag,&ActorLabelPrefix,&ComponentName,&ComponentTag})
    { if(!SafeText(*Text,65536)) { W.Fail(TEXT("selector_invalid"),TEXT("Selectors exceed the text bound or contain a NUL"));return W.Finish(); } }
    if(ActorTag.Len()>=NAME_SIZE||ComponentTag.Len()>=NAME_SIZE)
    { W.Fail(TEXT("selector_invalid"),TEXT("Actor/component tags exceed the Engine FName bound"));return W.Finish(); }
    auto Context=MakeShared<FJsonObject>();const bool Raw=!PrivateContextJson.IsEmpty();
    if(Raw)
    {
        TSharedPtr<FJsonValue> Value;
        if(!Parse(PrivateContextJson,Value)||Value->Type!=EJson::Object)
        { W.Fail(TEXT("context_invalid"),TEXT("Private context must be empty Named carrier or a JSON object for raw command presence"));return W.Finish(); }
        Context=Value->AsObject().ToSharedRef();
        const TSet<FString> Allowed={TEXT("actor_path"),TEXT("allow_multiple_actor_matches"),TEXT("coordinate_space"),TEXT("point_type"),TEXT("input_to_construction_script"),TEXT("rerun_construction_scripts"),TEXT("mark_dirty"),TEXT("strict_tolerance"),TEXT("component_name"),TEXT("closed_loop")};
        for(const auto& Pair:Context->Values)
        { if(!Allowed.Contains(FString(Pair.Key.ToView()))) { W.Fail(TEXT("context_invalid"),TEXT("Private context contains an unknown key"));return W.Finish(); } }
    }
    W.Result.Data->SetObjectField(TEXT("private_context"),Context);W.Result.Data->SetBoolField(TEXT("raw_command_context"),Raw);
    FString ActorPath,SpaceName,TypeName,ChosenComponent=Raw?FString():ComponentName;
    bool AllowMultiple=false,InputToUCS=true,RerunUCS=false,MarkDirty=true,Strict=false,Closed=Raw?false:bClosedLoop;
    const bool HasClosed=Raw?Context->HasField(TEXT("closed_loop")):true;
    if(!ContextString(W,Context,TEXT("actor_path"),ActorPath)||!ContextString(W,Context,TEXT("coordinate_space"),SpaceName)
        ||!ContextString(W,Context,TEXT("point_type"),TypeName)||!ContextString(W,Context,TEXT("component_name"),ChosenComponent)
        ||!ContextBool(W,Context,TEXT("allow_multiple_actor_matches"),AllowMultiple)||!ContextBool(W,Context,TEXT("input_to_construction_script"),InputToUCS)
        ||!ContextBool(W,Context,TEXT("rerun_construction_scripts"),RerunUCS)||!ContextBool(W,Context,TEXT("mark_dirty"),MarkDirty)
        ||!ContextBool(W,Context,TEXT("strict_tolerance"),Strict)||!ContextBool(W,Context,TEXT("closed_loop"),Closed)) { return W.Finish(); }
    W.Space=SpaceName.Equals(TEXT("local"),ESearchCase::IgnoreCase)?ESplineCoordinateSpace::Local:ESplineCoordinateSpace::World;
    ESplinePointType::Type Type=ESplinePointType::Linear;
    if(TypeName.Equals(TEXT("curve"),ESearchCase::IgnoreCase)) { Type=ESplinePointType::Curve; }
    else if(TypeName.Equals(TEXT("curve_clamped"),ESearchCase::IgnoreCase)) { Type=ESplinePointType::CurveClamped; }
    else if(TypeName.Equals(TEXT("constant"),ESearchCase::IgnoreCase)) { Type=ESplinePointType::Constant; }
    auto Selectors=MakeShared<FJsonObject>();Selectors->SetStringField(TEXT("actor_label"),ActorLabel);Selectors->SetStringField(TEXT("actor_name"),ActorName);
    Selectors->SetStringField(TEXT("actor_path"),ActorPath);Selectors->SetStringField(TEXT("actor_tag"),ActorTag);Selectors->SetStringField(TEXT("actor_label_prefix"),ActorLabelPrefix);
    Selectors->SetStringField(TEXT("component_name"),ChosenComponent);Selectors->SetStringField(TEXT("component_tag"),ComponentTag);
    Selectors->SetBoolField(TEXT("selected_only"),bSelectedOnly);Selectors->SetBoolField(TEXT("allow_multiple_actor_matches"),AllowMultiple);
    W.Result.Data->SetObjectField(TEXT("effective_selectors"),Selectors);
    W.Result.Data->SetBoolField(TEXT("closed_loop_field_present"),HasClosed);W.Result.Data->SetBoolField(TEXT("component_name_field_present"),!Raw||Context->HasField(TEXT("component_name")));
    W.Result.Data->SetStringField(TEXT("effective_component_name"),ChosenComponent);
    W.Result.Data->SetStringField(TEXT("effective_coordinate_space"),W.Space==ESplineCoordinateSpace::Local?TEXT("local"):TEXT("world"));
    W.Result.Data->SetNumberField(TEXT("effective_point_type_enum"),static_cast<int32>(Type));
    if(!bSelectedOnly&&ActorLabel.IsEmpty()&&ActorName.IsEmpty()&&ActorPath.IsEmpty()&&ActorTag.IsEmpty()&&ActorLabelPrefix.IsEmpty())
    { W.Fail(TEXT("selector_required"),TEXT("set_spline_component_points requires actor_label, actor_name, actor_path, actor_tag, actor_label_prefix, or selected_only=true"));return W.Finish(); }
    W.Result.Stage=TEXT("select_actor");TArray<TWeakObjectPtr<AActor>> Actors;int32 Scanned=0;bool ScanLimit=false;
    auto Add=[&](AActor* A)
    {
        if(++Scanned>MaxActors) { ScanLimit=true;return; }
        // Selected-only legacy iteration did not check World; the Editor-only scope fence is explicit here.
        if(Live(A)&&A->GetWorld()==W.World.Get()&&ActorMatches(A,ActorLabel,ActorName,ActorPath,ActorTag,ActorLabelPrefix)) { Actors.AddUnique(TWeakObjectPtr<AActor>(A)); }
    };
    if(bSelectedOnly)
    {
        USelection* Selection=GEditor->GetSelectedActors();
        if(Selection) { for(FSelectionIterator I(*Selection);I&&!ScanLimit;++I) { Add(Cast<AActor>(*I)); } }
    }
    else { for(TActorIterator<AActor> I(W.World.Get());I&&!ScanLimit;++I) { Add(*I); } }
    W.Result.Data->SetNumberField(TEXT("actors_scanned"),Scanned);W.Result.Data->SetNumberField(TEXT("matching_actor_count"),Actors.Num());
    if(ScanLimit) { W.Fail(TEXT("actor_limit"),TEXT("Actor scan exceeds 65536; use selected_only to bound selection"));return W.Finish(); }
    if(Actors.IsEmpty()) { W.Fail(TEXT("actor_missing"),TEXT("No matching actor found"));return W.Finish(); }
    if(Actors.Num()>1&&!AllowMultiple)
    {
        TArray<TSharedPtr<FJsonValue>> Matches;for(const auto& A:Actors) { Matches.Add(MakeShared<FJsonValueObject>(ActorJson(A.Get()))); }
        W.Result.Data->SetArrayField(TEXT("matching_actors"),Matches);
        W.Fail(TEXT("actor_ambiguous"),TEXT("Multiple matching actors found; pass allow_multiple_actor_matches=true to use the first match"));return W.Finish();
    }
    W.Actor=Actors[0];
    W.Result.Stage=TEXT("parse_points");TSharedPtr<FJsonValue> Values;
    if(!Parse(PointsJson,Values)||Values->Type!=EJson::Array||Values->AsArray().Num()<2||Values->AsArray().Num()>MaxPoints)
    { W.Fail(TEXT("points_invalid"),TEXT("'points' must be a JSON array containing 2..65536 points within the 1MiB carrier bound"));return W.Finish(); }
    TArray<FVector> Points;Points.Reserve(Values->AsArray().Num());
    for(int32 I=0;I<Values->AsArray().Num();++I) { FVector V;if(!Point(W,Values->AsArray()[I],I,V)) { return W.Finish(); }Points.Add(V); }
    W.Result.Data->SetArrayField(TEXT("requested_points"),Values->AsArray());
    W.Result.Stage=TEXT("select_component");TArray<TWeakObjectPtr<USplineComponent>> Candidates;bool TooMany=false;
    W.Current=FindSpline(W.Actor.Get(),ChosenComponent,ComponentTag,Candidates,TooMany);
    if(TooMany) { W.Fail(TEXT("component_limit"),TEXT("Actor has more than 4096 spline components"));return W.Finish(); }
    if(!Live(W.Current.Get())) { W.Fail(TEXT("component_missing"),FString::Printf(TEXT("No matching spline component found on actor: %s"),*W.Actor->GetActorLabel()));return W.Finish(); }
    W.Original=W.Current;W.HasTarget=true;
    if(!W.Admit()) { return W.Finish(); }
    if(W.Current->GetNumberOfSplinePoints()>MaxPoints) { W.Fail(TEXT("point_readback_limit"),TEXT("Existing spline exceeds the 65536-point public observation bound"));return W.Finish(); }
    W.Before=Observation(W.Actor.Get(),W.Current.Get(),W.Space);W.Result.Data->SetObjectField(TEXT("target_before"),W.Before.ToSharedRef());
    const bool ClosedBefore=W.Current->IsClosedLoop();
    W.Result.Data->SetBoolField(TEXT("requested_input_to_construction_script"),InputToUCS);W.Result.Data->SetBoolField(TEXT("mark_dirty_requested"),MarkDirty);
    W.Result.Data->SetBoolField(TEXT("rerun_construction_scripts_requested"),RerunUCS);
    W.Result.Data->SetBoolField(TEXT("effective_expected_closed_loop"),HasClosed?Closed:ClosedBefore);

    if(!W.Writer(TEXT("modify_actor"),TEXT("actor_modify_calls"))) { return W.Finish(); }
    const bool ActorTransaction=W.Actor->Modify();W.Count(TEXT("actor_modify_returns"));W.Result.Data->SetBoolField(TEXT("actor_saved_to_transaction_buffer"),ActorTransaction);
    if(!W.Writer(TEXT("modify_component"),TEXT("component_modify_calls"))) { return W.Finish(); }
    const bool ComponentTransaction=W.Current->Modify();W.Count(TEXT("component_modify_returns"));W.Result.Data->SetBoolField(TEXT("component_saved_to_transaction_buffer"),ComponentTransaction);
    if(!W.Writer(TEXT("set_input_flag"),TEXT("input_flag_writes"))) { return W.Finish(); }
    W.Current->bInputSplinePointsToConstructionScript=InputToUCS;
    if(!W.Writer(TEXT("set_edited_override"),TEXT("override_set_calls"))) { return W.Finish(); }
    W.Current->SetOverrideConstructionScript(true);W.Count(TEXT("override_set_returns"));
    if(!W.Writer(TEXT("set_points"),TEXT("set_points_calls"))) { return W.Finish(); }
    W.Current->SetSplinePoints(Points,W.Space,false);W.Count(TEXT("set_points_returns"));
    for(int32 I=0;I<Points.Num();++I)
    {
        if(!W.Writer(TEXT("set_point_types"),TEXT("set_point_type_calls"))) { return W.Finish(); }
        W.Current->SetSplinePointType(I,Type,false);W.Count(TEXT("set_point_type_returns"));
    }
    if(HasClosed)
    {
        if(!W.Writer(TEXT("set_closed_loop"),TEXT("closed_loop_set_calls"))) { return W.Finish(); }
        W.Current->SetClosedLoop(Closed,false);W.Count(TEXT("closed_loop_set_returns"));
    }
    if(!W.Writer(TEXT("update_spline"),TEXT("update_spline_calls"))) { return W.Finish(); }
    W.Current->UpdateSpline();W.Count(TEXT("update_spline_returns"));
    if(!W.Writer(TEXT("mark_render_state_dirty"),TEXT("mark_render_state_dirty_calls"))) { return W.Finish(); }
    W.Current->MarkRenderStateDirty();W.Count(TEXT("mark_render_state_dirty_returns"));
    if(MarkDirty)
    {
        if(!W.Writer(TEXT("mark_component_package_dirty"),TEXT("mark_package_dirty_calls"))) { return W.Finish(); }
        const bool MarkedComponent=W.Current->MarkPackageDirty();W.Count(TEXT("mark_package_dirty_returns"));W.Result.Data->SetBoolField(TEXT("component_mark_package_dirty_return"),MarkedComponent);
        if(!W.Writer(TEXT("mark_actor_package_dirty"),TEXT("mark_package_dirty_calls"))) { return W.Finish(); }
        const bool MarkedActor=W.Actor->MarkPackageDirty();W.Count(TEXT("mark_package_dirty_returns"));W.Result.Data->SetBoolField(TEXT("actor_mark_package_dirty_return"),MarkedActor);
    }
    if(RerunUCS)
    {
        if(!W.Writer(TEXT("rerun_construction_scripts"),TEXT("rerun_construction_scripts_calls"))) { return W.Finish(); }
        W.ReconstructionDispatched=true;W.Actor->RerunConstructionScripts();W.Count(TEXT("rerun_construction_scripts_returns"));
        // The original component can have been destroyed. No old component pointer is dereferenced here.
        W.Current.Reset();if(!W.Admit(false)) { return W.Finish(); }
        W.Current=FindSpline(W.Actor.Get(),ChosenComponent,ComponentTag,Candidates,TooMany);
        if(TooMany) { W.Fail(TEXT("component_limit_after_construction"),TEXT("Actor has more than 4096 spline components after construction scripts"));return W.Finish(); }
        if(!Live(W.Current.Get())) { W.Fail(TEXT("component_missing_after_construction"),TEXT("Spline component was not found after rerunning construction scripts"));return W.Finish(); }
    }
    W.Result.Stage=TEXT("verify_points");if(!W.Admit()) { return W.Finish(); }
    const int32 FinalCount=W.Current->GetNumberOfSplinePoints();
    const int32 Compared=FMath::Min(FinalCount,Points.Num());double MaxDelta=0;bool AllFinite=true,TypesMatch=true;
    TArray<TSharedPtr<FJsonValue>> FinalPoints,CandidateJson;
    for(int32 I=0;I<FMath::Min(FinalCount,MaxPoints);++I)
    {
        const FVector Final=W.Current->GetLocationAtSplinePoint(I,W.Space);FinalPoints.Add(MakeShared<FJsonValueArray>(VectorJson(Final)));
        const bool Finite=FMath::IsFinite(Final.X)&&FMath::IsFinite(Final.Y)&&FMath::IsFinite(Final.Z);AllFinite&=Finite;
        if(I<Compared) { const double Delta=FVector::Dist(Final,Points[I]);AllFinite&=FMath::IsFinite(Delta);if(FMath::IsFinite(Delta)) { MaxDelta=FMath::Max(MaxDelta,Delta); } }
        TypesMatch&=W.Current->GetSplinePointType(I)==Type;
    }
    for(const auto& C:Candidates) { if(Live(C.Get())) { CandidateJson.Add(MakeShared<FJsonValueObject>(SplineJson(C.Get()))); } }
    const double Tolerance=Strict?0.01:5.0;const bool Within=MaxDelta<=Tolerance;
    W.Legacy->SetObjectField(TEXT("actor"),ActorJson(W.Actor.Get()));W.Legacy->SetObjectField(TEXT("spline_component"),SplineJson(W.Current.Get()));
    W.Legacy->SetArrayField(TEXT("candidate_components"),CandidateJson);W.Legacy->SetArrayField(TEXT("points"),FinalPoints);
    W.Legacy->SetNumberField(TEXT("requested_point_count"),Points.Num());W.Legacy->SetNumberField(TEXT("final_point_count"),FinalCount);
    W.Legacy->SetNumberField(TEXT("max_point_delta_cm"),MaxDelta);W.Legacy->SetNumberField(TEXT("tolerance_cm"),Tolerance);
    W.Legacy->SetBoolField(TEXT("within_tolerance"),Within);W.Legacy->SetBoolField(TEXT("reran_construction_scripts"),RerunUCS);
    W.Legacy->SetStringField(TEXT("coordinate_space"),W.Space==ESplineCoordinateSpace::Local?TEXT("local"):TEXT("world"));
    // Preserve the original legacy success and tolerance fields separately from strengthened validation.
    W.Legacy->SetBoolField(TEXT("success"),true);
    W.Validation->SetBoolField(TEXT("point_count_matches"),FinalCount==Points.Num());W.Validation->SetBoolField(TEXT("readback_complete"),FinalCount<=MaxPoints);
    const bool LengthFinite=FMath::IsFinite(W.Current->GetSplineLength());
    W.Result.Data->SetBoolField(TEXT("legacy_numeric_projection_complete"),AllFinite&&LengthFinite&&FinalCount<=MaxPoints);
    W.Validation->SetBoolField(TEXT("spline_length_finite"),LengthFinite);
    W.Validation->SetBoolField(TEXT("coordinates_finite"),AllFinite);W.Validation->SetBoolField(TEXT("within_tolerance"),Within);
    W.Validation->SetBoolField(TEXT("point_types_match"),TypesMatch);W.Validation->SetBoolField(TEXT("closed_loop_matches"),W.Current->IsClosedLoop()==(HasClosed?Closed:ClosedBefore));
    W.Validation->SetBoolField(TEXT("input_to_construction_script_matches"),W.Current->bInputSplinePointsToConstructionScript==InputToUCS);
    W.Validation->SetBoolField(TEXT("edited_override_set"),W.Current->bSplineHasBeenEdited);
    const bool Passed=FinalCount==Points.Num()&&FinalCount<=MaxPoints&&AllFinite&&Within&&TypesMatch
        &&LengthFinite&&W.Current->IsClosedLoop()==(HasClosed?Closed:ClosedBefore)&&W.Current->bInputSplinePointsToConstructionScript==InputToUCS&&W.Current->bSplineHasBeenEdited;
    W.Result.bPostconditionPassed=Passed;
    if(!Passed)
    {
        // FResult is failure, while the observed legacy result retains the original success semantics.
        W.Result.ErrorCode=TEXT("spline_component_points_postcondition_failed");W.Result.Error=TEXT("Spline write returned but final count/coordinates/type/closed-loop/input/edited state did not meet the requested postcondition; changes are not rolled back");
        return W.Finish();
    }
    W.Result.Stage=TEXT("complete");W.Result.bSuccess=true;return W.Finish();
}
}
