// GN dedicated Editor-only Niagara DataChannel/Scratch DI authoring. Source candidate; no Engine/build evidence.
#include "Compatibility/UE58/CubelessNiagaraDataChannelAuthoringApi.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/World.h"
#include "Misc/Crc.h"
#include "Misc/SecureHash.h"
#include "Misc/PackageName.h"
#include "NiagaraSystem.h"
#include "CubelessNiagaraDataChannelAuthoringLibrary.h"
#include "Containers/Ticker.h"
#include "AssetCompilingManager.h"
#include "HAL/PlatformTime.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraTypes.h"
#include "NiagaraScratchPadContainer.h"
#include "NiagaraSimulationStageBase.h"
#include "NiagaraSimulationStageCompileData.h"
#include "NiagaraParameterBinding.h"
#include "NiagaraParameterStore.h"
#include "NiagaraUserRedirectionParameterStore.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNode.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeWithDynamicPins.h"
#include "NiagaraSystemEditorData.h"
#include "NiagaraDataChannelAsset.h"
#include "NiagaraDataChannel.h"
#include "NiagaraDataChannel_Global.h"
#include "NiagaraDataChannelVariable.h"
#include "NiagaraDataInterface.h"
#include "NiagaraConstants.h"
#include "NiagaraNodeOp.h"
#include "NiagaraScriptVariable.h"
#include "NiagaraEditorSettings.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"
#include "UObject/NoExportTypes.h"
#include "HAL/FileManager.h"
#include "EdGraphSchema_Niagara.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
namespace Cubeless::UE58::NiagaraDataChannelAuthoring
{
namespace
{
using Cubeless::UE58::Variable::FResult;
constexpr const TCHAR* NiagaraTempGenerationRoot=TEXT("/Game/_MCP_Temp/NiagaraGenerated/");
bool Running=false;
bool VariadicContinuationActive=false;
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
FString Json(const TSharedPtr<FJsonObject>& O){FString S;if(O)FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S));return S;}
FString NormalizeNiagaraObjectPathForLoad(const FString&);
struct FIdentity
{
    TStrongObjectPtr<UObject> Hold{nullptr};TWeakObjectPtr<UObject> Object,Outer,Root;TWeakObjectPtr<UPackage> Package,Effective;TWeakObjectPtr<UClass> Class;FString Path,RetiredRelativePath;TWeakObjectPtr<UObject> RetiredRoot;bool Writer=false,Retired=false;
    explicit FIdentity(UObject* O):Hold(O),Object(O),Outer(O->GetOuter()),Package(O->GetOutermost()),Effective(O->GetPackage()),Class(O->GetClass()),Path(O->GetPathName()){}
    bool Exact()const{auto* O=Object.Get();if(!Live(O)||!Live(Class.Get())||Class->HasAnyClassFlags(CLASS_NewerVersionExists)||O->GetClass()!=Class.Get())return false;if(Retired){auto* RetiredOwner=RetiredRoot.Get();if(!Live(RetiredOwner)||RetiredOwner->GetOuter()!=GetTransientPackage()||O->GetOutermost()!=GetTransientPackage())return false;if(O==RetiredOwner)return true;return O->GetOuter()==Outer.Get()&&O->IsIn(RetiredOwner)&&O->GetPathName(RetiredOwner).Equals(RetiredRelativePath,ESearchCase::CaseSensitive);}if(!Live(Package.Get())||!Live(Effective.Get())||O->GetPackage()!=Effective.Get()||O->GetOuter()!=Outer.Get()||O->GetOutermost()!=Package.Get()||!O->GetPathName().Equals(Path,ESearchCase::CaseSensitive))return false;auto* R=Root.Get();return !Writer||(Live(R)&&(O==R||O->IsIn(R))&&O->GetOutermost()==R->GetOutermost()&&O->GetPackage()==R->GetPackage());}
};
struct FPinIdentity
{UEdGraphPin* Pointer=nullptr;FGuid Id;FName Name;EEdGraphPinDirection Direction=EGPD_Input;FEdGraphPinType Type;TArray<UEdGraphPin*> Links;};
// Reference refresh compares a copied semantic plan; pre-call pin addresses remain opaque afterwards.
FString PinAddress(const UEdGraphPin* P){return FString::Printf(TEXT("%p"),static_cast<const void*>(P));}
bool ReferencePinTypeDifference(const FEdGraphPinType& A,const FEdGraphPinType& B,FString& Cell,FString& Expected,FString& Actual)
{
#define GN_TYPE_NAME(Field) if(A.Field!=B.Field){Cell=TEXT("PinType." #Field);Expected=A.Field.ToString();Actual=B.Field.ToString();return true;}
#define GN_TYPE_NUMBER(Field) if(A.Field!=B.Field){Cell=TEXT("PinType." #Field);Expected=FString::FromInt(static_cast<int32>(A.Field));Actual=FString::FromInt(static_cast<int32>(B.Field));return true;}
    GN_TYPE_NAME(PinCategory);GN_TYPE_NAME(PinSubCategory);
    if(!A.PinSubCategoryObject.HasSameIndexAndSerialNumber(B.PinSubCategoryObject)){Cell=TEXT("PinType.PinSubCategoryObject");Expected=FString::Printf(TEXT("captured weak identity hash=%u"),GetTypeHash(A.PinSubCategoryObject));Actual=FString::Printf(TEXT("different weak identity hash=%u (no resolution)"),GetTypeHash(B.PinSubCategoryObject));return true;}
    GN_TYPE_NAME(PinValueType.TerminalCategory);GN_TYPE_NAME(PinValueType.TerminalSubCategory);
    if(!A.PinValueType.TerminalSubCategoryObject.HasSameIndexAndSerialNumber(B.PinValueType.TerminalSubCategoryObject)){Cell=TEXT("PinType.PinValueType.TerminalSubCategoryObject");Expected=FString::Printf(TEXT("captured weak identity hash=%u"),GetTypeHash(A.PinValueType.TerminalSubCategoryObject));Actual=FString::Printf(TEXT("different weak identity hash=%u (no resolution)"),GetTypeHash(B.PinValueType.TerminalSubCategoryObject));return true;}
    GN_TYPE_NUMBER(PinValueType.bTerminalIsConst);GN_TYPE_NUMBER(PinValueType.bTerminalIsWeakPointer);GN_TYPE_NUMBER(ContainerType);GN_TYPE_NUMBER(bIsReference);GN_TYPE_NUMBER(bIsWeakPointer);
    if(A.PinSubCategoryMemberReference.MemberParent.GetHandle()!=B.PinSubCategoryMemberReference.MemberParent.GetHandle()){Cell=TEXT("PinType.PinSubCategoryMemberReference.MemberParent");Expected=TEXT("captured shallow object handle");Actual=TEXT("different shallow object handle (no resolution)");return true;}
    GN_TYPE_NAME(PinSubCategoryMemberReference.MemberName);
    if(A.PinSubCategoryMemberReference.MemberGuid!=B.PinSubCategoryMemberReference.MemberGuid){Cell=TEXT("PinType.PinSubCategoryMemberReference.MemberGuid");Expected=A.PinSubCategoryMemberReference.MemberGuid.ToString();Actual=B.PinSubCategoryMemberReference.MemberGuid.ToString();return true;}
    GN_TYPE_NUMBER(bIsConst);GN_TYPE_NUMBER(bIsUObjectWrapper);GN_TYPE_NUMBER(bSerializeAsSinglePrecisionFloat);GN_TYPE_NUMBER(PinValueType.bTerminalIsUObjectWrapper);
#undef GN_TYPE_NAME
#undef GN_TYPE_NUMBER
    return false;
}
struct FReferencePinValues
{
    FString Default,Auto;TObjectPtr<UObject> Object;FText Text;uint32 Flags=0;UEdGraphPin* Parent=nullptr;UEdGraphPin* PassThrough=nullptr;TArray<UEdGraphPin*> SubPins;
    static uint32 ReadFlags(const UEdGraphPin* P){return (P->bHidden?1u:0u)|(P->bNotConnectable?2u:0u)|(P->bDefaultValueIsReadOnly?4u:0u)|(P->bDefaultValueIsIgnored?8u:0u)|(P->bAdvancedView?16u:0u)|(P->bDeprecated?32u:0u)|(P->bDisplayAsMutableRef?64u:0u)|(P->bAllowFriendlyName?128u:0u)|(P->bOrphanedPin?256u:0u)|(P->ShouldSavePinIfOrphaned()?512u:0u)|(P->bUseBackwardsCompatForEmptyAutogeneratedValue?1024u:0u)|(P->bWasTrashed?2048u:0u);}
    explicit FReferencePinValues(const UEdGraphPin* P):Default(P->DefaultValue),Auto(P->AutogeneratedDefaultValue),Object(P->DefaultObject),Text(P->DefaultTextValue),Flags(ReadFlags(P)),Parent(P->ParentPin),PassThrough(P->ReferencePassThroughConnection),SubPins(P->SubPins){}
};
struct FNodeIdentity
{TWeakObjectPtr<UEdGraphNode> Node;FGuid Id;TArray<FPinIdentity> Pins;};
struct FGraphIdentity
{TWeakObjectPtr<UNiagaraGraph> Graph;TArray<FNodeIdentity> Nodes;};
struct FSourceIdentity
{TWeakObjectPtr<UNiagaraScriptSource> Source;TWeakObjectPtr<UNiagaraGraph> Graph;};
struct FEmitterIdentity
{FGuid Id,InstanceVersion,DataVersion;TWeakObjectPtr<UNiagaraEmitter> Emitter;FVersionedNiagaraEmitterData* Data=nullptr;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;TWeakObjectPtr<UNiagaraScratchPadContainer> Scratch,ParentScratch;TArray<TWeakObjectPtr<UNiagaraScript>> ScratchScripts,ParentScripts;TArray<TWeakObjectPtr<UNiagaraSimulationStageBase>> Stages;};
struct FScriptBinding{TWeakObjectPtr<UNiagaraScript> Script;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;FGuid Version;};
struct FStageBinding{TWeakObjectPtr<UNiagaraSimulationStageBase> Stage;TWeakObjectPtr<UNiagaraScript> Script;FName Name;};
struct FWork
{
    bool VariadicAsyncMode=false,VariadicCancelled=false,VariadicCompileSourcesFrozen=false;FString VariadicCancelReason;TFunction<bool()> VariadicSemanticCurrent;
    FResult R;FString FailedStage;TWeakObjectPtr<UWorld> World;bool WorldCaptured=false,SaveHealth=true,Writing=false;
    TWeakObjectPtr<UObject> WriterRoot;TArray<TWeakObjectPtr<UObject>> LocalRoots;TWeakObjectPtr<UNiagaraDataChannelAsset> ChannelAsset;TWeakObjectPtr<UNiagaraDataChannel> ExpectedChannel;bool ChannelCaptured=false,ChannelSchemaCaptured=false,ExpectedKeep=false;TArray<FNiagaraDataChannelVariable> ExpectedVariables;UEdGraphNode* Rebuilding=nullptr;
    TWeakObjectPtr<UNiagaraSystem> System;TWeakObjectPtr<UNiagaraScript> Spawn,Update;TArray<TWeakObjectPtr<UNiagaraScript>> Scratch;TArray<FEmitterIdentity> Emitters;
    TArray<FScriptBinding> ScriptBindings;TArray<FStageBinding> StageBindings;TArray<TUniquePtr<FIdentity>> Objects;TArray<FGraphIdentity> Graphs;TArray<FSourceIdentity> Sources;
    TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>(),Prefix=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Saves,Compiles,Partial,MutationRows;TSharedPtr<FJsonObject> Before;int64 Slots=0,TextUnits=0,Calls=0;
    FWork(){R.Compiled=false;R.Data->SetObjectField(TEXT("native_call_counts"),Counts);R.Data->SetObjectField(TEXT("actual_mutation_prefix"),Prefix);R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),false);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);}
    bool Fail(const TCHAR* Code,const FString& Message){if(R.Error.IsEmpty()){FailedStage=R.Stage;R.ErrorCode=Code;R.Error=Message;}R.bSuccess=false;return false;}
    TSharedPtr<FJsonObject> Error(const FString& Message){if(R.Error.IsEmpty())Fail(TEXT("niagara_ndc_request_failed"),Message);auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),false);O->SetStringField(TEXT("error"),Message);return O;}
    bool Budget(int64 N,int64 Text=0){Slots+=N;TextUnits+=Text;Prefix->SetNumberField(TEXT("captured_slots"),static_cast<double>(Slots));Prefix->SetNumberField(TEXT("observed_text_utf16_units"),static_cast<double>(TextUnits));return (Slots<=262144&&TextUnits<=32ll*1024*1024)||Fail(TEXT("niagara_ndc_budget_exceeded"),TEXT("Bounded capture/output budget exceeded; partial prefix retained."));}
    bool Pin(UObject* O){if(!Live(O))return Fail(TEXT("niagara_ndc_object_unavailable"),TEXT("Expected live loaded object."));for(const auto& I:Objects){if(I->Object.Get()==O)return I->Exact()||Fail(TEXT("niagara_ndc_identity_changed"),TEXT("Captured object changed."));}if(!Budget(1,O->GetPathName().Len()))return false;Objects.Add(MakeUnique<FIdentity>(O));return true;}
    bool Own(UObject* O)
    {auto* Root=WriterRoot.Get();if(!Root)Root=System.Get();for(const auto& Local:LocalRoots){auto* L=Local.Get();if(Live(L)&&(O==L||O->IsIn(L))){Root=L;break;}}if(!Admit(TEXT("writer_ownership"))||!Live(O)||!Live(Root)||(O!=Root&&!O->IsIn(Root))||O->GetOutermost()!=Root->GetOutermost()||O->GetPackage()!=Root->GetPackage())return Fail(TEXT("niagara_ndc_external_writer_unsupported"),TEXT("Actual writer is outside the specified System outer chain/package. No external emitter/shared graph/source was changed."));if(!Pin(O))return false;for(auto& I:Objects){if(I->Object.Get()==O){I->Writer=true;I->Root=Root;}}return true;}
    bool PinFailure(const FNodeIdentity& Node,int32 PinIndex,const FString& Property,const FString& Expected,const FString& Actual,const TCHAR* Code=TEXT("niagara_ndc_pin_links_changed"))
    {
        if(!R.Data->HasField(TEXT("variadic_async_first_mismatch"))){auto M=MakeShared<FJsonObject>();FString Path;for(const auto& I:Objects)if(I->Object.Get()==Node.Node.Get()){Path=I->Path;break;}M->SetStringField(TEXT("object"),Path);M->SetStringField(TEXT("object_identity_source"),TEXT("captured held node path; no failed-object SDK lookup"));M->SetStringField(TEXT("node_guid"),Node.Id.ToString());M->SetNumberField(TEXT("pin_index"),PinIndex);if(Node.Pins.IsValidIndex(PinIndex)){const auto& P=Node.Pins[PinIndex];M->SetStringField(TEXT("pin_id"),P.Id.ToString());M->SetStringField(TEXT("pin_name"),P.Name.ToString());M->SetStringField(TEXT("captured_pin_address"),PinAddress(P.Pointer));}M->SetStringField(TEXT("stage"),R.Stage);M->SetStringField(TEXT("property"),Property);M->SetStringField(TEXT("expected"),Expected);M->SetStringField(TEXT("actual"),Actual);M->SetStringField(TEXT("diagnostic_scope"),TEXT("First evaluated pin condition; copied/plain values only, old pins and linked addresses are not dereferenced"));R.Data->SetObjectField(TEXT("variadic_async_first_mismatch"),M);}return Fail(Code,FCString::Strcmp(Code,TEXT("niagara_ndc_pin_identity_changed"))==0?TEXT("Captured pin disappeared; stale pointer not dereferenced."):TEXT("Pin identity/type/links differ from expected admitted transition."));
    }
    bool Admit(const FString& Stage,const TMap<UEdGraphPin*,UEdGraphPin*>* ReferenceAddresses=nullptr)
    {
        R.Stage=Stage;if(!R.Error.IsEmpty())return false;
        if(VariadicAsyncMode&&VariadicCancelled)return Fail(TEXT("niagara_di_variadic_input_cancelled"),VariadicCancelReason);
        if(!IsInGameThread()||!GEditor||IsEngineExitRequested()||GExitPurge||IsGarbageCollecting()||GIsSavingPackage)return Fail(TEXT("niagara_ndc_context_unavailable"),TEXT("Live Editor game thread without GC/exit/save required; no replay."));
        if(Writing&&(GEditor->PlayWorld||GEditor->GetPIEWorldContext()))return Fail(TEXT("niagara_ndc_play_session_active"),TEXT("Niagara authoring requires stopped PIE/SIE."));
        UWorld* CurrentWorld=GEditor->GetEditorWorldContext().World();if(!Live(CurrentWorld)||CurrentWorld->bIsTearingDown)return Fail(TEXT("niagara_ndc_world_unavailable"),TEXT("Editor world unavailable or tearing down."));if(!WorldCaptured){World=CurrentWorld;WorldCaptured=true;}else if(World.Get()!=CurrentWorld)return Fail(TEXT("niagara_ndc_world_changed"),TEXT("Editor world changed across SDK dispatch."));
        if(ChannelCaptured&&(!Live(ChannelAsset.Get())||ChannelAsset->Get()!=ExpectedChannel.Get()))return Fail(TEXT("niagara_ndc_channel_replaced"),TEXT("Captured channel asset binding changed."));
        for(const auto& I:Objects){if(!I->Exact())return Fail(TEXT("niagara_ndc_identity_changed"),TEXT("Captured class/outer/package/path/ownership changed; no replacement adopted."));}
        if(ChannelSchemaCaptured){auto* C=ExpectedChannel.Get();if(!Live(C)||C->KeepPreviousFrameData()!=ExpectedKeep||C->GetVariables().Num()!=ExpectedVariables.Num())return Fail(TEXT("niagara_ndc_layout_changed"),TEXT("Captured Data Channel layout/previous-frame flag changed."));for(int32 I=0;I<ExpectedVariables.Num();++I)if(C->GetVariables()[I]!=ExpectedVariables[I])return Fail(TEXT("niagara_ndc_layout_changed"),TEXT("Captured ordered Data Channel variables changed."));}
        if(System.IsValid())
        {auto* O=System.Get();if(O->GetSystemSpawnScript()!=Spawn.Get()||O->GetSystemUpdateScript()!=Update.Get()||O->ScratchPadScripts.Num()!=Scratch.Num()||O->GetEmitterHandles().Num()!=Emitters.Num())return Fail(TEXT("niagara_ndc_system_topology_changed"),TEXT("Captured System emitter/script topology changed."));
            for(int32 I=0;I<Scratch.Num();++I)if(O->ScratchPadScripts[I]!=Scratch[I].Get())return Fail(TEXT("niagara_ndc_scratch_membership_changed"),TEXT("System scratch membership changed."));
            for(int32 I=0;I<Emitters.Num();++I){const auto& H=O->GetEmitterHandles()[I];const auto& E=Emitters[I];if(H.GetId()!=E.Id||H.GetInstance().Emitter!=E.Emitter.Get()||H.GetInstance().Version!=E.InstanceVersion||H.GetEmitterData()!=E.Data||(E.Data&&E.Data->Version.VersionGuid!=E.DataVersion))return Fail(TEXT("niagara_ndc_emitter_version_changed"),TEXT("Physical emitter/ref/version changed."));
                if(E.Data){if(E.Data->GraphSource!=E.Source.Get()||E.Data->ScratchPads!=E.Scratch.Get()||E.Data->ParentScratchPads!=E.ParentScratch.Get())return Fail(TEXT("niagara_ndc_emitter_routing_changed"),TEXT("Emitter source/scratch binding changed."));
                    auto Same=[](const UNiagaraScratchPadContainer* C,const TArray<TWeakObjectPtr<UNiagaraScript>>& P){if(!C)return P.IsEmpty();if(C->Scripts.Num()!=P.Num())return false;for(int32 J=0;J<P.Num();++J)if(C->Scripts[J]!=P[J].Get())return false;return true;};
                    if(!Same(E.Scratch.Get(),E.ScratchScripts)||!Same(E.ParentScratch.Get(),E.ParentScripts))return Fail(TEXT("niagara_ndc_scratch_membership_changed"),TEXT("Emitter scratch membership changed."));
                    const auto& A=E.Data->GetSimulationStages();if(A.Num()!=E.Stages.Num())return Fail(TEXT("niagara_ndc_stage_membership_changed"),TEXT("Simulation stage count changed."));for(int32 J=0;J<A.Num();++J)if(A[J]!=E.Stages[J].Get())return Fail(TEXT("niagara_ndc_stage_membership_changed"),TEXT("Simulation stage order changed."));
                }
            }
        }
        for(const auto& S:ScriptBindings){auto* O=S.Script.Get();if(!Live(O)||O->GetLatestSource()!=S.Source.Get()||O->GetExposedVersion().VersionGuid!=S.Version)return Fail(TEXT("niagara_ndc_script_source_changed"),TEXT("Script source/exposed version changed."));}
        for(const auto& S:StageBindings){auto* O=S.Stage.Get();if(!Live(O)||O->Script!=S.Script.Get()||O->SimulationStageName!=S.Name)return Fail(TEXT("niagara_ndc_stage_script_changed"),TEXT("Stage name/script binding changed."));}
        for(const auto& S:Sources){if(!Live(S.Source.Get())||S.Source->NodeGraph!=S.Graph.Get())return Fail(TEXT("niagara_ndc_source_graph_changed"),TEXT("Captured source graph binding changed."));}
        for(const auto& G:Graphs){auto* O=G.Graph.Get();if(!Live(O)||O->Nodes.Num()!=G.Nodes.Num())return Fail(TEXT("niagara_ndc_graph_topology_changed"),TEXT("Graph node count changed outside the planned removal."));
            for(int32 I=0;I<G.Nodes.Num();++I){const auto& E=G.Nodes[I];auto* N=E.Node.Get();if(!Live(N)||O->Nodes[I]!=N||(N!=Rebuilding&&(N->NodeGuid!=E.Id||N->Pins.Num()!=E.Pins.Num())))return Fail(TEXT("niagara_ndc_node_identity_changed"),TEXT("Captured node/pin membership changed."));
                if(N==Rebuilding)continue;for(int32 J=0;J<E.Pins.Num();++J){const auto& P=E.Pins[J];auto Address=[&](UEdGraphPin* Value){if(ReferenceAddresses)if(auto* Fresh=ReferenceAddresses->Find(Value))return *Fresh;return Value;};auto* ActualPin=N->Pins[J];auto* ExpectedPin=Address(P.Pointer);
                    if(ActualPin!=ExpectedPin)return PinFailure(E,J,TEXT("Pins[index]"),PinAddress(ExpectedPin),PinAddress(ActualPin),TEXT("niagara_ndc_pin_identity_changed"));
                    // Membership above is mandatory before inspecting the current pin. Never read the captured old address.
                    if(ActualPin){if(ActualPin->PinId!=P.Id)return PinFailure(E,J,TEXT("PinId"),P.Id.ToString(),ActualPin->PinId.ToString());if(ActualPin->GetOwningNodeUnchecked()!=N)return PinFailure(E,J,TEXT("OwningNode"),FString::Printf(TEXT("%p"),static_cast<void*>(N)),FString::Printf(TEXT("%p"),static_cast<void*>(ActualPin->GetOwningNodeUnchecked())));if(ActualPin->PinName!=P.Name)return PinFailure(E,J,TEXT("PinName"),P.Name.ToString(),ActualPin->PinName.ToString());if(ActualPin->Direction!=P.Direction)return PinFailure(E,J,TEXT("Direction"),FString::FromInt(P.Direction),FString::FromInt(ActualPin->Direction));if(ActualPin->PinType!=P.Type){FString Cell,Expected,Actual;ReferencePinTypeDifference(P.Type,ActualPin->PinType,Cell,Expected,Actual);return PinFailure(E,J,Cell.IsEmpty()?TEXT("PinType"):Cell,Expected,Actual);}
                        if(ActualPin->LinkedTo.Num()!=P.Links.Num())return PinFailure(E,J,TEXT("LinkedTo.Num"),FString::FromInt(P.Links.Num()),FString::FromInt(ActualPin->LinkedTo.Num()));for(int32 K=0;K<P.Links.Num();++K)if(ActualPin->LinkedTo[K]!=Address(P.Links[K]))return PinFailure(E,J,FString::Printf(TEXT("LinkedTo[%d] (opaque address)"),K),PinAddress(Address(P.Links[K])),PinAddress(ActualPin->LinkedTo[K]));}
                }
            }
        }return true;
    }
    TSharedRef<FJsonObject> MutationRow(const TCHAR* Stage){auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("stage"),Stage);Row->SetBoolField(TEXT("SDK_returned"),false);MutationRows.Add(MakeShared<FJsonValueObject>(Row));return Row;}
    bool SDK(const TCHAR* Stage,bool Mutation=false){if(!Admit(Stage))return false;if(!Budget(1,FCString::Strlen(Stage)))return false;if(++Calls>262144)return Fail(TEXT("niagara_ndc_sdk_budget"),TEXT("SDK call budget exceeded."));R.bChangeStateKnown=false;R.bMutationAttempted|=Mutation;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);double N=0;Counts->TryGetNumberField(Stage,N);Counts->SetNumberField(Stage,N+1);if(Mutation)Prefix->SetBoolField(Stage,true);return true;}
    bool After(const TCHAR* Stage){Prefix->SetBoolField(FString(Stage)+TEXT("_SDK_returned"),true);return Admit(FString(TEXT("after_"))+Stage);}
    template<typename F>bool Step(const TCHAR* Stage,bool Mutation,F&& Fnc){if(!SDK(Stage,Mutation))return false;Fnc();return After(Stage);}
    bool Modify(UObject* O){if(!Own(O))return false;Prefix->SetStringField(TEXT("last_modify_object"),O->GetPathName());return Step(TEXT("Modify"),true,[&]{O->Modify();});}
    bool Dirty(){return Step(TEXT("MarkPackageDirty"),true,[&]{System->MarkPackageDirty();});}
    bool CaptureScript(UNiagaraScript* O){if(!O)return true;if(VariadicAsyncMode&&VariadicCompileSourcesFrozen&&!ScriptBindings.ContainsByPredicate([O](const FScriptBinding& S){return S.Script.Get()==O;}))return Fail(TEXT("niagara_di_variadic_input_compile_source_changed"),TEXT("Unplanned compile script replaced an initially captured source; no new source adoption."));if(!Pin(O))return false;for(const auto& I:ScriptBindings)if(I.Script.Get()==O)return true;ScriptBindings.Add({O,O->GetLatestSource(),O->GetExposedVersion().VersionGuid});return true;}
    bool CaptureSystem(UNiagaraSystem* O);
    bool CaptureGraph(UNiagaraScriptSource* S);
    bool GraphForWrite(UNiagaraGraph* G);
    bool PinForWrite(UEdGraphPin* P);
    bool RefreshNode(UEdGraphNode* N);
    bool RefreshVariadicReference(UNiagaraNodeFunctionCall* N);
    template<typename F>bool EditNode(UEdGraphNode* N,const TCHAR* Stage,F&& Fnc);
    bool AddNode(UNiagaraGraph* G,UEdGraphNode* N);
    bool RemoveNode(UNiagaraGraph* G,UEdGraphNode* N,bool BreakAllLinks=true,bool AlwaysMarkDirty=false,bool Destroy=false);
    bool BreakLinks(UEdGraphPin* P,bool Notify);
    bool LinkPins(UEdGraphPin* A,UEdGraphPin* B);
    bool Request(bool& Accepted);
    bool Save(bool Direct,FString* Filename=nullptr);
    TSharedPtr<FJsonObject> Projection();
};
FWork* Current=nullptr;
bool ContinueRead(){return Current&&Current->Admit(TEXT("continuation"));}
#define GN_STEP(Stage,Mutation,Expr) do{if(!W.Step(TEXT(Stage),Mutation,[&]{Expr;}))return W.Error(W.R.Error);}while(false)

bool FWork::CaptureSystem(UNiagaraSystem* O)
{
    if(!Pin(O)||!Pin(O->GetOutermost()))return false;System=O;WriterRoot=O;Spawn=O->GetSystemSpawnScript();Update=O->GetSystemUpdateScript();if(!CaptureScript(Spawn.Get())||!CaptureScript(Update.Get()))return false;
    for(UNiagaraScript* S:O->ScratchPadScripts){if(S&&!CaptureScript(S))return false;Scratch.Add(S);}
    if(!Budget(O->GetEmitterHandles().Num())||O->GetEmitterHandles().Num()>4096)return Fail(TEXT("niagara_ndc_emitter_bound"),TEXT("Emitter inventory exceeds bound."));
    for(const auto& H:O->GetEmitterHandles()){FEmitterIdentity E;E.Id=H.GetId();E.InstanceVersion=H.GetInstance().Version;E.Emitter=H.GetInstance().Emitter;E.Data=H.GetEmitterData();if(E.Emitter.IsValid()&&!Pin(E.Emitter.Get()))return false;if(E.Data){E.DataVersion=E.Data->Version.VersionGuid;E.Source=E.Data->GraphSource;E.Scratch=E.Data->ScratchPads;E.ParentScratch=E.Data->ParentScratchPads;if(E.Source.IsValid()&&!Pin(E.Source.Get()))return false;
        auto CaptureScripts=[&](UNiagaraScratchPadContainer* C,TArray<TWeakObjectPtr<UNiagaraScript>>& A){if(!C)return true;if(!Pin(C)||!Budget(C->Scripts.Num()))return false;for(UNiagaraScript* S:C->Scripts){if(S&&!CaptureScript(S))return false;A.Add(S);}return true;};
        if(!CaptureScripts(E.Scratch.Get(),E.ScratchScripts)||!CaptureScripts(E.ParentScratch.Get(),E.ParentScripts))return false;
        TArray<UNiagaraScript*> OwnedScripts;E.Data->GetScripts(OwnedScripts,false,false);if(!Budget(OwnedScripts.Num()))return false;for(UNiagaraScript* P:OwnedScripts)if(!CaptureScript(P))return false;
        if(!Budget(E.Data->GetSimulationStages().Num()))return false;for(UNiagaraSimulationStageBase* S:E.Data->GetSimulationStages()){if(S&&(!Pin(S)||!CaptureScript(S->Script)))return false;if(S)StageBindings.Add({S,S->Script,S->SimulationStageName});E.Stages.Add(S);}
    }Emitters.Add(MoveTemp(E));}return true;
}
UNiagaraSystem* LoadSystem(const FString& Path)
{
    if(!Current->SDK(TEXT("LoadObject_System")))return nullptr;const FString Expected=NormalizeNiagaraObjectPathForLoad(Path);auto* O=LoadObject<UNiagaraSystem>(nullptr,*Expected);Current->Prefix->SetBoolField(TEXT("load_returned_nonnull"),O!=nullptr);if(!Current->After(TEXT("LoadObject_System")))return nullptr;if(O&&(!Live(O)||!O->GetPathName().Equals(Expected,ESearchCase::CaseSensitive)||!Current->CaptureSystem(O))){Current->Fail(TEXT("niagara_ndc_load_identity_unexpected"),TEXT("Loaded System differs from requested normalized exact path."));return nullptr;}return O;
}
bool FWork::CaptureGraph(UNiagaraScriptSource* S)
{
    if(!Admit(TEXT("capture_graph")))return false;if(!S||!S->NodeGraph)return true;if(!Pin(S)||!Pin(S->NodeGraph))return false;auto* G=S->NodeGraph.Get();
    bool Bound=false;for(const auto& E:Sources)if(E.Source.Get()==S){Bound=true;if(E.Graph.Get()!=G)return Fail(TEXT("niagara_ndc_source_graph_changed"),TEXT("Source graph changed."));}
    if(!Bound)Sources.Add({S,G});for(const auto& E:Graphs)if(E.Graph.Get()==G)return true;
    if(!Budget(G->Nodes.Num())||G->Nodes.Num()>16384)return Fail(TEXT("niagara_ndc_graph_bound"),TEXT("Graph node inventory exceeds bound."));
    FGraphIdentity E;E.Graph=G;
    for(UEdGraphNode* N:G->Nodes){if(!N||!Pin(N)||N->GetGraph()!=G)return Fail(TEXT("niagara_ndc_node_owner_invalid"),TEXT("Null/foreign graph node cannot be safely inspected or mutated."));FNodeIdentity NE;NE.Node=N;NE.Id=N->NodeGuid;if(!Budget(N->Pins.Num()))return false;
        for(UEdGraphPin* P:N->Pins){if(!P)return Fail(TEXT("niagara_ndc_null_pin"),TEXT("Null graph pin unsupported by Niagara public getters."));if(P->GetOwningNodeUnchecked()!=N||!Budget(P->LinkedTo.Num()))return Fail(TEXT("niagara_ndc_pin_owner_invalid"),TEXT("Pin owner/link inventory is invalid."));FPinIdentity PE;PE.Pointer=P;PE.Id=P->PinId;PE.Name=P->PinName;PE.Direction=P->Direction;PE.Type=P->PinType;PE.Links=P->LinkedTo;NE.Pins.Add(MoveTemp(PE));}
        E.Nodes.Add(MoveTemp(NE));}
    TSet<UEdGraphPin*> Known;for(const auto& N:E.Nodes)for(const auto& P:N.Pins)Known.Add(P.Pointer);for(const auto& N:E.Nodes)for(const auto& P:N.Pins)for(auto* L:P.Links){if(!L||!Known.Contains(L))return Fail(TEXT("niagara_ndc_unresolved_link_identity"),TEXT("Null/cross-graph link is outside captured exact pin membership; no stale link dereference."));}
    Graphs.Add(MoveTemp(E));return true;
}
bool FWork::GraphForWrite(UNiagaraGraph* G)
{
    if(!Admit(TEXT("graph_writer_admission"))||!Own(G))return false;bool Captured=false;
    for(const auto& S:Sources){if(S.Graph.Get()==G){if(!Own(S.Source.Get()))return false;Captured=true;}}
    TSet<UNiagaraScriptSource*> Sharing;int32 EmitterOwners=0;bool SystemOwner=false;auto HasGraph=[&](UNiagaraScriptSourceBase* Base){auto* Source=Cast<UNiagaraScriptSource>(Base);if(!Source)return false;if(!Live(Source)){Fail(TEXT("niagara_ndc_shared_source_unresolved"),TEXT("Unloaded source prevents exact shared-graph admission."));return false;}if(Source->NodeGraph==G){Sharing.Add(Source);return true;}return false;};for(const auto& E:Emitters){if(HasGraph(E.Source.Get()))++EmitterOwners;}for(const auto& E:ScriptBindings){const bool Matches=HasGraph(E.Source.Get());if(Matches&&(E.Script.Get()==Spawn.Get()||E.Script.Get()==Update.Get()))SystemOwner=true;}if(!R.Error.IsEmpty())return false;int32 ScratchRoles=0,ScriptRoles=0;auto IsGraph=[&](UNiagaraScript* Script){return Script&&Cast<UNiagaraScriptSource>(Script->GetLatestSource())&&Cast<UNiagaraScriptSource>(Script->GetLatestSource())->NodeGraph==G;};for(const auto& S:ScriptBindings)if(IsGraph(S.Script.Get()))++ScriptRoles;for(const auto& S:Scratch)if(IsGraph(S.Get()))++ScratchRoles;for(const auto& E:Emitters){for(const auto& S:E.ScratchScripts)if(IsGraph(S.Get()))++ScratchRoles;for(const auto& S:E.ParentScripts)if(IsGraph(S.Get()))++ScratchRoles;}if(Sharing.Num()>1||EmitterOwners>1||(EmitterOwners>0&&SystemOwner)||(ScratchRoles>0&&(ScratchRoles>1||ScriptRoles>1||EmitterOwners>0||SystemOwner)))return Fail(TEXT("niagara_ndc_shared_graph_writer_unsupported"),TEXT("The selected graph is shared by multiple source/owner roles. Refused before first Modify; no duplicate or unselected owner mutation."));
    if(!Captured)return Fail(TEXT("niagara_ndc_uncaptured_graph"),TEXT("Mutable graph has no captured exact source binding."));
    for(const auto& E:Graphs){if(E.Graph.Get()==G){for(const auto& N:E.Nodes)if(!Own(N.Node.Get()))return false;}}
    return true;
}
FPinIdentity* FindExpectedPin(FWork& W,UEdGraphPin* P)
{for(auto& G:W.Graphs)for(auto& N:G.Nodes)for(auto& E:N.Pins)if(E.Pointer==P)return &E;return nullptr;}
bool FWork::PinForWrite(UEdGraphPin* P)
{
    if(!Admit(TEXT("pin_writer_admission"))||!P)return Fail(TEXT("niagara_ndc_pin_unavailable"),TEXT("Pin unavailable."));
    auto* E=FindExpectedPin(*this,P);if(!E)return Fail(TEXT("niagara_ndc_uncaptured_pin"),TEXT("Pin is not in captured exact node membership; stale pointer not dereferenced."));
    auto* N=P->GetOwningNodeUnchecked();auto* G=Cast<UNiagaraGraph>(N->GetGraph());if(!GraphForWrite(G)||!Own(N))return false;
    for(auto* L:E->Links){if(!L||!FindExpectedPin(*this,L))return Fail(TEXT("niagara_ndc_external_pin_link_unsupported"),TEXT("Linked pin is outside captured mutable graph(s)."));auto* LN=L->GetOwningNodeUnchecked();if(LN->GetGraph()!=G||!Own(LN))return Fail(TEXT("niagara_ndc_external_pin_link_unsupported"),TEXT("Cross-graph linked writer is unsupported before mutation."));}
    return true;
}
bool FWork::BreakLinks(UEdGraphPin* P,bool Notify)
{
    if(!PinForWrite(P)||!SDK(TEXT("BreakAllPinLinks"),true))return false;auto* E=FindExpectedPin(*this,P);const TArray<UEdGraphPin*> Links=E->Links;
    for(auto* L:Links){auto* Other=FindExpectedPin(*this,L);if(!Other)return Fail(TEXT("niagara_ndc_link_plan_unavailable"),TEXT("Captured reciprocal pin unavailable."));Other->Links.Remove(P);}E->Links.Empty();
    auto Row=MutationRow(TEXT("BreakAllPinLinks"));Row->SetStringField(TEXT("node_guid"),P->GetOwningNodeUnchecked()->NodeGuid.ToString());Row->SetStringField(TEXT("pin_guid"),P->PinId.ToString());Row->SetStringField(TEXT("pin_name"),P->PinName.ToString());Row->SetNumberField(TEXT("links_before"),Links.Num());P->BreakAllPinLinks(Notify);Row->SetBoolField(TEXT("SDK_returned"),true);R.bChanged|=!Links.IsEmpty();return After(TEXT("BreakAllPinLinks"));
}
bool FWork::LinkPins(UEdGraphPin* A,UEdGraphPin* B)
{
    if(!PinForWrite(A)||!PinForWrite(B)||!SDK(TEXT("MakeLinkTo"),true))return false;
    auto* EA=FindExpectedPin(*this,A);auto* EB=FindExpectedPin(*this,B);EA->Links.AddUnique(B);EB->Links.AddUnique(A);auto Row=MutationRow(TEXT("MakeLinkTo"));Row->SetStringField(TEXT("pin_a"),A->PinId.ToString());Row->SetStringField(TEXT("pin_b"),B->PinId.ToString());A->MakeLinkTo(B);Row->SetBoolField(TEXT("SDK_returned"),true);R.bChanged=true;return After(TEXT("MakeLinkTo"));
}
bool ModifyPin(UEdGraphPin* P){if(!Current->PinForWrite(P))return false;return Current->Step(TEXT("Pin_Modify"),true,[&]{P->Modify();});}

