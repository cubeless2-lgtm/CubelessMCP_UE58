// Include ONCE at EOF of CubelessAnimInstanceRuntimePropertyProbeApi.cpp.
// This specific case API reuses that TU's reviewed private FRead, typed writers,
// storage scopes, participant checks and EW snapshot projectors. No RPC dependency.
#include "Compatibility/UE58/CubelessAnimStateRuntimeResponseApi.h"
namespace Cubeless::UE58::AnimInstanceRuntimePropertyProbe
{
namespace
{
struct FResponseCase
{
    FString Name;TArray<FAssignment> Assignments;int32 Ticks=0;double Delta=0;bool Restore=true;
};
FString ResponseString(const TSharedRef<FJsonObject>& J,const TCHAR* Key,const FString& Default)
{FString Value;return J->TryGetStringField(Key,Value)?Value:Default;}
bool ResponseAssignments(const TSharedRef<FJsonObject>& J,TArray<FAssignment>& Out,bool FixedProjection)
{
    auto Projection=MakeShared<FJsonObject>();
    for(const auto& KV:J->Values)
    {
        const FString Key(KV.Key.ToView());
        if(Key==TEXT("properties")||Key==TEXT("property_values")){Projection->SetField(Key,KV.Value);}
        else if(FixedProjection&&Key!=TEXT("property_name")&&Key!=TEXT("value")){return false;}
    }
    return Assignments(MakeShared<FJsonValueObject>(Projection),ResponseString(J,TEXT("property_name"),FString()),J->TryGetField(TEXT("value")),Out);
}
bool ResponseCases(const TSharedPtr<FJsonValue>& Input,const TSharedRef<FJsonObject>& Global,
    const FString& DefaultName,int32 DefaultTicks,double DefaultDelta,bool DefaultRestore,TArray<FResponseCase>& Out,FString& Error)
{
    // The legacy parser skips non-object rows and falls back if no object survives.
    // Finite work guards are deliberate UE58 bounds, not silent truncation.
    if(Input->Type!=EJson::Null&&Input->Type!=EJson::Array){Error=TEXT("cases requires null or an array");return false;}
    int64 TotalTicks=0,TotalAssignments=0;
    if(Input->Type==EJson::Array)
    {
        const auto& Rows=Input->AsArray();
        for(int32 I=0;I<Rows.Num();++I)
        {
            const auto& Row=Rows[I];if(Row->Type!=EJson::Object){continue;}
            if(Out.Num()>=256){Error=TEXT("Runtime response exceeds 256 executable cases; none are dispatched");return false;}
            const auto J=Row->AsObject().ToSharedRef();FResponseCase Case;
            Case.Name=ResponseString(J,TEXT("name"),FString::Printf(TEXT("case_%d"),I));
            if(!ResponseAssignments(J,Case.Assignments,false)){Error=TEXT("Case assignment count exceeds finite rows");return false;}
            double Count=DefaultTicks;J->TryGetNumberField(TEXT("tick_count"),Count);
            double Delta=DefaultDelta;J->TryGetNumberField(TEXT("tick_delta_time"),Delta);
            if(!FMath::IsFinite(Count)||!FMath::IsFinite(Delta)){Error=TEXT("Case tick values must be finite");return false;}
            // Clamp before converting, avoiding the legacy RoundToInt overflow outside int32.
            Case.Ticks=FMath::RoundToInt(FMath::Clamp(Count,0.0,240.0));Case.Delta=FMath::Clamp(Delta,0.0,1.0);
            Case.Restore=J->HasField(TEXT("restore_after_case"))?J->GetBoolField(TEXT("restore_after_case")):DefaultRestore;
            if(!Bounded(Case.Name)){Error=TEXT("Case name exceeds decoded UTF16/NUL bounds");return false;}
            TotalTicks+=Case.Ticks;TotalAssignments+=Case.Assignments.Num();
            if(TotalTicks>4096||TotalAssignments>MaxRows){Error=TEXT("Runtime response aggregate tick/assignment budget exceeded; no case is dispatched");return false;}
            Out.Add(MoveTemp(Case));
        }
    }
    if(Out.IsEmpty())
    {
        FResponseCase Case;Case.Name=DefaultName;Case.Ticks=DefaultTicks;Case.Delta=DefaultDelta;Case.Restore=DefaultRestore;
        if(!ResponseAssignments(Global,Case.Assignments,true)){Error=TEXT("PropertiesJson requires only properties/property_values/property_name/value");return false;}
        Out.Add(MoveTemp(Case));
    }
    return true;
}
bool ResponseApply(FRead& C,const TArray<FAssignment>& Work,bool PreviousValues,TArray<FAssignment>* Restore,
    const TSharedRef<FJsonObject>& Applied,TArray<TSharedPtr<FJsonValue>>& Errors,bool Restoring)
{
    TArray<TSharedPtr<FJsonValue>> Rows;int32 Succeeded=0;
    auto Prefix=[&](){Applied->SetNumberField(TEXT("requested_count"),Work.Num());Applied->SetNumberField(TEXT("success_count"),Succeeded);Applied->SetBoolField(TEXT("all_succeeded"),Succeeded==Work.Num());Applied->SetArrayField(TEXT("assignments"),Rows);};Prefix();
    for(const auto& Assignment:Work)
    {
        if(!C.Current()||!C.Row()){Prefix();return false;}
        auto Row=MakeShared<FJsonObject>();Rows.Add(MakeShared<FJsonValueObject>(Row));
        if(!C.Text(Row,TEXT("name"),Assignment.Name)){Prefix();return false;}
        Row->SetField(TEXT("requested_value"),Assignment.Value);Prefix();
        FName PropertyName;FString Error;FProperty* Property=nullptr;UStruct* Owner=nullptr;
        if(TryParseControlRigProbeName(Assignment.Name,TEXT("property name"),PropertyName,Error)){Property=FindFProperty<FProperty>(C.Anim->GetClass(),PropertyName);}
        if(Property)
        {
            Owner=Property->GetOwnerStruct();if(!C.Hold(Owner)||!PropertyCurrent(C,Property,PropertyName,Owner)){return false;}
            Row->SetStringField(TEXT("property_type"),Property->GetClass()->GetName());Row->SetNumberField(TEXT("property_flags"),double(Property->PropertyFlags));
        }
        TUniquePtr<FProjectionScope> RootScope;
        if(Property)
        {
            auto Shape=CaptureFieldShape(C,Property,[Owner,PropertyName](){return FindFProperty<FProperty>(Owner,PropertyName);});
            UAnimInstance* Root=C.Anim;UClass* Class=Root->GetClass();const int32 Size=Class->GetPropertiesSize();const void* Address=Property->ContainerPtrToValuePtr<void>(Root);
            RootScope=MakeUnique<FProjectionScope>(C,[Shape,Root,Class,Size,Address](){return Shape->Current()&&Root->GetClass()==Class&&Class->GetPropertiesSize()==Size&&Shape->Property->IsInContainer(Class)&&Shape->Property->ContainerPtrToValuePtr<void>(Root)==Address;});
            if(!C.Current()){return false;}
        }
        TSharedPtr<FJsonValue> Previous=MakeShared<FJsonValueNull>();
        if(Property)
        {
            Previous=PropertyJson(C,Property,Property->ContainerPtrToValuePtr<void>(C.Anim));
            if(PreviousValues){Row->SetField(TEXT("previous_value"),Previous);}
            if(!PropertyCurrent(C,Property,PropertyName,Owner)){return false;}
        }
        else if(PreviousValues){Row->SetField(TEXT("previous_value"),Previous);}
        bool Dispatched=false;
        const bool Success=SetUObjectPropertyFromJson(C.Anim,Assignment.Name,Assignment.Value,Error,Dispatched);
        if(Dispatched){C.R.bMutationAttempted=true;Call(C.R,Restoring?TEXT("explicit_restore_writer_calls"):TEXT("explicit_property_writer_calls"));}
        Row->SetBoolField(TEXT("writer_dispatched"),Dispatched);Row->SetBoolField(TEXT("success"),Success);
        if(Success)
        {
            ++Succeeded;
            // Capture successful restore values before the echo callback can fail.
            if(Restore){Restore->Add({Assignment.Name,Previous});}
            Prefix();if(!PropertyCurrent(C,Property,PropertyName,Owner)){return false;}
            auto Echo=PropertyJson(C,Property,Property->ContainerPtrToValuePtr<void>(C.Anim));Row->SetField(TEXT("echo_value"),Echo);
            if(!PropertyCurrent(C,Property,PropertyName,Owner)){return false;}
            const bool Different=!SameJson(Previous,Echo);Row->SetBoolField(TEXT("observed_value_difference"),Different);C.R.bChanged|=Different;
        }
        else{Row->SetStringField(TEXT("error"),Error);Errors.Add(MakeShared<FJsonValueString>(Error));}
        Prefix();if(!C.Current()){return false;}
    }
    return true;
}
bool ResponseSnapshot(FRead& C,const TSharedRef<FJsonObject>& Snapshot,const FString& Filter,
    bool States,bool Weights,bool Times,bool Transitions,bool Inactive,bool Montages,bool IncludeCurves,
    const TArray<FString>& CurvesRequested,int32 MachinesLimit,int32 ProbeLimit,int32 StatesLimit,int32 TransitionsLimit,int32 CurvesLimit,
    TArray<TSharedPtr<FJsonValue>>& Warnings)
{
    Snapshot->SetObjectField(TEXT("anim_instance"),AnimJson(C.Anim));C.Output=Snapshot;C.RequireInitialized=true;
    if(!C.Current()||!Machines(C,Filter,States,Weights,Times,Transitions,Inactive,MachinesLimit,ProbeLimit,StatesLimit,TransitionsLimit,Warnings)){return false;}
    if(Montages&&!Montage(C)){return false;}if(IncludeCurves&&!Curves(C,CurvesRequested,CurvesLimit,Warnings)){return false;}
    return C.Current();
}
}

Variable::FResult SampleCases(const FString& ActorLabel,const FString& ActorName,const FString& ActorPath,const FString& ComponentName,
    const FString& CasesJson,const FString& PropertiesJson,const FString& Name,int64 TickCount,double TickDeltaTime,
    bool bRefreshBoneTransforms,bool bRestoreAfterCase,bool bIncludeBaseline,const FString& StateMachineName,
    bool bIncludeStates,bool bIncludeStateWeights,bool bIncludeRelevantAnimTimes,bool bIncludeTransitionProgress,
    bool bIncludeInactiveTransitionProgress,bool bIncludeMontages,bool bIncludeCurves,const FString& CurveNamesJson,
    int64 MaxStateMachines,int64 MaxStateMachineInstancesToProbe,int64 MaxStatesPerMachine,int64 MaxTransitionsPerMachine,int64 MaxCurves,
    bool bPreferPIEWorld,bool bRequirePIEWorld,const FString& AnimInstanceSource)
{
    FResult R;R.Data->SetStringField(TEXT("command"),TEXT("sample_anim_state_machine_runtime_response"));
    auto Counts=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("native_call_counts"),Counts);
    for(const TCHAR* Key:{TEXT("world_discovery_calls"),TEXT("actor_label_calls"),TEXT("component_inventory_calls"),TEXT("anim_instance_calls"),TEXT("state_machine_instance_calls"),TEXT("current_montage_calls"),TEXT("all_curve_names_calls"),TEXT("curve_value_calls"),TEXT("explicit_property_writer_calls"),TEXT("explicit_restore_writer_calls"),TEXT("explicit_tick_calls"),TEXT("returned_tick_calls"),TEXT("explicit_refresh_calls"),TEXT("returned_refresh_calls"),TEXT("explicit_load_calls"),TEXT("explicit_modify_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls"),TEXT("explicit_initialize_calls"),TEXT("starts_pie_sie_calls")}){Counts->SetNumberField(Key,0);}
    for(const TCHAR* Key:{TEXT("read_only"),TEXT("asset_modified"),TEXT("saves_assets"),TEXT("starts_sie"),TEXT("automatic_action_retry"),TEXT("atomic_rollback_available"),TEXT("all_SDK_effects_verified_absent"),TEXT("asset_dirty_unchanged_verified"),TEXT("observation_incomplete"),TEXT("unique_external_pipeline_complete"),TEXT("packaged_runtime_verified")}){R.Data->SetBoolField(Key,false);}
    R.Data->SetBoolField(TEXT("runtime_only"),true);R.Data->SetStringField(TEXT("runtime_source"),TEXT("active_anim_instance_state_machine_response"));
    R.Data->SetStringField(TEXT("runtime_note"),TEXT("Applies requested properties to a live AnimInstance, optionally ticks the matched SkeletalMeshComponent, samples state-machine runtime state, and restores successful properties per case by default. It does not save or modify Animation Blueprint assets."));
    R.Data->SetStringField(TEXT("asset_modified_scope"),TEXT("Legacy false indicates no explicit asset writer; SDK/animation callbacks, lazy proxies and dirty changes remain unknown"));
    R.Data->SetStringField(TEXT("restore_scope"),TEXT("Successful assignment previous JSON values are reapplied in forward order to the current main AnimInstance after each case snapshot. Duplicate names may prevent initial-baseline restoration; tick/time/notifies and failed writes are not rolled back."));
    R.Data->SetBoolField(TEXT("restores_after_case_default"),bRestoreAfterCase);R.Data->SetBoolField(TEXT("prefer_pie_world"),bPreferPIEWorld);R.Data->SetBoolField(TEXT("require_pie_world"),bRequirePIEWorld);
    R.Data->SetStringField(TEXT("requested_actor_label"),ActorLabel);R.Data->SetStringField(TEXT("requested_actor_name"),ActorName);R.Data->SetStringField(TEXT("requested_actor_path"),ActorPath);R.Data->SetStringField(TEXT("requested_component_name"),ComponentName);
    TArray<TSharedPtr<FJsonValue>> Errors,Warnings,CaseRows;R.Data->SetArrayField(TEXT("errors"),Errors);R.Data->SetArrayField(TEXT("warnings"),Warnings);R.Data->SetArrayField(TEXT("cases"),CaseRows);R.Data->SetNumberField(TEXT("case_count"),0);R.Data->SetNumberField(TEXT("successful_case_count"),0);
    if(!IsEditorGameThread()||IsEngineExitRequested()||InFlight){return Fail(R,TEXT("anim_runtime_response_editor_unavailable"),TEXT("A non-reentrant live Editor game thread is required; PIE/SIE reads/writes are allowed, no session is started"));}
    TGuardValue<bool> Busy(InFlight,true);R.Stage=TEXT("validate_anim_runtime_response_inputs");
    for(const FString* Text:{&ActorLabel,&ActorName,&ActorPath,&ComponentName,&Name,&StateMachineName,&AnimInstanceSource}){if(!Bounded(*Text)){return Fail(R,TEXT("anim_runtime_response_input_bound"),TEXT("Runtime selector/name exceeds UTF16/NUL bounds"));}}
    FString Source=AnimInstanceSource.TrimStartAndEnd();if(Source.IsEmpty()){Source=TEXT("main");}const FString LowerSource=Source.ToLower();
    const bool UseMain=LowerSource==TEXT("main")||LowerSource==TEXT("primary")||LowerSource==TEXT("anim_instance");
    const bool UsePost=LowerSource==TEXT("post_process")||LowerSource==TEXT("postprocess")||LowerSource==TEXT("post_process_anim_bp")||LowerSource==TEXT("postprocess_anim_bp");
    if(!UseMain&&!UsePost){return Fail(R,TEXT("anim_runtime_instance_source_invalid"),FString::Printf(TEXT("Unsupported anim_instance_source='%s'. Expected 'main' or 'post_process'"),*Source));}
    R.Data->SetStringField(TEXT("requested_anim_instance_source"),Source);R.Data->SetStringField(TEXT("anim_instance_source"),UsePost?TEXT("post_process"):TEXT("main"));
    if(!FMath::IsFinite(TickDeltaTime)){return Fail(R,TEXT("anim_runtime_response_delta_invalid"),TEXT("tick_delta_time must be finite"));}
    const int32 DefaultTicks=int32(FMath::Clamp<int64>(TickCount,0,240));const double DefaultDelta=FMath::Clamp(TickDeltaTime,0.0,1.0);
    const int32 MachinesLimit=int32(FMath::Clamp<int64>(MaxStateMachines,0,1024)),ProbeLimit=int32(FMath::Clamp<int64>(MaxStateMachineInstancesToProbe,0,4096)),StatesLimit=int32(FMath::Clamp<int64>(MaxStatesPerMachine,0,2048)),TransitionsLimit=int32(FMath::Clamp<int64>(MaxTransitionsPerMachine,0,4096)),CurvesLimit=int32(FMath::Clamp<int64>(MaxCurves,0,2048));
    auto Limits=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("received_limits_decimal"),Limits);Limits->SetStringField(TEXT("tick_count"),LexToString(TickCount));Limits->SetStringField(TEXT("max_state_machines"),LexToString(MaxStateMachines));Limits->SetStringField(TEXT("max_state_machine_instances_to_probe"),LexToString(MaxStateMachineInstancesToProbe));Limits->SetStringField(TEXT("max_states_per_machine"),LexToString(MaxStatesPerMachine));Limits->SetStringField(TEXT("max_transitions_per_machine"),LexToString(MaxTransitionsPerMachine));Limits->SetStringField(TEXT("max_curves"),LexToString(MaxCurves));R.Data->SetNumberField(TEXT("received_tick_delta_time"),TickDeltaTime);
    TSharedPtr<FJsonValue> CasesInput,Properties;FString ParseError;
    if(!Decode(CasesJson,CasesInput,ParseError)||!Decode(PropertiesJson,Properties,ParseError)){return Fail(R,TEXT("anim_runtime_response_json_invalid"),ParseError);}
    if(Properties->Type!=EJson::Object){return Fail(R,TEXT("anim_runtime_response_properties_invalid"),TEXT("PropertiesJson requires a fixed object projection"));}
    TArray<FAssignment> GlobalAssignments;if(!ResponseAssignments(Properties->AsObject().ToSharedRef(),GlobalAssignments,true)){return Fail(R,TEXT("anim_runtime_response_properties_invalid"),TEXT("PropertiesJson has an unknown projection key or excessive assignments"));}
    TSharedPtr<FJsonValue> CurvesValue;TArray<FString> RequestedCurves;if(!Decode(CurveNamesJson,CurvesValue,ParseError)||!CurvesInput(CurveNamesJson,RequestedCurves,ParseError)){return Fail(R,TEXT("anim_runtime_response_curves_invalid"),ParseError);}
    // Build the finite case plan before any runtime SDK observation; the legacy
    // baseline remains before any case write, tick, or snapshot.
    TArray<FResponseCase> Cases;if(!ResponseCases(CasesInput,Properties->AsObject().ToSharedRef(),Name,DefaultTicks,DefaultDelta,bRestoreAfterCase,Cases,ParseError)){return Fail(R,TEXT("anim_runtime_response_cases_invalid"),ParseError);}
    R.Data->SetNumberField(TEXT("case_count"),Cases.Num());R.Data->SetNumberField(TEXT("completed_case_count"),0);R.Data->SetBoolField(TEXT("synchronous_SDK_call_may_block"),true);
    FRead C(R);C.UsePostProcess=UsePost;int32 SuccessfulCases=0;
    auto Prefix=[&]()
    {
        R.Data->SetArrayField(TEXT("errors"),Errors);R.Data->SetArrayField(TEXT("warnings"),Warnings);R.Data->SetArrayField(TEXT("cases"),CaseRows);R.Data->SetNumberField(TEXT("successful_case_count"),SuccessfulCases);R.Data->SetNumberField(TEXT("completed_output_rows"),C.Rows);R.Data->SetBoolField(TEXT("total_change_state_known"),R.bChangeStateKnown);R.Data->SetBoolField(TEXT("observed_explicit_change"),R.bChanged);
        TArray<TSharedPtr<FJsonValue>> Dirty;bool Changed=false;int32 Count=0;
        for(const auto& KV:C.InitialPackageDirty){if(Count++>=128){break;}auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("before"),KV.Value);if(Loaded(KV.Key)){J->SetStringField(TEXT("captured_actual_package"),KV.Key->GetPathName());const bool After=KV.Key->IsDirty();J->SetBoolField(TEXT("after"),After);Changed|=After!=KV.Value;}else{J->SetField(TEXT("after"),MakeShared<FJsonValueNull>());}Dirty.Add(MakeShared<FJsonValueObject>(J));}
        R.Data->SetArrayField(TEXT("captured_package_dirty_observations"),Dirty);R.Data->SetBoolField(TEXT("captured_package_dirty_observations_truncated"),C.InitialPackageDirty.Num()>128);R.Data->SetBoolField(TEXT("observed_captured_package_dirty_change"),Changed);R.Data->SetStringField(TEXT("dirty_observation_scope"),TEXT("First-sighting and current flags of at most128 captured actual packages; no whole-asset purity, current-identity or rollback proof"));
    };
    auto Incomplete=[&](){Prefix();return Fail(R,TEXT("anim_runtime_response_incomplete"),C.Error.IsEmpty()?TEXT("Runtime target/observation became incomplete; actual prefix retained, no retry or speculative restore"):C.Error);};
    R.Stage=TEXT("discover_anim_runtime_worlds");R.bChangeStateKnown=false;Call(R,TEXT("world_discovery_calls"));const TArray<UWorld*> Candidates=Worlds(bPreferPIEWorld,bRequirePIEWorld);
    if(Candidates.Num()>MaxRows){C.Bad(TEXT("World source exceeds finite rows"));return Incomplete();}for(UWorld* World:Candidates){if(!C.Hold(World)){return Incomplete();}}
    int32 MatchedActors=0,MatchedComponents=0;
    for(UWorld* World:Candidates)
    {
        C.World=World;C.Type=World->WorldType;if(!C.Current()){return Incomplete();}TArray<AActor*> Actors;
        for(TActorIterator<AActor> It(World);It;++It){AActor* Actor=*It;if(!Loaded(Actor)){continue;}if(Actors.Num()>=MaxRows||!C.Hold(Actor)){C.Bad(TEXT("Actor source exceeds finite bounds"));return Incomplete();}Actors.Add(Actor);}
        TArray<AActor*> Matches;
        for(AActor* Actor:Actors)
        {
            C.Actor=Actor;if(!C.Current()){return Incomplete();}
            if(!ActorLabel.IsEmpty()){Call(R,TEXT("actor_label_calls"));const FString Label=Actor->GetActorLabel();if(!C.Current()){return Incomplete();}if(!Label.Equals(ActorLabel,ESearchCase::IgnoreCase)){continue;}}
            if(!ActorName.IsEmpty()&&!Actor->GetName().Equals(ActorName,ESearchCase::IgnoreCase)){continue;}
            const FString Path=Actor->GetPathName();if(!ActorPath.IsEmpty()&&!Path.Equals(ActorPath,ESearchCase::IgnoreCase)&&!Path.EndsWith(ActorPath,ESearchCase::IgnoreCase)){continue;}
            TArray<USkeletalMeshComponent*> Components;Call(R,TEXT("component_inventory_calls"));Actor->GetComponents<USkeletalMeshComponent>(Components);if(!C.Current()){return Incomplete();}if(!Components.IsEmpty()){Matches.Add(Actor);}
        }
        MatchedActors+=Matches.Num();
        for(AActor* Actor:Matches)
        {
            C.Actor=Actor;C.Component=nullptr;if(!C.Current()){return Incomplete();}TArray<USkeletalMeshComponent*> Components;Call(R,TEXT("component_inventory_calls"));Actor->GetComponents<USkeletalMeshComponent>(Components);
            if(!C.Current()||Components.Num()>MaxRows){C.Bad(TEXT("Component source exceeds finite rows"));return Incomplete();}
            USkeletalMeshComponent* First=nullptr;int32 Count=0;
            for(USkeletalMeshComponent* Component:Components){if(!Loaded(Component)){continue;}if(!C.Hold(Component)){return Incomplete();}if(!First){First=Component;}if(ComponentName.IsEmpty()){++Count;continue;}if(Component->GetName().Equals(ComponentName,ESearchCase::IgnoreCase)){First=Component;++Count;break;}}
            if(ComponentName.IsEmpty()?First!=nullptr:Count>0){C.Component=First;MatchedComponents=Count;break;}
        }
        if(C.Component){break;}C.Actor=nullptr;
    }
    if(!C.Actor||!C.Component||!C.World){Prefix();return Fail(R,TEXT("anim_runtime_target_missing"),(bRequirePIEWorld?FString::Printf(TEXT("No SkeletalMeshComponent actor matched actor_label='%s', actor_name='%s', actor_path='%s', component_name='%s' in active PIE/SIE/play worlds"),*ActorLabel,*ActorName,*ActorPath,*ComponentName):FString::Printf(TEXT("No SkeletalMeshComponent actor matched actor_label='%s', actor_name='%s', actor_path='%s', component_name='%s' in candidate editor/PIE worlds"),*ActorLabel,*ActorName,*ActorPath,*ComponentName)));}
    if(MatchedActors>1){Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Matched %d actors; sampled the first matching actor in world priority order"),MatchedActors)));}
    if(ComponentName.IsEmpty()&&MatchedComponents>1){Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Actor has %d SkeletalMeshComponents; sampled the first component because component_name was not provided"),MatchedComponents)));}
    R.Stage=TEXT("resolve_anim_runtime_response_target");if(!C.Current()){return Incomplete();}Call(R,TEXT("anim_instance_calls"));UAnimInstance* Instance=C.SelectedAnim();
    if(Instance&&!C.Hold(Instance)){return Incomplete();}if(!C.Current()){return Incomplete();}
    if(!Instance)
    {
        UClass* Class=C.Component->GetAnimClass();if(Class&&!C.Hold(Class)){return Incomplete();}if(!C.Current()){return Incomplete();}
        Call(R,TEXT("post_process_class_calls"));UClass* PostClass=C.Component->GetPostProcessAnimBPClassToBeUsed().Get();if(PostClass&&!C.Hold(PostClass)){return Incomplete();}if(!C.Current()){return Incomplete();}Prefix();
        if(UsePost){const bool Disabled=C.Component->GetDisablePostProcessBlueprint();if(!C.Current()){return Incomplete();}return Fail(R,TEXT("anim_runtime_instance_missing"),FString::Printf(TEXT("Matched SkeletalMeshComponent '%s' has no post-process AnimInstance. anim_instance_source=post_process animation_mode=%s anim_class=%s post_process_class=%s post_process_disabled=%s"),*C.Component->GetName(),*AnimationMode(C.Component->GetAnimationMode()),Class?*Class->GetPathName():TEXT("<none>"),PostClass?*PostClass->GetPathName():TEXT("<none>"),Disabled?TEXT("true"):TEXT("false")));}
        return Fail(R,TEXT("anim_runtime_instance_missing"),FString::Printf(TEXT("Matched SkeletalMeshComponent '%s' has no AnimInstance. anim_instance_source=main animation_mode=%s anim_class=%s post_process_class=%s"),*C.Component->GetName(),*AnimationMode(C.Component->GetAnimationMode()),Class?*Class->GetPathName():TEXT("<none>"),PostClass?*PostClass->GetPathName():TEXT("<none>")));
    }
    C.Anim=Instance;if(!C.Current()){return Incomplete();}
    if(C.Anim->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject)||C.Anim->IsTemplate()){Prefix();return Fail(R,TEXT("anim_runtime_response_template_refused"),TEXT("Only a live component-owned AnimInstance can be written"));}
    R.Data->SetBoolField(TEXT("is_play_session_active"),IsPlaySessionActive());R.Data->SetStringField(TEXT("sampled_world_type"),WorldType(C.World));R.Data->SetStringField(TEXT("sampled_world_name"),C.World->GetName());R.Data->SetNumberField(TEXT("matched_actor_count"),MatchedActors);R.Data->SetNumberField(TEXT("matched_component_count"),MatchedComponents);
    auto ActorJson=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("actor"),ActorJson);Call(R,TEXT("actor_label_calls"));const FString Label=C.Actor->GetActorLabel();if(!C.Current()){return Incomplete();}
    C.Text(ActorJson,TEXT("label"),Label);C.Text(ActorJson,TEXT("name"),C.Actor->GetName());C.Text(ActorJson,TEXT("path"),C.Actor->GetPathName());C.Text(ActorJson,TEXT("class"),C.Actor->GetClass()->GetPathName());
    auto ComponentJson=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("component"),ComponentJson);USkeletalMesh* Mesh=C.Component->GetSkeletalMeshAsset();if(Mesh&&!C.Hold(Mesh)){return Incomplete();}UClass* AnimClass=C.Component->GetAnimClass();if(AnimClass&&!C.Hold(AnimClass)){return Incomplete();}if(!C.Current()){return Incomplete();}
    C.Text(ComponentJson,TEXT("name"),C.Component->GetName());C.Text(ComponentJson,TEXT("path"),C.Component->GetPathName());C.Text(ComponentJson,TEXT("class"),C.Component->GetClass()->GetPathName());C.Text(ComponentJson,TEXT("skeletal_mesh"),Mesh?Mesh->GetPathName():FString());C.Text(ComponentJson,TEXT("anim_class"),AnimClass?AnimClass->GetPathName():FString());C.Text(ComponentJson,TEXT("animation_mode"),AnimationMode(C.Component->GetAnimationMode()));
    const FTransform ComponentTransform=C.Component->GetComponentTransform();if(!C.Current()||ComponentTransform.ContainsNaN()){C.Bad(TEXT("Component transform is not a finite current sample"));return Incomplete();}ComponentJson->SetObjectField(TEXT("component_transform"),Transform(ComponentTransform));R.Data->SetObjectField(TEXT("anim_instance"),AnimJson(C.Anim));
    if(bIncludeBaseline)
    {
        R.Stage=TEXT("snapshot_anim_runtime_response_baseline");auto Baseline=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("baseline"),Baseline);
        if(!ResponseSnapshot(C,Baseline,StateMachineName,bIncludeStates,bIncludeStateWeights,bIncludeRelevantAnimTimes,bIncludeTransitionProgress,bIncludeInactiveTransitionProgress,bIncludeMontages,bIncludeCurves,RequestedCurves,MachinesLimit,ProbeLimit,StatesLimit,TransitionsLimit,CurvesLimit,Warnings)){return Incomplete();}
    }
    for(int32 Index=0;Index<Cases.Num();++Index)
    {
        const auto& Case=Cases[Index];if(!C.Current()||!C.Row()){return Incomplete();}C.RequireInitialized=false;
        auto J=MakeShared<FJsonObject>();CaseRows.Add(MakeShared<FJsonValueObject>(J));C.Text(J,TEXT("name"),Case.Name);J->SetNumberField(TEXT("case_index"),Index);J->SetBoolField(TEXT("restore_after_case"),Case.Restore);J->SetNumberField(TEXT("tick_count"),Case.Ticks);J->SetNumberField(TEXT("tick_delta_time"),Case.Delta);
        TArray<TSharedPtr<FJsonValue>> CaseErrors,CaseWarnings;TArray<FAssignment> Restore;
        auto CasePrefix=[&](){J->SetArrayField(TEXT("errors"),CaseErrors);J->SetArrayField(TEXT("warnings"),CaseWarnings);J->SetNumberField(TEXT("captured_restore_assignment_count"),Restore.Num());Prefix();};
        J->SetBoolField(TEXT("success"),false);J->SetBoolField(TEXT("completed"),false);J->SetBoolField(TEXT("restore_attempted"),false);J->SetBoolField(TEXT("restore_completed"),false);CasePrefix();J->SetStringField(TEXT("assignment_target_path"),C.Anim->GetPathName());
        auto Applied=MakeShared<FJsonObject>();J->SetObjectField(TEXT("applied_properties"),Applied);R.Stage=TEXT("apply_anim_runtime_response_case");
        if(!ResponseApply(C,Case.Assignments,true,Case.Restore?&Restore:nullptr,Applied,CaseErrors,false)){CasePrefix();return Incomplete();}CasePrefix();
        auto Tick=MakeShared<FJsonObject>();J->SetObjectField(TEXT("tick"),Tick);Tick->SetStringField(TEXT("tick_method"),TEXT("USkeletalMeshComponent::TickAnimation"));Tick->SetNumberField(TEXT("requested_tick_count"),Case.Ticks);Tick->SetNumberField(TEXT("tick_delta_time"),Case.Delta);Tick->SetBoolField(TEXT("refresh_bone_transforms"),bRefreshBoneTransforms);Tick->SetNumberField(TEXT("executed_tick_count"),0);Tick->SetNumberField(TEXT("executed_refresh_count"),0);Tick->SetBoolField(TEXT("success"),false);Tick->SetStringField(TEXT("execution_count_scope"),TEXT("Public SDK calls returned; internal update/evaluation/pose completion is not instrumented"));
        R.Stage=TEXT("tick_anim_runtime_response_case");
        for(int32 I=0;I<Case.Ticks;++I)
        {
            if(!TickParticipants(C)){CasePrefix();return Incomplete();}R.bMutationAttempted=true;Call(R,TEXT("explicit_tick_calls"));C.Anim=nullptr;C.Component->TickAnimation(float(Case.Delta),false);Call(R,TEXT("returned_tick_calls"));Tick->SetNumberField(TEXT("executed_tick_count"),I+1);CasePrefix();
            if(!RefreshCurrentAnim(C)){return Incomplete();}
            if(bRefreshBoneTransforms)
            {
                if(!TickParticipants(C)){CasePrefix();return Incomplete();}Call(R,TEXT("explicit_refresh_calls"));C.Anim=nullptr;C.Component->RefreshBoneTransforms();Call(R,TEXT("returned_refresh_calls"));Tick->SetNumberField(TEXT("executed_refresh_count"),I+1);CasePrefix();if(!RefreshCurrentAnim(C)){return Incomplete();}
            }
        }
        if(Case.Ticks>0){Call(R,TEXT("should_tick_animation_calls"));const bool ShouldTick=C.Component->ShouldTickAnimation();if(!C.Current()){CasePrefix();return Incomplete();}if(!ShouldTick){CaseWarnings.Add(MakeShared<FJsonValueString>(TEXT("SkeletalMeshComponent::ShouldTickAnimation returned false; forced TickAnimation was still called for the probe")));}}
        Tick->SetBoolField(TEXT("success"),true);CasePrefix();
        // Preserve the original switch to main before snapshot and forward restore.
        C.UsePostProcess=false;R.Data->SetBoolField(TEXT("snapshot_original_main_refresh_semantics"),true);if(!RefreshCurrentAnim(C)){CasePrefix();return Incomplete();}
        auto Snapshot=MakeShared<FJsonObject>();J->SetObjectField(TEXT("snapshot"),Snapshot);R.Stage=TEXT("snapshot_anim_runtime_response_case");
        if(!ResponseSnapshot(C,Snapshot,StateMachineName,bIncludeStates,bIncludeStateWeights,bIncludeRelevantAnimTimes,bIncludeTransitionProgress,bIncludeInactiveTransitionProgress,bIncludeMontages,bIncludeCurves,RequestedCurves,MachinesLimit,ProbeLimit,StatesLimit,TransitionsLimit,CurvesLimit,CaseWarnings)){CasePrefix();return Incomplete();}CasePrefix();
        if(Case.Restore&&!Restore.IsEmpty())
        {
            C.RequireInitialized=false;auto Restored=MakeShared<FJsonObject>();J->SetObjectField(TEXT("restore"),Restored);J->SetBoolField(TEXT("restore_attempted"),true);J->SetStringField(TEXT("restore_target_path"),C.Anim->GetPathName());R.Stage=TEXT("restore_anim_runtime_response_case");
            if(!ResponseApply(C,Restore,false,nullptr,Restored,CaseErrors,true)){CasePrefix();return Incomplete();}J->SetBoolField(TEXT("restore_completed"),true);CasePrefix();
        }
        const bool CaseSuccess=CaseErrors.IsEmpty();J->SetBoolField(TEXT("success"),CaseSuccess);J->SetBoolField(TEXT("completed"),true);
        if(CaseSuccess){++SuccessfulCases;}else{Errors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Case '%s' failed"),*Case.Name)));}
        R.Data->SetNumberField(TEXT("completed_case_count"),Index+1);CasePrefix();
    }
    Prefix();if(!C.Current()){return Incomplete();}R.bSuccess=Errors.IsEmpty();R.bPostconditionPassed=R.bSuccess;R.Data->SetBoolField(TEXT("success"),R.bSuccess);R.Stage=R.bSuccess?TEXT("anim_runtime_response_complete"):TEXT("anim_runtime_response_partial_failure");
    if(!R.bSuccess){R.ErrorCode=TEXT("anim_runtime_response_case_failed");R.Error=TEXT("One or more runtime response cases failed; successful writes, ticks, snapshots and forward restores remain recorded. No atomic rollback or automatic retry.");}return R;
}
}
