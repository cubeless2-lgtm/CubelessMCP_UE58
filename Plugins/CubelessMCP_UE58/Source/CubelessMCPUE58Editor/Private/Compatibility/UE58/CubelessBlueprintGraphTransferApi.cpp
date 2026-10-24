#include "Compatibility/UE58/CubelessBlueprintGraphTransferApi.h"
#include "Internationalization/Text.h"
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
#include "K2Node_MacroInstance.h"
#include "K2Node_Tunnel.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_Select.h"
#include "K2Node_Knot.h"
#include "K2Node_Switch.h"
#include "K2Node_SwitchInteger.h"
#include "K2Node_SwitchName.h"
#include "K2Node_SwitchString.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetStringLibrary.h"
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
#include "K2Node_EditablePinBase.h"
#include "UObject/Interface.h"
#include "K2Node_FunctionResult.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet/BlueprintFunctionLibrary.h"
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
// YE: Macro libraries are authoring containers. No generated class/CDO or
// physical tunnel-pin cache is assumed necessary or stable for their graph work.
bool MacroLibraryRole(UBlueprint* BP,FResult& R)
{
    if(!Loaded(BP)||BP->GetClass()!=UBlueprint::StaticClass()||BP->BlueprintType!=BPTYPE_MacroLibrary
        ||BP->GetOuter()!=BP->GetPackage()||!BP->GetPackage()->GetName().StartsWith(TEXT("/Game/"))
        ||!ReferenceGuard(BP->GetPathName())||BP->bBeingCompiled||BP->bQueuedForCompilation
        ||BP->bIsRegeneratingOnLoad||BP->bSuppressStructurallyModified||BP->Status==BS_BeingCreated
        ||!BP->ParentClass.GetGCPtr().IsResolved()
        ||(BP->ParentClass.GetGCPtr()!=UObject::StaticClass()&&BP->ParentClass.GetGCPtr()!=AActor::StaticClass())
        ||!BP->NewVariables.IsEmpty()||!BP->FunctionGraphs.IsEmpty()||!BP->UbergraphPages.IsEmpty()
        ||!BP->EventGraphs.IsEmpty()||!BP->DelegateSignatureGraphs.IsEmpty()||!BP->ImplementedInterfaces.IsEmpty()
        ||!BP->ComponentTemplates.IsEmpty()||!BP->Timelines.IsEmpty()||!BP->SimpleConstructionScript.IsResolved()
        ||BP->SimpleConstructionScript!=nullptr||BP->MacroGraphs.Num()>MaxGraphs)
        return Fail(R,TEXT("graph_transfer_macro_library_scope_pending"),TEXT("Direct native Object/Actor MacroLibrary with existing own macro graphs and no other authoring roles required; generated/custom parents and wider source shapes remain incomplete."));
    TSet<UEdGraph*> Seen;
    for(const auto& Slot:BP->MacroGraphs)
    {
        if(!Slot.IsResolved()||Slot==nullptr)return Fail(R,TEXT("graph_transfer_macro_library_graph_unresolved"),TEXT("Resolved existing macro graph slots required; no graph loading or creation."));
        const FString Path=Slot.GetPathName();auto* G=Cast<UEdGraph>(StaticFindObject(nullptr,nullptr,*Path));
        if(!Loaded(G)||Slot!=G||Seen.Contains(G)||G->GetClass()!=UEdGraph::StaticClass()||G->GetOuter()!=BP
            ||G->GetPackage()!=BP->GetPackage()||!G->Schema.GetGCPtr().IsResolved()
            ||G->Schema.GetGCPtr()!=UEdGraphSchema_K2::StaticClass()||!G->SubGraphs.IsEmpty())
            return Fail(R,TEXT("graph_transfer_macro_library_graph_pending"),TEXT("Exact directly owned existing stock K2 macro graphs required; nested/specialized graph source variants remain incomplete."));
        Seen.Add(G);
    }
    return true;
}
// ADF: an operation-specific existing BPI signature owner for repair only.
// No Import permission, CDO request or generated/skeleton cache identity gate.
bool InterfaceRepairRole(UBlueprint* BP,FResult& R)
{
    if(!Loaded(BP)||BP->GetClass()!=UBlueprint::StaticClass()||BP->GetOuter()!=BP->GetPackage()||BP->BlueprintType!=BPTYPE_Interface||!BP->GetExtensions().IsEmpty()||!BP->ParentClass.GetGCPtr().IsResolved()||BP->ParentClass.GetGCPtr()!=UInterface::StaticClass()||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bIsRegeneratingOnLoad||BP->bSuppressStructurallyModified||BP->Status==BS_BeingCreated||BP->FunctionGraphs.Num()>MaxGraphs||!BP->NewVariables.IsEmpty()||!BP->MacroGraphs.IsEmpty()||!BP->UbergraphPages.IsEmpty()||!BP->EventGraphs.IsEmpty()||!BP->DelegateSignatureGraphs.IsEmpty()||!BP->ImplementedInterfaces.IsEmpty()||!BP->ComponentTemplates.IsEmpty()||!BP->Timelines.IsEmpty()||!BP->SimpleConstructionScript.IsResolved()||BP->SimpleConstructionScript!=nullptr)
        return Fail(R,TEXT("graph_transfer_interface_repair_scope_pending"),TEXT("Direct native Interface Blueprint with existing own signature graphs and no actor/widget/component/inherited-interface roles required; wider original owners remain incomplete."));
    TSet<UEdGraph*> Seen;
    for(const auto& Slot:BP->FunctionGraphs)
    {
        if(!Slot.IsResolved()||Slot==nullptr)return Fail(R,TEXT("graph_transfer_interface_graph_pending"),TEXT("Existing resolved interface signature graphs required; no graph loading or creation."));
        auto* G=Cast<UEdGraph>(StaticFindObject(nullptr,nullptr,*Slot.GetPathName()));
        if(!Loaded(G)||Slot!=G||Seen.Contains(G)||G->GetClass()!=UEdGraph::StaticClass()||G->GetOuter()!=BP||G->GetPackage()!=BP->GetPackage()||!G->Schema.GetGCPtr().IsResolved()||G->Schema.GetGCPtr()!=UEdGraphSchema_K2::StaticClass()||!G->SubGraphs.IsEmpty()||G->Nodes.Num()>MaxNodes)
            return Fail(R,TEXT("graph_transfer_interface_graph_pending"),TEXT("Existing directly owned native K2 interface signature graphs required; nested/specialized bodies remain incomplete."));
        Seen.Add(G);int32 Entry=0,Result=0;
        for(const auto& NodeSlot:G->Nodes)
        {
            if(!NodeSlot.IsResolved()||NodeSlot==nullptr)return Fail(R,TEXT("graph_transfer_interface_node_pending"),TEXT("Existing resolved signature nodes required."));
            auto* N=Cast<UEdGraphNode>(StaticFindObject(nullptr,nullptr,*NodeSlot.GetPathName()));
            if(!Loaded(N)||NodeSlot!=N||N->GetOuter()!=G||N->GetPackage()!=BP->GetPackage())return Fail(R,TEXT("graph_transfer_interface_node_pending"),TEXT("Actual interface signature node ownership unavailable."));
            if(N->GetClass()==UK2Node_FunctionEntry::StaticClass())++Entry;else if(N->GetClass()==UK2Node_FunctionResult::StaticClass())++Result;else return Fail(R,TEXT("graph_transfer_interface_node_pending"),TEXT("Stock FunctionEntry and optional FunctionResult signature nodes only; executable/custom interface graph bodies remain incomplete."));
        }
        if(Entry!=1||Result>1)return Fail(R,TEXT("graph_transfer_interface_node_pending"),TEXT("One source Entry and at most one Result required for the existing interface signature profile."));
    }
    return true;
}
bool MacroLibraryCurrent(UBlueprint* BP,FResult& R)
{
    bool Interface=false;R.Data->TryGetBoolField(TEXT("native_interface_pin_link_repair"),Interface);
    if(Interface)
    {
        FString Path;R.Data->TryGetStringField(TEXT("interface_repair_blueprint_path"),Path);
        if(!IsEditorGameThread()||IsPlaySessionActive()||!InterfaceRepairRole(BP,R)||BP->GetPathName()!=Path){R.bChangeStateKnown=false;if(R.ErrorCode.IsEmpty())Fail(R,TEXT("graph_transfer_interface_current_lost"),TEXT("Interface repair owner/source role changed; accepted prefix retained, no following compile/save or retry."));return false;}
        return true;
    }
    bool Active=false;R.Data->TryGetBoolField(TEXT("native_macro_library_graph_transfer"),Active);
    if(!Active)return true;
    bool ActorParent=false;R.Data->TryGetBoolField(TEXT("macro_library_parent_is_actor"),ActorParent);
    FString ExpectedPath;R.Data->TryGetStringField(TEXT("macro_library_blueprint_path"),ExpectedPath);
    if(!IsEditorGameThread()||IsPlaySessionActive()||!MacroLibraryRole(BP,R)||BP->GetPathName()!=ExpectedPath
        ||BP->ParentClass.GetGCPtr()!=(ActorParent?AActor::StaticClass():UObject::StaticClass()))
    {if(R.ErrorCode.IsEmpty())Fail(R,TEXT("graph_transfer_macro_library_current_lost"),TEXT("MacroLibrary context/owner role changed; accepted prefix retained, no following compile/save or retry."));if(R.bMutationAttempted)R.bChangeStateKnown=false;return false;}
    return true;
}
bool Scope(UBlueprint* BP,FResult& R,bool AllowFunctionLibrary=false,bool AllowInterfaceRepair=false)
{
    if(!Loaded(BP)||!BP->GetOutermost()->GetName().StartsWith(TEXT("/Game/"))||!ReferenceGuard(BP->GetPathName())||!Loaded(BP->ParentClass)||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bIsRegeneratingOnLoad||BP->bSuppressStructurallyModified) { return Fail(R,TEXT("graph_transfer_owner_pending"),TEXT("Current owned loaded /Game Blueprint, parent and idle compiler state required.")); }
    const bool Library=BP->BlueprintType==BPTYPE_FunctionLibrary;
    const bool Macro=AllowFunctionLibrary&&BP->BlueprintType==BPTYPE_MacroLibrary;
    const bool Interface=AllowInterfaceRepair&&BP->BlueprintType==BPTYPE_Interface;
    if((BP->BlueprintType!=BPTYPE_Normal&&!(AllowFunctionLibrary&&Library)&&!Macro&&!Interface)||BP->NewVariables.Num()>512||BP->Status==BS_BeingCreated) { return Fail(R,TEXT("graph_transfer_blueprint_type_pending"),TEXT("Ordinary Blueprint or operation-opted direct native FunctionLibrary/MacroLibrary required; interface and other Blueprint owners remain original variants.")); }
    if(Interface)
    {
        if(!InterfaceRepairRole(BP,R))return false;
        R.Data->SetBoolField(TEXT("native_interface_pin_link_repair"),true);R.Data->SetStringField(TEXT("interface_repair_blueprint_path"),BP->GetPathName());R.Data->SetBoolField(TEXT("interface_repair_cdo_requested"),false);
        R.Data->SetStringField(TEXT("interface_repair_scope"),TEXT("Original validate/dry-run/apply, link cleanup/reciprocal repair and optional compile/save on existing direct Interface signature graphs. Held source graph/node roles, authored user-pin name/type/direction/default preservation and borrowed repair-pin lifetime protection reuse the existing lease. Compiler-generated class/CDO/skeleton and physical reconstructed pins/order are not authored invariants. Import, inherited interfaces, custom/executable bodies and generated class type descriptors remain incomplete; actual usage validation pending."));
    }
    if(Macro)
    {
        if(!MacroLibraryRole(BP,R))return false;
        R.Data->SetBoolField(TEXT("native_macro_library_graph_transfer"),true);R.Data->SetBoolField(TEXT("macro_library_cdo_requested"),false);R.Data->SetBoolField(TEXT("macro_library_parent_is_actor"),BP->ParentClass.GetGCPtr()==AActor::StaticClass());R.Data->SetStringField(TEXT("macro_library_blueprint_path"),BP->GetPathName());
        R.Data->SetStringField(TEXT("macro_library_scope"),TEXT("Explicit existing own stock K2 MacroGraphs of direct native Object/Actor MacroLibrary, generic flat import and common pin-link repair only. No implicit EventGraph, nested Timeline/Collapsed path, loader, CDO creation or consumer expansion/runtime proof; compile/save and actual usage validation pending."));
    }
    if(Library)
    {
        if(BP->GetClass()!=UBlueprint::StaticClass()||BP->GetOuter()!=BP->GetPackage()||BP->FunctionGraphs.Num()>MaxGraphs||!BP->ParentClass.GetGCPtr().IsResolved()||BP->ParentClass.Get()!=UBlueprintFunctionLibrary::StaticClass()||!BP->GeneratedClass.GetGCPtr().IsResolved()||!BP->SkeletonGeneratedClass.GetGCPtr().IsResolved()||!Loaded(BP->GeneratedClass.Get())||!Loaded(BP->SkeletonGeneratedClass.Get())||BP->GeneratedClass->ClassGeneratedBy!=BP||BP->SkeletonGeneratedClass->ClassGeneratedBy!=BP||BP->GeneratedClass->GetSuperClass()!=UBlueprintFunctionLibrary::StaticClass()||!BP->NewVariables.IsEmpty()||!BP->MacroGraphs.IsEmpty()||!BP->UbergraphPages.IsEmpty()||!BP->EventGraphs.IsEmpty()||!BP->DelegateSignatureGraphs.IsEmpty()||!BP->ImplementedInterfaces.IsEmpty()||!BP->ComponentTemplates.IsEmpty()||!BP->Timelines.IsEmpty()||!BP->SimpleConstructionScript.IsResolved()||BP->SimpleConstructionScript.Get()!=nullptr)
            return Fail(R,TEXT("graph_transfer_function_library_scope_pending"),TEXT("Current ordinary direct native BlueprintFunctionLibrary with existing classes, own functions only and no actor/widget/event/component roles required; generated parents and wider source shapes remain incomplete. No CDO is requested."));
        for(const auto& Slot:BP->FunctionGraphs)
        {if(!Slot.IsResolved())return Fail(R,TEXT("graph_transfer_function_library_graph_unresolved"),TEXT("Existing resolved function-graph slots required; no graph loader."));auto* G=Slot.Get();if(!Loaded(G)||G->GetClass()!=UEdGraph::StaticClass()||G->GetOuter()!=BP||!G->Schema.GetGCPtr().IsResolved()||G->Schema.Get()!=UEdGraphSchema_K2::StaticClass())return Fail(R,TEXT("graph_transfer_function_library_graph_pending"),TEXT("Existing directly owned native K2 function graphs required."));}
        R.Data->SetBoolField(TEXT("native_function_library_graph_transfer"),true);R.Data->SetBoolField(TEXT("function_library_cdo_requested"),false);R.Data->SetStringField(TEXT("function_library_scope"),TEXT("Direct native BlueprintFunctionLibrary import into an explicitly selected existing own function; common pin-link repair of existing K2 function graphs. No implicit EventGraph, graph creation, generated parent, MacroLibrary or specialized Timeline/Collapsed Actor-profile expansion. Existing node/type/reference budgets, partial mutation and requested compiler/save contracts apply; actual BFL paste/repair/compile/save usage validation is pending."));
    }
    return true;
}
bool Sample(UBlueprint* BP,bool Allow,FResult& R)
{
    const FString P=BP->GetOutermost()->GetName();bool B=false;for(const TCHAR* Root:{TEXT("/Game/_MCP_Sample"),TEXT("/Game/_MCP_Temp")}) { B|=P.Equals(Root,ESearchCase::IgnoreCase)||P.StartsWith(FString(Root)+TEXT("/"),ESearchCase::IgnoreCase); }
    R.Data->SetBoolField(TEXT("sample_or_temp_target"),B);R.Data->SetBoolField(TEXT("allow_non_sample"),Allow);return B||Allow||Fail(R,TEXT("graph_transfer_non_sample_refused"),TEXT("Original sample/temp target guard requires allow_non_sample=True elsewhere."));
}
UBlueprint* Load(const FString& P,FResult& R,bool AllowFunctionLibrary=false,bool AllowInterfaceRepair=false)
{
    if(!Text(P,2048,R)||!ReferenceGuard(P)) { Fail(R,TEXT("graph_transfer_path_invalid"),TEXT("Invalid Blueprint path or tooling runtime dependency."));return nullptr; }
    FString ObjectPath=P;if(P.StartsWith(TEXT("/Game/"))&&!P.Contains(TEXT("."))) { ObjectPath+=TEXT(".")+FPackageName::GetShortName(P); }if(!ObjectPath.StartsWith(TEXT("/Game/"))) { Fail(R,TEXT("graph_transfer_path_pending"),TEXT("Initial exact /Game object/package path scope; original bare-name lookup remains pending."));return nullptr; }const bool WasLoaded=FindObject<UBlueprint>(nullptr,*ObjectPath)!=nullptr;R.Data->SetBoolField(TEXT("implicit_postload_work_possible"),!WasLoaded);Count(R,TEXT("asset_load_calls"));auto* BP=LoadObject<UBlueprint>(nullptr,*ObjectPath);Count(R,TEXT("asset_load_returns"));R.Data->SetStringField(TEXT("returned_blueprint_path"),GetPathNameSafe(BP));if(!Scope(BP,R,AllowFunctionLibrary,AllowInterfaceRepair)) { if(!WasLoaded) { R.bChangeStateKnown=false; }return nullptr; }return BP;
}
struct FIndex
{
    TArray<UEdGraph*> Graphs;TSet<UEdGraphNode*> Nodes;TSet<UEdGraphPin*> Pins;TMap<UEdGraphPin*,UEdGraphNode*> Owners;int32 NodeEntries=0,PinEntries=0,Links=0;
};
// Request-local roles only for the newly admitted macro path. Pin addresses are
// guarded while repair uses borrowed storage, then released before notify/compile.
struct FMacroGraphLease
{
    struct FGraphRole{TStrongObjectPtr<UEdGraph> Graph;TStrongObjectPtr<UClass> Class;TStrongObjectPtr<UObject> Outer;TStrongObjectPtr<UPackage> Package;FString Path;FGuid Guid;int32 InterfaceNodeCount=0;};
    struct FNodeRole{TStrongObjectPtr<UEdGraphNode> Node;TStrongObjectPtr<UClass> Class;UEdGraph* Graph=nullptr;FGuid Guid;TSet<UEdGraphPin*> Pins;int32 Count=0;};
    FResult& R;TStrongObjectPtr<UBlueprint> Blueprint;TArray<FGraphRole> Graphs;TArray<FNodeRole> Nodes;bool Active=false,Stopped=false,Interface=false;
    struct FSignatureRole{TStrongObjectPtr<UK2Node_EditablePinBase> Node;UEdGraph* Graph=nullptr;UClass* Class=nullptr;FGuid Guid;TArray<FUserPinInfo> Pins;};
    TArray<FSignatureRole> InterfaceSignatures;TArray<TStrongObjectPtr<UObject>> InterfaceTypes;
    explicit FMacroGraphLease(FResult& In):R(In){}
    bool Stop(const TCHAR* Why)
    {if(!Stopped){Stopped=true;if(R.ErrorCode.IsEmpty())Fail(R,Interface?TEXT("graph_transfer_interface_current_lost"):TEXT("graph_transfer_macro_library_current_lost"),Why);if(Interface||R.bMutationAttempted)R.bChangeStateKnown=false;}return false;}
    bool Bind(UBlueprint* BP)
    {
        R.Data->TryGetBoolField(TEXT("native_macro_library_graph_transfer"),Active);R.Data->TryGetBoolField(TEXT("native_interface_pin_link_repair"),Interface);Active|=Interface;if(!Active)return true;
        if(!MacroLibraryCurrent(BP,R))return Stop(TEXT("Macro source context changed before graph role binding."));Blueprint.Reset(BP);
        const auto& Slots=Interface?BP->FunctionGraphs:BP->MacroGraphs;
        for(const auto& Slot:Slots)
        {auto* G=Cast<UEdGraph>(StaticFindObject(nullptr,nullptr,*Slot.GetPathName()));if(!Loaded(G)||Slot!=G)return Stop(TEXT("Macro graph slot changed during binding."));FGraphRole V;V.Graph.Reset(G);V.Class.Reset(G->GetClass());V.Outer.Reset(G->GetOuter());V.Package.Reset(G->GetPackage());V.Path=G->GetPathName();V.Guid=G->GraphGuid;V.InterfaceNodeCount=Interface?G->Nodes.Num():0;Graphs.Add(MoveTemp(V));}
        if(Interface)
        {
            for(const auto& Graph:Graphs)for(const auto& Slot:Graph.Graph->Nodes)
            {
                auto* N=Cast<UK2Node_EditablePinBase>(StaticFindObject(nullptr,nullptr,*Slot.GetPathName()));if(!Loaded(N)||Slot!=N||N->UserDefinedPins.Num()>512)return Stop(TEXT("Current bounded interface signature declarations required before observer callbacks."));
                FSignatureRole V;V.Node.Reset(N);V.Graph=Graph.Graph.Get();V.Class=N->GetClass();V.Guid=N->NodeGuid;
                for(const auto& Definition:N->UserDefinedPins)
                {
                    if(!Definition.IsValid())return Stop(TEXT("Null source interface user-pin declaration."));V.Pins.Add(*Definition);
                    const auto& T=Definition->PinType;
                    for(UObject* O:{T.PinSubCategoryObject.Get(),T.PinValueType.TerminalSubCategoryObject.Get()})if(O){if(!Loaded(O))return Stop(TEXT("Interface type object unavailable."));if(auto* C=Cast<UClass>(O))if(!C->HasAnyClassFlags(CLASS_Native))return Fail(R,TEXT("graph_transfer_interface_type_pending"),TEXT("Generated/self class descriptors require a separate compiler-aware source role; valid original type remains incomplete, no repair writer dispatched."));InterfaceTypes.Emplace(O);}
                    const auto& Ref=T.PinSubCategoryMemberReference.MemberParent;if(!Ref.IsResolved())return Stop(TEXT("Interface member-reference type source is unresolved."));if(Ref!=nullptr){UObject* O=StaticFindObject(nullptr,nullptr,*Ref.GetPathName());if(!Loaded(O)||Ref!=O)return Stop(TEXT("Interface member-reference source unavailable."));if(auto* C=Cast<UClass>(O))if(!C->HasAnyClassFlags(CLASS_Native))return Fail(R,TEXT("graph_transfer_interface_type_pending"),TEXT("Generated member-reference type source remains incomplete."));InterfaceTypes.Emplace(O);}
                }
                InterfaceSignatures.Add(MoveTemp(V));
            }
        }
        return Current();
    }
    bool Current()
    {
        if(!Active)return true;if(Stopped)return false;auto* BP=Blueprint.Get();
        if(!MacroLibraryCurrent(BP,R)||(Interface?BP->FunctionGraphs.Num():BP->MacroGraphs.Num())!=Graphs.Num())return Stop(TEXT("Macro source context or graph inventory changed after a callback."));
        for(const auto& V:Graphs)
        {auto* G=V.Graph.Get();if(!Loaded(G)||!Loaded(V.Class.Get())||!Loaded(V.Outer.Get())||!Loaded(V.Package.Get())||!(Interface?BP->FunctionGraphs.Contains(G):BP->MacroGraphs.Contains(G))||G->GetClass()!=V.Class.Get()||G->GetOuter()!=V.Outer.Get()||G->GetOuter()!=BP||G->GetPackage()!=V.Package.Get()||G->GetPathName()!=V.Path||G->GraphGuid!=V.Guid)return Stop(TEXT("Held macro graph identity/owner/membership changed; no following SDK or retry."));}
        if(Interface)
        {
            for(const auto& H:InterfaceTypes)if(!Loaded(H.Get()))return Stop(TEXT("Held authored interface type object retired."));
            for(const auto& V:Graphs)if(V.Graph->Nodes.Num()!=V.InterfaceNodeCount)return Stop(TEXT("Authored interface signature node inventory changed."));
            for(const auto& V:InterfaceSignatures)
            {
                auto* N=V.Node.Get();if(!Loaded(N)||N->GetClass()!=V.Class||N->GetOuter()!=V.Graph||!V.Graph->Nodes.Contains(N)||N->NodeGuid!=V.Guid||N->UserDefinedPins.Num()!=V.Pins.Num())return Stop(TEXT("Held interface source declaration role changed."));
                for(int32 I=0;I<V.Pins.Num();++I){const auto& Actual=N->UserDefinedPins[I];const auto& Expected=V.Pins[I];if(!Actual.IsValid()||Actual->PinName!=Expected.PinName||Actual->DesiredPinDirection!=Expected.DesiredPinDirection||!Actual->PinDefaultValue.Equals(Expected.PinDefaultValue,ESearchCase::CaseSensitive)||Actual->PinType!=Expected.PinType||Actual->PinType.bIsUObjectWrapper!=Expected.PinType.bIsUObjectWrapper||Actual->PinType.PinValueType.bTerminalIsUObjectWrapper!=Expected.PinType.PinValueType.bTerminalIsUObjectWrapper||Actual->PinType.bSerializeAsSinglePrecisionFloat!=Expected.PinType.bSerializeAsSinglePrecisionFloat)return Stop(TEXT("Authored interface user-pin name/type/direction/default changed; cache reconstruction is separate."));}
            }
        }
        return true;
    }
    bool BindPins(const FIndex& I)
    {
        if(!Active)return true;if(!Current())return false;
        for(auto* N:I.Nodes)
        {if(!Loaded(N)||!Graphs.ContainsByPredicate([&](const FGraphRole& V){return N->GetOuter()==V.Graph.Get()&&V.Graph->Nodes.Contains(N);}))return Stop(TEXT("Repair node role changed before binding."));FNodeRole V;V.Node.Reset(N);V.Class.Reset(N->GetClass());V.Graph=N->GetGraph();V.Guid=N->NodeGuid;V.Count=N->Pins.Num();for(auto* P:N->Pins)V.Pins.Add(P);Nodes.Add(MoveTemp(V));}
        return PinsCurrent();
    }
    bool PinsCurrent()
    {
        if(!Active)return true;if(!Current())return false;
        for(const auto& V:Nodes)
        {auto* N=V.Node.Get();if(!Loaded(N)||!Loaded(V.Class.Get())||N->GetClass()!=V.Class.Get()||N->GetOuter()!=V.Graph||!V.Graph->Nodes.Contains(N)||N->GetGraph()!=V.Graph||N->NodeGuid!=V.Guid||N->Pins.Num()!=V.Count)return Stop(TEXT("Repair node owner/membership or owning pin inventory changed after a callback."));TSet<UEdGraphPin*> ActualPins;for(auto* P:N->Pins){if(!V.Pins.Contains(P))return Stop(TEXT("Borrowed repair pin address retired after a callback; no old pin is dereferenced."));ActualPins.Add(P);}if(ActualPins.Num()!=V.Pins.Num())return Stop(TEXT("Owning pin-address set changed after a callback; no old pin is dereferenced."));}
        return true;
    }
};
bool Index(UBlueprint* BP,FIndex& I,FResult& R)
{
    if(!MacroLibraryCurrent(BP,R))return false;
    Count(R,TEXT("get_all_graphs_calls"));BP->GetAllGraphs(I.Graphs);if(!MacroLibraryCurrent(BP,R))return false;if(I.Graphs.Num()>MaxGraphs) { return Fail(R,TEXT("graph_transfer_graph_bound"),TEXT("Graph bound exceeded before further traversal.")); }
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
    if(!MacroLibraryCurrent(BP,R))return false;
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
bool Admission(UBlueprint* BP,const FString& S,FResult& R,bool SelectKnotPlanned=false,bool OwnMacroPlanned=false)
{
    R.Stage=TEXT("text_reference_admission");R.Data->SetBoolField(TEXT("runtime_reference_admission_passed"),false);if(!Text(S,MaxText,R)) { return false; }
    TArray<TSharedPtr<FJsonValue>> Refs,Classes;int32 StockFlowNodes=0;R.Data->SetNumberField(TEXT("flat_stock_flow_node_count"),StockFlowNodes);R.Data->SetArrayField(TEXT("reference_observations"),Refs);if(!LiteralReferences(S,R,Refs)) { return false; }const TCHAR* Stream=*S;FString Line;int32 Depth=0,Objects=0;UClass* C=nullptr;
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
            Count(R,TEXT("reference_lookup_calls"));C=FindObject<UClass>(nullptr,*CP);Count(R,TEXT("reference_lookup_returns"));auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("requested_class"),CP);J->SetStringField(TEXT("returned_class"),GetPathNameSafe(C));Classes.Add(MakeShared<FJsonValueObject>(J));R.Data->SetArrayField(TEXT("node_class_observations"),Classes);const bool Supported=C&&(C->GetName()==TEXT("K2Node_InputTouch")||C->GetName()==TEXT("K2Node_InputKey")||C->GetName()==TEXT("K2Node_InputAction")||C->GetName()==TEXT("K2Node_InputAxisEvent")||C->GetName()==TEXT("K2Node_Self")||C->GetName()==TEXT("K2Node_LiteralEnum")||C->GetName()==TEXT("K2Node_CallFunction")||C->GetName()==TEXT("K2Node_VariableGet")||C->GetName()==TEXT("K2Node_VariableSet")||C==UK2Node_IfThenElse::StaticClass()||C==UK2Node_ExecutionSequence::StaticClass()||C==UK2Node_SwitchInteger::StaticClass()||C==UK2Node_SwitchName::StaticClass()||C==UK2Node_SwitchString::StaticClass()||(SelectKnotPlanned&&(C==UK2Node_Select::StaticClass()||C==UK2Node_Knot::StaticClass()))||(OwnMacroPlanned&&C==UK2Node_MacroInstance::StaticClass()));if(!Supported||!Loaded(C)||!C->HasAnyClassFlags(CLASS_Native)||!C->IsChildOf(UK2Node::StaticClass())||!ReferenceGuard(CP)||!C->GetPathName().Equals(CP,ESearchCase::CaseSensitive)) { return Fail(R,TEXT("graph_transfer_node_class_pending"),TEXT("Loaded native input/self/literal/call/variable/Branch/Sequence/guarded Switch node serializer scope unavailable; other valid native node classes remain explicit pending variants.")); }
            // These stock flow nodes have no authored class-specific source objects.
            // Installed IfThenElse AllocateDefaultPins108 supplies four stock
            // ports; ExecutionSequence AllocateDefaultPins154/Reallocate281
            // reconstructs its ordered outputs. The original importer owns
            // PostPaste/Reconstruct and current physical pins are observed only
            // after it returns. Do not freeze serialized pin order/default cache
            // or bypass the separate Select/Knot closed-data plan. AAO admits the
            // three concrete Switch classes only with the separate bounded comparator plan.
            if(C==UK2Node_IfThenElse::StaticClass()||C==UK2Node_ExecutionSequence::StaticClass())
            { ++StockFlowNodes;R.Data->SetNumberField(TEXT("flat_stock_flow_node_count"),StockFlowNodes);J->SetBoolField(TEXT("stock_flat_flow_control"),true);R.Data->SetStringField(TEXT("flat_stock_flow_scope"),TEXT("Standalone or mixed flat native Branch/Sequence, using the unchanged original generic CanImport/clear/ImportNodesFromText/GUID/structural notification/requested compile/forced save path. Serialized pin physical order/default caches are not invariants. Wider Select/Knot clusters, custom/enum Switch, macros and additional native classes remain incomplete; actual usage validation pending.")); }
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
    if(Depth!=0||Objects==0) { return Fail(R,TEXT("graph_transfer_text_structure_invalid"),TEXT("Complete nonempty node text required.")); }R.Data->SetBoolField(TEXT("runtime_reference_admission_passed"),true);R.Data->SetStringField(TEXT("reference_admission_scope"),TEXT("Loaded native K2 classes, typed object literals, serialized pin DefaultObject and reflected member/property types with exact runtime package/class hierarchy identities; simple node blocks including flat Branch/Sequence plus separately guarded integer/name/string Switch and closed no-enum Select/Knot data clusters. Broader runtime packages, soft/custom reference schemas remain pending."));return true;
}
bool Disallowed(const UEdGraphSchema* S,UEdGraphPin* A,UEdGraphPin* B,FResult& R)
{
    if(!S||!A||!B) { return false; }if(A->Direction==B->Direction) { return true; }UEdGraphPin* Out=A->Direction==EGPD_Output?A:B;UEdGraphPin* In=A->Direction==EGPD_Output?B:A;Count(R,TEXT("schema_connection_calls"));const auto Response=S->CanCreateConnection(Out,In);Count(R,TEXT("schema_connection_returns"));return Response.Response==CONNECT_RESPONSE_DISALLOW;
}
void Finish(UBlueprint* BP,bool Validate,FResult& R,const TSharedRef<FJsonObject>& Legacy,const TFunction<bool()>& GraphCurrent={})
{
    if((GraphCurrent&&!GraphCurrent())||!MacroLibraryCurrent(BP,R))return;
    Legacy->SetBoolField(TEXT("compiled"),false);Legacy->SetBoolField(TEXT("saved"),false);if(Validate) { R.Data->SetObjectField(TEXT("legacy_result"),Legacy);return; }
    if(R.bCompileRequested)
    {
        R.Stage=TEXT("compile");R.bMutationAttempted=R.bChanged=true;R.bCompileAttempted=true;Count(R,TEXT("full_compile_calls"));FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);Count(R,TEXT("full_compile_returns"));if((GraphCurrent&&!GraphCurrent())||!MacroLibraryCurrent(BP,R))return;const bool Old=BP->GeneratedClass!=nullptr;Legacy->SetBoolField(TEXT("compiled"),Old);R.Data->SetBoolField(TEXT("legacy_generated_class_compile_predicate"),Old);R.Compiled=Log.NumErrors==0&&(BP->Status==BS_UpToDate||BP->Status==BS_UpToDateWithWarnings)&&Loaded(BP->GeneratedClass);auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("error_count"),Log.NumErrors);J->SetNumberField(TEXT("warning_count"),Log.NumWarnings);J->SetStringField(TEXT("status"),LexToString(static_cast<int32>(BP->Status)));J->SetStringField(TEXT("generated_class"),GetPathNameSafe(BP->GeneratedClass));J->SetBoolField(TEXT("success"),R.Compiled.GetValue());J->SetBoolField(TEXT("legacy_success"),Old);J->SetStringField(TEXT("option"),TEXT("Original default None; implicit compiler/editor delegates are separate from direct counters."));J->SetBoolField(TEXT("requested"),true);J->SetStringField(TEXT("blueprint"),BP->GetPathName());auto OldResult=MakeShared<FJsonObject>();OldResult->SetBoolField(TEXT("requested"),true);OldResult->SetBoolField(TEXT("success"),Old);OldResult->SetStringField(TEXT("blueprint"),BP->GetPathName());OldResult->SetStringField(TEXT("generated_class"),GetPathNameSafe(BP->GeneratedClass));Legacy->SetObjectField(TEXT("compile_result"),OldResult);R.Data->SetObjectField(TEXT("compile_result"),J);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
        TSharedRef<FJsonObject> Observed=MakeShared<FJsonObject>();if(!Capture(BP,TEXT("after_compile"),R,Observed)) { R.bChangeStateKnown=false; }
    }
    if((GraphCurrent&&!GraphCurrent())||!MacroLibraryCurrent(BP,R))return;
    if(R.bSaveRequested)
    {
        R.Stage=TEXT("save");R.bMutationAttempted=R.bChanged=true;R.bSaveAttempted=true;Count(R,TEXT("forced_save_calls"));R.bSaved=UEditorAssetLibrary::SaveLoadedAsset(BP,false);Count(R,TEXT("forced_save_returns"));Legacy->SetBoolField(TEXT("saved"),R.bSaved);if((GraphCurrent&&!GraphCurrent())||!MacroLibraryCurrent(BP,R))return;R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.Data->SetBoolField(TEXT("forced_save_returned"),R.bSaved);TSharedRef<FJsonObject> Observed=MakeShared<FJsonObject>();if(!Capture(BP,TEXT("after_save"),R,Observed)) { R.bChangeStateKnown=false; }
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

// AAO: original generic graph import includes stock Switch nodes. Their
// serialized comparator is compiler input (K2Node_Switch.cpp104 checks a resolved
// UFunction), not arbitrary node metadata. Installed SwitchInteger100 performs
// StartIndex++ for each old execution output; Name/String rebuild from PinNames.
struct FStockSwitchImport
{
    struct FComparator
    {
        TStrongObjectPtr<UClass> Class;TStrongObjectPtr<UFunction> Function;TStrongObjectPtr<UObject> CDO;
        TStrongObjectPtr<UPackage> Package;FString ClassPath,FunctionPath,CDOPath;FName Name;
        EFunctionFlags Flags=FUNC_None;TArray<FProperty*> Fields;TArray<FName> FieldNames;
        TArray<const FFieldClass*> Types;TArray<EPropertyFlags> PropertyFlags;
    };
    struct FPlan
    {
        FString Name;UClass* NodeClass=nullptr;FName Function;int32 Source=INDEX_NONE,Start=0;
        bool Sensitive=false,Default=true;TArray<FName> Names;FGuid Id;int32 OldCases=0,OldPins=0;
    };
    struct FNode
    {
        TStrongObjectPtr<UK2Node_Switch> Node;int32 Plan=INDEX_NONE;
    };
    FResult& R;TArray<TUniquePtr<FComparator>> Sources;TArray<FPlan> Plans;TArray<FNode> Nodes;
    TStrongObjectPtr<UEdGraph> Graph;
    explicit FStockSwitchImport(FResult& In):R(In){}
    bool Bad(const FString& Why,const TCHAR* Code=TEXT("graph_transfer_stock_switch_pending"))
    {if(R.bMutationAttempted)R.bChangeStateKnown=false;return Fail(R,Code,Why);}
    bool Lost(const FString& Role,const FString& Expected,const FString& Actual)
    {R.Data->SetStringField(TEXT("stock_switch_first_mismatch"),Role);R.Data->SetStringField(TEXT("stock_switch_expected"),Expected);R.Data->SetStringField(TEXT("stock_switch_actual"),Actual);return Bad(TEXT("Stock Switch source/current role changed; actual import prefix retained, no following compiler/save or replay."),TEXT("graph_transfer_stock_switch_current_lost"));}
    bool SourceCurrent(FComparator& S)
    {
        auto* C=S.Class.Get();auto* F=S.Function.Get();auto* D=S.CDO.Get();auto* P=S.Package.Get();
        if(!Loaded(C)||!Loaded(F)||!Loaded(D)||!Loaded(P)||C->GetPathName()!=S.ClassPath
            ||F->GetPathName()!=S.FunctionPath||D->GetPathName()!=S.CDOPath||C->GetPackage()!=P
            ||F->GetOuter()!=C||D->GetClass()!=C||D->GetOuter()!=P||!D->HasAnyFlags(RF_ClassDefaultObject)
            ||C->GetDefaultObject(false)!=D||!C->HasAnyClassFlags(CLASS_Native)
            ||C->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists)
            ||F->FunctionFlags!=S.Flags||FindUField<UFunction>(C,S.Name)!=F)
            return Lost(TEXT("native_comparator_identity"),S.FunctionPath,GetPathNameSafe(F));
        // Compare the entire current field inventory before borrowing old fields.
        TArray<FProperty*> Current;for(TFieldIterator<FProperty> It(F,EFieldIteratorFlags::ExcludeSuper);It;++It)Current.Add(*It);
        if(Current!=S.Fields)return Lost(TEXT("comparator_field_inventory"),LexToString(S.Fields.Num()),LexToString(Current.Num()));
        for(int32 I=0;I<Current.Num();++I)if(Current[I]->GetOwner<UFunction>()!=F
            ||Current[I]->GetFName()!=S.FieldNames[I]||Current[I]->GetClass()!=S.Types[I]
            ||Current[I]->GetPropertyFlags()!=S.PropertyFlags[I]||Current[I]->ArrayDim!=1)
            return Lost(TEXT("comparator_field_role"),S.FieldNames[I].ToString(),Current[I]->GetName());
        return true;
    }
    bool BindComparator(FPlan& Plan)
    {
        UClass* C=Plan.NodeClass==UK2Node_SwitchString::StaticClass()?UKismetStringLibrary::StaticClass():UKismetMathLibrary::StaticClass();
        for(int32 I=0;I<Sources.Num();++I)if(Sources[I]->Class.Get()==C&&Sources[I]->Name==Plan.Function){Plan.Source=I;return SourceCurrent(*Sources[I]);}
        auto S=MakeUnique<FComparator>();auto* F=FindUField<UFunction>(C,Plan.Function);auto* D=C->GetDefaultObject(false);
        if(!RuntimeObject(C)||!RuntimeObject(F)||!Loaded(D)||D->GetClass()!=C||F->GetOuter()!=C
            ||!F->HasAllFunctionFlags(FUNC_Native|FUNC_Static|FUNC_BlueprintPure)||F->NumParms!=3)
            return Bad(TEXT("Existing exact native comparator, three-parameter signature and already created runtime library CDO required; no CDO creation or source loader."));
        S->Class.Reset(C);S->Function.Reset(F);S->CDO.Reset(D);S->Package.Reset(C->GetPackage());
        S->ClassPath=C->GetPathName();S->FunctionPath=F->GetPathName();S->CDOPath=D->GetPathName();S->Name=Plan.Function;S->Flags=F->FunctionFlags;
        TSet<FName> Seen;
        for(TFieldIterator<FProperty> It(F,EFieldIteratorFlags::ExcludeSuper);It;++It)
        {
            auto* P=*It;const FName N=P->GetFName();const bool Return=N==TEXT("ReturnValue");
            const bool Input=N==TEXT("A")||N==TEXT("B");
            const bool Type=Return?P->IsA<FBoolProperty>():Plan.NodeClass==UK2Node_SwitchInteger::StaticClass()?P->IsA<FIntProperty>():Plan.NodeClass==UK2Node_SwitchName::StaticClass()?P->IsA<FNameProperty>():P->IsA<FStrProperty>();
            if((!Return&&!Input)||Seen.Contains(N)||P->GetOwner<UFunction>()!=F||P->ArrayDim!=1
                ||!P->HasAnyPropertyFlags(CPF_Parm)||P->HasAnyPropertyFlags(CPF_ReturnParm)!=Return||!Type)
                return Bad(TEXT("Only the installed exact primitive A/B/ReturnValue comparator signature is admitted; complex or changed function descriptors remain incomplete."));
            Seen.Add(N);S->Fields.Add(P);S->FieldNames.Add(N);S->Types.Add(P->GetClass());S->PropertyFlags.Add(P->GetPropertyFlags());
        }
        if(Seen.Num()!=3)return Bad(TEXT("Native comparator field inventory differs from its three public parameters."));
        Plan.Source=Sources.Num();Sources.Add(MoveTemp(S));return SourceCurrent(*Sources.Last());
    }
    static bool Scalar(const FString& Raw,FString& Value)
    {
        Value=FCollapsedTextPlan::Unquote(Raw);if(Value.Len()>256||Value.Contains(TEXT("\n"))||Value.Contains(TEXT("\r")))return false;
        // Quoted values may contain commas/Unicode/spaces. Unquoted values must
        // be a single token so a second assignment cannot bypass admission.
        const FString Trimmed=Raw.TrimStartAndEnd();if(Trimmed.StartsWith(TEXT("\""))){TArray<FString> Parts;return Trimmed.EndsWith(TEXT("\""))&&FCollapsedTextPlan::Pieces(TEXT("(")+Trimmed+TEXT(")"),Parts)&&Parts.Num()==1;}
        return !Value.Contains(TEXT("\""))&&(!Value.Contains(TEXT("="))&&!Value.Contains(TEXT(","))&&!Value.Contains(TEXT("("))&&!Value.Contains(TEXT(")")));
    }
    bool Admit(const FString& Text)
    {
        const TCHAR* Stream=*Text;FString Line;FPlan P;bool Active=false;TMap<FString,FString> Fields;TMap<int32,FString> Indexed;bool Whole=false;
        auto Complete=[&]()
        {
            bool Sensitive=false,Default=true;
            if(!FGuid::Parse(Fields.FindRef(TEXT("nodeguid")),P.Id)||!P.Id.IsValid())return Bad(TEXT("Explicit valid source Switch NodeGuid required for rename-independent returned-node matching."));
            for(const auto& Old:Plans)if(Old.Id==P.Id)return Bad(TEXT("Unique source Switch NodeGuid required."));
            if(Fields.Contains(TEXT("biscasesensitive"))&&(!FCollapsedTextPlan::Boolean(Fields.FindRef(TEXT("biscasesensitive")),Sensitive)||P.NodeClass!=UK2Node_SwitchString::StaticClass()))return Bad(TEXT("String Switch case sensitivity must be an exact boolean."));
            if(Fields.Contains(TEXT("bhasdefaultpin"))&&!FCollapsedTextPlan::Boolean(Fields.FindRef(TEXT("bhasdefaultpin")),Default))return Bad(TEXT("Stock Switch default-pin toggle must be an exact boolean."));
            P.Sensitive=Sensitive;P.Default=Default;
            if(Sensitive&&!Fields.Contains(TEXT("functionname")))return Bad(TEXT("Case-sensitive string Switch requires its matching explicit serialized FunctionName; importing a bool alone does not call PostEditChangeProperty."));
            P.Function=P.NodeClass==UK2Node_SwitchInteger::StaticClass()?TEXT("NotEqual_IntInt"):P.NodeClass==UK2Node_SwitchName::StaticClass()?TEXT("NotEqual_NameName"):Sensitive?TEXT("NotEqual_StrStr"):TEXT("NotEqual_StriStri");
            FString Value;if(Fields.Contains(TEXT("functionname"))&&(!Scalar(Fields.FindRef(TEXT("functionname")),Value)||FName(*Value)!=P.Function))return Bad(TEXT("Serialized Switch FunctionName must match its installed concrete comparator and string case-sensitivity mode."));
            const FString Expected=P.NodeClass==UK2Node_SwitchString::StaticClass()?TEXT("/Script/Engine.KismetStringLibrary"):TEXT("/Script/Engine.KismetMathLibrary");
            if(Fields.Contains(TEXT("functionclass"))&&FPackageName::ExportTextPathToObjectPath(FCollapsedTextPlan::Unquote(Fields.FindRef(TEXT("functionclass"))))!=Expected)return Bad(TEXT("Serialized Switch FunctionClass must be the exact native runtime library, never an arbitrary compiler callable."));
            if(Fields.Contains(TEXT("startindex")))
            {const FString V=Fields.FindRef(TEXT("startindex"));if(P.NodeClass!=UK2Node_SwitchInteger::StaticClass()||!LexTryParseString(P.Start,*V)||LexToString(P.Start)!=V)return Bad(TEXT("Integer Switch StartIndex must be a canonical signed int32."));}
            if(P.NodeClass==UK2Node_SwitchInteger::StaticClass())
            {if(Whole||!Indexed.IsEmpty()||int64(P.Start)+P.OldCases>MAX_int32)return Bad(TEXT("Integer Switch reconstructed case counter must not overflow; name/string PinNames fields do not apply."));}
            else
            {
                TArray<FString> Values;
                if(Whole&&!FCollapsedTextPlan::Pieces(Fields.FindRef(TEXT("pinnames")),Values))return Bad(TEXT("Bounded whole PinNames tuple required."));
                if(!Indexed.IsEmpty()){if(Whole)return Bad(TEXT("Mixed whole/indexed PinNames is not admitted."));for(int32 I=0;I<Indexed.Num();++I){const auto* V=Indexed.Find(I);if(!V)return Bad(TEXT("Contiguous bounded PinNames indices required."));Values.Add(*V);}}
                if(Values.Num()>128)return Bad(TEXT("Stock Switch case inventory exceeds the existing flat-node pin budget."));
                TSet<FName> Seen;for(const FString& Raw:Values)
                {if(!Scalar(Raw,Value))return Bad(TEXT("Single bounded serialized Switch case name required."));const FName N(*Value);if(N.IsNone()||N==TEXT("execute")||N==TEXT("Selection")||N==TEXT("Default")||N==P.Function||Seen.Contains(N))return Bad(TEXT("Unique nonreserved Switch case names required; ambiguous function/selection/default collisions remain incomplete."));Seen.Add(N);P.Names.Add(N);}
            }
            if(!BindComparator(P))return false;Plans.Add(P);return true;
        };
        while(FParse::Line(&Stream,Line))
        {
            const FString T=Line.TrimStartAndEnd();
            if(T.StartsWith(TEXT("Begin Object"),ESearchCase::IgnoreCase))
            {
                if(Active)return Bad(TEXT("Nested Switch objects remain incomplete."));FString C;FParse::Value(*T,TEXT("Class="),C);
                P=FPlan();P.NodeClass=C==TEXT("/Script/BlueprintGraph.K2Node_SwitchInteger")?UK2Node_SwitchInteger::StaticClass():C==TEXT("/Script/BlueprintGraph.K2Node_SwitchName")?UK2Node_SwitchName::StaticClass():C==TEXT("/Script/BlueprintGraph.K2Node_SwitchString")?UK2Node_SwitchString::StaticClass():nullptr;
                Active=P.NodeClass!=nullptr;Fields.Empty();Indexed.Empty();Whole=false;if(Active&&(!FParse::Value(*T,TEXT("Name="),P.Name)||P.Name.IsEmpty()))return Bad(TEXT("Explicit Switch node object name required."));continue;
            }
            if(!Active)continue;
            if(T.Equals(TEXT("End Object"),ESearchCase::IgnoreCase)){if(!Complete())return false;Active=false;continue;}
            if(T.StartsWith(TEXT("CustomProperties Pin "),ESearchCase::IgnoreCase))
            {
                if(++P.OldPins>128)return Bad(TEXT("Bounded stock Switch source pin inventory required."));TMap<FString,FString> Pin;
                if(!FCollapsedTextPlan::Map(T.Mid(21),Pin))return Bad(TEXT("Unambiguous serialized Switch pin tuple required."));
                const FString Direction=FCollapsedTextPlan::Unquote(Pin.FindRef(TEXT("Direction")));
                if(Direction==TEXT("EGPD_Output")&&FCollapsedTextPlan::Unquote(Pin.FindRef(TEXT("PinType.PinCategory")))==TEXT("exec")&&FName(*FCollapsedTextPlan::Unquote(Pin.FindRef(TEXT("PinName"))))!=TEXT("Default"))++P.OldCases;
                continue;
            }
            FString K,V;if(!T.Split(TEXT("="),&K,&V))continue;K=K.TrimStartAndEnd().ToLower();V.TrimStartAndEndInline();
            if(K.StartsWith(TEXT("pinnames(")))
            {int32 I;const FString Digits=K.Mid(9,K.Len()-10);if(!K.EndsWith(TEXT(")"))||!LexTryParseString(I,*Digits)||I<0||I>=128||LexToString(I)!=Digits||Indexed.Contains(I))return Bad(TEXT("Bounded unique canonical PinNames index required before engine deserialization."));Indexed.Add(I,V);continue;}
            if(K==TEXT("functionname")||K==TEXT("functionclass")||K==TEXT("biscasesensitive")||K==TEXT("bhasdefaultpin")||K==TEXT("startindex")||K==TEXT("pinnames")||K==TEXT("nodeguid"))
            {if(Fields.Contains(K))return Bad(TEXT("Duplicate critical Switch field is ambiguous."));Fields.Add(K,V);if(K==TEXT("pinnames"))Whole=true;continue;}
            // Qualified/array mutation spellings of these scalar fields must
            // not slip past the same admission used for their canonical forms.
            for(const TCHAR* Critical:{TEXT("functionname"),TEXT("functionclass"),TEXT("biscasesensitive"),TEXT("bhasdefaultpin"),TEXT("startindex"),TEXT("pinnames")})if(K.StartsWith(Critical))return Bad(TEXT("Alternate critical Switch property mutation syntax remains incomplete."));
        }
        if(Active)return Bad(TEXT("Unclosed Switch object block."));
        R.Data->SetNumberField(TEXT("flat_stock_switch_node_count"),Plans.Num());
        if(!Plans.IsEmpty())R.Data->SetStringField(TEXT("flat_stock_switch_scope"),TEXT("Flat stock SwitchInteger/Name/String with exact loaded native comparators, existing CDO identity, bounded cases and integer reconstruction counter. Original paste/GUID/structural notification/compiler/forced-save flow retained. Pin cache/order reconstruction is allowed. Wider custom/enum Switch, ambiguous names and larger inputs remain incomplete; actual paste/connection/save usage validation pending."));
        return Current();
    }
    bool Current()
    {
        for(const auto& S:Sources)if(!SourceCurrent(*S))return false;
        if(Nodes.IsEmpty())return true;auto* G=Graph.Get();if(!Loaded(G))return Lost(TEXT("target_graph"),TEXT("loaded selected graph"),GetPathNameSafe(G));
        for(const auto& Held:Nodes)
        {
            auto* N=Held.Node.Get();const auto& P=Plans[Held.Plan];const auto& S=*Sources[P.Source];
            if(!Loaded(N)||N->GetOuter()!=G||!G->Nodes.Contains(N)||N->GetClass()!=P.NodeClass)return Lost(TEXT("imported_switch_owner"),P.Name,GetPathNameSafe(N));
            if(N->FunctionName!=P.Function||!N->FunctionClass.GetGCPtr().IsResolved()||N->FunctionClass.GetGCPtr()!=S.Class.Get()||bool(N->bHasDefaultPin)!=P.Default)return Lost(TEXT("imported_switch_comparator"),P.Function.ToString(),N->FunctionName.ToString());
            if(auto* I=Cast<UK2Node_SwitchInteger>(N)){if(I->StartIndex!=P.Start)return Lost(TEXT("switch_start_index"),LexToString(P.Start),LexToString(I->StartIndex));}
            if(auto* Name=Cast<UK2Node_SwitchName>(N)){if(Name->PinNames!=P.Names)return Lost(TEXT("switch_case_names"),TEXT("admitted authored names"),TEXT("changed"));}
            if(auto* String=Cast<UK2Node_SwitchString>(N)){if(String->PinNames!=P.Names||bool(String->bIsCaseSensitive)!=P.Sensitive)return Lost(TEXT("switch_string_cases"),TEXT("admitted authored names/case sensitivity"),TEXT("changed"));}
            auto* OwnerBP=Cast<UBlueprint>(G->GetOuter());
            if(!Loaded(OwnerBP)||!OwnerBP->SkeletonGeneratedClass.GetGCPtr().IsResolved())return Lost(TEXT("switch_target_skeleton"),TEXT("resolved current owner skeleton slot"),TEXT("unresolved"));
            auto* Skeleton=OwnerBP->SkeletonGeneratedClass.Get();const bool OwnComparator=Skeleton&&Loaded(Skeleton)&&Skeleton->IsChildOf(S.Class.Get());
            UObject* ExpectedDefault=OwnComparator?nullptr:S.CDO.Get();
            UEdGraphPin* FunctionPin=nullptr;int32 Found=0;
            for(auto* Pin:N->Pins)if(Pin&&Pin->PinName==P.Function){FunctionPin=Pin;++Found;}
            if(Found!=1||!FunctionPin||FunctionPin->GetOwningNode()!=N||FunctionPin->Direction!=EGPD_Input
                ||FunctionPin->PinType.PinCategory!=UEdGraphSchema_K2::PC_Object||!FunctionPin->PinType.PinSubCategoryObject.IsValid()
                ||FunctionPin->PinType.PinSubCategoryObject.Get()!=S.Class.Get()||FunctionPin->DefaultObject!=ExpectedDefault
                ||!FunctionPin->bHidden||!FunctionPin->bNotConnectable||!FunctionPin->bDefaultValueIsReadOnly||!FunctionPin->LinkedTo.IsEmpty())
                return Lost(TEXT("switch_compiler_function_pin"),S.FunctionPath,TEXT("missing, ambiguous or altered current hidden comparator pin"));
        }
        return true;
    }
    bool Adopt(UEdGraph* G,const TSet<UEdGraphNode*>& Imported)
    {
        if(Plans.IsEmpty())return true;Graph.Reset(G);TSet<int32> Seen;
        for(auto* N:Imported)
        {
            if(!Loaded(N)||N->GetOuter()!=G||!G->Nodes.Contains(N))return Lost(TEXT("import_returned_node"),TEXT("current selected graph member"),GetPathNameSafe(N));
            if(N->GetClass()!=UK2Node_SwitchInteger::StaticClass()&&N->GetClass()!=UK2Node_SwitchName::StaticClass()&&N->GetClass()!=UK2Node_SwitchString::StaticClass())continue;
            const int32 I=Plans.IndexOfByPredicate([&](const FPlan& P){return N->NodeGuid==P.Id&&N->GetClass()==P.NodeClass;});
            if(I==INDEX_NONE||Seen.Contains(I))return Lost(TEXT("imported_switch_plan"),TEXT("one returned node per source GUID/class"),N->GetName());
            Seen.Add(I);FNode H;H.Node.Reset(CastChecked<UK2Node_Switch>(N));H.Plan=I;Nodes.Add(MoveTemp(H));
        }
        if(Seen.Num()!=Plans.Num())return Lost(TEXT("imported_switch_count"),LexToString(Plans.Num()),LexToString(Seen.Num()));
        return Current();
    }
};
// ABB: the original generic clipboard importer also accepts Select and reroute.
// These are callback-bearing nodes: Knot indexes Pins[0/1], Select consults its
// authored option count/index/enum before rebuilding pins. Plan the finite closed
// source ports before calling the SDK; reacquire logical ports after SDK return.
struct FSelectKnotImport
{
    struct FPinPlan
    {
        FGuid Id;FName Name;EEdGraphPinDirection Direction=EGPD_Input;FEdGraphPinType Type,Expected;
        FString Raw;TArray<FString> Links;
    };
    struct FPlan
    {
        FString Name;FGuid Id;bool Select=false;int32 Options=2;FEdGraphPinType Index;
        TArray<FPinPlan> Pins;TMap<FString,FString> Fields;
    };
    struct FHeld{TStrongObjectPtr<UEdGraphNode> Node;int32 Plan=INDEX_NONE;};
    FResult& R;bool Active=false;TArray<FPlan> Plans;TArray<FHeld> Nodes;TStrongObjectPtr<UEdGraph> Graph;
    TStrongObjectPtr<UClass> SelectClass,KnotClass;TArray<FProperty*> SelectFields;TArray<FName> FieldNames;TArray<FFieldClass*> FieldClasses;TArray<EPropertyFlags> FieldFlags;TArray<int32> FieldDims,FieldSizes,FieldOffsets;
    FProperty* EntryInner=nullptr;int32 EntryInnerSize=0;EPropertyFlags EntryInnerFlags=CPF_None;
    FIntProperty* CountProperty=nullptr;FStructProperty* IndexProperty=nullptr;FObjectProperty* EnumProperty=nullptr;FArrayProperty* EntriesProperty=nullptr;
    explicit FSelectKnotImport(FResult& In):R(In){}
    bool Bad(const FString& Why,bool Current=false)
    {if(R.bMutationAttempted)R.bChangeStateKnown=false;return Fail(R,Current?TEXT("graph_transfer_select_knot_current_lost"):TEXT("graph_transfer_select_knot_pending"),Why);}
    static FString U(const FString& V){return FCollapsedTextPlan::Unquote(V);}
    static bool Type(const TMap<FString,FString>& M,FEdGraphPinType& Out,const FString& Prefix)
    {
        const FString C=U(M.FindRef(Prefix+TEXT("PinCategory"))),Sub=U(M.FindRef(Prefix+TEXT("PinSubCategory")));
        if(C!=TEXT("wildcard")&&C!=TEXT("bool")&&C!=TEXT("int")&&C!=TEXT("int64")&&C!=TEXT("real")&&C!=TEXT("string")&&C!=TEXT("name")&&C!=TEXT("text"))return false;
        if((C==TEXT("real")&&(Sub!=TEXT("float")&&Sub!=TEXT("double")))||(C!=TEXT("real")&&!Sub.IsEmpty()))return false;
        for(const auto& P:M)if(P.Key.StartsWith(Prefix))
        {
            const FString K=P.Key.Mid(Prefix.Len()),V=U(P.Value);
            if(K==TEXT("PinCategory")||K==TEXT("PinSubCategory"))continue;
            if(K==TEXT("PinSubCategoryObject")&&(V==TEXT("None")||V.IsEmpty()))continue;
            if(K==TEXT("ContainerType")&&V==TEXT("None"))continue;
            if(K==TEXT("PinValueType")&&V==TEXT("()"))continue;
            if((K==TEXT("bIsReference")||K==TEXT("bIsConst")||K==TEXT("bIsWeakPointer")||K==TEXT("bIsUObjectWrapper")||K==TEXT("bSerializeAsSinglePrecisionFloat"))&&V==TEXT("False"))continue;
            return false;
        }
        Out.PinCategory=FName(*C);Out.PinSubCategory=FName(*Sub);return true;
    }
    bool Pin(const FString& Text,FPlan& O)
    {
        TMap<FString,FString> M;if(O.Pins.Num()>=34||!FCollapsedTextPlan::Map(Text,M))return Bad(TEXT("Select/Knot needs bounded unambiguous serialized pin tuples."));
        FPinPlan P;P.Name=FName(*U(M.FindRef(TEXT("PinName"))));const FString D=U(M.FindRef(TEXT("Direction")));
        if(P.Name.IsNone()||!FGuid::Parse(M.FindRef(TEXT("PinId")),P.Id)||!P.Id.IsValid()||(!D.IsEmpty()&&D!=TEXT("EGPD_Input")&&D!=TEXT("EGPD_Output"))||!Type(M,P.Type,TEXT("PinType.")))return Bad(TEXT("Exact primitive/wildcard non-container data pin/name/GUID/direction required."));
        P.Direction=D==TEXT("EGPD_Output")?EGPD_Output:EGPD_Input;P.Expected=P.Type;P.Raw=U(M.FindRef(TEXT("DefaultValue")));
        if(P.Raw.Len()>4096)return Bad(TEXT("Bounded primitive serialized defaults required."));
        for(const auto& V:M)
        {
            if(V.Key.StartsWith(TEXT("PinType."))||V.Key==TEXT("PinId")||V.Key==TEXT("PinName")||V.Key==TEXT("Direction")||V.Key==TEXT("DefaultValue")||V.Key==TEXT("AutogeneratedDefaultValue")||V.Key==TEXT("PinFriendlyName")||V.Key==TEXT("PinToolTip"))continue;
            if(V.Key==TEXT("LinkedTo"))
            {if(!FCollapsedTextPlan::Pieces(V.Value,P.Links)||P.Links.Num()>64||(P.Direction==EGPD_Input&&P.Links.Num()>1))return Bad(TEXT("Closed scalar clipboard links with at most one input source required."));TSet<FString> Seen;for(const auto& L:P.Links){if(Seen.Contains(L))return Bad(TEXT("Duplicate source link."));Seen.Add(L);}continue;}
            if(V.Key==TEXT("PersistentGuid")){FGuid Id;if(!FGuid::Parse(V.Value,Id)||Id.IsValid())return Bad(TEXT("Persistent user-defined pin variants remain incomplete."));continue;}
            const FString X=U(V.Value);
            if((V.Key==TEXT("DefaultObject")&&(X.IsEmpty()||X==TEXT("None")))||(V.Key==TEXT("DefaultTextValue")&&X.IsEmpty()))continue;
            if((V.Key==TEXT("bHidden")||V.Key==TEXT("bNotConnectable")||V.Key==TEXT("bDefaultValueIsReadOnly")||V.Key==TEXT("bOrphanedPin")||V.Key==TEXT("bAdvancedView"))&&X==TEXT("False"))continue;
            if((V.Key==TEXT("bDisplayAsMutableRef")||V.Key==TEXT("bDefaultValueIsIgnored"))&&(X==TEXT("True")||X==TEXT("False")))continue;
            return Bad(TEXT("Other serialized pin ownership/subpins/default objects/flags remain incomplete."));
        }
        for(const auto& Old:O.Pins)if(Old.Name==P.Name||Old.Id==P.Id)return Bad(TEXT("Unique logical source ports and source pin IDs required."));
        O.Pins.Add(MoveTemp(P));return true;
    }
    bool Complete(FPlan& O)
    {
        if(!FGuid::Parse(O.Fields.FindRef(TEXT("nodeguid")),O.Id)||!O.Id.IsValid())return Bad(TEXT("Valid unique source NodeGuid required for initial SDK-return adoption."));
        for(const auto& Old:Plans)if(Old.Id==O.Id||Old.Name==O.Name)return Bad(TEXT("Unique source node object names and GUIDs required."));
        if(!O.Select)
        {
            if(O.Pins.Num()!=2||O.Pins[0].Name!=TEXT("InputPin")||O.Pins[0].Direction!=EGPD_Input||O.Pins[1].Name!=TEXT("OutputPin")||O.Pins[1].Direction!=EGPD_Output)return Bad(TEXT("Knot SDK requires exactly InputPin then OutputPin before its indexing callbacks."));
            for(const auto& F:O.Fields)if(F.Key!=TEXT("nodeguid")&&F.Key!=TEXT("nodeposx")&&F.Key!=TEXT("nodeposy")&&F.Key!=TEXT("nodecomment")&&F.Key!=TEXT("bcommentbubblevisible")&&F.Key!=TEXT("bcommentbubblepinned")&&F.Key!=TEXT("enabledstate")&&F.Key!=TEXT("advancedpindisplay"))return Bad(TEXT("Other reroute authored fields remain incomplete."));
        }
        else
        {
            if(O.Fields.Contains(TEXT("numoptionpins"))){const FString V=O.Fields.FindRef(TEXT("numoptionpins"));if(!LexTryParseString(O.Options,*V)||LexToString(O.Options)!=V)return Bad(TEXT("Canonical Select option count required."));}
            if(O.Options<2||O.Options>32||O.Pins.Num()!=O.Options+2)return Bad(TEXT("Select options2..32 with exact Index/ReturnValue/Option N pin shape required."));
            if((O.Fields.Contains(TEXT("enum"))&&O.Fields.FindRef(TEXT("enum"))!=TEXT("None"))||(O.Fields.Contains(TEXT("enumentries"))&&O.Fields.FindRef(TEXT("enumentries"))!=TEXT("()")))return Bad(TEXT("Enum Select remains an explicit valid incomplete source variant."));
            auto* Index=O.Pins.FindByPredicate([](const FPinPlan& P){return P.Name==TEXT("Index");});auto* Return=O.Pins.FindByPredicate([](const FPinPlan& P){return P.Name==UEdGraphSchema_K2::PN_ReturnValue;});
            if(!Index||!Return||Index->Direction!=EGPD_Input||Return->Direction!=EGPD_Output||(Index->Type.PinCategory!=UEdGraphSchema_K2::PC_Boolean&&Index->Type.PinCategory!=UEdGraphSchema_K2::PC_Int)||Return->Type.PinCategory==UEdGraphSchema_K2::PC_Wildcard||(Index->Type.PinCategory==UEdGraphSchema_K2::PC_Boolean&&O.Options!=2))return Bad(TEXT("Select needs bool/int Index and an already typed primitive ReturnValue; wildcard inference/enum index remain incomplete."));
            O.Index=Index->Type;
            if(O.Fields.Contains(TEXT("indexpintype"))){TMap<FString,FString> M;FEdGraphPinType T;if(!FCollapsedTextPlan::Map(O.Fields.FindRef(TEXT("indexpintype")),M)||!Type(M,T,TEXT(""))||T!=O.Index)return Bad(TEXT("Authored IndexPinType must match its actual serialized bool/int Index port."));}
            for(int32 I=0;I<O.Options;++I){const FName Name(*FString::Printf(TEXT("Option %d"),I));auto* P=O.Pins.FindByPredicate([&](const FPinPlan& V){return V.Name==Name;});if(!P||P->Direction!=EGPD_Input||P->Type!=Return->Type)return Bad(TEXT("All Select Option N ports need the same complete primitive value type as ReturnValue."));}
            for(const auto& F:O.Fields)if(F.Key!=TEXT("nodeguid")&&F.Key!=TEXT("numoptionpins")&&F.Key!=TEXT("indexpintype")&&F.Key!=TEXT("enum")&&F.Key!=TEXT("enumentries")&&F.Key!=TEXT("nodeposx")&&F.Key!=TEXT("nodeposy")&&F.Key!=TEXT("nodecomment")&&F.Key!=TEXT("bcommentbubblevisible")&&F.Key!=TEXT("bcommentbubblepinned")&&F.Key!=TEXT("enabledstate")&&F.Key!=TEXT("advancedpindisplay"))return Bad(TEXT("Select transient reconstruction caches and alternate/unknown authored fields are not import inputs in this slice."));
        }
        Plans.Add(MoveTemp(O));return true;
    }
    // These stock logical roles are ASCII; FName matching is case-insensitive,
    // and reconstruction can restore canonical display spelling.
    static FString Key(int32 Node,FName Pin){return LexToString(Node)+TEXT("/")+Pin.ToString().ToLower();}
    static bool SameType(const FEdGraphPinType& A,const FEdGraphPinType& B)
    {return A==B&&A.bIsUObjectWrapper==B.bIsUObjectWrapper&&A.bSerializeAsSinglePrecisionFloat==B.bSerializeAsSinglePrecisionFloat&&A.PinValueType.bTerminalIsUObjectWrapper==B.PinValueType.bTerminalIsUObjectWrapper;}
    bool BindSource()
    {
        SelectClass.Reset(UK2Node_Select::StaticClass());KnotClass.Reset(UK2Node_Knot::StaticClass());
        for(TFieldIterator<FProperty> It(SelectClass.Get(),EFieldIteratorFlags::ExcludeSuper);It;++It){SelectFields.Add(*It);FieldNames.Add(It->GetFName());FieldClasses.Add(It->GetClass());FieldFlags.Add(It->GetPropertyFlags());FieldDims.Add(It->ArrayDim);FieldSizes.Add(It->GetSize());FieldOffsets.Add(It->GetOffset_ForInternal());}
        CountProperty=FindFProperty<FIntProperty>(SelectClass.Get(),TEXT("NumOptionPins"));IndexProperty=FindFProperty<FStructProperty>(SelectClass.Get(),TEXT("IndexPinType"));EnumProperty=FindFProperty<FObjectProperty>(SelectClass.Get(),TEXT("Enum"));EntriesProperty=FindFProperty<FArrayProperty>(SelectClass.Get(),TEXT("EnumEntries"));
        if(!CountProperty||!IndexProperty||IndexProperty->Struct!=FEdGraphPinType::StaticStruct()||!EnumProperty||!EntriesProperty||!EntriesProperty->Inner||EntriesProperty->Inner->GetClass()!=FNameProperty::StaticClass()||EntriesProperty->Inner->GetOwner<FProperty>()!=EntriesProperty||EntriesProperty->Inner->ArrayDim!=1)return Bad(TEXT("Installed Select authored field descriptors unavailable."));EntryInner=EntriesProperty->Inner;EntryInnerSize=EntryInner->GetSize();EntryInnerFlags=EntryInner->GetPropertyFlags();return Current();
    }
    bool Admit(const FString& Text)
    {
        const TCHAR* Probe=*Text;FString ProbeLine;while(FParse::Line(&Probe,ProbeLine)){const FString T=ProbeLine.TrimStartAndEnd();FString C;if(T.StartsWith(TEXT("Begin Object"),ESearchCase::IgnoreCase)&&FParse::Value(*T,TEXT("Class="),C)&&(C==TEXT("/Script/BlueprintGraph.K2Node_Select")||C==TEXT("/Script/BlueprintGraph.K2Node_Knot"))){Active=true;break;}}if(!Active)return true;
        const TCHAR* Stream=*Text;FString Line;bool Open=false;FPlan ParsedRow;
        while(FParse::Line(&Stream,Line))
        {
            const FString T=Line.TrimStartAndEnd();if(T.IsEmpty())continue;
            if(T.StartsWith(TEXT("Begin Object"),ESearchCase::IgnoreCase))
            {FString C,N;if(Open||Plans.Num()>=64||!FParse::Value(*T,TEXT("Class="),C)||!FParse::Value(*T,TEXT("Name="),N)||!FCollapsedTextPlan::Name(N)||(C!=TEXT("/Script/BlueprintGraph.K2Node_Select")&&C!=TEXT("/Script/BlueprintGraph.K2Node_Knot")))return Bad(TEXT("Closed flat Select/Knot-only data clusters up to64 nodes; mixed other callbacks/nested/enum/container source variants remain incomplete."));ParsedRow=FPlan();ParsedRow.Name=N;ParsedRow.Select=C==TEXT("/Script/BlueprintGraph.K2Node_Select");Open=true;continue;}
            if(T.Equals(TEXT("End Object"),ESearchCase::IgnoreCase)){if(!Open||!Complete(ParsedRow))return false;Open=false;continue;}
            if(!Open)return Bad(TEXT("Complete Select/Knot object blocks required."));
            if(T.StartsWith(TEXT("CustomProperties Pin "),ESearchCase::IgnoreCase)){if(!Pin(T.Mid(21),ParsedRow))return false;continue;}
            FString K,V;if(!T.Split(TEXT("="),&K,&V))return Bad(TEXT("Ordinary bounded clipboard assignments required."));K=K.TrimStartAndEnd().ToLower();V.TrimStartAndEndInline();if(ParsedRow.Fields.Contains(K))return Bad(TEXT("Duplicate Select/Knot authored assignment."));ParsedRow.Fields.Add(K,V);
        }
        if(Open||Plans.IsEmpty())return Bad(TEXT("Nonempty complete Select/Knot clipboard required."));
        // Resolve every serialized edge before SDK callbacks. Logical components
        // join both Knot ports, and all Select value ports, but not its Index.
        TMap<FString,int32> Port;TArray<TPair<int32,int32>> Locations;TArray<TArray<int32>> Adj;
        for(int32 I=0;I<Plans.Num();++I)for(int32 J=0;J<Plans[I].Pins.Num();++J){Port.Add(Key(I,Plans[I].Pins[J].Name),Locations.Num());Locations.Emplace(I,J);Adj.Emplace();}
        auto Join=[&](int32 A,int32 B){Adj[A].AddUnique(B);Adj[B].AddUnique(A);};
        for(int32 I=0;I<Plans.Num();++I)
        {
            const auto& O=Plans[I];int32 First=INDEX_NONE;
            for(int32 J=0;J<O.Pins.Num();++J){const auto& P=O.Pins[J];const int32 A=Port.FindChecked(Key(I,P.Name));if(!O.Select||P.Name!=TEXT("Index")){if(First!=INDEX_NONE)Join(First,A);else First=A;}
                for(const auto& L:P.Links){FString N,G;if(!L.Split(TEXT(" "),&N,&G))return Bad(TEXT("Closed source node-name/pin-GUID link required."));G.TrimStartAndEndInline();FGuid Id;if(!FGuid::Parse(G,Id)||!Id.IsValid())return Bad(TEXT("Valid peer GUID required."));const int32 K=Plans.IndexOfByPredicate([&](const FPlan& V){return V.Name==N;});if(K==INDEX_NONE||K==I)return Bad(TEXT("External and same-node clipboard links remain incomplete."));const auto* Q=Plans[K].Pins.FindByPredicate([&](const FPinPlan& V){return V.Id==Id;});if(!Q||Q->Direction==P.Direction||!Q->Links.Contains(O.Name+TEXT(" ")+P.Id.ToString(EGuidFormats::Digits)))return Bad(TEXT("Every clipboard edge needs one opposite-direction reciprocal peer with canonical source GUID."));Join(A,Port.FindChecked(Key(K,Q->Name)));}
            }
        }
        TSet<int32> Visited;for(int32 Start=0;Start<Locations.Num();++Start)
        {
            if(Visited.Contains(Start))continue;TArray<int32> Work{Start},Group;FEdGraphPinType Anchor;bool Anchored=false,Linked=false;
            while(!Work.IsEmpty()){const int32 A=Work.Pop(EAllowShrinking::No);if(Visited.Contains(A))continue;Visited.Add(A);Group.Add(A);const auto L=Locations[A];const auto& O=Plans[L.Key];const auto& P=O.Pins[L.Value];Linked|=!P.Links.IsEmpty();if(O.Select){if(Anchored&&Anchor!=P.Type)return Bad(TEXT("Incompatible primitive Select anchors in a reroute component."));Anchor=P.Type;Anchored=true;}for(int32 B:Adj[A])if(!Visited.Contains(B))Work.Add(B);}
            if(!Anchored){if(Linked)return Bad(TEXT("Linked Knot-only components without typed Select anchors remain incomplete; unlinked reroutes reset to wildcard by SDK."));Anchor.PinCategory=UEdGraphSchema_K2::PC_Wildcard;}
            for(int32 A:Group){const auto L=Locations[A];auto& P=Plans[L.Key].Pins[L.Value];if(Linked&&P.Type.PinCategory!=UEdGraphSchema_K2::PC_Wildcard&&P.Type!=Anchor)return Bad(TEXT("Serialized reroute concrete type disagrees with its anchored primitive component."));P.Expected=Anchor;}
        }
        R.Data->SetNumberField(TEXT("select_knot_planned_node_count"),Plans.Num());R.Data->SetStringField(TEXT("select_knot_scope"),TEXT("Closed flat Select(no enum,2..32 same primitive options,bool/int Index)/Knot clusters; unlinked Knot resets to wildcard and linked reroutes adopt typed Select components. Source input/output shape and reciprocal links are checked before SDK. Logical ports/types/authored defaults are reacquired after paste; reconstruction cache/physical pin identity/order is not frozen. Wider enum/container/reference/mixed producer/linked knot-only variants remain incomplete; actual paste/compile/save usage validation pending."));return BindSource();
    }
    bool Current()
    {
        if(!Active)return true;if(!Loaded(SelectClass.Get())||!Loaded(KnotClass.Get())||SelectClass.Get()!=UK2Node_Select::StaticClass()||KnotClass.Get()!=UK2Node_Knot::StaticClass())return Bad(TEXT("Held native Select/Knot class role changed."),true);
        TArray<FProperty*> Now;for(TFieldIterator<FProperty> It(SelectClass.Get(),EFieldIteratorFlags::ExcludeSuper);It;++It)Now.Add(*It);if(Now!=SelectFields)return Bad(TEXT("Select authored field inventory changed before borrowing captured descriptors."),true);
        for(int32 I=0;I<Now.Num();++I)if(Now[I]->GetOwnerStruct()!=SelectClass.Get()||Now[I]->GetFName()!=FieldNames[I]||Now[I]->GetClass()!=FieldClasses[I]||Now[I]->GetPropertyFlags()!=FieldFlags[I]||Now[I]->ArrayDim!=FieldDims[I]||Now[I]->GetSize()!=FieldSizes[I]||Now[I]->GetOffset_ForInternal()!=FieldOffsets[I]||Now[I]->HasSetterOrGetter())return Bad(TEXT("Select current field role changed."),true);
        if(!IndexProperty||IndexProperty->Struct!=FEdGraphPinType::StaticStruct()||!EntriesProperty||EntriesProperty->Inner!=EntryInner)return Bad(TEXT("Select current index/enum entry descriptor changed."),true);
        if(!EntryInner||EntryInner->GetClass()!=FNameProperty::StaticClass()||EntryInner->GetOwner<FProperty>()!=EntriesProperty||EntryInner->GetOwnerStruct()!=SelectClass.Get()||EntryInner->ArrayDim!=1||EntryInner->GetSize()!=EntryInnerSize||EntryInner->GetPropertyFlags()!=EntryInnerFlags||EntryInner->HasSetterOrGetter())return Bad(TEXT("Select current enum entry Inner role changed."),true);
        if(Nodes.IsEmpty())return true;if(!Loaded(Graph.Get()))return Bad(TEXT("Imported cluster graph role lost."),true);
        TMap<FString,UEdGraphPin*> Actual;
        for(const auto& H:Nodes)
        {
            auto* N=H.Node.Get();const auto& O=Plans[H.Plan];if(!Loaded(N)||N->GetOuter()!=Graph.Get()||!Graph->Nodes.Contains(N)||N->GetClass()!=(O.Select?SelectClass.Get():KnotClass.Get())||N->Pins.Num()!=O.Pins.Num())return Bad(TEXT("Imported cluster node owner/class/current pin inventory changed."),true);
            if(!O.Select&&(!N->Pins[0]||!N->Pins[1]||N->Pins[0]->WasTrashed()||N->Pins[1]->WasTrashed()||N->Pins[0]->PinName!=TEXT("InputPin")||N->Pins[1]->PinName!=TEXT("OutputPin")))return Bad(TEXT("Current Knot SDK port-index roles changed."),true);
            if(O.Select){const auto EnumSlot=EnumProperty->GetObjectPtrPropertyValue(EnumProperty->ContainerPtrToValuePtr<void>(N));if(!EnumSlot.IsResolved()||EnumSlot.Get()!=nullptr||CountProperty->GetPropertyValue_InContainer(N)!=O.Options||!SameType(*IndexProperty->ContainerPtrToValuePtr<FEdGraphPinType>(N),O.Index)||FScriptArrayHelper(EntriesProperty,EntriesProperty->ContainerPtrToValuePtr<void>(N)).Num()!=0)return Bad(TEXT("Imported Select authored count/index/enum differs from admitted source."),true);}
            TSet<FGuid> Ids;for(auto* P:N->Pins){if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||Ids.Contains(P->PinId)||P->ParentPin||!P->SubPins.IsEmpty()||P->bHidden||P->bOrphanedPin||P->bNotConnectable||P->DefaultObject||!P->DefaultTextValue.IsEmpty())return Bad(TEXT("Imported cluster current pin identity/default/ownership role lost."),true);Ids.Add(P->PinId);const auto* E=O.Pins.FindByPredicate([&](const FPinPlan& V){return V.Name==P->PinName;});if(!E||Actual.Contains(Key(H.Plan,P->PinName))||P->Direction!=E->Direction||!SameType(P->PinType,E->Expected)||(O.Select&&P->Direction==EGPD_Input&&!E->Raw.IsEmpty()&&P->DefaultValue!=E->Raw))return Bad(TEXT("Imported logical port direction/type/authored default differs; actual SDK prefix retained."),true);Actual.Add(Key(H.Plan,P->PinName),P);}
        }
        // Compare current logical connectivity, never the pre-paste pin address,
        // pin GUID, object name or transient reconstruction metadata.
        for(const auto& H:Nodes)for(const auto& E:Plans[H.Plan].Pins){auto* P=Actual.FindRef(Key(H.Plan,E.Name));if(!P||P->LinkedTo.Num()!=E.Links.Num())return Bad(TEXT("Imported cluster link count changed."),true);for(const auto& L:E.Links){FString N,G;L.Split(TEXT(" "),&N,&G);G.TrimStartAndEndInline();FGuid Id;FGuid::Parse(G,Id);const int32 I=Plans.IndexOfByPredicate([&](const FPlan& V){return V.Name==N;});const auto* Q=I==INDEX_NONE?nullptr:Plans[I].Pins.FindByPredicate([&](const FPinPlan& V){return V.Id==Id;});auto* Peer=Q?Actual.FindRef(Key(I,Q->Name)):nullptr;if(!Peer||!P->LinkedTo.Contains(Peer)||!Peer->LinkedTo.Contains(P))return Bad(TEXT("Imported cluster logical reciprocal link differs."),true);}}
        return true;
    }
    bool Adopt(UEdGraph* G,const TSet<UEdGraphNode*>& Imported)
    {
        if(!Active)return true;Graph.Reset(G);TSet<int32> Seen;for(auto* N:Imported){if(!Loaded(N)||N->GetOuter()!=G||!G->Nodes.Contains(N))return Bad(TEXT("Returned cluster node has no current target owner."),true);const int32 I=Plans.IndexOfByPredicate([&](const FPlan& O){return O.Id==N->NodeGuid&&N->GetClass()==(O.Select?SelectClass.Get():KnotClass.Get());});if(I==INDEX_NONE||Seen.Contains(I))return Bad(TEXT("Expected exactly one returned cluster node per source class/GUID."),true);Seen.Add(I);FHeld H;H.Node.Reset(N);H.Plan=I;Nodes.Add(MoveTemp(H));}if(Seen.Num()!=Plans.Num())return Bad(TEXT("Imported cluster count does not cover source nodes."),true);return Current();
    }
};