// Only new Engine graph objects are enrolled by these builder transitions.
bool FWork::RefreshNode(UEdGraphNode* N)
{
 if(!Admit(TEXT("refresh_node_snapshot"))||!Live(N))return false;auto* G=Cast<UNiagaraGraph>(N->GetGraph());auto* GE=Graphs.FindByPredicate([&](const auto& E){return E.Graph.Get()==G;});if(!GE||!G->Nodes.Contains(N))return Fail(TEXT("niagara_ndc_node_membership"),TEXT("Authored node not in exact captured graph."));auto* NE=GE->Nodes.FindByPredicate([&](const auto& E){return E.Node.Get()==N;});if(!NE)return Fail(TEXT("niagara_ndc_node_plan"),TEXT("Node transition has no exact plan."));
 FNodeIdentity Fresh;Fresh.Node=N;Fresh.Id=N->NodeGuid;if(!Budget(N->Pins.Num()))return false;for(auto* P:N->Pins){if(!P||P->GetOwningNodeUnchecked()!=N||!Budget(P->LinkedTo.Num()))return Fail(TEXT("niagara_ndc_pin_owner"),TEXT("Null/foreign authored pin."));FPinIdentity E;E.Pointer=P;E.Id=P->PinId;E.Name=P->PinName;E.Direction=P->Direction;E.Type=P->PinType;E.Links=P->LinkedTo;Fresh.Pins.Add(MoveTemp(E));}*NE=MoveTemp(Fresh);TSet<UEdGraphPin*> Known;for(const auto& Node:GE->Nodes)for(const auto& P:Node.Pins)Known.Add(P.Pointer);for(const auto& Node:GE->Nodes)for(const auto& P:Node.Pins)for(auto* L:P.Links)if(!L||!Known.Contains(L))return Fail(TEXT("niagara_ndc_unresolved_link"),TEXT("Authored link is outside captured pin membership."));return true;
}
// Only Variadic_RefreshReference may rebind these addresses. This is not a generic EditNode adoption.
bool FWork::RefreshVariadicReference(UNiagaraNodeFunctionCall* N)
{
    constexpr const TCHAR* Stage=TEXT("Variadic_RefreshReference");
    if(!Own(N)||!Admit(TEXT("variadic_reference_plan")))return false;
    FGraphIdentity* SelectedGraph=nullptr;FNodeIdentity* SelectedNode=nullptr;int32 NodeIndex=INDEX_NONE;
    for(auto& G:Graphs)for(int32 I=0;I<G.Nodes.Num();++I)if(G.Nodes[I].Node.Get()==N){if(SelectedNode)return Fail(TEXT("niagara_ndc_reference_plan_ambiguous"),TEXT("Reference node has more than one captured graph role."));SelectedGraph=&G;SelectedNode=&G.Nodes[I];NodeIndex=I;}
    if(!SelectedGraph||!SelectedNode||Rebuilding)return Fail(TEXT("niagara_ndc_reference_plan_unavailable"),TEXT("Exact captured reference node required outside another node transition."));
    auto* G=SelectedGraph->Graph.Get();const FNodeIdentity BeforeNode=*SelectedNode;
    struct FBeforePin{const FNodeIdentity* Node;int32 Index;FReferencePinValues Values;FBeforePin(const FNodeIdentity& E,int32 I):Node(&E),Index(I),Values(E.Pins[I].Pointer){}};
    TArray<FBeforePin> Values;TSet<UEdGraphPin*> Captured;
    // All pointers are still current here. Copy values before the SDK; no old pin is read afterwards.
    for(const auto& E:SelectedGraph->Nodes)for(int32 I=0;I<E.Pins.Num();++I){auto* P=E.Pins[I].Pointer;if(!P||Captured.Contains(P))return Fail(TEXT("niagara_ndc_reference_pin_plan_invalid"),TEXT("Reference graph requires unique nonnull captured pins."));Captured.Add(P);Values.Emplace(E,I);}
    for(int32 I=0;I<BeforeNode.Pins.Num();++I){auto* P=BeforeNode.Pins[I].Pointer;if(!BeforeNode.Pins[I].Id.IsValid()||P->ParentPin||!P->SubPins.IsEmpty()||P->ReferencePassThroughConnection||P->bOrphanedPin||P->bWasTrashed)return PinFailure(BeforeNode,I,TEXT("reference normal form"),TEXT("valid PinId, unsplit, no pass-through/orphan/trash"),TEXT("unsupported pre-call reference pin shape"));}
    auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("scope"),TEXT("Variadic_RefreshReference only; same ordered PinId/full type/default/flags/semantic links"));Receipt->SetStringField(TEXT("node_guid"),BeforeNode.Id.ToString());Receipt->SetBoolField(TEXT("pre_dispatch_current_verified"),false);Receipt->SetBoolField(TEXT("SDK_returned"),false);Receipt->SetBoolField(TEXT("semantic_rebind_verified"),false);R.Data->SetObjectField(TEXT("variadic_reference_pin_rebind"),Receipt);
    if(!SDK(Stage,true))return false;Receipt->SetBoolField(TEXT("pre_dispatch_current_verified"),true);
    const bool Refreshed=N->RefreshFromExternalChanges();
    Prefix->SetBoolField(TEXT("Variadic_RefreshReference_SDK_returned"),true);Receipt->SetBoolField(TEXT("SDK_returned"),true);Receipt->SetBoolField(TEXT("SDK_refresh_returned"),Refreshed);R.Stage=TEXT("after_Variadic_RefreshReference");
    // Old pin and peer addresses below are opaque keys only. Fresh membership is established before any pin dereference.
    if(!IsInGameThread()||!GEditor||IsEngineExitRequested()||GExitPurge||IsGarbageCollecting()||GIsSavingPackage||!Live(G)||!Live(N))return Fail(TEXT("niagara_ndc_reference_refresh_context_changed"),TEXT("Reference refresh returned without a safe held context; no pin read or follow-up SDK."));
    if(!G->Nodes.IsValidIndex(NodeIndex)||G->Nodes[NodeIndex]!=N)return PinFailure(BeforeNode,INDEX_NONE,TEXT("graph.Nodes[reference index]"),FString::Printf(TEXT("%p"),static_cast<void*>(N)),G->Nodes.IsValidIndex(NodeIndex)?FString::Printf(TEXT("%p"),static_cast<void*>(G->Nodes[NodeIndex])):TEXT("index absent"),TEXT("niagara_ndc_node_identity_changed"));
    if(N->NodeGuid!=BeforeNode.Id)return PinFailure(BeforeNode,INDEX_NONE,TEXT("NodeGuid"),BeforeNode.Id.ToString(),N->NodeGuid.ToString(),TEXT("niagara_ndc_node_identity_changed"));
    if(N->Pins.Num()!=BeforeNode.Pins.Num())return PinFailure(BeforeNode,INDEX_NONE,TEXT("Pins.Num"),FString::FromInt(BeforeNode.Pins.Num()),FString::FromInt(N->Pins.Num()),TEXT("niagara_ndc_node_identity_changed"));
    TMap<UEdGraphPin*,UEdGraphPin*> Addresses;TSet<UEdGraphPin*> Fresh;TSet<FGuid> PinIds;int32 Replaced=0;
    for(int32 I=0;I<BeforeNode.Pins.Num();++I){auto* P=N->Pins[I];const auto& Old=BeforeNode.Pins[I];if(!P||Fresh.Contains(P)||(P!=Old.Pointer&&Captured.Contains(P))||PinIds.Contains(Old.Id))return PinFailure(BeforeNode,I,TEXT("fresh reference pin uniqueness"),TEXT("unique nonnull fresh member; no other captured pin reused; unique semantic PinId"),PinAddress(P));Fresh.Add(P);PinIds.Add(Old.Id);Addresses.Add(Old.Pointer,P);Replaced+=P!=Old.Pointer;}
    // SDK TransferPersistentData replaces a peer's old endpoint at the same LinkedTo index. All other roles stay strict.
    if(!Admit(TEXT("after_Variadic_RefreshReference"),&Addresses))return false;
    for(const auto& E:Values){const auto& Old=E.Node->Pins[E.Index];auto* P=E.Node->Node.Get()->Pins[E.Index];const auto& V=E.Values;FString Cell,Expected,Actual;
        if(ReferencePinTypeDifference(Old.Type,P->PinType,Cell,Expected,Actual))return PinFailure(*E.Node,E.Index,Cell,Expected,Actual);
        if(!P->DefaultValue.Equals(V.Default,ESearchCase::CaseSensitive))return PinFailure(*E.Node,E.Index,TEXT("DefaultValue"),V.Default,P->DefaultValue);
        if(!P->AutogeneratedDefaultValue.Equals(V.Auto,ESearchCase::CaseSensitive))return PinFailure(*E.Node,E.Index,TEXT("AutogeneratedDefaultValue"),V.Auto,P->AutogeneratedDefaultValue);
        if(P->DefaultObject.GetHandle()!=V.Object.GetHandle())return PinFailure(*E.Node,E.Index,TEXT("DefaultObject"),TEXT("captured shallow object handle"),TEXT("different shallow object handle (no resolution)"));
        if(!P->DefaultTextValue.IdenticalTo(V.Text))return PinFailure(*E.Node,E.Index,TEXT("DefaultTextValue"),TEXT("captured FText identity/value"),TEXT("different FText identity/value"));
        if(FReferencePinValues::ReadFlags(P)!=V.Flags)return PinFailure(*E.Node,E.Index,TEXT("pin flags"),FString::Printf(TEXT("%u"),V.Flags),FString::Printf(TEXT("%u"),FReferencePinValues::ReadFlags(P)));
        if(P->ParentPin!=V.Parent||P->ReferencePassThroughConnection!=V.PassThrough||P->SubPins!=V.SubPins)return PinFailure(*E.Node,E.Index,TEXT("split/pass-through pin topology"),TEXT("captured plain pointer topology"),TEXT("different pointer topology"));
    }
    // Commit only the proven address substitution. No field/type/default/link ordering is sampled into the plan.
    int32 PeerLinks=0;TArray<TSharedPtr<FJsonValue>> Rows;
    for(int32 I=0;I<BeforeNode.Pins.Num();++I){const auto& Old=BeforeNode.Pins[I];auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("pin_index"),I);Row->SetStringField(TEXT("pin_id"),Old.Id.ToString());Row->SetStringField(TEXT("pin_name"),Old.Name.ToString());Row->SetStringField(TEXT("old_address"),PinAddress(Old.Pointer));Row->SetStringField(TEXT("fresh_address"),PinAddress(Addresses.FindChecked(Old.Pointer)));Rows.Add(MakeShared<FJsonValueObject>(Row));}
    for(auto& E:SelectedGraph->Nodes)for(auto& P:E.Pins){for(auto*& L:P.Links)if(auto* New=Addresses.Find(L)){PeerLinks+=E.Node.Get()!=N&&L!=*New;L=*New;}if(auto* New=Addresses.Find(P.Pointer))P.Pointer=*New;}
    Receipt->SetNumberField(TEXT("pin_address_replacements"),Replaced);Receipt->SetNumberField(TEXT("peer_link_address_replacements"),PeerLinks);Receipt->SetArrayField(TEXT("pins"),Rows);Receipt->SetBoolField(TEXT("semantic_rebind_verified"),true);Receipt->SetBoolField(TEXT("fresh_snapshot_adopted"),false);
    return Admit(TEXT("variadic_reference_rebind_verified"));
}
template<typename F>bool FWork::EditNode(UEdGraphNode* N,const TCHAR* Stage,F&& Fnc)
{
 if(!Own(N)||!SDK(Stage,true))return false;Rebuilding=N;Fnc();Prefix->SetBoolField(FString(Stage)+TEXT("_SDK_returned"),true);if(!Admit(FString(TEXT("after_"))+Stage))return false;if(!RefreshNode(N))return false;Rebuilding=nullptr;return Admit(TEXT("authored_node_verified"));
}
bool FWork::AddNode(UNiagaraGraph* G,UEdGraphNode* N)
{
 if(!GraphForWrite(G)||!Pin(N)||N->GetOuter()!=G||!SDK(TEXT("Graph_AddNode"),true))return false;auto* GE=Graphs.FindByPredicate([&](const auto& E){return E.Graph.Get()==G;});if(!GE)return false;FNodeIdentity E;E.Node=N;E.Id=N->NodeGuid;GE->Nodes.Add(E);Rebuilding=N;G->AddNode(N,false,false);Prefix->SetBoolField(TEXT("Graph_AddNode_SDK_returned"),true);if(!Admit(TEXT("after_Graph_AddNode"))||!RefreshNode(N))return false;Rebuilding=nullptr;R.bChanged=true;return Admit(TEXT("node_added"));
}
bool FWork::RemoveNode(UNiagaraGraph* G,UEdGraphNode* N,bool BreakAllLinks,bool AlwaysMarkDirty,bool Destroy)
{
 if(Destroy)return Fail(TEXT("niagara_ndc_destroy_unsupported"),TEXT("No unchecked destruction path."));if(!GraphForWrite(G)||!Own(N)||!SDK(TEXT("Graph_RemoveNode"),true))return false;auto* GE=Graphs.FindByPredicate([&](const auto& E){return E.Graph.Get()==G;});if(!GE)return false;TArray<UEdGraphPin*> Removing;for(const auto& NE:GE->Nodes)if(NE.Node.Get()==N)for(const auto& P:NE.Pins)Removing.Add(P.Pointer);for(auto& Node:GE->Nodes)for(auto& P:Node.Pins)for(auto* X:Removing)P.Links.Remove(X);GE->Nodes.RemoveAll([&](const auto& E){return E.Node.Get()==N;});auto Row=MutationRow(TEXT("Graph_RemoveNode"));Row->SetStringField(TEXT("node_path"),N->GetPathName());bool Removed=G->RemoveNode(N,BreakAllLinks,AlwaysMarkDirty);Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("returned_removed"),Removed);R.bChanged|=Removed;return After(TEXT("Graph_RemoveNode"))&&Removed;
}
bool FWork::Request(bool& Accepted)
{R.bCompileRequested=true;if(!SDK(TEXT("RequestCompile"),true))return false;R.bCompileAttempted=true;R.Compiled.Reset();Accepted=System->RequestCompile(false);auto Row=MakeShared<FJsonObject>();Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("accepted"),Accepted);Row->SetBoolField(TEXT("completion_verified"),false);Compiles.Add(MakeShared<FJsonValueObject>(Row));return After(TEXT("RequestCompile"));}
bool FWork::Save(bool Direct,FString* Filename)
{
 if(Direct)return Fail(TEXT("niagara_ndc_direct_save_unsupported"),TEXT("Only exact loaded asset save is used."));R.bSaveRequested=true;auto* O=WriterRoot.Get();if(!Own(O)||!SDK(TEXT("SaveLoadedAsset"),true))return false;R.bSaveAttempted=true;auto* P=O->GetOutermost();auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("asset"),O->GetPathName());Row->SetStringField(TEXT("package"),P->GetName());Saves.Add(MakeShared<FJsonValueObject>(Row));const bool Saved=UEditorAssetLibrary::SaveLoadedAsset(O,false);R.bSaved|=Saved;SaveHealth&=Saved;Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("SDK_save_success"),Saved);Row->SetBoolField(TEXT("actual_saved_history"),R.bSaved);if(!After(TEXT("SaveLoadedAsset"))){Row->SetBoolField(TEXT("post_save_observation_skipped"),true);return false;}const bool Dirty=P->IsDirty();Row->SetBoolField(TEXT("dirty_after"),Dirty);if(Saved&&Dirty){SaveHealth=false;return Fail(TEXT("niagara_ndc_saved_package_dirty"),TEXT("SDK save true history retained but package is dirty after callbacks; no replay."));}return Saved;
}
TSharedPtr<FJsonObject> FWork::Projection()
{
 auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("projection_complete"),false);if(!Admit(TEXT("target_projection")))return O;TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& I:Objects){auto* X=I->Object.Get();if(!Budget(1,I->Path.Len()))return O;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),I->Path);J->SetStringField(TEXT("class"),X->GetClass()->GetPathName());J->SetStringField(TEXT("package"),X->GetPackage()->GetName());J->SetBoolField(TEXT("dirty"),X->GetOutermost()->IsDirty());Rows.Add(MakeShared<FJsonValueObject>(J));}O->SetArrayField(TEXT("objects"),Rows);O->SetBoolField(TEXT("projection_complete"),true);return O;
}
FString NormalizeNiagaraObjectPathForLoad(const FString& Path){FString S=FPackageName::ExportTextPathToObjectPath(Path).TrimStartAndEnd();S.TrimQuotesInline();if(S.StartsWith(TEXT("/"))&&!S.Contains(TEXT(".")))S+=TEXT(".")+FPackageName::GetShortName(S);return S;}
bool IsTempGeneratedNiagaraPath(const FString& Path){FString P=FPackageName::ExportTextPathToObjectPath(Path).TrimStartAndEnd();int32 Dot=P.Find(TEXT("."));if(Dot!=INDEX_NONE)P.LeftInline(Dot);return P.Equals(TEXT("/Game/_MCP_Temp/NiagaraGenerated"))||P.StartsWith(NiagaraTempGenerationRoot);}
FString NiagaraTypeName(const FNiagaraTypeDefinition& T){return T.GetNameText().ToString();}
FString NiagaraScriptUsageName(ENiagaraScriptUsage U){auto* E=StaticEnum<ENiagaraScriptUsage>();return E?E->GetNameStringByValue(static_cast<int64>(U)):FString::FromInt(static_cast<int32>(U));}
bool IsNiagaraDynamicAddPin(const UEdGraphPin* P){return P&&P->PinType.PinCategory==UEdGraphSchema_Niagara::PinCategoryMisc&&P->PinType.PinSubCategory==FName(TEXT("DynamicAddPin"));}
UEdGraphPin* FindGraphPinByName(UEdGraphNode* N,EEdGraphPinDirection D,FName Name,bool Exact){if(!ContinueRead()||!Live(N))return nullptr;UEdGraphPin* Result=nullptr;for(auto* P:N->Pins){if(!P||P->GetOwningNodeUnchecked()!=N){Current->Fail(TEXT("niagara_ndc_pin_invalid"),TEXT("Pin has null or foreign owner."));return nullptr;}if(P->Direction==D&&(Exact?P->PinName==Name:P->PinName.ToString().Equals(Name.ToString(),ESearchCase::IgnoreCase))){Result=P;break;}}return Result;}

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

