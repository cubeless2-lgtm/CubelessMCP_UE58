// FJ: original Part8 GUID selection -> diagnostics -> real BlueprintEditor collapse.
// Editor-only source candidate. Owned derived toolkit, no Engine patch or base-to-derived cast.
#include "Compatibility/UE58/CubelessBlueprintFunctionExtractionApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "BlueprintEditor.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/MemberReference.h"
#include "Framework/Application/SlateApplication.h"
#include "GraphEditor.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_Composite.h"
#include "K2Node_Tunnel.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Internationalization/Text.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Misc/PackageName.h"
#include "Templates/UnrealTemplate.h"
#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace Cubeless::UE58::BlueprintFunctionExtraction
{
namespace
{
using Variable::FResult;
constexpr int32 MaxText=1048576, MaxValues=65536, MaxDepth=64;
constexpr int32 MaxSelection=2048, MaxGraphs=256, MaxNodes=4096, MaxPins=32768, MaxLinks=65536;
bool Busy=false;
// YD: the stock SDK makes FunctionLibrary function entries static and may reconstruct
// world-context/self gateway pins. Observe owner roles, not compiler cache or pin order.
bool DirectFunctionLibraryRole(UBlueprint* BP)
{
    return BP&&BP->BlueprintType==BPTYPE_FunctionLibrary
        &&BP->ParentClass.GetGCPtr().IsResolved()&&BP->ParentClass.GetGCPtr()==UBlueprintFunctionLibrary::StaticClass()
        &&BP->NewVariables.IsEmpty()&&BP->ImplementedInterfaces.IsEmpty()
        &&BP->MacroGraphs.IsEmpty()&&BP->UbergraphPages.IsEmpty()&&BP->EventGraphs.IsEmpty()
        &&BP->DelegateSignatureGraphs.IsEmpty()&&BP->ComponentTemplates.IsEmpty()&&BP->Timelines.IsEmpty()
        &&BP->SimpleConstructionScript.IsResolved()&&BP->SimpleConstructionScript==nullptr;
}
bool Loaded(const UObject* O)
{
    return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);
}
bool Fail(FResult& R,const TCHAR* Code,const FString& Error)
{
    R.bSuccess=R.bPostconditionPassed=false;R.ErrorCode=Code;R.Error=Error;
    bool Applied=false;R.Data->TryGetBoolField(TEXT("mutation_applied"),Applied);
    R.Data->SetStringField(TEXT("validation_phase"),Applied?TEXT("mutation"):TEXT("pre_mutation"));return false;
}
void Count(FResult& R,const TCHAR* Key)
{
    const auto C=R.Data->GetObjectField(TEXT("native_call_counts"));C->SetNumberField(Key,C->GetNumberField(Key)+1);
}
bool Text(const FString& S,int32 Limit,FResult& R)
{
    if(S.Len()>Limit) { return Fail(R,TEXT("function_extract_text_bound"),TEXT("Input exceeds the finite UTF16 text bound.")); }
    for(TCHAR Ch:S) { if(!Ch) { return Fail(R,TEXT("function_extract_nul"),TEXT("Embedded NUL is not supported.")); } }
    return true;
}
bool Parse(const FString& S,TSharedPtr<FJsonValue>& V,FResult& R)
{
    if(!Text(S,MaxText,R)) { return false; }
    bool Quoted=false,Escaped=false;int32 Depth=0;
    for(TCHAR Ch:S)
    {
        if(Quoted) { if(Escaped) { Escaped=false; }else if(Ch==TEXT('\\')) { Escaped=true; }else if(Ch==TEXT('"')) { Quoted=false; } }
        else if(Ch==TEXT('"')) { Quoted=true; }
        else if(Ch==TEXT('[')||Ch==TEXT('{')) { if(++Depth>MaxDepth) { return Fail(R,TEXT("function_extract_json_depth"),TEXT("JSON nesting exceeds 64 before deserialization.")); } }
        else if(Ch==TEXT(']')||Ch==TEXT('}')) { --Depth; }
    }
    TArray<TSharedPtr<FJsonValue>> Root;
    const auto Reader=TJsonReaderFactory<>::Create(FString(TEXT("["))+S+TEXT("]"));
    if(!FJsonSerializer::Deserialize(Reader,Root,FJsonSerializer::EFlags::None)||Root.Num()!=1||!Root[0].IsValid())
    { return Fail(R,TEXT("function_extract_json_invalid"),TEXT("Expected exactly one valid JSON value.")); }
    V=Root[0];TArray<TSharedPtr<FJsonValue>> Stack;Stack.Add(V);int32 Slots=0;
    while(!Stack.IsEmpty())
    {
        auto Item=Stack.Pop(EAllowShrinking::No);
        if(!Item.IsValid()||++Slots>MaxValues) { return Fail(R,TEXT("function_extract_json_budget"),TEXT("JSON exceeds the 65536 value-slot bound.")); }
        if(Item->Type==EJson::Number&&!FMath::IsFinite(Item->AsNumber())) { return Fail(R,TEXT("function_extract_json_number"),TEXT("Non-finite JSON numbers are unsupported.")); }
        if(Item->Type==EJson::Array) { for(const auto& Child:Item->AsArray()) { Stack.Add(Child); } }
        else if(Item->Type==EJson::Object) { for(const auto& Pair:Item->AsObject()->Values) { Stack.Add(Pair.Value); } }
        if(Stack.Num()>MaxValues-Slots) { return Fail(R,TEXT("function_extract_json_budget"),TEXT("JSON exceeds the finite value-slot bound.")); }
    }
    return true;
}
bool ToolingReference(const FString& S)
{
    const FString L=S.ToLower();
    for(const TCHAR* PluginName:{TEXT("CubelessMCP_UE58"),TEXT("ModelContextProtocol"),TEXT("ToolsetRegistry"),TEXT("PythonScriptPlugin"),TEXT("EditorScriptingUtilities")})
    {
        if(L.StartsWith(FString(TEXT("/"))+FString(PluginName).ToLower()+TEXT("/"))) { return true; }
        if(const auto Plugin=IPluginManager::Get().FindPlugin(PluginName))
        {
            for(const auto& Module:Plugin->GetDescriptor().Modules)
            { if(L.StartsWith(FString(TEXT("/script/"))+Module.Name.ToString().ToLower())) { return true; } }
        }
    }
    for(const TCHAR* P:{TEXT("/script/unrealmcp"),TEXT("/script/cubelessmcp"),TEXT("/script/cubelesscontrolrigtools"),TEXT("/script/mcp_unreal"),TEXT("/script/mcpunreal"),TEXT("/script/mcpgameproject"),TEXT("/script/unrealmcpgame"),TEXT("/script/toolsetregistry"),TEXT("/script/modelcontextprotocol"),TEXT("/script/pythonscriptplugin")})
    { if(L.StartsWith(P)) { return true; } }
    return false;
}
FString NormalizeType(FString S)
{
    S=S.ToLower().Replace(TEXT(" "),TEXT("")).Replace(TEXT("-"),TEXT("_"));
    if(S==TEXT("eventgraph")||S==TEXT("event_graph")||S==TEXT("ubergraph")||S==TEXT("uber_graph")) { return TEXT("event"); }
    if(S==TEXT("functiongraph")||S==TEXT("function_graph")) { return TEXT("function"); }
    if(S==TEXT("macrograph")||S==TEXT("macro_graph")) { return TEXT("macro"); }return S;
}
FString GraphKind(const UBlueprint* BP,const UEdGraph* G)
{
    if(BP->FunctionGraphs.Contains(G)) { return TEXT("function"); }if(BP->MacroGraphs.Contains(G)) { return TEXT("macro"); }
    if(BP->UbergraphPages.Contains(G)||BP->EventGraphs.Contains(G)) { return TEXT("event"); }
    if(BP->DelegateSignatureGraphs.Contains(G)) { return TEXT("delegate"); }return TEXT("unknown");
}
FString Status(EBlueprintStatus S)
{
    switch(S)
    {
    case BS_Unknown:return TEXT("unknown");case BS_Dirty:return TEXT("dirty");case BS_Error:return TEXT("error");
    case BS_UpToDate:return TEXT("up_to_date");case BS_BeingCreated:return TEXT("being_created");case BS_UpToDateWithWarnings:return TEXT("up_to_date_with_warnings");
    default:return TEXT("unknown");
    }
}
// UBlueprint::GetAllGraphs order, with explicit depth/cycle bounds before SDK recursive use.
bool Graphs(UBlueprint* BP,TArray<UEdGraph*>& Out,FResult& R)
{
    TArray<UEdGraph*> Roots;
    for(const auto& A:{&BP->FunctionGraphs,&BP->MacroGraphs,&BP->UbergraphPages,&BP->DelegateSignatureGraphs})
    { for(UEdGraph* G:*A) { if(G) { Roots.Add(G); } } }
    for(const auto& Interface:BP->ImplementedInterfaces) { for(UEdGraph* G:Interface.Graphs) { if(G) { Roots.Add(G); } } }
    // Extension::GetAllGraphs is virtual and can manufacture/mutate graph inventories.
    if(!BP->GetExtensions().IsEmpty()) { return Fail(R,TEXT("function_extract_extensions_pending"),TEXT("Blueprint extension graph inventories require a separate callback-aware scope.")); }
    if(Roots.Num()>MaxGraphs) { return Fail(R,TEXT("function_extract_graph_budget"),TEXT("Blueprint root graphs exceed 256.")); }
    struct FVisit { UEdGraph* G;int32 Depth; };
    TArray<FVisit> Stack;TSet<UEdGraph*> Seen;
    for(int32 I=Roots.Num()-1;I>=0;--I) { Stack.Add({Roots[I],1}); }
    while(!Stack.IsEmpty())
    {
        const auto V=Stack.Pop(EAllowShrinking::No);
        if(!Loaded(V.G)||V.G->GetClass()!=UEdGraph::StaticClass()||V.Depth>MaxDepth||Seen.Contains(V.G)||Out.Num()>=MaxGraphs)
        { return Fail(R,TEXT("function_extract_graph_inventory"),TEXT("Graph hierarchy must be loaded, stock, acyclic and bounded before SDK recursion.")); }
        Seen.Add(V.G);Out.Add(V.G);
        if(V.G->SubGraphs.Num()>MaxGraphs-Out.Num()-Stack.Num()) { return Fail(R,TEXT("function_extract_graph_budget"),TEXT("Graph hierarchy exceeds 256 entries.")); }
        for(int32 I=V.G->SubGraphs.Num()-1;I>=0;--I) { if(V.G->SubGraphs[I]) { Stack.Add({V.G->SubGraphs[I],V.Depth+1}); } }
    }
    return true;
}
struct FIdentity
{
    UObject* Object=nullptr;UClass* Class=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;FString Path;
    TStrongObjectPtr<UObject> Keep;TStrongObjectPtr<UClass> KeepClass;TStrongObjectPtr<UPackage> KeepPackage;
    void Capture(UObject* O)
    {
        Object=O;Class=O->GetClass();Outer=O->GetOuter();Package=O->GetPackage();Path=O->GetPathName();
        Keep.Reset(O);KeepClass.Reset(Class);KeepPackage.Reset(Package);
    }
    bool Current() const
    {
        return Loaded(Object)&&Loaded(Class)&&Loaded(Package)&&Object->GetClass()==Class&&Object->GetOuter()==Outer
            &&Object->GetPackage()==Package&&Object->GetPathName()==Path;
    }
};
struct FPin
{
    UEdGraphPin* Pin=nullptr;UEdGraphNode* Owner=nullptr;FGuid Id;FName Name;
    EEdGraphPinDirection Direction=EGPD_MAX;FEdGraphPinType Type;
    TArray<UEdGraphPin*> Links,SubPins;UEdGraphPin* Parent=nullptr;bool Orphaned=false;
    TStrongObjectPtr<UObject> KeepType,KeepMemberParent;
};
struct FNode
{
    FIdentity Identity;UEdGraphNode* Node=nullptr;FGuid Id;int32 X=0,Y=0;TArray<UEdGraphPin*> Pins;
};
struct FInventory
{
    TArray<TUniquePtr<FNode>> Nodes;TArray<FPin> Pins;TSet<UEdGraphPin*> PinSet;
    bool Capture(UEdGraph* G,FResult& R)
    {
        if(G->Nodes.Num()>MaxNodes) { return Fail(R,TEXT("function_extract_node_budget"),TEXT("Source graph exceeds 4096 nodes.")); }
        TSet<UEdGraphNode*> Seen;int32 LinkCount=0;
        for(UEdGraphNode* N:G->Nodes)
        {
            if(!Loaded(N)||Seen.Contains(N)||N->GetOuter()!=G||N->GetGraph()!=G||N->GetPackage()!=G->GetPackage()||ToolingReference(N->GetClass()->GetPathName()))
            { return Fail(R,TEXT("function_extract_node_identity"),TEXT("Expected unique loaded nodes owned by the captured source graph/package.")); }
            const FString NodeModule=N->GetClass()->GetPackage()->GetName();
            if(NodeModule!=TEXT("/Script/BlueprintGraph")&&NodeModule!=TEXT("/Script/Engine"))
            { return Fail(R,TEXT("function_extract_custom_node_pending"),TEXT("Custom/plugin node virtual overrides require a separate extraction scope; only stock BlueprintGraph/Engine node classes are admitted.")); }
            Seen.Add(N);
            auto Row=MakeUnique<FNode>();Row->Node=N;Row->Identity.Capture(N);Row->Id=N->NodeGuid;Row->X=N->NodePosX;Row->Y=N->NodePosY;
            if(N->Pins.Num()>MaxPins-Pins.Num()) { return Fail(R,TEXT("function_extract_pin_budget"),TEXT("Source pin inventory exceeds 32768.")); }
            for(UEdGraphPin* P:N->Pins)
            {
                // Only owning arrays are dereferenced; raw external LinkedTo pointers are validated after full inventory.
                if(!P||PinSet.Contains(P)||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||P->LinkedTo.Num()>MaxLinks-LinkCount)
                { return Fail(R,TEXT("function_extract_pin_identity"),TEXT("Invalid/duplicate owning pin or exceeded 65536 link entries.")); }
                FPin Entry;Entry.Pin=P;Entry.Owner=N;Entry.Id=P->PinId;Entry.Name=P->PinName;Entry.Direction=P->Direction;
                Entry.Type=P->PinType;Entry.KeepType.Reset(P->PinType.PinSubCategoryObject.Get());Entry.KeepMemberParent.Reset(P->PinType.PinSubCategoryMemberReference.MemberParent);Entry.Links=P->LinkedTo;Entry.SubPins=P->SubPins;Entry.Parent=P->ParentPin;Entry.Orphaned=P->bOrphanedPin;
                if(P->PinType.PinSubCategoryObject.IsValid()&&ToolingReference(P->PinType.PinSubCategoryObject->GetPathName())) { return Fail(R,TEXT("function_extract_runtime_dependency"),TEXT("Function signature cannot lift Cubeless/MCP tooling pin types into a game graph.")); }
                LinkCount+=Entry.Links.Num();Pins.Add(MoveTemp(Entry));PinSet.Add(P);Row->Pins.Add(P);
            }
            Nodes.Add(MoveTemp(Row));
        }
        for(const auto& P:Pins)
        {
            if(P.Parent&&(!PinSet.Contains(P.Parent)||P.Parent->GetOwningNodeUnchecked()!=P.Owner||!P.Parent->SubPins.Contains(P.Pin)))
            { return Fail(R,TEXT("function_extract_split_pin"),TEXT("Split pin parent is outside the captured owner inventory.")); }
            if(P.SubPins.Num()>MaxPins) { return Fail(R,TEXT("function_extract_split_pin"),TEXT("Split pin child slots exceed the finite bound.")); }
            for(UEdGraphPin* Sub:P.SubPins)
            { if(!PinSet.Contains(Sub)||Sub->GetOwningNodeUnchecked()!=P.Owner||Sub->ParentPin!=P.Pin) { return Fail(R,TEXT("function_extract_split_pin"),TEXT("Split pin child identity is not reciprocal.")); } }
            TSet<UEdGraphPin*> Parents;UEdGraphPin* Walk=P.Pin;
            while(Walk) { if(Parents.Contains(Walk)||Parents.Num()>=MaxDepth) { return Fail(R,TEXT("function_extract_split_depth"),TEXT("Split-pin parent recursion is cyclic or exceeds 64.")); }Parents.Add(Walk);Walk=Walk->ParentPin; }
            for(UEdGraphPin* L:P.Links)
            {
                if(!PinSet.Contains(L)||L==P.Pin||!L->LinkedTo.Contains(P.Pin))
                { return Fail(R,TEXT("function_extract_link_identity"),TEXT("Collapse requires reciprocal links between captured owning pins; external/null/self links remain unsupported.")); }
            }
        }
        return true;
    }
    bool Same(UEdGraph* G,FResult& R) const
    {
        if(G->Nodes.Num()!=Nodes.Num()) { return Fail(R,TEXT("function_extract_callback_changed_graph"),TEXT("Source node slots changed during a validation callback.")); }
        for(int32 I=0;I<Nodes.Num();++I)
        {
            const auto& N=*Nodes[I];
            if(G->Nodes[I]!=N.Node||!N.Identity.Current()||N.Node->NodeGuid!=N.Id||N.Node->NodePosX!=N.X||N.Node->NodePosY!=N.Y||N.Node->Pins.Num()!=N.Pins.Num())
            { return Fail(R,TEXT("function_extract_callback_changed_node"),TEXT("Captured source node identity/layout changed during a callback.")); }
            for(int32 J=0;J<N.Pins.Num();++J) { if(N.Node->Pins[J]!=N.Pins[J]) { return Fail(R,TEXT("function_extract_callback_changed_pins"),TEXT("Owning pin slots changed during a callback.")); } }
        }
        for(const auto& P:Pins)
        {
            if(P.Pin->WasTrashed()||P.Pin->GetOwningNodeUnchecked()!=P.Owner||P.Pin->PinId!=P.Id||P.Pin->PinName!=P.Name
                ||P.Pin->Direction!=P.Direction||P.Pin->PinType!=P.Type||P.Pin->LinkedTo!=P.Links||P.Pin->SubPins!=P.SubPins||P.Pin->ParentPin!=P.Parent||P.Pin->bOrphanedPin!=P.Orphaned)
            { return Fail(R,TEXT("function_extract_callback_changed_pin"),TEXT("Captured pin shape/link slots changed during a callback.")); }
        }
        return true;
    }
};
// CheckIfNodeConnectsToSelection recursively walks non-exec upstream edges without a visited set.
// Bound exactly the branches it can enter; selected targets short-circuit before recursion.
bool SafeCycleQueries(const FInventory& I,const TSet<UEdGraphNode*>& Selection,FResult& R)
{
    TMap<UEdGraphNode*,TArray<UEdGraphNode*>> Up;
    for(const auto& P:I.Pins)
    {
        if(P.Direction!=EGPD_Input||P.Type.PinCategory==UEdGraphSchema_K2::PC_Exec) { continue; }
        for(UEdGraphPin* L:P.Links) { Up.FindOrAdd(P.Owner).Add(L->GetOwningNodeUnchecked()); }
    }
    int32 Work=0;
    struct FFrame { UEdGraphNode* Node;int32 Next; };
    for(UEdGraphNode* Selected:Selection)
    {
        const auto* Inputs=Up.Find(Selected);if(!Inputs) { continue; }
        for(UEdGraphNode* Root:*Inputs)
        {
            if(Selection.Contains(Root)) { continue; }
            TArray<FFrame> Stack;TSet<UEdGraphNode*> Active;Stack.Add({Root,0});Active.Add(Root);
            while(!Stack.IsEmpty())
            {
                auto& Frame=Stack.Last();const auto* Children=Up.Find(Frame.Node);
                if(!Children||Frame.Next>=Children->Num()) { Active.Remove(Frame.Node);Stack.Pop(EAllowShrinking::No);continue; }
                if(++Work>MaxLinks) { return Fail(R,TEXT("function_extract_cycle_work_bound"),TEXT("SDK recursive cycle query exceeds the finite 65536 edge work budget.")); }
                UEdGraphNode* Child=(*Children)[Frame.Next++];
                if(Selection.Contains(Child)) { return true; } // SDK returns true from the whole selection query; no later branch executes.
                if(Active.Contains(Child)||Stack.Num()>=MaxDepth)
                { return Fail(R,TEXT("function_extract_recursive_cycle_unsupported"),TEXT("SDK upstream cycle/depth would recurse without a visited set; dispatch is refused before the diagnostic call.")); }
                Active.Add(Child);Stack.Add({Child,0});
            }
        }
    }
    return true;
}
// PX: one pre-existing closed Composite is moved by the existing exported SDK extraction.
// This is a request-local role/expected-value extension, not a replacement extractor.
struct FPXComposite
{
    FResult& R;bool Stopped=false,Ready=false,Moved=false,Rebinding=false,KeepReturn=false;UEdGraph* Destination=nullptr;UEdGraphNode* Gateway=nullptr;
    TStrongObjectPtr<UBlueprint> BP;TStrongObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;
    TStrongObjectPtr<UK2Node_Composite> Composite;TStrongObjectPtr<UEdGraph> Source,Bound;
    FIdentity EditorRole,WorldRole,BlueprintRole,SourceRole;
    TStrongObjectPtr<USimpleConstructionScript> SCS;TStrongObjectPtr<USCS_Node> RootNode;TStrongObjectPtr<USceneComponent> RootTemplate;FGuid RootGuid;FName RootName;FVector RootLocation,RootScale;FRotator RootRotation;EComponentMobility::Type RootMobility=EComponentMobility::Movable;
    TStrongObjectPtr<USceneComponent> CandidateRoot;FString RootPath;FName TemplateName;
    TStrongObjectPtr<UClass> Generated,Skeleton,CandidateGenerated,CandidateSkeleton;FString GeneratedPath,SkeletonPath;UClass* GeneratedMeta=nullptr;UClass* SkeletonMeta=nullptr;
    TArray<UEdGraphNode*> SourceSlots,DestinationSlots,Selected;
    TStrongObjectPtr<UK2Node_CustomEvent> EventTemplate;
    struct FEventPort { FName Name;FEdGraphPinType Type;FGuid SourcePin;UEdGraphNode* Peer=nullptr;FGuid PeerPin;FString OriginalDefault;FName SourceName;int32 SourceConnection=0; };
    TArray<FEventPort> EventPorts;
    struct FCompositeRole { TStrongObjectPtr<UK2Node_Composite> Node;TStrongObjectPtr<UEdGraph> Graph;UEdGraph* ParentGraph=nullptr;TArray<UEdGraphNode*> Slots;int32 Depth=0;bool Selected=false; };
    TArray<FCompositeRole> Composites;TArray<UEdGraph*> MovedSubgraphs;
    struct FBoundary { UEdGraphNode* Node=nullptr;FGuid Pin;FName Name;EEdGraphPinDirection Direction=EGPD_MAX;FEdGraphPinType Type; };
    TArray<FBoundary> Boundary;TArray<TPair<UEdGraphNode*,FGuid>> AutoEntry,AutoReturn;
    bool IsSelected(UEdGraphNode* N)const{return Selected.Contains(N);}
    const FBoundary* Port(UEdGraphNode* N,const FGuid& Id)const{return Boundary.FindByPredicate([&](const FBoundary& V){return V.Node==N&&V.Pin==Id;});}
    struct FField { FProperty* P=nullptr;FFieldClass* Class=nullptr;FName Name;uint64 Flags=0;int32 Offset=0,Size=0;uint8 BoolOffset=0,BoolByteMask=0,BoolFieldMask=0; };
    struct FNativeSource { TStrongObjectPtr<UClass> Owner;TStrongObjectPtr<UFunction> Function;TStrongObjectPtr<UObject> CDO;FIdentity ClassIdentity,FunctionIdentity,CDOIdentity;EClassFlags ClassFlags=CLASS_None;UClass* Super=nullptr;EFunctionFlags Flags=FUNC_None;uint8 NumParms=0;uint16 ParmsSize=0;TArray<uint8> Script;TArray<FField> Fields;TMap<FName,FEdGraphPinType> Pins;FString Metadata; };
    TArray<FNativeSource> NativeSources;
    struct FGraph { TStrongObjectPtr<UEdGraph> Hold;FGuid Guid;FName Name;TArray<UEdGraph*> Children; };TArray<FGraph> Graphs;
    TArray<UEdGraph*> Functions,Macros,Events,Delegates;
    struct FValue
    {
        FGuid Id;FName Name;EEdGraphPinDirection Direction=EGPD_MAX;FEdGraphPinType Type;
        FString Raw,Auto,TextDisplay,FriendlyDisplay;FText Text,Friendly;UObject* Object=nullptr;UObject* TypeObject=nullptr;UObject* TerminalObject=nullptr;UObject* MemberParent=nullptr;
        uint32 TextFlags=0,FriendlyFlags=0;bool Hidden=false,Orphan=false,Connectable=false,ReadOnly=false;FString OwnTypePath;
        TArray<TPair<FGuid,FGuid>> Links;
    };
    struct FPurityValue
    {
        FEnumProperty* Property=nullptr;FNumericProperty* Storage=nullptr;
        int32 Offset=0,InnerOffset=0,ClassSize=0;uint64 Flags=0,InnerFlags=0;int8 Value=0;
    };
    struct FNodeRole
    {
        TStrongObjectPtr<UEdGraphNode> Hold;UClass* Class=nullptr;UEdGraph* Graph=nullptr;FName Name;FGuid Guid;int32 X=0,Y=0;
        FString Comment;TArray<FValue> Values;TArray<UEdGraphPin*> Physical;bool Fresh=false,Derived=false;
        bool Tunnel=false,Inputs=false,Outputs=false;UK2Node_Tunnel* Sink=nullptr;UK2Node_Tunnel* Output=nullptr;
        bool Editable=false,CustomEvent=false;FName EventName;uint32 EventFlags=0;
        TArray<FUserPinInfo> UserPins;FMemberReference Reference;UObject* ReferenceOwner=nullptr;bool HasReference=false;
        FPurityValue Purity;bool HasPurity=false;
    };
    TArray<FNodeRole> Nodes;TArray<TStrongObjectPtr<UObject>> Holds;
    TMap<UEdGraphNode*,TArray<UEdGraphPin*>> Candidate;TMap<UEdGraphPin*,UEdGraphNode*> OwnedPins;
    UEdGraph* Diagnostic=nullptr;bool DiagnosticMember=false;
    explicit FPXComposite(FResult& In):R(In){}
    static bool Type(const FEdGraphPinType& A,const FEdGraphPinType& B)
    {return A==B&&A.bIsUObjectWrapper==B.bIsUObjectWrapper&&A.bSerializeAsSinglePrecisionFloat==B.bSerializeAsSinglePrecisionFloat&&A.PinValueType.bTerminalIsUObjectWrapper==B.PinValueType.bTerminalIsUObjectWrapper;}
    bool Stop(const TCHAR* Why)
    {
        if(!Stopped){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("reason"),Why);J->SetBoolField(TEXT("operands_observed"),false);R.Data->SetObjectField(TEXT("composite_first_current_failure"),J);}
        Stopped=true;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("composite_followup_sdk_stopped"),true);
        return Fail(R,TEXT("function_extract_composite_current_lost"),TEXT("Composite extraction source/current changed; actual prefix retained without SDK cleanup or automatic retry."));
    }
    static bool Slots(const TArray<TObjectPtr<UEdGraph>>& A,const TArray<UEdGraph*>& B)
    {if(A.Num()!=B.Num())return false;for(int32 I=0;I<A.Num();++I)if(!A[I].IsResolved()||A[I].Get()!=B[I])return false;return true;}
    static TArray<UEdGraph*> Copy(const TArray<TObjectPtr<UEdGraph>>& A)
    {TArray<UEdGraph*> B;for(const auto& P:A)B.Add(P.Get());return B;}
    static bool PrimitiveField(const FProperty* P)
    {const auto* C=P->GetClass();return C==FBoolProperty::StaticClass()||C==FIntProperty::StaticClass()||C==FInt64Property::StaticClass()||C==FFloatProperty::StaticClass()||C==FDoubleProperty::StaticClass()||C==FNameProperty::StaticClass()||C==FStrProperty::StaticClass()||C==FTextProperty::StaticClass();}
    static bool ReferenceParent(const FMemberReference& Ref,UObject*& Out)
    {
        auto* S=FMemberReference::StaticStruct();auto* P=FindFProperty<FObjectProperty>(S,TEXT("MemberParent"));
        if(!P||P->GetClass()!=FObjectProperty::StaticClass()||P->GetOwnerStruct()!=S||P->ArrayDim!=1||P->HasSetterOrGetter()||P->GetSize()!=sizeof(TObjectPtr<UObject>))return false;
        const auto* Slot=P->ContainerPtrToValuePtr<TObjectPtr<UObject>>(&Ref);if(!Slot||!Slot->IsResolved())return false;Out=Slot->Get();return true;
    }
    static bool WeakReadable(const TWeakObjectPtr<UObject>& W){return W.IsExplicitlyNull()||W.IsValid();}
    bool HoldType(const FEdGraphPinType& T)
    {
        if(!WeakReadable(T.PinSubCategoryObject)||!WeakReadable(T.PinValueType.TerminalSubCategoryObject)||!T.PinSubCategoryMemberReference.MemberParent.IsResolved())return false;
        for(auto* O:{T.PinSubCategoryObject.Get(),T.PinValueType.TerminalSubCategoryObject.Get(),T.PinSubCategoryMemberReference.MemberParent.Get()})if(O){if(!Loaded(O))return false;Holds.Emplace(O);}return true;
    }
    static const FMemberReference* NodeReference(const UEdGraphNode* N)
    {if(const auto* C=Cast<UK2Node_CallFunction>(N))return &C->FunctionReference;if(const auto* E=Cast<UK2Node_Event>(N))return &E->EventReference;if(const auto* Entry=Cast<UK2Node_FunctionEntry>(N))return &Entry->FunctionReference;if(const auto* Result=Cast<UK2Node_FunctionResult>(N))return &Result->FunctionReference;return nullptr;}
    static FString SourceMetadata(const FNativeSource& S)
    {
        FString V;for(const auto& P:S.Fields){const FName K(*(TEXT("CPP_Default_")+P.Name.ToString()));const FString Cell=S.Function->GetMetaData(K);V+=K.ToString()+TEXT(":")+LexToString(S.Function->HasMetaData(K))+TEXT(":")+LexToString(Cell.Len())+TEXT(":")+Cell;}
        for(const TCHAR* K:{TEXT("Latent"),TEXT("BlueprintInternalUseOnly"),TEXT("DeprecatedFunction"),TEXT("CustomStructureParam"),TEXT("ArrayParm"),TEXT("AutoCreateRefTerm"),TEXT("CommutativeAssociativeBinaryOperator")}){const FString Cell=S.Function->GetMetaData(K);V+=FString(K)+TEXT(":")+LexToString(S.Function->HasMetaData(K))+TEXT(":")+LexToString(Cell.Len())+TEXT(":")+Cell;}return V;
    }
    bool MathCurrent()const
    {
        for(const auto& S:NativeSources)
        {
            if(!S.ClassIdentity.Current()||!S.FunctionIdentity.Current()||!Loaded(S.Owner.Get())||!Loaded(S.Function.Get())||S.Owner->GetClassFlags()!=S.ClassFlags||S.Owner->GetSuperClass()!=S.Super||!S.Owner->ClassGeneratedBy.IsResolved()||S.Owner->ClassGeneratedBy.Get()!=nullptr||S.Function->GetClass()!=UFunction::StaticClass()||S.Function->GetOuter()!=S.Owner.Get()||S.Function->FunctionFlags!=S.Flags||S.Function->NumParms!=S.NumParms||S.Function->ParmsSize!=S.ParmsSize||S.Function->Script!=S.Script||S.Owner->FindFunctionByName(S.Function->GetFName(),EIncludeSuperFlag::ExcludeSuper)!=S.Function.Get())return false;
            if(!S.Owner->ClassDefaultObject.IsResolved()||S.Owner->ClassDefaultObject.Get()!=S.CDO.Get()||!S.CDOIdentity.Current()||S.CDO->GetClass()!=S.Owner.Get()||S.CDO->GetOuter()!=S.Owner->GetOuter()||S.CDO->GetPackage()!=S.Owner->GetPackage()||!S.CDO->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject))return false;
            TArray<FProperty*> Now;for(TFieldIterator<FProperty> It(S.Function.Get(),EFieldIterationFlags::None);It;++It)Now.Add(*It);if(Now.Num()!=S.Fields.Num())return false;
            for(int32 I=0;I<Now.Num();++I){const auto& F=S.Fields[I];if(Now[I]!=F.P||F.P->GetClass()!=F.Class||!PrimitiveField(F.P)||F.P->GetOwnerVariant()!=FFieldVariant(S.Function.Get())||F.P->GetFName()!=F.Name||uint64(F.P->GetPropertyFlags())!=F.Flags||F.P->ArrayDim!=1||F.P->HasSetterOrGetter()||F.P->GetOffset_ForInternal()!=F.Offset||F.P->GetSize()!=F.Size)return false;if(F.Class==FBoolProperty::StaticClass()){const auto* Bool=static_cast<FBoolProperty*>(F.P);if(Bool->GetByteOffset()!=F.BoolOffset||Bool->GetByteMask()!=F.BoolByteMask||Bool->GetFieldMask()!=F.BoolFieldMask)return false;}}
            if(!SourceMetadata(S).Equals(S.Metadata,ESearchCase::CaseSensitive))return false;
        }return true;
    }
    bool BindNativeCall(UK2Node_CallFunction* Call)
    {
        if(!Plain(false))return false;UObject* ReferenceOwner=nullptr;if(!Loaded(Call)||Call->GetClass()!=UK2Node_CallFunction::StaticClass()||!ReferenceParent(Call->FunctionReference,ReferenceOwner)||!Loaded(ReferenceOwner)||ReferenceOwner->GetPathName()!=TEXT("/Script/Engine.KismetMathLibrary")||ReferenceOwner->GetClass()!=UClass::StaticClass()||Call->FunctionReference.IsSelfContext()||Call->FunctionReference.IsLocalScope()||Call->FunctionReference.IsDeprecated())return Fail(R,TEXT("function_extract_composite_native_source_pending"),TEXT("Exact loaded native KismetMathLibrary static pure call references required; wider original function owners/signatures remain incomplete."));
        auto* Owner=static_cast<UClass*>(ReferenceOwner);Holds.Emplace(Owner);if(!Owner->ClassGeneratedBy.IsResolved()||Owner->ClassGeneratedBy.Get()!=nullptr||!Owner->HasAnyClassFlags(CLASS_Native)||Owner->IsEditorOnly())return false;
        UFunction* Fn=Owner->FindFunctionByName(Call->FunctionReference.GetMemberName(),EIncludeSuperFlag::ExcludeSuper);Holds.Emplace(Fn);if(!Plain(false))return false;
        if(!Loaded(Fn)||Fn->GetClass()!=UFunction::StaticClass()||Fn->GetOuter()!=Owner||Fn->GetSuperFunction()||!Fn->HasAllFunctionFlags(FUNC_Native|FUNC_Static|FUNC_BlueprintCallable|FUNC_BlueprintPure)||Fn->HasAnyFunctionFlags(FUNC_Delegate|FUNC_Net|FUNC_EditorOnly)||Fn->HasMetaData(TEXT("Latent"))||UEdGraphSchema_K2::HasWildcardParams(Fn)||Call->FunctionReference.GetMemberGuid().IsValid())return false;
        const FNativeSource* SourceRow=NativeSources.FindByPredicate([&](const FNativeSource& V){return V.Function.Get()==Fn;});
        if(!SourceRow)
        {
            if(!Owner->ClassDefaultObject.IsResolved())return false;UObject* ExistingCDO=Owner->ClassDefaultObject.Get();Holds.Emplace(ExistingCDO);
            if(!Loaded(ExistingCDO)||ExistingCDO->GetClass()!=Owner||ExistingCDO->GetOuter()!=Owner->GetOuter()||ExistingCDO->GetPackage()!=Owner->GetPackage()||!ExistingCDO->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject))return false;
            if(NativeSources.Num()>=MaxNodes)return false;FNativeSource V;V.CDO.Reset(ExistingCDO);V.CDOIdentity.Capture(ExistingCDO);V.Owner.Reset(Owner);V.Function.Reset(Fn);V.ClassIdentity.Capture(Owner);V.FunctionIdentity.Capture(Fn);V.ClassFlags=Owner->GetClassFlags();V.Super=Owner->GetSuperClass();V.Flags=Fn->FunctionFlags;V.NumParms=Fn->NumParms;V.ParmsSize=Fn->ParmsSize;V.Script=Fn->Script;
            for(TFieldIterator<FProperty> It(Fn,EFieldIterationFlags::None);It;++It)
            {
                auto* P=*It;if(V.Fields.Num()>=128||!PrimitiveField(P)||P->GetOwnerVariant()!=FFieldVariant(Fn)||P->ArrayDim!=1||P->HasSetterOrGetter()||!P->HasAnyPropertyFlags(CPF_Parm)||P->HasAnyPropertyFlags(CPF_ReferenceParm|CPF_ConstParm))return false;
                FField Field{P,P->GetClass(),P->GetFName(),uint64(P->GetPropertyFlags()),P->GetOffset_ForInternal(),P->GetSize()};if(P->GetClass()==FBoolProperty::StaticClass()){const auto* Bool=static_cast<FBoolProperty*>(P);Field.BoolOffset=Bool->GetByteOffset();Field.BoolByteMask=Bool->GetByteMask();Field.BoolFieldMask=Bool->GetFieldMask();}V.Fields.Add(Field);
            }
            V.Metadata=SourceMetadata(V);NativeSources.Add(MoveTemp(V));SourceRow=&NativeSources.Last();if(!Plain(false))return false;
            const int32 SourceIndex=NativeSources.Num()-1;const auto SourceFields=NativeSources[SourceIndex].Fields;
            for(const auto& F:SourceFields){FEdGraphPinType PinType;const bool Converted=GetDefault<UEdGraphSchema_K2>()->ConvertPropertyToPinType(F.P,PinType);if(!Plain(false))return false;if(!Converted)return false;NativeSources[SourceIndex].Pins.Add(F.Name,PinType);}
        }
        if(Call->Pins.Num()!=SourceRow->Fields.Num()+1)return false;
        auto* Self=Named(Call,UEdGraphSchema_K2::PN_Self,EGPD_Input);if(!Self||!Self->bHidden||!Self->LinkedTo.IsEmpty()||!Self->DefaultObject.IsResolved()||Self->DefaultObject.Get()!=SourceRow->CDO.Get()||!Self->PinType.PinSubCategoryObject.IsValid()||Self->PinType.PinCategory!=UEdGraphSchema_K2::PC_Object||Self->PinType.PinSubCategoryObject.Get()!=Owner)return false;
        TSet<UEdGraphPin*> Expected;Expected.Add(Self);
        for(const auto& F:SourceRow->Fields){const bool Output=(F.Flags&uint64(CPF_OutParm|CPF_ReturnParm))!=0;auto* P=Named(Call,F.Name,Output?EGPD_Output:EGPD_Input);const auto* PinType=SourceRow->Pins.Find(F.Name);if(!P||!PinType||!Type(P->PinType,*PinType)||Expected.Contains(P))return false;Expected.Add(P);}return Expected.Num()==Call->Pins.Num();
    }
    bool ReadPurity(UEdGraphNode* Node,FPurityValue& Expected,bool Capture)
    {
        UClass* CallClass=UK2Node_CallFunction::StaticClass();
        if(!Loaded(Node)||Node->GetClass()!=CallClass)return Stop(TEXT("purity_node_role"));
        FProperty* Root=FindFProperty<FProperty>(CallClass,TEXT("NodePurityOverride"));
        // Public reflection reads the authored private enum through its exact native int8 storage; no GetEnum resolution/export.
        if(!Root||Root->GetClass()!=FEnumProperty::StaticClass()||Root->GetOwnerStruct()!=CallClass||Root->GetOwnerProperty()!=Root||Root->GetFName()!=TEXT("NodePurityOverride"))return Stop(TEXT("purity_property_role"));
        auto* Property=static_cast<FEnumProperty*>(Root);
        if(Property->ArrayDim!=1||Property->HasSetterOrGetter()||Property->GetSize()!=sizeof(int8)||Property->GetOffset_ForInternal()<0||int64(Property->GetOffset_ForInternal())+Property->GetSize()>CallClass->GetPropertiesSize())return Stop(TEXT("purity_property_layout"));
        FNumericProperty* Storage=Property->GetUnderlyingProperty();
        if(!Storage||Storage->GetClass()!=FInt8Property::StaticClass()||Storage->GetOwnerProperty()!=Property||Storage->ArrayDim!=1||Storage->HasSetterOrGetter()||Storage->GetSize()!=sizeof(int8)||Storage->GetOffset_ForInternal()<0)return Stop(TEXT("purity_storage_role"));
        FPurityValue Actual;Actual.Property=Property;Actual.Storage=Storage;Actual.Offset=Property->GetOffset_ForInternal();Actual.InnerOffset=Storage->GetOffset_ForInternal();Actual.ClassSize=CallClass->GetPropertiesSize();Actual.Flags=uint64(Property->GetPropertyFlags());Actual.InnerFlags=uint64(Storage->GetPropertyFlags());
        if(!Capture&&(Expected.Property!=Property||Expected.Storage!=Storage||Expected.Offset!=Actual.Offset||Expected.InnerOffset!=Actual.InnerOffset||Expected.ClassSize!=Actual.ClassSize||Expected.Flags!=Actual.Flags||Expected.InnerFlags!=Actual.InnerFlags))return Stop(TEXT("purity_descriptor_changed"));
        const int64 Raw=Storage->GetSignedIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Node));
        if(Raw<int64(ENodePurityOverride::Unset)||Raw>int64(ENodePurityOverride::Impure))return Stop(TEXT("purity_value_invalid"));Actual.Value=int8(Raw);
        if(Capture)Expected=Actual;else if(Expected.Value!=Actual.Value)return Stop(TEXT("authored_purity_changed"));return true;
    }
    FNodeRole* Role(UEdGraphNode* N){return Nodes.FindByPredicate([&](const FNodeRole& V){return V.Hold.Get()==N;});}
    UEdGraphPin* Find(UEdGraphNode* N,const FGuid& Id)const
    {if(!Loaded(N))return nullptr;const auto* A=Candidate.Find(N);const auto& P=A?*A:N->Pins;for(auto* Q:P)if(Q&&Q->PinId==Id)return Q;return nullptr;}
    UEdGraphPin* Named(UEdGraphNode* N,FName Name,EEdGraphPinDirection Direction)const
    {if(!Loaded(N))return nullptr;for(auto* P:N->Pins)if(P&&P->PinName==Name&&P->Direction==Direction)return P;return nullptr;}
    bool Links(const FNodeRole& N,const FValue& E,UEdGraphPin* P)
    {
        TArray<UEdGraphPin*> Expected;
        for(const auto& Link:E.Links)
        {
            const auto* Peer=Nodes.FindByPredicate([&](const FNodeRole& V){return V.Guid==Link.Key;});if(!Peer)return false;
            // SDK removes the selected event template and breaks its links before moving nodes.
            // Exec consumers become unlinked entries; data consumers keep their own defaults, not event defaults.
            if(Moved&&Peer->Hold.Get()==EventTemplate.Get())
            {
                if(E.Type.PinCategory==UEdGraphSchema_K2::PC_Exec&&E.Direction==EGPD_Input&&AutoEntry.ContainsByPredicate([&](const TPair<UEdGraphNode*,FGuid>& V){return V.Key==N.Hold.Get()&&V.Value==E.Id;}))
                {UEdGraphPin* Q=nullptr;for(UEdGraphNode* T:Destination->Nodes)if(T->GetClass()==UK2Node_FunctionEntry::StaticClass())Q=Named(T,UEdGraphSchema_K2::PN_Then,EGPD_Output);if(!Q||!Type(Q->PinType,E.Type))return false;if(!Expected.Contains(Q))Expected.Add(Q);}
                continue;
            }
            UEdGraphPin* Q=Find(Peer->Hold.Get(),Link.Value);if(!Q)return false;
            if(Moved&&!N.Derived&&IsSelected(N.Hold.Get())!=IsSelected(Peer->Hold.Get()))
            {
                if(!IsSelected(N.Hold.Get()))
                {
                    const auto* B=Port(Peer->Hold.Get(),Link.Value);if(!B)return false;
                    const FName Name=E.Type.PinCategory==UEdGraphSchema_K2::PC_Exec?(E.Direction==EGPD_Output?UEdGraphSchema_K2::PN_Execute:UEdGraphSchema_K2::PN_Then):B->Name;
                    Q=Named(Gateway,Name,E.Direction==EGPD_Input?EGPD_Output:EGPD_Input);
                }
                else
                {
                    if(E.Type.PinCategory==UEdGraphSchema_K2::PC_Exec&&E.Direction==EGPD_Output&&!KeepReturn)continue;
                    const auto* B=Port(N.Hold.Get(),E.Id);if(!B)return false;Q=nullptr;for(UEdGraphNode* T:Destination->Nodes)
                    {
                        if((E.Direction==EGPD_Input&&T->GetClass()!=UK2Node_FunctionEntry::StaticClass())||(E.Direction==EGPD_Output&&T->GetClass()!=UK2Node_FunctionResult::StaticClass()))continue;
                        Q=Named(T,E.Type.PinCategory==UEdGraphSchema_K2::PC_Exec?(E.Direction==EGPD_Input?UEdGraphSchema_K2::PN_Then:UEdGraphSchema_K2::PN_Execute):B->Name,E.Direction==EGPD_Input?EGPD_Output:EGPD_Input);if(Q)break;
                    }
                }
                if(!Q||!Type(Q->PinType,E.Type))return false;
            }
            if(!Expected.Contains(Q))Expected.Add(Q);
        }
        // SDK also routes an initially unlinked exec through the new function's entry/result.
        if(Moved&&IsSelected(N.Hold.Get())&&E.Links.IsEmpty()&&E.Type.PinCategory==UEdGraphSchema_K2::PC_Exec&&
            (E.Direction==EGPD_Input?AutoEntry:AutoReturn).ContainsByPredicate([&](const TPair<UEdGraphNode*,FGuid>& V){return V.Key==N.Hold.Get()&&V.Value==E.Id;}))
        {
            for(UEdGraphNode* T:Destination->Nodes)
            {
                const bool IsPort=E.Direction==EGPD_Input?T->GetClass()==UK2Node_FunctionEntry::StaticClass():T->GetClass()==UK2Node_FunctionResult::StaticClass();
                if(IsPort){auto* Q=Named(T,E.Direction==EGPD_Input?UEdGraphSchema_K2::PN_Then:UEdGraphSchema_K2::PN_Execute,E.Direction==EGPD_Input?EGPD_Output:EGPD_Input);if(!Q)return false;if(!Expected.Contains(Q))Expected.Add(Q);}
            }
        }
        if(P->LinkedTo.Num()!=Expected.Num())return false;
        for(auto* Q:P->LinkedTo)if(!Expected.Contains(Q)||!Q->LinkedTo.Contains(P))return false;
        return true;
    }
    bool OwnerCurrent()
    {
        if(!IsInGameThread()||IsGarbageCollecting()||UE::IsSavingPackage()||GExitPurge||IsEngineExitRequested()||!EditorRole.Current()||GEditor!=Editor.Get()||!WorldRole.Current()||World->bIsTearingDown||!BlueprintRole.Current()||!SourceRole.Current())return Stop(TEXT("captured_owner_context"));
        if(!GEditor->PlayWorld.IsResolved()||GEditor->PlayWorld.Get()!=nullptr||GEditor->GetEditorWorldContext().World()!=World.Get()||IsPlaySessionActive()||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bIsRegeneratingOnLoad||BP->bSuppressStructurallyModified)return Stop(TEXT("captured_editor_phase"));
        return true;
    }
    bool Plain(bool Values=true)
    {
        if(Stopped)return false;
        if(!OwnerCurrent()||!MathCurrent())return Stop(TEXT("context_or_math_source"));
        auto Gen=BP->GeneratedClass;auto Skel=BP->SkeletonGeneratedClass;auto Parent=BP->ParentClass;
        if(!Gen.GetGCPtr().IsResolved()||!Skel.GetGCPtr().IsResolved()||!Parent.GetGCPtr().IsResolved()||Parent.GetGCPtr().Get()!=AActor::StaticClass())return Stop(TEXT("generated_handles"));
        UClass* CurrentGenerated=Gen.GetGCPtr().Get();UClass* CurrentSkeleton=Skel.GetGCPtr().Get();
        auto ClassRole=[&](UClass* C,UClass* Meta,const FString& Path)->bool{return Loaded(C)&&C->GetClass()==Meta&&C->ClassGeneratedBy.IsResolved()&&C->ClassGeneratedBy.Get()==BP.Get()&&C->GetOuter()==BP->GetPackage()&&C->GetSuperClass()==AActor::StaticClass()&&C->GetPathName().Equals(Path,ESearchCase::CaseSensitive);};
        if(!ClassRole(CurrentGenerated,GeneratedMeta,GeneratedPath)||!ClassRole(CurrentSkeleton,SkeletonMeta,SkeletonPath)||CurrentGenerated!=(Rebinding?CandidateGenerated.Get():Generated.Get())||CurrentSkeleton!=(Rebinding?CandidateSkeleton.Get():Skeleton.Get()))return Stop(TEXT("generated_roles"));
        auto* CurrentRoot=Rebinding?CandidateRoot.Get():RootTemplate.Get();
        if(!BP->SimpleConstructionScript.IsResolved()||BP->SimpleConstructionScript.Get()!=SCS.Get()||!Loaded(SCS.Get())||SCS->GetAllNodes().Num()!=1||SCS->GetAllNodes()[0]!=RootNode.Get()||!Loaded(RootNode.Get())||RootNode->GetOuter()!=SCS.Get()||RootNode->VariableGuid!=RootGuid||RootNode->GetVariableName()!=RootName||!RootNode->ComponentTemplate.IsResolved()||RootNode->ComponentTemplate.Get()!=CurrentRoot||!Loaded(CurrentRoot)||CurrentRoot->GetClass()!=USceneComponent::StaticClass()||CurrentRoot->GetOuter()!=CurrentGenerated||CurrentRoot->GetFName()!=TemplateName||!CurrentRoot->GetPathName().Equals(RootPath,ESearchCase::CaseSensitive)||CurrentRoot->GetRelativeLocation()!=RootLocation||!CurrentRoot->GetRelativeRotation().Equals(RootRotation,0)||CurrentRoot->GetRelativeScale3D()!=RootScale||CurrentRoot->Mobility!=RootMobility)return Stop(TEXT("captured_scs_root"));
        TArray<UEdGraph*> F=Functions;if(DiagnosticMember)F.Add(Diagnostic);if(Moved)F.Add(Destination);
        if(!Slots(BP->FunctionGraphs,F)||!Slots(BP->MacroGraphs,Macros)||!Slots(BP->UbergraphPages,Events)||!Slots(BP->DelegateSignatureGraphs,Delegates))return Stop(TEXT("authored_graph_roots"));
        for(const auto& G:Graphs)
        {
            UEdGraph* A=G.Hold.Get();auto Schema=Loaded(A)?A->Schema:TSubclassOf<UEdGraphSchema>();
            if(!Loaded(A)||A->GetClass()!=UEdGraph::StaticClass()||!Schema.GetGCPtr().IsResolved()||Schema.GetGCPtr().Get()!=UEdGraphSchema_K2::StaticClass()||A->GraphGuid!=G.Guid||A->GetFName()!=G.Name||A->GetPackage()!=BP->GetPackage())return Stop(TEXT("captured_graph_role"));
            TArray<UEdGraph*> Expected=G.Children;if(Moved&&A==Source.Get())for(auto* Sub:MovedSubgraphs)Expected.RemoveSingle(Sub);
            if(!Slots(A->SubGraphs,Expected))return Stop(TEXT("captured_subgraph_membership"));
            for(const auto& C:Composites)if(A==C.Graph.Get())
            {
                // Only selected gateways move into the function. Descendant gateways
                // and their BoundGraphs retain their immediate authored owner graph.
                UEdGraph* ExpectedParent=(Moved&&C.Selected)?Destination:C.ParentGraph;
                auto* OwnerNode=C.Node.Get();
                if(!Loaded(ExpectedParent)||!Loaded(OwnerNode)||OwnerNode->GetClass()!=UK2Node_Composite::StaticClass()||OwnerNode->GetOuter()!=ExpectedParent||!ExpectedParent->Nodes.Contains(OwnerNode)||A->GetOuter()!=OwnerNode||!OwnerNode->BoundGraph.IsResolved()||OwnerNode->BoundGraph.Get()!=A)return Stop(TEXT("bound_graph_binding"));
            }
        }
        if(Moved&&(!Loaded(Destination)||Destination->GetClass()!=UEdGraph::StaticClass()||!Destination->Schema.GetGCPtr().IsResolved()||Destination->Schema.GetGCPtr().Get()!=UEdGraphSchema_K2::StaticClass()||Destination->GetOuter()!=BP.Get()||!Loaded(Gateway)||Gateway->GetClass()!=UK2Node_CallFunction::StaticClass()||Gateway->GetOuter()!=Source.Get()||!Source->Nodes.Contains(Gateway)||!Slots(Destination->SubGraphs,MovedSubgraphs)))return Stop(TEXT("returned_function_roles"));
        auto Ordered=[&](UEdGraph* G,const TArray<UEdGraphNode*>& Expected)->bool{if(G->Nodes.Num()!=Expected.Num())return false;for(int32 I=0;I<Expected.Num();++I)if(!G->Nodes[I].IsResolved()||G->Nodes[I].Get()!=Expected[I])return false;return true;};
        auto ExpectedSource=SourceSlots;if(Moved){for(auto* N:Selected)ExpectedSource.RemoveSingle(N);ExpectedSource.Add(Gateway);}if(!Ordered(Source.Get(),ExpectedSource)||(Moved&&!Ordered(Destination,DestinationSlots)))return Stop(TEXT("ordered_node_membership"));
        for(const auto& C:Composites)if(!Ordered(C.Graph.Get(),C.Slots))return Stop(TEXT("ordered_bound_node_membership"));
        for(auto& N:Nodes)
        {
            auto* A=N.Hold.Get();if(Moved&&A==EventTemplate.Get())continue; // Removed SDK role: never dereference the retired event/pins.
            UEdGraph* G=(Moved&&IsSelected(A))?Destination:N.Graph;
            if(!Loaded(A)||A->GetClass()!=N.Class||A->GetFName()!=N.Name||A->GetOuter()!=G||!G->Nodes.Contains(A)||A->NodeGuid!=N.Guid||A->NodePosX!=N.X||A->NodePosY!=N.Y||!A->NodeComment.Equals(N.Comment,ESearchCase::CaseSensitive))return Stop(TEXT("captured_node_role"));
            const auto* Temp=Candidate.Find(A);const auto& Physical=Temp?*Temp:N.Physical;
            if(A->Pins!=Physical||Physical.Num()!=N.Values.Num())return Stop(TEXT("physical_pin_inventory"));
            if(N.HasPurity&&!ReadPurity(A,N.Purity,false))return false;
            if(N.HasReference){const auto* Ref=NodeReference(A);UObject* Owner=nullptr;if(!Ref||!ReferenceParent(*Ref,Owner)||Owner!=N.ReferenceOwner||!Ref->IsSameReference(N.Reference)||Ref->IsDeprecated()!=N.Reference.IsDeprecated())return Stop(TEXT("captured_member_reference"));}
            for(int32 I=0;I<N.Values.Num();++I)
            {
                auto* Q=Physical[I];const auto& V=N.Values[I];
                if(!Q||!WeakReadable(Q->PinType.PinSubCategoryObject)||!WeakReadable(Q->PinType.PinValueType.TerminalSubCategoryObject)||!Q->PinType.PinSubCategoryMemberReference.MemberParent.IsResolved()||Q->PinType.PinValueType.TerminalSubCategoryObject.Get()!=V.TerminalObject||Q->PinType.PinSubCategoryMemberReference.MemberParent.Get()!=V.MemberParent||(V.OwnTypePath.IsEmpty()&&Q->PinType.PinSubCategoryObject.Get()!=V.TypeObject))return Stop(TEXT("captured_pin_reference_slots"));
                FEdGraphPinType ActualType=Q->PinType;
                if(!V.OwnTypePath.IsEmpty()) {auto* C=Cast<UClass>(ActualType.PinSubCategoryObject.Get());if(!Loaded(C)||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy.Get()!=BP.Get()||!C->GetPathName().Equals(V.OwnTypePath,ESearchCase::CaseSensitive))return Stop(TEXT("compiled_own_pin_type"));ActualType.PinSubCategoryObject=V.Type.PinSubCategoryObject;}
                if(!Q||Q->WasTrashed()||Q->GetOwningNodeUnchecked()!=A||Q->PinId!=V.Id||Q->PinName!=V.Name||Q->Direction!=V.Direction||!Type(ActualType,V.Type)||!Q->DefaultObject.IsResolved()||Q->DefaultObject.Get()!=V.Object||!Q->DefaultValue.Equals(V.Raw,ESearchCase::CaseSensitive)||!Q->AutogeneratedDefaultValue.Equals(V.Auto,ESearchCase::CaseSensitive)||Q->bHidden!=V.Hidden||Q->bOrphanedPin!=V.Orphan||Q->bNotConnectable!=V.Connectable||Q->bDefaultValueIsReadOnly!=V.ReadOnly||Q->ParentPin||!Q->SubPins.IsEmpty()||!Links(N,V,Q))return Stop(TEXT("captured_pin_value_or_link"));
                if(FTextInspector::GetFlags(Q->DefaultTextValue)!=V.TextFlags||FTextInspector::GetFlags(Q->PinFriendlyName)!=V.FriendlyFlags)return Stop(TEXT("text_flags"));
                if(Values&&!Rebinding&&(FTextInspector::GetSharedDataId(Q->DefaultTextValue)!=FTextInspector::GetSharedDataId(V.Text)||FTextInspector::GetSharedDataId(Q->PinFriendlyName)!=FTextInspector::GetSharedDataId(V.Friendly)))return Stop(TEXT("physical_text_identity"));
            }
            if(N.CustomEvent){auto* E=static_cast<UK2Node_CustomEvent*>(A);if(E->CustomFunctionName!=N.EventName||E->FunctionFlags!=N.EventFlags)return Stop(TEXT("event_template_authored_identity"));}
            if(N.Editable)
            {
                auto* E=static_cast<UK2Node_EditablePinBase*>(A);if(E->UserDefinedPins.Num()!=N.UserPins.Num())return Stop(TEXT("editable_user_pin_count"));
                for(int32 I=0;I<N.UserPins.Num();++I){const auto& U=E->UserDefinedPins[I];const auto& V=N.UserPins[I];if(!U||!WeakReadable(U->PinType.PinSubCategoryObject)||!WeakReadable(U->PinType.PinValueType.TerminalSubCategoryObject)||!U->PinType.PinSubCategoryMemberReference.MemberParent.IsResolved()||U->PinName!=V.PinName||U->DesiredPinDirection!=V.DesiredPinDirection||!Type(U->PinType,V.PinType)||!U->PinDefaultValue.Equals(V.PinDefaultValue,ESearchCase::CaseSensitive))return Stop(TEXT("editable_user_pin_value"));}
            }
            if(N.Tunnel)
            {
                auto* T=static_cast<UK2Node_Tunnel*>(A);if(!T->InputSinkNode.IsResolved()||!T->OutputSourceNode.IsResolved()||T->InputSinkNode.Get()!=N.Sink||T->OutputSourceNode.Get()!=N.Output||T->bCanHaveInputs!=N.Inputs||T->bCanHaveOutputs!=N.Outputs||T->UserDefinedPins.Num()!=N.UserPins.Num())return Stop(TEXT("tunnel_binding"));
                for(int32 I=0;I<N.UserPins.Num();++I){const auto& U=T->UserDefinedPins[I];const auto& V=N.UserPins[I];if(!U||!WeakReadable(U->PinType.PinSubCategoryObject)||!WeakReadable(U->PinType.PinValueType.TerminalSubCategoryObject)||!U->PinType.PinSubCategoryMemberReference.MemberParent.IsResolved()||U->PinName!=V.PinName||U->DesiredPinDirection!=V.DesiredPinDirection||!Type(U->PinType,V.PinType)||!U->PinDefaultValue.Equals(V.PinDefaultValue,ESearchCase::CaseSensitive))return Stop(TEXT("tunnel_user_pin"));}
            }

        }
        const int32 SourceCount=Nodes.FilterByPredicate([&](const FNodeRole& N){return (N.Graph==Source.Get()&&(!Moved||!IsSelected(N.Hold.Get())))||(Moved&&N.Hold.Get()==Gateway);}).Num();
        if(Source->Nodes.Num()!=SourceCount)return Stop(TEXT("captured_node_counts"));
        for(const auto& C:Composites)if(C.Graph->Nodes.Num()!=Nodes.FilterByPredicate([&](const FNodeRole& N){return N.Graph==C.Graph.Get();}).Num())return Stop(TEXT("captured_bound_node_counts"));
        return true;
    }
    bool Reacquire()
    {
        if(Stopped||!OwnerCurrent())return false;Rebinding=true;Candidate.Reset();
        auto G=BP->GeneratedClass;auto K=BP->SkeletonGeneratedClass;
        if(!G.GetGCPtr().IsResolved()||!K.GetGCPtr().IsResolved()||!RootNode->ComponentTemplate.IsResolved())return Stop(TEXT("reacquire_resolved_handles"));
        CandidateGenerated.Reset(G.GetGCPtr().Get());CandidateSkeleton.Reset(K.GetGCPtr().Get());CandidateRoot.Reset(Cast<USceneComponent>(RootNode->ComponentTemplate.Get()));
        for(auto& N:Nodes){if(Moved&&N.Hold.Get()==EventTemplate.Get())continue;if(!Loaded(N.Hold.Get())||N.Hold->Pins.Num()!=N.Values.Num())return Stop(TEXT("reacquire_pin_count"));Candidate.Add(N.Hold.Get(),N.Hold->Pins);}
        if(!Plain(false))return false;
        for(auto& N:Nodes)if(!Moved||N.Hold.Get()!=EventTemplate.Get())for(int32 I=0;I<N.Values.Num();++I)
        {
            auto& V=N.Values[I];auto* Q=Candidate.FindChecked(N.Hold.Get())[I];
            const FText T=Q->DefaultTextValue;const FString TD=T.ToString();if(!Plain(false))return false;
            const FText F=Q->PinFriendlyName;const FString FD=F.ToString();if(!Plain(false))return false;
            if(!N.Fresh&&(!TD.Equals(V.TextDisplay,ESearchCase::CaseSensitive)||!FD.Equals(V.FriendlyDisplay,ESearchCase::CaseSensitive)))return Stop(TEXT("reacquire_logical_text"));
            if(N.Fresh){V.TextDisplay=TD;V.FriendlyDisplay=FD;}
            V.Text=T;V.Friendly=F;
        }
        for(auto& N:Nodes){if(Moved&&N.Hold.Get()==EventTemplate.Get())continue;N.Physical=Candidate.FindChecked(N.Hold.Get());N.Fresh=false;}Candidate.Reset();Holds.Emplace(Generated.Get());Holds.Emplace(Skeleton.Get());Holds.Emplace(RootTemplate.Get());Generated=MoveTemp(CandidateGenerated);Skeleton=MoveTemp(CandidateSkeleton);RootTemplate=MoveTemp(CandidateRoot);Rebinding=false;return Plain();
    }
    bool CaptureNode(UEdGraphNode* O,UEdGraph* A,bool Fresh=false)
    {
        if(!Loaded(O)||O->Pins.Num()>256||!O->NodeGuid.IsValid()||Nodes.ContainsByPredicate([&](const FNodeRole& V){return V.Guid==O->NodeGuid;}))return Stop(TEXT("capture_node"));
        FNodeRole V;V.Hold.Reset(O);V.Class=O->GetClass();V.Graph=A;V.Name=O->GetFName();V.Guid=O->NodeGuid;V.X=O->NodePosX;V.Y=O->NodePosY;V.Comment=O->NodeComment;V.Fresh=Fresh;V.Derived=Fresh;
        if(auto* E=Cast<UK2Node_EditablePinBase>(O)){if(E->UserDefinedPins.Num()>256)return Stop(TEXT("capture_editable_user_pin_bound"));V.Editable=true;for(const auto& U:E->UserDefinedPins){if(!U||!HoldType(U->PinType))return Stop(TEXT("capture_editable_user_pin"));V.UserPins.Add(*U);}}
        if(auto* E=Cast<UK2Node_CustomEvent>(O)){V.CustomEvent=true;V.EventName=E->CustomFunctionName;V.EventFlags=E->FunctionFlags;}
        if(auto* T=Cast<UK2Node_Tunnel>(O)){V.Tunnel=true;if(!T->InputSinkNode.IsResolved()||!T->OutputSourceNode.IsResolved())return Stop(TEXT("capture_tunnel"));V.Sink=T->InputSinkNode.Get();V.Output=T->OutputSourceNode.Get();V.Inputs=T->bCanHaveInputs;V.Outputs=T->bCanHaveOutputs;}
        if(const auto* Ref=(Fresh&&(O->GetClass()==UK2Node_FunctionEntry::StaticClass()||O->GetClass()==UK2Node_FunctionResult::StaticClass()))?nullptr:NodeReference(O)){if(!ReferenceParent(*Ref,V.ReferenceOwner)||(V.ReferenceOwner&&!Loaded(V.ReferenceOwner)))return Stop(TEXT("capture_member_reference"));if(V.ReferenceOwner)Holds.Emplace(V.ReferenceOwner);V.Reference=*Ref;V.HasReference=true;}
        if(O->GetClass()==UK2Node_CallFunction::StaticClass()){if(!ReadPurity(O,V.Purity,true))return false;V.HasPurity=true;}
        TSet<FGuid> PinIds;for(auto* Q:O->Pins)
        {
            if(!Q||!Q->PinId.IsValid()||PinIds.Contains(Q->PinId)||Q->WasTrashed()||Q->GetOwningNodeUnchecked()!=O||!Q->DefaultObject.IsResolved()||!HoldType(Q->PinType)||Q->ParentPin||!Q->SubPins.IsEmpty()||Q->LinkedTo.Num()>32)return Stop(TEXT("capture_pin"));PinIds.Add(Q->PinId);
            FValue E;E.Id=Q->PinId;E.Name=Q->PinName;E.Direction=Q->Direction;E.Type=Q->PinType;E.Raw=Q->DefaultValue;E.Auto=Q->AutogeneratedDefaultValue;E.Object=Q->DefaultObject.Get();if(E.Object){if(!Loaded(E.Object))return Stop(TEXT("capture_default_object"));Holds.Emplace(E.Object);}E.TypeObject=E.Type.PinSubCategoryObject.Get();E.TerminalObject=E.Type.PinValueType.TerminalSubCategoryObject.Get();E.MemberParent=E.Type.PinSubCategoryMemberReference.MemberParent.Get();if(E.Type.PinSubCategoryObject.IsValid())Holds.Emplace(E.Type.PinSubCategoryObject.Get());E.Text=Q->DefaultTextValue;E.Friendly=Q->PinFriendlyName;E.TextFlags=FTextInspector::GetFlags(E.Text);E.FriendlyFlags=FTextInspector::GetFlags(E.Friendly);E.Hidden=Q->bHidden;E.Orphan=Q->bOrphanedPin;E.Connectable=Q->bNotConnectable;E.ReadOnly=Q->bDefaultValueIsReadOnly;
            if(auto* C=Cast<UClass>(E.Type.PinSubCategoryObject.Get());Loaded(C)&&C->ClassGeneratedBy.IsResolved()&&C->ClassGeneratedBy.Get()==BP.Get())E.OwnTypePath=C->GetPathName();
            for(auto* L:Q->LinkedTo){auto* Peer=OwnedPins.FindRef(L);if(!Peer||L->GetOwningNodeUnchecked()!=Peer||Peer->GetGraph()!=A||!L->LinkedTo.Contains(Q))return Stop(TEXT("capture_closed_link"));E.Links.Add({Peer->NodeGuid,L->PinId});}
            V.Values.Add(MoveTemp(E));V.Physical.Add(Q);
        }
        Nodes.Add(MoveTemp(V));return true;
    }
    static bool PortType(const FEdGraphPinType& T)
    {
        if(T.IsContainer()||T.bIsReference||T.bIsConst||T.bIsWeakPointer||!T.PinSubCategoryObject.IsExplicitlyNull()||!T.PinValueType.TerminalSubCategoryObject.IsExplicitlyNull()||!T.PinSubCategoryMemberReference.MemberParent.IsResolved()||T.PinSubCategoryMemberReference.MemberParent.Get()!=nullptr)return false;
        const FName C=T.PinCategory;return C==UEdGraphSchema_K2::PC_Exec||C==UEdGraphSchema_K2::PC_Boolean||C==UEdGraphSchema_K2::PC_Int||C==UEdGraphSchema_K2::PC_Int64||C==UEdGraphSchema_K2::PC_Name||C==UEdGraphSchema_K2::PC_String||C==UEdGraphSchema_K2::PC_Text||(C==UEdGraphSchema_K2::PC_Real&&(T.PinSubCategory==UEdGraphSchema_K2::PC_Float||T.PinSubCategory==UEdGraphSchema_K2::PC_Double));
    }
    bool Qualify()
    {
        UK2Node_Tunnel* Entry=nullptr;UK2Node_Tunnel* Exit=nullptr;
        for(const auto& V:Nodes)if(V.Graph==Bound.Get()&&V.Class==UK2Node_Tunnel::StaticClass())
        {if(V.Outputs&&!V.Inputs&&!Entry)Entry=static_cast<UK2Node_Tunnel*>(V.Hold.Get());else if(V.Inputs&&!V.Outputs&&!Exit)Exit=static_cast<UK2Node_Tunnel*>(V.Hold.Get());else return false;}
        if(!Entry||!Exit||Composite->GetClass()!=UK2Node_Composite::StaticClass()||Entry->OutputSourceNode.Get()!=Composite.Get()||Exit->InputSinkNode.Get()!=Composite.Get()||Composite->InputSinkNode.Get()!=Entry||Composite->OutputSourceNode.Get()!=Exit||Composite->Pins.Num()!=Entry->Pins.Num()+Exit->Pins.Num())return false;
        TSet<FName> PortNames;int32 ExecInputs=0;
        for(auto* T:{Entry,Exit})for(auto* P:T->Pins)
        {
            const EEdGraphPinDirection Direction=T==Entry?EGPD_Output:EGPD_Input;
            if(!P||P->Direction!=Direction||!PortType(P->PinType)||PortNames.Contains(P->PinName))return false;PortNames.Add(P->PinName);
            auto* Peer=Named(Composite.Get(),P->PinName,Direction==EGPD_Output?EGPD_Input:EGPD_Output);if(!Peer||!Type(Peer->PinType,P->PinType))return false;
            if(T==Entry&&P->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec)++ExecInputs;
        }
        if(ExecInputs!=1)return false; // Original SDK has one function entry; other valid boundary forms remain pending.
        TMap<UEdGraphNode*,int32> Indegree;int32 LinksSeen=0;
        for(const auto& V:Nodes)if(V.Graph==Bound.Get())Indegree.Add(V.Hold.Get(),0);
        for(const auto& V:Nodes)if(V.Graph==Bound.Get())
        {
            auto* N=V.Hold.Get();
            if(V.Class==UK2Node_IfThenElse::StaticClass())
            {
                if(N->Pins.Num()!=4)return false;
                const TPair<FName,EEdGraphPinDirection> BranchPins[]={{UEdGraphSchema_K2::PN_Execute,EGPD_Input},{UEdGraphSchema_K2::PN_Then,EGPD_Output},{UEdGraphSchema_K2::PN_Else,EGPD_Output},{UEdGraphSchema_K2::PN_Condition,EGPD_Input}};
                for(const auto& Pair:BranchPins){auto* P=Named(N,Pair.Key,Pair.Value);if(!P||P->PinType.PinCategory!=(Pair.Key==UEdGraphSchema_K2::PN_Condition?UEdGraphSchema_K2::PC_Boolean:UEdGraphSchema_K2::PC_Exec)||!PortType(P->PinType))return false;}
            }
            else if(V.Class==UK2Node_ExecutionSequence::StaticClass())
            {
                if(N->Pins.Num()<3||N->Pins.Num()>256)return false;auto* In=Named(N,UEdGraphSchema_K2::PN_Execute,EGPD_Input);if(!In||In->PinType.PinCategory!=UEdGraphSchema_K2::PC_Exec||!PortType(In->PinType))return false;
                for(int32 I=0;I<N->Pins.Num()-1;++I){auto* P=static_cast<UK2Node_ExecutionSequence*>(N)->GetThenPinGivenIndex(I);if(!P||P->Direction!=EGPD_Output||P->PinName!=FName(*FString::Printf(TEXT("%s_%d"),*UEdGraphSchema_K2::PN_Then.ToString(),I))||P->PinType.PinCategory!=UEdGraphSchema_K2::PC_Exec||!PortType(P->PinType))return false;}
            }
            for(auto* P:N->Pins)
            {
                TSet<UEdGraphPin*> Peers;for(auto* Q:P->LinkedTo){if(++LinksSeen>MaxLinks||Peers.Contains(Q)||!OwnedPins.Contains(Q)||Q->Direction==P->Direction||!Type(Q->PinType,P->PinType))return false;Peers.Add(Q);if(P->Direction==EGPD_Output){auto* Degree=Indegree.Find(OwnedPins.FindRef(Q));if(!Degree)return false;++*Degree;}}
            }
        }
        // Bounded iterative topology check, no recursive SDK traversal on an unseen cyclic body.
        TArray<UEdGraphNode*> Queue;for(const auto& V:Indegree)if(V.Value==0)Queue.Add(V.Key);
        for(int32 I=0;I<Queue.Num();++I)for(auto* P:Queue[I]->Pins)if(P->Direction==EGPD_Output)for(auto* Q:P->LinkedTo){auto* Degree=Indegree.Find(OwnedPins.FindRef(Q));if(!Degree||*Degree<=0)return false;if(--*Degree==0)Queue.Add(OwnedPins.FindRef(Q));}
        return Queue.Num()==Indegree.Num();
    }
    bool Inventory()
    {
        OwnedPins.Reset();TArray<UEdGraphNode*> All;
        TArray<UEdGraph*> ScopeGraphs{Source.Get()};for(const auto& C:Composites)ScopeGraphs.Add(C.Graph.Get());if(Destination)ScopeGraphs.Add(Destination);
        for(UEdGraph* G:ScopeGraphs)if(G)
        {
            if(!Loaded(G)||G->Nodes.Num()>MaxNodes)return Stop(TEXT("capture_owned_nodes"));
            for(const auto& Slot:G->Nodes){if(!Slot.IsResolved())return Stop(TEXT("capture_unresolved_node"));auto* N=Slot.Get();Holds.Emplace(N);if(!Loaded(N)||N->GetOuter()!=G||All.Contains(N))return Stop(TEXT("capture_owned_node_role"));All.Add(N);}
        }
        for(auto* N:All)for(auto* P:N->Pins){if(!P||OwnedPins.Contains(P)||OwnedPins.Num()>=MaxPins)return Stop(TEXT("capture_owned_pin_inventory"));OwnedPins.Add(P,N);}
        return true;
    }
    bool PlanRename(const FString& Name)
    {
        if(!Plain())return false;auto* V=Role(Gateway);if(!V)return Stop(TEXT("rename_gateway_missing"));V->Reference.SetMemberName(FName(*Name));return true;
    }
    // Copied aggregate boundary: SDK keeps links inside the entire selection and thunks only outside links.
    bool PlanBoundary(const TSet<UEdGraphNode*>& Selection)
    {
        TArray<UEdGraphPin*> Pins;bool HasExecInput=false;
        for(auto* N:Selection)
        {
            if(N==EventTemplate.Get())continue;
            TArray<TPair<UEdGraphNode*,FGuid>> Outputs;
            for(auto* P:N->Pins)
            {
                bool Outside=false;for(auto* Q:P->LinkedTo)if(OwnedPins.FindRef(Q)!=EventTemplate.Get()&&!Selection.Contains(OwnedPins.FindRef(Q))){Outside=true;break;}
                if(Outside){if(!PortType(P->PinType))return Fail(R,TEXT("function_extract_composite_boundary_pending"),TEXT("Aggregate boundary requires the existing primitive/exec port profile; wider original pin types remain incomplete."));Pins.Add(P);if(P->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec&&P->Direction==EGPD_Input)HasExecInput=true;}
                else if((P->LinkedTo.IsEmpty()||(EventTemplate&&P->LinkedTo.Num()==1&&OwnedPins.FindRef(P->LinkedTo[0])==EventTemplate.Get()))&&P->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec)
                {if(P->Direction==EGPD_Input)AutoEntry.Add({N,P->PinId});else Outputs.Add({N,P->PinId});}
            }
            // SDK visits this same TSet and only the first node with unlinked outputs fills the empty return port.
            if(AutoReturn.IsEmpty()&&!Outputs.IsEmpty())AutoReturn=MoveTemp(Outputs);
        }
        if(HasExecInput)AutoEntry.Reset();
        // Same copied-coordinate ordering as BlueprintEditor::SortPinsByConnectionPosition; no virtual observers.
        auto Less=[&](const UEdGraphPin* A,const UEdGraphPin* B)->bool
        {
            if(A->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec)return true;
            if(B->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec)return false;
            auto* NA=OwnedPins.FindRef(const_cast<UEdGraphPin*>(A));auto* NB=OwnedPins.FindRef(const_cast<UEdGraphPin*>(B));
            if(NA->NodePosY!=NB->NodePosY)return NA->NodePosY<NB->NodePosY;
            if(NA->NodePosX!=NB->NodePosX)return NA->NodePosX<NB->NodePosX;
            return NA->Pins.IndexOfByKey(A)<NB->Pins.IndexOfByKey(B);
        };
        auto MinPeer=[&](UEdGraphPin* P)->UEdGraphPin*{auto* M=P->LinkedTo[0];for(int32 I=1;I<P->LinkedTo.Num();++I)if(Less(P->LinkedTo[I],M))M=P->LinkedTo[I];return M;};
        Pins.Sort([&](const UEdGraphPin& A,const UEdGraphPin& B){return Less(MinPeer(const_cast<UEdGraphPin*>(&A)),MinPeer(const_cast<UEdGraphPin*>(&B)));});
        TSet<FName> Used;Used.Add(UEdGraphSchema_K2::PN_Execute);Used.Add(UEdGraphSchema_K2::PN_Then);Used.Add(UEdGraphSchema_K2::PN_Self);
        for(auto* P:Pins)
        {
            FBoundary V;V.Node=OwnedPins.FindRef(P);V.Pin=P->PinId;V.Name=P->PinName;V.Direction=P->Direction;V.Type=P->PinType;
            if(P->PinType.PinCategory!=UEdGraphSchema_K2::PC_Exec)
            {
                // Template parameters exist on Entry before Gateway reconstruction; reject ambiguous name collisions instead of inventing a different SDK name.
                if(EventPorts.ContainsByPredicate([&](const FEventPort& E){return E.Name==V.Name;}))return Fail(R,TEXT("function_extract_composite_event_template_pending"),TEXT("Template/aggregate parameter name collisions remain incomplete; no extraction writer."));
                int32 Index=1;while(Used.Contains(V.Name)){++Index;V.Name=FName(*FString::Printf(TEXT("%s%d"),*P->PinName.ToString(),Index));}if(EventPorts.ContainsByPredicate([&](const FEventPort& E){return E.Name==V.Name;}))return Fail(R,TEXT("function_extract_composite_event_template_pending"),TEXT("Generated aggregate/template parameter name collisions remain incomplete; no extraction writer."));Used.Add(V.Name);
                if(P->Direction==EGPD_Output)KeepReturn=true;
            }
            Boundary.Add(MoveTemp(V));
        }
        return true;
    }
    bool ReturnedSignature(UEdGraphNode* Entry,UEdGraphNode* Result,UEdGraphNode* Call)
    {
        int32 Inputs=EventPorts.Num(),Outputs=0;for(const auto& B:Boundary)if(B.Type.PinCategory!=UEdGraphSchema_K2::PC_Exec){if(B.Direction==EGPD_Input)++Inputs;else ++Outputs;}
        if(!Entry||Entry->Pins.Num()!=1+Inputs||bool(Result)!=KeepReturn||(Result&&Result->Pins.Num()!=1+Outputs)||Call->Pins.Num()!=3+Inputs+Outputs)return Stop(TEXT("aggregate_signature_pin_count"));
        auto Exec=[&](UEdGraphNode* N,FName Name,EEdGraphPinDirection Direction)->bool{auto* P=Named(N,Name,Direction);return P&&P->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec&&PortType(P->PinType);};
        if(!Exec(Entry,UEdGraphSchema_K2::PN_Then,EGPD_Output)||!Exec(Call,UEdGraphSchema_K2::PN_Execute,EGPD_Input)||!Exec(Call,UEdGraphSchema_K2::PN_Then,EGPD_Output)||(Result&&!Exec(Result,UEdGraphSchema_K2::PN_Execute,EGPD_Input)))return Stop(TEXT("aggregate_signature_exec_shape"));
        auto* Self=Named(Call,UEdGraphSchema_K2::PN_Self,EGPD_Input);
        if(!Self||Self->PinType.PinCategory!=UEdGraphSchema_K2::PC_Object||!WeakReadable(Self->PinType.PinSubCategoryObject))return Stop(TEXT("aggregate_signature_self_shape"));
        auto* SelfClass=Cast<UClass>(Self->PinType.PinSubCategoryObject.Get());
        // CreateSelfPin uses PSC_Self with no class object for an own Blueprint function.
        // Preserve that SDK role separately from the existing explicit own-class form.
        const auto* CallNode=static_cast<UK2Node_CallFunction*>(Call);
        UObject* SelfReferenceParent=nullptr;
        const bool TrueSelf=Self->PinType.PinSubCategory==UEdGraphSchema_K2::PSC_Self
            &&Self->PinType.PinSubCategoryObject.IsExplicitlyNull()
            &&CallNode->FunctionReference.IsSelfContext()&&!CallNode->FunctionReference.IsLocalScope()
            &&ReferenceParent(CallNode->FunctionReference,SelfReferenceParent)&&!SelfReferenceParent
            &&!CallNode->FunctionReference.IsDeprecated()&&Destination
            &&CallNode->FunctionReference.GetMemberName()==Destination->GetFName()
            &&Call->GetOuter()==Source.Get()&&Source->GetOuter()==BP.Get();
        if(!TrueSelf&&(!Loaded(SelfClass)||!SelfClass->ClassGeneratedBy.IsResolved()||SelfClass->ClassGeneratedBy.Get()!=BP.Get()))return Stop(TEXT("aggregate_signature_self_owner"));
        for(const auto& B:Boundary)if(B.Type.PinCategory!=UEdGraphSchema_K2::PC_Exec)
        {
            auto* Outside=Named(Call,B.Name,B.Direction);auto* Inside=Named(B.Direction==EGPD_Input?Entry:Result,B.Name,B.Direction==EGPD_Input?EGPD_Output:EGPD_Input);
            if(!Outside||!Inside||!Type(Outside->PinType,B.Type)||!Type(Inside->PinType,B.Type))return Stop(TEXT("aggregate_signature_data_shape"));
        }
        if(EventTemplate)
        {
            auto* Editable=Cast<UK2Node_EditablePinBase>(Entry);if(!Editable)return Stop(TEXT("event_template_entry_type"));
            auto Rows=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Ports;
            for(const auto& E:EventPorts)
            {
                auto* I=Named(Entry,E.Name,EGPD_Output);auto* O=Named(Call,E.Name,EGPD_Input);
                const auto* U=Editable->UserDefinedPins.FindByPredicate([&](const TSharedPtr<FUserPinInfo>& V){return V&&V->PinName==E.Name;});
                // UE58 creates a new FUserPinInfo with an empty default, not a copy of the event's authored default.
                if(!I||!O||!Type(I->PinType,E.Type)||!Type(O->PinType,E.Type)||!I->LinkedTo.IsEmpty()||!O->LinkedTo.IsEmpty()||!U||(*U)->DesiredPinDirection!=EGPD_Output||!Type((*U)->PinType,E.Type)||!(*U)->PinDefaultValue.IsEmpty()||!I->AutogeneratedDefaultValue.IsEmpty())return Stop(TEXT("event_template_generated_user_pin"));
                auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("name"),E.Name.ToString());P->SetStringField(TEXT("original_event_parameter"),E.SourceName.ToString());P->SetStringField(TEXT("original_source_pin_id"),E.SourcePin.ToString(EGuidFormats::Digits));P->SetStringField(TEXT("original_consumer_pin_id"),E.PeerPin.ToString(EGuidFormats::Digits));P->SetNumberField(TEXT("original_connection_ordinal"),E.SourceConnection);P->SetStringField(TEXT("original_event_default"),E.OriginalDefault);P->SetStringField(TEXT("generated_entry_default"),(*U)->PinDefaultValue);P->SetBoolField(TEXT("event_default_copied"),false);P->SetBoolField(TEXT("original_data_link_disconnected_by_sdk"),true);Ports.Add(MakeShared<FJsonValueObject>(P));
            }
            Rows->SetArrayField(TEXT("ports"),Ports);Rows->SetBoolField(TEXT("sdk_event_template_removed"),true);Rows->SetBoolField(TEXT("defaults_or_links_repaired_by_plugin"),false);R.Data->SetObjectField(TEXT("composite_event_template_result"),Rows);
        }
        return true;
    }
    bool Move(UEdGraph* F,UEdGraphNode* Call)
    {
        if(Stopped)return false;Holds.Emplace(F);Holds.Emplace(Call);
        // No owning reads of returned objects until the captured source/context roles are live.
        if(!OwnerCurrent())return false;
        if(!Loaded(F)||!Loaded(Call)||F->GetOuter()!=BP.Get()||F->GetClass()!=UEdGraph::StaticClass()||!F->Schema.GetGCPtr().IsResolved()||F->Schema.GetGCPtr().Get()!=UEdGraphSchema_K2::StaticClass()||Call->GetClass()!=UK2Node_CallFunction::StaticClass()||F->Nodes.Num()!=Selected.Num()-(EventTemplate?1:0)+1+(KeepReturn?1:0))return Stop(TEXT("move_returned_objects"));
        Destination=F;Gateway=Call;Moved=true;if(!Inventory())return false;int32 Entry=0,Result=0;UEdGraphNode* EntryNode=nullptr;UEdGraphNode* ResultNode=nullptr;
        for(const auto& Slot:F->Nodes){if(!Slot.IsResolved())return Stop(TEXT("destination_unresolved_node"));DestinationSlots.Add(Slot.Get());}
        for(const auto& Slot:F->Nodes)
        {
            if(!Slot.IsResolved())return Stop(TEXT("move_returned_node_slot"));UEdGraphNode* N=Slot.Get();Holds.Emplace(N);
            if(IsSelected(N))continue;
            if(!Loaded(N)||(N->GetClass()!=UK2Node_FunctionEntry::StaticClass()&&N->GetClass()!=UK2Node_FunctionResult::StaticClass()))return Stop(TEXT("move_returned_terminator"));
            if(N->GetClass()==UK2Node_FunctionEntry::StaticClass()){++Entry;EntryNode=N;}else{++Result;ResultNode=N;}
            if(!CaptureNode(N,F,true))return false;
        }
        if(Entry!=1||Result!=(KeepReturn?1:0)||!ReturnedSignature(EntryNode,ResultNode,Call)||!CaptureNode(Call,Source.Get(),true))return Stop(TEXT("move_returned_signature"));
        R.Data->SetBoolField(TEXT("composite_sdk_move_returned"),true);return Reacquire();
    }
    bool Bind(UBlueprint* B,UEdGraph* G,const TSet<UEdGraphNode*>& Selection)
    {
        BP.Reset(B);Editor.Reset(GEditor);World.Reset(GEditor?GEditor->GetEditorWorldContext().World():nullptr);Source.Reset(G);
        if(!Loaded(B)||!Loaded(G)||!Loaded(Editor.Get())||!Loaded(World.Get()))return Stop(TEXT("initial_owner_context"));
        EditorRole.Capture(Editor.Get());WorldRole.Capture(World.Get());BlueprintRole.Capture(B);SourceRole.Capture(G);if(!OwnerCurrent())return false;
        for(auto* N:Selection)Selected.Add(N);
        if(B->ParentClass.Get()!=AActor::StaticClass()||(!B->FunctionGraphs.Contains(G)&&!B->UbergraphPages.Contains(G))||!B->ImplementedInterfaces.IsEmpty()||!B->GetExtensions().IsEmpty())return Fail(R,TEXT("function_extract_composite_profile_pending"),TEXT("Direct Actor own event/function graph required; broader valid owners remain incomplete."));
        // The SDK moves only selected gateway nodes and their immediate SubGraphs.
        // Observe the complete closed owned hierarchy before any virtual/native query;
        // never flatten descendants or infer that every nested graph is function-safe.
        struct FOwnedVisit { UEdGraph* Graph;int32 Depth; };
        TArray<FOwnedVisit> Pending;Pending.Add({G,0});TSet<UEdGraph*> SeenGraphs;SeenGraphs.Add(G);
        int32 NestedBodies=0,DeepestBody=0;
        for(int32 VisitIndex=0;VisitIndex<Pending.Num();++VisitIndex)
        {
            const auto Visit=Pending[VisitIndex];auto* ParentGraph=Visit.Graph;
            if(!Loaded(ParentGraph)||ParentGraph->GetClass()!=UEdGraph::StaticClass()||!ParentGraph->Schema.GetGCPtr().IsResolved()||ParentGraph->Schema.GetGCPtr().Get()!=UEdGraphSchema_K2::StaticClass()||ParentGraph->GetPackage()!=B->GetPackage()||ParentGraph->Nodes.Num()>MaxNodes||ParentGraph->SubGraphs.Num()>MaxGraphs)return Fail(R,TEXT("function_extract_composite_profile_pending"),TEXT("Composite closure requires bounded loaded own stock K2 graphs; wider original owners remain incomplete."));
            TArray<UEdGraph*> Children;
            for(const auto& Child:ParentGraph->SubGraphs)
            {if(!Child.IsResolved())return Stop(TEXT("initial_subgraph_unresolved"));if(Children.Contains(Child.Get()))return Fail(R,TEXT("function_extract_composite_profile_pending"),TEXT("Duplicate immediate Composite body membership is not admitted."));Children.Add(Child.Get());}
            int32 ImmediateBodies=0;
            for(const auto& Slot:ParentGraph->Nodes)
            {
                if(!Slot.IsResolved())return Stop(TEXT("initial_source_node_unresolved"));auto* O=Slot.Get();
                if(!Loaded(O)||O->GetOuter()!=ParentGraph)return Stop(TEXT("initial_source_node_role"));
                if(ParentGraph==G&&IsSelected(O)&&O->IsA<UK2Node_CustomEvent>())
                {
                    if(O->GetClass()!=UK2Node_CustomEvent::StaticClass()||EventTemplate)return Fail(R,TEXT("function_extract_composite_event_template_pending"),TEXT("Mixed Composite extraction currently admits one exact native CustomEvent template; multiple/custom event variants remain incomplete."));
                    EventTemplate.Reset(static_cast<UK2Node_CustomEvent*>(O));
                }
                if(auto* C=Cast<UK2Node_Composite>(O))
                {
                    if(C->GetClass()!=UK2Node_Composite::StaticClass()||!C->BoundGraph.IsResolved())return Stop(TEXT("initial_composite_binding"));
                    FCompositeRole V;V.Node.Reset(C);V.Graph.Reset(C->BoundGraph.Get());V.ParentGraph=ParentGraph;V.Depth=Visit.Depth+1;V.Selected=ParentGraph==G&&IsSelected(C);
                    if(!Loaded(V.Graph.Get())||V.Graph->GetOuter()!=C||V.Graph->Nodes.Num()<2||V.Graph->Nodes.Num()>MaxNodes||!Children.Contains(V.Graph.Get())||SeenGraphs.Contains(V.Graph.Get())||V.Depth>=MaxDepth||SeenGraphs.Num()>=MaxGraphs)return Fail(R,TEXT("function_extract_composite_profile_pending"),TEXT("Each Composite must own one distinct acyclic bounded body in its immediate parent SubGraphs; deeper/custom/out-of-budget original variants remain incomplete."));
                    for(const auto& Node:V.Graph->Nodes){if(!Node.IsResolved())return Stop(TEXT("initial_body_node_unresolved"));V.Slots.Add(Node.Get());}
                    SeenGraphs.Add(V.Graph.Get());Pending.Add({V.Graph.Get(),V.Depth});++ImmediateBodies;
                    if(V.Depth>1)++NestedBodies;DeepestBody=FMath::Max(DeepestBody,V.Depth);
                    Composites.Add(MoveTemp(V));
                }
            }
            if(ImmediateBodies!=Children.Num())return Fail(R,TEXT("function_extract_composite_profile_pending"),TEXT("Each observed SubGraphs list must consist exactly of its immediate Composite-owned bodies; broader original graph forms remain incomplete."));
        }
        if(Composites.IsEmpty())return Stop(TEXT("selected_composite_missing"));
        // Retain exactly the SDK TSet order for destination SubGraphs and the unlinked exec-return projection.
        for(auto* N:Selection)for(const auto& C:Composites)if(C.Selected&&C.ParentGraph==G&&C.Node.Get()==N)MovedSubgraphs.Add(C.Graph.Get());
        if(MovedSubgraphs.IsEmpty())return Stop(TEXT("selected_composite_missing"));
        Composite.Reset(Composites[0].Node.Get());Bound.Reset(Composites[0].Graph.Get());
        if(!B->SimpleConstructionScript.IsResolved())return Stop(TEXT("initial_scs_unresolved"));SCS.Reset(B->SimpleConstructionScript.Get());
        if(!Loaded(SCS.Get())||SCS->GetAllNodes().Num()!=1)return Fail(R,TEXT("function_extract_composite_scs_pending"),TEXT("First profile retains one exact Scene SCS root; broader SCS remains incomplete."));
        RootNode.Reset(SCS->GetAllNodes()[0]);if(!Loaded(RootNode.Get())||!RootNode->ComponentTemplate.IsResolved())return Stop(TEXT("initial_root_unresolved"));RootTemplate.Reset(Cast<USceneComponent>(RootNode->ComponentTemplate.Get()));
        if(!Loaded(RootTemplate.Get())||RootTemplate->GetClass()!=USceneComponent::StaticClass())return Fail(R,TEXT("function_extract_composite_scs_pending"),TEXT("Exact native Scene root required."));RootGuid=RootNode->VariableGuid;RootName=RootNode->GetVariableName();RootLocation=RootTemplate->GetRelativeLocation();RootRotation=RootTemplate->GetRelativeRotation();RootScale=RootTemplate->GetRelativeScale3D();RootMobility=RootTemplate->Mobility;
        RootPath=RootTemplate->GetPathName();TemplateName=RootTemplate->GetFName();auto Gen=B->GeneratedClass;auto Skel=B->SkeletonGeneratedClass;
        if(!Gen.GetGCPtr().IsResolved()||!Skel.GetGCPtr().IsResolved())return Stop(TEXT("initial_generated_handles"));Generated.Reset(Gen.GetGCPtr().Get());Skeleton.Reset(Skel.GetGCPtr().Get());if(!Loaded(Generated.Get())||!Loaded(Skeleton.Get()))return Stop(TEXT("initial_generated_roles"));GeneratedPath=Generated->GetPathName();SkeletonPath=Skeleton->GetPathName();GeneratedMeta=Generated->GetClass();SkeletonMeta=Skeleton->GetClass();Holds.Emplace(GeneratedMeta);Holds.Emplace(SkeletonMeta);
        Functions=Copy(B->FunctionGraphs);Macros=Copy(B->MacroGraphs);Events=Copy(B->UbergraphPages);Delegates=Copy(B->DelegateSignatureGraphs);
        TArray<UEdGraph*> All;if(!::Cubeless::UE58::BlueprintFunctionExtraction::Graphs(B,All,R))return false;
        // Only the source selection and its closed Composite hierarchy participate; other graph roles remain captured.
        for(auto* A:All){FGraph V;V.Hold.Reset(A);V.Guid=A->GraphGuid;V.Name=A->GetFName();V.Children=Copy(A->SubGraphs);Graphs.Add(MoveTemp(V));}
        if(!Inventory())return false;int32 Calls=0,BodyNodes=0;
        for(const auto& Slot:G->Nodes)SourceSlots.Add(Slot.Get());
        TArray<UEdGraph*> CaptureGraphs{G};for(const auto& C:Composites)CaptureGraphs.Add(C.Graph.Get());
        for(auto* A:CaptureGraphs)for(const auto& Slot:A->Nodes)
        {
            if(!Slot.IsResolved())return Stop(TEXT("capture_node_unresolved"));auto* O=Slot.Get();
            if(!Loaded(O)||!O->NodeGuid.IsValid()||O->Pins.Num()>256||Nodes.Num()>=MaxNodes)return Fail(R,TEXT("function_extract_composite_profile_pending"),TEXT("Bounded existing current node/pins required across source and owned bodies."));
            if(A!=G)
            {
                ++BodyNodes;
                if(O->GetClass()!=UK2Node_Tunnel::StaticClass()&&O->GetClass()!=UK2Node_CallFunction::StaticClass()&&O->GetClass()!=UK2Node_IfThenElse::StaticClass()&&O->GetClass()!=UK2Node_ExecutionSequence::StaticClass()&&O->GetClass()!=UK2Node_Composite::StaticClass())return Fail(R,TEXT("function_extract_composite_inner_pending"),TEXT("Finite native pure-call/Branch/Sequence/Tunnel2/Composite body proof is reused at each owned depth; custom/other body classes remain valid incomplete variants."));
            }
            if(!CaptureNode(O,A))return false;
        }
        // All source/target/body values and aggregate boundaries precede native-source observations.
        if(EventTemplate)
        {
            const auto* E=Role(EventTemplate.Get());if(!E)return Stop(TEXT("event_template_capture_missing"));TSet<FName> Names,EntryNames;EntryNames.Add(UEdGraphSchema_K2::PN_Then);int32 Execs=0,SourceParameters=0,FanoutParameters=0;
            for(const auto& V:E->Values)
            {
                if(V.Direction!=EGPD_Output)return Fail(R,TEXT("function_extract_composite_event_template_pending"),TEXT("Event template must use stock output-only primitive parameters; wider original templates remain incomplete."));
                if(V.Type.PinCategory==UEdGraphSchema_K2::PC_Delegate){if(!V.Links.IsEmpty())return Fail(R,TEXT("function_extract_composite_event_template_pending"),TEXT("Linked event delegates remain incomplete; no writer dispatched."));continue;}
                const bool Exec=V.Type.PinCategory==UEdGraphSchema_K2::PC_Exec;
                if(!PortType(V.Type)||(Exec&&V.Links.Num()>1))return Fail(R,TEXT("function_extract_composite_event_template_pending"),TEXT("Primitive data fan-out uses the captured finite link bound; exec fan-out and wider signatures remain incomplete."));
                if(Exec)++Execs;
                const FUserPinInfo* Definition=nullptr;
                if(!Exec&&!V.Links.IsEmpty())
                {
                    if(V.Name==UEdGraphSchema_K2::PN_Execute||V.Name==UEdGraphSchema_K2::PN_Then||V.Name==UEdGraphSchema_K2::PN_Self||Names.Contains(V.Name))return Fail(R,TEXT("function_extract_composite_event_template_pending"),TEXT("Ambiguous/reserved source event parameter names remain incomplete."));
                    Definition=E->UserPins.FindByPredicate([&](const FUserPinInfo& U){return U.PinName==V.Name;});
                    if(!Definition||Definition->DesiredPinDirection!=EGPD_Output||!Type(Definition->PinType,V.Type))return Fail(R,TEXT("function_extract_composite_event_template_pending"),TEXT("Each event data parameter requires its matching authored output user-pin definition."));
                    Names.Add(V.Name);++SourceParameters;if(V.Links.Num()>1)++FanoutParameters;
                }
                int32 Connection=0;
                for(const auto& L:V.Links)
                {
                    ++Connection;const auto* Peer=Nodes.FindByPredicate([&](const FNodeRole& N){return N.Guid==L.Key;});const auto* Pin=Peer?Peer->Values.FindByPredicate([&](const FValue& P){return P.Id==L.Value;}):nullptr;
                    if(!Peer||!Pin||!IsSelected(Peer->Hold.Get())||Peer->Graph!=G||Pin->Direction!=EGPD_Input||Pin->Links.Num()!=1||!Type(Pin->Type,V.Type))return Fail(R,TEXT("function_extract_composite_event_template_pending"),TEXT("Each fan-out consumer must be an exclusively linked selected source-graph input; external links remain incomplete."));
                    if(!Exec)
                    {
                        // SDK ExtractEventTemplateForFunction visits copied links and creates
                        // one Entry user pin per selected consumer. Gateway has only execute,
                        // then and self at this point; none of the admitted data names collide.
                        // EditablePinBase then applies the actual unique-name algorithm against
                        // Entry's prior pins (Data, Data2, Data3...), independently of peer order.
                        FName GeneratedName=V.Name;int32 Suffix=1;while(EntryNames.Contains(GeneratedName)){++Suffix;const FString CandidateName=FString::Printf(TEXT("%s%d"),*V.Name.ToString(),Suffix);if(CandidateName.Len()>=NAME_SIZE)return Fail(R,TEXT("function_extract_composite_event_template_name_bound"),TEXT("SDK fan-out suffix would exceed the native FName limit; no extraction writer."));GeneratedName=FName(*CandidateName);}
                        EntryNames.Add(GeneratedName);EventPorts.Add({GeneratedName,V.Type,V.Id,Peer->Hold.Get(),Pin->Id,Definition->PinDefaultValue,V.Name,Connection});
                    }
                }
            }
            if(Execs!=1)return Fail(R,TEXT("function_extract_composite_event_template_pending"),TEXT("Exactly one stock event exec output is required."));
            R.Data->SetBoolField(TEXT("composite_event_template_admitted"),true);R.Data->SetNumberField(TEXT("composite_event_template_parameter_count"),EventPorts.Num());R.Data->SetNumberField(TEXT("composite_event_template_source_parameter_count"),SourceParameters);R.Data->SetNumberField(TEXT("composite_event_template_data_fanout_parameter_count"),FanoutParameters);R.Data->SetStringField(TEXT("composite_event_template_sdk_semantics"),TEXT("SDK creates one uniquely named Entry parameter per selected data consumer with empty user defaults, removes the event and disconnects every original event data link. Plugin does not copy defaults or repair links. External/exec-fanout/custom signatures remain incomplete."));
        }
        if(!Plain(false)||!PlanBoundary(Selection))return false;
        if(EventTemplate)
        {
            int32 Inputs=EventPorts.Num(),Outputs=0;for(const auto& V:Boundary)if(V.Type.PinCategory!=UEdGraphSchema_K2::PC_Exec){if(V.Direction==EGPD_Input)++Inputs;else ++Outputs;}
            // Existing returned-node capture admits at most 256 pins. Reject the
            // predictable fan-out expansion before invoking the owned toolkit.
            if(Inputs+Outputs>253)return Fail(R,TEXT("function_extract_composite_event_template_pin_budget"),TEXT("Expanded template and aggregate signature must fit the existing 256-pin gateway observation bound; no extraction writer."));
            R.Data->SetNumberField(TEXT("composite_event_template_predicted_gateway_pins"),3+Inputs+Outputs);
        }
        TArray<UEdGraphNode*> NativeCallNodes;for(const auto& N:Nodes)if(N.Class==UK2Node_CallFunction::StaticClass()&&(N.Graph!=G||IsSelected(N.Hold.Get())))NativeCallNodes.Add(N.Hold.Get());
        for(auto* O:NativeCallNodes){if(!Plain(false))return false;if(!Loaded(O))return Stop(TEXT("native_call_slot"));if(!BindNativeCall(static_cast<UK2Node_CallFunction*>(O))){if(R.ErrorCode.IsEmpty())Fail(R,TEXT("function_extract_composite_native_signature_pending"),TEXT("Selected/body native pure function byvalue primitive signature is outside this finite source proof; no extraction writer."));return false;}++Calls;}
        for(const auto& C:Composites)
        {
            Composite.Reset(C.Node.Get());Bound.Reset(C.Graph.Get());
            if(!Qualify())return Fail(R,TEXT("function_extract_composite_closed_shape_pending"),TEXT("Each owned body requires the finite acyclic reciprocal typed Tunnel2/native pure-call/Branch/Sequence/Composite proof; wider original bodies remain incomplete."));
        }
        R.Data->SetNumberField(TEXT("composite_native_call_count"),Calls);R.Data->SetNumberField(TEXT("composite_body_node_count"),BodyNodes);R.Data->SetNumberField(TEXT("composite_unique_native_sources"),NativeSources.Num());
        R.Data->SetNumberField(TEXT("composite_nested_body_count"),NestedBodies);R.Data->SetNumberField(TEXT("composite_max_owned_depth"),DeepestBody);R.Data->SetBoolField(TEXT("composite_recursive_owned_closure_observed"),true);
        Ready=true;
        if(!Plain(false))return false;
        for(auto& V:Nodes)for(auto& Q:V.Values){const FText T=Q.Text;Q.TextDisplay=T.ToString();if(!Plain(false))return false;const FText F=Q.Friendly;Q.FriendlyDisplay=F.ToString();if(!Plain(false))return false;}
        R.Data->SetBoolField(TEXT("native_composite_function_extraction"),true);R.Data->SetNumberField(TEXT("composite_selected_count"),MovedSubgraphs.Num());R.Data->SetNumberField(TEXT("composite_preserved_body_count"),Composites.Num());R.Data->SetNumberField(TEXT("aggregate_boundary_pin_count"),Boundary.Num());R.Data->SetNumberField(TEXT("composite_math_count"),Calls);
        return Plain();
    }
};

