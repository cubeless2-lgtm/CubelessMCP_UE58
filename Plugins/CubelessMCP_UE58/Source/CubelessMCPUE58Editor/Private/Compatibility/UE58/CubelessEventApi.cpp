// UE56 event override contract and a separate explicit parent-call extension.
#include "Compatibility/UE58/CubelessEventApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessGraphApi.h"
#include "Compatibility/UE58/CubelessGeneratedComponentApi.h"
#include "Compatibility/UE58/CubelessFunctionPinApi.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "UObject/Interface.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_EditablePinBase.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "Internationalization/Text.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "UObject/UObjectGlobals.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_InputKey.h"
#include "K2Node_CallParentFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::Event
{
namespace
{
namespace Pin = Function::Pin;
struct FActorEventScope;
FActorEventScope* ActiveInterfaceEvent=nullptr;
const TFunction<bool()>* ActiveDefaultEventFence=nullptr;
bool DefaultEventDispatchBusy=false;
struct FDefaultEventNodeScope;
FDefaultEventNodeScope* ActiveDefaultEventNode=nullptr;
bool InterfaceObservationFence();
bool Fail(const TSharedRef<FJsonObject>& R, const TCHAR* Code, const FString& Error)
{ R->SetStringField(TEXT("error_code"), Code); R->SetStringField(TEXT("error"), Error); return false; }
TSharedRef<FJsonObject> Result()
{
    auto R = MakeShared<FJsonObject>(); R->SetObjectField(TEXT("data"), MakeShared<FJsonObject>());
    for (const TCHAR* Key : {TEXT("success"), TEXT("mutation_attempted"), TEXT("changed"), TEXT("postcondition_passed"),
        TEXT("compile_requested"), TEXT("compile_attempted"), TEXT("compiled"), TEXT("save_requested"), TEXT("save_attempted"), TEXT("saved"), TEXT("loaded_during_operation")})
    { R->SetBoolField(Key, false); }
    R->SetBoolField(TEXT("change_state_known"), true); R->SetStringField(TEXT("stage"), TEXT("preflight")); return R;
}
FString Text(const TSharedRef<FJsonObject>& Value)
{ FString Out; FJsonSerializer::Serialize(Value, TJsonWriterFactory<>::Create(&Out)); return Out; }
// Only enabled inside WM's default-graph compound request. Existing standalone
// event adapters retain their prior behavior; no destructor performs an SDK call.
struct FDefaultEventNodeScope
{
    TSharedRef<FJsonObject> R;bool Active=false,Stopped=false,Attached=false,PinsFrozen=false;
    TStrongObjectPtr<UEdGraph> Graph;TStrongObjectPtr<UEdGraphNode> Node;
    TArray<TObjectPtr<UEdGraphNode>> ExpectedNodes;TArray<UEdGraphPin*> Pins;
    explicit FDefaultEventNodeScope(TSharedRef<FJsonObject> In):R(In),Active(ActiveDefaultEventFence!=nullptr){}
    bool Stop(const TCHAR* Property)
    {
        if(!Stopped){auto D=R->GetObjectField(TEXT("data"));D->SetStringField(TEXT("default_event_child_first_mismatch"),Property);D->SetBoolField(TEXT("followup_sdk_stopped"),true);
            Fail(R,TEXT("default_event_child_scope_changed"),TEXT("Default graph child role changed; actual prefix retained with no further SDK, cleanup or automatic retry."));}
        Stopped=true;R->SetBoolField(TEXT("success"),false);R->SetBoolField(TEXT("postcondition_passed"),false);R->SetBoolField(TEXT("change_state_known"),false);R->SetField(TEXT("changed"),MakeShared<FJsonValueNull>());return false;
    }
    bool Current()
    {
        if(!Active)return true;if(Stopped)return false;
        if(ActiveDefaultEventFence&&!(*ActiveDefaultEventFence)())return Stop(TEXT("outer_scope"));
        if(Graph.Get()&&(!Graph.IsValid()||Graph->Nodes!=ExpectedNodes))return Stop(TEXT("node_membership"));
        if(Node.Get()&&(!IsValid(Node.Get())||!Graph.IsValid()||Node->GetOuter()!=Graph.Get()||(Attached&&!Graph->Nodes.Contains(Node.Get()))))return Stop(TEXT("node_role"));
        if(PinsFrozen){if(Node->Pins!=Pins)return Stop(TEXT("pin_membership"));for(auto* P:Pins)if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=Node.Get())return Stop(TEXT("pin_lifetime"));}
        return true;
    }
    bool Bind(UEdGraph* G){if(!Active)return true;if(!Current()||!IsValid(G))return Stop(TEXT("graph_role"));Graph.Reset(G);ExpectedNodes=G->Nodes;return Current();}
    bool Adopt(UEdGraphNode* N,bool InAttached=false){if(!Active)return true;if(!Current()||!IsValid(N)||N->GetOuter()!=Graph.Get())return Stop(TEXT("returned_node"));Node.Reset(N);Attached=InAttached;return Current();}
    bool Attaching(){if(!Active)return true;if(!Current())return false;ExpectedNodes.Add(Node.Get());Attached=true;return true;}
    bool FreezePins(){if(!Active)return true;if(!Current())return false;Pins=Node->Pins;PinsFrozen=true;return Current();}
};
bool DefaultEventCurrent(const TSharedRef<FJsonObject>& R)
{
    if(ActiveDefaultEventNode)return ActiveDefaultEventNode->Current();
    if(!ActiveDefaultEventFence||(*ActiveDefaultEventFence)())return true;
    R->SetBoolField(TEXT("success"),false);R->SetBoolField(TEXT("postcondition_passed"),false);R->SetBoolField(TEXT("change_state_known"),false);R->SetField(TEXT("changed"),MakeShared<FJsonValueNull>());
    return Fail(R,TEXT("default_event_graph_scope_changed"),TEXT("Default graph compound scope lost; no further SDK call."));
}

TSharedRef<FJsonObject> NodeState(const UEdGraphNode* Node)
{
    auto J = MakeShared<FJsonObject>();if(!InterfaceObservationFence())return J; J->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString(EGuidFormats::Digits));
    J->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString(EGuidFormats::Digits)); J->SetStringField(TEXT("object_path"), Node->GetPathName());
    J->SetStringField(TEXT("node_class"), Node->GetClass()->GetPathName()); J->SetStringField(TEXT("node_name"), Node->GetName());
    J->SetNumberField(TEXT("x"), Node->NodePosX); J->SetNumberField(TEXT("y"), Node->NodePosY);
    J->SetNumberField(TEXT("enabled_state"), static_cast<int32>(Node->GetDesiredEnabledState()));
    J->SetBoolField(TEXT("ghost"), Node->IsAutomaticallyPlacedGhostNode());
    TArray<TSharedPtr<FJsonValue>> Pins;
    const auto ObservedPins=Node->Pins;for (const UEdGraphPin* P : ObservedPins)
    {
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("name"), P->PinName.ToString());
        Row->SetStringField(TEXT("pin_id"), P->PinId.ToString(EGuidFormats::Digits)); Row->SetStringField(TEXT("direction"), P->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
        Row->SetObjectField(TEXT("pin_type"), Pin::Describe(P->PinType));if(!InterfaceObservationFence())return J; FString TypeText;
        FEdGraphPinType::StaticStruct()->ExportText(TypeText, &P->PinType, &P->PinType, nullptr, PPF_None, nullptr);if(!InterfaceObservationFence())return J;
        Row->SetStringField(TEXT("pin_type_text"), TypeText); Row->SetBoolField(TEXT("orphaned"), P->bOrphanedPin);
        Row->SetBoolField(TEXT("hidden"), P->bHidden); Row->SetStringField(TEXT("default_value"), P->DefaultValue);
        Row->SetStringField(TEXT("default_text"), P->DefaultTextValue.ToString());if(!InterfaceObservationFence())return J;
        Row->SetStringField(TEXT("default_object"), P->DefaultObject ? P->DefaultObject->GetPathName() : FString());
        TArray<TSharedPtr<FJsonValue>> Links;
        for (const UEdGraphPin* Other : P->LinkedTo)
        {
            auto Link = MakeShared<FJsonObject>(); Link->SetStringField(TEXT("node_path"), Other->GetOwningNode()->GetPathName());
            Link->SetStringField(TEXT("pin_name"), Other->PinName.ToString()); Link->SetStringField(TEXT("pin_id"), Other->PinId.ToString(EGuidFormats::Digits));
            Links.Add(MakeShared<FJsonValueObject>(Link));
        }
        Row->SetArrayField(TEXT("linked_to"), Links); Pins.Add(MakeShared<FJsonValueObject>(Row));
    }
    J->SetArrayField(TEXT("pins"), Pins);
    if (const UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
    {
        J->SetStringField(TEXT("event_name"), EventNode->EventReference.GetMemberName().ToString());
        const UClass* Owner = EventNode->EventReference.GetMemberParentClass();
        J->SetStringField(TEXT("event_member_parent_path"), Owner ? Owner->GetPathName() : FString());
        J->SetBoolField(TEXT("override_function"), EventNode->bOverrideFunction);
    }
    if (const UK2Node_CustomEvent* Custom = Cast<UK2Node_CustomEvent>(Node))
    {
        J->SetStringField(TEXT("custom_event_name"), Custom->CustomFunctionName.ToString());
        J->SetBoolField(TEXT("call_in_editor"), Custom->bCallInEditor);
        J->SetBoolField(TEXT("deprecated"), Custom->bIsDeprecated);
        J->SetNumberField(TEXT("function_flags"), Custom->FunctionFlags);
        J->SetNumberField(TEXT("user_pin_count"), Custom->UserDefinedPins.Num());
    }
    if (const UK2Node_InputKey* Input = Cast<UK2Node_InputKey>(Node))
    {
        J->SetStringField(TEXT("key"), Input->InputKey.ToString());
        J->SetBoolField(TEXT("control"), Input->bControl); J->SetBoolField(TEXT("alt"), Input->bAlt);
        J->SetBoolField(TEXT("shift"), Input->bShift); J->SetBoolField(TEXT("command"), Input->bCommand);
        J->SetBoolField(TEXT("consume_input"), Input->bConsumeInput); J->SetBoolField(TEXT("execute_when_paused"), Input->bExecuteWhenPaused);
        J->SetBoolField(TEXT("override_parent_binding"), Input->bOverrideParentBinding);
    }
    if (const UK2Node_CallParentFunction* Call = Cast<UK2Node_CallParentFunction>(Node))
    {
        const UFunction* Target = Call->GetTargetFunction();const TStrongObjectPtr<UFunction> KeepTarget(const_cast<UFunction*>(Target));if(!InterfaceObservationFence())return J; J->SetStringField(TEXT("function_name"), Call->GetFunctionName().ToString());
        J->SetStringField(TEXT("function_path"), Target ? Target->GetPathName() : FString());
        const UClass* Owner = Call->FunctionReference.GetMemberParentClass();
        J->SetStringField(TEXT("serialized_member_parent_class"), Owner ? Owner->GetPathName() : FString());
        J->SetStringField(TEXT("function_member_guid"), Call->FunctionReference.GetMemberGuid().ToString(EGuidFormats::Digits));
        J->SetBoolField(TEXT("self_context"), Call->FunctionReference.IsSelfContext());
    }
    return J;
}
bool Capture(UBlueprint* BP, UEdGraph* G, const TSharedRef<FJsonObject>& Out)
{
    if (!IsValid(BP) || !IsValid(G) || G->Nodes.Num() > 2048 || FBlueprintEditorUtils::FindBlueprintForGraph(G) != BP) { return false; }
    Out->SetStringField(TEXT("graph_path"), G->GetPathName()); Out->SetStringField(TEXT("graph_guid"), G->GraphGuid.ToString(EGuidFormats::Digits));
    Out->SetBoolField(TEXT("dirty"), BP->GetOutermost()->IsDirty()); Out->SetNumberField(TEXT("status"), static_cast<int32>(BP->Status.GetValue()));
    TArray<TSharedPtr<FJsonValue>> Rows; int32 PinCount = 0; TSet<FGuid> NodeIds;
    const auto ObservedNodes=G->Nodes;for (const UEdGraphNode* Node : ObservedNodes)
    {
        if (!IsValid(Node)) { return false; }
        if (Node->IsA<UK2Node>() && (!Node->NodeGuid.IsValid() || NodeIds.Contains(Node->NodeGuid))) { return false; }
        NodeIds.Add(Node->NodeGuid);
        TSet<FGuid> PinIds;
        for (const UEdGraphPin* P : Node->Pins)
        {
            if (++PinCount > 16384 || !P || P->WasTrashed() || !P->PinId.IsValid() || PinIds.Contains(P->PinId)) { return false; }
            PinIds.Add(P->PinId);
            for (const UEdGraphPin* Other : P->LinkedTo)
            { if (!Other || Other->WasTrashed() || !IsValid(Other->GetOwningNodeUnchecked()) || !Other->LinkedTo.Contains(const_cast<UEdGraphPin*>(P))) { return false; } }
        }
        Rows.Add(MakeShared<FJsonValueObject>(NodeState(Node)));if(!InterfaceObservationFence())return false;
    }
    Out->SetArrayField(TEXT("nodes"), Rows); return true;
}
bool ParentStateUnchanged(const TSharedRef<FJsonObject>& Inspection)
{
    if (!Inspection->HasField(TEXT("generated_parents"))) { return true; }
    for (const auto& Value : Inspection->GetArrayField(TEXT("generated_parents")))
    {
        const auto Row = Value->AsObject(); const UBlueprint* BP = FindObject<UBlueprint>(nullptr, *Row->GetStringField(TEXT("blueprint_path")));
        if (!IsValid(BP) || BP->GetOutermost()->IsDirty() != Row->GetBoolField(TEXT("dirty"))
            || static_cast<int32>(BP->Status.GetValue()) != Row->GetIntegerField(TEXT("status"))
            || !BP->GeneratedClass || BP->GeneratedClass->GetPathName() != Row->GetStringField(TEXT("generated_class"))) { return false; }
    }
    return true;
}
bool Prepare(const FString& GraphPath, int32 X, int32 Y, UBlueprint*& BP, UEdGraph*& G,
    TSharedRef<FJsonObject>& Before, TSharedRef<FJsonObject>& Parent, const TSharedRef<FJsonObject>& R, bool bAllowNativeComponent = false)
{
    if (!IsEditorGameThread() || IsPlaySessionActive())
    { return Fail(R, TEXT("event_editor_state_invalid"), TEXT("Event tools require the editor game thread outside PIE/SIE.")); }
    const auto Data = R->GetObjectField(TEXT("data")); const auto Ids = Graph::ReadNodeIdentities(GraphPath);
    if(!DefaultEventCurrent(R))return false;
    Data->SetObjectField(TEXT("identity_preflight"), Ids);
    if (!Ids->GetBoolField(TEXT("success"))) { return Fail(R, TEXT("event_graph_preflight_failed"), TEXT("Existing loaded graph failed the common identity guard.")); }
    G = FindObject<UEdGraph>(nullptr, *GraphPath); BP = G ? FBlueprintEditorUtils::FindBlueprintForGraph(G) : nullptr;
    if (!IsValid(BP) || !IsValid(G) || !IsValid(BP->ParentClass)
        || (!BP->ParentClass->IsChildOf(AActor::StaticClass()) && !(bAllowNativeComponent && BP->ParentClass == UActorComponent::StaticClass()))
        || BP->BlueprintType != BPTYPE_Normal || BP->bBeingCompiled || BP->bQueuedForCompilation || BP->bSuppressStructurallyModified || BP->bIsRegeneratingOnLoad || BP->Status == BS_BeingCreated
        || !G->GraphGuid.IsValid() || G->GetSchema()->GetClass() != UEdGraphSchema_K2::StaticClass()
        || (!BP->UbergraphPages.Contains(G) && !BP->EventGraphs.Contains(G)))
    { return Fail(R, TEXT("event_graph_scope_unsupported"), TEXT("Requires an idle normal Actor Blueprint's directly owned standard K2 event graph; selected event tools also allow an exact native ActorComponent parent.")); }
    if (!Capture(BP, G, Before)) { return Fail(R, TEXT("event_observation_incomplete"), TEXT("Graph snapshot is invalid or exceeds 16384 pins/2048 nodes.")); }
    if(!DefaultEventCurrent(R))return false;
    Data->SetObjectField(TEXT("before"), Before);
    // Coordinates are already int32 typed inputs. Existing nodes are never repositioned.
    Data->SetNumberField(TEXT("requested_x"), X); Data->SetNumberField(TEXT("requested_y"), Y);
    Parent = BP->ParentClass->ClassGeneratedBy ? GeneratedComponent::InspectSavedActorParent(BP->ParentClass) : InspectBlueprintParent(BP->ParentClass->GetPathName());
    if(!DefaultEventCurrent(R))return false;
    Data->SetObjectField(TEXT("parent_inspection"), Parent);
    if (!Parent->GetBoolField(TEXT("success"))) { return Fail(R, TEXT("event_parent_unsupported"), TEXT("Actor parent failed the common saved/runtime class guard.")); }
    for (UClass* Current = BP->ParentClass; Current; Current = Current->GetSuperClass())
    {
        if (Current->HasAnyClassFlags(CLASS_Native) && Current->GetOutermost()->GetName() != TEXT("/Script/Engine") && Current->GetOutermost()->GetName() != TEXT("/Script/CoreUObject"))
        { return Fail(R, TEXT("event_parent_unsupported"), TEXT("Native Actor ancestry is outside audited Engine/CoreUObject modules.")); }
    }
    auto Check = MakeShared<FJsonObject>();
    if (!Capture(BP, G, Check) || Text(Check) != Text(Before) || !ParentStateUnchanged(Parent))
    {
        Data->SetObjectField(TEXT("after_preflight"), Check); R->SetBoolField(TEXT("change_state_known"), false); R->SetField(TEXT("changed"), MakeShared<FJsonValueNull>());
        return Fail(R, TEXT("event_preflight_state_changed"), TEXT("Observation changed or obscured target/parent state; no node creation was dispatched."));
    }
    return true;
}
bool ExactActorEvent(const UFunction* F)
{
    if(!IsValid(F)||F->GetOuterUClass()!=AActor::StaticClass()||AActor::StaticClass()->FindFunctionByName(F->GetFName())!=F)return false;
    const FName N=F->GetFName();return N==TEXT("ReceiveActorBeginOverlap")||N==TEXT("ReceiveActorEndOverlap")||N==TEXT("ReceiveEndPlay");
}
bool PinsMatch(const UEdGraphNode* Node, const TMap<FName, FEdGraphPinType>& Params, bool bParent, bool bNew);
// UF: concrete readonly Game-interface source and existing nonselected graph roles.
// Snapshot uses the existing event capture; callbacks return through the same event fence.
struct FInterfaceEventSource
{
    struct FObject{TStrongObjectPtr<UObject> Hold;UClass* Class=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;FString Path;EObjectFlags Flags=RF_NoFlags;};
    struct FField{FProperty* P=nullptr;FFieldClass* Class=nullptr;FName Name;uint64 Flags=0;int32 Dim=0,Size=0,Offset=0;};
    struct FFunction{TStrongObjectPtr<UFunction> Hold;UClass* Owner=nullptr;EFunctionFlags Flags=FUNC_None;uint8 Count=0;uint16 Size=0;TArray<uint8> Script;TArray<FField> Fields;FString Metadata;};
    struct FPinRoles{UObject* Default=nullptr;UObject* Sub=nullptr;UObject* Terminal=nullptr;UObject* Member=nullptr;};
    struct FNode{TStrongObjectPtr<UEdGraphNode> Hold;FGuid Guid;TArray<UEdGraphPin*> Pins;TArray<FPinRoles> PinRoles;FMemberReference Reference;UObject* ReferenceParent=nullptr;TArray<TSharedPtr<FUserPinInfo>> UserHolds;TArray<FUserPinInfo> Users;TArray<FPinRoles> UserRoles;};
    struct FGraph{TStrongObjectPtr<UEdGraph> Hold;UBlueprint* Owner=nullptr;FGuid Guid,InterfaceId;bool AllowDelete=false;TArray<FNode> Nodes;TSharedPtr<FJsonObject> Before;};
    TSharedRef<FJsonObject> R;UBlueprint* Source=nullptr;UBlueprint* Target=nullptr;UClass* Generated=nullptr;UClass* Skeleton=nullptr;UObject* CDO=nullptr;UObject* SkelCDO=nullptr;EClassFlags GenFlags=CLASS_None,SkelFlags=CLASS_None;
    UFunction* Selected=nullptr;UFunction* PlannedTarget=nullptr;FGuid Guid;FMemberReference PlannedReference,ResolvedReference;FEdGraphPinType PlannedDelegate;
    TArray<FObject> Objects;TArray<FFunction> Functions;TArray<FGraph> Graphs;TArray<TObjectPtr<UEdGraph>> SourceGraphs,TargetFunctions,TargetUber,TargetEvents,TargetMacros,TargetDelegates;
    TArray<FBPInterfaceDescription> Interfaces;TArray<UFunction*> GenFunctions,SkelFunctions;FString Filename,FileHash;bool Bound=false,Ready=false,Observing=false,Stopped=false;
    explicit FInterfaceEventSource(TSharedRef<FJsonObject> In):R(In){}
    static bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
    bool Stop(const TCHAR* Stage)
    {if(!Stopped){Fail(R,TEXT("interface_event_source_changed"),FString(TEXT("Interface event source/target role changed at "))+Stage+TEXT("; actual prefix retained, no further SDK/retry."));R->GetObjectField(TEXT("data"))->SetStringField(TEXT("interface_event_first_mismatch"),Stage);}Stopped=true;R->SetBoolField(TEXT("success"),false);R->SetBoolField(TEXT("postcondition_passed"),false);R->SetBoolField(TEXT("change_state_known"),false);R->SetField(TEXT("changed"),MakeShared<FJsonValueNull>());return false;}
    bool Hold(UObject* O)
    {
        if(!Live(O)||!Live(O->GetClass())||!Live(O->GetOuter())||!Live(O->GetPackage()))return false;for(const auto& V:Objects)if(V.Hold.Get()==O)return true;
        FObject V;V.Hold.Reset(O);V.Class=O->GetClass();V.Outer=O->GetOuter();V.Package=O->GetPackage();V.Path=O->GetPathName();V.Flags=O->GetFlags();Objects.Add(MoveTemp(V));return true;
    }
    // Public reflected native USTRUCT cell; inspect the handle before public reference getters.
    static bool ReferenceParent(const FMemberReference& Ref,UObject*& Out)
    {
        auto* S=FMemberReference::StaticStruct();auto* P=FindFProperty<FObjectProperty>(S,TEXT("MemberParent"));
        if(!P||P->GetClass()!=FObjectProperty::StaticClass()||P->GetOwnerStruct()!=S||P->ArrayDim!=1||P->HasSetterOrGetter()||P->GetSize()!=sizeof(TObjectPtr<UObject>))return false;
        const auto* Slot=P->ContainerPtrToValuePtr<TObjectPtr<UObject>>(&Ref);if(!Slot||!Slot->IsResolved())return false;Out=Slot->Get();return true;
    }
    static const FMemberReference* NodeReference(const UEdGraphNode* N)
    {
        if(const auto* Entry=Cast<UK2Node_FunctionEntry>(N))return &Entry->FunctionReference;
        if(const auto* ResultNode=Cast<UK2Node_FunctionResult>(N))return &ResultNode->FunctionReference;
        if(const auto* EventNode=Cast<UK2Node_Event>(N))return &EventNode->EventReference;
        if(const auto* CallNode=Cast<UK2Node_CallFunction>(N))return &CallNode->FunctionReference;
        return nullptr;
    }
    // Weak type slots are object-index/serial references, not lazy object handles.
    // Explicit null is valid; stale/pending-kill/non-null-invalid slots are rejected before Get/hold.
    static bool WeakRole(const TWeakObjectPtr<UObject>& Slot)
    {return Slot.IsExplicitlyNull()||Slot.IsValid();}
    bool BindType(const FEdGraphPinType& Type,FPinRoles& Roles)
    {
        if(!FInterfaceEventSource::WeakRole(Type.PinSubCategoryObject)||!FInterfaceEventSource::WeakRole(Type.PinValueType.TerminalSubCategoryObject)||!Type.PinSubCategoryMemberReference.MemberParent.IsResolved())return false;
        Roles.Sub=Type.PinSubCategoryObject.Get();Roles.Terminal=Type.PinValueType.TerminalSubCategoryObject.Get();Roles.Member=Type.PinSubCategoryMemberReference.MemberParent.Get();
        for(auto* O:{Roles.Sub,Roles.Terminal,Roles.Member})if(O&&!Hold(O))return false;return true;
    }
    static bool TypeCurrent(const FEdGraphPinType& Type,const FPinRoles& Roles)
    {return FInterfaceEventSource::WeakRole(Type.PinSubCategoryObject)&&FInterfaceEventSource::WeakRole(Type.PinValueType.TerminalSubCategoryObject)&&Type.PinSubCategoryMemberReference.MemberParent.IsResolved()&&Type.PinSubCategoryObject.Get()==Roles.Sub&&Type.PinValueType.TerminalSubCategoryObject.Get()==Roles.Terminal&&Type.PinSubCategoryMemberReference.MemberParent.Get()==Roles.Member;}
    static bool Primitive(FProperty* P)
    {const auto* C=P->GetClass();return C==FBoolProperty::StaticClass()||C==FIntProperty::StaticClass()||C==FInt64Property::StaticClass()||C==FFloatProperty::StaticClass()||C==FDoubleProperty::StaticClass()||C==FNameProperty::StaticClass()||C==FStrProperty::StaticClass()||C==FTextProperty::StaticClass();}
    static FString Metadata(const FFunction& F)
    {
        FString V;for(const auto& P:F.Fields){const FName K(*(TEXT("CPP_Default_")+P.Name.ToString()));const FString Cell=F.Hold->GetMetaData(K);V+=K.ToString()+TEXT(":")+LexToString(F.Hold->HasMetaData(K))+TEXT(":")+LexToString(Cell.Len())+TEXT(":")+Cell;}
        for(const TCHAR* K:{TEXT("BlueprintInternalUseOnly"),TEXT("DeprecatedFunction"),TEXT("ForceAsFunction"),TEXT("AnimBlueprintFunction"),TEXT("Latent"),TEXT("BlueprintThreadSafe"),TEXT("NotBlueprintThreadSafe")}){const FString Cell=F.Hold->GetMetaData(K);V+=FString(K)+TEXT(":")+LexToString(F.Hold->HasMetaData(K))+TEXT(":")+LexToString(Cell.Len())+TEXT(":")+Cell;}return V;
    }
    bool Function(UFunction* F,UClass* Owner,TArray<UFunction*>& Order)
    {
        if(!Live(F)||F->GetClass()!=UFunction::StaticClass()||F->GetOuter()!=Owner||F->GetSuperFunction()||!Hold(F))return false;Order.Add(F);FFunction V;V.Hold.Reset(F);V.Owner=Owner;V.Flags=F->FunctionFlags;V.Count=F->NumParms;V.Size=F->ParmsSize;V.Script=F->Script;
        for(TFieldIterator<FProperty> It(F,EFieldIterationFlags::None);It;++It){auto* P=*It;if(V.Fields.Num()>=64||!Primitive(P)||P->GetOwnerStruct()!=F||P->GetOwnerProperty()!=P||P->HasSetterOrGetter()||P->ArrayDim!=1||!P->HasAnyPropertyFlags(CPF_Parm))return false;V.Fields.Add({P,P->GetClass(),P->GetFName(),uint64(P->GetPropertyFlags()),P->ArrayDim,P->GetSize(),P->GetOffset_ForInternal()});}
        V.Metadata=Metadata(V);Functions.Add(MoveTemp(V));return true;
    }
    bool Graph(UBlueprint* B,UEdGraph* G,bool SourceGraph)
    {
        if(!Live(G)||G->GetClass()!=UEdGraph::StaticClass()||G->GetOuter()!=B||!G->Schema.GetGCPtr().IsResolved()||G->Schema.Get()!=UEdGraphSchema_K2::StaticClass()||!G->GraphGuid.IsValid()||!G->SubGraphs.IsEmpty()||G->Nodes.Num()>2048||!Hold(G))return false;
        FGraph V;V.Hold.Reset(G);V.Owner=B;V.Guid=G->GraphGuid;V.InterfaceId=G->InterfaceGuid;V.AllowDelete=G->bAllowDeletion;
        for(const auto& NodeSlot:G->Nodes)
        {
            if(!NodeSlot.IsResolved())return false;UEdGraphNode* N=NodeSlot.Get();
            if(!Live(N)||N->GetOuter()!=G||!N->NodeGuid.IsValid()||N->Pins.Num()>256||!Hold(N)||(SourceGraph&&N->GetClass()!=UK2Node_FunctionEntry::StaticClass()&&N->GetClass()!=UK2Node_FunctionResult::StaticClass()))return false;
            FNode Node;Node.Hold.Reset(N);Node.Guid=N->NodeGuid;Node.Pins=N->Pins;
            if(const auto* Ref=NodeReference(N)){if(!ReferenceParent(*Ref,Node.ReferenceParent)||(Node.ReferenceParent&&!Hold(Node.ReferenceParent)))return false;Node.Reference=*Ref;}
            for(auto* P:N->Pins)
            {
                if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||!P->DefaultObject.IsResolved())return false;
                FPinRoles Roles;Roles.Default=P->DefaultObject.Get();if((Roles.Default&&!Hold(Roles.Default))||!BindType(P->PinType,Roles))return false;Node.PinRoles.Add(Roles);
            }
            if(auto* Editable=Cast<UK2Node_EditablePinBase>(N)){if(Editable->UserDefinedPins.Num()>64)return false;for(const auto& U:Editable->UserDefinedPins){if(!U)return false;FPinRoles Roles;if(!BindType(U->PinType,Roles))return false;Node.UserHolds.Add(U);Node.Users.Add(*U);Node.UserRoles.Add(Roles);}}
            V.Nodes.Add(MoveTemp(Node));
        }Graphs.Add(MoveTemp(V));return true;
    }
    bool Pure()
    {
        if(!Bound)return !Stopped;if(Stopped||!IsEditorGameThread()||IsPlaySessionActive()||IsGarbageCollecting()||UE::IsSavingPackage()||GExitPurge||IsEngineExitRequested())return Stop(TEXT("phase"));
        for(const auto& V:Objects){auto* O=V.Hold.Get();if(!Live(O)||!Live(V.Class)||!Live(V.Outer)||!Live(V.Package)||O->GetClass()!=V.Class||O->GetOuter()!=V.Outer||O->GetPackage()!=V.Package||O->GetFlags()!=V.Flags||!O->GetPathName().Equals(V.Path,ESearchCase::CaseSensitive))return Stop(TEXT("held_object"));}
        if(Source->BlueprintType!=BPTYPE_Interface||Source->Status!=BS_UpToDate||Source->GetPackage()->IsDirty()||Source->bBeingCompiled||Source->bQueuedForCompilation||Source->bIsRegeneratingOnLoad||Source->bSuppressStructurallyModified||!Source->ParentClass.GetGCPtr().IsResolved()||Source->ParentClass.Get()!=UInterface::StaticClass()||!Source->GeneratedClass.GetGCPtr().IsResolved()||!Source->SkeletonGeneratedClass.GetGCPtr().IsResolved()||Source->GeneratedClass.Get()!=Generated||Source->SkeletonGeneratedClass.Get()!=Skeleton||Source->FunctionGraphs!=SourceGraphs||!Source->NewVariables.IsEmpty()||!Source->ImplementedInterfaces.IsEmpty()||!Source->MacroGraphs.IsEmpty()||!Source->UbergraphPages.IsEmpty()||!Source->DelegateSignatureGraphs.IsEmpty())return Stop(TEXT("source_blueprint"));
        for(auto* C:{Generated,Skeleton})if(C->GetClass()!=UBlueprintGeneratedClass::StaticClass()||C->GetClassFlags()!=(C==Generated?GenFlags:SkelFlags)||C->GetSuperClass()!=UInterface::StaticClass()||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy.Get()!=Source||!C->ClassDefaultObject.IsResolved()||C->ClassDefaultObject.Get()!=(C==Generated?CDO:SkelCDO)||C->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists))return Stop(TEXT("source_class_CDO"));
        if(Target->ImplementedInterfaces.Num()!=Interfaces.Num()||Target->FunctionGraphs!=TargetFunctions||Target->UbergraphPages!=TargetUber||Target->EventGraphs!=TargetEvents||Target->MacroGraphs!=TargetMacros||Target->DelegateSignatureGraphs!=TargetDelegates)return Stop(TEXT("target_graph_interface_inventory"));
        for(int32 I=0;I<Interfaces.Num();++I){auto A=Target->ImplementedInterfaces[I];if(!A.Interface.GetGCPtr().IsResolved()||A.Interface.Get()!=Interfaces[I].Interface.Get()||A.Graphs!=Interfaces[I].Graphs)return Stop(TEXT("target_interface_slot"));}
        for(int32 Kind=0;Kind<2;++Kind){UClass* C=Kind==0?Generated:Skeleton;TArray<UFunction*> Now;for(TFieldIterator<UFunction> It(C,EFieldIterationFlags::None);It;++It){if(Now.Num()>=32)return Stop(TEXT("source_function_bound"));Now.Add(*It);}if(Now!=(Kind==0?GenFunctions:SkelFunctions))return Stop(TEXT("source_function_inventory"));}
        for(const auto& F:Functions){auto* Fn=F.Hold.Get();if(Fn->GetOuter()!=F.Owner||Fn->FunctionFlags!=F.Flags||Fn->NumParms!=F.Count||Fn->ParmsSize!=F.Size||Fn->Script!=F.Script)return Stop(TEXT("source_function"));TArray<FProperty*> Rows;for(TFieldIterator<FProperty> It(Fn,EFieldIterationFlags::None);It;++It)Rows.Add(*It);if(Rows.Num()!=F.Fields.Num())return Stop(TEXT("source_field_count"));for(int32 I=0;I<Rows.Num();++I){const auto& P=F.Fields[I];if(Rows[I]!=P.P||P.P->GetClass()!=P.Class||P.P->GetOwnerStruct()!=Fn||P.P->GetOwnerProperty()!=P.P||P.P->GetFName()!=P.Name||uint64(P.P->GetPropertyFlags())!=P.Flags||P.P->ArrayDim!=P.Dim||P.P->GetSize()!=P.Size||P.P->GetOffset_ForInternal()!=P.Offset||P.P->HasSetterOrGetter())return Stop(TEXT("source_field"));}if(!Metadata(F).Equals(F.Metadata,ESearchCase::CaseSensitive))return Stop(TEXT("source_defaults_metadata"));}
        for(const auto& V:Graphs)
        {
            auto* G=V.Hold.Get();if(G->GetOuter()!=V.Owner||G->GraphGuid!=V.Guid||G->InterfaceGuid!=V.InterfaceId||G->bAllowDeletion!=V.AllowDelete||!G->Schema.GetGCPtr().IsResolved()||G->Schema.Get()!=UEdGraphSchema_K2::StaticClass()||!G->SubGraphs.IsEmpty()||G->Nodes.Num()!=V.Nodes.Num())return Stop(TEXT("source_or_peer_graph"));
            for(int32 I=0;I<V.Nodes.Num();++I)
            {
                const auto& N=V.Nodes[I];if(G->Nodes[I]!=N.Hold.Get()||N.Hold->NodeGuid!=N.Guid||N.Hold->Pins!=N.Pins)return Stop(TEXT("source_or_peer_node"));
                if(const auto* Ref=NodeReference(N.Hold.Get())){UObject* ParentObject=nullptr;if(!ReferenceParent(*Ref,ParentObject)||ParentObject!=N.ReferenceParent||!Ref->IsSameReference(N.Reference)||Ref->IsDeprecated()!=N.Reference.IsDeprecated())return Stop(TEXT("source_or_peer_reference"));}
                for(int32 J=0;J<N.Pins.Num();++J){auto* P=N.Pins[J];const auto& Roles=N.PinRoles[J];if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N.Hold.Get()||!P->DefaultObject.IsResolved()||P->DefaultObject.Get()!=Roles.Default||!TypeCurrent(P->PinType,Roles))return Stop(TEXT("source_or_peer_pin"));}
                if(auto* Editable=Cast<UK2Node_EditablePinBase>(N.Hold.Get())){if(Editable->UserDefinedPins.Num()!=N.Users.Num())return Stop(TEXT("source_user_count"));for(int32 K=0;K<N.Users.Num();++K){const auto& U=Editable->UserDefinedPins[K];const auto& BeforeUser=N.Users[K];if(U!=N.UserHolds[K]||!U||!TypeCurrent(U->PinType,N.UserRoles[K])||U->PinName!=BeforeUser.PinName||U->DesiredPinDirection!=BeforeUser.DesiredPinDirection||U->PinType!=BeforeUser.PinType||!U->PinDefaultValue.Equals(BeforeUser.PinDefaultValue,ESearchCase::CaseSensitive))return Stop(TEXT("source_user_pin"));}}
            }
        }
        return true;
    }
    bool Check()
    {
        if(!Pure())return false;if(!Ready||Observing)return true;TGuardValue<bool> Guard(Observing,true);
        for(const auto& G:Graphs){auto Now=MakeShared<FJsonObject>();if(!Capture(G.Owner,G.Hold.Get(),Now)||!Pure())return false;for(const TCHAR* K:{TEXT("graph_path"),TEXT("graph_guid"),TEXT("nodes")}){auto A=MakeShared<FJsonObject>(),B=MakeShared<FJsonObject>();A->SetField(K,G.Before->TryGetField(K));B->SetField(K,Now->TryGetField(K));if(!Text(A).Equals(Text(B),ESearchCase::CaseSensitive))return Stop(TEXT("source_or_peer_snapshot"));}}return Pure();
    }
    bool File(bool CaptureFile)
    {if(!Pure())return false;TArray<uint8> Bytes;if(!FFileHelper::LoadFileToArray(Bytes,*Filename)||!Pure())return Stop(TEXT("source_file_read"));const FString Hash=FMD5::HashBytes(Bytes.GetData(),Bytes.Num());if(CaptureFile)FileHash=Hash;else if(Hash!=FileHash)return Stop(TEXT("source_file_changed"));return true;}
    bool Bind(UBlueprint* B,UEdGraph* SelectedGraph,UFunction* F)
    {
        if(B->ParentClass.Get()!=AActor::StaticClass())return false;Target=B;Selected=F;UClass* Decl=Live(F)?F->GetOuterUClass():nullptr;if(!Live(Decl)||Decl->GetClass()!=UBlueprintGeneratedClass::StaticClass()||!Decl->ClassGeneratedBy.IsResolved())return false;Source=Cast<UBlueprint>(Decl->ClassGeneratedBy.Get());
        if(!Live(Source)||Source==B||Source->GetClass()!=UBlueprint::StaticClass()||Source->BlueprintType!=BPTYPE_Interface||Source->Status!=BS_UpToDate||Source->GetPackage()->IsDirty()||Source->GetPackage()==B->GetPackage()||!Source->GetPackage()->GetName().StartsWith(TEXT("/Game/"))||!Source->GeneratedClass.GetGCPtr().IsResolved()||!Source->SkeletonGeneratedClass.GetGCPtr().IsResolved()||!Source->ParentClass.GetGCPtr().IsResolved()||Source->ParentClass.Get()!=UInterface::StaticClass())return false;
        Generated=Source->GeneratedClass.Get();Skeleton=Source->SkeletonGeneratedClass.Get();if(!Live(Generated)||!Live(Skeleton)||Decl!=Skeleton||!Generated->ClassDefaultObject.IsResolved()||!Skeleton->ClassDefaultObject.IsResolved()||!Hold(Source)||!Hold(B)||!Hold(Generated)||!Hold(Skeleton)||!Hold(F))return false;
        GenFlags=Generated->GetClassFlags();SkelFlags=Skeleton->GetClassFlags();CDO=Generated->ClassDefaultObject.Get();SkelCDO=Skeleton->ClassDefaultObject.Get();if((CDO&&!Hold(CDO))||(SkelCDO&&!Hold(SkelCDO)))return false;
        SourceGraphs=Source->FunctionGraphs;TargetFunctions=B->FunctionGraphs;TargetUber=B->UbergraphPages;TargetEvents=B->EventGraphs;TargetMacros=B->MacroGraphs;TargetDelegates=B->DelegateSignatureGraphs;Interfaces=B->ImplementedInterfaces;
        bool Found=false;for(auto D:Interfaces){if(!D.Interface.GetGCPtr().IsResolved()||!Live(D.Interface.Get())||!Hold(D.Interface.Get()))return false;Found|=D.Interface.Get()==Generated;}if(!Found||SourceGraphs.Num()>32)return false;
        for(auto* C:{Generated,Skeleton}){auto& Order=C==Generated?GenFunctions:SkelFunctions;for(TFieldIterator<UFunction> It(C,EFieldIterationFlags::None);It;++It)if(Order.Num()>=32||!Function(*It,C,Order))return false;}
        for(const auto& P:SourceGraphs){if(!P.IsResolved()||!Graph(Source,P.Get(),true))return false;if(P->GetFName()==F->GetFName()){if(Guid.IsValid())return false;Guid=P->GraphGuid;}}
        TArray<UEdGraph*> Peers;B->GetAllGraphs(Peers);if(Peers.Num()>256)return false;for(auto* G:Peers)if(G!=SelectedGraph&&!Graph(B,G,false))return false;
        if(!Guid.IsValid())return false;Filename=FPackageName::LongPackageNameToFilename(Source->GetPackage()->GetName(),FPackageName::GetAssetPackageExtension());Bound=true;if(!Pure())return false;
        for(auto& G:Graphs){G.Before=MakeShared<FJsonObject>();if(!Capture(G.Owner,G.Hold.Get(),G.Before.ToSharedRef())||!Pure())return false;}
        Ready=true;if(!Check()||!File(true))return false;
        PlannedReference.SetExternalMember(F->GetFName(),Generated);ResolvedReference=PlannedReference;PlannedTarget=ResolvedReference.ResolveMember<UFunction>(B->SkeletonGeneratedClass.Get());if(PlannedTarget&&!Hold(PlannedTarget))return false;if(!Check()||!PlannedTarget||(PlannedTarget->GetOuter()!=Generated&&PlannedTarget->GetOuter()!=Skeleton)||PlannedTarget->GetFName()!=F->GetFName())return false;
        FMemberReference ExpectedResolved=PlannedReference;ExpectedResolved.SetExternalMember(F->GetFName(),Generated,Guid);
        if(!ResolvedReference.IsSameReference(ExpectedResolved)||ResolvedReference.IsDeprecated()!=PlannedReference.IsDeprecated())return Stop(TEXT("planned_reference_resolution_shape"));
        R->GetObjectField(TEXT("data"))->SetStringField(TEXT("interface_event_reference_planned_resolved_guid"),ResolvedReference.GetMemberGuid().ToString(EGuidFormats::Digits));
        PlannedDelegate.PinCategory=UEdGraphSchema_K2::PC_Delegate;FMemberReference::FillSimpleMemberReference<UFunction>(PlannedTarget,PlannedDelegate.PinSubCategoryMemberReference);if(!Check())return false;
        auto Data=R->GetObjectField(TEXT("data"));Data->SetStringField(TEXT("interface_event_source"),Source->GetPathName());Data->SetStringField(TEXT("interface_event_declared_function"),F->GetPathName());Data->SetStringField(TEXT("interface_event_resolved_function"),PlannedTarget->GetPathName());Data->SetStringField(TEXT("interface_event_source_guid"),Guid.ToString());Data->SetStringField(TEXT("interface_event_source_file_hash"),FileHash);Data->SetBoolField(TEXT("interface_event_source_snapshot_ready"),true);return Check();
    }
};
// ZS: one concrete native compound-signature cohort; no source CDO/value access.
bool NativeCompoundEvent(UFunction* F)
{
    if(!FInterfaceEventSource::Live(F)||ExactActorEvent(F))return false;
    UClass* Owner=F->GetOuterUClass();if(!FInterfaceEventSource::Live(Owner)||!Owner->HasAnyClassFlags(CLASS_Native)||Owner->GetPackage()->GetName()!=TEXT("/Script/Engine"))return false;
    for(TFieldIterator<FProperty> It(F);It&&It->HasAnyPropertyFlags(CPF_Parm);++It)if(!FInterfaceEventSource::Primitive(*It)||It->HasAnyPropertyFlags(CPF_ReferenceParm))return true;
    return false;
}
struct FActorEventScope
{
    struct FHeld{TStrongObjectPtr<UObject> Object;UClass* Class=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;FString Path;EObjectFlags Flags=RF_NoFlags;uint32 PackageFlags=0;bool Dirty=false;};
    struct FOld{TStrongObjectPtr<UEdGraphNode> Node;FGuid Guid;TArray<UEdGraphPin*> Pins;TArray<FGuid> PinIds;TArray<TArray<UEdGraphPin*>> Links;TArray<UObject*> DefaultObjects;TArray<FEdGraphPinType> Types;TArray<FString> Values,Autos;TArray<FText> Texts;FMemberReference Reference;UObject* ReferenceParent=nullptr;FString Scope;bool Event=false,Call=false,Deprecated=false;};
    struct FNewPin{UEdGraphPin* Pin=nullptr;FGuid Id;FName Name;EEdGraphPinDirection Direction=EGPD_Output;FEdGraphPinType Type;FString DefaultValue,AutogeneratedDefaultValue,DefaultText;FText TextValue;bool Hidden=false;};
    TArray<FNewPin> NewPins;bool PinsFrozen=false,ReferenceCacheFilled=false;
    TArray<FHeld> Held;TArray<FOld> Old;TArray<UEdGraphNode*> ExpectedNodes;TStrongObjectPtr<UK2Node_Event> Added;
    TSharedRef<FJsonObject> R;FInterfaceEventSource InterfaceSource;bool InterfaceMode=false,NativeMode=false;bool Active=false,Stopped=false,Configured=false,GuidAdopted=false;UBlueprint* BP=nullptr;UEdGraph* Graph=nullptr;UClass* Parent=nullptr;UClass* Generated=nullptr;UClass* Skeleton=nullptr;UClass* SchemaClass=nullptr;UObject* SchemaCDO=nullptr;UObject* GeneratedCDO=nullptr;UObject* SkeletonCDO=nullptr;UFunction* Function=nullptr;
    UClass* ActorSuper=nullptr;UObject* ActorCDO=nullptr;EClassFlags ActorFlags=CLASS_None;EFunctionFlags FunctionFlags=FUNC_None;FName EventName;FGuid GraphGuid,AddedGuid;FString AddedPath;EObjectFlags AddedFlags=RF_NoFlags;int32 X=0,Y=0;uint16 ParamBytes=0;TArray<FProperty*> ParamFields;TArray<EPropertyFlags> ParamFlags;TArray<FString> EnumNames;TArray<int64> EnumValues;UEnum* EndEnum=nullptr;
    struct FNativeField
    {
        FProperty* P=nullptr;FFieldClass* Class=nullptr;FName Name;uint64 Flags=0;int32 Dim=0,Size=0,Offset=0;UStruct* Owner=nullptr;const FProperty* OwnerProperty=nullptr;
        FProperty* Inner=nullptr;FProperty* Other=nullptr;UObject* Type=nullptr;UObject* Meta=nullptr;
    };
    struct FNativeType
    {
        UObject* Object=nullptr;UStruct* Super=nullptr;uint32 Flags=0;int32 Size=0,Align=0;TArray<FProperty*> Fields;TArray<FName> Names;TArray<int64> Values;
    };
    TArray<FNativeField> NativeFields;TArray<FNativeType> NativeTypes;TArray<FProperty*> NativeRootFields;
    FInterfaceEventSource::FFunction NativeMetadata;UClass* NativeOwner=nullptr;bool NativeBound=false;
    TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;
    explicit FActorEventScope(TSharedRef<FJsonObject> In):R(In),InterfaceSource(In){}
    bool Loaded(const UObject* O)const{return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
    bool Stop(const TCHAR* Stage)
    {if(!Stopped){Fail(R,TEXT("event_actor_scope_changed"),TEXT("Selected event source/current roles changed; no next explicit SDK or automatic replay."));auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("boundary"),Stage);R->GetObjectField(TEXT("data"))->SetObjectField(TEXT("actor_event_first_failure"),J);}Stopped=true;R->SetBoolField(TEXT("success"),false);R->SetBoolField(TEXT("postcondition_passed"),false);R->SetBoolField(TEXT("change_state_known"),false);R->SetField(TEXT("changed"),MakeShared<FJsonValueNull>());return false;}
    bool Mismatch(const TCHAR* Stage,const FString& ObjectPath,const TCHAR* Property,const FString& Expected,const FString& Actual)
    {
        const bool First=!Stopped;Stop(Stage);if(First){auto J=R->GetObjectField(TEXT("data"))->GetObjectField(TEXT("actor_event_first_failure"));J->SetStringField(TEXT("object_path"),ObjectPath);J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);}return false;
    }
    FString RolePath(const UObject* O)const{return Loaded(O)?O->GetPathName():O?TEXT("unavailable_nonnull"):TEXT("null");}
    bool InterfaceReferenceCurrent(const TCHAR* Stage)
    {
        // ResolveMember mutates only its copied reference during planning; original authoring starts at GUID0.
        // Permit the exact SDK cache fill once, retaining every authored binding field and monotonic GUID state.
        const auto& Ref=Added->EventReference;UObject* ActualParent=nullptr;
        if(!FInterfaceEventSource::ReferenceParent(Ref,ActualParent))return Mismatch(Stage,AddedPath,TEXT("EventReference.MemberParent.IsResolved"),TEXT("true"),TEXT("false"));
        if(ActualParent!=InterfaceSource.Generated)return Mismatch(Stage,AddedPath,TEXT("EventReference.MemberParent"),RolePath(InterfaceSource.Generated),RolePath(ActualParent));
        if(Ref.GetMemberName()!=EventName)return Mismatch(Stage,AddedPath,TEXT("EventReference.MemberName"),EventName.ToString(),Ref.GetMemberName().ToString());
        if(!Ref.GetMemberScopeName().IsEmpty())return Mismatch(Stage,AddedPath,TEXT("EventReference.MemberScope"),TEXT(""),Ref.GetMemberScopeName());
        if(Ref.IsSelfContext())return Mismatch(Stage,AddedPath,TEXT("EventReference.bSelfContext"),TEXT("false"),TEXT("true"));
        if(Ref.IsLocalScope())return Mismatch(Stage,AddedPath,TEXT("EventReference.IsLocalScope"),TEXT("false"),TEXT("true"));
        if(Ref.IsDeprecated())return Mismatch(Stage,AddedPath,TEXT("EventReference.bWasDeprecated"),TEXT("false"),TEXT("true"));
        const FGuid ActualGuid=Ref.GetMemberGuid();const auto& Initial=InterfaceSource.PlannedReference;const auto& Resolved=InterfaceSource.ResolvedReference;
        if(ActualGuid==Resolved.GetMemberGuid()){if(!Ref.IsSameReference(Resolved))return Mismatch(Stage,AddedPath,TEXT("EventReference"),TEXT("exact resolved binding"),TEXT("other binding"));ReferenceCacheFilled=true;}
        else if(ReferenceCacheFilled||!Ref.IsSameReference(Initial))return Mismatch(Stage,AddedPath,TEXT("EventReference.MemberGuid"),ReferenceCacheFilled?Resolved.GetMemberGuid().ToString(EGuidFormats::Digits):Initial.GetMemberGuid().ToString(EGuidFormats::Digits)+TEXT(" or ")+Resolved.GetMemberGuid().ToString(EGuidFormats::Digits),ActualGuid.ToString(EGuidFormats::Digits));
        auto Data=R->GetObjectField(TEXT("data"));Data->SetStringField(TEXT("interface_event_reference_observed_guid"),ActualGuid.ToString(EGuidFormats::Digits));Data->SetBoolField(TEXT("interface_event_reference_cache_filled"),ReferenceCacheFilled);return true;
    }
    bool Context()
    {if(!DefaultEventCurrent(R))return false;if(!Active)return true;if(Stopped)return false;if(!IsEditorGameThread()||IsPlaySessionActive()||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||!GEditor||GEditor!=Editor.Get()||!World.IsValid()||World->bIsTearingDown||GEditor->GetEditorWorldContext().World()!=World.Get())return Stop(TEXT("context"));return true;}
    bool PinObject(UObject* O)
    {
        if(!Context()||!Loaded(O)||!Loaded(O->GetClass())||!Loaded(O->GetPackage())||(O->GetOuter()&&!Loaded(O->GetOuter())))return Stop(TEXT("capture_identity"));
        for(const auto& I:Held)if(I.Object.Get()==O)return true;
        FHeld I;I.Object.Reset(O);I.Class=O->GetClass();I.Outer=O->GetOuter();I.Package=O->GetPackage();I.Path=O->GetPathName();I.Flags=O->GetFlags();I.PackageFlags=I.Package->GetPackageFlags();I.Dirty=I.Package->IsDirty();Held.Add(MoveTemp(I));return true;
    }
    bool NativeParts(FProperty* P,FNativeField& V)const
    {
        if(!P||P->HasSetterOrGetter()||P->ArrayDim!=1||P->HasMetaData(TEXT("BitmaskEnum")))return false;
        V.P=P;V.Class=P->GetClass();V.Name=P->GetFName();V.Flags=uint64(P->GetPropertyFlags());V.Dim=P->ArrayDim;V.Size=P->GetSize();V.Offset=P->GetOffset_ForInternal();V.Owner=P->GetOwnerStruct();V.OwnerProperty=P->GetOwnerProperty();
        if(auto* A=CastField<FArrayProperty>(P)){V.Inner=A->Inner;return V.Inner!=nullptr;}
        if(auto* A=CastField<FSetProperty>(P)){V.Inner=A->ElementProp;return V.Inner!=nullptr;}
        if(auto* A=CastField<FMapProperty>(P)){V.Inner=A->KeyProp;V.Other=A->ValueProp;return V.Inner&&V.Other;}
        if(auto* A=CastField<FStructProperty>(P)){if(!A->Struct.IsResolved())return false;V.Type=A->Struct.Get();return V.Type!=nullptr;}
        if(auto* A=CastField<FClassProperty>(P)){if(!A->PropertyClass.IsResolved()||!A->MetaClass.IsResolved())return false;V.Type=A->PropertyClass.Get();V.Meta=A->MetaClass.Get();return V.Type&&V.Meta;}
        if(auto* A=CastField<FSoftClassProperty>(P)){if(!A->PropertyClass.IsResolved()||!A->MetaClass.IsResolved())return false;V.Type=A->PropertyClass.Get();V.Meta=A->MetaClass.Get();return V.Type&&V.Meta;}
        if(auto* A=CastField<FObjectPropertyBase>(P)){if(!A->PropertyClass.IsResolved())return false;V.Type=A->PropertyClass.Get();return V.Type!=nullptr;}
        if(auto* A=CastField<FInterfaceProperty>(P)){if(!A->InterfaceClass.IsResolved())return false;V.Type=A->InterfaceClass.Get();return V.Type!=nullptr;}
        if(auto* A=CastField<FByteProperty>(P)){if(!A->Enum.IsResolved())return false;V.Type=A->Enum.Get();return true;}
        if(auto* A=CastField<FEnumProperty>(P)){V.Type=A->GetEnum();V.Inner=A->GetUnderlyingProperty();return V.Type&&V.Inner;}
        return FInterfaceEventSource::Primitive(P)||P->GetClass()==FInt8Property::StaticClass()||P->GetClass()==FInt16Property::StaticClass()||P->GetClass()==FUInt16Property::StaticClass()||P->GetClass()==FUInt32Property::StaticClass()||P->GetClass()==FUInt64Property::StaticClass();
    }
    bool NativeType(UObject* Object,int32 Depth)
    {
        if(!Object)return true;if(Depth>32||NativeTypes.Num()>=128||!PinObject(Object)||!Loaded(Object->GetPackage()))return false;
        for(const auto& V:NativeTypes)if(V.Object==Object)return true;
        const FString Package=Object->GetPackage()->GetName();if((Package!=TEXT("/Script/Engine")&&Package!=TEXT("/Script/CoreUObject")&&Package!=TEXT("/Script/PhysicsCore"))||!Object->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||Object->GetPackage()->HasAnyPackageFlags(PKG_EditorOnly)||Object->GetPackage()->IsDirty())return false;
        FNativeType V;V.Object=Object;
        if(auto* C=Cast<UClass>(Object))
        {
            if(C->GetClass()!=UClass::StaticClass()||!C->HasAnyClassFlags(CLASS_Native)||C->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists)||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy.Get()!=nullptr||C->IsEditorOnly())return false;
            V.Super=C->GetSuperClass();V.Flags=uint32(C->ClassFlags);V.Size=C->GetStructureSize();V.Align=C->GetMinAlignment();NativeTypes.Add(MoveTemp(V));return !C->GetSuperClass()||NativeType(C->GetSuperClass(),Depth+1);
        }
        if(auto* S=Cast<UScriptStruct>(Object))
        {
            if(!(S->StructFlags&STRUCT_Native)||S->GetClass()!=UScriptStruct::StaticClass())return false;V.Super=S->GetSuperStruct();V.Flags=uint32(S->StructFlags);V.Size=S->GetStructureSize();V.Align=S->GetMinAlignment();
            for(TFieldIterator<FProperty> It(S,EFieldIterationFlags::None);It;++It){if(V.Fields.Num()>=256)return false;V.Fields.Add(*It);}const auto Fields=V.Fields;NativeTypes.Add(MoveTemp(V));
            if(S->GetSuperStruct()&&!NativeType(S->GetSuperStruct(),Depth+1))return false;for(auto* P:Fields)if(!NativeField(P,Depth+1))return false;return true;
        }
        if(auto* E=Cast<UEnum>(Object))
        {
            if(E->GetClass()!=UEnum::StaticClass()||E->NumEnums()<1||E->NumEnums()>1024)return false;for(int32 I=0;I<E->NumEnums();++I){V.Names.Add(E->GetNameByIndex(I));V.Values.Add(E->GetValueByIndex(I));}NativeTypes.Add(MoveTemp(V));return true;
        }return false;
    }
    bool NativeField(FProperty* P,int32 Depth)
    {
        if(Depth>32||NativeFields.Num()>=1024)return false;for(const auto& V:NativeFields)if(V.P==P)return true;FNativeField V;if(!NativeParts(P,V)||!Loaded(V.Owner))return false;NativeFields.Add(V);
        if(!NativeType(V.Type,Depth+1)||!NativeType(V.Meta,Depth+1))return false;
        for(auto* Child:{V.Inner,V.Other})if(Child&&(!NativeField(Child,Depth+1)||Child->GetOwner<FProperty>()!=P||Child->GetOwnerProperty()!=P->GetOwnerProperty()||Child->GetOwnerStruct()!=V.Owner))return false;return true;
    }
    bool NativeCurrent(const TCHAR* Stage)
    {
        if(!NativeBound)return true;
        if(!Loaded(NativeOwner)||Function->GetClass()!=UFunction::StaticClass()||Function->GetOuter()!=NativeOwner||Function->FunctionFlags!=FunctionFlags||Function->ParmsSize!=ParamBytes||Function->NumParms!=NativeRootFields.Num()||Function->Script!=NativeMetadata.Script||Function->GetSuperFunction())return Stop(Stage);
        TArray<FProperty*> Root;for(TFieldIterator<FProperty> It(Function,EFieldIterationFlags::None);It;++It)Root.Add(*It);if(Root!=NativeRootFields)return Stop(Stage);
        // Root/struct inventories precede all captured field dereferences.
        for(const auto& V:NativeTypes)
        {
            if(auto* C=Cast<UClass>(V.Object)){if(C->GetSuperClass()!=V.Super||uint32(C->ClassFlags)!=V.Flags||C->GetStructureSize()!=V.Size||C->GetMinAlignment()!=V.Align||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy.Get()!=nullptr)return Stop(Stage);}
            else if(auto* S=Cast<UScriptStruct>(V.Object)){if(S->GetSuperStruct()!=V.Super||uint32(S->StructFlags)!=V.Flags||S->GetStructureSize()!=V.Size||S->GetMinAlignment()!=V.Align)return Stop(Stage);TArray<FProperty*> Current;for(TFieldIterator<FProperty> It(S,EFieldIterationFlags::None);It;++It)Current.Add(*It);if(Current!=V.Fields)return Stop(Stage);}
            else if(auto* E=Cast<UEnum>(V.Object)){if(E->NumEnums()!=V.Names.Num())return Stop(Stage);for(int32 I=0;I<V.Names.Num();++I)if(E->GetNameByIndex(I)!=V.Names[I]||E->GetValueByIndex(I)!=V.Values[I])return Stop(Stage);}
            else return Stop(Stage);
        }
        for(const auto& V:NativeFields)
        {
            auto* P=V.P;if(!P||P->GetClass()!=V.Class||P->GetFName()!=V.Name||uint64(P->GetPropertyFlags())!=V.Flags||P->ArrayDim!=V.Dim||P->GetSize()!=V.Size||P->GetOffset_ForInternal()!=V.Offset||P->GetOwnerStruct()!=V.Owner||P->GetOwnerProperty()!=V.OwnerProperty)return Mismatch(Stage,RolePath(V.Owner)+TEXT(".")+V.Name.ToString(),TEXT("native_property_descriptor"),TEXT("captured identity/layout/flags"),TEXT("changed"));
            FNativeField Now;if(!NativeParts(P,Now)||Now.Inner!=V.Inner||Now.Other!=V.Other||Now.Type!=V.Type||Now.Meta!=V.Meta)return Mismatch(Stage,RolePath(V.Owner)+TEXT(".")+V.Name.ToString(),TEXT("native_property_type_roles"),TEXT("captured nested/native references"),TEXT("changed"));
        }
        return FInterfaceEventSource::Metadata(NativeMetadata)==NativeMetadata.Metadata||Stop(Stage);
    }
    bool BindNative()
    {
        NativeOwner=Function->GetOuterUClass();if(Function->GetClass()!=UFunction::StaticClass()||Function->GetOuter()!=NativeOwner||Function->GetSuperFunction()||!NativeType(NativeOwner,0))return false;
        NativeMetadata.Hold.Reset(Function);NativeMetadata.Owner=NativeOwner;NativeMetadata.Flags=FunctionFlags;NativeMetadata.Count=Function->NumParms;NativeMetadata.Size=ParamBytes;NativeMetadata.Script=Function->Script;
        for(TFieldIterator<FProperty> It(Function,EFieldIterationFlags::None);It;++It)
        {
            auto* P=*It;if(NativeRootFields.Num()>=126||P->GetOwnerStruct()!=Function||P->GetOwnerProperty()!=P||!P->HasAnyPropertyFlags(CPF_Parm)||P->GetOffset_ForInternal()<0||P->GetOffset_ForInternal()+P->GetSize()>ParamBytes||!NativeField(P,0))return false;
            NativeRootFields.Add(P);NativeMetadata.Fields.Add({P,P->GetClass(),P->GetFName(),uint64(P->GetPropertyFlags()),P->ArrayDim,P->GetSize(),P->GetOffset_ForInternal()});
        }
        NativeMetadata.Metadata=FInterfaceEventSource::Metadata(NativeMetadata);NativeBound=true;
        auto D=R->GetObjectField(TEXT("data"));D->SetBoolField(TEXT("native_compound_event_signature"),true);D->SetStringField(TEXT("native_event_declaring_class"),NativeOwner->GetPathName());D->SetNumberField(TEXT("native_event_parameter_count"),NativeRootFields.Num());D->SetNumberField(TEXT("native_event_held_descriptor_count"),NativeFields.Num());D->SetStringField(TEXT("native_event_scope"),TEXT("Already-loaded Engine event declaration with native Engine/CoreUObject/PhysicsCore scalar, object/class/interface/enum/struct/container input descriptors, including const/reference inputs. Native source identity/layout and current target/pin roles are protected; no source CDO construction, asset/generated types, delegate fields, bitmask enum lookup, runtime execution, explicit compile/save or global callback purity proof."));return NativeCurrent(TEXT("native_source_bound"));
    }
    bool Check(const TCHAR* Stage)
    {
        if(!DefaultEventCurrent(R))return false;if(!Active)return true;if(!Context())return false;
        for(const auto& I:Held){auto* O=I.Object.Get();if(!Loaded(O)||!Loaded(I.Class)||!Loaded(I.Package)||(I.Outer&&!Loaded(I.Outer))||O->GetClass()!=I.Class||O->GetOuter()!=I.Outer||O->GetPackage()!=I.Package||O->GetFlags()!=I.Flags||!O->GetPathName().Equals(I.Path,ESearchCase::CaseSensitive)||I.Package->GetPackageFlags()!=I.PackageFlags||(I.Package!=BP->GetPackage()&&I.Package->IsDirty()!=I.Dirty))return Stop(Stage);}
        if(BP->ParentClass!=Parent||BP->GeneratedClass!=Generated||BP->SkeletonGeneratedClass!=Skeleton||Graph->GraphGuid!=GraphGuid||Graph->Schema.Get()!=SchemaClass||SchemaClass->GetDefaultObject(false)!=SchemaCDO||(!BP->UbergraphPages.Contains(Graph)&&!BP->EventGraphs.Contains(Graph))||Graph->Nodes.Num()!=ExpectedNodes.Num())return Stop(Stage);
        for(int32 I=0;I<ExpectedNodes.Num();++I)if(Graph->Nodes[I]!=ExpectedNodes[I])return Stop(Stage);
        if(InterfaceMode||NativeMode){if(BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bIsRegeneratingOnLoad||BP->bSuppressStructurallyModified||!BP->ParentClass.GetGCPtr().IsResolved()||!BP->GeneratedClass.GetGCPtr().IsResolved()||!BP->SkeletonGeneratedClass.GetGCPtr().IsResolved()||Generated->GetClass()!=UBlueprintGeneratedClass::StaticClass()||Skeleton->GetClass()!=UBlueprintGeneratedClass::StaticClass()||Generated->GetSuperClass()!=Parent||Skeleton->GetSuperClass()!=Parent||!Generated->ClassGeneratedBy.IsResolved()||!Skeleton->ClassGeneratedBy.IsResolved()||Generated->ClassGeneratedBy.Get()!=BP||Skeleton->ClassGeneratedBy.Get()!=BP||!Generated->ClassDefaultObject.IsResolved()||!Skeleton->ClassDefaultObject.IsResolved()||Generated->ClassDefaultObject.Get()!=GeneratedCDO||Skeleton->ClassDefaultObject.Get()!=SkeletonCDO)return Stop(Stage);if(InterfaceMode&&(!InterfaceSource.Check()||Function!=InterfaceSource.Selected))return Stop(Stage);}
        if(NativeMode){if(!NativeCurrent(Stage))return false;}
        else if(!InterfaceMode)
        {
        if(!ExactActorEvent(Function)||Function->FunctionFlags!=FunctionFlags||Function->NumParms!=1||Function->ParmsSize!=ParamBytes||AActor::StaticClass()->GetSuperClass()!=ActorSuper||AActor::StaticClass()->GetClassFlags()!=ActorFlags||AActor::StaticClass()->GetDefaultObject(false)!=ActorCDO||!ActorCDO->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject))return Stop(Stage);
        int32 Index=0;for(TFieldIterator<FProperty> It(Function);It&&It->HasAnyPropertyFlags(CPF_Parm);++It,++Index)
        {
            if(!ParamFields.IsValidIndex(Index)||*It!=ParamFields[Index]||It->GetPropertyFlags()!=ParamFlags[Index]||It->ArrayDim!=1)return Stop(Stage);
            const auto* ObjectParam=CastField<FObjectProperty>(*It);const auto* ByteParam=CastField<FByteProperty>(*It);const auto* EnumParam=CastField<FEnumProperty>(*It);
            if(EventName==TEXT("ReceiveEndPlay")){if(It->GetFName()!=TEXT("EndPlayReason")||!((ByteParam&&ByteParam->Enum==EndEnum)||(EnumParam&&EnumParam->GetEnum()==EndEnum&&CastField<FByteProperty>(EnumParam->GetUnderlyingProperty()))))return Stop(Stage);}
            else if(It->GetFName()!=TEXT("OtherActor")||!ObjectParam||CastField<FClassProperty>(*It)||ObjectParam->PropertyClass!=AActor::StaticClass())return Stop(Stage);
        }if(Index!=ParamFields.Num())return Stop(Stage);
        if(EndEnum){if(EndEnum->NumEnums()!=EnumNames.Num())return Stop(Stage);for(int32 I=0;I<EnumNames.Num();++I)if(!EndEnum->GetNameByIndex(I).ToString().Equals(EnumNames[I],ESearchCase::CaseSensitive)||EndEnum->GetValueByIndex(I)!=EnumValues[I])return Stop(Stage);}
        }
        for(const auto& N:Old)
        {
            if(!Loaded(N.Node.Get())||N.Node->GetOuter()!=Graph||N.Node->NodeGuid!=N.Guid||N.Node->Pins.Num()!=N.Pins.Num())return Stop(Stage);
            for(int32 I=0;I<N.Pins.Num();++I){auto* P=N.Node->Pins[I];if(P!=N.Pins[I]||!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N.Node.Get()||P->PinId!=N.PinIds[I]||P->LinkedTo!=N.Links[I]||((InterfaceMode||NativeMode)&&!P->DefaultObject.IsResolved())||P->DefaultObject!=N.DefaultObjects[I])return Stop(Stage);}
            if(InterfaceMode||NativeMode){for(int32 I=0;I<N.Pins.Num();++I){auto* P=N.Node->Pins[I];if(!FInterfaceEventSource::WeakRole(P->PinType.PinSubCategoryObject)||!FInterfaceEventSource::WeakRole(P->PinType.PinValueType.TerminalSubCategoryObject)||!P->PinType.PinSubCategoryMemberReference.MemberParent.IsResolved()||P->PinType!=N.Types[I]||P->DefaultValue!=N.Values[I]||P->AutogeneratedDefaultValue!=N.Autos[I]||FTextInspector::GetSharedDataId(P->DefaultTextValue)!=FTextInspector::GetSharedDataId(N.Texts[I])||FTextInspector::GetFlags(P->DefaultTextValue)!=FTextInspector::GetFlags(N.Texts[I]))return Stop(Stage);}}
            if((InterfaceMode||NativeMode)&&(N.Call||N.Event)){const auto* Ref=FInterfaceEventSource::NodeReference(N.Node.Get());UObject* ParentObject=nullptr;if(!Ref||!FInterfaceEventSource::ReferenceParent(*Ref,ParentObject)||ParentObject!=N.ReferenceParent)return Stop(Stage);}
            if(N.Call){const auto* C=Cast<UK2Node_CallFunction>(N.Node.Get());if(!C||!C->FunctionReference.IsSameReference(N.Reference)||C->FunctionReference.GetMemberScopeName()!=N.Scope||C->FunctionReference.IsDeprecated()!=N.Deprecated)return Stop(Stage);}
            if(N.Event){const auto* E=Cast<UK2Node_Event>(N.Node.Get());if(!E||!E->EventReference.IsSameReference(N.Reference)||!E->EventReference.GetMemberScopeName().Equals(N.Scope,ESearchCase::CaseSensitive)||E->EventReference.IsDeprecated()!=N.Deprecated)return Stop(Stage);}
        }
        if(Added.IsValid())
        {
            if(!Loaded(Added.Get()))return InterfaceMode?Mismatch(Stage,AddedPath,TEXT("object_live"),TEXT("true"),TEXT("false")):Stop(Stage);
            if(InterfaceMode){if(Added->GetClass()!=UK2Node_Event::StaticClass())return Mismatch(Stage,AddedPath,TEXT("class"),RolePath(UK2Node_Event::StaticClass()),RolePath(Added->GetClass()));if(Added->GetOuter()!=Graph)return Mismatch(Stage,AddedPath,TEXT("outer"),RolePath(Graph),RolePath(Added->GetOuter()));if(Added->GetPackage()!=BP->GetPackage())return Mismatch(Stage,AddedPath,TEXT("package"),RolePath(BP->GetPackage()),RolePath(Added->GetPackage()));if(Added->GetFlags()!=AddedFlags)return Mismatch(Stage,AddedPath,TEXT("object_flags"),LexToString(uint32(AddedFlags)),LexToString(uint32(Added->GetFlags())));if(!Added->GetPathName().Equals(AddedPath,ESearchCase::CaseSensitive))return Mismatch(Stage,AddedPath,TEXT("path"),AddedPath,Added->GetPathName());}
            else if(Added->GetClass()!=UK2Node_Event::StaticClass()||Added->GetOuter()!=Graph||Added->GetPackage()!=BP->GetPackage()||Added->GetFlags()!=AddedFlags||!Added->GetPathName().Equals(AddedPath,ESearchCase::CaseSensitive))return Stop(Stage);
            if(Configured&&InterfaceMode&&!InterfaceReferenceCurrent(Stage))return false;
            if(Configured&&(Added->NodePosX!=X||Added->NodePosY!=Y||!Added->bOverrideFunction||Added->EventReference.GetMemberName()!=EventName||(InterfaceMode?false:(Added->EventReference.GetMemberParentClass()!=(NativeMode?NativeOwner:AActor::StaticClass())||Added->EventReference.GetMemberGuid().IsValid()))||!Added->EventReference.GetMemberScopeName().IsEmpty()||Added->EventReference.IsSelfContext()||Added->EventReference.IsLocalScope()||Added->EventReference.IsDeprecated()))return Stop(Stage);
            if((GuidAdopted&&Added->NodeGuid!=AddedGuid)||(!GuidAdopted&&Added->NodeGuid.IsValid())||Added->Pins.Num()>128)return Stop(Stage);
            for(const auto* P:Added->Pins)if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=Added.Get()||!P->LinkedTo.IsEmpty())return Stop(Stage);
            if(PinsFrozen)
            {
                if(Added->Pins.Num()!=NewPins.Num())return Stop(Stage);
                for(int32 I=0;I<NewPins.Num();++I){const auto& E=NewPins[I];const auto* P=Added->Pins[I];if(P!=E.Pin||P->PinId!=E.Id||P->PinName!=E.Name||P->Direction!=E.Direction||P->PinType!=E.Type||P->PinType.bIsUObjectWrapper!=E.Type.bIsUObjectWrapper||P->PinType.bSerializeAsSinglePrecisionFloat!=E.Type.bSerializeAsSinglePrecisionFloat||P->bOrphanedPin||P->bHidden!=E.Hidden||P->DefaultObject||!P->DefaultValue.Equals(E.DefaultValue,ESearchCase::CaseSensitive)||!P->AutogeneratedDefaultValue.Equals(E.AutogeneratedDefaultValue,ESearchCase::CaseSensitive)||((InterfaceMode||NativeMode)?(FTextInspector::GetSharedDataId(P->DefaultTextValue)!=FTextInspector::GetSharedDataId(E.TextValue)||FTextInspector::GetFlags(P->DefaultTextValue)!=FTextInspector::GetFlags(E.TextValue)):!P->DefaultTextValue.ToString().Equals(E.DefaultText,ESearchCase::CaseSensitive)))return Stop(Stage);}
            }
        }return true;
    }
    bool Bind(UBlueprint* B,UEdGraph* G,UFunction* F,int32 InX,int32 InY,bool FromInterface=false)
    {
        InterfaceMode=FromInterface;NativeMode=!FromInterface&&NativeCompoundEvent(F);Active=ExactActorEvent(F)||InterfaceMode||NativeMode;if(!Active)return true;Editor=GEditor;World=GEditor?GEditor->GetEditorWorldContext().World():nullptr;if(!Context())return false;
        BP=B;Graph=G;Function=F;Parent=B->ParentClass;Generated=B->GeneratedClass;Skeleton=B->SkeletonGeneratedClass;SchemaClass=G->Schema;SchemaCDO=Loaded(SchemaClass)?SchemaClass->GetDefaultObject(false):nullptr;ActorCDO=NativeMode?nullptr:AActor::StaticClass()->GetDefaultObject(false);ActorSuper=AActor::StaticClass()->GetSuperClass();ActorFlags=AActor::StaticClass()->GetClassFlags();FunctionFlags=F->FunctionFlags;ParamBytes=F->ParmsSize;GraphGuid=G->GraphGuid;EventName=F->GetFName();X=InX;Y=InY;
        if(InterfaceMode||NativeMode){if(!B->ParentClass.GetGCPtr().IsResolved()||!B->GeneratedClass.GetGCPtr().IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||!Loaded(Generated)||!Loaded(Skeleton)||!Generated->ClassDefaultObject.IsResolved()||!Skeleton->ClassDefaultObject.IsResolved())return Stop(TEXT("target_class_CDO_bind"));GeneratedCDO=Generated->ClassDefaultObject.Get();SkeletonCDO=Skeleton->ClassDefaultObject.Get();if((GeneratedCDO&&!PinObject(GeneratedCDO))||(SkeletonCDO&&!PinObject(SkeletonCDO)))return false;}
        if((!NativeMode&&!Loaded(ActorCDO))||!Loaded(SchemaCDO)||!PinObject(B)||!PinObject(G)||!PinObject(Parent)||!PinObject(Generated)||!PinObject(Skeleton)||!PinObject(SchemaClass)||!PinObject(SchemaCDO)||!PinObject(F)||!PinObject(AActor::StaticClass())||(!NativeMode&&!PinObject(ActorCDO))||!PinObject(UK2Node_Event::StaticClass()))return Stop(TEXT("source_bind"));
        for(TFieldIterator<FProperty> It(F);It&&It->HasAnyPropertyFlags(CPF_Parm);++It){ParamFields.Add(*It);ParamFlags.Add(It->GetPropertyFlags());}
        if(!NativeMode&&EventName==TEXT("ReceiveEndPlay")){EndEnum=StaticEnum<EEndPlayReason::Type>();if(!Loaded(EndEnum)||EndEnum->GetPathName()!=TEXT("/Script/Engine.EEndPlayReason")||EndEnum->NumEnums()<1||EndEnum->NumEnums()>1024||!PinObject(EndEnum))return Stop(TEXT("enum_bind"));for(int32 I=0;I<EndEnum->NumEnums();++I){EnumNames.Add(EndEnum->GetNameByIndex(I).ToString());EnumValues.Add(EndEnum->GetValueByIndex(I));}}
        for(UEdGraphNode* N:G->Nodes){if(!PinObject(N))return false;FOld O;O.Node.Reset(N);O.Guid=N->NodeGuid;for(auto* P:N->Pins){if(!P||P->WasTrashed())return Stop(TEXT("initial_pins"));O.Pins.Add(P);O.PinIds.Add(P->PinId);O.Links.Add(P->LinkedTo);if((InterfaceMode||NativeMode)&&(!P->DefaultObject.IsResolved()||!FInterfaceEventSource::WeakRole(P->PinType.PinSubCategoryObject)||!FInterfaceEventSource::WeakRole(P->PinType.PinValueType.TerminalSubCategoryObject)||!P->PinType.PinSubCategoryMemberReference.MemberParent.IsResolved()))return Stop(TEXT("initial_pin_roles"));O.DefaultObjects.Add(P->DefaultObject.Get());if(P->DefaultObject&&!PinObject(P->DefaultObject))return false;if(InterfaceMode||NativeMode){if(!P->DefaultObject.IsResolved()||!FInterfaceEventSource::WeakRole(P->PinType.PinSubCategoryObject)||!FInterfaceEventSource::WeakRole(P->PinType.PinValueType.TerminalSubCategoryObject)||!P->PinType.PinSubCategoryMemberReference.MemberParent.IsResolved())return Stop(TEXT("initial_pin_type"));for(auto* TypeObject:{P->PinType.PinSubCategoryObject.Get(),P->PinType.PinValueType.TerminalSubCategoryObject.Get(),P->PinType.PinSubCategoryMemberReference.MemberParent.Get()})if(TypeObject&&!PinObject(TypeObject))return false;O.Types.Add(P->PinType);O.Values.Add(P->DefaultValue);O.Autos.Add(P->AutogeneratedDefaultValue);O.Texts.Add(P->DefaultTextValue);}}if(const auto* E=Cast<UK2Node_Event>(N)){O.Event=true;O.Reference=E->EventReference;O.Scope=E->EventReference.GetMemberScopeName();O.Deprecated=E->EventReference.IsDeprecated();}else if(InterfaceMode||NativeMode){if(const auto* C=Cast<UK2Node_CallFunction>(N)){O.Call=true;O.Reference=C->FunctionReference;O.Scope=C->FunctionReference.GetMemberScopeName();O.Deprecated=C->FunctionReference.IsDeprecated();}}if((InterfaceMode||NativeMode)&&(O.Event||O.Call)){const auto* Ref=FInterfaceEventSource::NodeReference(N);if(!Ref||!FInterfaceEventSource::ReferenceParent(*Ref,O.ReferenceParent)||(O.ReferenceParent&&!PinObject(O.ReferenceParent)))return Stop(TEXT("initial_node_reference"));}Old.Add(MoveTemp(O));ExpectedNodes.Add(N);}
        if(NativeMode&&!BindNative()){if(!Stopped)Fail(R,TEXT("native_event_signature_scope_pending"),TEXT("Loaded finite native declaration/type-descriptor proof unavailable; broader signatures remain incomplete, no node writer dispatched."));return false;}
        if(InterfaceMode&&!InterfaceSource.Bind(B,G,F)){if(InterfaceSource.Stopped)return false;Fail(R,TEXT("interface_event_source_pending"),TEXT("Loaded saved-clean direct Game BPI, paired primitive declarations/signature graphs and a directly implemented event-eligible method required; wider valid original sources remain incomplete."));return false;}return Check(TEXT("bound"));
    }
    bool FreezePins(const TMap<FName,FEdGraphPinType>& Params)
    {
        if(!Check(TEXT("before_pin_freeze"))||!PinsMatch(Added.Get(),Params,false,true))return Stop(TEXT("standard_pins"));
        const auto StablePins=Added->Pins;for(auto* P:StablePins)
        {
            FEdGraphPinType Expected;
            if(P->PinName==UEdGraphSchema_K2::PN_Then)Expected.PinCategory=UEdGraphSchema_K2::PC_Exec;
            else if(P->PinName==TEXT("OutputDelegate")){if(InterfaceMode)Expected=InterfaceSource.PlannedDelegate;else{Expected.PinCategory=UEdGraphSchema_K2::PC_Delegate;Expected.PinSubCategoryMemberReference.MemberParent=NativeMode?NativeOwner:AActor::StaticClass();Expected.PinSubCategoryMemberReference.MemberName=EventName;}}
            else{const auto* Type=Params.Find(P->PinName);if(!Type)return Stop(TEXT("standard_pins"));Expected=*Type;}
            if(P->Direction!=EGPD_Output||P->PinType!=Expected||P->PinType.bIsUObjectWrapper!=Expected.bIsUObjectWrapper||P->PinType.bSerializeAsSinglePrecisionFloat!=Expected.bSerializeAsSinglePrecisionFloat||P->DefaultObject)return Stop(TEXT("standard_pins"));
            FNewPin E;E.Pin=P;E.Id=P->PinId;E.Name=P->PinName;E.Direction=P->Direction;E.Type=P->PinType;E.DefaultValue=P->DefaultValue;E.AutogeneratedDefaultValue=P->AutogeneratedDefaultValue;E.TextValue=P->DefaultTextValue;E.DefaultText=P->DefaultTextValue.ToString();if((InterfaceMode||NativeMode)&&(!Check(TEXT("pin_text_return"))||Added->Pins!=StablePins))return Stop(TEXT("pin_text_membership"));E.Hidden=P->bHidden;NewPins.Add(MoveTemp(E));
        }
        PinsFrozen=true;return Check(TEXT("pins_frozen"));
    }
    bool Adopt(UK2Node_Event* N)
    {
        if(!Context()||!Loaded(N)||N->GetClass()!=UK2Node_Event::StaticClass()||N->GetOuter()!=Graph||N->GetPackage()!=BP->GetPackage())return Stop(TEXT("allocation_return"));
        Added.Reset(N);AddedPath=N->GetPathName();AddedFlags=N->GetFlags();return Check(TEXT("allocation_return"));
    }
};
bool InterfaceObservationFence(){return (!ActiveDefaultEventFence||(*ActiveDefaultEventFence)())&&(!ActiveDefaultEventNode||ActiveDefaultEventNode->Current())&&(!ActiveInterfaceEvent||ActiveInterfaceEvent->Check(TEXT("observation_return")));}

