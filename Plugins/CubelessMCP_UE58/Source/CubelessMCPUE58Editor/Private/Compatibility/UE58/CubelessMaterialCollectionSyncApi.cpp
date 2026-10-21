// CY: original7 one named MPC-sync controller; initial bounded Editor world slice.
#include "Compatibility/UE58/CubelessMaterialCollectionSyncApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Containers/Ticker.h"
#include "Containers/UnrealString.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/World.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include <cfloat>

namespace Cubeless::UE58::MaterialCollectionSync
{
namespace
{
using FResult=Variable::FResult;
constexpr int32 MaxParameters=1024,MaxNamesJson=131072;
bool bOwnerActive=false,bInOperation=false;
struct FSyncState
{
    bool bEnabled=false;
    FString SourcePath,TargetPath,LastError,StopReason;
    TSet<FName> Filter;
    float Interval=0.1f,Accumulated=0.0f;
    FTSTicker::FDelegateHandle Ticker;
    FDelegateHandle WorldCleanup;
    TStrongObjectPtr<UMaterialParameterCollection> Source{nullptr},Target{nullptr};
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<UMaterialParameterCollectionInstance> SourceInstance,TargetInstance;
    TArray<FName> Scalars,Vectors;
    FString SourceLayout,TargetLayout;
    uint64 TickCount=0,Generation=0,CallbackCount=0;
    int32 LastScalarCount=0,LastVectorCount=0;
    TSharedPtr<FJsonObject> LastCopy;
    TSharedPtr<FJsonObject> LastTick;
};
FSyncState State;
bool Loaded(const UObject* O)
{ return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
bool HasNul(const FString& S)
{ for(int32 I=0;I<S.Len();++I) { if(S[I]==TCHAR('\0')) { return true; } }return false; }
bool ColorFinite(const FLinearColor& C)
{ return FMath::IsFinite(C.R)&&FMath::IsFinite(C.G)&&FMath::IsFinite(C.B)&&FMath::IsFinite(C.A); }
bool ColorExact(const FLinearColor& A,const FLinearColor& B)
{ return A.R==B.R&&A.G==B.G&&A.B==B.B&&A.A==B.A; }
void Count(FResult& R,const TCHAR* Key)
{ R.Data->SetNumberField(Key,R.Data->GetNumberField(Key)+1); }
bool Fail(FResult& R,const TCHAR* Code,const FString& Error)
{ R.bSuccess=false;R.ErrorCode=Code;R.Error=Error;return false; }
void Init(FResult& R,bool Defaults,bool Save)
{
    R.bSaveRequested=Defaults&&Save;
    for(const TCHAR* K:{TEXT("outer_transaction_calls"),TEXT("controller_stop_calls"),TEXT("ticker_remove_calls"),TEXT("world_cleanup_remove_calls"),
        TEXT("asset_load_calls"),TEXT("asset_load_returns"),TEXT("world_instance_lookup_calls"),TEXT("scalar_getter_calls"),TEXT("vector_getter_calls"),
        TEXT("scalar_setter_calls"),TEXT("vector_setter_calls"),TEXT("scalar_setter_returned_true"),TEXT("vector_setter_returned_true"),
        TEXT("asset_modify_calls"),TEXT("default_assignment_calls"),TEXT("post_edit_change_calls"),TEXT("mark_package_dirty_calls"),
        TEXT("save_attempt_count"),TEXT("save_return_count"),TEXT("ticker_add_calls"),TEXT("world_cleanup_add_calls"),TEXT("layout_query_calls"),TEXT("layout_parameter_visits")}) { R.Data->SetNumberField(K,0); }
    R.Data->SetBoolField(TEXT("caller_save_on_enable"),Save);R.Data->SetBoolField(TEXT("set_asset_defaults_on_enable"),Defaults);
    R.Data->SetBoolField(TEXT("asset_mutation_attempted"),false);R.Data->SetBoolField(TEXT("runtime_mutation_attempted"),false);
    R.Data->SetBoolField(TEXT("initial_sync"),false);R.Data->SetBoolField(TEXT("saved_on_enable"),false);
    R.Data->SetBoolField(TEXT("runtime_lazy_creation_possible"),false);R.Data->SetBoolField(TEXT("automatic_reload_or_restart"),false);
    R.Data->SetBoolField(TEXT("explicit_compile_requested"),false);R.Data->SetBoolField(TEXT("whole_original_or_render_runtime_verified"),false);
    R.Data->SetStringField(TEXT("changed_scope"),TEXT("Owned controller and selected effective/default value receipts; public instance lookup/render callbacks, source/default/PostEdit material fanout and global world effects are not whole-state measured."));
    R.Data->SetStringField(TEXT("ticker_scope"),TEXT("Original delayed CoreTicker plus float frame-delta accumulator retained, not exact wall-clock cadence. One strong-MPC job borrows one weak current Editor world; no cross-world follow/reload/restart. Tick receipts occur outside the initiating journal."));
    R.Data->SetStringField(TEXT("success_scope"),TEXT("Status/stop/controller-enabled point receipt only; enabled is an ongoing job, not proof of future callbacks, rendering, cold/cooked execution or whole-world stability. Original returned savedFalse does not negate successful controller activation."));
    R.Data->SetStringField(TEXT("stop_scope"),TEXT("Remove only this owned ticker/cleanup handle, release refs/cache/accumulator; retain original paths/filter/interval/tick/last counters/error. Runtime overrides are not restored and assets are not saved by stop."));
    R.Data->SetArrayField(TEXT("save_calls"),{});
}
TSharedRef<FJsonObject> Status()
{
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("enabled"),State.bEnabled);
    J->SetStringField(TEXT("source_collection_path"),State.SourcePath);J->SetStringField(TEXT("target_collection_path"),State.TargetPath);
    J->SetNumberField(TEXT("interval_seconds"),State.Interval);J->SetNumberField(TEXT("tick_count"),static_cast<double>(State.TickCount));
    J->SetStringField(TEXT("tick_count_decimal"),LexToString(State.TickCount));J->SetStringField(TEXT("generation_decimal"),LexToString(State.Generation));
    J->SetNumberField(TEXT("last_scalar_count"),State.LastScalarCount);J->SetNumberField(TEXT("last_vector_count"),State.LastVectorCount);
    J->SetNumberField(TEXT("cached_scalar_count"),State.Scalars.Num());J->SetNumberField(TEXT("cached_vector_count"),State.Vectors.Num());
    J->SetStringField(TEXT("last_error"),State.LastError);J->SetStringField(TEXT("stop_reason"),State.StopReason);
    J->SetNumberField(TEXT("callback_count"),static_cast<double>(State.CallbackCount));J->SetStringField(TEXT("callback_count_decimal"),LexToString(State.CallbackCount));
    J->SetBoolField(TEXT("ticker_handle_valid"),State.Ticker.IsValid());J->SetBoolField(TEXT("world_cleanup_handle_valid"),State.WorldCleanup.IsValid());
    J->SetBoolField(TEXT("owner_active"),bOwnerActive);J->SetStringField(TEXT("world_path"),State.World.IsValid()?State.World->GetPathName():FString());
    J->SetStringField(TEXT("source_object_path"),State.Source.IsValid()?State.Source->GetPathName():FString());J->SetStringField(TEXT("target_object_path"),State.Target.IsValid()?State.Target->GetPathName():FString());
    J->SetStringField(TEXT("source_layout_json"),State.SourceLayout);J->SetStringField(TEXT("target_layout_json"),State.TargetLayout);
    TArray<TSharedPtr<FJsonValue>> Names;for(FName Name:State.Filter) { Names.Add(MakeShared<FJsonValueString>(Name.ToString())); }J->SetArrayField(TEXT("parameter_names"),Names);
    if(State.LastCopy.IsValid()) { J->SetObjectField(TEXT("last_copy_receipt"),State.LastCopy); }
    if(State.LastTick.IsValid()) { J->SetObjectField(TEXT("last_tick_receipt"),State.LastTick); }return J;
}
void Publish(FResult& R)
{
    const auto J=Status();R.Data->SetObjectField(TEXT("controller_status"),J);
    for(const auto& P:J->Values) { R.Data->SetField(P.Key,P.Value); }
}
void Stop(const FString& Reason,FResult* R=nullptr)
{
    if(R) { Count(*R,TEXT("controller_stop_calls")); }
    State.bEnabled=false;++State.Generation;State.StopReason=Reason;
    if(State.Ticker.IsValid()) { if(R) { Count(*R,TEXT("ticker_remove_calls")); }FTSTicker::RemoveTicker(State.Ticker);State.Ticker.Reset(); }
    if(State.WorldCleanup.IsValid()) { if(R) { Count(*R,TEXT("world_cleanup_remove_calls")); }FWorldDelegates::OnWorldCleanup.Remove(State.WorldCleanup);State.WorldCleanup.Reset(); }
    State.Source.Reset();State.Target.Reset();State.World.Reset();State.SourceInstance.Reset();State.TargetInstance.Reset();
    State.Scalars.Reset();State.Vectors.Reset();State.SourceLayout.Reset();State.TargetLayout.Reset();State.Accumulated=0.0f;
}
void WorldCleanup(UWorld* World,bool,bool)
{ if(IsInGameThread()&&State.World.Get()==World) { State.LastError=TEXT("Owned borrowed world cleanup; job cancelled, no follow-world restart.");Stop(TEXT("world_cleanup")); } }
FString NormalizePath(const FString& Path)
{
    FString S=FPackageName::ExportTextPathToObjectPath(Path).TrimStartAndEnd();S.TrimQuotesInline();
    if((S.StartsWith(TEXT("/Game/"))||S.StartsWith(TEXT("/Engine/")))&&!S.Contains(TEXT(".")))
    { S=FString::Printf(TEXT("%s.%s"),*S,*FPackageName::GetShortName(S)); }return S;
}
UMaterialParameterCollection* Resolve(const FString& Path,bool Target,FResult& R)
{
    const FString S=NormalizePath(Path);
    if(S.IsEmpty()||S.Len()>2048||HasNul(S)||(!S.StartsWith(TEXT("/Game/"))&&!S.StartsWith(TEXT("/Engine/"))))
    { Fail(R,TEXT("material_sync_path_unsupported"),TEXT("Finite exact Game/Engine package/object collection path required; original relative/custom mount variants remain incomplete."));return nullptr; }
    Count(R,TEXT("asset_load_calls"));auto* C=Cast<UMaterialParameterCollection>(LoadObject<UObject>(nullptr,*S));Count(R,TEXT("asset_load_returns"));
    if(!Loaded(C)||C->GetClass()!=UMaterialParameterCollection::StaticClass()||!C->IsAsset()||C->GetBaseParameterCollection()
        ||C->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_Transient)||C->GetOutermost()->ContainsMap()
        ||C->GetOutermost()->HasAnyPackageFlags(PKG_Cooked)||(Target&&!C->GetOutermost()->GetName().StartsWith(TEXT("/Game/"))))
    { Fail(R,TEXT("material_sync_collection_unsupported"),TEXT("Loaded exact native uncooked MPC without inheritance required; target must be Game. Original valid variants remain residual."));return nullptr; }
    return C;
}
bool Filter(const FString& Json,TSet<FName>& Names,FResult& R)
{
    if(Json.Len()>MaxNamesJson||HasNul(Json)) { return Fail(R,TEXT("material_sync_filter_invalid"),TEXT("Bounded JSON names required.")); }
    TSharedPtr<FJsonObject> O;const auto Reader=TJsonReaderFactory<>::Create(TEXT("{\"names\":")+Json+TEXT("}"));
    if(!FJsonSerializer::Deserialize(Reader,O)||!O.IsValid()||O->Values.Num()!=1) { return Fail(R,TEXT("material_sync_filter_invalid"),TEXT("Original names array/null required.")); }
    const auto V=O->TryGetField(TEXT("names"));if(!V.IsValid()) { return Fail(R,TEXT("material_sync_filter_invalid"),TEXT("Names missing.")); }
    if(V->Type==EJson::Null) { return true; }if(V->Type!=EJson::Array||V->AsArray().Num()>MaxParameters) { return Fail(R,TEXT("material_sync_filter_invalid"),TEXT("At most1024 original text names required.")); }
    for(const auto& Item:V->AsArray())
    { if(!Item.IsValid()||Item->Type!=EJson::String) { return Fail(R,TEXT("material_sync_filter_invalid"),TEXT("Original public List[str] names required; raw coercion variants incomplete.")); }
      const FString Name=Item->AsString().TrimStartAndEnd();if(Name.Len()>512||HasNul(Name)) { return Fail(R,TEXT("material_sync_filter_invalid"),TEXT("Finite name bound exceeded.")); }if(!Name.IsEmpty()) { Names.Add(FName(*Name)); } }
    return true;
}
bool Layout(UMaterialParameterCollection* C,FString& Out,FResult& R)
{
    Count(R,TEXT("layout_query_calls"));
    if(!Loaded(C)||C->ScalarParameters.Num()+C->VectorParameters.Num()>MaxParameters) { return Fail(R,TEXT("material_sync_layout_invalid"),TEXT("Loaded bounded collection layout required.")); }
    TSet<FName> Names;TSet<FGuid> Ids;auto LayoutJson=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;Out.Reset();
    for(const auto& P:C->ScalarParameters)
    { Count(R,TEXT("layout_parameter_visits"));if(P.ParameterName.IsNone()||P.ParameterName.ToString().Len()>512||HasNul(P.ParameterName.ToString())||!P.Id.IsValid()||Names.Contains(P.ParameterName)||Ids.Contains(P.Id)||!FMath::IsFinite(P.DefaultValue)) { return Fail(R,TEXT("material_sync_layout_invalid"),TEXT("Unique finite native scalar declarations required.")); }
      Names.Add(P.ParameterName);Ids.Add(P.Id);auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("kind"),TEXT("scalar"));Row->SetStringField(TEXT("name"),P.ParameterName.ToString());Row->SetStringField(TEXT("id"),P.Id.ToString());Rows.Add(MakeShared<FJsonValueObject>(Row)); }
    for(const auto& P:C->VectorParameters)
    { Count(R,TEXT("layout_parameter_visits"));if(P.ParameterName.IsNone()||P.ParameterName.ToString().Len()>512||HasNul(P.ParameterName.ToString())||!P.Id.IsValid()||Names.Contains(P.ParameterName)||Ids.Contains(P.Id)||!ColorFinite(P.DefaultValue)) { return Fail(R,TEXT("material_sync_layout_invalid"),TEXT("Unique finite native vector declarations required.")); }
      Names.Add(P.ParameterName);Ids.Add(P.Id);auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("kind"),TEXT("vector"));Row->SetStringField(TEXT("name"),P.ParameterName.ToString());Row->SetStringField(TEXT("id"),P.Id.ToString());Rows.Add(MakeShared<FJsonValueObject>(Row)); }
    LayoutJson->SetArrayField(TEXT("rows"),Rows);FJsonSerializer::Serialize(LayoutJson,TJsonWriterFactory<>::Create(&Out));return true;
}
bool Instances(FResult& R,bool Initial=false)
{
    auto* W=State.World.Get();if(!Loaded(W)||W->WorldType!=EWorldType::Editor||W->bIsTearingDown||!GEditor||GEditor->GetEditorWorldContext().World()!=W||IsPlaySessionActive())
    { return Fail(R,TEXT("material_sync_world_changed"),TEXT("Borrowed original Editor world no longer current/ready; PIE/follow-world variant not selected.")); }
    Count(R,TEXT("world_instance_lookup_calls"));auto* SI=W->GetParameterCollectionInstance(State.Source.Get());
    Count(R,TEXT("world_instance_lookup_calls"));auto* TI=W->GetParameterCollectionInstance(State.Target.Get());
    R.Data->SetBoolField(TEXT("runtime_lazy_creation_possible"),true);
    if(!Loaded(SI)||!Loaded(TI)||SI->GetClass()!=UMaterialParameterCollectionInstance::StaticClass()||TI->GetClass()!=UMaterialParameterCollectionInstance::StaticClass()
        ||SI->GetOutermost()!=GetTransientPackage()||TI->GetOutermost()!=GetTransientPackage()
        ||SI->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject)||TI->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject)
        ||SI->GetCollection()!=State.Source.Get()||TI->GetCollection()!=State.Target.Get()
        ||(!Initial&&(SI!=State.SourceInstance.Get()||TI!=State.TargetInstance.Get())))
    { return Fail(R,TEXT("material_sync_instance_changed"),TEXT("Exact current transient native collection instances required; lookup effects retained, no fallback/restart.")); }
    State.SourceInstance=SI;State.TargetInstance=TI;return true;
}
TSharedRef<FJsonObject> Color(const FLinearColor& C)
{ auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("r"),C.R);J->SetNumberField(TEXT("g"),C.G);J->SetNumberField(TEXT("b"),C.B);J->SetNumberField(TEXT("a"),C.A);return J; }
bool Copy(bool Defaults,FResult& R)
{
    auto Receipt=MakeShared<FJsonObject>();State.LastCopy=Receipt;R.Data->SetObjectField(TEXT("copy_receipt"),Receipt);Receipt->SetBoolField(TEXT("complete"),false);
    Receipt->SetBoolField(TEXT("set_asset_defaults"),Defaults);Receipt->SetStringField(TEXT("generation_decimal"),LexToString(State.Generation));
    TArray<TSharedPtr<FJsonValue>> Rows;Receipt->SetArrayField(TEXT("rows"),Rows);State.LastScalarCount=State.LastVectorCount=0;
    if(!Instances(R)) { return false; }
    const TStrongObjectPtr<UMaterialParameterCollection> Source(State.Source.Get()),Target(State.Target.Get());
    const TStrongObjectPtr<UWorld> World(State.World.Get());const TStrongObjectPtr<UMaterialParameterCollectionInstance> SI(State.SourceInstance.Get()),TI(State.TargetInstance.Get());
    const uint64 Generation=State.Generation;bool DefaultChanged=false,All=true;
    auto ModifyOnce=[&](){if(!DefaultChanged) { R.Data->SetBoolField(TEXT("asset_mutation_attempted"),true);Count(R,TEXT("asset_modify_calls"));Target->Modify();DefaultChanged=true; }};
    for(FName Name:State.Scalars)
    {
        auto Row=MakeShared<FJsonObject>();Rows.Add(MakeShared<FJsonValueObject>(Row));Receipt->SetArrayField(TEXT("rows"),Rows);Row->SetStringField(TEXT("kind"),TEXT("scalar"));Row->SetStringField(TEXT("name"),Name.ToString());
        const auto* SP=Source->ScalarParameters.FindByPredicate([Name](const auto& P){return P.ParameterName==Name;});auto* TP=Target->ScalarParameters.FindByPredicate([Name](const auto& P){return P.ParameterName==Name;});
        if(!SP||!TP) { return Fail(R,TEXT("material_sync_layout_changed"),TEXT("Selected declaration disappeared; setter prefix retained.")); }
        float Value=SP->DefaultValue,Before=TP->DefaultValue;Count(R,TEXT("scalar_getter_calls"));const bool Got=SI->GetScalarParameterValue(Name,Value);
        Row->SetBoolField(TEXT("source_getter_returned"),Got);if(!FMath::IsFinite(Value)) { return Fail(R,TEXT("material_sync_value_nonfinite"),TEXT("Finite source scalar required.")); }
        const FGuid TargetId=TP->Id;Row->SetStringField(TEXT("source_parameter_id"),SP->Id.ToString());Row->SetStringField(TEXT("target_parameter_id"),TargetId.ToString());Row->SetNumberField(TEXT("source_value"),Value);Count(R,TEXT("scalar_getter_calls"));const bool BeforeGot=TI->GetScalarParameterValue(Name,Before);Row->SetBoolField(TEXT("target_before_getter_returned"),BeforeGot);Row->SetBoolField(TEXT("target_before_finite"),FMath::IsFinite(Before));
        if(FMath::IsFinite(Before)) { Row->SetNumberField(TEXT("target_before"),Before); }else { Row->SetField(TEXT("target_before"),MakeShared<FJsonValueNull>()); }
        R.bMutationAttempted=true;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("runtime_mutation_attempted"),true);Row->SetBoolField(TEXT("setter_attempted"),true);
        Count(R,TEXT("scalar_setter_calls"));const bool Set=TI->SetScalarParameterValue(Name,Value);Row->SetBoolField(TEXT("setter_returned"),Set);if(Set) { ++State.LastScalarCount;Count(R,TEXT("scalar_setter_returned_true")); }
        if(State.Generation!=Generation||!State.bEnabled) { return Fail(R,TEXT("material_sync_cancelled_during_copy"),TEXT("Owned job cancelled during public setter; no restart.")); }
        TP=Target->ScalarParameters.FindByPredicate([Name](const auto& P){return P.ParameterName==Name;});
        if(!TP||TP->Id!=TargetId) { return Fail(R,TEXT("material_sync_layout_changed"),TEXT("Selected scalar declaration changed during public setter; no default write through stale row.")); }
        if(Defaults&&!FMath::IsNearlyEqual(TP->DefaultValue,Value))
        { ModifyOnce();TP=Target->ScalarParameters.FindByPredicate([Name](const auto& P){return P.ParameterName==Name;});
          if(State.Generation!=Generation||!State.bEnabled||!TP||TP->Id!=TargetId) { return Fail(R,TEXT("material_sync_layout_changed"),TEXT("Selected scalar changed during Modify; actual prefix retained.")); }
          Count(R,TEXT("default_assignment_calls"));TP->DefaultValue=Value;Row->SetBoolField(TEXT("default_assigned"),true); }
        float After=0;Count(R,TEXT("scalar_getter_calls"));const bool GotAfter=TI->GetScalarParameterValue(Name,After);Row->SetBoolField(TEXT("target_after_getter_returned"),GotAfter);if(FMath::IsFinite(After)) { Row->SetNumberField(TEXT("target_after"),After); }else { Row->SetField(TEXT("target_after"),MakeShared<FJsonValueNull>()); }
        const bool OK=Set&&GotAfter&&FMath::IsFinite(After)&&After==Value;Row->SetBoolField(TEXT("postcondition_passed"),OK);All&=OK;
    }
    for(FName Name:State.Vectors)
    {
        auto Row=MakeShared<FJsonObject>();Rows.Add(MakeShared<FJsonValueObject>(Row));Receipt->SetArrayField(TEXT("rows"),Rows);Row->SetStringField(TEXT("kind"),TEXT("vector"));Row->SetStringField(TEXT("name"),Name.ToString());
        const auto* SP=Source->VectorParameters.FindByPredicate([Name](const auto& P){return P.ParameterName==Name;});auto* TP=Target->VectorParameters.FindByPredicate([Name](const auto& P){return P.ParameterName==Name;});
        if(!SP||!TP) { return Fail(R,TEXT("material_sync_layout_changed"),TEXT("Selected declaration disappeared; setter prefix retained.")); }
        const FGuid TargetId=TP->Id;Row->SetStringField(TEXT("source_parameter_id"),SP->Id.ToString());Row->SetStringField(TEXT("target_parameter_id"),TargetId.ToString());FLinearColor Value=SP->DefaultValue,Before=TP->DefaultValue;Count(R,TEXT("vector_getter_calls"));const bool Got=SI->GetVectorParameterValue(Name,Value);
        Row->SetBoolField(TEXT("source_getter_returned"),Got);if(!ColorFinite(Value)) { return Fail(R,TEXT("material_sync_value_nonfinite"),TEXT("Finite source vector required.")); }Row->SetObjectField(TEXT("source_value"),Color(Value));
        Count(R,TEXT("vector_getter_calls"));const bool BeforeGot=TI->GetVectorParameterValue(Name,Before);Row->SetBoolField(TEXT("target_before_getter_returned"),BeforeGot);Row->SetBoolField(TEXT("target_before_finite"),ColorFinite(Before));if(ColorFinite(Before)) { Row->SetObjectField(TEXT("target_before"),Color(Before)); }
        R.bMutationAttempted=true;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("runtime_mutation_attempted"),true);Row->SetBoolField(TEXT("setter_attempted"),true);
        Count(R,TEXT("vector_setter_calls"));const bool Set=TI->SetVectorParameterValue(Name,Value);Row->SetBoolField(TEXT("setter_returned"),Set);if(Set) { ++State.LastVectorCount;Count(R,TEXT("vector_setter_returned_true")); }
        if(State.Generation!=Generation||!State.bEnabled) { return Fail(R,TEXT("material_sync_cancelled_during_copy"),TEXT("Owned job cancelled during public setter; no restart.")); }
        TP=Target->VectorParameters.FindByPredicate([Name](const auto& P){return P.ParameterName==Name;});
        if(!TP||TP->Id!=TargetId) { return Fail(R,TEXT("material_sync_layout_changed"),TEXT("Selected vector declaration changed during public setter; no default write through stale row.")); }
        if(Defaults&&!TP->DefaultValue.Equals(Value))
        { ModifyOnce();TP=Target->VectorParameters.FindByPredicate([Name](const auto& P){return P.ParameterName==Name;});
          if(State.Generation!=Generation||!State.bEnabled||!TP||TP->Id!=TargetId) { return Fail(R,TEXT("material_sync_layout_changed"),TEXT("Selected vector changed during Modify; actual prefix retained.")); }
          Count(R,TEXT("default_assignment_calls"));TP->DefaultValue=Value;Row->SetBoolField(TEXT("default_assigned"),true); }
        FLinearColor After=FLinearColor::Transparent;Count(R,TEXT("vector_getter_calls"));const bool GotAfter=TI->GetVectorParameterValue(Name,After);Row->SetBoolField(TEXT("target_after_getter_returned"),GotAfter);if(ColorFinite(After)) { Row->SetObjectField(TEXT("target_after"),Color(After)); }
        const bool OK=Set&&GotAfter&&ColorFinite(After)&&ColorExact(After,Value);Row->SetBoolField(TEXT("postcondition_passed"),OK);All&=OK;
    }
    if(DefaultChanged) { Count(R,TEXT("post_edit_change_calls"));Target->PostEditChange();Count(R,TEXT("mark_package_dirty_calls"));Target->MarkPackageDirty(); }
    if(State.Generation!=Generation||!State.bEnabled) { return Fail(R,TEXT("material_sync_cancelled_during_copy"),TEXT("Owned job cancelled during default callbacks; no restart.")); }
    Receipt->SetBoolField(TEXT("asset_defaults_changed"),DefaultChanged);Receipt->SetNumberField(TEXT("scalar_count"),State.LastScalarCount);Receipt->SetNumberField(TEXT("vector_count"),State.LastVectorCount);
    Receipt->SetBoolField(TEXT("complete"),true);Receipt->SetBoolField(TEXT("postcondition_passed"),All);
    if(!All) { return Fail(R,TEXT("material_sync_copy_unverified"),TEXT("Runtime setter/readback mismatch; all returned ordered setter rows retained, no retry.")); }return true;
}
bool ObserveCopiedValues(FResult& R,const TCHAR* Key,bool Reacquire=false)
{
    auto J=MakeShared<FJsonObject>();R.Data->SetObjectField(Key,J);J->SetBoolField(TEXT("complete"),false);
    if(!State.bEnabled||!State.LastCopy.IsValid()||!Instances(R,Reacquire)) { return Fail(R,TEXT("material_sync_current_copy_unavailable"),TEXT("Owned current copy/world instances unavailable after callbacks; prefix retained.")); }
    FString S,T;if(!Layout(State.Source.Get(),S,R)||!Layout(State.Target.Get(),T,R)||!S.Equals(State.SourceLayout,ESearchCase::CaseSensitive)||!T.Equals(State.TargetLayout,ESearchCase::CaseSensitive))
    { return Fail(R,TEXT("material_sync_layout_changed"),TEXT("Selected declaration names/types/GUID/order changed after callbacks; retained prefix, no follow-up write.")); }
    TArray<TSharedPtr<FJsonValue>> Rows;J->SetArrayField(TEXT("rows"),Rows);bool All=true;
    for(const auto& V:State.LastCopy->GetArrayField(TEXT("rows")))
    {
        const auto Old=V->AsObject();const FName Name(*Old->GetStringField(TEXT("name")));auto Row=MakeShared<FJsonObject>();Rows.Add(MakeShared<FJsonValueObject>(Row));J->SetArrayField(TEXT("rows"),Rows);Row->SetStringField(TEXT("name"),Name.ToString());
        bool OK=false;
        if(Old->GetStringField(TEXT("kind"))==TEXT("scalar"))
        { float Current=0;Count(R,TEXT("scalar_getter_calls"));const bool Got=State.TargetInstance->GetScalarParameterValue(Name,Current);Row->SetBoolField(TEXT("getter_returned"),Got);Row->SetBoolField(TEXT("value_finite"),FMath::IsFinite(Current));if(FMath::IsFinite(Current)) { Row->SetNumberField(TEXT("value"),Current); }OK=Got&&FMath::IsFinite(Current)&&Current==Old->GetNumberField(TEXT("source_value")); }
        else
        { FLinearColor Current=FLinearColor::Transparent;Count(R,TEXT("vector_getter_calls"));const bool Got=State.TargetInstance->GetVectorParameterValue(Name,Current);Row->SetBoolField(TEXT("getter_returned"),Got);Row->SetBoolField(TEXT("value_finite"),ColorFinite(Current));if(ColorFinite(Current)) { Row->SetObjectField(TEXT("value"),Color(Current)); }const auto Expected=Old->GetObjectField(TEXT("source_value"));OK=Got&&ColorFinite(Current)&&Current.R==Expected->GetNumberField(TEXT("r"))&&Current.G==Expected->GetNumberField(TEXT("g"))&&Current.B==Expected->GetNumberField(TEXT("b"))&&Current.A==Expected->GetNumberField(TEXT("a")); }
        Row->SetBoolField(TEXT("postcondition_passed"),OK);All&=OK;
    }
    J->SetBoolField(TEXT("complete"),true);J->SetBoolField(TEXT("postcondition_passed"),All);
    return All?true:Fail(R,TEXT("material_sync_current_values_changed"),TEXT("Selected runtime values differ after default/save callbacks; no repair/retry."));
}
bool Tick(float DeltaTime)
{
    if(!IsInGameThread()) { return false; }
    if(!bOwnerActive||!State.bEnabled) { return false; }
    if(bInOperation) { return true; }
    TGuardValue<bool> Guard(bInOperation,true);++State.CallbackCount;
    if(!FMath::IsFinite(DeltaTime)||DeltaTime<0||!FMath::IsFinite(State.Accumulated+DeltaTime)) { State.LastError=TEXT("Ticker time nonfinite; no further setter dispatch.");Stop(TEXT("ticker_time_invalid"));return false; }
    State.Accumulated+=DeltaTime;if(State.Accumulated<State.Interval) { return true; }State.Accumulated=0.0f;
    FResult R;Init(R,false,false);FString S,T;auto TickReceipt=MakeShared<FJsonObject>();State.LastTick=TickReceipt;TickReceipt->SetBoolField(TEXT("complete"),false);TickReceipt->SetStringField(TEXT("callback_count_decimal"),LexToString(State.CallbackCount));
    auto PublishTick=[&](){auto Calls=MakeShared<FJsonObject>();for(const auto& P:R.Data->Values) { if(P.Value.IsValid()&&P.Value->Type==EJson::Number) { Calls->SetField(P.Key,P.Value); } }TickReceipt->SetObjectField(TEXT("native_call_counts"),Calls);TickReceipt->SetStringField(TEXT("error_code"),R.ErrorCode);TickReceipt->SetStringField(TEXT("error"),R.Error);TickReceipt->SetBoolField(TEXT("mutation_attempted"),R.bMutationAttempted);};
    if(!Layout(State.Source.Get(),S,R)||!Layout(State.Target.Get(),T,R)||!S.Equals(State.SourceLayout,ESearchCase::CaseSensitive)||!T.Equals(State.TargetLayout,ESearchCase::CaseSensitive)||!Copy(false,R))
    { if(R.Error.IsEmpty()) { Fail(R,TEXT("material_sync_layout_changed"),TEXT("Owned declaration layout changed; no follow-up write/reload.")); }State.LastError=R.Error;Stop(TEXT("tick_guard_failure"),&R);PublishTick();return false; }
    State.LastError.Reset();if(State.TickCount==MAX_uint64) { Fail(R,TEXT("material_sync_tick_counter_bound"),TEXT("Exact tick counter bound reached; actual returned copy prefix retained."));State.LastError=R.Error;Stop(TEXT("tick_counter_bound"),&R);PublishTick();return false; }++State.TickCount;
    PublishTick();TickReceipt->SetBoolField(TEXT("complete"),true);TickReceipt->SetBoolField(TEXT("postcondition_passed"),true);return State.bEnabled;
}
}

