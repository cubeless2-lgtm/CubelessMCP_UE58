#include "Compatibility/UE58/CubelessBlueprintGraphTransferApi.h"
#include "Compatibility/UE58/CubelessFunctionApi.h"
#include "Compatibility/UE58/CubelessFunctionPinApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "EdGraphUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/MemberReference.h"
#include "K2Node.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Composite.h"
#include "K2Node_Tunnel.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_Self.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Editor.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "EditorAssetLibrary.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "UObject/Package.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/GarbageCollection.h"
#include "CoreGlobals.h"
#include "K2Node_Timeline.h"
#include "Engine/TimelineTemplate.h"
#include "EditorFramework/AssetImportData.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveLinearColor.h"
#include "UObject/UObjectIterator.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"

namespace Cubeless::UE58::BlueprintGraphTransfer
{
namespace
{
using Variable::FResult;
constexpr int32 MaxGraphs=32, MaxNodes=512, MaxPins=4096, MaxLinks=8192, MaxText=1048576;
void Count(FResult& R,const TCHAR* K,int32 N=1)
{
    auto C=R.Data->GetObjectField(TEXT("native_call_counts")); C->SetNumberField(K,C->GetNumberField(K)+N);
}
bool Fail(FResult& R,const TCHAR* C,const FString& E)
{
    R.bSuccess=R.bPostconditionPassed=false;R.ErrorCode=C;R.Error=E;return false;
}
bool Loaded(const UObject* O)
{
    return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_BeginDestroyed|RF_FinishDestroyed);
}
FString Json(const TSharedRef<FJsonObject>& O)
{
    FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&S));return S;
}
bool Equal(const TSharedRef<FJsonObject>& A,const TSharedRef<FJsonObject>& B)
{
    return Json(A).Equals(Json(B),ESearchCase::CaseSensitive);
}
bool ReferenceGuard(const FString& S)
{
    // This guard is only package/reference identity; a harmless /Game asset name or comment is not a tooling dependency.
    const FString L=S.ToLower();if(!L.StartsWith(TEXT("/script/"))) { return true; }
    for(const TCHAR* P:{TEXT("/script/unrealmcp"),TEXT("/script/cubelessmcp"),TEXT("/script/cubelesscontrolrigtools"),TEXT("/script/mcp_unreal"),TEXT("/script/mcpunreal"),TEXT("/script/mcpgameproject"),TEXT("/script/unrealmcpgame"),TEXT("/script/toolsetregistry"),TEXT("/script/modelcontextprotocol"),TEXT("/script/pythonscriptplugin")}) { if(L.StartsWith(P)) { return false; } }return true;
}
bool Text(const FString& S,int32 Limit,FResult& R)
{
    bool Good=S.Len()<=Limit;for(int32 I=0;Good&&I<S.Len();++I) { Good=S[I]!=0; }
    return Good||Fail(R,TEXT("graph_transfer_text_bound"),TEXT("Bounded non-NUL text required; larger original inputs remain a tracked variant."));
}
void Init(FResult& R,bool Compile,bool Save)
{
    R.bCompileRequested=Compile;R.bSaveRequested=Save;auto C=MakeShared<FJsonObject>();
    for(const TCHAR* K:{TEXT("outer_transaction_calls"),TEXT("create_default_graph_calls"),TEXT("create_default_graph_returns"),TEXT("add_ubergraph_calls"),TEXT("asset_load_calls"),TEXT("asset_load_returns"),TEXT("graph_resolve_calls"),TEXT("graph_resolve_returns"),TEXT("file_size_query_calls"),TEXT("file_read_calls"),TEXT("file_read_returns"),TEXT("reference_lookup_calls"),TEXT("reference_lookup_returns"),TEXT("property_type_observation_calls"),TEXT("member_reference_import_calls"),TEXT("member_reference_import_returns"),TEXT("member_resolve_calls"),TEXT("member_resolve_returns"),TEXT("can_import_calls"),TEXT("can_import_returns"),TEXT("blueprint_modify_calls"),TEXT("graph_modify_calls"),TEXT("node_modify_calls"),TEXT("remove_node_calls"),TEXT("remove_node_returns"),TEXT("import_nodes_calls"),TEXT("import_nodes_returns"),TEXT("new_guid_calls"),TEXT("notify_graph_calls"),TEXT("mark_structural_calls"),TEXT("mark_modified_calls"),TEXT("pin_modify_calls"),TEXT("link_array_write_calls"),TEXT("reciprocal_add_calls"),TEXT("schema_connection_calls"),TEXT("schema_connection_returns"),TEXT("get_all_graphs_calls"),TEXT("capture_calls"),TEXT("full_compile_calls"),TEXT("full_compile_returns"),TEXT("forced_save_calls"),TEXT("forced_save_returns")}) { C->SetNumberField(K,0); }
    R.Data->SetObjectField(TEXT("native_call_counts"),C);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("automatic_replay_allowed"),false);R.Data->SetBoolField(TEXT("total_editor_side_effects_verified"),false);
    R.Data->SetStringField(TEXT("count_scope"),TEXT("Direct API calls only; SDK paste/reconstruct/global-pin-resolution/structural compiler callbacks are transitive effects, not fabricated zero counts."));
    R.Data->SetStringField(TEXT("finite_scope"),TEXT("Bounded idle owned ordinary Blueprint graphs/current allocated owning pin arrays. Null/duplicate/self/external/asymmetric links are observable. Dangling memory in an owning pin array, custom graph/node classes, nested text objects and larger inputs remain incomplete."));
}
bool Scope(UBlueprint* BP,FResult& R)
{
    if(!Loaded(BP)||!BP->GetOutermost()->GetName().StartsWith(TEXT("/Game/"))||!ReferenceGuard(BP->GetPathName())||!Loaded(BP->ParentClass)||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bIsRegeneratingOnLoad||BP->bSuppressStructurallyModified) { return Fail(R,TEXT("graph_transfer_owner_pending"),TEXT("Current owned loaded /Game Blueprint, parent and idle compiler state required.")); }
    if(BP->BlueprintType!=BPTYPE_Normal||BP->NewVariables.Num()>512||BP->Status==BS_BeingCreated) { return Fail(R,TEXT("graph_transfer_blueprint_type_pending"),TEXT("Initial ordinary Blueprint scope; interface/macro/other Blueprint owners remain original variants.")); }
    return true;
}
bool Sample(UBlueprint* BP,bool Allow,FResult& R)
{
    const FString P=BP->GetOutermost()->GetName();bool B=false;for(const TCHAR* Root:{TEXT("/Game/_MCP_Sample"),TEXT("/Game/_MCP_Temp")}) { B|=P.Equals(Root,ESearchCase::IgnoreCase)||P.StartsWith(FString(Root)+TEXT("/"),ESearchCase::IgnoreCase); }
    R.Data->SetBoolField(TEXT("sample_or_temp_target"),B);R.Data->SetBoolField(TEXT("allow_non_sample"),Allow);return B||Allow||Fail(R,TEXT("graph_transfer_non_sample_refused"),TEXT("Original sample/temp target guard requires allow_non_sample=True elsewhere."));
}
UBlueprint* Load(const FString& P,FResult& R)
{
    if(!Text(P,2048,R)||!ReferenceGuard(P)) { Fail(R,TEXT("graph_transfer_path_invalid"),TEXT("Invalid Blueprint path or tooling runtime dependency."));return nullptr; }
    FString ObjectPath=P;if(P.StartsWith(TEXT("/Game/"))&&!P.Contains(TEXT("."))) { ObjectPath+=TEXT(".")+FPackageName::GetShortName(P); }if(!ObjectPath.StartsWith(TEXT("/Game/"))) { Fail(R,TEXT("graph_transfer_path_pending"),TEXT("Initial exact /Game object/package path scope; original bare-name lookup remains pending."));return nullptr; }const bool WasLoaded=FindObject<UBlueprint>(nullptr,*ObjectPath)!=nullptr;R.Data->SetBoolField(TEXT("implicit_postload_work_possible"),!WasLoaded);Count(R,TEXT("asset_load_calls"));auto* BP=LoadObject<UBlueprint>(nullptr,*ObjectPath);Count(R,TEXT("asset_load_returns"));R.Data->SetStringField(TEXT("returned_blueprint_path"),GetPathNameSafe(BP));if(!Scope(BP,R)) { if(!WasLoaded) { R.bChangeStateKnown=false; }return nullptr; }return BP;
}
struct FIndex
{
    TArray<UEdGraph*> Graphs;TSet<UEdGraphNode*> Nodes;TSet<UEdGraphPin*> Pins;TMap<UEdGraphPin*,UEdGraphNode*> Owners;int32 NodeEntries=0,PinEntries=0,Links=0;
};
bool Index(UBlueprint* BP,FIndex& I,FResult& R)
{
    Count(R,TEXT("get_all_graphs_calls"));BP->GetAllGraphs(I.Graphs);if(I.Graphs.Num()>MaxGraphs) { return Fail(R,TEXT("graph_transfer_graph_bound"),TEXT("Graph bound exceeded before further traversal.")); }
    for(UEdGraph* G:I.Graphs)
    {
        if(!G) { continue; }if(!Loaded(G)||!Loaded(G->GetSchema())||G->GetSchema()->GetClass()!=UEdGraphSchema_K2::StaticClass()) { return Fail(R,TEXT("graph_transfer_schema_pending"),TEXT("Current allocated K2 graphs required; animation/ControlRig/other schema contracts remain tracked.")); }
        for(UEdGraphNode* N:G->Nodes)
        {
            if(!N) { continue; }if(++I.NodeEntries>MaxNodes||!Loaded(N)||N->GetGraph()!=G||I.Nodes.Contains(N)) { return Fail(R,TEXT("graph_transfer_node_state_pending"),TEXT("Bounded current owned node arrays required.")); }I.Nodes.Add(N);
            for(UEdGraphPin* P:N->Pins)
            {
                if(!P) { continue; }if(++I.PinEntries>MaxPins) { return Fail(R,TEXT("graph_transfer_pin_bound"),TEXT("Owning pin-array bound exceeded.")); }
                // Only pointers supplied by a current owning node array are inspected. External LinkedTo pointers are never dereferenced.
                if(P->DefaultValue.Len()>65536||P->AutogeneratedDefaultValue.Len()>65536||P->DefaultTextValue.ToString().Len()>65536||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()) { return Fail(R,TEXT("graph_transfer_owning_pin_pending"),TEXT("Trashed/owner-mismatched owning pin storage is outside the finite public lifetime proof.")); }
                if(P->LinkedTo.Num()>MaxLinks-I.Links) { return Fail(R,TEXT("graph_transfer_link_bound"),TEXT("Raw link bound exceeded.")); }I.Links+=P->LinkedTo.Num();I.Pins.Add(P);I.Owners.Add(P,N);
            }
        }
    }
    return true;
}
TSharedRef<FJsonObject> Link(UEdGraphPin* P,const FIndex& I)
{
    auto J=MakeShared<FJsonObject>();if(!P) { J->SetStringField(TEXT("kind"),TEXT("null"));return J; }
    if(!I.Pins.Contains(P)) { J->SetStringField(TEXT("address"),LexToString(reinterpret_cast<UPTRINT>(P)));J->SetStringField(TEXT("kind"),TEXT("external_unread"));return J; }
    J->SetStringField(TEXT("kind"),TEXT("owned"));J->SetStringField(TEXT("node_id"),I.Owners.FindChecked(P)->NodeGuid.ToString());J->SetStringField(TEXT("pin_id"),P->PinId.ToString());return J;
}
TSharedRef<FJsonObject> Pin(UEdGraphPin* P,const FIndex& I)
{
    auto J=Link(P,I);if(!P||!I.Pins.Contains(P)) { return J; }J->SetStringField(TEXT("name"),P->PinName.ToString());J->SetStringField(TEXT("direction"),P->Direction==EGPD_Input?TEXT("input"):TEXT("output"));auto T=Function::Pin::Describe(P->PinType);T->SetBoolField(TEXT("is_const"),P->PinType.bIsConst);J->SetObjectField(TEXT("pin_type"),T);J->SetStringField(TEXT("default_value"),P->DefaultValue);J->SetStringField(TEXT("default_object"),GetPathNameSafe(P->DefaultObject));J->SetStringField(TEXT("default_text"),P->DefaultTextValue.ToString());J->SetStringField(TEXT("autogenerated_default"),P->AutogeneratedDefaultValue);J->SetBoolField(TEXT("default_ignored"),P->bDefaultValueIsIgnored);J->SetBoolField(TEXT("hidden"),P->bHidden);J->SetBoolField(TEXT("orphaned"),P->bOrphanedPin);TArray<TSharedPtr<FJsonValue>> A;for(UEdGraphPin* L:P->LinkedTo) { A.Add(MakeShared<FJsonValueObject>(Link(L,I))); }J->SetArrayField(TEXT("linked_to"),A);return J;
}
TSharedRef<FJsonObject> Node(UEdGraphNode* N,const FIndex& I)
{
    auto J=MakeShared<FJsonObject>();if(!N) { J->SetBoolField(TEXT("is_null"),true);return J; }J->SetStringField(TEXT("node_id"),N->NodeGuid.ToString());J->SetStringField(TEXT("name"),N->GetName());J->SetStringField(TEXT("class"),N->GetClass()->GetPathName());J->SetNumberField(TEXT("x"),N->NodePosX);J->SetNumberField(TEXT("y"),N->NodePosY);J->SetStringField(TEXT("comment"),N->NodeComment);J->SetBoolField(TEXT("transactional"),N->HasAnyFlags(RF_Transactional));J->SetBoolField(TEXT("comment_visible"),N->bCommentBubbleVisible);J->SetBoolField(TEXT("comment_pinned"),N->bCommentBubblePinned);
    // Diagnostic only: resolve no handles and read no node outside this current index.
    if(auto* Tunnel=Cast<UK2Node_Tunnel>(N))
    {
        auto Slot=[&](const TObjectPtr<UK2Node_Tunnel>& Ref)
        {
            auto V=MakeShared<FJsonObject>();const bool Resolved=Ref.IsResolved();V->SetBoolField(TEXT("resolved"),Resolved);
            if(!Resolved){V->SetStringField(TEXT("kind"),TEXT("unresolved_unread"));return V;}
            auto* Target=Ref.Get();if(!Target){V->SetStringField(TEXT("kind"),TEXT("null"));return V;}
            if(!I.Nodes.Contains(Target)){V->SetStringField(TEXT("kind"),TEXT("external_unread"));V->SetStringField(TEXT("address"),LexToString(reinterpret_cast<UPTRINT>(Target)));return V;}
            V->SetStringField(TEXT("kind"),TEXT("owned"));V->SetStringField(TEXT("node_id"),Target->NodeGuid.ToString());V->SetStringField(TEXT("name"),Target->GetName());return V;
        };
        auto Bindings=MakeShared<FJsonObject>();Bindings->SetObjectField(TEXT("InputSinkNode"),Slot(Tunnel->InputSinkNode));Bindings->SetObjectField(TEXT("OutputSourceNode"),Slot(Tunnel->OutputSourceNode));J->SetObjectField(TEXT("tunnel_bindings"),Bindings);
    }
    TArray<TSharedPtr<FJsonValue>> A;for(UEdGraphPin* P:N->Pins) { A.Add(MakeShared<FJsonValueObject>(Pin(P,I))); }J->SetArrayField(TEXT("pins"),A);return J;
}
TSharedRef<FJsonObject> Variables(UBlueprint* BP)
{
    auto J=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> A;for(const auto& V:BP->NewVariables) { auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("name"),V.VarName.ToString());Q->SetStringField(TEXT("guid"),V.VarGuid.ToString());Q->SetStringField(TEXT("default_value"),V.DefaultValue);Q->SetStringField(TEXT("flags_uint64"),LexToString(V.PropertyFlags));Q->SetStringField(TEXT("category"),V.Category.ToString());Q->SetStringField(TEXT("friendly_name"),V.FriendlyName);auto T=Function::Pin::Describe(V.VarType);T->SetBoolField(TEXT("is_const"),V.VarType.bIsConst);Q->SetObjectField(TEXT("type"),T);auto D=MakeShared<FJsonObject>();for(const auto& Pair:V.MetaDataArray) { D->SetStringField(Pair.DataKey.ToString(),Pair.DataValue); }Q->SetObjectField(TEXT("metadata"),D);A.Add(MakeShared<FJsonValueObject>(Q)); }J->SetArrayField(TEXT("variables"),A);return J;
}
bool Capture(UBlueprint* BP,const TCHAR* Phase,FResult& R,TSharedRef<FJsonObject>& Out)
{
    Count(R,TEXT("capture_calls"));FIndex I;auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("complete"),false);J->SetStringField(TEXT("blueprint_path"),BP->GetPathName());J->SetStringField(TEXT("parent_class"),GetPathNameSafe(BP->ParentClass));J->SetObjectField(TEXT("member_declarations"),Variables(BP));J->SetBoolField(TEXT("package_dirty"),BP->GetOutermost()->IsDirty());J->SetStringField(TEXT("compiler_status"),LexToString(static_cast<int32>(BP->Status)));R.Data->SetObjectField(Phase,J);Out=J;
    if(!Index(BP,I,R)) { return false; }TArray<TSharedPtr<FJsonValue>> A;for(UEdGraph* G:I.Graphs) { auto Q=MakeShared<FJsonObject>();if(!G) { Q->SetBoolField(TEXT("is_null"),true);A.Add(MakeShared<FJsonValueObject>(Q));continue; }Q->SetStringField(TEXT("graph_id"),G->GraphGuid.ToString());Q->SetStringField(TEXT("name"),G->GetName());Q->SetStringField(TEXT("class"),G->GetClass()->GetPathName());Q->SetStringField(TEXT("schema"),G->GetSchema()->GetClass()->GetPathName());TArray<TSharedPtr<FJsonValue>> N;for(UEdGraphNode* P:G->Nodes) { N.Add(MakeShared<FJsonValueObject>(Node(P,I))); }Q->SetArrayField(TEXT("nodes"),N);A.Add(MakeShared<FJsonValueObject>(Q)); }J->SetArrayField(TEXT("graphs"),A);J->SetNumberField(TEXT("node_count"),I.NodeEntries);J->SetNumberField(TEXT("pin_count"),I.PinEntries);J->SetNumberField(TEXT("link_count"),I.Links);if(Json(J).Len()>2097152) { return Fail(R,TEXT("graph_transfer_capture_bound"),TEXT("Snapshot payload bound exceeded; actual prefix retained.")); }J->SetBoolField(TEXT("complete"),true);return true;
}
TSharedPtr<FJsonObject> GraphRow(const TSharedRef<FJsonObject>& S,const FGuid& ID)
{
    for(const auto& V:S->GetArrayField(TEXT("graphs"))) { auto J=V->AsObject();FString G;if(J->TryGetStringField(TEXT("graph_id"),G)&&G.Equals(ID.ToString(),ESearchCase::CaseSensitive)) { return J; } }return nullptr;
}
bool Protected(const TSharedRef<FJsonObject>& A,const TSharedRef<FJsonObject>& B,const TOptional<FGuid>& Selected,FResult& R)
{
    bool OK=A->GetStringField(TEXT("parent_class")).Equals(B->GetStringField(TEXT("parent_class")),ESearchCase::CaseSensitive)&&Equal(A->GetObjectField(TEXT("member_declarations")).ToSharedRef(),B->GetObjectField(TEXT("member_declarations")).ToSharedRef());
    const auto& AG=A->GetArrayField(TEXT("graphs"));const auto& BG=B->GetArrayField(TEXT("graphs"));OK&=AG.Num()==BG.Num();for(const auto& V:AG) { auto J=V->AsObject();FString S;if(!J->TryGetStringField(TEXT("graph_id"),S)) { continue; }FGuid ID;if(!FGuid::Parse(S,ID)) { OK=false;continue; }auto K=GraphRow(B,ID);if(!K) { OK=false;continue; }if(!Selected.IsSet()||ID!=Selected.GetValue()) { OK&=Equal(J.ToSharedRef(),K.ToSharedRef()); } }
    R.Data->SetBoolField(TEXT("protected_graphs_and_member_declarations_preserved"),OK);return OK||Fail(R,TEXT("graph_transfer_protected_state_changed"),TEXT("Actual other graph/member declaration/parent preservation differs; accepted prefix retained without rollback/replay."));
}
bool SafeRuntimePackage(const FString& P)
{
    for(const TCHAR* Name:{TEXT("/Script/CoreUObject"),TEXT("/Script/Engine"),TEXT("/Script/InputCore"),TEXT("/Script/UMG"),TEXT("/Script/Niagara"),TEXT("/Script/NiagaraCore"),TEXT("/Script/GameplayTags"),TEXT("/Script/AIModule")}) { if(P.Equals(Name,ESearchCase::CaseSensitive)) { return true; } }return false;
}
bool RuntimeIdentity(UObject* O)
{
    if(!Loaded(O)||!ReferenceGuard(O->GetPathName())) { return false; }const FString P=O->GetOutermost()->GetName();
    if(P.StartsWith(TEXT("/Script/"))&&!SafeRuntimePackage(P)) { return false; }if(!P.StartsWith(TEXT("/Script/"))&&!P.StartsWith(TEXT("/Game/"))&&!P.StartsWith(TEXT("/Engine/"))) { return false; }
    UClass* C=Cast<UClass>(O);if(!C) { C=O->GetClass(); }for(;C;C=C->GetSuperClass()) { if(!Loaded(C)||!ReferenceGuard(C->GetPathName())||(C->HasAnyClassFlags(CLASS_Native)&&!SafeRuntimePackage(C->GetOutermost()->GetName()))) { return false; } }return true;
}
bool RuntimeObject(UObject* O)
{
    if(!RuntimeIdentity(O)||O->IsEditorOnly()) { return false; }if(auto* F=Cast<UFunction>(O)) { return !F->HasAnyFunctionFlags(FUNC_EditorOnly)&&RuntimeIdentity(F->GetOuterUClass())&&!F->GetOuterUClass()->IsEditorOnly(); }
    if(auto* C=Cast<UClass>(O)) { return C->HasAnyClassFlags(CLASS_Native)&&!C->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists); }return true;
}
bool LiteralReferences(const FString& S,FResult& R,TArray<TSharedPtr<FJsonValue>>& Rows)
{
    // Parse typed T3D object literal tokens; actual loaded references are checked, not merely searched for a forbidden substring.
    for(int32 I=0;I<S.Len();++I)
    {
        if(S[I]==TEXT('"')) { ++I;for(;I<S.Len();++I) { if(S[I]==TEXT('\\')) { ++I;continue; }if(S[I]==TEXT('"')) { break; } }if(I>=S.Len()) { return Fail(R,TEXT("graph_transfer_reference_syntax"),TEXT("Unterminated serialized string.")); }continue; }
        if(S[I]!=TEXT('\'')) { continue; }int32 Begin=I-1;while(Begin>=0&&(FChar::IsAlnum(S[Begin])||S[Begin]==TEXT('_')||S[Begin]==TEXT('/'))) { --Begin; }const FString Kind=S.Mid(Begin+1,I-Begin-1);int32 End=I+1;for(;End<S.Len()&&S[End]!=TEXT('\'');++End) {}if(Kind.IsEmpty()||End>=S.Len()) { return Fail(R,TEXT("graph_transfer_reference_syntax"),TEXT("Unsupported single-quoted reference syntax; wider serialized formats remain pending.")); }FString Path=S.Mid(I+1,End-I-1);if(Path.Len()>=2&&Path[0]==TEXT('"')&&Path[Path.Len()-1]==TEXT('"')) { Path=Path.Mid(1,Path.Len()-2); }if(!Path.StartsWith(TEXT("/"))||!ReferenceGuard(Path)||Rows.Num()>=512) { return Fail(R,TEXT("graph_transfer_reference_pending"),TEXT("Only bounded absolute loaded runtime object references are admitted; local quoted/nested object variants remain pending.")); }
        Count(R,TEXT("reference_lookup_calls"));UObject* O=FindObject<UObject>(nullptr,*Path);Count(R,TEXT("reference_lookup_returns"));auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("literal_type"),Kind);J->SetStringField(TEXT("requested_path"),Path);J->SetStringField(TEXT("returned_path"),GetPathNameSafe(O));J->SetBoolField(TEXT("runtime_allowed"),RuntimeObject(O));Rows.Add(MakeShared<FJsonValueObject>(J));R.Data->SetArrayField(TEXT("reference_observations"),Rows);const bool KindOK=(Kind==TEXT("Class")&&Cast<UClass>(O))||(Kind==TEXT("ScriptStruct")&&Cast<UScriptStruct>(O))||(Kind==TEXT("Enum")&&Cast<UEnum>(O))||(Kind==TEXT("Function")&&Cast<UFunction>(O))||(Kind==TEXT("Package")&&Cast<UPackage>(O))||(Kind==TEXT("Object")&&O);J->SetBoolField(TEXT("literal_type_matches"),KindOK);if(!KindOK||!RuntimeObject(O)||!O->GetPathName().Equals(Path,ESearchCase::CaseSensitive)) { return Fail(R,TEXT("graph_transfer_reference_pending"),TEXT("Serialized reference must resolve to the exact loaded current runtime object before target paste.")); }I=End;
    }
    return true;
}
bool Admission(UBlueprint* BP,const FString& S,FResult& R)
{
    R.Stage=TEXT("text_reference_admission");R.Data->SetBoolField(TEXT("runtime_reference_admission_passed"),false);if(!Text(S,MaxText,R)) { return false; }
    TArray<TSharedPtr<FJsonValue>> Refs,Classes;R.Data->SetArrayField(TEXT("reference_observations"),Refs);if(!LiteralReferences(S,R,Refs)) { return false; }const TCHAR* Stream=*S;FString Line;int32 Depth=0,Objects=0;UClass* C=nullptr;
    while(FParse::Line(&Stream,Line))
    {
        FString T=Line.TrimStartAndEnd();if(T.StartsWith(TEXT("CustomProperties Pin "),ESearchCase::IgnoreCase))
        {
            FString Path;const TCHAR* Rest=*T;int32 Position=T.Find(TEXT("DefaultObject="),ESearchCase::CaseSensitive);if(Position!=INDEX_NONE)
            {
                Rest+=Position+14;FString Value;auto* StringProperty=FindFProperty<FStrProperty>(UEdGraphNode::StaticClass(),TEXT("NodeComment"));if(!StringProperty||!StringProperty->ImportText_Direct(Rest,&Value,nullptr,PPF_Delimited)) { return Fail(R,TEXT("graph_transfer_pin_object_syntax"),TEXT("Actual serialized pin DefaultObject FString did not parse.")); }if(!Value.IsEmpty()&&Value!=TEXT("None")) { Count(R,TEXT("reference_lookup_calls"));auto* O=FindObject<UObject>(nullptr,*Value);Count(R,TEXT("reference_lookup_returns"));auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("field"),TEXT("DefaultObject"));J->SetStringField(TEXT("requested_path"),Value);J->SetStringField(TEXT("returned_path"),GetPathNameSafe(O));Refs.Add(MakeShared<FJsonValueObject>(J));R.Data->SetArrayField(TEXT("reference_observations"),Refs);if(!RuntimeObject(O)||!O->GetPathName().Equals(Value,ESearchCase::CaseSensitive)) { return Fail(R,TEXT("graph_transfer_pin_object_pending"),TEXT("Pin DefaultObject must be exact current loaded runtime object before target paste.")); } }
            }
        }
        if(T.StartsWith(TEXT("Begin Object"),ESearchCase::IgnoreCase))
        {
            if(++Depth!=1||++Objects>128||T.Contains(TEXT("Archetype="),ESearchCase::IgnoreCase)) { return Fail(R,TEXT("graph_transfer_nested_text_pending"),TEXT("Initial simple native node blocks only; nested/archetype paste contracts remain explicit variants.")); }FString CP;if(!FParse::Value(*T,TEXT("Class="),CP)||!CP.StartsWith(TEXT("/Script/BlueprintGraph."))) { return Fail(R,TEXT("graph_transfer_node_class_pending"),TEXT("Explicit loaded native BlueprintGraph node class required; broader valid node classes remain pending.")); }
            Count(R,TEXT("reference_lookup_calls"));C=FindObject<UClass>(nullptr,*CP);Count(R,TEXT("reference_lookup_returns"));auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("requested_class"),CP);J->SetStringField(TEXT("returned_class"),GetPathNameSafe(C));Classes.Add(MakeShared<FJsonValueObject>(J));R.Data->SetArrayField(TEXT("node_class_observations"),Classes);const bool Supported=C&&(C->GetName()==TEXT("K2Node_InputTouch")||C->GetName()==TEXT("K2Node_InputKey")||C->GetName()==TEXT("K2Node_InputAction")||C->GetName()==TEXT("K2Node_InputAxisEvent")||C->GetName()==TEXT("K2Node_Self")||C->GetName()==TEXT("K2Node_LiteralEnum")||C->GetName()==TEXT("K2Node_CallFunction")||C->GetName()==TEXT("K2Node_VariableGet")||C->GetName()==TEXT("K2Node_VariableSet"));if(!Supported||!Loaded(C)||!C->HasAnyClassFlags(CLASS_Native)||!C->IsChildOf(UK2Node::StaticClass())||!ReferenceGuard(CP)||!C->GetPathName().Equals(CP,ESearchCase::CaseSensitive)) { return Fail(R,TEXT("graph_transfer_node_class_pending"),TEXT("Initial loaded native input/self/literal/call/variable node serializer scope unavailable; other valid native node classes remain explicit pending variants.")); }
        }
        else if(T.StartsWith(TEXT("End Object"),ESearchCase::IgnoreCase)) { if(Depth!=1) { return Fail(R,TEXT("graph_transfer_text_structure_invalid"),TEXT("Unbalanced node block.")); }Depth=0;C=nullptr; }
        else if(!T.IsEmpty())
        {
            if(Depth!=1||!C) { return Fail(R,TEXT("graph_transfer_text_structure_pending"),TEXT("Only complete ordinary node blocks are admitted.")); }int32 At;if(T.FindChar(TEXT('='),At)) { const FString Key=T.Left(At).TrimStartAndEnd(),Value=T.Mid(At+1);if(Key==TEXT("FunctionReference")||Key==TEXT("VariableReference")||Key==TEXT("DelegateReference"))
                {
                    if(Value.Contains(TEXT("MemberParent="))&&!Value.Contains(TEXT("MemberParent=Class'"))&&!Value.Contains(TEXT("MemberParent=Package'"))&&!Value.Contains(TEXT("MemberParent=None"))) { return Fail(R,TEXT("graph_transfer_member_parent_form_pending"),TEXT("Explicit validated exported member-parent object literal or None required before scratch parse.")); }auto* P=FindFProperty<FStructProperty>(C,*Key);if(!P||P->Struct!=FMemberReference::StaticStruct()) { return Fail(R,TEXT("graph_transfer_member_reference_pending"),TEXT("Exact reflected member reference type required.")); }FMemberReference Ref;Count(R,TEXT("member_reference_import_calls"));const TCHAR* End=P->ImportText_Direct(*Value,&Ref,nullptr,PPF_None);Count(R,TEXT("member_reference_import_returns"));if(!End||*End!=0) { return Fail(R,TEXT("graph_transfer_member_reference_invalid"),TEXT("Member reference text did not parse exactly.")); }UClass* Parent=Ref.GetMemberParentClass(BP->SkeletonGeneratedClass);if(!RuntimeIdentity(Parent)||Parent->IsEditorOnly()) { return Fail(R,TEXT("graph_transfer_member_parent_pending"),TEXT("Current runtime member scope required.")); }Count(R,TEXT("member_resolve_calls"));UObject* FunctionObject=nullptr;FProperty* Property=nullptr;if(Key==TEXT("FunctionReference")) { FunctionObject=Ref.ResolveMember<UFunction>(BP->SkeletonGeneratedClass); }else { Property=Ref.ResolveMember<FProperty>(BP->SkeletonGeneratedClass); }Count(R,TEXT("member_resolve_returns"));auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("field"),Key);J->SetStringField(TEXT("name"),Ref.GetMemberName().ToString());J->SetStringField(TEXT("parent"),Parent->GetPathName());J->SetStringField(TEXT("returned_function"),GetPathNameSafe(FunctionObject));J->SetStringField(TEXT("returned_property"),Property?Property->GetName():TEXT(""));Refs.Add(MakeShared<FJsonValueObject>(J));R.Data->SetArrayField(TEXT("reference_observations"),Refs);bool Types=true;auto TypeAllowed=[&](FProperty* Field){FEdGraphPinType Type;Count(R,TEXT("property_type_observation_calls"));if(!GetDefault<UEdGraphSchema_K2>()->ConvertPropertyToPinType(Field,Type)) { return false; }return (!Type.PinSubCategoryObject.Get()||RuntimeObject(Type.PinSubCategoryObject.Get()))&&(!Type.PinValueType.TerminalSubCategoryObject.Get()||RuntimeObject(Type.PinValueType.TerminalSubCategoryObject.Get()));};if(auto* Function=Cast<UFunction>(FunctionObject)) { for(TFieldIterator<FProperty> It(Function,EFieldIteratorFlags::ExcludeSuper);It;++It) { if(It->HasAnyPropertyFlags(CPF_Parm)) { Types&=TypeAllowed(*It); } } }if(Property) { Types&=TypeAllowed(Property); }J->SetBoolField(TEXT("actual_property_types_allowed"),Types);if(!Types||(FunctionObject&&!RuntimeObject(FunctionObject))||(!FunctionObject&&!Property)) { return Fail(R,TEXT("graph_transfer_member_unavailable"),TEXT("Member must resolve before paste; new-member synthesis/redirect/unknown signatures remain pending.")); }
                }
            }
        }
    }
    if(Depth!=0||Objects==0) { return Fail(R,TEXT("graph_transfer_text_structure_invalid"),TEXT("Complete nonempty node text required.")); }R.Data->SetBoolField(TEXT("runtime_reference_admission_passed"),true);R.Data->SetStringField(TEXT("reference_admission_scope"),TEXT("Loaded native K2 classes, typed object literals, serialized pin DefaultObject and reflected member/property types with exact runtime package/class hierarchy identities; simple node blocks only. Broader runtime packages, soft/custom reference schemas remain pending."));return true;
}
bool Disallowed(const UEdGraphSchema* S,UEdGraphPin* A,UEdGraphPin* B,FResult& R)
{
    if(!S||!A||!B) { return false; }if(A->Direction==B->Direction) { return true; }UEdGraphPin* Out=A->Direction==EGPD_Output?A:B;UEdGraphPin* In=A->Direction==EGPD_Output?B:A;Count(R,TEXT("schema_connection_calls"));const auto Response=S->CanCreateConnection(Out,In);Count(R,TEXT("schema_connection_returns"));return Response.Response==CONNECT_RESPONSE_DISALLOW;
}
void Finish(UBlueprint* BP,bool Validate,FResult& R,const TSharedRef<FJsonObject>& Legacy)
{
    Legacy->SetBoolField(TEXT("compiled"),false);Legacy->SetBoolField(TEXT("saved"),false);if(Validate) { R.Data->SetObjectField(TEXT("legacy_result"),Legacy);return; }
    if(R.bCompileRequested)
    {
        R.Stage=TEXT("compile");R.bMutationAttempted=R.bChanged=true;R.bCompileAttempted=true;Count(R,TEXT("full_compile_calls"));FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);Count(R,TEXT("full_compile_returns"));const bool Old=BP->GeneratedClass!=nullptr;Legacy->SetBoolField(TEXT("compiled"),Old);R.Data->SetBoolField(TEXT("legacy_generated_class_compile_predicate"),Old);R.Compiled=Log.NumErrors==0&&(BP->Status==BS_UpToDate||BP->Status==BS_UpToDateWithWarnings)&&Loaded(BP->GeneratedClass);auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("error_count"),Log.NumErrors);J->SetNumberField(TEXT("warning_count"),Log.NumWarnings);J->SetStringField(TEXT("status"),LexToString(static_cast<int32>(BP->Status)));J->SetStringField(TEXT("generated_class"),GetPathNameSafe(BP->GeneratedClass));J->SetBoolField(TEXT("success"),R.Compiled.GetValue());J->SetBoolField(TEXT("legacy_success"),Old);J->SetStringField(TEXT("option"),TEXT("Original default None; implicit compiler/editor delegates are separate from direct counters."));J->SetBoolField(TEXT("requested"),true);J->SetStringField(TEXT("blueprint"),BP->GetPathName());auto OldResult=MakeShared<FJsonObject>();OldResult->SetBoolField(TEXT("requested"),true);OldResult->SetBoolField(TEXT("success"),Old);OldResult->SetStringField(TEXT("blueprint"),BP->GetPathName());OldResult->SetStringField(TEXT("generated_class"),GetPathNameSafe(BP->GeneratedClass));Legacy->SetObjectField(TEXT("compile_result"),OldResult);R.Data->SetObjectField(TEXT("compile_result"),J);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
        TSharedRef<FJsonObject> Observed=MakeShared<FJsonObject>();if(!Capture(BP,TEXT("after_compile"),R,Observed)) { R.bChangeStateKnown=false; }
    }
    if(R.bSaveRequested)
    {
        R.Stage=TEXT("save");R.bMutationAttempted=R.bChanged=true;R.bSaveAttempted=true;Count(R,TEXT("forced_save_calls"));R.bSaved=UEditorAssetLibrary::SaveLoadedAsset(BP,false);Count(R,TEXT("forced_save_returns"));Legacy->SetBoolField(TEXT("saved"),R.bSaved);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.Data->SetBoolField(TEXT("forced_save_returned"),R.bSaved);TSharedRef<FJsonObject> Observed=MakeShared<FJsonObject>();if(!Capture(BP,TEXT("after_save"),R,Observed)) { R.bChangeStateKnown=false; }
    }
    Legacy->SetBoolField(TEXT("dirty_after"),BP->GetOutermost()->IsDirty());R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
}
void Terminal(FResult& R)
{
    if(!R.ErrorCode.IsEmpty()) { return; }if(!R.bChangeStateKnown) { Fail(R,TEXT("graph_transfer_observation_incomplete"),TEXT("Returned mutation/compiler/save prefix retained; current postcondition observation incomplete."));return; }
    if(R.bCompileAttempted&&(!R.Compiled.IsSet()||!R.Compiled.GetValue())) { Fail(R,TEXT("graph_transfer_compile_failed"),TEXT("Legacy GeneratedClass predicate retained separately; actual compiler errors/status are not healthy."));return; }if(R.bSaveAttempted&&!R.bSaved) { Fail(R,TEXT("graph_transfer_save_failed"),TEXT("Original direct force-save returnedFalse; legacy success/prefix retained."));return; }R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");
}
FString Kind(UBlueprint* BP,UEdGraph* G)
{
    if(BP->FunctionGraphs.Contains(G)) { return TEXT("function"); }if(BP->MacroGraphs.Contains(G)) { return TEXT("macro"); }if(BP->UbergraphPages.Contains(G)||BP->EventGraphs.Contains(G)) { return TEXT("event"); }if(BP->DelegateSignatureGraphs.Contains(G)) { return TEXT("delegate"); }return TEXT("unknown");
}
FString Normalize(FString S)
{
    S=S.ToLower();S.ReplaceInline(TEXT(" "),TEXT(""));S.ReplaceInline(TEXT("-"),TEXT("_"));if(S==TEXT("eventgraph")||S==TEXT("event_graph")||S==TEXT("ubergraph")||S==TEXT("uber_graph")) { return TEXT("event"); }if(S==TEXT("functiongraph")||S==TEXT("function_graph")) { return TEXT("function"); }if(S==TEXT("macrograph")||S==TEXT("macro_graph")) { return TEXT("macro"); }return S;
}
UEdGraph* Resolve(UBlueprint* BP,const FString& Name,const FString& Id,const FString& Type,FResult& R)
{
    Count(R,TEXT("graph_resolve_calls"));if(!Text(Name,1024,R)||!Text(Id,128,R)||!Text(Type,128,R)) { return nullptr; }const FString T=Normalize(Type);UEdGraph* Found=nullptr;
    if(Name.IsEmpty()&&Id.IsEmpty()&&T.IsEmpty())
    {
        for(UEdGraph* G:BP->UbergraphPages) { if(Loaded(G)&&G->GetName().Contains(TEXT("EventGraph"))) { Found=G;break; } }
        if(!Found)
        {
            // The original selector creates a default EventGraph before text/sample/dry checks. This is not a known-zero validation path.
            R.Stage=TEXT("implicit_default_graph");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("create_default_graph_calls"));Found=FBlueprintEditorUtils::CreateNewGraph(BP,FName(TEXT("EventGraph")),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());Count(R,TEXT("create_default_graph_returns"));R.Data->SetStringField(TEXT("implicit_graph_returned"),GetPathNameSafe(Found));if(!Found) { Fail(R,TEXT("graph_transfer_default_graph_failed"),TEXT("Original implicit EventGraph creation returnedNull; prefix unknown, no retry."));return nullptr; }Count(R,TEXT("add_ubergraph_calls"));FBlueprintEditorUtils::AddUbergraphPage(BP,Found);R.bChanged=true;TSharedRef<FJsonObject> State=MakeShared<FJsonObject>();if(!Capture(BP,TEXT("after_implicit_graph"),R,State)) { return nullptr; }R.bChangeStateKnown=true;
        }
    }
    else
    {
        TArray<UEdGraph*> All;Count(R,TEXT("get_all_graphs_calls"));BP->GetAllGraphs(All);if(All.Num()>MaxGraphs) { Fail(R,TEXT("graph_transfer_graph_bound"),TEXT("Selector graph bound exceeded."));return nullptr; }
        for(UEdGraph* G:All)
        {
            if(!Loaded(G)) { continue; }FGuid Guid;if(!Id.IsEmpty()&&((FGuid::Parse(Id,Guid)&&G->GraphGuid!=Guid)||(!FGuid::Parse(Id,Guid)&&G->GraphGuid.ToString()!=Id))) { continue; }if(!Name.IsEmpty()&&!G->GetName().Equals(Name,ESearchCase::IgnoreCase)) { continue; }if(!T.IsEmpty()&&Kind(BP,G)!=T) { continue; }Found=G;break; // Original first matching graph, not a new ambiguity error.
        }
    }
    Count(R,TEXT("graph_resolve_returns"));R.Data->SetStringField(TEXT("returned_graph_path"),GetPathNameSafe(Found));if(!Loaded(Found)||!Loaded(Found->GetSchema())||Found->GetSchema()->GetClass()!=UEdGraphSchema_K2::StaticClass()) { Fail(R,TEXT("graph_transfer_graph_unavailable"),TEXT("Original selectors did not resolve a current K2 graph; wider schema variants remain pending."));return nullptr; }return Found;
}
bool HealthyLinks(const FIndex& I,FResult& R)
{
    bool Good=true;for(UEdGraphPin* P:I.Pins) { TSet<UEdGraphPin*> Seen;const auto* Schema=I.Owners.FindChecked(P)->GetGraph()->GetSchema();for(UEdGraphPin* L:P->LinkedTo) { if(!L||L==P||!I.Pins.Contains(L)||Seen.Contains(L)) { Good=false;continue; }Seen.Add(L);if(Disallowed(Schema,P,L,R)||!L->LinkedTo.Contains(P)) { Good=false; } } }R.Data->SetBoolField(TEXT("current_owned_link_arrays_normalized"),Good);return Good;
}