bool Signature(UFunction* Function, TMap<FName, FEdGraphPinType>& Params, const TSharedRef<FJsonObject>& R, bool bAllowActor = false, bool bAllowNative = false)
{
    if (!IsValid(Function) || !UEdGraphSchema_K2::CanKismetOverrideFunction(Function) || !UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(Function)
        || Function->HasAnyFunctionFlags(FUNC_EditorOnly | FUNC_Static | FUNC_Final | FUNC_Delegate | FUNC_Net)
        || Function->HasMetaData(TEXT("Latent")) || UEdGraphSchema_K2::HasWildcardParams(Function))
    { return Fail(R, TEXT("event_signature_unsupported"), TEXT("Requires an overridable runtime event without static/final/network/delegate/latent/wildcard behavior.")); }
    const bool bActorExact=bAllowActor&&ExactActorEvent(Function);
    for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
    {
        FEdGraphPinType Type;
        const bool bComponentEndPlay = Function->GetOuterUClass() == UActorComponent::StaticClass() && Function->GetFName() == TEXT("ReceiveEndPlay");
        const bool bConverted = GetDefault<UEdGraphSchema_K2>()->ConvertPropertyToPinType(*It, Type);
        const UEnum* Enum = bConverted ? Cast<UEnum>(Type.PinSubCategoryObject.Get()) : nullptr;
        const bool bEndPlayReason = bComponentEndPlay && It->GetFName() == TEXT("EndPlayReason")
            && Type.ContainerType == EPinContainerType::None && !Type.bIsReference && !Type.bIsConst && Type.PinSubCategory.IsNone()
            && (Type.PinCategory == UEdGraphSchema_K2::PC_Byte || Type.PinCategory == UEdGraphSchema_K2::PC_Enum)
            && Enum && Enum->GetPathName() == TEXT("/Script/Engine.EEndPlayReason");
        FEdGraphPinType ActorExpected;ActorExpected.PinCategory=Function->GetFName()==TEXT("ReceiveEndPlay")?UEdGraphSchema_K2::PC_Byte:UEdGraphSchema_K2::PC_Object;
        if(bActorExact)ActorExpected.PinSubCategoryObject=Function->GetFName()==TEXT("ReceiveEndPlay")?static_cast<UObject*>(StaticEnum<EEndPlayReason::Type>()):static_cast<UObject*>(AActor::StaticClass());
        const auto* ActorObject=CastField<FObjectProperty>(*It);const auto* ActorByte=CastField<FByteProperty>(*It);const auto* ActorEnum=CastField<FEnumProperty>(*It);
        const bool bActorOverlap=bActorExact&&Function->GetFName()!=TEXT("ReceiveEndPlay")&&It->GetFName()==TEXT("OtherActor")&&ActorObject&&!CastField<FClassProperty>(*It)&&ActorObject->PropertyClass==AActor::StaticClass();
        const bool bActorEnd=bActorExact&&Function->GetFName()==TEXT("ReceiveEndPlay")&&It->GetFName()==TEXT("EndPlayReason")&&((ActorByte&&ActorByte->Enum==StaticEnum<EEndPlayReason::Type>())||(ActorEnum&&ActorEnum->GetEnum()==StaticEnum<EEndPlayReason::Type>()&&CastField<FByteProperty>(ActorEnum->GetUnderlyingProperty())));
        const bool bActorParam=(bActorOverlap||bActorEnd)&&!It->HasAnyPropertyFlags(CPF_ConstParm)&&Type==ActorExpected;
        if (Params.Num() >= 128 || (bAllowNative?(It->HasAnyPropertyFlags(CPF_ReturnParm)||(It->HasAnyPropertyFlags(CPF_OutParm)&&!It->HasAnyPropertyFlags(CPF_ReferenceParm))):It->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm | CPF_ReferenceParm))
            || !bConverted || (bActorExact&&!bActorParam) || (!bAllowNative&&!Pin::IsPrimitive(Type) && !bEndPlayReason&&!bActorParam))
        { return Fail(R, TEXT("event_signature_pending"), bAllowNative?TEXT("Native compound events require a successfully converted bounded input signature: no return values or output-only parameters. Reference/const inputs follow the installed Engine entry-pin rule."):TEXT("Supports primitive inputs, existing ActorComponent.ReceiveEndPlay, exact Actor Begin/EndOverlap/EndPlay, and separately guarded native compound input signatures; wider generated/custom/delegate signatures remain incomplete.")); }
        Params.Add(It->GetFName(), Type);
    }
    if(bActorExact&&Params.Num()!=1)return Fail(R,TEXT("event_signature_pending"),TEXT("Exact Actor special event requires its single declared input."));
    return true;
}
bool PinsMatch(const UEdGraphNode* Node, const TMap<FName, FEdGraphPinType>& Params, bool bParent, bool bNew)
{
    if (!Node || Node->Pins.Num() != Params.Num() + (bParent ? 3 : 2)) { return false; }
    const UEdGraphPin* Then = Node->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
    if (!Then || Then->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) { return false; }
    if (bParent)
    {
        const UEdGraphPin* Execute = Node->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
        const UEdGraphPin* Self = Node->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input);
        if (!Execute || Execute->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec || !Self || !Self->bHidden || Self->DefaultObject || !Self->LinkedTo.IsEmpty()) { return false; }
    }
    else
    {
        const UEdGraphPin* Delegate = Node->FindPin(TEXT("OutputDelegate"), EGPD_Output);
        if (!Delegate || Delegate->PinType.PinCategory != UEdGraphSchema_K2::PC_Delegate) { return false; }
    }
    for (const auto& Pair : Params)
    {
        const UEdGraphPin* P = Node->FindPin(Pair.Key, bParent ? EGPD_Input : EGPD_Output);
        if (!P || P->PinType != Pair.Value) { return false; }
    }
    TSet<FGuid> Ids;
    for (const UEdGraphPin* P : Node->Pins)
    {
        if (!P || P->bOrphanedPin || !P->PinId.IsValid() || Ids.Contains(P->PinId) || (bNew && !P->LinkedTo.IsEmpty())) { return false; }
        Ids.Add(P->PinId);
    }
    return true;
}
void Finish(UBlueprint* BP, UEdGraph* G, UEdGraphNode* Node, const TSharedRef<FJsonObject>& Before,
    const TSharedRef<FJsonObject>& Parent, bool bCreated, bool Verified, const TSharedRef<FJsonObject>& R,
    bool bStructural = false, bool bUpdatedCallInEditor = false)
{
    const auto Data = R->GetObjectField(TEXT("data")); auto After = MakeShared<FJsonObject>();
    const bool Known = Capture(BP, G, After);if(!DefaultEventCurrent(R))return; Data->SetObjectField(TEXT("after"), After);
    R->SetBoolField(TEXT("change_state_known"), Known); if (Known) { R->SetBoolField(TEXT("changed"), Text(Before) != Text(After)); }
    bool Preserved = Known && G->Nodes.Num() == Before->GetArrayField(TEXT("nodes")).Num() + (bCreated ? 1 : 0);
    if (Known)
    {
        for (const auto& Value : Before->GetArrayField(TEXT("nodes")))
        {
            const auto Old = Value->AsObject(); const UEdGraphNode* OldNode = FindObject<UEdGraphNode>(nullptr, *Old->GetStringField(TEXT("object_path")));
            auto Expected = MakeShared<FJsonObject>(); for (const auto& Pair : Old->Values) { Expected->SetField(FString(Pair.Key.ToView()), Pair.Value); }
            if (bUpdatedCallInEditor && OldNode == Node)
            { Expected->SetBoolField(TEXT("call_in_editor"), CastChecked<UK2Node_CustomEvent>(Node)->bCallInEditor); }
            Preserved &= IsValid(OldNode) && G->Nodes.Contains(OldNode) && Text(NodeState(OldNode)) == Text(Expected);if(!DefaultEventCurrent(R))return;
        }
    }
    const bool ParentPreserved = ParentStateUnchanged(Parent);if(!DefaultEventCurrent(R))return; Data->SetBoolField(TEXT("parent_observed_unchanged"), ParentPreserved);
    if (!ParentPreserved) { R->SetBoolField(TEXT("change_state_known"), false); R->SetField(TEXT("changed"), MakeShared<FJsonValueNull>()); }
    Data->SetBoolField(TEXT("existing_nodes_preserved"), Preserved); Data->SetBoolField(TEXT("created"), bCreated); Data->SetBoolField(TEXT("reused"), !bCreated);
    bool IdentityReturned = false;
    if (Known && IsValid(Node))
    {
        // Retain the owner: ranging over TemporarySharedRef->Values leaves a
        // dangling map once the temporary smart pointer is destroyed.
        const auto State = NodeState(Node);if(!DefaultEventCurrent(R))return;
        for (const auto& Pair : State->Values) { Data->SetField(FString(Pair.Key.ToView()), Pair.Value); }
        FString ObjectPath, NodeGuid, NodeId, NodeClass;
        IdentityReturned = Data->TryGetStringField(TEXT("object_path"), ObjectPath) && ObjectPath == Node->GetPathName()
            && Data->TryGetStringField(TEXT("node_guid"), NodeGuid) && NodeGuid == Node->NodeGuid.ToString(EGuidFormats::Digits)
            && Data->TryGetStringField(TEXT("node_id"), NodeId) && NodeId == NodeGuid
            && Data->TryGetStringField(TEXT("node_class"), NodeClass) && NodeClass == Node->GetClass()->GetPathName();
    }
    Data->SetBoolField(TEXT("returned_identity_verified"), IdentityReturned);
    auto GraphInfo = MakeShared<FJsonObject>(); GraphInfo->SetStringField(TEXT("object_path"), G->GetPathName()); GraphInfo->SetStringField(TEXT("graph_name"), G->GetName());
    GraphInfo->SetStringField(TEXT("graph_id"), G->GraphGuid.ToString(EGuidFormats::Digits)); GraphInfo->SetStringField(TEXT("graph_type"), TEXT("event"));
    GraphInfo->SetNumberField(TEXT("node_count"), G->Nodes.Num()); Data->SetObjectField(TEXT("graph"), GraphInfo);
    Data->SetBoolField(TEXT("implicit_skeleton_compile_requested"), bStructural);
    Data->SetBoolField(TEXT("explicit_full_compile_performed"), false);
    Data->SetBoolField(TEXT("updated_call_in_editor"), bUpdatedCallInEditor);
    Data->SetStringField(TEXT("observation_scope"), TEXT("existing target node/pin/default/link snapshots, dirty/status and parent generated class dirty/status; arbitrary callbacks and parent graph/CDO contents require independent pipeline observation"));
    const bool Passed = Verified && Known && Preserved && ParentPreserved && IdentityReturned;
    R->SetBoolField(TEXT("postcondition_passed"), Passed); R->SetBoolField(TEXT("success"), Passed); R->SetStringField(TEXT("stage"), Passed ? TEXT("complete") : TEXT("verification"));
    if (!Passed) { Fail(R, TEXT("event_postcondition_failed"), TEXT("Event/parent-call identity, typed pins or preservation check failed; retain partial changes and inspect before retry.")); }
}
}

