// GT dedicated Editor-only Niagara promotion. Source candidate, not build/runtime proof.
#include "Compatibility/UE58/CubelessNiagaraPromotionApi.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitterBase.h"
#include "NiagaraEffectType.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraScratchPadContainer.h"
#include "NiagaraGraph.h"
#include "NiagaraNode.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceRenderTarget2D.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraDecalRendererProperties.h"
#include "NiagaraVolumeRendererProperties.h"
#include "Misc/Crc.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Misc/ScopedSlowTask.h"
#include "Serialization/Archive.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
namespace Cubeless::UE58::NiagaraPromotion
{
namespace
{
using Cubeless::UE58::Variable::FResult;
bool Running=false;
constexpr const TCHAR* NiagaraTempGenerationRoot=TEXT("/Game/_MCP_Temp/NiagaraGenerated/");
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
bool Safe(){return IsInGameThread()&&GEditor&&!IsEngineExitRequested()&&!GExitPurge&&!IsGarbageCollecting()&&!GIsSavingPackage;}
FString Json(const TSharedPtr<FJsonObject>& O){FString S;if(O)FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S));return S;}
bool Forbidden(const FString& S){const FString L=S.ToLower();return L.Contains(TEXT("unrealmcp"))||L.Contains(TEXT("cubelessmcp"))||L.Contains(TEXT("modelcontextprotocol"))||L.Contains(TEXT("toolsetregistry"));}
struct FIdentity{TStrongObjectPtr<UObject> Hold{nullptr};TWeakObjectPtr<UObject> Object,Outer;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UPackage> Package,Effective;FString Path,PackageName;bool DirtyBefore=false;explicit FIdentity(UObject* O):Hold(O),Object(O),Outer(O->GetOuter()),Class(O->GetClass()),Package(O->GetOutermost()),Effective(O->GetPackage()),Path(O->GetPathName()),PackageName(O->GetOutermost()->GetName()),DirtyBefore(O->GetOutermost()->IsDirty()){}bool Exact()const{auto* O=Object.Get();return Live(O)&&Live(Class.Get())&&!Class->HasAnyClassFlags(CLASS_NewerVersionExists)&&Live(Package.Get())&&O->GetClass()==Class.Get()&&O->GetOuter()==Outer.Get()&&O->GetOutermost()==Package.Get()&&O->GetPackage()==Effective.Get()&&O->GetPathName().Equals(Path,ESearchCase::CaseSensitive);}};
struct FPin{UEdGraphPin* P=nullptr;FGuid Id,Persistent;FName Name;EEdGraphPinDirection Direction=EGPD_Input;FEdGraphPinType Type;bool NotConnectable=false,DefaultReadOnly=false;TWeakObjectPtr<UObject> DefaultObject;TArray<UEdGraphPin*> Links;FString Default,AutoDefault,DefaultText;};
struct FNode{TWeakObjectPtr<UEdGraphNode> Node;FGuid Id;TArray<FPin> Pins;};
struct FGraph{TWeakObjectPtr<UNiagaraScriptSource> Source;TWeakObjectPtr<UNiagaraGraph> Graph;TArray<FNode> Nodes;};
struct FScript{TWeakObjectPtr<UNiagaraScript> Script;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;FGuid Exposed;};
FString EmitterPropertyState(const FVersionedNiagaraEmitterData* D){auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("local_space"),D->bLocalSpace);O->SetNumberField(TEXT("sim_target"),static_cast<int32>(D->SimTarget));O->SetBoolField(TEXT("persistent_ids"),D->bRequiresPersistentIDs!=0);O->SetNumberField(TEXT("bounds_mode"),static_cast<int32>(D->CalculateBoundsMode));O->SetBoolField(TEXT("determinism"),D->bDeterminism);O->SetNumberField(TEXT("random_seed"),D->RandomSeed);O->SetNumberField(TEXT("scalability_quality_mask"),D->Platforms.QualityLevelMask);return Json(O);}
struct FEmitter{TWeakObjectPtr<UNiagaraEmitter> Emitter;FGuid Version;FVersionedNiagaraEmitterData* Data=nullptr;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;TArray<TWeakObjectPtr<UNiagaraRendererProperties>> Renderers;TArray<TWeakObjectPtr<UNiagaraScript>> Scripts;TWeakObjectPtr<UNiagaraEmitter> Parent,MergeParent;bool Latest=false;FString UniqueName,PropertyState;};
struct FHandle{FGuid Id,Version;TWeakObjectPtr<UNiagaraEmitter> Emitter;TWeakObjectPtr<UNiagaraEmitterBase> Base;FVersionedNiagaraEmitterData* Data=nullptr;FName Name;bool Enabled=false;ENiagaraEmitterMode Mode=ENiagaraEmitterMode::Standard;};
struct FSystem{TWeakObjectPtr<UNiagaraSystem> System;TWeakObjectPtr<UNiagaraScript> Spawn,Update;TArray<FHandle> Handles;};
struct FWork
{
 FResult R;bool Persistent=false,WorldCaptured=false,TypedPassed=false;FString FailedStage;TWeakObjectPtr<UWorld> World;TArray<TUniquePtr<FIdentity>> Objects;TArray<FGraph> Graphs;TArray<FScript> Scripts;TArray<FEmitter> Emitters;TArray<FSystem> Systems;TMap<UNiagaraSpriteRendererProperties*,UMaterialInterface*> ExpectedMaterials;TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Calls,SaveRows,CreationRows;TWeakObjectPtr<UNiagaraSystem> Source,Target;bool SourceDirty=false,SourceCaptured=false;FString SourceFilename,SourceFileHash;int64 SourceFileSize=-1;FString RequestedObject,RequestedPackage,RequestedFilename;int64 Slots=0,Chars=0;
 explicit FWork(bool P):Persistent(P){R.Compiled=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),false);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),false);}
 bool Fail(const TCHAR* Code,const FString& Message){if(R.Error.IsEmpty()){FailedStage=R.Stage;R.ErrorCode=Code;R.Error=Message;}R.bSuccess=false;return false;}
 TSharedPtr<FJsonObject> Error(const FString& M){if(R.Error.IsEmpty())Fail(TEXT("niagara_promotion_bootstrap_failed"),M);auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),false);O->SetStringField(TEXT("error"),M);return O;}
 bool Budget(int64 N,int64 Text=0){Slots+=N;Chars+=Text;return (Slots<=262144&&Chars<=32ll*1024*1024)||Fail(TEXT("niagara_promotion_output_budget_exceeded"),TEXT("Bounded snapshot/output exceeded; partial prefix retained."));}
 bool Admit(const FString& Stage){R.Stage=Stage;if(!R.Error.IsEmpty())return false;if(!Safe())return Fail(TEXT("niagara_promotion_context_unavailable"),TEXT("Live Editor game thread outside GC/exit/save required."));if(Persistent&&(GEditor->GetPIEWorldContext()||GEditor->PlayWorld))return Fail(TEXT("niagara_promotion_play_session_active"),TEXT("Persistent factory creation requires stopped PIE/SIE."));auto* W=GEditor->GetEditorWorldContext().World();if(!Live(W)||W->bIsTearingDown)return Fail(TEXT("niagara_promotion_world_unavailable"),TEXT("Editor world unavailable or tearing down."));if(!WorldCaptured){World=W;WorldCaptured=true;}else if(World.Get()!=W)return Fail(TEXT("niagara_promotion_world_changed"),TEXT("Editor world changed during SDK dispatch."));for(const auto& I:Objects)if(!I->Exact())return Fail(TEXT("niagara_promotion_identity_changed"),TEXT("Captured object class/outer/package/path changed; replacement not adopted."));
 if(SourceCaptured&&(!Live(Source.Get())||Source->GetOutermost()->IsDirty()!=SourceDirty))return Fail(TEXT("niagara_promotion_source_dirty_changed"),TEXT("Captured source dirty state changed across SDK callbacks; no replay or automatic source save."));
 for(const auto& S:Scripts)if(S.Script->GetLatestSource()!=S.Source.Get()||S.Script->GetExposedVersion().VersionGuid!=S.Exposed)return Fail(TEXT("niagara_promotion_script_changed"),TEXT("Captured script source/version changed."));
 for(const auto& E:Emitters){auto* O=E.Emitter.Get();if(O->GetEmitterData(E.Version)!=E.Data||(E.Latest&&O->GetLatestEmitterData()!=E.Data)||!O->GetUniqueEmitterName().Equals(E.UniqueName,ESearchCase::CaseSensitive)||!EmitterPropertyState(E.Data).Equals(E.PropertyState,ESearchCase::CaseSensitive)||E.Data->GraphSource!=E.Source.Get()||E.Data->VersionedParent.Emitter!=E.Parent.Get()||E.Data->VersionedParentAtLastMerge.Emitter!=E.MergeParent.Get()||E.Data->GetRenderers().Num()!=E.Renderers.Num())return Fail(TEXT("niagara_promotion_emitter_changed"),TEXT("Emitter version/source/parent/renderer topology changed."));for(int32 I=0;I<E.Renderers.Num();++I)if(E.Data->GetRenderers()[I]!=E.Renderers[I].Get())return Fail(TEXT("niagara_promotion_renderer_changed"),TEXT("Physical renderer slot changed."));TArray<UNiagaraScript*> A;E.Data->GetScripts(A,false,false);if(A.Num()!=E.Scripts.Num())return Fail(TEXT("niagara_promotion_emitter_scripts_changed"),TEXT("Emitter scripts changed."));for(int32 I=0;I<A.Num();++I)if(A[I]!=E.Scripts[I].Get())return Fail(TEXT("niagara_promotion_emitter_scripts_changed"),TEXT("Emitter script order changed."));}
 for(const auto& S:Systems){auto* O=S.System.Get();if(O->GetSystemSpawnScript()!=S.Spawn.Get()||O->GetSystemUpdateScript()!=S.Update.Get()||O->GetEmitterHandles().Num()!=S.Handles.Num())return Fail(TEXT("niagara_promotion_system_changed"),TEXT("System script/emitter topology changed."));for(int32 I=0;I<S.Handles.Num();++I){const auto& H=O->GetEmitterHandles()[I];const auto& E=S.Handles[I];if(H.GetId()!=E.Id||H.GetEmitterBase()!=E.Base.Get()||H.GetEmitterMode()!=E.Mode||H.GetName()!=E.Name||H.GetIsEnabled()!=E.Enabled||H.GetInstance().Emitter!=E.Emitter.Get()||H.GetInstance().Version!=E.Version||H.GetEmitterData()!=E.Data)return Fail(TEXT("niagara_promotion_system_changed"),TEXT("System physical emitter/version changed."));}}
 for(const auto& G:Graphs){if(G.Source->NodeGraph!=G.Graph.Get()||G.Graph->Nodes.Num()!=G.Nodes.Num())return Fail(TEXT("niagara_promotion_graph_changed"),TEXT("Captured source graph membership changed."));for(int32 I=0;I<G.Nodes.Num();++I){auto* N=G.Nodes[I].Node.Get();const auto& E=G.Nodes[I];if(G.Graph->Nodes[I]!=N||N->NodeGuid!=E.Id||N->Pins.Num()!=E.Pins.Num())return Fail(TEXT("niagara_promotion_node_changed"),TEXT("Captured node/pin topology changed."));for(int32 J=0;J<E.Pins.Num();++J){auto* P=N->Pins[J];const auto& V=E.Pins[J];if(P!=V.P||P->PinId!=V.Id||P->PersistentGuid!=V.Persistent||P->PinName!=V.Name||P->Direction!=V.Direction||P->PinType!=V.Type||P->bNotConnectable!=V.NotConnectable||P->bDefaultValueIsReadOnly!=V.DefaultReadOnly||P->DefaultObject!=V.DefaultObject.Get()||!P->AutogeneratedDefaultValue.Equals(V.AutoDefault,ESearchCase::CaseSensitive)||!P->DefaultTextValue.ToString().Equals(V.DefaultText,ESearchCase::CaseSensitive)||P->GetOwningNodeUnchecked()!=N||P->LinkedTo!=V.Links||!P->DefaultValue.Equals(V.Default,ESearchCase::CaseSensitive))return Fail(TEXT("niagara_promotion_pin_changed"),TEXT("Exact pin/links/default changed across SDK call."));}}}
 for(const auto& Pair:ExpectedMaterials)if(Pair.Key->Material!=Pair.Value)return Fail(TEXT("niagara_promotion_material_readback_failed"),TEXT("Owned sprite material differs from requested exact material."));return true;}
 bool Pin(UObject* O){if(!Admit(TEXT("pin_object"))||!Live(O))return Fail(TEXT("niagara_promotion_object_unavailable"),TEXT("Expected live stock object."));for(const auto& I:Objects)if(I->Object.Get()==O)return I->Exact();if(!Budget(1,O->GetPathName().Len()))return false;Objects.Add(MakeUnique<FIdentity>(O));return true;}
 bool SDK(const TCHAR* Stage,bool Mutation=false,bool PersistentWrite=false){if(!Admit(Stage)||!Budget(1,FCString::Strlen(Stage)))return false;R.bMutationAttempted|=Mutation;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);if(PersistentWrite)R.Data->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),true);double N=0;Counts->TryGetNumberField(Stage,N);Counts->SetNumberField(Stage,N+1);auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("stage"),Stage);O->SetBoolField(TEXT("entered"),true);O->SetBoolField(TEXT("returned"),false);O->SetBoolField(TEXT("mutation_attempted"),Mutation);Calls.Add(MakeShared<FJsonValueObject>(O));return true;}
 bool After(const TCHAR* Stage){if(!Calls.IsEmpty())Calls.Last()->AsObject()->SetBoolField(TEXT("returned"),true);return Admit(FString(TEXT("after_"))+Stage);}
 template<typename F>bool Step(const TCHAR* Stage,F&& Fn,bool Mutation=false,bool PersistentWrite=false){if(!SDK(Stage,Mutation,PersistentWrite))return false;Fn();return After(Stage);}
 bool Graph(UNiagaraScriptSourceBase* Base){if(!Admit(TEXT("capture_graph")))return false;if(!Base)return true;if(!Pin(Base))return false;auto* S=Cast<UNiagaraScriptSource>(Base);if(!S||!S->NodeGraph)return true;auto* G=S->NodeGraph.Get();for(const auto& E:Graphs)if(E.Source.Get()==S&&E.Graph.Get()==G)return true;if(!Pin(G)||G->Nodes.Num()>16384)return Fail(TEXT("niagara_promotion_graph_bound"),TEXT("Graph node bound exceeded."));FGraph E;E.Source=S;E.Graph=G;TSet<UEdGraphPin*> Known;for(UEdGraphNode* N:G->Nodes){if(!N||!Pin(N))return Fail(TEXT("niagara_promotion_graph_malformed"),TEXT("Null/unavailable graph node."));FNode NE;NE.Node=N;NE.Id=N->NodeGuid;for(auto* P:N->Pins){if(!P||P->GetOwningNodeUnchecked()!=N||!Budget(1+P->LinkedTo.Num(),P->DefaultValue.Len()+P->AutogeneratedDefaultValue.Len()+P->DefaultTextValue.ToString().Len()))return Fail(TEXT("niagara_promotion_pin_malformed"),TEXT("Null/foreign pin or capture bound exceeded."));FPin PE;PE.P=P;PE.Id=P->PinId;PE.Persistent=P->PersistentGuid;PE.Name=P->PinName;PE.Direction=P->Direction;PE.Type=P->PinType;PE.NotConnectable=P->bNotConnectable;PE.DefaultReadOnly=P->bDefaultValueIsReadOnly;PE.DefaultObject=P->DefaultObject;PE.AutoDefault=P->AutogeneratedDefaultValue;PE.DefaultText=P->DefaultTextValue.ToString();PE.Links=P->LinkedTo;PE.Default=P->DefaultValue;Known.Add(P);if(P->DefaultObject&&!Pin(P->DefaultObject))return false;if(P->PinType.PinSubCategoryObject.IsValid()&&!Pin(P->PinType.PinSubCategoryObject.Get()))return false;NE.Pins.Add(MoveTemp(PE));}if(auto* F=Cast<UNiagaraNodeFunctionCall>(N)){if(F->Signature.Inputs.Num()+F->Signature.Outputs.Num()>65536)return Fail(TEXT("niagara_promotion_signature_bound"),TEXT("Function signature bound exceeded."));if(F->FunctionScript&&!Pin(F->FunctionScript))return false;}E.Nodes.Add(MoveTemp(NE));}for(const auto& N:E.Nodes)for(const auto& P:N.Pins)for(auto* L:P.Links)if(!L||!Known.Contains(L))return Fail(TEXT("niagara_promotion_link_malformed"),TEXT("Link target is outside captured graph."));Graphs.Add(MoveTemp(E));return Admit(TEXT("graph_captured"));}
 bool Script(UNiagaraScript* S){if(!S)return true;for(const auto& E:Scripts)if(E.Script.Get()==S)return true;if(!Pin(S))return false;FScript E;E.Script=S;E.Source=S->GetLatestSource();E.Exposed=S->GetExposedVersion().VersionGuid;Scripts.Add(E);return Graph(E.Source.Get());}
 bool Emitter(UNiagaraEmitter* O,FVersionedNiagaraEmitterData* D,bool Latest){if(!O||!D||!Pin(O))return Fail(TEXT("niagara_promotion_emitter_data_missing"),TEXT("Expected current versioned emitter data."));for(const auto& E:Emitters)if(E.Emitter.Get()==O&&E.Data==D)return true;FEmitter E;E.Emitter=O;E.Data=D;E.Version=D->Version.VersionGuid;E.Latest=Latest;E.UniqueName=O->GetUniqueEmitterName();E.PropertyState=EmitterPropertyState(D);E.Source=D->GraphSource;E.Parent=D->VersionedParent.Emitter;E.MergeParent=D->VersionedParentAtLastMerge.Emitter;if(!Graph(E.Source.Get()))return false;if(D->GetRenderers().Num()>4096)return Fail(TEXT("niagara_promotion_renderer_bound"),TEXT("Renderer bound exceeded."));for(UNiagaraRendererProperties* P:D->GetRenderers()){E.Renderers.Add(P);if(P&&!Pin(P))return false;if(auto* Sprite=Cast<UNiagaraSpriteRendererProperties>(P)){ExpectedMaterials.Add(Sprite,Sprite->Material);if(Sprite->Material&&!Pin(Sprite->Material))return false;}}TArray<UNiagaraScript*> A;D->GetScripts(A,false,false);if(A.Num()>4096)return Fail(TEXT("niagara_promotion_script_bound"),TEXT("Script bound exceeded."));for(auto* P:A){E.Scripts.Add(P);if(!Script(P))return false;}Emitters.Add(MoveTemp(E));return Admit(TEXT("emitter_captured"));}
 bool System(UNiagaraSystem* O,int32 ExpectedCount){if(!Pin(O)||O->GetEmitterHandles().Num()!=ExpectedCount)return Fail(TEXT("niagara_promotion_system_shape_invalid"),TEXT("Factory output emitter count differs from requested construction."));FSystem E;E.System=O;E.Spawn=O->GetSystemSpawnScript();E.Update=O->GetSystemUpdateScript();if(!Script(E.Spawn.Get())||!Script(E.Update.Get()))return false;for(const auto& H:O->GetEmitterHandles()){if(!H.GetEmitterBase()||!Pin(H.GetEmitterBase()))return Fail(TEXT("niagara_promotion_output_owner_invalid"),TEXT("Exact public emitter base unavailable."));if((!H.GetInstance().Emitter||!H.GetEmitterData())&&H.GetEmitterMode()!=ENiagaraEmitterMode::Stateless)return Fail(TEXT("niagara_promotion_output_owner_invalid"),TEXT("Standard emitter has no current versioned instance."));FHandle F;F.Id=H.GetId();F.Base=H.GetEmitterBase();F.Name=H.GetName();F.Enabled=H.GetIsEnabled();F.Mode=H.GetEmitterMode();F.Version=H.GetInstance().Version;F.Emitter=H.GetInstance().Emitter;F.Data=H.GetEmitterData();E.Handles.Add(F);if(F.Emitter.IsValid()&&F.Data&&!Emitter(F.Emitter.Get(),F.Data,false))return false;}Systems.Add(MoveTemp(E));return Admit(TEXT("system_captured"));}
 TSharedPtr<FJsonObject> DirtyProjection(){auto O=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;const bool CanRead=Safe()&&WorldCaptured&&Live(World.Get())&&!World->bIsTearingDown&&GEditor->GetEditorWorldContext().World()==World.Get();for(const auto& I:Objects){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),I->Path);J->SetStringField(TEXT("package"),I->PackageName);J->SetBoolField(TEXT("dirty_before"),I->DirtyBefore);const bool Exact=CanRead&&I->Exact();J->SetBoolField(TEXT("current_observation_available"),Exact);if(Exact)J->SetBoolField(TEXT("dirty_after"),I->Package->IsDirty());Rows.Add(MakeShared<FJsonValueObject>(J));}O->SetArrayField(TEXT("objects"),Rows);O->SetBoolField(TEXT("safe_context"),CanRead);return O;}
};
FWork* Current=nullptr;
bool ContinueRead(){return Current&&Current->Admit(TEXT("read_continuation"));}