// UZ: bounded recursively owned collapsed text import. The simple/file/clear writer below is unchanged.
// ImportNodesFromText owns PostPaste/Reconstruct before returning (EdGraphUtilities.cpp484).
// This plan therefore describes logical pins/links, never pre-paste physical pin addresses.
struct FCollapsedPinPlan
{
    FGuid Id; FString Name,Category,SubCategory,Raw,Auto,TypeObject,DefaultObject;bool Hidden=false; EEdGraphPinDirection Direction=EGPD_Input;
    TArray<FString> Links;
};
struct FCollapsedObjectPlan
{
    FString Name,Class,MemberName; int32 Parent=INDEX_NONE; FGuid Guid; int32 X=0,Y=0;
    FString Comment; bool Inputs=false,Outputs=false; TMap<FString,FString> Fields;
    TArray<FCollapsedPinPlan> Pins; TArray<FString> UserNames;TMap<FString,FString> UserTypes,UserDirections,UserDefaults;
};
struct FCollapsedGraphPlan
{
    int32 Graph=INDEX_NONE,Composite=INDEX_NONE,Entry=INDEX_NONE,Exit=INDEX_NONE;
    TArray<int32> Nodes,Subs;bool SubOrderAuthored=false;
};
struct FCollapsedTextPlan
{
    TArray<FCollapsedObjectPlan> Objects;TArray<int32> Calls;TArray<FCollapsedGraphPlan> GraphPlans;int32 Graph=INDEX_NONE,Entry=INDEX_NONE,Exit=INDEX_NONE;
    int32 GraphPlanIndex(int32 GI) const { for(int32 I=0;I<GraphPlans.Num();++I)if(GraphPlans[I].Graph==GI)return I;return INDEX_NONE; }
    const FCollapsedGraphPlan* GraphForGraph(int32 GI) const { const int32 I=GraphPlanIndex(GI);return I==INDEX_NONE?nullptr:&GraphPlans[I]; }
    const FCollapsedGraphPlan* GraphForComposite(int32 CI) const { for(const auto& G:GraphPlans)if(G.Composite==CI)return &G;return nullptr; }
    bool Bad(FResult& R,const FString& Why) { return Fail(R,TEXT("graph_transfer_collapsed_text_pending"),Why); }
    static FString Unquote(FString S)
    {
        S.TrimStartAndEndInline();
        if(S.Len()>=2&&S[0]==TEXT('"')&&S[S.Len()-1]==TEXT('"'))
        { S=S.Mid(1,S.Len()-2);S.ReplaceInline(TEXT("\\\""),TEXT("\""));S.ReplaceInline(TEXT("\\\\"),TEXT("\\")); }
        return S;
    }
    static bool Pieces(FString S,TArray<FString>& Out)
    {
        S.TrimStartAndEndInline();if(S.Len()<2||S[0]!=TEXT('(')||S[S.Len()-1]!=TEXT(')')) { return false; }
        S=S.Mid(1,S.Len()-2);int32 Depth=0,Start=0;bool Quote=false;
        for(int32 I=0;I<S.Len();++I)
        {
            if(Quote&&S[I]==TEXT('\\')) { ++I;continue; }
            if(S[I]==TEXT('"')) { Quote=!Quote;continue; }if(Quote) { continue; }
            if(S[I]==TEXT('(')) { ++Depth; }else if(S[I]==TEXT(')')) { if(--Depth<0) { return false; } }
            else if(S[I]==TEXT(',')&&Depth==0) { const FString Part=S.Mid(Start,I-Start).TrimStartAndEnd();if(!Part.IsEmpty()) { Out.Add(Part); }Start=I+1; }
        }
        const FString Last=S.Mid(Start).TrimStartAndEnd();if(!Last.IsEmpty()) { Out.Add(Last); }return !Quote&&Depth==0;
    }
    static bool Map(const FString& S,TMap<FString,FString>& Out)
    {
        TArray<FString> Parts;if(!Pieces(S,Parts)||Parts.Num()>64) { return false; }
        for(const FString& P:Parts) { FString K,V;if(!P.Split(TEXT("="),&K,&V)||Out.Contains(K)) { return false; }Out.Add(K,V); }return true;
    }
    static bool Name(const FString& S)
    {
        if(S.IsEmpty()||S.Len()>128) { return false; }for(TCHAR C:S) { if(!FChar::IsAlnum(C)&&C!=TEXT('_')) { return false; } }return true;
    }
    static bool Boolean(const FString& S,bool& B)
    { if(S==TEXT("True")) { B=true;return true; }if(S==TEXT("False")) { B=false;return true; }return false; }
    static bool ScalarDefault(const FString& Category,const FString& Value)
    {
        if(Value.IsEmpty())return true;
        if(Category==TEXT("bool"))return Value==TEXT("true")||Value==TEXT("false");
        int32 Number;return Category==TEXT("int")&&LexTryParseString(Number,*Value)&&LexToString(Number)==Value;
    }
    bool Pin(const FString& S,FCollapsedObjectPlan& O,FResult& R,bool User)
    {
        if((User?O.UserNames.Num():O.Pins.Num())>=128)return Bad(R,TEXT("Finite native pin inventory bound exceeded."));
        TMap<FString,FString> M;if(!Map(S,M)) { return Bad(R,TEXT("Bounded unambiguous pin tuple required.")); }
        const FString N=Unquote(M.FindRef(TEXT("PinName")));if(!Name(N)) { return Bad(R,TEXT("Exact pin name required.")); }
        FString Category,SubCategory;const FString Dir=Unquote(M.FindRef(User?TEXT("DesiredPinDirection"):TEXT("Direction")));
        if(User)
        {
            TMap<FString,FString> Type;if(!Map(M.FindRef(TEXT("PinType")),Type)) { return Bad(R,TEXT("User-defined tunnel pin needs explicit exec type.")); }
            Category=Unquote(Type.FindRef(TEXT("PinCategory")));if(Type.Num()!=1||(Category!=TEXT("exec")&&Category!=TEXT("int")&&Category!=TEXT("bool"))) { return Bad(R,TEXT("Collapsed tunnel ports cover exec/int/bool; other original types remain incomplete.")); }
            for(const auto& P:M) { if(P.Key==TEXT("PinDefaultValue")) { const FString V=Unquote(P.Value);if(ScalarDefault(Category,V)) { continue; } }if(P.Key!=TEXT("PinName")&&P.Key!=TEXT("PinType")&&P.Key!=TEXT("DesiredPinDirection")) { return Bad(R,TEXT("Other tunnel user-pin fields remain incomplete.")); } }
            if(Dir!=TEXT("EGPD_Input")&&Dir!=TEXT("EGPD_Output")) { return Bad(R,TEXT("Explicit user-pin direction required.")); }
            if(O.UserNames.Contains(N)) { return Bad(R,TEXT("Duplicate user pin.")); }O.UserNames.Add(N);O.UserTypes.Add(N,Category);O.UserDirections.Add(N,Dir);O.UserDefaults.Add(N,Unquote(M.FindRef(TEXT("PinDefaultValue"))));return true;
        }
        FCollapsedPinPlan P;P.Name=N;P.Category=Unquote(M.FindRef(TEXT("PinType.PinCategory")));P.SubCategory=Unquote(M.FindRef(TEXT("PinType.PinSubCategory")));
        if(!FGuid::Parse(M.FindRef(TEXT("PinId")),P.Id)||!P.Id.IsValid()||(!Dir.IsEmpty()&&Dir!=TEXT("EGPD_Input")&&Dir!=TEXT("EGPD_Output"))) { return Bad(R,TEXT("Valid source pin GUID/direction required.")); }
        P.Direction=Dir==TEXT("EGPD_Output")?EGPD_Output:EGPD_Input;
        const bool Self=O.Class==TEXT("/Script/BlueprintGraph.K2Node_Self"),Call=O.Class==TEXT("/Script/BlueprintGraph.K2Node_CallFunction");
        const bool StaticSelf=Call&&P.Name==TEXT("self");
        if((Self&&(P.Category!=TEXT("object")||P.SubCategory!=TEXT("self")||P.Name!=TEXT("self")||P.Direction!=EGPD_Output))||(!Self&&!StaticSelf&&((P.Category!=TEXT("exec")&&P.Category!=TEXT("int")&&P.Category!=TEXT("bool"))||!P.SubCategory.IsEmpty()))||(Call&&!StaticSelf&&P.Category==TEXT("exec"))||(StaticSelf&&(P.Category!=TEXT("object")||!P.SubCategory.IsEmpty()||P.Direction!=EGPD_Input))) { return Bad(R,TEXT("Only exact Self, exec/int/bool ports and static pure native math calls are admitted.")); }
        P.Raw=Unquote(M.FindRef(TEXT("DefaultValue")));P.Auto=Unquote(M.FindRef(TEXT("AutogeneratedDefaultValue")));
        for(const auto& Pair:M)
        {
            const FString& K=Pair.Key;const FString V=Unquote(Pair.Value);
            if(K==TEXT("PinId")||K==TEXT("PinName")||K==TEXT("Direction")||K==TEXT("PinType.PinCategory")||K==TEXT("PinType.PinSubCategory")) { continue; }
            if(K==TEXT("LinkedTo"))
            {
                if(Self||!Pieces(Pair.Value,P.Links)||P.Links.Num()>64) { return Bad(R,TEXT("Bounded closed links required; Self stays unlinked.")); }
                TSet<FString> Seen;for(const FString& Link:P.Links){if(Seen.Contains(Link))return Bad(R,TEXT("Duplicate linked peer."));Seen.Add(Link);}
                if(((P.Category==TEXT("exec")&&P.Direction==EGPD_Output)||(P.Category!=TEXT("exec")&&P.Direction==EGPD_Input))&&P.Links.Num()>1)return Bad(R,TEXT("One outgoing execution peer or incoming scalar source is required by stock K2 schema."));continue;
            }
            if(K==TEXT("PersistentGuid")) { FGuid G;if(!FGuid::Parse(V,G)||G.IsValid()) { return Bad(R,TEXT("Persistent user pin GUID variants remain incomplete.")); }continue; }
            if(K==TEXT("DefaultValue")||K==TEXT("AutogeneratedDefaultValue")) { if(ScalarDefault(P.Category,V)) { continue; } }
            if(K==TEXT("PinType.PinSubCategory")&&V.IsEmpty()) { continue; }
            if(K==TEXT("PinType.PinSubCategoryObject")&&V==TEXT("None")) { continue; }
            if(StaticSelf&&K==TEXT("PinType.PinSubCategoryObject")&&(V==TEXT("/Script/CoreUObject.Class'/Script/Engine.KismetMathLibrary'")||V==TEXT("Class'/Script/Engine.KismetMathLibrary'"))) { P.TypeObject=TEXT("/Script/Engine.KismetMathLibrary");continue; }
            if(StaticSelf&&K==TEXT("DefaultObject")&&V==TEXT("/Script/Engine.Default__KismetMathLibrary")) { P.DefaultObject=V;continue; }
            if(StaticSelf&&K==TEXT("bHidden")&&V==TEXT("True")) { P.Hidden=true;continue; }
            if((K==TEXT("PinType.PinSubCategoryMemberReference")||K==TEXT("PinType.PinValueType"))&&Pair.Value==TEXT("()")) { continue; }
            if(K==TEXT("PinType.ContainerType")&&V==TEXT("None")) { continue; }
            if(K==TEXT("bHidden")||K==TEXT("bNotConnectable")||K==TEXT("bDefaultValueIsReadOnly")||K==TEXT("bDefaultValueIsIgnored")||K==TEXT("bAdvancedView")||K==TEXT("bOrphanedPin")||K==TEXT("PinType.bIsReference")||K==TEXT("PinType.bIsConst")||K==TEXT("PinType.bIsWeakPointer")||K==TEXT("PinType.bIsUObjectWrapper")||K==TEXT("PinType.bSerializeAsSinglePrecisionFloat"))
            { bool B;if(Boolean(V,B)&&!B) { continue; } }
            return Bad(R,TEXT("Additional pin defaults, types, text or references remain incomplete for collapsed import."));
        }
        if(StaticSelf&&(P.TypeObject.IsEmpty()||P.DefaultObject.IsEmpty()||!P.Hidden||!P.Links.IsEmpty())) { return Bad(R,TEXT("Static math self pin must name the exact hidden class/CDO role.")); }
        for(const auto& Old:O.Pins) { if(Old.Id==P.Id||Old.Name==P.Name) { return Bad(R,TEXT("Duplicate input pin identity/name.")); } }
        O.Pins.Add(MoveTemp(P));return true;
    }
    bool Parse(const FString& S,FResult& R)
    {
        if(!Text(S,MaxText,R)) { return false; }TArray<int32> Stack;const TCHAR* Stream=*S;FString Line;
        while(FParse::Line(&Stream,Line))
        {
            FString T=Line.TrimStartAndEnd();if(T.IsEmpty()) { continue; }
            if(T.StartsWith(TEXT("Begin Object "),ESearchCase::CaseSensitive))
            {
                FString C,N;FParse::Value(*T,TEXT("Class="),C);FParse::Value(*T,TEXT("Name="),N);N=Unquote(N);
                const FString Canonical=TEXT("Begin Object ")+(C.IsEmpty()?FString():TEXT("Class=")+C+TEXT(" "))+TEXT("Name=\"")+N+TEXT("\"");
                if(!Name(N)||Stack.Num()>16||!T.Equals(Canonical,ESearchCase::CaseSensitive)) { return Bad(R,TEXT("Canonical Class/Name or Name-only headers required; archetype/export-path/header extensions remain incomplete.")); }
                const int32 Parent=Stack.IsEmpty()?INDEX_NONE:Stack.Last();int32 Found=INDEX_NONE;
                for(int32 I=0;I<Objects.Num();++I) { if(Objects[I].Parent==Parent&&Objects[I].Name==N) { Found=I;break; } }
                if(!C.IsEmpty())
                {
                    if(Found!=INDEX_NONE||Objects.Num()>=256) { return Bad(R,TEXT("Duplicate/oversized object declarations.")); }
                    const bool Root=Stack.Num()==0&&Objects.IsEmpty()&&C==TEXT("/Script/BlueprintGraph.K2Node_Composite");
                    const bool Bound=Parent!=INDEX_NONE&&Objects[Parent].Class==TEXT("/Script/BlueprintGraph.K2Node_Composite")&&C==TEXT("/Script/Engine.EdGraph");
                    const bool Inner=Parent!=INDEX_NONE&&Objects[Parent].Class==TEXT("/Script/Engine.EdGraph")&&(C==TEXT("/Script/BlueprintGraph.K2Node_Composite")||C==TEXT("/Script/BlueprintGraph.K2Node_Tunnel")||C==TEXT("/Script/BlueprintGraph.K2Node_Self")||C==TEXT("/Script/BlueprintGraph.K2Node_CallFunction")||C==TEXT("/Script/BlueprintGraph.K2Node_IfThenElse")||C==TEXT("/Script/BlueprintGraph.K2Node_ExecutionSequence"));
                    if(!Root&&!Bound&&!Inner) { return Bad(R,TEXT("One recursively owned Composite tree with at most eight graph levels, 32 graphs and 256 objects; native Tunnel pairs, pure math Call, Branch, Sequence and Self. Custom/mixed top-level bodies remain incomplete.")); }
                    FCollapsedObjectPlan O;O.Name=N;O.Class=C;O.Parent=Parent;if(C==TEXT("/Script/BlueprintGraph.K2Node_Composite")) { O.Inputs=O.Outputs=true; }Found=Objects.Add(MoveTemp(O));if(Bound&&Graph==INDEX_NONE) { Graph=Found; }
                }
                else if(Found==INDEX_NONE) { return Bad(R,TEXT("Name-only property block must reopen its declared local object.")); }
                Stack.Add(Found);continue;
            }
            if(T==TEXT("End Object")) { if(Stack.IsEmpty()) { return Bad(R,TEXT("Unbalanced object text.")); }Stack.Pop();continue; }
            if(Stack.IsEmpty()) { return Bad(R,TEXT("Property outside declared object.")); }auto& O=Objects[Stack.Last()];
            if(T.StartsWith(TEXT("CustomProperties Pin "))) { if(!Pin(T.Mid(21),O,R,false)) { return false; }continue; }
            if(T.StartsWith(TEXT("CustomProperties UserDefinedPin "))) { if(O.Class!=TEXT("/Script/BlueprintGraph.K2Node_Tunnel")||!Pin(T.Mid(32),O,R,true)) { return Bad(R,TEXT("User pins are limited to the two inner stock tunnels.")); }continue; }
            FString K,V;if(!T.Split(TEXT("="),&K,&V)||O.Fields.Contains(K)) { return Bad(R,TEXT("Duplicate/unrecognized object assignment.")); }O.Fields.Add(K,V);
            if(K==TEXT("NodeGuid")||K==TEXT("GraphGuid")) { if(!FGuid::Parse(V,O.Guid)||!O.Guid.IsValid()) { return Bad(R,TEXT("Valid source object GUID required.")); } }
            else if(K==TEXT("NodePosX")||K==TEXT("NodePosY")) { int32 Number;if(!LexTryParseString(Number,*V)) { return Bad(R,TEXT("Integer position required.")); }if(K==TEXT("NodePosX")) { O.X=Number; }else { O.Y=Number; } }
            else if(K==TEXT("NodeComment")) { O.Comment=Unquote(V); }
            else if(K==TEXT("bCanHaveInputs")||K==TEXT("bCanHaveOutputs")) { bool B;if(!Boolean(V,B)) { return Bad(R,TEXT("Tunnel direction boolean required.")); }if(K==TEXT("bCanHaveInputs")) { O.Inputs=B; }else { O.Outputs=B; } }
            else if(K==TEXT("BoundGraph")||K==TEXT("InputSinkNode")||K==TEXT("OutputSourceNode")||K==TEXT("Schema")||K.StartsWith(TEXT("Nodes("))||K.StartsWith(TEXT("SubGraphs("))||K==TEXT("FunctionReference")) { /* validated against this exact tree below */ }
            else if(K==TEXT("bDefaultsToPureFunc")) { bool B;if(O.Class!=TEXT("/Script/BlueprintGraph.K2Node_CallFunction")||!Boolean(V,B)||!B) { return Bad(R,TEXT("Pure native call role required; impure calls remain incomplete.")); } }
            else if(K==TEXT("bIsEditable")||K==TEXT("bEditable")||K==TEXT("bAllowDeletion")||K==TEXT("bAllowRenaming")) { bool B;if(!Boolean(V,B)||!B) { return Bad(R,TEXT("Stock editable tree required.")); } }
            else if(K==TEXT("bCommentBubbleVisible")||K==TEXT("bCommentBubblePinned")) { bool B;if(!Boolean(V,B)||B) { return Bad(R,TEXT("Collapsed comment flag variant remains incomplete.")); } }
            else { return Bad(R,TEXT("Unadmitted object field; no arbitrary reference/member or field import is opened by this profile.")); }
        }
        if(!Stack.IsEmpty()||Objects.Num()<4||Graph==INDEX_NONE) { return Bad(R,TEXT("Complete nonempty collapsed tree required.")); }
        auto RefName=[&](const FString& V,const FString& C,const FString& N)
        { const FString U=Unquote(V);return U==C+TEXT("'")+N+TEXT("'")||U==N; };
        // All declarations are complete before deriving the ownership tree. No TArray
        // element reference crosses an append; source graph/node references stay local.
        for(int32 I=0;I<Objects.Num();++I)
        {
            const auto& O=Objects[I];
            if(!O.Guid.IsValid())return Bad(R,TEXT("Each planned node/graph needs an explicit valid GUID."));
            if(O.Class==TEXT("/Script/Engine.EdGraph"))
            {
                if(GraphPlans.Num()>=32||O.Parent==INDEX_NONE||Objects[O.Parent].Class!=TEXT("/Script/BlueprintGraph.K2Node_Composite"))return Bad(R,TEXT("Bounded graphs must have one local Composite owner."));
                int32 Depth=1,Ancestor=Objects[O.Parent].Parent;
                while(Ancestor!=INDEX_NONE){if(++Depth>8)return Bad(R,TEXT("Collapsed graph depth above eight remains valid incomplete."));Ancestor=Objects[Objects[Ancestor].Parent].Parent;}
                FCollapsedGraphPlan G;G.Graph=I;G.Composite=O.Parent;
                for(int32 J=0;J<Objects.Num();++J)if(Objects[J].Parent==I)G.Nodes.Add(J);
                if(G.Nodes.Num()<2||G.Nodes.Num()>128)return Bad(R,TEXT("Each collapsed graph needs bounded native nodes and complementary tunnels."));
                for(const auto& Existing:GraphPlans)if(Existing.Composite==G.Composite)return Bad(R,TEXT("A Composite cannot own multiple bound graphs."));
                GraphPlans.Add(MoveTemp(G));
            }
        }
        if(GraphPlans.IsEmpty()||GraphPlans[0].Graph!=Graph||GraphPlans[0].Composite!=0)return Bad(R,TEXT("One complete top-level collapsed tree required."));
        for(int32 I=0;I<Objects.Num();++I)
        {
            const auto& O=Objects[I];
            if(O.Class==TEXT("/Script/Engine.EdGraph"))
            {
                // EdGraph.Schema is a class slot, not a schema CDO object slot.
                // Retain the historical single-graph alias only where the stock
                // outer PostPaste repair was already observed. In a nested tree
                // every graph must carry a real class reference before any SDK call.
                const FString SchemaReference=Unquote(O.Fields.FindRef(TEXT("Schema")));
                const bool ClassSchema=SchemaReference==TEXT("/Script/CoreUObject.Class'/Script/BlueprintGraph.EdGraphSchema_K2'")||SchemaReference==TEXT("Class'/Script/BlueprintGraph.EdGraphSchema_K2'");
                const bool HistoricalSingleAlias=GraphPlans.Num()==1&&RefName(O.Fields.FindRef(TEXT("Schema")),TEXT("/Script/BlueprintGraph.EdGraphSchema_K2"),TEXT("/Script/BlueprintGraph.Default__EdGraphSchema_K2"));
                if(!ClassSchema&&!HistoricalSingleAlias)return Bad(R,TEXT("Every nested graph Schema must reference the stock K2 UClass; a schema CDO object is not a class and is rejected before import."));
                continue;
            }
            if(O.Class==TEXT("/Script/BlueprintGraph.K2Node_Composite"))
            {
                if(!GraphForComposite(I))return Bad(R,TEXT("Every Composite requires its declared bound graph."));
                continue;
            }
            const int32 GI=GraphPlanIndex(O.Parent);if(GI==INDEX_NONE)return Bad(R,TEXT("Inner node must be owned by a declared graph."));
            if(O.Class==TEXT("/Script/BlueprintGraph.K2Node_Tunnel"))
            {
                if(O.Inputs==O.Outputs||O.Pins.Num()>64||O.UserNames.Num()!=O.Pins.Num())return Bad(R,TEXT("Two complementary bounded exec/int/bool tunnels required per graph."));
                for(const auto& P:O.Pins)if(!O.UserNames.Contains(P.Name)||O.UserTypes.FindRef(P.Name)!=P.Category||O.UserDirections.FindRef(P.Name)!=(O.Outputs?TEXT("EGPD_Output"):TEXT("EGPD_Input"))||P.Direction!=(O.Outputs?EGPD_Output:EGPD_Input))return Bad(R,TEXT("Tunnel user definitions and actual ports must agree."));
                auto& G=GraphPlans[GI];int32& Slot=O.Outputs?G.Entry:G.Exit;
                if(Slot!=INDEX_NONE)return Bad(R,TEXT("Unique entry and exit tunnel required in each graph."));Slot=I;
            }
            else if(O.Class==TEXT("/Script/BlueprintGraph.K2Node_CallFunction"))
            {
                if(O.Pins.Num()<1||O.Pins.Num()>17)return Bad(R,TEXT("Bounded native by-value scalar signature required."));
                TMap<FString,FString> Ref;if(!Map(O.Fields.FindRef(TEXT("FunctionReference")),Ref)||Ref.Num()!=2||!Name(Unquote(Ref.FindRef(TEXT("MemberName")))))return Bad(R,TEXT("Exact external native function reference required."));
                const FString MP=Unquote(Ref.FindRef(TEXT("MemberParent")));
                if(MP!=TEXT("/Script/CoreUObject.Class'/Script/Engine.KismetMathLibrary'")&&MP!=TEXT("Class'/Script/Engine.KismetMathLibrary'"))return Bad(R,TEXT("Loaded native KismetMathLibrary source required; other original sources remain incomplete."));
                Objects[I].MemberName=Unquote(Ref.FindRef(TEXT("MemberName")));Calls.Add(I);
            }
            else if(O.Class==TEXT("/Script/BlueprintGraph.K2Node_IfThenElse"))
            {
                if(O.Pins.Num()!=4)return Bad(R,TEXT("Stock Branch signature required."));
                for(const auto& P:O.Pins){const bool Condition=P.Name==TEXT("Condition");if((!Condition&&P.Name!=TEXT("execute")&&P.Name!=TEXT("then")&&P.Name!=TEXT("else"))||P.Category!=(Condition?TEXT("bool"):TEXT("exec"))||P.Direction!=((Condition||P.Name==TEXT("execute"))?EGPD_Input:EGPD_Output))return Bad(R,TEXT("Stock Branch named ports required."));}
            }
            else if(O.Class==TEXT("/Script/BlueprintGraph.K2Node_ExecutionSequence"))
            {
                if(O.Pins.Num()<3||O.Pins.Num()>65)return Bad(R,TEXT("Stock Sequence execution input and bounded ordered outputs required."));
                for(int32 J=0;J<O.Pins.Num();++J){const auto& P=O.Pins[J];if(P.Category!=TEXT("exec")||P.Direction!=(J==0?EGPD_Input:EGPD_Output)||P.Name!=(J==0?FString(TEXT("execute")):FString(TEXT("then_"))+LexToString(J-1)))return Bad(R,TEXT("Sequence reconstruction uses contiguous ordered then_N names."));}
            }
            else if(O.Pins.Num()!=1||!O.Pins[0].Links.IsEmpty())return Bad(R,TEXT("Inner Self must have one unlinked self output."));
        }
        for(auto& G:GraphPlans)
        {
            if(G.Entry==INDEX_NONE||G.Exit==INDEX_NONE)return Bad(R,TEXT("Every graph needs complementary entry/exit tunnels."));
            const auto& C=Objects[G.Composite];
            if(C.Pins.Num()!=Objects[G.Entry].Pins.Num()+Objects[G.Exit].Pins.Num()||!RefName(C.Fields.FindRef(TEXT("BoundGraph")),TEXT("/Script/Engine.EdGraph"),Objects[G.Graph].Name))return Bad(R,TEXT("Composite binding and matching local tunnel roles required."));
            for(const auto& P:C.Pins)
            {
                const auto& Port=Objects[P.Direction==EGPD_Input?G.Entry:G.Exit];bool Found=false;
                for(const auto& Q:Port.Pins)Found|=P.Name==Q.Name&&P.Category==Q.Category;
                if(!Found||(G.Composite==0&&!P.Links.IsEmpty()))return Bad(R,TEXT("Composite ports must match complementary local tunnels; only the top-level Composite stays externally unlinked."));
            }
            TMap<FString,const FCollapsedPinPlan*> LocalPins;
            for(int32 NI:G.Nodes)for(const auto& P:Objects[NI].Pins)
            {
                const FString K=Objects[NI].Name+TEXT(" ")+P.Id.ToString(EGuidFormats::Digits);
                if(LocalPins.Contains(K))return Bad(R,TEXT("Duplicate pin identity in the same graph."));LocalPins.Add(K,&P);
            }
            for(int32 NI:G.Nodes)for(const auto& P:Objects[NI].Pins)for(const FString& L:P.Links)
            {
                const auto* Peer=LocalPins.Find(L);const FString Back=Objects[NI].Name+TEXT(" ")+P.Id.ToString(EGuidFormats::Digits);
                if(!Peer||(*Peer)->Direction==P.Direction||(*Peer)->Category!=P.Category||!(*Peer)->Links.Contains(Back))return Bad(R,TEXT("Each graph's links must be closed, reciprocal and type-matched; cross-graph raw links are not tunnel connections."));
            }
            int32 NodeRows=0,SubRows=0;
            for(const auto& F:Objects[G.Graph].Fields){NodeRows+=F.Key.StartsWith(TEXT("Nodes("))?1:0;SubRows+=F.Key.StartsWith(TEXT("SubGraphs("))?1:0;}
            if(NodeRows!=G.Nodes.Num())return Bad(R,TEXT("Exact ordered graph Nodes inventory required."));
            for(int32 J=0;J<G.Nodes.Num();++J)
            {
                const auto& O=Objects[G.Nodes[J]];const FString K=TEXT("Nodes(")+LexToString(J)+TEXT(")");
                if(!RefName(Objects[G.Graph].Fields.FindRef(K),O.Class,O.Name))return Bad(R,TEXT("Graph Nodes array must match declared direct-child node order."));
                if(const auto* Child=GraphForComposite(G.Nodes[J]))G.Subs.Add(Child->Graph);
            }
            // Exported SubGraphs is optional: PostPaste adds absent nested graphs.
            // When present its authored order remains authoritative, not a new baseline.
            if(SubRows)
            {
                if(SubRows!=G.Subs.Num())return Bad(R,TEXT("Serialized SubGraphs must cover the exact direct nested graph set."));
                TArray<int32> Ordered;
                for(int32 J=0;J<SubRows;++J)
                {
                    const FString V=Objects[G.Graph].Fields.FindRef(TEXT("SubGraphs(")+LexToString(J)+TEXT(")"));int32 Found=INDEX_NONE;
                    for(int32 SI:G.Subs){const auto& SO=Objects[SI];const FString Qualified=Objects[SO.Parent].Name+TEXT(".")+SO.Name;if(RefName(V,SO.Class,Qualified)||RefName(V,SO.Class,SO.Name)){if(Found!=INDEX_NONE)return Bad(R,TEXT("Ambiguous local SubGraphs reference."));Found=SI;}}
                    if(Found==INDEX_NONE||Ordered.Contains(Found))return Bad(R,TEXT("SubGraphs reference escapes/duplicates the owned tree."));Ordered.Add(Found);
                }
                G.Subs=MoveTemp(Ordered);G.SubOrderAuthored=true;
            }
        }
        Entry=GraphPlans[0].Entry;Exit=GraphPlans[0].Exit;
        for(int32 I=0;I<Objects.Num();++I)for(const auto& F:Objects[I].Fields)
        {
            if(F.Key==TEXT("InputSinkNode")||F.Key==TEXT("OutputSourceNode"))
            {
                const auto& O=Objects[I];const auto* Owned=GraphForComposite(I);const auto* Enclosing=GraphForGraph(O.Parent);
                const int32 Expected=Owned?(F.Key==TEXT("InputSinkNode")?Owned->Entry:Owned->Exit):(Enclosing?Enclosing->Composite:INDEX_NONE);
                if((!Owned&&O.Class!=TEXT("/Script/BlueprintGraph.K2Node_Tunnel"))||Expected==INDEX_NONE)return Bad(R,TEXT("Tunnel reference has no planned local owner."));
                if(!RefName(F.Value,Objects[Expected].Class,Objects[Expected].Name)&&F.Value!=TEXT("None"))return Bad(R,TEXT("Tunnel reference must stay in its local Composite tree."));
            }
            if((F.Key.StartsWith(TEXT("Nodes("))||F.Key.StartsWith(TEXT("SubGraphs("))||F.Key==TEXT("Schema"))&&Objects[I].Class!=TEXT("/Script/Engine.EdGraph"))return Bad(R,TEXT("Graph inventory fields require an actual local graph owner."));
            if(F.Key==TEXT("BoundGraph")&&Objects[I].Class!=TEXT("/Script/BlueprintGraph.K2Node_Composite"))return Bad(R,TEXT("BoundGraph requires a Composite owner."));
        }
        TSet<FGuid> Guids;for(const auto& O:Objects){if(Guids.Contains(O.Guid))return Bad(R,TEXT("Distinct source object GUIDs required."));Guids.Add(O.Guid);}
        return true;
    }
};