TSharedRef<FJsonObject> InspectSavedActorParent(const FString& ClassPath)
{
    if (!Pin::SafeName(ClassPath) || !ClassPath.StartsWith(TEXT("/Game/")))
    { auto R = Result(); Fail(R, TEXT("actor_parent_path_invalid"), TEXT("Use an exact already loaded /Game generated Actor class path.")); return R; }
    UClass* Class = FindObject<UClass>(nullptr, *ClassPath);
    if (!Class || Class->GetPathName() != ClassPath)
    { auto R = Result(); Fail(R, TEXT("actor_parent_not_loaded"), TEXT("Exact class is not loaded; this inspector never loads assets.")); return R; }
    return GeneratedComponent::InspectSavedActorParent(Class);
}

TSharedRef<FJsonObject> InspectSavedObjectParent(const FString& ClassPath)
{
    if (!Pin::SafeName(ClassPath) || !ClassPath.StartsWith(TEXT("/Game/")))
    { auto R = Result(); Fail(R, TEXT("object_parent_path_invalid"), TEXT("Use an exact already loaded /Game pure UObject-root generated class path.")); return R; }
    UClass* Class = FindObject<UClass>(nullptr, *ClassPath);
    if (!Class || Class->GetPathName() != ClassPath)
    { auto R = Result(); Fail(R, TEXT("object_parent_not_loaded"), TEXT("Exact class is not loaded; this inspector never loads assets.")); return R; }
    return GeneratedComponent::InspectSavedObjectParent(Class);
}