TSharedPtr<FJsonValue> NumberArrayToJson(std::initializer_list<double> Values)
{
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    for (double Value : Values)
        JsonValues.Add(MakeShared<FJsonValueNumber>(Value));
    return MakeShared<FJsonValueArray>(JsonValues);
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

bool TryParseNiagaraTypeDefinition(const FString& Value,FNiagaraTypeDefinition& Out)
{
 if(!ContinueRead())return false;FString N=Value.TrimStartAndEnd().ToLower();N.ReplaceInline(TEXT("_"),TEXT(""));N.ReplaceInline(TEXT("-"),TEXT(""));N.ReplaceInline(TEXT(" "),TEXT(""));
 if(N==TEXT("float")||N==TEXT("scalar")){Out=FNiagaraTypeDefinition::GetFloatDef();return true;}if(N==TEXT("int")||N==TEXT("integer")){Out=FNiagaraTypeDefinition::GetIntDef();return true;}if(N==TEXT("bool")||N==TEXT("boolean")){Out=FNiagaraTypeDefinition::GetBoolDef();return true;}if(N==TEXT("color")||N==TEXT("linearcolor")){Out=FNiagaraTypeDefinition::GetColorDef();return true;}if(N==TEXT("vec2")||N==TEXT("vector2")||N==TEXT("vector2d")){Out=FNiagaraTypeDefinition::GetVec2Def();return true;}if(N==TEXT("vec3")||N==TEXT("vector3")||N==TEXT("vector")){Out=FNiagaraTypeDefinition::GetVec3Def();return true;}if(N==TEXT("vec4")||N==TEXT("vector4")){Out=FNiagaraTypeDefinition::GetVec4Def();return true;}if(N==TEXT("position")){Out=FNiagaraTypeDefinition::GetPositionDef();return true;}
 FString Path;auto Alias=[&](std::initializer_list<const TCHAR*> Values,const TCHAR* Class){for(auto* V:Values)if(N==V){Path=TEXT("/Script/Niagara.")+FString(Class);return true;}return false;};
 Alias({TEXT("niagaradatainterfacearrayposition"),TEXT("arrayposition"),TEXT("positionarray")},TEXT("NiagaraDataInterfaceArrayPosition"))||Alias({TEXT("niagaradatainterfacearrayfloat3"),TEXT("arrayfloat3"),TEXT("float3array"),TEXT("arrayvector"),TEXT("vectorarray")},TEXT("NiagaraDataInterfaceArrayFloat3"))||Alias({TEXT("niagaradatainterfacearrayfloat2"),TEXT("arrayfloat2"),TEXT("float2array"),TEXT("arrayvector2"),TEXT("vector2array")},TEXT("NiagaraDataInterfaceArrayFloat2"))||Alias({TEXT("niagaradatainterfacearrayfloat"),TEXT("arrayfloat"),TEXT("floatarray")},TEXT("NiagaraDataInterfaceArrayFloat"))||Alias({TEXT("niagaradatainterfacearrayint32"),TEXT("arrayint32"),TEXT("int32array"),TEXT("arrayint"),TEXT("intarray")},TEXT("NiagaraDataInterfaceArrayInt32"))||Alias({TEXT("niagaradatainterfacearraymesh"),TEXT("arraymesh"),TEXT("mesharray"),TEXT("staticmesharray"),TEXT("arraystaticmesh")},TEXT("NiagaraDataInterfaceArrayMesh"))||Alias({TEXT("niagaradatainterfacetexture"),TEXT("texturedi"),TEXT("texture2d"),TEXT("texture")},TEXT("NiagaraDataInterfaceTexture"));
 if(Path.IsEmpty()&&Value.Contains(TEXT("/Script/")))Path=Value.TrimStartAndEnd();if(Path.IsEmpty())return false;if(!Current->SDK(TEXT("Load_DI_Class")))return false;auto* C=LoadObject<UClass>(nullptr,*Path);Current->Prefix->SetBoolField(TEXT("DI_class_load_returned_nonnull"),C!=nullptr);if(!Current->After(TEXT("Load_DI_Class")))return false;if(!Live(C)||!C->GetPathName().Equals(Path,ESearchCase::CaseSensitive)||!C->IsChildOf(UNiagaraDataInterface::StaticClass())||C->HasAnyClassFlags(CLASS_Abstract|CLASS_NewerVersionExists)||!Current->Pin(C))return false;Out=FNiagaraTypeDefinition(C);return true;
}
UClass* ExactStockClass(const TCHAR* Path,UClass* Base)
{if(!Current->SDK(TEXT("Load_Stock_Class")))return nullptr;auto* C=LoadObject<UClass>(nullptr,Path);Current->Prefix->SetBoolField(TEXT("stock_class_returned_nonnull"),C!=nullptr);if(!Current->After(TEXT("Load_Stock_Class")))return nullptr;if(!Live(C)||!C->GetPathName().Equals(Path,ESearchCase::CaseSensitive)||!C->IsChildOf(Base)||C->HasAnyClassFlags(CLASS_Abstract|CLASS_NewerVersionExists)||!Current->Pin(C)){Current->Fail(TEXT("niagara_ndc_stock_class_unavailable"),TEXT("Exact installed Engine class unavailable; no writer."));return nullptr;}return C;}
const UNiagaraDataInterface* DICdo(UClass* C,TArray<FNiagaraFunctionSignature>& Out)
{if(!Current->Pin(C)||!Current->SDK(TEXT("DI_GetDefaultObject")))return nullptr;auto* D=Cast<UNiagaraDataInterface>(C->GetDefaultObject());Current->Prefix->SetBoolField(TEXT("DI_CDO_returned_nonnull"),D!=nullptr);if(!Current->After(TEXT("DI_GetDefaultObject"))||!Live(D)||D->GetClass()!=C||!Current->Pin(D))return nullptr;if(!Current->SDK(TEXT("DI_GetFunctionSignatures")))return nullptr;D->GetFunctionSignatures(Out);if(!Current->After(TEXT("DI_GetFunctionSignatures"))||!Current->Budget(Out.Num())||Out.Num()>8192)return nullptr;return D;}
UEdGraphPin* FindNiagaraNdcPinByType(UEdGraphNode* N,EEdGraphPinDirection Direction,const FNiagaraTypeDefinition& Type)
{if(!ContinueRead()||!Live(N))return nullptr;for(auto* P:N->Pins){if(!P||P->GetOwningNodeUnchecked()!=N){Current->Fail(TEXT("niagara_ndc_pin_owner_invalid"),TEXT("Null/foreign pin."));return nullptr;}if(P->Direction==Direction&&!IsNiagaraDynamicAddPin(P)&&UEdGraphSchema_Niagara::PinToTypeDefinition(P)==Type)return P;}return nullptr;}
UEdGraphPin* CreateTypedPin(UNiagaraNodeWithDynamicPins* N,EEdGraphPinDirection D,const FNiagaraTypeDefinition& T,FName Name)
{UEdGraphPin* P=nullptr;if(!Current->EditNode(N,TEXT("CreatePin"),[&]{P=N->CreatePin(D,UEdGraphSchema_Niagara::TypeDefinitionToPinType(T),Name);}))return nullptr;return P;}
UNiagaraNode* CreateGraphNode(UNiagaraGraph* G,UClass* C)
{if(!Current->GraphForWrite(G)||!Current->Pin(C)||!Current->SDK(TEXT("NewObject_GraphNode"),true))return nullptr;auto* N=NewObject<UNiagaraNode>(G,C,NAME_None,RF_Transactional);Current->Prefix->SetBoolField(TEXT("new_node_returned_nonnull"),N!=nullptr);if(!Current->After(TEXT("NewObject_GraphNode")))return nullptr;if(!Live(N)||N->GetClass()!=C||N->GetOuter()!=G||!Current->Pin(N)||!Current->AddNode(G,N))return nullptr;if(!Current->EditNode(N,TEXT("CreateNewGuid"),[&]{N->CreateNewGuid();})||!Current->EditNode(N,TEXT("PostPlacedNewNode"),[&]{N->PostPlacedNewNode();})||!Current->EditNode(N,TEXT("AllocateDefaultPins"),[&]{N->AllocateDefaultPins();}))return nullptr;return N;}
UNiagaraNodeFunctionCall* CreateSignatureNode(UNiagaraGraph* G,const FNiagaraFunctionSignature& Signature)
{if(!Current->GraphForWrite(G)||!Current->SDK(TEXT("NewObject_FunctionCall"),true))return nullptr;auto* N=NewObject<UNiagaraNodeFunctionCall>(G,NAME_None,RF_Transactional);Current->Prefix->SetBoolField(TEXT("new_function_returned_nonnull"),N!=nullptr);if(!Current->After(TEXT("NewObject_FunctionCall")))return nullptr;if(!Live(N)||N->GetOuter()!=G||N->GetClass()!=UNiagaraNodeFunctionCall::StaticClass()||!Current->Pin(N))return nullptr;N->Signature=Signature;if(!Current->AddNode(G,N)||!Current->EditNode(N,TEXT("FunctionCall_CreateGuid"),[&]{N->CreateNewGuid();})||!Current->EditNode(N,TEXT("FunctionCall_PostPlaced"),[&]{N->PostPlacedNewNode();})||!Current->EditNode(N,TEXT("FunctionCall_AllocatePins"),[&]{N->AllocateDefaultPins();}))return nullptr;return N;}
UNiagaraNodeFunctionCall* CreateNiagaraNdcFunctionNode(UNiagaraGraph* G,const UNiagaraDataInterface* D,FName Name)
{if(!ContinueRead()||!Live(D))return nullptr;TArray<FNiagaraFunctionSignature> A;if(!Current->SDK(TEXT("DI_GetFunctionSignatures_builder")))return nullptr;D->GetFunctionSignatures(A);if(!Current->After(TEXT("DI_GetFunctionSignatures_builder")))return nullptr;const auto* S=A.FindByPredicate([&](const auto& X){return X.Name==Name;});return S?CreateSignatureNode(G,*S):nullptr;}

FMapProperty* DefaultPairProperty(UNiagaraNodeWithDynamicPins* N)
{if(!ContinueRead()||!Live(N)||!N->GetClass()->GetPathName().Equals(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"),ESearchCase::CaseSensitive))return nullptr;auto* P=FindFProperty<FMapProperty>(N->GetClass(),TEXT("PinOutputToPinDefaultPersistentId"));auto* K=P?CastField<FStructProperty>(P->KeyProp):nullptr;auto* V=P?CastField<FStructProperty>(P->ValueProp):nullptr;if(!P||P->GetOwnerStruct()!=N->GetClass()||!K||!V||K->Struct!=TBaseStructure<FGuid>::Get()||V->Struct!=TBaseStructure<FGuid>::Get()){Current->Fail(TEXT("niagara_ndc_default_pair_property_unsupported"),TEXT("Exact stock Guid-to-Guid reflected pin-default property unavailable."));return nullptr;}return P;}
UEdGraphPin* FindNiagaraNdcMapGetDefaultPin(UNiagaraGraph* G,FName Name)
{if(!ContinueRead())return nullptr;for(UEdGraphNode* N0:G->Nodes){auto* N=Cast<UNiagaraNodeWithDynamicPins>(N0);if(!N||!N->GetClass()->GetPathName().Equals(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"),ESearchCase::CaseSensitive))continue;auto* Out=FindGraphPinByName(N,EGPD_Output,Name,true);if(!Out)continue;auto* Prop=DefaultPairProperty(N);if(!Prop)return nullptr;FScriptMapHelper M(Prop,Prop->ContainerPtrToValuePtr<void>(N));FGuid Id;for(int32 I=0;I<M.GetMaxIndex();++I)if(M.IsValidIndex(I)&&*reinterpret_cast<const FGuid*>(M.GetKeyPtr(I))==Out->PersistentGuid){Id=*reinterpret_cast<const FGuid*>(M.GetValuePtr(I));break;}for(auto* P:N->Pins)if(P->Direction==EGPD_Input&&P->PersistentGuid==Id)return P;return nullptr;}return nullptr;}
UEdGraphPin* AddNiagaraNdcMapParameterPin(UNiagaraNodeWithDynamicPins* N,const FNiagaraTypeDefinition& Type,FName Name,EEdGraphPinDirection Direction)
{
 if(!ContinueRead()||!Live(N))return nullptr;auto* G=Cast<UNiagaraGraph>(N->GetGraph());const FString CP=N->GetClass()->GetPathName();const bool Get=CP.Equals(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"),ESearchCase::CaseSensitive),Set=CP.Equals(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapSet"),ESearchCase::CaseSensitive);if(!((Get&&Direction==EGPD_Output)||(Set&&Direction==EGPD_Input))||!Current->Modify(G)||!Current->Modify(N))return nullptr;if(Get&&!DefaultPairProperty(N))return nullptr;
 FNiagaraVariable V(Type,Name);auto& Meta=G->GetAllMetaData();UNiagaraScriptVariable* SV=Meta.FindRef(V);if(!SV){if(!Current->SDK(TEXT("NewObject_ScriptVariable"),true))return nullptr;SV=NewObject<UNiagaraScriptVariable>(G,NAME_None,RF_Transactional);Current->Prefix->SetBoolField(TEXT("new_metadata_returned_nonnull"),SV!=nullptr);if(!Current->After(TEXT("NewObject_ScriptVariable"))||!Live(SV)||SV->GetOuter()!=G||!Current->Pin(SV))return nullptr;if(!Current->Step(TEXT("ScriptVariable_Init"),true,[&]{SV->Init(V,FNiagaraVariableMetaData());Meta.Add(V,SV);}))return nullptr;}else if(!Current->Pin(SV)||SV->Variable!=V)return nullptr;
 if(!Current->Modify(SV)||!Current->Step(TEXT("ScriptVariable_UpdateChangeId"),true,[&]{SV->UpdateChangeId();}))return nullptr;UEdGraphPin* P=CreateTypedPin(N,Direction,Type,Name);if(!P)return nullptr;if(!Current->EditNode(N,TEXT("ParameterPin_Metadata"),[&]{P->PersistentGuid=FGuid::NewGuid();P->PinType.PinSubCategory=FName(TEXT("ParameterPin"));P->PinFriendlyName=FText::AsCultureInvariant(Name.ToString());}))return nullptr;
 if(Get){auto* D=CreateTypedPin(N,EGPD_Input,Type,NAME_None);if(!D)return nullptr;if(!Current->EditNode(N,TEXT("MapGet_DefaultPinType"),[&]{D->PinType=P->PinType;}))return nullptr;D->PersistentGuid=FGuid::NewGuid();D->bNotConnectable=true;D->bDefaultValueIsReadOnly=true;auto* Prop=DefaultPairProperty(N);if(!Prop)return nullptr;if(!Current->Step(TEXT("MapGet_DefaultGuidPair"),true,[&]{FScriptMapHelper M(Prop,Prop->ContainerPtrToValuePtr<void>(N));int32 I=M.AddDefaultValue_Invalid_NeedsRehash();*reinterpret_cast<FGuid*>(M.GetKeyPtr(I))=P->PersistentGuid;*reinterpret_cast<FGuid*>(M.GetValuePtr(I))=D->PersistentGuid;M.Rehash();}))return nullptr;}
 if(!Current->Step(TEXT("MapParameter_Synchronize"),true,[&]{N->MarkNodeRequiresSynchronization(TEXT("Cubeless: NDC parameter pin"),true);})||!Current->Step(TEXT("MapParameter_Notify"),true,[&]{G->NotifyGraphChanged();}))return nullptr;return P;
}
UEdGraphPin* AddNiagaraNdcVariadicOutputPin(UNiagaraNodeFunctionCall* N,const FNiagaraTypeDefinition& Type,FName Name)
{if(!ContinueRead()||!N||!N->Signature.VariadicOutput())return nullptr;UEdGraphPin* Add=nullptr;for(auto* P:N->Pins)if(P->Direction==EGPD_Output&&IsNiagaraDynamicAddPin(P)){Add=P;break;}if(!Add||!ModifyPin(Add))return nullptr;bool Created=false;if(!Current->EditNode(N,TEXT("NDC_TypedVariadicOutput"),[&]{Add->PinType=UEdGraphSchema_Niagara::TypeDefinitionToPinType(Type);Add->PinName=Name;Add->PinFriendlyName=FText::GetEmpty();if(!Add->PersistentGuid.IsValid())Add->PersistentGuid=FGuid::NewGuid();FEdGraphPinType AP(UEdGraphSchema_Niagara::PinCategoryMisc,FName(TEXT("DynamicAddPin")),nullptr,EPinContainerType::None,false,FEdGraphTerminalType());Created=N->CreatePin(EGPD_Output,AP,FName(TEXT("Add")))!=nullptr;if(Created){N->Signature.AddOutput(FNiagaraVariable(Type,Name));N->MarkNodeRequiresSynchronization(TEXT("Cubeless: NDC Read variadic output"),true);}})||!Created)return nullptr;return Add;}
bool SetNiagaraNdcModuleInputBinding(UNiagaraGraph* G,FName Name,FName Binding)
{if(!ContinueRead())return false;auto* SV=G->GetScriptVariable(Name);auto* P=FindNiagaraNdcMapGetDefaultPin(G,Name);if(!SV||!P||!Current->Modify(SV)||!ModifyPin(P))return false;if(!Current->Step(TEXT("ModuleInput_Binding"),true,[&]{SV->DefaultMode=ENiagaraDefaultMode::Binding;SV->DefaultBinding.SetName(Binding);SV->UpdateChangeId();P->bHidden=true;P->bNotConnectable=true;P->bDefaultValueIsReadOnly=true;G->NotifyGraphChanged();}))return false;return SV->DefaultMode==ENiagaraDefaultMode::Binding&&SV->DefaultBinding.GetName()==Binding&&P->bHidden;}
template<typename TValue>bool SetNiagaraNdcModuleInputDefault(UNiagaraGraph* G,FName Name,const FNiagaraTypeDefinition& Type,const TValue& Value)
{if(!ContinueRead())return false;auto* SV=G->GetScriptVariable(Name);auto* P=FindNiagaraNdcMapGetDefaultPin(G,Name);if(!SV||!P)return false;FNiagaraVariable V(Type,FName(TEXT("Default")));V.SetValue(Value);FString Text;if(!Current->SDK(TEXT("Schema_EncodeDefault")))return false;auto* Schema=GetDefault<UEdGraphSchema_Niagara>();const bool Encoded=Schema&&Schema->TryGetPinDefaultValueFromNiagaraVariable(V,Text);if(!Current->After(TEXT("Schema_EncodeDefault"))||!Encoded||!Current->Modify(SV)||!ModifyPin(P))return false;if(!Current->Step(TEXT("ModuleInput_Default"),true,[&]{SV->DefaultMode=ENiagaraDefaultMode::Value;SV->DefaultBinding=FNiagaraScriptVariableBinding();SV->SetDefaultValueData(V.GetData());SV->UpdateChangeId();P->DefaultValue=Text;P->AutogeneratedDefaultValue=Text;P->bHidden=false;P->bNotConnectable=true;P->bDefaultValueIsReadOnly=true;G->NotifyGraphChanged();}))return false;return P->DefaultValue.Equals(Text,ESearchCase::CaseSensitive)&&P->AutogeneratedDefaultValue.Equals(Text,ESearchCase::CaseSensitive)&&!P->bHidden;}
bool ConnectNiagaraNdcPins(const UEdGraphSchema_Niagara* Schema,UEdGraphPin* A,UEdGraphPin* B,const FString& Label,FString& Error)
{
 if(!Schema||!Current->PinForWrite(A)||!Current->PinForWrite(B)||!Current->SDK(TEXT("Schema_CanCreateConnection"))){Error=TEXT("Missing or invalid exact pin for ")+Label;return false;}const FPinConnectionResponse Response=Schema->CanCreateConnection(A,B);if(!Current->After(TEXT("Schema_CanCreateConnection")))return false;auto R=Response.Response;if(R==CONNECT_RESPONSE_DISALLOW||R==CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE||R==CONNECT_RESPONSE_MAKE_WITH_PROMOTION){Error=TEXT("Schema rejected or requires unsupported implicit topology for ")+Label;return false;}
 if(!Current->SDK(TEXT("Schema_TryCreateConnection"),true))return false;auto* EA=FindExpectedPin(*Current,A);auto* EB=FindExpectedPin(*Current,B);auto Break=[&](FPinIdentity* P){for(auto* L:P->Links){auto* Q=FindExpectedPin(*Current,L);if(Q)Q->Links.Remove(P->Pointer);}P->Links.Empty();};if(R==CONNECT_RESPONSE_BREAK_OTHERS_A||R==CONNECT_RESPONSE_BREAK_OTHERS_AB)Break(EA);if(R==CONNECT_RESPONSE_BREAK_OTHERS_B||R==CONNECT_RESPONSE_BREAK_OTHERS_AB)Break(EB);EA->Links.AddUnique(B);EB->Links.AddUnique(A);const bool Made=Schema->TryCreateConnection(A,B);Current->Prefix->SetBoolField(TEXT("Schema_TryCreateConnection_returned"),Made);Current->R.bChanged|=Made;if(!Current->After(TEXT("Schema_TryCreateConnection"))||!Made){Error=TEXT("Schema connection failed for ")+Label;return false;}return true;
}

enum class ENiagaraNdcScratchPadStage : uint8
{
    EmitterSpawnInit,
    EmitterUpdateSpawn,
    ParticleSpawnRead,
};

bool TryParseNiagaraNdcScratchPadStage(const FString& Value, ENiagaraNdcScratchPadStage& OutStage)
{
    if (Value.Equals(TEXT("emitter_spawn_init"), ESearchCase::IgnoreCase))
    {
        OutStage = ENiagaraNdcScratchPadStage::EmitterSpawnInit;
        return true;
    }
    if (Value.Equals(TEXT("emitter_update_spawn"), ESearchCase::IgnoreCase))
    {
        OutStage = ENiagaraNdcScratchPadStage::EmitterUpdateSpawn;
        return true;
    }
    if (Value.Equals(TEXT("particle_spawn_read"), ESearchCase::IgnoreCase))
    {
        OutStage = ENiagaraNdcScratchPadStage::ParticleSpawnRead;
        return true;
    }
    return false;
}

FString NiagaraNdcScratchPadStageName(ENiagaraNdcScratchPadStage Stage)
{
    switch (Stage)
    {
    case ENiagaraNdcScratchPadStage::EmitterSpawnInit:
        return TEXT("emitter_spawn_init");
    case ENiagaraNdcScratchPadStage::EmitterUpdateSpawn:
        return TEXT("emitter_update_spawn");
    case ENiagaraNdcScratchPadStage::ParticleSpawnRead:
        return TEXT("particle_spawn_read");
    default:
        return TEXT("unknown");
    }
}

ENiagaraScriptUsage NiagaraNdcScratchPadTargetUsage(ENiagaraNdcScratchPadStage Stage)
{
    switch (Stage)
    {
    case ENiagaraNdcScratchPadStage::EmitterSpawnInit:
        return ENiagaraScriptUsage::EmitterSpawnScript;
    case ENiagaraNdcScratchPadStage::EmitterUpdateSpawn:
        return ENiagaraScriptUsage::EmitterUpdateScript;
    case ENiagaraNdcScratchPadStage::ParticleSpawnRead:
        return ENiagaraScriptUsage::ParticleSpawnScript;
    default:
        return ENiagaraScriptUsage::Module;
    }
}

bool IsAsciiNiagaraScratchPadIdentifier(const FString& Value)
{
    if (Value.IsEmpty())
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        if (!(Character == TEXT('_') || (Character >= TEXT('0') && Character <= TEXT('9')) ||
            (Character >= TEXT('A') && Character <= TEXT('Z')) || (Character >= TEXT('a') && Character <= TEXT('z'))))
        {
            return false;
        }
    }
    return true;
}

struct FNiagaraNdcPristineModuleGraph
{
    UNiagaraNodeInput* InputNode = nullptr;
    UNiagaraNodeOutput* OutputNode = nullptr;
    UNiagaraNodeWithDynamicPins* MapGetNode = nullptr;
    UNiagaraNodeWithDynamicPins* MapSetNode = nullptr;
};

bool ResolvePristineNiagaraNdcModuleGraph(
    UNiagaraGraph* Graph,
    FNiagaraNdcPristineModuleGraph& OutNodes,
    FString& OutError)
{
    if (!ContinueRead() || !Graph)
    {
        OutError = TEXT("Scratch Pad template has no editable Niagara graph");
        return false;
    }
    if (Graph->Nodes.Num() != 4)
    {
        OutError = FString::Printf(
            TEXT("Fresh Scratch Pad template must contain exactly 4 nodes; found %d"),
            Graph->Nodes.Num());
        return false;
    }

    int32 InputNodeCount = 0;
    int32 OutputNodeCount = 0;
    int32 MapGetNodeCount = 0;
    int32 MapSetNodeCount = 0;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(Node))
        {
            if (InputNode->Input.GetType() == FNiagaraTypeDefinition::GetParameterMapDef())
            {
                OutNodes.InputNode = InputNode;
                ++InputNodeCount;
                continue;
            }
        }
        if (UNiagaraNodeOutput* OutputNode = Cast<UNiagaraNodeOutput>(Node))
        {
            if (OutputNode->GetUsage() == ENiagaraScriptUsage::Module)
            {
                OutNodes.OutputNode = OutputNode;
                ++OutputNodeCount;
                continue;
            }
        }
        if (Node && Node->GetClass()->GetName().Equals(TEXT("NiagaraNodeParameterMapGet")))
        {
            OutNodes.MapGetNode = Cast<UNiagaraNodeWithDynamicPins>(Node);
            ++MapGetNodeCount;
            continue;
        }
        if (Node && Node->GetClass()->GetName().Equals(TEXT("NiagaraNodeParameterMapSet")))
        {
            OutNodes.MapSetNode = Cast<UNiagaraNodeWithDynamicPins>(Node);
            ++MapSetNodeCount;
            continue;
        }

        OutError = FString::Printf(
            TEXT("Fresh Scratch Pad template contains unsupported node class: %s"),
            Node ? *Node->GetClass()->GetName() : TEXT("null"));
        return false;
    }

    if (InputNodeCount != 1 || OutputNodeCount != 1 || MapGetNodeCount != 1 || MapSetNodeCount != 1 ||
        !OutNodes.InputNode || !OutNodes.OutputNode || !OutNodes.MapGetNode || !OutNodes.MapSetNode)
    {
        OutError = FString::Printf(
            TEXT("Fresh Scratch Pad template topology mismatch (input=%d output=%d map_get=%d map_set=%d)"),
            InputNodeCount,
            OutputNodeCount,
            MapGetNodeCount,
            MapSetNodeCount);
        return false;
    }
    return true;
}

TSharedPtr<FJsonObject> ChannelLayoutJson(UNiagaraDataChannel* C)
{auto O=MakeShared<FJsonObject>();if(!ContinueRead()||!Live(C))return O;TArray<TSharedPtr<FJsonValue>> A;for(const auto& V:C->GetVariables()){if(!Current->Budget(1,V.GetName().ToString().Len()+V.GetType().GetName().Len()))return O;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),V.GetName().ToString());J->SetStringField(TEXT("type"),V.GetType().GetName());A.Add(MakeShared<FJsonValueObject>(J));}O->SetArrayField(TEXT("variables"),A);O->SetBoolField(TEXT("keep_previous_frame_data"),C->KeepPreviousFrameData());return O;}
bool CaptureChannel(UNiagaraDataChannelAsset* A)
{auto& W=*Current;if(!W.Pin(A))return false;auto* C=A->Get();if(C&&!W.Pin(C))return false;W.ChannelAsset=A;W.ExpectedChannel=C;W.ChannelCaptured=true;if(C){if(C->GetOuter()!=A||C->GetPackage()!=A->GetPackage()||C->GetOutermost()!=A->GetOutermost()||!W.Budget(C->GetVariables().Num())||C->GetVariables().Num()>65536)return W.Fail(TEXT("niagara_ndc_channel_owner_invalid"),TEXT("Channel subobject ownership/schema bound invalid."));W.ExpectedVariables.Reset();W.ExpectedVariables.Append(C->GetVariables().GetData(),C->GetVariables().Num());W.ExpectedKeep=C->KeepPreviousFrameData();W.ChannelSchemaCaptured=true;}return W.Admit(TEXT("channel_captured"));}
UNiagaraDataChannelAsset* LoadChannel(const FString& Path)
{auto& W=*Current;if(!W.SDK(TEXT("Load_DataChannelAsset")))return nullptr;const FString Expected=NormalizeNiagaraObjectPathForLoad(Path);auto* A=LoadObject<UNiagaraDataChannelAsset>(nullptr,*Expected);W.Prefix->SetBoolField(TEXT("channel_load_returned_nonnull"),A!=nullptr);if(!W.After(TEXT("Load_DataChannelAsset")))return nullptr;if(!A)return nullptr;if(!Live(A)||A->GetClass()!=UNiagaraDataChannelAsset::StaticClass()||!A->GetPathName().Equals(Expected,ESearchCase::CaseSensitive)||!CaptureChannel(A)){W.Fail(TEXT("niagara_ndc_channel_identity_invalid"),TEXT("Loaded DataChannel asset differs from exact requested class/path/owner."));return nullptr;}return A;}
bool FreshObjectPath(const FString& Path,UPackage* AllowedPackage=nullptr)
{auto& W=*Current;if(!W.Admit(TEXT("fresh_asset_admission")))return false;FString PackagePath=FPackageName::ObjectPathToPackageName(Path);if(!FPackageName::IsValidLongPackageName(PackagePath,false)||FPackageName::GetLongPackageAssetName(PackagePath).IsEmpty())return W.Fail(TEXT("niagara_ndc_asset_path_invalid"),TEXT("Valid long package/object name required."));if(FindObject<UObject>(nullptr,*Path)||FPackageName::DoesPackageExist(PackagePath))return W.Fail(TEXT("niagara_ndc_creation_collision"),TEXT("Exact requested asset memory or disk already exists; no overwrite."));if(!W.SDK(TEXT("AssetRegistry_LoadModule")))return false;auto* Registry=FModuleManager::LoadModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry"));if(!W.After(TEXT("AssetRegistry_LoadModule")))return false;if(!Registry)return W.Fail(TEXT("niagara_ndc_registry_unavailable"),TEXT("Asset registry unavailable for exact creation admission."));if(!W.SDK(TEXT("AssetRegistry_FreshPath")))return false;const bool Exists=Registry->Get().GetAssetByObjectPath(FSoftObjectPath(Path)).IsValid();if(!W.After(TEXT("AssetRegistry_FreshPath")))return false;if(Exists)return W.Fail(TEXT("niagara_ndc_registry_collision"),TEXT("Exact requested asset registry path already exists."));auto* ExistingPackage=FindPackage(nullptr,*PackagePath);if(FindObject<UObject>(nullptr,*Path)||FPackageName::DoesPackageExist(PackagePath)||(AllowedPackage?(ExistingPackage!=AllowedPackage||!Live(AllowedPackage)||!AllowedPackage->GetName().Equals(PackagePath,ESearchCase::CaseSensitive)):ExistingPackage!=nullptr))return W.Fail(TEXT("niagara_ndc_creation_collision"),TEXT("Exact memory/package/file collision after registry SDK; no overwrite."));return true;}
TSharedPtr<FJsonObject> HandleConfigure(const TSharedPtr<FJsonObject>& Params)
{
 auto& W=*Current;W.Writing=true;FString Path;Params->TryGetStringField(TEXT("asset_path"),Path);if(Path.IsEmpty())return W.Error(TEXT("asset_path is required"));bool Create=true,Keep=true,Save=true;Params->TryGetBoolField(TEXT("create_if_missing"),Create);Params->TryGetBoolField(TEXT("keep_previous_frame_data"),Keep);Params->TryGetBoolField(TEXT("save"),Save);const FString Exact=NormalizeNiagaraObjectPathForLoad(Path);
 // Missing StaticLoadObject creates an empty package in UE58. Select the existing
 // path without that loader side effect; this observation never grants creation.
 const FString ExistingPackagePath=FPackageName::ObjectPathToPackageName(Exact);
 const bool ObjectPresent=FindObject<UObject>(nullptr,*Exact)!=nullptr;
 const bool PackageFilePresent=FPackageName::DoesPackageExist(ExistingPackagePath);
 if(!W.SDK(TEXT("AssetRegistry_ConfigureModule")))return W.Error(W.R.Error);
 auto* ConfigureRegistry=FModuleManager::LoadModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry"));
 if(!W.After(TEXT("AssetRegistry_ConfigureModule")))return W.Error(W.R.Error);
 if(!ConfigureRegistry)return W.Error(TEXT("Asset registry unavailable for configure path selection"));
 if(!W.SDK(TEXT("AssetRegistry_ConfigurePath")))return W.Error(W.R.Error);
 const bool RegistryPresent=ConfigureRegistry->Get().GetAssetByObjectPath(FSoftObjectPath(Exact),false,false).IsValid();
 if(!W.After(TEXT("AssetRegistry_ConfigurePath")))return W.Error(W.R.Error);
 W.Prefix->SetBoolField(TEXT("configure_object_present"),ObjectPresent);
 W.Prefix->SetBoolField(TEXT("configure_package_file_present"),PackageFilePresent);
 W.Prefix->SetBoolField(TEXT("configure_registry_present"),RegistryPresent);
 const bool LoadExisting=ObjectPresent||PackageFilePresent||RegistryPresent;
 W.Prefix->SetBoolField(TEXT("skipped_missing_asset_load"),!LoadExisting);
 UNiagaraDataChannelAsset* A=LoadExisting?LoadChannel(Path):nullptr;
 bool Created=false,CreatedChannel=false;
 if(!W.R.Error.IsEmpty())return W.Error(W.R.Error);if(!A){if(!Create)return W.Error(TEXT("Data channel asset not found and create_if_missing is false"));if(!FreshObjectPath(Exact))return W.Error(W.R.Error);const FString PP=FPackageName::ObjectPathToPackageName(Exact),Name=FPackageName::GetLongPackageAssetName(PP);if(!W.SDK(TEXT("CreatePackage"),true))return W.Error(W.R.Error);auto* P=CreatePackage(*PP);W.Prefix->SetBoolField(TEXT("CreatePackage_returned_nonnull"),P!=nullptr);if(!W.After(TEXT("CreatePackage"))||!Live(P)||!P->GetName().Equals(PP,ESearchCase::CaseSensitive)||!W.Pin(P))return W.Error(TEXT("Failed exact package creation"));if(FindObject<UObject>(P,*Name)||!FreshObjectPath(Exact,P))return W.Error(TEXT("New asset path collision before factory"));if(!W.SDK(TEXT("New_DataChannelAsset"),true))return W.Error(W.R.Error);A=NewObject<UNiagaraDataChannelAsset>(P,*Name,RF_Public|RF_Standalone|RF_Transactional);W.Prefix->SetBoolField(TEXT("asset_creation_returned_nonnull"),A!=nullptr);if(!W.After(TEXT("New_DataChannelAsset"))||!Live(A)||A->GetClass()!=UNiagaraDataChannelAsset::StaticClass()||A->GetOuter()!=P||!A->GetPathName().Equals(Exact,ESearchCase::CaseSensitive)||!CaptureChannel(A))return W.Error(TEXT("New Data Channel asset identity invalid"));W.R.bChanged=true;Created=true;if(!W.Step(TEXT("AssetCreated"),true,[&]{FAssetRegistryModule::AssetCreated(A);}))return W.Error(W.R.Error);}
 W.WriterRoot=A;W.Before=W.Projection();if(!W.R.Error.IsEmpty()||!W.Modify(A))return W.Error(W.R.Error);auto* CP=FindFProperty<FObjectPropertyBase>(UNiagaraDataChannelAsset::StaticClass(),TEXT("DataChannel"));if(!CP||CP->GetOwnerStruct()!=UNiagaraDataChannelAsset::StaticClass()||CP->PropertyClass!=UNiagaraDataChannel::StaticClass())return W.Error(TEXT("Exact reflected DataChannel property unavailable"));UNiagaraDataChannel* C=A->Get();if(!C){if(!W.SDK(TEXT("New_GlobalChannel"),true))return W.Error(W.R.Error);C=NewObject<UNiagaraDataChannel_Global>(A,NAME_None,RF_Transactional);W.Prefix->SetBoolField(TEXT("channel_creation_returned_nonnull"),C!=nullptr);if(!W.After(TEXT("New_GlobalChannel"))||!Live(C)||C->GetOuter()!=A||C->GetClass()!=UNiagaraDataChannel_Global::StaticClass()||!W.Pin(C))return W.Error(TEXT("New Global channel identity invalid"));if(!W.SDK(TEXT("Set_DataChannel"),true))return W.Error(W.R.Error);W.ExpectedChannel=C;CP->SetObjectPropertyValue_InContainer(A,C);if(!W.After(TEXT("Set_DataChannel")))return W.Error(W.R.Error);if(!CaptureChannel(A))return W.Error(W.R.Error);CreatedChannel=true;W.R.bChanged=true;}if(!W.Modify(C))return W.Error(W.R.Error);
 const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;if(Params->TryGetArrayField(TEXT("variables"),Values)&&Values){if(Values->Num()>65536)return W.Error(TEXT("Data Channel variables exceed bounded inventory"));TArray<FNiagaraDataChannelVariable> Next;for(const auto& V:*Values){const TSharedPtr<FJsonObject>* J=nullptr;if(!V||!V->TryGetObject(J)||!J||!J->IsValid())return W.Error(TEXT("variables entries must be objects with name/type fields"));FString Name,Type;(*J)->TryGetStringField(TEXT("name"),Name);(*J)->TryGetStringField(TEXT("type"),Type);if(Name.IsEmpty()||Type.IsEmpty())return W.Error(TEXT("Each variable needs non-empty name and type"));FNiagaraTypeDefinition T;if(!TryParseNiagaraTypeDefinition(Type,T))return W.Error(TEXT("Unsupported Niagara type for data channel variable: ")+Type);if(!FNiagaraDataChannelVariable::IsAllowedType(T))return W.Error(TEXT("Installed Data Channel public IsAllowedType rejects this variable type"));FNiagaraDataChannelVariable X;X.SetName(FName(*Name));X.SetType(FNiagaraDataChannelVariable::ToDataChannelType(T));Next.Add(X);}auto* VP=FindFProperty<FArrayProperty>(C->GetClass(),TEXT("ChannelVariables"));auto* Inner=VP?CastField<FStructProperty>(VP->Inner):nullptr;if(!VP||VP->GetOwnerStruct()!=UNiagaraDataChannel::StaticClass()||!Inner||Inner->Struct!=FNiagaraDataChannelVariable::StaticStruct())return W.Error(TEXT("Exact ChannelVariables reflected array type unavailable"));if(!W.SDK(TEXT("Copy_ChannelVariables"),true))return W.Error(W.R.Error);W.ExpectedVariables=Next;VP->CopyCompleteValue(VP->ContainerPtrToValuePtr<void>(C),&Next);W.R.bChanged=true;if(!W.After(TEXT("Copy_ChannelVariables")))return W.Error(W.R.Error);}
 auto* KP=FindFProperty<FBoolProperty>(C->GetClass(),TEXT("bKeepPreviousFrameData"));if(!KP||KP->GetOwnerStruct()!=UNiagaraDataChannel::StaticClass())return W.Error(TEXT("Exact previous-frame reflected bool unavailable"));if(!W.SDK(TEXT("Set_KeepPreviousFrameData"),true))return W.Error(W.R.Error);W.ExpectedKeep=Keep;KP->SetPropertyValue_InContainer(C,Keep);W.R.bChanged=true;if(!W.After(TEXT("Set_KeepPreviousFrameData")))return W.Error(W.R.Error);if(!W.Step(TEXT("Channel_PostEditChange"),true,[&]{C->PostEditChange();})||!W.Step(TEXT("Asset_PostEditChange"),true,[&]{A->PostEditChange();})||!W.Step(TEXT("Asset_MarkPackageDirty"),true,[&]{A->MarkPackageDirty();}))return W.Error(W.R.Error);if(Save&&!W.Save(false))return W.Error(TEXT("Failed to save data channel asset"));if(!W.Admit(TEXT("configured_layout_readback")))return W.Error(W.R.Error);auto O=ChannelLayoutJson(C);O->SetBoolField(TEXT("success"),true);O->SetStringField(TEXT("asset_path"),A->GetPathName());O->SetBoolField(TEXT("created_asset"),Created);O->SetBoolField(TEXT("created_channel_object"),CreatedChannel);O->SetStringField(TEXT("channel_class"),C->GetClass()->GetName());O->SetBoolField(TEXT("saved"),W.R.bSaved);O->SetStringField(TEXT("write_scope"),TEXT("niagara_data_channel_asset"));W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),true);return O;
}

FString CleanupNodeTitle(const UEdGraphNode* N,ENodeTitleType::Type Kind)
{FString Out;if(!N||!Current->Step(TEXT("node_GetNodeTitle"),false,[&]{Out=N->GetNodeTitle(Kind).ToString();}))return {};if(!Current->Budget(0,Out.Len()))return {};return Out;}

FString CleanupNodeTooltip(const UEdGraphNode* N)
{FString Out;if(!N||!Current->Step(TEXT("node_GetTooltipText"),false,[&]{Out=N->GetTooltipText().ToString();}))return {};if(!Current->Budget(0,Out.Len()))return {};return Out;}

FString CleanupFunctionName(const UNiagaraNodeFunctionCall* N)
{FString Out;if(!N||!Current->Step(TEXT("node_GetFunctionName"),false,[&]{Out=N->GetFunctionName();}))return {};return Out;}

TArray<TSharedPtr<FJsonValue>> StringsToJson(const TArray<FString>& Values)
{
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    for (const FString& Value : Values)
    {
        JsonValues.Add(MakeShared<FJsonValueString>(Value));
    }
    return JsonValues;
}

void AddUniqueString(TArray<FString>& Values, const FString& Value)
{
    if (!Value.IsEmpty() && !Values.Contains(Value))
    {
        Values.Add(Value);
    }
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

FString NiagaraObjectPathOrEmpty(const UObject* Object)
{
    return Object ? Object->GetPathName() : FString();
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
    if (!ContinueRead()||!Pin)
    {
        return PinObject;
    }

    if(!Current->Budget(1,Pin->PinName.ToString().Len()+Pin->DefaultValue.Len()+Pin->PinToolTip.Len()+Pin->PinFriendlyName.ToString().Len()))return PinObject;
    PinObject->SetStringField(TEXT("name"), Pin->PinName.ToString());
    PinObject->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
    PinObject->SetStringField(TEXT("category"), Pin->PinType.PinCategory.ToString());
    PinObject->SetStringField(TEXT("subcategory"), Pin->PinType.PinSubCategory.ToString());
    PinObject->SetStringField(TEXT("default_value"), Pin->DefaultValue);
    PinObject->SetStringField(TEXT("default_object"), NiagaraObjectPathOrEmpty(Pin->DefaultObject));
    PinObject->SetNumberField(TEXT("linked_to_count"), Pin->LinkedTo.Num());
    return PinObject;
}

TSharedPtr<FJsonObject> DetailedPinRefToJsonObject(const UEdGraphPin* Pin)
{
    TSharedPtr<FJsonObject> PinObject = MakeShared<FJsonObject>();
    if (!ContinueRead()||!Pin)
    {
        return PinObject;
    }

    const UEdGraphNode* OwningNode = Pin->GetOwningNode();
    PinObject->SetStringField(TEXT("pin_id"), Pin->PinId.ToString(EGuidFormats::DigitsWithHyphens));
    PinObject->SetStringField(TEXT("pin_name"), Pin->PinName.ToString());
    PinObject->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
    PinObject->SetStringField(TEXT("node_name"), OwningNode ? OwningNode->GetName() : FString());
    PinObject->SetStringField(TEXT("node_guid"), OwningNode ? OwningNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens) : FString());
    PinObject->SetStringField(TEXT("node_class"), OwningNode ? OwningNode->GetClass()->GetName() : FString());
    return PinObject;
}

TSharedPtr<FJsonObject> DetailedPinToJsonObject(const UEdGraphPin* Pin, bool bIncludeLinkedPins)
{
    TSharedPtr<FJsonObject> PinObject = GraphPinToJsonObject(Pin);
    if (!ContinueRead()||!Pin)
    {
        return PinObject;
    }

    PinObject->SetStringField(TEXT("pin_id"), Pin->PinId.ToString(EGuidFormats::DigitsWithHyphens));
    PinObject->SetStringField(TEXT("persistent_guid"), Pin->PersistentGuid.ToString(EGuidFormats::DigitsWithHyphens));
    PinObject->SetStringField(TEXT("friendly_name"), Pin->PinFriendlyName.ToString());
    PinObject->SetStringField(TEXT("tooltip"), Pin->PinToolTip);
    PinObject->SetBoolField(TEXT("hidden"), Pin->bHidden);
    PinObject->SetBoolField(TEXT("not_connectable"), Pin->bNotConnectable);
    PinObject->SetBoolField(TEXT("advanced_view"), Pin->bAdvancedView);

    if (bIncludeLinkedPins)
    {
        TArray<TSharedPtr<FJsonValue>> LinkedValues;
        for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
        {
            if(!ContinueRead())return PinObject;
            LinkedValues.Add(MakeShared<FJsonValueObject>(DetailedPinRefToJsonObject(LinkedPin)));
        }
        PinObject->SetArrayField(TEXT("linked_pins"), LinkedValues);
    }

    return PinObject;
}

TArray<TSharedPtr<FJsonValue>> DetailedPinsToJson(TArray<UEdGraphPin*> Pins, bool bIncludeLinkedPins)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    for (const UEdGraphPin* Pin : Pins)
    {
        Values.Add(MakeShared<FJsonValueObject>(DetailedPinToJsonObject(Pin, bIncludeLinkedPins)));
    }
    return Values;
}