bool NoEmbeddedNul(const FString& S)
{for(int32 I=0;I<S.Len();++I){if(S[I]==TCHAR(0)){return false;}}return true;}
struct FExactKeys:DefaultKeyFuncs<FString>
{static bool Matches(const FString& A,const FString& B){return A.Equals(B,ESearchCase::CaseSensitive);}static uint32 GetKeyHash(const FString& K){return FCrc::StrCrc32(*K);}};
bool Finite(const TSharedPtr<FJsonValue>& V,int32 Depth,int32& Count)
{if(!V.IsValid()||Depth>64||++Count>262144){return false;}if(V->Type==EJson::Number){double N=0;return V->TryGetNumber(N)&&FMath::IsFinite(N);}if(V->Type==EJson::String){return NoEmbeddedNul(V->AsString());}if(V->Type==EJson::Array){for(const auto& X:V->AsArray()){if(!Finite(X,Depth+1,Count)){return false;}}}else if(V->Type==EJson::Object){for(const auto& X:V->AsObject()->Values){if(!NoEmbeddedNul(FString(X.Key))||!Finite(X.Value,Depth+1,Count)){return false;}}}return V->Type!=EJson::None;}
bool Parse(const FString& Text,TSharedPtr<FJsonObject>& Out)
{
    if(Text.IsEmpty()||Text.Len()>1048576||!NoEmbeddedNul(Text)){return false;}
    int32 Depth=0;bool InString=false,Escape=false;for(int32 I=0;I<Text.Len();++I){const TCHAR C=Text[I];if(InString){if(Escape){Escape=false;}else if(C=='\\'){Escape=true;}else if(C=='"'){InString=false;}}else if(C=='"'){InString=true;}else if(C=='{'||C=='['){if(++Depth>64){return false;}}else if(C=='}'||C==']'){if(--Depth<0){return false;}}}if(InString||Depth!=0){return false;}
    auto Reader=TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]"));TArray<TSet<FString,FExactKeys>> Keys;TArray<bool> Objects;EJsonNotation Token;int32 Tokens=0;
    while(Reader->ReadNext(Token)){if(Token==EJsonNotation::Error||++Tokens>262144){return false;}if(Token!=EJsonNotation::ObjectEnd&&Token!=EJsonNotation::ArrayEnd&&!Objects.IsEmpty()&&Objects.Last()){const FString K(Reader->GetIdentifier());if(!NoEmbeddedNul(K)||Keys.Last().Contains(K)){return false;}Keys.Last().Add(K);}if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart){Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();}else if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd){if(Objects.IsEmpty()){return false;}Objects.Pop();Keys.Pop();}}
    TArray<TSharedPtr<FJsonValue>> A;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]")),A)||A.Num()!=1||!A[0].IsValid()||A[0]->Type!=EJson::Object){return false;}int32 Count=0;if(!Finite(A[0],0,Count)){return false;}Out=A[0]->AsObject();return Out.IsValid();
}