TSharedRef<FJsonObject> AddCustom(const FString& GraphPath, const FString& Name, int32 X, int32 Y, bool bCallInEditor, bool bHasCallInEditor)
{
    auto R = Result();if(ActiveDefaultEventNode&&ActiveDefaultEventNode->Active){Fail(R,TEXT("event_compound_child_reentry"),TEXT("Compound event child already active; no nested writer entered."));return R;}FDefaultEventNodeScope DefaultNode(R);TGuardValue<FDefaultEventNodeScope*> NodeFence(ActiveDefaultEventNode,DefaultNode.Active?&DefaultNode:ActiveDefaultEventNode);
    if (!Pin::Identifier(Name)) { Fail(R, TEXT("custom_event_name_invalid"), TEXT("custom_event_name requires an exact ASCII identifier of 1..100 characters.")); return R; }
    UBlueprint* BP = nullptr; UEdGraph* G = nullptr; auto Before = MakeShared<FJsonObject>(); auto Parent = MakeShared<FJsonObject>();
    if (!Prepare(GraphPath, X, Y, BP, G, Before, Parent, R, true)) { return R; }
    if(!DefaultNode.Bind(G))return R;
    const TStrongObjectPtr<UBlueprint> KeepBP(BP); const TStrongObjectPtr<UEdGraph> KeepGraph(G);
    TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
    if (Graphs.Num() > 256) { Fail(R, TEXT("custom_event_graph_budget_exceeded"), TEXT("Custom event uniqueness inspection is limited to 256 graphs.")); return R; }
    UK2Node_CustomEvent* Existing = nullptr;
    for (UEdGraph* Candidate : Graphs)
    {
        if (!IsValid(Candidate) || Candidate->Nodes.Num() > 2048)
        { Fail(R, TEXT("custom_event_graph_budget_exceeded"), TEXT("Custom event uniqueness inspection found invalid/excessive graph state.")); return R; }
        for (UEdGraphNode* N : Candidate->Nodes)
        {
            UK2Node_CustomEvent* E = Cast<UK2Node_CustomEvent>(N);
            if (E && E->CustomFunctionName == FName(*Name))
            {
                if (Existing) { Fail(R, TEXT("custom_event_ambiguous"), TEXT("Multiple custom events use this name; no node was selected or repaired.")); return R; }
                Existing = E;
            }
        }
    }
    const TMap<FName, FEdGraphPinType> NoParams;
    auto Matches = [&](const UK2Node_CustomEvent* Node, bool bNew)
    {
        return Node && Node->GetClass() == UK2Node_CustomEvent::StaticClass() && Node->CustomFunctionName.ToString() == Name
            && Node->GetGraph() == G && Node->NodeGuid.IsValid() && !Node->bOverrideFunction && !Node->bIsDeprecated
            && Node->UserDefinedPins.IsEmpty() && Node->FunctionFlags == (FUNC_BlueprintCallable | FUNC_BlueprintEvent | FUNC_Public)
            && PinsMatch(Node, NoParams, false, bNew);
    };
    const auto Data = R->GetObjectField(TEXT("data")); Data->SetStringField(TEXT("custom_event_scope"), TEXT("own non-RPC no-parameter custom event; delegate/signature sources remain pending; compound default graph creation is reported separately"));
    Data->SetStringField(TEXT("signature_source_dispatcher_name"), TEXT("")); Data->SetStringField(TEXT("signature_source_class"), TEXT(""));
    Data->SetStringField(TEXT("signature_source_delegate_name"), TEXT("")); Data->SetStringField(TEXT("signature_function"), TEXT(""));
    if (Existing)
    {
        if(!DefaultNode.Adopt(Existing,true)||!DefaultNode.FreezePins())return R;
        if (!Matches(Existing, false)) { Fail(R, TEXT("custom_event_existing_unsupported"), TEXT("Existing event is outside the selected graph or has different identity/signature/flags; it was not modified.")); return R; }
        const bool bUpdate = bHasCallInEditor && Existing->bCallInEditor != bCallInEditor;
        if (bUpdate)
        {
            R->SetStringField(TEXT("stage"), TEXT("update_call_in_editor")); R->SetBoolField(TEXT("mutation_attempted"), true);
            R->SetBoolField(TEXT("change_state_known"), false); R->SetField(TEXT("changed"), MakeShared<FJsonValueNull>());
            BP->Modify();if(!DefaultEventCurrent(R))return R; G->Modify();if(!DefaultEventCurrent(R))return R; Existing->Modify();if(!DefaultEventCurrent(R))return R; Existing->bCallInEditor = bCallInEditor;
            // This is function metadata, not a pin edit. Preserve existing pin GUIDs/links.
            DefaultNode.PinsFrozen=false;FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);if(!DefaultEventCurrent(R)||!DefaultNode.FreezePins())return R;
        }
        Data->SetBoolField(TEXT("reused_existing"), true);
        Finish(BP, G, Existing, Before, Parent, false, Matches(Existing, false) && (!bHasCallInEditor || Existing->bCallInEditor == bCallInEditor), R, bUpdate, bUpdate);
        return R;
    }
    if (FKismetNameValidator(BP).IsValid(Name) != EValidatorResult::Ok)
    { Fail(R, TEXT("custom_event_name_collision"), TEXT("Name collides with a member/function or is invalid; automatic renaming is disabled.")); return R; }
    if (G->Nodes.Num() >= 2048) { Fail(R, TEXT("custom_event_graph_budget_exceeded"), TEXT("No room for a custom event below the 2048-node bound.")); return R; }
    R->SetStringField(TEXT("stage"), TEXT("add_custom_event")); R->SetBoolField(TEXT("mutation_attempted"), true);
    R->SetBoolField(TEXT("change_state_known"), false); R->SetField(TEXT("changed"), MakeShared<FJsonValueNull>());
    UK2Node_CustomEvent* Node=nullptr;
    if(DefaultNode.Active)
    {
        BP->Modify();if(!DefaultEventCurrent(R))return R;G->Modify();if(!DefaultEventCurrent(R))return R;
        Node=NewObject<UK2Node_CustomEvent>(G,NAME_None,RF_Transactional|(G->HasAnyFlags(RF_Transient)?RF_Transient:RF_NoFlags));
        if(!DefaultNode.Adopt(Node))return R;
        Node->CustomFunctionName=FName(*Name);Node->bCallInEditor=bHasCallInEditor&&bCallInEditor;Node->NodePosX=X;Node->NodePosY=Y;
        if(!DefaultNode.Attaching())return R;G->AddNode(Node,false,true);if(!DefaultEventCurrent(R))return R;
        Node->CreateNewGuid();if(!DefaultEventCurrent(R))return R;Node->PostPlacedNewNode();if(!DefaultEventCurrent(R))return R;
        if(Node->Pins.IsEmpty()){Node->AllocateDefaultPins();if(!DefaultEventCurrent(R))return R;}if(!DefaultNode.FreezePins())return R;
    }
    else
    {
        BP->Modify();G->Modify();FGraphNodeCreator<UK2Node_CustomEvent> Creator(*G);Node=Creator.CreateNode(false);
        Node->CustomFunctionName=FName(*Name);Node->bCallInEditor=bHasCallInEditor&&bCallInEditor;Node->NodePosX=X;Node->NodePosY=Y;Creator.Finalize();
    }
    DefaultNode.PinsFrozen=false;FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);if(!DefaultEventCurrent(R)||!DefaultNode.FreezePins())return R;
    Data->SetBoolField(TEXT("reused_existing"), false);
    Finish(BP, G, Node, Before, Parent, true, G->Nodes.Contains(Node) && Node->NodePosX == X && Node->NodePosY == Y
        && Matches(Node, true) && Node->bCallInEditor == (bHasCallInEditor && bCallInEditor), R, true);
    return R;
}