// ACN: original Part3:489 macro-node paste, not MacroLibrary container admission.
// EdGraphUtilities.cpp232/484 owns PostPaste/Reconstruct and global pin fixup.
// MacroInstance.cpp72/109/514 owns local macro references, port allocation and
// paste eligibility. Its resolved wildcard type and physical pins are caches.
struct FOwnMacroInstanceImport
{
    struct FPort
    {
        FGuid Id;FName Name;EEdGraphPinDirection Direction=EGPD_Input;FEdGraphPinType Type;
        FString Raw,Auto;bool HasRaw=false,HasAuto=false;TArray<FString> Links;
    };
    struct FPlan
    {
        FString Name,Reference;FGuid Id;TArray<FPort> Ports;TMap<FString,FString> Fields;int32 Source=INDEX_NONE;
    };
    struct FSource
    {
        TStrongObjectPtr<UEdGraph> Graph;FGuid Guid;FString Path,Projection;
        TArray<TStrongObjectPtr<UEdGraphNode>> Nodes;TArray<UEdGraphNode*> Slots;
        // ADO: saved external source roles are held before any importer/observer SDK.
        bool External=false,BaselineEstablished=false;TStrongObjectPtr<UBlueprint> Owner;TStrongObjectPtr<UPackage> OwnerPackage;TStrongObjectPtr<UClass> NativeParent;
        FString OwnerPath,PackagePath,File,FileHash;int64 FileBytes=0;
        TArray<TStrongObjectPtr<UEdGraph>> MacroGraphs;
        TArray<FGuid> NodeIds;TArray<UClass*> NodeClasses;
    };
    struct FHeld{TStrongObjectPtr<UK2Node_MacroInstance> Node;int32 Plan=INDEX_NONE;};
    struct FFieldRole{FProperty* Field=nullptr;UStruct* Owner=nullptr;FName Name;FFieldClass* Class=nullptr;EPropertyFlags Flags=CPF_None;int32 Offset=0,Size=0,Dim=0;};
    FResult& R;bool Active=false,Stopped=false,Adopted=false;
    TStrongObjectPtr<UBlueprint> Blueprint;TStrongObjectPtr<UEdGraph> Target;TStrongObjectPtr<UClass> Parent,MacroClass;
    TStrongObjectPtr<UPackage> Package;TStrongObjectPtr<UScriptStruct> ReferenceStruct;
    FString BPPath,TargetPath;FGuid TargetId;TArray<UEdGraph*> MacroSlots;
    TArray<FPlan> Plans;TArray<FSource> Sources;TArray<FHeld> Nodes;TArray<FFieldRole> Fields;
    FStructProperty* ReferenceProperty=nullptr;FObjectProperty* GraphProperty=nullptr;FObjectProperty* BlueprintProperty=nullptr;FStructProperty* GuidProperty=nullptr;
    TSharedPtr<FJsonObject> ProtectedBefore;
    explicit FOwnMacroInstanceImport(FResult& In):R(In){}
    bool Bad(const FString& Why,const FString& Object=FString(),const FString& Property=FString(),const FString& Expected=FString(),const FString& Actual=FString())
    {
        if(!Stopped)
        {
            Stopped=true;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("object"),Object);J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);J->SetStringField(TEXT("detail"),Why);R.Data->SetObjectField(TEXT("own_macro_first_mismatch"),J);
            if(R.ErrorCode.IsEmpty())Fail(R,R.bMutationAttempted?TEXT("graph_transfer_own_macro_current_lost"):TEXT("graph_transfer_own_macro_pending"),Why);
        }
        if(R.bMutationAttempted)R.bChangeStateKnown=false;return false;
    }
    static FString U(const FString& S){return FCollapsedTextPlan::Unquote(S);}
    static bool SameType(const FEdGraphPinType& A,const FEdGraphPinType& B){return FSelectKnotImport::SameType(A,B);}
    static bool Primitive(const FEdGraphPinType& T)
    {
        const FName C=T.PinCategory;
        return (C==UEdGraphSchema_K2::PC_Exec||C==UEdGraphSchema_K2::PC_Wildcard||C==UEdGraphSchema_K2::PC_Boolean||C==UEdGraphSchema_K2::PC_Int||C==UEdGraphSchema_K2::PC_Int64||C==UEdGraphSchema_K2::PC_Real||C==UEdGraphSchema_K2::PC_Name||C==UEdGraphSchema_K2::PC_String)
            &&T.ContainerType==EPinContainerType::None&&!T.bIsReference&&!T.bIsConst&&!T.bIsWeakPointer&&!T.bIsUObjectWrapper&&!T.bSerializeAsSinglePrecisionFloat&&T.PinSubCategoryObject.IsExplicitlyNull()&&T.PinSubCategoryMemberReference.MemberName.IsNone()
            &&T.PinValueType.TerminalCategory.IsNone()&&T.PinValueType.TerminalSubCategory.IsNone()&&T.PinValueType.TerminalSubCategoryObject.IsExplicitlyNull()&&!T.PinValueType.bTerminalIsConst&&!T.PinValueType.bTerminalIsWeakPointer&&!T.PinValueType.bTerminalIsUObjectWrapper
            &&((C==UEdGraphSchema_K2::PC_Real&&(T.PinSubCategory==TEXT("float")||T.PinSubCategory==TEXT("double")))||(C!=UEdGraphSchema_K2::PC_Real&&T.PinSubCategory.IsNone()));
    }
    bool Schema()
    {
        if(!Loaded(MacroClass.Get())||MacroClass.Get()!=UK2Node_MacroInstance::StaticClass()||!Loaded(ReferenceStruct.Get())||ReferenceStruct.Get()!=FGraphReference::StaticStruct())return Bad(TEXT("Native macro/reference schema identity changed."));
        for(const auto& V:Fields)
        {
            if(!Loaded(V.Owner))return Bad(TEXT("Native descriptor owner is not current."));auto* P=FindFProperty<FProperty>(V.Owner,V.Name);
            if(P!=V.Field||P->GetOwnerStruct()!=V.Owner||P->GetClass()!=V.Class||P->GetPropertyFlags()!=V.Flags||P->GetOffset_ForInternal()!=V.Offset||P->GetSize()!=V.Size||P->ArrayDim!=V.Dim||P->HasSetterOrGetter())return Bad(TEXT("Macro/reference descriptor owner/layout changed."),V.Owner->GetPathName(),V.Name.ToString());
        }
        return (ReferenceProperty&&ReferenceProperty->Struct.IsResolved()&&ReferenceProperty->Struct.Get()==ReferenceStruct.Get()&&GraphProperty&&GraphProperty->PropertyClass.IsResolved()&&GraphProperty->PropertyClass.Get()==UEdGraph::StaticClass()&&BlueprintProperty&&BlueprintProperty->PropertyClass.IsResolved()&&BlueprintProperty->PropertyClass.Get()==UBlueprint::StaticClass()&&GuidProperty&&GuidProperty->Struct.IsResolved()&&GuidProperty->Struct.Get()==TBaseStructure<FGuid>::Get())||Bad(TEXT("Current native macro/reference descriptor types differ."));
    }
    bool BindSchema()
    {
        MacroClass.Reset(UK2Node_MacroInstance::StaticClass());ReferenceStruct.Reset(FGraphReference::StaticStruct());
        ReferenceProperty=FindFProperty<FStructProperty>(MacroClass.Get(),TEXT("MacroGraphReference"));GraphProperty=FindFProperty<FObjectProperty>(ReferenceStruct.Get(),TEXT("MacroGraph"));BlueprintProperty=FindFProperty<FObjectProperty>(ReferenceStruct.Get(),TEXT("GraphBlueprint"));GuidProperty=FindFProperty<FStructProperty>(ReferenceStruct.Get(),TEXT("GraphGuid"));
        for(FProperty* P:{static_cast<FProperty*>(ReferenceProperty),static_cast<FProperty*>(GraphProperty),static_cast<FProperty*>(BlueprintProperty),static_cast<FProperty*>(GuidProperty)})
        {
            if(!P||P->ArrayDim!=1||P->HasSetterOrGetter())return Bad(TEXT("Exact native macro/reference fields are required."));FFieldRole V;V.Field=P;V.Owner=P->GetOwnerStruct();V.Name=P->GetFName();V.Class=P->GetClass();V.Flags=P->GetPropertyFlags();V.Offset=P->GetOffset_ForInternal();V.Size=P->GetSize();V.Dim=P->ArrayDim;Fields.Add(V);
        }
        return Schema()||Bad(TEXT("Native macro/reference field types differ."));
    }
    bool Base()
    {
        if(!Active)return true;if(Stopped)return false;auto* BP=Blueprint.Get();auto* G=Target.Get();
        if(!IsEditorGameThread()||IsPlaySessionActive()||!Loaded(BP)||!Loaded(G)||!Loaded(Package.Get())||!Loaded(Parent.Get())||BP->GetClass()!=UBlueprint::StaticClass()||BP->BlueprintType!=BPTYPE_Normal||BP->GetPathName()!=BPPath||BP->GetPackage()!=Package.Get()||!BP->ParentClass.GetGCPtr().IsResolved()||BP->ParentClass.Get()!=Parent.Get()||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bIsRegeneratingOnLoad||BP->bSuppressStructurallyModified||G->GetPathName()!=TargetPath||G->GraphGuid!=TargetId||G->GetOuter()!=BP||G->GetClass()!=UEdGraph::StaticClass()||!G->Schema.GetGCPtr().IsResolved()||G->Schema.Get()!=UEdGraphSchema_K2::StaticClass()||BP->MacroGraphs.Num()!=MacroSlots.Num())return Bad(TEXT("Own macro target/source Blueprint, graph or compiler context changed."),BPPath,TEXT("owner/current"));
        for(int32 I=0;I<MacroSlots.Num();++I)if(!BP->MacroGraphs[I].IsResolved()||BP->MacroGraphs[I].Get()!=MacroSlots[I])return Bad(TEXT("Own macro graph inventory changed."),BPPath,TEXT("MacroGraphs"));
        return Schema();
    }
    bool GraphProjection(FSource& S,const FIndex& I,FString& Out)
    {
        auto* G=S.Graph.Get();if(!Loaded(G)||G->GetClass()!=UEdGraph::StaticClass()||G->GetOuter()!=Blueprint.Get()||G->GetPackage()!=Package.Get()||G->GraphGuid!=S.Guid||G->GetPathName()!=S.Path||!G->Schema.GetGCPtr().IsResolved()||G->Schema.Get()!=UEdGraphSchema_K2::StaticClass()||!G->SubGraphs.IsEmpty()||G->Nodes.Num()!=S.Slots.Num())return Bad(TEXT("Referenced own macro graph role or node inventory changed."),S.Path,TEXT("graph role"));
        auto J=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;
        for(int32 K=0;K<S.Slots.Num();++K)
        {
            if(!G->Nodes[K].IsResolved()||G->Nodes[K].Get()!=S.Slots[K])return Bad(TEXT("Referenced macro node slot changed."),S.Path,TEXT("Nodes"));auto* N=S.Nodes[K].Get();
            if(!Loaded(N)||!I.Nodes.Contains(N)||N->GetOuter()!=G||!N->NodeGuid.IsValid())return Bad(TEXT("Referenced macro node owner/lifetime changed."),S.Path,TEXT("node"));
            UClass* C=N->GetClass();if(C!=UK2Node_Tunnel::StaticClass()&&C!=UK2Node_IfThenElse::StaticClass()&&C!=UK2Node_ExecutionSequence::StaticClass()&&C!=UK2Node_Knot::StaticClass())return Bad(TEXT("Flat stock Tunnel/Branch/Sequence/Knot source bodies required; other valid macros remain incomplete."),N->GetPathName(),TEXT("class"));
            auto Q=Node(N,I);Q->SetNumberField(TEXT("desired_enabled_state"),static_cast<int32>(N->GetDesiredEnabledState()));TArray<TSharedPtr<FJsonValue>> Pins;TSet<FString> Keys;
            for(auto* P:N->Pins)
            {
                if(!P||!I.Pins.Contains(P)||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||P->ParentPin||!P->SubPins.IsEmpty()||!Primitive(P->PinType)||!P->DefaultObject.IsResolved()||P->DefaultObject.Get()||!P->DefaultTextValue.IsEmpty()||P->bOrphanedPin||P->bNotConnectable)return Bad(TEXT("Source macro has an unsupported/noncurrent logical port."),N->GetPathName(),TEXT("pins"));
                const FString Key=P->PinName.ToString()+LexToString(static_cast<int32>(P->Direction));if(Keys.Contains(Key))return Bad(TEXT("Ambiguous source logical port."),N->GetPathName(),Key);Keys.Add(Key);
                auto V=Pin(P,I);V->RemoveField(TEXT("pin_id"));TArray<TSharedPtr<FJsonValue>> Links;
                for(auto* L:P->LinkedTo){if(!L||!I.Pins.Contains(L)||L->GetOwningNodeUnchecked()->GetGraph()!=G||!L->LinkedTo.Contains(P))return Bad(TEXT("Source macro link is not a current reciprocal own-graph edge."),N->GetPathName(),Key);Links.Add(MakeShared<FJsonValueString>(L->GetOwningNodeUnchecked()->NodeGuid.ToString()+TEXT(":")+L->PinName.ToString()+TEXT(":")+LexToString(static_cast<int32>(L->Direction))));}
                Links.Sort([](const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B){return A->AsString()<B->AsString();});V->SetArrayField(TEXT("linked_to"),Links);Pins.Add(MakeShared<FJsonValueObject>(V));
            }
            Pins.Sort([](const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B){return A->AsObject()->GetStringField(TEXT("name"))+A->AsObject()->GetStringField(TEXT("direction"))<B->AsObject()->GetStringField(TEXT("name"))+B->AsObject()->GetStringField(TEXT("direction"));});Q->SetArrayField(TEXT("pins"),Pins);
            if(auto* T=Cast<UK2Node_Tunnel>(N))
            {
                Q->SetBoolField(TEXT("can_have_inputs"),T->bCanHaveInputs);Q->SetBoolField(TEXT("can_have_outputs"),T->bCanHaveOutputs);TArray<TSharedPtr<FJsonValue>> User;
                for(const auto& V:T->UserDefinedPins){if(!V||!Primitive(V->PinType))return Bad(TEXT("Source tunnel user-pin descriptor is unsupported."),N->GetPathName(),TEXT("UserDefinedPins"));auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("name"),V->PinName.ToString());P->SetNumberField(TEXT("direction"),static_cast<int32>(V->DesiredPinDirection));P->SetStringField(TEXT("default"),V->PinDefaultValue);P->SetObjectField(TEXT("type"),Function::Pin::Describe(V->PinType));User.Add(MakeShared<FJsonValueObject>(P));}Q->SetArrayField(TEXT("user_pins"),User);
                auto* Meta=FindFProperty<FStructProperty>(UK2Node_Tunnel::StaticClass(),TEXT("MetaData"));if(!Meta||Meta->GetOwnerStruct()!=UK2Node_Tunnel::StaticClass()||Meta->HasSetterOrGetter())return Bad(TEXT("Source tunnel metadata descriptor unavailable."));FString Value;Meta->ExportText_InContainer(0,Value,T,nullptr,T,PPF_None);if(!Base()||Value.Len()>65536)return Bad(TEXT("Source tunnel metadata export/current boundary failed."));Q->SetStringField(TEXT("metadata"),Value);
            }
            Rows.Add(MakeShared<FJsonValueObject>(Q));
        }
        J->SetArrayField(TEXT("nodes"),Rows);Out=Json(J);return Out.Len()<=1048576||Bad(TEXT("Source macro projection bound exceeded."));
    }
    // ADO source: node_tools.py861 -> Part3.cpp489; the original importer has
    // no same-owner macro restriction. SDK MacroInstance.cpp72/514 preserves
    // MacroLibrary references; EdGraph.cpp54 lazily fills the nullable graph cache.
    // This observation contains no virtual getter, tracked object read, text rebuild,
    // load or exporter. Physical pins are reacquired per observation, never frozen.
    static TSharedRef<FJsonObject> PureType(const FEdGraphPinType& T)
    {
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("category"),T.PinCategory.ToString());J->SetStringField(TEXT("subcategory"),T.PinSubCategory.ToString());return J;
    }
    static bool ExternalPrimitive(const FEdGraphPinType& T)
    {
        return Primitive(T)&&T.PinSubCategoryMemberReference.MemberParent.IsResolved()&&T.PinSubCategoryMemberReference.MemberParent.GetHandle()==TObjectPtr<UObject>().GetHandle()&&!T.PinSubCategoryMemberReference.MemberGuid.IsValid();
    }
    static FString TextIdentity(const FText& T)
    {
        return LexToString(reinterpret_cast<UPTRINT>(FTextInspector::GetSharedDataId(T)))+TEXT(":")+LexToString(FTextInspector::GetFlags(T));
    }
    bool SourceBad(FSource& S,const FString& Why,const FString& Object=FString(),const FString& Property=FString(),const FString& Expected=FString(),const FString& Actual=FString())
    {
        // CanImport/PostPaste/observer callbacks can mutate source even before our writer.
        if(S.BaselineEstablished){R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("external_macro_source_change_uncertain"),true);}return Bad(Why,Object,Property,Expected,Actual);
    }
    bool ExternalRole(FSource& S)
    {
        auto* B=S.Owner.Get();auto* P=S.OwnerPackage.Get();auto* G=S.Graph.Get();
        if(!IsEditorGameThread()||IsPlaySessionActive()||!Loaded(B)||!Loaded(P)||!Loaded(G)||!Loaded(S.NativeParent.Get())||B->GetClass()!=UBlueprint::StaticClass()||B->BlueprintType!=BPTYPE_MacroLibrary||B->GetOuter()!=P||B->GetPackage()!=P||!B->GetPathName().Equals(S.OwnerPath,ESearchCase::CaseSensitive)||!P->GetPathName().Equals(S.PackagePath,ESearchCase::CaseSensitive)||P->IsDirty()||!P->IsFullyLoaded()||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||B->bSuppressStructurallyModified||B->Status==BS_BeingCreated||!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.GetGCPtr().GetHandle()!=TObjectPtr<UClass>(S.NativeParent.Get()).GetHandle()||B->NewVariables.Num()||B->FunctionGraphs.Num()||B->UbergraphPages.Num()||B->EventGraphs.Num()||B->DelegateSignatureGraphs.Num()||B->ImplementedInterfaces.Num()||B->ComponentTemplates.Num()||B->Timelines.Num()||!B->SimpleConstructionScript.IsResolved()||B->SimpleConstructionScript.GetHandle()!=TObjectPtr<USimpleConstructionScript>().GetHandle()||B->MacroGraphs.Num()!=S.MacroGraphs.Num())return SourceBad(S,TEXT("External saved MacroLibrary owner/current role changed."),S.OwnerPath,TEXT("owner/package/parent"));
        for(int32 K=0;K<S.MacroGraphs.Num();++K){auto* M=S.MacroGraphs[K].Get();if(!B->MacroGraphs[K].IsResolved()||B->MacroGraphs[K].GetHandle()!=TObjectPtr<UEdGraph>(M).GetHandle()||!Loaded(M)||M->GetClass()!=UEdGraph::StaticClass()||M->GetOuter()!=B||M->GetPackage()!=P)return SourceBad(S,TEXT("External MacroGraphs ownership/inventory changed."),S.OwnerPath,TEXT("MacroGraphs"));}
        if(G->GetClass()!=UEdGraph::StaticClass()||G->GetOuter()!=B||G->GetPackage()!=P||G->GraphGuid!=S.Guid||!G->GetPathName().Equals(S.Path,ESearchCase::CaseSensitive)||!G->Schema.GetGCPtr().IsResolved()||G->Schema.GetGCPtr().GetHandle()!=TObjectPtr<UClass>(UEdGraphSchema_K2::StaticClass()).GetHandle()||G->SubGraphs.Num()||G->Nodes.Num()!=S.Nodes.Num())return SourceBad(S,TEXT("External source graph ownership/GUID/schema changed."),S.Path,TEXT("graph role"));
        for(int32 K=0;K<S.Nodes.Num();++K){auto* N=S.Nodes[K].Get();if(!G->Nodes[K].IsResolved()||G->Nodes[K].GetHandle()!=TObjectPtr<UEdGraphNode>(N).GetHandle()||!Loaded(N)||N->GetOuter()!=G||N->GetPackage()!=P||N->NodeGuid!=S.NodeIds[K]||N->GetClass()!=S.NodeClasses[K])return SourceBad(S,TEXT("External source node ownership/GUID/class changed."),S.Path,TEXT("Nodes"));}
        return true;
    }
    bool ExternalProjection(FSource& S,FString& Out)
    {
        if(!ExternalRole(S))return false;TSet<UEdGraphPin*> Live;TMap<UEdGraphPin*,UEdGraphNode*> Owners;
        for(const auto& H:S.Nodes)for(auto* P:H->Pins){if(!P||Live.Contains(P)||P->WasTrashed()||P->GetOwningNodeUnchecked()!=H.Get()||P->ParentPin||P->SubPins.Num()||!ExternalPrimitive(P->PinType)||!P->DefaultObject.IsResolved()||P->DefaultObject.GetHandle()!=TObjectPtr<UObject>().GetHandle()||TextIdentity(P->DefaultTextValue)!=TextIdentity(FText::GetEmpty())||P->bOrphanedPin||P->bNotConnectable)return SourceBad(S,TEXT("External macro source requires current primitive/exec unsplit logical pins."),S.Path,TEXT("pins"));Live.Add(P);Owners.Add(P,H.Get());}
        auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("editable"),S.Graph->bEditable);J->SetBoolField(TEXT("deletion"),S.Graph->bAllowDeletion);J->SetBoolField(TEXT("renaming"),S.Graph->bAllowRenaming);TArray<TSharedPtr<FJsonValue>> Rows;
        for(const auto& H:S.Nodes)
        {
            auto* N=H.Get();UClass* C=N->GetClass();if(C!=UK2Node_Tunnel::StaticClass()&&C!=UK2Node_IfThenElse::StaticClass()&&C!=UK2Node_ExecutionSequence::StaticClass()&&C!=UK2Node_Knot::StaticClass())return SourceBad(S,TEXT("External source body outside finite stock Tunnel/Branch/Sequence/Knot support."),N->GetPathName(),TEXT("class"));
            auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("id"),N->NodeGuid.ToString());Q->SetStringField(TEXT("name"),N->GetName());Q->SetStringField(TEXT("class"),C->GetPathName());Q->SetNumberField(TEXT("x"),N->NodePosX);Q->SetNumberField(TEXT("y"),N->NodePosY);Q->SetStringField(TEXT("comment"),N->NodeComment);Q->SetBoolField(TEXT("comment_visible"),N->bCommentBubbleVisible);Q->SetBoolField(TEXT("comment_pinned"),N->bCommentBubblePinned);Q->SetNumberField(TEXT("enabled"),static_cast<int32>(N->GetDesiredEnabledState()));TArray<TSharedPtr<FJsonValue>> Pins;TSet<FString> Keys;
            for(auto* P:N->Pins)
            {
                const FString Key=P->PinName.ToString()+TEXT(":")+LexToString(static_cast<int32>(P->Direction));if(Keys.Contains(Key))return SourceBad(S,TEXT("External source logical pins must be unique."),N->GetPathName(),Key);Keys.Add(Key);auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("key"),Key);V->SetObjectField(TEXT("type"),PureType(P->PinType));V->SetStringField(TEXT("default"),P->DefaultValue);V->SetStringField(TEXT("auto"),P->AutogeneratedDefaultValue);V->SetStringField(TEXT("text_identity"),TextIdentity(P->DefaultTextValue));V->SetStringField(TEXT("friendly_identity"),TextIdentity(P->PinFriendlyName));V->SetBoolField(TEXT("ignored"),P->bDefaultValueIsIgnored);V->SetBoolField(TEXT("readonly"),P->bDefaultValueIsReadOnly);V->SetBoolField(TEXT("hidden"),P->bHidden);V->SetBoolField(TEXT("advanced"),P->bAdvancedView);TArray<TSharedPtr<FJsonValue>> Links;TSet<UEdGraphPin*> Seen;
                for(auto* L:P->LinkedTo){auto* Owner=Owners.FindRef(L);if(!Live.Contains(L)||!Owner||Seen.Contains(L)||!L->LinkedTo.Contains(P))return SourceBad(S,TEXT("External source links must be closed reciprocal logical edges."),N->GetPathName(),Key);Seen.Add(L);Links.Add(MakeShared<FJsonValueString>(Owner->NodeGuid.ToString()+TEXT(":")+L->PinName.ToString()+TEXT(":")+LexToString(static_cast<int32>(L->Direction))));}
                Links.Sort([](const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B){return A->AsString()<B->AsString();});V->SetArrayField(TEXT("links"),Links);Pins.Add(MakeShared<FJsonValueObject>(V));
            }
            Pins.Sort([](const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B){return A->AsObject()->GetStringField(TEXT("key"))<B->AsObject()->GetStringField(TEXT("key"));});Q->SetArrayField(TEXT("pins"),Pins);
            if(C==UK2Node_Tunnel::StaticClass())
            {
                auto* T=static_cast<UK2Node_Tunnel*>(N);auto Binding=[&](const TObjectPtr<UK2Node_Tunnel>& Slot,FString& Value){if(!Slot.IsResolved())return false;if(Slot.GetHandle()==TObjectPtr<UK2Node_Tunnel>().GetHandle()){Value=TEXT("null");return true;}for(const auto& X:S.Nodes)if(X->GetClass()==UK2Node_Tunnel::StaticClass()&&Slot.GetHandle()==TObjectPtr<UK2Node_Tunnel>(static_cast<UK2Node_Tunnel*>(X.Get())).GetHandle()){Value=X->NodeGuid.ToString();return true;}return false;};FString Input,Output;if(!Binding(T->InputSinkNode,Input)||!Binding(T->OutputSourceNode,Output))return SourceBad(S,TEXT("External tunnel bindings leave the held source graph."));Q->SetStringField(TEXT("input_sink"),Input);Q->SetStringField(TEXT("output_source"),Output);Q->SetBoolField(TEXT("can_inputs"),T->bCanHaveInputs);Q->SetBoolField(TEXT("can_outputs"),T->bCanHaveOutputs);TArray<TSharedPtr<FJsonValue>> User;
                for(const auto& V:T->UserDefinedPins){if(!V||!ExternalPrimitive(V->PinType))return SourceBad(S,TEXT("External tunnel user-pin type is unsupported."));auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("name"),V->PinName.ToString());P->SetNumberField(TEXT("direction"),static_cast<int32>(V->DesiredPinDirection));P->SetStringField(TEXT("default"),V->PinDefaultValue);P->SetObjectField(TEXT("type"),PureType(V->PinType));User.Add(MakeShared<FJsonValueObject>(P));}Q->SetArrayField(TEXT("user_pins"),User);
                // HasLatentFunctions is an SDK-populated transient cache, not authored metadata.
                const auto Meta=T->MetaData;auto D=MakeShared<FJsonObject>();D->SetStringField(TEXT("tooltip"),TextIdentity(Meta.ToolTip));D->SetStringField(TEXT("category"),TextIdentity(Meta.Category));D->SetStringField(TEXT("keywords"),TextIdentity(Meta.Keywords));D->SetStringField(TEXT("compact_title"),TextIdentity(Meta.CompactNodeTitle));D->SetStringField(TEXT("deprecation"),Meta.DeprecationMessage);D->SetBoolField(TEXT("deprecated"),Meta.bIsDeprecated);D->SetBoolField(TEXT("call_in_editor"),Meta.bCallInEditor);D->SetBoolField(TEXT("thread_safe"),Meta.bThreadSafe);D->SetBoolField(TEXT("unsafe_construction"),Meta.bIsUnsafeDuringActorConstruction);D->SetStringField(TEXT("color"),Meta.InstanceTitleColor.ToString());auto Map=MakeShared<FJsonObject>();for(const auto& Pair:Meta.GetMetaDataMap())Map->SetStringField(Pair.Key.ToString(),Pair.Value);D->SetObjectField(TEXT("map"),Map);Q->SetObjectField(TEXT("metadata"),D);
            }
            Rows.Add(MakeShared<FJsonValueObject>(Q));
        }
        J->SetArrayField(TEXT("nodes"),Rows);Out=Json(J);return Out.Len()<=1048576||SourceBad(S,TEXT("External source logical projection bound exceeded."));
    }
    bool ExternalCurrent()
    {
        for(auto& S:Sources)if(S.External){FString Now;if(!ExternalProjection(S,Now))return false;if(!Now.Equals(S.Projection,ESearchCase::CaseSensitive))return SourceBad(S,TEXT("External source authored logical state changed across an SDK boundary."),S.Path,TEXT("authored projection"),S.Projection,Now);}return true;
    }
    bool FinalFiles()
    {
        if(!Active)return true;if(Stopped||!ExternalCurrent())return false;TSet<FString> Seen;TArray<TSharedPtr<FJsonValue>> Files;
        for(auto& S:Sources)if(S.External&&!Seen.Contains(S.File)){Seen.Add(S.File);if(IFileManager::Get().FileSize(*S.File)!=S.FileBytes)return SourceBad(S,TEXT("External source saved file size changed."),S.File,TEXT("bytes"));const FMD5Hash ActualHash=FMD5Hash::HashFile(*S.File);if(!ActualHash.IsValid())return SourceBad(S,TEXT("External source file hash read failed."),S.File);const FString Hash=LexToString(ActualHash);if(!ExternalCurrent()||!Hash.Equals(S.FileHash,ESearchCase::CaseSensitive))return SourceBad(S,TEXT("External source saved file bytes changed."),S.File,TEXT("hash"),S.FileHash,Hash);auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("file"),S.File);J->SetStringField(TEXT("md5"),Hash);J->SetNumberField(TEXT("bytes"),S.FileBytes);Files.Add(MakeShared<FJsonValueObject>(J));}R.Data->SetArrayField(TEXT("external_macro_source_files_preserved"),Files);return true;
    }
    bool SourceCurrent()
    {
        if(!Base())return false;FIndex I;if(!Index(Blueprint.Get(),I,R))return Bad(TEXT("Current source graph inventory could not be observed."));
        for(auto& S:Sources){if(S.External)continue;FString Now;if(!GraphProjection(S,I,Now))return false;if(!Now.Equals(S.Projection,ESearchCase::CaseSensitive))return Bad(TEXT("Referenced own macro authored body differs after an SDK boundary."),S.Path,TEXT("authored graph projection"),S.Projection,Now);}
        return ExternalCurrent();
    }
    bool ActualReference(UK2Node_MacroInstance* N,const FSource& S)
    {
        if(!Schema())return false;const void* Address=ReferenceProperty->ContainerPtrToValuePtr<void>(N);
        const auto G=GraphProperty->GetObjectPtrPropertyValue(GraphProperty->ContainerPtrToValuePtr<void>(Address));const auto B=BlueprintProperty->GetObjectPtrPropertyValue(BlueprintProperty->ContainerPtrToValuePtr<void>(Address));const FGuid Id=*GuidProperty->ContainerPtrToValuePtr<FGuid>(Address);
        if(!G.IsResolved()||!B.IsResolved())return Bad(TEXT("Actual imported graph-reference handle became unresolved; no getter/resolve follows."),N->GetPathName(),!B.IsResolved()?TEXT("MacroGraphReference.GraphBlueprint"):TEXT("MacroGraphReference.MacroGraph"),TEXT("resolved"),TEXT("unresolved"));
        if(B.GetHandle()!=TObjectPtr<UBlueprint>(S.Owner.Get()).GetHandle())return Bad(TEXT("Actual imported source Blueprint changed."),N->GetPathName(),TEXT("MacroGraphReference.GraphBlueprint"),S.OwnerPath,TEXT("different resolved handle"));
        if(Id!=S.Guid)return Bad(TEXT("Actual imported source graph GUID changed."),N->GetPathName(),TEXT("MacroGraphReference.GraphGuid"),S.Guid.ToString(),Id.ToString());
        const bool NullCache=G.GetHandle()==TObjectPtr<UObject>().GetHandle();if(!NullCache&&G.GetHandle()!=TObjectPtr<UEdGraph>(S.Graph.Get()).GetHandle())return Bad(TEXT("Actual imported graph cache points outside the admitted source."),N->GetPathName(),TEXT("MacroGraphReference.MacroGraph"),S.Path,TEXT("different resolved handle"));
        auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("instance"),N->GetPathName());Receipt->SetStringField(TEXT("source_blueprint"),S.OwnerPath);Receipt->SetStringField(TEXT("source_graph"),S.Path);Receipt->SetStringField(TEXT("source_guid"),Id.ToString());Receipt->SetBoolField(TEXT("lookup_cache_null"),NullCache);R.Data->SetObjectField(TEXT("own_macro_last_observed_reference"),Receipt);return true;
    }
    bool Current()
    {
        if(!Active)return true;if(Stopped||!SourceCurrent())return false;
        if(ProtectedBefore){TSharedRef<FJsonObject> Now=MakeShared<FJsonObject>();if(!Capture(Blueprint.Get(),TEXT("own_macro_current"),R,Now)||!Protected(ProtectedBefore.ToSharedRef(),Now,TargetId,R))return Bad(TEXT("Other authored graphs/member declarations changed across the macro paste boundary."));}
        if(!Adopted)return ExternalCurrent();
        TMap<FString,UEdGraphPin*> Ports;
        for(const auto& H:Nodes)
        {
            auto* N=H.Node.Get();const auto& O=Plans[H.Plan];if(!Target->Nodes.Contains(N)||!Loaded(N)||N->GetOuter()!=Target.Get()||N->GetClass()!=MacroClass.Get()||N->GetDesiredEnabledState()!=ENodeEnabledState::Enabled||N->Pins.Num()!=O.Ports.Num()||!ActualReference(N,Sources[O.Source]))return Bad(TEXT("Imported macro node/port/reference role changed."),GetPathNameSafe(N));
            for(const TCHAR* Key:{TEXT("NodePosX"),TEXT("NodePosY")})if(const auto* V=O.Fields.Find(Key)){int32 Expected=0;LexTryParseString(Expected,**V);const int32 Actual=FCString::Strcmp(Key,TEXT("NodePosX"))==0?N->NodePosX:N->NodePosY;if(Actual!=Expected)return Bad(TEXT("Imported macro authored position differs."),N->GetPathName(),Key,LexToString(Expected),LexToString(Actual));}
            if(const auto* V=O.Fields.Find(TEXT("NodeComment")))if(!N->NodeComment.Equals(U(*V),ESearchCase::CaseSensitive))return Bad(TEXT("Imported macro authored comment differs."),N->GetPathName(),TEXT("NodeComment"),U(*V),N->NodeComment);
            for(const TCHAR* Key:{TEXT("bCommentBubbleVisible"),TEXT("bCommentBubblePinned"),TEXT("bCanRenameNode")})if(const auto* V=O.Fields.Find(Key)){bool Expected=false;FCollapsedTextPlan::Boolean(*V,Expected);auto* F=FindFProperty<FBoolProperty>(UEdGraphNode::StaticClass(),Key);if(!F||F->GetOwnerStruct()!=UEdGraphNode::StaticClass()||F->HasSetterOrGetter()||F->GetPropertyValue_InContainer(N)!=Expected)return Bad(TEXT("Imported macro authored Boolean annotation differs."),N->GetPathName(),Key,*V);}
            for(auto* P:N->Pins)
            {
                if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||P->ParentPin||!P->SubPins.IsEmpty()||!P->DefaultObject.IsResolved()||P->DefaultObject.Get()||!P->DefaultTextValue.IsEmpty()||P->bOrphanedPin||P->bNotConnectable)return Bad(TEXT("Imported macro logical pin is not current."),N->GetPathName());
                const auto* E=O.Ports.FindByPredicate([&](const FPort& V){return V.Name==P->PinName&&V.Direction==P->Direction;});const FString Key=LexToString(H.Plan)+TEXT(":")+P->PinName.ToString()+TEXT(":")+LexToString(static_cast<int32>(P->Direction));
                if(!E||Ports.Contains(Key)||!SameType(P->PinType,E->Type)||(E->HasRaw&&!P->DefaultValue.Equals(E->Raw,ESearchCase::CaseSensitive)))return Bad(TEXT("Imported macro logical port/type/authored default differs after PostPaste/Reconstruct."),N->GetPathName(),P->PinName.ToString(),E?E->Raw:TEXT("known source port"),P->DefaultValue);Ports.Add(Key,P);
            }
        }
        for(const auto& H:Nodes)for(const auto& E:Plans[H.Plan].Ports)
        {
            auto* P=Ports.FindRef(LexToString(H.Plan)+TEXT(":")+E.Name.ToString()+TEXT(":")+LexToString(static_cast<int32>(E.Direction)));if(!P||P->LinkedTo.Num()!=E.Links.Num())return Bad(TEXT("Imported macro logical link count differs."));
            for(const FString& L:E.Links){FString Name,Id;L.Split(TEXT(" "),&Name,&Id);FGuid Guid;FGuid::Parse(Id.TrimStartAndEnd(),Guid);const int32 Peer=Plans.IndexOfByPredicate([&](const FPlan& V){return V.Name==Name;});const auto* Q=Peer==INDEX_NONE?nullptr:Plans[Peer].Ports.FindByPredicate([&](const FPort& V){return V.Id==Guid;});auto* Actual=Q?Ports.FindRef(LexToString(Peer)+TEXT(":")+Q->Name.ToString()+TEXT(":")+LexToString(static_cast<int32>(Q->Direction))):nullptr;if(!Actual||!P->LinkedTo.Contains(Actual)||!Actual->LinkedTo.Contains(P))return Bad(TEXT("Imported macro reciprocal logical edge differs."));}
        }
        R.Data->SetBoolField(TEXT("own_macro_actual_logical_ports_verified"),true);return ExternalCurrent();
    }
    bool ParsePort(const FString& Text,FPlan& O)
    {
        TMap<FString,FString> M;if(O.Ports.Num()>=64||!FCollapsedTextPlan::Map(Text,M))return Bad(TEXT("Bounded unambiguous macro pin tuple required."));FPort P;P.Name=FName(*U(M.FindRef(TEXT("PinName"))));const FString Direction=U(M.FindRef(TEXT("Direction")));
        if(P.Name.IsNone()||!FGuid::Parse(M.FindRef(TEXT("PinId")),P.Id)||!P.Id.IsValid()||(!Direction.IsEmpty()&&Direction!=TEXT("EGPD_Input")&&Direction!=TEXT("EGPD_Output")))return Bad(TEXT("Macro source logical pin/GUID/direction invalid."));P.Direction=Direction==TEXT("EGPD_Output")?EGPD_Output:EGPD_Input;
        const bool Exec=U(M.FindRef(TEXT("PinType.PinCategory")))==TEXT("exec");auto Type=M;if(Type.FindRef(TEXT("PinType.PinSubCategoryMemberReference"))==TEXT("()"))Type.Remove(TEXT("PinType.PinSubCategoryMemberReference"));if(Exec)Type[TEXT("PinType.PinCategory")]=TEXT("\"bool\"");if(!FSelectKnotImport::Type(Type,P.Type,TEXT("PinType.")))return Bad(TEXT("Primitive/exec/unsplit wildcard non-container macro ports required; wider types remain incomplete."));if(Exec)P.Type.PinCategory=UEdGraphSchema_K2::PC_Exec;if(!Primitive(P.Type))return Bad(TEXT("Unsupported macro pin type."));
        for(const auto& Pair:M)
        {
            const auto& K=Pair.Key;const FString V=U(Pair.Value);if(K==TEXT("PinId")||K==TEXT("PinName")||K==TEXT("Direction")||K.StartsWith(TEXT("PinType.")))continue;
            if(K==TEXT("DefaultValue")){P.HasRaw=true;P.Raw=V;continue;}if(K==TEXT("AutogeneratedDefaultValue")){P.HasAuto=true;P.Auto=V;continue;}
            if(K==TEXT("LinkedTo")){if(!FCollapsedTextPlan::Pieces(Pair.Value,P.Links)||P.Links.Num()>16)return Bad(TEXT("Bounded closed macro links required."));TSet<FString> SeenLinks;for(const auto& Link:P.Links){if(SeenLinks.Contains(Link))return Bad(TEXT("Duplicate serialized macro peer links are not admitted."));SeenLinks.Add(Link);}continue;}
            if(K==TEXT("PersistentGuid")){FGuid G;if(FGuid::Parse(V,G)&&!G.IsValid())continue;}
            if(K==TEXT("DefaultObject")&&(V.IsEmpty()||V==TEXT("None")))continue;
            if(K==TEXT("bDefaultValueIsIgnored")||K==TEXT("bDefaultValueIsReadOnly")||K==TEXT("bAdvancedView")){bool B;if(FCollapsedTextPlan::Boolean(V,B))continue;}
            if(K==TEXT("bHidden")||K==TEXT("bOrphanedPin")||K==TEXT("bNotConnectable")){if(V==TEXT("False"))continue;}
            return Bad(TEXT("Additional serialized macro pin fields remain incomplete."),O.Name,K);
        }
        if(P.Raw.Len()>4096||P.Auto.Len()>4096||O.Ports.ContainsByPredicate([&](const FPort& E){return E.Id==P.Id||(E.Name==P.Name&&E.Direction==P.Direction);}))return Bad(TEXT("Ambiguous/beyond-bound macro logical pin."));O.Ports.Add(MoveTemp(P));return true;
    }
    static bool ObjectPath(const FString& Text,const TCHAR* Short,const TCHAR* Long,FString& Out)
    {
        FString S=U(Text);if(S==TEXT("None")){Out.Reset();return true;}int32 Quote=S.Find(TEXT("'"));if(Quote==INDEX_NONE||!S.EndsWith(TEXT("'")))return false;const FString C=S.Left(Quote);if(C!=Short&&C!=Long)return false;Out=FPackageName::ExportTextPathToObjectPath(S);Out.TrimQuotesInline();return Out.StartsWith(TEXT("/"))&&ReferenceGuard(Out);
    }
    bool Admit(UBlueprint* BP,UEdGraph* G,const FString& S,FString& AdmissionText)
    {
        AdmissionText=S;const TCHAR* Scan=*S;FString L;while(FParse::Line(&Scan,L)){FString C;if(L.TrimStartAndEnd().StartsWith(TEXT("Begin Object "))&&FParse::Value(*L,TEXT("Class="),C)&&C==TEXT("/Script/BlueprintGraph.K2Node_MacroInstance")){Active=true;break;}}
        if(!Active)return true;
        if(!BP->ParentClass.GetGCPtr().IsResolved())return Bad(TEXT("Own macro parent class must already be resolved; no source resolution."));Blueprint.Reset(BP);Target.Reset(G);Parent.Reset(BP->ParentClass.Get());Package.Reset(BP->GetPackage());BPPath=BP->GetPathName();TargetPath=G->GetPathName();TargetId=G->GraphGuid;
        if(BP->GetClass()!=UBlueprint::StaticClass()||BP->BlueprintType!=BPTYPE_Normal||BP->MacroGraphs.Num()>8||G->GetOuter()!=BP||!BindSchema())return Bad(TEXT("Existing ordinary Blueprint target and finite already-loaded macro source required; generated/library targets remain incomplete."));
        for(const auto& Slot:BP->MacroGraphs){if(!Slot.IsResolved()||!Loaded(Slot.Get()))return Bad(TEXT("Own macro graph slots must already be loaded/resolved."));MacroSlots.Add(Slot.Get());}
        const TCHAR* Stream=*S;int32 CurrentPlan=INDEX_NONE;AdmissionText.Reset();
        while(FParse::Line(&Stream,L))
        {
            FString T=L.TrimStartAndEnd();if(T.IsEmpty())continue;
            if(T.StartsWith(TEXT("Begin Object ")))
            {
                FString C,N;if(CurrentPlan!=INDEX_NONE||Plans.Num()>=16||!FParse::Value(*T,TEXT("Class="),C)||C!=TEXT("/Script/BlueprintGraph.K2Node_MacroInstance")||!FParse::Value(*T,TEXT("Name="),N)||!FCollapsedTextPlan::Name(N)||Plans.ContainsByPredicate([&](const FPlan& P){return P.Name==N;}))return Bad(TEXT("First own-macro import admits only complete flat stock MacroInstance blocks; mixed or nested clipboard forms remain incomplete."));FPlan O;O.Name=N;CurrentPlan=Plans.Add(MoveTemp(O));
            }
            else if(T==TEXT("End Object")){if(CurrentPlan==INDEX_NONE)return Bad(TEXT("Unbalanced macro node block."));CurrentPlan=INDEX_NONE;}
            else
            {
                if(CurrentPlan==INDEX_NONE)return Bad(TEXT("Unexpected text outside macro node blocks."));auto& O=Plans[CurrentPlan];
                if(T.StartsWith(TEXT("CustomProperties Pin "))){if(!ParsePort(T.Mid(21).TrimStartAndEnd(),O))return false;}
                else
                {
                    FString K,V;if(!T.Split(TEXT("="),&K,&V)||O.Fields.Contains(K))return Bad(TEXT("Duplicate/unsupported macro field."));O.Fields.Add(K,V);
                    if(K==TEXT("MacroGraphReference")){O.Reference=V;AdmissionText+=TEXT("MacroGraphReference=()\n");continue;}
                    if(K==TEXT("ResolvedWildcardType")){TMap<FString,FString> Type;FEdGraphPinType Ignore;if(!FCollapsedTextPlan::Map(V,Type))return Bad(TEXT("Unambiguous resolved wildcard cache tuple required."));if(!Type.IsEmpty()){if(Type.FindRef(TEXT("PinSubCategoryMemberReference"))==TEXT("()"))Type.Remove(TEXT("PinSubCategoryMemberReference"));const FString Category=U(Type.FindRef(TEXT("PinCategory")));if(Category.IsEmpty()||Category==TEXT("None"))Type.Add(TEXT("PinCategory"),TEXT("\"wildcard\""));if(!FSelectKnotImport::Type(Type,Ignore,TEXT("")))return Bad(TEXT("Only no-reference primitive resolved wildcard cache is admitted; cache is not a preservation invariant."));}}
                    else if(K==TEXT("MacroGraph")||K==TEXT("MacroGraph_DEPRECATED")){if(U(V)!=TEXT("None"))return Bad(TEXT("Legacy nonempty macro-reference cache input remains incomplete."));}
                    else if(K==TEXT("NodeGuid")){if(!FGuid::Parse(V,O.Id)||!O.Id.IsValid())return Bad(TEXT("Valid source macro NodeGuid required."));}
                    else if(K==TEXT("NodePosX")||K==TEXT("NodePosY")){int32 X;if(!LexTryParseString(X,*V))return Bad(TEXT("Bounded int32 macro node position required."));}
                    else if(K==TEXT("NodeComment")){if(U(V).Len()>65536)return Bad(TEXT("Macro node comment bound exceeded."));}
                    else if(K==TEXT("bCommentBubbleVisible")||K==TEXT("bCommentBubblePinned")||K==TEXT("bCanRenameNode")){bool B;if(!FCollapsedTextPlan::Boolean(V,B))return Bad(TEXT("Exact Boolean macro annotation field required."));}
                    else if(K==TEXT("EnabledState")){if(V!=TEXT("Enabled"))return Bad(TEXT("Disabled/development-only macro node states remain incomplete."));}
                    else if(K==TEXT("AdvancedPinDisplay")){if(V!=TEXT("NoPins")&&V!=TEXT("Hidden")&&V!=TEXT("Shown"))return Bad(TEXT("Unknown stock pin display state."));}
                    else return Bad(TEXT("Additional authored or compiler-cache macro fields remain incomplete."),O.Name,K);
                }
            }
            AdmissionText+=L+TEXT("\n");
        }
        if(CurrentPlan!=INDEX_NONE||Plans.IsEmpty()||!Base())return Bad(TEXT("Complete macro blocks and current source required."));
        FIndex I;if(!Index(BP,I,R))return Bad(TEXT("Source graph inventory could not be captured."));
        for(auto& O:Plans)
        {
            if(!O.Id.IsValid()||Plans.FilterByPredicate([&](const FPlan& P){return P.Id==O.Id;}).Num()!=1||O.Reference.IsEmpty())return Bad(TEXT("Unique source macro node GUID and explicit graph reference required."));TMap<FString,FString> Ref;if(!FCollapsedTextPlan::Map(O.Reference,Ref)||(Ref.Num()!=2&&Ref.Num()!=3)||!Ref.Contains(TEXT("GraphBlueprint"))||!Ref.Contains(TEXT("GraphGuid"))||(Ref.Num()==3&&!Ref.Contains(TEXT("MacroGraph"))))return Bad(TEXT("Exact Blueprint/GUID reference with optional nullable graph lookup cache required."));
            FString GraphPath,OwnerPath;FGuid Id;if(!ObjectPath(Ref.Contains(TEXT("MacroGraph"))?Ref[TEXT("MacroGraph")]:FString(TEXT("None")),TEXT("EdGraph"),TEXT("/Script/Engine.EdGraph"),GraphPath)||!ObjectPath(Ref[TEXT("GraphBlueprint")],TEXT("Blueprint"),TEXT("/Script/Engine.Blueprint"),OwnerPath)||!FGuid::Parse(Ref[TEXT("GraphGuid")],Id)||!Id.IsValid())return Bad(TEXT("Loaded source Blueprint and a valid owned macro graph GUID required."));
            const bool External=!OwnerPath.Equals(BPPath,ESearchCase::CaseSensitive);auto* Owner=External?FindObject<UBlueprint>(nullptr,*OwnerPath):BP;TStrongObjectPtr<UBlueprint> KeepOwner(Owner);if(!Loaded(Owner)||Owner->GetClass()!=UBlueprint::StaticClass())return Bad(TEXT("Source Blueprint must already be resident; no load or fallback follows."),OwnerPath);auto* OwnerPackage=Owner->GetPackage();TStrongObjectPtr<UPackage> KeepPackage(OwnerPackage);TArray<TStrongObjectPtr<UEdGraph>> SourceGraphs;
            if(External&&(OwnerPackage==Package.Get()||Owner->GetOuter()!=OwnerPackage||Owner->BlueprintType!=BPTYPE_MacroLibrary||!OwnerPackage->GetName().StartsWith(TEXT("/Game/"))||!ReferenceGuard(OwnerPath)||!Owner->ParentClass.GetGCPtr().IsResolved()||Owner->MacroGraphs.IsEmpty()||Owner->MacroGraphs.Num()>8))return Bad(TEXT("External source requires a distinct saved resident finite native Object/Actor MacroLibrary."),OwnerPath);
            for(const auto& Slot:Owner->MacroGraphs){if(!Slot.IsResolved())return Bad(TEXT("Source graph handle unresolved; no tracked read/resolve follows."));auto* M=static_cast<UEdGraph*>(StaticFindObject(UEdGraph::StaticClass(),nullptr,*Slot.GetPathName(),true));TStrongObjectPtr<UEdGraph> Hold(M);if(!Loaded(M)||Slot.GetHandle()!=TObjectPtr<UEdGraph>(M).GetHandle()||M->GetOuter()!=Owner||M->GetClass()!=UEdGraph::StaticClass())return Bad(TEXT("Source graph resident identity/owner differs."),OwnerPath);SourceGraphs.Add(MoveTemp(Hold));}
            UEdGraph* M=nullptr;for(const auto& Held:SourceGraphs)if(Held->GraphGuid==Id){if(M)return Bad(TEXT("Source macro GUID ambiguous."));M=Held.Get();}
            if(!M||M==G||(!GraphPath.IsEmpty()&&!GraphPath.Equals(M->GetPathName(),ESearchCase::CaseSensitive))||!M->SubGraphs.IsEmpty()||M->Nodes.Num()>128)return Bad(TEXT("Referenced source requires a distinct existing flat macro graph; deeper variants remain incomplete."));
            O.Source=Sources.IndexOfByPredicate([&](const FSource& V){return V.Graph.Get()==M;});
            if(O.Source==INDEX_NONE)
            {
                FSource V;V.External=External;V.Owner.Reset(Owner);V.OwnerPackage.Reset(OwnerPackage);V.OwnerPath=OwnerPath;V.PackagePath=OwnerPackage->GetName();V.MacroGraphs=MoveTemp(SourceGraphs);V.Graph.Reset(M);V.Guid=Id;V.Path=M->GetPathName();
                if(External){const auto ParentSlot=Owner->ParentClass.GetGCPtr();auto* Native=static_cast<UClass*>(StaticFindObject(UClass::StaticClass(),nullptr,*ParentSlot.GetPathName(),true));V.NativeParent.Reset(Native);if((Native!=UObject::StaticClass()&&Native!=AActor::StaticClass())||ParentSlot.GetHandle()!=TObjectPtr<UClass>(Native).GetHandle())return Bad(TEXT("External source parent must already be exact native Object or Actor."),OwnerPath);}
                for(const auto& Slot:M->Nodes){if(!Slot.IsResolved())return Bad(TEXT("Source node handle unresolved; no getter follows."));auto* N=static_cast<UEdGraphNode*>(StaticFindObject(UEdGraphNode::StaticClass(),nullptr,*Slot.GetPathName(),false));TStrongObjectPtr<UEdGraphNode> Hold(N);if(!Loaded(N)||Slot.GetHandle()!=TObjectPtr<UEdGraphNode>(N).GetHandle()||N->GetOuter()!=M||!N->NodeGuid.IsValid()||V.NodeIds.Contains(N->NodeGuid))return Bad(TEXT("Source node resident identity/owner/GUID differs."));V.Slots.Add(N);V.NodeIds.Add(N->NodeGuid);V.NodeClasses.Add(N->GetClass());V.Nodes.Add(MoveTemp(Hold));}
                O.Source=Sources.Add(MoveTemp(V));auto& Source=Sources[O.Source];if(External){if(!ExternalProjection(Source,Source.Projection))return false;Source.BaselineEstablished=true;if(!FPackageName::DoesPackageExist(Source.PackagePath,&Source.File))return Bad(TEXT("External source has no saved package file."),OwnerPath);Source.FileBytes=IFileManager::Get().FileSize(*Source.File);if(Source.FileBytes<=0||Source.FileBytes>67108864)return Bad(TEXT("External saved source file bound exceeded."),Source.File);const FMD5Hash InitialHash=FMD5Hash::HashFile(*Source.File);if(!InitialHash.IsValid())return Bad(TEXT("External source file hash read failed."),Source.File);Source.FileHash=LexToString(InitialHash);if(!ExternalCurrent())return false;}else if(!GraphProjection(Source,I,Source.Projection))return false;
            }
            TArray<UEdGraphPin*> SourcePorts;int32 Tunnels=0;for(const auto& Held:Sources[O.Source].Nodes){auto* N=Held.Get();if(N->GetClass()!=UK2Node_Tunnel::StaticClass())continue;++Tunnels;for(auto* P:N->Pins)SourcePorts.Add(P);}if(Tunnels!=2||SourcePorts.Num()!=O.Ports.Num()||SourcePorts.Num()>64)return Bad(TEXT("Two exact source tunnel gateways and matching complete instance port inventory required."));
            for(auto& P:O.Ports){UEdGraphPin** Match=SourcePorts.FindByPredicate([&](const UEdGraphPin* Q){return Q->PinName==P.Name&&Q->Direction!=P.Direction;});if(!Match||!SameType((*Match)->PinType,P.Type))return Bad(TEXT("Serialized macro logical ports/types differ from actual own tunnel ports."),O.Name,P.Name.ToString());if(!P.HasRaw&&P.Direction==EGPD_Input&&P.Type.PinCategory!=UEdGraphSchema_K2::PC_Exec){P.HasRaw=true;P.Raw=External?(*Match)->DefaultValue:(*Match)->GetDefaultAsString();}if(P.Type.PinCategory==UEdGraphSchema_K2::PC_Wildcard&&!P.Links.IsEmpty())return Bad(TEXT("Connected wildcard inference and concrete serialized wildcard specializations remain incomplete."));}
        }
        for(const auto& O:Plans)for(const auto& P:O.Ports)for(const auto& PeerLink:P.Links)
        {
            FString N,PeerGuidText;FGuid Id;if(!PeerLink.Split(TEXT(" "),&N,&PeerGuidText)||!FGuid::Parse(PeerGuidText.TrimStartAndEnd(),Id))return Bad(TEXT("Closed canonical macro peer link required."));const auto* Peer=Plans.FindByPredicate([&](const FPlan& P){return P.Name==N;});const auto* Q=Peer?Peer->Ports.FindByPredicate([&](const FPort& V){return V.Id==Id;}):nullptr;
            if(!Q||Peer==&O||Q->Direction==P.Direction||!SameType(P.Type,Q->Type)||!Q->Links.Contains(O.Name+TEXT(" ")+P.Id.ToString(EGuidFormats::Digits)))return Bad(TEXT("Original macro clipboard links must be closed, reciprocal and exactly typed."));
        }
        TSharedRef<FJsonObject> ProtectedState=MakeShared<FJsonObject>();if(!Capture(BP,TEXT("own_macro_source_before"),R,ProtectedState))return false;ProtectedBefore=ProtectedState;
        R.Data->SetStringField(TEXT("own_macro_scope"),TEXT("Original generic importer, same ordinary Blueprint's own macros or an already-loaded saved-clean distinct exact MacroLibrary with native Object/Actor parent and flat stock Tunnel/Branch/Sequence/Knot macros, exact source Blueprint/GUID plus nullable graph cache roles, primitive/exec logical ports and unlinked wildcards. MacroGraph lookup cache, ResolvedWildcardType, physical pin address/order and reconstruction caches are not frozen. Other source owners/cold loads, mixed node clipboard, nested/latent/custom bodies, containers/object/text pins, linked wildcard inference and full original/runtime parity remain incomplete; actual paste/compiler/save usage pending."));R.Data->SetNumberField(TEXT("own_macro_planned_nodes"),Plans.Num());R.Data->SetBoolField(TEXT("own_macro_references_prevalidated"),true);return Current();
    }
    bool Adopt(UEdGraph* G,const TSet<UEdGraphNode*>& Imported)
    {
        if(!Active)return true;if(!Current()||G!=Target.Get())return false;TSet<int32> Seen;
        for(auto* N:Imported)
        {
            if(!G->Nodes.Contains(N)||!Loaded(N)||N->GetOuter()!=G||N->GetClass()!=MacroClass.Get())return Bad(TEXT("Returned imported macro object is not in the current selected owner graph."));const int32 P=Plans.IndexOfByPredicate([&](const FPlan& O){return O.Id==N->NodeGuid;});if(P==INDEX_NONE||Seen.Contains(P))return Bad(TEXT("Returned macro nodes do not cover unique original source GUIDs."));Seen.Add(P);FHeld H;H.Node.Reset(static_cast<UK2Node_MacroInstance*>(N));H.Plan=P;Nodes.Add(MoveTemp(H));
        }
        if(Seen.Num()!=Plans.Num())return Bad(TEXT("Returned macro node count differs from the admitted source."));Adopted=true;return Current();
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
    bool LogicalPinOrder=false;bool Stopped=false,Acquired=false,Rebinding=false;TArray<FNode> Candidate;TArray<UEdGraphNode*> ExpectedAdded;
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
        if(N->Pins.Num()>128) { return Bad(TEXT("pin_bound")); }TSet<UEdGraphPin*> Unique;TSet<FGuid> LogicalIds;
        for(UEdGraphPin* P:N->Pins)
        {
            if(!P||Unique.Contains(P)||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||P->ParentPin||!P->SubPins.IsEmpty()||P->IsSnapped()
                ||!P->DefaultObject.IsResolved()) { return Bad(TEXT("owning_pin_role")); }
            if(LogicalPinOrder&&LogicalIds.Contains(P->PinId))return Bad(TEXT("duplicate_logical_pin_guid"));LogicalIds.Add(P->PinId);Unique.Add(P);FPin V;V.Address=P;V.Id=P->PinId;V.Name=P->PinName;V.Direction=P->Direction;V.Type=P->PinType;V.Raw=P->DefaultValue;V.Auto=P->AutogeneratedDefaultValue;
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
            const auto& E=Pins[I];if(LogicalPinOrder?!N->Pins.Contains(E.Address):N->Pins[I]!=E.Address) { return Bad(TEXT("pin_slot"),V.Path); }const auto* P=E.Address;
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
                const auto& P=E.Pins[J];auto* Match=LogicalPinOrder?A.Pins.FindByPredicate([&](const FPin& V){return V.Id==P.Id&&V.Name==P.Name&&V.Direction==P.Direction;}):&A.Pins[J];if(!Match)return Bad(TEXT("reconstructed_logical_pin_missing"),E.Path);auto& Q=*Match;const FText Copy=Q.Text;Q.TextDisplay=Copy.ToString();if(!Plain()) { return false; }const FText Friendly=Q.Friendly;Q.FriendlyDisplay=Friendly.ToString();if(!Plain()) { return false; }
                if(Q.Id!=P.Id||Q.Name!=P.Name||Q.Direction!=P.Direction||!CollapsedTypeSame(Q.Type,P.Type)||Q.Default!=P.Default||Q.Bits!=P.Bits||(LogicalPinOrder?(Q.LinkIds.Num()!=P.LinkIds.Num()||Q.LinkIds.ContainsByPredicate([&](const FString& L){return !P.LinkIds.Contains(L);})):Q.LinkIds!=P.LinkIds)
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
    FCollapsedObjectPlan TimelinePlan;TArray<FCollapsedObjectPlan> Companions;
    static FString PinKey(const FString& Node,const FGuid& Id){return Node+TEXT(":")+Id.ToString();}
    static bool LinkKey(const FString& Link,FString& Out)
    {TArray<FString> Parts;Link.ParseIntoArrayWS(Parts);FGuid Id;if(Parts.Num()!=2||!FCollapsedTextPlan::Name(Parts[0])||!FGuid::Parse(Parts[1],Id)||!Id.IsValid())return false;Out=PinKey(Parts[0],Id);return true;}
    bool ClosedLinks(FResult& R)
    {
        TMap<FString,const FCollapsedPinPlan*> Index;TArray<const FCollapsedObjectPlan*> Objects{&TimelinePlan};for(const auto& O:Companions)Objects.Add(&O);
        TSet<FString> NodeNames;TSet<FGuid> NodeIds;
        for(const auto* O:Objects)
        {
            if(NodeNames.Contains(O->Name)||NodeIds.Contains(O->Guid))return Fail(R,TEXT("timeline_clipboard_identity_pending"),TEXT("Unique copied node names/GUIDs required."));NodeNames.Add(O->Name);NodeIds.Add(O->Guid);TSet<FName> Names;
            for(const auto& P:O->Pins){const FString K=PinKey(O->Name,P.Id);if(!P.Id.IsValid()||Names.Contains(FName(*P.Name))||Index.Contains(K))return Fail(R,TEXT("timeline_clipboard_pin_pending"),TEXT("Unique copied pin names/GUIDs required."));Names.Add(FName(*P.Name));Index.Add(K,&P);}
        }
        for(const auto* O:Objects)for(const auto& P:O->Pins)
        {
            if(P.Links.Num()>64||((P.Category==TEXT("exec")&&P.Direction==EGPD_Output)||(P.Category!=TEXT("exec")&&P.Direction==EGPD_Input))&&P.Links.Num()>1)return Fail(R,TEXT("timeline_clipboard_link_pending"),TEXT("Bounded stock exec/scalar fanout required."));
            TSet<FString> Seen;for(const auto& L:P.Links)
            {
                FString K;if(!LinkKey(L,K)||Seen.Contains(K))return Fail(R,TEXT("timeline_clipboard_link_pending"),TEXT("Canonical unique clipboard peer references required."));Seen.Add(K);const auto* Slot=Index.Find(K);const auto* Q=Slot?*Slot:nullptr;
                if(!Q||Q==&P||Q->Direction==P.Direction||Q->Category!=P.Category||Q->SubCategory!=P.SubCategory||Q->TypeObject!=P.TypeObject)return Fail(R,TEXT("timeline_clipboard_link_pending"),TEXT("Closed internal reciprocal links with identical native pin types are required; external/dangling/implicit conversion variants remain incomplete."));
                const FString Own=PinKey(O->Name,P.Id);int32 Back=0;for(const auto& B:Q->Links){FString Peer;if(!LinkKey(B,Peer))return Fail(R,TEXT("timeline_clipboard_link_pending"),TEXT("Invalid reciprocal clipboard peer."));if(Peer==Own)++Back;}if(Back!=1)return Fail(R,TEXT("timeline_clipboard_link_pending"),TEXT("Exact reciprocal clipboard links required before target/source lookup."));
            }
        }return true;
    }
    bool Parse(const FString& S,FResult& R)
    {
        if(!Text(S,MaxText,R))return false;const TCHAR* Stream=*S;FString Line,Block,Class,NodeName;bool Open=false,TimelineSeen=false;TArray<FString> Bodies;
        while(FParse::Line(&Stream,Line))
        {
            const FString L=Line.TrimStartAndEnd();if(L.IsEmpty())continue;
            if(L.StartsWith(TEXT("Begin Object "),ESearchCase::CaseSensitive))
            {
                FString Export;if(Open||Bodies.Num()>=64||!FParse::Value(*L,TEXT("Class="),Class)||!FParse::Value(*L,TEXT("Name="),NodeName)||!FCollapsedTextPlan::Name(NodeName)||(Class!=TEXT("/Script/BlueprintGraph.K2Node_Timeline")&&Class!=TEXT("/Script/BlueprintGraph.K2Node_IfThenElse")&&Class!=TEXT("/Script/BlueprintGraph.K2Node_ExecutionSequence")))return Fail(R,TEXT("timeline_clipboard_text_pending"),TEXT("One Timeline with up to63 flat stock Branch/Sequence siblings is implemented; nested/custom/multiple Timeline variants remain incomplete."));
                FString Header=TEXT("Begin Object Class=")+Class+TEXT(" Name=\"")+NodeName+TEXT("\"");if(FParse::Value(*L,TEXT("ExportPath="),Export))Header+=TEXT(" ExportPath=\"")+Export+TEXT("\"");if(Header!=L)return Fail(R,TEXT("timeline_clipboard_header_pending"),TEXT("Canonical Class/Name/optional ExportPath header required."));Open=true;Block=L+TEXT("\n");continue;
            }
            if(!Open)return Fail(R,TEXT("timeline_clipboard_text_pending"),TEXT("Clipboard fields must belong to flat declared nodes."));Block+=L+TEXT("\n");if(L!=TEXT("End Object"))continue;Open=false;
            if(Class==TEXT("/Script/BlueprintGraph.K2Node_Timeline"))
            {if(TimelineSeen)return Fail(R,TEXT("timeline_clipboard_text_pending"),TEXT("Exactly one Timeline is implemented; additional Timeline objects remain incomplete."));if(!ParseSingle(Block,R))return false;TimelineSeen=true;}
            else
            {
                FCollapsedObjectPlan O;O.Name=NodeName;O.Class=Class;FCollapsedTextPlan Parser;const TCHAR* Body=*Block;FString Row;
                while(FParse::Line(&Body,Row))
                {
                    if(Row.StartsWith(TEXT("Begin Object "))||Row==TEXT("End Object"))continue;
                    if(Row.StartsWith(TEXT("CustomProperties Pin "))){if(O.Pins.Num()>=128||!Parser.Pin(Row.Mid(21),O,R,false))return false;continue;}
                    FString K,V;if(!Row.Split(TEXT("="),&K,&V)||O.Fields.Contains(K)||(K!=TEXT("NodeGuid")&&K!=TEXT("NodePosX")&&K!=TEXT("NodePosY")&&K!=TEXT("NodeComment")&&K!=TEXT("bCommentBubbleVisible")&&K!=TEXT("bCommentBubblePinned")&&K!=TEXT("bCanRenameNode")&&K!=TEXT("ErrorType")))return Fail(R,TEXT("timeline_clipboard_stock_field_pending"),TEXT("Only bounded stock sibling authored node fields are implemented."));O.Fields.Add(K,V);
                }
                if(!FGuid::Parse(O.Fields.FindRef(TEXT("NodeGuid")),O.Guid)||!O.Guid.IsValid())return Fail(R,TEXT("timeline_clipboard_guid_pending"),TEXT("Valid sibling node GUID required."));O.Comment=FCollapsedTextPlan::Unquote(O.Fields.FindRef(TEXT("NodeComment")));
                for(const TCHAR* K:{TEXT("NodePosX"),TEXT("NodePosY"),TEXT("ErrorType")})if(const auto* V=O.Fields.Find(K)){int32 N=0;if(!LexTryParseString(N,**V)||LexToString(N)!=*V)return Fail(R,TEXT("timeline_clipboard_integer_pending"),TEXT("Canonical stock int32 coordinate/error type required."));if(FString(K)==TEXT("NodePosX"))O.X=N;else if(FString(K)==TEXT("NodePosY"))O.Y=N;}
                for(const TCHAR* K:{TEXT("bCommentBubbleVisible"),TEXT("bCommentBubblePinned"),TEXT("bCanRenameNode")})if(const auto* V=O.Fields.Find(K)){bool B=false;if(!FCollapsedTextPlan::Boolean(*V,B))return Fail(R,TEXT("timeline_clipboard_bool_pending"),TEXT("Canonical stock node bool required."));}
                const bool Branch=Class==TEXT("/Script/BlueprintGraph.K2Node_IfThenElse");if((Branch&&O.Pins.Num()!=4)||(!Branch&&(O.Pins.Num()<3||O.Pins.Num()>65)))return Fail(R,TEXT("timeline_clipboard_stock_shape_pending"),TEXT("Stock Branch4 or Sequence execute/then_0..then_N required."));
                TSet<FString> Names;for(const auto& P:O.Pins){const bool Condition=Branch&&P.Name==TEXT("Condition");const bool Input=Condition||P.Name==TEXT("execute");if(Names.Contains(P.Name)||P.Category!=(Condition?TEXT("bool"):TEXT("exec"))||!P.SubCategory.IsEmpty()||P.Direction!=(Input?EGPD_Input:EGPD_Output)||(Branch&&!Condition&&P.Name!=TEXT("execute")&&P.Name!=TEXT("then")&&P.Name!=TEXT("else")))return Fail(R,TEXT("timeline_clipboard_stock_shape_pending"),TEXT("Exact stock sibling named/type/direction pin roles required."));Names.Add(P.Name);}
                if(!Names.Contains(TEXT("execute"))||(Branch&&(!Names.Contains(TEXT("Condition"))||!Names.Contains(TEXT("then"))||!Names.Contains(TEXT("else")))))return Fail(R,TEXT("timeline_clipboard_stock_shape_pending"),TEXT("Missing stock Branch/Sequence roles."));if(!Branch)for(int32 I=0;I<O.Pins.Num()-1;++I)if(!Names.Contains(TEXT("then_")+LexToString(I)))return Fail(R,TEXT("timeline_clipboard_stock_shape_pending"),TEXT("Contiguous Sequence then_N logical names required."));Companions.Add(MoveTemp(O));
            }
            Bodies.Add(Block);Block.Reset();
        }
        if(Open||!TimelineSeen)return Fail(R,TEXT("timeline_clipboard_text_pending"),TEXT("Balanced flat objects including one Timeline required."));TimelinePlan.Name=Name;TimelinePlan.Class=TEXT("/Script/BlueprintGraph.K2Node_Timeline");TimelinePlan.Guid=NodeGuid;TimelinePlan.X=X;TimelinePlan.Y=Y;TimelinePlan.Comment=Comment;
        for(const auto& M:Pins){FCollapsedPinPlan P;P.Name=FCollapsedTextPlan::Unquote(M.FindRef(TEXT("PinName")));if(!FCollapsedTextPlan::Name(P.Name)||!FGuid::Parse(M.FindRef(TEXT("PinId")),P.Id))return Fail(R,TEXT("timeline_clipboard_pin_pending"),TEXT("Valid Timeline pin name/GUID required."));P.Category=FCollapsedTextPlan::Unquote(M.FindRef(TEXT("PinType.PinCategory")));P.SubCategory=FCollapsedTextPlan::Unquote(M.FindRef(TEXT("PinType.PinSubCategory")));P.Direction=FCollapsedTextPlan::Unquote(M.FindRef(TEXT("Direction")))==TEXT("EGPD_Output")?EGPD_Output:EGPD_Input;P.Raw=FCollapsedTextPlan::Unquote(M.FindRef(TEXT("DefaultValue")));P.Auto=FCollapsedTextPlan::Unquote(M.FindRef(TEXT("AutogeneratedDefaultValue")));P.TypeObject=FCollapsedTextPlan::Unquote(M.FindRef(TEXT("PinType.PinSubCategoryObject")));if(P.TypeObject==TEXT("None"))P.TypeObject.Reset();if(M.Contains(TEXT("LinkedTo"))&&!FCollapsedTextPlan::Pieces(M.FindRef(TEXT("LinkedTo")),P.Links))return Fail(R,TEXT("timeline_clipboard_link_pending"),TEXT("Bounded closed Timeline links required."));TimelinePlan.Pins.Add(MoveTemp(P));}
        return ClosedLinks(R);
    }
    bool ParseSingle(const FString& S,FResult& R)
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
    // PostPasteNode's no-loaded-GUID branch constructs from the existing native
    // CDO, rather than duplicating an authored source Blueprint/template.
    bool OriginChecked=false,MissingOrigin=false;FObject FallbackDefault;
    TStrongObjectPtr<UClass> FallbackClass;TStrongObjectPtr<UPackage> FallbackPackage;
    uint32 FallbackClassFlags=0,FallbackObjectFlags=0;bool FallbackPackageDirty=false;
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
        if(!Context())return false;if(MissingOrigin)return FallbackCurrent()&&FindOrigin();
        if(!Source.Plain())return false;auto* B=Source.BP.Get();if(B->Timelines!=SourceSlots||static_cast<UBlueprintGeneratedClass*>(Source.Generated.Get())->Timelines!=SourceGenerated||static_cast<UBlueprintGeneratedClass*>(Source.Skeleton.Get())->Timelines!=SourceSkeleton)return Bad(TEXT("readonly_timeline_inventory"),TEXT("captured Blueprint/Gen/Skel arrays"),TEXT("changed"));
        // Close all retained curve identities before any track's content/path getter.
        for(const auto& E:SourceCurves){if(!ObjectCurrent(E.Role))return false;if(!Loaded(E.Package.Get())||(E.Import.Get()&&!Loaded(E.Import.Get())))return Bad(TEXT("readonly_curve_anchors"),E.Role.Path,TEXT("package/import unavailable"));auto* C=static_cast<UCurveBase*>(E.Role.Hold.Get());if(C->GetPackage()!=E.Package.Get()||!C->AssetImportData.IsResolved()||C->AssetImportData.Get()!=E.Import.Get())return Bad(TEXT("readonly_curve_roles"),E.Role.Path,TEXT("changed"));const FString A=CurveValue(C);if(A!=E.Value)return Bad(TEXT("readonly_curve_values"),E.Value,A);}
        for(const auto& E:SourceTemplates){if(!ObjectCurrent(E.Role))return false;FString A;if(!Values(static_cast<UTimelineTemplate*>(E.Role.Hold.Get()),A,true,false))return false;if(A!=E.Value)return Bad(TEXT("readonly_template_values"),E.Value,A);}
        for(const auto& E:SourceNodes){if(!Loaded(E.Node)||E.Node->TimelineName!=E.Name||E.Node->TimelineGuid!=E.Guid||NodeFlags(E.Node)!=E.Flags)return Bad(TEXT("readonly_timeline_node"),E.Name.ToString(),TEXT("changed"));}
        for(const auto& D:Disks){if(!Loaded(D.Package.Get())||D.Package->IsDirty()!=D.Dirty)return Bad(TEXT("readonly_package_dirty"),TEXT("captured false"),TEXT("changed/unavailable"));if(Files&&(IFileManager::Get().FileSize(*D.File)!=D.Bytes||FMD5Hash::HashFile(*D.File)!=D.Hash))return Bad(TEXT("readonly_source_file"),D.File,TEXT("size/hash changed"));}return Context();
    }
    bool FindOrigin()
    {
        UTimelineTemplate* First=nullptr;int32 Matches=0;for(TObjectIterator<UTimelineTemplate> It;It;++It){auto* T=*It;if(T->TimelineGuid==TextPlan.Selector){if(!First)First=T;++Matches;}}
        R.Data->SetNumberField(TEXT("timeline_loaded_matching_guid_count"),Matches);
        if(!OriginChecked)
        {
            OriginChecked=true;MissingOrigin=Matches==0;
            R.Data->SetBoolField(TEXT("timeline_source_absent_fallback"),MissingOrigin);
            R.Data->SetStringField(TEXT("timeline_guid_lookup_scope"),TEXT("Current loaded UObject iterator, matching installed Timeline PostPasteNode; not a search of all saved assets or a claim that no disk source exists."));
            if(MissingOrigin)return true;
            Origin.Reset(First);if(Matches!=1||!Loaded(First)||First->GetClass()!=UTimelineTemplate::StaticClass())return Pending(TEXT("An existing source requires one live native template matching the installed global GUID lookup; ambiguous/non-native loaded sources remain incomplete."));
        }
        if(MissingOrigin)return Matches==0||Bad(TEXT("global_template_selector"),TEXT("no loaded matching source throughout fallback"),FString::FromInt(Matches));
        return (First==Origin.Get()&&Matches==1)||Bad(TEXT("global_template_selector"),TEXT("same unique loaded source"),FString::FromInt(Matches));
    }
    bool FallbackCurrent()
    {
        if(!Context()||!ObjectCurrent(FallbackDefault))return false;auto* Class=FallbackClass.Get();
        if(!Loaded(Class)||Class!=UTimelineTemplate::StaticClass()||Class!=FallbackDefault.Class.Get()
            ||FallbackDefault.Outer.Get()!=Class->GetOuter()||!Loaded(FallbackPackage.Get())||Class->GetPackage()!=FallbackPackage.Get()
            ||Class->GetOutermost()->IsDirty()!=FallbackPackageDirty||static_cast<uint32>(Class->ClassFlags)!=FallbackClassFlags
            ||!Class->HasAnyClassFlags(CLASS_Native)||!Class->ClassGeneratedBy.IsResolved()||Class->ClassGeneratedBy!=nullptr
            ||!Class->ClassDefaultObject.IsResolved()||Class->ClassDefaultObject!=FallbackDefault.Hold.Get()
            ||FallbackDefault.Hold->GetPackage()!=FallbackPackage.Get()||static_cast<uint32>(FallbackDefault.Hold->GetFlags())!=FallbackObjectFlags)
            return Bad(TEXT("fallback_native_default_roles"),TEXT("same held native class/CDO/package and flags"),TEXT("changed/unavailable"));
        FString Actual;if(!Values(static_cast<UTimelineTemplate*>(FallbackDefault.Hold.Get()),Actual,false,false))return false;
        return Actual==Semantic||Bad(TEXT("fallback_native_default_values"),Semantic,Actual);
    }
    bool BindFallback()
    {
        auto* Class=UTimelineTemplate::StaticClass();FallbackClass.Reset(Class);
        if(!Loaded(Class)||!Class->HasAnyClassFlags(CLASS_Native)||!Class->ClassGeneratedBy.IsResolved()||Class->ClassGeneratedBy!=nullptr
            ||!Class->ClassDefaultObject.IsResolved())return Pending(TEXT("Missing-source fallback requires an already resolved loaded native TimelineTemplate class/CDO; no default object is created or loaded by preflight."));
        auto* Default=Class->ClassDefaultObject.Get();FallbackPackage.Reset(Class->GetPackage());
        if(!Loaded(Default)||Default->GetClass()!=Class||Default->GetOuter()!=Class->GetOuter()||!Default->HasAnyFlags(RF_ClassDefaultObject)
            ||!Loaded(FallbackPackage.Get())||Default->GetPackage()!=FallbackPackage.Get()||!Object(Default,FallbackDefault))return Pending(TEXT("Missing-source fallback requires the actual existing native TimelineTemplate CDO ownership roles."));
        const auto* Template=static_cast<UTimelineTemplate*>(Default);
        if(!Template->EventTracks.IsEmpty()||!Template->FloatTracks.IsEmpty()||!Template->VectorTracks.IsEmpty()||!Template->LinearColorTracks.IsEmpty()
            ||!Template->MetaDataArray.IsEmpty()||Template->GetNumDisplayTracks()!=0)
            return Pending(TEXT("Fallback currently admits native empty-track/metadata defaults; a modified CDO with authored tracks remains a valid incomplete variant."));
        // A missing source legitimately loses old authored tracks in the SDK. Do
        // not apply and then reject their stale serialized pins as an invariant.
        TSet<FName> Names;for(const TCHAR* Name:{TEXT("Play"),TEXT("PlayFromStart"),TEXT("Stop"),TEXT("Reverse"),TEXT("ReverseFromEnd"),TEXT("SetNewTime"),TEXT("Update"),TEXT("Finished"),TEXT("NewTime"),TEXT("Direction")})Names.Add(FName(Name));
        if(TextPlan.TimelinePlan.Pins.Num()!=Names.Num())return Pending(TEXT("Missing-source fallback currently accepts exactly the ten native base Timeline pin roles; stale authored-track pin removal/relink variants remain incomplete before mutation."));
        for(const auto& P:TextPlan.TimelinePlan.Pins)if(Names.Remove(FName(*P.Name))!=1)return Pending(TEXT("Missing-source fallback requires unique native base Timeline pin names."));
        FallbackClassFlags=static_cast<uint32>(Class->ClassFlags);FallbackObjectFlags=static_cast<uint32>(Default->GetFlags());FallbackPackageDirty=FallbackPackage->IsDirty();
        if(!Values(static_cast<UTimelineTemplate*>(Default),Semantic,false,false))return false;
        R.Data->SetStringField(TEXT("timeline_source_template"),FString());R.Data->SetStringField(TEXT("timeline_source_blueprint"),FString());
        R.Data->SetStringField(TEXT("timeline_source_selector_guid"),TextPlan.Selector.ToString());R.Data->SetStringField(TEXT("timeline_fallback_default_object"),FallbackDefault.Path);
        R.Data->SetStringField(TEXT("timeline_fallback_scope"),TEXT("Installed PostPasteNode no-loaded-GUID branch and AddNewTimeline; native empty default template is created under the target GeneratedClass with a fresh GUID/name. No authored source tracks are recovered. The existing native CDO is only observed, never created or mutated by preflight; stale authored-track pins/wider defaults remain incomplete."));
        return SourceCurrent(true);
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
        if(!FindOrigin())return false;if(MissingOrigin)return BindFallback();
        auto* O=Origin.Get();Target.Hold(O);Target.Hold(O->GetOuter());auto* GC=Cast<UBlueprintGeneratedClass>(O->GetOuter());if(!Loaded(GC)||!GC->ClassGeneratedBy.IsResolved())return Pending(TEXT("Source Timeline must belong to a resolved generated Blueprint class."));auto* B=Cast<UBlueprint>(GC->ClassGeneratedBy.Get());Target.Hold(B);
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
                if(K==TEXT("LinkedTo")){TArray<FString> L;if(!FCollapsedTextPlan::Pieces(P.Value,L)||L.Num()>64)return Pending(TEXT("Bounded closed Timeline pin links required; external/dangling references were refused before lookup."));continue;}
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
        FString A;if(!Values(T,A,false,false))return false;if(A!=Semantic)return Bad(MissingOrigin?TEXT("fallback_default_authored_values"):TEXT("duplicated_authored_values"),Semantic,A);
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
        if(!MissingOrigin)
        {
            for(int32 I=0;I<T->EventTracks.Num();++I)if(!CheckCurve(T->EventTracks[I].CurveKeys.Get(),Origin->EventTracks[I].CurveKeys.Get(),T->EventTracks[I].bIsExternalCurve))return false;
            for(int32 I=0;I<T->FloatTracks.Num();++I)if(!CheckCurve(T->FloatTracks[I].CurveFloat.Get(),Origin->FloatTracks[I].CurveFloat.Get(),T->FloatTracks[I].bIsExternalCurve))return false;
            for(int32 I=0;I<T->VectorTracks.Num();++I)if(!CheckCurve(T->VectorTracks[I].CurveVector.Get(),Origin->VectorTracks[I].CurveVector.Get(),T->VectorTracks[I].bIsExternalCurve))return false;
            for(int32 I=0;I<T->LinearColorTracks.Num();++I)if(!CheckCurve(T->LinearColorTracks[I].CurveLinearColor.Get(),Origin->LinearColorTracks[I].CurveLinearColor.Get(),T->LinearColorTracks[I].bIsExternalCurve))return false;
        }
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
            if(!E||Seen.Contains(P.Name)||P.Type.PinCategory!=E->Key||P.Type.PinSubCategory!=E->Value||P.Direction!=(Output?EGPD_Output:EGPD_Input)||P.Type.ContainerType!=EPinContainerType::None||P.Type.bIsReference||P.Type.bIsConst||P.Default||P.Type.PinSubCategoryObject.IsStale()||P.Type.PinValueType.TerminalSubCategoryObject.IsStale())return Bad(TEXT("allocated_pin_role"),TEXT("canonical unique plain Timeline pins with independently checked closed links"),P.Name.ToString());Seen.Add(P.Name);
            UObject* Type=P.Type.PinSubCategoryObject.Get();if(P.Name==TEXT("Direction")){if(!Loaded(Type)||Type->GetClass()!=UEnum::StaticClass()||Type->GetPathName()!=TEXT("/Script/Engine.ETimelineDirection"))return Bad(TEXT("direction_enum"),TEXT("native ETimelineDirection"),GetPathNameSafe(Type));}else if(Type!=Types.FindRef(P.Name))return Bad(TEXT("track_pin_type"),TEXT("native declared track type"),GetPathNameSafe(Type));
        }return true;
    }
    bool Returned(const TSet<UEdGraphNode*>& Imported)
    {
        if(!Context()||!SourceCurrent(true))return false;for(auto* N:Imported)Target.Hold(N);
        TArray<const FCollapsedObjectPlan*> Plans{&TextPlan.TimelinePlan};for(const auto& E:TextPlan.Companions)Plans.Add(&E);
        if(Imported.Num()!=Plans.Num())return Bad(TEXT("paste_returned_count"),FString::FromInt(Plans.Num()),FString::FromInt(Imported.Num()));
        TMap<FString,UEdGraphNode*> ByName;TMap<FString,UEdGraphPin*> Pins;
        for(const auto* E:Plans)
        {
            UEdGraphNode* N=nullptr;for(auto* O:Imported)if(Loaded(O)&&O->NodeGuid==E->Guid){if(N)return Bad(TEXT("paste_ambiguous_guid"),E->Guid.ToString(),TEXT("multiple returned nodes"));N=O;}
            if(!Loaded(N)||N->GetClass()->GetPathName()!=E->Class||N->GetGraph()!=Target.Selected||N->GetOuter()!=Target.Selected||!Target.Selected->Nodes.Contains(N))return Bad(TEXT("paste_returned_node"),E->Name,TEXT("missing/wrong native class or graph role"));
            const bool Timeline=E==&TextPlan.TimelinePlan;bool Visible=false,Pinned=false;if(Timeline){Visible=TextPlan.Visible;Pinned=TextPlan.Pinned;Pasted.Reset(static_cast<UK2Node_Timeline*>(N));}else{if(const auto* V=E->Fields.Find(TEXT("bCommentBubbleVisible")))FCollapsedTextPlan::Boolean(*V,Visible);if(const auto* V=E->Fields.Find(TEXT("bCommentBubblePinned")))FCollapsedTextPlan::Boolean(*V,Pinned);}
            if(N->NodePosX!=E->X||N->NodePosY!=E->Y||N->NodeComment!=E->Comment||bool(N->bCommentBubbleVisible)!=Visible||bool(N->bCommentBubblePinned)!=Pinned)return Bad(TEXT("paste_node_authored_fields"),E->Name,TEXT("coordinates/comment/bubble changed"));ByName.Add(E->Name,N);Target.ExpectedAdded.Add(N);
        }
        PasteReturned=true;if(!Target.Reacquire()||!NewTemplate(true))return false;
        TArray<FCollapsedTarget::FNode> Rows;
        for(const auto* E:Plans)
        {
            auto* N=ByName.FindChecked(E->Name);
            // Reacquire observes old-node FText callbacks before incoming nodes have a full snapshot.
            // Their copied roles remain authoritative; never accept callback changes as the first baseline.
            if(!Context()||!SourceCurrent())return false;
            if(!Loaded(N)||N->NodeGuid!=E->Guid||N->GetClass()->GetPathName()!=E->Class||N->GetGraph()!=Target.Selected||N->GetOuter()!=Target.Selected||!Target.Selected->Nodes.Contains(N))return Bad(TEXT("paste_incoming_role_after_reacquire"),E->Name,TEXT("copied GUID/exact class/graph role changed"));
            const bool Timeline=E==&TextPlan.TimelinePlan;bool Visible=false,Pinned=false;if(Timeline){Visible=TextPlan.Visible;Pinned=TextPlan.Pinned;}else{if(const auto* V=E->Fields.Find(TEXT("bCommentBubbleVisible")))FCollapsedTextPlan::Boolean(*V,Visible);if(const auto* V=E->Fields.Find(TEXT("bCommentBubblePinned")))FCollapsedTextPlan::Boolean(*V,Pinned);}
            if(N->NodePosX!=E->X||N->NodePosY!=E->Y||N->NodeComment!=E->Comment||bool(N->bCommentBubbleVisible)!=Visible||bool(N->bCommentBubblePinned)!=Pinned)return Bad(TEXT("paste_incoming_authored_after_reacquire"),E->Name,TEXT("copied coordinates/comment/bubble changed"));
            FCollapsedTarget::FNode Row;if(!Target.NodeCopy(N,Row)||(N==Pasted.Get()&&!PinTypes(Row.Pins)))return false;
            if(Row.Pins.Num()!=E->Pins.Num())return Bad(TEXT("copied_pin_count"),FString::FromInt(E->Pins.Num()),FString::FromInt(Row.Pins.Num()));
            for(const auto& P:E->Pins)
            {
                const auto* A=Row.Pins.FindByPredicate([&](const FCollapsedTarget::FPin& Q){return Q.Id==P.Id&&Q.Name==FName(*P.Name);});
                FString Auto=P.Auto,Raw=P.Raw;if(E!=&TextPlan.TimelinePlan&&P.Category==TEXT("bool")){if(Auto.IsEmpty())Auto=TEXT("true");if(Raw.IsEmpty())Raw=Auto;}
                if(!A||A->Type.PinCategory!=FName(*P.Category)||A->Type.PinSubCategory!=FName(*P.SubCategory)||A->Direction!=P.Direction||A->Raw!=Raw||A->Auto!=Auto||A->Default||A->Bits!=0||A->Type.ContainerType!=EPinContainerType::None||A->Type.bIsReference||A->Type.bIsConst)return Bad(TEXT("copied_pin_values"),E->Name+TEXT(".")+P.Name,TEXT("GUID/type/direction/default/flags changed"));if(E!=&TextPlan.TimelinePlan&&(!A->Type.PinSubCategoryObject.IsExplicitlyNull()||!A->Type.PinValueType.TerminalSubCategoryObject.IsExplicitlyNull()||A->Type.bIsWeakPointer||A->Type.bIsUObjectWrapper||A->Type.bSerializeAsSinglePrecisionFloat))return Bad(TEXT("copied_stock_pin_type"),E->Name+TEXT(".")+P.Name,TEXT("unexpected native type/object flags"));Pins.Add(FTITimelineText::PinKey(E->Name,P.Id),A->Address);
            }Rows.Add(MoveTemp(Row));
        }
        for(const auto* E:Plans)for(const auto& P:E->Pins)
        {
            auto* A=Pins.FindChecked(FTITimelineText::PinKey(E->Name,P.Id));if(A->LinkedTo.Num()!=P.Links.Num())return Bad(TEXT("copied_link_count"),FString::FromInt(P.Links.Num()),FString::FromInt(A->LinkedTo.Num()));
            for(const auto& L:P.Links){FString K;if(!FTITimelineText::LinkKey(L,K))return Bad(TEXT("copied_link_key"),L,TEXT("invalid"));auto* const* Q=Pins.Find(K);if(!Q||!A->LinkedTo.Contains(*Q)||!(*Q)->LinkedTo.Contains(A))return Bad(TEXT("copied_reciprocal_link"),L,TEXT("missing/substituted peer"));}
        }
        for(auto& Row:Rows)Target.Nodes.Add(MoveTemp(Row));if(!Target.SealLinks(Target.Nodes))return false;Ready=true;if(!Current())return false;
        for(auto& E:Target.Nodes)if(Imported.Contains(E.Object))for(auto& P:E.Pins){const FText Text=P.Text;const FString Display=Text.ToString();if(!Current())return false;const FText Friendly=P.Friendly;const FString FriendlyDisplay=Friendly.ToString();if(!Current())return false;P.TextDisplay=Display;P.FriendlyDisplay=FriendlyDisplay;}
        return Current(true);
    }
    bool Reacquire(){if(!Context()||!SourceCurrent(true)||!Target.Reacquire()||!NewTemplate(true))return false;return Current(true);}
};