FString NormalizeNiagaraObjectPathForLoad(const FString& ObjectPath)
{
    FString NormalizedPath = FPackageName::ExportTextPathToObjectPath(ObjectPath).TrimStartAndEnd();
    NormalizedPath.TrimQuotesInline();

    if (NormalizedPath.StartsWith(TEXT("/")) && !NormalizedPath.StartsWith(TEXT("/Script/")) && !NormalizedPath.Contains(TEXT(".")))
    {
        const FString AssetName = FPackageName::GetShortName(NormalizedPath);
        NormalizedPath = FString::Printf(TEXT("%s.%s"), *NormalizedPath, *AssetName);
    }

    return NormalizedPath;
}
FString PackagePathFromObjectPath(const FString& ObjectPath)
{
    FString PackagePath = FPackageName::ExportTextPathToObjectPath(ObjectPath).TrimStartAndEnd();
    PackagePath.TrimQuotesInline();
    if (PackagePath.EndsWith(TEXT(".uasset"), ESearchCase::IgnoreCase))
    {
        PackagePath.LeftChopInline(7);
    }
    const int32 LastSlashIndex = PackagePath.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
    const int32 DotIndex = PackagePath.Find(
        TEXT("."),
        ESearchCase::CaseSensitive,
        ESearchDir::FromStart,
        LastSlashIndex == INDEX_NONE ? 0 : LastSlashIndex + 1);
    if (DotIndex != INDEX_NONE)
    {
        PackagePath.LeftInline(DotIndex);
    }
    return PackagePath;
}
bool IsTempGeneratedNiagaraPath(const FString& ObjectPath)
{
    const FString PackagePath = PackagePathFromObjectPath(ObjectPath);
    FString TempRoot = NiagaraTempGenerationRoot;
    while (TempRoot.Len() > 1 && TempRoot.EndsWith(TEXT("/")))
    {
        TempRoot.LeftChopInline(1);
    }
    return PackagePath.Equals(TempRoot) || PackagePath.StartsWith(TempRoot + TEXT("/"));
}
FString NormalizeRequiredRoot(FString RequiredRoot)
{
    RequiredRoot.TrimStartAndEndInline();
    RequiredRoot.TrimQuotesInline();
    RequiredRoot.ReplaceInline(TEXT("\\"), TEXT("/"));
    while (RequiredRoot.Len() > 1 && RequiredRoot.EndsWith(TEXT("/")))
    {
        RequiredRoot.LeftChopInline(1);
    }
    return RequiredRoot;
}
bool IsUnderRequiredRoot(const FString& PackageName, const FString& RequiredRoot)
{
    const FString NormalizedRoot = NormalizeRequiredRoot(RequiredRoot);
    return NormalizedRoot.IsEmpty() ||
        PackageName.Equals(NormalizedRoot, ESearchCase::IgnoreCase) ||
        PackageName.StartsWith(NormalizedRoot + TEXT("/"), ESearchCase::IgnoreCase);
}
TArray<FString> GetStringArrayParam(const TSharedPtr<FJsonObject>& Params, const TCHAR* FieldName)
{
    TArray<FString> Values;
    const TArray<TSharedPtr<FJsonValue>>* JsonValues = nullptr;
    if (!Params.IsValid() || !Params->TryGetArrayField(FieldName, JsonValues) || !JsonValues)
    {
        return Values;
    }

    for (const TSharedPtr<FJsonValue>& JsonValue : *JsonValues)
    {
        FString Value;
        if (JsonValue.IsValid() && JsonValue->TryGetString(Value))
        {
            Value.TrimStartAndEndInline();
            Value.TrimQuotesInline();
            Value.ReplaceInline(TEXT("\\"), TEXT("/"));
            if (!Value.IsEmpty())
            {
                Values.AddUnique(NormalizeRequiredRoot(PackagePathFromObjectPath(Value)));
            }
        }
    }
    return Values;
}
TArray<TSharedPtr<FJsonValue>> StringArrayToJsonValues(const TArray<FString>& Values)
{
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    for (const FString& Value : Values)
    {
        JsonValues.Add(MakeShared<FJsonValueString>(Value));
    }
    return JsonValues;
}