TSharedRef<FJsonObject> AddInputKey(const FString& GraphPath, const FString& KeyName, int32 X, int32 Y,
    bool bControl, bool bAlt, bool bShift, bool bCommand, bool bConsumeInput, bool bExecuteWhenPaused, bool bOverrideParentBinding)
{
    auto R = Result();if(ActiveDefaultEventNode&&ActiveDefaultEventNode->Active){Fail(R,TEXT("event_compound_child_reentry"),TEXT("Compound event child already active; no nested writer entered."));return R;}FDefaultEventNodeScope DefaultNode(R);TGuardValue<FDefaultEventNodeScope*> NodeFence(ActiveDefaultEventNode,DefaultNode.Active?&DefaultNode:ActiveDefaultEventNode); const FString Trimmed = KeyName.TrimStartAndEnd();
    if (!Pin::SafeName(Trimmed) || Trimmed.IsEmpty()) { Fail(R, TEXT("input_key_name_invalid"), TEXT("key requires a bounded nonempty digital Blueprint key name.")); return R; }
    const FKey Key{FName(*Trimmed)};
    if (!Key.IsValid() || Key.IsAnalog() || Key.IsDeprecated() || !Key.IsBindableInBlueprints())
    { Fail(R, TEXT("input_key_unsupported"), TEXT("Unknown, analog, deprecated or non-Blueprint-bindable key; no node was created.")); return R; }
    UBlueprint* BP = nullptr; UEdGraph* G = nullptr; auto Before = MakeShared<FJsonObject>(); auto Parent = MakeShared<FJsonObject>();
    if (!Prepare(GraphPath, X, Y, BP, G, Before, Parent, R)) { return R; }
    if(!DefaultNode.Bind(G))return R;
    if (!GetDefault<UK2Node_InputKey>()->IsCompatibleWithGraph(G))
    { Fail(R, TEXT("input_key_graph_unsupported"), TEXT("Requires an input-capable standard Actor event graph outside construction scripts.")); return R; }
    if(!DefaultEventCurrent(R))return R;
    if (G->Nodes.Num() >= 2048) { Fail(R, TEXT("input_key_graph_budget_exceeded"), TEXT("No room for an input node below the 2048-node bound.")); return R; }
    const TStrongObjectPtr<UBlueprint> KeepBP(BP); const TStrongObjectPtr<UEdGraph> KeepGraph(G);
    R->SetStringField(TEXT("stage"), TEXT("add_input_key")); R->SetBoolField(TEXT("mutation_attempted"), true);
    R->SetBoolField(TEXT("change_state_known"), false); R->SetField(TEXT("changed"), MakeShared<FJsonValueNull>());
    UK2Node_InputKey* Node=nullptr;
    auto Configure=[&](){Node->InputKey=Key;Node->bControl=bControl;Node->bAlt=bAlt;Node->bShift=bShift;Node->bCommand=bCommand;
        Node->bConsumeInput=bConsumeInput;Node->bExecuteWhenPaused=bExecuteWhenPaused;Node->bOverrideParentBinding=bOverrideParentBinding;Node->NodePosX=X;Node->NodePosY=Y;};
    if(DefaultNode.Active)
    {
        BP->Modify();if(!DefaultEventCurrent(R))return R;G->Modify();if(!DefaultEventCurrent(R))return R;
        Node=NewObject<UK2Node_InputKey>(G,NAME_None,RF_Transactional|(G->HasAnyFlags(RF_Transient)?RF_Transient:RF_NoFlags));
        if(!DefaultNode.Adopt(Node))return R;Configure();
        if(!DefaultNode.Attaching())return R;G->AddNode(Node,false,true);if(!DefaultEventCurrent(R))return R;
        Node->CreateNewGuid();if(!DefaultEventCurrent(R))return R;Node->PostPlacedNewNode();if(!DefaultEventCurrent(R))return R;
        if(Node->Pins.IsEmpty()){Node->AllocateDefaultPins();if(!DefaultEventCurrent(R))return R;}if(!DefaultNode.FreezePins())return R;
    }
    else{BP->Modify();G->Modify();FGraphNodeCreator<UK2Node_InputKey> Creator(*G);Node=Creator.CreateNode(false);Configure();Creator.Finalize();}
    DefaultNode.PinsFrozen=false;FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);if(!DefaultEventCurrent(R)||!DefaultNode.FreezePins())return R;
    const UEdGraphPin* Pressed = Node->FindPin(TEXT("Pressed"), EGPD_Output); const UEdGraphPin* Released = Node->FindPin(TEXT("Released"), EGPD_Output);
    const UEdGraphPin* KeyPin = Node->FindPin(TEXT("Key"), EGPD_Output); TSet<FGuid> PinIds;
    bool PinsValid = Node->Pins.Num() == 3 && Pressed && Released && KeyPin
        && Pressed->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && Released->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
        && KeyPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && KeyPin->PinType.PinSubCategoryObject.Get() == FKey::StaticStruct();
    for (const UEdGraphPin* P : Node->Pins)
    { PinsValid &= P && P->PinId.IsValid() && !PinIds.Contains(P->PinId) && !P->bOrphanedPin && P->LinkedTo.IsEmpty(); if (P) { PinIds.Add(P->PinId); } }
    const auto Data = R->GetObjectField(TEXT("data")); Data->SetStringField(TEXT("duplicate_policy"), TEXT("always_create_new_matching_UE56"));
    Data->SetBoolField(TEXT("input_mapping_modified"), false); Data->SetBoolField(TEXT("compiled_binding_verified"), false);
    Data->SetBoolField(TEXT("hardware_input_verified"), false);
    Finish(BP, G, Node, Before, Parent, true, G->Nodes.Contains(Node) && Node->GetClass() == UK2Node_InputKey::StaticClass()
        && Node->NodeGuid.IsValid() && Node->InputKey == Key && Node->NodePosX == X && Node->NodePosY == Y
        && Node->bControl == bControl && Node->bAlt == bAlt && Node->bShift == bShift && Node->bCommand == bCommand
        && Node->bConsumeInput == bConsumeInput && Node->bExecuteWhenPaused == bExecuteWhenPaused
        && Node->bOverrideParentBinding == bOverrideParentBinding && PinsValid, R, true);
    return R;
}