class FOwnedExtractor final:public FBlueprintEditor
{
public:
    bool bCloseObserved=false;
    bool CanExtract(TSet<UEdGraphNode*>& Nodes) const { return CanCollapseSelectionToFunction(Nodes); }
    UEdGraph* Extract(const TSharedPtr<SGraphEditor>& Editor,TSet<UEdGraphNode*>& Nodes,UEdGraphNode*& Call)
    { return CollapseSelectionToFunction(Editor,Nodes,Call); }
    virtual void OnClose() override { FBlueprintEditor::OnClose();bCloseObserved=true; }
};
// A CloseWindow true return alone does not prove OnClose ran. Retain an unconfirmed owned toolkit.
TArray<TSharedPtr<FOwnedExtractor>> RetainedEditors;
struct FContext
{
    TUniquePtr<FPXComposite> PX;
    FResult& R;UBlueprint* BP=nullptr;UEdGraph* Source=nullptr;FIdentity Blueprint,Graph,Parent;FInventory Before;
    TSharedPtr<FOwnedExtractor> Editor;TSharedPtr<SGraphEditor> GraphEditor;
    UEdGraph* Diagnostic=nullptr;FIdentity DiagnosticIdentity;bool DiagnosticAdded=false,DiagnosticRemoveAttempted=false;
    bool Baseline=false,WasDirty=false,Restore=false,FunctionLibrary=false;EBlueprintStatus OriginalStatus=BS_Unknown;
    UEdGraph* ResultFunction=nullptr;UEdGraphNode* ResultGateway=nullptr;
    FIdentity ResultFunctionIdentity,ResultGatewayIdentity;FString RequestedFunction;FMemberReference ExpectedFunctionReference;
    void HoldOutputs(UEdGraph* Function,UEdGraphNode* Gateway,const FString& Name)
    {
        ResultFunction=Function;ResultGateway=Gateway;RequestedFunction=Name;
        ResultFunctionIdentity.Capture(Function);ResultGatewayIdentity.Capture(Gateway);
        ExpectedFunctionReference=CastChecked<UK2Node_CallFunction>(Gateway)->FunctionReference;
    }
    bool OutputsCurrent()
    {
        if(!Current(false)) { return false; }
        if(!ResultFunction) { return true; }
        if(!ResultFunctionIdentity.Current()||!ResultGatewayIdentity.Current()
            ||ResultFunction->GetOuter()!=BP||ResultFunction->GetPackage()!=Blueprint.Package||!BP->FunctionGraphs.Contains(ResultFunction)
            ||ResultFunction->GetName()!=RequestedFunction||ResultFunction->Nodes.Num()>MaxNodes
            ||ResultGateway->GetGraph()!=Source||!Source->Nodes.Contains(ResultGateway)||ResultGateway->GetPackage()!=Blueprint.Package)
        { return Fail(R,TEXT("function_extract_completed_output_changed"),TEXT("Returned function graph/gateway identity, requested name or membership changed after a callback.")); }
        const auto* Call=Cast<UK2Node_CallFunction>(ResultGateway);
        if(!Call||Call->FunctionReference.GetMemberName()!=ResultFunction->GetFName()||!Call->FunctionReference.IsSameReference(ExpectedFunctionReference))
        { return Fail(R,TEXT("function_extract_completed_reference_changed"),TEXT("Gateway no longer references the requested extracted function.")); }
        return true;
    }
    explicit FContext(FResult& In):R(In) {}
    bool Current(bool bSameSource=true)
    {
        if(PX&&!PX->Plain())return false;
        const bool Good=[&]()->bool
        {
        if(!IsEditorGameThread()||IsPlaySessionActive()||!FSlateApplication::IsInitialized()||!Blueprint.Current()||!Graph.Current()
            ||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bIsRegeneratingOnLoad||BP->bSuppressStructurallyModified
            ||!Parent.Current()||BP->ParentClass!=Parent.Object||!Loaded(BP->SkeletonGeneratedClass)||BP->SkeletonGeneratedClass->ClassGeneratedBy!=BP||!Source->bEditable||Source->Schema!=UEdGraphSchema_K2::StaticClass()||Source->GetSchema()!=GetDefault<UEdGraphSchema_K2>()||Source->GetTypedOuter<UBlueprint>()!=BP)
        { return Fail(R,TEXT("function_extract_owner_changed"),TEXT("Editor, source Blueprint/graph/package or compiler state changed during the operation.")); }
        if(FunctionLibrary&&(!DirectFunctionLibraryRole(BP)||Source->GetOuter()!=BP||!BP->FunctionGraphs.Contains(Source)))
        { return Fail(R,TEXT("function_extract_function_library_owner_changed"),TEXT("Direct FunctionLibrary owner or selected own function graph role changed after a callback.")); }
        TArray<UEdGraph*> All;if(!Graphs(BP,All,R)||!All.Contains(Source)) { return Fail(R,TEXT("function_extract_source_removed"),TEXT("Captured source graph is no longer in the bounded Blueprint inventory.")); }
        if(Editor.IsValid()&&(Editor->bCloseObserved||Editor->GetBlueprintObj()!=BP)) { return Fail(R,TEXT("function_extract_toolkit_owner_changed"),TEXT("Owned toolkit no longer addresses the captured Blueprint.")); }
        return !bSameSource||Before.Same(Source,R);
        }();
        if(PX&&(!Good||!PX->Plain()))return PX->Stop(TEXT("original_current_failed"));return Good;
    }
    bool RefreshPX()
    {
        if(!PX)return true;if(!PX->Reacquire())return false;
        // Rebind only previously authorized physical roles after logical equality, never values.
        if(!PX->Moved)
        {
            TMap<UEdGraphPin*,UEdGraphPin*> Map;
            for(auto& N:Before.Nodes)for(auto*& Q:N->Pins){const auto* Old=Before.Pins.FindByPredicate([&](const FPin& V){return V.Pin==Q&&V.Owner==N->Node;});if(!Old)return PX->Stop(TEXT("source_copied_pin_missing"));auto* New=PX->Find(N->Node,Old->Id);if(!New)return PX->Stop(TEXT("source_inventory_rebind"));Map.Add(Q,New);Q=New;}
            for(auto& Q:Before.Pins){auto* New=Map.FindRef(Q.Pin);if(!New)return PX->Stop(TEXT("source_inventory_pin_rebind"));Q.Pin=New;for(auto*& L:Q.Links)L=Map.FindRef(L);const auto* N=PX->Role(Q.Owner);const auto* V=N?N->Values.FindByPredicate([&](const FPXComposite::FValue& E){return E.Id==Q.Id;}):nullptr;if(V&&!V->OwnTypePath.IsEmpty()){Q.Type.PinSubCategoryObject=New->PinType.PinSubCategoryObject;Q.KeepType.Reset(New->PinType.PinSubCategoryObject.Get());}}
            Before.PinSet.Reset();for(const auto& Q:Before.Pins)Before.PinSet.Add(Q.Pin);
        }
        return true;
    }
    bool RemoveDiagnostic()
    {
        if(PX&&PX->Stopped)return false;if(PX&&!PX->Plain())return false;
        if(!Diagnostic) { return true; }
        if(!Blueprint.Current()||!DiagnosticIdentity.Current()||!IsEditorGameThread()||IsPlaySessionActive())
        { return Fail(R,TEXT("function_extract_diagnostic_cleanup_unconfirmed"),TEXT("Diagnostic identity changed; no further cleanup mutation was dispatched.")); }
        if(DiagnosticAdded)
        {
            if(!BP->FunctionGraphs.Contains(Diagnostic)) { return Fail(R,TEXT("function_extract_diagnostic_membership_changed"),TEXT("Diagnostic graph membership changed before cleanup.")); }
            if(DiagnosticRemoveAttempted) { return Fail(R,TEXT("function_extract_diagnostic_cleanup_unconfirmed"),TEXT("Diagnostic cleanup was already attempted; automatic cleanup retry is forbidden.")); }
            R.Stage=TEXT("diagnostic_cleanup");Count(R,TEXT("diagnostic_remove_calls"));DiagnosticRemoveAttempted=true;
            if(PX)PX->DiagnosticMember=false;FBlueprintEditorUtils::RemoveGraph(BP,Diagnostic);
            if(!RefreshPX())return false;
            // SDK RemoveGraph deliberately renames the graph from Blueprint to Blueprint->GetOuter(),
            // removes membership and marks it transient. Do not demand its old path/outer here.
            if(!Blueprint.Current()||!Loaded(Diagnostic)||Diagnostic->GetClass()!=DiagnosticIdentity.Class
                ||Diagnostic->GetOuter()!=Blueprint.Outer||Diagnostic->GetPackage()!=Blueprint.Package||BP->FunctionGraphs.Contains(Diagnostic))
            { return Fail(R,TEXT("function_extract_diagnostic_cleanup_unconfirmed"),TEXT("Diagnostic removal returned without the captured removal postcondition.")); }
        }
        R.Data->SetStringField(TEXT("diagnostic_removed_path"),Diagnostic->GetPathName());
        Count(R,TEXT("diagnostic_mark_garbage_calls"));DiagnosticIdentity.Keep.Reset();Diagnostic->MarkAsGarbage();Diagnostic=nullptr;DiagnosticAdded=false;
        R.Data->SetBoolField(TEXT("diagnostic_cleanup_confirmed"),true);return true;
    }
    void Finish()
    {
        if(PX&&PX->Stopped){if(Editor.IsValid())RetainedEditors.Add(Editor);R.Data->SetBoolField(TEXT("cleanup_required"),true);return;}
        const FString PriorCode=R.ErrorCode,PriorError=R.Error,PriorStage=R.Stage;
        R.Data->SetStringField(TEXT("operation_stage_before_cleanup"),PriorStage);
        bool Clean=RemoveDiagnostic();if(PX&&PX->Stopped){if(Editor.IsValid())RetainedEditors.Add(Editor);return;}
        if(Editor.IsValid())
        {
            const bool AlreadyClosed=Editor->bCloseObserved;R.Data->SetBoolField(TEXT("owned_editor_already_closed"),AlreadyClosed);
            bool Returned=false;
            if(!AlreadyClosed)
            {
                R.Data->SetBoolField(TEXT("owned_editor_close_requested"),true);Count(R,TEXT("owned_editor_close_calls"));
                Returned=Editor->CloseWindow(EAssetEditorCloseReason::AssetEditorHostClosed);
            }
            R.Data->SetBoolField(TEXT("owned_editor_close_return"),Returned);
            R.Data->SetBoolField(TEXT("owned_editor_OnClose_observed"),Editor->bCloseObserved);
            if(!Editor->bCloseObserved) { RetainedEditors.Add(Editor);Clean=false;Fail(R,TEXT("function_extract_toolkit_cleanup_unconfirmed"),TEXT("Owned toolkit was retained because CloseWindow did not confirm OnClose.")); }
            GraphEditor.Reset();Editor.Reset();
            if(Blueprint.Object&&!OutputsCurrent()) { Clean=false; }
        }
        if(PX&&PX->Stopped)return;
        if(Baseline&&Blueprint.Current())
        {
            R.Data->SetStringField(TEXT("compile_status_before_restore"),Status(BP->Status));
            R.Data->SetBoolField(TEXT("package_dirty_before_restore"),Blueprint.Package->IsDirty());
            if(Restore)
            {
                // Match original status/dirty restoration only while source state is still captured.
                if(Clean&&Current())
                {
                    R.Data->SetBoolField(TEXT("status_restore_attempted"),true);BP->Status=OriginalStatus;
                    R.Data->SetBoolField(TEXT("dirty_restore_attempted"),true);Count(R,TEXT("dirty_restore_calls"));
                    Blueprint.Package->SetDirtyFlag(WasDirty);
                    if(Current()&&BP->Status==OriginalStatus&&Blueprint.Package->IsDirty()==WasDirty) { R.Data->SetBoolField(TEXT("status_dirty_restore_confirmed"),true); }
                    else { Clean=false;Fail(R,TEXT("function_extract_status_restore_unconfirmed"),TEXT("Status/dirty restoration postcondition failed; no retry or rollback was attempted.")); }
                }
                else { Clean=false;Fail(R,TEXT("function_extract_status_restore_skipped"),TEXT("Source changed; status/dirty restoration was not used to conceal callback changes.")); }
            }
            if(PX&&PX->Stopped)return;
            if(Blueprint.Current())
            {
                R.Data->SetBoolField(TEXT("package_dirty"),Blueprint.Package->IsDirty());R.Data->SetStringField(TEXT("compile_status"),Status(BP->Status));
                R.Data->SetBoolField(TEXT("blueprint_newly_created_after"),BP->bIsNewlyCreated);
            }
        }
        R.Data->SetBoolField(TEXT("cleanup_required"),!Clean);
        if(!Clean) { R.bSuccess=R.bPostconditionPassed=false; }
        else if(!PriorCode.IsEmpty()) { R.ErrorCode=PriorCode;R.Error=PriorError;R.Stage=PriorStage; }
    }
};
FString NormalizePath(const FString& Input)
{
    FString P=FPackageName::ExportTextPathToObjectPath(Input).TrimStartAndEnd();P.TrimQuotesInline();
    if(P.EndsWith(TEXT("_C"))) { P.LeftChopInline(2); }
    if((P.StartsWith(TEXT("/Game/"))||P.StartsWith(TEXT("/Engine/")))&&!P.Contains(TEXT("."))) { P+=TEXT(".")+FPackageName::GetShortName(P); }
    return P;
}
UBlueprint* LoadBlueprint(const FString& Input,FResult& R)
{
    const FString Path=NormalizePath(Input);
    auto Try=[&](const FString& P)->UBlueprint*
    {
        if(P.Len()>=NAME_SIZE||!P.StartsWith(TEXT("/Game/"))||!FPackageName::IsValidLongPackageName(FPackageName::ObjectPathToPackageName(P),true)) { return nullptr; }
        Count(R,TEXT("asset_load_calls"));R.bChangeStateKnown=false;
        UBlueprint* BP=LoadObject<UBlueprint>(nullptr,*P);Count(R,TEXT("asset_load_returns"));return BP;
    };
    if(auto* BP=Try(Path)) { return BP; }
    if(!Input.Contains(TEXT("/"))&&!Input.Contains(TEXT(".")))
    { if(auto* BP=Try(FString::Printf(TEXT("/Game/Blueprints/%s.%s"),*Input,*Input))) { return BP; } }
    auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    FARFilter Filter;Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());Filter.bRecursiveClasses=true;Filter.PackagePaths.Add(TEXT("/Game"));Filter.bRecursivePaths=true;
    TArray<FString> Candidates;const FString Short=FPackageName::GetShortName(Path);int32 Counted=0;bool Overflow=false;
    Count(R,TEXT("registry_enumerate_calls"));R.bChangeStateKnown=false;
    Registry.EnumerateAssets(Filter,[&](const FAssetData& A)
    {
        if(++Counted>65536) { Overflow=true;return false; }
        if(A.AssetName.ToString().Equals(Input,ESearchCase::IgnoreCase)||A.AssetName.ToString().Equals(Short,ESearchCase::IgnoreCase)
            ||A.PackageName.ToString().Equals(Input,ESearchCase::IgnoreCase)||A.GetObjectPathString().Equals(Path,ESearchCase::IgnoreCase))
        { Candidates.Add(A.GetObjectPathString()); }return true;
    });
    R.Data->SetNumberField(TEXT("registry_assets_observed"),Counted);
    if(Overflow) { Fail(R,TEXT("function_extract_lookup_budget"),TEXT("Blueprint name lookup exceeds the finite Registry inventory; no arbitrary first match was chosen."));return nullptr; }
    if(Candidates.Num()!=1) { Fail(R,TEXT("function_extract_blueprint_not_found"),TEXT("Blueprint lookup is missing or ambiguous; use an exact /Game asset path."));return nullptr; }
    return Try(Candidates[0]);
}
}

