// GK dedicated Niagara input parameter APIs. Editor-only candidate; actual Engine build/run unverified.
#include "Compatibility/UE58/CubelessNiagaraInputParameterApi.h"
#include "CubelessNiagaraInputParameterJob.h"
#include "Containers/Ticker.h"
#include "Misc/CoreDelegates.h"
#include <atomic>
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/Crc.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "HAL/PlatformTime.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
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
#include "NiagaraDataInterfaceTexture.h"
#include "NiagaraDataInterfaceArrayFloat.h"
#include "NiagaraDataInterfaceArrayInt.h"
#include "NiagaraParameterMapHistory.h"
#include "NiagaraCommon.h"
#include "EdGraphSchema_Niagara.h"
#include "EdGraphSchema_K2.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
namespace Cubeless::UE58::NiagaraInputParameters
{
namespace
{
using Cubeless::UE58::Variable::FResult;
constexpr const TCHAR* NiagaraTempGenerationRoot=TEXT("/Game/_MCP_Temp/NiagaraGenerated/");
bool Running=false;
bool DeferredLeaseActive=false;std::atomic<bool> DeferredStopping{false};
bool Safe(){return IsInGameThread()&&!IsEngineExitRequested()&&!GExitPurge&&GEditor&&!IsGarbageCollecting()&&!GIsSavingPackage;}
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
FString Json(const TSharedPtr<FJsonObject>& O){FString S;if(O){FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S));}return S;}
FString NormalizeNiagaraObjectPathForLoad(const FString&);
bool Forbidden(const FString& S){const FString V=S.ToLower();return V.Contains(TEXT("unrealmcp"))||V.Contains(TEXT("cubelessmcp"))||V.Contains(TEXT("modelcontextprotocol"))||V.Contains(TEXT("toolsetregistry"));}
FString NodeContent(const UEdGraphNode* N)
{auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("comment"),N->NodeComment);J->SetNumberField(TEXT("x"),N->NodePosX);J->SetNumberField(TEXT("y"),N->NodePosY);TArray<TSharedPtr<FJsonValue>> Ps;for(const UEdGraphPin* P:N->Pins){auto O=MakeShared<FJsonObject>();if(P){O->SetStringField(TEXT("name"),P->PinName.ToString());O->SetStringField(TEXT("friendly"),P->PinFriendlyName.ToString());O->SetStringField(TEXT("tooltip"),P->PinToolTip);O->SetStringField(TEXT("default"),P->DefaultValue);O->SetStringField(TEXT("default_object"),P->DefaultObject?P->DefaultObject->GetPathName():FString());O->SetStringField(TEXT("default_text"),P->DefaultTextValue.ToString());O->SetStringField(TEXT("category"),P->PinType.PinCategory.ToString());O->SetStringField(TEXT("subcategory"),P->PinType.PinSubCategory.ToString());O->SetStringField(TEXT("subcategory_object"),P->PinType.PinSubCategoryObject.IsValid()?P->PinType.PinSubCategoryObject->GetPathName():FString());O->SetNumberField(TEXT("direction"),static_cast<int32>(P->Direction));}Ps.Add(MakeShared<FJsonValueObject>(O));}J->SetArrayField(TEXT("pins"),Ps);if(const auto* F=Cast<UNiagaraNodeFunctionCall>(N)){J->SetStringField(TEXT("function_script"),F->FunctionScript?F->FunctionScript->GetPathName():FString());J->SetStringField(TEXT("function_asset_path"),F->FunctionScriptAssetObjectPath.ToString());J->SetStringField(TEXT("signature"),F->Signature.GetNameString());}return Json(J);}
struct FObjectIdentity
{TStrongObjectPtr<UObject> Hold{nullptr};TWeakObjectPtr<UObject> Object,Outer;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UPackage> Package,EffectivePackage;FString Path;explicit FObjectIdentity(UObject* O):Hold(O),Object(O),Outer(O->GetOuter()),Class(O->GetClass()),Package(O->GetOutermost()),EffectivePackage(O->GetPackage()),Path(O->GetPathName()){}bool Exact()const{auto* O=Object.Get();return Live(O)&&Live(Class.Get())&&Live(Package.Get())&&Live(EffectivePackage.Get())&&!Class->HasAnyClassFlags(CLASS_NewerVersionExists)&&O->GetOuter()==Outer.Get()&&O->GetClass()==Class.Get()&&O->GetOutermost()==Package.Get()&&O->GetPackage()==EffectivePackage.Get()&&O->GetPathName().Equals(Path,ESearchCase::CaseSensitive);}};
struct FPinIdentity{UEdGraphPin* Pin=nullptr;FGuid Id;TArray<UEdGraphPin*> Links;};
struct FNodeIdentity{TWeakObjectPtr<UEdGraphNode> Node;TWeakObjectPtr<UNiagaraScript> FunctionScript;FGuid Id;FString Content;TArray<FPinIdentity> Pins;};
struct FGraphIdentity{TWeakObjectPtr<UNiagaraScriptSource> Source;TWeakObjectPtr<UNiagaraGraph> Graph;TArray<FNodeIdentity> Nodes;};
struct FScriptIdentity{TWeakObjectPtr<UNiagaraScript> Script;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;FGuid Version;};
struct FEmitterIdentity{FGuid Id,InstanceVersion,DataVersion;FString Name;bool Enabled=false;uint8 Mode=0;TWeakObjectPtr<UNiagaraEmitter> Emitter;FVersionedNiagaraEmitterData* Data=nullptr;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;TWeakObjectPtr<UNiagaraScratchPadContainer> Scratch,Parent;TArray<TWeakObjectPtr<UNiagaraScript>> Scripts,ScratchScripts,ParentScripts;};
struct FWork
{
    FResult R;TWeakObjectPtr<UCubelessNiagaraInputParameterJob> OwnedJob;FString OwnedJobId,ExistingRefusalError;bool ExistingRefusalCaptured=false;TWeakObjectPtr<UNiagaraScript> ExistingRefusalScript;FNiagaraVariable ExistingRefusalVariable;TArray<uint8> ExistingRefusalBytes;int32 ExistingRefusalOffset=INDEX_NONE,ExistingRefusalDataNum=0;bool Writer=false,WorldCaptured=false,DeferredMode=false,CancelRequested=false;FString CancelReason;TWeakObjectPtr<UEditorEngine> CapturedEditor;TFunction<bool()> DeferredStep;FString FailedStage;TWeakObjectPtr<UWorld> World;TWeakObjectPtr<UNiagaraSystem> System;TWeakObjectPtr<UNiagaraScript> Spawn,Update;TArray<TWeakObjectPtr<UNiagaraScript>> SystemScratch;TArray<FEmitterIdentity> Emitters;TArray<FGraphIdentity> Graphs;TArray<FScriptIdentity> Scripts;TArray<TUniquePtr<FObjectIdentity>> Objects;TSharedPtr<FJsonObject> Before;TSet<const UEdGraphNode*> ExpectedMutableNodes;TSharedRef<FJsonObject> Mutation=MakeShared<FJsonObject>(),SavePrefix=MakeShared<FJsonObject>();
    struct FExpectedRI{TWeakObjectPtr<UNiagaraScript> Script;FNiagaraVariable Variable;TArray<uint8> Bytes;};struct FExpectedPin{TWeakObjectPtr<UEdGraphNode> Node;UEdGraphPin* Pin=nullptr;FGuid Id;FString Value;};TArray<FExpectedRI> ExpectedRI;TArray<FExpectedPin> ExpectedPins;int64 TotalPinSlots=0;int32 ProjectionObjectLimit=INDEX_NONE;
    TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>(),Transient=MakeShared<FJsonObject>(),Compile=MakeShared<FJsonObject>();
    explicit FWork(bool W):Writer(W){R.Compiled=false;R.Data->SetObjectField(TEXT("actual_mutation_prefix"),Mutation);R.Data->SetObjectField(TEXT("actual_save_prefix"),SavePrefix);R.Data->SetObjectField(TEXT("native_call_counts"),Counts);R.Data->SetObjectField(TEXT("transient_work_prefix"),Transient);R.Data->SetObjectField(TEXT("actual_compile_prefix"),Compile);R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),false);R.Data->SetBoolField(TEXT("source_asset_mutation_attempted"),false);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);}
    bool Fail(const TCHAR* Code,const FString& Message){if(R.Error.IsEmpty()){FailedStage=R.Stage;R.ErrorCode=Code;R.Error=Message;}R.bSuccess=false;return false;}
    TSharedPtr<FJsonObject> Error(const FString& Message){if(R.Error.IsEmpty()){Fail(TEXT("niagara_input_parameters_request_failed"),Message);}auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),false);O->SetStringField(TEXT("error"),Message);return O;}
    bool Pin(UObject* O){if(Objects.Num()>=65536){return Fail(TEXT("niagara_input_parameters_object_bound"),TEXT("Captured object budget65536 exceeded."));}if(!Live(O)){return Fail(TEXT("niagara_input_parameters_object_unavailable"),TEXT("Captured Engine object is not live."));}for(const auto& I:Objects){if(I->Object.Get()==O){return I->Exact();}}Objects.Add(MakeUnique<FObjectIdentity>(O));return true;}
    bool Admit(const FString& Stage,bool FailureObservation=false)
    {
        R.Stage=Stage;if(DeferredMode&&(DeferredStopping.load()||CancelRequested)){return Fail(TEXT("niagara_input_parameters_async_cancelled"),CancelReason.IsEmpty()?TEXT("Owned input writer continuation stopped; no further SDK."):CancelReason);}if(!R.Error.IsEmpty()&&!FailureObservation){return false;}if(!IsInGameThread()||IsEngineExitRequested()||GExitPurge||!GEditor||IsGarbageCollecting()||GIsSavingPackage){return Fail(TEXT("niagara_input_parameters_context_unavailable"),TEXT("Live Editor game thread without GC/exit/save required; no replay."));}if(DeferredMode&&WorldCaptured&&CapturedEditor.Get()!=GEditor){return Fail(TEXT("niagara_input_parameters_editor_changed"),TEXT("Captured Editor identity changed before getter; no replay."));}if(Writer&&(GEditor->GetPIEWorldContext()||GEditor->PlayWorld)){return Fail(TEXT("niagara_input_parameters_play_session_active"),TEXT("Niagara parameter authoring requires stopped PIE/SIE."));}auto* Current=GEditor->GetEditorWorldContext().World();if(!Live(Current)||Current->bIsTearingDown){return Fail(TEXT("niagara_input_parameters_world_unavailable"),TEXT("Editor world unavailable or tearing down."));}if(!WorldCaptured){World=Current;if(DeferredMode)CapturedEditor=GEditor;WorldCaptured=true;}else if(World.Get()!=Current){return Fail(TEXT("niagara_input_parameters_world_changed"),TEXT("Editor world changed across SDK dispatch."));}for(const auto& I:Objects){if(!I->Exact()){return Fail(TEXT("niagara_input_parameters_object_identity_changed"),TEXT("Captured class/outer/package/path changed; replacement was not adopted."));}}
        auto SameScripts=[](const auto* C,const TArray<TWeakObjectPtr<UNiagaraScript>>& Expected){if(!C){return Expected.IsEmpty();}if(C->Scripts.Num()!=Expected.Num()){return false;}for(int32 I=0;I<Expected.Num();++I){if(C->Scripts[I]!=Expected[I].Get()){return false;}}return true;};
        if(System.IsValid())
        {auto* S=System.Get();if(S->GetSystemSpawnScript()!=Spawn.Get()||S->GetSystemUpdateScript()!=Update.Get()||S->ScratchPadScripts.Num()!=SystemScratch.Num()||S->GetEmitterHandles().Num()!=Emitters.Num()){return Fail(TEXT("niagara_input_parameters_system_routing_changed"),TEXT("System script/emitter membership changed."));}for(int32 I=0;I<SystemScratch.Num();++I){if(S->ScratchPadScripts[I]!=SystemScratch[I].Get()){return Fail(TEXT("niagara_input_parameters_system_routing_changed"),TEXT("System scratch script order changed."));}}for(int32 I=0;I<Emitters.Num();++I){const auto& H=S->GetEmitterHandles()[I];const auto& E=Emitters[I];if(H.GetId()!=E.Id||!H.GetName().ToString().Equals(E.Name,ESearchCase::CaseSensitive)||H.GetIsEnabled()!=E.Enabled||static_cast<uint8>(H.GetEmitterMode())!=E.Mode||H.GetInstance().Version!=E.InstanceVersion||H.GetInstance().Emitter!=E.Emitter.Get()||H.GetEmitterData()!=E.Data||(E.Data&&E.Data->Version.VersionGuid!=E.DataVersion)){return Fail(TEXT("niagara_input_parameters_emitter_version_changed"),TEXT("Physical emitter version/ref changed."));}if(E.Data){if(E.Data->GraphSource!=E.Source.Get()||E.Data->ScratchPads!=E.Scratch.Get()||E.Data->ParentScratchPads!=E.Parent.Get()||!SameScripts(E.Scratch.Get(),E.ScratchScripts)||!SameScripts(E.Parent.Get(),E.ParentScripts)){return Fail(TEXT("niagara_input_parameters_emitter_routing_changed"),TEXT("Emitter source/scratch routing changed."));}TArray<UNiagaraScript*> A;E.Data->GetScripts(A,false,false);if(A.Num()!=E.Scripts.Num()){return Fail(TEXT("niagara_input_parameters_emitter_routing_changed"),TEXT("Emitter script count changed."));}for(int32 J=0;J<A.Num();++J){if(A[J]!=E.Scripts[J].Get()){return Fail(TEXT("niagara_input_parameters_emitter_routing_changed"),TEXT("Emitter script order changed."));}}}}}
        for(const auto& S:Scripts){if(!Live(S.Script.Get())||S.Script->GetLatestSource()!=S.Source.Get()||S.Script->GetExposedVersion().VersionGuid!=S.Version){return Fail(TEXT("niagara_input_parameters_script_source_changed"),TEXT("Script source/exposed version changed."));}}
        for(const auto& G:Graphs)
        {if(!Live(G.Source.Get())||G.Source->NodeGraph!=G.Graph.Get()||!Live(G.Graph.Get())||G.Graph->Nodes.Num()!=G.Nodes.Num()){return Fail(TEXT("niagara_input_parameters_graph_changed"),TEXT("Source graph/node membership changed."));}for(int32 I=0;I<G.Nodes.Num();++I){const auto& E=G.Nodes[I];auto* N=E.Node.Get();if(G.Graph->Nodes[I]!=N){return Fail(TEXT("niagara_input_parameters_node_changed"),TEXT("Physical node order changed."));}if(!N){continue;}if(!Live(N)||N->GetGraph()!=G.Graph.Get()||N->NodeGuid!=E.Id||N->Pins.Num()!=E.Pins.Num()){return Fail(TEXT("niagara_input_parameters_node_changed"),TEXT("Captured node/pin identity changed."));}if(const auto* F=Cast<UNiagaraNodeFunctionCall>(N)){if(F->FunctionScript!=E.FunctionScript.Get()){return Fail(TEXT("niagara_input_parameters_function_script_changed"),TEXT("Selected graph function script pointer changed."));}}for(int32 J=0;J<E.Pins.Num();++J){auto* P=N->Pins[J];const auto& EP=E.Pins[J];if(P!=EP.Pin||(P&&(P->PinId!=EP.Id||P->GetOwningNodeUnchecked()!=N||P->LinkedTo!=EP.Links))){return Fail(TEXT("niagara_input_parameters_pin_changed"),TEXT("Exact pin identity/links changed; stale pointer not dereferenced."));}}if(!ExpectedMutableNodes.Contains(N)&&!NodeContent(N).Equals(E.Content,ESearchCase::CaseSensitive)){return Fail(TEXT("niagara_input_parameters_node_content_changed"),TEXT("Source node content changed during observation."));}}}
        return true;
    }
    bool SDK(const TCHAR* Stage,bool IsMutation=false){if(!Admit(Stage)){return false;}R.bChangeStateKnown=false;R.bMutationAttempted|=IsMutation;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);double N=0;Counts->TryGetNumberField(Stage,N);Counts->SetNumberField(Stage,N+1);return true;}
    bool After(const TCHAR* Stage){return Admit(FString(TEXT("after_"))+Stage);}
    bool Graph(UNiagaraScriptSourceBase* Base)
    {
        auto* S=Cast<UNiagaraScriptSource>(Base);if(!S||!S->NodeGraph){return true;}auto* G=S->NodeGraph.Get();
        for(const auto& Captured:Graphs){if(Captured.Source.Get()==S&&Captured.Graph.Get()==G){return true;}}
        if(!Pin(S)||!Pin(G)||G->Nodes.Num()>16384){return Fail(TEXT("niagara_input_parameters_graph_bound"),TEXT("Graph inventory unavailable or exceeds bound."));}
        FGraphIdentity E;E.Source=S;E.Graph=G;int32 Slots=0;TSet<UEdGraphPin*> KnownPins;
        for(UEdGraphNode* N:G->Nodes)
        {
            if(!N||!Pin(N)||N->GetGraph()!=G){return Fail(TEXT("niagara_input_parameters_malformed_graph"),TEXT("Null/foreign graph node cannot be safely inspected; no source writer dispatched."));}
            FNodeIdentity NE;NE.Node=N;NE.Id=N->NodeGuid;
            for(UEdGraphPin* P:N->Pins)
            {
                if(++Slots>262144||++TotalPinSlots>262144){return Fail(TEXT("niagara_input_parameters_pin_bound"),TEXT("Graph pin bound exceeded."));}
                if(!P||P->GetOwningNodeUnchecked()!=N||KnownPins.Contains(P)){return Fail(TEXT("niagara_input_parameters_malformed_graph"),TEXT("Null/foreign/duplicate pin slot cannot be safely inspected; no source writer dispatched."));}
                if(P->LinkedTo.Num()>262144){return Fail(TEXT("niagara_input_parameters_link_bound"),TEXT("Pin link inventory exceeds bound."));}
                FPinIdentity PE;PE.Pin=P;PE.Id=P->PinId;PE.Links=P->LinkedTo;NE.Pins.Add(MoveTemp(PE));KnownPins.Add(P);
            }
            E.Nodes.Add(MoveTemp(NE));
        }
        // Check raw links by pointer membership before dereferencing linked pin/owner in original helpers.
        int64 LinkSlots=0;for(const auto& N:E.Nodes){for(const auto& P:N.Pins){for(auto* L:P.Links){if(++LinkSlots>262144){return Fail(TEXT("niagara_input_parameters_link_bound"),TEXT("Graph link inventory exceeds bound."));}if(!L||!KnownPins.Contains(L)){return Fail(TEXT("niagara_input_parameters_unresolved_link_identity"),TEXT("Null/cross-graph link is outside exact captured pin membership; no stale link dereference."));}}}}
        for(auto& NE:E.Nodes){auto* N=NE.Node.Get();if(auto* F=Cast<UNiagaraNodeFunctionCall>(N)){NE.FunctionScript=F->FunctionScript;if(F->FunctionScript&&!Pin(F->FunctionScript)){return false;}}NE.Content=NodeContent(N);}
        Graphs.Add(MoveTemp(E));return true;
    }
    bool Script(UNiagaraScript* S)
    {if(!S){return true;}for(const auto& E:Scripts){if(E.Script.Get()==S){return true;}}if(!Pin(S)){return false;}FScriptIdentity E;E.Script=S;E.Source=S->GetLatestSource();E.Version=S->GetExposedVersion().VersionGuid;Scripts.Add(E);return Graph(E.Source.Get());}
    UNiagaraSystem* LoadSystem(const FString& Path)
    {if(Forbidden(Path)){Fail(TEXT("niagara_input_parameters_runtime_dependency_rejected"),TEXT("MCP/Cubeless/Toolset content reference forbidden."));return nullptr;}if(!SDK(TEXT("load_system"))){return nullptr;}const FString Expected=NormalizeNiagaraObjectPathForLoad(Path);auto* S=LoadObject<UNiagaraSystem>(nullptr,*Expected);R.Data->SetBoolField(TEXT("load_system_returned_nonnull"),S!=nullptr);if(!After(TEXT("load_system"))){return nullptr;}if(!S){return nullptr;}if(!S->GetPathName().Equals(Expected,ESearchCase::CaseSensitive)||!Pin(S)||!Pin(S->GetOutermost())){Fail(TEXT("niagara_input_parameters_loaded_identity_unexpected"),TEXT("Loaded System does not match exact normalized path."));return nullptr;}System=S;Spawn=S->GetSystemSpawnScript();Update=S->GetSystemUpdateScript();if(!Script(Spawn.Get())||!Script(Update.Get())){return nullptr;}for(UNiagaraScript* P:S->ScratchPadScripts){SystemScratch.Add(P);if(!Script(P)){return nullptr;}}if(S->GetEmitterHandles().Num()>4096){Fail(TEXT("niagara_input_parameters_emitter_bound"),TEXT("Emitter bound exceeded."));return nullptr;}for(const auto& H:S->GetEmitterHandles()){FEmitterIdentity E;E.Id=H.GetId();E.Name=H.GetName().ToString();E.Enabled=H.GetIsEnabled();E.Mode=static_cast<uint8>(H.GetEmitterMode());E.InstanceVersion=H.GetInstance().Version;E.Emitter=H.GetInstance().Emitter;E.Data=H.GetEmitterData();if(E.Data){if(!Pin(E.Emitter.Get())){return nullptr;}E.DataVersion=E.Data->Version.VersionGuid;E.Source=E.Data->GraphSource;E.Scratch=E.Data->ScratchPads;E.Parent=E.Data->ParentScratchPads;if(!Graph(E.Source.Get())){return nullptr;}TArray<UNiagaraScript*> ES;E.Data->GetScripts(ES,false,false);if(ES.Num()>8192){Fail(TEXT("niagara_input_parameters_script_bound"),TEXT("Emitter script budget8192 exceeded."));return nullptr;}for(auto* P:ES){E.Scripts.Add(P);if(!Script(P)){return nullptr;}}if(E.Scratch.IsValid()){if(!Pin(E.Scratch.Get())){return nullptr;}for(UNiagaraScript* P:E.Scratch->Scripts){E.ScratchScripts.Add(P);if(!Script(P)){return nullptr;}}}if(E.Parent.IsValid()){if(!Pin(E.Parent.Get())){return nullptr;}for(UNiagaraScript* P:E.Parent->Scripts){E.ParentScripts.Add(P);if(!Script(P)){return nullptr;}}}}Emitters.Add(MoveTemp(E));}ProjectionObjectLimit=Objects.Num();Before=Projection();return R.Error.IsEmpty()?S:nullptr;}
    TSharedPtr<FJsonObject> Projection()
    {
        auto O=MakeShared<FJsonObject>();if(!Admit(TEXT("current_target_projection"),true)){O->SetBoolField(TEXT("projection_skipped_unsafe"),true);return O;}
        TArray<TSharedPtr<FJsonValue>> Rows;int64 Slots=0;
        int32 ProjectionIndex=0;for(const auto& I:Objects){if(ProjectionObjectLimit!=INDEX_NONE&&ProjectionIndex++>=ProjectionObjectLimit){break;}auto* A=I->Object.Get();if(A->GetOutermost()==GetTransientPackage()){continue;}auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),I->Path);J->SetStringField(TEXT("package"),A->GetOutermost()->GetName());J->SetBoolField(TEXT("dirty"),A->GetOutermost()->IsDirty());Rows.Add(MakeShared<FJsonValueObject>(J));}O->SetArrayField(TEXT("objects"),Rows);
        TArray<TSharedPtr<FJsonValue>> Stores;
        for(const auto& Entry:Scripts){auto* S=Entry.Script.Get();auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("script"),S->GetPathName());TArray<FNiagaraVariable> Params;S->RapidIterationParameters.GetParameters(Params);if(Params.Num()>20000){Fail(TEXT("niagara_input_parameters_store_bound"),TEXT("RI store exceeds20000 fields."));return O;}TArray<TSharedPtr<FJsonValue>> Values;
            for(const auto& V:Params){if(++Slots>262144){Fail(TEXT("niagara_input_parameters_projection_bound"),TEXT("RI projection bound exceeded."));return O;}auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("name"),V.GetName().ToString());P->SetStringField(TEXT("type"),V.GetType().GetNameText().ToString());const auto& Store=S->RapidIterationParameters;const int32 Offset=Store.IndexOf(V),Size=V.GetSizeInBytes();const auto& Bytes=Store.GetParameterDataArray();const bool ValidData=Offset>=0&&Size>0&&Size<=65536&&Offset<=Bytes.Num()-Size;P->SetBoolField(TEXT("has_value"),ValidData);FString Hex;if(ValidData){if((Slots+=Size)>4*1024*1024){Fail(TEXT("niagara_input_parameters_projection_byte_bound"),TEXT("RI raw projection exceeds4MiB; no further writer work."));return O;}Hex=BytesToHex(Bytes.GetData()+Offset,Size);}P->SetStringField(TEXT("bytes_hex"),Hex);Values.Add(MakeShared<FJsonValueObject>(P));}
            Row->SetArrayField(TEXT("parameters"),Values);Stores.Add(MakeShared<FJsonValueObject>(Row));}O->SetArrayField(TEXT("RI_stores"),Stores);
        TArray<TSharedPtr<FJsonValue>> Contents;for(const auto& G:Graphs){for(const auto& N:G.Nodes){auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("path"),N.Node->GetPathName());Row->SetStringField(TEXT("content"),NodeContent(N.Node.Get()));Contents.Add(MakeShared<FJsonValueObject>(Row));}}O->SetArrayField(TEXT("graph_content"),Contents);O->SetBoolField(TEXT("projection_complete"),true);return O;
    }
    bool WriterTarget(UObject* O)
    {
        if(!Admit(TEXT("before_writer_target"))||!Live(System.Get())||!Pin(O)){return false;}auto* TargetSystem=System.Get();
        if((O!=TargetSystem&&!O->IsIn(TargetSystem))||O->GetOutermost()!=TargetSystem->GetOutermost()||O->GetPackage()!=TargetSystem->GetPackage())
        {return Fail(TEXT("niagara_input_parameters_external_writer_unsupported"),TEXT("Writer must be the exact selected System or its contained object with the same actual and outermost package; sibling/external source variants remain incomplete. No writer dispatched."));}
        if(O==TargetSystem){return true;}
        auto* TargetScript=Cast<UNiagaraScript>(O);UNiagaraGraph* TargetGraph=Cast<UNiagaraGraph>(O);
        auto GraphFromSource=[](UNiagaraScriptSourceBase* Base)->UNiagaraGraph*{auto* Source=Cast<UNiagaraScriptSource>(Base);return Source?Source->NodeGraph.Get():nullptr;};
        if(auto* N=Cast<UEdGraphNode>(O)){TargetGraph=Cast<UNiagaraGraph>(N->GetGraph());}else if(TargetScript){TargetGraph=GraphFromSource(TargetScript->GetLatestSource());}else if(auto* Source=Cast<UNiagaraScriptSource>(O)){TargetGraph=Source->NodeGraph.Get();}
        for(const auto& E:Emitters){if(O==E.Emitter.Get()){TargetGraph=GraphFromSource(E.Source.Get());break;}}
        int32 EmitterRoles=0;TSet<UNiagaraScriptSource*> SharingSources;
        for(const auto& E:Emitters)
        {
            bool Uses=O==E.Emitter.Get()||O==E.Source.Get()||(E.Emitter.IsValid()&&O->IsIn(E.Emitter.Get()));
            auto UsesScript=[&](const TWeakObjectPtr<UNiagaraScript>& S){return (TargetScript&&S.Get()==TargetScript)||(TargetGraph&&S.IsValid()&&GraphFromSource(S->GetLatestSource())==TargetGraph);};
            for(const auto& S:E.Scripts){Uses|=UsesScript(S);}for(const auto& S:E.ScratchScripts){Uses|=UsesScript(S);}for(const auto& S:E.ParentScripts){Uses|=UsesScript(S);}
            if(TargetGraph&&GraphFromSource(E.Source.Get())==TargetGraph){Uses=true;if(auto* Source=Cast<UNiagaraScriptSource>(E.Source.Get())){SharingSources.Add(Source);}}
            if(Uses){++EmitterRoles;}
        }
        bool SystemRole=false;for(const auto& S:Scripts){if(S.Script.Get()==Spawn.Get()||S.Script.Get()==Update.Get())
        {if(TargetScript&&S.Script.Get()==TargetScript){SystemRole=true;}if(TargetGraph&&GraphFromSource(S.Source.Get())==TargetGraph){SystemRole=true;}}if(TargetGraph&&GraphFromSource(S.Source.Get())==TargetGraph){if(auto* Source=Cast<UNiagaraScriptSource>(S.Source.Get())){SharingSources.Add(Source);}}}
        if(EmitterRoles>1||(EmitterRoles>0&&SystemRole)||SharingSources.Num()>1)
        {return Fail(TEXT("niagara_input_parameters_shared_writer_unsupported"),TEXT("Selected Script/Graph is shared by multiple emitter/source/System roles; refuse before first Modify so an unselected owner is not modified."));}
        return true;
    }
    bool Modify(UObject* O,const TCHAR* Role)
    {if(!WriterTarget(O)||!SDK(Role,true)){return false;}Mutation->SetBoolField(FString(Role)+TEXT("_attempted"),true);O->Modify();Mutation->SetBoolField(FString(Role)+TEXT("_returned"),true);R.Data->SetBoolField(TEXT("source_asset_mutation_attempted"),true);return After(Role);}
    bool Dirty()
    {if(!SDK(TEXT("MarkPackageDirty"),true)){return false;}const bool Returned=System->MarkPackageDirty();Mutation->SetBoolField(TEXT("MarkPackageDirty_returned"),Returned);return After(TEXT("MarkPackageDirty"));}
    bool SetParameter(UNiagaraScript* S,const FNiagaraVariable& V,bool Add)
    {if(!WriterTarget(S)){return false;}Mutation->SetStringField(TEXT("RI_script_path"),S->GetPathName());Mutation->SetStringField(TEXT("RI_variable_name"),V.GetName().ToString());Mutation->SetStringField(TEXT("RI_variable_type"),V.GetType().GetNameText().ToString());Mutation->SetStringField(TEXT("RI_expected_bytes_hex"),V.GetAllocatedSizeInBytes()>0?BytesToHex(V.GetData(),V.GetAllocatedSizeInBytes()):FString());if(!SDK(TEXT("SetParameterData"),true)){return false;}const bool Returned=S->RapidIterationParameters.SetParameterData(V.GetData(),V,Add);Mutation->SetBoolField(TEXT("SetParameterData_returned"),Returned);Mutation->SetBoolField(TEXT("SetParameterData_add"),Add);if(!After(TEXT("SetParameterData"))){return false;}const auto& Store=S->RapidIterationParameters;const int32 Offset=Store.IndexOf(V),Size=V.GetSizeInBytes();const auto& Bytes=Store.GetParameterDataArray();const bool Equal=Offset>=0&&Size>0&&Size<=65536&&Offset<=Bytes.Num()-Size&&V.GetAllocatedSizeInBytes()>=Size&&FMemory::Memcmp(Bytes.GetData()+Offset,V.GetData(),Size)==0;Mutation->SetBoolField(TEXT("SetParameterData_exact_readback"),Equal);if(Returned&&Equal){FExpectedRI E;E.Script=S;E.Variable=V;E.Bytes.Append(V.GetData(),Size);ExpectedRI.Add(MoveTemp(E));}return (Returned&&Equal)||Fail(TEXT("niagara_input_parameters_RI_readback_failed"),TEXT("Returned RI writer/readback failed; actual prefix retained."));}
    bool Save(bool& Saved)
    {
        R.bSaveRequested=true;if(!Admit(TEXT("before_save_identity"))||!SDK(TEXT("SaveLoadedAsset"))){return false;}R.bSaveAttempted=true;SavePrefix->SetBoolField(TEXT("SDK_attempted"),true);
        const bool Returned=UEditorAssetLibrary::SaveLoadedAsset(System.Get(),false);Saved=Returned;R.bSaved|=Returned;SavePrefix->SetBoolField(TEXT("SDK_returned"),true);SavePrefix->SetBoolField(TEXT("saved"),Returned);
        if(!After(TEXT("SaveLoadedAsset"))){return false;}const bool DirtyAfter=System->GetOutermost()->IsDirty();SavePrefix->SetBoolField(TEXT("target_dirty_after"),DirtyAfter);
        if(!Returned){return Fail(TEXT("niagara_input_parameters_save_failed"),TEXT("Actual SaveLoadedAsset returnedFalse; no replay."));}
        if(DirtyAfter){return Fail(TEXT("niagara_input_parameters_saved_target_still_dirty"),TEXT("SDK save returnedTrue but the exact target package remains dirty; savedTrue history retained and no replay."));}
        return true;
    }
    bool ExpectedReadback()
    {if(!Admit(TEXT("final_selected_value_readback"))){return false;}for(const auto& E:ExpectedRI){auto* S=E.Script.Get();if(!Live(S)){return Fail(TEXT("niagara_input_parameters_selected_script_lost"),TEXT("Selected Script unavailable after writer."));}const auto& Store=S->RapidIterationParameters;const int32 Offset=Store.IndexOf(E.Variable);const auto& Bytes=Store.GetParameterDataArray();const int32 Size=E.Bytes.Num();const bool Same=Offset>=0&&Size>0&&Offset<=Bytes.Num()-Size&&FMemory::Memcmp(Bytes.GetData()+Offset,E.Bytes.GetData(),Size)==0;Mutation->SetBoolField(TEXT("final_selected_RI_exact"),Same);if(!Same){return Fail(TEXT("niagara_input_parameters_final_RI_changed"),TEXT("Selected RI bytes changed after compiler/save; actual prefix retained."));}}for(const auto& E:ExpectedPins){auto* N=E.Node.Get();if(!Live(N)||!N->Pins.Contains(E.Pin)||E.Pin->PinId!=E.Id||!E.Pin->DefaultValue.Equals(E.Value,ESearchCase::CaseSensitive)){return Fail(TEXT("niagara_input_parameters_final_static_pin_changed"),TEXT("Selected static pin changed after compiler/save; actual prefix retained."));}}return true;}
    UObject* Mutable(const UObject* O){for(const auto& I:Objects){if(I->Object.Get()==O){return I->Object.Get();}}return nullptr;}
    bool Request(bool Force,bool& Accepted)
    {R.bCompileRequested=true;if(!SDK(TEXT("RequestCompile"),true)){return false;}R.bCompileAttempted=true;R.Compiled.Reset();Compile->SetBoolField(TEXT("attempted"),true);Accepted=System->RequestCompile(Force);Compile->SetBoolField(TEXT("SDK_returned"),true);Compile->SetBoolField(TEXT("accepted"),Accepted);Compile->SetBoolField(TEXT("completion_verified"),false);return After(TEXT("RequestCompile"));}
};
FWork* Current=nullptr;
bool ContinueRead(){return Current&&Current->Admit(TEXT("read_continuation"));}
FString ObserveNodeText(const UEdGraphNode* N,bool Tooltip)
{if(!Current||!Current->SDK(Tooltip?TEXT("node_GetTooltipText"):TEXT("node_GetNodeTitle"))){return FString();}const FString S=Tooltip?N->GetTooltipText().ToString():N->GetNodeTitle(ENodeTitleType::ListView).ToString();if(!Current->After(Tooltip?TEXT("node_GetTooltipText"):TEXT("node_GetNodeTitle"))){return FString();}return S;}



