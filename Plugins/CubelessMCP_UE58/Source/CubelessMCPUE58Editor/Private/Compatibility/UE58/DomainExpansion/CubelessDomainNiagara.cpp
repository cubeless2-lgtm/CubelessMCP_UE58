#include "Compatibility/UE58/DomainExpansion/CubelessDomainCommon.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraSystemFactoryNew.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/NiagaraEmitterHandleViewModel.h"
#include "AssetCompilingManager.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/PackageName.h"
#include "Containers/Ticker.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UObject/StructOnScope.h"
#include "Curves/CurveFloat.h"
#endif

namespace Cubeless::UE58::DomainExpansion
{
namespace
{
FString ObjectState(UObject* Object)
{
	FString State;
	for(TFieldIterator<FProperty> It(Object->GetClass());It;++It)
	{
		FProperty* P=*It; if(P->HasAnyPropertyFlags(CPF_Transient)) { continue; }
		FString Text; P->ExportTextItem_Direct(Text,P->ContainerPtrToValuePtr<void>(Object),nullptr,Object,PPF_None);
		State+=P->GetName()+TEXT("=")+Text+TEXT("\n");
	}
	return State;
}
UObject* StatelessObject(const FNiagaraEmitterHandle& Handle)
{
	if(Handle.GetEmitterMode()!=ENiagaraEmitterMode::Stateless) { return nullptr; }
	const auto* Property=FindFProperty<FObjectPropertyBase>(FNiagaraEmitterHandle::StaticStruct(),TEXT("StatelessEmitter"));
	return Property ? Property->GetObjectPropertyValue_InContainer(&Handle) : nullptr;
}
// Snapshot only fields supported by the authored writer. SourceId and compiler/runtime
// data have no CPF_Edit leaf path and intentionally do not participate.
struct FAuthoredValues : TMap<FString,FString>
{
    // This is the authored handle binding, not compiler-generated module/cache identity.
    TMap<FGuid,TWeakObjectPtr<UObject>> EmitterBindings;
};
bool SelectedEmitterIsCurrent(UNiagaraSystem* System,const FGuid& HandleId,UObject* Emitter)
{
    if(!Live(System)||!Live(Emitter))return false;
    int32 Matches=0;
    for(const auto& Handle:System->GetEmitterHandles())if(Handle.GetId()==HandleId)
    {if(StatelessObject(Handle)!=Emitter)return false;++Matches;}
    return Matches==1;
}
void CaptureEditableLeaves(UStruct* Type,void* Data,const FString& Prefix,FAuthoredValues& Out,int32 Depth=0)
{
    if(Depth>=8)return;
    for(TFieldIterator<FProperty> It(Type);It;++It)
    {
        FProperty* P=*It;
        if(!P->HasAnyPropertyFlags(CPF_Edit)||P->HasAnyPropertyFlags(CPF_Transient|CPF_EditConst)||P->ArrayDim!=1||P->HasSetterOrGetter())continue;
        const FString Key=Prefix+P->GetName();void* V=P->ContainerPtrToValuePtr<void>(Data);
        if(SimpleLeaf(P)){FString Value;P->ExportTextItem_Direct(Value,V,nullptr,nullptr,PPF_None);Out.Add(Key,MoveTemp(Value));}
        else if(auto* Struct=CastField<FStructProperty>(P))CaptureEditableLeaves(Struct->Struct,V,Key+TEXT("."),Out,Depth+1);
    }
}
bool CaptureAuthored(UNiagaraSystem* System,FAuthoredValues& Out,FString& Why)
{
    Out.Empty();Out.EmitterBindings.Empty();if(!Live(System)){Why=TEXT("Candidate unavailable while reading authored values");return false;}
    int32 EmitterCount=0;
    for(const auto& Handle:System->GetEmitterHandles())
    {
        UObject* Emitter=StatelessObject(Handle);if(!Emitter)continue;
        const FString Prefix=TEXT("emitter[")+Handle.GetName().ToString()+TEXT("]");
        if(Out.Contains(Prefix+TEXT(".class"))||!Live(Emitter)||!Emitter->IsIn(System)){Why=TEXT("Unique candidate-owned stateless emitters required for authored guard");return false;}
        if(!Handle.GetId().IsValid()||Out.EmitterBindings.Contains(Handle.GetId())){Why=TEXT("Unique valid stateless handle IDs required for authored guard");return false;}
        Out.EmitterBindings.Add(Handle.GetId(),Emitter);
        ++EmitterCount;Out.Add(Prefix+TEXT(".class"),Emitter->GetClass()->GetPathName());
        const auto* Spawn=FindFProperty<FArrayProperty>(Emitter->GetClass(),TEXT("SpawnInfos"));
        const auto* SpawnInner=Spawn?CastField<FStructProperty>(Spawn->Inner):nullptr;
        const auto* Modules=FindFProperty<FArrayProperty>(Emitter->GetClass(),TEXT("Modules"));
        const auto* ModuleInner=Modules?CastField<FObjectPropertyBase>(Modules->Inner):nullptr;
        if(!SpawnInner||!ModuleInner){Why=TEXT("Stateless authored array contract unavailable");return false;}
        FScriptArrayHelper Spawns(Spawn,Spawn->ContainerPtrToValuePtr<void>(Emitter));
        Out.Add(Prefix+TEXT(".spawn_count"),LexToString(Spawns.Num()));
        for(int32 I=0;I<Spawns.Num();++I)CaptureEditableLeaves(SpawnInner->Struct,Spawns.GetRawPtr(I),Prefix+FString::Printf(TEXT(".spawn[%d]."),I),Out);
        FScriptArrayHelper Array(Modules,Modules->ContainerPtrToValuePtr<void>(Emitter));
        Out.Add(Prefix+TEXT(".module_count"),LexToString(Array.Num()));
        for(int32 I=0;I<Array.Num();++I)
        {
            UObject* Module=ModuleInner->GetObjectPropertyValue(Array.GetRawPtr(I));
            const FString ModulePrefix=Prefix+FString::Printf(TEXT(".module[%d]."),I);
            if(!Module){Out.Add(ModulePrefix+TEXT("identity"),TEXT("null"));continue;}
            if(!Live(Module)||!Module->IsIn(Emitter)){Why=TEXT("Emitter-owned module required for authored guard");return false;}
            Out.Add(ModulePrefix+TEXT("identity"),Module->GetName()+TEXT(":")+Module->GetClass()->GetPathName());
            CaptureEditableLeaves(Module->GetClass(),Module,ModulePrefix,Out);
        }
    }
    Out.Add(TEXT("stateless_emitter_count"),LexToString(EmitterCount));return true;
}
bool SameAuthored(const FAuthoredValues& Expected,const FAuthoredValues& Actual,FString& Why)
{
    for(const auto& Binding:Expected.EmitterBindings)
    {
        const auto* Current=Actual.EmitterBindings.Find(Binding.Key);
        if(!Current||!Current->HasSameIndexAndSerialNumber(Binding.Value))
        {
            Why=FString::Printf(TEXT("Authored handle binding conflict at %s; expected emitter=%s; actual emitter=%s"),
                *Binding.Key.ToString(),*GetPathNameSafe(Binding.Value.Get()),Current?*GetPathNameSafe(Current->Get()):TEXT("<absent>"));return false;
        }
    }
    if(Expected.EmitterBindings.Num()!=Actual.EmitterBindings.Num()){Why=TEXT("Stateless authored handle set changed");return false;}
    TArray<FString> Keys;Expected.GetKeys(Keys);for(const auto& Pair:Actual)if(!Expected.Contains(Pair.Key))Keys.Add(Pair.Key);Keys.Sort();
    for(const FString& Key:Keys)
    {
        const FString* Before=Expected.Find(Key);const FString* After=Actual.Find(Key);
        if(!Before||!After||*Before!=*After)
        {
            Why=FString::Printf(TEXT("Authored conflict at %s; expected=%s; actual=%s"),*Key,
                Before?*Before->Left(512):TEXT("<absent>"),After?*After->Left(512):TEXT("<absent>"));return false;
        }
    }
    return true;
}
bool CheckAuthored(UNiagaraSystem* System,const FAuthoredValues& Expected,FString& Why)
{FAuthoredValues Actual;return CaptureAuthored(System,Actual,Why)&&SameAuthored(Expected,Actual,Why);}
FObject Inspect(UNiagaraSystem* System)
{
	FObject R=Result(); TArray<TSharedPtr<FJsonValue>> Rows;
	for(const auto& Handle:System->GetEmitterHandles())
	{
		UObject* Emitter=StatelessObject(Handle); if(!Emitter) { continue; }
		FObject Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("emitter"),Handle.GetName().ToString()); Row->SetStringField(TEXT("state_hash"),Hash(ObjectState(Emitter)));
		const auto* Spawn=FindFProperty<FArrayProperty>(Emitter->GetClass(),TEXT("SpawnInfos")); const auto* SpawnInner=Spawn ? CastField<FStructProperty>(Spawn->Inner) : nullptr;
		const auto* Modules=FindFProperty<FArrayProperty>(Emitter->GetClass(),TEXT("Modules")); const auto* ModuleInner=Modules ? CastField<FObjectPropertyBase>(Modules->Inner) : nullptr;
		if(!Spawn || !SpawnInner || !Modules || !ModuleInner) { return Error(TEXT("Stateless reflected spawn/module contract unavailable")); }
		FScriptArrayHelper Spawns(Spawn,Spawn->ContainerPtrToValuePtr<void>(Emitter)); TArray<TSharedPtr<FJsonValue>> SpawnRows;
		for(int32 I=0;I<Spawns.Num() && I<64;++I) { FObject Entry=MakeShared<FJsonObject>(); Entry->SetNumberField(TEXT("index"),I); FObject Fields=MakeShared<FJsonObject>(); Describe(SpawnInner->Struct,Spawns.GetRawPtr(I),Fields); Entry->SetObjectField(TEXT("properties"),Fields); SpawnRows.Add(MakeShared<FJsonValueObject>(Entry)); }
		Row->SetArrayField(TEXT("spawn_infos"),SpawnRows);
		FScriptArrayHelper Array(Modules,Modules->ContainerPtrToValuePtr<void>(Emitter)); TArray<TSharedPtr<FJsonValue>> ModuleRows;
		for(int32 I=0;I<Array.Num() && I<128;++I) { UObject* Module=ModuleInner->GetObjectPropertyValue(Array.GetRawPtr(I)); if(!Module) { continue; } FObject Entry=MakeShared<FJsonObject>(),Fields=MakeShared<FJsonObject>(); Describe(Module->GetClass(),Module,Fields); Entry->SetStringField(TEXT("name"),Module->GetName()); Entry->SetStringField(TEXT("class"),Module->GetClass()->GetPathName()); Entry->SetObjectField(TEXT("properties"),Fields); ModuleRows.Add(MakeShared<FJsonValueObject>(Entry)); }
		Row->SetArrayField(TEXT("modules"),ModuleRows); Rows.Add(MakeShared<FJsonValueObject>(Row));
	}
	R->SetArrayField(TEXT("emitters"),Rows); R->SetBoolField(TEXT("compilation_pending"),System->HasOutstandingCompilationRequests(true) || System->HasActiveCompilations()); R->SetBoolField(TEXT("system_valid"),System->IsValid()); return R;
}
bool Configure(UNiagaraSystem* System,const FGuid& SelectedHandleId,UObject* Emitter,const FObject& Spec,FString& Why,const TFunction<bool()>& JobCurrent)
{
    if(!Live(Emitter)){Why=TEXT("Live emitter required");return false;}
    TStrongObjectPtr<UObject> Hold(Emitter);const FString Path=Emitter->GetPathName();UClass* Class=Emitter->GetClass();
    auto Current=[&]{if(!JobCurrent()||!SafeContext()||!SelectedEmitterIsCurrent(System,SelectedHandleId,Emitter)||Emitter->GetPathName()!=Path||Emitter->GetClass()!=Class){Why=TEXT("Candidate/source/Editor or selected handle/emitter binding changed during settings callback");return false;}return true;};
    if(!Current())return false;
	if(Spec->HasField(TEXT("spawn_infos")))
	{
		const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr; if(!Spec->TryGetArrayField(TEXT("spawn_infos"),Rows) || !Rows || Rows->Num()>64) { Why=TEXT("At most 64 spawn info objects required"); return false; }
		auto* P=FindFProperty<FArrayProperty>(Emitter->GetClass(),TEXT("SpawnInfos")); auto* Inner=P ? CastField<FStructProperty>(P->Inner) : nullptr;
		if(!P || !Inner) { Why=TEXT("Spawn info contract unavailable"); return false; }
		FScriptArrayHelper Array(P,P->ContainerPtrToValuePtr<void>(Emitter)); Array.EmptyValues(); Array.AddValues(Rows->Num());
        for(int32 I=0;I<Rows->Num();++I) { const auto Object=(*Rows)[I]->Type==EJson::Object ? (*Rows)[I]->AsObject() : nullptr; if(!Object || !Patch(Inner->Struct,Array.GetRawPtr(I),Object,Why)) { return false; } }
		FAuthoredValues BeforeCallback;if(!CaptureAuthored(System,BeforeCallback,Why))return false;
        FPropertyChangedEvent Event(P); Emitter->PostEditChangeProperty(Event);if(!Current()||!CheckAuthored(System,BeforeCallback,Why))return false;
	}
	if(Spec->HasField(TEXT("module_edits")))
	{
		const TArray<TSharedPtr<FJsonValue>>* Edits=nullptr; if(!Spec->TryGetArrayField(TEXT("module_edits"),Edits) || !Edits || Edits->Num()>128) { Why=TEXT("At most 128 exact module edits required"); return false; }
		auto* P=FindFProperty<FArrayProperty>(Emitter->GetClass(),TEXT("Modules")); auto* Inner=P ? CastField<FObjectPropertyBase>(P->Inner) : nullptr;
		if(!P || !Inner) { Why=TEXT("Module contract unavailable"); return false; }
		FScriptArrayHelper Array(P,P->ContainerPtrToValuePtr<void>(Emitter));
		for(const auto& Value:*Edits)
		{
            const auto Edit=Value->Type==EJson::Object ? Value->AsObject() : nullptr; if(!Edit) { Why=TEXT("Module edit object required"); return false; }
            const FString Name=String(Edit,TEXT("module_name")); const TSharedPtr<FJsonObject>* Settings=nullptr;
            if(Name.IsEmpty() || !Edit->TryGetObjectField(TEXT("settings"),Settings)) { Why=TEXT("Exact module_name/settings required"); return false; }
			if(!Current())return false;
            FScriptArrayHelper CurrentArray(P,P->ContainerPtrToValuePtr<void>(Emitter));
            UObject* Target=nullptr;
			for(int32 I=0;I<CurrentArray.Num();++I) { UObject* M=Inner->GetObjectPropertyValue(CurrentArray.GetRawPtr(I)); if(M && M->GetName()==Name) { if(Target) { Why=TEXT("Ambiguous module name"); return false; } Target=M; } }
			if(!Target || !Target->IsIn(Emitter) || !Target->GetClass()->GetPathName().StartsWith(TEXT("/Script/Niagara."))) { Why=TEXT("An emitter-owned engine module is required"); return false; }
			for(const auto& Pair:(*Settings)->Values) { FString Key(*Pair.Key),First; Key.Split(TEXT("."),&First,nullptr); if(First.IsEmpty()) { First=Key; } if(!BoundedText(First,NAME_SIZE-1)){Why=TEXT("Property name exceeds engine token bound");return false;} const FProperty* Field=FindFProperty<FProperty>(Target->GetClass(),*First); if(!Field || !Field->HasAnyPropertyFlags(CPF_Edit) || Field->HasAnyPropertyFlags(CPF_Transient)) { Why=TEXT("Only editable authored module fields are supported"); return false; } }
			if(!Patch(Target->GetClass(),Target,*Settings,Why)) { return false; }
			FAuthoredValues BeforeCallback;if(!CaptureAuthored(System,BeforeCallback,Why))return false;
            TStrongObjectPtr<UObject> ModuleHold(Target);Target->PostEditChange();if(!Current()||!Live(Target)||!Target->IsIn(Emitter)){Why=TEXT("Owned module changed during callback");return false;}
            if(!CheckAuthored(System,BeforeCallback,Why))return false;
		}
	}
	return true;
}
}