Variable::FResult ImportTimeline(const FString& Path,const FString& TextToImport,const FString& FilePath,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool Clear,bool KeepRoot,bool Allow,bool ValidateOnly,bool DryRun,bool Compile,bool Save)
{
    FResult R;Init(R,Compile,Save);R.Data->SetBoolField(TEXT("native_timeline_clipboard_import"),true);R.Data->SetBoolField(TEXT("runtime_execution_verified"),false);R.Data->SetBoolField(TEXT("opaque_paste_callback_intermediates_verified"),false);
    R.Data->SetStringField(TEXT("finite_scope"),TEXT("Inline or once-read file source with one native Timeline plus up to63 flat stock Branch/ordered Sequence siblings with closed reciprocal same-type links into a direct Actor EventGraph, including bounded existing Timelines and clear_graph using original Node.Modify/Graph.RemoveNode. Prior authored templates/curves remain even when their Timeline nodes are removed. Existing nodes/peers use QZ stock callbacks plus exact Timeline; keep_root_node applies only to admitted root roles (no function/Anim root in this EventGraph profile). A matching loaded source uses the existing unique distinct saved-clean native template duplication path; zero loaded matching GUID uses the installed empty native CDO/AddNewTimeline fallback with exactly ten copied base pins. Absence only describes loaded UObjects, not all disk assets; no source tracks are recovered. Implicit compile reacquires survivor classes/pins/templates/curves; external links/nested/additional Timeline/custom nodes/stale authored-track pins/modified CDO/wider owners remain incomplete; actual fallback/mixed paste/save/runtime usage validation is pending."));
    if(!IsEditorGameThread()||IsPlaySessionActive()){Fail(R,TEXT("graph_transfer_editor_invalid"),TEXT("Idle Editor game thread required."));return R;}
    // FilePath is provenance only here: Import already selected inline text or read the file once.
    FTITimelineText Plan;if(!Plan.Parse(TextToImport,R))return R;R.Data->SetBoolField(TEXT("timeline_closed_link_preflight_passed"),true);R.Data->SetNumberField(TEXT("timeline_clipboard_planned_node_count"),1+Plan.Companions.Num());R.Data->SetBoolField(TEXT("mixed_timeline_stock_profile"),!Plan.Companions.IsEmpty());
    FString ObjectPath=Path;if(ObjectPath.StartsWith(TEXT("/Game/"))&&!ObjectPath.Contains(TEXT(".")))ObjectPath+=TEXT(".")+FPackageName::GetShortName(Path);
    Count(R,TEXT("reference_lookup_calls"));auto* B=FindObject<UBlueprint>(nullptr,*ObjectPath);TStrongObjectPtr<UBlueprint> Held(B);Count(R,TEXT("reference_lookup_returns"));if(!Scope(B,R)||!Sample(B,Allow,R))return R;
    UEdGraph* G=nullptr;FGuid Requested;const bool HasId=!GraphId.IsEmpty();if(HasId&&!FGuid::Parse(GraphId,Requested)){Fail(R,TEXT("graph_transfer_graph_unavailable"),TEXT("Invalid graph GUID."));return R;}const FString Type=Normalize(GraphType);
    Count(R,TEXT("get_all_graphs_calls"));TArray<UEdGraph*> All;B->GetAllGraphs(All);for(auto* Row:All){if(!Loaded(Row)||(HasId&&Row->GraphGuid!=Requested)||(!GraphName.IsEmpty()&&!Row->GetName().Equals(GraphName,ESearchCase::IgnoreCase))||(!Type.IsEmpty()&&Kind(B,Row)!=Type))continue;if(GraphName.IsEmpty()&&!HasId&&Type.IsEmpty()&&(!B->UbergraphPages.Contains(Row)||!Row->GetName().Contains(TEXT("EventGraph"))))continue;G=Row;break;}
    TStrongObjectPtr<UEdGraph> HeldGraph(G);if(!Loaded(G)){Fail(R,TEXT("graph_transfer_graph_unavailable"),TEXT("Existing EventGraph required; implicit missing-graph creation remains incomplete for Timeline profile."));return R;}
    FCollapsedTarget Lease(R);Lease.LogicalPinOrder=!Plan.Companions.IsEmpty();if(!Lease.Bind(B,G))return R;FTITimelineImport TI(R,Lease,Plan);if(!TI.CheckPinText()||!TI.Bind()||!TI.Current(true)||!TI.FindOrigin())return R;
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
        // CreateNewGuid is a pure GUID assignment (EdGraphNode.cpp); change all copied GUIDs
        // before one shared logical-link reseal, never accept a new topology baseline.
        for(auto* N:Imported)
        {
            Count(R,TEXT("new_guid_calls"));N->CreateNewGuid();if(!TI.Context())return;
            if(!N->NodeGuid.IsValid()){TI.Bad(TEXT("new_node_guid"),TEXT("valid unique GUID"),TEXT("invalid"));return;}for(const auto& E:Lease.Nodes)if(E.Object!=N&&E.Guid==N->NodeGuid){TI.Bad(TEXT("new_node_guid"),TEXT("unique GUID"),N->NodeGuid.ToString());return;}for(auto& E:Lease.Nodes)if(E.Object==N)E.Guid=N->NodeGuid;
        }
        for(auto& E:Lease.Nodes)for(auto& P:E.Pins)P.LinkIds.Reset();if(!Lease.SealLinks(Lease.Nodes)||!TI.Current(true))return;
        Count(R,TEXT("notify_graph_calls"));G->NotifyGraphChanged();if(!TI.Current(true))return;R.Stage=TEXT("timeline_structural");Count(R,TEXT("mark_structural_calls"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B);if(!TI.Reacquire())return;R.bChangeStateKnown=true;
        // Copy original NodeToJson(false) before the optional compiler/save prefix.
        TArray<TSharedPtr<FJsonValue>> Rows;
        for(auto* N:Imported)
        {
            if(!TI.Current())return;const FText Title=N->GetNodeTitle(ENodeTitleType::FullTitle);if(!TI.Current())return;const FString TitleString=Title.ToString();if(!TI.Current())return;
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("node_id"),N->NodeGuid.ToString());J->SetStringField(TEXT("name"),N->GetName());J->SetStringField(TEXT("class"),N->GetClass()->GetName());J->SetStringField(TEXT("node_class"),N->GetClass()->GetPathName());J->SetStringField(TEXT("title"),TitleString);J->SetNumberField(TEXT("x"),N->NodePosX);J->SetNumberField(TEXT("y"),N->NodePosY);J->SetStringField(TEXT("node_comment"),N->NodeComment);J->SetBoolField(TEXT("comment_bubble_visible"),N->bCommentBubbleVisible);J->SetBoolField(TEXT("comment_bubble_pinned"),N->bCommentBubblePinned);Rows.Add(MakeShared<FJsonValueObject>(J));Legacy->SetArrayField(TEXT("imported_nodes"),Rows);
        }
        if(Compile){R.Stage=TEXT("timeline_compile");R.bCompileAttempted=true;R.bChangeStateKnown=false;FCompilerResultsLog Log;Count(R,TEXT("full_compile_calls"));FKismetEditorUtilities::CompileBlueprint(B,EBlueprintCompileOptions::None,&Log);Count(R,TEXT("full_compile_returns"));R.Data->SetNumberField(TEXT("compiler_errors"),Log.NumErrors);R.Data->SetNumberField(TEXT("compiler_warnings"),Log.NumWarnings);if(!TI.Context())return;R.Compiled=Log.NumErrors==0&&B->Status!=BS_Error;auto Generated=B->GeneratedClass;const bool LegacyCompiled=Generated.GetGCPtr().IsResolved()&&Generated.GetGCPtr().Get()!=nullptr;Legacy->SetBoolField(TEXT("compiled"),LegacyCompiled);auto CR=MakeShared<FJsonObject>();CR->SetBoolField(TEXT("requested"),true);CR->SetBoolField(TEXT("success"),LegacyCompiled);CR->SetStringField(TEXT("blueprint"),Lease.BPPath);TStrongObjectPtr<UClass> HeldClass(Generated.GetGCPtr().IsResolved()?Generated.GetGCPtr().Get():nullptr);CR->SetStringField(TEXT("generated_class"),Loaded(HeldClass.Get())?HeldClass->GetPathName():FString());Legacy->SetObjectField(TEXT("compile_result"),CR);if(!TI.Reacquire())return;R.bChangeStateKnown=true;}
        // Preserve original requested save after a known failed compile; never after unknown.
        if(Save){R.Stage=TEXT("timeline_save");R.bSaveAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("forced_save_calls"));const bool Saved=UEditorAssetLibrary::SaveLoadedAsset(B,false);Count(R,TEXT("forced_save_returns"));R.bSaved=Saved;Legacy->SetBoolField(TEXT("saved"),Saved);if(!Saved){Lease.Stopped=true;Fail(R,TEXT("graph_transfer_save_failed"),TEXT("Forced SaveLoadedAsset returnedFalse; returned receipt retained, no further source SDK."));return;}if(!TI.Current(true))return;R.bChangeStateKnown=true;}
        Legacy->SetBoolField(TEXT("dirty_after"),B->GetOutermost()->IsDirty());R.Data->SetStringField(TEXT("new_template_guid"),TI.CreatedGuid.ToString());R.Data->SetStringField(TEXT("new_timeline_variable"),TI.NewName.ToString());R.Data->SetBoolField(TEXT("current_imported_nodes_verified"),true);R.Data->SetBoolField(TEXT("readonly_timeline_source_preserved"),!TI.MissingOrigin);R.Data->SetBoolField(TEXT("readonly_timeline_source_preservation_applicable"),!TI.MissingOrigin);R.Data->SetBoolField(TEXT("timeline_fallback_default_preserved"),TI.MissingOrigin);Terminal(R);
    };
    Apply();if(!Lease.Stopped&&R.bChangeStateKnown){const FString Stage=R.Stage;R.Stage=TEXT("timeline_transaction_publication");if(!TI.Current(true))return R;R.Data->SetBoolField(TEXT("transaction_publication_current_verified"),true);R.Stage=Stage;}return R;
}