TSharedPtr<FJsonObject> DetailedGraphNodeToJsonObject(
    const UEdGraphNode* Node,
    int32 NodeIndex,
    bool bIncludePins,
    bool bIncludeLinkedPins)
{
    TSharedPtr<FJsonObject> NodeObject = MakeShared<FJsonObject>();
    if (!Node)
    {
        return NodeObject;
    }

    const FString Title=CleanupNodeTitle(Node,ENodeTitleType::ListView);if(!ContinueRead())return NodeObject;
    const FString Tooltip=CleanupNodeTooltip(Node);if(!ContinueRead())return NodeObject;
    NodeObject->SetNumberField(TEXT("node_index"), NodeIndex);
    NodeObject->SetStringField(TEXT("node_name"), Node->GetName());
    NodeObject->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    NodeObject->SetStringField(TEXT("node_class"), Node->GetClass()->GetName());
    NodeObject->SetStringField(TEXT("node_title"), Title);
    NodeObject->SetStringField(TEXT("tooltip"), Tooltip);
    NodeObject->SetNumberField(TEXT("x"), Node->NodePosX);
    NodeObject->SetNumberField(TEXT("y"), Node->NodePosY);
    NodeObject->SetNumberField(TEXT("width"), Node->NodeWidth);
    NodeObject->SetNumberField(TEXT("height"), Node->NodeHeight);
    NodeObject->SetNumberField(TEXT("pin_count"), Node->Pins.Num());

    if (const UNiagaraNodeFunctionCall* FunctionCall = Cast<UNiagaraNodeFunctionCall>(Node))
    {
        FString FunctionName = CleanupFunctionName(FunctionCall);
        if(!ContinueRead())return NodeObject;
        if (FunctionName.IsEmpty())
        {
            FunctionName = FunctionCall->Signature.GetNameString();
        }

        const UNiagaraScript* FunctionScript = FunctionCall->FunctionScript;
        const UObject* ScriptOuter = FunctionScript ? FunctionScript->GetOuter() : nullptr;
        NodeObject->SetStringField(TEXT("niagara_node_kind"), TEXT("function_call"));
        NodeObject->SetStringField(TEXT("function_name"), FunctionName);
        NodeObject->SetStringField(TEXT("function_script"), NiagaraObjectPathOrEmpty(FunctionScript));
        NodeObject->SetStringField(TEXT("function_script_asset_object_path"), FunctionCall->FunctionScriptAssetObjectPath.ToString());
        NodeObject->SetStringField(TEXT("called_usage"), NiagaraScriptUsageName(FunctionCall->GetCalledUsage()));
        NodeObject->SetBoolField(TEXT("is_scratch_pad"), ScriptOuter && ScriptOuter->IsA<UNiagaraScratchPadContainer>());
        NodeObject->SetStringField(TEXT("signature_name"), FunctionCall->Signature.GetNameString());
        NodeObject->SetArrayField(TEXT("signature_inputs"), NiagaraVariablesToJson(FunctionCall->Signature.Inputs));
        NodeObject->SetArrayField(TEXT("signature_outputs"), NiagaraVariableBasesToJson(FunctionCall->Signature.Outputs));
        NodeObject->SetArrayField(TEXT("control_hints"), StringsToJson(BuildNiagaraControlHints(FunctionName)));
    }
    else if (const UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(Node))
    {
        NodeObject->SetStringField(TEXT("niagara_node_kind"), TEXT("input"));
        NodeObject->SetObjectField(TEXT("variable"), NiagaraVariableToJsonObject(InputNode->Input));
        NodeObject->SetNumberField(TEXT("call_sort_priority"), InputNode->CallSortPriority);
        NodeObject->SetBoolField(TEXT("is_exposed"), InputNode->IsExposed());
        NodeObject->SetBoolField(TEXT("is_required"), InputNode->IsRequired());
        NodeObject->SetBoolField(TEXT("is_hidden"), InputNode->IsHidden());
        NodeObject->SetBoolField(TEXT("can_auto_bind"), InputNode->CanAutoBind());
        NodeObject->SetArrayField(TEXT("control_hints"), StringsToJson(BuildNiagaraControlHints(InputNode->Input.GetName().ToString())));
    }
    else if (const UNiagaraNodeOutput* OutputNode = Cast<UNiagaraNodeOutput>(Node))
    {
        NodeObject->SetStringField(TEXT("niagara_node_kind"), TEXT("output"));
        NodeObject->SetStringField(TEXT("usage"), NiagaraScriptUsageName(OutputNode->GetUsage()));
        NodeObject->SetStringField(TEXT("usage_id"), OutputNode->GetUsageId().ToString(EGuidFormats::DigitsWithHyphens));
        NodeObject->SetArrayField(TEXT("outputs"), NiagaraVariablesToJson(OutputNode->GetOutputs()));
    }
    else
    {
        NodeObject->SetStringField(TEXT("niagara_node_kind"), TEXT("generic"));
    }

    if (bIncludePins)
    {
        NodeObject->SetArrayField(TEXT("pins"), DetailedPinsToJson(Node->Pins, bIncludeLinkedPins));
    }

    return NodeObject;
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
TArray<TSharedPtr<FJsonValue>> NiagaraScriptUsagesToJson(const TArray<ENiagaraScriptUsage>& Usages)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    for (ENiagaraScriptUsage Usage : Usages)
    {
        Values.Add(MakeShared<FJsonValueString>(NiagaraScriptUsageName(Usage)));
    }
    return Values;
}
TSharedPtr<FJsonObject> BuildFunctionCallJson(const UNiagaraNodeFunctionCall* FunctionCall, int32 NodeIndex, bool bIncludePins)
{
    TSharedPtr<FJsonObject> NodeObject = MakeShared<FJsonObject>();
    if (!FunctionCall)
    {
        return NodeObject;
    }

    FString FunctionName=CleanupFunctionName(FunctionCall);if(!ContinueRead())return NodeObject;
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
        for(auto* P:FunctionCall->Pins){if(P->Direction==EGPD_Input)InputPins.Add(P);else OutputPins.Add(P); }
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
    // Created reader templates carry a parameter-map input, never an asset/DI default.
    auto* OP=FindFProperty<FObjectPropertyBase>(InputNode->GetClass(),TEXT("ObjectAsset"));auto* DP=FindFProperty<FObjectPropertyBase>(InputNode->GetClass(),TEXT("DataInterface"));
    const UObject* OA=OP?OP->GetObjectPropertyValue_InContainer(InputNode):nullptr;const UObject* DI=DP?DP->GetObjectPropertyValue_InContainer(InputNode):nullptr;
    if(OA||DI||InputNode->Input.GetType().IsDataInterface()){Current->Fail(TEXT("niagara_ndc_template_input_diagnostic_unsupported"),TEXT("Unexpected asset/DI-bearing input in the pristine reader template; existing object was not dereferenced or changed."));return InputObject;}
    auto Ref=MakeShared<FJsonObject>();Ref->SetBoolField(TEXT("is_valid"),false);Ref->SetBoolField(TEXT("is_texture_render_target"),false);Ref->SetBoolField(TEXT("is_texture_render_target_2d"),false);InputObject->SetObjectField(TEXT("object_asset"),Ref);
    InputObject->SetArrayField(TEXT("control_hints"), StringsToJson(BuildNiagaraControlHints(InputNode->Input.GetName().ToString())));
    return InputObject;
}
TSharedPtr<FJsonObject> BuildOutputNodeJson(const UNiagaraNodeOutput* OutputNode)
{
    TSharedPtr<FJsonObject> OutputObject = MakeShared<FJsonObject>();
    if (!OutputNode)
    {
        return OutputObject;
    }

    OutputObject->SetStringField(TEXT("node_name"), OutputNode->GetName());
    OutputObject->SetStringField(TEXT("node_guid"), OutputNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    OutputObject->SetStringField(TEXT("usage"), NiagaraScriptUsageName(OutputNode->GetUsage()));
    OutputObject->SetStringField(TEXT("usage_id"), OutputNode->GetUsageId().ToString(EGuidFormats::DigitsWithHyphens));
    OutputObject->SetArrayField(TEXT("outputs"), NiagaraVariablesToJson(OutputNode->GetOutputs()));
    return OutputObject;
}
TSharedPtr<FJsonObject> BuildGraphAnalysisJson(
    const UNiagaraScriptSourceBase* SourceBase,
    const FString& Context,
    bool bIncludePins,
    int32 MaxFunctionCalls)
{
    TSharedPtr<FJsonObject> GraphObject=MakeShared<FJsonObject>();if(!ContinueRead())return GraphObject;
    GraphObject->SetStringField(TEXT("context"), Context);
    GraphObject->SetStringField(TEXT("source_class"), SourceBase ? SourceBase->GetClass()->GetName() : FString());
    GraphObject->SetStringField(TEXT("source_path"), NiagaraObjectPathOrEmpty(SourceBase));

    const UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(SourceBase);
    UNiagaraGraph* Graph = Source ? Source->NodeGraph : nullptr;
    GraphObject->SetStringField(TEXT("graph_path"), NiagaraObjectPathOrEmpty(Graph));
    GraphObject->SetBoolField(TEXT("has_graph"), Graph != nullptr);

    if (!Graph)
    {
        GraphObject->SetNumberField(TEXT("node_count"), 0);
        GraphObject->SetNumberField(TEXT("function_call_count"), 0);
        GraphObject->SetNumberField(TEXT("input_node_count"), 0);
        GraphObject->SetNumberField(TEXT("output_node_count"), 0);
        GraphObject->SetArrayField(TEXT("function_calls"), TArray<TSharedPtr<FJsonValue>>());
        GraphObject->SetArrayField(TEXT("input_nodes"), TArray<TSharedPtr<FJsonValue>>());
        GraphObject->SetArrayField(TEXT("output_nodes"), TArray<TSharedPtr<FJsonValue>>());
        return GraphObject;
    }

    TArray<UNiagaraNodeFunctionCall*> FunctionCalls;
    TArray<UNiagaraNodeInput*> InputNodes;
    TArray<UNiagaraNodeOutput*> OutputNodes;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (UNiagaraNodeFunctionCall* FunctionCall = Cast<UNiagaraNodeFunctionCall>(Node))
        {
            FunctionCalls.Add(FunctionCall);
        }
        else if (UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(Node))
        {
            InputNodes.Add(InputNode);
        }
        else if (UNiagaraNodeOutput* OutputNode = Cast<UNiagaraNodeOutput>(Node))
        {
            OutputNodes.Add(OutputNode);
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
    InputNodes.Sort(
        [](const UNiagaraNodeInput& Left, const UNiagaraNodeInput& Right)
        {
            return Left.CallSortPriority < Right.CallSortPriority;
        });

    TArray<TSharedPtr<FJsonValue>> FunctionValues;
    TArray<FString> ControlHints;
    const int32 SafeMaxFunctionCalls = FMath::Max(0, MaxFunctionCalls);
    for (int32 Index = 0; Index < FunctionCalls.Num() && Index < SafeMaxFunctionCalls; ++Index)
    {
        TSharedPtr<FJsonObject> FunctionObject = BuildFunctionCallJson(FunctionCalls[Index], Index, bIncludePins);
        if(!ContinueRead())return GraphObject;
        for (const TSharedPtr<FJsonValue>& HintValue : FunctionObject->GetArrayField(TEXT("control_hints")))
        {
            FString Hint;
            if (HintValue->TryGetString(Hint))
            {
                AddUniqueString(ControlHints, Hint);
            }
        }
        FunctionValues.Add(MakeShared<FJsonValueObject>(FunctionObject));
    }

    TArray<TSharedPtr<FJsonValue>> InputValues;
    for (const UNiagaraNodeInput* InputNode : InputNodes)
    {
        TSharedPtr<FJsonObject> InputObject = BuildInputNodeJson(InputNode);
        if(!ContinueRead())return GraphObject;
        for (const TSharedPtr<FJsonValue>& HintValue : InputObject->GetArrayField(TEXT("control_hints")))
        {
            FString Hint;
            if (HintValue->TryGetString(Hint))
            {
                AddUniqueString(ControlHints, Hint);
            }
        }
        InputValues.Add(MakeShared<FJsonValueObject>(InputObject));
    }

    TArray<TSharedPtr<FJsonValue>> OutputValues;
    for (const UNiagaraNodeOutput* OutputNode : OutputNodes)
    {
        OutputValues.Add(MakeShared<FJsonValueObject>(BuildOutputNodeJson(OutputNode)));
    }

    GraphObject->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
    GraphObject->SetNumberField(TEXT("function_call_count"), FunctionCalls.Num());
    GraphObject->SetNumberField(TEXT("input_node_count"), InputNodes.Num());
    GraphObject->SetNumberField(TEXT("output_node_count"), OutputNodes.Num());
    GraphObject->SetBoolField(TEXT("function_calls_truncated"), FunctionCalls.Num() > SafeMaxFunctionCalls);
    GraphObject->SetArrayField(TEXT("control_hints"), StringsToJson(ControlHints));
    GraphObject->SetArrayField(TEXT("function_calls"), FunctionValues);
    GraphObject->SetArrayField(TEXT("input_nodes"), InputValues);
    GraphObject->SetArrayField(TEXT("output_nodes"), OutputValues);
    return GraphObject;
}
TSharedPtr<FJsonObject> BuildScratchPadInterfaceJson(
    const UNiagaraScript* Script,
    const FString& OwnerKind,
    const FString& OwnerName,
    int32 OwnerEmitterIndex,
    const FString& ContainerKind,
    int32 ScriptIndex,
    bool bIncludeGraphSummary,
    int32 MaxFunctionCalls)
{
    TSharedPtr<FJsonObject> ScriptObject = MakeShared<FJsonObject>();
    if (!ContinueRead()||!Script)
    {
        return ScriptObject;
    }

    TArray<ENiagaraScriptUsage> SupportedUsageContexts;
    if (const FVersionedNiagaraScriptData* ScriptData = Script->GetLatestScriptData())
    {
        SupportedUsageContexts = ScriptData->GetSupportedUsageContexts();
    }

    const FString Context = FString::Printf(
        TEXT("%s:%s:%s"),
        *OwnerKind,
        *ContainerKind,
        *Script->GetName());
    TSharedPtr<FJsonObject> GraphObject = BuildGraphAnalysisJson(
        Script->GetLatestSource(),
        Context,
        false,
        MaxFunctionCalls);

    if(!ContinueRead())return ScriptObject;
    ScriptObject->SetStringField(TEXT("name"), Script->GetName());
    ScriptObject->SetStringField(TEXT("path"), Script->GetPathName());
    ScriptObject->SetStringField(TEXT("owner_kind"), OwnerKind);
    ScriptObject->SetStringField(TEXT("owner_name"), OwnerName);
    ScriptObject->SetNumberField(TEXT("owner_emitter_index"), OwnerEmitterIndex);
    ScriptObject->SetStringField(TEXT("container_kind"), ContainerKind);
    ScriptObject->SetNumberField(TEXT("script_index"), ScriptIndex);
    ScriptObject->SetStringField(TEXT("usage"), NiagaraScriptUsageName(Script->GetUsage()));
    ScriptObject->SetArrayField(TEXT("supported_usage_contexts"), NiagaraScriptUsagesToJson(SupportedUsageContexts));
    ScriptObject->SetBoolField(TEXT("has_graph"), GraphObject->GetBoolField(TEXT("has_graph")));
    ScriptObject->SetNumberField(TEXT("node_count"), GraphObject->GetNumberField(TEXT("node_count")));
    ScriptObject->SetNumberField(TEXT("function_call_count"), GraphObject->GetNumberField(TEXT("function_call_count")));
    ScriptObject->SetNumberField(TEXT("input_count"), GraphObject->GetNumberField(TEXT("input_node_count")));
    ScriptObject->SetNumberField(TEXT("output_count"), GraphObject->GetNumberField(TEXT("output_node_count")));
    const TArray<TSharedPtr<FJsonValue>>* ControlHints = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Outputs = nullptr;
    ScriptObject->SetArrayField(TEXT("control_hints"), GraphObject->TryGetArrayField(TEXT("control_hints"), ControlHints) ? *ControlHints : TArray<TSharedPtr<FJsonValue>>());
    ScriptObject->SetArrayField(TEXT("inputs"), GraphObject->TryGetArrayField(TEXT("input_nodes"), Inputs) ? *Inputs : TArray<TSharedPtr<FJsonValue>>());
    ScriptObject->SetArrayField(TEXT("outputs"), GraphObject->TryGetArrayField(TEXT("output_nodes"), Outputs) ? *Outputs : TArray<TSharedPtr<FJsonValue>>());
    ScriptObject->SetBoolField(TEXT("read_only"), true);
    ScriptObject->SetStringField(TEXT("authoring_status"), TEXT("interface_inspection_only"));
    if (bIncludeGraphSummary)
    {
        ScriptObject->SetObjectField(TEXT("graph_summary"), GraphObject);
    }
    return ScriptObject;
}


TSharedPtr<FJsonObject> ReaderGraphFingerprint(UNiagaraGraph* G)
{
 auto& W=*Current;auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("complete"),false);if(!W.Admit(TEXT("reader_typed_graph_snapshot"))||!Live(G))return O;
 TArray<TSharedPtr<FJsonValue>> Nodes,Metadata;
 for(UEdGraphNode* N:G->Nodes){if(!W.Budget(1))return O;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("guid"),N->NodeGuid.ToString());J->SetStringField(TEXT("class"),N->GetClass()->GetPathName());TArray<TSharedPtr<FJsonValue>> Pins;
  for(auto* P:N->Pins){if(!W.Budget(1,P->DefaultValue.Len()+P->AutogeneratedDefaultValue.Len()))return O;auto Q=DetailedPinToJsonObject(P,false);Q->RemoveField(TEXT("tooltip")); /* Localized lazy PostLoad presentation remains in detailed diagnostics, outside authored semantic equality. */ TArray<TSharedPtr<FJsonValue>> Links;for(auto* L:P->LinkedTo){auto LJ=MakeShared<FJsonObject>();LJ->SetStringField(TEXT("node_guid"),L->GetOwningNodeUnchecked()->NodeGuid.ToString());LJ->SetStringField(TEXT("pin_id"),L->PinId.ToString());Links.Add(MakeShared<FJsonValueObject>(LJ));}Q->SetArrayField(TEXT("linked_pins"),Links);Q->SetStringField(TEXT("autogenerated_default"),P->AutogeneratedDefaultValue);Q->SetStringField(TEXT("type"),NiagaraTypeName(UEdGraphSchema_Niagara::PinToTypeDefinition(P)));Pins.Add(MakeShared<FJsonValueObject>(Q));}J->SetArrayField(TEXT("pins"),Pins);
  if(auto* F=Cast<UNiagaraNodeFunctionCall>(N)){J->SetStringField(TEXT("function"),F->Signature.Name.ToString());J->SetArrayField(TEXT("signature_inputs"),NiagaraVariablesToJson(F->Signature.Inputs));J->SetArrayField(TEXT("signature_outputs"),NiagaraVariableBasesToJson(F->Signature.Outputs));J->SetNumberField(TEXT("required_inputs"),F->Signature.RequiredInputs);J->SetNumberField(TEXT("required_outputs"),F->Signature.RequiredOutputs);TArray<FString> S;for(const auto& K:F->FunctionSpecifiers)S.Add(K.Key.ToString()+TEXT("=")+K.Value.ToString());S.Sort();J->SetArrayField(TEXT("specifiers"),StringsToJson(S));}
  if(auto* I=Cast<UNiagaraNodeInput>(N))J->SetObjectField(TEXT("input_variable"),NiagaraVariableToJsonObject(I->Input));if(auto* Out=Cast<UNiagaraNodeOutput>(N)){J->SetArrayField(TEXT("outputs"),NiagaraVariablesToJson(Out->GetOutputs()));J->SetStringField(TEXT("usage"),NiagaraScriptUsageName(Out->GetUsage()));}
  Nodes.Add(MakeShared<FJsonValueObject>(J));
 }
 TArray<FString> Names;for(const auto& K:G->GetAllMetaData())Names.Add(K.Key.GetName().ToString());Names.Sort();for(const auto& Name:Names){auto* S=G->GetScriptVariable(FName(*Name));if(!Live(S)||!W.Pin(S))return O;auto J=MakeShared<FJsonObject>();J->SetObjectField(TEXT("variable"),NiagaraVariableToJsonObject(S->Variable));J->SetNumberField(TEXT("default_mode"),static_cast<int32>(S->DefaultMode));J->SetStringField(TEXT("binding"),S->DefaultBinding.GetName().ToString());TArray<TSharedPtr<FJsonValue>> Bytes;const int32 Count=S->GetDefaultValueVariant().GetNumBytes();if(Count<0||Count>65536||!W.Budget(Count))return O;const uint8* P=S->GetDefaultValueData();if(P)for(int32 I=0;I<Count;++I)Bytes.Add(MakeShared<FJsonValueNumber>(P[I]));J->SetArrayField(TEXT("default_bytes"),Bytes);Metadata.Add(MakeShared<FJsonValueObject>(J));}
 O->SetArrayField(TEXT("nodes"),Nodes);O->SetArrayField(TEXT("metadata"),Metadata);O->SetBoolField(TEXT("complete"),true);return O;
}
TSharedPtr<FJsonObject> HandleReader(const TSharedPtr<FJsonObject>& Params)
{
FWork& W=*Current;
    if (!Params.IsValid())
    {
        return W.Error(TEXT("Missing parameters"));
    }

    FString SystemPath;
    FString ScratchPadName;
    FString DataChannelAssetPath;
    FString StageString = TEXT("particle_spawn_read");
    Params->TryGetStringField(TEXT("system_path"), SystemPath);
    Params->TryGetStringField(TEXT("scratch_pad_name"), ScratchPadName);
    Params->TryGetStringField(TEXT("data_channel_asset_path"), DataChannelAssetPath);
    Params->TryGetStringField(TEXT("stage"), StageString);
    SystemPath.TrimStartAndEndInline();
    ScratchPadName.TrimStartAndEndInline();
    DataChannelAssetPath.TrimStartAndEndInline();
    StageString.TrimStartAndEndInline();
    if (SystemPath.IsEmpty() || ScratchPadName.IsEmpty() || DataChannelAssetPath.IsEmpty())
    {
        return W.Error(
            TEXT("system_path, scratch_pad_name, and data_channel_asset_path are required"));
    }
    if (!IsAsciiNiagaraScratchPadIdentifier(ScratchPadName))
    {
        return W.Error(
            TEXT("scratch_pad_name must contain only ASCII letters, digits, or underscores"));
    }

    ENiagaraNdcScratchPadStage Stage = ENiagaraNdcScratchPadStage::ParticleSpawnRead;
    if(StageString.Equals(TEXT("direct_index_read"),ESearchCase::IgnoreCase)){W.Fail(TEXT("niagara_ndc_direct_index_read_unsupported"),TEXT("Historical direct_index_read requires its distinct graph/index contract, not the current particle_spawn_read implementation."));return W.Error(W.R.Error);}
    if (!TryParseNiagaraNdcScratchPadStage(StageString, Stage))
    {
        return W.Error(
            TEXT("stage must be emitter_spawn_init, emitter_update_spawn, or particle_spawn_read"));
    }
    const ENiagaraScriptUsage TargetUsage = NiagaraNdcScratchPadTargetUsage(Stage);

    bool bAllowSourceEdit = false;
    bool bDryRun = false;
    bool bSave = true;
    bool bRequestCompile = true;
    bool bReadCurrentFrame = false;
    Params->TryGetBoolField(TEXT("allow_source_edit"), bAllowSourceEdit);
    Params->TryGetBoolField(TEXT("dry_run"), bDryRun);
    Params->TryGetBoolField(TEXT("save"), bSave);
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);
    Params->TryGetBoolField(TEXT("read_current_frame"), bReadCurrentFrame);
    if (!bAllowSourceEdit && !IsTempGeneratedNiagaraPath(SystemPath))
    {
        return W.Error(
            FString::Printf(TEXT("Refusing to create an NDC reader Scratch Pad outside %s: %s"), NiagaraTempGenerationRoot, *SystemPath));
    }

    FString PositionVariableName = TEXT("Position");
    FString VelocityVariableName = TEXT("Velocity");
    FString SpawnCountVariableName = TEXT("SpawnCount");
    FString EventTypeVariableName = TEXT("EventType");
    FString PositionTarget = TEXT("Particles.Position");
    FString VelocityTarget = TEXT("Particles.Velocity");
    FString SpawnCountTarget = TEXT("Particles.NDCSpawnCount");
    FString EventTypeTarget = TEXT("Particles.NDCEventType");
    FString ReadSuccessTarget = TEXT("Particles.NDCReadSuccess");
    Params->TryGetStringField(TEXT("position_variable"), PositionVariableName);
    Params->TryGetStringField(TEXT("velocity_variable"), VelocityVariableName);
    Params->TryGetStringField(TEXT("spawn_count_variable"), SpawnCountVariableName);
    Params->TryGetStringField(TEXT("event_type_variable"), EventTypeVariableName);
    Params->TryGetStringField(TEXT("position_target"), PositionTarget);
    Params->TryGetStringField(TEXT("velocity_target"), VelocityTarget);
    Params->TryGetStringField(TEXT("spawn_count_target"), SpawnCountTarget);
    Params->TryGetStringField(TEXT("event_type_target"), EventTypeTarget);
    Params->TryGetStringField(TEXT("read_success_target"), ReadSuccessTarget);

    TArray<FString*> SourceNames = {
        &PositionVariableName,
        &VelocityVariableName,
        &SpawnCountVariableName,
        &EventTypeVariableName,
    };
    TSet<FString> UniqueSourceNames;
    for (FString* SourceName : SourceNames)
    {
        SourceName->TrimStartAndEndInline();
        if (SourceName->IsEmpty() || SourceName->Contains(TEXT(".")))
        {
            return W.Error(
                TEXT("NDC source variable names must be non-empty, unqualified names"));
        }
        const FString CanonicalName = SourceName->ToLower();
        if (UniqueSourceNames.Contains(CanonicalName))
        {
            return W.Error(
                FString::Printf(TEXT("NDC source variable names must be unique: %s"), **SourceName));
        }
        UniqueSourceNames.Add(CanonicalName);
    }

    TArray<FString*> TargetNames = {
        &PositionTarget,
        &VelocityTarget,
        &SpawnCountTarget,
        &EventTypeTarget,
        &ReadSuccessTarget,
    };
    TSet<FString> UniqueTargetNames;
    for (FString* TargetName : TargetNames)
    {
        TargetName->TrimStartAndEndInline();
        if (!TargetName->StartsWith(TEXT("Particles.")) || TargetName->Len() <= 10)
        {
            return W.Error(
                FString::Printf(TEXT("Particle targets must use a non-empty Particles.* namespace: %s"), **TargetName));
        }
        const FString CanonicalName = TargetName->ToLower();
        if (UniqueTargetNames.Contains(CanonicalName))
        {
            return W.Error(
                FString::Printf(TEXT("Particle target names must be unique: %s"), **TargetName));
        }
        UniqueTargetNames.Add(CanonicalName);
    }

    W.Writing=!bDryRun;UNiagaraSystem* System = LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(
            FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    auto ScratchPadNameExists = [&ScratchPadName](const TArray<TObjectPtr<UNiagaraScript>>& Scripts)
    {
        return Scripts.ContainsByPredicate(
            [&ScratchPadName](const TObjectPtr<UNiagaraScript>& Script)
            {
                return Script && Script->GetName().Equals(ScratchPadName, ESearchCase::IgnoreCase);
            });
    };
    bool bScratchPadAlreadyExists = ScratchPadNameExists(System->ScratchPadScripts);
    for (const FNiagaraEmitterHandle& EmitterHandle : System->GetEmitterHandles())
    {
        if (const FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
        {
            bScratchPadAlreadyExists = bScratchPadAlreadyExists ||
                (EmitterData->ScratchPads && ScratchPadNameExists(EmitterData->ScratchPads->Scripts)) ||
                (EmitterData->ParentScratchPads && ScratchPadNameExists(EmitterData->ParentScratchPads->Scripts));
        }
    }
    if (bScratchPadAlreadyExists || FindObject<UObject>(System, *ScratchPadName))
    {
        return W.Error(
            FString::Printf(TEXT("Scratch Pad name already exists; this command never overwrites: %s"), *ScratchPadName));
    }

    UNiagaraDataChannelAsset* DataChannelAsset = LoadChannel(DataChannelAssetPath);
    UNiagaraDataChannel* DataChannel = DataChannelAsset ? DataChannelAsset->Get() : nullptr;
    if (!DataChannelAsset || !DataChannel)
    {
        return W.Error(
            FString::Printf(TEXT("Failed to load Niagara Data Channel asset: %s"), *DataChannelAssetPath));
    }
    if (!DataChannel->IsA<UNiagaraDataChannel_Global>())
    {
        return W.Error(
            FString::Printf(TEXT("This narrow command requires a Global Niagara Data Channel; found %s"), *DataChannel->GetClass()->GetName()));
    }
    if (!bReadCurrentFrame && !DataChannel->KeepPreviousFrameData())
    {
        return W.Error(
            TEXT("read_current_frame=false requires the Data Channel to keep previous-frame data"));
    }

    struct FRequiredNdcVariable
    {
        FString Role;
        FString SourceName;
        FString TargetName;
        FNiagaraTypeDefinition ExpectedType;
        TSharedPtr<FNiagaraDataChannelVariable> ResolvedVariable;
    };
    TArray<FRequiredNdcVariable> RequiredVariables = {
        {TEXT("position"), PositionVariableName, PositionTarget, FNiagaraTypeDefinition::GetPositionDef(), nullptr},
        {TEXT("velocity"), VelocityVariableName, VelocityTarget,
            FNiagaraDataChannelVariable::ToDataChannelType(FNiagaraTypeDefinition::GetVec3Def()), nullptr},
        {TEXT("spawn_count"), SpawnCountVariableName, SpawnCountTarget, FNiagaraTypeDefinition::GetIntDef(), nullptr},
        {TEXT("event_type"), EventTypeVariableName, EventTypeTarget, FNiagaraTypeDefinition::GetIntDef(), nullptr},
    };
    for (FRequiredNdcVariable& RequiredVariable : RequiredVariables)
    {
        for (const FNiagaraDataChannelVariable& Candidate : DataChannel->GetVariables())
        {
            if (Candidate.GetName().ToString().Equals(RequiredVariable.SourceName, ESearchCase::CaseSensitive))
            {
                RequiredVariable.ResolvedVariable = MakeShared<FNiagaraDataChannelVariable>(Candidate);
                break;
            }
        }
        if (!RequiredVariable.ResolvedVariable)
        {
            return W.Error(
                FString::Printf(TEXT("Data Channel is missing required variable '%s' for role %s"),
                    *RequiredVariable.SourceName,
                    *RequiredVariable.Role));
        }
        if (RequiredVariable.ResolvedVariable->GetType() != RequiredVariable.ExpectedType)
        {
            return W.Error(
                FString::Printf(TEXT("Data Channel variable '%s' has type %s; expected %s"),
                    *RequiredVariable.SourceName,
                    *NiagaraTypeName(RequiredVariable.ResolvedVariable->GetType()),
                    *NiagaraTypeName(RequiredVariable.ExpectedType)));
        }
    }

    if(!W.SDK(TEXT("Load_DefaultModule")))return W.Error(W.R.Error);
    UNiagaraScript* DefaultModule = Cast<UNiagaraScript>(GetDefault<UNiagaraEditorSettings>()->DefaultModuleScript.TryLoad());
    if(!W.After(TEXT("Load_DefaultModule"))||!Live(DefaultModule)||!W.CaptureScript(DefaultModule))return W.Error(TEXT("Default module load/identity failed"));
    UNiagaraScriptSource* DefaultSource = DefaultModule ? Cast<UNiagaraScriptSource>(DefaultModule->GetLatestSource()) : nullptr;
    UNiagaraGraph* DefaultGraph = DefaultSource ? DefaultSource->NodeGraph : nullptr;
    if(!DefaultSource||!W.CaptureGraph(DefaultSource))return W.Error(TEXT("Default template source unavailable"));
    FNiagaraNdcPristineModuleGraph DefaultGraphNodes;
    FString TopologyError;
    if (!DefaultModule || !ResolvePristineNiagaraNdcModuleGraph(DefaultGraph, DefaultGraphNodes, TopologyError))
    {
        return W.Error(
            FString::Printf(TEXT("Niagara DefaultModuleScript is not a supported fresh template: %s"), *TopologyError));
    }

    BuildInputNodeJson(DefaultGraphNodes.InputNode);if(!W.Admit(TEXT("reader_template_input_preflight")))return W.Error(W.R.Error);
    UClass* ReaderClass=ExactStockClass(TEXT("/Script/Niagara.NiagaraDataInterfaceDataChannelRead"),UNiagaraDataInterface::StaticClass());
    TArray<FNiagaraFunctionSignature> ReaderSignatures;
    const UNiagaraDataInterface* ReaderCdo=ReaderClass?DICdo(ReaderClass,ReaderSignatures):nullptr;
    if(!ReaderCdo)return W.Error(TEXT("Failed to resolve installed public Data Channel reader signatures"));
    auto HasReaderSignature = [&ReaderSignatures](const FName Name)
    {
        return ReaderSignatures.ContainsByPredicate(
            [&Name](const FNiagaraFunctionSignature& Signature)
            {
                return Signature.Name == Name;
            });
    };
    if (Stage == ENiagaraNdcScratchPadStage::EmitterUpdateSpawn && !HasReaderSignature(FName(TEXT("SpawnDirect"))))
    {
        return W.Error(TEXT("Niagara Data Channel Read DI has no SpawnDirect signature"));
    }
    if (Stage == ENiagaraNdcScratchPadStage::ParticleSpawnRead &&
        (!HasReaderSignature(FName(TEXT("GetNDCSpawnData"))) || !HasReaderSignature(FName(TEXT("Read")))))
    {
        return W.Error(
            TEXT("Niagara Data Channel Read DI is missing GetNDCSpawnData or Read"));
    }

    TArray<TSharedPtr<FJsonValue>> ChannelSchemaJson;
    for (const FNiagaraDataChannelVariable& Variable : DataChannel->GetVariables())
    {
        TSharedPtr<FJsonObject> VariableJson = MakeShared<FJsonObject>();
        VariableJson->SetStringField(TEXT("name"), Variable.GetName().ToString());
        VariableJson->SetStringField(TEXT("type"), NiagaraTypeName(Variable.GetType()));
        ChannelSchemaJson.Add(MakeShared<FJsonValueObject>(VariableJson));
    }
    TArray<TSharedPtr<FJsonValue>> MappingJson;
    for (const FRequiredNdcVariable& Variable : RequiredVariables)
    {
        TSharedPtr<FJsonObject> VariableJson = MakeShared<FJsonObject>();
        VariableJson->SetStringField(TEXT("role"), Variable.Role);
        VariableJson->SetStringField(TEXT("source_name"), Variable.SourceName);
        VariableJson->SetStringField(TEXT("source_type"), NiagaraTypeName(Variable.ExpectedType));
        VariableJson->SetStringField(TEXT("target_name"), Variable.TargetName);
        MappingJson.Add(MakeShared<FJsonValueObject>(VariableJson));
    }

    auto BuildBaseResult = [&]()
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("system_path"), System->GetPathName());
        Result->SetStringField(TEXT("scratch_pad_name"), ScratchPadName);
        Result->SetStringField(TEXT("stage"), NiagaraNdcScratchPadStageName(Stage));
        Result->SetStringField(TEXT("target_usage"), NiagaraScriptUsageName(TargetUsage));
        Result->SetStringField(TEXT("data_channel_asset_path"), DataChannelAsset->GetPathName());
        Result->SetStringField(TEXT("data_channel_class"), DataChannel->GetClass()->GetName());
        Result->SetBoolField(TEXT("keep_previous_frame_data"), DataChannel->KeepPreviousFrameData());
        Result->SetBoolField(TEXT("read_current_frame"), bReadCurrentFrame);
        Result->SetNumberField(TEXT("data_channel_variable_count"), DataChannel->GetVariables().Num());
        Result->SetArrayField(TEXT("data_channel_schema"), ChannelSchemaJson);
        Result->SetArrayField(TEXT("mapped_variables"), MappingJson);
        Result->SetStringField(TEXT("data_channel_module_input"), TEXT("Module.DataChannel"));
        Result->SetStringField(TEXT("shared_reader_parameter"), TEXT("Emitter.SpawnDataChannel"));
        Result->SetBoolField(TEXT("position_transform_authored"), false);
        Result->SetBoolField(TEXT("requires_world_space_emitter"), true);
        Result->SetBoolField(TEXT("stack_inserted"), false);
        Result->SetBoolField(TEXT("data_interface_asset_bound"), false);
        return Result;
    };

    if (bDryRun)
    {
        TSharedPtr<FJsonObject> Result = BuildBaseResult();
        Result->SetBoolField(TEXT("dry_run"), true);
        Result->SetBoolField(TEXT("preflight_pass"), true);
        Result->SetBoolField(TEXT("mutation_performed"), false);
        Result->SetBoolField(TEXT("saved"), false);
        Result->SetNumberField(TEXT("template_node_count"), DefaultGraph->Nodes.Num());
        Result->SetStringField(TEXT("template_path"), DefaultModule->GetPathName());
        Result->SetStringField(TEXT("write_scope"), TEXT("niagara_ndc_reader_scratch_pad_module_preflight"));
        W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),true);return Result;
    }

    if(!W.Admit(TEXT("reader_prewrite")))return W.Error(W.R.Error);W.Before=W.Projection();if(!W.Admit(TEXT("reader_transient_prewrite")))return W.Error(W.R.Error);
    const FName WorkingName = MakeUniqueObjectName(
        GetTransientPackage(),
        UNiagaraScript::StaticClass(),
        FName(*FString::Printf(TEXT("MCP_NdcReaderWork_%s"), *ScratchPadName)));
    if(!W.SDK(TEXT("Duplicate_WorkingScript"),true))return W.Error(W.R.Error);
    UNiagaraScript* WorkingScript = Cast<UNiagaraScript>(
        StaticDuplicateObject(DefaultModule, GetTransientPackage(), WorkingName, RF_Transient | RF_Transactional));
    W.Prefix->SetBoolField(TEXT("working_duplicate_returned_nonnull"),WorkingScript!=nullptr);if(!W.After(TEXT("Duplicate_WorkingScript")))return W.Error(W.R.Error);
    if (!WorkingScript)
    {
        return W.Error(TEXT("Failed to duplicate the Niagara default module into transient working memory"));
    }
    if(!Live(WorkingScript)||WorkingScript->GetClass()!=UNiagaraScript::StaticClass()||WorkingScript->GetOuter()!=GetTransientPackage()||WorkingScript->GetFName()!=WorkingName)return W.Error(TEXT("Transient working duplicate exact identity failed"));
    W.LocalRoots.Add(WorkingScript);if(!W.CaptureScript(WorkingScript))return W.Error(W.R.Error);
    if(!W.Step(TEXT("WorkingScript_FlagsAndUsage"),true,[&]{WorkingScript->ClearFlags(RF_Public|RF_Standalone);if(auto* SD=WorkingScript->GetLatestScriptData())SD->ModuleUsageBitmask|=(1<<static_cast<int32>(TargetUsage));}))return W.Error(W.R.Error);
    auto* WorkingSource=Cast<UNiagaraScriptSource>(WorkingScript->GetLatestSource());auto* Graph=WorkingSource?WorkingSource->NodeGraph.Get():nullptr;
    if(!WorkingSource||!W.CaptureGraph(WorkingSource)||!W.GraphForWrite(Graph))return W.Error(TEXT("Transient working source/graph identity failed"));
    FNiagaraNdcPristineModuleGraph GraphNodes;
    if(!ResolvePristineNiagaraNdcModuleGraph(Graph,GraphNodes,TopologyError))return W.Error(TEXT("Transient duplicate topology failed: ")+TopologyError);
    const UEdGraphSchema_Niagara* GraphSchema = GetDefault<UEdGraphSchema_Niagara>();
    if (!GraphSchema)
    {
        return W.Error(TEXT("Scratch Pad graph has no Niagara schema"));
    }

    const FNiagaraTypeDefinition ReaderType(ReaderClass);
    const FName ModuleDataChannelName(TEXT("Module.DataChannel"));
    const FName SharedDataChannelName(TEXT("Emitter.SpawnDataChannel"));
    FString WiringError;

    UEdGraphPin* DataChannelInputPin = AddNiagaraNdcMapParameterPin(
        GraphNodes.MapGetNode,
        ReaderType,
        ModuleDataChannelName,
        EGPD_Output);
    if (!DataChannelInputPin)
    {
        return W.Error(TEXT("Failed to add Module.DataChannel input to the Scratch Pad"));
    }

    if (Stage == ENiagaraNdcScratchPadStage::EmitterSpawnInit)
    {
        UEdGraphPin* SharedDataChannelOutputPin = AddNiagaraNdcMapParameterPin(
            GraphNodes.MapSetNode,
            ReaderType,
            SharedDataChannelName,
            EGPD_Input);
        if (!ConnectNiagaraNdcPins(
                GraphSchema,
                DataChannelInputPin,
                SharedDataChannelOutputPin,
                TEXT("Module.DataChannel -> Emitter.SpawnDataChannel"),
                WiringError))
        {
            return W.Error(WiringError);
        }
    }
    else if (Stage == ENiagaraNdcScratchPadStage::EmitterUpdateSpawn)
    {
        if (!SetNiagaraNdcModuleInputBinding(Graph, ModuleDataChannelName, SharedDataChannelName))
        {
            return W.Error(TEXT("Failed to bind Module.DataChannel to Emitter.SpawnDataChannel"));
        }

        UEdGraphPin* ModuleInputMapPin = FindNiagaraNdcPinByType(
            GraphNodes.InputNode,
            EGPD_Output,
            FNiagaraTypeDefinition::GetParameterMapDef());
        UEdGraphPin* ModuleOutputMapPin = FindNiagaraNdcPinByType(
            GraphNodes.OutputNode,
            EGPD_Input,
            FNiagaraTypeDefinition::GetParameterMapDef());
        if(!W.RemoveNode(Graph,GraphNodes.MapSetNode,true,false))return W.Error(W.R.Error);
        GraphNodes.MapSetNode = nullptr;

        UNiagaraNodeFunctionCall* SpawnFunction = CreateNiagaraNdcFunctionNode(
            Graph,
            ReaderCdo,
            FName(TEXT("SpawnDirect")));
        if (!SpawnFunction)
        {
            return W.Error(TEXT("Failed to create the SpawnDirect Data Channel function node"));
        }
        UEdGraphPin* SpawnExecInput = FindNiagaraNdcPinByType(
            SpawnFunction,
            EGPD_Input,
            FNiagaraTypeDefinition::GetParameterMapDef());
        UEdGraphPin* SpawnExecOutput = FindNiagaraNdcPinByType(
            SpawnFunction,
            EGPD_Output,
            FNiagaraTypeDefinition::GetParameterMapDef());
        UEdGraphPin* SpawnDiInput = FindNiagaraNdcPinByType(SpawnFunction, EGPD_Input, ReaderType);
        if (!ConnectNiagaraNdcPins(GraphSchema, ModuleInputMapPin, SpawnExecInput, TEXT("module input -> SpawnDirect exec"), WiringError) ||
            !ConnectNiagaraNdcPins(GraphSchema, SpawnExecOutput, ModuleOutputMapPin, TEXT("SpawnDirect exec -> module output"), WiringError) ||
            !ConnectNiagaraNdcPins(GraphSchema, DataChannelInputPin, SpawnDiInput, TEXT("Data Channel -> SpawnDirect"), WiringError))
        {
            return W.Error(WiringError);
        }

        UEdGraphPin* EnablePin = AddNiagaraNdcMapParameterPin(
            GraphNodes.MapGetNode,
            FNiagaraTypeDefinition::GetBoolDef(),
            FName(TEXT("Module.SpawnEnabled")),
            EGPD_Output);
        UEdGraphPin* EmitterIdPin = AddNiagaraNdcMapParameterPin(
            GraphNodes.MapGetNode,
            FNiagaraTypeDefinition(FNiagaraEmitterID::StaticStruct()),
            FName(TEXT("Module.EmitterID")),
            EGPD_Output);
        UEdGraphPin* MinCountPin = AddNiagaraNdcMapParameterPin(
            GraphNodes.MapGetNode,
            FNiagaraTypeDefinition::GetIntDef(),
            FName(TEXT("Module.MinCount")),
            EGPD_Output);
        UEdGraphPin* MaxCountPin = AddNiagaraNdcMapParameterPin(
            GraphNodes.MapGetNode,
            FNiagaraTypeDefinition::GetIntDef(),
            FName(TEXT("Module.MaxCount")),
            EGPD_Output);
        UEdGraphPin* RandomScaleMinPin = AddNiagaraNdcMapParameterPin(
            GraphNodes.MapGetNode,
            FNiagaraTypeDefinition::GetFloatDef(),
            FName(TEXT("Module.RandomScaleMin")),
            EGPD_Output);
        UEdGraphPin* RandomScaleMaxPin = AddNiagaraNdcMapParameterPin(
            GraphNodes.MapGetNode,
            FNiagaraTypeDefinition::GetFloatDef(),
            FName(TEXT("Module.RandomScaleMax")),
            EGPD_Output);
        if (!EnablePin || !EmitterIdPin || !MinCountPin || !MaxCountPin || !RandomScaleMinPin || !RandomScaleMaxPin)
        {
            return W.Error(TEXT("Failed to add one or more SpawnDirect module inputs"));
        }

        if (!SetNiagaraNdcModuleInputDefault(Graph, EnablePin->PinName, FNiagaraTypeDefinition::GetBoolDef(), FNiagaraBool(true)) ||
            !SetNiagaraNdcModuleInputBinding(Graph, EmitterIdPin->PinName, SYS_PARAM_ENGINE_EMITTER_ID.GetName()) ||
            !SetNiagaraNdcModuleInputDefault(Graph, MinCountPin->PinName, FNiagaraTypeDefinition::GetIntDef(), int32(-1)) ||
            !SetNiagaraNdcModuleInputDefault(Graph, MaxCountPin->PinName, FNiagaraTypeDefinition::GetIntDef(), int32(-1)) ||
            !SetNiagaraNdcModuleInputDefault(Graph, RandomScaleMinPin->PinName, FNiagaraTypeDefinition::GetFloatDef(), 1.0f) ||
            !SetNiagaraNdcModuleInputDefault(Graph, RandomScaleMaxPin->PinName, FNiagaraTypeDefinition::GetFloatDef(), 1.0f))
        {
            return W.Error(TEXT("Failed to set one or more SpawnDirect module-input defaults"));
        }

        struct FSpawnInputConnection
        {
            UEdGraphPin* SourcePin;
            FName TargetPinName;
        };
        const TArray<FSpawnInputConnection> SpawnInputConnections = {
            {EnablePin, FName(TEXT("Enable"))},
            {EmitterIdPin, FName(TEXT("Emitter ID"))},
            {MinCountPin, FName(TEXT("ClampMin"))},
            {MaxCountPin, FName(TEXT("ClampMax"))},
            {RandomScaleMinPin, FName(TEXT("RandomScaleMin"))},
            {RandomScaleMaxPin, FName(TEXT("RandomScaleMax"))},
        };
        for (const FSpawnInputConnection& Connection : SpawnInputConnections)
        {
            UEdGraphPin* TargetPin = FindGraphPinByName(SpawnFunction, EGPD_Input, Connection.TargetPinName, false);
            if (!ConnectNiagaraNdcPins(
                    GraphSchema,
                    Connection.SourcePin,
                    TargetPin,
                    FString::Printf(TEXT("module input -> SpawnDirect.%s"), *Connection.TargetPinName.ToString()),
                    WiringError))
            {
                return W.Error(WiringError);
            }
        }

        FString SpawnCountTypeText;
        UScriptStruct* TypeStruct = FNiagaraTypeDefinition::StaticStruct();
        FNiagaraTypeDefinition SpawnCountType = FNiagaraTypeDefinition::GetIntDef();
        TypeStruct->ExportText(SpawnCountTypeText, &SpawnCountType, nullptr, nullptr, PPF_None, nullptr);
        if(!W.Modify(SpawnFunction)||!W.SDK(TEXT("SpawnDirect_Specifiers"),true))return W.Error(W.R.Error);
        SpawnFunction->FunctionSpecifiers.FindOrAdd(FName(TEXT("VarName"))) = FName(*SpawnCountVariableName);
        SpawnFunction->FunctionSpecifiers.FindOrAdd(FName(TEXT("VarType"))) = FName(*SpawnCountTypeText);
        if(!W.After(TEXT("SpawnDirect_Specifiers"))||!W.Step(TEXT("SpawnDirect_Synchronize"),true,[&]{SpawnFunction->MarkNodeRequiresSynchronization(TEXT("MCP: configure NDC SpawnDirect"),true);}))return W.Error(W.R.Error);
    }
    else
    {
        if (!SetNiagaraNdcModuleInputBinding(Graph, ModuleDataChannelName, SharedDataChannelName))
        {
            return W.Error(TEXT("Failed to bind Module.DataChannel to Emitter.SpawnDataChannel"));
        }

        UNiagaraNodeFunctionCall* SpawnDataFunction = CreateNiagaraNdcFunctionNode(
            Graph,
            ReaderCdo,
            FName(TEXT("GetNDCSpawnData")));
        UNiagaraNodeFunctionCall* ReadFunction = CreateNiagaraNdcFunctionNode(
            Graph,
            ReaderCdo,
            FName(TEXT("Read")));
        if (!SpawnDataFunction || !ReadFunction)
        {
            return W.Error(TEXT("Failed to create GetNDCSpawnData and Read function nodes"));
        }

        UEdGraphPin* SpawnDataDiInput = FindNiagaraNdcPinByType(SpawnDataFunction, EGPD_Input, ReaderType);
        UEdGraphPin* ReadDiInput = FindNiagaraNdcPinByType(ReadFunction, EGPD_Input, ReaderType);
        if (!ConnectNiagaraNdcPins(GraphSchema, DataChannelInputPin, SpawnDataDiInput, TEXT("Data Channel -> GetNDCSpawnData"), WiringError) ||
            !ConnectNiagaraNdcPins(GraphSchema, DataChannelInputPin, ReadDiInput, TEXT("Data Channel -> Read"), WiringError))
        {
            return W.Error(WiringError);
        }

        UEdGraphPin* EmitterIdPin = AddNiagaraNdcMapParameterPin(
            GraphNodes.MapGetNode,
            FNiagaraTypeDefinition(FNiagaraEmitterID::StaticStruct()),
            FName(TEXT("Module.EmitterID")),
            EGPD_Output);
        UEdGraphPin* SpawnDataEmitterIdInput = FindGraphPinByName(
            SpawnDataFunction,
            EGPD_Input,
            FName(TEXT("Emitter ID")),
            false);
        if (!EmitterIdPin || !SetNiagaraNdcModuleInputBinding(Graph, EmitterIdPin->PinName, SYS_PARAM_ENGINE_EMITTER_ID.GetName()) ||
            !ConnectNiagaraNdcPins(GraphSchema, EmitterIdPin, SpawnDataEmitterIdInput, TEXT("Engine emitter ID -> GetNDCSpawnData"), WiringError))
        {
            return W.Error(
                WiringError.IsEmpty() ? TEXT("Failed to configure the GetNDCSpawnData emitter ID input") : WiringError);
        }

        if(!W.SDK(TEXT("NewObject_ExecIndex"),true))return W.Error(W.R.Error);
        auto* ExecIndexNode=NewObject<UNiagaraNodeOp>(Graph,NAME_None,RF_Transactional);W.Prefix->SetBoolField(TEXT("exec_index_returned_nonnull"),ExecIndexNode!=nullptr);
        if(!W.After(TEXT("NewObject_ExecIndex"))||!Live(ExecIndexNode)||ExecIndexNode->GetOuter()!=Graph||!W.Pin(ExecIndexNode))return W.Error(TEXT("Execution-index node creation failed"));
        ExecIndexNode->OpName=FName(TEXT("Util::ExecIndex"));
        if(!W.AddNode(Graph,ExecIndexNode)||!W.EditNode(ExecIndexNode,TEXT("ExecIndex_CreateGuid"),[&]{ExecIndexNode->CreateNewGuid();})||!W.EditNode(ExecIndexNode,TEXT("ExecIndex_PostPlaced"),[&]{ExecIndexNode->PostPlacedNewNode();})||!W.EditNode(ExecIndexNode,TEXT("ExecIndex_Allocate"),[&]{ExecIndexNode->AllocateDefaultPins();}))return W.Error(W.R.Error);
        UEdGraphPin* ExecIndexOutput = FindNiagaraNdcPinByType(
            ExecIndexNode,
            EGPD_Output,
            FNiagaraTypeDefinition::GetIntDef());
        UEdGraphPin* SpawnedExecIndexInput = FindGraphPinByName(
            SpawnDataFunction,
            EGPD_Input,
            FName(TEXT("Spawned Particle Exec Index")),
            false);
        if (!ConnectNiagaraNdcPins(
                GraphSchema,
                ExecIndexOutput,
                SpawnedExecIndexInput,
                TEXT("execution index -> GetNDCSpawnData"),
                WiringError))
        {
            return W.Error(WiringError);
        }

        UEdGraphPin* NdcIndexOutput = FindGraphPinByName(
            SpawnDataFunction,
            EGPD_Output,
            FName(TEXT("NDC Index")),
            false);
        UEdGraphPin* ReadIndexInput = FindGraphPinByName(ReadFunction, EGPD_Input, FName(TEXT("Index")), false);
        if (!ConnectNiagaraNdcPins(GraphSchema, NdcIndexOutput, ReadIndexInput, TEXT("NDC Index -> Read.Index"), WiringError))
        {
            return W.Error(WiringError);
        }

        UEdGraphPin* ReadSuccessOutput = FindGraphPinByName(
            ReadFunction,
            EGPD_Output,
            FName(TEXT("Success")),
            false);
        UEdGraphPin* ReadSuccessWrite = AddNiagaraNdcMapParameterPin(
            GraphNodes.MapSetNode,
            FNiagaraTypeDefinition::GetBoolDef(),
            FName(*ReadSuccessTarget),
            EGPD_Input);
        if (!ConnectNiagaraNdcPins(GraphSchema, ReadSuccessOutput, ReadSuccessWrite, TEXT("Read.Success -> particle parameter"), WiringError))
        {
            return W.Error(WiringError);
        }

        for (const FRequiredNdcVariable& Variable : RequiredVariables)
        {
            FNiagaraTypeDefinition GraphType = Variable.ResolvedVariable->GetType();
            if (!GraphType.IsEnum())
            {
                if (UScriptStruct* SwcStruct = FNiagaraTypeHelper::GetSWCStruct(GraphType.GetScriptStruct()))
                {
                    GraphType = FNiagaraTypeDefinition(SwcStruct);
                }
            }
            UEdGraphPin* ReadOutput = AddNiagaraNdcVariadicOutputPin(
                ReadFunction,
                GraphType,
                FName(*Variable.SourceName));
            UEdGraphPin* ParticleWrite = AddNiagaraNdcMapParameterPin(
                GraphNodes.MapSetNode,
                GraphType,
                FName(*Variable.TargetName),
                EGPD_Input);
            if (!ConnectNiagaraNdcPins(
                    GraphSchema,
                    ReadOutput,
                    ParticleWrite,
                    FString::Printf(TEXT("Read.%s -> %s"), *Variable.SourceName, *Variable.TargetName),
                    WiringError))
            {
                return W.Error(WiringError);
            }
        }
        if(!W.Step(TEXT("SpawnData_Synchronize"),true,[&]{SpawnDataFunction->MarkNodeRequiresSynchronization(TEXT("MCP: configure GetNDCSpawnData"),true);})||!W.Step(TEXT("Read_Synchronize"),true,[&]{ReadFunction->MarkNodeRequiresSynchronization(TEXT("MCP: configure NDC Read outputs"),true);}))return W.Error(W.R.Error);
    }

    if(!W.Step(TEXT("ReaderGraph_Notify"),true,[&]{Graph->NotifyGraphChanged();}))return W.Error(W.R.Error);

    auto DesiredGraph=ReaderGraphFingerprint(Graph);bool DesiredComplete=false;DesiredGraph->TryGetBoolField(TEXT("complete"),DesiredComplete);if(!DesiredComplete||!W.Admit(TEXT("reader_desired_snapshot")))return W.Error(W.R.Error);
    if(!W.Admit(TEXT("reader_install_admission")))return W.Error(W.R.Error);
    auto* SystemPackage=System->GetOutermost();const bool bPackageWasDirty=SystemPackage->IsDirty();W.R.Data->SetBoolField(TEXT("reader_package_was_dirty"),bPackageWasDirty);
    if(ScratchPadNameExists(System->ScratchPadScripts)||FindObject<UObject>(System,*ScratchPadName))return W.Error(TEXT("Scratch Pad collision appeared before install; no overwrite"));
    if(!W.Modify(System)||!W.SDK(TEXT("Duplicate_InstalledScript"),true))return W.Error(W.R.Error);
    if(ScratchPadNameExists(System->ScratchPadScripts)||FindObject<UObject>(System,*ScratchPadName))return W.Error(TEXT("Scratch Pad collision after Modify callback; no overwrite"));
    for(const auto& H:System->GetEmitterHandles()){if(auto* D=H.GetEmitterData())if((D->ScratchPads&&ScratchPadNameExists(D->ScratchPads->Scripts))||(D->ParentScratchPads&&ScratchPadNameExists(D->ParentScratchPads->Scripts)))return W.Error(TEXT("Emitter scratch collision after Modify callback; no overwrite"));}
    auto* NewScript=Cast<UNiagaraScript>(StaticDuplicateObject(WorkingScript,System,FName(*ScratchPadName),RF_Transactional));
    W.Prefix->SetBoolField(TEXT("installed_duplicate_returned_nonnull"),NewScript!=nullptr);W.Prefix->SetStringField(TEXT("requested_installed_script_name"),ScratchPadName);
    if(!W.After(TEXT("Duplicate_InstalledScript")))return W.Error(W.R.Error);
    if(!Live(NewScript)||NewScript->GetClass()!=UNiagaraScript::StaticClass()||NewScript->GetOuter()!=System||NewScript->GetFName()!=FName(*ScratchPadName)||NewScript->GetPackage()!=SystemPackage||!W.CaptureScript(NewScript))return W.Error(TEXT("Installed duplicate exact ownership failed; actual creation prefix retained"));
    auto* InstalledSource=Cast<UNiagaraScriptSource>(NewScript->GetLatestSource());if(!InstalledSource||!W.CaptureGraph(InstalledSource)||!W.Own(NewScript))return W.Error(TEXT("Installed duplicate source/graph unavailable"));
    if(!W.Step(TEXT("InstalledScript_Flags"),true,[&]{NewScript->ClearFlags(RF_Public|RF_Standalone|RF_Transient);NewScript->SetFlags(RF_Transactional);}))return W.Error(W.R.Error);
    if(!W.SDK(TEXT("System_AddScratchScript"),true))return W.Error(W.R.Error);W.Scratch.Add(NewScript);System->ScratchPadScripts.Add(NewScript);W.R.bChanged=true;
    if(!W.After(TEXT("System_AddScratchScript"))||!W.Dirty())return W.Error(W.R.Error);
    bool CompileAccepted=false;if(bRequestCompile&&!W.Request(CompileAccepted))return W.Error(W.R.Error);
    bool bSaved=false;
    if(bSave){bSaved=W.Save(false);if(!bSaved){auto E=W.Error(TEXT("Failed to save the Niagara system after creating the NDC Scratch Pad; owned output retained for inspection, no uncertain rollback/replay"));E->SetBoolField(TEXT("mutation_applied_in_memory"),true);E->SetStringField(TEXT("mutation_state"),TEXT("unknown_or_present"));E->SetBoolField(TEXT("rollback_removed_from_scratch_pad_array"),false);E->SetBoolField(TEXT("rollback_reparented_object"),false);E->SetBoolField(TEXT("rollback_succeeded"),false);E->SetBoolField(TEXT("package_dirty_state_restored"),false);E->SetBoolField(TEXT("legacy_automatic_save_failure_rollback_supported"),false);W.R.Data->SetStringField(TEXT("remaining_difference"),TEXT("Original failed-save cleanup/detach is not applied while async compiler/references/Undo ownership are unproven; residual target and actual save history retained."));return E;}}
    if(!W.Admit(TEXT("reader_final_graph")))return W.Error(W.R.Error);
    UNiagaraScriptSource* FinalSource = Cast<UNiagaraScriptSource>(NewScript->GetLatestSource());
    UNiagaraGraph* FinalGraph = FinalSource ? FinalSource->NodeGraph : nullptr;
    TArray<TSharedPtr<FJsonValue>> FunctionNodesJson;
    int32 LinkEndpointCount = 0;
    if (FinalGraph)
    {
        const TArray<TObjectPtr<UEdGraphNode>> FinalNodes=FinalGraph->Nodes;
        for (UEdGraphNode* Node : FinalNodes)
        {
            if(!W.Admit(TEXT("reader_function_diagnostics")))return W.Error(W.R.Error);
            if (Cast<UNiagaraNodeFunctionCall>(Node) || Cast<UNiagaraNodeOp>(Node))
            {
                FunctionNodesJson.Add(MakeShared<FJsonValueObject>(DetailedGraphNodeToJsonObject(Node,INDEX_NONE,true,true)));if(!W.Admit(TEXT("after_reader_function_diagnostics")))return W.Error(W.R.Error);
            }
            if (Node)
            {
                for (const UEdGraphPin* Pin : Node->Pins)
                {
                    LinkEndpointCount += Pin ? Pin->LinkedTo.Num() : 0;
                }
            }
        }
    }

    TSharedPtr<FJsonObject> FollowUp = MakeShared<FJsonObject>();
    FollowUp->SetStringField(TEXT("stack_target_usage"), NiagaraScriptUsageName(TargetUsage));
    FollowUp->SetStringField(TEXT("scratch_pad_script_path"), NewScript->GetPathName());
    FollowUp->SetStringField(TEXT("scratch_pad_name"), NewScript->GetName());
    FollowUp->SetStringField(TEXT("operation"), TEXT("add_scratch_pad_module_to_stack"));
    FollowUp->SetBoolField(TEXT("bind_data_interface_after_stack_insert"), Stage == ENiagaraNdcScratchPadStage::EmitterSpawnInit);
    FollowUp->SetStringField(TEXT("data_interface_input_name"), TEXT("DataChannel"));
    TSharedPtr<FJsonObject> DataInterfaceProperties = MakeShared<FJsonObject>();
    DataInterfaceProperties->SetStringField(TEXT("Channel"), DataChannelAsset->GetPathName());
    DataInterfaceProperties->SetBoolField(TEXT("bReadCurrentFrame"), bReadCurrentFrame);
    FollowUp->SetObjectField(TEXT("data_interface_properties"), DataInterfaceProperties);
    FollowUp->SetStringField(
        TEXT("canonical_order"),
        TEXT("emitter_spawn_init -> emitter_update_spawn -> particle_spawn_read"));

    TSharedPtr<FJsonObject> Result = BuildBaseResult();
    Result->SetBoolField(TEXT("dry_run"), false);
    Result->SetBoolField(TEXT("preflight_pass"), true);
    Result->SetBoolField(TEXT("mutation_performed"), true);
    Result->SetBoolField(TEXT("created"), true);
    Result->SetStringField(TEXT("scratch_pad_script_path"), NewScript->GetPathName());
    Result->SetStringField(TEXT("template_path"), DefaultModule->GetPathName());
    Result->SetNumberField(TEXT("graph_node_count_before"), 4);
    Result->SetNumberField(TEXT("graph_node_count_after"), FinalGraph ? FinalGraph->Nodes.Num() : 0);
    Result->SetNumberField(TEXT("graph_link_count_after"), LinkEndpointCount / 2);
    Result->SetArrayField(TEXT("created_function_nodes"), FunctionNodesJson);
    Result->SetBoolField(TEXT("compile_requested"), bRequestCompile);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetObjectField(TEXT("required_follow_up"), FollowUp);
    Result->SetObjectField(
        TEXT("interface"),
        BuildScratchPadInterfaceJson(
            NewScript,
            TEXT("system"),
            System->GetName(),
            INDEX_NONE,
            TEXT("system_scratch_pads"),
            System->ScratchPadScripts.Num() - 1,
            true,
            80));
    if(!W.Admit(TEXT("reader_interface_complete")))return W.Error(W.R.Error);
    auto ActualGraph=ReaderGraphFingerprint(FinalGraph);bool ActualComplete=false;ActualGraph->TryGetBoolField(TEXT("complete"),ActualComplete);W.R.Data->SetObjectField(TEXT("expected_reader_graph"),DesiredGraph);W.R.Data->SetObjectField(TEXT("actual_reader_graph"),ActualGraph);if(!ActualComplete||!W.Admit(TEXT("reader_typed_after"))||!Json(DesiredGraph).Equals(Json(ActualGraph),ESearchCase::CaseSensitive))return W.Error(TEXT("Installed/compiled/saved reader graph and metadata differ from the exact authored plan"));
    W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),true);
    Result->SetBoolField(TEXT("compile_completion_verified"),false);Result->SetBoolField(TEXT("compile_enqueue_accepted"),CompileAccepted);
    Result->SetStringField(TEXT("write_scope"), TEXT("niagara_ndc_reader_scratch_pad_module_create"));
    return Result;
}