Variable::FResult Collapse(const FString& BlueprintName,const FString& GraphName,const FString& FunctionName,const FString& NodeIdsJson,const FString& GraphType,int64 ExpectedNodeCount,bool bAllowNonTemp,bool bDryRun,bool bCompile,bool bSaveAsset,const FString& PrivateContextJson)
{
    FResult R;R.bCompileRequested=bCompile;R.bSaveRequested=bSaveAsset;
    auto D=R.Data;auto Counts=MakeShared<FJsonObject>();
    for(const TCHAR* K:{TEXT("asset_load_calls"),TEXT("asset_load_returns"),TEXT("registry_enumerate_calls"),TEXT("toolkit_init_calls"),TEXT("open_graph_calls"),TEXT("can_encapsulate_calls"),TEXT("can_paste_calls"),TEXT("node_title_calls"),TEXT("diagnostic_create_calls"),TEXT("diagnostic_add_calls"),TEXT("diagnostic_cycle_calls"),TEXT("diagnostic_remove_calls"),TEXT("diagnostic_mark_garbage_calls"),TEXT("can_collapse_calls"),TEXT("collapse_calls"),TEXT("modify_blueprint_calls"),TEXT("modify_source_calls"),TEXT("rename_graph_calls"),TEXT("mark_structural_calls"),TEXT("reconstruct_call_node_calls"),TEXT("owned_editor_close_calls"),TEXT("dirty_restore_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls")}) { Counts->SetNumberField(K,0); }
    D->SetObjectField(TEXT("native_call_counts"),Counts);
    D->SetStringField(TEXT("blueprint_name"),BlueprintName);D->SetStringField(TEXT("function_name"),FunctionName.TrimStartAndEnd());
    D->SetStringField(TEXT("expected_node_count_decimal"),LexToString(ExpectedNodeCount));
    for(const TCHAR* K:{TEXT("all_guards_passed"),TEXT("can_collapse"),TEXT("selection_cycling"),TEXT("mutation_applied"),TEXT("observed_explicit_change"),TEXT("owned_editor_close_requested"),TEXT("owned_editor_OnClose_observed"),TEXT("diagnostic_cleanup_confirmed"),TEXT("status_restore_attempted"),TEXT("dirty_restore_attempted"),TEXT("status_dirty_restore_confirmed"),TEXT("cleanup_required"),TEXT("automatic_retry_performed"),TEXT("total_editor_side_effects_verified"),TEXT("atomic_rollback_available")}) { D->SetBoolField(K,false); }
    D->SetBoolField(TEXT("dry_run"),bDryRun);D->SetBoolField(TEXT("compile_requested"),bCompile);D->SetBoolField(TEXT("save_requested"),bSaveAsset);
    D->SetBoolField(TEXT("compiled"),false);D->SetBoolField(TEXT("compile_succeeded"),true);D->SetBoolField(TEXT("save_attempted"),false);D->SetBoolField(TEXT("saved"),false);
    D->SetBoolField(TEXT("save_succeeded"),bDryRun||!bSaveAsset);D->SetArrayField(TEXT("invalid_function_nodes"),{});
    D->SetStringField(TEXT("count_scope"),TEXT("Direct wrapper calls; SDK temporary graphs, skeleton compiles, reconstruction, UI callbacks and PostLoad are not measured as zero."));
    D->SetStringField(TEXT("restoration_scope"),TEXT("Only captured source Blueprint status/package dirty are restored in dry/rejected diagnostics. UI settings/debug objects/selection/compile caches/transient graph lifetimes are not atomically restored."));
    if(Busy) { Fail(R,TEXT("function_extract_busy"),TEXT("Function extraction is already in flight; no retry was dispatched."));return R; }
    if(!IsEditorGameThread()||IsPlaySessionActive()||IsRunningCommandlet()||!FSlateApplication::IsInitialized())
    { Fail(R,TEXT("function_extract_editor_state"),TEXT("An initialized Slate Editor game thread outside PIE/SIE/commandlet is required, including dry-run diagnostics."));return R; }
    for(const auto& E:RetainedEditors)
    { if(E.IsValid()&&!E->bCloseObserved) { Fail(R,TEXT("function_extract_retained_toolkit"),TEXT("An earlier owned toolkit has unconfirmed cleanup; close it explicitly before a new request."));return R; } }
    RetainedEditors.Reset();
    TGuardValue<bool> InFlight(Busy,true);
    FContext C(R);
    auto Run=[&]()->bool
    {
        R.Stage=TEXT("validation");const FString BPName=BlueprintName.TrimStartAndEnd(),FN=FunctionName.TrimStartAndEnd();D->SetStringField(TEXT("blueprint_name"),BPName);
        if(!Text(BlueprintName,NAME_SIZE-1,R)||!Text(GraphName,NAME_SIZE-1,R)||!Text(GraphType,NAME_SIZE-1,R)||!Text(FN,NAME_SIZE-1,R)||BPName.IsEmpty()||FN.IsEmpty()||FN.Equals(TEXT("None"),ESearchCase::IgnoreCase))
        { return Fail(R,TEXT("function_extract_required_text"),TEXT("Nonempty bounded blueprint_name and function_name are required.")); }
        for(TCHAR Ch:FN) { if(!FChar::IsAlnum(Ch)&&Ch!=TEXT('_')) { return Fail(R,TEXT("function_extract_function_name"),TEXT("function_name may contain only letters, numbers and underscores.")); } }
        TSharedPtr<FJsonValue> NodesValue,ContextValue;if(!Parse(NodeIdsJson,NodesValue,R)||!Parse(PrivateContextJson,ContextValue,R)) { return false; }
        if(NodesValue->Type!=EJson::Array||NodesValue->AsArray().IsEmpty()||NodesValue->AsArray().Num()>MaxSelection)
        { return Fail(R,TEXT("function_extract_selection_count"),TEXT("node_ids must contain 1..2048 GUID strings.")); }
        const auto& Values=NodesValue->AsArray();D->SetNumberField(TEXT("selected_node_count"),Values.Num());D->SetNumberField(TEXT("raw_selected_node_count"),Values.Num());
        if(ExpectedNodeCount>=0&&ExpectedNodeCount!=static_cast<int64>(Values.Num()))
        { return Fail(R,TEXT("function_extract_expected_count"),TEXT("expected_node_count does not match the raw GUID array count.")); }
        if(ContextValue->Type!=EJson::Object) { return Fail(R,TEXT("function_extract_private_context"),TEXT("Private context must be an object containing only optional graph_id.")); }
        const auto Context=ContextValue->AsObject();D->SetObjectField(TEXT("private_graph_context"),Context);
        for(const auto& Pair:Context->Values)
        { if(FString(Pair.Key.ToView())!=TEXT("graph_id")) { return Fail(R,TEXT("function_extract_private_context"),TEXT("Unknown private context key.")); } }
        FString GraphId;Context->TryGetStringField(TEXT("graph_id"),GraphId);
        if(!Text(GraphId,NAME_SIZE-1,R)) { return false; }
        // Local trim only validates presence. The original resolver receives original raw selector text.
        if(GraphName.TrimStartAndEnd().IsEmpty()&&GraphId.TrimStartAndEnd().IsEmpty())
        { return Fail(R,TEXT("function_extract_graph_selector"),TEXT("An explicit graph_name or graph_id is required.")); }
        C.BP=LoadBlueprint(BPName,R);
        if(!C.BP) { if(R.ErrorCode.IsEmpty()) { Fail(R,TEXT("function_extract_blueprint_not_found"),TEXT("Blueprint could not be loaded.")); }return false; }
        if(!Loaded(C.BP)||C.BP->GetClass()!=UBlueprint::StaticClass()||(C.BP->BlueprintType!=BPTYPE_Normal&&C.BP->BlueprintType!=BPTYPE_FunctionLibrary)||!Loaded(C.BP->ParentClass)
            ||!Loaded(C.BP->SkeletonGeneratedClass)||C.BP->SkeletonGeneratedClass->ClassGeneratedBy!=C.BP||C.BP->GetOuter()!=C.BP->GetPackage()||!C.BP->GetPackage()->GetName().StartsWith(TEXT("/Game/"))
            ||C.BP->bBeingCompiled||C.BP->bQueuedForCompilation||C.BP->bIsRegeneratingOnLoad||C.BP->bSuppressStructurallyModified||C.BP->Status==BS_BeingCreated)
        { return Fail(R,TEXT("function_extract_blueprint_scope"),TEXT("A loaded idle top-level ordinary or direct native FunctionLibrary /Game UBlueprint with a valid skeleton class is required; custom/level/animation/interface Blueprint variants remain unsupported.")); }
        C.FunctionLibrary=C.BP->BlueprintType==BPTYPE_FunctionLibrary;
        if(C.FunctionLibrary&&!DirectFunctionLibraryRole(C.BP))
        { return Fail(R,TEXT("function_extract_function_library_scope_pending"),TEXT("Only a direct native UBlueprintFunctionLibrary with no own variables/interfaces/components/event/macro/delegate graphs is admitted; other FunctionLibrary source variants remain incomplete.")); }
        D->SetBoolField(TEXT("function_library_source"),C.FunctionLibrary);
        if(C.FunctionLibrary){D->SetStringField(TEXT("function_library_signature_transition"),TEXT("Stock SDK static/world-context/self reconstruction is retained; no physical pin-order or compiler-metadata equality claim."));D->SetBoolField(TEXT("function_library_composite_event_supported"),false);}
        for(UClass* Parent=C.BP->ParentClass;Parent;Parent=Parent->GetSuperClass())
        { if(ToolingReference(Parent->GetPathName())) { return Fail(R,TEXT("function_extract_runtime_dependency"),TEXT("Blueprint parent cannot depend on Cubeless/MCP Editor tooling.")); } }
        C.Blueprint.Capture(C.BP);C.Parent.Capture(C.BP->ParentClass);
        if(!bAllowNonTemp&&!C.Blueprint.Package->GetName().StartsWith(TEXT("/Game/_MCP_Temp/")))
        { return Fail(R,TEXT("function_extract_non_temp"),TEXT("Non-temp Blueprint requires allow_non_temp=True.")); }
        TArray<UEdGraph*> All;if(!Graphs(C.BP,All,R)) { return false; }
        for(UEdGraph* G:All)
        {
            if(!GraphId.IsEmpty()) { FGuid Parsed;if(FGuid::Parse(GraphId,Parsed)) { if(G->GraphGuid!=Parsed) { continue; } }else if(G->GraphGuid.ToString()!=GraphId) { continue; } }
            if(!GraphName.IsEmpty()&&!G->GetName().Equals(GraphName,ESearchCase::IgnoreCase)) { continue; }
            const FString Kind=NormalizeType(GraphType);if(!Kind.IsEmpty()&&Kind!=TEXT("any")&&GraphKind(C.BP,G)!=Kind) { continue; }
            C.Source=G;break; // Preserve original first matching graph order.
        }
        if(!C.Source) { return Fail(R,TEXT("function_extract_graph_missing"),TEXT("Requested Blueprint graph was not found; no replacement function graph was created.")); }
        if(C.Source->Schema!=UEdGraphSchema_K2::StaticClass()||C.Source->GetSchema()!=GetDefault<UEdGraphSchema_K2>()||FBlueprintEditorUtils::IsGraphReadOnly(C.Source)||C.Source->GetPackage()!=C.Blueprint.Package||C.Source->GetTypedOuter<UBlueprint>()!=C.BP)
        { return Fail(R,TEXT("function_extract_graph_scope"),TEXT("A writable stock K2 graph in the captured Blueprint/package is required.")); }
        if(C.FunctionLibrary&&(C.Source->GetOuter()!=C.BP||!C.BP->FunctionGraphs.Contains(C.Source)))
        { return Fail(R,TEXT("function_extract_function_library_graph_pending"),TEXT("FunctionLibrary extraction requires an explicitly selected existing own FunctionGraphs member; implicit graphs and other graph roles remain incomplete.")); }
        C.Graph.Capture(C.Source);if(!C.Before.Capture(C.Source,R)) { return false; }
        D->SetStringField(TEXT("source_graph_name"),C.Source->GetName());
        auto GraphJson=MakeShared<FJsonObject>();GraphJson->SetStringField(TEXT("graph_id"),C.Source->GraphGuid.ToString());GraphJson->SetStringField(TEXT("graph_name"),C.Source->GetName());GraphJson->SetStringField(TEXT("graph_type"),GraphKind(C.BP,C.Source));GraphJson->SetStringField(TEXT("schema"),C.Source->GetSchema()->GetClass()->GetName());GraphJson->SetNumberField(TEXT("node_count"),C.Source->Nodes.Num());D->SetObjectField(TEXT("graph"),GraphJson);
        if(!FBlueprintEditorUtils::IsGraphNameUnique(C.BP,FName(*FN))) { return Fail(R,TEXT("function_extract_name_exists"),TEXT("A Blueprint graph/function already has function_name.")); }
        if(!C.Current()) { return false; }
        const bool HasCompositeSelection=[&]()->bool
        {
            for(const auto& V:Values)if(V.IsValid()&&V->Type==EJson::String)
            {
                const FString S=V->AsString().TrimStartAndEnd();FGuid Id;if(S.Len()>=NAME_SIZE||!FGuid::Parse(S,Id))continue;
                for(const auto& N:C.Before.Nodes)if(N->Id==Id&&N->Node->IsA<UK2Node_Composite>())return true;
            }
            return false;
        }();
        if(C.FunctionLibrary&&HasCompositeSelection)
        { return Fail(R,TEXT("function_extract_function_library_composite_pending"),TEXT("FunctionLibrary Composite/Event compound extraction remains incomplete; the Actor compound observer was not applied and toolkit mutation was not dispatched.")); }
        TSet<FGuid> Guids;TSet<UEdGraphNode*> Selection;TArray<UEdGraphNode*> SelectedOrder;TArray<TSharedPtr<FJsonValue>> ValidIds;
        int64 SumX=0,SumY=0;int32 CustomEvents=0;
        for(int32 I=0;I<Values.Num();++I)
        {
            if(!Values[I].IsValid()||Values[I]->Type!=EJson::String) { return Fail(R,TEXT("function_extract_guid_type"),FString::Printf(TEXT("node_ids[%d] must be a GUID string."),I)); }
            const FString S=Values[I]->AsString().TrimStartAndEnd();if(!Text(S,NAME_SIZE-1,R)) { return false; }FGuid Id;
            if(!FGuid::Parse(S,Id)||!Id.IsValid()) { return Fail(R,TEXT("function_extract_guid_invalid"),FString::Printf(TEXT("Invalid GUID at node_ids[%d]."),I)); }
            if(Guids.Contains(Id)) { return Fail(R,TEXT("function_extract_guid_duplicate"),TEXT("Duplicate GUID in node_ids.")); }Guids.Add(Id);
            UEdGraphNode* N=nullptr;for(const auto& Row:C.Before.Nodes) { if(Row->Id==Id) { N=Row->Node;break; } }
            if(!N) { return Fail(R,TEXT("function_extract_node_missing"),TEXT("Selected GUID does not exist in the target graph.")); }
            if(C.FunctionLibrary&&N->IsA<UK2Node_CustomEvent>())
            { return Fail(R,TEXT("function_extract_function_library_event_pending"),TEXT("FunctionLibrary CustomEvent template extraction remains incomplete; no toolkit mutation was dispatched.")); }
            if(!HasCompositeSelection)
            {
            Count(R,TEXT("can_encapsulate_calls"));const bool Allowed=GetDefault<UEdGraphSchema_K2>()->CanEncapuslateNode(*N);
            if(!C.Current()) { return false; }
            if(!Allowed)
            {
                Count(R,TEXT("node_title_calls"));const FString Title=N->GetNodeTitle(ENodeTitleType::FullTitle).ToString();if(!C.Current()) { return false; }
                D->SetStringField(TEXT("rejected_node_id"),Id.ToString());D->SetStringField(TEXT("rejected_node_title"),Title);
                return Fail(R,TEXT("function_extract_cannot_encapsulate"),FString::Printf(TEXT("Node cannot be placed in a Blueprint function: %s [%s]"),*Title,*S));
            }
            if(auto* Event=Cast<UK2Node_CustomEvent>(N))
            {
                ++CustomEvents;
                if(!GetDefault<UEdGraphSchema_K2>()->FindExecutionPin(*Event,EGPD_Output)) { return Fail(R,TEXT("function_extract_event_exec_missing"),TEXT("Custom event lacks the SDK-required execution output.")); }
            }
            }
            SumX+=N->NodePosX;SumY+=N->NodePosY;
            if(FMath::Abs(static_cast<int64>(N->NodePosX))>1000000||FMath::Abs(static_cast<int64>(N->NodePosY))>1000000
                ||SumX<MIN_int32||SumX>MAX_int32||SumY<MIN_int32||SumY>MAX_int32)
            { return Fail(R,TEXT("function_extract_position_bound"),TEXT("Selected node positions exceed safe SDK int32 sum/offset bounds.")); }
            Selection.Add(N);SelectedOrder.Add(N);ValidIds.Add(MakeShared<FJsonValueString>(Id.ToString()));
        }
        if(HasCompositeSelection)
        {C.PX=MakeUnique<FPXComposite>(R);if(!C.PX->Bind(C.BP,C.Source,Selection))return false;}
        if(HasCompositeSelection)for(auto* N:SelectedOrder)
        {
            Count(R,TEXT("can_encapsulate_calls"));const bool Allowed=GetDefault<UEdGraphSchema_K2>()->CanEncapuslateNode(*N);
            if(!C.Current())return false;
            if(!Allowed)
            {
                Count(R,TEXT("node_title_calls"));const FString Title=N->GetNodeTitle(ENodeTitleType::FullTitle).ToString();if(!C.Current())return false;
                D->SetStringField(TEXT("rejected_node_id"),N->NodeGuid.ToString());D->SetStringField(TEXT("rejected_node_title"),Title);
                return Fail(R,TEXT("function_extract_cannot_encapsulate"),FString::Printf(TEXT("Node cannot be placed in a Blueprint function: %s [%s]"),*Title,*N->NodeGuid.ToString()));
            }
            if(auto* Event=Cast<UK2Node_CustomEvent>(N)){++CustomEvents;if(!GetDefault<UEdGraphSchema_K2>()->FindExecutionPin(*Event,EGPD_Output))return Fail(R,TEXT("function_extract_event_exec_missing"),TEXT("Custom event lacks the SDK-required execution output."));}
        }
        D->SetArrayField(TEXT("selected_node_ids"),ValidIds);
        D->SetNumberField(TEXT("custom_event_template_count"),CustomEvents); // SDK CanCollapse reports multiple event templates as a normal rejection.
        if(!SafeCycleQueries(C.Before,Selection,R)) { return false; }
        C.Baseline=true;C.WasDirty=C.Blueprint.Package->IsDirty();C.OriginalStatus=C.BP->Status;
        D->SetBoolField(TEXT("was_dirty"),C.WasDirty);D->SetBoolField(TEXT("blueprint_newly_created_before"),C.BP->bIsNewlyCreated);
        C.Restore=bDryRun;R.Stage=TEXT("toolkit_initialization");R.bMutationAttempted=true;
        D->SetBoolField(TEXT("owned_toolkit_initialization_dispatched"),true);
        C.Editor=MakeShared<FOwnedExtractor>();Count(R,TEXT("toolkit_init_calls"));C.Editor->InitBlueprintEditor(EToolkitMode::Standalone,TSharedPtr<IToolkitHost>(),{C.BP},false);
        if(!C.Current()) { return false; }
        D->SetBoolField(TEXT("owned_toolkit_initialized"),true);
        if(C.BP->bIsNewlyCreated!=D->GetBoolField(TEXT("blueprint_newly_created_before"))) { D->SetBoolField(TEXT("observed_explicit_change"),true); }
        Count(R,TEXT("open_graph_calls"));C.GraphEditor=C.Editor->OpenGraphAndBringToFront(C.Source,false);
        if(!C.Current()||!C.GraphEditor.IsValid()||C.GraphEditor->GetCurrentGraph()!=C.Source)
        { return Fail(R,TEXT("function_extract_open_graph_failed"),TEXT("Owned Blueprint editor did not open the captured source graph.")); }
        D->SetNumberField(TEXT("owned_editor_initial_selected_count"),C.GraphEditor->GetSelectedNodes().Num());
        R.Stage=TEXT("diagnostics");Count(R,TEXT("diagnostic_create_calls"));
        C.Diagnostic=FBlueprintEditorUtils::CreateNewGraph(C.BP,FBlueprintEditorUtils::FindUniqueKismetName(C.BP,TEXT("MCPFunctionCollapseDiagnostic")),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
        if(C.PX&&Loaded(C.Diagnostic))C.DiagnosticIdentity.Capture(C.Diagnostic);
        if(!C.Current()||!Loaded(C.Diagnostic)||C.Diagnostic->GetOuter()!=C.BP||C.Diagnostic->GetPackage()!=C.Blueprint.Package)
        { return Fail(R,TEXT("function_extract_diagnostic_create_failed"),TEXT("Diagnostic graph creation did not return a current owned graph.")); }
        C.DiagnosticIdentity.Capture(C.Diagnostic);D->SetBoolField(TEXT("observed_explicit_change"),true);
        Count(R,TEXT("diagnostic_add_calls"));C.DiagnosticAdded=true;
        if(C.PX){C.PX->Diagnostic=C.Diagnostic;C.PX->DiagnosticMember=true;}
        FBlueprintEditorUtils::AddFunctionGraph<UClass>(C.BP,C.Diagnostic,false,nullptr);if(!C.RefreshPX())return false;
        if(!C.Current()||!C.DiagnosticIdentity.Current()||!C.BP->FunctionGraphs.Contains(C.Diagnostic)) { return Fail(R,TEXT("function_extract_diagnostic_add_failed"),TEXT("Diagnostic AddFunctionGraph did not preserve current owner/membership.")); }
        TArray<TSharedPtr<FJsonValue>> Invalid;
        for(UEdGraphNode* N:SelectedOrder)
        {
            Count(R,TEXT("can_paste_calls"));const bool CanPaste=N->CanPasteHere(C.Diagnostic);
            if(!C.Current()||!C.DiagnosticIdentity.Current()) { return Fail(R,TEXT("function_extract_diagnostic_callback_changed"),TEXT("Diagnostic or source identity changed during a node callback.")); }
            if(!CanPaste)
            {
                auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("node_id"),N->NodeGuid.ToString());Row->SetStringField(TEXT("node_class"),N->GetClass()->GetPathName());
                Count(R,TEXT("node_title_calls"));const FString Title=N->GetNodeTitle(ENodeTitleType::FullTitle).ToString();if(!C.Current()||!C.DiagnosticIdentity.Current()) { return Fail(R,TEXT("function_extract_diagnostic_callback_changed"),TEXT("Diagnostic or source identity changed during a node callback.")); }
                Row->SetStringField(TEXT("node_title"),Title);Invalid.Add(MakeShared<FJsonValueObject>(Row));D->SetArrayField(TEXT("invalid_function_nodes"),Invalid);
            }
        }
        FCompilerResultsLog Log;Log.bAnnotateMentionedNodes=false;Count(R,TEXT("diagnostic_cycle_calls"));
        const bool Cycling=FBlueprintEditorUtils::CheckIfSelectionIsCycling(Selection,Log);D->SetBoolField(TEXT("selection_cycling"),Cycling);
        if(!C.Current()||!C.RemoveDiagnostic()||!C.Current()) { return false; }
        R.Stage=TEXT("can_collapse");Count(R,TEXT("can_collapse_calls"));const bool CanCollapse=C.Editor->CanExtract(Selection);D->SetBoolField(TEXT("can_collapse"),CanCollapse);
        if(!C.Current()) { return false; }
        if(!CanCollapse) { C.Restore=true;return Fail(R,TEXT("function_extract_rejected"),TEXT("Unreal rejected Collapse to Function; inspect invalid_function_nodes and selection_cycling.")); }
        D->SetBoolField(TEXT("all_guards_passed"),true);
        UEdGraph* FunctionGraph=nullptr;UEdGraphNode* FunctionCall=nullptr;
        TStrongObjectPtr<UEdGraph> KeepFunction;TStrongObjectPtr<UEdGraphNode> KeepCall;
        if(!bDryRun)
        {
            R.Stage=TEXT("collapse");Count(R,TEXT("modify_blueprint_calls"));C.BP->Modify();if(!C.Current()) { return false; }
            Count(R,TEXT("modify_source_calls"));C.Source->Modify();if(!C.Current()||!SafeCycleQueries(C.Before,Selection,R)||C.GraphEditor->GetCurrentGraph()!=C.Source) { return false; }
            Count(R,TEXT("collapse_calls"));D->SetBoolField(TEXT("mutation_applied"),true);
            FunctionGraph=C.Editor->Extract(C.GraphEditor,Selection,FunctionCall);KeepFunction.Reset(FunctionGraph);KeepCall.Reset(FunctionCall);
            if(C.PX){R.bChanged=true;if(!C.PX->Move(FunctionGraph,FunctionCall))return false;}
            D->SetNumberField(TEXT("selected_node_count"),Selection.Num());
            auto Outputs=[&]()->bool
            {
                if(!C.Current(false)||!Loaded(FunctionGraph)||!Loaded(FunctionCall)||FunctionGraph->GetOuter()!=C.BP||FunctionGraph->GetPackage()!=C.Blueprint.Package
                    ||!C.BP->FunctionGraphs.Contains(FunctionGraph)||FunctionCall->GetGraph()!=C.Source||!C.Source->Nodes.Contains(FunctionCall)||FunctionCall->GetPackage()!=C.Blueprint.Package)
                { return Fail(R,TEXT("function_extract_partial_collapse"),TEXT("Collapse returned without current owned function graph/call node; partial mutation is retained without rollback.")); }
                return true;
            };
            if(!Outputs()) { return false; }
            for(UEdGraphNode* Selected:SelectedOrder)
            {
                if(C.PX?Selected==C.PX->EventTemplate.Get():Selected->IsA<UK2Node_CustomEvent>()) { continue; } // Compound path compares the retained role without dereferencing the removed event.
                if(!Loaded(Selected)||Selected->GetGraph()!=FunctionGraph||!FunctionGraph->Nodes.Contains(Selected)||Selected->GetPackage()!=C.Blueprint.Package)
                { return Fail(R,TEXT("function_extract_selected_move_unconfirmed"),TEXT("Selected node movement into the returned function graph is incomplete.")); }
            }
            const auto* Call=Cast<UK2Node_CallFunction>(FunctionCall);
            if(!Call) { return Fail(R,TEXT("function_extract_call_type"),TEXT("Returned gateway is not a function call node.")); }
            FIdentity ResultGraph,ResultCall;ResultGraph.Capture(FunctionGraph);ResultCall.Capture(FunctionCall);
            Count(R,TEXT("rename_graph_calls"));if(C.PX&&!C.PX->PlanRename(FN))return false;FBlueprintEditorUtils::RenameGraph(FunctionGraph,FN);if(!C.RefreshPX())return false;
            if(!Outputs()||!ResultCall.Current()||FunctionGraph->GetName()!=FN) { return Fail(R,TEXT("function_extract_rename_partial"),TEXT("Function rename did not meet the requested result; partial mutation is retained.")); }
            if(Call->FunctionReference.GetMemberName()!=FunctionGraph->GetFName())
            { return Fail(R,TEXT("function_extract_call_reference"),TEXT("Gateway function reference does not match the renamed function graph.")); }
            C.HoldOutputs(FunctionGraph,FunctionCall,FN);
            ResultGraph.Capture(FunctionGraph);Count(R,TEXT("mark_structural_calls"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(C.BP);if(!C.RefreshPX())return false;
            if(!Outputs()||!ResultGraph.Current()||!ResultCall.Current()) { return Fail(R,TEXT("function_extract_output_identity_changed"),TEXT("Function result identity changed after structural/reconstruction callbacks.")); }
            Count(R,TEXT("reconstruct_call_node_calls"));FunctionCall->ReconstructNode();if(!C.RefreshPX())return false;
            if(!Outputs()||!ResultGraph.Current()||!ResultCall.Current()) { return Fail(R,TEXT("function_extract_output_identity_changed"),TEXT("Function result identity changed after structural/reconstruction callbacks.")); }
            D->SetStringField(TEXT("function_graph_id"),FunctionGraph->GraphGuid.ToString());D->SetNumberField(TEXT("function_node_count"),FunctionGraph->Nodes.Num());
            D->SetStringField(TEXT("function_call_node_id"),FunctionCall->NodeGuid.ToString());D->SetNumberField(TEXT("function_call_x"),FunctionCall->NodePosX);D->SetNumberField(TEXT("function_call_y"),FunctionCall->NodePosY);
            // Original closes its owned editor before explicit compile/save. Finish performs that once below.
        }
        return true;
    };
    bool Completed=Run();
    if(C.PX&&!C.PX->Ready&&!C.PX->Stopped)return R;
    C.Finish();if(C.PX&&C.PX->Stopped)return R;Completed=Completed&&R.ErrorCode.IsEmpty();
    if(Completed&&(!C.Current(bDryRun)||!C.OutputsCurrent())) { Completed=false; }
    if(Completed&&!bDryRun&&bCompile)
    {
        R.Stage=TEXT("compile");R.bCompileAttempted=true;D->SetBoolField(TEXT("compile_succeeded"),false);D->SetBoolField(TEXT("compile_completion_observed"),false);Count(R,TEXT("explicit_compile_calls"));
        FKismetEditorUtilities::CompileBlueprint(C.BP);D->SetBoolField(TEXT("compiled"),true);if(!C.RefreshPX())Completed=false;
        if(!C.OutputsCurrent()) { Completed=false; }
        else
        {
            const bool Good=C.BP->Status==BS_UpToDate||C.BP->Status==BS_UpToDateWithWarnings;R.Compiled=Good;D->SetBoolField(TEXT("compile_completion_observed"),true);
            D->SetBoolField(TEXT("compile_succeeded"),Good);D->SetStringField(TEXT("compile_status"),Status(C.BP->Status));
            if(!Good) { Completed=false;Fail(R,TEXT("function_extract_compile_failed"),TEXT("Function extraction was applied but explicit Blueprint compilation failed.")); }
        }
    }
    const bool PXCompileFailure=C.PX&&!C.PX->Stopped&&R.ErrorCode==TEXT("function_extract_compile_failed");
    const FString CompileFailureCode=R.ErrorCode,CompileFailureError=R.Error,CompileFailureStage=R.Stage;
    if(Completed&&!bDryRun&&bSaveAsset)
    {
        if(C.OutputsCurrent())
        {
            R.Stage=TEXT("save");R.bSaveAttempted=true;D->SetBoolField(TEXT("save_attempted"),true);Count(R,TEXT("explicit_save_calls"));
            R.bSaved=UEditorAssetLibrary::SaveLoadedAsset(C.BP,false);D->SetBoolField(TEXT("saved"),R.bSaved);D->SetBoolField(TEXT("save_succeeded"),R.bSaved);
            if(C.PX&&!R.bSaved){const FString SaveStage=R.Stage;C.PX->Stop(TEXT("save_returned_false"));R.Stage=SaveStage;Fail(R,TEXT("function_extract_save_failed"),TEXT("Save returnedFalse; actual save/compile prefix retained, no later SDK."));return R;}
            if(!C.OutputsCurrent()||!R.bSaved) { Completed=false;Fail(R,TEXT("function_extract_save_failed"),TEXT("Extraction save failed or owner changed; RAM/file partial effects are retained and no retry is permitted.")); }
        }
        else { Completed=false; }
    }
    if(C.PX&&C.PX->Stopped)return R;
    if(C.Baseline&&C.Blueprint.Current())
    {
        D->SetBoolField(TEXT("package_dirty"),C.Blueprint.Package->IsDirty());D->SetStringField(TEXT("compile_status"),Status(C.BP->Status));
        if(C.Graph.Current())
        {
            const auto* GJ=D->Values.Find(TEXT("graph"));
            if(GJ&&(*GJ)->Type==EJson::Object) { (*GJ)->AsObject()->SetNumberField(TEXT("node_count"),C.Source->Nodes.Num()); }
        }
    }
    if(Completed&&!C.OutputsCurrent()) { Completed=false; }
    if(PXCompileFailure){Completed=false;R.ErrorCode=CompileFailureCode;R.Error=CompileFailureError;R.Stage=CompileFailureStage;}
    if(C.PX&&C.PX->Ready&&!C.PX->Stopped){if(!C.PX->Plain())return R;R.bChangeStateKnown=true;D->SetBoolField(TEXT("composite_captured_values_and_links_preserved"),true);}
    D->SetBoolField(TEXT("success"),Completed);D->SetBoolField(TEXT("total_change_state_known"),R.bChangeStateKnown);
    R.bSuccess=R.bPostconditionPassed=Completed;if(Completed) { R.Stage=TEXT("completed"); }
    return R;
}
}