bool CollapsedTypeSame(const FEdGraphPinType& A,const FEdGraphPinType& B)
{
    return A==B&&A.bIsUObjectWrapper==B.bIsUObjectWrapper&&A.bSerializeAsSinglePrecisionFloat==B.bSerializeAsSinglePrecisionFloat
        &&A.PinValueType.bTerminalIsUObjectWrapper==B.PinValueType.bTerminalIsUObjectWrapper;
}
// A concrete target lease. Only these observed fields are claimed, not arbitrary inherited UObject data.
struct FCollapsedTarget
{
    struct FPin
    {
        UEdGraphPin* Address=nullptr;FGuid Id;FName Name;EEdGraphPinDirection Direction;FEdGraphPinType Type;
        FString Raw,Auto,TextDisplay,FriendlyDisplay;FText Text,Friendly;UObject* Default=nullptr;TArray<UEdGraphPin*> Links;
        TArray<FString> LinkIds;uint32 Bits=0;
    };
    struct FNode
    {
        UEdGraphNode* Object=nullptr;UEdGraph* Graph=nullptr;UClass* Class=nullptr;FGuid Guid;FString Path,Comment;
        int32 X=0,Y=0;bool Visible=false,Pinned=false;TArray<FPin> Pins;FMemberReference Member;
        UK2Node_Tunnel* Input=nullptr;UK2Node_Tunnel* Output=nullptr;UEdGraph* BoundGraph=nullptr;bool Tunnel=false,CanInputs=false,CanOutputs=false;
        TArray<FUserPinInfo> UserPins;
    };
    struct FGraph { UEdGraph* Object=nullptr;UObject* Outer=nullptr;FString Path;FGuid Guid;bool Editable=false,Deletion=false,Renaming=false;TArray<UEdGraphNode*> Nodes;TArray<UEdGraph*> Subs; };
    FResult& R;TStrongObjectPtr<UBlueprint> BP;TStrongObjectPtr<UPackage> Package;
    TArray<TStrongObjectPtr<UObject>> Holds;TArray<FGraph> Graphs;TArray<FNode> Nodes;
    TArray<TObjectPtr<UEdGraph>> Functions,Macros,Uber,Delegates,Events;
    TMap<UEdGraph*,FString> EventPaths;
    TArray<FBPVariableDescription> VariablesBefore;TArray<FBPInterfaceDescription> InterfacesBefore;
    TStrongObjectPtr<UClass> Generated,Skeleton;UClass* Parent=nullptr;FString BPPath,PackagePath,GeneratedPath,SkeletonPath;
    TStrongObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;
    TStrongObjectPtr<UClass> MathClass;TStrongObjectPtr<UObject> MathCDO;
    struct FMathField
    {
        FProperty* P=nullptr;const FFieldClass* Class=nullptr;FName Name;FString Category;int32 Offset=0,Size=0,Dim=0;uint64 Flags=0;
        uint8 BoolSize=0,BoolOffset=0,BoolByteMask=0,BoolFieldMask=0;TMap<FName,FString> Meta;
    };
    struct FMathSource
    {
        UFunction* Function=nullptr;FName Name;FString Path;EFunctionFlags Flags=FUNC_None;uint8 NumParms=0;uint16 ParmsSize=0,ReturnOffset=0;
        TArray<FMathField> Fields;FMemberReference Reference;TArray<FName> MetaKeys;TMap<FName,FString> Meta;TMap<FName,bool> MetaHas;bool MetadataReady=false;
    };
    TArray<FMathSource> MathSources;
    USimpleConstructionScript* SCS=nullptr;TArray<USCS_Node*> SCSNodes;TArray<USCS_Node*> SCSRoots;
    USCS_Node* RootNode=nullptr;USceneComponent* RootTemplate=nullptr;FGuid RootGuid;FName RootName;FTransform RootRelative;EComponentMobility::Type RootMobility;
    UEdGraph* Selected=nullptr;UK2Node_Composite* Composite=nullptr;UEdGraph* Bound=nullptr;
    UClass* ReturnedEditorClass=nullptr;UClass* ReturnedWorldClass=nullptr;
    bool Stopped=false,Acquired=false,Rebinding=false;TArray<FNode> Candidate;TArray<UEdGraphNode*> ExpectedAdded;
    explicit FCollapsedTarget(FResult& In):R(In) {}
    void Hold(UObject* O) { if(O) { Holds.Emplace(O); } }
    bool Bad(const TCHAR* Why,const FString& Object=FString())
    {
        if(!Stopped)
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("reason"),Why);J->SetStringField(TEXT("object"),Object);J->SetBoolField(TEXT("operand_values_observed"),false);
            R.Data->SetObjectField(TEXT("first_current_failure"),J);
            Fail(R,TEXT("graph_transfer_collapsed_current_lost"),Why);
        }
        Stopped=true;R.bChangeStateKnown=false;return false;
    }
    static uint32 Bits(const UEdGraphPin* P)
    {
        return uint32(P->bHidden)|(uint32(P->bNotConnectable)<<1)|(uint32(P->bDefaultValueIsReadOnly)<<2)|(uint32(P->bDefaultValueIsIgnored)<<3)
            |(uint32(P->bAdvancedView)<<4)|(uint32(P->bOrphanedPin)<<5)|(uint32(P->bDisplayAsMutableRef)<<6);
    }
    static FString LinkId(UEdGraphPin* P)
    { return P->GetOwningNodeUnchecked()->NodeGuid.ToString()+TEXT(":")+P->PinId.ToString(); }
    bool ReturnedContext()
    {
        if(Stopped)return false;
        if(!IsInGameThread()||IsGarbageCollecting()||GIsSavingPackage||IsEngineExitRequested())return Bad(TEXT("returned_context_phase"));
        if(!Loaded(Editor.Get())||!Loaded(World.Get())||GEditor!=Editor.Get()||GWorld!=World.Get()
            ||Editor->GetClass()!=ReturnedEditorClass||World->GetClass()!=ReturnedWorldClass)return Bad(TEXT("returned_editor_world_role"));
        if(!Editor->PlayWorld.IsResolved()||Editor->PlayWorld.Get()!=nullptr||IsPlaySessionActive())return Bad(TEXT("returned_play_context"));
        if(!Loaded(BP.Get())||BP->GetClass()!=UBlueprint::StaticClass()||!Loaded(Package.Get())||BP->GetOuter()!=Package.Get()
            ||!BP->GetPathName().Equals(BPPath,ESearchCase::CaseSensitive)||!Package->GetName().Equals(PackagePath,ESearchCase::CaseSensitive))return Bad(TEXT("returned_blueprint_owner_role"));
        const FGraph* Captured=Graphs.FindByPredicate([&](const FGraph& G){return G.Object==Selected;});
        if(!Captured||!Loaded(Selected)||Selected->GetClass()!=UEdGraph::StaticClass()||Selected->GetOuter()!=Captured->Outer
            ||!Selected->GetPathName().Equals(Captured->Path,ESearchCase::CaseSensitive)||Selected->GraphGuid!=Captured->Guid)return Bad(TEXT("returned_selected_graph_role"));
        return true;
    }
    bool PinCopy(UEdGraphNode* N,TArray<FPin>& Out)
    {
        if(N->Pins.Num()>128) { return Bad(TEXT("pin_bound")); }TSet<UEdGraphPin*> Unique;
        for(UEdGraphPin* P:N->Pins)
        {
            if(!P||Unique.Contains(P)||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||P->ParentPin||!P->SubPins.IsEmpty()||P->IsSnapped()
                ||!P->DefaultObject.IsResolved()) { return Bad(TEXT("owning_pin_role")); }
            Unique.Add(P);FPin V;V.Address=P;V.Id=P->PinId;V.Name=P->PinName;V.Direction=P->Direction;V.Type=P->PinType;V.Raw=P->DefaultValue;V.Auto=P->AutogeneratedDefaultValue;
            V.Text=P->DefaultTextValue;V.Friendly=P->PinFriendlyName;V.Default=P->DefaultObject.Get();Hold(V.Default);Hold(V.Type.PinSubCategoryObject.Get());Hold(V.Type.PinValueType.TerminalSubCategoryObject.Get());
            V.Links=P->LinkedTo;V.Bits=Bits(P);Out.Add(MoveTemp(V));
        }
        return true;
    }
    bool NodeCopy(UEdGraphNode* N,FNode& V)
    {
        if(!Loaded(N)||!N->GetClass()->HasAnyClassFlags(CLASS_Native)||N->GetClass()->GetOutermost()->GetName()!=TEXT("/Script/BlueprintGraph")) { return Bad(TEXT("native_node_role")); }
        Hold(N);Hold(N->GetClass());V.Object=N;V.Graph=N->GetGraph();V.Class=N->GetClass();V.Guid=N->NodeGuid;V.Path=N->GetPathName();V.Comment=N->NodeComment;V.X=N->NodePosX;V.Y=N->NodePosY;
        V.Visible=N->bCommentBubbleVisible;V.Pinned=N->bCommentBubblePinned;
        if(auto* Call=Cast<UK2Node_CallFunction>(N)) { V.Member=Call->FunctionReference; }
        if(auto* Tunnel=Cast<UK2Node_Tunnel>(N))
        {
            if(!Tunnel->InputSinkNode.IsResolved()||!Tunnel->OutputSourceNode.IsResolved()) { return Bad(TEXT("tunnel_slot_unresolved")); }
            V.Tunnel=true;V.Input=Tunnel->InputSinkNode.Get();V.Output=Tunnel->OutputSourceNode.Get();Hold(V.Input);Hold(V.Output);V.CanInputs=Tunnel->bCanHaveInputs;V.CanOutputs=Tunnel->bCanHaveOutputs;
            for(const auto& P:Tunnel->UserDefinedPins) { if(!P) { return Bad(TEXT("null_user_pin")); }V.UserPins.Add(*P); }
        }
        if(N->GetClass()==UK2Node_Composite::StaticClass())
        {
            const auto* C=static_cast<UK2Node_Composite*>(N);if(!C->BoundGraph.IsResolved())return Bad(TEXT("composite_bound_slot_unresolved"));
            V.BoundGraph=C->BoundGraph.Get();Hold(V.BoundGraph);
        }
        return PinCopy(N,V.Pins);
    }
    bool Classes(bool Reacquire)
    {
        auto G=BP->GeneratedClass;auto S=BP->SkeletonGeneratedClass;
        if(!G.GetGCPtr().IsResolved()||!S.GetGCPtr().IsResolved()) { return Bad(TEXT("class_slot_unresolved")); }
        UClass* GC=G.GetGCPtr().Get();UClass* SK=S.GetGCPtr().Get();const TStrongObjectPtr<UClass> GH(GC),SH(SK);
        for(UClass* C:{GC,SK}) { if(!Loaded(C)||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy.Get()!=BP.Get()||C->GetOuter()!=Package.Get()||C->GetSuperClass()!=Parent||C->GetClass()!=UBlueprintGeneratedClass::StaticClass()) { return Bad(TEXT("generated_class_role")); } }
        if(GeneratedPath.IsEmpty()) { GeneratedPath=GC->GetPathName();SkeletonPath=SK->GetPathName(); }
        if(!GC->GetPathName().Equals(GeneratedPath,ESearchCase::CaseSensitive)||!SK->GetPathName().Equals(SkeletonPath,ESearchCase::CaseSensitive)) { return Bad(TEXT("generated_class_path")); }
        if(Reacquire) { Generated.Reset(GC);Skeleton.Reset(SK); }
        return (GC==Generated.Get()&&SK==Skeleton.Get())||Bad(TEXT("class_slot_changed"));
    }
    bool PinCurrent(const FNode& V,const TArray<FPin>& Pins)
    {
        auto* N=V.Object;if(N->Pins.Num()!=Pins.Num()) { return Bad(TEXT("pin_inventory"),V.Path); }
        for(int32 I=0;I<Pins.Num();++I)
        {
            const auto& E=Pins[I];if(N->Pins[I]!=E.Address) { return Bad(TEXT("pin_slot"),V.Path); }const auto* P=E.Address;
            if(P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||P->PinId!=E.Id||P->PinName!=E.Name||P->Direction!=E.Direction||!CollapsedTypeSame(P->PinType,E.Type)
                ||P->ParentPin||!P->SubPins.IsEmpty()||P->IsSnapped()||Bits(P)!=E.Bits||!P->DefaultValue.Equals(E.Raw,ESearchCase::CaseSensitive)||!P->AutogeneratedDefaultValue.Equals(E.Auto,ESearchCase::CaseSensitive)
                ||!P->DefaultObject.IsResolved()||P->DefaultObject.Get()!=E.Default||P->LinkedTo!=E.Links
                ||FTextInspector::GetSharedDataId(P->DefaultTextValue)!=FTextInspector::GetSharedDataId(E.Text)||FTextInspector::GetFlags(P->DefaultTextValue)!=FTextInspector::GetFlags(E.Text)
                ||FTextInspector::GetSharedDataId(P->PinFriendlyName)!=FTextInspector::GetSharedDataId(E.Friendly)||FTextInspector::GetFlags(P->PinFriendlyName)!=FTextInspector::GetFlags(E.Friendly)) { return Bad(TEXT("pin_value_or_link"),V.Path); }
        }
        return true;
    }
    bool Plain()
    {
        if(Stopped) { return false; }
        if(!IsEditorGameThread()||IsPlaySessionActive()||GEditor!=Editor.Get()||GWorld!=World.Get()||!Loaded(Editor.Get())||!Loaded(World.Get())) { return Bad(TEXT("editor_world_context")); }
        if(!Loaded(BP.Get())) { return Bad(TEXT("blueprint_unavailable")); }
        auto ParentSlot=BP->ParentClass;
        if(!Loaded(BP.Get())||!BP->GetPathName().Equals(BPPath,ESearchCase::CaseSensitive)||BP->GetOutermost()!=Package.Get()||!Package->GetName().Equals(PackagePath,ESearchCase::CaseSensitive)||!ParentSlot.GetGCPtr().IsResolved()||ParentSlot.GetGCPtr().Get()!=Parent||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bIsRegeneratingOnLoad
            ||BP->FunctionGraphs!=Functions||BP->MacroGraphs!=Macros||BP->UbergraphPages!=Uber||BP->DelegateSignatureGraphs!=Delegates) { return Bad(TEXT("blueprint_role"),BPPath); }
        if(!EventsCurrent()||!ClearRootsCurrent()) { return false; }
        if(!Rebinding&&!Classes(false)) { return false; }
        if(BP->NewVariables.Num()!=VariablesBefore.Num()||BP->ImplementedInterfaces.Num()!=InterfacesBefore.Num()) { return Bad(TEXT("declaration_inventory"),BPPath); }
        for(int32 I=0;I<VariablesBefore.Num();++I)
        {
            const auto& A=BP->NewVariables[I];const auto& E=VariablesBefore[I];
            if(A.VarName!=E.VarName||A.VarGuid!=E.VarGuid||!CollapsedTypeSame(A.VarType,E.VarType)||A.PropertyFlags!=E.PropertyFlags||!A.DefaultValue.Equals(E.DefaultValue,ESearchCase::CaseSensitive)
                ||!A.FriendlyName.Equals(E.FriendlyName,ESearchCase::CaseSensitive)||FTextInspector::GetSharedDataId(A.Category)!=FTextInspector::GetSharedDataId(E.Category)||FTextInspector::GetFlags(A.Category)!=FTextInspector::GetFlags(E.Category)||A.MetaDataArray.Num()!=E.MetaDataArray.Num()) { return Bad(TEXT("declaration_value"),BPPath); }
            for(int32 K=0;K<A.MetaDataArray.Num();++K) { if(A.MetaDataArray[K].DataKey!=E.MetaDataArray[K].DataKey||!A.MetaDataArray[K].DataValue.Equals(E.MetaDataArray[K].DataValue,ESearchCase::CaseSensitive)) { return Bad(TEXT("declaration_metadata")); } }
        }
        for(int32 I=0;I<InterfacesBefore.Num();++I)
        {
            auto A=BP->ImplementedInterfaces[I].Interface;auto E=InterfacesBefore[I].Interface;
            if(!A.GetGCPtr().IsResolved()||!E.GetGCPtr().IsResolved()||A.GetGCPtr().Get()!=E.GetGCPtr().Get()||BP->ImplementedInterfaces[I].Graphs!=InterfacesBefore[I].Graphs) { return Bad(TEXT("interface_role")); }
        }
        if(!BP->SimpleConstructionScript.IsResolved()||BP->SimpleConstructionScript.Get()!=SCS||!Loaded(SCS)||SCS->GetAllNodes()!=SCSNodes||SCS->GetRootNodes()!=SCSRoots||!Loaded(RootNode)||!RootNode->ComponentTemplate.IsResolved()||RootNode->ComponentTemplate.Get()!=RootTemplate
            ||RootNode->VariableGuid!=RootGuid||RootNode->GetVariableName()!=RootName||!RootNode->GetChildNodes().IsEmpty()||!Loaded(RootTemplate)||RootTemplate->GetAttachParent()||!RootTemplate->GetAttachChildren().IsEmpty()
            ||!RootTemplate->GetRelativeTransform().Equals(RootRelative,0.f)||RootTemplate->Mobility!=RootMobility) { return Bad(TEXT("scs_observed_root")); }
        for(const auto& G:Graphs)
        {
            if(!Loaded(G.Object)||G.Object->GetClass()!=UEdGraph::StaticClass()||G.Object->GetOuter()!=G.Outer||!G.Object->GetPathName().Equals(G.Path,ESearchCase::CaseSensitive)||G.Object->GraphGuid!=G.Guid) { return Bad(TEXT("graph_role"),G.Path); }
            auto Schema=G.Object->Schema;if(!Schema.GetGCPtr().IsResolved()||Schema.GetGCPtr().Get()!=UEdGraphSchema_K2::StaticClass()||bool(G.Object->bEditable)!=G.Editable||bool(G.Object->bAllowDeletion)!=G.Deletion||bool(G.Object->bAllowRenaming)!=G.Renaming) { return Bad(TEXT("graph_schema_binding"),G.Path); }
            TArray<UEdGraphNode*> E=G.Nodes;if(G.Object==Selected) { E.Append(ExpectedAdded); }
            TArray<UEdGraph*> ES=G.Subs;if(G.Object==Selected&&Acquired) { ES.Add(Bound); }
            if(G.Object->Nodes.Num()!=E.Num()||G.Object->SubGraphs.Num()!=ES.Num()) { return Bad(TEXT("graph_inventory"),G.Path); }
            for(int32 I=0;I<E.Num();++I) { if(!G.Object->Nodes[I].IsResolved()||G.Object->Nodes[I].Get()!=E[I]) { return Bad(TEXT("graph_node_order"),G.Path); } }
            for(int32 I=0;I<ES.Num();++I) { if(!G.Object->SubGraphs[I].IsResolved()||G.Object->SubGraphs[I].Get()!=ES[I]) { return Bad(TEXT("subgraph_order"),G.Path); } }
        }
        const auto& Values=Rebinding?Candidate:Nodes;
        for(const auto& V:Values)
        {
            auto* N=V.Object;if(!Loaded(N)||N->GetGraph()!=V.Graph||N->GetClass()!=V.Class||N->NodeGuid!=V.Guid||!N->GetPathName().Equals(V.Path,ESearchCase::CaseSensitive)||N->NodePosX!=V.X||N->NodePosY!=V.Y||!N->NodeComment.Equals(V.Comment,ESearchCase::CaseSensitive)||N->bCommentBubbleVisible!=V.Visible||N->bCommentBubblePinned!=V.Pinned||!PinCurrent(V,V.Pins)) { return Bad(TEXT("node_observed_state"),V.Path); }
            if(auto* Call=Cast<UK2Node_CallFunction>(N)) { if(!Call->FunctionReference.IsSameReference(V.Member)) { return Bad(TEXT("call_member_binding"),V.Path); } }
            if(V.BoundGraph)
            {
                if(N->GetClass()!=UK2Node_Composite::StaticClass())return Bad(TEXT("captured_composite_class"));
                auto* C=static_cast<UK2Node_Composite*>(N);if(!C->BoundGraph.IsResolved()||C->BoundGraph.Get()!=V.BoundGraph)return Bad(TEXT("captured_composite_bound"));
            }
            if(V.Tunnel)
            {
                auto* Tunnel=static_cast<UK2Node_Tunnel*>(N);if(!Tunnel->InputSinkNode.IsResolved()||!Tunnel->OutputSourceNode.IsResolved()||Tunnel->InputSinkNode.Get()!=V.Input||Tunnel->OutputSourceNode.Get()!=V.Output||bool(Tunnel->bCanHaveInputs)!=V.CanInputs||bool(Tunnel->bCanHaveOutputs)!=V.CanOutputs||Tunnel->UserDefinedPins.Num()!=V.UserPins.Num()) { return Bad(TEXT("tunnel_current"),V.Path); }
                for(int32 I=0;I<V.UserPins.Num();++I) { const auto& P=Tunnel->UserDefinedPins[I];const auto& E=V.UserPins[I];if(!P||P->PinName!=E.PinName||!CollapsedTypeSame(P->PinType,E.PinType)||P->DesiredPinDirection!=E.DesiredPinDirection||!P->PinDefaultValue.Equals(E.PinDefaultValue,ESearchCase::CaseSensitive)) { return Bad(TEXT("tunnel_user_pin"),V.Path); } }
            }
        }
        if(Acquired)
        {
            if(!Loaded(Composite)||!Composite->BoundGraph.IsResolved()||Composite->BoundGraph.Get()!=Bound||!Loaded(Bound)||Bound->GetOuter()!=Composite) { return Bad(TEXT("collapsed_binding")); }
        }
        return MathCurrent();
    }
    bool EventsCurrent()
    {
        if(BP->EventGraphs!=Events) { return Bad(TEXT("compiler_event_stub_inventory")); }
        for(const auto& Slot:BP->EventGraphs)
        {
            if(!Slot.IsResolved()) { return Bad(TEXT("compiler_event_stub_unresolved")); }UEdGraph* G=Slot.Get();
            if(!Loaded(G)||G->GetClass()!=UEdGraph::StaticClass()||G->GetOuter()!=BP.Get()||!G->HasAnyFlags(RF_Transient)||!G->GetPathName().Equals(EventPaths.FindRef(G),ESearchCase::CaseSensitive)) { return Bad(TEXT("compiler_event_stub_role")); }
            auto Schema=G->Schema;if(!Schema.GetGCPtr().IsResolved()||Schema.GetGCPtr().Get()!=UEdGraphSchema_K2::StaticClass()) { return Bad(TEXT("compiler_event_stub_schema")); }
        }
        return true;
    }
    bool ReacquireEvents()
    {
        // KismetCompiler.cpp4119 retires these transient stubs; 3656 creates replacements under this Blueprint.
        // This physical compiler-owned array is separate from all protected authored graph arrays above.
        if(Stopped||!Loaded(BP.Get())||!IsEditorGameThread()||IsPlaySessionActive()||GEditor!=Editor.Get()||GWorld!=World.Get()) { return Bad(TEXT("compiler_event_stub_context")); }
        const auto Now=BP->EventGraphs;if(Now.Num()>MaxGraphs) { return Bad(TEXT("compiler_event_stub_bound")); }
        for(const auto& Slot:Now) { if(!Slot.IsResolved()) { return Bad(TEXT("compiler_event_stub_unresolved")); }Hold(Slot.Get()); }
        TMap<UEdGraph*,FString> Paths;
        for(const auto& Slot:Now)
        {
            UEdGraph* G=Slot.Get();if(!Loaded(G)||G->GetClass()!=UEdGraph::StaticClass()||G->GetOuter()!=BP.Get()||!G->HasAnyFlags(RF_Transient)||Paths.Contains(G)) { return Bad(TEXT("compiler_event_stub_returned_role")); }Paths.Add(G,G->GetPathName());
            auto Schema=G->Schema;if(!Schema.GetGCPtr().IsResolved()||Schema.GetGCPtr().Get()!=UEdGraphSchema_K2::StaticClass()) { return Bad(TEXT("compiler_event_stub_returned_schema")); }
        }
        Events=Now;EventPaths=MoveTemp(Paths);return EventsCurrent();
    }
    bool MathCurrent()
    {
        if(!MathClass)return MathSources.IsEmpty()||Bad(TEXT("math_missing_class"));
        UClass* C=MathClass.Get();
        if(!Loaded(C)||C->GetClass()!=UClass::StaticClass()||!C->GetPathName().Equals(TEXT("/Script/Engine.KismetMathLibrary"),ESearchCase::CaseSensitive)||!C->HasAnyClassFlags(CLASS_Native)
            ||!C->ClassDefaultObject.IsResolved()||C->ClassDefaultObject.Get()!=MathCDO.Get()||!Loaded(MathCDO.Get())||MathCDO->GetClass()!=C||MathCDO->GetOuter()!=C->GetOuter()||!MathCDO->HasAnyFlags(RF_ClassDefaultObject))return Bad(TEXT("math_source_class_cdo"));
        for(const auto& M:MathSources)
        {
            UFunction* F=M.Function;
            if(!Loaded(F)||F->GetClass()!=UFunction::StaticClass()||F->GetOuter()!=C||F->GetFName()!=M.Name||!F->GetPathName().Equals(M.Path,ESearchCase::CaseSensitive)||F->FunctionFlags!=M.Flags||F->NumParms!=M.NumParms||F->ParmsSize!=M.ParmsSize||F->ReturnValueOffset!=M.ReturnOffset
                ||C->FindFunctionByName(M.Name,EIncludeSuperFlag::ExcludeSuper)!=F)return Bad(TEXT("math_source_function"),M.Path);
            TArray<FProperty*> Fields;for(TFieldIterator<FProperty> It(F,EFieldIteratorFlags::ExcludeSuper);It;++It)Fields.Add(*It);
            if(Fields.Num()!=M.Fields.Num())return Bad(TEXT("math_field_inventory"),M.Path);
            for(int32 I=0;I<Fields.Num();++I)
            {
                const auto& E=M.Fields[I];FProperty* P=Fields[I];
                // Inventory and exact native field class precede every typed layout/default read.
                if(P!=E.P||P->GetClass()!=E.Class||P->GetOwnerUObject()!=F||P->GetFName()!=E.Name||P->GetOffset_ForInternal()!=E.Offset||P->ElementSize!=E.Size||P->ArrayDim!=E.Dim||uint64(P->PropertyFlags)!=E.Flags)return Bad(TEXT("math_field_binding"),M.Path);
                if(E.Class==FBoolProperty::StaticClass()){auto* B=static_cast<FBoolProperty*>(P);if(B->GetBoolFieldSize()!=E.BoolSize||B->GetByteOffset()!=E.BoolOffset||B->GetByteMask()!=E.BoolByteMask||B->GetFieldMask()!=E.BoolFieldMask)return Bad(TEXT("math_bool_storage"),M.Path);}
                const auto* Meta=P->GetMetaDataMap();if((Meta?*Meta:TMap<FName,FString>()).OrderIndependentCompareEqual(E.Meta)==false)return Bad(TEXT("math_field_metadata"),M.Path);
            }
            // Native UFunction metadata getters are nonvirtual value lookups. Once
            // captured, every existing Plain callback fence also protects defaults.
            if(M.MetadataReady)for(const FName Key:M.MetaKeys)
                if(!F->GetMetaData(Key).Equals(M.Meta.FindRef(Key),ESearchCase::CaseSensitive)||F->HasMetaData(Key)!=M.MetaHas.FindRef(Key))return Bad(TEXT("math_source_metadata"),M.Path);
        }
        return true;
    }
    const FMathSource* MathSource(const FString& Name) const
    {for(const auto& M:MathSources)if(M.Name==FName(*Name))return &M;return nullptr;}
    bool MathSnapshot(bool Initial=false)
    {
        if(!MathClass)return Plain();
        for(int32 I=0;I<MathSources.Num();++I)
        {
            const bool Capture=Initial&&!MathSources[I].MetadataReady;
            const TArray<FName> Keys=MathSources[I].MetaKeys;
            for(const FName Key:Keys)
            {
                const FString Value=MathSources[I].Function->GetMetaData(Key);if(!Plain())return false;
                const bool Present=MathSources[I].Function->HasMetaData(Key);if(!Plain())return false;
                if(Capture){MathSources[I].Meta.Add(Key,Value);MathSources[I].MetaHas.Add(Key,Present);}
                else if(!MathSources[I].MetadataReady||!Value.Equals(MathSources[I].Meta.FindRef(Key),ESearchCase::CaseSensitive)||Present!=MathSources[I].MetaHas.FindRef(Key))return Bad(TEXT("math_source_metadata"),MathSources[I].Path);
            }
            MathSources[I].MetadataReady=true;
        }
        return Plain();
    }
    bool BindMath(const FCollapsedTextPlan& Plan)
    {
        if(Plan.Calls.IsEmpty())return Plain();
        Count(R,TEXT("reference_lookup_calls"));UClass* C=FindObject<UClass>(nullptr,TEXT("/Script/Engine.KismetMathLibrary"));const TStrongObjectPtr<UClass> HeldClass(C);Count(R,TEXT("reference_lookup_returns"));
        if(!Plain())return false;
        if(!Loaded(C)||C->GetClass()!=UClass::StaticClass()||!C->HasAnyClassFlags(CLASS_Native)||!C->ClassDefaultObject.IsResolved())return Bad(TEXT("math_source_unavailable"));
        UObject* CDO=C->ClassDefaultObject.Get();const TStrongObjectPtr<UObject> HeldCDO(CDO);MathClass.Reset(C);MathCDO.Reset(CDO);if(!Plain())return false;
        for(const int32 Index:Plan.Calls)
        {
            const auto& O=Plan.Objects[Index];
            if(!MathSource(O.MemberName))
            {
                const FString Path=TEXT("/Script/Engine.KismetMathLibrary:")+O.MemberName;
                Count(R,TEXT("reference_lookup_calls"));UFunction* F=FindObject<UFunction>(nullptr,*Path);Hold(F);Count(R,TEXT("reference_lookup_returns"));if(!Plain())return false;
                if(!Loaded(F)||F->GetClass()!=UFunction::StaticClass()||F->GetOuter()!=C||!F->HasAllFunctionFlags(FUNC_Native|FUNC_Static|FUNC_BlueprintPure|FUNC_BlueprintCallable)||F->HasAnyFunctionFlags(FUNC_Delegate|FUNC_Event|FUNC_Net)||F->GetSuperFunction())return Bad(TEXT("math_native_signature_pending"),Path);
                FMathSource M;M.Function=F;M.Name=F->GetFName();M.Path=F->GetPathName();M.Flags=F->FunctionFlags;M.NumParms=F->NumParms;M.ParmsSize=F->ParmsSize;M.ReturnOffset=F->ReturnValueOffset;M.Reference.SetExternalMember(M.Name,C);
                for(const TCHAR* K:{TEXT("Latent"),TEXT("WorldContext"),TEXT("BlueprintInternalUseOnly"),TEXT("DefaultToSelf"),TEXT("HidePin"),TEXT("InternalUseParam"),TEXT("HideSelfPin"),TEXT("ExpandEnumAsExecs"),TEXT("ExpandBoolAsExecs"),TEXT("AutoCreateRefTerm"),TEXT("AdvancedDisplay"),TEXT("DeterminesOutputType"),TEXT("DynamicOutputParam"),TEXT("ArrayParm"),TEXT("ArrayTypeDependentParams"),TEXT("CustomStructureParam")})M.MetaKeys.Add(FName(K));
                for(TFieldIterator<FProperty> It(F,EFieldIteratorFlags::ExcludeSuper);It;++It)
                {
                    FProperty* P=*It;const FFieldClass* FC=P->GetClass();
                    if((FC!=FIntProperty::StaticClass()&&FC!=FBoolProperty::StaticClass())||P->GetOwnerUObject()!=F)return Bad(TEXT("math_scalar_type_pending"),Path);
                    if(!P->HasAnyPropertyFlags(CPF_Parm)||P->HasAnyPropertyFlags(CPF_ReferenceParm|CPF_ConstParm|CPF_AdvancedDisplay|CPF_RequiredParm)||P->HasSetterOrGetter()||P->ArrayDim!=1||P->GetOffset_ForInternal()<0||P->GetOffset_ForInternal()+P->ElementSize>F->ParmsSize||M.Fields.Num()>=16)return Bad(TEXT("math_byvalue_field_pending"),Path);
                    FMathField V;V.P=P;V.Class=FC;V.Name=P->GetFName();V.Offset=P->GetOffset_ForInternal();V.Size=P->ElementSize;V.Dim=P->ArrayDim;V.Flags=uint64(P->PropertyFlags);V.Category=FC==FIntProperty::StaticClass()?TEXT("int"):TEXT("bool");
                    if(FC==FIntProperty::StaticClass()){if(V.Size!=sizeof(int32))return Bad(TEXT("math_int_layout"),Path);}
                    else{auto* B=static_cast<FBoolProperty*>(P);V.BoolSize=B->GetBoolFieldSize();V.BoolOffset=B->GetByteOffset();V.BoolByteMask=B->GetByteMask();V.BoolFieldMask=B->GetFieldMask();if(!B->IsNativeBool()||V.Size!=sizeof(bool)||V.BoolSize!=sizeof(bool)||V.BoolOffset!=0)return Bad(TEXT("math_bool_layout"),Path);}
                    if(const auto* Meta=P->GetMetaDataMap())V.Meta=*Meta;
                    M.MetaKeys.AddUnique(V.Name);M.MetaKeys.AddUnique(FName(*(FString(TEXT("CPP_Default_"))+V.Name.ToString())));M.Fields.Add(MoveTemp(V));
                }
                if(M.Fields.Num()!=F->NumParms)return Bad(TEXT("math_direct_field_count"),Path);
                MathSources.Add(MoveTemp(M));if(!MathSnapshot(true))return false;
                const auto& Added=MathSources.Last();for(int32 K=0;K<16;++K)if(Added.MetaHas.FindRef(Added.MetaKeys[K]))return Bad(TEXT("math_pin_metadata_pending"),Path);
                for(const auto& Field:Added.Fields){FString Default=Added.Meta.FindRef(Field.Name);if(Default.IsEmpty())Default=Added.Meta.FindRef(FName(*(FString(TEXT("CPP_Default_"))+Field.Name.ToString())));if(!FCollapsedTextPlan::ScalarDefault(Field.Category,Default))return Bad(TEXT("math_default_pending"),Path);}
            }
            const auto* M=MathSource(O.MemberName);if(!M||O.Pins.Num()!=M->Fields.Num()+1)return Bad(TEXT("math_planned_pin_count"),O.MemberName);
            bool Self=false;for(const auto& P:O.Pins)
            {
                if(P.Name==TEXT("self")){Self=true;continue;}
                const FMathField* Field=M->Fields.FindByPredicate([&](const FMathField& E){return E.Name==FName(*P.Name);});
                if(!Field||P.Category!=Field->Category||P.Direction!=((Field->Flags&uint64(CPF_ReturnParm|CPF_OutParm))?EGPD_Output:EGPD_Input))return Bad(TEXT("math_planned_signature"),O.MemberName);
            }
            if(!Self)return Bad(TEXT("math_planned_self_missing"),O.MemberName);
        }
        R.Data->SetNumberField(TEXT("collapsed_native_function_source_count"),MathSources.Num());R.Data->SetNumberField(TEXT("collapsed_native_call_count"),Plan.Calls.Num());
        return MathSnapshot();
    }
    // QZ optional clear state; the existing append-only lease remains unchanged when false.
    struct FClearTarget{UEdGraphNode* Object=nullptr;FGuid Guid;FString Path,Name,ClassPath;};TArray<FClearTarget> ClearTargets;TSet<UEdGraphNode*> ClearRetired;bool ClearMode=false;
    struct FClearRoot{UK2Node_FunctionTerminator* Object=nullptr;UClass* Class=nullptr;FMemberReference Ref;int32 EntryFlags=0;TArray<FUserPinInfo> UserPins;};TArray<FClearRoot> ClearRoots;
    static bool ClearClass(const UClass* C)
    {return C==UK2Node_Event::StaticClass()||C==UK2Node_CustomEvent::StaticClass()||C==UK2Node_IfThenElse::StaticClass()||C==UK2Node_ExecutionSequence::StaticClass()||C==UK2Node_CallFunction::StaticClass()||C==UK2Node_VariableGet::StaticClass()||C==UK2Node_VariableSet::StaticClass()||C==UK2Node_Self::StaticClass();}
    static bool ClearRootClass(const UClass* C)
    {return C==UK2Node_FunctionEntry::StaticClass()||C==UK2Node_FunctionResult::StaticClass();}
    bool ClearRootsCurrent()
    {
        if(!ClearMode)return true;if(!Loaded(Selected))return Bad(TEXT("clear_target_unavailable"));
        for(const auto& E:ClearRoots){auto* N=E.Object;if(!Loaded(N)||N->GetClass()!=E.Class||N->GetOuter()!=Selected||!Selected->Nodes.Contains(N)||!N->FunctionReference.IsSameReference(E.Ref)||N->UserDefinedPins.Num()!=E.UserPins.Num())return Bad(TEXT("retained_root_role"));if(E.Class==UK2Node_FunctionEntry::StaticClass()&&static_cast<UK2Node_FunctionEntry*>(N)->GetExtraFlags()!=E.EntryFlags)return Bad(TEXT("retained_entry_flags"));
            for(int32 I=0;I<E.UserPins.Num();++I){const auto& U=N->UserDefinedPins[I];const auto& V=E.UserPins[I];if(!U||U->PinType.PinSubCategoryObject.IsStale()||U->PinType.PinValueType.TerminalSubCategoryObject.IsStale()||(U->PinType.PinSubCategoryObject.Get()&&!Loaded(U->PinType.PinSubCategoryObject.Get()))||(U->PinType.PinValueType.TerminalSubCategoryObject.Get()&&!Loaded(U->PinType.PinValueType.TerminalSubCategoryObject.Get()))||!CollapsedTypeSame(U->PinType,V.PinType)||U->PinName!=V.PinName||U->DesiredPinDirection!=V.DesiredPinDirection||!U->PinDefaultValue.Equals(V.PinDefaultValue,ESearchCase::CaseSensitive))return Bad(TEXT("retained_root_user_pin"));}}
        return true;
    }
    bool PrepareClear(bool TimelineNodes=false)
    {
        if(!Plain())return false;TMap<UEdGraphPin*,UClass*> Owners;for(const auto& N:Nodes)for(const auto& P:N.Pins)Owners.Add(P.Address,N.Class);
        int32 EntryCount=0,ResultCount=0;TArray<TSharedPtr<FJsonValue>> KeepRows,RemoveRows;
        for(const auto& N:Nodes)if(N.Graph==Selected){if(ClearRootClass(N.Class)){FClearRoot E;E.Object=static_cast<UK2Node_FunctionTerminator*>(N.Object);E.Class=N.Class;E.Ref=E.Object->FunctionReference;if(N.Class==UK2Node_FunctionEntry::StaticClass()){++EntryCount;E.EntryFlags=static_cast<UK2Node_FunctionEntry*>(N.Object)->GetExtraFlags();}else ++ResultCount;
                for(const auto& U:E.Object->UserDefinedPins){if(!U||U->PinType.PinSubCategoryObject.IsStale()||U->PinType.PinValueType.TerminalSubCategoryObject.IsStale())return Fail(R,TEXT("collapsed_clear_root_source_pending"),TEXT("Retained root UserPinInfo needs nonstale resolved type sources."));for(UObject* O:{U->PinType.PinSubCategoryObject.Get(),U->PinType.PinValueType.TerminalSubCategoryObject.Get()})if(O){if(!Loaded(O))return Fail(R,TEXT("collapsed_clear_root_source_pending"),TEXT("Retained root type source unavailable before removal."));Hold(O);}E.UserPins.Add(*U);}ClearRoots.Add(MoveTemp(E));auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("node_path"),N.Path);J->SetStringField(TEXT("node_id"),N.Guid.ToString());J->SetStringField(TEXT("node_class"),N.Class->GetPathName());KeepRows.Add(MakeShared<FJsonValueObject>(J));continue;}
            if(!ClearClass(N.Class)&&!(TimelineNodes&&N.Class==UK2Node_Timeline::StaticClass()))return Fail(R,TEXT("collapsed_clear_node_callback_pending"),TEXT("First clear scope removes exact stock Event/CustomEvent/Branch/Sequence/plain Call/Variable/Self; old nested/Timeline/Macro or node-specific callback semantics remain incomplete."));
            for(const auto& P:N.Pins)for(auto* L:P.Links){const auto* Peer=Owners.Find(L);if(!Peer||(!ClearClass(*Peer)&&!ClearRootClass(*Peer)&&!(TimelineNodes&&*Peer==UK2Node_Timeline::StaticClass())))return Fail(R,TEXT("collapsed_clear_peer_callback_pending"),TEXT("Affected peer NodeConnectionListChanged semantics are outside the stock empty callback scope."));}
            FClearTarget E;E.Object=N.Object;E.Path=N.Path;E.Guid=N.Guid;E.Name=N.Object->GetName();E.ClassPath=N.Class->GetPathName();ClearTargets.Add(E);auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("node_path"),E.Path);J->SetStringField(TEXT("node_id"),E.Guid.ToString());J->SetStringField(TEXT("name"),E.Name);J->SetStringField(TEXT("node_class"),E.ClassPath);RemoveRows.Add(MakeShared<FJsonValueObject>(J));}
        const bool Function=Functions.Contains(Selected);if((Function&&(EntryCount!=1||ResultCount>1))||(!Function&&(EntryCount||ResultCount)))return Fail(R,TEXT("collapsed_clear_roots_pending"),TEXT("Own function requires one Entry and at most one Result; other selected graphs cannot contain function roots."));
        ClearMode=true;R.Data->SetArrayField(TEXT("clear_retained_roots"),KeepRows);R.Data->SetArrayField(TEXT("clear_planned_removals"),RemoveRows);R.Data->SetNumberField(TEXT("clear_retained_root_count"),KeepRows.Num());R.Data->SetNumberField(TEXT("clear_planned_removal_count"),ClearTargets.Num());R.Data->SetNumberField(TEXT("clear_removal_attempt_count"),0);R.Data->SetNumberField(TEXT("clear_absence_verified_count"),0);return Plain();
    }
    bool RetireForRemove(UEdGraphNode* SelectedNode)
    {
        // Pure copied plan only. From this point until return no current observer is entered.
        TSet<UEdGraphPin*> Addresses;TSet<FString> Ids;bool Found=false;for(const auto& N:Nodes)if(N.Object==SelectedNode){Found=true;for(const auto& P:N.Pins){Addresses.Add(P.Address);Ids.Add(N.Guid.ToString()+TEXT(":")+P.Id.ToString());}}
        if(!Found||ClearRetired.Contains(SelectedNode))return Bad(TEXT("clear_selected_membership"));ClearRetired.Add(SelectedNode);
        for(auto& N:Nodes)if(N.Object!=SelectedNode)for(auto& P:N.Pins){P.Links.RemoveAll([&](UEdGraphPin* A){return Addresses.Contains(A);});P.LinkIds.RemoveAll([&](const FString& I){return Ids.Contains(I);});}
        Nodes.RemoveAll([&](const FNode& N){return N.Object==SelectedNode;});for(auto& G:Graphs)if(G.Object==Selected)G.Nodes.Remove(SelectedNode);return SealLinks(Nodes);
    }

    bool SealLinks(TArray<FNode>& Values)
    {
        TMap<UEdGraphPin*,FString> IDs;TMap<UEdGraphPin*,const FPin*> Pins;
        for(const auto& V:Values) { for(const auto& P:V.Pins) { if(IDs.Contains(P.Address)) { return Bad(TEXT("duplicate_pin")); }IDs.Add(P.Address,V.Guid.ToString()+TEXT(":")+P.Id.ToString());Pins.Add(P.Address,&P); } }
        for(auto& V:Values) { for(auto& P:V.Pins) { P.LinkIds.Reset();TSet<UEdGraphPin*> Seen;for(auto* L:P.Links) { const auto* Peer=Pins.Find(L);if(!Peer||Seen.Contains(L)||L==P.Address||!(*Peer)->Links.Contains(P.Address)) { return Bad(TEXT("external_or_nonreciprocal_link")); }Seen.Add(L);P.LinkIds.Add(IDs.FindChecked(L)); } } }return true;
    }
    bool Bind(UBlueprint* In,UEdGraph* Target)
    {
        BP.Reset(In);Package.Reset(In->GetOutermost());Editor.Reset(GEditor);World.Reset(GWorld);Selected=Target;auto ParentSlot=In->ParentClass;if(!ParentSlot.GetGCPtr().IsResolved()) { return Bad(TEXT("parent_slot_unresolved")); }Parent=ParentSlot.GetGCPtr().Get();Hold(Parent);BPPath=In->GetPathName();PackagePath=Package->GetName();
        Functions=In->FunctionGraphs;Macros=In->MacroGraphs;Uber=In->UbergraphPages;Delegates=In->DelegateSignatureGraphs;Events=In->EventGraphs;VariablesBefore=In->NewVariables;InterfacesBefore=In->ImplementedInterfaces;
        if(!Loaded(Editor.Get())||!Loaded(World.Get()))return Bad(TEXT("initial_return_context_unavailable"));
        ReturnedEditorClass=Editor->GetClass();ReturnedWorldClass=World->GetClass();Hold(ReturnedEditorClass);Hold(ReturnedWorldClass);
        if(In->GetClass()!=UBlueprint::StaticClass()||Parent!=AActor::StaticClass()||!Classes(true)||!ReacquireEvents()) { return Bad(TEXT("ordinary_actor_class_profile")); }
        SCS=In->SimpleConstructionScript;Hold(SCS);if(!Loaded(SCS)) { return Bad(TEXT("scs_unavailable")); }SCSNodes=SCS->GetAllNodes();SCSRoots=SCS->GetRootNodes();
        if(SCSNodes.Num()!=1||SCSRoots.Num()!=1||SCSNodes[0]!=SCSRoots[0]) { return Bad(TEXT("scs_first_profile_pending")); }
        RootNode=SCSNodes[0];Hold(RootNode);RootTemplate=Cast<USceneComponent>(RootNode->ComponentTemplate);Hold(RootTemplate);
        if(!Loaded(RootTemplate)||RootTemplate->GetClass()!=USceneComponent::StaticClass()) { return Bad(TEXT("scs_native_scene_root_pending")); }
        RootGuid=RootNode->VariableGuid;RootName=RootNode->GetVariableName();RootRelative=RootTemplate->GetRelativeTransform();RootMobility=RootTemplate->Mobility;
        TArray<UEdGraph*> All;Count(R,TEXT("get_all_graphs_calls"));In->GetAllGraphs(All);if(All.Num()>MaxGraphs) { return Bad(TEXT("graph_bound")); }
        TSet<UEdGraph*> Seen;TSet<UEdGraphNode*> SeenNodes;int32 PinCount=0;
        for(UEdGraph* G:All)
        {
            Hold(G);if(!Loaded(G)||Seen.Contains(G)||!G->SubGraphs.IsEmpty()||G->GetClass()!=UEdGraph::StaticClass()||G->GetOuter()!=In) { return Bad(TEXT("existing_direct_graph_profile")); }Seen.Add(G);
            auto SC=G->Schema;if(!SC.GetGCPtr().IsResolved()||SC.GetGCPtr().Get()!=UEdGraphSchema_K2::StaticClass()) { return Bad(TEXT("schema_binding")); }
            FGraph V;V.Object=G;V.Outer=G->GetOuter();V.Path=G->GetPathName();V.Guid=G->GraphGuid;V.Editable=G->bEditable;V.Deletion=G->bAllowDeletion;V.Renaming=G->bAllowRenaming;for(UEdGraphNode* N:G->Nodes) { if(SeenNodes.Contains(N)||SeenNodes.Num()>=MaxNodes) { return Bad(TEXT("old_node_bound_or_duplicate")); }SeenNodes.Add(N);V.Nodes.Add(N);FNode NV;if(!NodeCopy(N,NV)) { return false; }PinCount+=NV.Pins.Num();if(PinCount>MaxPins) { return Bad(TEXT("old_pin_bound")); }Nodes.Add(MoveTemp(NV)); }
            Graphs.Add(MoveTemp(V));
        }
        if(!Seen.Contains(Selected)||!SealLinks(Nodes)||!Plain()) { return false; }
        for(auto& V:Nodes) { for(auto& P:V.Pins) { const FText Copy=P.Text;P.TextDisplay=Copy.ToString();if(!Plain()) { return false; }const FText Friendly=P.Friendly;P.FriendlyDisplay=Friendly.ToString();if(!Plain()) { return false; } } }
        return Plain();
    }
    bool Reacquire()
    {
        // Structural compiler may replace pins/classes. Capture physical candidates first; original logical values stay authoritative.
        Rebinding=true;if(!ReacquireEvents()) { return false; }Candidate.Reset();for(const auto& E:Nodes) { FNode V;if(!NodeCopy(E.Object,V)) { return false; }
            if(V.Class!=E.Class||V.Graph!=E.Graph||V.Guid!=E.Guid||!V.Path.Equals(E.Path,ESearchCase::CaseSensitive)||V.X!=E.X||V.Y!=E.Y||!V.Comment.Equals(E.Comment,ESearchCase::CaseSensitive)||V.Visible!=E.Visible||V.Pinned!=E.Pinned||!V.Member.IsSameReference(E.Member)||V.Input!=E.Input||V.Output!=E.Output||V.BoundGraph!=E.BoundGraph||V.CanInputs!=E.CanInputs||V.CanOutputs!=E.CanOutputs||V.UserPins.Num()!=E.UserPins.Num()) { return Bad(TEXT("reconstructed_node_role"),E.Path); }
            for(int32 I=0;I<E.UserPins.Num();++I) { const auto& A=V.UserPins[I];const auto& P=E.UserPins[I];if(A.PinName!=P.PinName||!CollapsedTypeSame(A.PinType,P.PinType)||A.DesiredPinDirection!=P.DesiredPinDirection||!A.PinDefaultValue.Equals(P.PinDefaultValue,ESearchCase::CaseSensitive)) { return Bad(TEXT("reconstructed_user_pin"),E.Path); } }
            Candidate.Add(MoveTemp(V)); }
        if(!SealLinks(Candidate)||!Plain()||!Classes(true)) { return false; }
        for(int32 I=0;I<Nodes.Num();++I)
        {
            const auto& E=Nodes[I];auto& A=Candidate[I];if(A.Pins.Num()!=E.Pins.Num()) { return Bad(TEXT("reconstructed_pin_count")); }
            for(int32 J=0;J<E.Pins.Num();++J)
            {
                const auto& P=E.Pins[J];auto& Q=A.Pins[J];const FText Copy=Q.Text;Q.TextDisplay=Copy.ToString();if(!Plain()) { return false; }const FText Friendly=Q.Friendly;Q.FriendlyDisplay=Friendly.ToString();if(!Plain()) { return false; }
                if(Q.Id!=P.Id||Q.Name!=P.Name||Q.Direction!=P.Direction||!CollapsedTypeSame(Q.Type,P.Type)||Q.Default!=P.Default||Q.Bits!=P.Bits||Q.LinkIds!=P.LinkIds
                    ||!Q.Raw.Equals(P.Raw,ESearchCase::CaseSensitive)||!Q.Auto.Equals(P.Auto,ESearchCase::CaseSensitive)||!Q.TextDisplay.Equals(P.TextDisplay,ESearchCase::CaseSensitive)||FTextInspector::GetFlags(Q.Text)!=FTextInspector::GetFlags(P.Text)||!Q.FriendlyDisplay.Equals(P.FriendlyDisplay,ESearchCase::CaseSensitive)||FTextInspector::GetFlags(Q.Friendly)!=FTextInspector::GetFlags(P.Friendly)) { return Bad(TEXT("reconstructed_logical_pin"),E.Path); }
            }
        }
        Nodes=MoveTemp(Candidate);Rebinding=false;return Plain();
    }
};