bool ClassPathOrChild(const UClass* C,const TCHAR* Path){for(;C;C=C->GetSuperClass()){if(C->GetPathName().Equals(Path,ESearchCase::CaseSensitive)){return true;}}return false;}
const uint8* ReadRapid(UNiagaraScript* S,const FNiagaraVariableBase& V)
{if(!Current||!Current->Admit(TEXT("read_RI_bytes"))||!Live(S)){return nullptr;}const auto& Store=S->RapidIterationParameters;const int32 Offset=Store.IndexOf(V),Size=V.GetSizeInBytes();const auto& Bytes=Store.GetParameterDataArray();return Offset>=0&&Size>0&&Size<=65536&&Offset<=Bytes.Num()-Size?Bytes.GetData()+Offset:nullptr;}
bool QueryInputs(const UNiagaraNodeFunctionCall& Node,TArray<FNiagaraVariable>& Inputs,TSet<FNiagaraVariable>& Hidden,FCompileConstantResolver Resolver,FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions Options,bool Ignore)
{if(!Current||!Current->SDK(TEXT("GetStackFunctionInputs"))){return false;}FNiagaraStackGraphUtilities::GetStackFunctionInputs(Node,Inputs,Hidden,Resolver,Options,Ignore);if(!Current->After(TEXT("GetStackFunctionInputs"))){return false;}if(Inputs.Num()>20000){return Current->Fail(TEXT("niagara_input_parameters_input_bound"),TEXT("Resolved input count exceeds20000."));}return true;}
bool QueryStaticPins(const UNiagaraNodeFunctionCall& Node,TArray<UEdGraphPin*>& Inputs,TSet<UEdGraphPin*>& Hidden,FCompileConstantResolver& Resolver)
{if(!Current||!Current->SDK(TEXT("GetStackFunctionStaticSwitchPins"))){return false;}FNiagaraStackGraphUtilities::GetStackFunctionStaticSwitchPins(Node,Inputs,Hidden,Resolver);return Current->After(TEXT("GetStackFunctionStaticSwitchPins"));}
TSharedPtr<FJsonObject> WaitForNiagaraCompileJson(UNiagaraSystem* S,bool Wait,double Timeout,double Poll)
{auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("wait_for_completion"),Wait);O->SetNumberField(TEXT("timeout_seconds"),FMath::Clamp(Timeout,0.0,60.0));O->SetNumberField(TEXT("poll_interval_seconds"),FMath::Clamp(Poll,0.02,1.0));O->SetBoolField(TEXT("wait_timed_out"),false);O->SetNumberField(TEXT("wait_iterations"),0);O->SetNumberField(TEXT("wait_elapsed_seconds"),0);O->SetBoolField(TEXT("blocking_poll_or_pump_invoked"),false);if(!Current||!Current->SDK(TEXT("compile_wait_point_observation"))){return O;}const bool Pending=S->HasOutstandingCompilationRequests(true),Active=S->HasActiveCompilations();O->SetBoolField(TEXT("outstanding_compilation_requests_after"),Pending);O->SetBoolField(TEXT("active_compilations_after"),Active);if(!Current->After(TEXT("compile_wait_point_observation"))){return O;}if(Wait){Current->Fail(TEXT("niagara_input_parameters_async_wait_unsupported"),TEXT("Actual deferred compile completion/save continuation is not implemented; no fake timeout or waitFalse substitution."));O->SetStringField(TEXT("wait_status"),TEXT("unsupported"));}else{O->SetStringField(TEXT("wait_status"),TEXT("not_requested_point_observation_only"));}return O;}

struct FInputCompileWait
{
 TSharedPtr<FJsonObject> Result=MakeShared<FJsonObject>();
 double Started=0,Next=0,Timeout=0,Poll=.1;int32 Iterations=0;bool Done=false;
 FInputCompileWait(double Seconds,double Interval):Started(FPlatformTime::Seconds()),Timeout(FMath::Clamp(Seconds,0.0,60.0)),Poll(FMath::Clamp(Interval,.02,1.0))
 {Result->SetBoolField(TEXT("wait_for_completion"),true);Result->SetNumberField(TEXT("timeout_seconds"),Timeout);Result->SetNumberField(TEXT("poll_interval_seconds"),Poll);Result->SetBoolField(TEXT("wait_timed_out"),false);Result->SetBoolField(TEXT("blocking_poll_or_pump_invoked"),false);Result->SetBoolField(TEXT("finite_GT_ticker_wait"),true);}
 bool Tick(FWork& W,UNiagaraSystem* System)
 {
  if(Done)return true;
  const double Now=FPlatformTime::Seconds();if(Now<Next)return false;
  bool Outstanding=false,Active=false;
  auto Observe=[&]()
  {
   if(!W.SDK(TEXT("async_wait_HasOutstandingCompilationRequests")))return false;
   Outstanding=System->HasOutstandingCompilationRequests(true);Result->SetBoolField(TEXT("outstanding_compilation_requests_after"),Outstanding);
   if(!W.After(TEXT("async_wait_HasOutstandingCompilationRequests")))return false;
   if(!W.SDK(TEXT("async_wait_HasActiveCompilations")))return false;
   Active=System->HasActiveCompilations();Result->SetBoolField(TEXT("active_compilations_after"),Active);
   return W.After(TEXT("async_wait_HasActiveCompilations"));
  };
  if(!Observe()){Done=true;return true;}
  if(Outstanding||Active)
  {
   if(!W.SDK(TEXT("async_wait_PollForCompilationComplete"))){Done=true;return true;}
   System->PollForCompilationComplete();
   if(!W.After(TEXT("async_wait_PollForCompilationComplete"))||!Observe()){Done=true;return true;}
  }
  const double Elapsed=FPlatformTime::Seconds()-Started;
  const bool TimedOut=(Outstanding||Active)&&Elapsed>=Timeout;
  Result->SetNumberField(TEXT("wait_elapsed_seconds"),Elapsed);Result->SetBoolField(TEXT("wait_timed_out"),TimedOut);
  if((!Outstanding&&!Active)||TimedOut){Done=true;Result->SetNumberField(TEXT("wait_iterations"),Iterations);Result->SetStringField(TEXT("wait_status"),TimedOut?TEXT("bounded_timeout_observed"):TEXT("idle_observed"));return true;}
  ++Iterations;Result->SetNumberField(TEXT("wait_iterations"),Iterations);
  Next=FPlatformTime::Seconds()+Poll;return false;
 }
};
void BeginInputWait(FWork& W,UNiagaraSystem* System,double Timeout,double Poll,TFunction<TSharedPtr<FJsonObject>(const TSharedPtr<FJsonObject>&)> Completion)
{
 auto Wait=MakeShared<FInputCompileWait>(Timeout,Poll);
 W.R.Data->SetObjectField(TEXT("compile_wait_partial"),Wait->Result);
 W.DeferredStep=[&W,System,Wait,Completion=MoveTemp(Completion)]()mutable
 {
  if(!Wait->Tick(W,System))return false;
  if(!W.Admit(TEXT("input_wait_completion_admission")))return true;
  TSharedPtr<FJsonObject> Legacy=Completion(Wait->Result);
  if(Legacy.IsValid())W.R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
  return true;
 };
}

struct FNiagaraScriptCompileSnapshot{TObjectPtr<UNiagaraScript> Script=nullptr;FString OwnerKind,OwnerName,ScriptPath;FNiagaraVMExecutableDataId CachedId,ComputedId;ENiagaraScriptCompileStatus CompileStatus=ENiagaraScriptCompileStatus::NCS_Unknown;};
TSharedPtr<FJsonObject> BuildObjectPropertySnapshotJson(UObject* Object,int32 MaxProperties,int32 MaxPropertyValueLength)
{
    auto Out=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Values;int32 PropertyCount=0,Eligible=0,Transient=0,NonEditable=0,Truncated=0;const int32 Max=FMath::Clamp(MaxProperties,0,500),Length=FMath::Clamp(MaxPropertyValueLength,64,4096);
    struct FField{FName Name;const FProperty* Identity=nullptr;TWeakObjectPtr<UStruct> Owner;FString Type;};TArray<FField> Fields;
    if(Object&&Current&&Current->Pin(Object)){
        for(TFieldIterator<FProperty> It(Object->GetClass(),EFieldIteratorFlags::IncludeSuper);It;++It){const FProperty* P=*It;if(!P){continue;}++PropertyCount;if(P->HasAnyPropertyFlags(CPF_Transient)){++Transient;continue;}if(!P->HasAnyPropertyFlags(CPF_Edit)){++NonEditable;continue;}++Eligible;if(Fields.Num()<Max){FField F;F.Name=P->GetFName();F.Identity=P;F.Owner=P->GetOwnerStruct();F.Type=P->GetCPPType();Fields.Add(MoveTemp(F));}}
        for(const auto& F:Fields){if(!Current->SDK(TEXT("DI_ExportText"))){break;}const FProperty* P=FindFProperty<FProperty>(Object->GetClass(),F.Name);if(P!=F.Identity||P->GetOwnerStruct()!=F.Owner.Get()){Current->Fail(TEXT("niagara_input_parameters_DI_field_identity_changed"),TEXT("DI field layout changed before export."));break;}FString Value;P->ExportText_InContainer(0,Value,Object,Object,Object,PPF_None);if(!Current->After(TEXT("DI_ExportText"))){break;}const FProperty* After=FindFProperty<FProperty>(Object->GetClass(),F.Name);if(After!=F.Identity||After->GetOwnerStruct()!=F.Owner.Get()){Current->Fail(TEXT("niagara_input_parameters_DI_field_identity_changed"),TEXT("DI field layout changed during export; actual observation prefix retained."));break;}const bool Cut=Value.Len()>Length;if(Cut){Value.LeftInline(Length);++Truncated;}auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),F.Name.ToString());Row->SetStringField(TEXT("cpp_type"),F.Type);Row->SetStringField(TEXT("value"),Value);Row->SetBoolField(TEXT("value_truncated"),Cut);Row->SetBoolField(TEXT("is_transient"),false);Row->SetBoolField(TEXT("is_editable"),true);Values.Add(MakeShared<FJsonValueObject>(Row));}
    }
    Out->SetNumberField(TEXT("property_count"),PropertyCount);Out->SetNumberField(TEXT("eligible_property_count"),Eligible);Out->SetNumberField(TEXT("included_property_count"),Values.Num());Out->SetNumberField(TEXT("skipped_transient_count"),Transient);Out->SetNumberField(TEXT("skipped_non_editable_count"),NonEditable);Out->SetNumberField(TEXT("truncated_value_count"),Truncated);Out->SetNumberField(TEXT("max_property_value_length"),Length);Out->SetBoolField(TEXT("properties_truncated"),Eligible>Values.Num());Out->SetArrayField(TEXT("properties"),Values);return Out;
}

FString NormalizeNiagaraObjectPathForLoad(const FString& ObjectPath);
FString PackagePathFromObjectPath(const FString& ObjectPath);
bool IsTempGeneratedNiagaraPath(const FString& ObjectPath);
void ReadNiagaraCompileWaitParams(
    const TSharedPtr<FJsonObject>& Params,
    bool& bWaitForCompletion,
    double& TimeoutSeconds,
    double& PollIntervalSeconds);
FString NiagaraObjectPathOrEmpty(const UObject* Object);
TSharedPtr<FJsonValue> NumberArrayToJson(std::initializer_list<double> Values);
TArray<TSharedPtr<FJsonValue>> StringArrayToJsonValues(const TArray<FString>& Values);
TSharedPtr<FJsonValue> NiagaraVariableDataToJsonValue(const FNiagaraVariable& Variable);
bool JsonArrayToDoubles(const TSharedPtr<FJsonValue>& Value, TArray<double>& OutValues);
FString NiagaraTypeName(const FNiagaraTypeDefinition& TypeDef);
FString NiagaraScriptUsageName(ENiagaraScriptUsage Usage);
FString TextureRenderTargetFormatName(ETextureRenderTargetFormat Format);
FString NiagaraCompileStatusName(ENiagaraScriptCompileStatus Status);
bool IsNiagaraCompileErrorStatus(ENiagaraScriptCompileStatus Status);
bool IsNiagaraCompileWarningStatus(ENiagaraScriptCompileStatus Status);
TSharedPtr<FJsonObject> NiagaraVariableToJsonObject(const FNiagaraVariableBase& Variable);
FString NiagaraCompileIdFingerprint(const FNiagaraVMExecutableDataId& CompileId);
void AddNiagaraScriptCompileSnapshot(
    UNiagaraScript* Script,
    const FString& OwnerKind,
    const FString& OwnerName,
    TSet<const UNiagaraScript*>& SeenScripts,
    TArray<FNiagaraScriptCompileSnapshot>& OutSnapshots);
TArray<FNiagaraScriptCompileSnapshot> CaptureNiagaraSystemCompileSnapshots(UNiagaraSystem* System);
const FNiagaraScriptCompileSnapshot* FindNiagaraScriptCompileSnapshot(
    const TArray<FNiagaraScriptCompileSnapshot>& Snapshots,
    const FString& ScriptPath);
TSharedPtr<FJsonObject> BuildNiagaraScriptCompileSnapshotJson(
    const FNiagaraScriptCompileSnapshot& Snapshot,
    int32 ScriptIndex);
TSharedPtr<FJsonObject> NiagaraVariableWithDataToJsonObject(const FNiagaraVariable& Variable);
TArray<TSharedPtr<FJsonValue>> NiagaraVariablesToJson(const TArray<FNiagaraVariable>& Variables);
TArray<TSharedPtr<FJsonValue>> NiagaraVariableBasesToJson(const TArray<FNiagaraVariableBase>& Variables);
TSharedPtr<FJsonObject> GraphPinToJsonObject(const UEdGraphPin* Pin);
TSharedPtr<FJsonObject> LinkedPinToJsonObject(const UEdGraphPin* Pin);
TArray<TSharedPtr<FJsonValue>> LinkedPinsToJson(const UEdGraphPin* Pin, bool bIncludeLinkedSources);
TArray<TSharedPtr<FJsonValue>> GraphPinsToJson(const TArray<UEdGraphPin*>& Pins);
bool IsSettableNiagaraUserParameterType(const FNiagaraTypeDefinition& TypeDef);
bool IsMcpSupportedNiagaraArrayDataInterfaceType(const FNiagaraTypeDefinition& TypeDef);
bool IsSettableNiagaraRapidIterationModuleInputType(const FNiagaraTypeDefinition& TypeDef);
bool IsRenderTarget2DDataInterfaceType(const FNiagaraTypeDefinition& TypeDef);
bool IsTexture2DDataInterfaceType(const FNiagaraTypeDefinition& TypeDef);
bool SetNiagaraVariableDataFromJson(FNiagaraVariable& Variable, const TSharedPtr<FJsonValue>& JsonValue, FString& OutError);
void AddUniqueString(TArray<FString>& Values, const FString& Value);
TArray<TSharedPtr<FJsonValue>> StringsToJson(const TArray<FString>& Values);
void AddHintIfContains(const FString& SearchText, const FString& Needle, const FString& Hint, TArray<FString>& Hints);
TArray<FString> BuildNiagaraControlHints(const FString& SearchText);
FString InferNiagaraInputControlKind(const FString& FunctionName, const FString& PinName, const FString& PinCategory);
int32 NiagaraInputControlPriority(const FString& ControlKind, const FString& FunctionName, const FString& PinName);
bool IsLowSignalNiagaraInputPinName(const FString& PinName);
bool IsCandidateNiagaraInputPin(const UEdGraphPin* Pin);
TSharedPtr<FJsonObject> BuildModuleInputCandidateJson(
    const UNiagaraNodeFunctionCall* FunctionCall,
    const UEdGraphPin* Pin,
    const FString& EmitterName,
    int32 EmitterIndex,
    int32 ModuleIndex,
    bool bIncludeLinkedSources);
UNiagaraScript* FindEmitterScriptForUsage(FVersionedNiagaraEmitterData* EmitterData, ENiagaraScriptUsage Usage);
FNiagaraVariable BuildRapidIterationParameterForInput(
    const UNiagaraNodeFunctionCall* FunctionCall,
    const FNiagaraVariable& InputVariable,
    const FString& EmitterName,
    ENiagaraScriptUsage ScriptUsage);
FNiagaraParameterHandle BuildAliasedModuleParameterHandleForInput(
    const UNiagaraNodeFunctionCall* FunctionCall,
    const FNiagaraVariable& InputVariable);
bool NiagaraModuleInputNameMatches(const FNiagaraVariable& Candidate, const FString& InputName);
UEdGraphPin* FindNiagaraStackFunctionInputOverridePin(
    const UNiagaraNodeFunctionCall* FunctionCall,
    const FNiagaraParameterHandle& AliasedInputHandle);
TSharedPtr<FJsonObject> BuildResolvedStackInputJson(
    const UNiagaraNodeFunctionCall* FunctionCall,
    const FNiagaraVariable& InputVariable,
    const TSet<FNiagaraVariable>& HiddenVariables,
    UNiagaraScript* OwningScript,
    const FString& EmitterName);
TArray<TSharedPtr<FJsonValue>> BuildResolvedStackInputsJson(
    const UNiagaraNodeFunctionCall* FunctionCall,
    FVersionedNiagaraEmitter OwningEmitter,
    FVersionedNiagaraEmitterData* EmitterData,
    const FString& EmitterName,
    int32 MaxResolvedInputs);
TSharedPtr<FJsonObject> BuildModuleInputModuleJson(
    const UNiagaraNodeFunctionCall* FunctionCall,
    FVersionedNiagaraEmitter OwningEmitter,
    FVersionedNiagaraEmitterData* EmitterData,
    const FString& EmitterName,
    int32 EmitterIndex,
    int32 ModuleIndex,
    bool bIncludeLinkedSources,
    bool bIncludeResolvedStackInputs,
    int32 MaxCandidatesPerModule,
    int32 MaxResolvedInputsPerModule,
    int32& OutCandidateCount,
    TArray<TSharedPtr<FJsonObject>>& OutTopCandidates);
TArray<UNiagaraNodeFunctionCall*> CollectSortedNiagaraFunctionCalls(const UNiagaraScriptSourceBase* SourceBase);
TSharedPtr<FJsonObject> BuildFunctionCallJson(const UNiagaraNodeFunctionCall* FunctionCall, int32 NodeIndex, bool bIncludePins);
TSharedPtr<FJsonObject> BuildInputNodeJson(const UNiagaraNodeInput* InputNode);
TSharedPtr<FJsonObject> BuildUObjectReferenceJson(const UObject* Object);
TSharedPtr<FJsonObject> BuildRenderTargetObjectJson(const UObject* Object);
UObject* GetObjectPropertyForInspection(const UObject* Object, const FName PropertyName);
TSharedPtr<FJsonObject> BuildDataInterfaceInspectionJson(
    const UNiagaraDataInterface* DataInterface,
    UNiagaraSystem* System,
    bool bIncludeProperties,
    int32 MaxProperties,
    int32 MaxPropertyValueLength);
TSharedPtr<FJsonObject> BuildNiagaraDataInterfaceOverrideJson(
    const UEdGraphPin* OverridePin,
    UNiagaraSystem* System,
    bool bIncludeDataInterfaceProperties,
    int32 MaxDataInterfaceProperties,
    int32 MaxDataInterfacePropertyValueLength);

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

void ReadNiagaraCompileWaitParams(
    const TSharedPtr<FJsonObject>& Params,
    bool& bWaitForCompletion,
    double& TimeoutSeconds,
    double& PollIntervalSeconds)
{
    bWaitForCompletion = false;
    TimeoutSeconds = 10.0;
    PollIntervalSeconds = 0.1;
    if (Params.IsValid())
    {
        Params->TryGetBoolField(TEXT("wait_for_completion"), bWaitForCompletion);
        Params->TryGetNumberField(TEXT("timeout_seconds"), TimeoutSeconds);
        Params->TryGetNumberField(TEXT("poll_interval_seconds"), PollIntervalSeconds);
    }
    TimeoutSeconds = FMath::Clamp(TimeoutSeconds, 0.0, 60.0);
    PollIntervalSeconds = FMath::Clamp(PollIntervalSeconds, 0.02, 1.0);
}

FString NiagaraObjectPathOrEmpty(const UObject* Object)
{
    return Object ? Object->GetPathName() : FString();
}

TSharedPtr<FJsonValue> NumberArrayToJson(std::initializer_list<double> Values)
{
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    for (double Value : Values)
    {
        JsonValues.Add(MakeShared<FJsonValueNumber>(Value));
    }
    return MakeShared<FJsonValueArray>(JsonValues);
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

TSharedPtr<FJsonValue> NiagaraVariableDataToJsonValue(const FNiagaraVariable& Variable)
{
    if (!Variable.IsDataAllocated())
    {
        return MakeShared<FJsonValueNull>();
    }

    const FNiagaraTypeDefinition& TypeDef = Variable.GetType();
    if (TypeDef == FNiagaraTypeDefinition::GetFloatDef())
    {
        return MakeShared<FJsonValueNumber>(Variable.GetValue<float>());
    }
    if (TypeDef == FNiagaraTypeDefinition::GetIntDef())
    {
        return MakeShared<FJsonValueNumber>(Variable.GetValue<int32>());
    }
    if (TypeDef == FNiagaraTypeDefinition::GetBoolDef())
    {
        const FNiagaraBool Value = Variable.GetValue<FNiagaraBool>();
        return MakeShared<FJsonValueBoolean>(Value.GetValue());
    }
    if (TypeDef.IsEnum() && TypeDef.GetEnum())
    {
        const UEnum* Enum = TypeDef.GetEnum();
        const FNiagaraInt32 Value = Variable.GetValue<FNiagaraInt32>();
        const int32 EnumIndex = Enum->GetIndexByValue(Value.Value);
        if (EnumIndex != INDEX_NONE)
        {
            return MakeShared<FJsonValueString>(Enum->GetNameStringByIndex(EnumIndex));
        }
        return MakeShared<FJsonValueNumber>(Value.Value);
    }
    if (TypeDef == FNiagaraTypeDefinition::GetColorDef())
    {
        const FLinearColor Value = Variable.GetValue<FLinearColor>();
        return NumberArrayToJson({ Value.R, Value.G, Value.B, Value.A });
    }
    if (TypeDef == FNiagaraTypeDefinition::GetVec2Def())
    {
        const FVector2f Value = Variable.GetValue<FVector2f>();
        return NumberArrayToJson({ Value.X, Value.Y });
    }
    if (TypeDef == FNiagaraTypeDefinition::GetVec3Def())
    {
        const FVector3f Value = Variable.GetValue<FVector3f>();
        return NumberArrayToJson({ Value.X, Value.Y, Value.Z });
    }
    if (TypeDef == FNiagaraTypeDefinition::GetVec4Def())
    {
        const FVector4f Value = Variable.GetValue<FVector4f>();
        return NumberArrayToJson({ Value.X, Value.Y, Value.Z, Value.W });
    }
    if (TypeDef == FNiagaraTypeDefinition::GetPositionDef())
    {
        const FVector3f Value = Variable.GetValue<FVector3f>();
        return NumberArrayToJson({ Value.X, Value.Y, Value.Z });
    }

    return MakeShared<FJsonValueString>(Variable.ToString());
}

bool JsonArrayToDoubles(const TSharedPtr<FJsonValue>& Value, TArray<double>& OutValues)
{
    if (!Value.IsValid() || Value->Type != EJson::Array)
    {
        return false;
    }

    for (const TSharedPtr<FJsonValue>& Item : Value->AsArray())
    {
        double NumberValue = 0.0;
        if (!Item.IsValid() || !Item->TryGetNumber(NumberValue) || !FMath::IsFinite(NumberValue))
        {
            return false;
        }
        OutValues.Add(NumberValue);
    }
    return true;
}

FString NiagaraTypeName(const FNiagaraTypeDefinition& TypeDef)
{
    return TypeDef.GetNameText().ToString();
}

FString NiagaraScriptUsageName(ENiagaraScriptUsage Usage)
{
    if (const UEnum* Enum = StaticEnum<ENiagaraScriptUsage>())
    {
        return Enum->GetNameStringByValue(static_cast<int64>(Usage));
    }
    return FString::FromInt(static_cast<int32>(Usage));
}

FString TextureRenderTargetFormatName(ETextureRenderTargetFormat Format)
{
    switch (Format)
    {
    case RTF_R8:
        return TEXT("RTF_R8");
    case RTF_RG8:
        return TEXT("RTF_RG8");
    case RTF_RGBA8:
        return TEXT("RTF_RGBA8");
    case RTF_RGBA8_SRGB:
        return TEXT("RTF_RGBA8_SRGB");
    case RTF_R16f:
        return TEXT("RTF_R16f");
    case RTF_RG16f:
        return TEXT("RTF_RG16f");
    case RTF_RGBA16f:
        return TEXT("RTF_RGBA16f");
    case RTF_R32f:
        return TEXT("RTF_R32f");
    case RTF_RG32f:
        return TEXT("RTF_RG32f");
    case RTF_RGBA32f:
        return TEXT("RTF_RGBA32f");
    case RTF_RGB10A2:
        return TEXT("RTF_RGB10A2");
    default:
        return FString::FromInt(static_cast<int32>(Format));
    }
}

FString NiagaraCompileStatusName(ENiagaraScriptCompileStatus Status)
{
    if (const UEnum* Enum = StaticEnum<ENiagaraScriptCompileStatus>())
    {
        return Enum->GetNameStringByValue(static_cast<int64>(Status));
    }
    return FString::FromInt(static_cast<int32>(Status));
}

bool IsNiagaraCompileErrorStatus(ENiagaraScriptCompileStatus Status)
{
    return Status == ENiagaraScriptCompileStatus::NCS_Error;
}

bool IsNiagaraCompileWarningStatus(ENiagaraScriptCompileStatus Status)
{
    return Status == ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings ||
        Status == ENiagaraScriptCompileStatus::NCS_ComputeUpToDateWithWarnings;
}

TSharedPtr<FJsonObject> NiagaraVariableToJsonObject(const FNiagaraVariableBase& Variable)
{
    TSharedPtr<FJsonObject> VariableObject = MakeShared<FJsonObject>();
    VariableObject->SetStringField(TEXT("name"), Variable.GetName().ToString());
    VariableObject->SetStringField(TEXT("type"), NiagaraTypeName(Variable.GetType()));
    VariableObject->SetBoolField(TEXT("type_is_valid"), Variable.GetType().IsValid());
    VariableObject->SetBoolField(TEXT("type_is_data_interface"), Variable.GetType().IsDataInterface());
    VariableObject->SetBoolField(TEXT("type_is_uobject"), Variable.GetType().IsUObject());
    if (UClass* TypeClass = Variable.GetType().GetClass())
    {
        VariableObject->SetStringField(TEXT("type_class"), TypeClass->GetName());
        VariableObject->SetStringField(TEXT("type_class_path"), TypeClass->GetPathName());
    }
    return VariableObject;
}

FString NiagaraCompileIdFingerprint(const FNiagaraVMExecutableDataId& CompileId)
{
    if (!CompileId.IsValid())
    {
        return FString();
    }

    FString KeyString;
    CompileId.AppendKeyString(KeyString);
    return FMD5::HashAnsiString(*KeyString);
}

void AddNiagaraScriptCompileSnapshot(
    UNiagaraScript* Script,
    const FString& OwnerKind,
    const FString& OwnerName,
    TSet<const UNiagaraScript*>& SeenScripts,
    TArray<FNiagaraScriptCompileSnapshot>& OutSnapshots)
{
    if (!Script || SeenScripts.Contains(Script))
    {
        return;
    }

    if(!Current||!Current->SDK(TEXT("CaptureCompileSnapshot"))){return;}
    SeenScripts.Add(Script);
    FNiagaraScriptCompileSnapshot& Snapshot = OutSnapshots.AddDefaulted_GetRef();
    Snapshot.Script = Script;
    Snapshot.OwnerKind = OwnerKind;
    Snapshot.OwnerName = OwnerName;
    Snapshot.ScriptPath = Script->GetPathName();
    Snapshot.CachedId = Script->GetVMExecutableDataCompilationId();
    Script->ComputeVMCompilationId(Snapshot.ComputedId, FGuid());
    if(!Current->After(TEXT("ComputeVMCompilationId"))){return;}
    Snapshot.CompileStatus = Script->GetLastCompileStatus();
    Current->After(TEXT("CaptureCompileSnapshot"));
}

TArray<FNiagaraScriptCompileSnapshot> CaptureNiagaraSystemCompileSnapshots(UNiagaraSystem* System)
{
    TArray<FNiagaraScriptCompileSnapshot> Snapshots;if(!System||!ContinueRead()){return Snapshots;}
    // Snapshot routing values before any ComputeVMCompilationId callback. Never increment a live System handle iterator after SDK dispatch.
    UNiagaraScript* SpawnScript=System->GetSystemSpawnScript();UNiagaraScript* UpdateScript=System->GetSystemUpdateScript();const TArray<FNiagaraEmitterHandle> HandleSnapshot=System->GetEmitterHandles();
    TSet<const UNiagaraScript*> SeenScripts;
    AddNiagaraScriptCompileSnapshot(SpawnScript,TEXT("system"),TEXT("system_spawn"),SeenScripts,Snapshots);if(!ContinueRead()){return Snapshots;}
    AddNiagaraScriptCompileSnapshot(UpdateScript,TEXT("system"),TEXT("system_update"),SeenScripts,Snapshots);if(!ContinueRead()){return Snapshots;}
    int32 EmitterIndex=0;
    for(const FNiagaraEmitterHandle& EmitterHandle:HandleSnapshot)
    {
        if(!ContinueRead()){return Snapshots;}
        if(FVersionedNiagaraEmitterData* EmitterData=EmitterHandle.GetEmitterData())
        {
            const FString OwnerName=EmitterHandle.GetName().ToString();TArray<UNiagaraScript*> EmitterScripts;EmitterData->GetScripts(EmitterScripts,false,false);
            for(UNiagaraScript* Script:EmitterScripts)
            {
                if(!ContinueRead()){return Snapshots;}
                AddNiagaraScriptCompileSnapshot(Script,FString::Printf(TEXT("emitter:%d"),EmitterIndex),OwnerName,SeenScripts,Snapshots);
                if(!ContinueRead()){return Snapshots;}
            }
        }
        ++EmitterIndex;
    }
    return Snapshots;
}

const FNiagaraScriptCompileSnapshot* FindNiagaraScriptCompileSnapshot(
    const TArray<FNiagaraScriptCompileSnapshot>& Snapshots,
    const FString& ScriptPath)
{
    return Snapshots.FindByPredicate(
        [&ScriptPath](const FNiagaraScriptCompileSnapshot& Snapshot)
        {
            return Snapshot.ScriptPath == ScriptPath;
        });
}

TSharedPtr<FJsonObject> BuildNiagaraScriptCompileSnapshotJson(
    const FNiagaraScriptCompileSnapshot& Snapshot,
    int32 ScriptIndex)
{
    TSharedPtr<FJsonObject> ScriptObject = MakeShared<FJsonObject>();
    ScriptObject->SetNumberField(TEXT("script_index"), ScriptIndex);
    ScriptObject->SetStringField(TEXT("owner_kind"), Snapshot.OwnerKind);
    ScriptObject->SetStringField(TEXT("owner_name"), Snapshot.OwnerName);
    ScriptObject->SetStringField(TEXT("script_path"), Snapshot.ScriptPath);
    ScriptObject->SetStringField(TEXT("usage"), Snapshot.Script ? NiagaraScriptUsageName(Snapshot.Script->GetUsage()) : FString());
    ScriptObject->SetStringField(TEXT("compile_status"), NiagaraCompileStatusName(Snapshot.CompileStatus));
    ScriptObject->SetBoolField(TEXT("has_error"), IsNiagaraCompileErrorStatus(Snapshot.CompileStatus));
    ScriptObject->SetBoolField(TEXT("has_warning"), IsNiagaraCompileWarningStatus(Snapshot.CompileStatus));
    ScriptObject->SetBoolField(TEXT("cached_id_valid"), Snapshot.CachedId.IsValid());
    ScriptObject->SetBoolField(TEXT("computed_id_valid"), Snapshot.ComputedId.IsValid());
    ScriptObject->SetStringField(TEXT("cached_id_fingerprint"), NiagaraCompileIdFingerprint(Snapshot.CachedId));
    ScriptObject->SetStringField(TEXT("computed_id_fingerprint"), NiagaraCompileIdFingerprint(Snapshot.ComputedId));
    ScriptObject->SetBoolField(
        TEXT("source_synchronized"),
        Snapshot.CachedId.IsValid() && Snapshot.ComputedId.IsValid() && Snapshot.CachedId == Snapshot.ComputedId);
    return ScriptObject;
}

TSharedPtr<FJsonObject> NiagaraVariableWithDataToJsonObject(const FNiagaraVariable& Variable)
{
    TSharedPtr<FJsonObject> VariableObject = NiagaraVariableToJsonObject(Variable);
    VariableObject->SetBoolField(TEXT("has_data"), Variable.IsDataAllocated());
    VariableObject->SetNumberField(TEXT("data_size_bytes"), Variable.GetAllocatedSizeInBytes());
    VariableObject->SetField(TEXT("value"), NiagaraVariableDataToJsonValue(Variable));
    return VariableObject;
}

TArray<TSharedPtr<FJsonValue>> NiagaraVariablesToJson(const TArray<FNiagaraVariable>& Variables)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    for (const FNiagaraVariable& Variable : Variables)
    {
        Values.Add(MakeShared<FJsonValueObject>(NiagaraVariableToJsonObject(Variable)));
    }
    return Values;
}