namespace
{
struct FNiagaraBuildJob
{
    TStrongObjectPtr<UNiagaraSystem> Candidate,Source;
    TStrongObjectPtr<UPackage> Package;
    TWeakObjectPtr<UWorld> World;
    FString Path,SourcePath,Phase=TEXT("initial_compile");
    FObject Request,Reply;
    FAuthoredValues Authored;
    bool Done=false;
    double PhaseStarted=FPlatformTime::Seconds();
    bool Current()const
    {
        return SafeContext()&&Live(Candidate.Get())&&Live(Package.Get())&&Live(World.Get())&&!World->bIsTearingDown&&
            GEditor->GetEditorWorldContext().World()==World.Get()&&Candidate->GetPackage()==Package.Get()&&Candidate->GetPathName()==Path&&
            (!Source.IsValid()||(Live(Source.Get())&&Source->GetPathName()==SourcePath&&!Source->GetPackage()->IsDirty()));
    }
};
TMap<FString,TSharedPtr<FNiagaraBuildJob>> NiagaraBuilds;
FTSTicker::FDelegateHandle NiagaraTicker;
void FailJob(FNiagaraBuildJob& J,const FString& Why,bool Pending=false)
{
    const bool EnginePending=Pending||(Live(J.Candidate.Get())&&(J.Candidate->HasOutstandingCompilationRequests(true)||J.Candidate->HasActiveCompilations()));
    const FObject Previous=J.Reply;
    J.Reply=CandidateFailure(J.Current()?J.Candidate.Get():nullptr,Why,EnginePending);
    // Keep compiler facts already observed before a publication/check failure;
    // failure of authored validation is not evidence that compilation failed.
    if(Previous.IsValid())for(const TCHAR* Key:{TEXT("compile_attempted"),TEXT("compile_succeeded")})
    {bool Observed;if(Previous->TryGetBoolField(Key,Observed))J.Reply->SetBoolField(Key,Observed);}
    J.Reply->SetStringField(TEXT("target_path"),J.Path);
    J.Reply->SetStringField(TEXT("stage"),J.Phase);
    J.Reply->SetBoolField(TEXT("operation_complete"),true);
    J.Done=true;J.Source.Reset();J.Package.Reset();J.World.Reset();
}
bool Busy(const UNiagaraSystem* System)
{return System->HasOutstandingCompilationRequests(true)||System->HasActiveCompilations();}
void Advance(FNiagaraBuildJob& J)
{
    if(J.Done)return;
    if(!J.Current()){FailJob(J,TEXT("Candidate/source/Editor identity changed; stop without follow-up writer"));return;}
    FString Conflict;if(!CheckAuthored(J.Candidate.Get(),J.Authored,Conflict)){FailJob(J,Conflict);return;}
    if(J.Phase==TEXT("initial_compile")||J.Phase==TEXT("final_compile"))
    {
        // One nonblocking engine poll per frame; no Sleep or game-thread wait loop.
        J.Candidate->PollForCompilationComplete();
        if(!J.Current()){FailJob(J,TEXT("Candidate changed during native compilation poll"));return;}
        if(!CheckAuthored(J.Candidate.Get(),J.Authored,Conflict)){FailJob(J,Conflict);return;}
        if(Busy(J.Candidate.Get()))
        {
            if(FPlatformTime::Seconds()-J.PhaseStarted>=10.0)FailJob(J,TEXT("Engine compilation remains pending; candidate retained, no save or retry"),true);
            return;
        }
        if(J.Phase==TEXT("initial_compile"))
        {
            J.Phase=TEXT("apply_spec");FString Why;
            const TArray<TSharedPtr<FJsonValue>>* Specs=nullptr;
            if(J.Request->TryGetArrayField(TEXT("emitters"),Specs))
            {
                for(const auto& Value:*Specs)
                {
                    if(!Value.IsValid()||Value->Type!=EJson::Object){FailJob(J,TEXT("Emitter spec object required"));return;}
                    const auto Spec=Value->AsObject();const FString Name=String(Spec,TEXT("emitter_name"));UObject* Emitter=nullptr;FGuid SelectedHandleId;
                    for(const auto& H:J.Candidate->GetEmitterHandles())if(H.GetName().ToString()==Name)
                    {if(Emitter){FailJob(J,TEXT("Ambiguous emitter name"));return;}Emitter=StatelessObject(H);SelectedHandleId=H.GetId();}
                    if(!Live(Emitter)||!Emitter->IsIn(J.Candidate.Get())){FailJob(J,TEXT("Exact stateless emitter name required"));return;}
                    TStrongObjectPtr<UObject> Hold(Emitter);
                    if(!Configure(J.Candidate.Get(),SelectedHandleId,Emitter,Spec,Why,[&J]{return J.Current();})){FailJob(J,Why);return;}
                    if(!J.Current()||!SelectedEmitterIsCurrent(J.Candidate.Get(),SelectedHandleId,Emitter)||!Emitter->IsIn(J.Candidate.Get())){FailJob(J,TEXT("Selected handle/emitter binding changed during settings callback"));return;}
                }
            }
            if(!CaptureAuthored(J.Candidate.Get(),J.Authored,Conflict)){FailJob(J,Conflict);return;}
            J.Phase=TEXT("final_compile");J.PhaseStarted=FPlatformTime::Seconds();
            J.Candidate->RequestCompile(false);
            if(!J.Current()){FailJob(J,TEXT("Candidate changed during native compile request"));return;}
            if(!CheckAuthored(J.Candidate.Get(),J.Authored,Conflict)){FailJob(J,Conflict);return;}
            // Return to the editor before the next compile poll.
            return;
        }
        J.Phase=TEXT("publish");
        if(!J.Candidate->IsValid()){FailJob(J,TEXT("Candidate system invalid after compilation"));return;}
        J.Reply=Inspect(J.Candidate.Get());
        J.Reply->SetBoolField(TEXT("compile_succeeded"),true);
        J.Reply->SetBoolField(TEXT("compile_attempted"),true);
        PublishCandidate(J.Candidate.Get(),J.Reply);
        if(!J.Current()){FailJob(J,TEXT("Candidate/source/Editor changed during publication"));return;}
        if(!J.Reply->GetBoolField(TEXT("success"))){const FString Why=String(J.Reply,TEXT("error"));FailJob(J,Why.IsEmpty()?TEXT("Candidate publication failed"):Why);return;}
        if(!CheckAuthored(J.Candidate.Get(),J.Authored,Conflict)){FailJob(J,Conflict);return;}
        J.Reply->SetStringField(TEXT("stage"),TEXT("complete"));
        J.Reply->SetBoolField(TEXT("operation_complete"),true);
        J.Done=true;J.Source.Reset();J.Package.Reset();J.World.Reset();
    }
}
FObject JobReply(const FNiagaraBuildJob& J)
{
    if(J.Done)return J.Reply;
    auto R=Result();R->SetStringField(TEXT("target_path"),J.Path);R->SetStringField(TEXT("stage"),J.Phase);
    R->SetBoolField(TEXT("accepted"),true);R->SetBoolField(TEXT("operation_complete"),false);
    R->SetBoolField(TEXT("compilation_pending"),true);R->SetBoolField(TEXT("candidate_retained"),true);
    R->SetBoolField(TEXT("do_not_retry"),true);R->SetBoolField(TEXT("read_only"),false);
    R->SetBoolField(TEXT("mutation_performed"),true);R->SetBoolField(TEXT("change_state_known"),false);
    R->SetStringField(TEXT("next_action"),TEXT("Inspect target with inspect_niagara_stateless_authoring; accepted is not compilation/publication completion. Never replay creation/variant."));
    return R;
}
void StartTicker()
{
    if(NiagaraTicker.IsValid())return;
    NiagaraTicker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
    {
        TArray<TSharedPtr<FNiagaraBuildJob>> Jobs;NiagaraBuilds.GenerateValueArray(Jobs);
        for(const auto& J:Jobs)if(J.IsValid()&&!J->Done)Advance(*J);
        // Keep the delegate until subsystem shutdown; finished results remain observable.
        return true;
    }),0.02f);
}
}
void ShutdownNiagara()
{
    if(NiagaraTicker.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(NiagaraTicker);NiagaraTicker.Reset();}
    // Dropping a continuation is not a claim that engine compilation was cancelled.
    NiagaraBuilds.Empty();
}
FObject Niagara(const FObject& P)
{
    const FString Op=String(P,TEXT("operation"));
    if(Op==TEXT("authoring.niagara.inspect"))
    {
        const FString Path=String(P,TEXT("asset_path"));auto* System=Path.StartsWith(TEXT("/"))?LoadObject<UNiagaraSystem>(nullptr,*Path):nullptr;
        if(!System)return Error(TEXT("Niagara system required"));
        auto R=Inspect(System);
        if(const auto* J=NiagaraBuilds.Find(System->GetPathName()))R->SetObjectField(TEXT("authoring_status"),JobReply(**J));
        int32 Pending=0;for(const auto& E:NiagaraBuilds)Pending+=!E.Value->Done||(Live(E.Value->Candidate.Get())&&Busy(E.Value->Candidate.Get()));R->SetNumberField(TEXT("domain_pending_count"),Pending);
        return R;
    }
    const FString Target=String(P,TEXT("target_path")),Source=String(P,TEXT("source_path"));FString Why;
    if(!UnusedAssetPath(Target,Why))return Error(Why);
    if(NiagaraBuilds.Num()>=32)
    {for(auto It=NiagaraBuilds.CreateIterator();It;++It)if(It.Value()->Done){It.RemoveCurrent();break;}}
    if(NiagaraBuilds.Num()>=32)return Error(TEXT("Too many active retained Niagara operations; inspect pending work"));
    TStrongObjectPtr<UNiagaraSystem> SourceSystem(Source.IsEmpty()?nullptr:LoadObject<UNiagaraSystem>(nullptr,*Source));
    if(!Source.IsEmpty()&&(!IndependentSavedSource(SourceSystem.Get(),Why)||Busy(SourceSystem.Get())))return Error(Why.IsEmpty()?TEXT("Fully compiled source required"):Why);
    const TArray<TSharedPtr<FJsonValue>>* Specs=nullptr;
    if(P->HasField(TEXT("emitters"))&&(!P->TryGetArrayField(TEXT("emitters"),Specs)||!Specs||Specs->Num()>16))return Error(TEXT("At most16 emitter edit specs required"));
    auto R=Result();R->SetBoolField(TEXT("dry_run"),DryRun(P));R->SetStringField(TEXT("target_path"),Target);
    if(DryRun(P)){R->SetStringField(TEXT("validation_level"),TEXT("path/source preflight; full candidate validation occurs during asynchronous apply"));return R;}
    UPackage* Package=CreatePackage(*Target);const FName Name(*FPackageName::GetLongPackageAssetName(Target));
    auto J=MakeShared<FNiagaraBuildJob>();J->Package.Reset(Package);J->Source.Reset(SourceSystem.Get());J->SourcePath=SourceSystem.IsValid()?SourceSystem->GetPathName():TEXT("");J->World=GEditor->GetEditorWorldContext().World();J->Request=P;
    J->Candidate.Reset(SourceSystem.IsValid()?DuplicateObject<UNiagaraSystem>(SourceSystem.Get(),Package,Name):NewObject<UNiagaraSystem>(Package,Name,RF_Public|RF_Standalone|RF_Transactional));
    J->Path=Target+TEXT(".")+Name.ToString();
    if(!J->Candidate.IsValid()){FailJob(*J,TEXT("Unable to allocate candidate system; package may remain"));return JobReply(*J);}
    J->Candidate->SetFlags(RF_Public|RF_Standalone|RF_Transactional);
    if(!SourceSystem.IsValid())
    {
        UNiagaraSystemFactoryNew::InitializeSystem(J->Candidate.Get(),true);
        if(!J->Current()){FailJob(*J,TEXT("Candidate changed during system initialization"));return JobReply(*J);}
        auto ViewModel=MakeShared<FNiagaraSystemViewModel>();FNiagaraSystemViewModelOptions Options;Options.bCanAutoCompile=false;Options.bCanSimulate=false;Options.bIsForDataProcessingOnly=true;Options.MessageLogGuid=FGuid::NewGuid();
        ViewModel->Initialize(*J->Candidate,Options);if(!J->Current()){FailJob(*J,TEXT("Candidate changed during ViewModel initialization"));return JobReply(*J);}
        const auto Handle=ViewModel->AddStatelessEmitter();
        if(!J->Current()||!Handle.IsValid()||!Handle->GetEmitterHandle()||Handle->GetEmitterHandle()->GetEmitterMode()!=ENiagaraEmitterMode::Stateless)
        {FailJob(*J,TEXT("Editor stateless creation failed"));return JobReply(*J);}
    }
    if(!CaptureAuthored(J->Candidate.Get(),J->Authored,Why)){FailJob(*J,Why);return JobReply(*J);}
    NiagaraBuilds.Add(J->Path,J);Advance(*J);if(!J->Done)StartTicker();return JobReply(*J);
}
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDomainNiagaraAuthoredConflictTest,
    "CubelessMCP.DomainExpansion.Authoring.NiagaraAuthoredConflict",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDomainNiagaraAuthoredConflictTest::RunTest(const FString& Parameters)
{
    FAuthoredValues Expected;Expected.Add(TEXT("emitter[E].spawn[0].Amount.Min"),TEXT("8"));
    Expected.Add(TEXT("emitter[E].module[0].bModuleEnabled"),TEXT("True"));
    FString Why;auto Actual=Expected;
    TestTrue(TEXT("Unchanged supported authored fields compare equal"),SameAuthored(Expected,Actual,Why));
    Actual[TEXT("emitter[E].spawn[0].Amount.Min")]=TEXT("99");
    TestFalse(TEXT("A frame or compile callback cannot replace requested spawn value"),SameAuthored(Expected,Actual,Why));
    TestTrue(TEXT("Conflict identifies first field and both values"),Why.Contains(TEXT("Amount.Min"))&&Why.Contains(TEXT("expected=8"))&&Why.Contains(TEXT("actual=99")));
    Actual=Expected;Actual.Remove(TEXT("emitter[E].module[0].bModuleEnabled"));
    TestFalse(TEXT("Removed authored module field is a conflict"),SameAuthored(Expected,Actual,Why));
    Actual=Expected;Actual.Add(TEXT("emitter[E].module[1].identity"),TEXT("Added:Module"));
    TestFalse(TEXT("New authored module cannot be silently published"),SameAuthored(Expected,Actual,Why));
    // Same class and authored values do not make a replacement emitter the selected object.
    TStrongObjectPtr<UObject> OriginalBinding(NewObject<UCurveFloat>()),ReplacementBinding(NewObject<UCurveFloat>());
    const FGuid HandleId=FGuid::NewGuid();Expected.EmitterBindings.Add(HandleId,OriginalBinding.Get());
    Actual=Expected;Actual.EmitterBindings[HandleId]=ReplacementBinding.Get();
    TestFalse(TEXT("Identical authored values cannot hide handle-to-emitter replacement"),SameAuthored(Expected,Actual,Why));
    TestTrue(TEXT("Handle conflict identifies the selected handle"),Why.Contains(HandleId.ToString()));
    Actual=Expected;TestTrue(TEXT("Stable selected emitter does not freeze regenerated caches"),SameAuthored(Expected,Actual,Why));
    // Real installed reflected type proves the filter, not just synthetic map equality.
    UScriptStruct* SpawnType=FindObject<UScriptStruct>(nullptr,TEXT("/Script/Niagara.NiagaraStatelessSpawnInfo"));
    if(!TestNotNull(TEXT("Installed stateless spawn reflected type"),SpawnType))return false;
    FStructOnScope Spawn(SpawnType);FAuthoredValues SpawnBefore,SpawnAfter;
    CaptureEditableLeaves(SpawnType,Spawn.GetStructMemory(),TEXT("spawn."),SpawnBefore);
    TestTrue(TEXT("Editable spawn leaf is guarded"),SpawnBefore.Contains(TEXT("spawn.bEnabled")));
    auto* SourceId=FindFProperty<FStructProperty>(SpawnType,TEXT("SourceId"));
    if(!TestNotNull(TEXT("Editor source identity field"),SourceId))return false;
    *SourceId->ContainerPtrToValuePtr<FGuid>(Spawn.GetStructMemory())=FGuid::NewGuid();
    CaptureEditableLeaves(SpawnType,Spawn.GetStructMemory(),TEXT("spawn."),SpawnAfter);
    TestTrue(TEXT("Non-authored SourceId reconstruction is allowed"),SameAuthored(SpawnBefore,SpawnAfter,Why));
    auto* Enabled=FindFProperty<FBoolProperty>(SpawnType,TEXT("bEnabled"));
    if(!TestNotNull(TEXT("Editable spawn enable flag"),Enabled))return false;
    Enabled->SetPropertyValue_InContainer(Spawn.GetStructMemory(),!Enabled->GetPropertyValue_InContainer(Spawn.GetStructMemory()));
    SpawnAfter.Empty();CaptureEditableLeaves(SpawnType,Spawn.GetStructMemory(),TEXT("spawn."),SpawnAfter);
    TestFalse(TEXT("Actual supported spawn edit is detected"),SameAuthored(SpawnBefore,SpawnAfter,Why));
    return true;
}
#endif

}