bool IsCollapsedText(const FString& S)
{
    const TCHAR* Stream=*S;FString Line;while(FParse::Line(&Stream,Line)) { const FString T=Line.TrimStartAndEnd();FString C;if(T.StartsWith(TEXT("Begin Object "))&&FParse::Value(*T,TEXT("Class="),C)&&C==TEXT("/Script/BlueprintGraph.K2Node_Composite")) { return true; } }return false;
}
bool CollapsedAcquired(FCollapsedTarget& T,const FCollapsedTextPlan& Plan,const TSet<UEdGraphNode*>& Returned)
{
    // ImportNodesFromText includes recursive PostPaste/Reconstruct. Inspect only
    // fresh returned owning slots; no pre-import pin address is consumed here.
    for(UEdGraphNode* N:Returned)T.Hold(N);
    if(!T.ReturnedContext())return false;
    if(Returned.Num()!=1)return T.Bad(TEXT("returned_top_count"));
    UEdGraphNode* Top=*Returned.CreateConstIterator();
    if(!Loaded(Top)||Top->GetClass()!=UK2Node_Composite::StaticClass()||Top->GetGraph()!=T.Selected)return T.Bad(TEXT("returned_composite_role"));
    auto* C=static_cast<UK2Node_Composite*>(Top);
    TMap<int32,UEdGraphNode*> ByPlan;ByPlan.Add(0,C);
    TMap<int32,UEdGraph*> ActualGraphs;TArray<FCollapsedTarget::FGraph> NewGraphs;
    TSet<UEdGraph*> SeenGraphs;TSet<UEdGraphNode*> SeenNodes;SeenNodes.Add(C);
    for(const auto& GP:Plan.GraphPlans)
    {
        UEdGraphNode* const* OwnerSlot=ByPlan.Find(GP.Composite);
        if(!OwnerSlot||!Loaded(*OwnerSlot)||(*OwnerSlot)->GetClass()!=UK2Node_Composite::StaticClass())return T.Bad(TEXT("nested_composite_membership"));
        auto* Owner=static_cast<UK2Node_Composite*>(*OwnerSlot);
        if(!Owner->BoundGraph.IsResolved())return T.Bad(TEXT("returned_bound_unresolved"));
        UEdGraph* B=Owner->BoundGraph.Get();T.Hold(B);
        if(!Loaded(B)||B->GetClass()!=UEdGraph::StaticClass()||B->GetOuter()!=Owner||SeenGraphs.Contains(B)||B->GraphGuid!=Plan.Objects[GP.Graph].Guid||B->Nodes.Num()!=GP.Nodes.Num())return T.Bad(TEXT("returned_bound_role"));
        auto SC=B->Schema;if(!SC.GetGCPtr().IsResolved()||SC.GetGCPtr().Get()!=UEdGraphSchema_K2::StaticClass())return T.Bad(TEXT("returned_bound_schema"));
        SeenGraphs.Add(B);ActualGraphs.Add(GP.Graph,B);
        FCollapsedTarget::FGraph BG;BG.Object=B;BG.Outer=Owner;BG.Path=B->GetPathName();BG.Guid=B->GraphGuid;BG.Editable=B->bEditable;BG.Deletion=B->bAllowDeletion;BG.Renaming=B->bAllowRenaming;
        // Native EdGraph ctor enables Edit/Delete; AllowRenaming stays zero
        // unless explicitly serialized. PostPaste does not assign this flag.
        const bool ExpectedRenaming=Plan.Objects[GP.Graph].Fields.FindRef(TEXT("bAllowRenaming"))==TEXT("True");
        if(!BG.Editable||!BG.Deletion||BG.Renaming!=ExpectedRenaming)return T.Bad(TEXT("returned_bound_authored_flags"));
        for(int32 J=0;J<GP.Nodes.Num();++J)
        {
            if(!B->Nodes[J].IsResolved())return T.Bad(TEXT("returned_inner_slot_unresolved"));
            UEdGraphNode* N=B->Nodes[J].Get();T.Hold(N);const int32 PlanIndex=GP.Nodes[J];const auto& E=Plan.Objects[PlanIndex];
            if(!Loaded(N)||N->GetGraph()!=B||SeenNodes.Contains(N)||N->NodeGuid!=E.Guid||!N->GetClass()->GetPathName().Equals(E.Class,ESearchCase::CaseSensitive))return T.Bad(TEXT("returned_inner_owning_order"));
            SeenNodes.Add(N);BG.Nodes.Add(N);ByPlan.Add(PlanIndex,N);
        }
        // Hold resolved graph slots immediately, then validate their planned
        // memberships after every nested graph has been acquired.
        for(const auto& Slot:B->SubGraphs)
        {
            if(!Slot.IsResolved())return T.Bad(TEXT("returned_nested_slot_unresolved"));
            UEdGraph* Sub=Slot.Get();T.Hold(Sub);if(!Loaded(Sub))return T.Bad(TEXT("returned_nested_unavailable"));BG.Subs.Add(Sub);
        }
        NewGraphs.Add(MoveTemp(BG));
    }
    for(int32 I=0;I<Plan.GraphPlans.Num();++I)
    {
        const auto& GP=Plan.GraphPlans[I];const auto& Actual=NewGraphs[I];
        if(Actual.Subs.Num()!=GP.Subs.Num())return T.Bad(TEXT("returned_nested_graph_count"));
        TSet<UEdGraph*> SeenSubs;
        for(int32 J=0;J<Actual.Subs.Num();++J)
        {
            auto* Sub=Actual.Subs[J];bool Found=false;for(int32 SI:GP.Subs)Found|=ActualGraphs.FindRef(SI)==Sub;
            if(!Found||SeenSubs.Contains(Sub)||(GP.SubOrderAuthored&&ActualGraphs.FindRef(GP.Subs[J])!=Sub))return T.Bad(TEXT("returned_nested_graph_membership_or_order"));SeenSubs.Add(Sub);
        }
    }
    T.Composite=C;T.Bound=ActualGraphs.FindRef(Plan.Graph);T.ExpectedAdded.Add(C);T.Acquired=true;
    T.Graphs.Append(MoveTemp(NewGraphs));
    TArray<FCollapsedTarget::FNode> New;
    for(int32 I=0;I<Plan.Objects.Num();++I)
    {
        if(Plan.Objects[I].Class==TEXT("/Script/Engine.EdGraph"))continue;
        auto* N=ByPlan.FindRef(I);if(!N)return T.Bad(TEXT("returned_node_plan_incomplete"));
        FCollapsedTarget::FNode V;if(!T.NodeCopy(N,V))return false;New.Add(MoveTemp(V));
    }
    auto Match=[&](int32 PlanIndex,UEdGraphNode* N,bool Root)
    {
        const auto& E=Plan.Objects[PlanIndex];const auto* OwnedGraph=Plan.GraphForComposite(PlanIndex);
        auto Mismatch=[&](const FString& Field,const FString& Expected,const FString& Actual)
        {
            if(!T.R.Data->HasField(TEXT("first_mapping_mismatch"))) { auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),T.R.Stage);J->SetStringField(TEXT("source_node"),E.Name);J->SetStringField(TEXT("field"),Field);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);J->SetBoolField(TEXT("operands_observed"),true);T.R.Data->SetObjectField(TEXT("first_mapping_mismatch"),J); }return false;
        };
        if(!Loaded(N)) { return false; }
        if(!N->GetClass()->GetPathName().Equals(E.Class,ESearchCase::CaseSensitive)) { return Mismatch(TEXT("class"),E.Class,N->GetClass()->GetPathName()); }
        if(!Root&&N->NodeGuid!=E.Guid) { return Mismatch(TEXT("node_guid"),E.Guid.ToString(),N->NodeGuid.ToString()); }
        if(N->NodePosX!=E.X||N->NodePosY!=E.Y||!N->NodeComment.Equals(E.Comment,ESearchCase::CaseSensitive)) { return Mismatch(TEXT("position_or_comment"),LexToString(E.X)+TEXT(",")+LexToString(E.Y)+TEXT(":")+E.Comment,LexToString(N->NodePosX)+TEXT(",")+LexToString(N->NodePosY)+TEXT(":")+N->NodeComment); }
        if(OwnedGraph)
        {
            const auto* CompositeNode=static_cast<UK2Node_Composite*>(N);
            if(bool(CompositeNode->bCanHaveInputs)!=E.Inputs||bool(CompositeNode->bCanHaveOutputs)!=E.Outputs)return Mismatch(TEXT("composite_direction_flags"),LexToString(E.Inputs)+TEXT(":")+LexToString(E.Outputs),LexToString(bool(CompositeNode->bCanHaveInputs))+TEXT(":")+LexToString(bool(CompositeNode->bCanHaveOutputs)));
        }
        if(N->Pins.Num()!=E.Pins.Num()) { return Mismatch(TEXT("pin_count"),LexToString(E.Pins.Num()),LexToString(N->Pins.Num())); }
        const auto* Call=Cast<UK2Node_CallFunction>(N);const auto* Math=Call?T.MathSource(E.MemberName):nullptr;
        if(Call&&(!Math||!Call->FunctionReference.IsSameReference(Math->Reference)||!Call->bDefaultsToPureFunc))return Mismatch(TEXT("function_reference"),E.MemberName,TEXT("actual_member_or_pure_role_changed"));
        for(const auto& P:E.Pins)
        {
            const auto* A=N->FindPin(FName(*P.Name),P.Direction);FEdGraphPinType ExpectedType;ExpectedType.PinCategory=FName(*P.Category);ExpectedType.PinSubCategory=FName(*P.SubCategory);ExpectedType.PinSubCategoryObject=P.TypeObject.IsEmpty()?nullptr:T.MathClass.Get();
            if(!A||A->PinType.PinSubCategoryObject.IsStale()||A->PinType.PinValueType.TerminalSubCategoryObject.IsStale()||!CollapsedTypeSame(A->PinType,ExpectedType)||!A->DefaultObject.IsResolved()
                ||A->DefaultObject.Get()!=(P.DefaultObject.IsEmpty()?nullptr:T.MathCDO.Get())||FCollapsedTarget::Bits(A)!=(P.Hidden?1u:0u))return Mismatch(P.Name+TEXT(".type_default_flags"),P.Category+TEXT(":")+P.TypeObject,TEXT("actual_typed_role_or_flags_differ"));
            FString ExpectedAuto=P.Auto,ExpectedRaw=P.Raw;
            if(P.Category==TEXT("int")||P.Category==TEXT("bool"))
            {
                if(ExpectedAuto.IsEmpty())
                {
                    if(Call)
                    {
                        ExpectedAuto=Math->Meta.FindRef(FName(*P.Name));if(ExpectedAuto.IsEmpty())ExpectedAuto=Math->Meta.FindRef(FName(*(FString(TEXT("CPP_Default_"))+P.Name)));
                        if(ExpectedAuto.IsEmpty())ExpectedAuto=P.Category==TEXT("bool")?TEXT("false"):TEXT("0");
                    }
                    else if(E.Class==TEXT("/Script/BlueprintGraph.K2Node_IfThenElse")&&P.Name==TEXT("Condition"))ExpectedAuto=TEXT("true");
                    else if(OwnedGraph) { const auto& Port=Plan.Objects[P.Direction==EGPD_Input?OwnedGraph->Entry:OwnedGraph->Exit];ExpectedAuto=Port.UserDefaults.FindRef(P.Name); }
                    else { ExpectedAuto=E.UserDefaults.FindRef(P.Name); }
                }
                if(ExpectedRaw.IsEmpty()) { ExpectedRaw=ExpectedAuto; }
            }
            if(!A->DefaultValue.Equals(ExpectedRaw,ESearchCase::CaseSensitive)) { return Mismatch(P.Name+TEXT(".raw"),ExpectedRaw,A->DefaultValue); }
            if(!A->AutogeneratedDefaultValue.Equals(ExpectedAuto,ESearchCase::CaseSensitive)) { return Mismatch(P.Name+TEXT(".autogenerated"),ExpectedAuto,A->AutogeneratedDefaultValue); }
            // SDK reconstruction can allocate different addresses. The source PinId->actual port mapping is reported explicitly.
        }
        return true;
    };
    for(const auto& Pair:ByPlan)if(!Match(Pair.Key,Pair.Value,Pair.Key==0))return T.Bad(TEXT("requested_recursive_node_mapping"));
    auto Observations=MakeShared<FJsonObject>();T.R.Data->SetObjectField(TEXT("tunnel_pair_binding_observation"),Observations);
    auto Role=[&](UEdGraphNode* N)
    {
        if(!N)return FString(TEXT("None"));for(const auto& Pair:ByPlan)if(Pair.Value==N)return LexToString(Pair.Key)+TEXT(":")+Plan.Objects[Pair.Key].Name;
        return FString(TEXT("outside_imported_set@"))+LexToString(reinterpret_cast<UPTRINT>(N));
    };
    auto SlotMatches=[&](const FString& Owner,const TCHAR* Property,const TObjectPtr<UK2Node_Tunnel>& Ref,UK2Node_Tunnel* Expected)
    {
        const bool Resolved=Ref.IsResolved();UK2Node_Tunnel* Actual=Resolved?Ref.Get():nullptr;
        auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("owner"),Owner);V->SetStringField(TEXT("property"),Property);V->SetBoolField(TEXT("resolved"),Resolved);V->SetStringField(TEXT("expected"),Role(Expected));V->SetStringField(TEXT("actual"),Resolved?Role(Actual):FString(TEXT("unresolved_unread")));Observations->SetObjectField(Owner+TEXT(".")+Property,V);
        const bool Matches=Resolved&&Actual==Expected;if(!Matches)T.R.Data->SetObjectField(TEXT("first_tunnel_binding_mismatch"),V);return Matches;
    };
    for(const auto& GP:Plan.GraphPlans)
    {
        auto* Owner=static_cast<UK2Node_Composite*>(ByPlan.FindChecked(GP.Composite));
        auto* Entry=static_cast<UK2Node_Tunnel*>(ByPlan.FindChecked(GP.Entry));auto* Exit=static_cast<UK2Node_Tunnel*>(ByPlan.FindChecked(GP.Exit));
        const FString OwnerName=LexToString(GP.Composite)+TEXT(":")+Plan.Objects[GP.Composite].Name;
        if(!SlotMatches(OwnerName,TEXT("InputSinkNode"),Owner->InputSinkNode,Entry)
            ||!SlotMatches(OwnerName,TEXT("OutputSourceNode"),Owner->OutputSourceNode,Exit)
            ||!SlotMatches(Role(Entry),TEXT("OutputSourceNode"),Entry->OutputSourceNode,Owner)
            ||!SlotMatches(Role(Entry),TEXT("InputSinkNode"),Entry->InputSinkNode,nullptr)
            ||!SlotMatches(Role(Exit),TEXT("InputSinkNode"),Exit->InputSinkNode,Owner)
            ||!SlotMatches(Role(Exit),TEXT("OutputSourceNode"),Exit->OutputSourceNode,nullptr))return T.Bad(TEXT("recursive_tunnel_pair_binding"));
        if(Entry->bCanHaveInputs||!Entry->bCanHaveOutputs||!Exit->bCanHaveInputs||Exit->bCanHaveOutputs)return T.Bad(TEXT("tunnel_direction_flags"));
    }
    TMap<FString,UEdGraphPin*> ActualPins;
    for(const auto& Pair:ByPlan)
    {
        const auto& E=Plan.Objects[Pair.Key];for(const auto& P:E.Pins)ActualPins.Add(LexToString(E.Parent)+TEXT(":")+E.Name+TEXT(" ")+P.Id.ToString(EGuidFormats::Digits),Pair.Value->FindPin(FName(*P.Name),P.Direction));
    }
    for(const auto& Pair:ByPlan)
    {
        const auto& E=Plan.Objects[Pair.Key];for(const auto& P:E.Pins)
        {
            auto* A=Pair.Value->FindPin(FName(*P.Name),P.Direction);if(A->LinkedTo.Num()!=P.Links.Num())return T.Bad(TEXT("requested_link_count"));
            for(int32 I=0;I<P.Links.Num();++I){const auto* Peer=ActualPins.Find(LexToString(E.Parent)+TEXT(":")+P.Links[I]);if(!Peer||A->LinkedTo[I]!=*Peer||!(*Peer)->LinkedTo.Contains(A))return T.Bad(TEXT("requested_recursive_link_mapping"));}
        }
    }
    for(auto* P:C->Pins)if(!P->LinkedTo.IsEmpty())return T.Bad(TEXT("unexpected_external_composite_link"));
    TArray<TSharedPtr<FJsonValue>> Mapping;
    for(const auto& Pair:ByPlan)
    {
        const auto& Source=Plan.Objects[Pair.Key];auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("source_object_index"),Pair.Key);J->SetNumberField(TEXT("source_parent_graph_index"),Source.Parent);J->SetStringField(TEXT("source_node_name"),Source.Name);J->SetStringField(TEXT("source_node_id"),Source.Guid.ToString());J->SetStringField(TEXT("actual_node_path"),Pair.Value->GetPathName());J->SetStringField(TEXT("actual_node_id"),Pair.Value->NodeGuid.ToString());
        TArray<TSharedPtr<FJsonValue>> Pins;for(const auto& P:Source.Pins) { auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("source_pin_id"),P.Id.ToString());auto* A=Pair.Value->FindPin(FName(*P.Name),P.Direction);Q->SetStringField(TEXT("actual_pin_id"),A->PinId.ToString());Q->SetStringField(TEXT("pin_name"),P.Name);Pins.Add(MakeShared<FJsonValueObject>(Q)); }J->SetArrayField(TEXT("pins"),Pins);Mapping.Add(MakeShared<FJsonValueObject>(J));
    }
    T.R.Data->SetArrayField(TEXT("collapsed_mapping"),Mapping);T.Nodes.Append(MoveTemp(New));
    // First actual roles must agree with the input-derived tree before becoming current authority.
    if(!T.SealLinks(T.Nodes)||!T.Plain()) { return false; }
    for(const int32 Index:Plan.Calls)
    {
        auto* Call=static_cast<UK2Node_CallFunction*>(ByPlan.FindChecked(Index));UFunction* F=Call->GetTargetFunction();const TStrongObjectPtr<UFunction> Held(F);
        if(!T.Plain())return false;const auto* Source=T.MathSource(Plan.Objects[Index].MemberName);
        if(!Source||F!=Source->Function)return T.Bad(TEXT("returned_math_function_binding"));
    }
    for(auto& N:T.Nodes) { for(auto& P:N.Pins) { if(P.TextDisplay.IsEmpty()) { const FText Copy=P.Text;P.TextDisplay=Copy.ToString();if(!T.Plain()) { return false; } }if(P.FriendlyDisplay.IsEmpty()) { const FText Copy=P.Friendly;P.FriendlyDisplay=Copy.ToString();if(!T.Plain()) { return false; } } } }
    return T.MathSnapshot();
}
// TI uses the existing survivor lease, while separating the clipboard selector
// from PostDuplicate's regenerated template GUID and cached names.
struct FTITimelineText
{
    FString Name,TimelineName,Comment;FGuid NodeGuid,Selector;int32 X=0,Y=0;bool Visible=false,Pinned=false;
    TArray<TMap<FString,FString>> Pins;
    bool Parse(const FString& S,FResult& R)
    {
        if(!Text(S,MaxText,R))return false;bool Open=false,Closed=false;TMap<FString,FString> Fields;
        const TCHAR* Stream=*S;FString Line;
        while(FParse::Line(&Stream,Line))
        {
            const FString L=Line.TrimStartAndEnd();if(L.IsEmpty())continue;
            if(L.StartsWith(TEXT("Begin Object "),ESearchCase::CaseSensitive))
            {
                FString Class,Export;if(Open||Closed||!FParse::Value(*L,TEXT("Class="),Class)||Class!=TEXT("/Script/BlueprintGraph.K2Node_Timeline")||!FParse::Value(*L,TEXT("Name="),Name)||!FCollapsedTextPlan::Name(Name))return Fail(R,TEXT("timeline_clipboard_text_pending"),TEXT("One flat native Timeline object required; mixed/nested clipboard inputs remain incomplete."));
                FString Header=TEXT("Begin Object Class=")+Class+TEXT(" Name=\"")+Name+TEXT("\"");
                if(FParse::Value(*L,TEXT("ExportPath="),Export))Header+=TEXT(" ExportPath=\"")+Export+TEXT("\"");
                if(L!=Header)return Fail(R,TEXT("timeline_clipboard_header_pending"),TEXT("Canonical stock Class/Name/optional ExportPath header required; archetype/header extensions remain incomplete."));Open=true;continue;
            }
            if(L==TEXT("End Object")){if(!Open||Closed)return Fail(R,TEXT("timeline_clipboard_text_pending"),TEXT("Unbalanced Timeline text."));Open=false;Closed=true;continue;}
            if(!Open)return Fail(R,TEXT("timeline_clipboard_text_pending"),TEXT("Timeline fields must be inside the single object."));
            if(L.StartsWith(TEXT("CustomProperties Pin "),ESearchCase::CaseSensitive))
            {TMap<FString,FString> P;if(Pins.Num()>=128||!FCollapsedTextPlan::Map(L.Mid(21),P))return Fail(R,TEXT("timeline_clipboard_pin_pending"),TEXT("Bounded stock pin tuple required."));Pins.Add(MoveTemp(P));continue;}
            FString K,V;if(!L.Split(TEXT("="),&K,&V)||Fields.Contains(K))return Fail(R,TEXT("timeline_clipboard_field_pending"),TEXT("Unique direct Timeline fields required."));
            if(K!=TEXT("TimelineName")&&K!=TEXT("TimelineGuid")&&K!=TEXT("NodeGuid")&&K!=TEXT("NodePosX")&&K!=TEXT("NodePosY")&&K!=TEXT("NodeComment")&&K!=TEXT("bCommentBubbleVisible")&&K!=TEXT("bCommentBubblePinned")&&K!=TEXT("bCanRenameNode")&&K!=TEXT("ErrorType"))return Fail(R,TEXT("timeline_clipboard_field_pending"),TEXT("Additional serialized node fields remain incomplete; they are not ignored."));Fields.Add(K,V);
        }
        if(!Closed||Open||!FGuid::Parse(Fields.FindRef(TEXT("TimelineGuid")),Selector)||!Selector.IsValid()||!FGuid::Parse(Fields.FindRef(TEXT("NodeGuid")),NodeGuid)||!NodeGuid.IsValid())return Fail(R,TEXT("timeline_clipboard_guid_pending"),TEXT("Valid copied node and source template selector GUIDs required."));
        TimelineName=FCollapsedTextPlan::Unquote(Fields.FindRef(TEXT("TimelineName")));Comment=FCollapsedTextPlan::Unquote(Fields.FindRef(TEXT("NodeComment")));if(!FCollapsedTextPlan::Name(TimelineName))return Fail(R,TEXT("timeline_clipboard_name_pending"),TEXT("Canonical Timeline variable name required."));
        for(const TCHAR* K:{TEXT("NodePosX"),TEXT("NodePosY"),TEXT("ErrorType")})if(const FString* V=Fields.Find(K)){int32 N=0;if(!LexTryParseString(N,**V)||LexToString(N)!=*V)return Fail(R,TEXT("timeline_clipboard_integer_pending"),TEXT("Canonical int32 node coordinates/error type required."));if(FString(K)==TEXT("NodePosX"))X=N;else if(FString(K)==TEXT("NodePosY"))Y=N;}
        for(const TCHAR* K:{TEXT("bCommentBubbleVisible"),TEXT("bCommentBubblePinned"),TEXT("bCanRenameNode")})if(const FString* V=Fields.Find(K)){bool B=false;if(!FCollapsedTextPlan::Boolean(*V,B))return Fail(R,TEXT("timeline_clipboard_bool_pending"),TEXT("Exact node bool spelling required."));if(FString(K)==TEXT("bCommentBubbleVisible"))Visible=B;else if(FString(K)==TEXT("bCommentBubblePinned"))Pinned=B;}
        return true;
    }
};
bool IsTimelineText(const FString& S)
{const TCHAR* Stream=*S;FString L;while(FParse::Line(&Stream,L)){FString C;if(L.TrimStartAndEnd().StartsWith(TEXT("Begin Object "))&&FParse::Value(*L,TEXT("Class="),C)&&C==TEXT("/Script/BlueprintGraph.K2Node_Timeline"))return true;}return false;}