TArray<TSharedPtr<FJsonValue>> NiagaraVariableBasesToJson(const TArray<FNiagaraVariableBase>& Variables)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    for (const FNiagaraVariableBase& Variable : Variables)
    {
        Values.Add(MakeShared<FJsonValueObject>(NiagaraVariableToJsonObject(Variable)));
    }
    return Values;
}

TSharedPtr<FJsonObject> GraphPinToJsonObject(const UEdGraphPin* Pin)
{
    TSharedPtr<FJsonObject> PinObject = MakeShared<FJsonObject>();
    if (!Pin)
    {
        return PinObject;
    }

    PinObject->SetStringField(TEXT("name"), Pin->PinName.ToString());
    PinObject->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
    PinObject->SetStringField(TEXT("category"), Pin->PinType.PinCategory.ToString());
    PinObject->SetStringField(TEXT("subcategory"), Pin->PinType.PinSubCategory.ToString());
    PinObject->SetStringField(TEXT("default_value"), Pin->DefaultValue);
    PinObject->SetStringField(TEXT("default_object"), NiagaraObjectPathOrEmpty(Pin->DefaultObject));
    PinObject->SetNumberField(TEXT("linked_to_count"), Pin->LinkedTo.Num());
    return PinObject;
}

TSharedPtr<FJsonObject> LinkedPinToJsonObject(const UEdGraphPin* Pin)
{
    TSharedPtr<FJsonObject> PinObject = MakeShared<FJsonObject>();
    if (!Pin)
    {
        return PinObject;
    }

    const UEdGraphNode* OwningNode = Pin->GetOwningNode();
    PinObject->SetStringField(TEXT("pin_name"), Pin->PinName.ToString());
    PinObject->SetStringField(TEXT("node_name"), OwningNode ? OwningNode->GetName() : FString());
    PinObject->SetStringField(TEXT("node_class"), OwningNode ? OwningNode->GetClass()->GetName() : FString());
    PinObject->SetStringField(TEXT("default_value"), Pin->DefaultValue);
    PinObject->SetStringField(TEXT("default_object"), NiagaraObjectPathOrEmpty(Pin->DefaultObject));
    return PinObject;
}

TArray<TSharedPtr<FJsonValue>> LinkedPinsToJson(const UEdGraphPin* Pin, bool bIncludeLinkedSources)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    if (!Pin || !bIncludeLinkedSources)
    {
        return Values;
    }

    for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
    {
        Values.Add(MakeShared<FJsonValueObject>(LinkedPinToJsonObject(LinkedPin)));
    }
    return Values;
}

TArray<TSharedPtr<FJsonValue>> GraphPinsToJson(const TArray<UEdGraphPin*>& Pins)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    for (const UEdGraphPin* Pin : Pins)
    {
        Values.Add(MakeShared<FJsonValueObject>(GraphPinToJsonObject(Pin)));
    }
    return Values;
}

bool IsSettableNiagaraUserParameterType(const FNiagaraTypeDefinition& TypeDef)
{
    return TypeDef == FNiagaraTypeDefinition::GetFloatDef() ||
        TypeDef == FNiagaraTypeDefinition::GetIntDef() ||
        TypeDef == FNiagaraTypeDefinition::GetBoolDef() ||
        TypeDef == FNiagaraTypeDefinition::GetColorDef() ||
        TypeDef == FNiagaraTypeDefinition::GetVec2Def() ||
        TypeDef == FNiagaraTypeDefinition::GetVec3Def() ||
        TypeDef == FNiagaraTypeDefinition::GetVec4Def() ||
        TypeDef == FNiagaraTypeDefinition::GetPositionDef() ||
        IsMcpSupportedNiagaraArrayDataInterfaceType(TypeDef) ||
        (TypeDef.IsUObject() && !TypeDef.IsDataInterface());
}

bool IsMcpSupportedNiagaraArrayDataInterfaceType(const FNiagaraTypeDefinition& TypeDef)
{
    UClass* InputClass = TypeDef.GetClass();
    return TypeDef.IsDataInterface() &&
        InputClass &&
        (InputClass->IsChildOf(UNiagaraDataInterfaceArrayPosition::StaticClass()) ||
            InputClass->IsChildOf(UNiagaraDataInterfaceArrayFloat3::StaticClass()) ||
            InputClass->IsChildOf(UNiagaraDataInterfaceArrayFloat2::StaticClass()) ||
            InputClass->IsChildOf(UNiagaraDataInterfaceArrayFloat::StaticClass()) ||
            InputClass->IsChildOf(UNiagaraDataInterfaceArrayInt32::StaticClass()) ||
            ClassPathOrChild(InputClass,TEXT("/Script/Niagara.NiagaraDataInterfaceArrayMesh")));
}

bool IsSettableNiagaraRapidIterationModuleInputType(const FNiagaraTypeDefinition& TypeDef)
{
    return TypeDef == FNiagaraTypeDefinition::GetFloatDef() ||
        TypeDef == FNiagaraTypeDefinition::GetIntDef() ||
        TypeDef == FNiagaraTypeDefinition::GetBoolDef() ||
        TypeDef == FNiagaraTypeDefinition::GetColorDef() ||
        TypeDef == FNiagaraTypeDefinition::GetVec2Def() ||
        TypeDef == FNiagaraTypeDefinition::GetVec3Def() ||
        TypeDef == FNiagaraTypeDefinition::GetVec4Def() ||
        TypeDef == FNiagaraTypeDefinition::GetPositionDef();
}

bool IsRenderTarget2DDataInterfaceType(const FNiagaraTypeDefinition& TypeDef)
{
    UClass* InputClass = TypeDef.GetClass();
    return TypeDef.IsDataInterface() &&
        InputClass &&
        InputClass->IsChildOf(UNiagaraDataInterfaceRenderTarget2D::StaticClass());
}

bool IsTexture2DDataInterfaceType(const FNiagaraTypeDefinition& TypeDef)
{
    UClass* InputClass = TypeDef.GetClass();
    return TypeDef.IsDataInterface() &&
        InputClass &&
        InputClass->IsChildOf(UNiagaraDataInterfaceTexture::StaticClass());
}

bool SetNiagaraVariableDataFromJson(FNiagaraVariable& Variable, const TSharedPtr<FJsonValue>& JsonValue, FString& OutError)
{
    const FNiagaraTypeDefinition& TypeDef = Variable.GetType();
    if (TypeDef == FNiagaraTypeDefinition::GetFloatDef())
    {
        double NumberValue = 0.0;
        if (!JsonValue.IsValid() || !JsonValue->TryGetNumber(NumberValue))
        {
            OutError = TEXT("Expected numeric value for float Niagara module input");
            return false;
        }
        if(!FMath::IsFinite(NumberValue)||FMath::Abs(NumberValue)>MAX_flt){OutError=TEXT("Finite float32 required; original overflow is unsafe");return false;}
        Variable.SetValue<float>(static_cast<float>(NumberValue));
        return true;
    }
    if (TypeDef == FNiagaraTypeDefinition::GetIntDef())
    {
        double NumberValue = 0.0;
        if (!JsonValue.IsValid() || !JsonValue->TryGetNumber(NumberValue))
        {
            OutError = TEXT("Expected numeric value for int Niagara module input");
            return false;
        }
        if(!FMath::IsFinite(NumberValue)||NumberValue<MIN_int32||NumberValue>MAX_int32){OutError=TEXT("Finite int32 cast range required; original fractional truncation retained");return false;}
        Variable.SetValue<int32>(static_cast<int32>(NumberValue));
        return true;
    }
    if (TypeDef == FNiagaraTypeDefinition::GetBoolDef())
    {
        bool BoolValue = false;
        if (!JsonValue.IsValid() || !JsonValue->TryGetBool(BoolValue))
        {
            OutError = TEXT("Expected boolean value for bool Niagara module input");
            return false;
        }
        Variable.SetValue<FNiagaraBool>(FNiagaraBool(BoolValue));
        return true;
    }
    if (TypeDef.IsEnum() && TypeDef.GetEnum())
    {
        const UEnum* Enum = TypeDef.GetEnum();
        int32 ResolvedIndex = INDEX_NONE;

        if (JsonValue.IsValid() && JsonValue->Type == EJson::Number)
        {
            double NumericValue = 0.0;
            if (!JsonValue->TryGetNumber(NumericValue) || !FMath::IsFinite(NumericValue))
            {
                OutError = TEXT("Expected a finite integer for Niagara enum module input");
                return false;
            }
            const double RoundedValue = FMath::RoundToDouble(NumericValue);
            if (NumericValue != RoundedValue || RoundedValue < static_cast<double>(MIN_int32) || RoundedValue > static_cast<double>(MAX_int32))
            {
                OutError = TEXT("Expected an exact int32 for Niagara enum module input");
                return false;
            }
            ResolvedIndex = Enum->GetIndexByValue(static_cast<int64>(RoundedValue));
        }
        else if (JsonValue.IsValid() && JsonValue->Type == EJson::String)
        {
            FString RequestedValue;
            if (!JsonValue->TryGetString(RequestedValue))
            {
                OutError = TEXT("Expected a string or integer for Niagara enum module input");
                return false;
            }
            RequestedValue.TrimStartAndEndInline();
            if (RequestedValue.IsEmpty())
            {
                OutError = TEXT("Niagara enum module input cannot be empty");
                return false;
            }

            TArray<int32> MatchingIndices;
            for (int32 EnumIndex = 0; EnumIndex < Enum->NumEnums(); ++EnumIndex)
            {
                if (Enum->GetNameStringByIndex(EnumIndex).Equals(RequestedValue, ESearchCase::IgnoreCase) ||
                    Enum->GetNameByIndex(EnumIndex).ToString().Equals(RequestedValue, ESearchCase::IgnoreCase) ||
                    Enum->GetAuthoredNameStringByIndex(EnumIndex).Equals(RequestedValue, ESearchCase::IgnoreCase) ||
                    Enum->GetDisplayNameTextByIndex(EnumIndex).ToString().Equals(RequestedValue, ESearchCase::IgnoreCase))
                {
                    MatchingIndices.Add(EnumIndex);
                }
            }
            if (MatchingIndices.Num() == 1)
            {
                ResolvedIndex = MatchingIndices[0];
            }
            else if (MatchingIndices.Num() > 1)
            {
                OutError = FString::Printf(TEXT("Ambiguous Niagara enum module input for %s: %s"), *Enum->GetPathName(), *RequestedValue);
                return false;
            }
        }
        else
        {
            OutError = TEXT("Expected a display/internal enum name string or integer for Niagara enum module input");
            return false;
        }

        if (ResolvedIndex == INDEX_NONE)
        {
            OutError = FString::Printf(TEXT("Invalid Niagara enum module input for %s"), *Enum->GetPathName());
            return false;
        }

        const FString InternalName = Enum->GetNameStringByIndex(ResolvedIndex);
        const bool bTerminalMax = InternalName.EndsWith(TEXT("_MAX"), ESearchCase::IgnoreCase) ||
            InternalName.Equals(TEXT("MAX"), ESearchCase::IgnoreCase);
        if (Enum->HasMetaData(TEXT("Hidden"), ResolvedIndex) || bTerminalMax)
        {
            OutError = FString::Printf(TEXT("Hidden or terminal Niagara enum module input is not selectable for %s"), *Enum->GetPathName());
            return false;
        }

        const int64 ResolvedValue = Enum->GetValueByIndex(ResolvedIndex);
        if (ResolvedValue < static_cast<int64>(MIN_int32) || ResolvedValue > static_cast<int64>(MAX_int32))
        {
            OutError = FString::Printf(TEXT("Niagara enum value is outside int32 range for %s"), *Enum->GetPathName());
            return false;
        }

        FNiagaraInt32 NiagaraEnumValue;
        NiagaraEnumValue.Value = static_cast<int32>(ResolvedValue);
        Variable.SetValue<FNiagaraInt32>(NiagaraEnumValue);
        return true;
    }

    TArray<double> Values;
    if (!JsonArrayToDoubles(JsonValue, Values))
    {
        OutError = TEXT("Expected numeric array value for vector/color Niagara module input");
        return false;
    }

    for(double N:Values){if(!FMath::IsFinite(N)||FMath::Abs(N)>MAX_flt){OutError=TEXT("Finite float32 vector/color component required");return false;}}
    if (TypeDef == FNiagaraTypeDefinition::GetColorDef())
    {
        if (Values.Num() < 3)
        {
            OutError = TEXT("Expected color array with at least 3 values");
            return false;
        }
        const float Alpha = Values.Num() >= 4 ? static_cast<float>(Values[3]) : 1.0f;
        Variable.SetValue<FLinearColor>(FLinearColor(static_cast<float>(Values[0]), static_cast<float>(Values[1]), static_cast<float>(Values[2]), Alpha));
        return true;
    }
    if (TypeDef == FNiagaraTypeDefinition::GetVec2Def())
    {
        if (Values.Num() < 2)
        {
            OutError = TEXT("Expected vec2 array with 2 values");
            return false;
        }
        Variable.SetValue<FVector2f>(FVector2f(Values[0], Values[1]));
        return true;
    }
    if (TypeDef == FNiagaraTypeDefinition::GetVec3Def())
    {
        if (Values.Num() < 3)
        {
            OutError = TEXT("Expected vec3 array with 3 values");
            return false;
        }
        Variable.SetValue<FVector3f>(FVector3f(Values[0], Values[1], Values[2]));
        return true;
    }
    if (TypeDef == FNiagaraTypeDefinition::GetVec4Def())
    {
        if (Values.Num() < 4)
        {
            OutError = TEXT("Expected vec4 array with 4 values");
            return false;
        }
        Variable.SetValue<FVector4f>(FVector4f(Values[0], Values[1], Values[2], Values[3]));
        return true;
    }
    if (TypeDef == FNiagaraTypeDefinition::GetPositionDef())
    {
        if (Values.Num() < 3)
        {
            OutError = TEXT("Expected position array with 3 values");
            return false;
        }
        Variable.SetValue<FVector3f>(FVector3f(Values[0], Values[1], Values[2]));
        return true;
    }

    OutError = FString::Printf(TEXT("Unsupported Niagara module input type: %s"), *NiagaraTypeName(TypeDef));
    return false;
}

void AddUniqueString(TArray<FString>& Values, const FString& Value)
{
    if (!Value.IsEmpty() && !Values.Contains(Value))
    {
        Values.Add(Value);
    }
}

TArray<TSharedPtr<FJsonValue>> StringsToJson(const TArray<FString>& Values)
{
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    for (const FString& Value : Values)
    {
        JsonValues.Add(MakeShared<FJsonValueString>(Value));
    }
    return JsonValues;
}

void AddHintIfContains(const FString& SearchText, const FString& Needle, const FString& Hint, TArray<FString>& Hints)
{
    if (SearchText.Contains(Needle, ESearchCase::IgnoreCase))
    {
        AddUniqueString(Hints, Hint);
    }
}

TArray<FString> BuildNiagaraControlHints(const FString& SearchText)
{
    TArray<FString> Hints;
    AddHintIfContains(SearchText, TEXT("color"), TEXT("color_or_tint_control"), Hints);
    AddHintIfContains(SearchText, TEXT("colour"), TEXT("color_or_tint_control"), Hints);
    AddHintIfContains(SearchText, TEXT("tint"), TEXT("color_or_tint_control"), Hints);
    AddHintIfContains(SearchText, TEXT("ribbonwidth"), TEXT("ribbon_width_control"), Hints);
    AddHintIfContains(SearchText, TEXT("ribbon width"), TEXT("ribbon_width_control"), Hints);
    AddHintIfContains(SearchText, TEXT("width"), TEXT("width_control"), Hints);
    AddHintIfContains(SearchText, TEXT("scale"), TEXT("scale_control"), Hints);
    AddHintIfContains(SearchText, TEXT("size"), TEXT("size_control"), Hints);
    AddHintIfContains(SearchText, TEXT("lifetime"), TEXT("lifetime_control"), Hints);
    AddHintIfContains(SearchText, TEXT("life time"), TEXT("lifetime_control"), Hints);
    AddHintIfContains(SearchText, TEXT("duration"), TEXT("duration_control"), Hints);
    AddHintIfContains(SearchText, TEXT("velocity"), TEXT("velocity_control"), Hints);
    AddHintIfContains(SearchText, TEXT("speed"), TEXT("velocity_control"), Hints);
    AddHintIfContains(SearchText, TEXT("spawn"), TEXT("spawn_control"), Hints);
    AddHintIfContains(SearchText, TEXT("materialparam"), TEXT("dynamic_material_parameter_control"), Hints);
    AddHintIfContains(SearchText, TEXT("dynamicmaterial"), TEXT("dynamic_material_parameter_control"), Hints);
    AddHintIfContains(SearchText, TEXT("user."), TEXT("user_parameter_reference"), Hints);
    AddHintIfContains(SearchText, TEXT("scratch"), TEXT("scratch_pad_reference"), Hints);
    AddHintIfContains(SearchText, TEXT("trail"), TEXT("trail_behavior"), Hints);
    AddHintIfContains(SearchText, TEXT("ribbon"), TEXT("ribbon_behavior"), Hints);
    return Hints;
}

FString InferNiagaraInputControlKind(const FString& FunctionName, const FString& PinName, const FString& PinCategory)
{
    const FString SearchText = FString::Printf(TEXT("%s %s %s"), *FunctionName, *PinName, *PinCategory);

    if (SearchText.Contains(TEXT("dynamicmaterial"), ESearchCase::IgnoreCase) ||
        SearchText.Contains(TEXT("materialparam"), ESearchCase::IgnoreCase) ||
        SearchText.Contains(TEXT("material parameter"), ESearchCase::IgnoreCase))
    {
        return TEXT("dynamic_material_parameter");
    }
    if (SearchText.Contains(TEXT("color"), ESearchCase::IgnoreCase) ||
        SearchText.Contains(TEXT("colour"), ESearchCase::IgnoreCase) ||
        SearchText.Contains(TEXT("tint"), ESearchCase::IgnoreCase))
    {
        return TEXT("color");
    }
    if (SearchText.Contains(TEXT("ribbonwidth"), ESearchCase::IgnoreCase) ||
        SearchText.Contains(TEXT("ribbon width"), ESearchCase::IgnoreCase) ||
        SearchText.Contains(TEXT("width"), ESearchCase::IgnoreCase))
    {
        return TEXT("width");
    }
    if (SearchText.Contains(TEXT("scale"), ESearchCase::IgnoreCase) ||
        SearchText.Contains(TEXT("size"), ESearchCase::IgnoreCase))
    {
        return TEXT("scale_or_size");
    }
    if (SearchText.Contains(TEXT("velocity"), ESearchCase::IgnoreCase) ||
        SearchText.Contains(TEXT("speed"), ESearchCase::IgnoreCase))
    {
        return TEXT("velocity");
    }
    if (SearchText.Contains(TEXT("lifetime"), ESearchCase::IgnoreCase) ||
        SearchText.Contains(TEXT("life time"), ESearchCase::IgnoreCase) ||
        SearchText.Contains(TEXT("duration"), ESearchCase::IgnoreCase))
    {
        return TEXT("lifetime");
    }
    if (SearchText.Contains(TEXT("spawn"), ESearchCase::IgnoreCase) ||
        SearchText.Contains(TEXT("count"), ESearchCase::IgnoreCase) ||
        SearchText.Contains(TEXT("rate"), ESearchCase::IgnoreCase))
    {
        return TEXT("spawn");
    }
    if (SearchText.Contains(TEXT("user."), ESearchCase::IgnoreCase))
    {
        return TEXT("user_parameter_reference");
    }
    if (PinCategory.Contains(TEXT("float"), ESearchCase::IgnoreCase) ||
        PinCategory.Contains(TEXT("double"), ESearchCase::IgnoreCase) ||
        PinCategory.Contains(TEXT("int"), ESearchCase::IgnoreCase) ||
        PinCategory.Contains(TEXT("bool"), ESearchCase::IgnoreCase) ||
        PinCategory.Contains(TEXT("struct"), ESearchCase::IgnoreCase))
    {
        return TEXT("numeric_or_struct");
    }
    return TEXT("unknown");
}

int32 NiagaraInputControlPriority(const FString& ControlKind, const FString& FunctionName, const FString& PinName)
{
    int32 Priority = 10;
    if (ControlKind == TEXT("color") ||
        ControlKind == TEXT("dynamic_material_parameter") ||
        ControlKind == TEXT("scale_or_size") ||
        ControlKind == TEXT("width"))
    {
        Priority = 80;
    }
    else if (ControlKind == TEXT("velocity") ||
        ControlKind == TEXT("spawn") ||
        ControlKind == TEXT("lifetime"))
    {
        Priority = 75;
    }
    else if (ControlKind == TEXT("user_parameter_reference"))
    {
        Priority = 70;
    }
    else if (ControlKind == TEXT("numeric_or_struct"))
    {
        Priority = 35;
    }

    if (FunctionName.Contains(TEXT("ScaleColor"), ESearchCase::IgnoreCase) ||
        FunctionName.Contains(TEXT("DynamicMaterial"), ESearchCase::IgnoreCase))
    {
        Priority += 25;
    }
    else if (FunctionName.Contains(TEXT("AddVelocity"), ESearchCase::IgnoreCase) ||
        FunctionName.Contains(TEXT("SpawnBurst"), ESearchCase::IgnoreCase))
    {
        Priority += 18;
    }
    else if (FunctionName.Contains(TEXT("ScaleMesh"), ESearchCase::IgnoreCase))
    {
        Priority += 15;
    }
    else if (FunctionName.Equals(TEXT("EmitterState"), ESearchCase::IgnoreCase))
    {
        Priority -= 35;
    }

    if (PinName.StartsWith(TEXT("Use "), ESearchCase::IgnoreCase) ||
        PinName.StartsWith(TEXT("Use"), ESearchCase::IgnoreCase))
    {
        Priority -= 10;
    }
    if (PinName.Contains(TEXT("Curve"), ESearchCase::IgnoreCase) && FunctionName.Contains(TEXT("ScaleColor"), ESearchCase::IgnoreCase))
    {
        Priority += 8;
    }

    return FMath::Clamp(Priority, 0, 100);
}

bool IsLowSignalNiagaraInputPinName(const FString& PinName)
{
    return PinName.Equals(TEXT("InputMap"), ESearchCase::IgnoreCase) ||
        PinName.Equals(TEXT("OutputMap"), ESearchCase::IgnoreCase) ||
        PinName.Contains(TEXT("Parameter Map"), ESearchCase::IgnoreCase) ||
        PinName.Contains(TEXT("ParameterMap"), ESearchCase::IgnoreCase) ||
        PinName.Contains(TEXT("Write Parameter Index"), ESearchCase::IgnoreCase);
}

bool IsCandidateNiagaraInputPin(const UEdGraphPin* Pin)
{
    if (!Pin || Pin->Direction != EGPD_Input)
    {
        return false;
    }

    const FString PinName = Pin->PinName.ToString();
    const FString PinCategory = Pin->PinType.PinCategory.ToString();
    if (PinCategory.Equals(TEXT("exec"), ESearchCase::IgnoreCase) ||
        IsLowSignalNiagaraInputPinName(PinName))
    {
        return false;
    }

    return true;
}