TSharedRef<FJsonObject> AddOverride(const FString& GraphPath, const FString& EventName, int32 X, int32 Y)
{
    auto R = Result();if(ActiveDefaultEventNode&&ActiveDefaultEventNode->Active){Fail(R,TEXT("event_compound_child_reentry"),TEXT("Compound event child already active; no nested writer entered."));return R;}FDefaultEventNodeScope DefaultNode(R);TGuardValue<FDefaultEventNodeScope*> NodeFence(ActiveDefaultEventNode,DefaultNode.Active?&DefaultNode:ActiveDefaultEventNode);
    if(ActiveInterfaceEvent){Fail(R,TEXT("interface_event_reentry_pending"),TEXT("An interface event observer/writer is active; no nested event request dispatched."));return R;}
    if (!Pin::Identifier(EventName)) { Fail(R, TEXT("event_name_invalid"), TEXT("event_name requires an exact ASCII identifier of 1..100 characters.")); return R; }
    UBlueprint* BP = nullptr; UEdGraph* G = nullptr; auto Before = MakeShared<FJsonObject>(); auto Parent = MakeShared<FJsonObject>();
    const bool bComponentEvent = EventName == TEXT("ReceiveBeginPlay") || EventName == TEXT("ReceiveEndPlay");
    if (!Prepare(GraphPath, X, Y, BP, G, Before, Parent, R, bComponentEvent)) { return R; }
    if(!DefaultNode.Bind(G))return R;
    const TStrongObjectPtr<UBlueprint> KeepBP(BP); const TStrongObjectPtr<UEdGraph> KeepGraph(G);
    UFunction* Function=BP->ParentClass->FindFunctionByName(FName(*EventName));bool FromInterface=false;FString Resolution=TEXT("parent");
    if(!Function)
    {
        // The public resolver reads ClassGeneratedBy/SkeletonGeneratedClass before lookup.
        // Admit and hold these nullable roles first; this path performs no lazy loading.
        struct FLookupRole{TStrongObjectPtr<UClass> Class;TStrongObjectPtr<UBlueprint> Blueprint;TStrongObjectPtr<UClass> Gen,Skel;UObject* GeneratedBy=nullptr;};
        TArray<FLookupRole> LookupRoles;
        auto LookupRole=[&](UClass* C)->bool
        {
            if(!FInterfaceEventSource::Live(C)||!C->ClassGeneratedBy.IsResolved())return false;for(const auto& V:LookupRoles)if(V.Class.Get()==C)return true;
            FLookupRole V;V.Class.Reset(C);V.GeneratedBy=C->ClassGeneratedBy.Get();if(V.GeneratedBy){if(!FInterfaceEventSource::Live(V.GeneratedBy)||V.GeneratedBy->GetClass()!=UBlueprint::StaticClass())return false;auto* B=static_cast<UBlueprint*>(V.GeneratedBy);V.Blueprint.Reset(B);if(!B->GeneratedClass.GetGCPtr().IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved())return false;auto* GC=B->GeneratedClass.Get();auto* SC=B->SkeletonGeneratedClass.Get();if((GC&&!FInterfaceEventSource::Live(GC))||(SC&&!FInterfaceEventSource::Live(SC)))return false;V.Gen.Reset(GC);V.Skel.Reset(SC);}LookupRoles.Add(MoveTemp(V));return true;
        };
        bool LookupReady=true;
        for(auto D:BP->ImplementedInterfaces){if(!D.Interface.GetGCPtr().IsResolved()||!LookupRole(D.Interface.Get())){LookupReady=false;break;}}
        TSet<UClass*> ParentClasses;for(UClass* C=BP->ParentClass;LookupReady&&C;C=C->GetSuperClass()){if(ParentClasses.Num()>=32||ParentClasses.Contains(C)||!FInterfaceEventSource::Live(C)){LookupReady=false;break;}ParentClasses.Add(C);for(const auto& D:C->Interfaces)if(!D.Class.IsResolved()||!LookupRole(D.Class.Get())){LookupReady=false;break;}}
        if(!LookupReady){Fail(R,TEXT("event_interface_reference_pending"),TEXT("Loaded resolved interface class/source/paired-class slots required before public lookup; wider lazy/cold sources remain incomplete."));return R;}
        bool Invalid=false;Function=FBlueprintEditorUtils::FindFunctionInImplementedInterfaces(BP,FName(*EventName),&Invalid,true);if(!DefaultEventCurrent(R))return R;const TStrongObjectPtr<UFunction> KeepLookupReturn(Function);
        if(!IsEditorGameThread()||IsGarbageCollecting()||UE::IsSavingPackage()||GExitPurge||IsEngineExitRequested()||!FInterfaceEventSource::Live(BP)||!FInterfaceEventSource::Live(G)){R->SetBoolField(TEXT("change_state_known"),false);R->SetField(TEXT("changed"),MakeShared<FJsonValueNull>());Fail(R,TEXT("event_interface_lookup_changed"),TEXT("Context/target changed during public interface lookup; no writer dispatched."));return R;}
        for(const auto& V:LookupRoles){auto* C=V.Class.Get();auto* B=V.Blueprint.Get();if(!FInterfaceEventSource::Live(C)||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy.Get()!=V.GeneratedBy||(B&&(!FInterfaceEventSource::Live(B)||!B->GeneratedClass.GetGCPtr().IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||B->GeneratedClass.Get()!=V.Gen.Get()||B->SkeletonGeneratedClass.Get()!=V.Skel.Get()))){R->SetBoolField(TEXT("change_state_known"),false);R->SetField(TEXT("changed"),MakeShared<FJsonValueNull>());Fail(R,TEXT("event_interface_lookup_changed"),TEXT("Interface source slots changed during public lookup; no writer dispatched."));return R;}}
        FromInterface=Function!=nullptr;Resolution=TEXT("implemented_interface");
        if(Invalid){Fail(R,TEXT("event_interface_reference_invalid"),TEXT("Public interface lookup encountered an invalid interface; no writer dispatched."));return R;}
    }
    if(!Function&&BP->GeneratedClass){Function=BP->GeneratedClass->FindFunctionByName(FName(*EventName));Resolution=TEXT("own_generated");}
    const TStrongObjectPtr<UFunction> ResolvedFunction(Function);R->GetObjectField(TEXT("data"))->SetStringField(TEXT("event_resolution_source"),Resolution);
    if(Function&&Resolution==TEXT("own_generated")){Fail(R,TEXT("event_own_generated_source_pending"),TEXT("Original own-generated fallback resolved; its mutable/custom event source roles remain valid incomplete, no writer dispatched."));return R;}TMap<FName,FEdGraphPinType> Params;
    FActorEventScope ActorScope(R);TGuardValue<FActorEventScope*> Observation(ActiveInterfaceEvent,(FromInterface||NativeCompoundEvent(Function))?&ActorScope:nullptr);if(!ActorScope.Bind(BP,G,Function,X,Y,FromInterface))return R;
    if(FromInterface){auto Current=MakeShared<FJsonObject>();if(!Capture(BP,G,Current)||Text(Current)!=Text(Before)){ActorScope.Stop(TEXT("target_source_preflight_observation"));return R;}}
    if(!DefaultEventCurrent(R))return R;
    if (!Signature(Function, Params, R,true,ActorScope.NativeMode)) { return R; }if(!ActorScope.Check(TEXT("signature_return")))return R;
    UClass* Declaring = Function->GetOuterUClass()->GetAuthoritativeClass();if(!DefaultEventCurrent(R))return R;
    if (Declaring->IsEditorOnly() || (Declaring->HasAnyClassFlags(CLASS_Native) && Declaring->GetOutermost()->GetName() != TEXT("/Script/Engine")))
    { Fail(R, TEXT("event_owner_unsupported"), TEXT("Event owner must belong to the validated runtime Engine Actor/component ancestry.")); return R; }
    if(!ActorScope.Check(TEXT("owner_return")))return R;
    TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);if(!ActorScope.Check(TEXT("allgraphs_return")))return R;
    if (Graphs.Num() > 256) { Fail(R, TEXT("event_graph_budget_exceeded"), TEXT("Override uniqueness inspection is limited to 256 graphs.")); return R; }
    UK2Node_Event* Existing = nullptr;
    for (UEdGraph* Candidate : Graphs)
    {
        if (!IsValid(Candidate) || Candidate->Nodes.Num() > 2048) { Fail(R, TEXT("event_graph_budget_exceeded"), TEXT("Override uniqueness inspection found an invalid or excessive graph.")); return R; }
        for (UEdGraphNode* N : Candidate->Nodes)
        {
            UK2Node_Event* E = Cast<UK2Node_Event>(N);
            if (E && E->EventReference.GetMemberName().ToString() == EventName)
            {
                if (Existing) { Fail(R, TEXT("event_override_ambiguous"), TEXT("More than one existing event matched; no node was moved or repaired.")); return R; }
                Existing = E;
            }
        }
    }
    if (Existing)
    {
        if(!DefaultNode.Adopt(Existing,true)||!DefaultNode.FreezePins())return R;
        const UClass* ExistingOwner = Existing->EventReference.GetMemberParentClass();
        // Factory defaults may name the original native declaration while a
        // manual override names the closest generated parent declaration.
        if (Existing->GetGraph() != G || Existing->GetClass() != UK2Node_Event::StaticClass() || !Existing->bOverrideFunction
            || Existing->EventReference.IsSelfContext() || !ExistingOwner || !(FromInterface?ExistingOwner==ActorScope.InterfaceSource.Generated:BP->ParentClass->IsChildOf(ExistingOwner))
            || !ExistingOwner->FindFunctionByName(FName(*EventName)) || !PinsMatch(Existing, Params, false, false))
        { Fail(R, TEXT("event_override_mismatch"), TEXT("Existing override is outside the selected graph or has a different class/signature/owner; it was not replaced.")); return R; }
        if(FromInterface){const auto* Delegate=Existing->FindPin(UK2Node_Event::DelegateOutputName,EGPD_Output);if(!Delegate||Delegate->PinType!=ActorScope.InterfaceSource.PlannedDelegate||(Existing->EventReference.GetMemberGuid().IsValid()&&Existing->EventReference.GetMemberGuid()!=ActorScope.InterfaceSource.Guid)){Fail(R,TEXT("event_override_mismatch"),TEXT("Existing interface event reference/delegate signature does not match the selected source; no repair dispatched."));return R;}auto* Actual=Existing->FindEventSignatureFunction();const TStrongObjectPtr<UFunction> KeepActual(Actual);if(!ActorScope.Check(TEXT("reuse_signature_return")))return R;if(Actual!=ActorScope.InterfaceSource.PlannedTarget){Fail(R,TEXT("event_override_mismatch"),TEXT("Existing interface event resolves a different source function."));return R;}}
        if(!ActorScope.Check(TEXT("before_reuse_observation")))return R;
        Finish(BP,G,Existing,Before,Parent,false,true,R);if(ActorScope.Check(TEXT("reuse_observation_return"))&&FromInterface)ActorScope.InterfaceSource.File(false);return R;
    }
    if (G->Nodes.Num() >= 2048) { Fail(R, TEXT("event_graph_budget_exceeded"), TEXT("No room for an event below the 2048-node graph bound.")); return R; }
    R->SetStringField(TEXT("stage"), TEXT("add_event")); R->SetBoolField(TEXT("mutation_attempted"), true); R->SetBoolField(TEXT("change_state_known"), false); R->SetField(TEXT("changed"), MakeShared<FJsonValueNull>());
    UK2Node_Event* Node=nullptr;
    if(!ActorScope.Active&&!DefaultNode.Active)
    {
        BP->Modify(); G->Modify(); FGraphNodeCreator<UK2Node_Event> Creator(*G);Node=Creator.CreateNode(false);
        Node->EventReference.SetExternalMember(FName(*EventName), Declaring);Node->bOverrideFunction=true;Node->NodePosX=X;Node->NodePosY=Y;Creator.Finalize();
        FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
    }
    else if(!ActorScope.Active)
    {
        BP->Modify();if(!DefaultEventCurrent(R))return R;G->Modify();if(!DefaultEventCurrent(R))return R;
        Node=NewObject<UK2Node_Event>(G,NAME_None,RF_Transactional|(G->HasAnyFlags(RF_Transient)?RF_Transient:RF_NoFlags));if(!DefaultNode.Adopt(Node))return R;
        Node->EventReference.SetExternalMember(FName(*EventName),Declaring);Node->bOverrideFunction=true;Node->NodePosX=X;Node->NodePosY=Y;
        if(!DefaultNode.Attaching())return R;G->AddNode(Node,false,true);if(!DefaultEventCurrent(R))return R;
        Node->CreateNewGuid();if(!DefaultEventCurrent(R))return R;Node->PostPlacedNewNode();if(!DefaultEventCurrent(R))return R;
        if(Node->Pins.IsEmpty()){Node->AllocateDefaultPins();if(!DefaultEventCurrent(R))return R;}if(!DefaultNode.FreezePins())return R;
        DefaultNode.PinsFrozen=false;FBlueprintEditorUtils::MarkBlueprintAsModified(BP);if(!DefaultEventCurrent(R)||!DefaultNode.FreezePins())return R;
    }
    else
    {
        if(!ActorScope.Check(TEXT("before_modify")))return R;BP->Modify();if(!ActorScope.Check(TEXT("after_blueprint_modify")))return R;G->Modify();if(!ActorScope.Check(TEXT("after_graph_modify")))return R;
        // Public equivalent of GraphNodeCreator, split at explicit callback returns.
        // Its destructor asserts if Finalize is skipped; no unsafe unwind is hidden.
        Node=NewObject<UK2Node_Event>(G,NAME_None,RF_Transactional|(G->HasAnyFlags(RF_Transient)?RF_Transient:RF_NoFlags));R->GetObjectField(TEXT("data"))->SetBoolField(TEXT("actor_event_allocation_returned"),Node!=nullptr);
        const TStrongObjectPtr<UK2Node_Event> ReturnedNode(Node);if(FromInterface&&!ActorScope.Check(TEXT("allocation_return_before_adopt")))return R;if(!ActorScope.Adopt(Node)||!DefaultNode.Adopt(Node))return R;
        Node->EventReference.SetExternalMember(FName(*EventName),Declaring);Node->bOverrideFunction=true;Node->NodePosX=X;Node->NodePosY=Y;ActorScope.Configured=true;
        if(FromInterface)R->GetObjectField(TEXT("data"))->SetStringField(TEXT("interface_event_reference_initial_guid"),Node->EventReference.GetMemberGuid().ToString(EGuidFormats::Digits));
        if(!ActorScope.Check(TEXT("before_add_node")))return R;ActorScope.ExpectedNodes.Add(Node);if(!DefaultNode.Attaching())return R;G->AddNode(Node,false,true);if(!ActorScope.Check(TEXT("after_add_node")))return R;
        Node->CreateNewGuid();if(!ActorScope.Context())return R;ActorScope.AddedGuid=Node->NodeGuid;ActorScope.GuidAdopted=true;if(!Node->NodeGuid.IsValid()||!ActorScope.Check(TEXT("after_guid")))return R;
        Node->PostPlacedNewNode();if(!ActorScope.Check(TEXT("after_postplaced")))return R;Node->AllocateDefaultPins();if(!ActorScope.Check(TEXT("after_allocate_pins"))||!ActorScope.FreezePins(Params)||!DefaultNode.FreezePins())return R;
        FBlueprintEditorUtils::MarkBlueprintAsModified(BP);if(!ActorScope.Check(TEXT("after_mark_modified")))return R;
        R->GetObjectField(TEXT("data"))->SetBoolField(TEXT("exact_actor_event_source_and_current_roles_verified"),!ActorScope.NativeMode);R->GetObjectField(TEXT("data"))->SetBoolField(TEXT("native_event_source_and_current_roles_verified"),ActorScope.NativeMode);
    }
    const bool Verified = G->Nodes.Contains(Node) && Node->NodeGuid.IsValid() && Node->GetClass() == UK2Node_Event::StaticClass()
        && Node->bOverrideFunction && Node->EventReference.GetMemberName().ToString() == EventName && Node->EventReference.GetMemberParentClass() == Declaring
        && Node->NodePosX == X && Node->NodePosY == Y && PinsMatch(Node, Params, false, true);
    if(!ActorScope.Check(TEXT("before_final_observation")))return R;Finish(BP,G,Node,Before,Parent,true,Verified,R);if(ActorScope.Check(TEXT("final_observation_return"))&&FromInterface)ActorScope.InterfaceSource.File(false);return R;
}

