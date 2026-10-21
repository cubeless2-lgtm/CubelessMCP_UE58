// Original LandscapeCommands public4, through installed public UE5.8 Engine APIs.
// Editor-only leaf. Python normalization, journals and Toolset Registry remain separate.
#include "Compatibility/UE58/CubelessLandscapeApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/Level.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Crc.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/WeakObjectPtr.h"

namespace Cubeless::UE58::LandscapeAuthoring
{
namespace
{
using Variable::FResult;
constexpr int32 MaxJsonChars=1024*1024,MaxProxies=65536,MaxComponents=65536,MaxLayers=65536,MaxVertices=1025*1025;
bool Running=false;
bool Live(const UObject* O)
{
    return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);
}
bool SafeText(const FString& Text,int32 Maximum)
{
    if(Text.Len()>Maximum) { return false; }for(TCHAR C:Text) { if(C==0) { return false; } }return true;
}
struct FCaseSensitiveJsonKeyFuncs : DefaultKeyFuncs<FString>
{
    static bool Matches(const FString& A,const FString& B) { return A.Equals(B,ESearchCase::CaseSensitive); }
    static uint32 GetKeyHash(const FString& K) { return FCrc::StrCrc32(*K); }
};
bool Parse(const FString& Text,TSharedPtr<FJsonValue>& Value)
{
    if(Text.IsEmpty()||!SafeText(Text,MaxJsonChars)) { return false; }
    auto Reader=TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]"));TArray<TSet<FString,FCaseSensitiveJsonKeyFuncs>> Keys;TArray<bool> Objects;EJsonNotation Token;int32 Slots=0;
    while(Reader->ReadNext(Token))
    {
        if(Token==EJsonNotation::Error||++Slots>262144) { return false; }
        if(Token!=EJsonNotation::ObjectEnd&&Token!=EJsonNotation::ArrayEnd&&!Objects.IsEmpty()&&Objects.Last())
        { const FString Key(Reader->GetIdentifier());if(Keys.Last().Contains(Key)) { return false; }Keys.Last().Add(Key); }
        if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart)
        { Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();if(Objects.Num()>16) { return false; } }
        else if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd)
        { if(Objects.IsEmpty()) { return false; }Objects.Pop();Keys.Pop(); }
    }
    TArray<TSharedPtr<FJsonValue>> Values;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]")),Values)||Values.Num()!=1||!Values[0].IsValid()) { return false; }Value=Values[0];return true;
}
FString Json(const TSharedRef<FJsonObject>& J)
{ FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));return Text; }
TArray<TSharedPtr<FJsonValue>> VJson(const FVector& V)
{
    TArray<TSharedPtr<FJsonValue>> A;for(double N:{V.X,V.Y,V.Z}) { if(FMath::IsFinite(N)) { A.Add(MakeShared<FJsonValueNumber>(N)); }else { A.Add(MakeShared<FJsonValueNull>()); } }return A;
}
bool Finite(const FVector& V) { return FMath::IsFinite(V.X)&&FMath::IsFinite(V.Y)&&FMath::IsFinite(V.Z); }
bool Dependency(const FString& Value)
{
    FString Candidate=FPackageName::ExportTextPathToObjectPath(Value).TrimStartAndEnd();Candidate.TrimQuotesInline();
    for(const TCHAR* Token:{TEXT("/Script/UnrealMCP"),TEXT("/UnrealMCP"),TEXT("UnrealMCP"),TEXT("MCPUnreal"),TEXT("mcp_unreal"),TEXT("/Script/CubelessControlRigTools"),TEXT("CubelessControlRigTools"),TEXT("CubelessControlRigToolsEditor"),TEXT("CubelessMCP_UE58"),TEXT("CubelessMCPUE58")})
    { if(Candidate.Contains(Token,ESearchCase::IgnoreCase)) { return true; } }return false;
}
FString RootPath(FString Value)
{
    Value.TrimStartAndEndInline();Value.TrimQuotesInline();Value.ReplaceInline(TEXT("\\"),TEXT("/"));while(Value.Len()>1&&Value.EndsWith(TEXT("/"))) { Value.LeftChopInline(1); }return Value;
}
bool UnderRoot(const FString& Package,const FString& Root)
{ return Root.IsEmpty()||Package==Root||Package.StartsWith(Root+TEXT("/")); }
TSharedRef<FJsonObject> BaseProjection(ALandscapeProxy* P)
{
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("proxy_valid"),Live(P));if(!Live(P)) { return J; }
    J->SetStringField(TEXT("path"),P->GetPathName());J->SetStringField(TEXT("label"),P->GetActorLabel());J->SetStringField(TEXT("name"),P->GetName());J->SetStringField(TEXT("class_path"),P->GetClass()->GetPathName());
    J->SetStringField(TEXT("world"),Live(P->GetWorld())?P->GetWorld()->GetPathName():FString());J->SetStringField(TEXT("level"),Live(P->GetLevel())?P->GetLevel()->GetPathName():FString());
    J->SetNumberField(TEXT("component_count"),P->LandscapeComponents.Num());J->SetArrayField(TEXT("location"),VJson(P->GetActorLocation()));J->SetArrayField(TEXT("scale"),VJson(P->GetActorScale3D()));
    J->SetStringField(TEXT("stored_landscape_material"),Live(P->LandscapeMaterial.Get())?P->LandscapeMaterial->GetPathName():FString());J->SetBoolField(TEXT("proxy_package_dirty"),P->GetPackage()->IsDirty());return J;
}
struct FWork
{
    FResult Result;FString Command;bool WriterOperation,Creation;bool HasTarget=false,SpawnReturned=false;
    TSharedRef<FJsonObject> Legacy=MakeShared<FJsonObject>(),Calls=MakeShared<FJsonObject>(),Checks=MakeShared<FJsonObject>(),Prefix=MakeShared<FJsonObject>();
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ULevel> SpawnLevel;TWeakObjectPtr<ALandscapeProxy> Proxy;
    TStrongObjectPtr<UMaterialInterface> Material{nullptr};TUniquePtr<FScopedTransaction> Transaction;TSharedPtr<FJsonObject> Before;
    bool WorldDirtyBefore=false,ExpectCreate=false,ExpectMaterial=false;FVector ExpectedLocation=FVector::ZeroVector,ExpectedScale=FVector(100.0);FString ExpectedLabel;FGuid ExpectedGuid;int32 ExpectedX=0,ExpectedY=0,ExpectedSections=0,ExpectedQuads=0;
    FWork(const TCHAR* Name,bool Writer,bool IsCreate=false):Command(Name),WriterOperation(Writer),Creation(IsCreate)
    {
        Result.Compiled=false;Legacy->SetBoolField(TEXT("success"),false);Result.Data->SetObjectField(TEXT("legacy_result"),Legacy);Result.Data->SetObjectField(TEXT("native_call_counts"),Calls);Result.Data->SetObjectField(TEXT("postconditions"),Checks);Result.Data->SetObjectField(TEXT("creation_prefix"),Prefix);
        Result.Data->SetBoolField(TEXT("atomic_rollback_available"),false);Result.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);Result.Data->SetBoolField(TEXT("disk_reload_verified"),false);
        Result.Data->SetBoolField(TEXT("height_values_read_back"),false);Result.Data->SetBoolField(TEXT("render_completion_verified"),false);Result.Data->SetBoolField(TEXT("implicit_Engine_writes_measured"),false);
        Result.Data->SetBoolField(TEXT("wrapper_explicit_wait_performed"),false);Result.Data->SetBoolField(TEXT("SDK_internal_texture_finish_may_wait"),Creation);Result.Data->SetBoolField(TEXT("SDK_may_show_progress_dialog"),Creation);
        Result.Data->SetStringField(TEXT("scope"),TEXT("Loaded ALandscapeProxy actors in the captured initialized Editor world. Create uses the original World-package root guard and captured current spawn level. No forced WP cell traversal or Foliage LevelPartition gate."));
        Result.Data->SetStringField(TEXT("total_change_scope"),Writer?TEXT("Actual target actor/component public projection and dirty observations only; loads, actor construction, Import textures/layers, transaction/material/render/delegate side effects are not a complete global inventory."):TEXT("Direct wrapper read calls and captured public proxy/world dirty projection only; global default-material cache/SDK purity is not established."));
        Result.Data->SetStringField(TEXT("synchronous_SDK_limit"),TEXT("Public Import is synchronous, creates a slow-task dialog and internally calls FinishCachePlatformData. PostEditChangeProperty can rebuild materials/Nanite. No worker UObject writes or extra wrapper waits/flush/compile/save are added."));
        Result.Data->SetStringField(TEXT("retry_risk"),TEXT("Create may leave an actor/textures/components before failure and replay may duplicate it. Material/delegate effects may remain. Inspect actual receipts; no automatic retry or atomic rollback."));
        auto Limits=MakeShared<FJsonObject>();Limits->SetNumberField(TEXT("JSON_UTF16_characters"),MaxJsonChars);Limits->SetNumberField(TEXT("JSON_depth"),16);Limits->SetNumberField(TEXT("proxy_iterator"),MaxProxies);Limits->SetNumberField(TEXT("component_projection"),MaxComponents);Limits->SetNumberField(TEXT("target_layers"),MaxLayers);Limits->SetNumberField(TEXT("create_vertices"),MaxVertices);Result.Data->SetObjectField(TEXT("limits"),Limits);
        for(const TCHAR* K:{TEXT("load_material_calls"),TEXT("load_material_returns"),TEXT("begin_transaction_calls"),TEXT("begin_transaction_returns"),TEXT("end_transaction_calls"),TEXT("end_transaction_returns"),TEXT("spawn_actor_calls"),TEXT("spawn_actor_returns"),TEXT("set_label_calls"),TEXT("set_label_returns"),TEXT("set_scale_calls"),TEXT("set_scale_returns"),TEXT("proxy_modify_calls"),TEXT("proxy_modify_returns"),TEXT("material_field_writes"),TEXT("post_edit_property_calls"),TEXT("post_edit_property_returns"),TEXT("import_calls"),TEXT("import_returns"),TEXT("update_layer_info_calls"),TEXT("update_layer_info_returns"),TEXT("register_components_calls"),TEXT("register_components_returns"),TEXT("proxy_mark_dirty_calls"),TEXT("proxy_mark_dirty_returns"),TEXT("world_mark_dirty_calls"),TEXT("world_mark_dirty_returns"),TEXT("get_landscape_material_calls"),TEXT("get_landscape_material_returns"),TEXT("height_readback_calls"),TEXT("compile_blueprint_calls"),TEXT("asset_save_calls"),TEXT("map_save_calls"),TEXT("wrapper_wait_calls"),TEXT("start_pie_calls"),TEXT("stop_pie_calls")}) { Calls->SetNumberField(K,0); }
    }
    void Count(const TCHAR* K) { double N=0;Calls->TryGetNumberField(K,N);Calls->SetNumberField(K,N+1); }
    bool Fail(const TCHAR* Code,const FString& Error)
    {
        if(Result.Error.IsEmpty()) { Result.ErrorCode=FString(TEXT("landscape_"))+Code;Result.Error=Error; }
        Result.bSuccess=false;Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("error"),Result.Error);return false;
    }
    bool Admit(bool NeedProxy=false)
    {
        if(!IsEditorGameThread()||!GEditor||(WriterOperation&&IsPlaySessionActive())||!Live(World.Get())||!World->IsInitialized()||World->WorldType!=EWorldType::Editor||GEditor->GetEditorWorldContext().World()!=World.Get())
        { return Fail(TEXT("editor_changed"),TEXT("Captured initialized Editor world is unavailable or changed; no further writer was dispatched")); }
        if(Creation&&(!Live(SpawnLevel.Get())||SpawnLevel->GetWorld()!=World.Get()||World->GetCurrentLevel()!=SpawnLevel.Get()))
        { return Fail(TEXT("spawn_level_changed"),TEXT("Captured current spawn level changed during an SDK callback")); }
        if(Material.Get()&&!Live(Material.Get())) { return Fail(TEXT("material_changed"),TEXT("Captured material is no longer live")); }
        if(NeedProxy&&(!Live(Proxy.Get())||Proxy->GetWorld()!=World.Get()||!Live(Proxy->GetLevel())||Proxy->GetLevel()->GetWorld()!=World.Get()||(Creation&&Proxy->GetLevel()!=SpawnLevel.Get())))
        { return Fail(TEXT("proxy_changed"),TEXT("Captured landscape proxy was destroyed or left its admitted world/level")); }return true;
    }
    bool Initialize()
    {
        if(!IsEditorGameThread()||!GEditor||(WriterOperation&&IsPlaySessionActive())) { return Fail(TEXT("editor_required"),WriterOperation?TEXT("Requires an idle Editor game thread"):TEXT("Requires the Editor game thread")); }
        World=GEditor->GetEditorWorldContext().World();if(Live(World.Get())) { SpawnLevel=World->GetCurrentLevel();WorldDirtyBefore=World->GetPackage()->IsDirty(); }return Admit();
    }
    bool Writer(const TCHAR* Stage,const TCHAR* Counter,bool NeedProxy=false)
    { Result.Stage=Stage;if(!Admit(NeedProxy)) { return false; }Result.bMutationAttempted=true;Result.bChangeStateKnown=false;Count(Counter);return true; }
    bool Begin()
    {
        if(!Writer(TEXT("begin_transaction"),TEXT("begin_transaction_calls"),HasTarget)) { return false; }
        Transaction=MakeUnique<FScopedTransaction>(FText::FromString(TEXT("Cubeless ")+Command));Count(TEXT("begin_transaction_returns"));return Admit(HasTarget);
    }
    void Close()
    { if(Transaction) { Result.bChangeStateKnown=false;Count(TEXT("end_transaction_calls"));Transaction.Reset();Count(TEXT("end_transaction_returns")); } }
    bool ReadMaterial(UMaterialInterface*& Out)
    {
        if(!Admit(true)) { return false; }Result.bChangeStateKnown=false;Result.Data->SetBoolField(TEXT("effective_material_getter_global_purity_unverified"),true);Count(TEXT("get_landscape_material_calls"));Out=Proxy->GetLandscapeMaterial();Count(TEXT("get_landscape_material_returns"));if(!Admit(true)) { return false; }if(Out&&!Live(Out)) { return Fail(TEXT("material_unresolved"),TEXT("Effective landscape material is not a live object")); }return true;
    }
    bool LegacyProjection(TSharedRef<FJsonObject> J)
    {
        UMaterialInterface* M=nullptr;if(!ReadMaterial(M)) { return false; }
        ALandscapeProxy* P=Proxy.Get();const FGuid Guid=P->GetLandscapeGuid();if(!Admit(true)) { return false; }
        J->SetStringField(TEXT("label"),P->GetActorLabel());J->SetStringField(TEXT("name"),P->GetName());J->SetStringField(TEXT("class"),P->GetClass()->GetName());J->SetNumberField(TEXT("component_count"),P->LandscapeComponents.Num());
        J->SetStringField(TEXT("landscape_material"),Live(M)?M->GetPathName():FString());J->SetStringField(TEXT("location"),P->GetActorLocation().ToString());J->SetStringField(TEXT("landscape_guid"),Guid.ToString(EGuidFormats::DigitsWithHyphens));return true;
    }
    TSharedRef<FJsonObject> Observation()
    {
        auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("world_valid"),Live(World.Get()));
        if(Live(World.Get())) { J->SetStringField(TEXT("world"),World->GetPathName());J->SetBoolField(TEXT("world_package_dirty"),World->GetPackage()->IsDirty()); }
        J->SetBoolField(TEXT("proxy_valid"),Live(Proxy.Get()));Prefix->SetBoolField(TEXT("spawn_actor_returned"),SpawnReturned);Prefix->SetBoolField(TEXT("created_actor_still_valid"),SpawnReturned&&Live(Proxy.Get()));
        if(!Live(Proxy.Get())) { return J; }auto Base=BaseProjection(Proxy.Get());J->SetObjectField(TEXT("proxy"),Base);
        ALandscapeProxy* P=Proxy.Get();TSet<FString> Heightmaps;int32 ValidComponents=0,RegisteredComponents=0;bool Complete=P->LandscapeComponents.Num()<=MaxComponents;
        for(int32 N=0;N<FMath::Min(P->LandscapeComponents.Num(),MaxComponents);++N)
        {
            ULandscapeComponent* C=P->LandscapeComponents[N];if(!Live(C)) { Complete=false;continue; }++ValidComponents;if(C->IsRegistered()) { ++RegisteredComponents; }
            UTexture2D* T=C->GetHeightmap(false);if(Live(T)) { Heightmaps.Add(T->GetPathName()); }
        }
        TArray<TSharedPtr<FJsonValue>> Textures;for(const FString& S:Heightmaps) { Textures.Add(MakeShared<FJsonValueString>(S)); }
        J->SetNumberField(TEXT("valid_components"),ValidComponents);J->SetNumberField(TEXT("registered_components"),RegisteredComponents);J->SetBoolField(TEXT("component_projection_complete"),Complete);J->SetArrayField(TEXT("public_base_heightmap_paths"),Textures);
        J->SetBoolField(TEXT("heightmap_paths_are_complete_texture_side_effect_inventory"),false);J->SetBoolField(TEXT("height_values_read_back"),false);
        Prefix->SetNumberField(TEXT("actual_component_entries"),P->LandscapeComponents.Num());Prefix->SetNumberField(TEXT("observed_valid_components"),ValidComponents);Prefix->SetNumberField(TEXT("observed_registered_components"),RegisteredComponents);Prefix->SetArrayField(TEXT("observed_public_base_heightmap_paths"),Textures);
        return J;
    }
    bool CreatePostconditions()
    {
        ALandscape* A=Cast<ALandscape>(Proxy.Get());if(!Live(A)||!Admit(true)) { return Fail(TEXT("create_target_lost"),TEXT("Created ALandscape is no longer a live admitted target")); }
        ULandscapeInfo* I=A->GetLandscapeInfo();if(!Admit(true)) { return false; }const bool Layers=!A->GetEditLayersConst().IsEmpty();
        UMaterialInterface* M=nullptr;if(!ReadMaterial(M)) { return false; }A=Cast<ALandscape>(Proxy.Get());if(!Live(A)||!Admit(true)) { return false; }
        // Read final scalar/component projections after potential material/default-cache callbacks.
        const bool Transform=A->GetActorLocation().Equals(ExpectedLocation,1e-6)&&A->GetActorScale3D().Equals(ExpectedScale,1e-6);const bool Label=A->GetActorLabel().Equals(ExpectedLabel,ESearchCase::CaseSensitive);
        const FGuid Guid=A->GetLandscapeGuid();if(!Admit(true)) { return false; }const bool Count=A->LandscapeComponents.Num()==ExpectedX*ExpectedY;bool Topology=Count;TSet<FIntPoint> Bases;
        for(ULandscapeComponent* C:A->LandscapeComponents)
        {
            if(!Live(C)||C->GetOwner()!=A||C->ComponentSizeQuads!=ExpectedSections*ExpectedQuads||C->NumSubsections!=ExpectedSections||C->SubsectionSizeQuads!=ExpectedQuads) { Topology=false;continue; }
            const FIntPoint B=C->GetSectionBase();if(Bases.Contains(B)||B.X<0||B.Y<0||B.X%(ExpectedSections*ExpectedQuads)!=0||B.Y%(ExpectedSections*ExpectedQuads)!=0||B.X/(ExpectedSections*ExpectedQuads)>=ExpectedX||B.Y/(ExpectedSections*ExpectedQuads)>=ExpectedY) { Topology=false; }Bases.Add(B);
        }
        const bool Info=Live(I);const bool Mat=!Material.Get()||(A->LandscapeMaterial.Get()==Material.Get()&&M==Material.Get());const bool GuidMatch=Guid.IsValid()&&Guid==ExpectedGuid;
        Checks->SetBoolField(TEXT("component_count_matches_grid"),Count);Checks->SetBoolField(TEXT("component_section_topology_matches"),Topology);Checks->SetBoolField(TEXT("actor_transform_matches"),Transform);Checks->SetBoolField(TEXT("actor_label_matches"),Label);Checks->SetBoolField(TEXT("landscape_guid_matches_import"),GuidMatch);Checks->SetBoolField(TEXT("landscape_info_present"),Info);Checks->SetBoolField(TEXT("UE58_edit_layer_present"),Layers);Checks->SetBoolField(TEXT("optional_material_matches"),Mat);
        Checks->SetStringField(TEXT("height_validation_scope"),TEXT("Constant uint16 Import input prepared; actual height values/render/collision completion not read back here. Public height getter itself blocks/modifies textures and is intentionally not added."));
        if(!Transform||!Label||!GuidMatch||!Count||!Topology||!Info||!Layers||!Mat) { return Fail(TEXT("create_postcondition_failed"),TEXT("Public created actor/component/transform/material/edit-layer postconditions differ; actual actor/textures/components may remain")); }return true;
    }
    FResult Finish()
    {
        Close();bool Okay=World.IsValid()?Admit(HasTarget):true;
        if(Okay&&Result.bSuccess&&ExpectCreate) { Okay=CreatePostconditions(); }
        if(Okay&&Result.bSuccess&&ExpectMaterial)
        {
            UMaterialInterface* M=nullptr;Okay=ReadMaterial(M);
            if(Okay) { const bool Match=Proxy->LandscapeMaterial.Get()==Material.Get()&&M==Material.Get();Checks->SetBoolField(TEXT("stored_and_effective_material_match"),Match);if(!Match) { Okay=Fail(TEXT("material_postcondition_failed"),TEXT("Material assignment/post-edit callbacks returned a different stored/effective material; actual changes may remain")); } }
        }
        if(World.IsValid())
        {
            const auto After=Observation();Result.Data->SetObjectField(TEXT("target_after"),After);
            const bool WorldDirty=Live(World.Get())&&World->GetPackage()->IsDirty()!=WorldDirtyBefore;bool Changed=WorldDirty||Result.bChanged;
            if(Before.IsValid()&&Live(Proxy.Get())) { Changed=Changed||Json(Before.ToSharedRef())!=Json(BaseProjection(Proxy.Get())); }
            if(SpawnReturned) { Changed=true; }Result.bChanged=Changed;Result.Data->SetBoolField(TEXT("observed_target_projection_changed"),Changed);
            if(!WriterOperation&&Changed) { Result.bChangeStateKnown=false;Okay=Fail(TEXT("observer_projection_changed"),TEXT("Direct observer changed the captured dirty/proxy projection; global state is uncertain")); }
        }
        if(Result.bSuccess&&Okay) { Result.bPostconditionPassed=true;Result.Stage=TEXT("complete");Legacy->SetBoolField(TEXT("success"),true); }
        else { Result.bSuccess=false; }
        Result.Data->SetBoolField(TEXT("total_change_state_unknown"),!Result.bChangeStateKnown);Result.Data->SetBoolField(TEXT("explicit_compile_attempted"),false);Result.Data->SetBoolField(TEXT("explicit_save_attempted"),false);return Result;
    }
};
bool Context(FWork& W,const FString& Text,TConstArrayView<const TCHAR*> Fields,TSharedPtr<FJsonObject>& Raw)
{
    if(Text.IsEmpty()) { return true; }TSharedPtr<FJsonValue> V;
    if(!Parse(Text,V)||V->Type!=EJson::Object||!V->AsObject().IsValid()) { return W.Fail(TEXT("context_invalid"),TEXT("Private raw context must be one bounded JSON object")); }Raw=V->AsObject();
    for(const auto& Pair:Raw->Values)
    {
        const FString K(Pair.Key.ToView());bool Allowed=false;for(const TCHAR* Field:Fields) { if(K.Equals(Field,ESearchCase::CaseSensitive)) { Allowed=true;break; } }
        if(!Allowed) { return W.Fail(TEXT("context_invalid"),TEXT("Private raw context contains a field not read by the original command")); }
    }
    W.Result.Data->SetObjectField(TEXT("legacy_raw_context"),Raw.ToSharedRef());return true;
}
bool Text(FWork& W,const TSharedPtr<FJsonObject>& Raw,const TCHAR* Key,const FString& Named,const FString& Default,FString& Out)
{
    Out=Raw.IsValid()?Default:Named;if(Raw.IsValid()) { Raw->TryGetStringField(Key,Out); }
    if(!SafeText(Out,65536)) { return W.Fail(TEXT("text_invalid"),FString::Printf(TEXT("%s exceeds the text bound or contains NUL"),Key)); }return true;
}
bool Number(FWork& W,const TSharedPtr<FJsonObject>& Raw,const TCHAR* Key,int64 Named,int32 Default,int32& Out)
{
    if(Raw.IsValid())
    {
        double V=Default;Raw->TryGetNumberField(Key,V);
        if(!FMath::IsFinite(V)||V<=static_cast<double>(MIN_int32)-1.0||V>=static_cast<double>(MAX_int32)+1.0) { return W.Fail(TEXT("integer_conversion_unsafe"),FString::Printf(TEXT("%s cannot be safely converted to legacy int32"),Key)); }Out=static_cast<int32>(V);
    }
    else
    {
        if(Named<MIN_int32||Named>MAX_int32) { return W.Fail(TEXT("integer_conversion_unsafe"),FString::Printf(TEXT("Named %s cannot be safely converted to legacy int32"),Key)); }Out=static_cast<int32>(Named);
    }return true;
}
bool Vector(FWork& W,const TSharedPtr<FJsonObject>& Raw,const TCHAR* Key,const FString& NamedJson,const FVector& Default,FVector& Out)
{
    const TArray<TSharedPtr<FJsonValue>>* A=nullptr;TSharedPtr<FJsonValue> Carrier;Out=Default;
    if(Raw.IsValid()) { Raw->TryGetArrayField(Key,A); }
    else { if(!Parse(NamedJson,Carrier)) { return W.Fail(TEXT("vector_carrier_invalid"),FString::Printf(TEXT("%s requires a bounded valid JSON carrier"),Key)); }if(Carrier->Type==EJson::Array) { A=&Carrier->AsArray(); } }
    if(A&&A->Num()==3)
    {
        double N[3];for(int32 I=0;I<3;++I) { if(!(*A)[I].IsValid()) { return W.Fail(TEXT("vector_invalid"),TEXT("Invalid JSON vector slot")); }N[I]=(*A)[I]->AsNumber(); }
        Out=FVector(N[0],N[1],N[2]);
    }
    if(!Finite(Out)) { return W.Fail(TEXT("vector_nonfinite"),FString::Printf(TEXT("%s must remain finite after original SDK AsNumber conversion"),Key)); }return true;
}
bool LoadMaterial(FWork& W,const FString& Path)
{
    if(Dependency(Path)) { return W.Fail(TEXT("dependency_reference"),TEXT("Refusing to persist MCP-only reference in 'material_path': ")+Path+TEXT(". UnrealMCP is an editor authoring bridge; finished assets must not depend on it.")); }
    if(!W.Writer(TEXT("load_material"),TEXT("load_material_calls"))) { return false; }W.Material.Reset(Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(Path)));W.Count(TEXT("load_material_returns"));
    if(!W.Admit()) { return false; }if(!Live(W.Material.Get())) { return W.Fail(TEXT("material_not_found"),TEXT("Material not found: ")+Path); }return true;
}
bool Find(FWork& W,const FString& Label)
{
    W.Result.Stage=TEXT("find_loaded_proxy");if(!W.Admit()) { return false; }ALandscapeProxy* First=nullptr;int32 Seen=0;
    for(TActorIterator<ALandscapeProxy> It(W.World.Get());It;++It)
    {
        if(++Seen>MaxProxies) { return W.Fail(TEXT("proxy_limit"),TEXT("Loaded landscape proxy scan exceeds its bound")); }ALandscapeProxy* P=*It;if(!Live(P)) { continue; }if(!First) { First=P; }
        // Original FString == selector is IgnoreCase, and the first match wins.
        if(!Label.IsEmpty()&&(P->GetActorLabel().Equals(Label,ESearchCase::IgnoreCase)||P->GetName().Equals(Label,ESearchCase::IgnoreCase))) { W.Proxy=P;W.HasTarget=true;return W.Admit(true); }
    }
    if(Label.IsEmpty()&&First) { W.Proxy=First;W.HasTarget=true;return W.Admit(true); }
    return W.Fail(TEXT("not_found"),Label.IsEmpty()?TEXT("No landscape actors in the current level"):TEXT("Landscape not found: ")+Label);
}
bool Enter(FWork& W)
{
    if(!W.Initialize()) { return false; }if(Running) { return W.Fail(TEXT("reentrant"),TEXT("Another landscape operation is already active on this game thread")); }Running=true;return true;
}
}