TSharedPtr<FJsonObject> BuildModuleInputCandidateJson(
    const UNiagaraNodeFunctionCall* FunctionCall,
    const UEdGraphPin* Pin,
    const FString& EmitterName,
    int32 EmitterIndex,
    int32 ModuleIndex,
    bool bIncludeLinkedSources)
{
    TSharedPtr<FJsonObject> CandidateObject = MakeShared<FJsonObject>();
    if (!FunctionCall || !Pin)
    {
        return CandidateObject;
    }

    FString FunctionName = FunctionCall->GetFunctionName();
    if (FunctionName.IsEmpty())
    {
        FunctionName = FunctionCall->Signature.GetNameString();
    }

    const FString PinName = Pin->PinName.ToString();
    const FString PinCategory = Pin->PinType.PinCategory.ToString();
    const FString ControlKind = InferNiagaraInputControlKind(FunctionName, PinName, PinCategory);
    const FNiagaraTypeDefinition PinType = UEdGraphSchema_Niagara::PinToTypeDefinition(Pin);
    const bool bRapidIterationAuthorable = PinType.IsValid() && IsSettableNiagaraRapidIterationModuleInputType(PinType);
    const bool bUserParameterAuthorable =
        PinType.IsValid() &&
        !PinType.IsDataInterface() &&
        !PinType.IsUObject() &&
        IsSettableNiagaraUserParameterType(PinType);
    const bool bRenderTarget2DAuthorable = PinType.IsValid() && IsRenderTarget2DDataInterfaceType(PinType);
    const bool bTexture2DAuthorable = PinType.IsValid() && IsTexture2DDataInterfaceType(PinType);
    TArray<FString> DedicatedAuthoringTools;
    FString AuthoringRouteHint = TEXT("inspection_only");
    if (bRapidIterationAuthorable)
    {
        DedicatedAuthoringTools.Add(TEXT("create_niagara_module_input_override"));
        DedicatedAuthoringTools.Add(TEXT("set_niagara_module_input_value"));
        DedicatedAuthoringTools.Add(TEXT("set_niagara_module_input_literal_override"));
        DedicatedAuthoringTools.Add(TEXT("set_niagara_module_inputs_batch"));
        AuthoringRouteHint = TEXT("rapid_iteration_literal_or_user_parameter");
    }
    if (bUserParameterAuthorable)
    {
        DedicatedAuthoringTools.Add(TEXT("set_niagara_module_input_user_parameter"));
        DedicatedAuthoringTools.Add(TEXT("set_niagara_module_input_linked_parameter"));
        AuthoringRouteHint = TEXT("rapid_iteration_or_user_parameter");
    }
    if (bRenderTarget2DAuthorable)
    {
        DedicatedAuthoringTools.Add(TEXT("set_niagara_render_target2d_module_input"));
        AuthoringRouteHint = TEXT("render_target2d_data_interface_user_parameter");
    }
    if (bTexture2DAuthorable)
    {
        DedicatedAuthoringTools.Add(TEXT("set_niagara_texture2d_module_input"));
        AuthoringRouteHint = TEXT("texture2d_data_interface_user_parameter");
    }
    if (!bRenderTarget2DAuthorable && !bTexture2DAuthorable && PinType.IsValid() && PinType.IsDataInterface())
    {
        AuthoringRouteHint = TEXT("data_interface_read_only_unless_dedicated_tool_exists");
    }
    else if (PinType.IsValid() && PinType.IsUObject())
    {
        AuthoringRouteHint = TEXT("object_input_read_only_unless_dedicated_tool_exists");
    }

    CandidateObject->SetStringField(TEXT("emitter_name"), EmitterName);
    CandidateObject->SetNumberField(TEXT("emitter_index"), EmitterIndex);
    CandidateObject->SetNumberField(TEXT("module_index"), ModuleIndex);
    CandidateObject->SetStringField(TEXT("module_name"), FunctionName);
    CandidateObject->SetStringField(TEXT("module_node_name"), FunctionCall->GetName());
    CandidateObject->SetStringField(TEXT("module_node_guid"), FunctionCall->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    CandidateObject->SetStringField(TEXT("pin_name"), PinName);
    CandidateObject->SetStringField(TEXT("pin_category"), PinCategory);
    CandidateObject->SetStringField(TEXT("pin_subcategory"), Pin->PinType.PinSubCategory.ToString());
    CandidateObject->SetStringField(TEXT("input_type"), PinType.IsValid() ? NiagaraTypeName(PinType) : FString());
    CandidateObject->SetStringField(TEXT("default_value"), Pin->DefaultValue);
    CandidateObject->SetStringField(TEXT("default_object"), NiagaraObjectPathOrEmpty(Pin->DefaultObject));
    CandidateObject->SetNumberField(TEXT("linked_to_count"), Pin->LinkedTo.Num());
    CandidateObject->SetArrayField(TEXT("linked_sources"), LinkedPinsToJson(Pin, bIncludeLinkedSources));
    CandidateObject->SetStringField(TEXT("control_kind"), ControlKind);
    CandidateObject->SetNumberField(TEXT("priority"), NiagaraInputControlPriority(ControlKind, FunctionName, PinName));
    CandidateObject->SetBoolField(TEXT("dedicated_authoring_tools_available"), DedicatedAuthoringTools.Num() > 0);
    CandidateObject->SetArrayField(TEXT("dedicated_authoring_tools"), StringArrayToJsonValues(DedicatedAuthoringTools));
    CandidateObject->SetStringField(TEXT("authoring_route_hint"), AuthoringRouteHint);
    CandidateObject->SetBoolField(TEXT("can_author_now"), false);
    CandidateObject->SetStringField(TEXT("can_author_now_note"), TEXT("false means this inspection response is read-only; use a dedicated authoring tool when one is listed"));
    CandidateObject->SetStringField(
        TEXT("authoring_status"),
        DedicatedAuthoringTools.Num() > 0
            ? TEXT("inspect_only_candidate; dedicated authoring tools are available for this inferred input type")
            : TEXT("inspect_only_candidate; no dedicated authoring tool inferred for this input type"));
    CandidateObject->SetStringField(TEXT("authoring_guard"), TEXT("mutation tools still enforce temp/source-edit guards, overwrite flags, input type support, and graph-shape checks"));
    return CandidateObject;
}

UNiagaraScript* FindEmitterScriptForUsage(FVersionedNiagaraEmitterData* EmitterData, ENiagaraScriptUsage Usage)
{
    if (!EmitterData)
    {
        return nullptr;
    }

    TArray<UNiagaraScript*> EmitterScripts;
    EmitterData->GetScripts(EmitterScripts, false, false);
    // Match interpolated spawn scripts too: a ParticleSpawn module's output usage
    // is ParticleSpawnScript, but the emitter's script reports
    // ParticleSpawnScriptInterpolated when interpolated spawning is on. Exact
    // equality misses that and yields "no owning script", blocking module-input
    // writes on particle-spawn modules (ShapeLocation, AddVelocity, sprite size...).
    for (UNiagaraScript* Script : EmitterScripts)
    {
        if (Script && Script->IsEquivalentUsage(Usage))
        {
            return Script;
        }
    }
    return nullptr;
}

FNiagaraVariable BuildRapidIterationParameterForInput(
    const UNiagaraNodeFunctionCall* FunctionCall,
    const FNiagaraVariable& InputVariable,
    const FString& EmitterName,
    ENiagaraScriptUsage ScriptUsage)
{
    if (!FunctionCall || EmitterName.IsEmpty())
    {
        return FNiagaraVariable();
    }

    const FNiagaraParameterHandle InputHandle(InputVariable.GetName());
    const FNiagaraParameterHandle ModuleHandle = InputHandle.IsModuleHandle()
        ? InputHandle
        : FNiagaraParameterHandle::CreateModuleParameterHandle(InputVariable.GetName());
    const FNiagaraParameterHandle AliasedHandle = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(ModuleHandle, FunctionCall);
    FNiagaraVariable AliasedVariable(InputVariable.GetType(), AliasedHandle.GetParameterHandleString());
    return FNiagaraUtilities::ConvertVariableToRapidIterationConstantName(AliasedVariable, *EmitterName, ScriptUsage);
}

FNiagaraParameterHandle BuildAliasedModuleParameterHandleForInput(
    const UNiagaraNodeFunctionCall* FunctionCall,
    const FNiagaraVariable& InputVariable)
{
    if (!FunctionCall)
    {
        return FNiagaraParameterHandle();
    }

    const FNiagaraParameterHandle InputHandle(InputVariable.GetName());
    const FNiagaraParameterHandle ModuleHandle = InputHandle.IsModuleHandle()
        ? InputHandle
        : FNiagaraParameterHandle::CreateModuleParameterHandle(InputVariable.GetName());
    return FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(ModuleHandle, FunctionCall);
}

bool NiagaraModuleInputNameMatches(const FNiagaraVariable& Candidate, const FString& InputName)
{
    const FString CandidateName = Candidate.GetName().ToString();
    FString ShortName = CandidateName;
    ShortName.RemoveFromStart(TEXT("Module."));
    return CandidateName.Equals(InputName, ESearchCase::IgnoreCase) ||
        ShortName.Equals(InputName, ESearchCase::IgnoreCase);
}

UEdGraphPin* FindNiagaraStackFunctionInputOverridePin(
    const UNiagaraNodeFunctionCall* FunctionCall,
    const FNiagaraParameterHandle& AliasedInputHandle)
{
    if (!FunctionCall)
    {
        return nullptr;
    }

    UEdGraph* Graph = FunctionCall->GetGraph();
    if (!Graph)
    {
        return nullptr;
    }

    const FName TargetPinName = AliasedInputHandle.GetParameterHandleString();
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node || !ClassPathOrChild(Node->GetClass(),TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapSet")))
        {
            continue;
        }

        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin && Pin->Direction == EGPD_Input && Pin->PinName == TargetPinName)
            {
                return Pin;
            }
        }
    }

    return nullptr;
}

TSharedPtr<FJsonObject> BuildResolvedStackInputJson(
    const UNiagaraNodeFunctionCall* FunctionCall,
    const FNiagaraVariable& InputVariable,
    const TSet<FNiagaraVariable>& HiddenVariables,
    UNiagaraScript* OwningScript,
    const FString& EmitterName)
{
    TSharedPtr<FJsonObject> InputObject = MakeShared<FJsonObject>();
    InputObject->SetObjectField(TEXT("variable"), NiagaraVariableToJsonObject(InputVariable));
    InputObject->SetBoolField(TEXT("is_hidden"), HiddenVariables.Contains(InputVariable));
    InputObject->SetBoolField(TEXT("can_author_now"), false);

    if (!OwningScript)
    {
        InputObject->SetStringField(TEXT("value_source"), TEXT("no_owning_script"));
        return InputObject;
    }

    FNiagaraVariable RapidIterationParameter = BuildRapidIterationParameterForInput(
        FunctionCall,
        InputVariable,
        EmitterName,
        OwningScript->GetUsage());
    const uint8* ValueData = RapidIterationParameter.IsValid()
        ? ReadRapid(OwningScript,RapidIterationParameter)
        : nullptr;

    TSharedPtr<FJsonObject> RapidIterationObject = NiagaraVariableToJsonObject(RapidIterationParameter);
    RapidIterationObject->SetBoolField(TEXT("has_value"), ValueData != nullptr);
    if (ValueData)
    {
        RapidIterationParameter.SetData(ValueData);
        RapidIterationObject->SetField(TEXT("value"), NiagaraVariableDataToJsonValue(RapidIterationParameter));
        RapidIterationObject->SetStringField(TEXT("value_string"), RapidIterationParameter.ToString());
        InputObject->SetStringField(TEXT("value_source"), TEXT("rapid_iteration"));
    }
    else
    {
        RapidIterationObject->SetField(TEXT("value"), MakeShared<FJsonValueNull>());
        InputObject->SetStringField(TEXT("value_source"), TEXT("unresolved_default"));
    }
    InputObject->SetObjectField(TEXT("rapid_iteration_parameter"), RapidIterationObject);
    return InputObject;
}

TArray<TSharedPtr<FJsonValue>> BuildResolvedStackInputsJson(
    const UNiagaraNodeFunctionCall* FunctionCall,
    FVersionedNiagaraEmitter OwningEmitter,
    FVersionedNiagaraEmitterData* EmitterData,
    const FString& EmitterName,
    int32 MaxResolvedInputs)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    if (!FunctionCall)
    {
        return Values;
    }

    const ENiagaraScriptUsage OutputUsage = FNiagaraStackGraphUtilities::GetOutputNodeUsage(*FunctionCall);
    FCompileConstantResolver ConstantResolver(OwningEmitter, OutputUsage, FunctionCall->DebugState);

    TArray<FNiagaraVariable> InputVariables;
    TSet<FNiagaraVariable> HiddenVariables;
    QueryInputs(
        *FunctionCall,
        InputVariables,
        HiddenVariables,
        ConstantResolver,
        FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions::ModuleInputsOnly,
        true);
    if(!ContinueRead()){return Values;}

    UNiagaraScript* OwningScript = FindEmitterScriptForUsage(EmitterData, OutputUsage);
    const int32 SafeMaxInputs = FMath::Max(0, MaxResolvedInputs);
    for (int32 Index = 0; Index < InputVariables.Num() && Index < SafeMaxInputs; ++Index)
    {
        Values.Add(MakeShared<FJsonValueObject>(BuildResolvedStackInputJson(
            FunctionCall,
            InputVariables[Index],
            HiddenVariables,
            OwningScript,
            EmitterName)));
    }
    return Values;
}

TSharedPtr<FJsonObject> BuildModuleInputModuleJson(
    const UNiagaraNodeFunctionCall* FunctionCall,
    FVersionedNiagaraEmitter OwningEmitter,
    FVersionedNiagaraEmitterData* EmitterData,
    const FString& EmitterName,
    int32 EmitterIndex,
    int32 ModuleIndex,
    bool bIncludeLinkedSources,
    bool bIncludeResolvedStackInputs,
    int32 MaxCandidatesPerModule,
    int32 MaxResolvedInputsPerModule,
    int32& OutCandidateCount,
    TArray<TSharedPtr<FJsonObject>>& OutTopCandidates)
{
    TSharedPtr<FJsonObject> ModuleObject = BuildFunctionCallJson(FunctionCall, ModuleIndex, false);
    TArray<TSharedPtr<FJsonValue>> CandidateValues;
    if (!FunctionCall)
    {
        ModuleObject->SetArrayField(TEXT("input_candidates"), CandidateValues);
        ModuleObject->SetNumberField(TEXT("input_candidate_count"), 0);
        return ModuleObject;
    }

    const TArray<TSharedPtr<FJsonValue>> ResolvedStackInputValues = bIncludeResolvedStackInputs
        ? BuildResolvedStackInputsJson(FunctionCall, OwningEmitter, EmitterData, EmitterName, MaxResolvedInputsPerModule)
        : TArray<TSharedPtr<FJsonValue>>();
    if(!ContinueRead()){return ModuleObject;}
    ModuleObject->SetBoolField(TEXT("resolved_stack_inputs_enabled"), bIncludeResolvedStackInputs);
    ModuleObject->SetNumberField(TEXT("resolved_stack_input_count"), ResolvedStackInputValues.Num());
    ModuleObject->SetArrayField(TEXT("resolved_stack_inputs"), ResolvedStackInputValues);

    TArray<UEdGraphPin*> InputPins;
    FunctionCall->GetInputPins(InputPins);
    int32 CandidateCount = 0;
    for (const UEdGraphPin* Pin : InputPins)
    {
        if (!IsCandidateNiagaraInputPin(Pin))
        {
            continue;
        }

        TSharedPtr<FJsonObject> CandidateObject = BuildModuleInputCandidateJson(
            FunctionCall,
            Pin,
            EmitterName,
            EmitterIndex,
            ModuleIndex,
            bIncludeLinkedSources);
        ++CandidateCount;
        ++OutCandidateCount;
        OutTopCandidates.Add(CandidateObject);
        if (CandidateValues.Num() < MaxCandidatesPerModule)
        {
            CandidateValues.Add(MakeShared<FJsonValueObject>(CandidateObject));
        }
    }

    ModuleObject->SetNumberField(TEXT("input_candidate_count"), CandidateCount);
    ModuleObject->SetArrayField(TEXT("input_candidates"), CandidateValues);
    return ModuleObject;
}

TArray<UNiagaraNodeFunctionCall*> CollectSortedNiagaraFunctionCalls(const UNiagaraScriptSourceBase* SourceBase)
{
    TArray<UNiagaraNodeFunctionCall*> FunctionCalls;
    const UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(SourceBase);
    UNiagaraGraph* Graph = Source ? Source->NodeGraph : nullptr;
    if (!Graph)
    {
        return FunctionCalls;
    }

    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (UNiagaraNodeFunctionCall* FunctionCall = Cast<UNiagaraNodeFunctionCall>(Node))
        {
            FunctionCalls.Add(FunctionCall);
        }
    }

    FunctionCalls.Sort(
        [](const UNiagaraNodeFunctionCall& Left, const UNiagaraNodeFunctionCall& Right)
        {
            if (Left.NodePosY == Right.NodePosY)
            {
                return Left.NodePosX < Right.NodePosX;
            }
            return Left.NodePosY < Right.NodePosY;
        });
    return FunctionCalls;
}

TSharedPtr<FJsonObject> BuildFunctionCallJson(const UNiagaraNodeFunctionCall* FunctionCall, int32 NodeIndex, bool bIncludePins)
{
    TSharedPtr<FJsonObject> NodeObject = MakeShared<FJsonObject>();
    if (!FunctionCall)
    {
        return NodeObject;
    }

    FString FunctionName = FunctionCall->GetFunctionName();
    if (FunctionName.IsEmpty())
    {
        FunctionName = FunctionCall->Signature.GetNameString();
    }

    const UNiagaraScript* FunctionScript = FunctionCall->FunctionScript;
    const UObject* ScriptOuter = FunctionScript ? FunctionScript->GetOuter() : nullptr;
    const bool bIsScratchPad = ScriptOuter && ScriptOuter->IsA<UNiagaraScratchPadContainer>();

    NodeObject->SetNumberField(TEXT("node_index"), NodeIndex);
    NodeObject->SetStringField(TEXT("node_name"), FunctionCall->GetName());
    NodeObject->SetStringField(TEXT("node_guid"), FunctionCall->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    NodeObject->SetStringField(TEXT("function_name"), FunctionName);
    NodeObject->SetStringField(TEXT("function_script"), NiagaraObjectPathOrEmpty(FunctionScript));
    NodeObject->SetStringField(TEXT("function_script_asset_object_path"), FunctionCall->FunctionScriptAssetObjectPath.ToString());
    NodeObject->SetStringField(TEXT("script_usage"), FunctionScript ? NiagaraScriptUsageName(FunctionScript->GetUsage()) : FString());
    NodeObject->SetStringField(TEXT("called_usage"), NiagaraScriptUsageName(FunctionCall->GetCalledUsage()));
    NodeObject->SetBoolField(TEXT("is_scratch_pad"), bIsScratchPad);
    NodeObject->SetStringField(TEXT("source_kind"), bIsScratchPad ? TEXT("scratch_pad") : (FunctionScript ? TEXT("script_asset") : TEXT("signature_only")));
    NodeObject->SetArrayField(TEXT("signature_inputs"), NiagaraVariablesToJson(FunctionCall->Signature.Inputs));
    NodeObject->SetArrayField(TEXT("signature_outputs"), NiagaraVariableBasesToJson(FunctionCall->Signature.Outputs));

    TArray<FString> SpecifierValues;
    for (const TPair<FName, FName>& Specifier : FunctionCall->FunctionSpecifiers)
    {
        AddUniqueString(SpecifierValues, FString::Printf(TEXT("%s=%s"), *Specifier.Key.ToString(), *Specifier.Value.ToString()));
    }
    NodeObject->SetArrayField(TEXT("function_specifiers"), StringsToJson(SpecifierValues));

    FString SearchText = FunctionName;
    SearchText += TEXT(" ");
    SearchText += NiagaraObjectPathOrEmpty(FunctionScript);
    for (const FNiagaraVariable& Variable : FunctionCall->Signature.Inputs)
    {
        SearchText += TEXT(" ");
        SearchText += Variable.GetName().ToString();
    }
    for (const FNiagaraVariable& Variable : FunctionCall->Signature.Outputs)
    {
        SearchText += TEXT(" ");
        SearchText += Variable.GetName().ToString();
    }
    NodeObject->SetArrayField(TEXT("control_hints"), StringsToJson(BuildNiagaraControlHints(SearchText)));

    if (bIncludePins)
    {
        TArray<UEdGraphPin*> InputPins;
        TArray<UEdGraphPin*> OutputPins;
        FunctionCall->GetInputPins(InputPins);
        FunctionCall->GetOutputPins(OutputPins);
        NodeObject->SetArrayField(TEXT("input_pins"), GraphPinsToJson(InputPins));
        NodeObject->SetArrayField(TEXT("output_pins"), GraphPinsToJson(OutputPins));
    }

    return NodeObject;
}

TSharedPtr<FJsonObject> BuildInputNodeJson(const UNiagaraNodeInput* InputNode)
{
    TSharedPtr<FJsonObject> InputObject = MakeShared<FJsonObject>();
    if (!InputNode)
    {
        return InputObject;
    }

    InputObject->SetStringField(TEXT("node_name"), InputNode->GetName());
    InputObject->SetStringField(TEXT("node_guid"), InputNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    InputObject->SetObjectField(TEXT("variable"), NiagaraVariableToJsonObject(InputNode->Input));
    InputObject->SetNumberField(TEXT("call_sort_priority"), InputNode->CallSortPriority);
    InputObject->SetBoolField(TEXT("is_exposed"), InputNode->IsExposed());
    InputObject->SetBoolField(TEXT("is_required"), InputNode->IsRequired());
    InputObject->SetBoolField(TEXT("is_hidden"), InputNode->IsHidden());
    InputObject->SetBoolField(TEXT("can_auto_bind"), InputNode->CanAutoBind());
    InputObject->SetObjectField(TEXT("object_asset"), BuildRenderTargetObjectJson(GetObjectPropertyForInspection(InputNode, TEXT("ObjectAsset"))));
    const UNiagaraDataInterface* DataInterface = Cast<UNiagaraDataInterface>(GetObjectPropertyForInspection(InputNode, TEXT("DataInterface")));
    if (DataInterface || InputNode->Input.GetType().IsDataInterface())
    {
        InputObject->SetObjectField(TEXT("data_interface"), BuildDataInterfaceInspectionJson(DataInterface,nullptr,false,120,512));
    }
    InputObject->SetArrayField(TEXT("control_hints"), StringsToJson(BuildNiagaraControlHints(InputNode->Input.GetName().ToString())));
    return InputObject;
}

TSharedPtr<FJsonObject> BuildUObjectReferenceJson(const UObject* Object)
{
    TSharedPtr<FJsonObject> ObjectJson = MakeShared<FJsonObject>();
    ObjectJson->SetBoolField(TEXT("is_valid"), Object != nullptr);
    if (!Object)
    {
        return ObjectJson;
    }

    ObjectJson->SetStringField(TEXT("name"), Object->GetName());
    ObjectJson->SetStringField(TEXT("path"), Object->GetPathName());
    ObjectJson->SetStringField(TEXT("class"), Object->GetClass() ? Object->GetClass()->GetName() : FString());
    ObjectJson->SetStringField(TEXT("class_path"), Object->GetClass() ? Object->GetClass()->GetPathName() : FString());
    ObjectJson->SetStringField(TEXT("outer_path"), Object->GetOuter() ? Object->GetOuter()->GetPathName() : FString());
    return ObjectJson;
}

TSharedPtr<FJsonObject> BuildRenderTargetObjectJson(const UObject* Object)
{
    TSharedPtr<FJsonObject> ObjectJson = BuildUObjectReferenceJson(Object);
    const UTextureRenderTarget* RenderTarget = Cast<UTextureRenderTarget>(Object);
    ObjectJson->SetBoolField(TEXT("is_texture_render_target"), RenderTarget != nullptr);
    if (RenderTarget)
    {
        ObjectJson->SetNumberField(TEXT("target_gamma"), RenderTarget->TargetGamma);
        ObjectJson->SetBoolField(TEXT("can_create_uav"), RenderTarget->bCanCreateUAV != 0);
        ObjectJson->SetBoolField(TEXT("needs_two_copies"), RenderTarget->bNeedsTwoCopies != 0);
    }

    const UTextureRenderTarget2D* RenderTarget2D = Cast<UTextureRenderTarget2D>(Object);
    ObjectJson->SetBoolField(TEXT("is_texture_render_target_2d"), RenderTarget2D != nullptr);
    if (RenderTarget2D)
    {
        ObjectJson->SetNumberField(TEXT("size_x"), RenderTarget2D->SizeX);
        ObjectJson->SetNumberField(TEXT("size_y"), RenderTarget2D->SizeY);
        ObjectJson->SetStringField(TEXT("render_target_format"), TextureRenderTargetFormatName(RenderTarget2D->RenderTargetFormat));

        TArray<TSharedPtr<FJsonValue>> ClearColorValues;
        ClearColorValues.Add(MakeShared<FJsonValueNumber>(RenderTarget2D->ClearColor.R));
        ClearColorValues.Add(MakeShared<FJsonValueNumber>(RenderTarget2D->ClearColor.G));
        ClearColorValues.Add(MakeShared<FJsonValueNumber>(RenderTarget2D->ClearColor.B));
        ClearColorValues.Add(MakeShared<FJsonValueNumber>(RenderTarget2D->ClearColor.A));
        ObjectJson->SetArrayField(TEXT("clear_color"), ClearColorValues);
    }
    return ObjectJson;
}

UObject* GetObjectPropertyForInspection(const UObject* Object, const FName PropertyName)
{
    const FObjectPropertyBase* Property = Object
        ? FindFProperty<FObjectPropertyBase>(Object->GetClass(), PropertyName)
        : nullptr;
    UObject* Value=Property?Property->GetObjectPropertyValue_InContainer(Object):nullptr;
    return Value&&Current&&!Current->Pin(Value)?nullptr:Value;
}

TSharedPtr<FJsonObject> BuildDataInterfaceInspectionJson(
    const UNiagaraDataInterface* DataInterface,
    UNiagaraSystem* System,
    bool bIncludeProperties,
    int32 MaxProperties,
    int32 MaxPropertyValueLength)
{
    TSharedPtr<FJsonObject> DataInterfaceJson = BuildUObjectReferenceJson(DataInterface);
    DataInterfaceJson->SetBoolField(TEXT("is_data_interface"), DataInterface != nullptr);
    DataInterfaceJson->SetBoolField(TEXT("is_render_target_2d_data_interface"), DataInterface && DataInterface->IsA<UNiagaraDataInterfaceRenderTarget2D>());
    DataInterfaceJson->SetBoolField(TEXT("properties_included"), bIncludeProperties && DataInterface != nullptr);
    if (bIncludeProperties && DataInterface)
    {
        DataInterfaceJson->SetObjectField(TEXT("properties"), BuildObjectPropertySnapshotJson(Current?Current->Mutable(DataInterface):nullptr, MaxProperties, MaxPropertyValueLength));
    }

    if(!ContinueRead()){return DataInterfaceJson;}
    const UNiagaraDataInterfaceRenderTarget2D* RenderTargetDataInterface = Cast<UNiagaraDataInterfaceRenderTarget2D>(DataInterface);
    if (RenderTargetDataInterface)
    {
        const FNiagaraVariable& UserParameter = RenderTargetDataInterface->RenderTargetUserParameter.Parameter;
        DataInterfaceJson->SetBoolField(TEXT("inherit_user_parameter_settings"), RenderTargetDataInterface->bInheritUserParameterSettings);
        DataInterfaceJson->SetObjectField(TEXT("render_target_user_parameter"), NiagaraVariableToJsonObject(UserParameter));
        DataInterfaceJson->SetStringField(TEXT("render_target_user_parameter_name"), UserParameter.GetName().ToString());
        DataInterfaceJson->SetBoolField(TEXT("has_render_target_user_parameter"), UserParameter.IsValid());

        const UObject* UserParameterObject = nullptr;
        if (System && UserParameter.IsValid())
        {
            UserParameterObject = System->GetExposedParameters().GetUObject(UserParameter).Get();
        }
        DataInterfaceJson->SetObjectField(TEXT("user_parameter_object"), BuildRenderTargetObjectJson(UserParameterObject));
    }
    return DataInterfaceJson;
}

TSharedPtr<FJsonObject> BuildNiagaraDataInterfaceOverrideJson(
    const UEdGraphPin* OverridePin,
    UNiagaraSystem* System,
    bool bIncludeDataInterfaceProperties,
    int32 MaxDataInterfaceProperties,
    int32 MaxDataInterfacePropertyValueLength)
{
    TSharedPtr<FJsonObject> OverrideJson = MakeShared<FJsonObject>();
    OverrideJson->SetBoolField(TEXT("has_override_pin"), OverridePin != nullptr);
    if (!OverridePin)
    {
        OverrideJson->SetNumberField(TEXT("link_count"), 0);
        return OverrideJson;
    }

    OverrideJson->SetStringField(TEXT("pin_name"), OverridePin->PinName.ToString());
    OverrideJson->SetStringField(TEXT("pin_type_category"), OverridePin->PinType.PinCategory.ToString());
    OverrideJson->SetStringField(TEXT("default_value"), OverridePin->DefaultValue);
    OverrideJson->SetStringField(TEXT("default_object"), NiagaraObjectPathOrEmpty(OverridePin->DefaultObject));
    OverrideJson->SetNumberField(TEXT("link_count"), OverridePin->LinkedTo.Num());

    if (OverridePin->LinkedTo.Num() != 1)
    {
        OverrideJson->SetBoolField(TEXT("has_single_linked_value"), false);
        return OverrideJson;
    }

    OverrideJson->SetBoolField(TEXT("has_single_linked_value"), true);

    const UEdGraphPin* LinkedPin = OverridePin->LinkedTo[0];
    const UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
    TSharedPtr<FJsonObject> LinkedNodeJson = MakeShared<FJsonObject>();
    LinkedNodeJson->SetBoolField(TEXT("is_valid"), LinkedNode != nullptr);
    if (LinkedNode)
    {
        LinkedNodeJson->SetStringField(TEXT("node_name"), LinkedNode->GetName());
        LinkedNodeJson->SetStringField(TEXT("node_guid"), LinkedNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
        LinkedNodeJson->SetStringField(TEXT("node_class"), LinkedNode->GetClass() ? LinkedNode->GetClass()->GetName() : FString());
        LinkedNodeJson->SetStringField(TEXT("node_class_path"), LinkedNode->GetClass() ? LinkedNode->GetClass()->GetPathName() : FString());
    }
    if (LinkedPin)
    {
        LinkedNodeJson->SetStringField(TEXT("linked_pin_name"), LinkedPin->PinName.ToString());
        LinkedNodeJson->SetStringField(TEXT("linked_pin_type_category"), LinkedPin->PinType.PinCategory.ToString());
        LinkedNodeJson->SetNumberField(TEXT("linked_pin_link_count"), LinkedPin->LinkedTo.Num());
    }
    OverrideJson->SetObjectField(TEXT("linked_node"), LinkedNodeJson);

    const UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(LinkedNode);
    OverrideJson->SetBoolField(TEXT("linked_node_is_input_node"), InputNode != nullptr);
    if (!InputNode)
    {
        return OverrideJson;
    }

    OverrideJson->SetObjectField(TEXT("input_node"), BuildInputNodeJson(InputNode));
    if(!ContinueRead()){return OverrideJson;}
    OverrideJson->SetObjectField(TEXT("object_asset"), BuildRenderTargetObjectJson(GetObjectPropertyForInspection(InputNode, TEXT("ObjectAsset"))));

    const UNiagaraDataInterface* DataInterface = Cast<UNiagaraDataInterface>(GetObjectPropertyForInspection(InputNode, TEXT("DataInterface")));
    OverrideJson->SetObjectField(
        TEXT("data_interface"),
        BuildDataInterfaceInspectionJson(
            DataInterface,
            System,
            bIncludeDataInterfaceProperties,
            MaxDataInterfaceProperties,
            MaxDataInterfacePropertyValueLength));
    return OverrideJson;
}

TSharedPtr<FJsonObject> HandleInspectNiagaraRapidIterationParameters(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FString SystemPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }

    TArray<FString> ContainsFilters;
    const TArray<TSharedPtr<FJsonValue>>* ContainsValues = nullptr;
    if (Params->TryGetArrayField(TEXT("contains"), ContainsValues) && ContainsValues)
    {
        for (const TSharedPtr<FJsonValue>& Value : *ContainsValues)
        {
            FString Filter;
            if (Value.IsValid() && Value->TryGetString(Filter) && !Filter.IsEmpty())
            {
                ContainsFilters.Add(Filter);
            }
        }
    }

    int32 MaxParametersPerScript = 256;
    Params->TryGetNumberField(TEXT("max_parameters_per_script"), MaxParametersPerScript);
    MaxParametersPerScript = FMath::Clamp(MaxParametersPerScript, 0, 2000);

    UNiagaraSystem* System = W.LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    auto MatchesFilters = [&ContainsFilters](const FString& Name)
    {
        if (ContainsFilters.IsEmpty())
        {
            return true;
        }
        for (const FString& Filter : ContainsFilters)
        {
            if (Name.Contains(Filter, ESearchCase::IgnoreCase))
            {
                return true;
            }
        }
        return false;
    };

    auto BuildScriptObject = [&MatchesFilters, MaxParametersPerScript](
        UNiagaraScript* Script,
        const FString& OwnerKind,
        const FString& OwnerName,
        int32 EmitterIndex,
        int32 ScriptIndex,
        int32& OutMatchedParameterCount)
    {
        TSharedPtr<FJsonObject> ScriptObject = MakeShared<FJsonObject>();
        if (!Script)
        {
            ScriptObject->SetBoolField(TEXT("valid"), false);
            return ScriptObject;
        }

        TArray<FNiagaraVariable> Parameters;
        Script->RapidIterationParameters.GetParameters(Parameters);

        TArray<TSharedPtr<FJsonValue>> ParameterValues;
        int32 MatchedCount = 0;
        bool bTruncated = false;
        for (const FNiagaraVariable& Parameter : Parameters)
        {
            const FString ParameterName = Parameter.GetName().ToString();
            if (!MatchesFilters(ParameterName))
            {
                continue;
            }

            ++MatchedCount;
            if (ParameterValues.Num() >= MaxParametersPerScript)
            {
                bTruncated = true;
                continue;
            }

            FNiagaraVariable ValueParameter(Parameter);
            const uint8* ValueData = ReadRapid(Script,Parameter);

            TSharedPtr<FJsonObject> ParameterObject = NiagaraVariableToJsonObject(Parameter);
            ParameterObject->SetBoolField(TEXT("has_value"), ValueData != nullptr);
            ParameterObject->SetArrayField(TEXT("control_hints"), StringsToJson(BuildNiagaraControlHints(ParameterName)));
            if (ValueData)
            {
                ValueParameter.SetData(ValueData);
                ParameterObject->SetField(TEXT("value"), NiagaraVariableDataToJsonValue(ValueParameter));
                ParameterObject->SetStringField(TEXT("value_string"), ValueParameter.ToString());
            }
            else
            {
                ParameterObject->SetField(TEXT("value"), MakeShared<FJsonValueNull>());
            }
            ParameterValues.Add(MakeShared<FJsonValueObject>(ParameterObject));
        }

        OutMatchedParameterCount += MatchedCount;

        ScriptObject->SetBoolField(TEXT("valid"), true);
        ScriptObject->SetStringField(TEXT("name"), Script->GetName());
        ScriptObject->SetStringField(TEXT("path"), Script->GetPathName());
        ScriptObject->SetStringField(TEXT("owner_kind"), OwnerKind);
        ScriptObject->SetStringField(TEXT("owner_name"), OwnerName);
        ScriptObject->SetNumberField(TEXT("emitter_index"), EmitterIndex);
        ScriptObject->SetNumberField(TEXT("script_index"), ScriptIndex);
        ScriptObject->SetStringField(TEXT("usage"), NiagaraScriptUsageName(Script->GetUsage()));
        ScriptObject->SetNumberField(TEXT("parameter_count"), Parameters.Num());
        ScriptObject->SetNumberField(TEXT("matched_parameter_count"), MatchedCount);
        ScriptObject->SetNumberField(TEXT("reported_parameter_count"), ParameterValues.Num());
        ScriptObject->SetBoolField(TEXT("truncated"), bTruncated);
        ScriptObject->SetArrayField(TEXT("parameters"), ParameterValues);
        return ScriptObject;
    };

    TArray<TSharedPtr<FJsonValue>> ScriptValues;
    int32 ScriptCount = 0;
    int32 MatchedParameterCount = 0;

    auto AddScript = [&BuildScriptObject, &ScriptValues, &ScriptCount, &MatchedParameterCount](
        UNiagaraScript* Script,
        const FString& OwnerKind,
        const FString& OwnerName,
        int32 EmitterIndex)
    {
        if (!Script)
        {
            return;
        }
        ScriptValues.Add(MakeShared<FJsonValueObject>(BuildScriptObject(
            Script,
            OwnerKind,
            OwnerName,
            EmitterIndex,
            ScriptCount,
            MatchedParameterCount)));
        ++ScriptCount;
    };

    AddScript(System->GetSystemSpawnScript(), TEXT("system"), TEXT("system_spawn"), INDEX_NONE);
    AddScript(System->GetSystemUpdateScript(), TEXT("system"), TEXT("system_update"), INDEX_NONE);

    int32 EmitterIndex = 0;
    for (const FNiagaraEmitterHandle& EmitterHandle : System->GetEmitterHandles())
    {
        if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
        {
            TArray<UNiagaraScript*> EmitterScripts;
            EmitterData->GetScripts(EmitterScripts, false, false);
            for (UNiagaraScript* Script : EmitterScripts)
            {
                AddScript(Script, TEXT("emitter"), EmitterHandle.GetName().ToString(), EmitterIndex);
            }
        }
        ++EmitterIndex;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetArrayField(TEXT("contains"), StringArrayToJsonValues(ContainsFilters));
    Result->SetNumberField(TEXT("script_count"), ScriptCount);
    Result->SetNumberField(TEXT("matched_parameter_count"), MatchedParameterCount);
    Result->SetNumberField(TEXT("max_parameters_per_script"), MaxParametersPerScript);
    Result->SetArrayField(TEXT("scripts"), ScriptValues);
    Result->SetStringField(TEXT("read_only_scope"), TEXT("rapid_iteration_parameter_inspection"));
    return Result;
#else
    return W.Error(TEXT("inspect_niagara_rapid_iteration_parameters requires editor-only Niagara data"));
#endif
}


TSharedPtr<FJsonObject> HandleSetNiagaraRapidIterationParameter(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FString SystemPath;
    FString ParameterName;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName) || ParameterName.IsEmpty())
    {
        return W.Error(TEXT("Missing 'parameter_name' parameter"));
    }

    const TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
    if (!Value.IsValid())
    {
        return W.Error(TEXT("Missing 'value' parameter"));
    }

    bool bAllowSourceEdit = false;
    Params->TryGetBoolField(TEXT("allow_source_edit"), bAllowSourceEdit);
    if (!bAllowSourceEdit && !IsTempGeneratedNiagaraPath(SystemPath))
    {
        return W.Error(
            FString::Printf(TEXT("Refusing to edit Niagara RapidIteration parameter outside %s: %s"), NiagaraTempGenerationRoot, *SystemPath));
    }

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);

    bool bRequestCompile = true;
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);

    FString TargetOwnerKind;
    FString TargetOwnerName;
    FString TargetScriptUsage;
    int32 TargetEmitterIndex = INDEX_NONE;
    Params->TryGetStringField(TEXT("owner_kind"), TargetOwnerKind);
    Params->TryGetStringField(TEXT("owner_name"), TargetOwnerName);
    Params->TryGetStringField(TEXT("script_usage"), TargetScriptUsage);
    Params->TryGetNumberField(TEXT("emitter_index"), TargetEmitterIndex);

    UNiagaraSystem* System = W.LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    struct FRapidIterationMatch
    {
        UNiagaraScript* Script = nullptr;
        FNiagaraVariable Parameter;
        FString OwnerKind;
        FString OwnerName;
        FString UsageName;
        int32 EmitterIndex = INDEX_NONE;
        int32 ScriptIndex = INDEX_NONE;
    };

    TArray<FRapidIterationMatch> Matches;

    int32 ScriptIndex = 0;
    auto CollectScriptMatches = [&](
        UNiagaraScript* Script,
        const FString& OwnerKind,
        const FString& OwnerName,
        int32 EmitterIndex)
    {
        if (!Script)
        {
            return;
        }

        const FString UsageName = NiagaraScriptUsageName(Script->GetUsage());
        const bool bOwnerMatches =
            (TargetOwnerKind.IsEmpty() || TargetOwnerKind.Equals(OwnerKind, ESearchCase::IgnoreCase)) &&
            (TargetOwnerName.IsEmpty() || TargetOwnerName.Equals(OwnerName, ESearchCase::IgnoreCase)) &&
            (TargetEmitterIndex == INDEX_NONE || TargetEmitterIndex == EmitterIndex) &&
            (TargetScriptUsage.IsEmpty() || TargetScriptUsage.Equals(UsageName, ESearchCase::IgnoreCase));
        if (!bOwnerMatches)
        {
            ++ScriptIndex;
            return;
        }

        TArray<FNiagaraVariable> Parameters;
        Script->RapidIterationParameters.GetParameters(Parameters);
        for (const FNiagaraVariable& Parameter : Parameters)
        {
            if (Parameter.GetName().ToString().Equals(ParameterName, ESearchCase::IgnoreCase))
            {
                FRapidIterationMatch Match;
                Match.Script = Script;
                Match.Parameter = Parameter;
                Match.OwnerKind = OwnerKind;
                Match.OwnerName = OwnerName;
                Match.UsageName = UsageName;
                Match.EmitterIndex = EmitterIndex;
                Match.ScriptIndex = ScriptIndex;
                Matches.Add(Match);
            }
        }

        ++ScriptIndex;
    };

    CollectScriptMatches(System->GetSystemSpawnScript(), TEXT("system"), TEXT("system_spawn"), INDEX_NONE);
    CollectScriptMatches(System->GetSystemUpdateScript(), TEXT("system"), TEXT("system_update"), INDEX_NONE);

    int32 EmitterIndex = 0;
    for (const FNiagaraEmitterHandle& EmitterHandle : System->GetEmitterHandles())
    {
        if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
        {
            TArray<UNiagaraScript*> EmitterScripts;
            EmitterData->GetScripts(EmitterScripts, false, false);
            for (UNiagaraScript* Script : EmitterScripts)
            {
                CollectScriptMatches(Script, TEXT("emitter"), EmitterHandle.GetName().ToString(), EmitterIndex);
            }
        }
        ++EmitterIndex;
    }

    if (Matches.IsEmpty())
    {
        return W.Error(FString::Printf(TEXT("No matching Niagara RapidIteration parameter was found: %s"), *ParameterName));
    }

    if (Matches.Num() > 1)
    {
        TArray<FString> MatchSamples;
        for (const FRapidIterationMatch& Match : Matches)
        {
            if (MatchSamples.Num() >= 24)
            {
                break;
            }
            MatchSamples.Add(FString::Printf(
                TEXT("%s/%s emitter_index=%d script_index=%d usage=%s"),
                *Match.OwnerKind,
                *Match.OwnerName,
                Match.EmitterIndex,
                Match.ScriptIndex,
                *Match.UsageName));
        }

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetBoolField(TEXT("success"), false);
        ResultObj->SetStringField(TEXT("error"), FString::Printf(
            TEXT("Multiple Niagara RapidIteration parameters matched '%s'; provide owner_kind, owner_name, script_usage, or emitter_index"),
            *ParameterName));
        ResultObj->SetStringField(TEXT("parameter_name"), ParameterName);
        ResultObj->SetNumberField(TEXT("match_count"), Matches.Num());
        ResultObj->SetArrayField(TEXT("matches"), StringArrayToJsonValues(MatchSamples));
        return ResultObj;
    }

    const FRapidIterationMatch& Match = Matches[0];
    UNiagaraScript* MatchedScript = Match.Script;
    const FNiagaraVariable& MatchedParameter = Match.Parameter;

    const uint8* ExistingData = ReadRapid(MatchedScript,MatchedParameter);
    if (!ExistingData)
    {
        return W.Error(FString::Printf(TEXT("RapidIteration parameter has no value data: %s"), *ParameterName));
    }

    FNiagaraVariable UpdatedParameter(MatchedParameter);
    UpdatedParameter.SetData(ExistingData);
    TSharedPtr<FJsonValue> PreviousValue = NiagaraVariableDataToJsonValue(UpdatedParameter);

    FString ErrorMessage;
    if (!SetNiagaraVariableDataFromJson(UpdatedParameter, Value, ErrorMessage))
    {
        return W.Error(ErrorMessage);
    }

    auto SelectedWrite=MakeShared<FJsonObject>();SelectedWrite->SetStringField(TEXT("script_path"),MatchedScript->GetPathName());SelectedWrite->SetStringField(TEXT("parameter_name"),UpdatedParameter.GetName().ToString());SelectedWrite->SetStringField(TEXT("parameter_type"),NiagaraTypeName(UpdatedParameter.GetType()));SelectedWrite->SetStringField(TEXT("owner_kind"),Match.OwnerKind);SelectedWrite->SetStringField(TEXT("owner_name"),Match.OwnerName);SelectedWrite->SetNumberField(TEXT("emitter_index"),Match.EmitterIndex);SelectedWrite->SetNumberField(TEXT("script_index"),Match.ScriptIndex);SelectedWrite->SetField(TEXT("previous_value"),PreviousValue);SelectedWrite->SetField(TEXT("requested_value"),Value);SelectedWrite->SetField(TEXT("expected_new_value"),NiagaraVariableDataToJsonValue(UpdatedParameter));W.R.Data->SetObjectField(TEXT("selected_RI_write"),SelectedWrite);
    if(!W.WriterTarget(MatchedScript)){return W.Error(W.R.Error);}
    W.R.bSaveRequested=bSave;W.R.bCompileRequested=bRequestCompile;
    if(!W.Modify(System,TEXT("system_Modify"))){return W.Error(W.R.Error);}
    if(!W.Modify(MatchedScript,TEXT("script_Modify"))){return W.Error(W.R.Error);}
    if(!W.SetParameter(MatchedScript,UpdatedParameter,false)){return W.Error(W.R.Error);}
    if(!W.Dirty()){return W.Error(W.R.Error);}
    if (bRequestCompile)
    {
        bool CompileAccepted=false;if(!W.Request(false,CompileAccepted)){return W.Error(W.R.Error);}
    }

    bool bSaved = false;
    if (bSave)
    {
        if(!W.Save(bSaved)){return W.Error(TEXT("Failed to save Niagara system; actual save return/prefix retained"));}
        if (!bSaved)
        {
            return W.Error(TEXT("Failed to save Niagara system after RapidIteration parameter update"));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetStringField(TEXT("parameter_name"), UpdatedParameter.GetName().ToString());
    Result->SetStringField(TEXT("parameter_type"), NiagaraTypeName(UpdatedParameter.GetType()));
    Result->SetStringField(TEXT("owner_kind"), Match.OwnerKind);
    Result->SetStringField(TEXT("owner_name"), Match.OwnerName);
    Result->SetNumberField(TEXT("emitter_index"), Match.EmitterIndex);
    Result->SetNumberField(TEXT("script_index"), Match.ScriptIndex);
    Result->SetStringField(TEXT("script_name"), MatchedScript->GetName());
    Result->SetStringField(TEXT("script_usage"), NiagaraScriptUsageName(MatchedScript->GetUsage()));
    Result->SetField(TEXT("previous_value"), PreviousValue);
    Result->SetField(TEXT("new_value"), NiagaraVariableDataToJsonValue(UpdatedParameter));
    Result->SetBoolField(TEXT("compile_requested"), bRequestCompile);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("write_scope"), TEXT("exact_rapid_iteration_parameter"));
    return Result;
#else
    return W.Error(TEXT("set_niagara_rapid_iteration_parameter requires editor-only Niagara data"));
#endif
}