struct FTITimelineImport
{
    FResult& R;FCollapsedTarget& Target;FCollapsedTarget Source;FTITimelineText& TextPlan;
    TStrongObjectPtr<UTimelineTemplate> Origin,Created;TStrongObjectPtr<UK2Node_Timeline> Pasted;
    TArray<TObjectPtr<UTimelineTemplate>> SourceSlots,SourceGenerated,SourceSkeleton;
    struct FObject{TStrongObjectPtr<UObject> Hold,Outer;TStrongObjectPtr<UClass> Class;FString Path;};
    struct FSourceTemplate{FObject Role;FString Value;};TArray<FSourceTemplate> SourceTemplates;
    struct FSourceCurve{FObject Role;TStrongObjectPtr<UObject> Import;TStrongObjectPtr<UPackage> Package;FString Value;};TArray<FSourceCurve> SourceCurves;
    struct FDisk{TStrongObjectPtr<UPackage> Package;FString File;int64 Bytes=-1;FMD5Hash Hash;bool Dirty=false;};TArray<FDisk> Disks;
    struct FSourceNode{UK2Node_Timeline* Node=nullptr;FName Name;FGuid Guid;uint32 Flags=0;};TArray<FSourceNode> SourceNodes;
    FString Semantic,CreatedIdentity;FGuid CreatedGuid;FName NewName;TArray<FObject> FreshCurves;FObject FreshTemplate;bool Ready=false;
    // TO preserves authored target templates even when their graph nodes are removed.
    // Compiler-derived class arrays are tracked separately from Blueprint authority.
    struct FOldCurve{FObject Role;TStrongObjectPtr<UPackage> Package;TStrongObjectPtr<UObject> Import;FString Value;FName Name;int32 OuterRole=-1;};
    struct FOldTemplate{FObject Role;FString Identity;FName Name;FGuid Guid;int32 OuterRole=-1;TArray<FOldCurve> Curves;};
    TArray<TUniquePtr<FOldTemplate>> OldTemplates;TArray<FSourceNode> OldNodes;
    TArray<FString> OldGeneratedKeys,OldSkeletonKeys,CurrentGeneratedKeys,CurrentSkeletonKeys;
    TArray<TStrongObjectPtr<UTimelineTemplate>> CurrentGeneratedSlots,CurrentSkeletonSlots;
    bool PasteReturned=false;