void Activate()
{ if(IsInGameThread()&&!bOwnerActive) { bOwnerActive=true; } }
void Shutdown()
{ if(IsInGameThread()) { bOwnerActive=false;Stop(TEXT("module_shutdown")); } }

Variable::FResult Control(const FString& RequestedAction,const FString& SourcePath,const FString& TargetPath,const FString& NamesJson,double Interval,bool Defaults,bool Save)
{
    FResult R;Init(R,Defaults,Save);R.Data->SetStringField(TEXT("requested_action"),RequestedAction);
    if(!IsEditorGameThread()||!bOwnerActive||bInOperation||RequestedAction.Len()>64||HasNul(RequestedAction)) { Fail(R,TEXT("material_sync_owner_or_action_invalid"),TEXT("Activated owned editor-module/game-thread/non-reentrant bounded action required."));return R; }
    TGuardValue<bool> Guard(bInOperation,true);Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Transaction(NSLOCTEXT("CubelessMCP","MaterialCollectionSync","Material Parameter Collection Sync"));
    const FString Action=RequestedAction.ToLower();R.Stage=TEXT("controller_before");R.Data->SetObjectField(TEXT("controller_before"),Status());
    if(Action==TEXT("status")) { R.bSaveRequested=false;Publish(R);R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("controller_status");return R; }
    if(Action==TEXT("disable")||Action==TEXT("stop"))
    { R.bSaveRequested=false;R.bMutationAttempted=true;R.bChanged=State.bEnabled||State.Ticker.IsValid()||State.Source.IsValid()||State.Target.IsValid();Stop(TEXT("explicit_stop"),&R);Publish(R);R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("controller_stopped");return R; }
    if(Action!=TEXT("enable")&&Action!=TEXT("start")) { Fail(R,TEXT("material_sync_action_invalid"),TEXT("Unknown MPC sync action: ")+RequestedAction);Publish(R);return R; }
    TSet<FName> Names;if(SourcePath.IsEmpty()||TargetPath.IsEmpty()) { Fail(R,TEXT("material_sync_paths_missing"),TEXT("Missing original source_collection_path or target_collection_path for enable/start."));Publish(R);return R; }
    if(!FMath::IsFinite(Interval)||FMath::Abs(Interval)>FLT_MAX||!Filter(NamesJson,Names,R)) { if(R.Error.IsEmpty()) { Fail(R,TEXT("material_sync_interval_invalid"),TEXT("Finite float32 interval required; original clamp min0.01 preserved.")); }Publish(R);return R; }
    R.Stage=TEXT("replace_previous_job");R.bMutationAttempted=true;R.bChangeStateKnown=false;Stop(TEXT("replaced_by_enable"),&R);
    State.SourcePath=SourcePath;State.TargetPath=TargetPath;State.Filter=MoveTemp(Names);State.Interval=FMath::Max(0.01f,static_cast<float>(Interval));State.TickCount=State.CallbackCount=0;State.LastScalarCount=State.LastVectorCount=0;State.LastError.Reset();State.LastCopy.Reset();State.LastTick.Reset();
    R.Stage=TEXT("resolve_collections");State.Source.Reset(Resolve(SourcePath,false,R));if(!State.Source.IsValid()) { State.LastError=R.Error;Stop(TEXT("enable_failed"),&R);Publish(R);return R; }
    State.Target.Reset(Resolve(TargetPath,true,R));if(!State.Target.IsValid()) { State.LastError=R.Error;Stop(TEXT("enable_failed"),&R);Publish(R);return R; }
    const TStrongObjectPtr<UMaterialParameterCollection> KeepSource(State.Source.Get()),KeepTarget(State.Target.Get());
    if(State.Source.Get()==State.Target.Get()||(R.bSaveRequested&&!State.Target->GetOutermost()->IsFullyLoaded())) { Fail(R,TEXT("material_sync_target_alias_or_save_scope"),TEXT("Distinct collection assets and fully-loaded target package for save selected; original self-sync remains residual."));State.LastError=R.Error;Stop(TEXT("enable_failed"),&R);Publish(R);return R; }
    if(!Layout(State.Source.Get(),State.SourceLayout,R)||!Layout(State.Target.Get(),State.TargetLayout,R)) { State.LastError=R.Error;Stop(TEXT("enable_failed"),&R);Publish(R);return R; }
    for(const auto& P:State.Source->ScalarParameters) { if((State.Filter.IsEmpty()||State.Filter.Contains(P.ParameterName))&&State.Target->ScalarParameters.ContainsByPredicate([&P](const auto& T){return T.ParameterName==P.ParameterName;})) { State.Scalars.Add(P.ParameterName); } }
    for(const auto& P:State.Source->VectorParameters) { if((State.Filter.IsEmpty()||State.Filter.Contains(P.ParameterName))&&State.Target->VectorParameters.ContainsByPredicate([&P](const auto& T){return T.ParameterName==P.ParameterName;})) { State.Vectors.Add(P.ParameterName); } }
    if(State.Scalars.IsEmpty()&&State.Vectors.IsEmpty()) { Fail(R,TEXT("material_sync_no_matching_parameters"),TEXT("No same-name MPC parameters found to sync."));State.LastError=R.Error;Stop(TEXT("enable_failed"),&R);Publish(R);return R; }
    State.World=GEditor?GEditor->GetEditorWorldContext().World():nullptr;
    R.Stage=TEXT("initial_instance_lookup");if(IsPlaySessionActive()||!Instances(R,true)) { State.LastError=R.Error.IsEmpty()?TEXT("Idle Editor world required; original PIE variant residual."):R.Error;if(R.Error.IsEmpty()) { Fail(R,TEXT("material_sync_world_unsupported"),State.LastError); }Stop(TEXT("enable_failed"),&R);Publish(R);return R; }
    Count(R,TEXT("world_cleanup_add_calls"));State.WorldCleanup=FWorldDelegates::OnWorldCleanup.AddStatic(&WorldCleanup);State.bEnabled=true;State.StopReason.Reset();
    R.Stage=TEXT("initial_runtime_copy");if(!Copy(Defaults,R)) { State.LastError=R.Error;Stop(TEXT("initial_copy_failed"),&R);Publish(R);return R; }
    R.Data->SetBoolField(TEXT("initial_sync"),true);R.Data->SetNumberField(TEXT("initial_scalar_count"),State.LastScalarCount);R.Data->SetNumberField(TEXT("initial_vector_count"),State.LastVectorCount);
    // PostEdit may recreate instances; permit this single observed initial reacquisition only.
    R.Stage=TEXT("after_default_callbacks");if(!ObserveCopiedValues(R,TEXT("after_default_values"),true)) { State.LastError=R.Error;Stop(TEXT("initial_callbacks_failed"),&R);Publish(R);return R; }
    if(R.bSaveRequested)
    { auto Call=MakeShared<FJsonObject>();Call->SetStringField(TEXT("asset_path"),State.Target->GetPathName());Call->SetBoolField(TEXT("only_if_dirty"),false);Call->SetBoolField(TEXT("attempted"),true);Call->SetBoolField(TEXT("returned"),false);R.Data->SetArrayField(TEXT("save_calls"),{MakeShared<FJsonValueObject>(Call)});
      R.Stage=TEXT("save_on_enable");R.bSaveAttempted=true;Count(R,TEXT("save_attempt_count"));R.bSaved=UEditorAssetLibrary::SaveLoadedAsset(State.Target.Get(),false);Count(R,TEXT("save_return_count"));Call->SetBoolField(TEXT("returned"),true);Call->SetBoolField(TEXT("saved"),R.bSaved);R.Data->SetBoolField(TEXT("saved_on_enable"),R.bSaved); }
    R.Stage=TEXT("after_enable_values");if(!ObserveCopiedValues(R,TEXT("after_enable_values"))) { State.LastError=R.Error;Stop(TEXT("after_enable_values_failed"),&R);Publish(R);return R; }
    R.Stage=TEXT("register_ticker");Count(R,TEXT("ticker_add_calls"));State.Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick),State.Interval);
    if(!State.Ticker.IsValid()||!State.WorldCleanup.IsValid()) { Fail(R,TEXT("material_sync_handle_invalid"),TEXT("Owned ticker/cleanup registration handle unavailable; actual copy/save prefix retained."));State.LastError=R.Error;Stop(TEXT("registration_failed"),&R);Publish(R);return R; }
    R.bChanged=true;R.bChangeStateKnown=true;R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("controller_enabled");Publish(R);return R;
}
}