TSharedPtr<FJsonObject> HandleSetNiagaraModuleStaticSwitch(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FString SystemPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }

    FString InputName;
    Params->TryGetStringField(TEXT("input_name"), InputName);
    InputName.TrimStartAndEndInline();
    const TSharedPtr<FJsonValue> RequestedValue = Params->TryGetField(TEXT("value"));
    const bool bHasValue = RequestedValue.IsValid() && RequestedValue->Type != EJson::Null;

    FString ExpectedCurrentValue;
    const bool bHasExpectedCurrentValue = Params->TryGetStringField(TEXT("expected_current_value"), ExpectedCurrentValue);
    ExpectedCurrentValue.TrimStartAndEndInline();

    bool bAllowSourceEdit = false;
    Params->TryGetBoolField(TEXT("allow_source_edit"), bAllowSourceEdit);
    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);
    bool bRequestCompile = true;
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);
    bool bWaitForCompletion = false;
    double TimeoutSeconds = 10.0;
    double PollIntervalSeconds = 0.1;
    ReadNiagaraCompileWaitParams(Params, bWaitForCompletion, TimeoutSeconds, PollIntervalSeconds);
    bool bRequireCompileGateForSave = false;
    Params->TryGetBoolField(TEXT("require_compile_gate_for_save"), bRequireCompileGateForSave);

    int32 TargetEmitterIndex = INDEX_NONE;
    int32 TargetModuleIndex = INDEX_NONE;
    FString TargetEmitterName;
    FString TargetModuleName;
    FString TargetModuleGuid;
    FString TargetScope;
    Params->TryGetNumberField(TEXT("emitter_index"), TargetEmitterIndex);
    Params->TryGetNumberField(TEXT("module_index"), TargetModuleIndex);
    Params->TryGetStringField(TEXT("emitter_name"), TargetEmitterName);
    Params->TryGetStringField(TEXT("module_name"), TargetModuleName);
    Params->TryGetStringField(TEXT("module_node_guid"), TargetModuleGuid);
    Params->TryGetStringField(TEXT("target_scope"), TargetScope);
    if (TargetScope.IsEmpty())
    {
        Params->TryGetStringField(TEXT("scope"), TargetScope);
    }
    TargetScope.TrimStartAndEndInline();
    const bool bTargetSystemScope = TargetScope.Equals(TEXT("system"), ESearchCase::IgnoreCase);

    const bool bWantsEdit = !InputName.IsEmpty() && bHasValue;
    if (bWantsEdit && bSave && bRequireCompileGateForSave && (!bRequestCompile || !bWaitForCompletion))
    {
        return W.Error(
            TEXT("Saving a static-switch mutation requires request_compile=true and wait_for_completion=true when require_compile_gate_for_save=true"));
    }
    if (bWantsEdit && !bAllowSourceEdit && !IsTempGeneratedNiagaraPath(SystemPath))
    {
        return W.Error(
            FString::Printf(TEXT("Refusing to edit Niagara module outside %s: %s"), NiagaraTempGenerationRoot, *SystemPath));
    }

    UNiagaraSystem* System = W.LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    UNiagaraNodeFunctionCall* MatchedFunctionCall = nullptr;
    FVersionedNiagaraEmitterData* MatchedEmitterData = nullptr;
    FVersionedNiagaraEmitter MatchedOwningEmitter;
    FString MatchedEmitterName;
    int32 MatchedEmitterIndex = INDEX_NONE;
    int32 MatchedModuleIndex = INDEX_NONE;
    FString MatchedModuleName;
    bool bMatchedSystemModule = false;

    if (bTargetSystemScope)
    {
        if (UNiagaraScript* SystemSpawnScript = System->GetSystemSpawnScript())
        {
            const TArray<UNiagaraNodeFunctionCall*> FunctionCalls = CollectSortedNiagaraFunctionCalls(SystemSpawnScript->GetLatestSource());
            for (int32 ModuleIndex = 0; ModuleIndex < FunctionCalls.Num(); ++ModuleIndex)
            {
                UNiagaraNodeFunctionCall* FunctionCall = FunctionCalls[ModuleIndex];
                if (!FunctionCall)
                {
                    continue;
                }
                FString FunctionName = FunctionCall->GetFunctionName();
                if (FunctionName.IsEmpty())
                {
                    FunctionName = FunctionCall->Signature.GetNameString();
                }
                const FString FunctionGuid = FunctionCall->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens);
                const bool bModuleMatches =
                    (TargetModuleIndex == INDEX_NONE || TargetModuleIndex == ModuleIndex) &&
                    (TargetModuleName.IsEmpty() || TargetModuleName.Equals(FunctionName, ESearchCase::IgnoreCase)) &&
                    (TargetModuleGuid.IsEmpty() || TargetModuleGuid.Equals(FunctionGuid, ESearchCase::IgnoreCase));
                if (bModuleMatches)
                {
                    MatchedFunctionCall = FunctionCall;
                    MatchedEmitterName = TEXT("system");
                    MatchedEmitterIndex = INDEX_NONE;
                    MatchedModuleIndex = ModuleIndex;
                    MatchedModuleName = FunctionName;
                    bMatchedSystemModule = true;
                    break;
                }
            }
        }
    }
    else
    {
        int32 CurrentEmitterIndex = 0;
        for (const FNiagaraEmitterHandle& EmitterHandle : System->GetEmitterHandles())
        {
            const FString EmitterName = EmitterHandle.GetName().ToString();
            const bool bEmitterMatches =
                (TargetEmitterIndex == INDEX_NONE || TargetEmitterIndex == CurrentEmitterIndex) &&
                (TargetEmitterName.IsEmpty() || TargetEmitterName.Equals(EmitterName, ESearchCase::IgnoreCase));
            if (bEmitterMatches)
            {
                if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
                {
                    const TArray<UNiagaraNodeFunctionCall*> FunctionCalls = CollectSortedNiagaraFunctionCalls(EmitterData->GraphSource);
                    for (int32 ModuleIndex = 0; ModuleIndex < FunctionCalls.Num(); ++ModuleIndex)
                    {
                        UNiagaraNodeFunctionCall* FunctionCall = FunctionCalls[ModuleIndex];
                        if (!FunctionCall)
                        {
                            continue;
                        }
                        FString FunctionName = FunctionCall->GetFunctionName();
                        if (FunctionName.IsEmpty())
                        {
                            FunctionName = FunctionCall->Signature.GetNameString();
                        }
                        const FString FunctionGuid = FunctionCall->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens);
                        const bool bModuleMatches =
                            (TargetModuleIndex == INDEX_NONE || TargetModuleIndex == ModuleIndex) &&
                            (TargetModuleName.IsEmpty() || TargetModuleName.Equals(FunctionName, ESearchCase::IgnoreCase)) &&
                            (TargetModuleGuid.IsEmpty() || TargetModuleGuid.Equals(FunctionGuid, ESearchCase::IgnoreCase));
                        if (bModuleMatches)
                        {
                            MatchedFunctionCall = FunctionCall;
                            MatchedEmitterData = EmitterData;
                            MatchedOwningEmitter = EmitterHandle.GetInstance();
                            MatchedEmitterName = EmitterName;
                            MatchedEmitterIndex = CurrentEmitterIndex;
                            MatchedModuleIndex = ModuleIndex;
                            MatchedModuleName = FunctionName;
                            break;
                        }
                    }
                }
            }
            if (MatchedFunctionCall)
            {
                break;
            }
            ++CurrentEmitterIndex;
        }
    }

    if (!MatchedFunctionCall || (!MatchedEmitterData && !bMatchedSystemModule))
    {
        return W.Error(TEXT("No matching Niagara module was found"));
    }

    TArray<UEdGraphPin*> SwitchPins;
    TSet<UEdGraphPin*> HiddenPins;
    if (bMatchedSystemModule)
    {
        for (UEdGraphPin* Pin : MatchedFunctionCall->Pins)
        {
            if (!Pin || Pin->Direction != EGPD_Input || Pin->PinName == TEXT("Module") || Pin->PinName == TEXT("Source"))
            {
                continue;
            }

            const FName PinCategory = Pin->PinType.PinCategory;
            if (PinCategory == UEdGraphSchema_Niagara::PinCategoryEnum ||
                PinCategory == UEdGraphSchema_Niagara::PinCategoryType ||
                PinCategory == UEdGraphSchema_Niagara::PinCategoryStaticClass ||
                PinCategory == UEdGraphSchema_K2::PC_Boolean)
            {
                SwitchPins.Add(Pin);
            }
        }
    }
    else
    {
        const ENiagaraScriptUsage OutputUsage = FNiagaraStackGraphUtilities::GetOutputNodeUsage(*MatchedFunctionCall);
        FCompileConstantResolver ConstantResolver(MatchedOwningEmitter, OutputUsage, MatchedFunctionCall->DebugState);
        if(!QueryStaticPins(*MatchedFunctionCall, SwitchPins, HiddenPins, ConstantResolver)){return W.Error(W.R.Error);}
    }

    TArray<TSharedPtr<FJsonValue>> SwitchList;
    UEdGraphPin* MatchedPin = nullptr;
    bool bMatchedPinHidden = false;
    const bool bHiddenStateKnown = !bMatchedSystemModule;
    auto IsTerminalMaxEnumEntry = [](const UEnum* EnumPtr, int32 EnumIndex)
    {
        if (!EnumPtr || EnumIndex < 0 || EnumIndex >= EnumPtr->NumEnums())
        {
            return true;
        }
        const FString InternalName = EnumPtr->GetNameStringByIndex(EnumIndex);
        const FString FullName = EnumPtr->GetNameByIndex(EnumIndex).ToString();
        return (EnumPtr->ContainsExistingMax() && EnumIndex == EnumPtr->NumEnums() - 1) ||
            InternalName.Equals(TEXT("MAX"), ESearchCase::IgnoreCase) ||
            InternalName.EndsWith(TEXT("_MAX"), ESearchCase::IgnoreCase) ||
            FullName.EndsWith(TEXT("::MAX"), ESearchCase::IgnoreCase) ||
            FullName.EndsWith(TEXT("_MAX"), ESearchCase::IgnoreCase);
    };
    auto IsSelectableEnumEntry = [&IsTerminalMaxEnumEntry](const UEnum* EnumPtr, int32 EnumIndex)
    {
        if (IsTerminalMaxEnumEntry(EnumPtr, EnumIndex))
        {
            return false;
        }
#if WITH_METADATA
        if (EnumPtr->HasMetaData(TEXT("Hidden"), EnumIndex) || EnumPtr->HasMetaData(TEXT("Spacer"), EnumIndex))
        {
            return false;
        }
#endif
        return true;
    };
    for (UEdGraphPin* Pin : SwitchPins)
    {
        if (!Pin)
        {
            continue;
        }
        const FString PinName = Pin->PinName.ToString();
        TSharedPtr<FJsonObject> SwObj = MakeShared<FJsonObject>();
        SwObj->SetStringField(TEXT("name"), PinName);
        SwObj->SetStringField(TEXT("current_value"), Pin->DefaultValue);
        SwObj->SetStringField(TEXT("category"), Pin->PinType.PinCategory.ToString());
        SwObj->SetStringField(TEXT("autogenerated_default_value"), Pin->AutogeneratedDefaultValue);
        SwObj->SetBoolField(TEXT("does_default_match_autogenerated"), Pin->DoesDefaultValueMatchAutogenerated());
        SwObj->SetBoolField(TEXT("hidden_known"), bHiddenStateKnown);
        SwObj->SetBoolField(TEXT("hidden"), bHiddenStateKnown && HiddenPins.Contains(Pin));

        const FNiagaraTypeDefinition SwitchType = UEdGraphSchema_Niagara::PinToTypeDefinition(Pin);
        SwObj->SetStringField(TEXT("input_type"), NiagaraTypeName(SwitchType));
        if (SwitchType.IsEnum() && SwitchType.GetEnum())
        {
            const UEnum* EnumPtr = SwitchType.GetEnum();
            TArray<TSharedPtr<FJsonValue>> Options;
            TArray<TSharedPtr<FJsonValue>> EnumOptions;
            for (int32 EnumIdx = 0; EnumIdx < EnumPtr->NumEnums(); ++EnumIdx)
            {
                const bool bSelectable = IsSelectableEnumEntry(EnumPtr, EnumIdx);
                const FString InternalName = EnumPtr->GetNameStringByIndex(EnumIdx);
                if (bSelectable)
                {
                    Options.Add(MakeShared<FJsonValueString>(InternalName));
                }

                TSharedPtr<FJsonObject> EnumOption = MakeShared<FJsonObject>();
                EnumOption->SetNumberField(TEXT("index"), EnumIdx);
                EnumOption->SetNumberField(TEXT("numeric_value"), EnumPtr->GetValueByIndex(EnumIdx));
                EnumOption->SetStringField(TEXT("internal_name"), InternalName);
                EnumOption->SetStringField(TEXT("full_name"), EnumPtr->GetNameByIndex(EnumIdx).ToString());
                EnumOption->SetStringField(TEXT("authored_name"), EnumPtr->GetAuthoredNameStringByIndex(EnumIdx));
                EnumOption->SetStringField(TEXT("display_name"), EnumPtr->GetDisplayNameTextByIndex(EnumIdx).ToString());
                EnumOption->SetBoolField(TEXT("terminal_max"), IsTerminalMaxEnumEntry(EnumPtr, EnumIdx));
#if WITH_METADATA
                EnumOption->SetBoolField(TEXT("hidden"), EnumPtr->HasMetaData(TEXT("Hidden"), EnumIdx));
#else
                EnumOption->SetBoolField(TEXT("hidden"), false);
#endif
                EnumOption->SetBoolField(TEXT("selectable"), bSelectable);
                EnumOptions.Add(MakeShared<FJsonValueObject>(EnumOption));
            }
            SwObj->SetArrayField(TEXT("options"), Options);
            SwObj->SetArrayField(TEXT("enum_options"), EnumOptions);
            SwObj->SetStringField(TEXT("enum_path"), EnumPtr->GetPathName());

            const int32 CurrentEnumIndex = EnumPtr->GetIndexByNameString(Pin->DefaultValue, EGetByNameFlags::None);
            SwObj->SetBoolField(TEXT("current_value_valid"),
                CurrentEnumIndex != INDEX_NONE && IsSelectableEnumEntry(EnumPtr, CurrentEnumIndex));
            if (CurrentEnumIndex != INDEX_NONE)
            {
                SwObj->SetNumberField(TEXT("current_enum_index"), CurrentEnumIndex);
                SwObj->SetNumberField(TEXT("current_enum_value"), EnumPtr->GetValueByIndex(CurrentEnumIndex));
                SwObj->SetStringField(TEXT("current_display_value"), EnumPtr->GetDisplayNameTextByIndex(CurrentEnumIndex).ToString());
            }
        }
        SwitchList.Add(MakeShared<FJsonValueObject>(SwObj));

        FString ShortName = PinName;
        ShortName.RemoveFromStart(TEXT("Module."));
        if (!InputName.IsEmpty() && (PinName.Equals(InputName, ESearchCase::IgnoreCase) || ShortName.Equals(InputName, ESearchCase::IgnoreCase)))
        {
            MatchedPin = Pin;
            bMatchedPinHidden = bHiddenStateKnown && HiddenPins.Contains(Pin);
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetStringField(TEXT("target_scope"), bMatchedSystemModule ? TEXT("system") : TEXT("emitter"));
    Result->SetStringField(TEXT("emitter_name"), MatchedEmitterName);
    Result->SetNumberField(TEXT("emitter_index"), MatchedEmitterIndex);
    Result->SetStringField(TEXT("module_name"), MatchedModuleName);
    Result->SetStringField(TEXT("module_node_guid"), MatchedFunctionCall->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    Result->SetNumberField(TEXT("module_index"), MatchedModuleIndex);
    Result->SetArrayField(TEXT("static_switches"), SwitchList);

    if (!bWantsEdit)
    {
        // Discovery mode: list available static switches and their values/options.
        Result->SetBoolField(TEXT("success"), true);
        Result->SetBoolField(TEXT("read_only"), true);
        Result->SetStringField(TEXT("write_scope"), TEXT("static_switch_inspect"));
        return Result;
    }

    if (!MatchedPin)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(TEXT("No matching static switch input: %s"), *InputName));
        return Result;
    }

    if (bMatchedPinHidden)
    {
        TSharedPtr<FJsonObject> Error = W.Error(
            FString::Printf(TEXT("Static switch input is hidden in the current compile context: %s"), *InputName));
        Error->SetStringField(TEXT("error_code"), TEXT("niagara_static_switch_hidden"));
        Error->SetStringField(TEXT("module_node_guid"), MatchedFunctionCall->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
        Error->SetStringField(TEXT("input_name"), MatchedPin->PinName.ToString());
        return Error;
    }

    const FString BeforeValue = MatchedPin->DefaultValue;
    if (bHasExpectedCurrentValue && !BeforeValue.Equals(ExpectedCurrentValue, ESearchCase::CaseSensitive))
    {
        TSharedPtr<FJsonObject> Error = W.Error(
            FString::Printf(
                TEXT("Static switch stale-value contract mismatch for %s. Expected '%s', found '%s'"),
                *MatchedPin->PinName.ToString(),
                *ExpectedCurrentValue,
                *BeforeValue));
        Error->SetStringField(TEXT("error_code"), TEXT("niagara_static_switch_stale_value"));
        Error->SetStringField(TEXT("expected_current_value"), ExpectedCurrentValue);
        Error->SetStringField(TEXT("actual_current_value"), BeforeValue);
        Error->SetBoolField(TEXT("mutation_applied_in_memory"), false);
        Error->SetBoolField(TEXT("saved"), false);
        return Error;
    }

    FString CanonicalValue;
    const FNiagaraTypeDefinition SwitchType = UEdGraphSchema_Niagara::PinToTypeDefinition(MatchedPin);
    const UEnum* ResolvedEnum = SwitchType.IsEnum() ? SwitchType.GetEnum() : nullptr;
    int32 ResolvedEnumIndex = INDEX_NONE;
    int64 ResolvedEnumNumericValue = INDEX_NONE;
    FString ResolvedEnumInternalName;
    FString ResolvedEnumDisplayName;
    FString ResolvedEnumAuthoredName;

    if (ResolvedEnum)
    {
        if (RequestedValue->Type == EJson::Number)
        {
            double NumericValue = 0.0;
            if (!RequestedValue->TryGetNumber(NumericValue) || !FMath::IsFinite(NumericValue))
            {
                return W.Error(TEXT("Static enum value must be a finite number"));
            }
            const double RoundedValue = FMath::RoundToDouble(NumericValue);
            if (NumericValue != RoundedValue || RoundedValue < static_cast<double>(MIN_int32) || RoundedValue > static_cast<double>(MAX_int32))
            {
                return W.Error(TEXT("Static enum numeric value must be an exact int32"));
            }
            ResolvedEnumNumericValue = static_cast<int64>(RoundedValue);
            ResolvedEnumIndex = ResolvedEnum->GetIndexByValue(ResolvedEnumNumericValue);
        }
        else if (RequestedValue->Type == EJson::String)
        {
            FString RequestedString;
            if (!RequestedValue->TryGetString(RequestedString))
            {
                return W.Error(TEXT("Static enum value must be a string or integer"));
            }
            RequestedString.TrimStartAndEndInline();
            if (RequestedString.IsEmpty())
            {
                return W.Error(TEXT("Static enum string value cannot be empty"));
            }

            ResolvedEnumIndex = ResolvedEnum->GetIndexByNameString(RequestedString, EGetByNameFlags::None);
            if (ResolvedEnumIndex == INDEX_NONE)
            {
                TSet<int32> MatchingIndices;
                for (int32 EnumIndex = 0; EnumIndex < ResolvedEnum->NumEnums(); ++EnumIndex)
                {
                    const FString InternalName = ResolvedEnum->GetNameStringByIndex(EnumIndex);
                    const FString FullName = ResolvedEnum->GetNameByIndex(EnumIndex).ToString();
                    const FString AuthoredName = ResolvedEnum->GetAuthoredNameStringByIndex(EnumIndex);
                    const FString DisplayName = ResolvedEnum->GetDisplayNameTextByIndex(EnumIndex).ToString();
                    if (InternalName.Equals(RequestedString, ESearchCase::IgnoreCase) ||
                        FullName.Equals(RequestedString, ESearchCase::IgnoreCase) ||
                        AuthoredName.Equals(RequestedString, ESearchCase::IgnoreCase) ||
                        DisplayName.Equals(RequestedString, ESearchCase::IgnoreCase))
                    {
                        MatchingIndices.Add(EnumIndex);
                    }
                }
                if (MatchingIndices.Num() == 1)
                {
                    for (int32 MatchingIndex : MatchingIndices)
                    {
                        ResolvedEnumIndex = MatchingIndex;
                    }
                }
                else if (MatchingIndices.Num() > 1)
                {
                    return W.Error(
                        FString::Printf(TEXT("Static enum value is ambiguous for %s: %s"), *ResolvedEnum->GetPathName(), *RequestedString));
                }
            }
        }
        else
        {
            return W.Error(TEXT("Static enum value must be a display/internal name string or integer"));
        }

        if (ResolvedEnumIndex == INDEX_NONE || !IsSelectableEnumEntry(ResolvedEnum, ResolvedEnumIndex))
        {
            return W.Error(
                FString::Printf(TEXT("Static enum value is invalid, hidden, or terminal for %s"), *ResolvedEnum->GetPathName()));
        }

        ResolvedEnumNumericValue = ResolvedEnum->GetValueByIndex(ResolvedEnumIndex);
        ResolvedEnumInternalName = ResolvedEnum->GetNameStringByIndex(ResolvedEnumIndex);
        ResolvedEnumDisplayName = ResolvedEnum->GetDisplayNameTextByIndex(ResolvedEnumIndex).ToString();
        ResolvedEnumAuthoredName = ResolvedEnum->GetAuthoredNameStringByIndex(ResolvedEnumIndex);

        FNiagaraVariable EnumVariable(SwitchType, NAME_None);
        FNiagaraInt32 EnumValue;
        EnumValue.Value = static_cast<int32>(ResolvedEnumNumericValue);
        EnumVariable.SetValue<FNiagaraInt32>(EnumValue);
        const UEdGraphSchema_Niagara* NiagaraSchema = GetDefault<UEdGraphSchema_Niagara>();
        if (!NiagaraSchema || !NiagaraSchema->TryGetPinDefaultValueFromNiagaraVariable(EnumVariable, CanonicalValue))
        {
            return W.Error(
                FString::Printf(TEXT("Failed to serialize static enum value for %s"), *ResolvedEnum->GetPathName()));
        }
        const int32 RoundTripIndex = ResolvedEnum->GetIndexByNameString(CanonicalValue, EGetByNameFlags::CaseSensitive);
        if (RoundTripIndex != ResolvedEnumIndex)
        {
            return W.Error(
                FString::Printf(TEXT("Static enum canonical serialization did not round-trip for %s"), *ResolvedEnum->GetPathName()));
        }
    }
    else if (SwitchType == FNiagaraTypeDefinition::GetBoolDef())
    {
        bool BoolValue = false;
        if (RequestedValue->Type == EJson::Boolean)
        {
            if (!RequestedValue->TryGetBool(BoolValue))
            {
                return W.Error(TEXT("Failed to parse static bool value"));
            }
        }
        else if (RequestedValue->Type == EJson::String)
        {
            FString BoolString;
            RequestedValue->TryGetString(BoolString);
            BoolString.TrimStartAndEndInline();
            if (BoolString.Equals(TEXT("true"), ESearchCase::IgnoreCase))
            {
                BoolValue = true;
            }
            else if (BoolString.Equals(TEXT("false"), ESearchCase::IgnoreCase))
            {
                BoolValue = false;
            }
            else
            {
                return W.Error(TEXT("Static bool string value must be 'true' or 'false'"));
            }
        }
        else
        {
            return W.Error(TEXT("Static bool value must be a boolean or true/false string"));
        }
        CanonicalValue = BoolValue ? TEXT("true") : TEXT("false");
    }
    else
    {
        if (!RequestedValue->TryGetString(CanonicalValue) || CanonicalValue.IsEmpty())
        {
            return W.Error(
                FString::Printf(TEXT("Unsupported non-string static switch value for type: %s"), *NiagaraTypeName(SwitchType)));
        }
    }

    Result->SetStringField(TEXT("input_name"), MatchedPin->PinName.ToString());
    Result->SetStringField(TEXT("input_type"), NiagaraTypeName(SwitchType));
    Result->SetStringField(TEXT("before"), BeforeValue);
    Result->SetStringField(TEXT("canonical_requested_value"), CanonicalValue);
    Result->SetBoolField(TEXT("hidden_known"), bHiddenStateKnown);
    Result->SetBoolField(TEXT("hidden"), bMatchedPinHidden);
    if (bHasExpectedCurrentValue)
    {
        Result->SetStringField(TEXT("expected_current_value"), ExpectedCurrentValue);
    }
    if (ResolvedEnum)
    {
        Result->SetStringField(TEXT("enum_path"), ResolvedEnum->GetPathName());
        Result->SetNumberField(TEXT("resolved_enum_index"), ResolvedEnumIndex);
        Result->SetNumberField(TEXT("resolved_enum_value"), ResolvedEnumNumericValue);
        Result->SetStringField(TEXT("resolved_enum_internal_name"), ResolvedEnumInternalName);
        Result->SetStringField(TEXT("resolved_enum_authored_name"), ResolvedEnumAuthoredName);
        Result->SetStringField(TEXT("resolved_enum_display_name"), ResolvedEnumDisplayName);
    }

    if (BeforeValue.Equals(CanonicalValue, ESearchCase::CaseSensitive))
    {
        Result->SetBoolField(TEXT("success"), true);
        Result->SetBoolField(TEXT("changed"), false);
        Result->SetBoolField(TEXT("verified_noop"), true);
        Result->SetStringField(TEXT("after"), BeforeValue);
        Result->SetBoolField(TEXT("compile_requested"), false);
        Result->SetBoolField(TEXT("compile_wait_pass"), true);
        Result->SetBoolField(TEXT("saved"), false);
        Result->SetBoolField(TEXT("mutation_applied_in_memory"), false);
        Result->SetStringField(TEXT("write_scope"), TEXT("module_static_switch_verified_noop"));
        return Result;
    }

    if(bWaitForCompletion&&!W.DeferredMode){W.Fail(TEXT("niagara_input_parameters_async_wait_unsupported"),TEXT("Required static-switch effect-proof wait/save continuation remains incomplete; no writer dispatched."));return W.Error(W.R.Error);}
    if(!bMatchedSystemModule&&(!MatchedOwningEmitter.Emitter||!W.WriterTarget(MatchedOwningEmitter.Emitter))){return W.Error(W.R.Error);}
    W.R.Data->SetObjectField(TEXT("selected_static_write"),Result);
    if(!W.WriterTarget(MatchedFunctionCall)){return W.Error(W.R.Error);}
    const TArray<FNiagaraScriptCompileSnapshot> BeforeCompileSnapshots = CaptureNiagaraSystemCompileSnapshots(System);
    if(!ContinueRead()){return W.Error(W.R.Error);}

    W.R.bSaveRequested=bSave;W.R.bCompileRequested=bRequestCompile;
    if(!W.Modify(System,TEXT("system_Modify"))){return W.Error(W.R.Error);}
    if (!bMatchedSystemModule)
    {
        if (UNiagaraEmitter* OwnerEmitter = MatchedOwningEmitter.Emitter)
        {
            if(!W.Modify(OwnerEmitter,TEXT("emitter_Modify"))){return W.Error(W.R.Error);}
        }
    }
    if(!W.Modify(MatchedFunctionCall,TEXT("function_Modify"))){return W.Error(W.R.Error);}
    if(!W.SDK(TEXT("pin_Modify"),true)){return W.Error(W.R.Error);}
    MatchedPin->Modify();W.Mutation->SetBoolField(TEXT("pin_Modify_returned"),true);if(!W.After(TEXT("pin_Modify"))){return W.Error(W.R.Error);}
    if(!W.SDK(TEXT("TrySetDefaultValue"),true)){return W.Error(W.R.Error);}
    if(!MatchedFunctionCall->Pins.Contains(MatchedPin)||MatchedPin->GetOwningNodeUnchecked()!=MatchedFunctionCall||!MatchedPin->DefaultValue.Equals(BeforeValue,ESearchCase::CaseSensitive)){W.Fail(TEXT("niagara_input_parameters_static_current_changed"),TEXT("Exact selected pin/current value changed before setter; no stale expected-current overwrite."));return W.Error(W.R.Error);}
    W.ExpectedMutableNodes.Add(MatchedFunctionCall);
    GetDefault<UEdGraphSchema_Niagara>()->TrySetDefaultValue(*MatchedPin, CanonicalValue);W.Mutation->SetBoolField(TEXT("TrySetDefaultValue_returned"),true);if(!W.After(TEXT("TrySetDefaultValue"))){return W.Error(W.R.Error);}
    if (!MatchedPin->DefaultValue.Equals(CanonicalValue, ESearchCase::CaseSensitive))
    {
        return W.Error(TEXT("Niagara static switch pin rejected the canonical value"));
    }
    {FWork::FExpectedPin E;E.Node=MatchedFunctionCall;E.Pin=MatchedPin;E.Id=MatchedPin->PinId;E.Value=CanonicalValue;W.ExpectedPins.Add(MoveTemp(E));}
    if(!W.SDK(TEXT("MarkNodeRequiresSynchronization"),true)){return W.Error(W.R.Error);}
    MatchedFunctionCall->MarkNodeRequiresSynchronization(TEXT("MCP: set static switch"), true);if(!W.After(TEXT("MarkNodeRequiresSynchronization"))){return W.Error(W.R.Error);}
    if(!W.Dirty()){return W.Error(W.R.Error);}

    const TArray<FNiagaraScriptCompileSnapshot> AfterMutationSnapshots = CaptureNiagaraSystemCompileSnapshots(System);
    if(!ContinueRead()){return W.Error(W.R.Error);}
    TArray<FString> AffectedScriptPaths;
    for (const FNiagaraScriptCompileSnapshot& AfterMutationSnapshot : AfterMutationSnapshots)
    {
        const FNiagaraScriptCompileSnapshot* BeforeSnapshot = FindNiagaraScriptCompileSnapshot(
            BeforeCompileSnapshots,
            AfterMutationSnapshot.ScriptPath);
        if (BeforeSnapshot &&
            AfterMutationSnapshot.ComputedId.IsValid() &&
            AfterMutationSnapshot.ComputedId != BeforeSnapshot->ComputedId)
        {
            AffectedScriptPaths.AddUnique(AfterMutationSnapshot.ScriptPath);
        }
    }

    bool bCompileLaunched = false;
    TArray<FNiagaraScriptCompileSnapshot> RequestedCompileSnapshots;
    if (bRequestCompile)
    {
        if(!W.Request(false,bCompileLaunched)){return W.Error(W.R.Error);}
        RequestedCompileSnapshots = CaptureNiagaraSystemCompileSnapshots(System);
        if(!ContinueRead()){return W.Error(W.R.Error);}
    }
    auto CompleteStatic=[=,&W](const TSharedPtr<FJsonObject>& WaitObject)->TSharedPtr<FJsonObject>
    {
    if(!ContinueRead()){W.R.Data->SetObjectField(TEXT("compile_wait_partial"),WaitObject);return W.Error(W.R.Error);}
    int32 CompileErrorCount = 0;
    int32 CompileNonReadyCount = 0;
    int32 CompileUnsynchronizedCount = 0;
    int32 CompileInputChangedAfterRequestCount = 0;
    int32 CompileSnapshotMissingCount = 0;
    TArray<TSharedPtr<FJsonValue>> CompileEffects;
    bool bCompileEffectVerified = false;
    if (bWaitForCompletion)
    {
        const TArray<FNiagaraScriptCompileSnapshot> FinalCompileSnapshots = CaptureNiagaraSystemCompileSnapshots(System);
        if(!ContinueRead()){return W.Error(W.R.Error);}
        for (const FString& AffectedScriptPath : AffectedScriptPaths)
        {
            const FNiagaraScriptCompileSnapshot* ExpectedSnapshot = FindNiagaraScriptCompileSnapshot(
                RequestedCompileSnapshots,
                AffectedScriptPath);
            const FNiagaraScriptCompileSnapshot* FinalSnapshot = FindNiagaraScriptCompileSnapshot(
                FinalCompileSnapshots,
                AffectedScriptPath);
            if (!ExpectedSnapshot || !FinalSnapshot)
            {
                ++CompileSnapshotMissingCount;
                continue;
            }

            const bool bCompileReady =
                FinalSnapshot->CompileStatus == ENiagaraScriptCompileStatus::NCS_UpToDate ||
                FinalSnapshot->CompileStatus == ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings ||
                FinalSnapshot->CompileStatus == ENiagaraScriptCompileStatus::NCS_ComputeUpToDateWithWarnings;
            const bool bCompileInputStable =
                FinalSnapshot->ComputedId.IsValid() &&
                FinalSnapshot->ComputedId == ExpectedSnapshot->ComputedId;
            const bool bCompiledDataSynchronized =
                FinalSnapshot->CachedId.IsValid() &&
                FinalSnapshot->ComputedId.IsValid() &&
                FinalSnapshot->CachedId == FinalSnapshot->ComputedId;
            const bool bHasError = IsNiagaraCompileErrorStatus(FinalSnapshot->CompileStatus);

            CompileErrorCount += bHasError ? 1 : 0;
            CompileNonReadyCount += bCompileReady ? 0 : 1;
            CompileUnsynchronizedCount += bCompiledDataSynchronized ? 0 : 1;
            CompileInputChangedAfterRequestCount += bCompileInputStable ? 0 : 1;

            TSharedPtr<FJsonObject> ScriptObject = BuildNiagaraScriptCompileSnapshotJson(
                *FinalSnapshot,
                CompileEffects.Num());
            ScriptObject->SetStringField(
                TEXT("expected_post_edit_id_fingerprint"),
                NiagaraCompileIdFingerprint(ExpectedSnapshot->ComputedId));
            ScriptObject->SetBoolField(TEXT("compile_ready"), bCompileReady);
            ScriptObject->SetBoolField(TEXT("compile_input_stable"), bCompileInputStable);
            ScriptObject->SetBoolField(TEXT("compiled_data_synchronized"), bCompiledDataSynchronized);
            CompileEffects.Add(MakeShared<FJsonValueObject>(ScriptObject));
        }

        const bool bFinalPinReadbackVerified =
            MatchedPin->DefaultValue.Equals(CanonicalValue, ESearchCase::CaseSensitive);
        bCompileEffectVerified =
            bCompileLaunched &&
            bFinalPinReadbackVerified &&
            AffectedScriptPaths.Num() > 0 &&
            CompileSnapshotMissingCount == 0 &&
            CompileNonReadyCount == 0 &&
            CompileUnsynchronizedCount == 0 &&
            CompileInputChangedAfterRequestCount == 0 &&
            CompileErrorCount == 0;
    }
    const bool bFinalPinReadbackVerified =
        MatchedPin->DefaultValue.Equals(CanonicalValue, ESearchCase::CaseSensitive);
    const bool bCompileIdle =
        !WaitObject->GetBoolField(TEXT("wait_timed_out")) &&
        !WaitObject->GetBoolField(TEXT("outstanding_compilation_requests_after")) &&
        !WaitObject->GetBoolField(TEXT("active_compilations_after"));
    const bool bCompileWaitPass = !bWaitForCompletion ||
        (bRequestCompile && bCompileIdle && bCompileEffectVerified);
    if(W.DeferredMode&&bWaitForCompletion){W.Compile->SetBoolField(TEXT("completion_verified"),bCompileWaitPass);W.Compile->SetBoolField(TEXT("effect_gate_evaluated"),true);if(bCompileIdle&&bRequestCompile)W.R.Compiled=bCompileWaitPass;}
    if (!bCompileWaitPass)
    {
        TSharedPtr<FJsonObject> Error = W.Error(
            TEXT("Niagara static switch compile did not prove ready synchronized compiled data; the in-memory mutation was not saved"));
        Error->SetStringField(TEXT("error_code"), TEXT("niagara_static_switch_compile_wait_failed"));
        Error->SetBoolField(TEXT("mutation_applied_in_memory"), true);
        Error->SetBoolField(TEXT("compile_requested"), bRequestCompile);
        Error->SetBoolField(TEXT("compile_launched"), bCompileLaunched);
        Error->SetBoolField(TEXT("compile_idle"), bCompileIdle);
        Error->SetBoolField(TEXT("compile_wait_pass"), false);
        Error->SetBoolField(TEXT("compile_effect_verified"), bCompileEffectVerified);
        Error->SetBoolField(TEXT("final_pin_readback_verified"), bFinalPinReadbackVerified);
        Error->SetNumberField(TEXT("affected_script_count"), AffectedScriptPaths.Num());
        Error->SetNumberField(TEXT("compile_error_count"), CompileErrorCount);
        Error->SetNumberField(TEXT("compile_non_ready_count"), CompileNonReadyCount);
        Error->SetNumberField(TEXT("compile_unsynchronized_count"), CompileUnsynchronizedCount);
        Error->SetNumberField(TEXT("compile_input_changed_after_request_count"), CompileInputChangedAfterRequestCount);
        Error->SetNumberField(TEXT("compile_snapshot_missing_count"), CompileSnapshotMissingCount);
        Error->SetArrayField(TEXT("compile_effects"), CompileEffects);
        Error->SetObjectField(TEXT("compile_wait"), WaitObject);
        Error->SetBoolField(TEXT("saved"), false);
        return Error;
    }

    bool bSaved = false;
    if (bSave)
    {
        if(!W.Save(bSaved)){return W.Error(TEXT("Failed to save Niagara system; actual save return/prefix retained"));}
        if (!bSaved)
        {
            return W.Error(TEXT("Failed to save Niagara system after static switch update"));
        }
    }

    Result->SetBoolField(TEXT("success"), true);
    Result->SetBoolField(TEXT("changed"), true);
    Result->SetBoolField(TEXT("verified_noop"), false);
    Result->SetStringField(TEXT("after"), MatchedPin->DefaultValue);
    Result->SetBoolField(TEXT("compile_requested"), bRequestCompile);
    Result->SetBoolField(TEXT("compile_launched"), bCompileLaunched);
    Result->SetBoolField(TEXT("compile_idle"), bCompileIdle);
    Result->SetBoolField(TEXT("compile_wait_pass"), bCompileWaitPass);
    Result->SetBoolField(TEXT("compile_effect_verified"), bCompileEffectVerified);
    Result->SetBoolField(TEXT("final_pin_readback_verified"), bFinalPinReadbackVerified);
    Result->SetNumberField(TEXT("affected_script_count"), AffectedScriptPaths.Num());
    Result->SetNumberField(TEXT("compile_error_count"), CompileErrorCount);
    Result->SetNumberField(TEXT("compile_non_ready_count"), CompileNonReadyCount);
    Result->SetNumberField(TEXT("compile_unsynchronized_count"), CompileUnsynchronizedCount);
    Result->SetNumberField(TEXT("compile_input_changed_after_request_count"), CompileInputChangedAfterRequestCount);
    Result->SetNumberField(TEXT("compile_snapshot_missing_count"), CompileSnapshotMissingCount);
    Result->SetArrayField(TEXT("compile_effects"), CompileEffects);
    Result->SetObjectField(TEXT("compile_wait"), WaitObject);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetBoolField(TEXT("mutation_applied_in_memory"), true);
    Result->SetStringField(TEXT("write_scope"), TEXT("module_static_switch"));
    return Result;
    };
    if(W.DeferredMode&&bWaitForCompletion){Result->SetBoolField(TEXT("success"),false);BeginInputWait(W,System,TimeoutSeconds,PollIntervalSeconds,MoveTemp(CompleteStatic));return Result;}
    return CompleteStatic(WaitForNiagaraCompileJson(System,bWaitForCompletion,TimeoutSeconds,PollIntervalSeconds));
#else
    return W.Error(TEXT("set_niagara_module_static_switch requires editor-only Niagara data"));
#endif
}


TSharedPtr<FJsonObject> HandleInspectNiagaraModuleInputs(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    FString SystemPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }

    bool bIncludeLinkedSources = true;
    Params->TryGetBoolField(TEXT("include_linked_sources"), bIncludeLinkedSources);

    bool bIncludeResolvedStackInputs = false;
    Params->TryGetBoolField(TEXT("include_resolved_stack_inputs"), bIncludeResolvedStackInputs);

    int32 MaxModules = 200;
    Params->TryGetNumberField(TEXT("max_modules"), MaxModules);
    MaxModules = FMath::Clamp(MaxModules, 0, 1000);

    int32 MaxCandidatesPerModule = 24;
    Params->TryGetNumberField(TEXT("max_candidates_per_module"), MaxCandidatesPerModule);
    MaxCandidatesPerModule = FMath::Clamp(MaxCandidatesPerModule, 0, 200);

    int32 MaxResolvedInputsPerModule = 8;
    Params->TryGetNumberField(TEXT("max_resolved_inputs_per_module"), MaxResolvedInputsPerModule);
    MaxResolvedInputsPerModule = FMath::Clamp(MaxResolvedInputsPerModule, 0, 200);

    int32 MaxTopCandidates = 80;
    Params->TryGetNumberField(TEXT("max_top_candidates"), MaxTopCandidates);
    MaxTopCandidates = FMath::Clamp(MaxTopCandidates, 0, 500);

    UNiagaraSystem* System = W.LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    TArray<TSharedPtr<FJsonValue>> EmitterValues;
    TArray<TSharedPtr<FJsonObject>> TopCandidateObjects;
    int32 TotalModuleCount = 0;
    int32 TotalCandidateCount = 0;

    int32 EmitterIndex = 0;
    for (const FNiagaraEmitterHandle& EmitterHandle : System->GetEmitterHandles())
    {
        TSharedPtr<FJsonObject> EmitterObject = MakeShared<FJsonObject>();
        const FString EmitterName = EmitterHandle.GetName().ToString();
        EmitterObject->SetStringField(TEXT("name"), EmitterName);
        EmitterObject->SetNumberField(TEXT("emitter_index"), EmitterIndex);
        EmitterObject->SetBoolField(TEXT("enabled"), EmitterHandle.GetIsEnabled());

        TArray<TSharedPtr<FJsonValue>> ModuleValues;
        if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
        {
            const FVersionedNiagaraEmitter OwningEmitter = EmitterHandle.GetInstance();
            const TArray<UNiagaraNodeFunctionCall*> FunctionCalls = CollectSortedNiagaraFunctionCalls(EmitterData->GraphSource);
            const int32 SafeModuleCount = FMath::Min(FunctionCalls.Num(), MaxModules);
            for (int32 ModuleIndex = 0; ModuleIndex < SafeModuleCount; ++ModuleIndex)
            {
                ModuleValues.Add(MakeShared<FJsonValueObject>(BuildModuleInputModuleJson(
                    FunctionCalls[ModuleIndex],
                    OwningEmitter,
                    EmitterData,
                    EmitterName,
                    EmitterIndex,
                    ModuleIndex,
                    bIncludeLinkedSources,
                    bIncludeResolvedStackInputs,
                    MaxCandidatesPerModule,
                    MaxResolvedInputsPerModule,
                    TotalCandidateCount,
                    TopCandidateObjects)));
                if(!ContinueRead()){return W.Error(W.R.Error);}
            }
            TotalModuleCount += FunctionCalls.Num();
            EmitterObject->SetBoolField(TEXT("modules_truncated"), FunctionCalls.Num() > SafeModuleCount);
            EmitterObject->SetNumberField(TEXT("module_count"), FunctionCalls.Num());
        }
        else
        {
            EmitterObject->SetBoolField(TEXT("modules_truncated"), false);
            EmitterObject->SetNumberField(TEXT("module_count"), 0);
        }

        EmitterObject->SetArrayField(TEXT("modules"), ModuleValues);
        EmitterValues.Add(MakeShared<FJsonValueObject>(EmitterObject));
        ++EmitterIndex;
    }

    TopCandidateObjects.Sort(
        [](const TSharedPtr<FJsonObject>& Left, const TSharedPtr<FJsonObject>& Right)
        {
            const double LeftPriority = Left.IsValid() ? Left->GetNumberField(TEXT("priority")) : 0.0;
            const double RightPriority = Right.IsValid() ? Right->GetNumberField(TEXT("priority")) : 0.0;
            if (!FMath::IsNearlyEqual(LeftPriority, RightPriority))
            {
                return LeftPriority > RightPriority;
            }
            const FString LeftModule = Left.IsValid() ? Left->GetStringField(TEXT("module_name")) : FString();
            const FString RightModule = Right.IsValid() ? Right->GetStringField(TEXT("module_name")) : FString();
            return LeftModule < RightModule;
        });

    TArray<TSharedPtr<FJsonValue>> TopCandidateValues;
    for (int32 Index = 0; Index < TopCandidateObjects.Num() && Index < MaxTopCandidates; ++Index)
    {
        TopCandidateValues.Add(MakeShared<FJsonValueObject>(TopCandidateObjects[Index]));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetBoolField(TEXT("include_linked_sources"), bIncludeLinkedSources);
    Result->SetBoolField(TEXT("include_resolved_stack_inputs"), bIncludeResolvedStackInputs);
    Result->SetNumberField(TEXT("emitter_count"), System->GetEmitterHandles().Num());
    Result->SetNumberField(TEXT("module_count"), TotalModuleCount);
    Result->SetNumberField(TEXT("candidate_count"), TotalCandidateCount);
    Result->SetNumberField(TEXT("top_candidate_count"), TopCandidateValues.Num());
    Result->SetBoolField(TEXT("can_author_module_inputs"), false);
    Result->SetStringField(TEXT("authoring_status"), TEXT("read_only; use this result as generation planning input before enabling temp-asset module writes"));
    Result->SetArrayField(TEXT("emitters"), EmitterValues);
    Result->SetArrayField(TEXT("top_candidates"), TopCandidateValues);
    return Result;
}


TSharedPtr<FJsonObject> HandleInspectNiagaraDataInterfaceOverrides(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    FString SystemPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }

    FString TargetInputName;
    Params->TryGetStringField(TEXT("input_name"), TargetInputName);

    int32 TargetEmitterIndex = INDEX_NONE;
    int32 TargetModuleIndex = INDEX_NONE;
    FString TargetEmitterName;
    FString TargetModuleName;
    FString TargetModuleGuid;
    Params->TryGetNumberField(TEXT("emitter_index"), TargetEmitterIndex);
    Params->TryGetNumberField(TEXT("module_index"), TargetModuleIndex);
    Params->TryGetStringField(TEXT("emitter_name"), TargetEmitterName);
    Params->TryGetStringField(TEXT("module_name"), TargetModuleName);
    Params->TryGetStringField(TEXT("module_node_guid"), TargetModuleGuid);

    int32 MaxModules = 200;
    Params->TryGetNumberField(TEXT("max_modules"), MaxModules);
    MaxModules = FMath::Clamp(MaxModules, 0, 1000);

    int32 MaxInputsPerModule = 64;
    Params->TryGetNumberField(TEXT("max_inputs_per_module"), MaxInputsPerModule);
    MaxInputsPerModule = FMath::Clamp(MaxInputsPerModule, 0, 200);

    bool bIncludeDataInterfaceProperties = false;
    Params->TryGetBoolField(TEXT("include_data_interface_properties"), bIncludeDataInterfaceProperties);

    int32 MaxDataInterfaceProperties = 120;
    Params->TryGetNumberField(TEXT("max_data_interface_properties"), MaxDataInterfaceProperties);
    MaxDataInterfaceProperties = FMath::Clamp(MaxDataInterfaceProperties, 0, 500);

    int32 MaxDataInterfacePropertyValueLength = 512;
    Params->TryGetNumberField(TEXT("max_data_interface_property_value_length"), MaxDataInterfacePropertyValueLength);
    MaxDataInterfacePropertyValueLength = FMath::Clamp(MaxDataInterfacePropertyValueLength, 64, 4096);

    UNiagaraSystem* System = W.LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    TArray<TSharedPtr<FJsonValue>> EmitterValues;
    int32 TotalScannedModuleCount = 0;
    int32 TotalMatchedModuleCount = 0;
    int32 TotalIncludedModuleCount = 0;
    int32 TotalDataInterfaceInputCount = 0;
    int32 TotalOverriddenInputCount = 0;

    int32 CurrentEmitterIndex = 0;
    for (const FNiagaraEmitterHandle& EmitterHandle : System->GetEmitterHandles())
    {
        const FString EmitterName = EmitterHandle.GetName().ToString();
        const bool bEmitterMatches =
            (TargetEmitterIndex == INDEX_NONE || TargetEmitterIndex == CurrentEmitterIndex) &&
            (TargetEmitterName.IsEmpty() || TargetEmitterName.Equals(EmitterName, ESearchCase::IgnoreCase));

        if (!bEmitterMatches)
        {
            ++CurrentEmitterIndex;
            continue;
        }

        TSharedPtr<FJsonObject> EmitterObject = MakeShared<FJsonObject>();
        EmitterObject->SetStringField(TEXT("name"), EmitterName);
        EmitterObject->SetNumberField(TEXT("emitter_index"), CurrentEmitterIndex);
        EmitterObject->SetBoolField(TEXT("enabled"), EmitterHandle.GetIsEnabled());

        TArray<TSharedPtr<FJsonValue>> ModuleValues;
        int32 EmitterScannedModuleCount = 0;
        int32 EmitterMatchedModuleCount = 0;
        int32 EmitterDataInterfaceInputCount = 0;
        int32 EmitterOverriddenInputCount = 0;
        bool bModulesTruncated = false;

        if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
        {
            const FVersionedNiagaraEmitter OwningEmitter = EmitterHandle.GetInstance();
            const TArray<UNiagaraNodeFunctionCall*> FunctionCalls = CollectSortedNiagaraFunctionCalls(EmitterData->GraphSource);
            EmitterScannedModuleCount = FunctionCalls.Num();
            TotalScannedModuleCount += FunctionCalls.Num();

            for (int32 ModuleIndex = 0; ModuleIndex < FunctionCalls.Num(); ++ModuleIndex)
            {
                UNiagaraNodeFunctionCall* FunctionCall = FunctionCalls[ModuleIndex];
                if (!FunctionCall)
                {
                    continue;
                }

                FString FunctionName = FunctionCall->GetFunctionName();
                if (FunctionName.IsEmpty())
                {
                    FunctionName = FunctionCall->Signature.GetNameString();
                }
                const FString FunctionGuid = FunctionCall->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens);
                const bool bModuleMatches =
                    (TargetModuleIndex == INDEX_NONE || TargetModuleIndex == ModuleIndex) &&
                    (TargetModuleName.IsEmpty() || TargetModuleName.Equals(FunctionName, ESearchCase::IgnoreCase)) &&
                    (TargetModuleGuid.IsEmpty() || TargetModuleGuid.Equals(FunctionGuid, ESearchCase::IgnoreCase));

                if (!bModuleMatches)
                {
                    continue;
                }

                const ENiagaraScriptUsage OutputUsage = FNiagaraStackGraphUtilities::GetOutputNodeUsage(*FunctionCall);
                FCompileConstantResolver ConstantResolver(OwningEmitter, OutputUsage, FunctionCall->DebugState);

                TArray<FNiagaraVariable> InputVariables;
                TSet<FNiagaraVariable> HiddenVariables;
                QueryInputs(
                    *FunctionCall,
                    InputVariables,
                    HiddenVariables,
                    ConstantResolver,
                    FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions::ModuleInputsOnly,
                    true);
    if(!ContinueRead()){return W.Error(W.R.Error);}

                TArray<TSharedPtr<FJsonValue>> DataInterfaceInputValues;
                int32 ModuleDataInterfaceInputCount = 0;
                int32 ModuleOverriddenInputCount = 0;
                bool bInputsTruncated = false;
                for (const FNiagaraVariable& InputVariable : InputVariables)
                {
                    if (!InputVariable.GetType().IsDataInterface())
                    {
                        continue;
                    }
                    if (!TargetInputName.IsEmpty() && !NiagaraModuleInputNameMatches(InputVariable, TargetInputName))
                    {
                        continue;
                    }

                    ++ModuleDataInterfaceInputCount;
                    ++EmitterDataInterfaceInputCount;
                    ++TotalDataInterfaceInputCount;

                    const FNiagaraParameterHandle AliasedInputHandle = BuildAliasedModuleParameterHandleForInput(FunctionCall, InputVariable);
                    const UEdGraphPin* OverridePin = FindNiagaraStackFunctionInputOverridePin(FunctionCall, AliasedInputHandle);
                    const int32 LinkCount = OverridePin ? OverridePin->LinkedTo.Num() : 0;
                    if (LinkCount > 0)
                    {
                        ++ModuleOverriddenInputCount;
                        ++EmitterOverriddenInputCount;
                        ++TotalOverriddenInputCount;
                    }

                    if (DataInterfaceInputValues.Num() >= MaxInputsPerModule)
                    {
                        bInputsTruncated = true;
                        continue;
                    }

                    TSharedPtr<FJsonObject> InputObject = MakeShared<FJsonObject>();
                    InputObject->SetObjectField(TEXT("variable"), NiagaraVariableToJsonObject(InputVariable));
                    InputObject->SetStringField(TEXT("input_name"), InputVariable.GetName().ToString());
                    InputObject->SetStringField(TEXT("input_type"), NiagaraTypeName(InputVariable.GetType()));
                    InputObject->SetBoolField(TEXT("is_hidden"), HiddenVariables.Contains(InputVariable));
                    InputObject->SetStringField(TEXT("aliased_input_handle"), AliasedInputHandle.GetParameterHandleString().ToString());
                    InputObject->SetBoolField(TEXT("has_override_pin"), OverridePin != nullptr);
                    InputObject->SetNumberField(TEXT("override_link_count"), LinkCount);
                    InputObject->SetObjectField(
                        TEXT("override"),
                        BuildNiagaraDataInterfaceOverrideJson(
                            OverridePin,
                            System,
                            bIncludeDataInterfaceProperties,
                            MaxDataInterfaceProperties,
                            MaxDataInterfacePropertyValueLength));
                    if(!ContinueRead()){return W.Error(W.R.Error);}
                    DataInterfaceInputValues.Add(MakeShared<FJsonValueObject>(InputObject));
                }

                if (ModuleDataInterfaceInputCount == 0)
                {
                    continue;
                }

                ++EmitterMatchedModuleCount;
                ++TotalMatchedModuleCount;

                if (ModuleValues.Num() >= MaxModules)
                {
                    bModulesTruncated = true;
                    continue;
                }

                TSharedPtr<FJsonObject> ModuleObject = BuildFunctionCallJson(FunctionCall, ModuleIndex, false);
                ModuleObject->SetStringField(TEXT("emitter_name"), EmitterName);
                ModuleObject->SetNumberField(TEXT("emitter_index"), CurrentEmitterIndex);
                ModuleObject->SetNumberField(TEXT("data_interface_input_count"), ModuleDataInterfaceInputCount);
                ModuleObject->SetNumberField(TEXT("overridden_input_count"), ModuleOverriddenInputCount);
                ModuleObject->SetBoolField(TEXT("data_interface_inputs_truncated"), bInputsTruncated);
                ModuleObject->SetArrayField(TEXT("data_interface_inputs"), DataInterfaceInputValues);
                ModuleValues.Add(MakeShared<FJsonValueObject>(ModuleObject));
                ++TotalIncludedModuleCount;
            }
        }

        EmitterObject->SetNumberField(TEXT("scanned_module_count"), EmitterScannedModuleCount);
        EmitterObject->SetNumberField(TEXT("matched_module_count"), EmitterMatchedModuleCount);
        EmitterObject->SetNumberField(TEXT("data_interface_input_count"), EmitterDataInterfaceInputCount);
        EmitterObject->SetNumberField(TEXT("overridden_input_count"), EmitterOverriddenInputCount);
        EmitterObject->SetBoolField(TEXT("modules_truncated"), bModulesTruncated);
        EmitterObject->SetArrayField(TEXT("modules"), ModuleValues);
        EmitterValues.Add(MakeShared<FJsonValueObject>(EmitterObject));

        ++CurrentEmitterIndex;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetBoolField(TEXT("read_only"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetStringField(TEXT("input_name_filter"), TargetInputName);
    Result->SetNumberField(TEXT("emitter_count"), System->GetEmitterHandles().Num());
    Result->SetNumberField(TEXT("scanned_module_count"), TotalScannedModuleCount);
    Result->SetNumberField(TEXT("matched_module_count"), TotalMatchedModuleCount);
    Result->SetNumberField(TEXT("included_module_count"), TotalIncludedModuleCount);
    Result->SetNumberField(TEXT("data_interface_input_count"), TotalDataInterfaceInputCount);
    Result->SetNumberField(TEXT("overridden_input_count"), TotalOverriddenInputCount);
    Result->SetBoolField(TEXT("include_data_interface_properties"), bIncludeDataInterfaceProperties);
    Result->SetStringField(TEXT("authoring_status"), TEXT("read_only; inspects Data Interface override graph nodes and user-parameter object bindings without compiling or saving"));
    Result->SetArrayField(TEXT("emitters"), EmitterValues);
    return Result;
}


TSharedPtr<FJsonObject> HandleSetNiagaraModuleInputValue(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    FString SystemPath;
    FString InputName;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("input_name"), InputName))
    {
        return W.Error(TEXT("Missing 'input_name' parameter"));
    }

    const TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
    if (!Value.IsValid())
    {
        return W.Error(TEXT("Missing 'value' parameter"));
    }

    bool bAllowSourceEdit = false;
    Params->TryGetBoolField(TEXT("allow_source_edit"), bAllowSourceEdit);
    if (!bAllowSourceEdit && !IsTempGeneratedNiagaraPath(SystemPath))
    {
        return W.Error(
            FString::Printf(TEXT("Refusing to edit Niagara module input outside %s: %s"), NiagaraTempGenerationRoot, *SystemPath));
    }

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);

    bool bRequestCompile = true;
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);

    bool bWaitForCompletion = false;
    double TimeoutSeconds = 10.0;
    double PollIntervalSeconds = 0.1;
    ReadNiagaraCompileWaitParams(Params, bWaitForCompletion, TimeoutSeconds, PollIntervalSeconds);

    int32 TargetEmitterIndex = INDEX_NONE;
    int32 TargetModuleIndex = INDEX_NONE;
    FString TargetEmitterName;
    FString TargetModuleName;
    FString TargetModuleGuid;
    Params->TryGetNumberField(TEXT("emitter_index"), TargetEmitterIndex);
    Params->TryGetNumberField(TEXT("module_index"), TargetModuleIndex);
    Params->TryGetStringField(TEXT("emitter_name"), TargetEmitterName);
    Params->TryGetStringField(TEXT("module_name"), TargetModuleName);
    Params->TryGetStringField(TEXT("module_node_guid"), TargetModuleGuid);

    UNiagaraSystem* System = W.LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    UNiagaraNodeFunctionCall* MatchedFunctionCall = nullptr;
    FVersionedNiagaraEmitterData* MatchedEmitterData = nullptr;
    FVersionedNiagaraEmitter MatchedOwningEmitter;
    FString MatchedEmitterName;
    int32 MatchedEmitterIndex = INDEX_NONE;
    int32 MatchedModuleIndex = INDEX_NONE;

    int32 CurrentEmitterIndex = 0;
    for (const FNiagaraEmitterHandle& EmitterHandle : System->GetEmitterHandles())
    {
        const FString EmitterName = EmitterHandle.GetName().ToString();
        const bool bEmitterMatches =
            (TargetEmitterIndex == INDEX_NONE || TargetEmitterIndex == CurrentEmitterIndex) &&
            (TargetEmitterName.IsEmpty() || TargetEmitterName.Equals(EmitterName, ESearchCase::IgnoreCase));

        if (bEmitterMatches)
        {
            if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
            {
                const TArray<UNiagaraNodeFunctionCall*> FunctionCalls = CollectSortedNiagaraFunctionCalls(EmitterData->GraphSource);
                for (int32 ModuleIndex = 0; ModuleIndex < FunctionCalls.Num(); ++ModuleIndex)
                {
                    UNiagaraNodeFunctionCall* FunctionCall = FunctionCalls[ModuleIndex];
                    if (!FunctionCall)
                    {
                        continue;
                    }

                    FString FunctionName = FunctionCall->GetFunctionName();
                    if (FunctionName.IsEmpty())
                    {
                        FunctionName = FunctionCall->Signature.GetNameString();
                    }
                    const FString FunctionGuid = FunctionCall->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens);
                    const bool bModuleMatches =
                        (TargetModuleIndex == INDEX_NONE || TargetModuleIndex == ModuleIndex) &&
                        (TargetModuleName.IsEmpty() || TargetModuleName.Equals(FunctionName, ESearchCase::IgnoreCase)) &&
                        (TargetModuleGuid.IsEmpty() || TargetModuleGuid.Equals(FunctionGuid, ESearchCase::IgnoreCase));

                    if (bModuleMatches)
                    {
                        MatchedFunctionCall = FunctionCall;
                        MatchedEmitterData = EmitterData;
                        MatchedOwningEmitter = EmitterHandle.GetInstance();
                        MatchedEmitterName = EmitterName;
                        MatchedEmitterIndex = CurrentEmitterIndex;
                        MatchedModuleIndex = ModuleIndex;
                        break;
                    }
                }
            }
        }

        if (MatchedFunctionCall)
        {
            break;
        }
        ++CurrentEmitterIndex;
    }

    if (!MatchedFunctionCall || !MatchedEmitterData)
    {
        return W.Error(TEXT("No matching Niagara module was found"));
    }

    const ENiagaraScriptUsage OutputUsage = FNiagaraStackGraphUtilities::GetOutputNodeUsage(*MatchedFunctionCall);
    UNiagaraScript* OwningScript = FindEmitterScriptForUsage(MatchedEmitterData, OutputUsage);
    if (!OwningScript)
    {
        return W.Error(TEXT("No owning Niagara script was found for the matched module"));
    }

    FCompileConstantResolver ConstantResolver(MatchedOwningEmitter, OutputUsage, MatchedFunctionCall->DebugState);
    TArray<FNiagaraVariable> InputVariables;
    TSet<FNiagaraVariable> HiddenVariables;
    QueryInputs(
        *MatchedFunctionCall,
        InputVariables,
        HiddenVariables,
        ConstantResolver,
        FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions::ModuleInputsOnly,
        true);
    if(!ContinueRead()){return W.Error(W.R.Error);}

    FNiagaraVariable* MatchedInputVariable = InputVariables.FindByPredicate(
        [&InputName](const FNiagaraVariable& Candidate)
        {
            const FString CandidateName = Candidate.GetName().ToString();
            FString ShortName = CandidateName;
            ShortName.RemoveFromStart(TEXT("Module."));
            return CandidateName.Equals(InputName, ESearchCase::IgnoreCase) ||
                ShortName.Equals(InputName, ESearchCase::IgnoreCase);
        });

    if (!MatchedInputVariable)
    {
        return W.Error(FString::Printf(TEXT("No matching module input was found: %s"), *InputName));
    }

    FNiagaraVariable RapidIterationParameter = BuildRapidIterationParameterForInput(
        MatchedFunctionCall,
        *MatchedInputVariable,
        MatchedEmitterName,
        OwningScript->GetUsage());

    const uint8* ExistingData = RapidIterationParameter.IsValid()
        ? ReadRapid(OwningScript,RapidIterationParameter)
        : nullptr;
    if (!ExistingData)
    {
        return W.Error(
            FString::Printf(TEXT("Refusing to create a new module input override. Existing RapidIteration value was not found for %s"), *MatchedInputVariable->GetName().ToString()));
    }

    RapidIterationParameter.SetData(ExistingData);
    TSharedPtr<FJsonValue> PreviousValue = NiagaraVariableDataToJsonValue(RapidIterationParameter);

    FString ErrorMessage;
    if (!SetNiagaraVariableDataFromJson(RapidIterationParameter, Value, ErrorMessage))
    {
        return W.Error(ErrorMessage);
    }

    auto SelectedWrite=MakeShared<FJsonObject>();SelectedWrite->SetStringField(TEXT("script_path"),OwningScript->GetPathName());SelectedWrite->SetStringField(TEXT("parameter_name"),RapidIterationParameter.GetName().ToString());SelectedWrite->SetStringField(TEXT("parameter_type"),NiagaraTypeName(RapidIterationParameter.GetType()));SelectedWrite->SetStringField(TEXT("module_name"),MatchedFunctionCall->GetFunctionName());SelectedWrite->SetStringField(TEXT("module_node_guid"),MatchedFunctionCall->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));SelectedWrite->SetStringField(TEXT("input_name"),MatchedInputVariable->GetName().ToString());SelectedWrite->SetNumberField(TEXT("emitter_index"),MatchedEmitterIndex);SelectedWrite->SetNumberField(TEXT("module_index"),MatchedModuleIndex);SelectedWrite->SetField(TEXT("previous_value"),PreviousValue);SelectedWrite->SetField(TEXT("requested_value"),Value);SelectedWrite->SetField(TEXT("expected_new_value"),NiagaraVariableDataToJsonValue(RapidIterationParameter));W.R.Data->SetObjectField(TEXT("selected_RI_write"),SelectedWrite);
    if(!W.WriterTarget(OwningScript)){return W.Error(W.R.Error);}
    if(bWaitForCompletion&&!W.DeferredMode){W.Fail(TEXT("niagara_input_parameters_async_wait_unsupported"),TEXT("Actual requested compile wait after save needs deferred final-return lifetime; no writer dispatched."));return W.Error(W.R.Error);}
    W.R.bSaveRequested=bSave;W.R.bCompileRequested=bRequestCompile;
    if(!W.Modify(System,TEXT("system_Modify"))){return W.Error(W.R.Error);}
    if(!W.Modify(OwningScript,TEXT("script_Modify"))){return W.Error(W.R.Error);}
    if(!W.SetParameter(OwningScript,RapidIterationParameter,false)){return W.Error(W.R.Error);}
    if(!W.Dirty()){return W.Error(W.R.Error);}
    if (bRequestCompile)
    {
        bool CompileAccepted=false;if(!W.Request(false,CompileAccepted)){return W.Error(W.R.Error);}
    }

    bool bSaved = false;
    if (bSave)
    {
        if(!W.Save(bSaved)){return W.Error(TEXT("Failed to save Niagara system; actual save return/prefix retained"));}
        if (!bSaved)
        {
            return W.Error(TEXT("Failed to save Niagara system after module input value update"));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetStringField(TEXT("emitter_name"), MatchedEmitterName);
    Result->SetNumberField(TEXT("emitter_index"), MatchedEmitterIndex);
    Result->SetStringField(TEXT("module_name"), MatchedFunctionCall->GetFunctionName());
    Result->SetStringField(TEXT("module_node_guid"), MatchedFunctionCall->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    Result->SetNumberField(TEXT("module_index"), MatchedModuleIndex);
    Result->SetStringField(TEXT("input_name"), MatchedInputVariable->GetName().ToString());
    Result->SetStringField(TEXT("input_type"), NiagaraTypeName(MatchedInputVariable->GetType()));
    Result->SetObjectField(TEXT("rapid_iteration_parameter"), NiagaraVariableWithDataToJsonObject(RapidIterationParameter));
    Result->SetField(TEXT("previous_value"), PreviousValue);
    Result->SetField(TEXT("new_value"), NiagaraVariableDataToJsonValue(RapidIterationParameter));
    Result->SetBoolField(TEXT("compile_requested"), bRequestCompile);
    if(W.DeferredMode&&bWaitForCompletion)
    {
        Result->SetBoolField(TEXT("success"),false);Result->SetBoolField(TEXT("saved"),bSaved);Result->SetStringField(TEXT("write_scope"),TEXT("existing_rapid_iteration_parameter_only"));
        BeginInputWait(W,System,TimeoutSeconds,PollIntervalSeconds,[Result](const TSharedPtr<FJsonObject>& WaitObject){Result->SetObjectField(TEXT("compile_wait"),WaitObject);Result->SetBoolField(TEXT("success"),true);return Result;});return Result;
    }
    Result->SetObjectField(TEXT("compile_wait"), WaitForNiagaraCompileJson(System, bWaitForCompletion, TimeoutSeconds, PollIntervalSeconds));
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("write_scope"), TEXT("existing_rapid_iteration_parameter_only"));
    return Result;
}


TSharedPtr<FJsonObject> HandleCreateNiagaraModuleInputOverride(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    FString SystemPath;
    FString InputName;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("input_name"), InputName))
    {
        return W.Error(TEXT("Missing 'input_name' parameter"));
    }

    const TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
    if (!Value.IsValid())
    {
        return W.Error(TEXT("Missing 'value' parameter"));
    }

    bool bAllowSourceEdit = false;
    Params->TryGetBoolField(TEXT("allow_source_edit"), bAllowSourceEdit);
    if (!bAllowSourceEdit && !IsTempGeneratedNiagaraPath(SystemPath))
    {
        return W.Error(
            FString::Printf(TEXT("Refusing to create Niagara module input override outside %s: %s"), NiagaraTempGenerationRoot, *SystemPath));
    }

    bool bOverwriteExisting = false;
    Params->TryGetBoolField(TEXT("overwrite_existing"), bOverwriteExisting);

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);

    bool bRequestCompile = true;
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);

    bool bWaitForCompletion = false;
    double TimeoutSeconds = 10.0;
    double PollIntervalSeconds = 0.1;
    ReadNiagaraCompileWaitParams(Params, bWaitForCompletion, TimeoutSeconds, PollIntervalSeconds);

    int32 TargetEmitterIndex = INDEX_NONE;
    int32 TargetModuleIndex = INDEX_NONE;
    FString TargetEmitterName;
    FString TargetModuleName;
    FString TargetModuleGuid;
    Params->TryGetNumberField(TEXT("emitter_index"), TargetEmitterIndex);
    Params->TryGetNumberField(TEXT("module_index"), TargetModuleIndex);
    Params->TryGetStringField(TEXT("emitter_name"), TargetEmitterName);
    Params->TryGetStringField(TEXT("module_name"), TargetModuleName);
    Params->TryGetStringField(TEXT("module_node_guid"), TargetModuleGuid);

    UNiagaraSystem* System = W.LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    UNiagaraNodeFunctionCall* MatchedFunctionCall = nullptr;
    FVersionedNiagaraEmitterData* MatchedEmitterData = nullptr;
    FVersionedNiagaraEmitter MatchedOwningEmitter;
    FString MatchedEmitterName;
    int32 MatchedEmitterIndex = INDEX_NONE;
    int32 MatchedModuleIndex = INDEX_NONE;

    int32 CurrentEmitterIndex = 0;
    for (const FNiagaraEmitterHandle& EmitterHandle : System->GetEmitterHandles())
    {
        const FString EmitterName = EmitterHandle.GetName().ToString();
        const bool bEmitterMatches =
            (TargetEmitterIndex == INDEX_NONE || TargetEmitterIndex == CurrentEmitterIndex) &&
            (TargetEmitterName.IsEmpty() || TargetEmitterName.Equals(EmitterName, ESearchCase::IgnoreCase));

        if (bEmitterMatches)
        {
            if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
            {
                const TArray<UNiagaraNodeFunctionCall*> FunctionCalls = CollectSortedNiagaraFunctionCalls(EmitterData->GraphSource);
                for (int32 ModuleIndex = 0; ModuleIndex < FunctionCalls.Num(); ++ModuleIndex)
                {
                    UNiagaraNodeFunctionCall* FunctionCall = FunctionCalls[ModuleIndex];
                    if (!FunctionCall)
                    {
                        continue;
                    }

                    FString FunctionName = FunctionCall->GetFunctionName();
                    if (FunctionName.IsEmpty())
                    {
                        FunctionName = FunctionCall->Signature.GetNameString();
                    }
                    const FString FunctionGuid = FunctionCall->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens);
                    const bool bModuleMatches =
                        (TargetModuleIndex == INDEX_NONE || TargetModuleIndex == ModuleIndex) &&
                        (TargetModuleName.IsEmpty() || TargetModuleName.Equals(FunctionName, ESearchCase::IgnoreCase)) &&
                        (TargetModuleGuid.IsEmpty() || TargetModuleGuid.Equals(FunctionGuid, ESearchCase::IgnoreCase));

                    if (bModuleMatches)
                    {
                        MatchedFunctionCall = FunctionCall;
                        MatchedEmitterData = EmitterData;
                        MatchedOwningEmitter = EmitterHandle.GetInstance();
                        MatchedEmitterName = EmitterName;
                        MatchedEmitterIndex = CurrentEmitterIndex;
                        MatchedModuleIndex = ModuleIndex;
                        break;
                    }
                }
            }
        }

        if (MatchedFunctionCall)
        {
            break;
        }
        ++CurrentEmitterIndex;
    }

    if (!MatchedFunctionCall || !MatchedEmitterData)
    {
        return W.Error(TEXT("No matching Niagara module was found"));
    }

    const ENiagaraScriptUsage OutputUsage = FNiagaraStackGraphUtilities::GetOutputNodeUsage(*MatchedFunctionCall);
    UNiagaraScript* OwningScript = FindEmitterScriptForUsage(MatchedEmitterData, OutputUsage);
    if (!OwningScript)
    {
        return W.Error(TEXT("No owning Niagara script was found for the matched module"));
    }

    FCompileConstantResolver ConstantResolver(MatchedOwningEmitter, OutputUsage, MatchedFunctionCall->DebugState);
    TArray<FNiagaraVariable> InputVariables;
    TSet<FNiagaraVariable> HiddenVariables;
    QueryInputs(
        *MatchedFunctionCall,
        InputVariables,
        HiddenVariables,
        ConstantResolver,
        FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions::ModuleInputsOnly,
        true);
    if(!ContinueRead()){return W.Error(W.R.Error);}

    FNiagaraVariable* MatchedInputVariable = InputVariables.FindByPredicate(
        [&InputName](const FNiagaraVariable& Candidate)
        {
            const FString CandidateName = Candidate.GetName().ToString();
            FString ShortName = CandidateName;
            ShortName.RemoveFromStart(TEXT("Module."));
            return CandidateName.Equals(InputName, ESearchCase::IgnoreCase) ||
                ShortName.Equals(InputName, ESearchCase::IgnoreCase);
        });

    if (!MatchedInputVariable)
    {
        return W.Error(FString::Printf(TEXT("No matching module input was found: %s"), *InputName));
    }

    FNiagaraVariable RapidIterationParameter = BuildRapidIterationParameterForInput(
        MatchedFunctionCall,
        *MatchedInputVariable,
        MatchedEmitterName,
        OwningScript->GetUsage());

    if (!RapidIterationParameter.IsValid())
    {
        return W.Error(TEXT("Failed to build RapidIteration parameter for module input"));
    }

    const uint8* ExistingData = ReadRapid(OwningScript,RapidIterationParameter);
    if (ExistingData && !bOverwriteExisting)
    {
        const FString RefusalError=FString::Printf(TEXT("RapidIteration override already exists for %s. Use overwrite_existing=true or set_niagara_module_input_value."), *MatchedInputVariable->GetName().ToString());
        if(W.DeferredMode&&W.R.Error.IsEmpty()&&W.Admit(TEXT("read_RI_bytes")))
        {
            const int32 Size=RapidIterationParameter.GetSizeInBytes(),Offset=OwningScript->RapidIterationParameters.IndexOf(RapidIterationParameter),DataNum=OwningScript->RapidIterationParameters.GetParameterDataArray().Num();
            if(Size>0&&Size<=65536&&Offset>=0&&Size<=DataNum&&Offset<=DataNum-Size)
            {
                W.ExistingRefusalCaptured=true;W.ExistingRefusalScript=OwningScript;W.ExistingRefusalVariable=RapidIterationParameter;W.ExistingRefusalBytes.Append(ExistingData,Size);W.ExistingRefusalOffset=Offset;W.ExistingRefusalDataNum=DataNum;W.ExistingRefusalError=RefusalError;
            }
        }
        return W.Error(RefusalError);
    }

    TSharedPtr<FJsonValue> PreviousValue = MakeShared<FJsonValueNull>();
    if (ExistingData)
    {
        RapidIterationParameter.SetData(ExistingData);
        PreviousValue = NiagaraVariableDataToJsonValue(RapidIterationParameter);
    }
    else
    {
        RapidIterationParameter.AllocateData();
    }

    FString ErrorMessage;
    if (!SetNiagaraVariableDataFromJson(RapidIterationParameter, Value, ErrorMessage))
    {
        return W.Error(ErrorMessage);
    }

    auto SelectedWrite=MakeShared<FJsonObject>();SelectedWrite->SetStringField(TEXT("script_path"),OwningScript->GetPathName());SelectedWrite->SetStringField(TEXT("parameter_name"),RapidIterationParameter.GetName().ToString());SelectedWrite->SetStringField(TEXT("parameter_type"),NiagaraTypeName(RapidIterationParameter.GetType()));SelectedWrite->SetStringField(TEXT("module_name"),MatchedFunctionCall->GetFunctionName());SelectedWrite->SetStringField(TEXT("module_node_guid"),MatchedFunctionCall->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));SelectedWrite->SetStringField(TEXT("input_name"),MatchedInputVariable->GetName().ToString());SelectedWrite->SetNumberField(TEXT("emitter_index"),MatchedEmitterIndex);SelectedWrite->SetNumberField(TEXT("module_index"),MatchedModuleIndex);SelectedWrite->SetField(TEXT("previous_value"),PreviousValue);SelectedWrite->SetField(TEXT("requested_value"),Value);SelectedWrite->SetField(TEXT("expected_new_value"),NiagaraVariableDataToJsonValue(RapidIterationParameter));W.R.Data->SetObjectField(TEXT("selected_RI_write"),SelectedWrite);
    if(!W.WriterTarget(OwningScript)){return W.Error(W.R.Error);}
    if(bWaitForCompletion&&!W.DeferredMode){W.Fail(TEXT("niagara_input_parameters_async_wait_unsupported"),TEXT("Actual requested compile wait after save needs deferred final-return lifetime; no writer dispatched."));return W.Error(W.R.Error);}
    W.R.bSaveRequested=bSave;W.R.bCompileRequested=bRequestCompile;
    if(!W.Modify(System,TEXT("system_Modify"))){return W.Error(W.R.Error);}
    if(!W.Modify(OwningScript,TEXT("script_Modify"))){return W.Error(W.R.Error);}
    if(!W.SetParameter(OwningScript,RapidIterationParameter,true)){return W.Error(W.R.Error);}
    if(!W.Dirty()){return W.Error(W.R.Error);}
    if (bRequestCompile)
    {
        bool CompileAccepted=false;if(!W.Request(false,CompileAccepted)){return W.Error(W.R.Error);}
    }

    bool bSaved = false;
    if (bSave)
    {
        if(!W.Save(bSaved)){return W.Error(TEXT("Failed to save Niagara system; actual save return/prefix retained"));}
        if (!bSaved)
        {
            return W.Error(TEXT("Failed to save Niagara system after module input override creation"));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetStringField(TEXT("emitter_name"), MatchedEmitterName);
    Result->SetNumberField(TEXT("emitter_index"), MatchedEmitterIndex);
    Result->SetStringField(TEXT("module_name"), MatchedFunctionCall->GetFunctionName());
    Result->SetStringField(TEXT("module_node_guid"), MatchedFunctionCall->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    Result->SetNumberField(TEXT("module_index"), MatchedModuleIndex);
    Result->SetStringField(TEXT("input_name"), MatchedInputVariable->GetName().ToString());
    Result->SetStringField(TEXT("input_type"), NiagaraTypeName(MatchedInputVariable->GetType()));
    Result->SetObjectField(TEXT("rapid_iteration_parameter"), NiagaraVariableWithDataToJsonObject(RapidIterationParameter));
    Result->SetBoolField(TEXT("created"), ExistingData == nullptr);
    Result->SetBoolField(TEXT("overwrote_existing"), ExistingData != nullptr);
    Result->SetField(TEXT("previous_value"), PreviousValue);
    Result->SetField(TEXT("new_value"), NiagaraVariableDataToJsonValue(RapidIterationParameter));
    Result->SetBoolField(TEXT("compile_requested"), bRequestCompile);
    if(W.DeferredMode&&bWaitForCompletion)
    {
        Result->SetBoolField(TEXT("success"),false);Result->SetBoolField(TEXT("saved"),bSaved);Result->SetStringField(TEXT("write_scope"),TEXT("new_or_explicitly_overwritten_rapid_iteration_parameter"));
        BeginInputWait(W,System,TimeoutSeconds,PollIntervalSeconds,[Result](const TSharedPtr<FJsonObject>& WaitObject){Result->SetObjectField(TEXT("compile_wait"),WaitObject);Result->SetBoolField(TEXT("success"),true);return Result;});return Result;
    }
    Result->SetObjectField(TEXT("compile_wait"), WaitForNiagaraCompileJson(System, bWaitForCompletion, TimeoutSeconds, PollIntervalSeconds));
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("write_scope"), TEXT("new_or_explicitly_overwritten_rapid_iteration_parameter"));
    return Result;
}



bool BoundValue(const TSharedPtr<FJsonValue>& V,int32 Depth,int64& Slots)
{if(!V.IsValid()||Depth>64||++Slots>262144){return false;}switch(V->Type){case EJson::None:return false;case EJson::Number:return FMath::IsFinite(V->AsNumber());case EJson::String:{const FString& Text=V->AsString();int32 NullAt=INDEX_NONE;return Text.Len()<=1048576&&(!Text.FindChar(TCHAR(0),NullAt)||NullAt==Text.Len());}case EJson::Array:for(const auto& Item:V->AsArray()){if(!BoundValue(Item,Depth+1,Slots)){return false;}}return true;case EJson::Object:for(const auto& Pair:V->AsObject()->Values){if(FString(Pair.Key).Len()>1024||!BoundValue(Pair.Value,Depth+1,Slots)){return false;}}return true;default:return true;}}
using FHandler=TSharedPtr<FJsonObject>(*)(FWork&,const TSharedPtr<FJsonObject>&);
bool ExactExistingOverrideRefusalKnown(FWork& W,const TSharedPtr<FJsonObject>& Legacy,const TCHAR* Command,const TSharedPtr<FJsonObject>& After,bool BeforeComplete,bool AfterComplete)
{
 if(!W.DeferredMode||!W.ExistingRefusalCaptured||FString(Command)!=TEXT("create_niagara_module_input_override")||W.CancelRequested||DeferredStopping.load()||!W.WorldCaptured||W.CapturedEditor.Get()!=GEditor)return false;
 if(W.R.ErrorCode!=TEXT("niagara_input_parameters_request_failed")||W.FailedStage!=TEXT("read_RI_bytes")||!W.R.Error.Equals(W.ExistingRefusalError,ESearchCase::CaseSensitive)||!Legacy.IsValid())return false;
 FString LegacyError;bool LegacySuccess=true;
 if(!Legacy->TryGetBoolField(TEXT("success"),LegacySuccess)||LegacySuccess||!Legacy->TryGetStringField(TEXT("error"),LegacyError)||!LegacyError.Equals(W.ExistingRefusalError,ESearchCase::CaseSensitive))return false;
 if(!W.R.bMutationAttempted||W.R.bChanged||W.R.bCompileAttempted||W.R.bSaveAttempted||W.R.bSaved||!W.R.Compiled.IsSet()||W.R.Compiled.GetValue()||!W.ExpectedRI.IsEmpty()||!W.ExpectedPins.IsEmpty()||!W.ExpectedMutableNodes.IsEmpty()||!W.Mutation->Values.IsEmpty()||!W.SavePrefix->Values.IsEmpty()||!W.Compile->Values.IsEmpty())return false;
 bool SourceAttempted=true,Allocated=false;FString Id;
 if(!W.R.Data->TryGetBoolField(TEXT("source_asset_mutation_attempted"),SourceAttempted)||SourceAttempted||!W.R.Data->TryGetBoolField(TEXT("job_allocation_nonnull"),Allocated)||!Allocated||!W.R.Data->TryGetStringField(TEXT("native_job_id"),Id)||Id.IsEmpty()||!Id.Equals(W.OwnedJobId,ESearchCase::CaseSensitive))return false;
 double Jobs=0,Loads=0,Queries=0;
 if(W.Counts->Values.Num()!=3||!W.Counts->TryGetNumberField(TEXT("NewObject_InputParameterJob"),Jobs)||Jobs!=1||!W.Counts->TryGetNumberField(TEXT("load_system"),Loads)||Loads!=1||!W.Counts->TryGetNumberField(TEXT("GetStackFunctionInputs"),Queries)||Queries!=1)return false;
 if(!BeforeComplete||!AfterComplete||!W.Before.IsValid()||!After.IsValid()||!Json(W.Before).Equals(Json(After),ESearchCase::CaseSensitive))return false;
 if(!W.Admit(TEXT("exact_existing_RI_refusal_no_change"),true)||!Live(W.OwnedJob.Get())||W.OwnedJob->bIsTerminal)return false;
 UNiagaraScript* Script=W.ExistingRefusalScript.Get();
 if(!Live(Script)||!W.Objects.ContainsByPredicate([&](const auto& I){return I->Object.Get()==Script;}))return false;
 const auto& Store=Script->RapidIterationParameters;TArray<FNiagaraVariable> Variables;Store.GetParameters(Variables);if(Variables.Num()>20000)return false;
 int32 ExactDeclarations=0;for(const auto& V:Variables)if(V.GetType()==W.ExistingRefusalVariable.GetType()&&V.GetName().ToString().Equals(W.ExistingRefusalVariable.GetName().ToString(),ESearchCase::CaseSensitive))++ExactDeclarations;
 const int32 Offset=Store.IndexOf(W.ExistingRefusalVariable),Size=W.ExistingRefusalBytes.Num();const auto& Bytes=Store.GetParameterDataArray();
 if(ExactDeclarations!=1||Offset!=W.ExistingRefusalOffset||Bytes.Num()!=W.ExistingRefusalDataNum||Offset<0||Size<=0||Size>65536||Size>Bytes.Num()||Offset>Bytes.Num()-Size||FMemory::Memcmp(Bytes.GetData()+Offset,W.ExistingRefusalBytes.GetData(),Size)!=0)return false;
 auto Proof=MakeShared<FJsonObject>();Proof->SetStringField(TEXT("scope"),TEXT("This exact existing-RI overwrite=False refusal only; captured target projection and selected typed RI remain unchanged. Job allocation and SDK/global cache unknown are preserved."));
 Proof->SetStringField(TEXT("script_path"),Script->GetPathName());Proof->SetStringField(TEXT("parameter_name"),W.ExistingRefusalVariable.GetName().ToString());Proof->SetNumberField(TEXT("selected_byte_count"),Size);
 Proof->SetBoolField(TEXT("same_owned_job"),true);Proof->SetBoolField(TEXT("same_editor_world_and_captured_bindings"),true);Proof->SetBoolField(TEXT("complete_before_after_equal"),true);Proof->SetBoolField(TEXT("same_RI_declaration_offset_size_and_bytes"),true);Proof->SetBoolField(TEXT("explicit_source_writer_compile_save_zero"),true);Proof->SetBoolField(TEXT("target_scoped_known_no_change"),true);
 W.R.Data->SetObjectField(TEXT("existing_RI_refusal_no_change_proof"),Proof);return true;
}

FString Finish(FWork& W,TSharedPtr<FJsonObject> Legacy,const TCHAR* Command,bool Writer)
{
    if(Legacy.IsValid()){
        int64 Slots=0;if(!BoundValue(MakeShared<FJsonValueObject>(Legacy),0,Slots)||Json(Legacy).Len()>8*1024*1024){W.Fail(TEXT("niagara_input_parameters_result_bound"),TEXT("Legacy result exceeds finite output bound; actual prefix retained."));Legacy=MakeShared<FJsonObject>();Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("error"),TEXT("Legacy result omitted for finite output bound"));W.R.Data->SetBoolField(TEXT("legacy_result_omitted_for_bound"),true);}W.R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
        bool Success=false;Legacy->TryGetBoolField(TEXT("success"),Success);if(!Success&&W.R.Error.IsEmpty()){FString Code,Error;Legacy->TryGetStringField(TEXT("error_code"),Code);Legacy->TryGetStringField(TEXT("error"),Error);W.Fail(Code.IsEmpty()?TEXT("niagara_input_parameters_original_failed"):*Code,Error.IsEmpty()?TEXT("Original operation failed; partial receipt retained."):Error);}if(W.R.ErrorCode==TEXT("niagara_input_parameters_request_failed")){FString Code;if(Legacy->TryGetStringField(TEXT("error_code"),Code)&&!Code.IsEmpty()){W.R.ErrorCode=Code;}}
        if(Success&&W.R.Error.IsEmpty()&&W.ExpectedReadback()){W.R.bPostconditionPassed=true;W.R.bSuccess=true;}
    }else if(W.R.Error.IsEmpty()){W.Fail(TEXT("niagara_input_parameters_no_result"),TEXT("No original result; no replay."));}
    auto After=W.Projection();W.R.Data->SetObjectField(TEXT("target_after"),After);
    bool BeforeComplete=false,AfterComplete=false;After->TryGetBoolField(TEXT("projection_complete"),AfterComplete);if(W.Before.IsValid()){W.Before->TryGetBoolField(TEXT("projection_complete"),BeforeComplete);W.R.Data->SetObjectField(TEXT("target_before"),W.Before);}
    const bool ScopedObservationComplete=BeforeComplete&&AfterComplete;
    if(ScopedObservationComplete)
    {
        const bool Changed=!Json(W.Before).Equals(Json(After),ESearchCase::CaseSensitive);W.R.bChanged|=Changed;W.R.Data->SetBoolField(TEXT("observed_projection_changed"),Changed);
        if(!Writer&&Changed&&W.R.Error.IsEmpty()){W.Fail(TEXT("niagara_input_parameters_observer_projection_changed"),TEXT("Direct target projection changed during query; cache/global purity not claimed."));}
    }
    const bool SuccessfulScopedKnown=W.R.bSuccess&&W.R.bPostconditionPassed&&W.R.Error.IsEmpty()&&ScopedObservationComplete&&W.ExpectedReadback()&&W.Admit(TEXT("final_scoped_target_observation"));
    const bool ExactRefusalKnown=!SuccessfulScopedKnown&&ExactExistingOverrideRefusalKnown(W,Legacy,Command,After,BeforeComplete,AfterComplete);
    const bool ScopedKnown=SuccessfulScopedKnown||ExactRefusalKnown;
    if(ScopedKnown){W.R.bChangeStateKnown=true;}
    W.R.Data->SetBoolField(TEXT("scoped_target_observation_complete"),ScopedObservationComplete);W.R.Data->SetBoolField(TEXT("scoped_target_change_state_known"),ScopedKnown);
    W.R.Data->SetStringField(TEXT("change_state_known_scope"),TEXT("Captured target identities/routing, baseline object package dirtiness, graph content and RI bytes with complete before/after projections, exact expected-value readback and final admission only. SDK/global cache effects and asynchronous compilation/render completion remain unverified."));
    if(!W.R.Error.IsEmpty()){W.R.bSuccess=false;W.R.bPostconditionPassed=false;W.R.Stage=W.FailedStage;W.R.Data->SetStringField(TEXT("failed_stage"),W.FailedStage);}else{W.R.Stage=TEXT("complete");}
    W.R.Data->SetNumberField(TEXT("baseline_object_projection_count"),W.ProjectionObjectLimit);W.R.Data->SetNumberField(TEXT("additional_query_objects_pinned"),FMath::Max(0,W.Objects.Num()-W.ProjectionObjectLimit));W.R.Data->SetBoolField(TEXT("additional_query_objects_have_before_projection"),false);if(!W.DeferredMode)W.R.Data->SetBoolField(TEXT("source_asset_mutation_attempted"),W.R.bMutationAttempted);bool SDKGlobalUnknown=false;W.R.Data->TryGetBoolField(TEXT("SDK_global_purity_unverified"),SDKGlobalUnknown);W.R.Data->SetBoolField(TEXT("total_change_state_unknown"),SDKGlobalUnknown||!W.R.bChangeStateKnown);W.R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);W.R.Data->SetStringField(TEXT("observation_scope"),TEXT("Captured target class/outer/package/path, emitter versions/script routing, exact graph nodes/pins/links and current RI bytes/dirty flags. Loader/cache/compile/save SDK global effects remain unknown. No cold disk/runtime/whole project purity proof."));W.R.Data->SetStringField(TEXT("wait_scope"),W.DeferredMode?TEXT("Owned finite GT ticks with real System polling. Static proof precedes save; value/override save precedes independent wait. Timeout is a diagnostic, not a compile success claim. SDK internal costs remain unbounded."):TEXT("Synchronous point observation only; changing waitTrue is refused before first writer without continuation."));return Cubeless::Operations::EncodeVariableResult(W.R,Command);
}
FString Run(const FString& ParamsJson,const TCHAR* Command,FHandler Handler,bool Writer)
{
    FWork W(Writer);if(Running||DeferredLeaseActive){W.Fail(TEXT("niagara_input_parameters_reentrant"),TEXT("Nested execution refused; no replay."));return Cubeless::Operations::EncodeVariableResult(W.R,Command);}TGuardValue<bool> Busy(Running,true);TGuardValue<FWork*> Active(Current,&W);TSharedPtr<FJsonObject> P;int64 Slots=0;
    W.R.Data->SetStringField(TEXT("operation"),Command);W.R.Data->SetBoolField(TEXT("operation_complete"),true);W.R.Data->SetBoolField(TEXT("async_final_return_implemented"),false);W.R.Data->SetBoolField(TEXT("blocking_wait_implemented"),false);W.R.Data->SetBoolField(TEXT("runtime_dependency_added"),false);
    if(ParamsJson.Len()>1048576||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ParamsJson),P,FJsonSerializer::EFlags::StoreNumbersAsStrings)||!P.IsValid()||!BoundValue(MakeShared<FJsonValueObject>(P),0,Slots)){W.Fail(TEXT("niagara_input_parameters_input_invalid"),TEXT("Bounded finite object JSON required; raw omission and values are not replaced with public defaults."));return Cubeless::Operations::EncodeVariableResult(W.R,Command);}
    W.R.Data->SetObjectField(TEXT("raw_native_params"),P);TSharedPtr<FJsonObject> Legacy;
    if(W.Admit(TEXT("entry"))){Legacy=Handler(W,P);}
    return Finish(W,Legacy,Command,Writer);
}
}
FString InspectRapidIteration(const FString& ParamsJson){return Run(ParamsJson,TEXT("inspect_niagara_rapid_iteration_parameters"),HandleInspectNiagaraRapidIterationParameters,false);}
FString SetRapidIteration(const FString& ParamsJson){return Run(ParamsJson,TEXT("set_niagara_rapid_iteration_parameter"),HandleSetNiagaraRapidIterationParameter,true);}
FString SetStaticSwitch(const FString& ParamsJson){return Run(ParamsJson,TEXT("set_niagara_module_static_switch"),HandleSetNiagaraModuleStaticSwitch,true);}
FString InspectModuleInputs(const FString& ParamsJson){return Run(ParamsJson,TEXT("inspect_niagara_module_inputs"),HandleInspectNiagaraModuleInputs,false);}
FString InspectDataInterfaces(const FString& ParamsJson){return Run(ParamsJson,TEXT("inspect_niagara_data_interface_overrides"),HandleInspectNiagaraDataInterfaceOverrides,false);}
FString SetInputValue(const FString& ParamsJson){return Run(ParamsJson,TEXT("set_niagara_module_input_value"),HandleSetNiagaraModuleInputValue,true);}
FString CreateInputOverride(const FString& ParamsJson){return Run(ParamsJson,TEXT("create_niagara_module_input_override"),HandleCreateNiagaraModuleInputOverride,true);}
}