    explicit FTITimelineImport(FResult& In,FCollapsedTarget& Lease,FTITimelineText& Plan):R(In),Target(Lease),Source(In),TextPlan(Plan){}
    bool Bad(const FString& Field,const FString& Expected,const FString& Actual)
    {
        if(!Target.Stopped){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("field"),Field);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);R.Data->SetObjectField(TEXT("timeline_first_mismatch"),J);}return Target.Bad(TEXT("timeline_current_or_postcondition"),Field);
    }
    bool Pending(const TCHAR* Why){return R.bMutationAttempted?Bad(TEXT("timeline_profile_changed"),TEXT("preflight-admitted native roles"),Why):Fail(R,TEXT("timeline_clipboard_profile_pending"),Why);}
    bool Context()
    {
        if(Target.Stopped||Source.Stopped)return false;
        if(!IsEditorGameThread()||IsPlaySessionActive()||GEditor!=Target.Editor.Get()||GWorld!=Target.World.Get()||!Loaded(Target.Editor.Get())||!Loaded(Target.World.Get())||!Loaded(Target.BP.Get())||!Loaded(Target.Package.Get())||!Loaded(Target.Selected))return Bad(TEXT("editor_owner_context"),TEXT("held idle editor/world/target"),TEXT("changed/unavailable"));
        auto* B=Target.BP.Get();auto Parent=B->ParentClass;
        if(B->GetClass()!=UBlueprint::StaticClass()||B->GetOutermost()!=Target.Package.Get()||B->GetPathName()!=Target.BPPath||Target.Package->GetName()!=Target.PackagePath||!Parent.GetGCPtr().IsResolved()||Parent.GetGCPtr().Get()!=Target.Parent||!Loaded(Target.Parent)||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad)return Bad(TEXT("target_blueprint_identity"),Target.BPPath,TEXT("changed/unavailable"));
        for(const auto& E:Target.Graphs)if(E.Object==Target.Selected){auto* G=Target.Selected;auto Schema=G->Schema;return (G->GetClass()==UEdGraph::StaticClass()&&G->GetOuter()==E.Outer&&G->GetOuter()==B&&G->GetPathName()==E.Path&&G->GraphGuid==E.Guid&&Schema.GetGCPtr().IsResolved()&&Schema.GetGCPtr().Get()==UEdGraphSchema_K2::StaticClass())||Bad(TEXT("target_graph_identity"),E.Path,TEXT("changed/unavailable"));}
        return Bad(TEXT("target_graph_identity"),TEXT("captured selected graph"),TEXT("missing role"));
    }
    static FString Bits(float V){uint32 B=0;FMemory::Memcpy(&B,&V,sizeof(B));return FString::Printf(TEXT("%08x"),B);}
    static FString Rich(const FRichCurve& V)
    {
        FString S=Bits(V.DefaultValue)+TEXT("/")+FString::FromInt(V.PreInfinityExtrap)+TEXT("/")+FString::FromInt(V.PostInfinityExtrap);
        for(const auto& K:V.GetConstRefOfKeys())S+=TEXT("|")+FString::FromInt(K.InterpMode)+TEXT(",")+FString::FromInt(K.TangentMode)+TEXT(",")+FString::FromInt(K.TangentWeightMode)+TEXT(",")+Bits(K.Time)+TEXT(",")+Bits(K.Value)+TEXT(",")+Bits(K.ArriveTangent)+TEXT(",")+Bits(K.ArriveTangentWeight)+TEXT(",")+Bits(K.LeaveTangent)+TEXT(",")+Bits(K.LeaveTangentWeight);return S;
    }
    static FString CurveValue(UCurveBase* V)
    {
        FString S;if(V->GetClass()==UCurveFloat::StaticClass()){const auto* F=static_cast<UCurveFloat*>(V);return FString::FromInt(F->bIsEventCurve)+TEXT(":")+Rich(F->FloatCurve);}
        if(V->GetClass()==UCurveVector::StaticClass()){for(const auto& K:static_cast<UCurveVector*>(V)->FloatCurves)S+=TEXT(";")+Rich(K);}
        else{const auto* F=static_cast<UCurveLinearColor*>(V);for(const auto& K:F->FloatCurves)S+=TEXT(";")+Rich(K);for(float A:{F->AdjustHue,F->AdjustSaturation,F->AdjustBrightness,F->AdjustBrightnessCurve,F->AdjustVibrance,F->AdjustMinAlpha,F->AdjustMaxAlpha})S+=TEXT(";")+Bits(A);}return S;
    }
    bool Object(UObject* O,FObject& V)
    {
        V.Hold.Reset(O);if(!Loaded(O))return Bad(TEXT("returned_object"),TEXT("live held object"),TEXT("unavailable"));V.Class.Reset(O->GetClass());V.Outer.Reset(O->GetOuter());if(!Loaded(V.Class.Get())||!Loaded(V.Outer.Get()))return Bad(TEXT("returned_object_role"),TEXT("live actual class/outer"),TEXT("unavailable"));V.Path=O->GetPathName();return true;
    }
    bool ObjectCurrent(const FObject& V)
    {auto* O=V.Hold.Get();return (Loaded(O)&&Loaded(V.Class.Get())&&Loaded(V.Outer.Get())&&O->GetClass()==V.Class.Get()&&O->GetOuter()==V.Outer.Get()&&O->GetPathName()==V.Path)||Bad(TEXT("captured_object_role"),V.Path,TEXT("changed/unavailable"));}
    bool Disk(UPackage* P)
    {
        for(const auto& D:Disks)if(D.Package.Get()==P)return true;FDisk D;D.Package.Reset(P);if(!Loaded(P)||P==Target.Package.Get()||!P->GetName().StartsWith(TEXT("/Game/")))return Pending(TEXT("Readonly source packages must be distinct loaded /Game packages."));D.Dirty=P->IsDirty();if(D.Dirty||!FPackageName::TryConvertLongPackageNameToFilename(P->GetName(),D.File,FPackageName::GetAssetPackageExtension()))return Pending(TEXT("Saved clean source packages required; dirty/unsaved originals remain incomplete."));D.File=FPaths::ConvertRelativePathToFull(D.File);D.Bytes=IFileManager::Get().FileSize(*D.File);if(D.Bytes<0)return Pending(TEXT("Readonly source disk file must exist."));D.Hash=FMD5Hash::HashFile(*D.File);if(!D.Hash.IsValid())return Pending(TEXT("Readonly source disk hash unavailable."));Disks.Add(MoveTemp(D));return Context();
    }
    bool Curve(UCurveBase* V,bool Capture)
    {
        if(!V)return Pending(TEXT("Non-null native curve tracks required; invalid/missing curve sources remain incomplete."));
        if(!Loaded(V)||(V->GetClass()!=UCurveFloat::StaticClass()&&V->GetClass()!=UCurveVector::StaticClass()&&V->GetClass()!=UCurveLinearColor::StaticClass()))return Pending(TEXT("Exact loaded native Float/Vector/LinearColor curves required."));
        int32 Keys=0;if(auto* F=Cast<UCurveFloat>(V))Keys=F->FloatCurve.GetNumKeys();else if(auto* VectorCurve=Cast<UCurveVector>(V))for(const auto& K:VectorCurve->FloatCurves)Keys+=K.GetNumKeys();else for(const auto& K:static_cast<UCurveLinearColor*>(V)->FloatCurves)Keys+=K.GetNumKeys();if(Keys>4096)return Pending(TEXT("At most4096 keys per curve in the initial Timeline paste scope."));
        if(Capture){for(const auto& E:SourceCurves)if(E.Role.Hold.Get()==V)return true;FSourceCurve E;if(!Object(V,E.Role)||!V->AssetImportData.IsResolved())return Pending(TEXT("Curve import-data role must already be resolved."));E.Import.Reset(V->AssetImportData.Get());E.Package.Reset(V->GetPackage());if(E.Import.Get()&&!Loaded(E.Import.Get()))return Bad(TEXT("readonly_curve_import"),TEXT("nullable live import-data"),TEXT("unavailable"));if(!Disk(E.Package.Get()))return false;E.Value=CurveValue(V);SourceCurves.Add(MoveTemp(E));}return true;
    }
    bool Values(UTimelineTemplate* T,FString& Out,bool Identity,bool Capture)
    {
        if(!Loaded(T)||T->GetClass()!=UTimelineTemplate::StaticClass())return Bad(TEXT("timeline_template"),TEXT("exact loaded native template"),TEXT("unavailable/other class"));
        if(T->EventTracks.Num()+T->FloatTracks.Num()+T->VectorTracks.Num()+T->LinearColorTracks.Num()>64||T->MetaDataArray.Num()>128)return Pending(TEXT("Bounded64 Timeline tracks/128 metadata entries required."));
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("length"),Bits(T->TimelineLength));J->SetNumberField(TEXT("mode"),T->LengthMode);J->SetNumberField(TEXT("flags"),uint32(T->bAutoPlay)|(uint32(T->bLoop)<<1)|(uint32(T->bReplicated)<<2)|(uint32(T->bIgnoreTimeDilation)<<3));J->SetNumberField(TEXT("tick"),T->TimelineTickGroup);
        if(Identity){J->SetStringField(TEXT("guid"),T->TimelineGuid.ToString());J->SetStringField(TEXT("variable"),T->GetVariableName().ToString());J->SetStringField(TEXT("direction"),T->GetDirectionPropertyName().ToString());J->SetStringField(TEXT("update"),T->GetUpdateFunctionName().ToString());J->SetStringField(TEXT("finished"),T->GetFinishedFunctionName().ToString());}
        auto Track=[&](const FTTTrackBase& A,UCurveBase* C,const FString& Cache,TArray<TSharedPtr<FJsonValue>>& Rows)->bool
        {if(!Curve(C,Capture))return false;auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("name"),A.GetTrackName().ToString());Q->SetBoolField(TEXT("external"),A.bIsExternalCurve);Q->SetBoolField(TEXT("expanded"),A.bIsExpanded);Q->SetBoolField(TEXT("synchronized"),A.bIsCurveViewSynchronized);Q->SetStringField(TEXT("curve_value"),CurveValue(C));if(Identity){Q->SetStringField(TEXT("cache"),Cache);Q->SetStringField(TEXT("curve"),C->GetPathName());}Rows.Add(MakeShared<FJsonValueObject>(Q));return true;};
        TArray<TSharedPtr<FJsonValue>> Rows;
        for(const auto& E:T->EventTracks){if(!E.CurveKeys.IsResolved())return Bad(TEXT("curve_slot"),TEXT("resolved"),TEXT("unresolved"));Target.Hold(E.CurveKeys.Get());if(!Track(E,E.CurveKeys.Get(),E.GetFunctionName().ToString(),Rows))return false;}J->SetArrayField(TEXT("events"),Rows);Rows.Reset();
        for(const auto& E:T->FloatTracks){if(!E.CurveFloat.IsResolved())return Bad(TEXT("curve_slot"),TEXT("resolved"),TEXT("unresolved"));Target.Hold(E.CurveFloat.Get());if(!Track(E,E.CurveFloat.Get(),E.GetPropertyName().ToString(),Rows))return false;}J->SetArrayField(TEXT("floats"),Rows);Rows.Reset();
        for(const auto& E:T->VectorTracks){if(!E.CurveVector.IsResolved())return Bad(TEXT("curve_slot"),TEXT("resolved"),TEXT("unresolved"));Target.Hold(E.CurveVector.Get());if(!Track(E,E.CurveVector.Get(),E.GetPropertyName().ToString(),Rows))return false;}J->SetArrayField(TEXT("vectors"),Rows);Rows.Reset();
        for(const auto& E:T->LinearColorTracks){if(!E.CurveLinearColor.IsResolved())return Bad(TEXT("curve_slot"),TEXT("resolved"),TEXT("unresolved"));Target.Hold(E.CurveLinearColor.Get());if(!Track(E,E.CurveLinearColor.Get(),E.GetPropertyName().ToString(),Rows))return false;}J->SetArrayField(TEXT("colors"),Rows);Rows.Reset();
        for(const auto& E:T->MetaDataArray){auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("key"),E.DataKey.ToString());Q->SetStringField(TEXT("value"),E.DataValue);Rows.Add(MakeShared<FJsonValueObject>(Q));}J->SetArrayField(TEXT("metadata"),Rows);Rows.Reset();
        const int32 N=T->GetNumDisplayTracks();const int32 Sizes[]={T->EventTracks.Num(),T->FloatTracks.Num(),T->VectorTracks.Num(),T->LinearColorTracks.Num()};TSet<FString> Display;
        if(N<0||N>64)return Pending(TEXT("Bounded Timeline display order required."));for(int32 I=0;I<N;++I){const auto D=T->GetDisplayTrackId(I);if(D.TrackType<0||D.TrackType>3||D.TrackIndex<0||D.TrackIndex>=Sizes[D.TrackType])return Pending(TEXT("Canonical valid native display-track indices required."));const FString Key=FString::FromInt(D.TrackType)+TEXT(":")+FString::FromInt(D.TrackIndex);if(Display.Contains(Key))return Pending(TEXT("Duplicate display-track indices remain incomplete."));Display.Add(Key);Rows.Add(MakeShared<FJsonValueString>(Key));}if(N!=Sizes[0]+Sizes[1]+Sizes[2]+Sizes[3])return Pending(TEXT("Every track must have one display slot in this profile."));J->SetArrayField(TEXT("display"),Rows);Out=Json(J);return true;
    }
    static uint32 NodeFlags(const UK2Node_Timeline* N){return uint32(N->bAutoPlay)|(uint32(N->bLoop)<<1)|(uint32(N->bReplicated)<<2)|(uint32(N->bIgnoreTimeDilation)<<3);}
    bool SourceCurrent(bool Files=false)
    {
        if(!Context()||!Source.Plain())return false;auto* B=Source.BP.Get();if(B->Timelines!=SourceSlots||static_cast<UBlueprintGeneratedClass*>(Source.Generated.Get())->Timelines!=SourceGenerated||static_cast<UBlueprintGeneratedClass*>(Source.Skeleton.Get())->Timelines!=SourceSkeleton)return Bad(TEXT("readonly_timeline_inventory"),TEXT("captured Blueprint/Gen/Skel arrays"),TEXT("changed"));
        // Close all retained curve identities before any track's content/path getter.
        for(const auto& E:SourceCurves){if(!ObjectCurrent(E.Role))return false;if(!Loaded(E.Package.Get())||(E.Import.Get()&&!Loaded(E.Import.Get())))return Bad(TEXT("readonly_curve_anchors"),E.Role.Path,TEXT("package/import unavailable"));auto* C=static_cast<UCurveBase*>(E.Role.Hold.Get());if(C->GetPackage()!=E.Package.Get()||!C->AssetImportData.IsResolved()||C->AssetImportData.Get()!=E.Import.Get())return Bad(TEXT("readonly_curve_roles"),E.Role.Path,TEXT("changed"));const FString A=CurveValue(C);if(A!=E.Value)return Bad(TEXT("readonly_curve_values"),E.Value,A);}
        for(const auto& E:SourceTemplates){if(!ObjectCurrent(E.Role))return false;FString A;if(!Values(static_cast<UTimelineTemplate*>(E.Role.Hold.Get()),A,true,false))return false;if(A!=E.Value)return Bad(TEXT("readonly_template_values"),E.Value,A);}
        for(const auto& E:SourceNodes){if(!Loaded(E.Node)||E.Node->TimelineName!=E.Name||E.Node->TimelineGuid!=E.Guid||NodeFlags(E.Node)!=E.Flags)return Bad(TEXT("readonly_timeline_node"),E.Name.ToString(),TEXT("changed"));}
        for(const auto& D:Disks){if(!Loaded(D.Package.Get())||D.Package->IsDirty()!=D.Dirty)return Bad(TEXT("readonly_package_dirty"),TEXT("captured false"),TEXT("changed/unavailable"));if(Files&&(IFileManager::Get().FileSize(*D.File)!=D.Bytes||FMD5Hash::HashFile(*D.File)!=D.Hash))return Bad(TEXT("readonly_source_file"),D.File,TEXT("size/hash changed"));}return Context();
    }
    bool FindOrigin()
    {
        UTimelineTemplate* First=nullptr;int32 Matches=0;for(TObjectIterator<UTimelineTemplate> It;It;++It){auto* T=*It;if(T->TimelineGuid==TextPlan.Selector){if(!First)First=T;++Matches;}}
        if(!Origin.Get()){Origin.Reset(First);if(Matches!=1||!Loaded(First)||First->GetClass()!=UTimelineTemplate::StaticClass())return Pending(TEXT("Exactly one live native source template matching the installed global GUID lookup is required; missing/ambiguous loaded sources remain incomplete."));}
        return (First==Origin.Get()&&Matches==1)||Bad(TEXT("global_template_selector"),TEXT("same unique loaded source"),FString::FromInt(Matches));
    }
    static FString TimelineKey(UTimelineTemplate* T){return T->TimelineGuid.ToString()+TEXT(":")+T->GetVariableName().ToString();}
    bool OldIdentity(UTimelineTemplate* T,FString& Out)
    {
        FString Full;if(!Values(T,Full,true,false))return false;TSharedPtr<FJsonObject> J;
        if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Full),J)||!J)return Bad(TEXT("old_template_identity_copy"),TEXT("copied native Timeline projection"),TEXT("unavailable"));
        // Curve paths may follow a reconstructed owning class. All authored curve
        // values and exact logical ownership are compared separately below.
        for(const TCHAR* K:{TEXT("events"),TEXT("floats"),TEXT("vectors"),TEXT("colors")})for(const auto& V:J->GetArrayField(K))V->AsObject()->RemoveField(TEXT("curve"));
        Out=Json(J.ToSharedRef());return true;
    }
    int32 OwnerRole(UObject* O,UTimelineTemplate* Template)const
    {if(O==Template)return 0;if(O==Target.BP.Get())return 1;if(O==Target.Generated.Get())return 2;if(O==Target.Skeleton.Get())return 3;if(O&&O->IsA<UPackage>())return 4;return -1;}
    UObject* ExpectedOuter(int32 Role,UTimelineTemplate* T,const FObject& Before)const
    {switch(Role){case 0:return T;case 1:return Target.BP.Get();case 2:return Target.Generated.Get();case 3:return Target.Skeleton.Get();case 4:return Before.Outer.Get();default:return nullptr;}}
    bool CurveSlots(UTimelineTemplate* T,TArray<UCurveBase*>& Out)
    {
        const auto Add=[&](const auto& Slot)->bool{if(!Slot.IsResolved())return Bad(TEXT("old_curve_slot"),TEXT("resolved actual curve"),TEXT("unresolved"));auto* C=Slot.Get();Target.Hold(C);if(!Curve(C,false))return false;Out.Add(C);return true;};
        for(const auto& E:T->EventTracks)if(!Add(E.CurveKeys))return false;for(const auto& E:T->FloatTracks)if(!Add(E.CurveFloat))return false;for(const auto& E:T->VectorTracks)if(!Add(E.CurveVector))return false;for(const auto& E:T->LinearColorTracks)if(!Add(E.CurveLinearColor))return false;return true;
    }
    bool ClassTimelineSlots(UBlueprintGeneratedClass* C,TArray<FString>& Keys,TArray<TStrongObjectPtr<UTimelineTemplate>>& Holds)
    {
        if(!Loaded(C)||C->Timelines.Num()>64)return Bad(TEXT("class_timeline_inventory"),TEXT("live bounded generated/skeleton class"),TEXT("unavailable/exceeds64"));
        for(const auto& Slot:C->Timelines){if(!Slot.IsResolved())return Bad(TEXT("class_timeline_slot"),TEXT("resolved"),TEXT("unresolved"));auto* T=Slot.Get();TStrongObjectPtr<UTimelineTemplate> Held(T);if(!Loaded(T)||T->GetClass()!=UTimelineTemplate::StaticClass()||!Target.BP->Timelines.Contains(T))return Pending(TEXT("Compiler template arrays must refer to the current authored Blueprint Timeline slots; stale/foreign derived caches remain incomplete."));Keys.Add(TimelineKey(T));Holds.Add(MoveTemp(Held));}return true;
    }
    bool BindOldTarget()
    {
        if(!Context()||!Target.Plain())return false;auto* B=Target.BP.Get();if(B->Timelines.Num()>63)return Pending(TEXT("At most63 existing authored templates plus the one pasted Timeline are admitted."));
        for(const auto& Slot:B->Timelines){if(!Slot.IsResolved())return Pending(TEXT("Existing target Timeline slots must be resolved."));Target.Hold(Slot.Get());}
        TSet<FString> Keys;
        for(const auto& Slot:B->Timelines)
        {
            auto* T=Slot.Get();auto E=MakeUnique<FOldTemplate>();if(!Object(T,E->Role)||T->GetClass()!=UTimelineTemplate::StaticClass()||T->GetPackage()!=Target.Package.Get())return Pending(TEXT("Existing target templates must be exact native Timeline objects in the target package."));
            E->OuterRole=OwnerRole(T->GetOuter(),nullptr);if(E->OuterRole<1||E->OuterRole>3)return Pending(TEXT("Existing template owner must be its Blueprint or actual generated/skeleton class."));
            E->Name=T->GetFName();E->Guid=T->TimelineGuid;if(!E->Guid.IsValid()||Keys.Contains(TimelineKey(T)))return Pending(TEXT("Unique valid authored target Timeline GUID/name roles required."));Keys.Add(TimelineKey(T));
            if(!OldIdentity(T,E->Identity))return false;TArray<UCurveBase*> Curves;if(!CurveSlots(T,Curves))return false;
            for(auto* C:Curves){FOldCurve V;if(!Object(C,V.Role)||!C->AssetImportData.IsResolved())return Pending(TEXT("Existing curve import-data slot must be resolved."));V.Package.Reset(C->GetPackage());V.Import.Reset(C->AssetImportData.Get());if(!Loaded(V.Package.Get())||(V.Import.Get()&&!Loaded(V.Import.Get())))return Pending(TEXT("Existing curve package/import-data must be live."));V.Name=C->GetFName();V.OuterRole=OwnerRole(C->GetOuter(),T);if(V.OuterRole<0)return Pending(TEXT("Existing curve must belong to its Timeline, Blueprint, actual class or a package."));if(V.Package.Get()!=Target.Package.Get()&&!Disk(V.Package.Get()))return false;V.Value=CurveValue(C);E->Curves.Add(MoveTemp(V));}
            OldTemplates.Add(MoveTemp(E));
        }
        for(const auto& N:Target.Nodes)if(N.Class==UK2Node_Timeline::StaticClass()){auto* T=static_cast<UK2Node_Timeline*>(N.Object);FSourceNode E;E.Node=T;E.Name=T->TimelineName;E.Guid=T->TimelineGuid;E.Flags=NodeFlags(T);OldNodes.Add(E);}
        if(!ClassTimelineSlots(static_cast<UBlueprintGeneratedClass*>(Target.Generated.Get()),OldGeneratedKeys,CurrentGeneratedSlots)||!ClassTimelineSlots(static_cast<UBlueprintGeneratedClass*>(Target.Skeleton.Get()),OldSkeletonKeys,CurrentSkeletonSlots))return false;
        CurrentGeneratedKeys=OldGeneratedKeys;CurrentSkeletonKeys=OldSkeletonKeys;R.Data->SetNumberField(TEXT("preserved_prior_timeline_count"),OldTemplates.Num());return OldTargetCurrent(false);
    }
    bool OldTargetCurrent(bool Rebind)
    {
        if(!Context())return false;auto* B=Target.BP.Get();const int32 Expected=OldTemplates.Num()+(PasteReturned?1:0);
        if(B->Timelines.Num()!=Expected)return Bad(TEXT("target_timeline_inventory"),FString::FromInt(Expected),FString::FromInt(B->Timelines.Num()));
        for(const auto& Slot:B->Timelines){if(!Slot.IsResolved())return Bad(TEXT("target_timeline_slot"),TEXT("resolved"),TEXT("unresolved"));Target.Hold(Slot.Get());}
        for(int32 I=0;I<OldTemplates.Num();++I)
        {
            auto& E=*OldTemplates[I];auto* T=B->Timelines[I].Get();TStrongObjectPtr<UTimelineTemplate> Held(T);
            if(!Loaded(T)||T->GetClass()!=UTimelineTemplate::StaticClass())return Bad(TEXT("preserved_template_role"),E.Role.Path,TEXT("unavailable or nonnative class before typed getter"));
            const FString ActualRole=T->GetFName().ToString()+TEXT("/")+T->TimelineGuid.ToString()+TEXT("/")+GetPathNameSafe(T->GetOuter())+TEXT("/")+GetPathNameSafe(T->GetPackage());
            const FString ExpectedRole=E.Name.ToString()+TEXT("/")+E.Guid.ToString()+TEXT("/")+GetPathNameSafe(ExpectedOuter(E.OuterRole,T,E.Role))+TEXT("/")+Target.PackagePath;
            if(T->GetFName()!=E.Name||T->TimelineGuid!=E.Guid||T->GetPackage()!=Target.Package.Get()||T->GetOuter()!=ExpectedOuter(E.OuterRole,T,E.Role))return Bad(TEXT("preserved_template_role"),ExpectedRole,ActualRole);
            if(!Rebind&&(T!=E.Role.Hold.Get()||!ObjectCurrent(E.Role)))return Bad(TEXT("preserved_template_identity"),E.Role.Path,TEXT("unexpected slot replacement"));
            FString Value;if(!OldIdentity(T,Value))return false;if(Value!=E.Identity)return Bad(TEXT("preserved_template_authored_values"),E.Identity,Value);
            TArray<UCurveBase*> Curves;if(!CurveSlots(T,Curves))return false;if(Curves.Num()!=E.Curves.Num())return Bad(TEXT("preserved_curve_count"),FString::FromInt(E.Curves.Num()),FString::FromInt(Curves.Num()));
            for(int32 J=0;J<Curves.Num();++J){auto& V=E.Curves[J];auto* C=Curves[J];if(!Loaded(C)||C->GetClass()!=V.Role.Class.Get()||C->GetFName()!=V.Name||C->GetPackage()!=V.Package.Get()||C->GetOuter()!=ExpectedOuter(V.OuterRole,T,V.Role)||!C->AssetImportData.IsResolved()||C->AssetImportData.Get()!=V.Import.Get()||(V.Import.Get()&&!Loaded(V.Import.Get())))return Bad(TEXT("preserved_curve_role"),V.Role.Path,TEXT("changed/unavailable"));if((!Rebind||V.OuterRole==4)&&(C!=V.Role.Hold.Get()||!ObjectCurrent(V.Role)))return Bad(TEXT("preserved_curve_identity"),V.Role.Path,TEXT("unexpected replacement"));const FString Actual=CurveValue(C);if(Actual!=V.Value)return Bad(TEXT("preserved_curve_authored_values"),V.Value,Actual);if(Rebind){FObject Fresh;if(!Object(C,Fresh))return false;V.Role=MoveTemp(Fresh);}}
            if(Rebind){FObject Fresh;if(!Object(T,Fresh))return false;E.Role=MoveTemp(Fresh);}
        }
        for(const auto& E:OldNodes)if(!Target.ClearRetired.Contains(E.Node)){if(!Loaded(E.Node)||E.Node->TimelineName!=E.Name||E.Node->TimelineGuid!=E.Guid||NodeFlags(E.Node)!=E.Flags)return Bad(TEXT("surviving_timeline_node"),E.Name.ToString(),TEXT("changed/unavailable"));}
        TArray<FString> G,S,Full;TArray<TStrongObjectPtr<UTimelineTemplate>> GH,SH;
        for(const auto& Slot:B->Timelines){auto* T=Slot.Get();if(!Loaded(T)||T->GetClass()!=UTimelineTemplate::StaticClass())return Bad(TEXT("target_template_slot_class"),TEXT("live native Timeline"),TEXT("unavailable/other"));Full.Add(TimelineKey(T));}
        if(!ClassTimelineSlots(static_cast<UBlueprintGeneratedClass*>(Target.Generated.Get()),G,GH)||!ClassTimelineSlots(static_cast<UBlueprintGeneratedClass*>(Target.Skeleton.Get()),S,SH))return false;
        // Compiler FinishCompilingClass assigns the Blueprint array. A class not
        // rebuilt in this callback may retain its captured pre-paste array.
        if(Rebind){if((G!=OldGeneratedKeys&&G!=Full)||(S!=OldSkeletonKeys&&S!=Full))return Bad(TEXT("compiler_timeline_array_transition"),TEXT("captured derived array or exact authored current array"),TEXT("different logical slots"));CurrentGeneratedKeys=G;CurrentSkeletonKeys=S;CurrentGeneratedSlots=MoveTemp(GH);CurrentSkeletonSlots=MoveTemp(SH);}
        else{if(G!=CurrentGeneratedKeys||S!=CurrentSkeletonKeys||GH.Num()!=CurrentGeneratedSlots.Num()||SH.Num()!=CurrentSkeletonSlots.Num())return Bad(TEXT("compiler_timeline_array_current"),TEXT("captured current slots"),TEXT("changed"));for(int32 I=0;I<GH.Num();++I)if(GH[I].Get()!=CurrentGeneratedSlots[I].Get())return Bad(TEXT("generated_timeline_slot_current"),TEXT("same current template"),TEXT("replaced"));for(int32 I=0;I<SH.Num();++I)if(SH[I].Get()!=CurrentSkeletonSlots[I].Get())return Bad(TEXT("skeleton_timeline_slot_current"),TEXT("same current template"),TEXT("replaced"));}
        return Context();
    }
    bool Bind()
    {
        if(!Target.BP->ComponentTemplates.IsEmpty()||!Target.BP->ImplementedInterfaces.IsEmpty()||!Target.BP->NewVariables.IsEmpty())return Pending(TEXT("Timeline destination own declarations/component templates/interfaces remain incomplete."));
        if(!Target.BP->UbergraphPages.Contains(Target.Selected))return Pending(TEXT("An existing direct EventGraph is required for Timeline append/clear; function/Anim root variants remain incomplete."));if(!BindOldTarget())return false;
        if(!FindOrigin())return false;auto* O=Origin.Get();Target.Hold(O);Target.Hold(O->GetOuter());auto* GC=Cast<UBlueprintGeneratedClass>(O->GetOuter());if(!Loaded(GC)||!GC->ClassGeneratedBy.IsResolved())return Pending(TEXT("Source Timeline must belong to a resolved generated Blueprint class."));auto* B=Cast<UBlueprint>(GC->ClassGeneratedBy.Get());Target.Hold(B);
        if(!Loaded(B)||B==Target.BP.Get()||!Scope(B,R)||!B->Timelines.Contains(O))return Pending(TEXT("Loaded distinct ordinary source Blueprint with actual template membership required."));
        UEdGraph* SG=nullptr;for(const auto& G:B->UbergraphPages){if(!G.IsResolved())return Pending(TEXT("Source graph slots must be resolved."));if(Loaded(G.Get())){SG=G.Get();break;}}if(!SG)return Pending(TEXT("Source requires a current direct EventGraph."));if(!Source.Bind(B,SG)||!Disk(B->GetOutermost()))return false;
        SourceSlots=B->Timelines;SourceGenerated=static_cast<UBlueprintGeneratedClass*>(Source.Generated.Get())->Timelines;SourceSkeleton=static_cast<UBlueprintGeneratedClass*>(Source.Skeleton.Get())->Timelines;if(SourceSlots.Num()>64||SourceGenerated.Num()>64||SourceSkeleton.Num()>64)return Pending(TEXT("Bounded source template inventories required."));
        for(const auto* Slots:{&SourceSlots,&SourceGenerated,&SourceSkeleton})for(const auto& Slot:*Slots){if(!Slot.IsResolved())return Pending(TEXT("Source template slots must be resolved."));auto* T=Slot.Get();bool Seen=false;for(const auto& E:SourceTemplates)Seen|=E.Role.Hold.Get()==T;if(Seen)continue;FSourceTemplate E;if(!Object(T,E.Role))return false;if(T->GetClass()!=UTimelineTemplate::StaticClass()||T->GetPackage()!=Source.Package.Get()||(T->GetOuter()!=B&&T->GetOuter()!=Source.Generated.Get()&&T->GetOuter()!=Source.Skeleton.Get()))return Pending(TEXT("Exact own source Timeline templates required."));if(!Values(T,E.Value,true,true))return false;SourceTemplates.Add(MoveTemp(E));}
        for(const auto& N:Source.Nodes)if(N.Class==UK2Node_Timeline::StaticClass()){auto* T=static_cast<UK2Node_Timeline*>(N.Object);FSourceNode V;V.Node=T;V.Name=T->TimelineName;V.Guid=T->TimelineGuid;V.Flags=NodeFlags(T);SourceNodes.Add(V);}
        if(!Values(O,Semantic,false,false)||!SourceCurrent(true))return false;R.Data->SetStringField(TEXT("timeline_source_template"),O->GetPathName());R.Data->SetStringField(TEXT("timeline_source_selector_guid"),TextPlan.Selector.ToString());R.Data->SetStringField(TEXT("timeline_source_blueprint"),B->GetPathName());return true;
    }
    bool CheckPinText()
    {
        TSet<FGuid> Ids;TSet<FString> Names;
        for(const auto& M:TextPlan.Pins){FGuid Id;const FString Name=FCollapsedTextPlan::Unquote(M.FindRef(TEXT("PinName")));if(!FCollapsedTextPlan::Name(Name)||!FGuid::Parse(M.FindRef(TEXT("PinId")),Id)||!Id.IsValid()||Ids.Contains(Id)||Names.Contains(Name))return Pending(TEXT("Unique copied pin names and valid GUIDs required."));Ids.Add(Id);Names.Add(Name);
            for(const auto& P:M){const FString V=FCollapsedTextPlan::Unquote(P.Value);const auto& K=P.Key;if(K==TEXT("PinId")||K==TEXT("PinName"))continue;
                if(K==TEXT("LinkedTo")){TArray<FString> L;if(!FCollapsedTextPlan::Pieces(P.Value,L)||!L.IsEmpty())return Pending(TEXT("Single Timeline paste currently requires no input/external pin links."));continue;}
                if(K==TEXT("Direction")&&(V==TEXT("EGPD_Input")||V==TEXT("EGPD_Output")))continue;
                if(K==TEXT("PinType.PinCategory")&&(V==TEXT("exec")||V==TEXT("real")||V==TEXT("byte")||V==TEXT("struct")))continue;
                if(K==TEXT("PinType.PinSubCategory")&&(V.IsEmpty()||V==TEXT("float")))continue;
                if(K==TEXT("PinType.PinSubCategoryObject")&&(V==TEXT("None")||V==TEXT("/Script/CoreUObject.Enum'/Script/Engine.ETimelineDirection'")||V==TEXT("Enum'/Script/Engine.ETimelineDirection'")||V==TEXT("/Script/CoreUObject.ScriptStruct'/Script/CoreUObject.Vector'")||V==TEXT("ScriptStruct'/Script/CoreUObject.Vector'")||V==TEXT("/Script/CoreUObject.ScriptStruct'/Script/CoreUObject.LinearColor'")||V==TEXT("ScriptStruct'/Script/CoreUObject.LinearColor'")))continue;
                if(K==TEXT("DefaultValue")||K==TEXT("AutogeneratedDefaultValue")){double D=0;if(V.IsEmpty()||V==TEXT("Forward")||(LexTryParseString(D,*V)&&FMath::IsFinite(D)))continue;}
                if(K==TEXT("DefaultObject")&&V==TEXT("None"))continue;
                if(K==TEXT("PersistentGuid")){FGuid G;if(FGuid::Parse(V,G)&&!G.IsValid())continue;}
                if(K==TEXT("PinType.ContainerType")&&V==TEXT("None"))continue;if((K==TEXT("PinType.PinValueType")||K==TEXT("PinType.PinSubCategoryMemberReference"))&&V==TEXT("()"))continue;
                if((K==TEXT("PinType.bIsReference")||K==TEXT("PinType.bIsConst")||K==TEXT("PinType.bIsWeakPointer")||K==TEXT("PinType.bIsUObjectWrapper")||K==TEXT("PinType.bSerializeAsSinglePrecisionFloat")||K==TEXT("bHidden")||K==TEXT("bNotConnectable")||K==TEXT("bDefaultValueIsReadOnly")||K==TEXT("bDefaultValueIsIgnored")||K==TEXT("bAdvancedView")||K==TEXT("bOrphanedPin"))&&V==TEXT("False"))continue;
                return Pending(TEXT("Additional pin references/text/default flags remain incomplete; no serialized field is silently discarded."));
            }}return true;
    }
    static FString Sanitized(FString S){for(int32 I=0;I<S.Len();++I){const TCHAR C=S[I];if(!((C>=TEXT('A')&&C<=TEXT('Z'))||(C>=TEXT('a')&&C<=TEXT('z'))||C==TEXT('_')||(I>0&&C>=TEXT('0')&&C<=TEXT('9'))))S[I]=TEXT('_');}return S;}
    bool Caches(UTimelineTemplate* T)
    {
        const FString Name=T->GetVariableName().ToString(),Guid=T->TimelineGuid.ToString();
        if(T->GetDirectionPropertyName()!=FName(*Sanitized(Name+TEXT("__Direction_")+Guid))||T->GetUpdateFunctionName()!=FName(*(Name+TEXT("__UpdateFunc")))||T->GetFinishedFunctionName()!=FName(*(Name+TEXT("__FinishedFunc"))))return Bad(TEXT("regenerated_timeline_caches"),Name+TEXT("/new template GUID"),TEXT("different"));
        for(const auto& E:T->EventTracks)if(E.GetFunctionName()!=FName(*(Name+TEXT("__")+E.GetTrackName().ToString()+TEXT("__EventFunc"))))return Bad(TEXT("event_track_cache"),Name,TEXT("different"));
        for(const auto& E:T->FloatTracks)if(E.GetPropertyName()!=FName(*Sanitized(Name+TEXT("_")+E.GetTrackName().ToString()+TEXT("_")+Guid)))return Bad(TEXT("float_track_cache"),Name,TEXT("different"));
        for(const auto& E:T->VectorTracks)if(E.GetPropertyName()!=FName(*Sanitized(Name+TEXT("_")+E.GetTrackName().ToString()+TEXT("_")+Guid)))return Bad(TEXT("vector_track_cache"),Name,TEXT("different"));
        for(const auto& E:T->LinearColorTracks)if(E.GetPropertyName()!=FName(*Sanitized(Name+TEXT("_")+E.GetTrackName().ToString()+TEXT("_")+Guid)))return Bad(TEXT("color_track_cache"),Name,TEXT("different"));return true;
    }
    bool NewTemplate(bool Rebind)
    {
        if(!Context()||!SourceCurrent()||!OldTargetCurrent(Rebind))return false;auto* B=Target.BP.Get();const int32 AddedIndex=OldTemplates.Num();if(B->Timelines.Num()!=AddedIndex+1||!B->Timelines[AddedIndex].IsResolved())return Bad(TEXT("returned_blueprint_timelines"),FString::FromInt(AddedIndex+1),FString::FromInt(B->Timelines.Num()));
        auto* T=B->Timelines[AddedIndex].Get();TStrongObjectPtr<UTimelineTemplate> HoldT(T);if(!Loaded(T)||T==Origin.Get()||T->GetClass()!=UTimelineTemplate::StaticClass()||T->GetOuter()!=Target.Generated.Get()||T->GetPackage()!=Target.Package.Get()||!T->HasAnyFlags(RF_Transactional))return Bad(TEXT("returned_template_owner"),TEXT("new exact template owned by actual generated class"),TEXT("different/unavailable"));
        if(!Loaded(Pasted.Get())||Pasted->TimelineGuid!=TextPlan.Selector||Pasted->TimelineName!=NewName||T->GetVariableName()!=NewName||NodeFlags(Pasted.Get())!=(uint32(T->bAutoPlay)|(uint32(T->bLoop)<<1)|(uint32(T->bReplicated)<<2)|(uint32(T->bIgnoreTimeDilation)<<3)))return Bad(TEXT("pasted_timeline_role"),TEXT("selector GUID and planned unique variable/flags"),TEXT("changed"));
        FString A;if(!Values(T,A,false,false))return false;if(A!=Semantic)return Bad(TEXT("duplicated_authored_values"),Semantic,A);
        if(!T->TimelineGuid.IsValid()||T->TimelineGuid==TextPlan.Selector||T->GetFName()!=FName(*UTimelineTemplate::TimelineVariableNameToTemplateName(NewName))||!Caches(T))return Bad(TEXT("new_template_guid_name"),TEXT("new valid template GUID and canonical object/cache names"),T->TimelineGuid.ToString());
        FString Cache;if(!Values(T,Cache,true,false))return false;if(!Created.Get()){CreatedGuid=T->TimelineGuid;CreatedIdentity=Cache;}else if(T->TimelineGuid!=CreatedGuid||Cache!=CreatedIdentity)return Bad(TEXT("returned_timeline_logical_identity"),CreatedIdentity,Cache);
        if(!Rebind&&(T!=Created.Get()||!ObjectCurrent(FreshTemplate)))return Bad(TEXT("returned_template_slot"),FreshTemplate.Path,TEXT("unexpected replacement"));
        TArray<FObject> NewCurves;
        auto CheckCurve=[&](UCurveBase* Actual,UCurveBase* Original,bool External)->bool
        {
            if(!Loaded(Actual)||!Loaded(Original)||Actual->GetClass()!=Original->GetClass())return Bad(TEXT("new_curve_class"),TEXT("source exact native curve class"),TEXT("changed"));
            const bool Shared=External||Original->GetOuter()->IsA(UPackage::StaticClass());if(Shared){if(Actual!=Original)return Bad(TEXT("external_curve_reference"),Original->GetPathName(),Actual->GetPathName());}
            else if(Actual==Original||Actual->GetOuter()!=Target.Generated.Get()||Actual->GetPackage()!=Target.Package.Get())return Bad(TEXT("owned_curve_clone"),TEXT("distinct clone under actual target generated class"),Actual->GetPathName());
            FObject Role;if(!Object(Actual,Role))return false;NewCurves.Add(MoveTemp(Role));return true;
        };
        for(int32 I=0;I<T->EventTracks.Num();++I)if(!CheckCurve(T->EventTracks[I].CurveKeys.Get(),Origin->EventTracks[I].CurveKeys.Get(),T->EventTracks[I].bIsExternalCurve))return false;
        for(int32 I=0;I<T->FloatTracks.Num();++I)if(!CheckCurve(T->FloatTracks[I].CurveFloat.Get(),Origin->FloatTracks[I].CurveFloat.Get(),T->FloatTracks[I].bIsExternalCurve))return false;
        for(int32 I=0;I<T->VectorTracks.Num();++I)if(!CheckCurve(T->VectorTracks[I].CurveVector.Get(),Origin->VectorTracks[I].CurveVector.Get(),T->VectorTracks[I].bIsExternalCurve))return false;
        for(int32 I=0;I<T->LinearColorTracks.Num();++I)if(!CheckCurve(T->LinearColorTracks[I].CurveLinearColor.Get(),Origin->LinearColorTracks[I].CurveLinearColor.Get(),T->LinearColorTracks[I].bIsExternalCurve))return false;
        if(!Rebind){if(NewCurves.Num()!=FreshCurves.Num())return Bad(TEXT("new_curve_inventory"),FString::FromInt(FreshCurves.Num()),FString::FromInt(NewCurves.Num()));for(int32 I=0;I<FreshCurves.Num();++I)if(NewCurves[I].Hold.Get()!=FreshCurves[I].Hold.Get())return Bad(TEXT("new_curve_identity"),FreshCurves[I].Path,TEXT("unexpected slot replacement"));else if(!ObjectCurrent(FreshCurves[I]))return false;}
        Created.Reset(T);if(!Object(T,FreshTemplate))return false;FreshCurves=MoveTemp(NewCurves);return true;
    }
    bool Current(bool Files=false){if(!Context()||!SourceCurrent(Files)||!Target.Plain())return false;return Ready?NewTemplate(false):OldTargetCurrent(false);}
    bool AfterClearRemove(){if(!Context()||!SourceCurrent(true)||!Target.Reacquire()||!OldTargetCurrent(true))return false;return Current(true);}
    bool PinTypes(const TArray<FCollapsedTarget::FPin>& Pins)
    {
        TMap<FName,TPair<FName,FName>> Expected;
        for(const TCHAR* N:{TEXT("Play"),TEXT("PlayFromStart"),TEXT("Stop"),TEXT("Reverse"),TEXT("ReverseFromEnd"),TEXT("SetNewTime")})Expected.Add(FName(N),{UEdGraphSchema_K2::PC_Exec,NAME_None});
        Expected.Add(TEXT("Update"),{UEdGraphSchema_K2::PC_Exec,NAME_None});Expected.Add(TEXT("Finished"),{UEdGraphSchema_K2::PC_Exec,NAME_None});Expected.Add(TEXT("NewTime"),{UEdGraphSchema_K2::PC_Real,UEdGraphSchema_K2::PC_Float});Expected.Add(TEXT("Direction"),{UEdGraphSchema_K2::PC_Byte,NAME_None});
        TMap<FName,UObject*> Types;for(const auto& E:Created->EventTracks)Expected.Add(E.GetTrackName(),{UEdGraphSchema_K2::PC_Exec,NAME_None});for(const auto& E:Created->FloatTracks)Expected.Add(E.GetTrackName(),{UEdGraphSchema_K2::PC_Real,UEdGraphSchema_K2::PC_Float});for(const auto& E:Created->VectorTracks){Expected.Add(E.GetTrackName(),{UEdGraphSchema_K2::PC_Struct,NAME_None});Types.Add(E.GetTrackName(),TBaseStructure<FVector>::Get());}for(const auto& E:Created->LinearColorTracks){Expected.Add(E.GetTrackName(),{UEdGraphSchema_K2::PC_Struct,NAME_None});Types.Add(E.GetTrackName(),TBaseStructure<FLinearColor>::Get());}
        if(Expected.Num()!=Pins.Num())return Bad(TEXT("allocated_timeline_pin_count"),FString::FromInt(Expected.Num()),FString::FromInt(Pins.Num()));TSet<FName> Seen;
        for(const auto& P:Pins){const auto* E=Expected.Find(P.Name);bool Output=P.Name==TEXT("Update")||P.Name==TEXT("Finished")||P.Name==TEXT("Direction")||Types.Contains(P.Name);for(const auto& Track:Created->EventTracks)Output|=Track.GetTrackName()==P.Name;for(const auto& Track:Created->FloatTracks)Output|=Track.GetTrackName()==P.Name;
            if(!E||Seen.Contains(P.Name)||!P.Links.IsEmpty()||P.Type.PinCategory!=E->Key||P.Type.PinSubCategory!=E->Value||P.Direction!=(Output?EGPD_Output:EGPD_Input)||P.Type.ContainerType!=EPinContainerType::None||P.Type.bIsReference||P.Type.bIsConst||P.Default||P.Type.PinSubCategoryObject.IsStale()||P.Type.PinValueType.TerminalSubCategoryObject.IsStale())return Bad(TEXT("allocated_pin_role"),TEXT("canonical unique unlinked plain Timeline pins"),P.Name.ToString());Seen.Add(P.Name);
            UObject* Type=P.Type.PinSubCategoryObject.Get();if(P.Name==TEXT("Direction")){if(!Loaded(Type)||Type->GetClass()!=UEnum::StaticClass()||Type->GetPathName()!=TEXT("/Script/Engine.ETimelineDirection"))return Bad(TEXT("direction_enum"),TEXT("native ETimelineDirection"),GetPathNameSafe(Type));}else if(Type!=Types.FindRef(P.Name))return Bad(TEXT("track_pin_type"),TEXT("native declared track type"),GetPathNameSafe(Type));
        }return true;
    }
    bool Returned(const TSet<UEdGraphNode*>& Imported)
    {
        if(!Context()||!SourceCurrent(true))return false;for(auto* N:Imported)Target.Hold(N);
        if(Imported.Num()!=1)return Bad(TEXT("paste_returned_count"),TEXT("1"),FString::FromInt(Imported.Num()));auto* N=*Imported.CreateConstIterator();if(!Loaded(N)||N->GetClass()!=UK2Node_Timeline::StaticClass()||N->GetGraph()!=Target.Selected||N->GetOuter()!=Target.Selected||!Target.Selected->Nodes.Contains(N))return Bad(TEXT("paste_returned_node"),TEXT("held native Timeline member of selected graph"),TEXT("other/unavailable"));Pasted.Reset(static_cast<UK2Node_Timeline*>(N));
        if(N->NodeGuid!=TextPlan.NodeGuid||N->NodePosX!=TextPlan.X||N->NodePosY!=TextPlan.Y||N->NodeComment!=TextPlan.Comment||bool(N->bCommentBubbleVisible)!=TextPlan.Visible||bool(N->bCommentBubblePinned)!=TextPlan.Pinned)return Bad(TEXT("paste_node_authored_fields"),TEXT("copied input GUID/coordinates/comment"),TEXT("changed"));
        PasteReturned=true;Target.ExpectedAdded.Add(N);if(!Target.Reacquire()||!NewTemplate(true))return false;
        FCollapsedTarget::FNode Row;if(!Target.NodeCopy(N,Row)||!PinTypes(Row.Pins))return false;
        TMap<FString,const FCollapsedTarget::FPin*> ByName;for(const auto& P:Row.Pins)ByName.Add(P.Name.ToString(),&P);
        for(const auto& M:TextPlan.Pins){const FString Name=FCollapsedTextPlan::Unquote(M.FindRef(TEXT("PinName")));const auto* Slot=ByName.Find(Name);if(!Slot)return Bad(TEXT("copied_pin_name"),Name,TEXT("missing"));const auto& P=**Slot;FGuid Id;FGuid::Parse(M.FindRef(TEXT("PinId")),Id);const auto Raw=[&](const TCHAR* K){return FCollapsedTextPlan::Unquote(M.FindRef(K));};if(P.Id!=Id||P.Type.PinCategory!=FName(*Raw(TEXT("PinType.PinCategory")))||P.Type.PinSubCategory!=FName(*Raw(TEXT("PinType.PinSubCategory")))||(P.Direction==EGPD_Output)!=(Raw(TEXT("Direction"))==TEXT("EGPD_Output"))||P.Raw!=Raw(TEXT("DefaultValue"))||P.Auto!=Raw(TEXT("AutogeneratedDefaultValue")))return Bad(TEXT("copied_pin_values"),Name,TEXT("GUID/type/direction/default changed"));}
        Target.Nodes.Add(MoveTemp(Row));if(!Target.SealLinks(Target.Nodes))return false;Ready=true;return Current(true);
    }
    bool Reacquire(){if(!Context()||!SourceCurrent(true)||!Target.Reacquire()||!NewTemplate(true))return false;return Current(true);}
};