Variable::FResult List()
{
    FWork W(TEXT("list_landscapes"),false);if(!Enter(W)) { return W.Finish(); }ON_SCOPE_EXIT { Running=false; };
    W.Result.Stage=TEXT("list_loaded_proxies");TArray<TSharedPtr<FJsonValue>> Rows;int32 Seen=0;
    for(TActorIterator<ALandscapeProxy> It(W.World.Get());It;++It)
    {
        if(++Seen>MaxProxies) { W.Fail(TEXT("proxy_limit"),TEXT("Loaded landscape proxy listing exceeds its bound; partial success not returned"));return W.Finish(); }
        if(!Live(*It)) { continue; }W.Proxy=*It;W.HasTarget=true;const auto Before=BaseProjection(W.Proxy.Get());auto Row=MakeShared<FJsonObject>();
        if(!W.LegacyProjection(Row)) { return W.Finish(); }if(Json(Before)!=Json(BaseProjection(W.Proxy.Get()))) { W.Result.bChangeStateKnown=false;W.Result.bChanged=true;W.Result.Data->SetBoolField(TEXT("observed_proxy_projection_changed"),true);W.Fail(TEXT("observer_projection_changed"),TEXT("Proxy projection changed during GetLandscapeMaterial/GUID read"));return W.Finish(); }Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    W.Proxy.Reset();W.HasTarget=false;W.Legacy->SetNumberField(TEXT("landscape_count"),Rows.Num());W.Legacy->SetArrayField(TEXT("landscapes"),Rows);W.Checks->SetBoolField(TEXT("loaded_world_iterator_projection_observed"),true);W.Result.Data->SetBoolField(TEXT("read_only_direct_API"),true);W.Result.bSuccess=true;return W.Finish();
}

Variable::FResult Inspect(const FString& ActorLabel,const FString& PrivateContextJson)
{
    FWork W(TEXT("get_landscape_info"),false);TSharedPtr<FJsonObject> Raw;const TCHAR* Fields[]={TEXT("actor_label")};if(!Context(W,PrivateContextJson,Fields,Raw)) { return W.Finish(); }
    FString Label;if(!Text(W,Raw,TEXT("actor_label"),ActorLabel,TEXT(""),Label)) { return W.Finish(); }
    if(!Enter(W)) { return W.Finish(); }ON_SCOPE_EXIT { Running=false; };if(!Find(W,Label)) { return W.Finish(); }W.Before=BaseProjection(W.Proxy.Get());W.Result.Data->SetObjectField(TEXT("target_before"),W.Before.ToSharedRef());
    if(!W.LegacyProjection(W.Legacy)) { return W.Finish(); }W.Legacy->SetStringField(TEXT("scale"),W.Proxy->GetActorScale3D().ToString());
    const auto& Layers=W.Proxy->GetTargetLayers();if(Layers.Num()>MaxLayers) { W.Fail(TEXT("layer_limit"),TEXT("Landscape target layer count exceeds bounded projection"));return W.Finish(); }TArray<TSharedPtr<FJsonValue>> Objects;
    for(const auto& Pair:Layers) { if(Pair.Value.LayerInfoObj) { if(!Live(Pair.Value.LayerInfoObj.Get())) { W.Fail(TEXT("layer_info_unresolved"),TEXT("Landscape layer info is not a live object"));return W.Finish(); }Objects.Add(MakeShared<FJsonValueString>(Pair.Value.LayerInfoObj->GetPathName())); } }W.Legacy->SetArrayField(TEXT("layer_info_objects"),Objects);
    W.Result.Data->SetBoolField(TEXT("read_only_direct_API"),true);W.Result.bSuccess=true;return W.Finish();
}

Variable::FResult Create(const FString& ActorLabel,const FString& MaterialPath,const FString& LocationJson,const FString& ScaleJson,
    int64 ComponentCountX,int64 ComponentCountY,int64 SectionsPerComponent,int64 QuadsPerSection,int64 HeightValue,
    bool bAllowNonTempLevel,const FString& RequiredRoot,const FString& PrivateContextJson)
{
    FWork W(TEXT("create_landscape"),true,true);TSharedPtr<FJsonObject> Raw;
    const TCHAR* Fields[]={TEXT("actor_label"),TEXT("material_path"),TEXT("location"),TEXT("scale"),TEXT("component_count_x"),TEXT("component_count_y"),TEXT("sections_per_component"),TEXT("quads_per_section"),TEXT("height_value"),TEXT("allow_non_temp_level"),TEXT("required_root")};
    if(!Context(W,PrivateContextJson,Fields,Raw)||!Enter(W)) { return W.Finish(); }ON_SCOPE_EXIT { Running=false; };
    FString Label,MaterialName,Root;if(!Text(W,Raw,TEXT("actor_label"),ActorLabel,TEXT("MCP_Landscape"),Label)) { return W.Finish(); }Label.TrimStartAndEndInline();if(Label.IsEmpty()) { Label=TEXT("MCP_Landscape"); }
    if(Dependency(Label)) { W.Fail(TEXT("dependency_reference"),TEXT("Refusing to persist MCP-only reference in 'actor_label': ")+Label+TEXT(". UnrealMCP is an editor authoring bridge; finished assets must not depend on it."));return W.Finish(); }
    if(!Text(W,Raw,TEXT("required_root"),RequiredRoot,TEXT("/Game/_MCP_Temp"),Root)) { return W.Finish(); }Root=RootPath(Root);bool Allow=Raw.IsValid()?false:bAllowNonTempLevel;if(Raw.IsValid()) { Raw->TryGetBoolField(TEXT("allow_non_temp_level"),Allow); }
    const FString Package=W.World->GetOutermost()->GetName();const bool Under=UnderRoot(Package,Root);W.Result.Stage=TEXT("world_package_root_guard");W.Result.Data->SetStringField(TEXT("guarded_world_package"),Package);W.Result.Data->SetStringField(TEXT("normalized_required_root"),Root);W.Result.Data->SetBoolField(TEXT("effective_allow_non_temp_level"),Allow);
    if(!Allow&&!Under) { W.Fail(TEXT("world_root_guard"),FString::Printf(TEXT("create_landscape is restricted to levels under %s by default; current level is %s"),*Root,*Package));return W.Finish(); }
    if(!Text(W,Raw,TEXT("material_path"),MaterialPath,TEXT(""),MaterialName)||(!MaterialName.IsEmpty()&&!LoadMaterial(W,MaterialName))) { return W.Finish(); }
    W.Result.Stage=TEXT("validate_dimensions");int32 X,Y,Sections,Quads,Height;
    if(!Number(W,Raw,TEXT("component_count_x"),ComponentCountX,1,X)||!Number(W,Raw,TEXT("component_count_y"),ComponentCountY,1,Y)||!Number(W,Raw,TEXT("sections_per_component"),SectionsPerComponent,1,Sections)||!Number(W,Raw,TEXT("quads_per_section"),QuadsPerSection,7,Quads)) { return W.Finish(); }
    X=FMath::Clamp(X,1,16);Y=FMath::Clamp(Y,1,16);Sections=FMath::Clamp(Sections,1,2);
    if(Quads!=7&&Quads!=15&&Quads!=31&&Quads!=63&&Quads!=127&&Quads!=255) { W.Fail(TEXT("quads_invalid"),FString::Printf(TEXT("quads_per_section must be one of 7, 15, 31, 63, 127, or 255; got %d"),Quads));return W.Finish(); }
    const int32 SizeX=X*Sections*Quads+1,SizeY=Y*Sections*Quads+1;
    if(!Number(W,Raw,TEXT("height_value"),HeightValue,32768,Height)) { return W.Finish(); }Height=FMath::Clamp(Height,0,65535);
    if(SizeX<=1||SizeY<=1||static_cast<int64>(SizeX)*SizeY>MaxVertices) { W.Fail(TEXT("dimensions_invalid"),FString::Printf(TEXT("Requested landscape dimensions are invalid or too large: %dx%d vertices"),SizeX,SizeY));return W.Finish(); }
    FVector Location,Scale;if(!Vector(W,Raw,TEXT("location"),LocationJson,FVector::ZeroVector,Location)||!Vector(W,Raw,TEXT("scale"),ScaleJson,FVector(100.0),Scale)) { return W.Finish(); }
    W.ExpectedLabel=Label;W.ExpectedLocation=Location;W.ExpectedScale=Scale;W.ExpectedX=X;W.ExpectedY=Y;W.ExpectedSections=Sections;W.ExpectedQuads=Quads;
    auto Effective=MakeShared<FJsonObject>();Effective->SetNumberField(TEXT("component_count_x"),X);Effective->SetNumberField(TEXT("component_count_y"),Y);Effective->SetNumberField(TEXT("sections_per_component"),Sections);Effective->SetNumberField(TEXT("quads_per_section"),Quads);Effective->SetNumberField(TEXT("height_value"),Height);Effective->SetNumberField(TEXT("size_x"),SizeX);Effective->SetNumberField(TEXT("size_y"),SizeY);Effective->SetArrayField(TEXT("location"),VJson(Location));Effective->SetArrayField(TEXT("scale"),VJson(Scale));W.Result.Data->SetObjectField(TEXT("effective_creation_parameters"),Effective);
    if(!W.Begin()||!W.Writer(TEXT("spawn_landscape"),TEXT("spawn_actor_calls"))) { return W.Finish(); }ALandscape* Spawned=W.World->SpawnActor<ALandscape>(Location,FRotator::ZeroRotator);W.Prefix->SetBoolField(TEXT("spawn_call_returned_nonnull"),Spawned!=nullptr);W.Proxy=Spawned;W.SpawnReturned=Live(W.Proxy.Get());W.HasTarget=W.SpawnReturned;W.Count(TEXT("spawn_actor_returns"));
    if(!W.SpawnReturned) { W.Fail(TEXT("spawn_failed"),TEXT("Failed to spawn Landscape actor"));return W.Finish(); }if(!W.Admit(true)) { return W.Finish(); }W.Before=MakeShared<FJsonObject>();W.Before->SetBoolField(TEXT("proxy_valid"),false);W.Result.Data->SetObjectField(TEXT("target_before"),W.Before.ToSharedRef());
    if(!W.Writer(TEXT("set_actor_label"),TEXT("set_label_calls"),true)) { return W.Finish(); }W.Proxy->SetActorLabel(Label);W.Count(TEXT("set_label_returns"));
    if(!W.Writer(TEXT("set_actor_scale"),TEXT("set_scale_calls"),true)) { return W.Finish(); }W.Proxy->SetActorScale3D(Scale);W.Count(TEXT("set_scale_returns"));
    if(W.Material.Get()) { if(!W.Writer(TEXT("set_material_field"),TEXT("material_field_writes"),true)) { return W.Finish(); }W.Proxy->LandscapeMaterial=W.Material.Get(); }
    TArray<uint16> HeightData;HeightData.Init(static_cast<uint16>(Height),SizeX*SizeY);TMap<FGuid,TArray<uint16>> HeightMaps;HeightMaps.Add(FGuid(),MoveTemp(HeightData));TMap<FGuid,TArray<FLandscapeImportLayerInfo>> MaterialLayers;MaterialLayers.Add(FGuid(),TArray<FLandscapeImportLayerInfo>());
    W.Checks->SetBoolField(TEXT("constant_uint16_import_input_prepared"),HeightMaps.FindChecked(FGuid()).Num()==SizeX*SizeY);W.Prefix->SetNumberField(TEXT("prepared_height_samples"),SizeX*SizeY);W.Prefix->SetNumberField(TEXT("prepared_height_value"),Height);
    W.Result.Stage=TEXT("validate_import_prerequisite");if(!W.Admit(true)) { return W.Finish(); }
    if(!W.Proxy->LandscapeComponents.IsEmpty()||!Live(W.Proxy->GetRootComponent())) { W.Fail(TEXT("import_prerequisite_failed"),TEXT("Public Import requires a live blank actor/root; actual actor remains"));return W.Finish(); }
    W.ExpectedGuid=FGuid::NewGuid();W.Prefix->SetStringField(TEXT("requested_import_guid"),W.ExpectedGuid.ToString(EGuidFormats::DigitsWithHyphens));
    if(!W.ExpectedGuid.IsValid()) { W.Fail(TEXT("guid_invalid"),TEXT("Generated Import guid is invalid; created actor remains"));return W.Finish(); }
    if(!W.Writer(TEXT("import_landscape"),TEXT("import_calls"),true)) { return W.Finish(); }
    W.Proxy->Import(W.ExpectedGuid,0,0,SizeX-1,SizeY-1,Sections,Quads,HeightMaps,TEXT(""),MaterialLayers,ELandscapeImportAlphamapType::Additive,TArrayView<const FLandscapeLayer>());W.Count(TEXT("import_returns"));W.ExpectCreate=true;
    if(!W.Admit(true)) { return W.Finish(); }ULandscapeInfo* I=W.Proxy->GetLandscapeInfo();if(!W.Admit(true)) { return W.Finish(); }
    if(Live(I)) { if(!W.Writer(TEXT("update_layer_info"),TEXT("update_layer_info_calls"),true)) { return W.Finish(); }I->UpdateLayerInfoMap(W.Proxy.Get());W.Count(TEXT("update_layer_info_returns")); }
    if(!W.Writer(TEXT("register_components"),TEXT("register_components_calls"),true)) { return W.Finish(); }W.Proxy->RegisterAllComponents();W.Count(TEXT("register_components_returns"));
    if(!W.Writer(TEXT("mark_proxy_dirty"),TEXT("proxy_mark_dirty_calls"),true)) { return W.Finish(); }W.Result.Data->SetBoolField(TEXT("explicit_proxy_mark_dirty_return"),W.Proxy->MarkPackageDirty());W.Count(TEXT("proxy_mark_dirty_returns"));
    if(!W.Writer(TEXT("mark_world_dirty"),TEXT("world_mark_dirty_calls"),true)) { return W.Finish(); }W.Result.Data->SetBoolField(TEXT("explicit_world_mark_dirty_return"),W.World->MarkPackageDirty());W.Count(TEXT("world_mark_dirty_returns"));
    W.Close();if(!W.Admit(true)||!W.LegacyProjection(W.Legacy)) { return W.Finish(); }
    W.Legacy->SetBoolField(TEXT("created"),true);W.Legacy->SetStringField(TEXT("current_world_package_name"),Package);W.Legacy->SetBoolField(TEXT("current_world_under_required_root"),Under);W.Legacy->SetStringField(TEXT("required_root"),Root);W.Legacy->SetNumberField(TEXT("component_count_x"),X);W.Legacy->SetNumberField(TEXT("component_count_y"),Y);W.Legacy->SetNumberField(TEXT("sections_per_component"),Sections);W.Legacy->SetNumberField(TEXT("quads_per_section"),Quads);W.Legacy->SetNumberField(TEXT("size_x"),SizeX);W.Legacy->SetNumberField(TEXT("size_y"),SizeY);W.Legacy->SetBoolField(TEXT("level_dirty"),true);W.Legacy->SetStringField(TEXT("note"),TEXT("Level save is left to the user per map-safety policy"));W.Result.bSuccess=true;return W.Finish();
}

Variable::FResult SetMaterial(const FString& MaterialPath,const FString& ActorLabel,const FString& PrivateContextJson)
{
    FWork W(TEXT("set_landscape_material"),true);TSharedPtr<FJsonObject> Raw;const TCHAR* Fields[]={TEXT("material_path"),TEXT("actor_label")};if(!Context(W,PrivateContextJson,Fields,Raw)) { return W.Finish(); }
    FString Path,Label;if(!Text(W,Raw,TEXT("material_path"),MaterialPath,TEXT(""),Path)) { return W.Finish(); }if(Path.IsEmpty()) { W.Fail(TEXT("material_path_missing"),TEXT("Missing 'material_path' parameter"));return W.Finish(); }
    if(!Enter(W)) { return W.Finish(); }ON_SCOPE_EXIT { Running=false; };if(!LoadMaterial(W,Path)||!Text(W,Raw,TEXT("actor_label"),ActorLabel,TEXT(""),Label)||!Find(W,Label)) { return W.Finish(); }
    W.Before=BaseProjection(W.Proxy.Get());W.Result.Data->SetObjectField(TEXT("target_before"),W.Before.ToSharedRef());
    if(!W.Begin()||!W.Writer(TEXT("modify_proxy"),TEXT("proxy_modify_calls"),true)) { return W.Finish(); }W.Proxy->Modify();W.Count(TEXT("proxy_modify_returns"));
    if(!W.Writer(TEXT("set_material_field"),TEXT("material_field_writes"),true)) { return W.Finish(); }W.Proxy->LandscapeMaterial=W.Material.Get();
    W.Result.Stage=TEXT("find_material_property");FProperty* Property=FindFProperty<FProperty>(ALandscapeProxy::StaticClass(),TEXT("LandscapeMaterial"));if(!Property) { W.Fail(TEXT("material_property_missing"),TEXT("Public LandscapeMaterial property unavailable after field assignment; actual material may remain"));return W.Finish(); }
    if(!W.Writer(TEXT("post_edit_material"),TEXT("post_edit_property_calls"),true)) { return W.Finish(); }FPropertyChangedEvent Changed(Property);W.Proxy->PostEditChangeProperty(Changed);W.Count(TEXT("post_edit_property_returns"));W.ExpectMaterial=true;
    if(!W.Writer(TEXT("mark_proxy_dirty"),TEXT("proxy_mark_dirty_calls"),true)) { return W.Finish(); }W.Result.Data->SetBoolField(TEXT("explicit_proxy_mark_dirty_return"),W.Proxy->MarkPackageDirty());W.Count(TEXT("proxy_mark_dirty_returns"));
    W.Close();if(!W.Admit(true)) { return W.Finish(); }W.Legacy->SetStringField(TEXT("landscape"),W.Proxy->GetActorLabel());W.Legacy->SetStringField(TEXT("landscape_material"),W.Material->GetPathName());W.Legacy->SetBoolField(TEXT("level_dirty"),true);W.Legacy->SetStringField(TEXT("note"),TEXT("Level save is left to the user per map-safety policy"));W.Result.bSuccess=true;return W.Finish();
}
}