// Root-owned GN fragment. Insert inside the existing anonymous namespace after the builders.
TSharedPtr<FJsonObject> HandleFunctionCall(const TSharedPtr<FJsonObject>& Params)
{
    FWork& W=*Current;W.Writing=true;
    if(!Params.IsValid())return W.Error(TEXT("Missing parameters"));
    // Historical strict gate is preserved; the missing success-output authoring is explicit.
    bool CaptureSuccess=false;
    if(Params->HasField(TEXT("capture_success")))
    {
        if(!Params->HasTypedField<EJson::Boolean>(TEXT("capture_success")))
        {W.Fail(TEXT("niagara_ndc_capture_success_boolean_required"),TEXT("capture_success must be a boolean"));return W.Error(W.R.Error);}
        Params->TryGetBoolField(TEXT("capture_success"),CaptureSuccess);
        if(CaptureSuccess){W.Fail(TEXT("niagara_ndc_capture_success_unimplemented"),TEXT("Historical capture_success authoring is not implemented; no asset load or writer dispatched."));return W.Error(W.R.Error);}
    }
    if(!W.Admit(TEXT("di_function_call_admission")))return W.Error(W.R.Error);
    FString SystemPath,ScratchPadName,FunctionName,DiInputName,DiTypeString;
    Params->TryGetStringField(TEXT("system_path"),SystemPath);Params->TryGetStringField(TEXT("scratch_pad_name"),ScratchPadName);
    Params->TryGetStringField(TEXT("function_name"),FunctionName);Params->TryGetStringField(TEXT("di_input_name"),DiInputName);Params->TryGetStringField(TEXT("di_type"),DiTypeString);
    if(SystemPath.IsEmpty()||ScratchPadName.IsEmpty()||FunctionName.IsEmpty()||DiInputName.IsEmpty()||DiTypeString.IsEmpty())return W.Error(TEXT("system_path, scratch_pad_name, function_name, di_input_name, and di_type are required"));
    bool AllowSource=false,SaveRequested=true,CompileRequested=true;
    Params->TryGetBoolField(TEXT("allow_source_edit"),AllowSource);Params->TryGetBoolField(TEXT("save"),SaveRequested);Params->TryGetBoolField(TEXT("request_compile"),CompileRequested);
    if(!AllowSource&&!IsTempGeneratedNiagaraPath(SystemPath))return W.Error(FString::Printf(TEXT("Refusing to edit Niagara system outside %s: %s"),NiagaraTempGenerationRoot,*SystemPath));
    UNiagaraSystem* System=LoadSystem(SystemPath);
    if(!System)return W.Error(W.R.Error.IsEmpty()?FString::Printf(TEXT("Failed to load Niagara system: %s"),*SystemPath):W.R.Error);
    if(!AllowSource&&!IsTempGeneratedNiagaraPath(System->GetPathName()))return W.Error(TEXT("Resolved Niagara system is outside the generated root; no writer dispatched."));
    UNiagaraScript* SelectedScript=nullptr;
    // Preserve original precedence: system scratch first, then the first emitter scratch match.
    for(const TObjectPtr<UNiagaraScript>& Candidate:System->ScratchPadScripts)
    {if(Candidate&&Candidate->GetName().Equals(ScratchPadName,ESearchCase::IgnoreCase)){SelectedScript=Candidate.Get();break;}}
    if(!SelectedScript)
    {
        for(FNiagaraEmitterHandle& EmitterHandle:System->GetEmitterHandles())
        {
            if(FVersionedNiagaraEmitterData* EmitterData=EmitterHandle.GetEmitterData())
            {if(EmitterData->ScratchPads)for(const TObjectPtr<UNiagaraScript>& Candidate:EmitterData->ScratchPads->Scripts)
                if(Candidate&&Candidate->GetName().Equals(ScratchPadName,ESearchCase::IgnoreCase)){SelectedScript=Candidate.Get();break;}}
            if(SelectedScript)break;
        }
    }
    if(!SelectedScript)return W.Error(FString::Printf(TEXT("No matching Scratch Pad script was found: %s"),*ScratchPadName));
    if(!W.CaptureScript(SelectedScript)||!W.SDK(TEXT("GetLatestSource_DI_Call")))return W.Error(W.R.Error);
    UNiagaraScriptSource* ScriptSource=Cast<UNiagaraScriptSource>(SelectedScript->GetLatestSource());
    if(!W.After(TEXT("GetLatestSource_DI_Call")))return W.Error(W.R.Error);
    UNiagaraGraph* Graph=ScriptSource?ScriptSource->NodeGraph.Get():nullptr;
    if(!Graph)return W.Error(TEXT("Scratch Pad script has no editable Niagara graph"));
    if(!W.Own(SelectedScript)||!W.CaptureGraph(ScriptSource)||!W.GraphForWrite(Graph))return W.Error(W.R.Error);
    FNiagaraTypeDefinition DiType;
    if(!TryParseNiagaraTypeDefinition(DiTypeString,DiType)||!DiType.IsDataInterface()||!DiType.GetClass())return W.Error(FString::Printf(TEXT("di_type must resolve to a Niagara data interface class: %s"),*DiTypeString));
    TArray<FNiagaraFunctionSignature> Signatures;
    if(!DICdo(DiType.GetClass(),Signatures))return W.Error(W.R.Error.IsEmpty()?TEXT("Failed to resolve data interface CDO"):W.R.Error);
    const FNiagaraFunctionSignature* BaseSignature=Signatures.FindByPredicate([&](const FNiagaraFunctionSignature& Candidate){return Candidate.Name.ToString().Equals(FunctionName,ESearchCase::IgnoreCase);});
    if(!BaseSignature)
    {TArray<FString> Names;for(const FNiagaraFunctionSignature& Candidate:Signatures){if(!W.Budget(1,Candidate.Name.ToString().Len()))return W.Error(W.R.Error);Names.Add(Candidate.Name.ToString());}return W.Error(FString::Printf(TEXT("Data interface function '%s' was not found. Available: %s"),*FunctionName,*FString::Join(Names,TEXT(", "))));}
    FNiagaraFunctionSignature Signature=*BaseSignature;
    struct FCallInputSpec{FString PinName,ParameterName,DefaultValue;FNiagaraTypeDefinition Type;bool HasType=false,Variadic=false;};
    TArray<FCallInputSpec> InputSpecs;
    const TArray<TSharedPtr<FJsonValue>>* InputsJson=nullptr;
    if(Params->TryGetArrayField(TEXT("inputs"),InputsJson)&&InputsJson)
    {
        if(InputsJson->Num()>4096||!W.Budget(InputsJson->Num()))return W.Error(TEXT("DI-call inputs exceed the bounded 4096-entry inventory."));
        for(const TSharedPtr<FJsonValue>& Value:*InputsJson)
        {
            const TSharedPtr<FJsonObject>* InputObject=nullptr;
            if(!Value.IsValid()||!Value->TryGetObject(InputObject)||!InputObject||!InputObject->IsValid())return W.Error(TEXT("Each inputs entry must be an object"));
            FCallInputSpec Spec;FString TypeString;
            (*InputObject)->TryGetStringField(TEXT("pin_name"),Spec.PinName);(*InputObject)->TryGetStringField(TEXT("parameter_name"),Spec.ParameterName);
            (*InputObject)->TryGetStringField(TEXT("default_value"),Spec.DefaultValue);(*InputObject)->TryGetStringField(TEXT("type"),TypeString);Spec.PinName.TrimStartAndEndInline();
            if(Spec.PinName.IsEmpty())return W.Error(TEXT("Each inputs entry requires pin_name"));
            if(!W.Budget(1,Spec.PinName.Len()+Spec.ParameterName.Len()+Spec.DefaultValue.Len()+TypeString.Len()))return W.Error(W.R.Error);
            if(!TypeString.IsEmpty()){if(!TryParseNiagaraTypeDefinition(TypeString,Spec.Type))return W.Error(FString::Printf(TEXT("Unsupported input type for '%s': %s"),*Spec.PinName,*TypeString));Spec.HasType=true;}
            const bool BasePin=Signature.Inputs.ContainsByPredicate([&](const FNiagaraVariable& Candidate){return Candidate.GetName().ToString().Equals(Spec.PinName,ESearchCase::IgnoreCase);});
            if(!BasePin){if(!Spec.HasType)return W.Error(FString::Printf(TEXT("Variadic input '%s' requires a type"),*Spec.PinName));Spec.Variadic=true;Signature.Inputs.Add(FNiagaraVariable(Spec.Type,FName(*Spec.PinName)));}
            InputSpecs.Add(Spec);
        }
    }
    // Retain the referenced reflected types across every subsequent callback guard.
    auto PinSignatureType=[&](const FNiagaraTypeDefinition& Type)->bool
    {if(UObject* EnumObject=Type.GetEnum())return W.Pin(EnumObject);if(UObject* StructObject=Type.GetStruct())return W.Pin(StructObject);if(UObject* ClassObject=Type.GetClass())return W.Pin(ClassObject);return Type.IsValid();};
    if(Signature.Inputs.Num()>8192||Signature.Outputs.Num()>8192||!W.Budget(Signature.Inputs.Num()+Signature.Outputs.Num()))return W.Error(TEXT("DI signature exceeds bounded pin inventory."));
    for(const FNiagaraVariable& InputVariable:Signature.Inputs)if(!PinSignatureType(InputVariable.GetType()))return W.Error(W.R.Error.IsEmpty()?TEXT("DI signature input type is invalid"):W.R.Error);
    for(const FNiagaraVariable& OutputVariable:Signature.Outputs)if(!PinSignatureType(OutputVariable.GetType()))return W.Error(W.R.Error.IsEmpty()?TEXT("DI signature output type is invalid"):W.R.Error);
    UNiagaraNodeOutput* OutputNode=nullptr;
    for(UEdGraphNode* Node:Graph->Nodes)if(UNiagaraNodeOutput* Candidate=Cast<UNiagaraNodeOutput>(Node)){OutputNode=Candidate;break;}
    UEdGraphPin* OutputMap=OutputNode?FindGraphPinByName(OutputNode,EGPD_Input,FName(TEXT("OutputMap")),true):nullptr;
    if(!OutputMap||OutputMap->LinkedTo.Num()!=1||!OutputMap->LinkedTo[0])return W.Error(TEXT("Scratch Pad output node has no single-linked OutputMap pin to splice into"));
    UEdGraphPin* ChainTail=OutputMap->LinkedTo[0];
    if(!FindExpectedPin(W,OutputMap)||!FindExpectedPin(W,ChainTail)||!W.PinForWrite(OutputMap)||!W.PinForWrite(ChainTail))return W.Error(W.R.Error);
    const int32 BeforeNodeCount=Graph->Nodes.Num(),OutputX=OutputNode->NodePosX,OutputY=OutputNode->NodePosY;
    W.Before=W.Projection();W.R.Data->SetStringField(TEXT("selected_scratch_script"),SelectedScript->GetPathName());W.R.Data->SetStringField(TEXT("selected_graph"),Graph->GetPathName());
    W.Prefix->SetNumberField(TEXT("di_call_graph_nodes_before"),BeforeNodeCount);W.Prefix->SetStringField(TEXT("di_call_tail_pin"),ChainTail->PinId.ToString());W.Prefix->SetStringField(TEXT("di_call_output_pin"),OutputMap->PinId.ToString());
    if(!W.R.Error.IsEmpty()||!W.Modify(System)||!W.Modify(SelectedScript)||!W.Modify(ScriptSource)||!W.Modify(Graph))return W.Error(W.R.Error);
    UNiagaraNodeFunctionCall* CallNode=CreateSignatureNode(Graph,Signature);
    if(!CallNode)return W.Error(W.R.Error.IsEmpty()?TEXT("Failed to create Niagara function call node"):W.R.Error);
    auto Offset=[](int32 Origin,int64 Delta){return static_cast<int32>(FMath::Clamp<int64>(static_cast<int64>(Origin)+Delta,MIN_int32,MAX_int32));};
    if(!W.EditNode(CallNode,TEXT("DI_Call_NodeLayout"),[&]{CallNode->NodePosX=Offset(OutputX,-320);CallNode->NodePosY=Offset(OutputY,240);})||!W.Modify(CallNode))return W.Error(W.R.Error);
    W.Prefix->SetStringField(TEXT("di_call_node_guid"),CallNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    UEdGraphPin* ExecIn=nullptr;UEdGraphPin* ExecOut=nullptr;
    for(UEdGraphPin* CallPin:CallNode->Pins)
    {if(!CallPin||IsNiagaraDynamicAddPin(CallPin))continue;if(UEdGraphSchema_Niagara::PinToTypeDefinition(CallPin)==FNiagaraTypeDefinition::GetParameterMapDef()){if(CallPin->Direction==EGPD_Input&&!ExecIn)ExecIn=CallPin;else if(CallPin->Direction==EGPD_Output&&!ExecOut)ExecOut=CallPin;}}
    if(!ExecIn||!ExecOut)
    {W.Prefix->SetBoolField(TEXT("di_call_no_exec_cleanup_attempted"),true);const bool Removed=W.RemoveNode(Graph,CallNode,true,false);W.Prefix->SetBoolField(TEXT("di_call_no_exec_cleanup_returned_removed"),Removed);return W.Error(TEXT("Function call node did not expose ParameterMap exec pins; the DI function may not be impure (bRequiresExecPin)"));}
    if(!W.BreakLinks(OutputMap,true)||!W.LinkPins(ChainTail,ExecIn)||!W.LinkPins(ExecOut,OutputMap))return W.Error(W.R.Error);
    // Keep the original partial-failure order: stock MapGet resolution follows the splice.
    UClass* MapGetClass=ExactStockClass(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"),UNiagaraNodeWithDynamicPins::StaticClass());
    if(!MapGetClass)return W.Error(W.R.Error.IsEmpty()?TEXT("NiagaraNodeParameterMapGet class was not found"):W.R.Error);
    int32 CreatedGetNodeCount=0;
    struct FGetProof{TWeakObjectPtr<UNiagaraNodeWithDynamicPins> Node;UEdGraphPin* Source=nullptr;UEdGraphPin* Output=nullptr;UEdGraphPin* Consumer=nullptr;FNiagaraTypeDefinition Type;FName Name;};
    TArray<FGetProof> GetProofs;
    auto CreateMapGetLink=[&](const FString& ParameterName,const FNiagaraTypeDefinition& ParameterType,UEdGraphPin* ConsumerPin)->bool
    {
        if(!W.Admit(TEXT("di_call_map_get"))||!W.PinForWrite(ConsumerPin))return false;
        UNiagaraNode* NewBase=CreateGraphNode(Graph,MapGetClass);if(!NewBase)return false;
        auto* GetNode=Cast<UNiagaraNodeWithDynamicPins>(NewBase);if(!GetNode)return W.Fail(TEXT("niagara_ndc_mapget_class_changed"),TEXT("Exact stock ParameterMapGet is not a dynamic-pin node."));
        const int32 GetIndex=CreatedGetNodeCount++;
        W.Prefix->SetNumberField(TEXT("di_call_get_nodes_acknowledged"),CreatedGetNodeCount);
        if(!W.EditNode(GetNode,TEXT("DI_Get_NodeLayout"),[&]{GetNode->NodePosX=Offset(CallNode->NodePosX,-480);GetNode->NodePosY=Offset(CallNode->NodePosY,static_cast<int64>(GetIndex)*90);})||!W.Modify(GetNode))return false;
        UEdGraphPin* SourcePin=FindNiagaraNdcPinByType(GetNode,EGPD_Input,FNiagaraTypeDefinition::GetParameterMapDef());
        if(!SourcePin)return W.Fail(TEXT("niagara_ndc_mapget_source_missing"),TEXT("ParameterMapGet exposed no map source pin."));
        if(!W.LinkPins(ChainTail,SourcePin))return false;
        // Original DI-call uses a typed output directly; it does not author new metadata/default pairs.
        UEdGraphPin* GetOutput=CreateTypedPin(GetNode,EGPD_Output,ParameterType,FName(*ParameterName));
        if(!GetOutput)return false;
        if(!W.LinkPins(GetOutput,ConsumerPin))return false;
        FGetProof Proof;Proof.Node=GetNode;Proof.Source=SourcePin;Proof.Output=GetOutput;Proof.Consumer=ConsumerPin;Proof.Type=ParameterType;Proof.Name=FName(*ParameterName);GetProofs.Add(Proof);
        return true;
    };
    if(Signature.Inputs.IsEmpty())return W.Error(TEXT("DI function signature has no inputs"));
    const FName DiPinName=Signature.Inputs[0].GetName();UEdGraphPin* DiPin=FindGraphPinByName(CallNode,EGPD_Input,DiPinName,false);
    if(!DiPin)return W.Error(FString::Printf(TEXT("Function call node has no DI pin named '%s'"),*DiPinName.ToString()));
    const FString DiParameterName=DiInputName.StartsWith(TEXT("Module."))?DiInputName:FString::Printf(TEXT("Module.%s"),*DiInputName);
    if(!CreateMapGetLink(DiParameterName,DiType,DiPin))return W.Error(W.R.Error.IsEmpty()?TEXT("Failed to link member DI ParameterMapGet"):W.R.Error);
    TArray<TSharedPtr<FJsonValue>> AppliedInputs;
    TMap<UEdGraphPin*,FString> ExpectedLiterals;
    for(const FCallInputSpec& Spec:InputSpecs)
    {
        UEdGraphPin* InputPin=FindGraphPinByName(CallNode,EGPD_Input,FName(*Spec.PinName),false);
        if(!InputPin)return W.Error(FString::Printf(TEXT("Function call node has no input pin named '%s'"),*Spec.PinName));
        auto InputRow=MakeShared<FJsonObject>();InputRow->SetStringField(TEXT("pin_name"),Spec.PinName);InputRow->SetBoolField(TEXT("variadic"),Spec.Variadic);
        W.Partial.Add(MakeShared<FJsonValueObject>(InputRow));
        if(!Spec.ParameterName.IsEmpty())
        {const FNiagaraTypeDefinition LinkType=Spec.HasType?Spec.Type:UEdGraphSchema_Niagara::PinToTypeDefinition(InputPin);if(!CreateMapGetLink(Spec.ParameterName,LinkType,InputPin))return W.Error(W.R.Error.IsEmpty()?TEXT("Failed to link requested ParameterMapGet input"):W.R.Error);InputRow->SetStringField(TEXT("linked_parameter"),Spec.ParameterName);}
        else if(!Spec.DefaultValue.IsEmpty())
        {if(!ModifyPin(InputPin)||!W.Step(TEXT("DI_Call_SetDefault"),true,[&]{InputPin->DefaultValue=Spec.DefaultValue;ExpectedLiterals.Add(InputPin,Spec.DefaultValue);InputRow->SetStringField(TEXT("default_value"),Spec.DefaultValue);W.R.bChanged=true;}))return W.Error(W.R.Error);if(!InputPin->DefaultValue.Equals(Spec.DefaultValue,ESearchCase::CaseSensitive))return W.Error(TEXT("Requested DI-call literal default did not read back exactly."));}
        AppliedInputs.Add(MakeShared<FJsonValueObject>(InputRow));
    }
    if(!W.Step(TEXT("DI_Call_Synchronize"),true,[&]{CallNode->MarkNodeRequiresSynchronization(TEXT("Cubeless: add DI function call"),true);})||!W.Dirty())return W.Error(W.R.Error);
    auto VerifyAuthoredCall=[&]()->bool
    {
        if(!W.Admit(TEXT("di_call_structured_readback")))return false;
        if(!(CallNode->Signature==Signature)||Graph->Nodes.Num()!=BeforeNodeCount+1+CreatedGetNodeCount||OutputMap->LinkedTo!=TArray<UEdGraphPin*>{ExecOut}||ExecIn->LinkedTo!=TArray<UEdGraphPin*>{ChainTail}||!ChainTail->LinkedTo.Contains(ExecIn))return W.Fail(TEXT("niagara_ndc_call_readback_failed"),TEXT("DI-call signature/node count/exec splice differs from the admitted request."));
        for(const FGetProof& Proof:GetProofs)
        {
            if(!Proof.Node.IsValid()||!FindExpectedPin(W,Proof.Source)||!FindExpectedPin(W,Proof.Output)||!FindExpectedPin(W,Proof.Consumer))return W.Fail(TEXT("niagara_ndc_get_readback_expired"),TEXT("Captured DI-call getter or consumer pin expired; no stale pointer dereference."));
            if(Proof.Output->GetOwningNodeUnchecked()!=Proof.Node.Get()||Proof.Source->GetOwningNodeUnchecked()!=Proof.Node.Get()||Proof.Source->LinkedTo!=TArray<UEdGraphPin*>{ChainTail}||Proof.Output->LinkedTo!=TArray<UEdGraphPin*>{Proof.Consumer}||!Proof.Consumer->LinkedTo.Contains(Proof.Output)||Proof.Output->PinName!=Proof.Name||UEdGraphSchema_Niagara::PinToTypeDefinition(Proof.Output)!=Proof.Type)return W.Fail(TEXT("niagara_ndc_get_readback_failed"),TEXT("Authored ParameterMapGet name/type/reciprocal links differ from the requested input."));
        }
        for(const auto& Literal:ExpectedLiterals)
        {if(!FindExpectedPin(W,Literal.Key))return W.Fail(TEXT("niagara_ndc_literal_readback_expired"),TEXT("Requested literal pin expired after SDK callbacks; no stale pointer dereference."));if(Literal.Key->GetOwningNodeUnchecked()!=CallNode||!Literal.Key->DefaultValue.Equals(Literal.Value,ESearchCase::CaseSensitive))return W.Fail(TEXT("niagara_ndc_literal_readback_failed"),TEXT("Final requested DI-call literal differs after compile/save callbacks; actual saved history retained."));}
        return true;
    };
    if(!VerifyAuthoredCall())return W.Error(W.R.Error);
    bool CompileAccepted=false;if(CompileRequested&&!W.Request(CompileAccepted))return W.Error(W.R.Error);
    if(SaveRequested&&!W.Save(false))return W.Error(W.R.Error.IsEmpty()?TEXT("Failed to save Niagara system after DI function call insert"):W.R.Error);
    if(!VerifyAuthoredCall())return W.Error(W.R.Error);
    W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),true);
    W.R.Data->SetBoolField(TEXT("di_call_structured_readback_verified"),true);W.R.Data->SetBoolField(TEXT("di_call_compile_request_accepted"),CompileAccepted);
    W.R.Data->SetBoolField(TEXT("di_call_compile_completion_verified"),false);W.R.Data->SetBoolField(TEXT("historical_capture_success_implemented"),false);
    W.R.Data->SetBoolField(TEXT("unselected_metadata_and_default_values_complete_verified"),false);
    auto Result=MakeShared<FJsonObject>();Result->SetBoolField(TEXT("success"),true);Result->SetStringField(TEXT("system_path"),System->GetPathName());Result->SetStringField(TEXT("scratch_pad_script_name"),SelectedScript->GetName());
    Result->SetStringField(TEXT("function_name"),Signature.Name.ToString());Result->SetStringField(TEXT("call_node_guid"),CallNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));Result->SetStringField(TEXT("di_parameter_name"),DiParameterName);Result->SetStringField(TEXT("di_class"),DiType.GetClass()->GetName());
    Result->SetNumberField(TEXT("signature_input_count"),Signature.Inputs.Num());Result->SetNumberField(TEXT("required_input_count"),Signature.RequiredInputs);Result->SetArrayField(TEXT("inputs"),AppliedInputs);Result->SetBoolField(TEXT("saved"),W.R.bSaved);Result->SetStringField(TEXT("write_scope"),TEXT("scratch_pad_di_function_call"));return Result;
}