Variable::FResult ImportTimeline(const FString& Path,const FString& TextToImport,const FString& FilePath,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool Clear,bool KeepRoot,bool Allow,bool ValidateOnly,bool DryRun,bool Compile,bool Save)
{
    FResult R;Init(R,Compile,Save);R.Data->SetBoolField(TEXT("native_timeline_clipboard_import"),true);R.Data->SetBoolField(TEXT("runtime_execution_verified"),false);R.Data->SetBoolField(TEXT("opaque_paste_callback_intermediates_verified"),false);
    R.Data->SetStringField(TEXT("finite_scope"),TEXT("Inline single unlinked native Timeline into a direct Actor EventGraph, including bounded existing Timelines and clear_graph using original Node.Modify/Graph.RemoveNode. Prior authored templates/curves remain even when their Timeline nodes are removed. Existing nodes/peers use QZ stock callbacks plus exact Timeline; keep_root_node applies only to admitted root roles (no function/Anim root in this EventGraph profile). Source is a unique loaded distinct saved-clean native template. Implicit compile reacquires survivor classes/pins/templates/curves; file/mixed/linked/nested/missing-source and wider owners remain incomplete."));
    if(!IsEditorGameThread()||IsPlaySessionActive()){Fail(R,TEXT("graph_transfer_editor_invalid"),TEXT("Idle Editor game thread required."));return R;}
    if(!FilePath.IsEmpty()){Fail(R,TEXT("timeline_clipboard_profile_pending"),TEXT("Timeline file input remains incomplete; inline append and admitted clear are supported."));return R;}
    FTITimelineText Plan;if(!Plan.Parse(TextToImport,R))return R;
    FString ObjectPath=Path;if(ObjectPath.StartsWith(TEXT("/Game/"))&&!ObjectPath.Contains(TEXT(".")))ObjectPath+=TEXT(".")+FPackageName::GetShortName(Path);
    Count(R,TEXT("reference_lookup_calls"));auto* B=FindObject<UBlueprint>(nullptr,*ObjectPath);TStrongObjectPtr<UBlueprint> Held(B);Count(R,TEXT("reference_lookup_returns"));if(!Scope(B,R)||!Sample(B,Allow,R))return R;
    UEdGraph* G=nullptr;FGuid Requested;const bool HasId=!GraphId.IsEmpty();if(HasId&&!FGuid::Parse(GraphId,Requested)){Fail(R,TEXT("graph_transfer_graph_unavailable"),TEXT("Invalid graph GUID."));return R;}const FString Type=Normalize(GraphType);
    Count(R,TEXT("get_all_graphs_calls"));TArray<UEdGraph*> All;B->GetAllGraphs(All);for(auto* Row:All){if(!Loaded(Row)||(HasId&&Row->GraphGuid!=Requested)||(!GraphName.IsEmpty()&&!Row->GetName().Equals(GraphName,ESearchCase::IgnoreCase))||(!Type.IsEmpty()&&Kind(B,Row)!=Type))continue;if(GraphName.IsEmpty()&&!HasId&&Type.IsEmpty()&&(!B->UbergraphPages.Contains(Row)||!Row->GetName().Contains(TEXT("EventGraph"))))continue;G=Row;break;}
    TStrongObjectPtr<UEdGraph> HeldGraph(G);if(!Loaded(G)){Fail(R,TEXT("graph_transfer_graph_unavailable"),TEXT("Existing EventGraph required; implicit missing-graph creation remains incomplete for Timeline profile."));return R;}
    FCollapsedTarget Lease(R);if(!Lease.Bind(B,G))return R;FTITimelineImport TI(R,Lease,Plan);if(!TI.CheckPinText()||!TI.Bind()||!TI.Current(true)||!TI.FindOrigin())return R;
    if(Clear&&!Lease.PrepareClear(true))return R;R.Data->SetBoolField(TEXT("native_timeline_clear_import"),Clear);
    TI.NewName=FBlueprintEditorUtils::FindUniqueTimelineName(B);if(!TI.Current(true))return R;
    auto Legacy=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("legacy_result"),Legacy);Legacy->SetStringField(TEXT("command"),TEXT("import_blueprint_graph_nodes_from_text"));Legacy->SetStringField(TEXT("blueprint_name"),Path);Legacy->SetStringField(TEXT("blueprint_path"),B->GetPathName());Legacy->SetStringField(TEXT("blueprint_package_path"),B->GetOutermost()->GetName());Legacy->SetStringField(TEXT("graph_name"),G->GetName());Legacy->SetStringField(TEXT("graph_id"),G->GraphGuid.ToString());Legacy->SetStringField(TEXT("graph_schema"),TEXT("EdGraphSchema_K2"));Legacy->SetStringField(TEXT("text_file_path"),FilePath);Legacy->SetNumberField(TEXT("text_length"),TextToImport.Len());const bool Validate=ValidateOnly||DryRun;Legacy->SetBoolField(TEXT("validate_only"),Validate);Legacy->SetBoolField(TEXT("clear_graph"),Clear);Legacy->SetBoolField(TEXT("keep_root_node"),KeepRoot);Legacy->SetBoolField(TEXT("allow_non_sample"),Allow);Legacy->SetBoolField(TEXT("sample_or_temp_target"),R.Data->GetBoolField(TEXT("sample_or_temp_target")));Legacy->SetBoolField(TEXT("compile_requested"),Compile);Legacy->SetBoolField(TEXT("save_requested"),Save);Legacy->SetBoolField(TEXT("compiled"),false);Legacy->SetBoolField(TEXT("saved"),false);Legacy->SetNumberField(TEXT("removed_node_count"),0);Legacy->SetNumberField(TEXT("imported_node_count"),0);
    R.Stage=TEXT("timeline_can_import");Count(R,TEXT("can_import_calls"));const bool Can=FEdGraphUtilities::CanImportNodesFromText(G,TextToImport);Count(R,TEXT("can_import_returns"));Legacy->SetBoolField(TEXT("can_import"),Can);Legacy->SetBoolField(TEXT("success"),Can);if(!TI.Current(true))return R;if(!Can){Fail(R,TEXT("graph_transfer_cannot_import"),TEXT("Public CanImportNodesFromText returnedFalse."));return R;}if(Validate){R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;}
    const auto Apply=[&]()
    {
        Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","ImportTimelineBlueprintGraphText","Import Timeline Blueprint Graph Text"));if(!TI.Current(true)||!TI.FindOrigin())return;
        R.Stage=TEXT("timeline_apply_import");R.bMutationAttempted=R.bChanged=true;R.bChangeStateKnown=false;Count(R,TEXT("blueprint_modify_calls"));const bool BM=B->Modify();R.Data->SetBoolField(TEXT("blueprint_modify_transaction_recorded"),BM);if(!TI.Current())return;Count(R,TEXT("graph_modify_calls"));const bool GM=G->Modify();R.Data->SetBoolField(TEXT("graph_modify_transaction_recorded"),GM);if(!TI.Current(true)||!TI.FindOrigin())return;
        if(Clear)
        {
            int32 Removed=0;TArray<TSharedPtr<FJsonValue>> RemovalRows;
            for(const auto& Item:Lease.ClearTargets)
            {
                if(!TI.Current(true))return;R.Stage=TEXT("timeline_clear_node_modify");auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("node_path"),Item.Path);Receipt->SetStringField(TEXT("node_id"),Item.Guid.ToString());Receipt->SetStringField(TEXT("name"),Item.Name);Receipt->SetStringField(TEXT("node_class"),Item.ClassPath);Receipt->SetBoolField(TEXT("remove_entered"),false);Receipt->SetBoolField(TEXT("absence_verified"),false);RemovalRows.Add(MakeShared<FJsonValueObject>(Receipt));R.Data->SetArrayField(TEXT("removal_receipts"),RemovalRows);
                UEdGraphNode* Removing=Item.Object;Count(R,TEXT("node_modify_calls"));Removing->Modify();if(!TI.Current(true))return;
                R.Stage=TEXT("timeline_clear_remove");if(!Lease.RetireForRemove(Removing))return;Receipt->SetBoolField(TEXT("remove_entered"),true);R.Data->SetNumberField(TEXT("clear_removal_attempt_count"),Removed+1);R.bChangeStateKnown=false;
                Count(R,TEXT("remove_node_calls"));const bool Returned=G->RemoveNode(Removing);Removing=nullptr;Count(R,TEXT("remove_node_returns"));Receipt->SetBoolField(TEXT("returned"),Returned);++Removed;Legacy->SetNumberField(TEXT("removed_node_count"),Removed);
                // Original RemoveNode does not invoke Timeline.DestroyNode or
                // RemoveTimeline: old template/curve authority stays protected.
                if(!TI.AfterClearRemove())return;Receipt->SetBoolField(TEXT("absence_verified"),true);R.Data->SetNumberField(TEXT("clear_absence_verified_count"),Removed);R.bChangeStateKnown=true;
            }
            if(!TI.Current(true))return;R.Stage=TEXT("timeline_apply_import");R.bChangeStateKnown=false;
        }
        TSet<UEdGraphNode*> Imported;Count(R,TEXT("import_nodes_calls"));FEdGraphUtilities::ImportNodesFromText(G,TextToImport,Imported);for(auto* N:Imported)Lease.Hold(N);Count(R,TEXT("import_nodes_returns"));Legacy->SetNumberField(TEXT("imported_node_count"),Imported.Num());R.Data->SetNumberField(TEXT("imported_returned_count"),Imported.Num());R.Data->SetBoolField(TEXT("implicit_skeleton_regeneration_possible"),true);if(!TI.Returned(Imported))return;
        auto* N=TI.Pasted.Get();Count(R,TEXT("new_guid_calls"));N->CreateNewGuid();if(!N->NodeGuid.IsValid()){TI.Bad(TEXT("new_node_guid"),TEXT("valid unique GUID"),TEXT("invalid"));return;}for(const auto& E:Lease.Nodes)if(E.Object!=N&&E.Guid==N->NodeGuid){TI.Bad(TEXT("new_node_guid"),TEXT("unique GUID"),N->NodeGuid.ToString());return;}for(auto& E:Lease.Nodes)if(E.Object==N)E.Guid=N->NodeGuid;
        for(auto& E:Lease.Nodes)for(auto& P:E.Pins)P.LinkIds.Reset();if(!Lease.SealLinks(Lease.Nodes)||!TI.Current(true))return;
        Count(R,TEXT("notify_graph_calls"));G->NotifyGraphChanged();if(!TI.Current(true))return;R.Stage=TEXT("timeline_structural");Count(R,TEXT("mark_structural_calls"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B);if(!TI.Reacquire())return;R.bChangeStateKnown=true;
        // Copy original NodeToJson(false) before the optional compiler/save prefix.
        const FText Title=N->GetNodeTitle(ENodeTitleType::FullTitle);if(!TI.Current())return;const FString TitleString=Title.ToString();if(!TI.Current())return;
        TArray<TSharedPtr<FJsonValue>> Rows;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("node_id"),N->NodeGuid.ToString());J->SetStringField(TEXT("name"),N->GetName());J->SetStringField(TEXT("class"),N->GetClass()->GetName());J->SetStringField(TEXT("node_class"),N->GetClass()->GetPathName());J->SetStringField(TEXT("title"),TitleString);J->SetNumberField(TEXT("x"),N->NodePosX);J->SetNumberField(TEXT("y"),N->NodePosY);J->SetStringField(TEXT("node_comment"),N->NodeComment);J->SetBoolField(TEXT("comment_bubble_visible"),N->bCommentBubbleVisible);J->SetBoolField(TEXT("comment_bubble_pinned"),N->bCommentBubblePinned);Rows.Add(MakeShared<FJsonValueObject>(J));Legacy->SetArrayField(TEXT("imported_nodes"),Rows);
        if(Compile){R.Stage=TEXT("timeline_compile");R.bCompileAttempted=true;R.bChangeStateKnown=false;FCompilerResultsLog Log;Count(R,TEXT("full_compile_calls"));FKismetEditorUtilities::CompileBlueprint(B,EBlueprintCompileOptions::None,&Log);Count(R,TEXT("full_compile_returns"));R.Data->SetNumberField(TEXT("compiler_errors"),Log.NumErrors);R.Data->SetNumberField(TEXT("compiler_warnings"),Log.NumWarnings);if(!TI.Context())return;R.Compiled=Log.NumErrors==0&&B->Status!=BS_Error;auto Generated=B->GeneratedClass;const bool LegacyCompiled=Generated.GetGCPtr().IsResolved()&&Generated.GetGCPtr().Get()!=nullptr;Legacy->SetBoolField(TEXT("compiled"),LegacyCompiled);auto CR=MakeShared<FJsonObject>();CR->SetBoolField(TEXT("requested"),true);CR->SetBoolField(TEXT("success"),LegacyCompiled);CR->SetStringField(TEXT("blueprint"),Lease.BPPath);TStrongObjectPtr<UClass> HeldClass(Generated.GetGCPtr().IsResolved()?Generated.GetGCPtr().Get():nullptr);CR->SetStringField(TEXT("generated_class"),Loaded(HeldClass.Get())?HeldClass->GetPathName():FString());Legacy->SetObjectField(TEXT("compile_result"),CR);if(!TI.Reacquire())return;R.bChangeStateKnown=true;}
        // Preserve original requested save after a known failed compile; never after unknown.
        if(Save){R.Stage=TEXT("timeline_save");R.bSaveAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("forced_save_calls"));const bool Saved=UEditorAssetLibrary::SaveLoadedAsset(B,false);Count(R,TEXT("forced_save_returns"));R.bSaved=Saved;Legacy->SetBoolField(TEXT("saved"),Saved);if(!Saved){Lease.Stopped=true;Fail(R,TEXT("graph_transfer_save_failed"),TEXT("Forced SaveLoadedAsset returnedFalse; returned receipt retained, no further source SDK."));return;}if(!TI.Current(true))return;R.bChangeStateKnown=true;}
        Legacy->SetBoolField(TEXT("dirty_after"),B->GetOutermost()->IsDirty());R.Data->SetStringField(TEXT("new_template_guid"),TI.CreatedGuid.ToString());R.Data->SetStringField(TEXT("new_timeline_variable"),TI.NewName.ToString());R.Data->SetBoolField(TEXT("current_imported_nodes_verified"),true);R.Data->SetBoolField(TEXT("readonly_timeline_source_preserved"),true);Terminal(R);
    };
    Apply();if(!Lease.Stopped&&R.bChangeStateKnown){const FString Stage=R.Stage;R.Stage=TEXT("timeline_transaction_publication");if(!TI.Current(true))return R;R.Data->SetBoolField(TEXT("transaction_publication_current_verified"),true);R.Stage=Stage;}return R;
}