Variable::FResult ImportCollapsed(const FString& Path,const FString& TextToImport,const FString& FilePath,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool Clear,bool KeepRoot,bool Allow,bool ValidateOnly,bool DryRun,bool Compile,bool Save)
{
    FResult R;Init(R,Compile,Save);R.Data->SetBoolField(TEXT("native_collapsed_graph_import"),true);
    R.Data->SetStringField(TEXT("finite_scope"),TEXT("Inline or once-read file source containing recursively owned Composite/BoundGraph tree (up to eight graph levels, 32 graphs, 256 objects), exact complementary Tunnel2 per graph with bounded exec/int32/bool ports, optional unlinked Self, multiple exact plain CallFunction nodes from loaded native KismetMathLibrary static pure callable by-value int32/bool signatures, stock Branch and ordered Sequence. Actual reciprocal topology includes scalar output fanout; function parameter/default/storage sources are checked before paste and after each callback. Loaded direct Actor K2 target/native Scene root and existing original clear/keep-root/partial/compile/save paths are preserved. Other function owners/types/ref/container/metadata expansion, larger trees or mixed top-level objects, custom sources and runtime execution remain valid incomplete scope."));
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

// Concrete import source selection only; no second paste, journal or transaction.
// A file's copied text is authoritative for this request and is never reread.
bool ReadImportFileSource(const FString& FilePath,FString& TextToImport,FResult& R)
{
    R.Stage=TEXT("read_import_file");R.Data->SetBoolField(TEXT("input_file_selected"),true);R.Data->SetStringField(TEXT("input_file_path"),FilePath);
    R.Data->SetStringField(TEXT("input_file_resolution_order"),TEXT("Physical file read occurs once before profile selection. Specialized successful text retains parse-before-target safety; generic cached file failures are returned only after original target/graph resolution, preserving generic error precedence."));
    if(!Text(FilePath,4096,R))return false;
    Count(R,TEXT("file_size_query_calls"));const int64 Bytes=IFileManager::Get().FileSize(*FilePath);R.Data->SetStringField(TEXT("file_size_bytes"),LexToString(Bytes));
    if(Bytes>static_cast<int64>(MaxText)*4)return Fail(R,TEXT("graph_transfer_file_bound"),TEXT("File byte bound exceeded before whole-file read; larger original files remain pending."));
    Count(R,TEXT("file_read_calls"));const bool Read=FFileHelper::LoadFileToString(TextToImport,*FilePath);Count(R,TEXT("file_read_returns"));R.Data->SetBoolField(TEXT("file_read_returned"),Read);R.Data->SetNumberField(TEXT("file_text_length"),TextToImport.Len());
    if(!Read)return Fail(R,TEXT("graph_transfer_file_read_failed"),TEXT("Original text_file_path read failed."));
    if(!Text(TextToImport,MaxText,R))return false;
    if(TextToImport.IsEmpty())return Fail(R,TEXT("graph_transfer_text_missing"),TEXT("Provide either text or text_file_path."));
    return true;
}
void AdoptImportFileSource(FResult& Target,const FResult& Source)
{
    auto Counts=Target.Data->GetObjectField(TEXT("native_call_counts"));const auto Before=Source.Data->GetObjectField(TEXT("native_call_counts"));
    for(const TCHAR* Key:{TEXT("file_size_query_calls"),TEXT("file_read_calls"),TEXT("file_read_returns")})Counts->SetNumberField(Key,Counts->GetNumberField(Key)+Before->GetNumberField(Key));
    for(const TCHAR* Key:{TEXT("input_file_selected"),TEXT("input_file_path"),TEXT("input_file_resolution_order"),TEXT("file_size_bytes"),TEXT("file_read_returned"),TEXT("file_text_length")})if(auto Value=Source.Data->TryGetField(Key))Target.Data->SetField(Key,Value);
    Target.Data->SetStringField(TEXT("input_file_stage"),Source.Stage);
    Target.Data->SetStringField(TEXT("input_file_error_code"),Source.ErrorCode);
    Target.Data->SetStringField(TEXT("input_file_error"),Source.Error);
    Target.Data->SetBoolField(TEXT("input_file_reread"),false);
}
Variable::FResult Import(const FString& Path,const FString& InlineText,const FString& FilePath,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool Clear,bool KeepRoot,bool Allow,bool ValidateOnly,bool DryRun,bool Compile,bool Save)
{
    FResult R;Init(R,Compile,Save);FString SelectedText=InlineText;
    const bool FileSelected=SelectedText.IsEmpty()&&!FilePath.IsEmpty();bool FileReady=true;
    FResult FileSource;
    if(FileSelected)
    {
        if(!IsEditorGameThread()||IsPlaySessionActive()){Fail(R,TEXT("graph_transfer_editor_invalid"),TEXT("Original graph import requires idle Editor game thread, no PIE/SIE."));return R;}
        Init(FileSource,Compile,Save);FileReady=ReadImportFileSource(FilePath,SelectedText,FileSource);
        // Cache both success and failure. A failed read does not establish a specialized profile.
        AdoptImportFileSource(R,FileSource);R.Data->SetBoolField(TEXT("input_file_source_ready"),FileReady);
    }
    // The original nonempty inline value always wins, even when a file path is supplied.
    if(FileReady&&IsTimelineText(SelectedText)){auto Result=ImportTimeline(Path,SelectedText,FilePath,GraphName,GraphId,GraphType,Clear,KeepRoot,Allow,ValidateOnly,DryRun,Compile,Save);if(FileSelected){AdoptImportFileSource(Result,FileSource);Result.Data->SetBoolField(TEXT("input_file_source_ready"),true);}return Result;}
    if(FileReady&&IsCollapsedText(SelectedText)){auto Result=ImportCollapsed(Path,SelectedText,FilePath,GraphName,GraphId,GraphType,Clear,KeepRoot,Allow,ValidateOnly,DryRun,Compile,Save);if(FileSelected){AdoptImportFileSource(Result,FileSource);Result.Data->SetBoolField(TEXT("input_file_source_ready"),true);}return Result;}
    if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("graph_transfer_editor_invalid"),TEXT("Original graph import requires idle Editor game thread, no PIE/SIE."));return R; }
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","ImportBlueprintGraphText","Import Blueprint Graph Text"));UBlueprint* BP=Load(Path,R,true);if(!BP) { return R; }const TStrongObjectPtr<UBlueprint> KeepBP(BP);FMacroGraphLease Macro(R);if(!Macro.Bind(BP))return R;
    if(BP->BlueprintType==BPTYPE_FunctionLibrary&&GraphName.IsEmpty()&&GraphId.IsEmpty()&&Normalize(GraphType).IsEmpty()){Fail(R,TEXT("graph_transfer_function_library_selector_required"),TEXT("FunctionLibrary import requires an explicit existing function selector; implicit EventGraph creation remains incomplete and was not attempted."));return R;}
    if(BP->BlueprintType==BPTYPE_MacroLibrary&&GraphName.IsEmpty()&&GraphId.IsEmpty()&&Normalize(GraphType).IsEmpty()){Fail(R,TEXT("graph_transfer_macro_library_selector_required"),TEXT("MacroLibrary import requires an explicit existing macro graph selector; implicit EventGraph creation remains incomplete and was not attempted."));return R;}
    TSharedRef<FJsonObject> BeforeResolve=MakeShared<FJsonObject>();if(!Capture(BP,TEXT("before_resolution"),R,BeforeResolve)) { return R; }UEdGraph* G=Resolve(BP,GraphName,GraphId,GraphType,R);if(!Macro.Current())return R;if(!G) { return R; }const TStrongObjectPtr<UEdGraph> KeepGraph(G);
    if(BP->BlueprintType==BPTYPE_FunctionLibrary&&!BP->FunctionGraphs.Contains(G)){Fail(R,TEXT("graph_transfer_function_library_target_pending"),TEXT("Only an explicitly resolved existing own FunctionGraphs member is admitted; no EventGraph/macro/delegate creation."));return R;}
    if(BP->BlueprintType==BPTYPE_MacroLibrary&&(!BP->MacroGraphs.Contains(G)||G->GetOuter()!=BP)){Fail(R,TEXT("graph_transfer_macro_library_target_pending"),TEXT("Only an explicitly resolved existing own MacroGraphs member is admitted; no implicit graph creation."));return R;}
    const FGuid Guid=G->GraphGuid;
    if(FileSelected&&!FileReady){R.Stage=FileSource.Stage;Fail(R,*FileSource.ErrorCode,FileSource.Error);return R;}
    FString TextToImport=SelectedText;if(!Text(TextToImport,MaxText,R)||!Text(FilePath,4096,R)) { return R; }if(TextToImport.IsEmpty()) { Fail(R,TEXT("graph_transfer_text_missing"),TEXT("Provide either text or text_file_path."));return R; }if(!Sample(BP,Allow,R)) { return R; }
    const bool Validate=ValidateOnly||DryRun;auto Legacy=MakeShared<FJsonObject>();Legacy->SetStringField(TEXT("command"),TEXT("import_blueprint_graph_nodes_from_text"));Legacy->SetStringField(TEXT("blueprint_name"),Path);Legacy->SetStringField(TEXT("blueprint_path"),BP->GetPathName());Legacy->SetStringField(TEXT("blueprint_package_path"),BP->GetOutermost()->GetName());Legacy->SetStringField(TEXT("graph_name"),G->GetName());Legacy->SetStringField(TEXT("graph_id"),Guid.ToString());Legacy->SetStringField(TEXT("graph_schema"),G->GetSchema()->GetClass()->GetName());Legacy->SetStringField(TEXT("text_file_path"),FilePath);Legacy->SetNumberField(TEXT("text_length"),TextToImport.Len());Legacy->SetBoolField(TEXT("validate_only"),Validate);Legacy->SetBoolField(TEXT("sample_or_temp_target"),R.Data->GetBoolField(TEXT("sample_or_temp_target")));Legacy->SetBoolField(TEXT("allow_non_sample"),Allow);Legacy->SetBoolField(TEXT("clear_graph"),Clear);Legacy->SetBoolField(TEXT("keep_root_node"),KeepRoot);Legacy->SetBoolField(TEXT("compile_requested"),Compile);Legacy->SetBoolField(TEXT("save_requested"),Save);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
    FStockSwitchImport Switch(R);FSelectKnotImport DataCluster(R);FOwnMacroInstanceImport OwnMacro(R);FString MacroAdmissionText;auto GraphSourcesCurrent=[&](){return Macro.Current()&&Switch.Current()&&DataCluster.Current()&&OwnMacro.Current();};
    if(!OwnMacro.Admit(BP,G,TextToImport,MacroAdmissionText)||!DataCluster.Admit(TextToImport)||!Switch.Admit(TextToImport)||!Admission(BP,OwnMacro.Active?MacroAdmissionText:TextToImport,R,DataCluster.Active,OwnMacro.Active)||!GraphSourcesCurrent()) { return R; }R.Stage=TEXT("can_import");Count(R,TEXT("can_import_calls"));const bool Can=FEdGraphUtilities::CanImportNodesFromText(G,TextToImport);Count(R,TEXT("can_import_returns"));if(!GraphSourcesCurrent())return R;Legacy->SetBoolField(TEXT("success"),Can);Legacy->SetBoolField(TEXT("can_import"),Can);R.Data->SetBoolField(TEXT("can_import_preflight_side_effect_free"),false);if(!Can) { Legacy->SetStringField(TEXT("error"),TEXT("FEdGraphUtilities::CanImportNodesFromText returned false"));Fail(R,TEXT("graph_transfer_cannot_import"),Legacy->GetStringField(TEXT("error")));return R; }
    TSharedRef<FJsonObject> Before=MakeShared<FJsonObject>();if(!Capture(BP,TEXT("before_import"),R,Before)||!GraphSourcesCurrent()) { return R; }if(Validate) { Legacy->SetNumberField(TEXT("imported_node_count"),0);Finish(BP,true,R,Legacy,[&](){return GraphSourcesCurrent();});if(R.ErrorCode.IsEmpty()&&!OwnMacro.FinalFiles())return R;Terminal(R);return R; }
    R.Stage=TEXT("apply_import");R.bMutationAttempted=true;R.bChangeStateKnown=false;R.bChanged=true;Count(R,TEXT("blueprint_modify_calls"));BP->Modify();if(!GraphSourcesCurrent())return R;Count(R,TEXT("graph_modify_calls"));G->Modify();if(!GraphSourcesCurrent())return R;int32 Removed=0;TArray<TSharedPtr<FJsonValue>> Removal;
    if(Clear)
    {
        TArray<TStrongObjectPtr<UEdGraphNode>> Old;for(UEdGraphNode* N:G->Nodes) { if(N) { Old.Emplace(N); } }
        UClass* AnimRoot=FindObject<UClass>(nullptr,TEXT("/Script/AnimGraph.AnimGraphNode_Root"));for(const auto& Held:Old) { auto* N=Held.Get();if(!GraphSourcesCurrent())return R;if(Macro.Active&&(!Loaded(N)||N->GetOuter()!=G||!G->Nodes.Contains(N)))return Macro.Stop(TEXT("Selected clear node owner/membership changed before its next getter.")),R;if(KeepRoot&&(N->IsA(UK2Node_FunctionEntry::StaticClass())||N->IsA(UK2Node_FunctionResult::StaticClass())||(Loaded(AnimRoot)&&N->IsA(AnimRoot)))) { continue; }auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("node_id"),N->NodeGuid.ToString());Q->SetStringField(TEXT("node_path"),N->GetPathName());Count(R,TEXT("node_modify_calls"));N->Modify();if(!GraphSourcesCurrent())return R;if(Macro.Active&&(!Loaded(N)||N->GetOuter()!=G||!G->Nodes.Contains(N)))return Macro.Stop(TEXT("Selected clear node changed owner/membership after Modify.")),R;Count(R,TEXT("remove_node_calls"));const bool Returned=G->RemoveNode(N);Count(R,TEXT("remove_node_returns"));Q->SetBoolField(TEXT("returned"),Returned);Removal.Add(MakeShared<FJsonValueObject>(Q));R.Data->SetArrayField(TEXT("removal_receipts"),Removal);++Removed;if(!GraphSourcesCurrent())return R; }
    }
    Legacy->SetNumberField(TEXT("removed_node_count"),Removed);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);TSet<UEdGraphNode*> Imported;Count(R,TEXT("import_nodes_calls"));FEdGraphUtilities::ImportNodesFromText(G,TextToImport,Imported);Count(R,TEXT("import_nodes_returns"));R.Data->SetNumberField(TEXT("imported_returned_count"),Imported.Num());if(Macro.Active){TArray<TSharedPtr<FJsonValue>> RawAddresses;for(auto* N:Imported)RawAddresses.Add(MakeShared<FJsonValueString>(LexToString(reinterpret_cast<UPTRINT>(N))));R.Data->SetArrayField(TEXT("macro_import_returned_addresses_unadopted"),RawAddresses);}if(!GraphSourcesCurrent())return R;if(!Switch.Adopt(G,Imported)||!DataCluster.Adopt(G,Imported)||!OwnMacro.Adopt(G,Imported))return R;TArray<TStrongObjectPtr<UEdGraphNode>> HoldImported;TArray<TSharedPtr<FJsonValue>> Returned;
    for(UEdGraphNode* N:Imported) { if(Macro.Active&&(!Loaded(N)||N->GetOuter()!=G||!G->Nodes.Contains(N)))return Macro.Stop(TEXT("Returned imported node no longer has the selected macro owner/membership; raw addresses retained without dereference.")),R;auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("returned_node_path"),GetPathNameSafe(N));Returned.Add(MakeShared<FJsonValueObject>(Q));R.Data->SetArrayField(TEXT("import_returned_prefix"),Returned);if(N) { HoldImported.Emplace(N);Count(R,TEXT("new_guid_calls"));N->CreateNewGuid();Q->SetStringField(TEXT("new_node_id"),N->NodeGuid.ToString()); } }
    Legacy->SetNumberField(TEXT("imported_node_count"),Imported.Num());Count(R,TEXT("notify_graph_calls"));G->NotifyGraphChanged();if(!GraphSourcesCurrent())return R;Count(R,TEXT("mark_structural_calls"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);if(!GraphSourcesCurrent())return R;R.Data->SetBoolField(TEXT("implicit_skeleton_regeneration_possible"),true);
    TSharedRef<FJsonObject> After=MakeShared<FJsonObject>();if(!Capture(BP,TEXT("after_import"),R,After)||!GraphSourcesCurrent()) { return R; }R.bChangeStateKnown=true;FIndex Current;if(!Index(BP,Current,R)) { R.bChangeStateKnown=false;return R; }TArray<TSharedPtr<FJsonValue>> Rows;bool Good=Imported.Num()>0;TSet<FGuid> NewIds;
    for(UEdGraphNode* N:Imported) { if(!N||!Current.Nodes.Contains(N)||N->GetGraph()!=G||!N->NodeGuid.IsValid()||NewIds.Contains(N->NodeGuid)) { Good=false;continue; }NewIds.Add(N->NodeGuid);Rows.Add(MakeShared<FJsonValueObject>(Node(N,Current)));if(!N->HasAnyFlags(RF_Transactional)||!N->GetClass()->HasAnyClassFlags(CLASS_Native)||!N->GetClass()->GetPathName().StartsWith(TEXT("/Script/BlueprintGraph."))) { Good=false; }for(UEdGraphPin* P:N->Pins) { if(!P) { continue; }if((P->DefaultObject&&!RuntimeObject(P->DefaultObject))||(P->PinType.PinSubCategoryObject.Get()&&!RuntimeObject(P->PinType.PinSubCategoryObject.Get()))) { Good=false; } }if(auto* Call=Cast<UK2Node_CallFunction>(N)) { Good&=RuntimeObject(Call->GetTargetFunction()); } }
    Legacy->SetArrayField(TEXT("imported_nodes"),Rows);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.Data->SetBoolField(TEXT("current_imported_nodes_verified"),Good);if(!Protected(Before,After,Guid,R)||!Good) { if(R.ErrorCode.IsEmpty()) { Fail(R,TEXT("graph_transfer_import_postcondition_failed"),TEXT("Returned imported node/class/type/reference/current graph proof incomplete; accepted clear/paste prefix retained, no rollback or compiler/save replay.")); }return R; }
    Finish(BP,false,R,Legacy,[&](){return GraphSourcesCurrent();});if(R.ErrorCode.IsEmpty()&&!OwnMacro.FinalFiles())return R;Terminal(R);return R;
}
Variable::FResult Repair(const FString& Path,bool Allow,bool ValidateOnly,bool DryRun,bool Compile,bool Save)
{
    FResult R;Init(R,Compile,Save);if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("graph_transfer_editor_invalid"),TEXT("Original repair requires idle Editor game thread, no PIE/SIE."));return R; }Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","RepairBlueprintPinLinks","Repair Blueprint Pin Links"));UBlueprint* BP=Load(Path,R,true,true);if(!BP) { return R; }const TStrongObjectPtr<UBlueprint> KeepBP(BP);FMacroGraphLease Macro(R);if(!Macro.Bind(BP))return R;if(!Sample(BP,Allow,R)) { return R; }const bool Validate=ValidateOnly||DryRun;TSharedRef<FJsonObject> Before=MakeShared<FJsonObject>();if(!Capture(BP,TEXT("before_repair"),R,Before)) { return R; }FIndex I;if(!Index(BP,I,R)||!Macro.BindPins(I)) { return R; }
    TArray<TStrongObjectPtr<UEdGraph>> KeepGraphs;TArray<TStrongObjectPtr<UEdGraphNode>> KeepNodes;for(UEdGraph* G:I.Graphs) { if(G) { KeepGraphs.Emplace(G); } }for(UEdGraphNode* N:I.Nodes) { KeepNodes.Emplace(N); }
    TMap<UEdGraph*,int32> GraphChanges;int32 Null=0,Duplicate=0,Self=0,External=0,Schema=0,Reciprocal=0,Skipped=0,ChangedPins=0;TArray<TSharedPtr<FJsonValue>> Summaries;auto MarkGraph=[&](UEdGraph* G){ if(G) { ++GraphChanges.FindOrAdd(G); } };
    R.Stage=Validate?TEXT("validate_links"):TEXT("repair_links");
    for(UEdGraph* G:I.Graphs)
    {
        if(!G) { continue; }int32 Visited=0,Changes=0;TArray<TObjectPtr<UEdGraphNode>> VisitNodes;if(Macro.Active)VisitNodes=G->Nodes;const auto& CurrentNodes=Macro.Active?VisitNodes:G->Nodes;for(const auto& Slot:CurrentNodes)
        {
            if(Macro.Active&&!Slot.IsResolved())return Macro.Stop(TEXT("Repair node slot became unresolved; no source loader.")),R;UEdGraphNode* N=Macro.Active?Cast<UEdGraphNode>(StaticFindObject(nullptr,nullptr,*Slot.GetPathName())):Slot.Get();if(Macro.Active&&(!Loaded(N)||Slot!=N))return Macro.Stop(TEXT("Repair node slot no longer matches its resolved held receiver.")),R;
            if(!N) { continue; }TArray<UEdGraphPin*> VisitPins;if(Macro.Active)VisitPins=N->Pins;const auto& CurrentPins=Macro.Active?VisitPins:N->Pins;for(UEdGraphPin* P:CurrentPins)
            {
                if(!P) { continue; }++Visited;bool Change=false;TSet<UEdGraphPin*> Seen;TArray<UEdGraphPin*> Clean;Clean.Reserve(P->LinkedTo.Num());TArray<UEdGraphPin*> VisitLinks;if(Macro.Active)VisitLinks=P->LinkedTo;const auto& CurrentLinks=Macro.Active?VisitLinks:P->LinkedTo;for(UEdGraphPin* L:CurrentLinks)
                {
                    if(!L) { ++Null;Change=true;continue; }if(L==P) { ++Self;Change=true;continue; }if(!I.Pins.Contains(L)) { ++External;Change=true;continue; }const UEdGraphSchema* ConnectionSchema=G->GetSchema();if(!Macro.PinsCurrent())return R;const bool Refused=Disallowed(ConnectionSchema,P,L,R);if(!Macro.PinsCurrent())return R;if(Refused) { ++Schema;Change=true;continue; }if(Seen.Contains(L)) { ++Duplicate;Change=true;continue; }Seen.Add(L);Clean.Add(L);
                }
                if(Change&&!Validate) { R.bMutationAttempted=R.bChanged=true;R.bChangeStateKnown=false;Count(R,TEXT("pin_modify_calls"));P->Modify();if(!Macro.PinsCurrent())return R;Count(R,TEXT("link_array_write_calls"));P->LinkedTo=Clean;MarkGraph(G);++Changes;++ChangedPins; }else if(Change) { ++Changes;++ChangedPins; }
            }
        }
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("graph_name"),G->GetName());J->SetStringField(TEXT("graph_id"),G->GraphGuid.ToString());J->SetNumberField(TEXT("pins_visited"),Visited);J->SetNumberField(TEXT("first_pass_changes"),Changes);Summaries.Add(MakeShared<FJsonValueObject>(J));
    }
    // Original set iteration and dry-run reading of unmodified first-pass arrays are retained.
    for(UEdGraphPin* P:I.Pins)
    {
        UEdGraph* PG=I.Owners.FindChecked(P)->GetGraph();TArray<UEdGraphPin*> VisitLinks;if(Macro.Active)VisitLinks=P->LinkedTo;const auto& CurrentLinks=Macro.Active?VisitLinks:P->LinkedTo;for(UEdGraphPin* L:CurrentLinks)
        {
            if(!L||!I.Pins.Contains(L)) { continue; }UEdGraph* LG=I.Owners.FindChecked(L)->GetGraph();if(!L->LinkedTo.Contains(P)) { const auto* S=PG?PG->GetSchema():LG->GetSchema();if(!Macro.PinsCurrent())return R;const bool Refused=Disallowed(S,P,L,R);if(!Macro.PinsCurrent())return R;if(Refused) { ++Skipped;continue; }++Reciprocal;MarkGraph(LG);if(!Validate) { R.bMutationAttempted=R.bChanged=true;R.bChangeStateKnown=false;Count(R,TEXT("pin_modify_calls"));L->Modify();if(!Macro.PinsCurrent())return R;Count(R,TEXT("reciprocal_add_calls"));L->LinkedTo.Add(P); }++ChangedPins; }if(LG!=PG) { MarkGraph(PG);MarkGraph(LG); }
        }
    }
    const bool Had=Null>0||Duplicate>0||Self>0||External>0||Schema>0||Reciprocal>0;if(Had&&!Validate) { Count(R,TEXT("blueprint_modify_calls"));BP->Modify();if(!Macro.Current())return R;for(const auto& Pair:GraphChanges) { if(Pair.Key) { Count(R,TEXT("graph_modify_calls"));Pair.Key->Modify();if(!Macro.Current())return R;Count(R,TEXT("notify_graph_calls"));Pair.Key->NotifyGraphChanged();if(!Macro.Current())return R; } }Count(R,TEXT("mark_modified_calls"));FBlueprintEditorUtils::MarkBlueprintAsModified(BP);if(!Macro.Current())return R; }
    auto Legacy=MakeShared<FJsonObject>();Legacy->SetBoolField(TEXT("success"),true);Legacy->SetStringField(TEXT("command"),TEXT("repair_blueprint_pin_links"));Legacy->SetBoolField(TEXT("validate_only"),Validate);Legacy->SetStringField(TEXT("blueprint_name"),Path);Legacy->SetStringField(TEXT("blueprint_path"),BP->GetPathName());Legacy->SetStringField(TEXT("blueprint_package_path"),BP->GetOutermost()->GetName());Legacy->SetBoolField(TEXT("sample_or_temp_target"),R.Data->GetBoolField(TEXT("sample_or_temp_target")));Legacy->SetBoolField(TEXT("allow_non_sample"),Allow);Legacy->SetBoolField(TEXT("compile_requested"),Compile);Legacy->SetBoolField(TEXT("save_requested"),Save);Legacy->SetNumberField(TEXT("graph_count"),I.Graphs.Num());Legacy->SetNumberField(TEXT("node_count"),I.NodeEntries);Legacy->SetNumberField(TEXT("pin_count"),I.PinEntries);Legacy->SetNumberField(TEXT("changed_pin_count"),ChangedPins);Legacy->SetNumberField(TEXT("removed_null_links"),Null);Legacy->SetNumberField(TEXT("removed_duplicate_links"),Duplicate);Legacy->SetNumberField(TEXT("removed_self_links"),Self);Legacy->SetNumberField(TEXT("removed_external_links"),External);Legacy->SetNumberField(TEXT("removed_schema_disallowed_links"),Schema);Legacy->SetNumberField(TEXT("added_reciprocal_links"),Reciprocal);Legacy->SetNumberField(TEXT("skipped_schema_disallowed_reciprocal_links"),Skipped);Legacy->SetBoolField(TEXT("had_changes"),Had);Legacy->SetArrayField(TEXT("graphs"),Summaries);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
    TSharedRef<FJsonObject> After=MakeShared<FJsonObject>();if(!Capture(BP,TEXT("after_repair"),R,After)) { return R; }R.bChangeStateKnown=true;if(Validate) { const bool Same=Equal(Before,After);R.Data->SetBoolField(TEXT("validation_owned_state_preserved"),Same);if(!Same) { Fail(R,TEXT("graph_transfer_validation_state_changed"),TEXT("Schema validation/current owned state differs; no apply, no fake known-zero total effect claim."));return R; }Finish(BP,true,R,Legacy,[&](){return Macro.Current();});Terminal(R);return R; }
    FIndex Current;if(!Index(BP,Current,R)||!HealthyLinks(Current,R)) { Fail(R,TEXT("graph_transfer_repair_postcondition_failed"),TEXT("Current owned link arrays still malformed; accepted repair prefix retained without retry."));return R; }if(!Before->GetStringField(TEXT("parent_class")).Equals(After->GetStringField(TEXT("parent_class")),ESearchCase::CaseSensitive)||!Equal(Before->GetObjectField(TEXT("member_declarations")).ToSharedRef(),After->GetObjectField(TEXT("member_declarations")).ToSharedRef())) { Fail(R,TEXT("graph_transfer_repair_source_changed"),TEXT("Member declarations/parent changed during repair callbacks; accepted prefix retained."));return R; }
    Finish(BP,false,R,Legacy,[&](){return Macro.Current();});Terminal(R);return R;
}
}