// Fragment owned by fk; integrate inside the existing GN anonymous namespace, after shared helpers.
// Source-only: Part5:5487-6760; no new dry_run. Required compile/wait/save and semantic recovery remain incomplete.
// Integrator include: Misc/SecureHash.h (public Core FMD5 fingerprint).
struct FGNVariadicCompileSnapshot
{
    FString OwnerKind,OwnerName,ScriptPath,Usage;
    FNiagaraVMExecutableDataId CachedId,ComputedId;
    ENiagaraScriptCompileStatus Status=ENiagaraScriptCompileStatus::NCS_Unknown;
};
bool GNVariadicCompileError(ENiagaraScriptCompileStatus S){return S==ENiagaraScriptCompileStatus::NCS_Error;}
bool GNVariadicCompileWarning(ENiagaraScriptCompileStatus S){return S==ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings||S==ENiagaraScriptCompileStatus::NCS_ComputeUpToDateWithWarnings;}
FString GNVariadicCompileFingerprint(const FNiagaraVMExecutableDataId& Id)
{if(!Id.IsValid())return {};FString Key;Id.AppendKeyString(Key);return FMD5::HashAnsiString(*Key);}
TSharedPtr<FJsonObject> GNVariadicCompileSnapshotJson(const FGNVariadicCompileSnapshot& S,int32 Index)
{
 auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("script_index"),Index);O->SetStringField(TEXT("owner_kind"),S.OwnerKind);O->SetStringField(TEXT("owner_name"),S.OwnerName);O->SetStringField(TEXT("script_path"),S.ScriptPath);O->SetStringField(TEXT("usage"),S.Usage);auto* E=StaticEnum<ENiagaraScriptCompileStatus>();O->SetStringField(TEXT("compile_status"),E?E->GetNameStringByValue(static_cast<int64>(S.Status)):FString::FromInt(static_cast<int32>(S.Status)));O->SetBoolField(TEXT("has_error"),GNVariadicCompileError(S.Status));O->SetBoolField(TEXT("has_warning"),GNVariadicCompileWarning(S.Status));O->SetBoolField(TEXT("cached_id_valid"),S.CachedId.IsValid());O->SetBoolField(TEXT("computed_id_valid"),S.ComputedId.IsValid());O->SetStringField(TEXT("cached_id_fingerprint"),GNVariadicCompileFingerprint(S.CachedId));O->SetStringField(TEXT("computed_id_fingerprint"),GNVariadicCompileFingerprint(S.ComputedId));O->SetBoolField(TEXT("source_synchronized"),S.CachedId.IsValid()&&S.ComputedId.IsValid()&&S.CachedId==S.ComputedId);return O;
}
TArray<FGNVariadicCompileSnapshot> GNVariadicCaptureCompile(UNiagaraSystem* System)
{
 FWork& W=*Current;TArray<FGNVariadicCompileSnapshot> Out;if(!W.Admit(TEXT("variadic_compile_snapshot_admission")))return Out;
 struct FTarget{UNiagaraScript* Script;FString Kind,Name;};TArray<FTarget> Targets;TSet<UNiagaraScript*> Seen;
 auto Add=[&](UNiagaraScript* S,const FString& K,const FString& N){if(!S||Seen.Contains(S))return true;if(!W.CaptureScript(S)||!W.Budget(1,N.Len()+K.Len()))return false;Seen.Add(S);Targets.Add({S,K,N});return true;};
 if(!Add(System->GetSystemSpawnScript(),TEXT("system"),TEXT("system_spawn"))||!Add(System->GetSystemUpdateScript(),TEXT("system"),TEXT("system_update")))return Out;
 for(int32 I=0;I<W.Emitters.Num();++I){const auto& E=W.Emitters[I];if(!E.Data)continue;TArray<UNiagaraScript*> Scripts;E.Data->GetScripts(Scripts,false,false);if(!W.Budget(Scripts.Num()))return Out;const FString Name=System->GetEmitterHandles()[I].GetName().ToString();for(UNiagaraScript* S:Scripts)if(!Add(S,FString::Printf(TEXT("emitter:%d"),I),Name))return Out;}
 for(const auto& T:Targets){if(!W.Admit(TEXT("variadic_compile_snapshot_target")))return Out;FGNVariadicCompileSnapshot S;S.OwnerKind=T.Kind;S.OwnerName=T.Name;S.ScriptPath=T.Script->GetPathName();S.Usage=NiagaraScriptUsageName(T.Script->GetUsage());S.CachedId=T.Script->GetVMExecutableDataCompilationId();S.Status=T.Script->GetLastCompileStatus();if(!W.Step(TEXT("Variadic_ComputeVMCompilationId"),false,[&]{T.Script->ComputeVMCompilationId(S.ComputedId,FGuid());}))return Out;if(!W.Budget(1,S.ScriptPath.Len()+S.OwnerName.Len()))return Out;Out.Add(MoveTemp(S));}
 return Out;
}
TSharedPtr<FJsonObject> GNVariadicCompileState(UNiagaraSystem* System,bool Wait,double Timeout,double Poll)
{
 FWork& W=*Current;auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("wait_for_completion"),Wait);O->SetNumberField(TEXT("timeout_seconds"),Timeout);O->SetNumberField(TEXT("poll_interval_seconds"),Poll);O->SetBoolField(TEXT("wait_timed_out"),false);O->SetNumberField(TEXT("wait_iterations"),0);O->SetNumberField(TEXT("wait_elapsed_seconds"),0.0);bool Outstanding=false,Active=false;
 if(!W.Step(TEXT("Variadic_CompileState"),false,[&]{Outstanding=System->HasOutstandingCompilationRequests(true);Active=System->HasActiveCompilations();}))return O;
 O->SetBoolField(TEXT("outstanding_compilation_requests_after"),Outstanding);O->SetBoolField(TEXT("active_compilations_after"),Active);O->SetBoolField(TEXT("blocking_wait_executed"),false);O->SetBoolField(TEXT("wait_completion_verified"),Wait&&!Outstanding&&!Active);return O;
}