Variable::FResult ImportCollapsed(const FString& Path,const FString& TextToImport,const FString& FilePath,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool Clear,bool KeepRoot,bool Allow,bool ValidateOnly,bool DryRun,bool Compile,bool Save)
{
    FResult R;Init(R,Compile,Save);R.Data->SetBoolField(TEXT("native_collapsed_graph_import"),true);
    R.Data->SetStringField(TEXT("finite_scope"),TEXT("Inline recursively owned Composite/BoundGraph tree (up to eight graph levels, 32 graphs, 256 objects), exact complementary Tunnel2 per graph with bounded exec/int32/bool ports, optional unlinked Self, multiple exact plain CallFunction nodes from loaded native KismetMathLibrary static pure callable by-value int32/bool signatures, stock Branch and ordered Sequence. Actual reciprocal topology includes scalar output fanout; function parameter/default/storage sources are checked before paste and after each callback. Loaded direct Actor K2 target/native Scene root and existing original clear/keep-root/partial/compile/save paths are preserved. Other function owners/types/ref/container/metadata expansion, larger trees or mixed top-level objects, file input, custom sources and runtime execution remain valid incomplete scope."));
    if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("graph_transfer_editor_invalid"),TEXT("Idle Editor game thread required."));return R; }
    if(Clear&&!KeepRoot) { Fail(R,TEXT("graph_transfer_collapsed_profile_pending"),TEXT("Collapsed clear first supports keep_root_node=True only; keepFalse and old nested removal remain incomplete."));return R; }
    R.Data->SetBoolField(TEXT("native_collapsed_clear_replace"),Clear);
    FCollapsedTextPlan Plan;if(!Plan.Parse(TextToImport,R)) { return R; }
    R.Data->SetNumberField(TEXT("planned_collapsed_graph_count"),Plan.GraphPlans.Num());R.Data->SetNumberField(TEXT("planned_collapsed_object_count"),Plan.Objects.Num());
    FString ObjectPath=Path;if(ObjectPath.StartsWith(TEXT("/Game/"))&&!ObjectPath.Contains(TEXT("."))) { ObjectPath+=TEXT(".")+FPackageName::GetShortName(Path); }
    if(!ObjectPath.StartsWith(TEXT("/Game/"))||!ReferenceGuard(ObjectPath)) { Fail(R,TEXT("graph_transfer_path_invalid"),TEXT("Exact loaded owned /Game Blueprint required."));return R; }
    Count(R,TEXT("reference_lookup_calls"));UBlueprint* Found=FindObject<UBlueprint>(nullptr,*ObjectPath);const TStrongObjectPtr<UBlueprint> ReturnedBP(Found);Count(R,TEXT("reference_lookup_returns"));
    if(!Scope(Found,R)||!Sample(Found,Allow,R)) { return R; }
    // No implicit graph creation in this added profile. Original existing selector order is retained.
    UEdGraph* Target=nullptr;TArray<UEdGraph*> All;Count(R,TEXT("get_all_graphs_calls"));Found->GetAllGraphs(All);FGuid Requested;const bool HasId=!GraphId.IsEmpty();
    if(HasId&&!FGuid::Parse(GraphId,Requested)) { Fail(R,TEXT("graph_transfer_graph_unavailable"),TEXT("Invalid graph GUID."));return R; }
    const FString WantedType=Normalize(GraphType);
    for(auto* G:All)
    {
        if(!Loaded(G)||(HasId&&G->GraphGuid!=Requested)||(!GraphName.IsEmpty()&&!G->GetName().Equals(GraphName,ESearchCase::IgnoreCase))||(!WantedType.IsEmpty()&&Kind(Found,G)!=WantedType)) { continue; }
        if(GraphName.IsEmpty()&&!HasId&&WantedType.IsEmpty()&&(!Found->UbergraphPages.Contains(G)||!G->GetName().Contains(TEXT("EventGraph")))) { continue; }Target=G;break;
    }
    const TStrongObjectPtr<UEdGraph> ReturnedGraph(Target);if(!Loaded(Target)) { Fail(R,TEXT("graph_transfer_graph_unavailable"),TEXT("Existing K2 target graph required by first collapsed profile."));return R; }
    FCollapsedTarget Lease(R);if(!Lease.Bind(Found,Target)||!Lease.BindMath(Plan)) { return R; }
    if(Clear&&!Lease.PrepareClear()) { return R; }
    int32 PlannedPins=0,ExistingPins=0;for(const auto& O:Plan.Objects)PlannedPins+=O.Pins.Num();for(const auto& N:Lease.Nodes)ExistingPins+=N.Pins.Num();
    if(Lease.Graphs.Num()+Plan.GraphPlans.Num()>MaxGraphs||Lease.Nodes.Num()+Plan.Objects.Num()-Plan.GraphPlans.Num()>MaxNodes||ExistingPins+PlannedPins>MaxPins) { Fail(R,TEXT("graph_transfer_collapsed_bound"),TEXT("Planned recursive graph/node additions exceed the existing finite bound."));return R; }
    for(const auto& O:Plan.Objects)
    {
        for(const auto& G:Lease.Graphs) { if(G.Guid==O.Guid) { Fail(R,TEXT("graph_transfer_collapsed_guid_collision"),TEXT("Source GUID aliases an existing graph."));return R; } }
        for(const auto& N:Lease.Nodes) { if(N.Guid==O.Guid) { Fail(R,TEXT("graph_transfer_collapsed_guid_collision"),TEXT("Source GUID aliases an existing node."));return R; } }
    }
    Count(R,TEXT("reference_lookup_calls"));UObject* Collision=FindObject<UObject>(Target,*Plan.Objects[0].Name);const TStrongObjectPtr<UObject> HeldCollision(Collision);Count(R,TEXT("reference_lookup_returns"));if(!Lease.Plain()) { return R; }
    if(Collision) { Fail(R,TEXT("graph_transfer_collapsed_name_collision"),TEXT("First collapsed profile refuses an existing object with the requested root name before public paste."));return R; }
    auto Legacy=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("legacy_result"),Legacy);Legacy->SetStringField(TEXT("command"),TEXT("import_blueprint_graph_nodes_from_text"));Legacy->SetStringField(TEXT("blueprint_name"),Path);Legacy->SetStringField(TEXT("blueprint_path"),Found->GetPathName());Legacy->SetStringField(TEXT("blueprint_package_path"),Found->GetOutermost()->GetName());Legacy->SetStringField(TEXT("graph_name"),Target->GetName());Legacy->SetStringField(TEXT("graph_id"),Target->GraphGuid.ToString());Legacy->SetStringField(TEXT("graph_schema"),TEXT("EdGraphSchema_K2"));Legacy->SetStringField(TEXT("text_file_path"),FilePath);Legacy->SetNumberField(TEXT("text_length"),TextToImport.Len());
    const bool Validate=ValidateOnly||DryRun;Legacy->SetBoolField(TEXT("validate_only"),Validate);Legacy->SetBoolField(TEXT("clear_graph"),Clear);Legacy->SetBoolField(TEXT("keep_root_node"),KeepRoot);Legacy->SetBoolField(TEXT("allow_non_sample"),Allow);Legacy->SetBoolField(TEXT("sample_or_temp_target"),R.Data->GetBoolField(TEXT("sample_or_temp_target")));Legacy->SetBoolField(TEXT("compile_requested"),Compile);Legacy->SetBoolField(TEXT("save_requested"),Save);Legacy->SetBoolField(TEXT("compiled"),false);Legacy->SetBoolField(TEXT("saved"),false);Legacy->SetNumberField(TEXT("removed_node_count"),0);Legacy->SetNumberField(TEXT("imported_node_count"),0);
    R.Stage=TEXT("collapsed_can_import");Count(R,TEXT("can_import_calls"));const bool Can=FEdGraphUtilities::CanImportNodesFromText(Target,TextToImport);Count(R,TEXT("can_import_returns"));Legacy->SetBoolField(TEXT("can_import"),Can);Legacy->SetBoolField(TEXT("success"),Can);
    if(!Lease.Plain()||!Lease.MathSnapshot()) { return R; }if(!Can) { Fail(R,TEXT("graph_transfer_cannot_import"),TEXT("Public CanImportNodesFromText returnedFalse."));return R; }
    if(Validate) { R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R; }
    if(Clear)
    {
        R.Data->SetStringField(TEXT("clear_scope"),TEXT("keepRoot=True stock leaf-node removal using original Graph.RemoveNode; retained FunctionEntry/Result and copied survivor edges; old nested/Timeline/Macro removal is incomplete."));
        const auto ApplyClear=[&]()
        {
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction ClearTransaction(NSLOCTEXT("CubelessMCP","ImportCollapsedBlueprintGraphText","Import Collapsed Blueprint Graph Text"));
    if(!Lease.Plain()||!Lease.MathSnapshot())return;
    R.Stage=TEXT("collapsed_apply_import");R.bMutationAttempted=R.bChanged=true;R.bChangeStateKnown=false;
    Count(R,TEXT("blueprint_modify_calls"));const bool BPModified=Found->Modify();R.Data->SetBoolField(TEXT("blueprint_modify_transaction_recorded"),BPModified);if(!Lease.Plain()) { return; }
    Count(R,TEXT("graph_modify_calls"));const bool GraphModified=Target->Modify();R.Data->SetBoolField(TEXT("graph_modify_transaction_recorded"),GraphModified);if(!Lease.Plain()) { return; }
    int32 Removed=0;TArray<TSharedPtr<FJsonValue>> RemovalRows;
    for(const auto& Item:Lease.ClearTargets)
    {
        if(!Lease.Plain())return;R.Stage=TEXT("collapsed_clear_node_modify");auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("node_path"),Item.Path);Receipt->SetStringField(TEXT("node_id"),Item.Guid.ToString());Receipt->SetStringField(TEXT("name"),Item.Name);Receipt->SetStringField(TEXT("node_class"),Item.ClassPath);Receipt->SetBoolField(TEXT("remove_entered"),false);Receipt->SetBoolField(TEXT("absence_verified"),false);RemovalRows.Add(MakeShared<FJsonValueObject>(Receipt));R.Data->SetArrayField(TEXT("removal_receipts"),RemovalRows);
        UEdGraphNode* Removing=Item.Object;Count(R,TEXT("node_modify_calls"));Removing->Modify();if(!Lease.Plain())return;
        R.Stage=TEXT("collapsed_clear_remove");if(!Lease.RetireForRemove(Removing))return;Receipt->SetBoolField(TEXT("remove_entered"),true);R.Data->SetNumberField(TEXT("clear_removal_attempt_count"),Removed+1);R.bChangeStateKnown=false;Count(R,TEXT("remove_node_calls"));const bool Returned=Target->RemoveNode(Removing);Removing=nullptr;Count(R,TEXT("remove_node_returns"));Receipt->SetBoolField(TEXT("returned"),Returned);++Removed;Legacy->SetNumberField(TEXT("removed_node_count"),Removed);
        // Reacquire only surviving owned objects/pins. The retired selected object is never read again.
        if(!Lease.Reacquire()||!Lease.MathSnapshot())return;Receipt->SetBoolField(TEXT("absence_verified"),true);R.Data->SetNumberField(TEXT("clear_absence_verified_count"),Removed);R.bChangeStateKnown=true;
    }
    if(!Lease.Plain())return;R.Stage=TEXT("collapsed_apply_import");R.bChangeStateKnown=false;
    TSet<UEdGraphNode*> Imported;Count(R,TEXT("import_nodes_calls"));FEdGraphUtilities::ImportNodesFromText(Target,TextToImport,Imported);
    for(auto* N:Imported) { Lease.Hold(N); }Count(R,TEXT("import_nodes_returns"));Legacy->SetNumberField(TEXT("imported_node_count"),Imported.Num());R.Data->SetNumberField(TEXT("imported_returned_count"),Imported.Num());
    if(!CollapsedAcquired(Lease,Plan,Imported)) { return; }
    // Original handler assigns new GUIDs to returned top nodes only. Internal IDs remain a mapped logical input contract.
    Count(R,TEXT("new_guid_calls"));Lease.Composite->CreateNewGuid();if(!Lease.Composite->NodeGuid.IsValid()) { Lease.Bad(TEXT("new_top_guid_invalid"));return; }
    for(const auto& N:Lease.Nodes) { if(N.Object!=Lease.Composite&&N.Guid==Lease.Composite->NodeGuid) { Lease.Bad(TEXT("new_top_guid_collision"));return; } }
    for(auto& N:Lease.Nodes) { if(N.Object==Lease.Composite) { N.Guid=Lease.Composite->NodeGuid; } }
    for(const auto& Row:R.Data->GetArrayField(TEXT("collapsed_mapping"))) { auto J=Row->AsObject();if(J->GetIntegerField(TEXT("source_object_index"))==0) { J->SetStringField(TEXT("returned_node_id"),J->GetStringField(TEXT("actual_node_id")));J->SetStringField(TEXT("actual_node_id"),Lease.Composite->NodeGuid.ToString()); } }
    for(auto& N:Lease.Nodes) { for(auto& P:N.Pins) { P.LinkIds.Reset(); } }if(!Lease.SealLinks(Lease.Nodes)||!Lease.Plain()) { return; }
    Count(R,TEXT("notify_graph_calls"));Target->NotifyGraphChanged();if(!Lease.Plain()) { return; }
    R.Stage=TEXT("collapsed_structural");Count(R,TEXT("mark_structural_calls"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Found);R.Data->SetBoolField(TEXT("implicit_skeleton_regeneration_possible"),true);
    if(!Lease.Reacquire()||!Lease.MathSnapshot()) { return; }R.bChangeStateKnown=true;R.Data->SetBoolField(TEXT("collapsed_tree_and_captured_survivors_preserved"),true);
    if(Compile)
    {
        R.Stage=TEXT("collapsed_compile");R.bCompileAttempted=true;R.bChangeStateKnown=false;FCompilerResultsLog Log;Count(R,TEXT("full_compile_calls"));FKismetEditorUtilities::CompileBlueprint(Found,EBlueprintCompileOptions::None,&Log);Count(R,TEXT("full_compile_returns"));
        R.Data->SetNumberField(TEXT("compiler_errors"),Log.NumErrors);R.Data->SetNumberField(TEXT("compiler_warnings"),Log.NumWarnings);if(!Loaded(Found)) { Lease.Bad(TEXT("compiler_returned_owner_lost"));return; }
        R.Compiled=Log.NumErrors==0&&Found->Status!=BS_Error;auto GeneratedReceipt=Found->GeneratedClass;Legacy->SetBoolField(TEXT("compiled"),GeneratedReceipt.GetGCPtr().IsResolved()&&GeneratedReceipt.GetGCPtr().Get()!=nullptr);
        if(!R.Compiled.GetValue())
        {
            auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("stage"),R.Stage);Receipt->SetStringField(TEXT("code"),TEXT("graph_transfer_compile_failed"));Receipt->SetStringField(TEXT("error"),TEXT("Actual full compiler errors/status failed; original GeneratedClass predicate is retained separately."));Receipt->SetNumberField(TEXT("compiler_errors"),Log.NumErrors);Receipt->SetNumberField(TEXT("compiler_warnings"),Log.NumWarnings);R.Data->SetObjectField(TEXT("compile_failure_before_requested_save"),Receipt);
        }
        if(!Lease.Reacquire()||!Lease.MathSnapshot()) { return; }R.bChangeStateKnown=true;
        // Original Finish attempts requested save even after a known failed compile. Unknown still returns before save.
    }
    if(Save)
    {
        R.Stage=TEXT("collapsed_save");R.bSaveAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("forced_save_calls"));const bool Saved=UEditorAssetLibrary::SaveLoadedAsset(Found,false);Count(R,TEXT("forced_save_returns"));R.bSaved=Saved;Legacy->SetBoolField(TEXT("saved"),Saved);
        if(!Saved) { Lease.Stopped=true;Fail(R,TEXT("graph_transfer_save_failed"),TEXT("Forced SaveLoadedAsset returnedFalse; actual receipt retained and no further source SDK is called."));return; }
        if(!Lease.Plain()||!Lease.MathSnapshot()) { return; }R.bChangeStateKnown=true;
    }
    TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& N:Lease.Nodes) { if(N.Object==Lease.Composite) { auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("node_id"),N.Guid.ToString());J->SetStringField(TEXT("name"),N.Object->GetName());J->SetStringField(TEXT("class"),N.Class->GetPathName());J->SetNumberField(TEXT("x"),N.X);J->SetNumberField(TEXT("y"),N.Y);Rows.Add(MakeShared<FJsonValueObject>(J)); } }
    Legacy->SetArrayField(TEXT("imported_nodes"),Rows);Legacy->SetBoolField(TEXT("dirty_after"),Found->GetOutermost()->IsDirty());R.Data->SetBoolField(TEXT("current_imported_nodes_verified"),true);R.Data->SetBoolField(TEXT("runtime_execution_verified"),false);R.Data->SetBoolField(TEXT("opaque_paste_callback_intermediate_states_verified"),false);
    Terminal(R);return;
        };
        ApplyClear();
        if(!Lease.Stopped&&R.bChangeStateKnown){const FString TerminalStage=R.Stage;R.Stage=TEXT("clear_transaction_publication");if(!Lease.Plain())return R;R.Data->SetBoolField(TEXT("transaction_publication_current_verified"),true);R.Stage=TerminalStage;}
        return R;
    }
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","ImportCollapsedBlueprintGraphText","Import Collapsed Blueprint Graph Text"));
    if(!Lease.Plain()||!Lease.MathSnapshot())return R;
    R.Stage=TEXT("collapsed_apply_import");R.bMutationAttempted=R.bChanged=true;R.bChangeStateKnown=false;
    Count(R,TEXT("blueprint_modify_calls"));const bool BPModified=Found->Modify();R.Data->SetBoolField(TEXT("blueprint_modify_transaction_recorded"),BPModified);if(!Lease.Plain()) { return R; }
    Count(R,TEXT("graph_modify_calls"));const bool GraphModified=Target->Modify();R.Data->SetBoolField(TEXT("graph_modify_transaction_recorded"),GraphModified);if(!Lease.Plain()) { return R; }
    TSet<UEdGraphNode*> Imported;Count(R,TEXT("import_nodes_calls"));FEdGraphUtilities::ImportNodesFromText(Target,TextToImport,Imported);
    for(auto* N:Imported) { Lease.Hold(N); }Count(R,TEXT("import_nodes_returns"));Legacy->SetNumberField(TEXT("imported_node_count"),Imported.Num());R.Data->SetNumberField(TEXT("imported_returned_count"),Imported.Num());
    if(!CollapsedAcquired(Lease,Plan,Imported)) { return R; }
    // Original handler assigns new GUIDs to returned top nodes only. Internal IDs remain a mapped logical input contract.
    Count(R,TEXT("new_guid_calls"));Lease.Composite->CreateNewGuid();if(!Lease.Composite->NodeGuid.IsValid()) { Lease.Bad(TEXT("new_top_guid_invalid"));return R; }
    for(const auto& N:Lease.Nodes) { if(N.Object!=Lease.Composite&&N.Guid==Lease.Composite->NodeGuid) { Lease.Bad(TEXT("new_top_guid_collision"));return R; } }
    for(auto& N:Lease.Nodes) { if(N.Object==Lease.Composite) { N.Guid=Lease.Composite->NodeGuid; } }
    for(const auto& Row:R.Data->GetArrayField(TEXT("collapsed_mapping"))) { auto J=Row->AsObject();if(J->GetIntegerField(TEXT("source_object_index"))==0) { J->SetStringField(TEXT("returned_node_id"),J->GetStringField(TEXT("actual_node_id")));J->SetStringField(TEXT("actual_node_id"),Lease.Composite->NodeGuid.ToString()); } }
    for(auto& N:Lease.Nodes) { for(auto& P:N.Pins) { P.LinkIds.Reset(); } }if(!Lease.SealLinks(Lease.Nodes)||!Lease.Plain()) { return R; }
    Count(R,TEXT("notify_graph_calls"));Target->NotifyGraphChanged();if(!Lease.Plain()) { return R; }
    R.Stage=TEXT("collapsed_structural");Count(R,TEXT("mark_structural_calls"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Found);R.Data->SetBoolField(TEXT("implicit_skeleton_regeneration_possible"),true);
    if(!Lease.Reacquire()||!Lease.MathSnapshot()) { return R; }R.bChangeStateKnown=true;R.Data->SetBoolField(TEXT("collapsed_tree_and_captured_survivors_preserved"),true);
    if(Compile)
    {
        R.Stage=TEXT("collapsed_compile");R.bCompileAttempted=true;R.bChangeStateKnown=false;FCompilerResultsLog Log;Count(R,TEXT("full_compile_calls"));FKismetEditorUtilities::CompileBlueprint(Found,EBlueprintCompileOptions::None,&Log);Count(R,TEXT("full_compile_returns"));
        R.Data->SetNumberField(TEXT("compiler_errors"),Log.NumErrors);R.Data->SetNumberField(TEXT("compiler_warnings"),Log.NumWarnings);if(!Loaded(Found)) { Lease.Bad(TEXT("compiler_returned_owner_lost"));return R; }
        R.Compiled=Log.NumErrors==0&&Found->Status!=BS_Error;auto GeneratedReceipt=Found->GeneratedClass;Legacy->SetBoolField(TEXT("compiled"),GeneratedReceipt.GetGCPtr().IsResolved()&&GeneratedReceipt.GetGCPtr().Get()!=nullptr);
        if(!R.Compiled.GetValue())
        {
            auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("stage"),R.Stage);Receipt->SetStringField(TEXT("code"),TEXT("graph_transfer_compile_failed"));Receipt->SetStringField(TEXT("error"),TEXT("Actual full compiler errors/status failed; original GeneratedClass predicate is retained separately."));Receipt->SetNumberField(TEXT("compiler_errors"),Log.NumErrors);Receipt->SetNumberField(TEXT("compiler_warnings"),Log.NumWarnings);R.Data->SetObjectField(TEXT("compile_failure_before_requested_save"),Receipt);
        }
        if(!Lease.Reacquire()||!Lease.MathSnapshot()) { return R; }R.bChangeStateKnown=true;
        // Original Finish attempts requested save even after a known failed compile. Unknown still returns before save.
    }
    if(Save)
    {
        R.Stage=TEXT("collapsed_save");R.bSaveAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("forced_save_calls"));const bool Saved=UEditorAssetLibrary::SaveLoadedAsset(Found,false);Count(R,TEXT("forced_save_returns"));R.bSaved=Saved;Legacy->SetBoolField(TEXT("saved"),Saved);
        if(!Saved) { Lease.Stopped=true;Fail(R,TEXT("graph_transfer_save_failed"),TEXT("Forced SaveLoadedAsset returnedFalse; actual receipt retained and no further source SDK is called."));return R; }
        if(!Lease.Plain()||!Lease.MathSnapshot()) { return R; }R.bChangeStateKnown=true;
    }
    TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& N:Lease.Nodes) { if(N.Object==Lease.Composite) { auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("node_id"),N.Guid.ToString());J->SetStringField(TEXT("name"),N.Object->GetName());J->SetStringField(TEXT("class"),N.Class->GetPathName());J->SetNumberField(TEXT("x"),N.X);J->SetNumberField(TEXT("y"),N.Y);Rows.Add(MakeShared<FJsonValueObject>(J)); } }
    Legacy->SetArrayField(TEXT("imported_nodes"),Rows);Legacy->SetBoolField(TEXT("dirty_after"),Found->GetOutermost()->IsDirty());R.Data->SetBoolField(TEXT("current_imported_nodes_verified"),true);R.Data->SetBoolField(TEXT("runtime_execution_verified"),false);R.Data->SetBoolField(TEXT("opaque_paste_callback_intermediate_states_verified"),false);
    Terminal(R);return R;
}


}

Variable::FResult Import(const FString& Path,const FString& InlineText,const FString& FilePath,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool Clear,bool KeepRoot,bool Allow,bool ValidateOnly,bool DryRun,bool Compile,bool Save)
{
    if(IsTimelineText(InlineText)) { return ImportTimeline(Path,InlineText,FilePath,GraphName,GraphId,GraphType,Clear,KeepRoot,Allow,ValidateOnly,DryRun,Compile,Save); }
    if(IsCollapsedText(InlineText)) { return ImportCollapsed(Path,InlineText,FilePath,GraphName,GraphId,GraphType,Clear,KeepRoot,Allow,ValidateOnly,DryRun,Compile,Save); }
    FResult R;Init(R,Compile,Save);if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("graph_transfer_editor_invalid"),TEXT("Original graph import requires idle Editor game thread, no PIE/SIE."));return R; }
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","ImportBlueprintGraphText","Import Blueprint Graph Text"));UBlueprint* BP=Load(Path,R);if(!BP) { return R; }const TStrongObjectPtr<UBlueprint> KeepBP(BP);TSharedRef<FJsonObject> BeforeResolve=MakeShared<FJsonObject>();if(!Capture(BP,TEXT("before_resolution"),R,BeforeResolve)) { return R; }UEdGraph* G=Resolve(BP,GraphName,GraphId,GraphType,R);if(!G) { return R; }const TStrongObjectPtr<UEdGraph> KeepGraph(G);const FGuid Guid=G->GraphGuid;
    FString TextToImport=InlineText;if(!Text(TextToImport,MaxText,R)||!Text(FilePath,4096,R)) { return R; }if(TextToImport.IsEmpty()&&!FilePath.IsEmpty()) { Count(R,TEXT("file_size_query_calls"));const int64 Bytes=IFileManager::Get().FileSize(*FilePath);R.Data->SetStringField(TEXT("file_size_bytes"),LexToString(Bytes));if(Bytes>static_cast<int64>(MaxText)*4) { Fail(R,TEXT("graph_transfer_file_bound"),TEXT("File byte bound exceeded before whole-file read; larger original files remain pending."));return R; }Count(R,TEXT("file_read_calls"));const bool Read=FFileHelper::LoadFileToString(TextToImport,*FilePath);Count(R,TEXT("file_read_returns"));R.Data->SetBoolField(TEXT("file_read_returned"),Read);if(!Read) { Fail(R,TEXT("graph_transfer_file_read_failed"),TEXT("Original text_file_path read failed."));return R; }if(!Text(TextToImport,MaxText,R)) { return R; } }if(TextToImport.IsEmpty()) { Fail(R,TEXT("graph_transfer_text_missing"),TEXT("Provide either text or text_file_path."));return R; }if(!Sample(BP,Allow,R)) { return R; }
    const bool Validate=ValidateOnly||DryRun;auto Legacy=MakeShared<FJsonObject>();Legacy->SetStringField(TEXT("command"),TEXT("import_blueprint_graph_nodes_from_text"));Legacy->SetStringField(TEXT("blueprint_name"),Path);Legacy->SetStringField(TEXT("blueprint_path"),BP->GetPathName());Legacy->SetStringField(TEXT("blueprint_package_path"),BP->GetOutermost()->GetName());Legacy->SetStringField(TEXT("graph_name"),G->GetName());Legacy->SetStringField(TEXT("graph_id"),Guid.ToString());Legacy->SetStringField(TEXT("graph_schema"),G->GetSchema()->GetClass()->GetName());Legacy->SetStringField(TEXT("text_file_path"),FilePath);Legacy->SetNumberField(TEXT("text_length"),TextToImport.Len());Legacy->SetBoolField(TEXT("validate_only"),Validate);Legacy->SetBoolField(TEXT("sample_or_temp_target"),R.Data->GetBoolField(TEXT("sample_or_temp_target")));Legacy->SetBoolField(TEXT("allow_non_sample"),Allow);Legacy->SetBoolField(TEXT("clear_graph"),Clear);Legacy->SetBoolField(TEXT("keep_root_node"),KeepRoot);Legacy->SetBoolField(TEXT("compile_requested"),Compile);Legacy->SetBoolField(TEXT("save_requested"),Save);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
    if(!Admission(BP,TextToImport,R)) { return R; }R.Stage=TEXT("can_import");Count(R,TEXT("can_import_calls"));const bool Can=FEdGraphUtilities::CanImportNodesFromText(G,TextToImport);Count(R,TEXT("can_import_returns"));Legacy->SetBoolField(TEXT("success"),Can);Legacy->SetBoolField(TEXT("can_import"),Can);R.Data->SetBoolField(TEXT("can_import_preflight_side_effect_free"),false);if(!Can) { Legacy->SetStringField(TEXT("error"),TEXT("FEdGraphUtilities::CanImportNodesFromText returned false"));Fail(R,TEXT("graph_transfer_cannot_import"),Legacy->GetStringField(TEXT("error")));return R; }
    TSharedRef<FJsonObject> Before=MakeShared<FJsonObject>();if(!Capture(BP,TEXT("before_import"),R,Before)) { return R; }if(Validate) { Legacy->SetNumberField(TEXT("imported_node_count"),0);Finish(BP,true,R,Legacy);Terminal(R);return R; }
    R.Stage=TEXT("apply_import");R.bMutationAttempted=true;R.bChangeStateKnown=false;R.bChanged=true;Count(R,TEXT("blueprint_modify_calls"));BP->Modify();Count(R,TEXT("graph_modify_calls"));G->Modify();int32 Removed=0;TArray<TSharedPtr<FJsonValue>> Removal;
    if(Clear)
    {
        TArray<TStrongObjectPtr<UEdGraphNode>> Old;for(UEdGraphNode* N:G->Nodes) { if(N) { Old.Emplace(N); } }
        UClass* AnimRoot=FindObject<UClass>(nullptr,TEXT("/Script/AnimGraph.AnimGraphNode_Root"));for(const auto& Held:Old) { auto* N=Held.Get();if(KeepRoot&&(N->IsA(UK2Node_FunctionEntry::StaticClass())||N->IsA(UK2Node_FunctionResult::StaticClass())||(Loaded(AnimRoot)&&N->IsA(AnimRoot)))) { continue; }auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("node_id"),N->NodeGuid.ToString());Q->SetStringField(TEXT("node_path"),N->GetPathName());Count(R,TEXT("node_modify_calls"));N->Modify();Count(R,TEXT("remove_node_calls"));const bool Returned=G->RemoveNode(N);Count(R,TEXT("remove_node_returns"));Q->SetBoolField(TEXT("returned"),Returned);Removal.Add(MakeShared<FJsonValueObject>(Q));R.Data->SetArrayField(TEXT("removal_receipts"),Removal);++Removed; }
    }
    Legacy->SetNumberField(TEXT("removed_node_count"),Removed);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);TSet<UEdGraphNode*> Imported;Count(R,TEXT("import_nodes_calls"));FEdGraphUtilities::ImportNodesFromText(G,TextToImport,Imported);Count(R,TEXT("import_nodes_returns"));R.Data->SetNumberField(TEXT("imported_returned_count"),Imported.Num());TArray<TStrongObjectPtr<UEdGraphNode>> HoldImported;TArray<TSharedPtr<FJsonValue>> Returned;
    for(UEdGraphNode* N:Imported) { auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("returned_node_path"),GetPathNameSafe(N));Returned.Add(MakeShared<FJsonValueObject>(Q));R.Data->SetArrayField(TEXT("import_returned_prefix"),Returned);if(N) { HoldImported.Emplace(N);Count(R,TEXT("new_guid_calls"));N->CreateNewGuid();Q->SetStringField(TEXT("new_node_id"),N->NodeGuid.ToString()); } }
    Legacy->SetNumberField(TEXT("imported_node_count"),Imported.Num());Count(R,TEXT("notify_graph_calls"));G->NotifyGraphChanged();Count(R,TEXT("mark_structural_calls"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);R.Data->SetBoolField(TEXT("implicit_skeleton_regeneration_possible"),true);
    TSharedRef<FJsonObject> After=MakeShared<FJsonObject>();if(!Capture(BP,TEXT("after_import"),R,After)) { return R; }R.bChangeStateKnown=true;FIndex Current;if(!Index(BP,Current,R)) { R.bChangeStateKnown=false;return R; }TArray<TSharedPtr<FJsonValue>> Rows;bool Good=Imported.Num()>0;TSet<FGuid> NewIds;
    for(UEdGraphNode* N:Imported) { if(!N||!Current.Nodes.Contains(N)||N->GetGraph()!=G||!N->NodeGuid.IsValid()||NewIds.Contains(N->NodeGuid)) { Good=false;continue; }NewIds.Add(N->NodeGuid);Rows.Add(MakeShared<FJsonValueObject>(Node(N,Current)));if(!N->HasAnyFlags(RF_Transactional)||!N->GetClass()->HasAnyClassFlags(CLASS_Native)||!N->GetClass()->GetPathName().StartsWith(TEXT("/Script/BlueprintGraph."))) { Good=false; }for(UEdGraphPin* P:N->Pins) { if(!P) { continue; }if((P->DefaultObject&&!RuntimeObject(P->DefaultObject))||(P->PinType.PinSubCategoryObject.Get()&&!RuntimeObject(P->PinType.PinSubCategoryObject.Get()))) { Good=false; } }if(auto* Call=Cast<UK2Node_CallFunction>(N)) { Good&=RuntimeObject(Call->GetTargetFunction()); } }
    Legacy->SetArrayField(TEXT("imported_nodes"),Rows);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.Data->SetBoolField(TEXT("current_imported_nodes_verified"),Good);if(!Protected(Before,After,Guid,R)||!Good) { if(R.ErrorCode.IsEmpty()) { Fail(R,TEXT("graph_transfer_import_postcondition_failed"),TEXT("Returned imported node/class/type/reference/current graph proof incomplete; accepted clear/paste prefix retained, no rollback or compiler/save replay.")); }return R; }
    Finish(BP,false,R,Legacy);Terminal(R);return R;
}
Variable::FResult Repair(const FString& Path,bool Allow,bool ValidateOnly,bool DryRun,bool Compile,bool Save)
{
    FResult R;Init(R,Compile,Save);if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("graph_transfer_editor_invalid"),TEXT("Original repair requires idle Editor game thread, no PIE/SIE."));return R; }Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","RepairBlueprintPinLinks","Repair Blueprint Pin Links"));UBlueprint* BP=Load(Path,R);if(!BP) { return R; }const TStrongObjectPtr<UBlueprint> KeepBP(BP);if(!Sample(BP,Allow,R)) { return R; }const bool Validate=ValidateOnly||DryRun;TSharedRef<FJsonObject> Before=MakeShared<FJsonObject>();if(!Capture(BP,TEXT("before_repair"),R,Before)) { return R; }FIndex I;if(!Index(BP,I,R)) { return R; }
    TArray<TStrongObjectPtr<UEdGraph>> KeepGraphs;TArray<TStrongObjectPtr<UEdGraphNode>> KeepNodes;for(UEdGraph* G:I.Graphs) { if(G) { KeepGraphs.Emplace(G); } }for(UEdGraphNode* N:I.Nodes) { KeepNodes.Emplace(N); }
    TMap<UEdGraph*,int32> GraphChanges;int32 Null=0,Duplicate=0,Self=0,External=0,Schema=0,Reciprocal=0,Skipped=0,ChangedPins=0;TArray<TSharedPtr<FJsonValue>> Summaries;auto MarkGraph=[&](UEdGraph* G){ if(G) { ++GraphChanges.FindOrAdd(G); } };
    R.Stage=Validate?TEXT("validate_links"):TEXT("repair_links");
    for(UEdGraph* G:I.Graphs)
    {
        if(!G) { continue; }int32 Visited=0,Changes=0;for(UEdGraphNode* N:G->Nodes)
        {
            if(!N) { continue; }for(UEdGraphPin* P:N->Pins)
            {
                if(!P) { continue; }++Visited;bool Change=false;TSet<UEdGraphPin*> Seen;TArray<UEdGraphPin*> Clean;Clean.Reserve(P->LinkedTo.Num());for(UEdGraphPin* L:P->LinkedTo)
                {
                    if(!L) { ++Null;Change=true;continue; }if(L==P) { ++Self;Change=true;continue; }if(!I.Pins.Contains(L)) { ++External;Change=true;continue; }if(Disallowed(G->GetSchema(),P,L,R)) { ++Schema;Change=true;continue; }if(Seen.Contains(L)) { ++Duplicate;Change=true;continue; }Seen.Add(L);Clean.Add(L);
                }
                if(Change&&!Validate) { R.bMutationAttempted=R.bChanged=true;R.bChangeStateKnown=false;Count(R,TEXT("pin_modify_calls"));P->Modify();Count(R,TEXT("link_array_write_calls"));P->LinkedTo=Clean;MarkGraph(G);++Changes;++ChangedPins; }else if(Change) { ++Changes;++ChangedPins; }
            }
        }
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("graph_name"),G->GetName());J->SetStringField(TEXT("graph_id"),G->GraphGuid.ToString());J->SetNumberField(TEXT("pins_visited"),Visited);J->SetNumberField(TEXT("first_pass_changes"),Changes);Summaries.Add(MakeShared<FJsonValueObject>(J));
    }
    // Original set iteration and dry-run reading of unmodified first-pass arrays are retained.
    for(UEdGraphPin* P:I.Pins)
    {
        UEdGraph* PG=I.Owners.FindChecked(P)->GetGraph();for(UEdGraphPin* L:P->LinkedTo)
        {
            if(!L||!I.Pins.Contains(L)) { continue; }UEdGraph* LG=I.Owners.FindChecked(L)->GetGraph();if(!L->LinkedTo.Contains(P)) { const auto* S=PG?PG->GetSchema():LG->GetSchema();if(Disallowed(S,P,L,R)) { ++Skipped;continue; }++Reciprocal;MarkGraph(LG);if(!Validate) { R.bMutationAttempted=R.bChanged=true;R.bChangeStateKnown=false;Count(R,TEXT("pin_modify_calls"));L->Modify();Count(R,TEXT("reciprocal_add_calls"));L->LinkedTo.Add(P); }++ChangedPins; }if(LG!=PG) { MarkGraph(PG);MarkGraph(LG); }
        }
    }
    const bool Had=Null>0||Duplicate>0||Self>0||External>0||Schema>0||Reciprocal>0;if(Had&&!Validate) { Count(R,TEXT("blueprint_modify_calls"));BP->Modify();for(const auto& Pair:GraphChanges) { if(Pair.Key) { Count(R,TEXT("graph_modify_calls"));Pair.Key->Modify();Count(R,TEXT("notify_graph_calls"));Pair.Key->NotifyGraphChanged(); } }Count(R,TEXT("mark_modified_calls"));FBlueprintEditorUtils::MarkBlueprintAsModified(BP); }
    auto Legacy=MakeShared<FJsonObject>();Legacy->SetBoolField(TEXT("success"),true);Legacy->SetStringField(TEXT("command"),TEXT("repair_blueprint_pin_links"));Legacy->SetBoolField(TEXT("validate_only"),Validate);Legacy->SetStringField(TEXT("blueprint_name"),Path);Legacy->SetStringField(TEXT("blueprint_path"),BP->GetPathName());Legacy->SetStringField(TEXT("blueprint_package_path"),BP->GetOutermost()->GetName());Legacy->SetBoolField(TEXT("sample_or_temp_target"),R.Data->GetBoolField(TEXT("sample_or_temp_target")));Legacy->SetBoolField(TEXT("allow_non_sample"),Allow);Legacy->SetBoolField(TEXT("compile_requested"),Compile);Legacy->SetBoolField(TEXT("save_requested"),Save);Legacy->SetNumberField(TEXT("graph_count"),I.Graphs.Num());Legacy->SetNumberField(TEXT("node_count"),I.NodeEntries);Legacy->SetNumberField(TEXT("pin_count"),I.PinEntries);Legacy->SetNumberField(TEXT("changed_pin_count"),ChangedPins);Legacy->SetNumberField(TEXT("removed_null_links"),Null);Legacy->SetNumberField(TEXT("removed_duplicate_links"),Duplicate);Legacy->SetNumberField(TEXT("removed_self_links"),Self);Legacy->SetNumberField(TEXT("removed_external_links"),External);Legacy->SetNumberField(TEXT("removed_schema_disallowed_links"),Schema);Legacy->SetNumberField(TEXT("added_reciprocal_links"),Reciprocal);Legacy->SetNumberField(TEXT("skipped_schema_disallowed_reciprocal_links"),Skipped);Legacy->SetBoolField(TEXT("had_changes"),Had);Legacy->SetArrayField(TEXT("graphs"),Summaries);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
    TSharedRef<FJsonObject> After=MakeShared<FJsonObject>();if(!Capture(BP,TEXT("after_repair"),R,After)) { return R; }R.bChangeStateKnown=true;if(Validate) { const bool Same=Equal(Before,After);R.Data->SetBoolField(TEXT("validation_owned_state_preserved"),Same);if(!Same) { Fail(R,TEXT("graph_transfer_validation_state_changed"),TEXT("Schema validation/current owned state differs; no apply, no fake known-zero total effect claim."));return R; }Finish(BP,true,R,Legacy);Terminal(R);return R; }
    FIndex Current;if(!Index(BP,Current,R)||!HealthyLinks(Current,R)) { Fail(R,TEXT("graph_transfer_repair_postcondition_failed"),TEXT("Current owned link arrays still malformed; accepted repair prefix retained without retry."));return R; }if(!Before->GetStringField(TEXT("parent_class")).Equals(After->GetStringField(TEXT("parent_class")),ESearchCase::CaseSensitive)||!Equal(Before->GetObjectField(TEXT("member_declarations")).ToSharedRef(),After->GetObjectField(TEXT("member_declarations")).ToSharedRef())) { Fail(R,TEXT("graph_transfer_repair_source_changed"),TEXT("Member declarations/parent changed during repair callbacks; accepted prefix retained."));return R; }
    Finish(BP,false,R,Legacy);Terminal(R);return R;
}
}