bool IsMCPDependencyReference(const FString& Value)
{
    FString Candidate = FPackageName::ExportTextPathToObjectPath(Value).TrimStartAndEnd();
    Candidate.TrimQuotesInline();
    if (Candidate.IsEmpty())
    {
        return false;
    }

    return Candidate.Contains(TEXT("CubelessMCP"),ESearchCase::IgnoreCase)||Candidate.Contains(TEXT("ToolsetRegistry"),ESearchCase::IgnoreCase)||Candidate.Contains(TEXT("ModelContextProtocol"),ESearchCase::IgnoreCase)||Candidate.Contains(TEXT("/Script/UnrealMCP"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("/UnrealMCP"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("UnrealMCP"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("MCPUnreal"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("mcp_unreal"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("/Script/CubelessControlRigTools"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("CubelessControlRigTools"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("CubelessControlRigToolsEditor"), ESearchCase::IgnoreCase);
}
FString GetMCPDependencyReferenceError(const FString& FieldName, const FString& Value)
{
    const FString FieldLabel = FieldName.IsEmpty() ? TEXT("value") : FieldName;
    return FString::Printf(
        TEXT("Refusing to persist MCP-only reference in '%s': %s. UnrealMCP is an editor authoring bridge; finished assets must not depend on it."),
        *FieldLabel,
        *Value);
}struct FNiagaraAssetTarget{FString RequestedPath,PackageName,PackagePath,AssetName,ObjectPath,Filename;};
bool ResolveNiagaraPromotionTarget(
    const FString& RequestedPath,
    bool bAllowTempTarget,
    FNiagaraAssetTarget& OutTarget,
    FString& OutError)
{
    FString PackageName = PackagePathFromObjectPath(RequestedPath);
    PackageName.ReplaceInline(TEXT("\\"), TEXT("/"));
    if (PackageName.IsEmpty())
    {
        OutError = TEXT("Missing 'target_system_path' parameter");
        return false;
    }

    if (IsMCPDependencyReference(RequestedPath) ||
        IsMCPDependencyReference(PackageName))
    {
        OutError = GetMCPDependencyReferenceError(TEXT("target_system_path"), RequestedPath);
        return false;
    }

    FText InvalidPackageReason;
    if (!FPackageName::IsValidLongPackageName(PackageName, false, &InvalidPackageReason) ||
        !PackageName.StartsWith(TEXT("/Game/")))
    {
        OutError = FString::Printf(
            TEXT("Promotion target must be a valid /Game long package path '%s': %s"),
            *RequestedPath,
            *InvalidPackageReason.ToString());
        return false;
    }

    if (!bAllowTempTarget && IsUnderRequiredRoot(PackageName, TEXT("/Game/_MCP_Temp")))
    {
        OutError = FString::Printf(
            TEXT("Promotion target must be outside /Game/_MCP_Temp unless allow_temp_target=true: %s"),
            *PackageName);
        return false;
    }

    FString PackageFilename;
    if (!FPackageName::TryConvertLongPackageNameToFilename(PackageName, PackageFilename, FPackageName::GetAssetPackageExtension()))
    {
        OutError = FString::Printf(TEXT("Failed to resolve package filename for promotion target: %s"), *PackageName);
        return false;
    }

    const FString AssetName = FPackageName::GetShortName(PackageName);
    FText InvalidObjectNameReason;
    if (!FName(*AssetName).IsValidObjectName(InvalidObjectNameReason))
    {
        OutError = FString::Printf(
            TEXT("Invalid Niagara promotion target object name '%s': %s"),
            *AssetName,
            *InvalidObjectNameReason.ToString());
        return false;
    }

    const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackageName, *AssetName);
    if (FPackageName::DoesPackageExist(PackageName) ||
        FPaths::FileExists(PackageFilename) ||
        StaticFindObject(UObject::StaticClass(), nullptr, *ObjectPath) != nullptr)
    {
        OutError = FString::Printf(
            TEXT("Promotion target already exists at %s. First pass promotion refuses overwrite."),
            *ObjectPath);
        return false;
    }

    OutTarget.RequestedPath = RequestedPath;
    OutTarget.PackageName = PackageName;
    OutTarget.PackagePath = FPackageName::GetLongPackagePath(PackageName);
    OutTarget.AssetName = AssetName;
    OutTarget.ObjectPath = ObjectPath;
    OutTarget.Filename = PackageFilename;
    return true;
}TSharedPtr<FJsonObject> BuildNiagaraDependencyAuditJson(
    UObject* Asset,
    const FString& SavedFilename,
    bool bBlockTempDependenciesForNonTempAsset,
    const TArray<FString>& AdditionalForbiddenDependencyPrefixes = TArray<FString>())
{
    auto& W=*Current;TSharedPtr<FJsonObject> AuditObject = MakeShared<FJsonObject>();
    AuditObject->SetBoolField(TEXT("success"), false);
    AuditObject->SetBoolField(TEXT("audit_complete"), false);
    AuditObject->SetBoolField(TEXT("asset_resolved"), Asset != nullptr);
    AuditObject->SetBoolField(TEXT("package_name_present"), false);
    AuditObject->SetBoolField(TEXT("dependency_query_attempted"), false);
    AuditObject->SetBoolField(TEXT("dependency_query_succeeded"), false);
    AuditObject->SetBoolField(TEXT("saved_file_scanned"), false);
    AuditObject->SetNumberField(TEXT("dependency_count"), 0);
    AuditObject->SetNumberField(TEXT("forbidden_dependency_hit_count"), 0);
    AuditObject->SetNumberField(TEXT("temp_dependency_hit_count"), 0);
    AuditObject->SetBoolField(TEXT("has_forbidden_dependency"), false);
    AuditObject->SetBoolField(TEXT("has_temp_dependency"), false);

    if(!W.Admit(TEXT("dependency_audit_entry")))return AuditObject;
    if (!Asset)
    {
        return AuditObject;
    }

    const FString PackageName = Asset->GetOutermost() ? Asset->GetOutermost()->GetName() : FString();
    const bool bPackageNamePresent = !PackageName.IsEmpty();
    AuditObject->SetBoolField(TEXT("package_name_present"), bPackageNamePresent);
    AuditObject->SetStringField(TEXT("package_name"), PackageName);
    AuditObject->SetStringField(TEXT("asset_path"), Asset->GetPathName());
    AuditObject->SetStringField(TEXT("saved_filename"), SavedFilename);
    AuditObject->SetArrayField(TEXT("additional_forbidden_dependency_prefixes"), StringArrayToJsonValues(AdditionalForbiddenDependencyPrefixes));

    FAssetRegistryModule* Registry=nullptr;if(!W.Step(TEXT("LoadModule_AssetRegistry"),[&]{Registry=FModuleManager::LoadModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry"));}))return AuditObject;if(!Registry){W.Fail(TEXT("niagara_promotion_registry_unavailable"),TEXT("Public AssetRegistry module unavailable."));return AuditObject;}
    if (!SavedFilename.IsEmpty())
    {
        TArray<FString> ModifiedFiles;
        ModifiedFiles.Add(SavedFilename);
        if(!W.Step(TEXT("ScanModifiedAssetFiles"),[&]{Registry->Get().ScanModifiedAssetFiles(ModifiedFiles);}))return AuditObject;
        AuditObject->SetBoolField(TEXT("saved_file_scanned"), true);
    }

    TArray<FName> Dependencies;
    bool bDependencyQuerySucceeded = false;
    if (bPackageNamePresent)
    {
        AuditObject->SetBoolField(TEXT("dependency_query_attempted"), true);
        if(!W.Step(TEXT("GetDependencies"),[&]{bDependencyQuerySucceeded=Registry->Get().GetDependencies(FName(*PackageName),Dependencies);}))return AuditObject;
        if(Dependencies.Num()>65536||!W.Budget(Dependencies.Num())){W.Fail(TEXT("niagara_promotion_dependency_bound"),TEXT("Direct dependency output exceeds bounded collection; partial audit retained."));return AuditObject;}
    }
    AuditObject->SetBoolField(TEXT("dependency_query_succeeded"), bDependencyQuerySucceeded);
    AuditObject->SetBoolField(TEXT("audit_complete"), bDependencyQuerySucceeded);
    AuditObject->SetBoolField(TEXT("success"), bDependencyQuerySucceeded);

    TArray<TSharedPtr<FJsonValue>> DependencyValues;
    TArray<TSharedPtr<FJsonValue>> ForbiddenHits;
    TArray<TSharedPtr<FJsonValue>> TempHits;
    for (const FName& Dependency : Dependencies)
    {
        const FString DependencyString = Dependency.ToString();if(!W.Budget(1,DependencyString.Len()))return AuditObject;
        DependencyValues.Add(MakeShared<FJsonValueString>(DependencyString));
        if (IsMCPDependencyReference(DependencyString))
        {
            ForbiddenHits.Add(MakeShared<FJsonValueString>(DependencyString));
        }
        for (const FString& ForbiddenPrefix : AdditionalForbiddenDependencyPrefixes)
        {
            if (!ForbiddenPrefix.IsEmpty() && IsUnderRequiredRoot(DependencyString, ForbiddenPrefix))
            {
                ForbiddenHits.Add(MakeShared<FJsonValueString>(DependencyString));
                break;
            }
        }
        if (bBlockTempDependenciesForNonTempAsset && IsUnderRequiredRoot(DependencyString, TEXT("/Game/_MCP_Temp")))
        {
            TempHits.Add(MakeShared<FJsonValueString>(DependencyString));
        }
    }

    if (IsMCPDependencyReference(Asset->GetPathName()))
    {
        ForbiddenHits.Add(MakeShared<FJsonValueString>(Asset->GetPathName()));
    }

    AuditObject->SetNumberField(TEXT("dependency_count"), DependencyValues.Num());
    AuditObject->SetNumberField(TEXT("forbidden_dependency_hit_count"), ForbiddenHits.Num());
    AuditObject->SetNumberField(TEXT("temp_dependency_hit_count"), TempHits.Num());
    AuditObject->SetBoolField(TEXT("has_forbidden_dependency"), ForbiddenHits.Num() > 0);
    AuditObject->SetBoolField(TEXT("has_temp_dependency"), TempHits.Num() > 0);
    AuditObject->SetArrayField(TEXT("dependencies"), DependencyValues);
    AuditObject->SetArrayField(TEXT("forbidden_dependency_hits"), ForbiddenHits);
    AuditObject->SetArrayField(TEXT("temp_dependency_hits"), TempHits);
    AuditObject->SetStringField(TEXT("dependency_scope"),TEXT("AssetRegistry default Package-category direct dependencies from on-disk package; not transitive or RAM closure. saved_file_scanned means ScanModifiedAssetFiles returned, not independent binary/cold validation."));return AuditObject;
}
bool ReadBoundedFileHash(FWork& W,const FString& Filename,FString& Hash,int64& Size)
{
 Hash.Reset();Size=IFileManager::Get().FileSize(*Filename);if(Size<0)return false;if(Size>256ll*1024*1024)return W.Fail(TEXT("niagara_promotion_file_budget"),TEXT("Package exceeds bounded 256MiB source evidence hash; no writer or replay."));TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*Filename));if(!Reader||Reader->TotalSize()!=Size)return false;FMD5 Md5;TArray<uint8> Chunk;Chunk.SetNumUninitialized(65536);int64 Left=Size;while(Left>0){const int64 N=FMath::Min<int64>(Left,Chunk.Num());Reader->Serialize(Chunk.GetData(),N);if(Reader->IsError())return false;Md5.Update(Chunk.GetData(),static_cast<uint32>(N));Left-=N;}uint8 Digest[16];Md5.Final(Digest);Hash=BytesToHex(Digest,16);return !Reader->IsError()&&Reader->TotalSize()==Size&&IFileManager::Get().FileSize(*Filename)==Size;
}
UNiagaraSystem* LoadSource(FWork& W,const FString& Path)
{
 const FString Exact=NormalizeNiagaraObjectPathForLoad(Path);UNiagaraSystem* O=nullptr;if(!W.Step(TEXT("LoadObject_SourceSystem"),[&]{O=LoadObject<UNiagaraSystem>(nullptr,*Exact);W.R.Data->SetBoolField(TEXT("source_load_returned_nonnull"),O!=nullptr);})){return nullptr;}if(!Live(O)||!O->GetPathName().Equals(Exact,ESearchCase::CaseSensitive)||!W.Pin(O->GetOutermost())||!W.System(O,O->GetEmitterHandles().Num()))return nullptr;W.Source=O;W.SourceDirty=O->GetOutermost()->IsDirty();W.SourceCaptured=true;W.R.Data->SetStringField(TEXT("captured_source_path"),O->GetPathName());return W.Admit(TEXT("source_captured"))?O:nullptr;
}
bool SourceFileEvidence(FWork& W,const FString& Filename,bool Verify)
{
 if(Filename.IsEmpty())return true;if(!W.Admit(TEXT("source_file_evidence")))return false;FString Hash;int64 Size=-1;if(!ReadBoundedFileHash(W,Filename,Hash,Size))return W.Fail(TEXT("niagara_promotion_source_file_unavailable"),TEXT("Exact source disk evidence unavailable or concurrently changed."));if(Verify){const bool Same=Filename.Equals(W.SourceFilename,ESearchCase::CaseSensitive)&&Size==W.SourceFileSize&&Hash.Equals(W.SourceFileHash,ESearchCase::CaseSensitive);W.R.Data->SetBoolField(TEXT("source_disk_hash_unchanged"),Same);if(!Same)return W.Fail(TEXT("niagara_promotion_source_disk_changed"),TEXT("Source package disk bytes changed; actual target prefix retained without replay."));}else{W.SourceFilename=Filename;W.SourceFileSize=Size;W.SourceFileHash=Hash;W.R.Data->SetStringField(TEXT("source_file_hash_before"),Hash);W.R.Data->SetNumberField(TEXT("source_file_size_before"),static_cast<double>(Size));}return W.Admit(TEXT("after_source_file_evidence"));
}
bool FreshTarget(FWork& W,const FNiagaraAssetTarget& T,UPackage* OwnEmpty=nullptr)
{
 if(!W.Admit(TEXT("fresh_target_admission")))return false;auto Local=[&](){if(T.PackageName.Equals(PackagePathFromObjectPath(W.Source->GetPathName()),ESearchCase::IgnoreCase))return false;if(FindObject<UObject>(nullptr,*T.ObjectPath)||FPackageName::DoesPackageExist(T.PackageName)||IFileManager::Get().FileExists(*T.Filename))return false;UPackage* Existing=FindPackage(nullptr,*T.PackageName);if(Existing!=OwnEmpty)return false;if(OwnEmpty){if(!Live(OwnEmpty)||!OwnEmpty->GetName().Equals(T.PackageName,ESearchCase::CaseSensitive))return false;TArray<UObject*> Exports;GetObjectsWithPackage(OwnEmpty,Exports,true);if(!Exports.IsEmpty())return false;}return true;};if(!Local())return W.Fail(TEXT("niagara_promotion_target_collision"),TEXT("Target memory/package/disk is not exact absent or the owned empty package; no overwrite."));FAssetRegistryModule* Registry=nullptr;if(!W.Step(TEXT("LoadModule_TargetRegistry"),[&]{Registry=FModuleManager::LoadModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry"));})||!Registry)return false;TArray<FAssetData> Assets;if(!W.Step(TEXT("GetAssetsByPackageName_Target"),[&]{Registry->Get().GetAssetsByPackageName(FName(*T.PackageName),Assets,false,false);}))return false;if(!Assets.IsEmpty()||!Local())return W.Fail(TEXT("niagara_promotion_target_collision"),TEXT("Target registry/SDK callback introduced a package, object or disk collision; no overwrite."));return W.Admit(TEXT("fresh_target_final"));
}
bool PackageOwnership(FWork& W,UNiagaraSystem* Root,const TCHAR* Stage)
{
 if(!W.Admit(Stage)||!Live(Root)||!W.Target.IsValid()||W.Target.Get()!=Root)return false;UPackage* P=Root->GetOutermost();if(Root->GetOuter()!=P||Root->GetPackage()!=P||!Root->GetPathName().Equals(W.RequestedObject,ESearchCase::CaseSensitive)||!P->GetName().Equals(W.RequestedPackage,ESearchCase::CaseSensitive))return W.Fail(TEXT("niagara_promotion_target_owner_changed"),TEXT("Exact duplicated root/package changed."));TArray<UObject*> Exports;if(!W.Step(TEXT("GetObjectsWithPackage_Target"),[&]{GetObjectsWithPackage(P,Exports,true);}))return false;if(Exports.Num()>65536||!W.Budget(Exports.Num()))return false;for(UObject* O:Exports){if(!Live(O)||O->GetPackage()!=P||O->GetOutermost()!=P||(O!=Root&&!O->IsIn(Root)))return W.Fail(TEXT("niagara_promotion_foreign_package_export"),TEXT("Target package contains an export outside the exact duplicate root subtree; save refused, actual residual retained."));if(!W.Pin(O))return false;}return W.Admit(Stage);
}
// This is a current-memory hard-reference observation, not an AssetRegistry disk audit.
// The unsaved temporary branch leaves the actual disk-audit failure intact for its caller.
TSharedPtr<FJsonObject> ObserveUnsavedTemporaryHardReferences(FWork& W,UNiagaraSystem* Root,const FNiagaraAssetTarget& Target,bool BlockTemp,const TArray<FString>& ForbiddenPrefixes)
{
 auto Evidence=MakeShared<FJsonObject>();Evidence->SetBoolField(TEXT("observations_complete"),false);
 Evidence->SetBoolField(TEXT("soft_reference_audit_performed"),false);Evidence->SetBoolField(TEXT("transitive_audit_performed"),false);
 Evidence->SetBoolField(TEXT("full_RAM_dependency_audit_performed"),false);Evidence->SetBoolField(TEXT("on_disk_audit_performed"),false);
 W.R.Data->SetObjectField(TEXT("unsaved_temporary_hard_reference_evidence"),Evidence);
 if(!PackageOwnership(W,Root,TEXT("unsaved_reference_ownership_before")))return Evidence;
 UPackage* Package=Root->GetOutermost();TArray<UObject*> Exports;TArray<TStrongObjectPtr<UObject>> ExportHolds;
 if(!W.Step(TEXT("GetObjectsWithPackage_UnsavedReferences"),[&]{GetObjectsWithPackage(Package,Exports,true);for(UObject* Export:Exports)if(Export)ExportHolds.Emplace(Export);}))return Evidence;
 if(Exports.Num()>65536||!W.Budget(Exports.Num())){W.Fail(TEXT("niagara_promotion_unsaved_reference_bound"),TEXT("Unsaved export observation exceeds its finite bound."));return Evidence;}
 TSet<UObject*> ExportMembership;for(UObject* Export:Exports)ExportMembership.Add(Export);
 // Only membership equality is checked here; no newly observed authored values seed W's baseline.
 auto CurrentExports=[&]()->bool
 {
  if(!W.Admit(TEXT("unsaved_reference_current")))return false;
  if(!Live(Root)||Root!=W.Target.Get()||Root->GetOuter()!=Package||Root->GetOutermost()!=Package||!Root->GetPathName().Equals(Target.ObjectPath,ESearchCase::CaseSensitive)
    ||IFileManager::Get().FileExists(*Target.Filename)||FPackageName::DoesPackageExist(Target.PackageName))
   return W.Fail(TEXT("niagara_promotion_unsaved_target_changed"),TEXT("Exact unsaved temporary target identity or absent-file role changed."));
  TArray<UObject*> Now;GetObjectsWithPackage(Package,Now,true);
  if(Now.Num()!=Exports.Num())return W.Fail(TEXT("niagara_promotion_unsaved_exports_changed"),TEXT("Unsaved target export inventory changed across reference observation."));
  for(UObject* Export:Now)if(!ExportMembership.Contains(Export))return W.Fail(TEXT("niagara_promotion_unsaved_exports_changed"),TEXT("Unsaved target export membership changed across reference observation."));
  for(UObject* Export:Exports)if(!Live(Export)||Export->GetPackage()!=Package||Export->GetOutermost()!=Package||(Export!=Root&&!Export->IsIn(Root)))
   return W.Fail(TEXT("niagara_promotion_unsaved_exports_changed"),TEXT("Retained unsaved target export left its exact owned subtree."));
  return true;
 };
 if(!CurrentExports())return Evidence;
 TSet<FString> Packages;int64 ReferenceCount=0;
 for(UObject* Export:Exports)
 {
  if(!CurrentExports())return Evidence;
  TArray<UObject*> References;TArray<TStrongObjectPtr<UObject>> ReferenceHolds;
  if(!W.Step(TEXT("FindReferences_UnsavedExport"),[&]{FReferenceFinder Finder(References,nullptr,false,false,false,false);Finder.FindReferences(Export);for(UObject* Ref:References)if(Ref)ReferenceHolds.Emplace(Ref);}))return Evidence;
  // Returned references are held inside the return lambda, before W.Step's callback/current fence.
  if(!CurrentExports())return Evidence;
  if(References.Num()>65536||!W.Budget(References.Num())){W.Fail(TEXT("niagara_promotion_unsaved_reference_bound"),TEXT("Unsaved direct-reference observation exceeds its finite bound."));return Evidence;}
  for(UObject* Ref:References)
  {
   if(!Ref)continue;
   if(!Live(Ref)){W.Fail(TEXT("niagara_promotion_unsaved_reference_unavailable"),TEXT("Observed held hard reference is not a live loaded object."));return Evidence;}
   UPackage* RefPackage=Ref->GetOutermost();if(!Live(RefPackage)){W.Fail(TEXT("niagara_promotion_unsaved_reference_unavailable"),TEXT("Observed hard reference has no current live package."));return Evidence;}
   if(RefPackage!=Package)Packages.Add(RefPackage->GetName());++ReferenceCount;
  }
 }
 TArray<FString> Ordered=Packages.Array();Ordered.Sort();TArray<TSharedPtr<FJsonValue>> PackageRows,ForbiddenRows,TempRows;
 for(const FString& Name:Ordered)
 {
  PackageRows.Add(MakeShared<FJsonValueString>(Name));bool IsForbidden=IsMCPDependencyReference(Name);
  for(const FString& Prefix:ForbiddenPrefixes)if(!Prefix.IsEmpty()&&IsUnderRequiredRoot(Name,Prefix)){IsForbidden=true;break;}
  if(IsForbidden)ForbiddenRows.Add(MakeShared<FJsonValueString>(Name));
  if(BlockTemp&&IsUnderRequiredRoot(Name,TEXT("/Game/_MCP_Temp")))TempRows.Add(MakeShared<FJsonValueString>(Name));
 }
 Evidence->SetNumberField(TEXT("observed_export_count"),Exports.Num());Evidence->SetNumberField(TEXT("observed_hard_reference_count"),static_cast<double>(ReferenceCount));
 Evidence->SetArrayField(TEXT("direct_hard_reference_packages"),PackageRows);Evidence->SetArrayField(TEXT("forbidden_dependency_hits"),ForbiddenRows);Evidence->SetArrayField(TEXT("temp_dependency_hits"),TempRows);
 Evidence->SetBoolField(TEXT("has_forbidden_dependency"),!ForbiddenRows.IsEmpty());Evidence->SetBoolField(TEXT("has_temp_dependency"),!TempRows.IsEmpty());
 Evidence->SetBoolField(TEXT("observations_complete"),CurrentExports());
 Evidence->SetStringField(TEXT("scope"),TEXT("Public FReferenceFinder over retained exports of the exact unsaved temporary duplicate; directly observed hard UObject package references only. No soft, transitive, full RAM or on-disk dependency audit and no source-to-target dependency equality claim."));
 return Evidence;
}
bool SavePromoted(FWork& W,UNiagaraSystem* O,FString& Filename)
{
 Filename.Reset();if(!PackageOwnership(W,O,TEXT("before_save_package_ownership"))||!SourceFileEvidence(W,W.SourceFilename,true))return false;UPackage* P=O->GetOutermost();const FString Path=W.RequestedFilename;if(!W.SDK(TEXT("SavePackage"),true,true))return false;W.R.bSaveAttempted=true;auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("requested_asset"),W.RequestedObject);Row->SetStringField(TEXT("requested_package"),W.RequestedPackage);Row->SetStringField(TEXT("requested_filename"),Path);FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;Args.bSlowTask=false;bool Returned=UPackage::SavePackage(P,O,*Path,Args);W.R.bSaved|=Returned;Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("SDK_save_success"),Returned);W.SaveRows.Add(MakeShared<FJsonValueObject>(Row));if(Returned)Filename=Path;if(!W.After(TEXT("SavePackage"))){Row->SetBoolField(TEXT("post_save_observation_skipped"),true);return false;}const bool Dirty=P->IsDirty();Row->SetBoolField(TEXT("dirty_after"),Dirty);if(Returned&&Dirty)return W.Fail(TEXT("niagara_promotion_saved_package_dirty"),TEXT("SavePackage returned true but exact package is dirty after callbacks; write history retained, dirty not cleared."));return Returned&&PackageOwnership(W,O,TEXT("after_save_package_ownership"));
}
TSharedPtr<FJsonObject> HandleAudit(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITOR
    FString SystemPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }

    SystemPath.TrimStartAndEndInline();
    if (IsMCPDependencyReference(SystemPath))
    {
        return W.Error(GetMCPDependencyReferenceError(TEXT("system_path"), SystemPath));
    }

    bool bAllowTempTarget = false;
    bool bAllowTempDependencies = false;
    if (Params.IsValid())
    {
        Params->TryGetBoolField(TEXT("allow_temp_target"), bAllowTempTarget);
        Params->TryGetBoolField(TEXT("allow_temp_dependencies"), bAllowTempDependencies);
    }

    const FString SourcePackageName = PackagePathFromObjectPath(SystemPath);
    FString ProposedTargetPath;
    FString ProposedTargetPackageName;
    bool bHasProposedTargetPath = false;
    bool bTargetPathValid = true;
    TArray<TSharedPtr<FJsonValue>> Blockers;
    if (Params.IsValid() && Params->TryGetStringField(TEXT("proposed_target_path"), ProposedTargetPath))
    {
        ProposedTargetPath.TrimStartAndEndInline();
        bHasProposedTargetPath = !ProposedTargetPath.IsEmpty();
    }
    if (bHasProposedTargetPath)
    {
        ProposedTargetPackageName = PackagePathFromObjectPath(ProposedTargetPath);
        ProposedTargetPackageName.ReplaceInline(TEXT("\\"), TEXT("/"));
        FText InvalidPackageReason;
        if (IsMCPDependencyReference(ProposedTargetPath) ||
            IsMCPDependencyReference(ProposedTargetPackageName))
        {
            bTargetPathValid = false;
            Blockers.Add(MakeShared<FJsonValueString>(GetMCPDependencyReferenceError(TEXT("proposed_target_path"), ProposedTargetPath)));
        }
        else if (!FPackageName::IsValidLongPackageName(ProposedTargetPackageName, false, &InvalidPackageReason) ||
            !ProposedTargetPackageName.StartsWith(TEXT("/Game/")))
        {
            bTargetPathValid = false;
            Blockers.Add(MakeShared<FJsonValueString>(FString::Printf(
                TEXT("proposed_target_path must be a valid /Game long package path: %s"),
                *InvalidPackageReason.ToString())));
        }
        else if (!bAllowTempTarget && IsUnderRequiredRoot(ProposedTargetPackageName, TEXT("/Game/_MCP_Temp")))
        {
            bTargetPathValid = false;
            Blockers.Add(MakeShared<FJsonValueString>(TEXT("proposed_target_path is under /Game/_MCP_Temp; promotion target must be outside temp unless allow_temp_target=true")));
        }
    }

    UNiagaraSystem* System = LoadSource(W,SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara System: %s"), *SystemPath));
    }

    FString PackageFilename;
    const FString PackageName = System->GetOutermost() ? System->GetOutermost()->GetName() : SourcePackageName;
    if (!FPackageName::TryConvertLongPackageNameToFilename(PackageName, PackageFilename, FPackageName::GetAssetPackageExtension()) ||
        !FPaths::FileExists(PackageFilename))
    {
        PackageFilename.Empty();
    }

    if(!SourceFileEvidence(W,PackageFilename,false))return W.Error(W.R.Error);
    TArray<FString> AdditionalForbiddenPrefixes = GetStringArrayParam(Params, TEXT("forbidden_dependency_prefixes"));
    AdditionalForbiddenPrefixes.Append(GetStringArrayParam(Params, TEXT("forbidden_source_roots")));
    AdditionalForbiddenPrefixes.Sort();

    const bool bBlockTempDependenciesForPromotion = !bAllowTempDependencies;
    TSharedPtr<FJsonObject> DependencyAudit = BuildNiagaraDependencyAuditJson(
        System,
        PackageFilename,
        bBlockTempDependenciesForPromotion,
        AdditionalForbiddenPrefixes);

    W.R.Data->SetObjectField(TEXT("last_dependency_audit"),DependencyAudit);if(!ContinueRead())return W.Error(W.R.Error);
    const bool bDependencyAuditComplete = DependencyAudit.IsValid() &&
        DependencyAudit->HasTypedField<EJson::Boolean>(TEXT("audit_complete")) &&
        DependencyAudit->GetBoolField(TEXT("audit_complete")) &&
        DependencyAudit->HasTypedField<EJson::Boolean>(TEXT("dependency_query_succeeded")) &&
        DependencyAudit->GetBoolField(TEXT("dependency_query_succeeded"));
    const bool bPersistenceEvidenceComplete = DependencyAudit.IsValid() &&
        DependencyAudit->HasTypedField<EJson::Boolean>(TEXT("saved_file_scanned")) &&
        DependencyAudit->GetBoolField(TEXT("saved_file_scanned"));
    const bool bHasForbiddenDependency = DependencyAudit->GetBoolField(TEXT("has_forbidden_dependency"));
    const bool bHasTempDependency = DependencyAudit->GetBoolField(TEXT("has_temp_dependency"));
    if (!bDependencyAuditComplete)
    {
        Blockers.Add(MakeShared<FJsonValueString>(TEXT("Candidate dependency audit was incomplete")));
    }
    if (!bPersistenceEvidenceComplete)
    {
        Blockers.Add(MakeShared<FJsonValueString>(TEXT("Candidate saved-file dependency evidence was incomplete")));
    }
    if (bHasForbiddenDependency)
    {
        Blockers.Add(MakeShared<FJsonValueString>(TEXT("Candidate has forbidden dependency hits")));
    }
    if (bHasTempDependency)
    {
        Blockers.Add(MakeShared<FJsonValueString>(TEXT("Candidate has /Game/_MCP_Temp dependency hits")));
    }

    const bool bPromotionReady = bTargetPathValid && bDependencyAuditComplete &&
        bPersistenceEvidenceComplete && !bHasForbiddenDependency && !bHasTempDependency;

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("command"), TEXT("audit_niagara_promotion_candidate"));
    Result->SetBoolField(TEXT("read_only"), true);
    Result->SetBoolField(TEXT("moves_assets"), false);
    Result->SetBoolField(TEXT("saves_assets"), false);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetStringField(TEXT("package_name"), PackageName);
    Result->SetBoolField(TEXT("candidate_is_temp_generated"), IsTempGeneratedNiagaraPath(System->GetPathName()));
    Result->SetBoolField(TEXT("has_proposed_target_path"), bHasProposedTargetPath);
    Result->SetStringField(TEXT("proposed_target_path"), ProposedTargetPath);
    Result->SetStringField(TEXT("proposed_target_package_name"), ProposedTargetPackageName);
    Result->SetBoolField(TEXT("proposed_target_path_valid"), bTargetPathValid);
    Result->SetBoolField(TEXT("allow_temp_target"), bAllowTempTarget);
    Result->SetBoolField(TEXT("allow_temp_dependencies"), bAllowTempDependencies);
    Result->SetBoolField(TEXT("dependency_audit_complete"), bDependencyAuditComplete);
    Result->SetBoolField(TEXT("persistence_evidence_complete"), bPersistenceEvidenceComplete);
    Result->SetBoolField(TEXT("promotion_ready"), bPromotionReady);
    Result->SetArrayField(TEXT("promotion_blockers"), Blockers);
    Result->SetArrayField(TEXT("forbidden_dependency_prefixes"), StringArrayToJsonValues(AdditionalForbiddenPrefixes));
    Result->SetObjectField(TEXT("dependency_audit"), DependencyAudit);
    Result->SetStringField(TEXT("policy"), TEXT("Generated Niagara content may be promoted only after no MCP and no temp dependencies remain unless explicitly allowed"));
    if(!SourceFileEvidence(W,PackageFilename,true))return W.Error(W.R.Error);W.TypedPassed=W.Admit(TEXT("audit_candidate_typed_complete"));return Result;
#else
    return W.Error(TEXT("audit_niagara_promotion_candidate requires editor-only Niagara data"));
#endif
}TSharedPtr<FJsonObject> HandlePromote(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITOR
    FString SourceSystemPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("source_system_path"), SourceSystemPath))
    {
        return W.Error(TEXT("Missing 'source_system_path' parameter"));
    }

    FString TargetSystemPath;
    if (!Params->TryGetStringField(TEXT("target_system_path"), TargetSystemPath))
    {
        return W.Error(TEXT("Missing 'target_system_path' parameter"));
    }

    SourceSystemPath.TrimStartAndEndInline();
    TargetSystemPath.TrimStartAndEndInline();
    if (IsMCPDependencyReference(SourceSystemPath))
    {
        return W.Error(GetMCPDependencyReferenceError(TEXT("source_system_path"), SourceSystemPath));
    }

    bool bDryRun = true;
    bool bAllowNonTempSource = false;
    bool bAllowTempTarget = false;
    bool bAllowTempDependencies = false;
    bool bOverwriteExisting = false;
    bool bSave = true;
    bool bRequestCompile = true;
    if (Params.IsValid())
    {
        Params->TryGetBoolField(TEXT("dry_run"), bDryRun);
        Params->TryGetBoolField(TEXT("allow_non_temp_source"), bAllowNonTempSource);
        Params->TryGetBoolField(TEXT("allow_temp_target"), bAllowTempTarget);
        Params->TryGetBoolField(TEXT("allow_temp_dependencies"), bAllowTempDependencies);
        Params->TryGetBoolField(TEXT("overwrite_existing"), bOverwriteExisting);
        Params->TryGetBoolField(TEXT("save"), bSave);
        Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);
    }
    if (bOverwriteExisting)
    {
        return W.Error(TEXT("overwrite_existing is not supported for first-pass Niagara promotion"));
    }

    UNiagaraSystem* SourceSystem = LoadSource(W,SourceSystemPath);
    if (!SourceSystem)
    {
        return W.Error(FString::Printf(TEXT("Failed to load source Niagara System: %s"), *SourceSystemPath));
    }
    if (!bAllowNonTempSource && !IsTempGeneratedNiagaraPath(SourceSystem->GetPathName()))
    {
        return W.Error(FString::Printf(
            TEXT("Promotion source must be under %s unless allow_non_temp_source=true: %s"),
            NiagaraTempGenerationRoot,
            *SourceSystem->GetPathName()));
    }

    FNiagaraAssetTarget Target;
    FString TargetError;
    if (!ResolveNiagaraPromotionTarget(TargetSystemPath, bAllowTempTarget, Target, TargetError))
    {
        return W.Error(TargetError);
    }

    const UPackage* SourcePackage = SourceSystem->GetOutermost();
    const bool bSourcePackageDirty = SourcePackage && SourcePackage->IsDirty();

    FString SourcePackageFilename;
    const FString SourcePackageName = SourcePackage ? SourcePackage->GetName() : PackagePathFromObjectPath(SourceSystemPath);
    if (!FPackageName::TryConvertLongPackageNameToFilename(SourcePackageName, SourcePackageFilename, FPackageName::GetAssetPackageExtension()) ||
        !FPaths::FileExists(SourcePackageFilename))
    {
        SourcePackageFilename.Empty();
    }

    if(!SourceFileEvidence(W,SourcePackageFilename,false))return W.Error(W.R.Error);
    TArray<FString> AdditionalForbiddenPrefixes = GetStringArrayParam(Params, TEXT("forbidden_dependency_prefixes"));
    AdditionalForbiddenPrefixes.Append(GetStringArrayParam(Params, TEXT("forbidden_source_roots")));
    AdditionalForbiddenPrefixes.Sort();

    TSharedPtr<FJsonObject> PreflightAudit = BuildNiagaraDependencyAuditJson(
        SourceSystem,
        SourcePackageFilename,
        !bAllowTempDependencies,
        AdditionalForbiddenPrefixes);
    W.R.Data->SetObjectField(TEXT("preflight_dependency_audit"),PreflightAudit);if(!ContinueRead())return W.Error(W.R.Error);
    const bool bPreflightAuditComplete = PreflightAudit.IsValid() &&
        PreflightAudit->HasTypedField<EJson::Boolean>(TEXT("audit_complete")) &&
        PreflightAudit->GetBoolField(TEXT("audit_complete")) &&
        PreflightAudit->HasTypedField<EJson::Boolean>(TEXT("dependency_query_succeeded")) &&
        PreflightAudit->GetBoolField(TEXT("dependency_query_succeeded"));
    const bool bPreflightPersistenceEvidenceComplete = PreflightAudit.IsValid() &&
        PreflightAudit->HasTypedField<EJson::Boolean>(TEXT("saved_file_scanned")) &&
        PreflightAudit->GetBoolField(TEXT("saved_file_scanned"));
    TArray<TSharedPtr<FJsonValue>> Blockers;
    if (bSourcePackageDirty)
    {
        Blockers.Add(MakeShared<FJsonValueString>(TEXT("Source package is dirty; save or recreate the generated source before promotion")));
    }
    if (!bPreflightAuditComplete)
    {
        Blockers.Add(MakeShared<FJsonValueString>(TEXT("Source dependency audit was incomplete; promotion is blocked")));
    }
    if (!bPreflightPersistenceEvidenceComplete)
    {
        Blockers.Add(MakeShared<FJsonValueString>(TEXT("Source saved-file dependency evidence was incomplete; promotion is blocked")));
    }
    if (PreflightAudit->GetBoolField(TEXT("has_forbidden_dependency")))
    {
        Blockers.Add(MakeShared<FJsonValueString>(TEXT("Source has forbidden dependency hits")));
    }
    if (PreflightAudit->GetBoolField(TEXT("has_temp_dependency")))
    {
        Blockers.Add(MakeShared<FJsonValueString>(TEXT("Source has /Game/_MCP_Temp dependency hits")));
    }
    const bool bTargetIsTemporary = IsUnderRequiredRoot(Target.PackageName, TEXT("/Game/_MCP_Temp"));
    if (!bTargetIsTemporary && !bSave)
    {
        Blockers.Add(MakeShared<FJsonValueString>(TEXT("Non-temporary promotion targets require save=true so durable post-promotion evidence can be verified")));
    }

    const bool bCanPromote = Blockers.Num() == 0;
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("command"), TEXT("promote_niagara_system_candidate"));
    Result->SetBoolField(TEXT("dry_run"), bDryRun);
    Result->SetBoolField(TEXT("can_promote"), bCanPromote);
    Result->SetBoolField(TEXT("source_package_dirty"), bSourcePackageDirty);
    Result->SetStringField(TEXT("source_system_path"), SourceSystem->GetPathName());
    Result->SetStringField(TEXT("target_system_path"), Target.ObjectPath);
    Result->SetStringField(TEXT("target_package_name"), Target.PackageName);
    Result->SetStringField(TEXT("target_filename"), Target.Filename);
    Result->SetBoolField(TEXT("allow_non_temp_source"), bAllowNonTempSource);
    Result->SetBoolField(TEXT("allow_temp_target"), bAllowTempTarget);
    Result->SetBoolField(TEXT("allow_temp_dependencies"), bAllowTempDependencies);
    Result->SetBoolField(TEXT("overwrite_existing"), bOverwriteExisting);
    Result->SetBoolField(TEXT("compile_requested"), bRequestCompile);
    Result->SetBoolField(TEXT("save_requested"), bSave);
    Result->SetBoolField(TEXT("mutation_performed"), false);
    Result->SetBoolField(TEXT("mutation_applied_in_memory"), false);
    Result->SetBoolField(TEXT("preflight_dependency_audit_complete"), bPreflightAuditComplete);
    Result->SetBoolField(TEXT("preflight_persistence_evidence_complete"), bPreflightPersistenceEvidenceComplete);
    Result->SetArrayField(TEXT("promotion_blockers"), Blockers);
    Result->SetObjectField(TEXT("preflight_dependency_audit"), PreflightAudit);

    if (!bCanPromote || bDryRun)
    {
        Result->SetBoolField(TEXT("promoted"), false);
        Result->SetBoolField(TEXT("saved"), false);
        Result->SetBoolField(TEXT("moves_assets"), false);
        Result->SetStringField(TEXT("policy"), TEXT("dry_run or blocked promotion; no asset mutation performed"));
        W.TypedPassed=W.Admit(TEXT("promotion_plan_complete"));if(!SourceFileEvidence(W,SourcePackageFilename,true))return W.Error(W.R.Error);return Result;
    }


    W.Persistent=true;W.RequestedObject=Target.ObjectPath;W.RequestedPackage=Target.PackageName;W.RequestedFilename=Target.Filename;W.R.Data->SetObjectField(TEXT("legacy_partial_result"),Result);
    if(!FreshTarget(W,Target)||!SourceFileEvidence(W,SourcePackageFilename,true))return W.Error(W.R.Error);
    TStrongObjectPtr<UPackage> ReturnedTargetPackage;UPackage* TargetPackage=nullptr;if(!W.Step(TEXT("CreatePackage_Target"),[&]{TargetPackage=CreatePackage(*Target.PackageName);ReturnedTargetPackage.Reset(TargetPackage);auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("requested_package"),Target.PackageName);Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("returned_nonnull"),TargetPackage!=nullptr);W.CreationRows.Add(MakeShared<FJsonValueObject>(Row));W.R.bChanged|=TargetPackage!=nullptr;},true,true))return W.Error(W.R.Error);
    if(!Live(TargetPackage)||!TargetPackage->GetName().Equals(Target.PackageName,ESearchCase::CaseSensitive)||!W.Pin(TargetPackage))return W.Error(TEXT("Exact new target package identity unavailable."));
    if(!W.Step(TEXT("FullyLoad_TargetPackage"),[&]{TargetPackage->FullyLoad();},true,true)||!FreshTarget(W,Target,TargetPackage)||!SourceFileEvidence(W,SourcePackageFilename,true))return W.Error(W.R.Error);
    TStrongObjectPtr<UObject> ReturnedDuplicate;UObject* RawDuplicate=nullptr;UNiagaraSystem* PromotedSystem=nullptr;if(!W.Step(TEXT("StaticDuplicateObject_System"),[&]{RawDuplicate=StaticDuplicateObject(SourceSystem,TargetPackage,FName(*Target.AssetName),RF_Public|RF_Standalone|RF_Transactional);ReturnedDuplicate.Reset(RawDuplicate);PromotedSystem=Cast<UNiagaraSystem>(RawDuplicate);auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("requested_object"),Target.ObjectPath);Row->SetStringField(TEXT("requested_package"),Target.PackageName);Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("returned_nonnull"),RawDuplicate!=nullptr);Row->SetBoolField(TEXT("live_identity_verified"),false);W.CreationRows.Add(MakeShared<FJsonValueObject>(Row));W.R.bChanged|=RawDuplicate!=nullptr;},true,true))return W.Error(W.R.Error);
    if(!Live(PromotedSystem)||PromotedSystem==SourceSystem||PromotedSystem->GetClass()!=SourceSystem->GetClass()||PromotedSystem->GetOuter()!=TargetPackage||PromotedSystem->GetPackage()!=TargetPackage||!PromotedSystem->GetPathName().Equals(Target.ObjectPath,ESearchCase::CaseSensitive)||!W.Pin(PromotedSystem))return W.Error(TEXT("Duplicate returned outside exact requested class/path/package; no replacement adopted."));
    W.Target=PromotedSystem;if(!W.System(PromotedSystem,SourceSystem->GetEmitterHandles().Num())||!PackageOwnership(W,PromotedSystem,TEXT("after_duplicate_package_ownership")))return W.Error(W.R.Error);W.CreationRows.Last()->AsObject()->SetBoolField(TEXT("live_identity_verified"),true);
    if(!W.Step(TEXT("SetFlags_Target"),[&]{PromotedSystem->SetFlags(RF_Public|RF_Standalone|RF_Transactional);},true,true)||!W.Step(TEXT("MarkPackageDirty_Target"),[&]{PromotedSystem->MarkPackageDirty();},true,true)||!W.Step(TEXT("AssetCreated_Target"),[&]{FAssetRegistryModule::AssetCreated(PromotedSystem);},true,true))return W.Error(W.R.Error);
    Result->SetBoolField(TEXT("promoted"),true);Result->SetBoolField(TEXT("mutation_performed"),true);Result->SetBoolField(TEXT("mutation_applied_in_memory"),true);Result->SetBoolField(TEXT("moves_assets"),false);Result->SetBoolField(TEXT("duplicated_asset"),true);Result->SetStringField(TEXT("promoted_system_path"),Target.ObjectPath);
    if(bRequestCompile){if(!W.SDK(TEXT("RequestCompile_Target"),true,true))return W.Error(W.R.Error);W.R.bCompileAttempted=true;W.R.Compiled.Reset();const bool Accepted=PromotedSystem->RequestCompile(false);W.R.Data->SetBoolField(TEXT("compile_request_returned"),true);W.R.Data->SetBoolField(TEXT("compile_request_accepted"),Accepted);W.R.Data->SetBoolField(TEXT("compile_completion_verified"),false);if(!W.After(TEXT("RequestCompile_Target")))return W.Error(W.R.Error);}
    bool bSaved = false;
    FString SavedFilename;
    if (bSave)
    {
        bSaved = SavePromoted(W,PromotedSystem,SavedFilename);
        Result->SetBoolField(TEXT("saved"),W.R.bSaved);if(!W.R.Error.IsEmpty())return W.Error(W.R.Error);
        if (!bSaved)
        {
            const FString ErrorMessage = FString::Printf(
                TEXT("Failed to save promoted Niagara System after in-memory duplication: %s"),
                *PromotedSystem->GetPathName());
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), ErrorMessage);
            Result->SetStringField(TEXT("error_code"), TEXT("niagara_promotion_save_state_unknown"));
            Result->SetBoolField(TEXT("saved"), false);
            Result->SetBoolField(TEXT("mutation_state_unknown"), false);
            Result->SetBoolField(TEXT("save_state_unknown"), true);
            Result->SetBoolField(TEXT("requires_package_reload"), true);
            Result->SetBoolField(TEXT("cleanup_required_unknown"), true);
            Result->SetBoolField(TEXT("do_not_retry"), true);
            Result->SetStringField(TEXT("recovery_policy"), TEXT("reload_then_offline_exact_cleanup_required"));
            return Result;
        }
    }

    TSharedPtr<FJsonObject> PostPromotionAudit = BuildNiagaraDependencyAuditJson(
        PromotedSystem,
        SavedFilename,
        !bAllowTempDependencies,
        AdditionalForbiddenPrefixes);
    W.R.Data->SetObjectField(TEXT("post_promotion_dependency_audit"),PostPromotionAudit);if(!ContinueRead())return W.Error(W.R.Error);
    const bool bPostAuditComplete = PostPromotionAudit.IsValid() &&
        PostPromotionAudit->HasTypedField<EJson::Boolean>(TEXT("audit_complete")) &&
        PostPromotionAudit->GetBoolField(TEXT("audit_complete")) &&
        PostPromotionAudit->HasTypedField<EJson::Boolean>(TEXT("dependency_query_succeeded")) &&
        PostPromotionAudit->GetBoolField(TEXT("dependency_query_succeeded"));
    const bool bPostPersistenceEvidenceComplete = bTargetIsTemporary ||
        (bSave && bSaved &&
            PostPromotionAudit.IsValid() &&
            PostPromotionAudit->HasTypedField<EJson::Boolean>(TEXT("saved_file_scanned")) &&
            PostPromotionAudit->GetBoolField(TEXT("saved_file_scanned")));
    const bool bPostAuditPass =
        bPostAuditComplete &&
        bPostPersistenceEvidenceComplete &&
        !PostPromotionAudit->GetBoolField(TEXT("has_forbidden_dependency")) &&
        !PostPromotionAudit->GetBoolField(TEXT("has_temp_dependency"));

    Result->SetBoolField(TEXT("promoted"), true);
    Result->SetBoolField(TEXT("mutation_performed"), true);
    Result->SetBoolField(TEXT("mutation_applied_in_memory"), true);
    Result->SetBoolField(TEXT("moves_assets"), false);
    Result->SetBoolField(TEXT("duplicated_asset"), true);
    Result->SetStringField(TEXT("promoted_system_path"), PromotedSystem->GetPathName());
    Result->SetBoolField(TEXT("compile_requested"), bRequestCompile);
    Result->SetBoolField(TEXT("save_requested"), bSave);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("saved_filename"), SavedFilename);
    Result->SetBoolField(TEXT("post_promotion_dependency_audit_complete"), bPostAuditComplete);
    Result->SetBoolField(TEXT("post_promotion_persistence_evidence_complete"), bPostPersistenceEvidenceComplete);
    Result->SetBoolField(TEXT("post_promotion_audit_pass"), bPostAuditPass);
    Result->SetObjectField(TEXT("post_promotion_dependency_audit"), PostPromotionAudit);
    Result->SetStringField(TEXT("policy"), TEXT("Promotion duplicates generated Niagara content to a normal project package; source temp asset is left untouched"));
    bool bUnsavedTemporaryDuplicateVerified=false;
    if(!bPostAuditPass && bTargetIsTemporary && !bSave && !bSaved && !bPostAuditComplete
        && bPreflightAuditComplete && bPreflightPersistenceEvidenceComplete && !bSourcePackageDirty
        && !SourcePackageFilename.IsEmpty() && SavedFilename.IsEmpty()
        && !PostPromotionAudit->GetBoolField(TEXT("dependency_query_succeeded"))
        && !PostPromotionAudit->GetBoolField(TEXT("has_forbidden_dependency")) && !PostPromotionAudit->GetBoolField(TEXT("has_temp_dependency"))
        && !IFileManager::Get().FileExists(*Target.Filename) && !FPackageName::DoesPackageExist(Target.PackageName))
    {
        if(!SourceFileEvidence(W,SourcePackageFilename,true)||!PackageOwnership(W,PromotedSystem,TEXT("unsaved_duplicate_ownership")))return W.Error(W.R.Error);
        auto HardReferences=ObserveUnsavedTemporaryHardReferences(W,PromotedSystem,Target,!bAllowTempDependencies,AdditionalForbiddenPrefixes);
        if(!ContinueRead())return W.Error(W.R.Error);
        const bool HardObserved=HardReferences->GetBoolField(TEXT("observations_complete"));
        bool HardForbidden=false,HardTemp=false;HardReferences->TryGetBoolField(TEXT("has_forbidden_dependency"),HardForbidden);HardReferences->TryGetBoolField(TEXT("has_temp_dependency"),HardTemp);
        if(!HardObserved||HardForbidden||HardTemp)
        {
            W.R.Data->SetBoolField(TEXT("unsaved_temporary_duplicate_verified"),false);
            W.Fail(TEXT("niagara_promotion_unsaved_reference_policy_failed"),TEXT("Unsaved temporary duplicate hard-reference observation is incomplete or contains forbidden packages; actual disk-audit failure remains preserved."));return W.Error(W.R.Error);
        }
        if(!SourceFileEvidence(W,SourcePackageFilename,true)||!PackageOwnership(W,PromotedSystem,TEXT("unsaved_duplicate_final_ownership"))
            ||IFileManager::Get().FileExists(*Target.Filename)||FPackageName::DoesPackageExist(Target.PackageName))return W.Error(TEXT("Unsaved temporary target final current/absence evidence unavailable."));
        auto Unsaved=MakeShared<FJsonObject>();Unsaved->SetStringField(TEXT("profile"),TEXT("exact_new_unsaved_temporary_duplicate"));
        Unsaved->SetStringField(TEXT("target_system_path"),Target.ObjectPath);Unsaved->SetStringField(TEXT("target_package_name"),Target.PackageName);Unsaved->SetStringField(TEXT("source_system_path"),SourceSystem->GetPathName());
        for(const TCHAR* Key:{TEXT("observations_complete"),TEXT("source_saved_clean"),TEXT("source_preflight_dependency_audit_complete"),TEXT("source_disk_hash_unchanged"),TEXT("target_file_absent"),TEXT("target_identity_current"),TEXT("target_package_ownership_verified")})Unsaved->SetBoolField(Key,true);
        Unsaved->SetObjectField(TEXT("hard_reference_evidence"),HardReferences);
        Result->SetObjectField(TEXT("unsaved_temporary_duplicate_evidence"),Unsaved);W.R.Data->SetObjectField(TEXT("unsaved_temporary_duplicate_evidence"),Unsaved);
        Result->SetBoolField(TEXT("unsaved_temporary_duplicate_verified"),true);Result->SetBoolField(TEXT("on_disk_dependency_audit_deferred"),true);
        Result->SetBoolField(TEXT("promotion_contract_complete"),false);Result->SetStringField(TEXT("public_success_authority"),TEXT("verified_unsaved_temporary_duplicate"));
        Result->SetStringField(TEXT("deferred_disk_audit_error_code"),TEXT("niagara_promotion_post_dependency_audit_incomplete"));
        Result->SetStringField(TEXT("deferred_disk_audit_error"),TEXT("The new temporary duplicate has no saved file; actual on-disk GetDependencies failed. Current hard-reference observation does not complete the disk audit. A composite that later saves must obtain a real post-save dependency audit."));
        W.R.Data->SetBoolField(TEXT("unsaved_temporary_duplicate_verified"),true);W.R.Data->SetBoolField(TEXT("on_disk_dependency_audit_deferred"),true);
        W.R.Data->SetStringField(TEXT("public_success_authority"),TEXT("verified_unsaved_temporary_duplicate"));
        bUnsavedTemporaryDuplicateVerified=true;
    }
    if (!bPostAuditPass && !bUnsavedTemporaryDuplicateVerified)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(
            TEXT("error"),
            bPostAuditComplete && bPostPersistenceEvidenceComplete
                ? TEXT("Promoted Niagara System failed the post-promotion dependency audit")
                : TEXT("Promoted Niagara System dependency or saved-file audit evidence was incomplete after promotion"));
        Result->SetStringField(
            TEXT("error_code"),
            bPostAuditComplete && bPostPersistenceEvidenceComplete
                ? TEXT("niagara_promotion_post_dependency_audit_failed")
                : TEXT("niagara_promotion_post_dependency_audit_incomplete"));
        Result->SetBoolField(TEXT("mutation_state_unknown"), false);
        Result->SetBoolField(TEXT("save_state_unknown"), false);
        Result->SetBoolField(TEXT("requires_package_reload"), true);
        Result->SetBoolField(TEXT("cleanup_required_unknown"), true);
        Result->SetBoolField(TEXT("do_not_retry"), true);
        Result->SetStringField(
            TEXT("recovery_policy"),
            TEXT("reload_then_offline_exact_cleanup_required"));
    }
    if(!SourceFileEvidence(W,SourcePackageFilename,true)||!PackageOwnership(W,PromotedSystem,TEXT("promotion_final_package_ownership")))return W.Error(W.R.Error);W.TypedPassed=W.Admit(TEXT("promotion_complete"));return Result;