TSharedRef<FJsonObject> AddParentCall(const FString& GraphPath, const FString& EventNodePath, int32 X, int32 Y)
{
    auto R = Result();
    if (!Pin::SafeName(EventNodePath)) { Fail(R, TEXT("event_node_path_invalid"), TEXT("Use an exact bounded event node object path.")); return R; }
    UBlueprint* BP = nullptr; UEdGraph* G = nullptr; auto Before = MakeShared<FJsonObject>(); auto Parent = MakeShared<FJsonObject>();
    if (!Prepare(GraphPath, X, Y, BP, G, Before, Parent, R)) { return R; }
    const TStrongObjectPtr<UBlueprint> KeepBP(BP); const TStrongObjectPtr<UEdGraph> KeepGraph(G);
    UK2Node_Event* EventNode = FindObject<UK2Node_Event>(nullptr, *EventNodePath);
    const UClass* EventOwner = IsValid(EventNode) ? EventNode->EventReference.GetMemberParentClass() : nullptr;
    if (!IsValid(EventNode) || EventNode->GetPathName() != EventNodePath || EventNode->GetClass() != UK2Node_Event::StaticClass() || EventNode->GetGraph() != G
        || !G->Nodes.Contains(EventNode) || !EventNode->bOverrideFunction || EventNode->EventReference.GetMemberName() != TEXT("ReceiveTick")
        || EventNode->EventReference.IsSelfContext() || !EventOwner || !BP->ParentClass->IsChildOf(EventOwner))
    { Fail(R, TEXT("parent_event_unsupported"), TEXT("This parent-call slice requires an exact ReceiveTick override in the selected event graph.")); return R; }
    UBlueprint* ParentBP = Cast<UBlueprint>(BP->ParentClass->ClassGeneratedBy);
    UFunction* Compiled = BP->ParentClass->FindFunctionByName(TEXT("ReceiveTick"));
    UFunction* Current = ParentBP && ParentBP->SkeletonGeneratedClass ? ParentBP->SkeletonGeneratedClass->FindFunctionByName(TEXT("ReceiveTick")) : nullptr;
    if (!IsValid(ParentBP) || ParentBP->ParentClass != AActor::StaticClass() || ParentBP->GeneratedClass != BP->ParentClass
        || !IsValid(Compiled) || !IsValid(Current) || Compiled->GetOuterUClass() != BP->ParentClass || Current->GetOuterUClass() != ParentBP->SkeletonGeneratedClass
        || Compiled->Script.IsEmpty())
    { Fail(R, TEXT("parent_function_unsupported"), TEXT("Requires the direct saved Actor Blueprint parent's own compiled ReceiveTick implementation and current skeleton; deeper inheritance/native-only Tick remains pending.")); return R; }
    TMap<FName, FEdGraphPinType> Params, CurrentParams;
    if (!Signature(Compiled, Params, R) || !Signature(Current, CurrentParams, R)) { return R; }
    if (Params.Num() != 1 || Params.Num() != CurrentParams.Num() || !Params.Contains(TEXT("DeltaSeconds")) || !CurrentParams.Contains(TEXT("DeltaSeconds"))
        || Params[TEXT("DeltaSeconds")] != CurrentParams[TEXT("DeltaSeconds")] || !PinsMatch(EventNode, Params, false, false))
    { Fail(R, TEXT("parent_function_signature_mismatch"), TEXT("Current/compiled parent signature and event pins differ; no node was created.")); return R; }
    const TStrongObjectPtr<UBlueprint> KeepParent(ParentBP); UK2Node_CallParentFunction* Existing = nullptr;
    for (UEdGraphNode* N : G->Nodes)
    {
        UK2Node_CallParentFunction* Call = Cast<UK2Node_CallParentFunction>(N);
        if (Call && Call->GetFunctionName() == TEXT("ReceiveTick"))
        { if (Existing) { Fail(R, TEXT("parent_function_ambiguous"), TEXT("Multiple parent Tick calls exist; no automatic selection or repair.")); return R; } Existing = Call; }
    }
    FGuid ExpectedGuid; UBlueprint::GetGuidFromClassByFieldName<UFunction>(BP->ParentClass, TEXT("ReceiveTick"), ExpectedGuid);
    const UFunction* FirstDeclaration = Compiled;
    while (FirstDeclaration->GetSuperFunction()) { FirstDeclaration = FirstDeclaration->GetSuperFunction(); }
    const UClass* DeclarationClass = FirstDeclaration->GetOuterUClass()->GetAuthoritativeClass();
    FGuid DeclarationGuid; UBlueprint::GetGuidFromClassByFieldName<UFunction>(DeclarationClass, TEXT("ReceiveTick"), DeclarationGuid);
    const bool ExactNativeDeclaration = DeclarationClass == AActor::StaticClass() && DeclarationClass->HasAnyClassFlags(CLASS_Native)
        && FirstDeclaration == AActor::StaticClass()->FindFunctionByName(TEXT("ReceiveTick")) && !DeclarationGuid.IsValid();
    auto ReferenceMode = [&](const UK2Node_CallParentFunction* Call, bool bNew) -> FString
    {
        if (!Call || Call->FunctionReference.IsSelfContext() || Call->FunctionReference.GetMemberName() != TEXT("ReceiveTick")) { return FString(); }
        if (Call->FunctionReference.GetMemberParentClass() == BP->ParentClass && Call->FunctionReference.GetMemberGuid() == ExpectedGuid
            && Call->GetTargetFunction() == Current) { return TEXT("direct_generated_parent"); }
        // The compiler's ParentCall special case resolves from NewClass.SuperClass,
        // independently of the serialized declaration (CallFunctionHandler.cpp:663).
        // Factory nodes can therefore keep the exact native origin and zero GUID.
        // Only existing nodes get this alternative; creation keeps SetFromFunction(Compiled).
        if (!bNew && ExactNativeDeclaration && Call->FunctionReference.GetMemberParentClass() == DeclarationClass
            && Call->FunctionReference.GetMemberGuid() == DeclarationGuid && Call->GetTargetFunction() == FirstDeclaration)
        { return TEXT("original_native_declaration"); }
        return FString();
    };
    auto Matches = [&](UK2Node_CallParentFunction* Call, bool bNew)
    {
        const UEdGraphPin* Self = Call ? Call->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input) : nullptr;
        return Call && Call->GetClass() == UK2Node_CallParentFunction::StaticClass() && Call->NodeGuid.IsValid()
            && !ReferenceMode(Call, bNew).IsEmpty() && PinsMatch(Call, Params, true, bNew)
            && Self && Self->PinType.PinCategory == UEdGraphSchema_K2::PC_Object && Self->PinType.PinSubCategory.IsNone()
            && Self->PinType.PinSubCategoryObject.Get() == DeclarationClass;
    };
    const auto Data = R->GetObjectField(TEXT("data")); Data->SetStringField(TEXT("compiled_function_path"), Compiled->GetPathName());
    Data->SetStringField(TEXT("expected_resolved_function_path"), Current->GetPathName()); Data->SetStringField(TEXT("event_node_path"), EventNodePath);
    Data->SetBoolField(TEXT("function_member_guid_available"), ExpectedGuid.IsValid());
    Data->SetStringField(TEXT("effective_compile_parent_class"), BP->ParentClass->GetPathName());
    Data->SetStringField(TEXT("effective_compile_function_path"), Compiled->GetPathName());
    Data->SetStringField(TEXT("expected_generated_parent_editor_function_path"), Current->GetPathName());
    Data->SetStringField(TEXT("compile_resolution_basis"), TEXT("UE58 FKCHandler_CallFunction::GetCallingContext/FindFunction: nearest superclass implementation; parent-context final function statement"));
    Data->SetBoolField(TEXT("runtime_execution_verified"), false);
    if (Existing)
    {
        if (!Matches(Existing, false)) { Fail(R, TEXT("parent_function_mismatch"), TEXT("Existing parent call has a different identity/signature; it was not replaced.")); return R; }
        const FString Mode = ReferenceMode(Existing, false); Data->SetStringField(TEXT("serialized_reference_mode"), Mode);
        Data->SetStringField(TEXT("expected_resolved_function_path"), Mode == TEXT("original_native_declaration") ? FirstDeclaration->GetPathName() : Current->GetPathName());
        Finish(BP, G, Existing, Before, Parent, false, true, R); return R;
    }
    if (G->Nodes.Num() >= 2048) { Fail(R, TEXT("event_graph_budget_exceeded"), TEXT("No room for a parent call below the graph bound.")); return R; }
    R->SetStringField(TEXT("stage"), TEXT("add_parent_call")); R->SetBoolField(TEXT("mutation_attempted"), true); R->SetBoolField(TEXT("change_state_known"), false); R->SetField(TEXT("changed"), MakeShared<FJsonValueNull>());
    BP->Modify(); G->Modify(); FGraphNodeCreator<UK2Node_CallParentFunction> Creator(*G); UK2Node_CallParentFunction* Node = Creator.CreateNode(false);
    // The public virtual dispatch avoids directly importing a non-exported
    // MinimalAPI override; its implementation keeps the explicit parent context.
    UK2Node_CallFunction* Base = Node; Base->SetFromFunction(Compiled); Node->NodePosX = X; Node->NodePosY = Y; Creator.Finalize();
    FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
    Data->SetStringField(TEXT("serialized_reference_mode"), ReferenceMode(Node, true));
    Finish(BP, G, Node, Before, Parent, true, G->Nodes.Contains(Node) && Node->NodePosX == X && Node->NodePosY == Y && Matches(Node, true), R); return R;
}