TSharedPtr<FJsonObject> HandleVariadic(const TSharedPtr<FJsonObject>& Params)
{
    FWork& W=*Current;
#if WITH_EDITORONLY_DATA
    if (!Params.IsValid())
    {
        return W.Error(TEXT("Missing parameters"));
    }

    FString SystemPath;
    FString ScratchPadName;
    FString ScratchPadScriptPath;
    FString FunctionNodeGuidString;
    FString ExpectedFunctionName;
    FString ExpectedDiClass;
    FString PinName;
    FString ModuleInputName;
    FString InputTypeString = TEXT("int");
    FString ExpectedReferencingModuleGuidString;
    FString ExpectedInputMapNodeGuidString;
    FString ExpectedOutputNodeGuidString;
    Params->TryGetStringField(TEXT("system_path"), SystemPath);
    Params->TryGetStringField(TEXT("scratch_pad_name"), ScratchPadName);
    Params->TryGetStringField(TEXT("scratch_pad_script_path"), ScratchPadScriptPath);
    Params->TryGetStringField(TEXT("function_node_guid"), FunctionNodeGuidString);
    Params->TryGetStringField(TEXT("expected_function_name"), ExpectedFunctionName);
    Params->TryGetStringField(TEXT("expected_di_class"), ExpectedDiClass);
    Params->TryGetStringField(TEXT("pin_name"), PinName);
    Params->TryGetStringField(TEXT("module_input_name"), ModuleInputName);
    Params->TryGetStringField(TEXT("input_type"), InputTypeString);
    Params->TryGetStringField(TEXT("expected_referencing_module_node_guid"), ExpectedReferencingModuleGuidString);
    Params->TryGetStringField(TEXT("expected_input_map_node_guid"), ExpectedInputMapNodeGuidString);
    Params->TryGetStringField(TEXT("expected_output_node_guid"), ExpectedOutputNodeGuidString);
    ScratchPadName.TrimStartAndEndInline();
    ScratchPadScriptPath.TrimStartAndEndInline();
    PinName.TrimStartAndEndInline();
    ModuleInputName.TrimStartAndEndInline();
    if (SystemPath.IsEmpty() || (ScratchPadName.IsEmpty() && ScratchPadScriptPath.IsEmpty()) ||
        FunctionNodeGuidString.IsEmpty() || ExpectedFunctionName.IsEmpty() || ExpectedDiClass.IsEmpty() ||
        PinName.IsEmpty() || ModuleInputName.IsEmpty() || ExpectedReferencingModuleGuidString.IsEmpty())
    {
        return W.Error(
            TEXT("system_path, a Scratch Pad selector, function_node_guid, expected_function_name, expected_di_class, pin_name, module_input_name, and expected_referencing_module_node_guid are required"));
    }
    if (PinName.Contains(TEXT(".")))
    {
        return W.Error(TEXT("pin_name must be an unqualified variadic signature input name"));
    }
    if (!ModuleInputName.Contains(TEXT(".")))
    {
        ModuleInputName = FString::Printf(TEXT("Module.%s"), *ModuleInputName);
    }
    else if (!ModuleInputName.StartsWith(TEXT("Module."), ESearchCase::IgnoreCase))
    {
        return W.Error(TEXT("module_input_name must be unqualified or use the Module.* namespace"));
    }

    FGuid FunctionNodeGuid;
    FGuid ExpectedReferencingModuleGuid;
    FGuid ExpectedInputMapNodeGuid;
    FGuid ExpectedOutputNodeGuid;
    if (!FGuid::Parse(FunctionNodeGuidString, FunctionNodeGuid) || !FunctionNodeGuid.IsValid())
    {
        return W.Error(TEXT("function_node_guid must be a valid non-zero GUID"));
    }
    if (!FGuid::Parse(ExpectedReferencingModuleGuidString, ExpectedReferencingModuleGuid) || !ExpectedReferencingModuleGuid.IsValid())
    {
        return W.Error(TEXT("expected_referencing_module_node_guid must be a valid non-zero GUID"));
    }
    if ((!ExpectedInputMapNodeGuidString.IsEmpty() &&
         (!FGuid::Parse(ExpectedInputMapNodeGuidString, ExpectedInputMapNodeGuid) || !ExpectedInputMapNodeGuid.IsValid())) ||
        (!ExpectedOutputNodeGuidString.IsEmpty() &&
         (!FGuid::Parse(ExpectedOutputNodeGuidString, ExpectedOutputNodeGuid) || !ExpectedOutputNodeGuid.IsValid())))
    {
        return W.Error(
            TEXT("expected_input_map_node_guid and expected_output_node_guid must be valid non-zero GUIDs when provided"));
    }

    FNiagaraTypeDefinition InputType;
    if (!TryParseNiagaraTypeDefinition(InputTypeString, InputType) || InputType != FNiagaraTypeDefinition::GetIntDef())
    {
        return W.Error(TEXT("This narrow command currently permits only an int variadic input"));
    }
    const TSharedPtr<FJsonValue> DefaultValue = Params->TryGetField(TEXT("default_value"));
    if (!DefaultValue.IsValid())
    {
        return W.Error(TEXT("default_value is required"));
    }
    FNiagaraVariable DefaultVariable(InputType, FName(*ModuleInputName));
    FString ConversionError;
    if (!SetNiagaraVariableDataFromJson(DefaultVariable, DefaultValue, ConversionError))
    {
        return W.Error(ConversionError);
    }
    FString DefaultValueString;
    const UEdGraphSchema_Niagara* NiagaraSchema = nullptr;
    bool bDefaultEncoded=false;
    if(!W.Step(TEXT("Variadic_EncodeDefault"),false,[&]{NiagaraSchema=GetDefault<UEdGraphSchema_Niagara>();bDefaultEncoded=NiagaraSchema&&NiagaraSchema->TryGetPinDefaultValueFromNiagaraVariable(DefaultVariable,DefaultValueString);}))return W.Error(W.R.Error);
    if (!bDefaultEncoded)
    {
        return W.Error(TEXT("Failed to encode the int default value as a Niagara pin default"));
    }

    bool bAllowSourceEdit = false;
    bool bSave = true;
    bool bRequestCompile = true;
    Params->TryGetBoolField(TEXT("allow_source_edit"), bAllowSourceEdit);
    Params->TryGetBoolField(TEXT("save"), bSave);
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);
    bool bWaitForCompletion = true;
    double TimeoutSeconds = 60.0;
    double PollIntervalSeconds = 0.1;
    bWaitForCompletion=false;TimeoutSeconds=10.0;PollIntervalSeconds=0.1;
    Params->TryGetBoolField(TEXT("wait_for_completion"),bWaitForCompletion);Params->TryGetNumberField(TEXT("timeout_seconds"),TimeoutSeconds);Params->TryGetNumberField(TEXT("poll_interval_seconds"),PollIntervalSeconds);
    if(!FMath::IsFinite(TimeoutSeconds)||!FMath::IsFinite(PollIntervalSeconds))return W.Error(TEXT("Compile wait values must be finite"));
    TimeoutSeconds=FMath::Clamp(TimeoutSeconds,0.0,60.0);PollIntervalSeconds=FMath::Clamp(PollIntervalSeconds,0.02,1.0);
    W.R.bSaveRequested=bSave;W.R.bCompileRequested=bRequestCompile;
    if (!bAllowSourceEdit && !IsTempGeneratedNiagaraPath(SystemPath))
    {
        return W.Error(
            FString::Printf(TEXT("Refusing to edit Niagara system outside %s: %s"), NiagaraTempGenerationRoot, *SystemPath));
    }
    if (GEditor && GEditor->GetPIEWorldContext() != nullptr)
    {
        return W.Error(TEXT("Refusing to mutate a Niagara Scratch Pad while a play session is active"));
    }
    if (bSave && (!bRequestCompile || !bWaitForCompletion))
    {
        return W.Error(
            TEXT("save=true requires request_compile=true and wait_for_completion=true for this atomic command"));
    }

    struct FExpectedInput
    {
        FString PinName;
        FString TypeName;
        FGuid LinkedNodeGuid;
        FString LinkedPinName;
    };
    TArray<FExpectedInput> ExpectedInputs;
    const TArray<TSharedPtr<FJsonValue>>* ExpectedInputsJson = nullptr;
    if (!Params->TryGetArrayField(TEXT("expected_existing_inputs"), ExpectedInputsJson) || !ExpectedInputsJson || ExpectedInputsJson->IsEmpty())
    {
        return W.Error(TEXT("expected_existing_inputs must be a non-empty array"));
    }
    if(ExpectedInputsJson->Num()>16384||!W.Budget(ExpectedInputsJson->Num()))return W.Error(TEXT("expected_existing_inputs exceeds the bounded pin inventory"));
    for (const TSharedPtr<FJsonValue>& Value : *ExpectedInputsJson)
    {
        const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
        FExpectedInput Expected;
        FString LinkedGuidString;
        if (!Object.IsValid() || !Object->TryGetStringField(TEXT("pin_name"), Expected.PinName) ||
            !Object->TryGetStringField(TEXT("type"), Expected.TypeName) ||
            !Object->TryGetStringField(TEXT("linked_node_guid"), LinkedGuidString) ||
            !Object->TryGetStringField(TEXT("linked_pin_name"), Expected.LinkedPinName) ||
            !FGuid::Parse(LinkedGuidString, Expected.LinkedNodeGuid) || !Expected.LinkedNodeGuid.IsValid())
        {
            return W.Error(
                TEXT("Each expected_existing_inputs entry requires pin_name, type, linked_node_guid, and linked_pin_name"));
        }
        ExpectedInputs.Add(Expected);
    }

    UNiagaraSystem* System = LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }
    W.Writing=true;if(!W.Admit(TEXT("variadic_writer_context")))return W.Error(W.R.Error);
    // Bind all current source->graph identities before compile-id SDK callbacks.
    for(const auto& Binding:W.ScriptBindings){auto* Source=Cast<UNiagaraScriptSource>(Binding.Source.Get());if(Source&&!W.CaptureGraph(Source))return W.Error(W.R.Error);}
    for(const auto& Emitter:W.Emitters){auto* Source=Cast<UNiagaraScriptSource>(Emitter.Source.Get());if(Source&&!W.CaptureGraph(Source))return W.Error(W.R.Error);}
    UPackage* SystemPackage = System->GetOutermost();
    const FString SystemPackageName = SystemPackage ? SystemPackage->GetName() : FString();
    const bool bPackageWasDirty = SystemPackage && SystemPackage->IsDirty();
    if (bPackageWasDirty)
    {
        return W.Error(
            TEXT("Niagara package must be clean; this command does not support a dirty-package override"));
    }
    TSharedPtr<FJsonObject> PreflightCompileState = GNVariadicCompileState(System, false, TimeoutSeconds, PollIntervalSeconds);
    if(!ContinueRead())return W.Error(W.R.Error);
    const bool bCompileWasBusy =
        PreflightCompileState->GetBoolField(TEXT("outstanding_compilation_requests_after")) ||
        PreflightCompileState->GetBoolField(TEXT("active_compilations_after"));
    if (bCompileWasBusy)
    {
        TSharedPtr<FJsonObject> Error = W.Error(
            TEXT("Niagara compiler must be idle; this command does not support a compile-busy override"));
        Error->SetObjectField(TEXT("preflight_compile_state"), PreflightCompileState);
        return Error;
    }
    const TArray<FGNVariadicCompileSnapshot> PreMutationCompileSnapshots = GNVariadicCaptureCompile(System);
    if(!ContinueRead())return W.Error(W.R.Error);
    int32 PreflightKnownScriptCount = 0;
    int32 PreflightCompileErrorCount = 0;
    int32 PreflightCompileWarningCount = 0;
    int32 PreflightCompileNonReadyCount = 0;
    int32 PreflightCompileUnsynchronizedCount = 0;
    TArray<TSharedPtr<FJsonValue>> PreflightCompileSnapshotsJson;
    for (const FGNVariadicCompileSnapshot& Snapshot : PreMutationCompileSnapshots)
    {
        if (Snapshot.Status == ENiagaraScriptCompileStatus::NCS_Unknown)
        {
            continue;
        }
        ++PreflightKnownScriptCount;
        PreflightCompileErrorCount += GNVariadicCompileError(Snapshot.Status) ? 1 : 0;
        PreflightCompileWarningCount += GNVariadicCompileWarning(Snapshot.Status) ? 1 : 0;
        PreflightCompileNonReadyCount += Snapshot.Status == ENiagaraScriptCompileStatus::NCS_UpToDate ? 0 : 1;
        PreflightCompileUnsynchronizedCount +=
            Snapshot.CachedId.IsValid() && Snapshot.ComputedId.IsValid() && Snapshot.CachedId == Snapshot.ComputedId
                ? 0
                : 1;
        PreflightCompileSnapshotsJson.Add(MakeShared<FJsonValueObject>(
            GNVariadicCompileSnapshotJson(Snapshot, PreflightCompileSnapshotsJson.Num())));
    }
    const bool bPreflightCompileBaselineReady =
        !PreMutationCompileSnapshots.IsEmpty() && PreflightKnownScriptCount > 0 &&
        PreflightCompileErrorCount == 0 && PreflightCompileWarningCount == 0 &&
        PreflightCompileNonReadyCount == 0 && PreflightCompileUnsynchronizedCount == 0;
    if (!bPreflightCompileBaselineReady)
    {
        TSharedPtr<FJsonObject> Error = W.Error(
            TEXT("Niagara compile baseline must be non-empty, warning-free, up-to-date, and cached/computed synchronized before mutation"));
        Error->SetStringField(TEXT("error_code"), TEXT("niagara_di_variadic_input_compile_baseline_not_ready"));
        Error->SetNumberField(TEXT("known_script_count"), PreflightKnownScriptCount);
        Error->SetNumberField(TEXT("compile_error_count"), PreflightCompileErrorCount);
        Error->SetNumberField(TEXT("compile_warning_count"), PreflightCompileWarningCount);
        Error->SetNumberField(TEXT("compile_non_ready_count"), PreflightCompileNonReadyCount);
        Error->SetNumberField(TEXT("compile_unsynchronized_count"), PreflightCompileUnsynchronizedCount);
        Error->SetArrayField(TEXT("compile_snapshots"), PreflightCompileSnapshotsJson);
        Error->SetBoolField(TEXT("mutation_applied_in_memory"), false);
        Error->SetBoolField(TEXT("saved"), false);
        return Error;
    }

    UNiagaraScript* ScratchPadScript = nullptr;
    int32 ScratchPadMatchCount = 0;
    const FString NormalizedScratchPadPath = NormalizeNiagaraObjectPathForLoad(ScratchPadScriptPath);
    for (const TObjectPtr<UNiagaraScript>& CandidatePtr : System->ScratchPadScripts)
    {
        UNiagaraScript* Candidate = CandidatePtr.Get();
        if (!Candidate)
        {
            continue;
        }
        const bool bNameMatches = ScratchPadName.IsEmpty() || Candidate->GetName().Equals(ScratchPadName, ESearchCase::CaseSensitive);
        const bool bPathMatches = ScratchPadScriptPath.IsEmpty() || Candidate->GetPathName().Equals(NormalizedScratchPadPath, ESearchCase::CaseSensitive);
        if (bNameMatches && bPathMatches)
        {
            ScratchPadScript = Candidate;
            ++ScratchPadMatchCount;
        }
    }
    if (ScratchPadMatchCount != 1 || !ScratchPadScript)
    {
        return W.Error(
            FString::Printf(TEXT("Scratch Pad selector must resolve to exactly one system-owned script; matched %d"), ScratchPadMatchCount));
    }
    if (ScratchPadScript->GetUsage() != ENiagaraScriptUsage::Module)
    {
        return W.Error(TEXT("Target Scratch Pad must be a Module script"));
    }
    UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(ScratchPadScript->GetLatestSource());
    UNiagaraGraph* Graph = ScriptSource ? ScriptSource->NodeGraph : nullptr;
    if (!Graph)
    {
        return W.Error(TEXT("Target Scratch Pad has no editable Niagara graph"));
    }

    if(!W.CaptureGraph(ScriptSource)||!W.Own(ScratchPadScript)||!W.GraphForWrite(Graph))return W.Error(W.R.Error);
    UNiagaraNodeFunctionCall* CallNode = nullptr;
    int32 CallNodeMatchCount = 0;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (UNiagaraNodeFunctionCall* Candidate = Cast<UNiagaraNodeFunctionCall>(Node))
        {
            if (Candidate->NodeGuid == FunctionNodeGuid)
            {
                CallNode = Candidate;
                ++CallNodeMatchCount;
            }
        }
    }
    if (CallNodeMatchCount != 1 || !CallNode)
    {
        return W.Error(
            FString::Printf(TEXT("function_node_guid must resolve to exactly one function call; matched %d"), CallNodeMatchCount));
    }
    if (CallNode->FunctionScript != nullptr || !CallNode->Signature.Name.ToString().Equals(ExpectedFunctionName, ESearchCase::CaseSensitive))
    {
        return W.Error(TEXT("Target is not the expected signature-only DI function call"));
    }
    if (!CallNode->Signature.VariadicInput() || CallNode->Signature.Inputs.IsEmpty())
    {
        return W.Error(TEXT("Target DI function signature does not permit variadic inputs"));
    }
    UClass* ActualDiClass = CallNode->Signature.Inputs[0].GetType().GetClass();
    if (!ActualDiClass ||
        (!ActualDiClass->GetName().Equals(ExpectedDiClass, ESearchCase::IgnoreCase) &&
         !ActualDiClass->GetPathName().Equals(ExpectedDiClass, ESearchCase::IgnoreCase)))
    {
        return W.Error(
            FString::Printf(TEXT("DI class mismatch. Expected %s, found %s"), *ExpectedDiClass, ActualDiClass ? *ActualDiClass->GetPathName() : TEXT("none")));
    }
    if(CallNode->Signature.Inputs.Num()>16384||CallNode->Signature.Outputs.Num()>16384||!W.Budget(CallNode->Signature.Inputs.Num()+CallNode->Signature.Outputs.Num()))return W.Error(TEXT("DI call signature exceeds bounded inventory"));
    const FNiagaraFunctionSignature BeforeDiSignature=CallNode->Signature;
    TArray<FNiagaraFunctionSignature> BaseDiSignatures;
    const UNiagaraDataInterface* DiCdo=DICdo(ActualDiClass,BaseDiSignatures);
    if(!DiCdo||!ContinueRead())return W.Error(W.R.Error.IsEmpty()?TEXT("Current DI CDO is unavailable"):W.R.Error);
    if(!FNiagaraFunctionSignature::StaticStruct()->CompareScriptStruct(&BeforeDiSignature,&CallNode->Signature,0))return W.Error(TEXT("DI signature changed while reading the current CDO"));
    const FNiagaraFunctionSignature* BaseDiSignaturePtr = BaseDiSignatures.FindByPredicate(
        [&ExpectedFunctionName](const FNiagaraFunctionSignature& Candidate)
        {
            return Candidate.Name.ToString().Equals(ExpectedFunctionName, ESearchCase::CaseSensitive);
        });
    if (!BaseDiSignaturePtr)
    {
        return W.Error(TEXT("Current DI CDO does not advertise the expected base function signature"));
    }
    const FNiagaraFunctionSignature BaseDiSignature = *BaseDiSignaturePtr;
    if(BaseDiSignature.Inputs.Num()>16384||BaseDiSignature.Outputs.Num()>16384||!W.Budget(BaseDiSignature.Inputs.Num()+BaseDiSignature.Outputs.Num()))return W.Error(TEXT("DI CDO base signature exceeds bounded inventory"));
    const FNiagaraFunctionSignature ExistingSignature = CallNode->Signature;
    auto PinSignatureTypes=[&](const FNiagaraFunctionSignature& Signature)->bool{auto PinType=[&](const FNiagaraTypeDefinition& Type){if(auto* Enum=Type.GetEnum())return W.Pin(Enum);if(auto* Struct=Type.GetStruct())return W.Pin(Struct);if(auto* Class=Type.GetClass())return W.Pin(Class);return Type.IsValid();};for(const auto& Variable:Signature.Inputs)if(!PinType(Variable.GetType()))return false;for(const auto& Variable:Signature.Outputs)if(!PinType(Variable.GetType()))return false;return true;};
    if(!PinSignatureTypes(ExistingSignature)||!PinSignatureTypes(BaseDiSignature))return W.Error(W.R.Error.IsEmpty()?TEXT("Signature contains an invalid reflected type"):W.R.Error);
    auto SignaturesFullyEqual = [](const FNiagaraFunctionSignature& Left, const FNiagaraFunctionSignature& Right)
    {
        const UScriptStruct* SignatureStruct = FNiagaraFunctionSignature::StaticStruct();
        return SignatureStruct && SignatureStruct->CompareScriptStruct(&Left, &Right, 0);
    };
    auto BuildSignatureFieldDiffs = [&BaseDiSignature, CallNode, &ExpectedInputs](
        const FNiagaraFunctionSignature& CurrentSignature,
        const FNiagaraFunctionSignature& Reconstructed)
    {
        TArray<TSharedPtr<FJsonValue>> Diffs;
        const UScriptStruct* SignatureStruct = FNiagaraFunctionSignature::StaticStruct();
        if (!SignatureStruct)
        {
            return Diffs;
        }
        static const TSet<FName> NonSemanticRefreshFields = {
            FName(TEXT("ExperimentalMessage")),
            FName(TEXT("Description")),
            FName(TEXT("InputDescriptions")),
            FName(TEXT("OutputDescriptions")),
            FName(TEXT("SourceFile")),
            FName(TEXT("SourceLine")),
            FName(TEXT("bHidden")),
        };
        for (TFieldIterator<FProperty> It(SignatureStruct); It; ++It)
        {
            const FProperty* Property = *It;
            if (!Property || Property->Identical_InContainer(&CurrentSignature, &Reconstructed, 0, 0))
            {
                continue;
            }
            FString CurrentText;
            FString ReconstructedText;
            Property->ExportTextItem_Direct(
                CurrentText,
                Property->ContainerPtrToValuePtr<void>(&CurrentSignature),
                nullptr,
                nullptr,
                PPF_None);
            Property->ExportTextItem_Direct(
                ReconstructedText,
                Property->ContainerPtrToValuePtr<void>(&Reconstructed),
                nullptr,
                nullptr,
                PPF_None);
            bool bIgnoredForSemanticComparison = NonSemanticRefreshFields.Contains(Property->GetFName());
            FString SpecializedIgnoreReason;
            if (Property->GetFName() == FName(TEXT("Inputs")))
            {
                bool bInputsMatchNarrowPolicy = CurrentSignature.Inputs.Num() == Reconstructed.Inputs.Num();
                const int32 StrictRequiredInputCount = BaseDiSignature.NumRequiredInputs();
                auto IsAllocatedAllZero = [](const FNiagaraVariable& Variable)
                {
                    if (!Variable.IsDataAllocated() || !Variable.GetData())
                    {
                        return false;
                    }
                    for (int32 ByteIndex = 0; ByteIndex < Variable.GetSizeInBytes(); ++ByteIndex)
                    {
                        if (Variable.GetData()[ByteIndex] != 0)
                        {
                            return false;
                        }
                    }
                    return true;
                };
                for (int32 InputIndex = 0; bInputsMatchNarrowPolicy && InputIndex < CurrentSignature.Inputs.Num(); ++InputIndex)
                {
                    const FNiagaraVariable& CurrentInput = CurrentSignature.Inputs[InputIndex];
                    const FNiagaraVariable& ReconstructedInput = Reconstructed.Inputs[InputIndex];
                    const bool bNameAndTypeMatch =
                        CurrentInput.GetName() == ReconstructedInput.GetName() &&
                        CurrentInput.GetType() == ReconstructedInput.GetType();
                    if (!bNameAndTypeMatch)
                    {
                        bInputsMatchNarrowPolicy = false;
                        break;
                    }
                    const bool bPayloadsMatch = CurrentInput.HoldsSameData(ReconstructedInput);
                    if (InputIndex < StrictRequiredInputCount)
                    {
                        bInputsMatchNarrowPolicy = bPayloadsMatch;
                        continue;
                    }
                    if (bPayloadsMatch)
                    {
                        continue;
                    }

                    bool bExpectedExactSingleLink = ExpectedInputs.IsValidIndex(InputIndex);
                    if (bExpectedExactSingleLink)
                    {
                        const FExpectedInput& Expected = ExpectedInputs[InputIndex];
                        UEdGraphPin* LinkedInputPin = FindGraphPinByName(
                            CallNode,
                            EGPD_Input,
                            CurrentInput.GetName(),
                            false);
                        bExpectedExactSingleLink =
                            Expected.PinName.Equals(CurrentInput.GetName().ToString(), ESearchCase::CaseSensitive) &&
                            Expected.TypeName.Equals(NiagaraTypeName(CurrentInput.GetType()), ESearchCase::IgnoreCase) &&
                            LinkedInputPin && LinkedInputPin->LinkedTo.Num() == 1 && LinkedInputPin->LinkedTo[0] &&
                            LinkedInputPin->LinkedTo[0]->GetOwningNode() &&
                            LinkedInputPin->LinkedTo[0]->GetOwningNode()->NodeGuid == Expected.LinkedNodeGuid &&
                            LinkedInputPin->LinkedTo[0]->PinName.ToString().Equals(Expected.LinkedPinName, ESearchCase::CaseSensitive);
                    }
                    const bool bEmptyToZeroOnly =
                        (!CurrentInput.IsDataAllocated() && IsAllocatedAllZero(ReconstructedInput)) ||
                        (!ReconstructedInput.IsDataAllocated() && IsAllocatedAllZero(CurrentInput));
                    bInputsMatchNarrowPolicy = bExpectedExactSingleLink && bEmptyToZeroOnly;
                }
                bIgnoredForSemanticComparison = bInputsMatchNarrowPolicy;
                if (bInputsMatchNarrowPolicy)
                {
                    SpecializedIgnoreReason =
                        TEXT("Input names/order/types match; required base input payloads are exact, and each differing variadic payload is only empty versus all-zero on an exact expected single-linked pin, so its default is unreachable");
                }
            }
            else if (Property->GetFName() == FName(TEXT("Outputs")))
            {
                bool bOutputShapeMatches =
                    CurrentSignature.Outputs.Num() == Reconstructed.Outputs.Num() &&
                    CurrentText.Equals(ReconstructedText, ESearchCase::CaseSensitive);
                for (int32 OutputIndex = 0; bOutputShapeMatches && OutputIndex < CurrentSignature.Outputs.Num(); ++OutputIndex)
                {
                    bOutputShapeMatches =
                        CurrentSignature.Outputs[OutputIndex].GetName() == Reconstructed.Outputs[OutputIndex].GetName() &&
                        CurrentSignature.Outputs[OutputIndex].GetType() == Reconstructed.Outputs[OutputIndex].GetType();
                }
                bIgnoredForSemanticComparison = bOutputShapeMatches;
                if (bOutputShapeMatches)
                {
                    SpecializedIgnoreReason =
                        TEXT("Output export text, count, order, names, and types are identical; reflected raw inequality is non-semantic");
                }
            }
            TSharedPtr<FJsonObject> Diff = MakeShared<FJsonObject>();
            Diff->SetStringField(TEXT("field"), Property->GetName());
            Diff->SetStringField(TEXT("current_export_text"), CurrentText);
            Diff->SetStringField(TEXT("reconstructed_export_text"), ReconstructedText);
            Diff->SetBoolField(TEXT("transient"), Property->HasAnyPropertyFlags(CPF_Transient));
            Diff->SetBoolField(TEXT("skip_for_compile_hash"), Property->HasMetaData(FName(TEXT("SkipForCompileHash"))));
            Diff->SetBoolField(TEXT("ignored_for_semantic_comparison"), bIgnoredForSemanticComparison);
            if (bIgnoredForSemanticComparison)
            {
                Diff->SetStringField(
                    TEXT("ignore_reason"),
                    !SpecializedIgnoreReason.IsEmpty()
                        ? SpecializedIgnoreReason
                        : (Property->GetFName() == FName(TEXT("bHidden"))
                            ? TEXT("Transient editor placement visibility; RefreshSignature resets it from the current DI CDO and it does not alter an existing call's data flow")
                            : TEXT("Editor description/source identity marked SkipForCompileHash or descriptive-only by FNiagaraFunctionSignature")));
            }
            Diffs.Add(MakeShared<FJsonValueObject>(Diff));
        }
        return Diffs;
    };
    auto CountFunctionalSignatureDiffs = [](const TArray<TSharedPtr<FJsonValue>>& Diffs)
    {
        int32 Count = 0;
        for (const TSharedPtr<FJsonValue>& Value : Diffs)
        {
            const TSharedPtr<FJsonObject> Diff = Value.IsValid() ? Value->AsObject() : nullptr;
            if (Diff.IsValid() && !Diff->GetBoolField(TEXT("ignored_for_semantic_comparison")))
            {
                ++Count;
            }
        }
        return Count;
    };
    const bool bBaseFlagsMatch =
        ExistingSignature.bMemberFunction == BaseDiSignature.bMemberFunction &&
        ExistingSignature.bRequiresExecPin == BaseDiSignature.bRequiresExecPin &&
        ExistingSignature.bRequiresContext == BaseDiSignature.bRequiresContext &&
        ExistingSignature.RequiredInputs == BaseDiSignature.RequiredInputs &&
        ExistingSignature.RequiredOutputs == BaseDiSignature.RequiredOutputs &&
        ExistingSignature.Outputs == BaseDiSignature.Outputs &&
        ExistingSignature.OwnerName == BaseDiSignature.OwnerName;
    bool bBaseInputPrefixMatches = ExistingSignature.Inputs.Num() >= BaseDiSignature.Inputs.Num();
    for (int32 BaseInputIndex = 0; bBaseInputPrefixMatches && BaseInputIndex < BaseDiSignature.Inputs.Num(); ++BaseInputIndex)
    {
        bBaseInputPrefixMatches = ExistingSignature.Inputs[BaseInputIndex] == BaseDiSignature.Inputs[BaseInputIndex];
    }
    if (!BaseDiSignature.VariadicInput() || !BaseDiSignature.bMemberFunction ||
        !bBaseFlagsMatch || !bBaseInputPrefixMatches)
    {
        return W.Error(
            TEXT("Existing signature base inputs, flags, required counts, outputs, or owner differ from the current DI CDO base signature"));
    }

    auto RebuildDiSignatureLikeEngine = [&BaseDiSignature, CallNode]()
    {
        FNiagaraFunctionSignature Rebuilt = BaseDiSignature;
        for (UEdGraphPin* Pin : CallNode->Pins)
        {
            if (!Pin || IsNiagaraDynamicAddPin(Pin) ||
                UEdGraphSchema_Niagara::PinToTypeDefinition(Pin) == FNiagaraTypeDefinition::GetParameterMapDef())
            {
                continue;
            }
            const FNiagaraVariable PinVariable = UEdGraphSchema_Niagara::PinToNiagaraVariable(Pin);
            if (Pin->Direction == EGPD_Input && !BaseDiSignature.Inputs.Contains(PinVariable))
            {
                Rebuilt.AddInput(PinVariable, FText::FromString(Pin->PinToolTip));
            }
            else if (Pin->Direction == EGPD_Output && !BaseDiSignature.Outputs.Contains(PinVariable))
            {
                Rebuilt.AddOutput(PinVariable, FText::FromString(Pin->PinToolTip));
            }
        }
        return Rebuilt;
    };
    const FNiagaraFunctionSignature PreflightReconstructedSignature = RebuildDiSignatureLikeEngine();
    const TArray<TSharedPtr<FJsonValue>> PreflightSignatureFieldDiffs = BuildSignatureFieldDiffs(
        ExistingSignature,
        PreflightReconstructedSignature);
    const int32 PreflightFunctionalSignatureDiffCount = CountFunctionalSignatureDiffs(PreflightSignatureFieldDiffs);
    if (PreflightFunctionalSignatureDiffCount > 0)
    {
        TSharedPtr<FJsonObject> Error = W.Error(
            TEXT("Existing DI signature has functional reflected-field differences from UE 5.6 RefreshSignature reconstruction"));
        Error->SetStringField(TEXT("error_code"), TEXT("niagara_di_signature_refresh_mismatch"));
        Error->SetNumberField(TEXT("signature_field_diff_count"), PreflightSignatureFieldDiffs.Num());
        Error->SetNumberField(TEXT("functional_signature_field_diff_count"), PreflightFunctionalSignatureDiffCount);
        Error->SetArrayField(TEXT("signature_field_diffs"), PreflightSignatureFieldDiffs);
        return Error;
    }
    if (ExpectedInputs.Num() != CallNode->Signature.Inputs.Num())
    {
        return W.Error(
            FString::Printf(TEXT("Existing signature input count mismatch. Expected %d, found %d"), ExpectedInputs.Num(), CallNode->Signature.Inputs.Num()));
    }

    auto VerifyExpectedInputs = [&](UNiagaraNodeFunctionCall* NodeToVerify) -> FString
    {
        if (!NodeToVerify)
        {
            return TEXT("Function call is missing");
        }
        for (int32 Index = 0; Index < ExpectedInputs.Num(); ++Index)
        {
            const FExpectedInput& Expected = ExpectedInputs[Index];
            if (!NodeToVerify->Signature.Inputs.IsValidIndex(Index))
            {
                return FString::Printf(TEXT("Existing signature input %d is missing"), Index);
            }
            const FNiagaraVariable& SignatureInput = NodeToVerify->Signature.Inputs[Index];
            if (!SignatureInput.GetName().ToString().Equals(Expected.PinName, ESearchCase::CaseSensitive) ||
                !NiagaraTypeName(SignatureInput.GetType()).Equals(Expected.TypeName, ESearchCase::IgnoreCase))
            {
                return FString::Printf(TEXT("Existing signature input %d does not match expected name/type"), Index);
            }
            UEdGraphPin* Pin = FindGraphPinByName(NodeToVerify, EGPD_Input, SignatureInput.GetName(), false);
            if (!Pin || Pin->LinkedTo.Num() != 1 || !Pin->LinkedTo[0] || !Pin->LinkedTo[0]->GetOwningNode() ||
                Pin->LinkedTo[0]->GetOwningNode()->NodeGuid != Expected.LinkedNodeGuid ||
                !Pin->LinkedTo[0]->PinName.ToString().Equals(Expected.LinkedPinName, ESearchCase::CaseSensitive))
            {
                return FString::Printf(TEXT("Existing input link mismatch for '%s'"), *Expected.PinName);
            }
        }
        return FString();
    };
    if (const FString VerificationError = VerifyExpectedInputs(CallNode); !VerificationError.IsEmpty())
    {
        // Only this ordinary expected-input refusal can close a pending async read as scoped unchanged.
        // Never generalize no explicit writer to global SDK purity or to a failed current observation.
        if(W.VariadicAsyncMode&&W.R.Error.IsEmpty()&&!W.R.bMutationAttempted&&!W.R.bCompileAttempted&&!W.R.bSaveAttempted&&!W.R.bChanged&&W.MutationRows.IsEmpty()&&W.Compiles.IsEmpty()&&W.Saves.IsEmpty()&&W.Partial.IsEmpty())
        {
            W.R.Data->SetBoolField(TEXT("variadic_preflight_refusal_current_verified"),false);
            if(!W.SDK(TEXT("Variadic_RefusalSignatureCurrent")))return W.Error(W.R.Error);
            const bool SignatureUnchanged=SignaturesFullyEqual(CallNode->Signature,ExistingSignature);
            if(!W.After(TEXT("Variadic_RefusalSignatureCurrent")))return W.Error(W.R.Error);
            const bool PackageUnchanged=SystemPackage&&SystemPackage==System->GetOutermost()&&SystemPackage->IsDirty()==bPackageWasDirty&&!bPackageWasDirty;
            if(SignatureUnchanged&&PackageUnchanged&&W.Admit(TEXT("variadic_expected_inputs_refused_current")))
            {
                W.R.bChangeStateKnown=true;
                W.R.Data->SetBoolField(TEXT("variadic_preflight_refusal_current_verified"),true);
                W.R.Data->SetBoolField(TEXT("scoped_target_change_state_known"),true);
                W.R.Data->SetBoolField(TEXT("scoped_target_observation_complete"),true);
                W.R.Data->SetStringField(TEXT("scoped_known_scope"),TEXT("Expected-input refusal before any authored mutation/compile/save: captured System/source/graph/node/pin identities, types and links, unchanged DI signature and clean package are current. Loader/getter/compiler/global side effects remain SDK_global_purity_unverified; no general writer-zero approval."));
            }
        }
        return W.Error(VerificationError);
    }

    struct FPinLinkFingerprint
    {
        FGuid NodeGuid;
        FString PinName;
    };
    UEdGraphPin* ExecInputPin = nullptr;
    UEdGraphPin* ExecOutputPin = nullptr;
    for (UEdGraphPin* Pin : CallNode->Pins)
    {
        if (!Pin || IsNiagaraDynamicAddPin(Pin) || UEdGraphSchema_Niagara::PinToTypeDefinition(Pin) != FNiagaraTypeDefinition::GetParameterMapDef())
        {
            continue;
        }
        if (Pin->Direction == EGPD_Input && !ExecInputPin)
        {
            ExecInputPin = Pin;
        }
        else if (Pin->Direction == EGPD_Output && !ExecOutputPin)
        {
            ExecOutputPin = Pin;
        }
    }
    if (!ExecInputPin || !ExecOutputPin || ExecInputPin->LinkedTo.Num() != 1 || ExecOutputPin->LinkedTo.Num() != 1 ||
        !ExecInputPin->LinkedTo[0] || !ExecOutputPin->LinkedTo[0])
    {
        return W.Error(TEXT("Target Append must have exactly one linked ParameterMap input and output"));
    }
    const FPinLinkFingerprint ExecInputBefore{ExecInputPin->LinkedTo[0]->GetOwningNode()->NodeGuid, ExecInputPin->LinkedTo[0]->PinName.ToString()};
    const FPinLinkFingerprint ExecOutputBefore{ExecOutputPin->LinkedTo[0]->GetOwningNode()->NodeGuid, ExecOutputPin->LinkedTo[0]->PinName.ToString()};

    for (const FNiagaraVariable& ExistingInput : CallNode->Signature.Inputs)
    {
        if (ExistingInput.GetName().ToString().Equals(PinName, ESearchCase::IgnoreCase))
        {
            return W.Error(TEXT("Duplicate variadic input rejected: signature already contains pin_name"));
        }
    }
    for (UEdGraphPin* ExistingPin : CallNode->Pins)
    {
        if (ExistingPin && ExistingPin->Direction == EGPD_Input && ExistingPin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
        {
            return W.Error(TEXT("Duplicate variadic input rejected: call node already contains pin_name"));
        }
    }
    const FNiagaraVariable ModuleInputVariable(InputType, FName(*ModuleInputName));
    for (const TPair<FNiagaraVariable, TObjectPtr<UNiagaraScriptVariable>>& Pair : Graph->GetAllMetaData())
    {
        if (Pair.Key.GetName().ToString().Equals(ModuleInputName, ESearchCase::IgnoreCase))
        {
            return W.Error(TEXT("Duplicate variadic input rejected: Scratch Pad metadata already contains module_input_name"));
        }
    }
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node || !Node->GetClass()->GetPathName().Equals(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"),ESearchCase::CaseSensitive))
        {
            continue;
        }
        for (UEdGraphPin* ExistingPin : Node->Pins)
        {
            if (ExistingPin && ExistingPin->Direction == EGPD_Output && ExistingPin->PinName.ToString().Equals(ModuleInputName, ESearchCase::IgnoreCase))
            {
                return W.Error(TEXT("Duplicate variadic input rejected: ParameterMapGet already contains module_input_name"));
            }
        }
    }

    UNiagaraNodeFunctionCall* ReferencingModule = nullptr;
    int32 ReferencingModuleCount = 0;
    struct FAsyncModuleBinding{TWeakObjectPtr<UNiagaraNodeFunctionCall> Node;TObjectPtr<UNiagaraScript> Script;};TArray<FAsyncModuleBinding> AsyncModuleBindings;
    for(const auto& CapturedEmitter:W.Emitters)
    {
        if(!ContinueRead())return W.Error(W.R.Error);
        auto* Source=Cast<UNiagaraScriptSource>(CapturedEmitter.Source.Get());if(!Source)continue;
        if(!W.CaptureGraph(Source))return W.Error(W.R.Error);
        auto* EmitterGraph=Source->NodeGraph.Get();if(!EmitterGraph)continue;
        for(UEdGraphNode* Node:EmitterGraph->Nodes)
        {
            auto* FunctionCall=Cast<UNiagaraNodeFunctionCall>(Node);
            if(W.VariadicAsyncMode&&FunctionCall)AsyncModuleBindings.Add({FunctionCall,FunctionCall->FunctionScript});
            if(FunctionCall&&FunctionCall->FunctionScript==ScratchPadScript)
            {ReferencingModule=FunctionCall;++ReferencingModuleCount;}
        }
    }
    if (ReferencingModuleCount != 1 || !ReferencingModule || ReferencingModule->NodeGuid != ExpectedReferencingModuleGuid)
    {
        return W.Error(
            FString::Printf(TEXT("Expected exactly one matching stack reference with the supplied GUID; found %d"), ReferencingModuleCount));
    }

    UEdGraphPin* InputMapOutputPin = nullptr;
    UNiagaraNodeInput* ModuleInputMapNode = nullptr;
    UNiagaraNodeOutput* ModuleOutputNode = nullptr;
    int32 ModuleInputMapNodeCount = 0;
    int32 ModuleOutputNodeCount = 0;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(Node))
        {
            if (InputNode->Input.GetType() == FNiagaraTypeDefinition::GetParameterMapDef())
            {
                ++ModuleInputMapNodeCount;
                ModuleInputMapNode = InputNode;
                for (UEdGraphPin* Pin : InputNode->Pins)
                {
                    if (Pin && Pin->Direction == EGPD_Output && UEdGraphSchema_Niagara::PinToTypeDefinition(Pin) == FNiagaraTypeDefinition::GetParameterMapDef())
                    {
                        InputMapOutputPin = Pin;
                        break;
                    }
                }
            }
        }
        else if (UNiagaraNodeOutput* OutputNode = Cast<UNiagaraNodeOutput>(Node))
        {
            if (OutputNode->GetUsage() == ENiagaraScriptUsage::Module)
            {
                ++ModuleOutputNodeCount;
                ModuleOutputNode = OutputNode;
            }
        }
    }
    const bool bInputMapGuidMatches =
        !ExpectedInputMapNodeGuid.IsValid() || (ModuleInputMapNode && ModuleInputMapNode->NodeGuid == ExpectedInputMapNodeGuid);
    const bool bOutputGuidMatches =
        !ExpectedOutputNodeGuid.IsValid() || (ModuleOutputNode && ModuleOutputNode->NodeGuid == ExpectedOutputNodeGuid);
    if (ModuleInputMapNodeCount != 1 || ModuleOutputNodeCount != 1 ||
        !InputMapOutputPin || !ModuleInputMapNode || !ModuleOutputNode ||
        !bInputMapGuidMatches || !bOutputGuidMatches)
    {
        return W.Error(
            FString::Printf(
                TEXT("Scratch Pad must have exactly one module InputMap and one module Output matching optional expected GUIDs; found %d and %d"),
                ModuleInputMapNodeCount,
                ModuleOutputNodeCount));
    }

    // UNiagaraNodeWithDynamicPins::RequestNewTypedPin is public in the UE 5.6
    // header but NiagaraEditor does not export its symbol. Reproduce its small
    // operation locally: consume the one input Add pin, create its replacement,
    // then apply the function-call-specific RefreshSignature effect by appending
    // the newly typed variable to the already verified DI signature.
    UEdGraphPin* InputAddPin = nullptr;
    int32 InputAddPinCount = 0;
    for (UEdGraphPin* Pin : CallNode->Pins)
    {
        if (Pin && Pin->Direction == EGPD_Input && IsNiagaraDynamicAddPin(Pin))
        {
            InputAddPin = Pin;
            ++InputAddPinCount;
        }
    }
    if (InputAddPinCount != 1 || !InputAddPin)
    {
        return W.Error(
            FString::Printf(TEXT("Expected exactly one variadic input Add pin; found %d"), InputAddPinCount));
    }
    const FEdGraphPinType ReplacementAddPinType = InputAddPin->PinType;


    // Every original successful compile/save branch needs a real completion gate and semantic restore.
    // This synchronous fragment implements only the original explicit no-compile/no-save success variant.
    if((bRequestCompile||bSave)&&!W.VariadicAsyncMode)
    {
        W.Fail(TEXT("niagara_di_variadic_input_compile_wait_unsupported"),TEXT("This synchronous ABI cannot complete the required variadic compile/idle/save and semantic recovery contract; refused before first Modify."));
        auto E=W.Error(W.R.Error);E->SetBoolField(TEXT("mutation_applied_in_memory"),false);E->SetBoolField(TEXT("saved"),false);E->SetBoolField(TEXT("compile_launched"),false);E->SetBoolField(TEXT("compile_completion_verified"),false);E->SetObjectField(TEXT("preflight_compile_state"),PreflightCompileState);E->SetArrayField(TEXT("preflight_compile_snapshots"),PreflightCompileSnapshotsJson);return E;
    }
    auto* ReferenceGraph=Cast<UNiagaraGraph>(ReferencingModule->GetGraph());
    if(!ReferenceGraph||!W.GraphForWrite(ReferenceGraph)||!W.Own(ReferencingModule)||!W.Own(ScratchPadScript)||!W.Own(ScriptSource)||!W.GraphForWrite(Graph)||!W.Own(CallNode))return W.Error(W.R.Error);
    UClass* ParameterMapGetClass=ExactStockClass(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"),UNiagaraNodeWithDynamicPins::StaticClass());
    if(!ParameterMapGetClass)return W.Error(W.R.Error.IsEmpty()?TEXT("Unable to load exact ParameterMapGet class"):W.R.Error);
    auto* PairProperty=FindFProperty<FMapProperty>(ParameterMapGetClass,TEXT("PinOutputToPinDefaultPersistentId"));
    auto* PairKey=PairProperty?CastField<FStructProperty>(PairProperty->KeyProp):nullptr;auto* PairValue=PairProperty?CastField<FStructProperty>(PairProperty->ValueProp):nullptr;
    if(!PairProperty||PairProperty->GetOwnerStruct()!=ParameterMapGetClass||!PairKey||!PairValue||PairKey->Struct!=TBaseStructure<FGuid>::Get()||PairValue->Struct!=TBaseStructure<FGuid>::Get())return W.Error(TEXT("Exact stock Guid-to-Guid default pair property unavailable before mutation"));
    if(!ContinueRead())return W.Error(W.R.Error);
    // Public metadata observations are copied by value. No DI Equals/Serialize callback is used here.
    struct FMetaState{FNiagaraVariable Key,Variable;TWeakObjectPtr<UNiagaraScriptVariable> Object;FGuid Change;ENiagaraDefaultMode Mode;FName Binding;ENiagaraVariantMode ValueMode;TArray<uint8> Bytes;TWeakObjectPtr<UObject> ValueObject;};
    struct FTypedProof{TArray<FMetaState> Meta;FNiagaraFunctionSignature Signature;};
    auto TypedProof=MakeShared<FTypedProof>();auto& ExpectedMeta=TypedProof->Meta;
    auto ReadMeta=[&](const FNiagaraVariable& Key,UNiagaraScriptVariable* S,FMetaState& Out)->bool
    {
        if(!Live(S)||!W.Pin(S)||!W.Own(S))return false;Out.Key=Key;Out.Object=S;Out.Variable=S->Variable;Out.Change=S->GetChangeId();Out.Mode=S->DefaultMode;Out.Binding=S->DefaultBinding.GetName();const auto& Variant=S->GetDefaultValueVariant();Out.ValueMode=Variant.GetMode();
        if(Out.ValueMode==ENiagaraVariantMode::Bytes){const int32 Count=Variant.GetNumBytes();if(Count<0||Count>65536||!W.Budget(Count))return W.Fail(TEXT("niagara_ndc_metadata_budget"),TEXT("Metadata byte payload exceeds bounded capture."));const uint8* Bytes=S->GetDefaultValueData();if(Count&&!Bytes)return W.Fail(TEXT("niagara_ndc_metadata_payload"),TEXT("Metadata byte mode has no payload."));if(Count)Out.Bytes.Append(Bytes,Count);}
        else if(Out.ValueMode==ENiagaraVariantMode::Object)Out.ValueObject=Variant.GetUObject().Get();else if(Out.ValueMode==ENiagaraVariantMode::DataInterface)Out.ValueObject=Variant.GetDataInterface();return true;
    };
    for(const auto& Pair:Graph->GetAllMetaData()){FMetaState M;if(!ReadMeta(Pair.Key,Pair.Value.Get(),M))return W.Error(W.R.Error);ExpectedMeta.Add(MoveTemp(M));}
    TypedProof->Signature=CallNode->Signature;auto& ExpectedSignature=TypedProof->Signature;
    auto VariadicGuard=[&W,TypedProof,CallNode,ReferencingModule,ScratchPadScript,Graph,ExpectedReferencingModuleGuid,SignaturesFullyEqual]()->bool
    {
        if(!W.Admit(TEXT("variadic_typed_admission")))return false;
        if(CallNode->FunctionScript||ReferencingModule->FunctionScript!=ScratchPadScript||ReferencingModule->NodeGuid!=ExpectedReferencingModuleGuid||!SignaturesFullyEqual(CallNode->Signature,TypedProof->Signature))return W.Fail(TEXT("niagara_ndc_variadic_signature_changed"),TEXT("Exact DI signature or referencing-module script changed across SDK callback."));
        const auto& Map=Graph->GetAllMetaData();if(Map.Num()!=TypedProof->Meta.Num())return W.Fail(TEXT("niagara_ndc_variadic_metadata_changed"),TEXT("Graph metadata membership changed outside the explicit plan."));
        for(const auto& M:TypedProof->Meta){UNiagaraScriptVariable* S=Map.FindRef(M.Key);if(S!=M.Object.Get()||!Live(S)||S->Variable!=M.Variable||!S->Variable.HoldsSameData(M.Variable)||S->GetChangeId()!=M.Change||S->DefaultMode!=M.Mode||S->DefaultBinding.GetName()!=M.Binding)return W.Fail(TEXT("niagara_ndc_variadic_metadata_changed"),TEXT("Captured metadata variable/default identity changed."));const auto& V=S->GetDefaultValueVariant();if(V.GetMode()!=M.ValueMode)return W.Fail(TEXT("niagara_ndc_variadic_metadata_changed"),TEXT("Metadata default storage mode changed."));if(M.ValueMode==ENiagaraVariantMode::Bytes){const int32 Count=V.GetNumBytes();const uint8* Bytes=S->GetDefaultValueData();if(Count!=M.Bytes.Num()||(Count&&(!Bytes||FMemory::Memcmp(Bytes,M.Bytes.GetData(),Count)!=0)))return W.Fail(TEXT("niagara_ndc_variadic_metadata_changed"),TEXT("Metadata default bytes changed."));}else if(M.ValueMode==ENiagaraVariantMode::Object){if(V.GetUObject().Get()!=M.ValueObject.Get())return W.Fail(TEXT("niagara_ndc_variadic_metadata_changed"),TEXT("Metadata default UObject changed."));}else if(M.ValueMode==ENiagaraVariantMode::DataInterface&&V.GetDataInterface()!=M.ValueObject.Get())return W.Fail(TEXT("niagara_ndc_variadic_metadata_changed"),TEXT("Metadata default DI pointer changed."));}
        return true;
    };
    if(!VariadicGuard())return W.Error(W.R.Error);
    W.Before=W.Projection();if(!ContinueRead())return W.Error(W.R.Error);
    W.R.Data->SetObjectField(TEXT("variadic_graph_before"),ReaderGraphFingerprint(Graph));if(!VariadicGuard())return W.Error(W.R.Error);
    bool bMutationStarted=false;
    auto PartialFailure=[&](const FString& Message,const TCHAR* Code=TEXT("niagara_di_variadic_input_partial_failure"))->TSharedPtr<FJsonObject>
    {
        bMutationStarted|=W.R.bMutationAttempted;W.Fail(Code,Message);auto E=W.Error(W.R.Error);E->SetBoolField(TEXT("mutation_applied_in_memory"),bMutationStarted);E->SetBoolField(TEXT("saved"),W.R.bSaved);E->SetBoolField(TEXT("rollback_attempted"),false);E->SetBoolField(TEXT("rollback_complete"),false);E->SetBoolField(TEXT("semantic_restore_available"),false);E->SetBoolField(TEXT("do_not_retry"),bMutationStarted);E->SetStringField(TEXT("restore_limit"),TEXT("Original compensation requires compile/wait/reload; no unsafe recovery, additional dispatch, or atomic rollback is claimed."));W.R.Data->SetBoolField(TEXT("variadic_semantic_restore_incomplete"),bMutationStarted);return E;
    };
    auto Changed=[&](const TCHAR* Stage){auto Row=W.MutationRow(Stage);Row->SetStringField(TEXT("function_node_guid"),FunctionNodeGuid.ToString());Row->SetStringField(TEXT("referencing_module_guid"),ExpectedReferencingModuleGuid.ToString());Row->SetStringField(TEXT("module_input_name"),ModuleInputName);Row->SetStringField(TEXT("pin_name"),PinName);return Row;};
    // Once Modify is entered, failure is a partial mutation receipt, never a retry or fake restore.
    for(UObject* Object:{static_cast<UObject*>(System),static_cast<UObject*>(ScratchPadScript),static_cast<UObject*>(ScriptSource),static_cast<UObject*>(Graph),static_cast<UObject*>(CallNode),static_cast<UObject*>(ReferencingModule)})
        if(!VariadicGuard()||!W.Modify(Object)||!VariadicGuard())return PartialFailure(W.R.Error);
    UNiagaraScriptVariable* ScriptVariable=nullptr;
    if(!VariadicGuard()||!W.SDK(TEXT("Variadic_NewMetadata"),true))return PartialFailure(W.R.Error);
    auto MetadataRow=Changed(TEXT("Variadic_NewMetadata"));
    ScriptVariable=NewObject<UNiagaraScriptVariable>(Graph,NAME_None,RF_Transactional);
    MetadataRow->SetBoolField(TEXT("SDK_returned"),true);MetadataRow->SetBoolField(TEXT("returned_nonnull"),ScriptVariable!=nullptr);W.R.bChanged|=ScriptVariable!=nullptr;
    if(!W.After(TEXT("Variadic_NewMetadata"))||!VariadicGuard())return PartialFailure(W.R.Error);
    if(!Live(ScriptVariable)||ScriptVariable->GetClass()!=UNiagaraScriptVariable::StaticClass()||ScriptVariable->GetOuter()!=Graph||!W.Own(ScriptVariable))return PartialFailure(TEXT("New metadata did not preserve exact class/owner"));
    if(!W.SDK(TEXT("Variadic_InitMetadata"),true))return PartialFailure(W.R.Error);
    auto InitRow=Changed(TEXT("Variadic_InitMetadata"));
    ScriptVariable->Init(ModuleInputVariable,FNiagaraVariableMetaData());ScriptVariable->DefaultMode=ENiagaraDefaultMode::Value;ScriptVariable->DefaultBinding=FNiagaraScriptVariableBinding();ScriptVariable->SetDefaultValueData(DefaultVariable.GetData());ScriptVariable->UpdateChangeId();
    InitRow->SetBoolField(TEXT("SDK_returned"),true);if(!W.After(TEXT("Variadic_InitMetadata"))||!VariadicGuard())return PartialFailure(W.R.Error);
    FMetaState NewMeta;if(!ReadMeta(ModuleInputVariable,ScriptVariable,NewMeta))return PartialFailure(W.R.Error);
    if(!VariadicGuard()||!W.SDK(TEXT("Variadic_InsertMetadata"),true))return PartialFailure(W.R.Error);auto InsertRow=Changed(TEXT("Variadic_InsertMetadata"));Graph->GetAllMetaData().Add(ModuleInputVariable,ScriptVariable);ExpectedMeta.Add(MoveTemp(NewMeta));InsertRow->SetBoolField(TEXT("SDK_returned"),true);W.R.bChanged=true;if(!W.After(TEXT("Variadic_InsertMetadata"))||!VariadicGuard())return PartialFailure(W.R.Error);
    if(!ModifyPin(InputAddPin)||!VariadicGuard())return PartialFailure(W.R.Error);
    UEdGraphPin* ReplacementAddPin=nullptr;
    if(!W.EditNode(CallNode,TEXT("Variadic_ConsumeInputAdd"),[&]{InputAddPin->PinType=UEdGraphSchema_Niagara::TypeDefinitionToPinType(InputType);InputAddPin->PinName=FName(*PinName);InputAddPin->PinFriendlyName=FText::GetEmpty();InputAddPin->bDefaultValueIsIgnored=false;ReplacementAddPin=CallNode->CreatePin(EGPD_Input,ReplacementAddPinType,FName(TEXT("Add")));W.Prefix->SetBoolField(TEXT("variadic_replacement_add_returned_nonnull"),ReplacementAddPin!=nullptr);}))return PartialFailure(W.R.Error);
    if(!VariadicGuard())return PartialFailure(W.R.Error);
    UEdGraphPin* NewCallInputPin=InputAddPin;
    if(!FindExpectedPin(W,NewCallInputPin)||!FindExpectedPin(W,ReplacementAddPin)||NewCallInputPin->Direction!=EGPD_Input||UEdGraphSchema_Niagara::PinToTypeDefinition(NewCallInputPin)!=InputType||!NewCallInputPin->PinName.ToString().Equals(PinName,ESearchCase::CaseSensitive)||!ReplacementAddPin||!IsNiagaraDynamicAddPin(ReplacementAddPin))return PartialFailure(TEXT("Niagara rejected the requested variadic int input"));
    const FNiagaraFunctionSignature RebuiltAfterInput=RebuildDiSignatureLikeEngine();if(!ContinueRead())return PartialFailure(W.R.Error);
    if(!W.Step(TEXT("Variadic_RebuildSignature"),true,[&]{CallNode->Signature=RebuiltAfterInput;ExpectedSignature=RebuiltAfterInput;})||!VariadicGuard())return PartialFailure(W.R.Error);
    auto* MapGetNode=Cast<UNiagaraNodeWithDynamicPins>(CreateGraphNode(Graph,ParameterMapGetClass));
    if(!VariadicGuard()||!MapGetNode)return PartialFailure(W.R.Error.IsEmpty()?TEXT("Failed to create exact ParameterMapGet node"):W.R.Error);
    if(!W.Modify(MapGetNode)||!VariadicGuard()||!W.Step(TEXT("Variadic_MapGetPosition"),true,[&]{MapGetNode->NodePosX=static_cast<int32>(FMath::Clamp<int64>(static_cast<int64>(CallNode->NodePosX)-480,MIN_int32,MAX_int32));MapGetNode->NodePosY=static_cast<int32>(FMath::Clamp<int64>(static_cast<int64>(CallNode->NodePosY)+360,MIN_int32,MAX_int32));})||!VariadicGuard())return PartialFailure(W.R.Error);
    UEdGraphPin* MapGetSourcePin=FindNiagaraNdcPinByType(MapGetNode,EGPD_Input,FNiagaraTypeDefinition::GetParameterMapDef());
    if(!MapGetSourcePin)return PartialFailure(TEXT("Created ParameterMapGet has no ParameterMap source pin"));
    auto* MapGetOutputPin=CreateTypedPin(MapGetNode,EGPD_Output,InputType,FName(*ModuleInputName));if(!VariadicGuard()||!MapGetOutputPin)return PartialFailure(W.R.Error.IsEmpty()?TEXT("Failed to create output pin"):W.R.Error);
    auto* MapGetDefaultPin=CreateTypedPin(MapGetNode,EGPD_Input,InputType,NAME_None);if(!VariadicGuard()||!MapGetDefaultPin)return PartialFailure(W.R.Error.IsEmpty()?TEXT("Failed to create default pin"):W.R.Error);
    if(!W.EditNode(MapGetNode,TEXT("Variadic_MapGetPinMetadata"),[&]{MapGetOutputPin->PersistentGuid=FGuid::NewGuid();MapGetOutputPin->PinType.PinSubCategory=FName(TEXT("ParameterPin"));MapGetOutputPin->PinFriendlyName=FText::AsCultureInvariant(ModuleInputName);MapGetDefaultPin->PersistentGuid=FGuid::NewGuid();MapGetDefaultPin->DefaultValue=DefaultValueString;MapGetDefaultPin->AutogeneratedDefaultValue=DefaultValueString;MapGetDefaultPin->bNotConnectable=true;MapGetDefaultPin->bDefaultValueIsReadOnly=true;})||!VariadicGuard())return PartialFailure(W.R.Error);
    auto* ActualPairProperty=DefaultPairProperty(MapGetNode);if(!ActualPairProperty||ActualPairProperty!=PairProperty)return PartialFailure(TEXT("Exact reflected ParameterMapGet default pair property changed"));
    if(!W.Step(TEXT("Variadic_MapGetDefaultPair"),true,[&]{FScriptMapHelper M(ActualPairProperty,ActualPairProperty->ContainerPtrToValuePtr<void>(MapGetNode));const int32 Index=M.AddDefaultValue_Invalid_NeedsRehash();*reinterpret_cast<FGuid*>(M.GetKeyPtr(Index))=MapGetOutputPin->PersistentGuid;*reinterpret_cast<FGuid*>(M.GetValuePtr(Index))=MapGetDefaultPin->PersistentGuid;M.Rehash();})||!VariadicGuard())return PartialFailure(W.R.Error);
    if(!W.LinkPins(InputMapOutputPin,MapGetSourcePin)||!VariadicGuard()||!W.LinkPins(MapGetOutputPin,NewCallInputPin)||!VariadicGuard())return PartialFailure(W.R.Error);
    if(!W.Step(TEXT("Variadic_MapGetSynchronize"),true,[&]{MapGetNode->MarkNodeRequiresSynchronization(TEXT("Cubeless: DI variadic module input read"),true);})||!VariadicGuard())return PartialFailure(W.R.Error);
    if(!W.Step(TEXT("Variadic_CallSynchronize"),true,[&]{CallNode->MarkNodeRequiresSynchronization(TEXT("Cubeless: DI variadic input added"),true);})||!VariadicGuard())return PartialFailure(W.R.Error);
    if(!W.RefreshVariadicReference(ReferencingModule)||!VariadicGuard())return PartialFailure(W.R.Error);
    if(!W.Step(TEXT("Variadic_ReferenceSynchronize"),true,[&]{ReferencingModule->MarkNodeRequiresSynchronization(TEXT("Cubeless: Scratch Pad signature refreshed"),true);})||!VariadicGuard())return PartialFailure(W.R.Error);
    if(!W.Step(TEXT("Variadic_NotifyGraphChanged"),true,[&]{Graph->NotifyGraphChanged();})||!VariadicGuard()||!W.Dirty()||!VariadicGuard())return PartialFailure(W.R.Error);
    if(const FString VerificationError=VerifyExpectedInputs(CallNode);!VerificationError.IsEmpty())return PartialFailure(VerificationError,TEXT("existing_input_readback_failed"));
    if(!VariadicGuard())return PartialFailure(W.R.Error);
    ExecInputPin=nullptr;ExecOutputPin=nullptr;
    for(UEdGraphPin* P:CallNode->Pins){if(IsNiagaraDynamicAddPin(P)||UEdGraphSchema_Niagara::PinToTypeDefinition(P)!=FNiagaraTypeDefinition::GetParameterMapDef())continue;if(P->Direction==EGPD_Input&&!ExecInputPin)ExecInputPin=P;else if(P->Direction==EGPD_Output&&!ExecOutputPin)ExecOutputPin=P;}
    const bool bExecLinksPreserved=ExecInputPin&&ExecOutputPin&&ExecInputPin->LinkedTo.Num()==1&&ExecOutputPin->LinkedTo.Num()==1&&ExecInputPin->LinkedTo[0]&&ExecOutputPin->LinkedTo[0]&&ExecInputPin->LinkedTo[0]->GetOwningNodeUnchecked()->NodeGuid==ExecInputBefore.NodeGuid&&ExecInputPin->LinkedTo[0]->PinName.ToString().Equals(ExecInputBefore.PinName,ESearchCase::CaseSensitive)&&ExecOutputPin->LinkedTo[0]->GetOwningNodeUnchecked()->NodeGuid==ExecOutputBefore.NodeGuid&&ExecOutputPin->LinkedTo[0]->PinName.ToString().Equals(ExecOutputBefore.PinName,ESearchCase::CaseSensitive);
    if(!bExecLinksPreserved||NewCallInputPin->LinkedTo.Num()!=1||NewCallInputPin->LinkedTo[0]!=MapGetOutputPin||MapGetSourcePin->LinkedTo.Num()!=1||MapGetSourcePin->LinkedTo[0]!=InputMapOutputPin||ReferencingModule->NodeGuid!=ExpectedReferencingModuleGuid)return PartialFailure(TEXT("Exact graph readback failed: original exec links or new parameter link differ"),TEXT("graph_readback_failed"));
    int32 NewSignatureInputCount=0;for(const auto& V:CallNode->Signature.Inputs)if(V.GetName().ToString().Equals(PinName,ESearchCase::CaseSensitive)&&V.GetType()==InputType)++NewSignatureInputCount;
    const bool bRefreshSignatureEquivalent=SignaturesFullyEqual(RebuildDiSignatureLikeEngine(),CallNode->Signature);
    if(!ContinueRead())return PartialFailure(W.R.Error);
    if(NewSignatureInputCount!=1||CallNode->Signature.Inputs.Num()!=ExpectedInputs.Num()+1||!bRefreshSignatureEquivalent)return PartialFailure(TEXT("New int signature does not exactly equal public pin reconstruction"),TEXT("signature_readback_failed"));
    UNiagaraScriptVariable* ActualMetadata=Graph->GetAllMetaData().FindRef(ModuleInputVariable);auto* ActualDefault=FindNiagaraNdcMapGetDefaultPin(Graph,FName(*ModuleInputName));
    if(!VariadicGuard())return PartialFailure(W.R.Error);
    if(ActualMetadata!=ScriptVariable||ActualMetadata->Variable!=ModuleInputVariable||ActualMetadata->DefaultMode!=ENiagaraDefaultMode::Value||ActualMetadata->DefaultBinding.GetName()!=NAME_None||!ActualMetadata->GetDefaultValueData()||FMemory::Memcmp(ActualMetadata->GetDefaultValueData(),DefaultVariable.GetData(),DefaultVariable.GetSizeInBytes())!=0||ActualDefault!=MapGetDefaultPin||!MapGetDefaultPin->DefaultValue.Equals(DefaultValueString,ESearchCase::CaseSensitive)||!MapGetDefaultPin->AutogeneratedDefaultValue.Equals(DefaultValueString,ESearchCase::CaseSensitive)||!MapGetDefaultPin->bNotConnectable||!MapGetDefaultPin->bDefaultValueIsReadOnly)return PartialFailure(TEXT("Metadata/default pair exact readback differs"),TEXT("module_default_readback_failed"));
    auto CompileWait=GNVariadicCompileState(System,bWaitForCompletion,TimeoutSeconds,PollIntervalSeconds);if(!VariadicGuard())return PartialFailure(W.R.Error);
    const bool bCompileIdle=!CompileWait->GetBoolField(TEXT("outstanding_compilation_requests_after"))&&!CompileWait->GetBoolField(TEXT("active_compilations_after"));
    if(bWaitForCompletion&&!bCompileIdle&&!W.VariadicAsyncMode)return PartialFailure(TEXT("A compile became pending after authoring; requested wait requires an async continuation not implemented by this fragment"),TEXT("niagara_di_variadic_input_compile_wait_unsupported"));
    const auto FinalCompileSnapshots=GNVariadicCaptureCompile(System);if(!VariadicGuard())return PartialFailure(W.R.Error);TArray<TSharedPtr<FJsonValue>> CompileSnapshotsJson;int32 CompileInputChangedCount=0;
    for(const auto& S:FinalCompileSnapshots){if(S.Status==ENiagaraScriptCompileStatus::NCS_Unknown)continue;for(const auto& B:PreMutationCompileSnapshots)if(B.ScriptPath.Equals(S.ScriptPath,ESearchCase::CaseSensitive)&&B.ComputedId.IsValid()&&S.ComputedId.IsValid()&&B.ComputedId!=S.ComputedId){++CompileInputChangedCount;break;}CompileSnapshotsJson.Add(MakeShared<FJsonValueObject>(GNVariadicCompileSnapshotJson(S,CompileSnapshotsJson.Num())));}
    auto Result=MakeShared<FJsonObject>();Result->SetBoolField(TEXT("success"),true);Result->SetStringField(TEXT("system_path"),System->GetPathName());Result->SetStringField(TEXT("scratch_pad_script_path"),ScratchPadScript->GetPathName());Result->SetStringField(TEXT("input_map_node_guid"),ModuleInputMapNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));Result->SetStringField(TEXT("output_node_guid"),ModuleOutputNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));Result->SetStringField(TEXT("function_node_guid"),CallNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));Result->SetStringField(TEXT("function_name"),CallNode->Signature.Name.ToString());Result->SetStringField(TEXT("di_class"),ActualDiClass->GetPathName());Result->SetStringField(TEXT("pin_name"),PinName);Result->SetStringField(TEXT("module_input_name"),ModuleInputName);Result->SetStringField(TEXT("input_type"),NiagaraTypeName(InputType));Result->SetField(TEXT("default_value"),NiagaraVariableDataToJsonValue(DefaultVariable));Result->SetStringField(TEXT("default_pin_value"),DefaultValueString);Result->SetStringField(TEXT("parameter_map_get_node_guid"),MapGetNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));Result->SetObjectField(TEXT("variadic_input_pin"),GraphPinToJsonObject(NewCallInputPin));Result->SetObjectField(TEXT("module_input_pin"),GraphPinToJsonObject(MapGetOutputPin));
    if(!VariadicGuard())return PartialFailure(W.R.Error);
    Result->SetStringField(TEXT("referencing_module_node_guid"),ReferencingModule->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));Result->SetNumberField(TEXT("signature_input_count_before"),ExpectedInputs.Num());Result->SetNumberField(TEXT("signature_input_count_after"),CallNode->Signature.Inputs.Num());Result->SetBoolField(TEXT("original_input_links_preserved"),true);Result->SetBoolField(TEXT("exec_links_preserved"),true);Result->SetBoolField(TEXT("di_base_signature_verified"),true);Result->SetBoolField(TEXT("refresh_signature_equivalent"),bRefreshSignatureEquivalent);Result->SetNumberField(TEXT("preflight_signature_field_diff_count"),PreflightSignatureFieldDiffs.Num());Result->SetNumberField(TEXT("preflight_functional_signature_field_diff_count"),PreflightFunctionalSignatureDiffCount);Result->SetArrayField(TEXT("preflight_signature_field_diffs"),PreflightSignatureFieldDiffs);Result->SetBoolField(TEXT("package_was_dirty"),bPackageWasDirty);Result->SetBoolField(TEXT("compile_was_busy"),bCompileWasBusy);Result->SetBoolField(TEXT("preflight_compile_baseline_ready"),bPreflightCompileBaselineReady);Result->SetNumberField(TEXT("preflight_known_script_count"),PreflightKnownScriptCount);Result->SetObjectField(TEXT("preflight_compile_state"),PreflightCompileState);Result->SetArrayField(TEXT("preflight_compile_snapshots"),PreflightCompileSnapshotsJson);Result->SetBoolField(TEXT("compile_launched"),false);Result->SetBoolField(TEXT("compile_idle"),bCompileIdle);Result->SetNumberField(TEXT("compile_input_changed_count"),CompileInputChangedCount);Result->SetBoolField(TEXT("compile_gate_pass"),true);Result->SetObjectField(TEXT("compile_wait"),CompileWait);Result->SetArrayField(TEXT("compile_snapshots"),CompileSnapshotsJson);Result->SetBoolField(TEXT("saved"),false);Result->SetBoolField(TEXT("mutation_applied_in_memory"),true);Result->SetStringField(TEXT("write_scope"),TEXT("scratch_pad_di_variadic_input"));Result->SetBoolField(TEXT("compile_completion_verified"),false);Result->SetBoolField(TEXT("semantic_restore_available"),false);Result->SetStringField(TEXT("supported_variant"),TEXT("original_explicit_request_compile_false_save_false"));
    auto AuthoredGraph=ReaderGraphFingerprint(Graph);W.R.Data->SetObjectField(TEXT("variadic_graph_after"),AuthoredGraph);if(!VariadicGuard())return PartialFailure(W.R.Error);
    if(W.VariadicAsyncMode)
    {
        Result->SetBoolField(TEXT("compile_gate_pass"),false);Result->SetStringField(TEXT("supported_variant"),TEXT("authored_pending_async_compile_wait_save"));
        TArray<TSharedPtr<FJsonValue>> AllTargets;for(const auto& S:PreMutationCompileSnapshots){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("script_path"),S.ScriptPath);J->SetStringField(TEXT("owner_kind"),S.OwnerKind);J->SetStringField(TEXT("owner_name"),S.OwnerName);J->SetStringField(TEXT("usage"),S.Usage);AllTargets.Add(MakeShared<FJsonValueObject>(J));}W.R.Data->SetArrayField(TEXT("variadic_compile_targets_before"),AllTargets);W.VariadicCompileSourcesFrozen=true;
        auto AuthoredReferenceGraph=ReaderGraphFingerprint(ReferenceGraph);if(!VariadicGuard())return PartialFailure(W.R.Error);
        if(!AuthoredGraph->GetBoolField(TEXT("complete"))||!AuthoredReferenceGraph->GetBoolField(TEXT("complete")))return PartialFailure(TEXT("Complete authored graph fingerprints required before asynchronous compile"));
        // No old pin is captured by this continuation. FWork first compares current physical inventory,
        // then the existing observer freshly enumerates current pins/defaults/links/signatures/metadata.
        const FString ExpectedAuthoredGraph=Json(AuthoredGraph),ExpectedReferenceGraph=Json(AuthoredReferenceGraph);
        W.VariadicSemanticCurrent=[&W,VariadicGuard,Graph,ReferenceGraph,ExpectedAuthoredGraph,ExpectedReferenceGraph,AsyncModuleBindings]()->bool
        {
            if(!VariadicGuard()||!W.Admit(TEXT("variadic_async_before_semantic_capture")))return false;
            for(const auto& Binding:AsyncModuleBindings){auto* N=Binding.Node.Get();if(!N||N->FunctionScript.GetHandle()!=Binding.Script.GetHandle())return W.Fail(TEXT("niagara_di_variadic_input_module_binding_changed"),TEXT("Captured emitter call script binding changed across yield; exactly-one module proof is no longer current."));}
            auto ActualGraph=ReaderGraphFingerprint(Graph);if(!VariadicGuard())return false;
            auto ActualReferenceGraph=ReaderGraphFingerprint(ReferenceGraph);if(!VariadicGuard())return false;
            const FString ActualAuthored=Json(ActualGraph),ActualReference=Json(ActualReferenceGraph);
            if(!ActualGraph->GetBoolField(TEXT("complete"))||!ActualReferenceGraph->GetBoolField(TEXT("complete"))||!ActualAuthored.Equals(ExpectedAuthoredGraph,ESearchCase::CaseSensitive)||!ActualReference.Equals(ExpectedReferenceGraph,ESearchCase::CaseSensitive))
            {auto M=MakeShared<FJsonObject>();M->SetStringField(TEXT("object"),TEXT("captured scratch/reference graph"));M->SetStringField(TEXT("property"),TEXT("authored semantic fingerprint"));M->SetStringField(TEXT("expected"),FMD5::HashAnsiString(*(ExpectedAuthoredGraph+ExpectedReferenceGraph)));M->SetStringField(TEXT("actual"),FMD5::HashAnsiString(*(ActualAuthored+ActualReference)));W.R.Data->SetObjectField(TEXT("variadic_async_first_mismatch"),M);return W.Fail(TEXT("niagara_di_variadic_input_semantic_changed"),TEXT("Authored signatures/metadata/defaults/reciprocal links changed across compiler/wait/save; no adoption or replay."));}
            W.R.Data->SetBoolField(TEXT("variadic_async_semantic_current_verified"),true);return W.Admit(TEXT("variadic_async_after_semantic_capture"));
        };
    }
    W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),true);return Result;