#else
    return W.Error(TEXT("promote_niagara_system_candidate requires editor-only Niagara data"));
#endif
}
FString FinishGT(FWork& W,TSharedPtr<FJsonObject> Legacy,const TCHAR* Operation)
{
 const TSharedPtr<FJsonObject>* Partial=nullptr;bool Leaf=false;if(Legacy)Legacy->TryGetBoolField(TEXT("success"),Leaf);
 if(!Leaf&&W.R.Data->TryGetObjectField(TEXT("legacy_partial_result"),Partial)&&Partial&&Partial->IsValid()){auto Combined=MakeShared<FJsonObject>(**Partial);if(Legacy)for(const auto& Pair:Legacy->Values)Combined->SetField(Pair.Key,Pair.Value);Legacy=Combined;}
 if(W.R.bSaved&&Legacy)Legacy->SetBoolField(TEXT("saved"),true);
 if(!Leaf&&W.R.Error.IsEmpty()){FString Code,Message;if(Legacy){Legacy->TryGetStringField(TEXT("error_code"),Code);Legacy->TryGetStringField(TEXT("error"),Message);}W.Fail(Code.IsEmpty()?TEXT("niagara_promotion_request_failed"):*Code,Message.IsEmpty()?TEXT("Promotion operation failed; actual prefix retained."):Message);}
 bool Complete=false;if(Leaf&&W.TypedPassed&&W.R.Error.IsEmpty())Complete=W.Admit(TEXT("final_typed_admission"));if(Leaf&&(!W.TypedPassed||!Complete)&&W.R.Error.IsEmpty())W.Fail(TEXT("niagara_promotion_postcondition_incomplete"),TEXT("Exact source/target and bounded dependency audit result admission incomplete."));
 W.R.bSuccess=Leaf&&W.TypedPassed&&Complete&&W.R.Error.IsEmpty();W.R.bPostconditionPassed=W.R.bSuccess;if(W.R.bSuccess)W.R.bChangeStateKnown=true;
 W.R.Data->SetObjectField(TEXT("legacy_result"),Legacy?Legacy:MakeShared<FJsonObject>());W.R.Data->SetStringField(TEXT("operation"),Operation);W.R.Data->SetBoolField(TEXT("operation_complete"),true);W.R.Data->SetObjectField(TEXT("native_call_counts"),W.Counts);W.R.Data->SetArrayField(TEXT("actual_sdk_prefix"),W.Calls);W.R.Data->SetArrayField(TEXT("actual_creation_prefix"),W.CreationRows);W.R.Data->SetArrayField(TEXT("actual_save_prefix"),W.SaveRows);W.R.Data->SetObjectField(TEXT("object_dirty_observations"),W.DirtyProjection());W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),W.TypedPassed&&Complete);
 W.R.Data->SetBoolField(TEXT("scoped_target_change_state_known"),W.R.bSuccess);W.R.Data->SetBoolField(TEXT("transitive_dependency_audit_performed"),false);W.R.Data->SetBoolField(TEXT("full_serialized_or_RAM_dependency_audit_performed"),false);W.R.Data->SetBoolField(TEXT("cold_persistence_verified"),false);W.R.Data->SetBoolField(TEXT("automatic_failure_cleanup_implemented"),false);
 W.R.Data->SetStringField(TEXT("audit_scope"),TEXT("Original direct on-disk Package-category AssetRegistry GetDependencies after optional ScanModifiedAssetFiles. saved_file_scanned records that scan returned; no transitive closure, full serialized/RAM reference audit, package cook/pak independence or cold reload is proved."));
 W.R.Data->SetStringField(TEXT("source_preservation_scope"),TEXT("No direct authoring/compile/save is requested on source. Captured class/outer/effective package/path, System/emitter/script/graph/pin routing, selected emitter settings/materials and initial dirty state stay exact across guarded SDK calls. Existing saved source file is bounded-hashed before and after; it is not a full reflective in-memory state equality proof. SDK global effects remain unverified."));
 W.R.Data->SetStringField(TEXT("compile_scope"),TEXT("Original promotion has no wait input. RequestCompile acceptance is distinct from actual completion (compiled null). Original request then save order is preserved. Niagara PreSave/SavePackage or registry SDK may synchronously wait/work internally; bounded wrapper calls are not advertised as nonblocking. No new wait/timeout, sleep, pump, flush or async server was added."));
 W.R.Data->SetStringField(TEXT("save_scope"),TEXT("Input save/request_compile intention is separate from attempts/accepted result/current OR SavePackage true history. SDK true is preserved before Guard and dirty checks; a later failure does not erase a possible disk write. No dirty flag is manually cleared. Package ownership is checked before/after save; SDK callback atomicity and disk TOCTOU are not guaranteed."));
 W.R.Data->SetStringField(TEXT("failure_scope"),TEXT("A new exact package/duplicate can remain after partial SDK failure or failed post-audit/save. Actual nonnull return, compile/save history and original reload/offline cleanup/noRetry fields are retained. No force deletion, overwrite, Undo reset, semantic rollback or automatic replay is attempted."));W.R.Data->SetBoolField(TEXT("residual_target_possible"),!W.R.bSuccess&&W.R.bMutationAttempted);W.R.Stage=W.R.Error.IsEmpty()?TEXT("complete"):W.FailedStage;return Operations::EncodeVariableResult(W.R,Operation);
}
using FGTHandler=TSharedPtr<FJsonObject>(*)(FWork&,const TSharedPtr<FJsonObject>&);
FString RunGT(const FString& Text,const TCHAR* Operation,FGTHandler Handler,bool Query)
{
 FWork W(false);if(!IsInGameThread()){W.Fail(TEXT("niagara_promotion_game_thread_required"),TEXT("Editor game thread required; no SDK dispatch."));return FinishGT(W,W.Error(W.R.Error),Operation);}if(Running){W.Fail(TEXT("niagara_promotion_reentrant_request"),TEXT("Dedicated promotion request already active; nested Native dispatch rejected."));return FinishGT(W,W.Error(W.R.Error),Operation);}TGuardValue<bool> Lease(Running,true);TGuardValue<FWork*> Context(Current,&W);TSharedPtr<FJsonObject> Params;if(!Parse(Text,Params)){W.Fail(TEXT("niagara_promotion_invalid_params_json"),TEXT("Finite JSON object within1Mi UTF16/depth64/262144 tokens/visits, exact case keys and no embedded NUL required."));return FinishGT(W,W.Error(W.R.Error),Operation);}
 if(!Query){bool Save=true,Compile=true,Dry=true;Params->TryGetBoolField(TEXT("save"),Save);Params->TryGetBoolField(TEXT("request_compile"),Compile);Params->TryGetBoolField(TEXT("dry_run"),Dry);W.R.bSaveRequested=Save;W.R.bCompileRequested=Compile;W.Persistent=!Dry;}W.R.Data->SetObjectField(TEXT("requested_params"),MakeShared<FJsonObject>(*Params));if(!W.Admit(TEXT("preflight")))return FinishGT(W,W.Error(W.R.Error),Operation);return FinishGT(W,Handler(W,Params),Operation);
}
}
FString AuditCandidate(const FString& ParamsJson){return RunGT(ParamsJson,TEXT("audit_niagara_promotion_candidate"),&HandleAudit,true);}
FString PromoteCandidate(const FString& ParamsJson){return RunGT(ParamsJson,TEXT("promote_niagara_system_candidate"),&HandlePromote,false);}
}