namespace Cubeless::UE58::NiagaraInputParameterAsync
{
using namespace Cubeless::UE58::NiagaraInputParameters;
namespace
{
std::atomic<bool>& Stopping=DeferredStopping;
TSharedPtr<FJobState> ActiveJob;
TMap<const UCubelessNiagaraInputParameterJob*,FString> SnapshotCache;
TMap<const UCubelessNiagaraInputParameterJob*,TWeakObjectPtr<UCubelessNiagaraInputParameterJob>> CacheOwners;
// Unsafe records retain exact target/Job holds; Shutdown never drops them inside SDK calls or GC.
TArray<TSharedPtr<FJobState>>& UnsafeRecords(){static auto* Records=new TArray<TSharedPtr<FJobState>>();return *Records;}
enum class EPhase:uint8 { Prepare, Continue, Final, Terminal, Quarantined };
}
struct FJobState:public TSharedFromThis<FJobState>
{
 FWork Work{true};
 TStrongObjectPtr<UCubelessNiagaraInputParameterJob> Job{nullptr};
 UCubelessNiagaraInputParameterJob* RawJob=nullptr;
 TSharedPtr<FJsonObject> Params,Legacy;
 FString Operation,Id,LastSnapshot;
 EPhase Phase=EPhase::Prepare;
 FTSTicker::FDelegateHandle Ticker;
 FDelegateHandle PreExit;
 uint64 LastFrame=MAX_uint64;
 int64 Revision=0;
 bool Executing=false,Dispatched=false,FinalEncoded=false;
 explicit FJobState(const TCHAR* Op):Operation(Op),Id(FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens))
 {Work.DeferredMode=true;Work.R.Data->SetBoolField(TEXT("async_input_writer_continuation"),true);}
 bool PublicationSafe()const
 {
  if(!Safe()||Work.CapturedEditor.Get()!=GEditor)return false;
  if(Work.Writer&&(GEditor->GetPIEWorldContext()||GEditor->PlayWorld))return false;
  UWorld* EditorWorld=GEditor->GetEditorWorldContext().World();
  return Work.WorldCaptured&&Live(EditorWorld)&&!EditorWorld->bIsTearingDown&&Work.World.Get()==EditorWorld;
 }
 void Unhook()
 {
  if(Ticker.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(Ticker);Ticker.Reset();}
  if(PreExit.IsValid()){FCoreDelegates::OnEnginePreExit.Remove(PreExit);PreExit.Reset();}
 }
 void Metadata(bool Terminal)
 {
  auto& D=Work.R.Data;bool Persistent=false;D->TryGetBoolField(TEXT("source_asset_mutation_attempted"),Persistent);D->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),Persistent);D->SetBoolField(TEXT("transient_creation_only"),!Persistent);
  D->SetStringField(TEXT("native_job_id"),Id);
  D->SetNumberField(TEXT("native_checkpoint_revision"),++Revision);
  D->SetBoolField(TEXT("native_job_terminal"),Terminal);
  D->SetBoolField(TEXT("operation_complete"),Terminal);
  D->SetStringField(TEXT("operation"),Operation);
  D->SetBoolField(TEXT("automatic_retry_permitted"),false);
  D->SetBoolField(TEXT("async_core_dispatched"),Dispatched);
  
  
  
  
  
  D->SetBoolField(TEXT("timeout_does_not_imply_compile_success"),true);
  D->SetBoolField(TEXT("SDK_calls_guaranteed_nonblocking"),false);
  D->SetStringField(TEXT("continuation_scope"),TEXT("Exact existing input writer core on finite GT ticks. Static effect proof precedes save; value/override save precedes independent wait. Bounded timeout is not compiled success. No global pump/Sleep/replay; SDK calls may block internally."));
  D->SetObjectField(TEXT("native_call_counts"),Work.Counts);
  
  
  D->SetObjectField(TEXT("actual_save_prefix"),Work.SavePrefix);
  D->SetObjectField(TEXT("legacy_result"),Legacy.IsValid()?Legacy:MakeShared<FJsonObject>());
 }
 void Cache(const FString& Encoded,bool Terminal,bool ObjectSafe)
 {
  LastSnapshot=Encoded;
  if(RawJob)SnapshotCache.Add(RawJob,Encoded);
  if(ObjectSafe&&Job.IsValid())
  {
   Job->SnapshotJson=Encoded;
   // Snapshot precedes the reflected terminal property.
   Job->bIsTerminal=Terminal;
  }
 }
 void Quarantine(const FString& Reason)
 {
  if(Phase==EPhase::Quarantined)return;
  Unhook();
  Work.CancelRequested=true;Work.CancelReason=Reason;RefreshLegacy();
  if(Work.R.Error.IsEmpty())Work.Fail(TEXT("niagara_input_parameters_async_context_quarantined"),Reason);
  Work.R.bSuccess=false;Work.R.bPostconditionPassed=false;Work.R.bChangeStateKnown=false;
  Work.R.Data->SetBoolField(TEXT("unsafe_job_publication_quarantined"),true);
  Work.R.Data->SetBoolField(TEXT("native_writers_stopped"),true);
  Work.R.Data->SetBoolField(TEXT("do_not_retry"),true);
  if(!Legacy)Legacy=MakeShared<FJsonObject>();
  Legacy->SetBoolField(TEXT("success"),false);Legacy->SetBoolField(TEXT("operation_complete"),false);
  if(!Legacy->HasField(TEXT("error")))Legacy->SetStringField(TEXT("error"),Reason);
  Metadata(false);
  Phase=EPhase::Quarantined;
  Cache(Operations::EncodeVariableResult(Work.R,*Operation),false,false);
  // No Job-property/target access or strong-reference release in unsafe context.
  UnsafeRecords().AddUnique(AsShared());
 }
 void PublishPending()
 {
  Work.R.bSuccess=false;Work.R.bPostconditionPassed=false;
  Metadata(false);
  if(!PublicationSafe()){Quarantine(TEXT("Editor/world/GC/exit unavailable while publishing pending input writer snapshot."));return;}
  Cache(Operations::EncodeVariableResult(Work.R,*Operation),false,true);
 }
 void SetFailureLegacy()
 {
  if(!Legacy)Legacy=MakeShared<FJsonObject>();
  Legacy->SetBoolField(TEXT("success"),false);
  Legacy->SetStringField(TEXT("error"),Work.R.Error.IsEmpty()?TEXT("InputParameter job stopped; actual prefix retained."):Work.R.Error);
 }
 void Finalize()
 {
  if(FinalEncoded||Phase==EPhase::Terminal||Phase==EPhase::Quarantined)return;
  if(!PublicationSafe()){Quarantine(TEXT("InputParameter terminal publication lost its exact Editor world; no recovery or replay."));return;}
  if(!Work.R.Error.IsEmpty()){RefreshLegacy();SetFailureLegacy();}
  Metadata(true);
  LastSnapshot=Finish(Work,Legacy,*Operation,true);
  FinalEncoded=true;
  if(!PublicationSafe()){FinalEncoded=false;Quarantine(TEXT("InputParameter final guard lost publication context."));return;}
  Cache(LastSnapshot,true,true);
  Phase=EPhase::Terminal;Unhook();
 }
 void ObserveChangedPrefix()
 {
  if(!Work.R.Error.IsEmpty()||!Work.Before.IsValid()||!Work.Admit(TEXT("async_changed_prefix")))return;
  auto Projection=Work.Projection();bool Complete=false;Projection->TryGetBoolField(TEXT("projection_complete"),Complete);
  if(Complete){Work.R.bChanged|=!Json(Work.Before).Equals(Json(Projection),ESearchCase::CaseSensitive);Work.R.Data->SetBoolField(TEXT("current_partial_projection_complete"),true);}
 }
 void RefreshLegacy()
 {
  const TSharedPtr<FJsonObject>* Stored=nullptr;if(Work.R.Data->TryGetObjectField(TEXT("legacy_result"),Stored)&&Stored&&Stored->IsValid())Legacy=*Stored;
 }
 void ExecutePhase()
 {
  if(Work.CancelRequested||Stopping.load())
  {
   if(!Work.CancelRequested){Work.CancelRequested=true;Work.CancelReason=TEXT("Module shutdown stopped the owned input writer job.");}
   Work.Fail(TEXT("niagara_input_parameters_cancelled"),Work.CancelReason);
  }
  if(!Work.R.Error.IsEmpty())
  {
   RefreshLegacy();Phase=EPhase::Final;Finalize();return;
  }
  if(!Work.Admit(TEXT("async_conversion_phase_admission")))
  {
   RefreshLegacy();Phase=EPhase::Final;Finalize();return;
  }
  if(Phase==EPhase::Prepare)
  {
   if(Dispatched){Work.Fail(TEXT("niagara_input_parameters_async_replay_forbidden"),TEXT("Original operation already dispatched."));Phase=EPhase::Final;Finalize();return;}
   Dispatched=true;Legacy=Operation==TEXT("set_niagara_module_static_switch")?HandleSetNiagaraModuleStaticSwitch(Work,Params):Operation==TEXT("set_niagara_module_input_value")?HandleSetNiagaraModuleInputValue(Work,Params):HandleCreateNiagaraModuleInputOverride(Work,Params);if(Legacy.IsValid())Work.R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
   ObserveChangedPrefix();Phase=Work.DeferredStep?EPhase::Continue:EPhase::Final;
  }
  else if(Phase==EPhase::Continue)
  {
   const bool Complete=Work.DeferredStep();RefreshLegacy();if(Complete){ObserveChangedPrefix();Phase=EPhase::Final;}
  }
  else if(Phase==EPhase::Final){Finalize();return;}
  if(!Work.R.Error.IsEmpty()){RefreshLegacy();Phase=EPhase::Final;Finalize();return;}
  PublishPending();
 }
 void ReleaseSafe()
 {
  if(Executing||Phase!=EPhase::Terminal||!PublicationSafe())return;
  Work.DeferredStep=nullptr;
  Work.Objects.Reset();Job.Reset();
  if(ActiveJob.Get()==this){ActiveJob.Reset();DeferredLeaseActive=false;}
 }
 bool Tick(float)
 {
  TSharedRef<FJobState> Hold=AsShared();
  if(Phase==EPhase::Terminal||Phase==EPhase::Quarantined)return false;
  if(Executing||Running||LastFrame==GFrameCounter)return true;
  LastFrame=GFrameCounter;
  if(!PublicationSafe()){Quarantine(TEXT("InputParameter ticker lost live Editor/game-thread/world context."));return false;}
  {
   TGuardValue<bool> Busy(Executing,true);
   TGuardValue<bool> CoreLease(Running,true);
   TGuardValue<FWork*> Context(Current,&Work);
   ExecutePhase();
  }
  ReleaseSafe();
  return Phase!=EPhase::Terminal&&Phase!=EPhase::Quarantined;
 }
 void Hook()
 {
  const TWeakPtr<FJobState> Weak=AsShared();
  Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float Delta){auto State=Weak.Pin();return State.IsValid()&&State->Tick(Delta);}),0.0f);
  PreExit=FCoreDelegates::OnEnginePreExit.AddLambda([Weak](){Stopping.store(true);if(!IsInGameThread())return;auto State=Weak.Pin();if(State.IsValid()){State->Work.CancelRequested=true;State->Work.CancelReason=TEXT("Engine pre-exit stopped input writer job.");State->Unhook();if(!State->Executing)State->Quarantine(State->Work.CancelReason);}});
 }
};
namespace
{
void PruneSnapshots()
{
 for(auto It=CacheOwners.CreateIterator();It;++It)if(!It.Value().IsValid()){SnapshotCache.Remove(It.Key());It.RemoveCurrent();}
}
UCubelessNiagaraInputParameterJob* Start(const FString& Text,const TCHAR* Operation)
{
 if(!IsInGameThread()||Stopping.load()||Running||DeferredLeaseActive||!Safe())return nullptr;
 auto State=MakeShared<FJobState>(Operation);
 State->Work.R.bSaveRequested=true;
 if(!State->Work.Admit(TEXT("async_input_writer_start_admission")))return nullptr;
 PruneSnapshots();
 if(SnapshotCache.Num()>=256)return nullptr;
 ActiveJob=State;DeferredLeaseActive=true;
 UCubelessNiagaraInputParameterJob* Created=nullptr;
 {
  TGuardValue<bool> Busy(State->Executing,true);
  TGuardValue<bool> CoreLease(Running,true);
  TGuardValue<FWork*> Context(Current,&State->Work);
  if(State->Work.SDK(TEXT("NewObject_InputParameterJob"),true))
  {
   Created=NewObject<UCubelessNiagaraInputParameterJob>(GetTransientPackage());
   State->Work.R.Data->SetBoolField(TEXT("job_allocation_returned"),true);
   State->Work.R.Data->SetBoolField(TEXT("job_allocation_nonnull"),Created!=nullptr);
   State->RawJob=Created;
   if(Stopping.load()||State->Work.CancelRequested||!State->PublicationSafe())
   {
    State->Quarantine(TEXT("InputParameter Job allocation interrupted; persistent input writer dispatch not entered."));
    return nullptr;
   }
   if(!State->Work.After(TEXT("NewObject_InputParameterJob"))||!Created)
   {
    State->Quarantine(TEXT("InputParameter Job allocation could not be safely adopted."));
    return nullptr;
   }
   State->Job.Reset(Created);State->Work.OwnedJob=Created;State->Work.OwnedJobId=State->Id;CacheOwners.Add(Created,Created);
   int64 Slots=0;
    if(Text.Len()>1048576||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),State->Params,FJsonSerializer::EFlags::StoreNumbersAsStrings)||!State->Params.IsValid()||!BoundValue(MakeShared<FJsonValueObject>(State->Params),0,Slots))
   {
    State->Work.Fail(TEXT("niagara_input_parameters_invalid_params_json"),TEXT("Finite JSON object within1Mi/depth64/262144 slots required; raw presence/defaults are preserved."));
    State->Legacy=State->Work.Error(State->Work.R.Error);State->Phase=EPhase::Final;State->Finalize();
   }
   else
   {
    State->Work.R.Data->SetObjectField(TEXT("raw_native_params"),State->Params);State->Work.R.bSaveRequested=true;State->Work.R.bCompileRequested=true;State->Params->TryGetBoolField(TEXT("save"),State->Work.R.bSaveRequested);State->Params->TryGetBoolField(TEXT("request_compile"),State->Work.R.bCompileRequested);
    State->Legacy=MakeShared<FJsonObject>();State->Legacy->SetBoolField(TEXT("success"),false);State->Legacy->SetBoolField(TEXT("operation_complete"),false);
    State->PublishPending();
   }
  }
 }
 if(!Created)
 {
  if(State->Phase!=EPhase::Quarantined){ActiveJob.Reset();DeferredLeaseActive=false;}
  return nullptr;
 }
 if(State->Phase==EPhase::Terminal){State->ReleaseSafe();return Created;}
 if(State->Phase==EPhase::Quarantined)return Created;
 State->Hook();return Created;
}
}
UCubelessNiagaraInputParameterJob* StartStaticSwitch(const FString& ParamsJson){return Start(ParamsJson,TEXT("set_niagara_module_static_switch"));}
UCubelessNiagaraInputParameterJob* StartInputValue(const FString& ParamsJson){return Start(ParamsJson,TEXT("set_niagara_module_input_value"));}
UCubelessNiagaraInputParameterJob* StartInputOverride(const FString& ParamsJson){return Start(ParamsJson,TEXT("create_niagara_module_input_override"));}
FString SnapshotJson(const UCubelessNiagaraInputParameterJob* Job)
{
 if(!IsInGameThread()||!Job)return FString();
 if(const FString* Snapshot=SnapshotCache.Find(Job))return *Snapshot;
 return FString();
}
bool Cancel(UCubelessNiagaraInputParameterJob* Job,const FString& Reason)
{
 if(!IsInGameThread()||!Job||!ActiveJob.IsValid()||ActiveJob->RawJob!=Job)return false;
 auto State=ActiveJob;
 if(State->Phase==EPhase::Terminal||State->Phase==EPhase::Quarantined)return false;
 State->Work.CancelRequested=true;State->Work.CancelReason=Reason.Left(1024);State->Unhook();
 if(State->Executing)return true;
 if(!State->PublicationSafe()){State->Quarantine(TEXT("Cancellation lost input writer publication context; actual prefix retained."));return true;}
 {
  TGuardValue<bool> Busy(State->Executing,true);TGuardValue<bool> CoreLease(Running,true);TGuardValue<FWork*> Context(Current,&State->Work);
  State->Work.Fail(TEXT("niagara_input_parameters_async_cancelled"),State->Work.CancelReason);
  State->Phase=EPhase::Final;State->Finalize();
 }
 State->ReleaseSafe();return true;
}
void Shutdown(const FString& Reason)
{
 Stopping.store(true);
 if(!IsInGameThread())return;
 auto State=ActiveJob;if(!State.IsValid())return;
 State->Work.CancelRequested=true;State->Work.CancelReason=Reason.Left(1024);State->Unhook();
 if(State->Executing)return;
 if(!State->PublicationSafe()){State->Quarantine(TEXT("Module shutdown retained unsafe input writer state without touching UObject publication."));return;}
 Cancel(State->RawJob,State->Work.CancelReason);
}
}