#else
    return W.Error(TEXT("add_niagara_scratch_pad_di_variadic_input requires editor-only Niagara data"));
#endif
}


FString Finish(FWork& W,const TSharedPtr<FJsonObject>& Legacy,const TCHAR* Operation)
{
 bool Success=false;if(Legacy)Legacy->TryGetBoolField(TEXT("success"),Success);
 if(!Success&&W.R.Error.IsEmpty()){FString E,C;if(Legacy){Legacy->TryGetStringField(TEXT("error"),E);Legacy->TryGetStringField(TEXT("error_code"),C);}W.Fail(C.IsEmpty()?TEXT("niagara_ndc_request_failed"):*C,E.IsEmpty()?TEXT("Dedicated operation failed; actual prefix preserved."):E);}
 if(!W.SaveHealth&&W.R.Error.IsEmpty())W.Fail(TEXT("niagara_ndc_save_failed"),TEXT("An actual save failed; earlier mutation/compile/save history retained."));
 bool Complete=false,Typed=false;W.R.Data->TryGetBoolField(TEXT("typed_target_postcondition_passed"),Typed);
 if(Success&&W.R.Error.IsEmpty()){
  if(!W.Before){W.Before=W.Projection();W.R.Data->SetStringField(TEXT("before_snapshot_scope"),TEXT("Post-load successful query/preflight baseline; not pre-load/global SDK purity proof."));}
  if(W.R.Error.IsEmpty()){auto After=W.Projection();W.R.Data->SetObjectField(TEXT("after"),After);bool B=false,A=false;W.Before->TryGetBoolField(TEXT("projection_complete"),B);After->TryGetBoolField(TEXT("projection_complete"),A);Complete=B&&A&&W.Admit(TEXT("final_exact_target_admission"));}
 }
 if(W.Before)W.R.Data->SetObjectField(TEXT("before"),W.Before);
 W.R.bSuccess=Success&&Typed&&Complete&&W.R.Error.IsEmpty();W.R.bPostconditionPassed=W.R.bSuccess;
 if(Success&&!W.R.bSuccess&&W.R.Error.IsEmpty())W.Fail(TEXT("niagara_ndc_postcondition_incomplete"),TEXT("Success requires the dedicated typed result, complete bounded target projection, and final identity admission."));
 if(W.R.bSuccess)W.R.bChangeStateKnown=true;
 W.R.Data->SetObjectField(TEXT("legacy_result"),Legacy?Legacy:MakeShared<FJsonObject>());W.R.Data->SetStringField(TEXT("operation"),Operation);W.R.Data->SetBoolField(TEXT("operation_complete"),true);W.R.Data->SetBoolField(TEXT("postcondition_passed"),W.R.bPostconditionPassed);W.R.Data->SetBoolField(TEXT("scoped_target_observation_complete"),Complete);W.R.Data->SetBoolField(TEXT("scoped_target_change_state_known"),W.R.bSuccess);W.R.Data->SetArrayField(TEXT("actual_save_prefix"),W.Saves);W.R.Data->SetArrayField(TEXT("actual_compile_prefix"),W.Compiles);W.R.Data->SetArrayField(TEXT("actual_item_prefix"),W.Partial);W.R.Data->SetArrayField(TEXT("actual_sdk_prefix"),W.MutationRows);
 W.R.Data->SetStringField(TEXT("scoped_known_scope"),TEXT("Dedicated typed layout/graph/metadata verification and exact captured target identity only. Loader/getter/compiler/global side effects remain SDK_global_purity_unverified."));
 W.R.Data->SetStringField(TEXT("save_scope"),TEXT("Actual exact SaveLoadedAsset returned true is OR history, retained after dirty/identity failure; not atomic persistence or rollback proof."));W.R.Data->SetStringField(TEXT("compile_scope"),W.VariadicAsyncMode?TEXT("Variadic completion requires actual idle including GPU requests and nonempty warning-free UpToDate cached/computed synchronized script fingerprints with a changed compilation input; RequestCompile return is enqueue history only. Failure never implies atomic restore."):TEXT("Reader/DI-call request records actual enqueue/return only; compiled null is not actual idle, GPU or executable completion. Variadic required wait/restore remains explicitly incomplete before first writer."));
 W.R.Data->SetBoolField(TEXT("required_variadic_compile_wait_implemented"),W.VariadicAsyncMode);W.R.Data->SetBoolField(TEXT("SDK_indirect_save_possible"),true);W.R.Data->SetBoolField(TEXT("Cubeless_runtime_asset_dependency_allowed"),false);W.R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);
 if(!W.R.Error.IsEmpty())W.R.Data->SetBoolField(TEXT("post_failure_live_projection_skipped"),true);
 W.R.Stage=W.R.Error.IsEmpty()?TEXT("complete"):W.FailedStage;return Operations::EncodeVariableResult(W.R,Operation);
}
// MT feature-local continuation. No compiler blocking wait, transaction across yield, or replay.
struct FVariadicContinuation;
TSharedPtr<FVariadicContinuation> VariadicPending;
TMap<const UCubelessNiagaraDataChannelVariadicJob*,TSharedPtr<FVariadicContinuation>> VariadicJobs;
TArray<TSharedPtr<FVariadicContinuation>> VariadicUnsafeHolds;
bool VariadicStopping=false,VariadicStarting=false;
FDelegateHandle VariadicPreExit;
bool VariadicGlobals()
{return IsInGameThread()&&GEditor&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage();}
bool VariadicCanRelease()
{return IsInGameThread()&&!GExitPurge&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&UObjectInitialized();}
struct FVariadicContinuation:TSharedFromThis<FVariadicContinuation>
{
    FWork W;TSharedPtr<FJsonObject> Params,Legacy;
    TStrongObjectPtr<UCubelessNiagaraDataChannelVariadicJob> JobHold{nullptr};TWeakObjectPtr<UCubelessNiagaraDataChannelVariadicJob> Job;UCubelessNiagaraDataChannelVariadicJob* Key=nullptr;
    FGuid Id=FGuid::NewGuid();FString Path,LastJson;FTSTicker::FDelegateHandle Ticker;
    bool Executing=false,Dispatched=false,Terminal=false,RequestCompile=true,Wait=false,SaveRequested=true,CompileAccepted=false,Outstanding=false,Active=false;
    double Timeout=10.0,Poll=0.1,WaitStarted=0.0,NextPoll=0.0;int32 Iterations=0;int64 Revision=0;uint64 LastFrame=MAX_uint64;
    static constexpr const TCHAR* Operation=TEXT("add_niagara_scratch_pad_di_variadic_input");
    FVariadicContinuation(){W.VariadicAsyncMode=true;W.R.Data->SetBoolField(TEXT("external_transaction_across_yield_allowed"),false);W.R.Data->SetBoolField(TEXT("all_SDK_calls_nonblocking_proven"),false);W.R.Data->SetBoolField(TEXT("global_compiler_cancel_attempted"),false);}
    void Unhook(){if(Ticker.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(Ticker);Ticker.Reset();}}
    bool JobExact()const
    {auto* O=Job.Get();return O==Key&&Live(O)&&O->GetClass()==UCubelessNiagaraDataChannelVariadicJob::StaticClass()&&O->GetOuter()==GetTransientPackage()&&O->GetPackage()==GetTransientPackage()&&O->GetOutermost()==GetTransientPackage()&&O->HasAnyFlags(RF_Transient)&&O->GetPathName().Equals(Path,ESearchCase::CaseSensitive);}
    void Prefix(bool Final)
    {
        W.R.Data->SetStringField(TEXT("native_job_id"),Id.ToString(EGuidFormats::Digits));W.R.Data->SetStringField(TEXT("native_job_path"),Path);
        W.R.Data->SetStringField(TEXT("native_job_class"),TEXT("/Script/CubelessMCPUE58Editor.CubelessNiagaraDataChannelVariadicJob"));
        W.R.Data->SetNumberField(TEXT("native_checkpoint_revision"),++Revision);W.R.Data->SetBoolField(TEXT("native_job_terminal"),Final);W.R.Data->SetBoolField(TEXT("operation_complete"),Final);
        W.R.Data->SetBoolField(TEXT("single_core_dispatch"),Dispatched);W.R.Data->SetBoolField(TEXT("required_variadic_compile_wait_implemented"),true);
        W.R.Data->SetBoolField(TEXT("snapshot_returns_cached_json_only"),true);W.R.Data->SetBoolField(TEXT("native_controller_stopped"),Final);
        W.R.Data->SetBoolField(TEXT("compiler_may_continue_after_job"),W.R.bCompileAttempted&&(Outstanding||Active||!W.R.Compiled.IsSet()));
        W.R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);W.R.Data->SetBoolField(TEXT("semantic_restore_available"),false);W.R.Data->SetBoolField(TEXT("rollback_attempted"),false);
        W.R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);W.R.Data->SetBoolField(TEXT("job_is_runtime_asset_dependency"),false);
        W.R.Data->SetStringField(TEXT("semantic_restore_scope"),TEXT("No live graph compensation/reload after an unknown compiler/callback boundary. A pre-writer refusal has no authored graph change; any entered writer/compile/save prefix is retained. Original conditional compensation/cold restoration remains incomplete, never atomic."));
        W.R.Data->SetArrayField(TEXT("actual_compile_prefix"),W.Compiles);W.R.Data->SetArrayField(TEXT("actual_save_prefix"),W.Saves);W.R.Data->SetArrayField(TEXT("actual_item_prefix"),W.Partial);W.R.Data->SetArrayField(TEXT("actual_sdk_prefix"),W.MutationRows);
        if(Legacy)W.R.Data->SetObjectField(TEXT("legacy_result"),Legacy);if(W.Before)W.R.Data->SetObjectField(TEXT("before"),W.Before);
    }
    void Pending()
    {W.R.bSuccess=W.R.bPostconditionPassed=false;W.R.bChangeStateKnown=false;Prefix(false);LastJson=Operations::EncodeVariableResult(W.R,Operation);}
    void ReleaseHolds()
    {
        if(!VariadicCanRelease()){W.R.Data->SetBoolField(TEXT("native_execution_hold_release_returned"),false);W.R.Data->SetBoolField(TEXT("quarantined_execution_holds"),true);if(!VariadicUnsafeHolds.Contains(AsShared()))VariadicUnsafeHolds.Add(AsShared());return;}
        W.VariadicSemanticCurrent=nullptr;for(auto& I:W.Objects)I->Hold.Reset();JobHold.Reset();W.R.Data->SetBoolField(TEXT("native_execution_hold_release_returned"),true);W.R.Data->SetBoolField(TEXT("quarantined_execution_holds"),false);
    }
    void Final(bool Safe)
    {
        if(Terminal)return;Unhook();Terminal=true;
        if(!W.R.Error.IsEmpty())
        {
            if(!W.R.Data->HasField(TEXT("variadic_async_first_mismatch")))
            {auto M=MakeShared<FJsonObject>();FString Selector;if(Params)Params->TryGetStringField(TEXT("system_path"),Selector);M->SetStringField(TEXT("object"),Selector.IsEmpty()?Path:Selector);M->SetStringField(TEXT("object_identity_source"),Selector.IsEmpty()?TEXT("captured transient job path"):TEXT("requested system selector; no failed-object SDK lookup"));M->SetStringField(TEXT("property"),W.R.ErrorCode);M->SetStringField(TEXT("expected"),TEXT("captured current/phase condition true"));M->SetStringField(TEXT("actual"),W.R.Error);M->SetStringField(TEXT("diagnostic_scope"),TEXT("Combined condition failure; not a fabricated individual reflected value"));W.R.Data->SetObjectField(TEXT("variadic_async_first_mismatch"),M);}
            W.R.bSuccess=W.R.bPostconditionPassed=false;if(!Legacy)Legacy=MakeShared<FJsonObject>();Legacy->SetBoolField(TEXT("success"),false);
            Legacy->SetStringField(TEXT("error_code"),W.R.ErrorCode);Legacy->SetStringField(TEXT("error"),W.R.Error);Legacy->SetBoolField(TEXT("mutation_applied_in_memory"),W.R.bMutationAttempted);
            Legacy->SetBoolField(TEXT("saved"),W.R.bSaved);Legacy->SetBoolField(TEXT("compile_launched"),CompileAccepted);Legacy->SetBoolField(TEXT("semantic_restore_available"),false);Legacy->SetBoolField(TEXT("do_not_retry"),W.R.bMutationAttempted||W.R.bCompileAttempted||W.R.bSaveAttempted);
        }
        Prefix(true);
        if(Safe&&W.R.Error.IsEmpty())LastJson=Finish(W,Legacy,Operation);
        else{if(!Safe)W.R.bChangeStateKnown=false;W.R.Data->SetBoolField(TEXT("post_failure_live_projection_skipped"),true);W.R.Stage=W.FailedStage;LastJson=Operations::EncodeVariableResult(W.R,Operation);}
        if(VariadicPending.Get()==this){VariadicPending.Reset();VariadicContinuationActive=false;}
        ReleaseHolds();LastJson=Operations::EncodeVariableResult(W.R,Operation); // Plain values only after release.
    }
    bool CurrentSemantic()
    {if(!W.Admit(TEXT("variadic_async_current")))return false;if(!W.VariadicSemanticCurrent)return W.Fail(TEXT("niagara_di_variadic_input_semantic_proof_missing"),TEXT("Authored retained semantic proof is missing; no compiler/save dispatch."));return W.VariadicSemanticCurrent();}
    bool Observe()
    {
        if(!CurrentSemantic()||!W.SDK(TEXT("MT_HasOutstandingCompilationRequests")))return false;
        Outstanding=W.System->HasOutstandingCompilationRequests(true);W.R.Data->SetBoolField(TEXT("last_outstanding_compilation_requests"),Outstanding);if(!W.After(TEXT("MT_HasOutstandingCompilationRequests")))return false;
        if(!W.SDK(TEXT("MT_HasActiveCompilations")))return false;Active=W.System->HasActiveCompilations();W.R.Data->SetBoolField(TEXT("last_active_compilations"),Active);
        return W.After(TEXT("MT_HasActiveCompilations"))&&CurrentSemantic();
    }
    void WaitReceipt(bool TimedOut)
    {
        auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("wait_for_completion"),Wait);O->SetNumberField(TEXT("timeout_seconds"),Timeout);O->SetNumberField(TEXT("poll_interval_seconds"),Poll);
        O->SetBoolField(TEXT("wait_timed_out"),TimedOut);O->SetNumberField(TEXT("wait_iterations"),Iterations);O->SetNumberField(TEXT("wait_elapsed_seconds"),FPlatformTime::Seconds()-WaitStarted);
        O->SetBoolField(TEXT("outstanding_compilation_requests_after"),Outstanding);O->SetBoolField(TEXT("active_compilations_after"),Active);
        O->SetBoolField(TEXT("blocking_wait_executed"),false);O->SetBoolField(TEXT("wait_completion_verified"),Wait&&!Outstanding&&!Active&&!TimedOut);Legacy->SetObjectField(TEXT("compile_wait"),O);Legacy->SetBoolField(TEXT("compile_launched"),CompileAccepted);Legacy->SetBoolField(TEXT("compile_idle"),!Outstanding&&!Active&&!TimedOut);
    }
    bool CompileGate()
    {
        if(!CurrentSemantic())return false;const auto Snapshots=GNVariadicCaptureCompile(W.System.Get());if(!CurrentSemantic())return false;
        const auto& ExpectedTargets=W.R.Data->GetArrayField(TEXT("variadic_compile_targets_before"));bool TargetsSame=Snapshots.Num()==ExpectedTargets.Num();
        for(int32 I=0;TargetsSame&&I<Snapshots.Num();++I){const auto E=ExpectedTargets[I]->AsObject();const auto& S=Snapshots[I];TargetsSame=S.ScriptPath.Equals(E->GetStringField(TEXT("script_path")),ESearchCase::CaseSensitive)&&S.OwnerKind.Equals(E->GetStringField(TEXT("owner_kind")),ESearchCase::CaseSensitive)&&S.OwnerName.Equals(E->GetStringField(TEXT("owner_name")),ESearchCase::CaseSensitive)&&S.Usage.Equals(E->GetStringField(TEXT("usage")),ESearchCase::CaseSensitive);}
        if(!TargetsSame)return W.Fail(TEXT("niagara_di_variadic_input_compile_source_changed"),TEXT("Current compile target path/owner/usage inventory differs from the captured pre-mutation inventory; no source adoption."));
        int32 Errors=0,Warnings=0,NonReady=0,Unsynchronized=0,Changed=0;TArray<TSharedPtr<FJsonValue>> Rows;
        const auto& Before=Legacy->GetArrayField(TEXT("preflight_compile_snapshots"));
        for(const auto& S:Snapshots)
        {
            if(S.Status==ENiagaraScriptCompileStatus::NCS_Unknown)continue;Errors+=GNVariadicCompileError(S.Status)?1:0;Warnings+=GNVariadicCompileWarning(S.Status)?1:0;
            NonReady+=S.Status==ENiagaraScriptCompileStatus::NCS_UpToDate?0:1;Unsynchronized+=S.CachedId.IsValid()&&S.ComputedId.IsValid()&&S.CachedId==S.ComputedId?0:1;
            const FString Fingerprint=GNVariadicCompileFingerprint(S.ComputedId);
            for(const auto& B:Before){const auto J=B->AsObject();if(J->GetStringField(TEXT("script_path")).Equals(S.ScriptPath,ESearchCase::CaseSensitive)&&J->GetBoolField(TEXT("computed_id_valid"))&&S.ComputedId.IsValid()&&!J->GetStringField(TEXT("computed_id_fingerprint")).Equals(Fingerprint,ESearchCase::CaseSensitive)){++Changed;break;}}
            Rows.Add(MakeShared<FJsonValueObject>(GNVariadicCompileSnapshotJson(S,Rows.Num())));
        }
        if(!CurrentSemantic())return false;const bool Idle=!Outstanding&&!Active;
        const bool Pass=!RequestCompile||(Wait&&Idle&&!Rows.IsEmpty()&&Changed>0&&Errors==0&&Warnings==0&&NonReady==0&&Unsynchronized==0);
        Legacy->SetBoolField(TEXT("compile_launched"),CompileAccepted);Legacy->SetBoolField(TEXT("compile_idle"),Idle);Legacy->SetBoolField(TEXT("compile_gate_pass"),Pass);
        Legacy->SetNumberField(TEXT("compile_input_changed_count"),Changed);Legacy->SetNumberField(TEXT("compile_error_count"),Errors);Legacy->SetNumberField(TEXT("compile_warning_count"),Warnings);
        Legacy->SetNumberField(TEXT("compile_non_ready_count"),NonReady);Legacy->SetNumberField(TEXT("compile_unsynchronized_count"),Unsynchronized);Legacy->SetArrayField(TEXT("compile_snapshots"),Rows);
        Legacy->SetBoolField(TEXT("compile_completion_verified"),RequestCompile&&Pass);W.R.Data->SetBoolField(TEXT("compile_idle_observed_at_wait_end"),Idle);
        if(RequestCompile){if(Idle)W.R.Compiled=Pass;else W.R.Compiled.Reset();}else W.R.Compiled=false;
        if(!Pass)
        {W.R.bChangeStateKnown=Idle;auto M=MakeShared<FJsonObject>();M->SetStringField(TEXT("object"),Legacy->GetStringField(TEXT("system_path")));M->SetStringField(TEXT("property"),TEXT("compile gate"));M->SetStringField(TEXT("expected"),TEXT("requested wait, GPU-inclusive idle, nonempty known UpToDate warning-free synchronized snapshots and changed input fingerprint"));M->SetStringField(TEXT("actual"),FString::Printf(TEXT("wait=%d idle=%d scripts=%d changed=%d errors=%d warnings=%d nonready=%d unsynchronized=%d"),Wait,Idle,Rows.Num(),Changed,Errors,Warnings,NonReady,Unsynchronized));W.R.Data->SetObjectField(TEXT("variadic_async_first_mismatch"),M);return W.Fail(TEXT("niagara_di_variadic_input_compile_gate_failed"),TEXT("Actual compiler/readback did not prove the original variadic completion gate; prefix retained without compensation or retry."));}
        return true;
    }
    bool Complete()
    {
        if(!CompileGate())return false;
        if(SaveRequested)
        {if(!CurrentSemantic())return false;if(!W.Save(false)){if(W.R.Error.IsEmpty())W.Fail(TEXT("niagara_di_variadic_input_save_failed"),TEXT("Actual SaveLoadedAsset returned false; original mutation/compile/save prefix retained without retry."));return false;}if(!CurrentSemantic())return false;}
        if(!CurrentSemantic())return false;Legacy->SetBoolField(TEXT("saved"),W.R.bSaved);Legacy->SetStringField(TEXT("supported_variant"),TEXT("original_async_compile_wait_save_with_partial_no_atomic_restore"));
        W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),true);W.R.bChangeStateKnown=true;return true;
    }
    void Cancel(const FString& Reason)
    {
        if(Terminal)return;W.VariadicCancelled=true;W.VariadicCancelReason=Reason.Left(8192);if(Executing)return;
        W.Fail(TEXT("niagara_di_variadic_input_cancelled"),W.VariadicCancelReason);W.R.bChangeStateKnown=!Dispatched;Final(VariadicGlobals());
    }
    bool Tick(float)
    {
        if(Terminal)return false;
        if(!VariadicGlobals()||VariadicStopping)
        {W.Fail(TEXT("niagara_di_variadic_input_unsafe_boundary"),TEXT("Exit/GC/save/world lifecycle boundary stopped this controller; no further target SDK, compiler pump, save or replay."));Final(false);return false;}
        if(Executing||LastFrame==GFrameCounter)return true;LastFrame=GFrameCounter;
        if(!JobExact()){W.Fail(TEXT("niagara_di_variadic_input_job_changed"),TEXT("Captured exact transient job identity changed; no target SDK or retry."));Final(false);return false;}
        if(Running){W.Fail(TEXT("niagara_di_variadic_input_core_busy"),TEXT("Another synchronous invocation owns the core; no target SDK."));Final(false);return false;}
        if(Dispatched&&FPlatformTime::Seconds()<NextPoll)return true;
        TGuardValue<bool> Busy(Executing,true);TGuardValue<bool> Lease(Running,true);TGuardValue<FWork*> Context(Current,&W);
        if(!Dispatched)
        {
            Dispatched=true;Legacy=HandleVariadic(Params);if(!W.R.Error.IsEmpty()||!Legacy||!Legacy->GetBoolField(TEXT("success"))){if(W.R.Error.IsEmpty())W.Fail(TEXT("niagara_di_variadic_input_core_failed"),TEXT("Original authoring core returned no successful in-memory receipt; no compiler/save dispatch."));Final(VariadicGlobals());return false;}
            if(!CurrentSemantic()){Final(VariadicGlobals());return false;}
            if(RequestCompile&&!W.Request(CompileAccepted)){Final(VariadicGlobals());return false;}
            WaitStarted=FPlatformTime::Seconds();if(!Observe()){Final(VariadicGlobals());return false;}
        }
        else
        {
            if(!Observe()){Final(VariadicGlobals());return false;}
            if(Wait&&(Outstanding||Active))
            {
                if(!W.SDK(TEXT("MT_PollForCompilationComplete"))){Final(VariadicGlobals());return false;}
                W.System->PollForCompilationComplete(false);W.Prefix->SetBoolField(TEXT("MT_PollForCompilationComplete_returned"),true);
                if(!W.After(TEXT("MT_PollForCompilationComplete"))||!CurrentSemantic()||!W.SDK(TEXT("MT_ProcessAsyncTasks"))){Final(VariadicGlobals());return false;}
                FAssetCompilingManager::Get().ProcessAsyncTasks(true);W.Prefix->SetBoolField(TEXT("MT_ProcessAsyncTasks_returned"),true);
                if(!W.After(TEXT("MT_ProcessAsyncTasks"))||!Observe()){Final(VariadicGlobals());return false;}
            }
        }
        if(!VariadicGlobals()){W.Fail(TEXT("niagara_di_variadic_input_unsafe_boundary"),TEXT("SDK returned after an unsafe lifecycle boundary; actual prefix retained."));Final(false);return false;}
        const bool TimedOut=Wait&&(Outstanding||Active)&&FPlatformTime::Seconds()-WaitStarted>=Timeout;WaitReceipt(TimedOut);
        if(TimedOut){W.Fail(TEXT("niagara_di_variadic_input_compile_timeout"),TEXT("Bounded wait expired; compiler may continue, no save/rollback/retry."));W.R.Compiled.Reset();Final(true);return false;}
        if(!Wait||(!Outstanding&&!Active)){if(!Complete()){Final(VariadicGlobals());return false;}Final(true);return false;}
        ++Iterations;NextPoll=FPlatformTime::Seconds()+Poll;Pending();return true;
    }
};
void ShutdownVariadic()
{
    VariadicStopping=true;if(!IsInGameThread())return;
    if(VariadicPreExit.IsValid()){FEditorDelegates::OnEditorPreExit.Remove(VariadicPreExit);VariadicPreExit.Reset();}
    if(auto S=VariadicPending){S->Unhook();S->Cancel(TEXT("Editor/module/Toolset shutdown stopped the owned variadic controller; compiler not cancelled."));}
}
UCubelessNiagaraDataChannelVariadicJob* StartVariadicContinuation(const FString& Text)
{
    if(!VariadicGlobals()||VariadicStopping||VariadicStarting||Running||VariadicPending.IsValid()||VariadicContinuationActive)return nullptr;
    TGuardValue<bool> StartingLease(VariadicStarting,true);
    auto S=MakeShared<FVariadicContinuation>();
    if(!Parse(Text,S->Params))S->W.Fail(TEXT("niagara_di_variadic_input_params_invalid"),TEXT("Original bounded finite JSON params required before target dispatch."));
    // Expired terminal handles are not approval tokens and do not reserve the controller indefinitely.
    for(auto It=VariadicJobs.CreateIterator();It;++It){if(It.Value()->Terminal&&!It.Value()->Job.IsValid())It.RemoveCurrent();}
    if(VariadicJobs.Num()>=64)return nullptr; // No new job/object/registry entry after the live-handle cap.
    if(S->Params){S->Params->TryGetBoolField(TEXT("request_compile"),S->RequestCompile);S->Params->TryGetBoolField(TEXT("save"),S->SaveRequested);S->Params->TryGetBoolField(TEXT("wait_for_completion"),S->Wait);S->Params->TryGetNumberField(TEXT("timeout_seconds"),S->Timeout);S->Params->TryGetNumberField(TEXT("poll_interval_seconds"),S->Poll);S->W.R.Data->SetObjectField(TEXT("requested_params"),S->Params);}
    S->Timeout=FMath::Clamp(S->Timeout,0.0,60.0);S->Poll=FMath::Clamp(S->Poll,0.02,1.0);S->W.R.bCompileRequested=S->RequestCompile;S->W.R.bSaveRequested=S->SaveRequested;
    auto* Created=NewObject<UCubelessNiagaraDataChannelVariadicJob>(GetTransientPackage(),NAME_None,RF_Transient);S->Key=Created;S->Job=Created;S->JobHold.Reset(Created);
    if(!VariadicGlobals()||!Live(Created)){if(!VariadicCanRelease())VariadicUnsafeHolds.Add(S);return nullptr;}
    S->Path=Created->GetPathName();if(!S->JobExact())return nullptr;
    if(VariadicStopping||VariadicPending.IsValid()||VariadicContinuationActive)S->W.Fail(TEXT("niagara_di_variadic_input_start_owner_changed"),TEXT("Continuation owner/stopping changed during transient job construction; target not dispatched."));
    if(VariadicJobs.Contains(Created))return nullptr;VariadicJobs.Add(Created,S);
    if(!S->W.R.Error.IsEmpty()){S->Final(true);return Created;}
    VariadicPending=S;VariadicContinuationActive=true;S->W.R.Stage=TEXT("queued_variadic_next_editor_frame");S->Pending();
    if(!VariadicPreExit.IsValid())VariadicPreExit=FEditorDelegates::OnEditorPreExit.AddStatic(&ShutdownVariadic);
    TWeakPtr<FVariadicContinuation> Weak=S;S->Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float Delta){if(auto Strong=Weak.Pin())return Strong->Tick(Delta);return false;}),0.0f);
    if(!S->Ticker.IsValid()){S->W.Fail(TEXT("niagara_di_variadic_input_ticker_unavailable"),TEXT("Owned ticker registration failed before target writer."));S->Final(true);}
    return Created;
}
FString VariadicSnapshot(UCubelessNiagaraDataChannelVariadicJob* Job)
{
    if(IsInGameThread()){const auto* Found=VariadicJobs.Find(Job);if(Found&&(*Found)->Job.Get()==Job)return (*Found)->LastJson;}
    FWork W;W.Fail(TEXT("niagara_di_variadic_input_job_unknown"),TEXT("Exact owned job snapshot unavailable; no target read/write or retry."));W.R.Data->SetBoolField(TEXT("operation_complete"),true);W.R.Data->SetBoolField(TEXT("native_job_terminal"),true);return Operations::EncodeVariableResult(W.R,TEXT("add_niagara_scratch_pad_di_variadic_input"));
}
bool CancelVariadic(UCubelessNiagaraDataChannelVariadicJob* Job,const FString& Reason)
{
    if(!IsInGameThread())return false;auto* Found=VariadicJobs.Find(Job);if(!Found||(*Found)->Job.Get()!=Job||(*Found)->Terminal)return false;auto S=*Found;S->Cancel(Reason);return true;
}

using FHandler=TSharedPtr<FJsonObject>(*)(const TSharedPtr<FJsonObject>&);
FString RunDedicated(const FString& Text,const TCHAR* Operation,FHandler Handler)
{
 FWork W;TSharedPtr<FJsonObject> Params;if(!Parse(Text,Params)){W.Fail(TEXT("niagara_ndc_invalid_params_json"),TEXT("Expected finite JSON object within1Mi UTF16/depth64/262144 tokens and visits, exact duplicate-key and embedded-NUL guards."));return Finish(W,W.Error(W.R.Error),Operation);}
 if(!IsInGameThread()){W.Fail(TEXT("niagara_ndc_game_thread_required"),TEXT("Dedicated Native request requires the Editor game thread; no global context/lease accessed."));return Finish(W,W.Error(W.R.Error),Operation);}
 if(Running||VariadicContinuationActive||VariadicStarting){W.Fail(TEXT("niagara_ndc_reentrant_request"),TEXT("Dedicated Niagara DataChannel authoring request already active; no uncertain replay."));return Finish(W,W.Error(W.R.Error),Operation);}
 TGuardValue<bool> Lease(Running,true);TGuardValue<FWork*> Context(Current,&W);W.R.Data->SetObjectField(TEXT("requested_params"),Params);bool SaveIntent=true;Params->TryGetBoolField(TEXT("save"),SaveIntent);W.R.bSaveRequested=SaveIntent;if(FCString::Strcmp(Operation,TEXT("configure_niagara_data_channel_asset"))!=0){bool CompileIntent=true;Params->TryGetBoolField(TEXT("request_compile"),CompileIntent);W.R.bCompileRequested=CompileIntent;}if(!W.Admit(TEXT("preflight")))return Finish(W,W.Error(W.R.Error),Operation);return Finish(W,Handler(Params),Operation);
}
}
UCubelessNiagaraDataChannelVariadicJob* StartVariadicInputJob(const FString& ParamsJson){return StartVariadicContinuation(ParamsJson);}
FString VariadicInputJobSnapshot(UCubelessNiagaraDataChannelVariadicJob* Job){return VariadicSnapshot(Job);}
bool CancelVariadicInputJob(UCubelessNiagaraDataChannelVariadicJob* Job,const FString& Reason){return CancelVariadic(Job,Reason);}
void ShutdownVariadicInputJobs(){ShutdownVariadic();}
FString ConfigureChannel(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("configure_niagara_data_channel_asset"),&HandleConfigure);}
FString CreateReader(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("create_niagara_ndc_reader_scratch_pad_module"),&HandleReader);}
FString AddFunctionCall(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("add_niagara_scratch_pad_di_function_call"),&HandleFunctionCall);}
FString AddVariadicInput(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("add_niagara_scratch_pad_di_variadic_input"),&HandleVariadic);}
}