namespace
{
// WM: fixed event-tool compound prefix, not a public graph creation service.
// Compiler-generated classes/CDOs are intentionally not pointer invariants.
struct FDefaultEventGraphScope
{
    TSharedRef<FJsonObject> R;
    TStrongObjectPtr<UBlueprint> BP;
    TStrongObjectPtr<UPackage> Package;
    TStrongObjectPtr<UClass> Parent;
    TWeakObjectPtr<UEditorEngine> Editor;
    TWeakObjectPtr<UWorld> World;
    FString Path;
    TArray<TObjectPtr<UEdGraph>> Uber,Events,Functions,Macros,Delegates;
    TArray<FBPVariableDescription> Variables;
    TArray<TStrongObjectPtr<UObject>> Holds;
    TArray<TSharedRef<FJsonObject>> Before;
    TArray<UEdGraph*> Old;TArray<UObject*> OldOuters;
    TStrongObjectPtr<UEdGraph> Created;
    UEdGraph* Observing=nullptr;TArray<TObjectPtr<UEdGraphNode>> ObservedNodes;TArray<TArray<UEdGraphPin*>> ObservedPins;
    bool Attached=false,Stopped=false;
    explicit FDefaultEventGraphScope(TSharedRef<FJsonObject> In,UBlueprint* B):R(In),BP(B),Package(B->GetOutermost()),Parent(B->ParentClass),
        Editor(GEditor),World(GEditor->GetEditorWorldContext().World()),Path(B->GetPathName()),Uber(B->UbergraphPages),Events(B->EventGraphs),
        Functions(B->FunctionGraphs),Macros(B->MacroGraphs),Delegates(B->DelegateSignatureGraphs),Variables(B->NewVariables){}
    bool Stop(const TCHAR* Property)
    {
        if(!Stopped){auto D=R->GetObjectField(TEXT("data"));D->SetStringField(TEXT("default_graph_first_mismatch"),Property);
            D->SetStringField(TEXT("default_graph_failure_stage"),R->GetStringField(TEXT("stage")));
            D->SetBoolField(TEXT("followup_sdk_stopped"),true);Fail(R,TEXT("default_event_graph_scope_changed"),TEXT("Default graph compound scope changed; actual prefix retained. No cleanup or automatic retry."));}
        Stopped=true;R->SetBoolField(TEXT("success"),false);R->SetBoolField(TEXT("postcondition_passed"),false);
        R->SetBoolField(TEXT("change_state_known"),false);R->SetField(TEXT("changed"),MakeShared<FJsonValueNull>());return false;
    }
    bool Current()
    {
        if(Stopped)return false;
        if(!IsEditorGameThread()||IsPlaySessionActive()||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()
            ||!Editor.IsValid()||GEditor!=Editor.Get()||!World.IsValid()||World->bIsTearingDown
            ||GEditor->GetEditorWorldContext().World()!=World.Get())return Stop(TEXT("editor_world"));
        if(!IsValid(BP.Get())||!IsValid(Package.Get())||!IsValid(Parent.Get())||BP->GetPathName()!=Path||BP->GetOutermost()!=Package.Get()
            ||BP->ParentClass!=Parent.Get()||BP->BlueprintType!=BPTYPE_Normal||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bSuppressStructurallyModified
            ||BP->bIsRegeneratingOnLoad||BP->Status==BS_BeingCreated||BP->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_BeginDestroyed|RF_FinishDestroyed))return Stop(TEXT("blueprint_role"));
        auto ExpectedUber=Uber;if(Attached)ExpectedUber.Add(Created.Get());
        if(BP->UbergraphPages!=ExpectedUber||BP->EventGraphs!=Events||BP->FunctionGraphs!=Functions||BP->MacroGraphs!=Macros||BP->DelegateSignatureGraphs!=Delegates)
            return Stop(TEXT("graph_membership"));
        if(BP->NewVariables.Num()!=Variables.Num())return Stop(TEXT("authored_variables_count"));
        for(int32 I=0;I<Old.Num();++I)if(!IsValid(Old[I])||Old[I]->GetOuter()!=OldOuters[I]||Old[I]->GetPathName()!=Before[I]->GetStringField(TEXT("graph_path"))
            ||Old[I]->GraphGuid.ToString(EGuidFormats::Digits)!=Before[I]->GetStringField(TEXT("graph_guid")))return Stop(TEXT("existing_graph_role"));
        if(Created.Get()&&(!Created.IsValid()||Created->GetOuter()!=BP.Get()||Created->GetFName()!=TEXT("EventGraph")||Created->Schema.Get()!=UEdGraphSchema_K2::StaticClass()
            ||!Created->GraphGuid.IsValid()))return Stop(TEXT("created_graph_role"));
        if(Observing)
        {
            if(!IsValid(Observing)||Observing->Nodes!=ObservedNodes)return Stop(TEXT("observer_node_membership"));
            for(int32 I=0;I<ObservedNodes.Num();++I){auto* N=ObservedNodes[I].Get();if(!IsValid(N)||N->GetOuter()!=Observing||N->Pins!=ObservedPins[I])return Stop(TEXT("observer_pin_membership"));
                for(auto* P:ObservedPins[I])if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N)return Stop(TEXT("observer_pin_lifetime"));}
        }
        return true;
    }
    bool Observe(UEdGraph* G,const TSharedRef<FJsonObject>& Out)
    {
        if(!Current()||!IsValid(G))return false;
        Observing=G;ObservedNodes=G->Nodes;ObservedPins.Reset();
        for(UEdGraphNode* N:ObservedNodes){if(!IsValid(N)){Observing=nullptr;return Stop(TEXT("observation_node_lifetime"));}Holds.Emplace(N);ObservedPins.Add(N->Pins);}
        if(!Current())return false;
        const bool Complete=Capture(BP.Get(),G,Out);if(!Current())return false;
        Observing=nullptr;ObservedNodes.Reset();ObservedPins.Reset();
        return Complete||Stop(TEXT("observation_incomplete"));
    }
    bool CaptureOriginals()
    {
        if(!Current())return false;TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);if(!Current()||Graphs.Num()>256)return Stop(TEXT("graph_budget"));
        // Populate roles incrementally so Current never indexes an unobserved snapshot.
        for(UEdGraph* G:Graphs)
        {
            if(!IsValid(G)||!IsValid(G->GetOuter())||FBlueprintEditorUtils::FindBlueprintForGraph(G)!=BP.Get())return Stop(TEXT("existing_graph_owner"));
            Holds.Emplace(G->GetOuter());
            Holds.Emplace(G);for(UEdGraphNode* N:G->Nodes){if(!IsValid(N))return Stop(TEXT("existing_node_lifetime"));Holds.Emplace(N);}
            auto S=MakeShared<FJsonObject>();if(!Observe(G,S))return false;
            S->RemoveField(TEXT("dirty"));S->RemoveField(TEXT("status"));Old.Add(G);OldOuters.Add(G->GetOuter());Before.Add(S);
        }
        return Current();
    }
    bool Preserved()
    {
        if(!Current())return false;
        auto SameVariables=[&](){for(int32 I=0;I<Variables.Num();++I)if(!FBPVariableDescription::StaticStruct()->CompareScriptStruct(&BP->NewVariables[I],&Variables[I],PPF_None))return Stop(TEXT("authored_variables"));return true;};
        if(!SameVariables())return false;
        for(int32 I=0;I<Old.Num();++I){auto S=MakeShared<FJsonObject>();if(!Observe(Old[I],S))return false;
            S->RemoveField(TEXT("dirty"));S->RemoveField(TEXT("status"));if(Text(S)!=Text(Before[I]))return Stop(TEXT("existing_authored_graph"));}
        return Current()&&SameVariables();
    }
};
}

TSharedRef<FJsonObject> AddResolved(const FString& RequestJson)
{
    auto R=Result();auto D=R->GetObjectField(TEXT("data"));TSharedPtr<FJsonObject> Q;
    if(DefaultEventDispatchBusy){Fail(R,TEXT("event_compound_reentrant"),TEXT("An event graph compound request is already active; no writer entered."));return R;}
    TGuardValue<bool> Dispatch(DefaultEventDispatchBusy,true);
    if(RequestJson.Len()>32768||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(RequestJson),Q)||!Q.IsValid())
    {Fail(R,TEXT("event_request_invalid"),TEXT("Expected a bounded internal event-tool request object."));return R;}
    FString Operation,Path,Name,Id,Kind,Selector;double X=0,Y=0;bool Create=false;
    if(!Q->TryGetStringField(TEXT("operation"),Operation)||!Q->TryGetStringField(TEXT("blueprint_path"),Path)
        ||!Q->TryGetStringField(TEXT("graph_name"),Name)||!Q->TryGetStringField(TEXT("graph_id"),Id)||!Q->TryGetStringField(TEXT("graph_type"),Kind)
        ||!Q->TryGetStringField(TEXT("selector"),Selector)||!Q->TryGetNumberField(TEXT("x"),X)||!Q->TryGetNumberField(TEXT("y"),Y)
        ||!Q->TryGetBoolField(TEXT("create_graph_if_missing"),Create)||!FMath::IsFinite(X)||!FMath::IsFinite(Y)||X!=FMath::TruncToDouble(X)||Y!=FMath::TruncToDouble(Y)
        ||FMath::Abs(X)>1000000||FMath::Abs(Y)>1000000||!Pin::SafeName(Path)||!Path.StartsWith(TEXT("/Game/"))
        ||!Pin::SafeName(Name,true)||!Pin::SafeName(Id,true)||!Pin::SafeName(Kind,true)||!Pin::SafeName(Selector))
    {Fail(R,TEXT("event_request_invalid"),TEXT("Invalid event-tool selectors, coordinates or create flag; no writer entered."));return R;}
    const bool Custom=Operation==TEXT("add_blueprint_custom_event_node"),Input=Operation==TEXT("add_blueprint_input_key_node");
    if(!Custom&&!Input&&Operation!=TEXT("add_blueprint_event_node")){Fail(R,TEXT("event_operation_invalid"),TEXT("Only the three fixed legacy event tools use this entry."));return R;}
    bool Options[7]={false,false,false,false,true,false,false};
    const TCHAR* CustomKeys[]={TEXT("call_in_editor"),TEXT("has_call_in_editor")};
    const TCHAR* InputKeys[]={TEXT("control"),TEXT("alt"),TEXT("shift"),TEXT("command"),TEXT("consume_input"),TEXT("execute_when_paused"),TEXT("override_parent_binding")};
    for(int32 I=0;I<(Input?7:Custom?2:0);++I)if(!Q->TryGetBoolField(Input?InputKeys[I]:CustomKeys[I],Options[I]))
    {Fail(R,TEXT("event_option_invalid"),TEXT("Event options must be typed booleans; no writer entered."));return R;}
    Kind=Kind.TrimStartAndEnd().ToLower();if(Kind==TEXT("eventgraph")||Kind==TEXT("ubergraph"))Kind=TEXT("event");
    if(!Kind.IsEmpty()&&Kind!=TEXT("event")&&Kind!=TEXT("any"))
    {Fail(R,TEXT("event_graph_type_unsupported"),TEXT("Event graph node creation in function/macro/delegate graphs remains unimplemented."));return R;}
    FGuid Guid;if(!Id.IsEmpty()&&(!FGuid::Parse(Id,Guid)||!Guid.IsValid())){Fail(R,TEXT("event_graph_selector_invalid"),TEXT("graph_id must be a valid nonzero GUID."));return R;}
    auto Globals=[](){return IsEditorGameThread()&&!IsPlaySessionActive()&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&GEditor;};
    if(!Globals()){Fail(R,TEXT("event_editor_state_invalid"),TEXT("Requires an idle editor game thread."));return R;}
    TWeakObjectPtr<UEditorEngine> Editor=GEditor;TWeakObjectPtr<UWorld> World=GEditor->GetEditorWorldContext().World();
    const FString AssetPath=Path.Contains(TEXT("."))?Path:Path+TEXT(".")+FPackageName::GetShortName(Path);
    const bool WasLoaded=FindObject<UBlueprint>(nullptr,*AssetPath)!=nullptr;
    R->SetBoolField(TEXT("loaded_during_operation"),!WasLoaded);D->SetBoolField(TEXT("loaded_during_operation"),!WasLoaded);D->SetBoolField(TEXT("implicit_postload_work_possible"),!WasLoaded);
    UBlueprint* BP=LoadObject<UBlueprint>(nullptr,*AssetPath);
    if(!Globals()||GEditor!=Editor.Get()||!World.IsValid()||World->bIsTearingDown||GEditor->GetEditorWorldContext().World()!=World.Get())
    {R->SetBoolField(TEXT("change_state_known"),false);R->SetField(TEXT("changed"),MakeShared<FJsonValueNull>());Fail(R,TEXT("event_load_context_changed"),TEXT("Load returned into a different editor context; no further SDK call or retry."));return R;}
    if(!IsValid(BP)||BP->GetPathName()!=AssetPath||!IsValid(BP->ParentClass)||BP->BlueprintType!=BPTYPE_Normal
        ||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bSuppressStructurallyModified||BP->bIsRegeneratingOnLoad||BP->Status==BS_BeingCreated
        ||(!BP->ParentClass->IsChildOf(AActor::StaticClass())&&!(Custom&&BP->ParentClass==UActorComponent::StaticClass())))
    {Fail(R,TEXT("event_blueprint_scope_unsupported"),TEXT("Requires a normal idle loaded Actor Blueprint (basic custom events also allow native ActorComponent)."));return R;}
    const TStrongObjectPtr<UBlueprint> KeepBP(BP);UEdGraph* G=nullptr;
    const bool Default=Name.IsEmpty()&&Id.IsEmpty()&&Kind.IsEmpty();
    if(Default){for(UEdGraph* Candidate:BP->UbergraphPages)if(IsValid(Candidate)&&Candidate->GetName().Contains(TEXT("EventGraph"))){G=Candidate;break;}}
    else{TArray<UEdGraph*> All;BP->GetAllGraphs(All);if(All.Num()>256){Fail(R,TEXT("event_graph_budget_exceeded"),TEXT("Selection exceeds 256 graphs."));return R;}
        for(UEdGraph* Candidate:All)if(IsValid(Candidate)&&(Name.IsEmpty()||Candidate->GetName().Equals(Name,ESearchCase::IgnoreCase))
            &&(Id.IsEmpty()||Candidate->GraphGuid==Guid)&&(Kind.IsEmpty()||Kind==TEXT("any")||BP->UbergraphPages.Contains(Candidate)||BP->EventGraphs.Contains(Candidate))){G=Candidate;break;}}
    auto Selection=MakeShared<FJsonObject>();Selection->SetBoolField(TEXT("created"),false);Selection->SetBoolField(TEXT("create_graph_if_missing"),Create);
    Selection->SetBoolField(TEXT("legacy_default_selection"),Default);D->SetObjectField(TEXT("graph_selection"),Selection);
    auto Call=[&](){const FString GraphPath=G->GetPathName();return Custom?AddCustom(GraphPath,Selector,static_cast<int32>(X),static_cast<int32>(Y),Options[0],Options[1]):
        Input?AddInputKey(GraphPath,Selector,static_cast<int32>(X),static_cast<int32>(Y),Options[0],Options[1],Options[2],Options[3],Options[4],Options[5],Options[6]):
        AddOverride(GraphPath,Selector,static_cast<int32>(X),static_cast<int32>(Y));};
    auto Merge=[&](TSharedRef<FJsonObject> Child,bool Prefix){auto CD=Child->GetObjectField(TEXT("data"));for(const auto& Pair:D->Values)CD->SetField(Pair.Key,Pair.Value);
        if(Prefix){Child->SetBoolField(TEXT("mutation_attempted"),true);if(Child->GetBoolField(TEXT("change_state_known")))Child->SetBoolField(TEXT("changed"),true);}
        Child->SetBoolField(TEXT("loaded_during_operation"),!WasLoaded);return Child;};
    if(G&&!BP->UbergraphPages.Contains(G)&&!BP->EventGraphs.Contains(G)){Fail(R,TEXT("event_graph_type_unsupported"),TEXT("Selected graph is not an event graph; event-node creation in other graph kinds remains pending."));return R;}
    if(G){const TStrongObjectPtr<UEdGraph> KeepGraph(G);Selection->SetStringField(TEXT("graph_path"),G->GetPathName());Selection->SetStringField(TEXT("graph_type"),TEXT("event"));return Merge(Call(),false);}
    if(!(Default||(Create&&Name.IsEmpty()&&Id.IsEmpty()&&Kind==TEXT("event"))))
    {Fail(R,TEXT("event_graph_not_found"),TEXT("Graph not found. Only the original implicit default/EventGraph bootstrap is implemented; explicit missing GUID/named event graphs are not created."));return R;}
    FDefaultEventGraphScope Scope(R,BP);TFunction<bool()> Current=[&](){return Scope.Current();};
    TGuardValue<const TFunction<bool()>*> Fence(ActiveDefaultEventFence,&Current);
    if(!Scope.Current()||!Scope.CaptureOriginals())return R;
    auto Parent=BP->ParentClass->ClassGeneratedBy?GeneratedComponent::InspectSavedActorParent(BP->ParentClass):InspectBlueprintParent(BP->ParentClass->GetPathName());
    D->SetObjectField(TEXT("bootstrap_parent_inspection"),Parent);
    if(!Scope.Current())return R;
    if(!Parent->GetBoolField(TEXT("success"))||!ParentStateUnchanged(Parent)||!Scope.Current()||!Scope.Preserved())
    {if(!Scope.Stopped)Fail(R,TEXT("event_bootstrap_parent_unsupported"),TEXT("Parent or authored graph preservation preflight failed; no graph writer entered."));return R;}
    if(FindObjectFast<UObject>(BP,TEXT("EventGraph")))
    {Fail(R,TEXT("event_default_graph_name_collision"),TEXT("Default graph name collides with an owned object; implicit rename is not implemented. No writer entered."));return R;}
    R->SetStringField(TEXT("stage"),TEXT("create_default_event_graph"));R->SetBoolField(TEXT("mutation_attempted"),true);
    R->SetBoolField(TEXT("change_state_known"),false);R->SetField(TEXT("changed"),MakeShared<FJsonValueNull>());
    BP->Modify();if(!Scope.Current())return R;
    if(FindObjectFast<UObject>(BP,TEXT("EventGraph"))){Scope.Stop(TEXT("default_name_after_modify"));return R;}
    G=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("EventGraph"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
    D->SetBoolField(TEXT("default_graph_allocation_returned_nonnull"),G!=nullptr);
    if(!Scope.Current())return R;
    if(!IsValid(G)){Fail(R,TEXT("event_default_graph_creation_failed"),TEXT("CreateNewGraph returned no graph; modification effects remain uncertain. Do not retry automatically."));return R;}
    Scope.Created.Reset(G);Selection->SetStringField(TEXT("graph_path"),G->GetPathName());D->SetBoolField(TEXT("default_graph_allocated"),true);
    if(!Scope.Current())return R;
    Scope.Attached=true;D->SetBoolField(TEXT("implicit_skeleton_compile_possible"),true);
    FBlueprintEditorUtils::AddUbergraphPage(BP,G);D->SetBoolField(TEXT("add_ubergraph_page_returned"),true);
    if(!Scope.Current())return R;Selection->SetBoolField(TEXT("created"),true);D->SetBoolField(TEXT("default_graph_attached"),true);
    if(!Scope.Preserved())return R;
    if(!ParentStateUnchanged(Parent)){Scope.Stop(TEXT("readonly_parent_after_attach"));return R;}
    if(!Scope.Current())return R;
    if(!G->Nodes.IsEmpty()||!BP->GetOutermost()->IsDirty()){Fail(R,TEXT("event_default_graph_postcondition_failed"),TEXT("Default graph was attached but graph/dirty verification failed; no rollback or retry."));return R;}
    Selection->SetBoolField(TEXT("created"),true);Selection->SetStringField(TEXT("graph_type"),TEXT("event"));
    D->SetBoolField(TEXT("default_graph_attached"),true);D->SetBoolField(TEXT("default_graph_authored_state_preserved"),true);
    D->SetBoolField(TEXT("dirty_after_default_graph"),BP->GetOutermost()->IsDirty());
    R->SetBoolField(TEXT("changed"),true);R->SetBoolField(TEXT("change_state_known"),true);
    R->SetStringField(TEXT("stage"),TEXT("add_node_after_default_graph"));auto Child=Call();
    D->SetObjectField(TEXT("default_graph_child_result"),Child);
    if(!Scope.Current()||!Scope.Preserved())return R;
    D->RemoveField(TEXT("default_graph_child_result"));return Merge(Child,true);
}

}
