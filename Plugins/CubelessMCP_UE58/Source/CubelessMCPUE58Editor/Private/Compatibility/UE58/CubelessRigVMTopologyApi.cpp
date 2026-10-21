// UE56 default-model topology contract; UE58 public ControlRig/RigVM APIs only.
#include "Compatibility/UE58/CubelessRigVMTopologyApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "ControlRig.h"
#include "ControlRigAssetReference.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigEditorAsset.h"
#include "ControlRigRuntimeAsset.h"
#include "Dom/JsonValue.h"
#include "Containers/UnrealString.h"
#include "EditorAssetLibrary.h"
#include "Misc/CoreMisc.h"
#include "Misc/PackageName.h"
#include "RigVMBlueprintGeneratedClass.h"
#include "RigVMModel/Nodes/RigVMUnitNode.h"
#include "RigVMModel/RigVMClient.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMModel/RigVMFunctionLibrary.h"
#include "RigVMModel/RigVMLink.h"
#include "RigVMModel/RigVMNode.h"
#include "RigVMModel/RigVMPin.h"
#include "UObject/Package.h"
#include "UObject/Class.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace Cubeless::UE58::RigVMTopology
{
namespace
{
using Variable::FResult;
constexpr int32 SafetyDepth = 64, SafetyRows = 65536, SafetyTreeChecks = 131072;
constexpr int32 SafetyStringChars = 1048576, SafetyTotalStringChars = 8388608;
// This inspector owns no async work. Reject callback reentry without touching Engine settings.
bool ReadInFlight = false;

bool Loaded(const UObject* Object)
{
    return IsValid(Object) && !Object->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects
        | RF_BeginDestroyed | RF_FinishDestroyed);
}
void Count(FResult& R, const TCHAR* Key)
{
    const auto C = R.Data->GetObjectField(TEXT("native_call_counts"));
    C->SetNumberField(Key, C->GetNumberField(Key) + 1);
}
void Init(FResult& R)
{
    auto C = MakeShared<FJsonObject>();
    for (const TCHAR* Key : {TEXT("asset_load_calls"), TEXT("asset_load_returns"), TEXT("class_load_calls"),
        TEXT("class_find_calls"), TEXT("default_model_calls"), TEXT("asset_reference_calls"),
        TEXT("node_projection_attempts"), TEXT("pin_projection_attempts"), TEXT("link_projection_attempts"),
        TEXT("pin_tree_validation_attempts"), TEXT("getter_identity_checks"), TEXT("event_projection_calls"),
        TEXT("explicit_modify_calls"), TEXT("explicit_compile_calls"), TEXT("explicit_save_calls"), TEXT("runtime_execution_calls")})
    { C->SetNumberField(Key, 0); }
    R.Data->SetObjectField(TEXT("native_call_counts"), C);
    R.Data->SetStringField(TEXT("command"), TEXT("inspect_controlrig_rigvm_topology"));
    R.Data->SetStringField(TEXT("requested_control_rig"), TEXT(""));
    R.Data->SetBoolField(TEXT("read_only"), true);
    R.Data->SetBoolField(TEXT("source_modified"), false); // Legacy field: explicit authoring only.
    R.Data->SetBoolField(TEXT("explicit_authoring_read_only"), true);
    R.Data->SetBoolField(TEXT("legacy_source_modified_is_total_change_proof"), false);
    R.Data->SetBoolField(TEXT("total_SDK_side_effects_verified"), false);
    R.Data->SetBoolField(TEXT("atomic_snapshot_verified"), false);
    R.Data->SetBoolField(TEXT("observation_incomplete"), false);
    R.Data->SetBoolField(TEXT("observed_dirty_changed"), false);
    R.Data->SetBoolField(TEXT("automatic_action_retry"), false);
    R.Data->SetBoolField(TEXT("atomic_rollback_available"), false);
    R.Data->SetBoolField(TEXT("unique_external_pipeline_complete"), false);
    R.Data->SetBoolField(TEXT("packaged_runtime_verified"), false);
    R.Data->SetNumberField(TEXT("explicit_graph_get_event_names_calls"), 0);
    R.Data->SetNumberField(TEXT("projection_safety_rows"), SafetyRows);
    R.Data->SetNumberField(TEXT("projection_safety_depth"), SafetyDepth);
    R.Data->SetStringField(TEXT("change_state_scope"), TEXT("Explicit Modify/compile/save/execute/solve calls are zero. Load/PostLoad, virtual getters, RigVM type/default caches and callbacks have unknown total effects; dirty observations do not establish total purity."));
}
FResult Fail(FResult R, const TCHAR* Code, const FString& Error)
{
    R.bSuccess = false; R.bPostconditionPassed = false; R.ErrorCode = Code; R.Error = Error;
    R.Data->SetStringField(TEXT("message"), Error);
    return R;
}
FString Normalize(const FString& Path)
{
    FString N = FPackageName::ExportTextPathToObjectPath(Path).TrimStartAndEnd();
    N.TrimQuotesInline();
    if ((N.StartsWith(TEXT("/Game/")) || N.StartsWith(TEXT("/Engine/"))) && !N.Contains(TEXT(".")))
    { N += TEXT(".") + FPackageName::GetShortName(N); }
    return N;
}
bool InputPath(const FString& Path)
{
    if (Path.Len() > SafetyStringChars) { return false; }
    for (int32 I = 0; I < Path.Len(); ++I) { if (Path[I] == TCHAR(0)) { return false; } }
    return Normalize(Path).Len() < NAME_SIZE;
}
UObject* LoadAsset(const FString& Path, FResult& R)
{
    R.bChangeStateKnown = false; // Set before dispatch, including a null/error return.
    const FString N = Normalize(Path);
    Count(R, TEXT("asset_load_calls"));
    UObject* Object = UEditorAssetLibrary::LoadAsset(N);
    if (Object) { Count(R, TEXT("asset_load_returns")); return Object; }
    // Preserve the original typed-load fallback; this is not a retry of a mutation request.
    Count(R, TEXT("asset_load_calls"));
    Object = StaticLoadObject(UObject::StaticClass(), nullptr, *N);
    if (Object) { Count(R, TEXT("asset_load_returns")); }
    return Object;
}
UClass* LoadClass(const FString& Path, FResult& R)
{
    R.bChangeStateKnown = false;
    Count(R, TEXT("class_load_calls"));
    if (UClass* C = LoadObject<UClass>(nullptr, *Normalize(Path))) { return C; }
    Count(R, TEXT("class_find_calls"));
    if (UClass* C = FindFirstObject<UClass>(*Path)) { return C; }
    Count(R, TEXT("class_load_calls"));
    return LoadObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *Path));
}
FString Direction(ERigVMPinDirection D)
{
    switch (D)
    {
    case ERigVMPinDirection::Input: return TEXT("input");
    case ERigVMPinDirection::Output: return TEXT("output");
    case ERigVMPinDirection::IO: return TEXT("io");
    case ERigVMPinDirection::Visible: return TEXT("visible");
    case ERigVMPinDirection::Hidden: return TEXT("hidden");
    default: return TEXT("invalid");
    }
}
TSharedRef<FJsonObject> Vector(const FVector2D& V)
{
    auto J = MakeShared<FJsonObject>(); J->SetNumberField(TEXT("x"), V.X); J->SetNumberField(TEXT("y"), V.Y); return J;
}

struct FRead
{
    FRead(FResult& InR, UObject* InAsset) : R(InR), Asset(InAsset), Package(InAsset->GetPackage()),
        AssetClass(InAsset->GetClass()), AssetOuter(InAsset->GetOuter()), AssetPath(InAsset->GetPathName()),
        PackagePath(Package->GetPathName()), bDirtyBefore(Package->IsDirty()) {}
    FResult& R;
    TStrongObjectPtr<UObject> Asset;
    TStrongObjectPtr<UPackage> Package;
    TStrongObjectPtr<UClass> AssetClass;
    TStrongObjectPtr<UClass> InputClass;
    UObject* AssetOuter;
    FString AssetPath, PackagePath;
    bool bDirtyBefore;
    TStrongObjectPtr<URigVMGraph> Graph;
    UClass* GraphClass = nullptr;
    UObject* GraphOuter = nullptr;
    FString GraphPath;
    FRigVMClient* Client = nullptr;
    TArray<URigVMNode*> NodePrefix;
    TArray<URigVMNode*> EventNodes;
    TArray<URigVMLink*> LinkPrefix;
    int32 NodeCount = 0, LinkCount = 0, Rows = 0, TreeChecks = 0, StringChars = 0;
    bool bGraphSnapshot = false;
    FString Error;
    TSet<UObject*> Held;
    TArray<TStrongObjectPtr<UObject>> Lifetimes;
    TMap<UObject*, TPair<UPackage*, FString>> ActualPackages;

    bool Bad(const FString& Message)
    { if (Error.IsEmpty()) { Error = Message; } R.Data->SetBoolField(TEXT("observation_incomplete"), true); return false; }
    void Hold(UObject* O)
    {
        if (O && !Held.Contains(O))
        {
            Held.Add(O); Lifetimes.Emplace(O);
            UPackage* P = O->GetPackage();
            ActualPackages.Emplace(O, TPair<UPackage*, FString>(P, P ? P->GetPathName() : FString()));
            if (P && P != O) { Hold(P); }
        }
    }
    bool SamePackage(UObject* O)
    {
        const auto* Captured = ActualPackages.Find(O);
        return Captured && Loaded(O) && Loaded(Captured->Key) && O->GetPackage() == Captured->Key
            && Captured->Key->GetPathName() == Captured->Value ? true : Bad(TEXT("Captured nested object's actual package identity changed"));
    }
    bool RawCurrent()
    {
        return IsEditorGameThread() && !IsPlaySessionActive() && !IsEngineExitRequested()
            && Loaded(Asset.Get()) && Loaded(Package.Get()) && Loaded(AssetClass.Get())
            && !AssetClass->HasAnyClassFlags(CLASS_NewerVersionExists)
            && Asset->GetClass() == AssetClass.Get() && Asset->GetOuter() == AssetOuter
            && Asset->GetPathName() == AssetPath && Asset->GetPackage() == Package.Get() && Package->GetPathName() == PackagePath
            && (!InputClass.IsValid() || (Loaded(InputClass.Get()) && !InputClass->HasAnyClassFlags(CLASS_NewerVersionExists)
                && InputClass->ClassGeneratedBy == Asset.Get()));
    }
    bool Current()
    {
        Count(R, TEXT("getter_identity_checks"));
        if (!RawCurrent()) { return Bad(TEXT("Captured editor asset/package/class identity is no longer current")); }
        auto* Interface = Cast<IControlRigEditorAssetInterface>(Asset.Get());
        if (!Interface) { return Bad(TEXT("Captured ControlRig editor interface is unavailable")); }
        FRigVMClient* Now = Interface->GetRigVMClient();
        if (!RawCurrent() || Now != Client || !Now) { return Bad(TEXT("RigVM client changed during an editor-interface callback")); }
        Count(R, TEXT("default_model_calls"));
        URigVMGraph* Model = Now->GetDefaultModel();
        if (!RawCurrent() || Model != Graph.Get() || !Loaded(Model) || !SamePackage(Model)
            || !SamePackage(GraphClass) || GraphClass->HasAnyClassFlags(CLASS_NewerVersionExists) || Model->GetClass() != GraphClass
            || Model->GetOuter() != GraphOuter || Model->GetPathName() != GraphPath)
        { return Bad(TEXT("Default RigVM model identity or package changed during observation")); }
        if (bGraphSnapshot)
        {
            const auto& Nodes = Model->GetNodes(); const auto& Links = Model->GetLinks();
            if (Nodes.Num() != NodeCount || Links.Num() != LinkCount) { return Bad(TEXT("RigVM node/link membership count changed")); }
        }
        return true;
    }
    bool SameProjectedSlots()
    {
        if (!Current()) { return false; }
        const auto& Nodes = Graph->GetNodes(); const auto& Links = Graph->GetLinks();
        for (int32 I = 0; I < EventNodes.Num(); ++I) { if (Nodes[I] != EventNodes[I]) { return Bad(TEXT("RigVM node order/identity changed")); } }
        for (int32 I = 0; I < LinkPrefix.Num(); ++I) { if (Links[I] != LinkPrefix[I]) { return Bad(TEXT("RigVM link order/identity changed")); } }
        return true;
    }
    bool Slot(const TCHAR* Counter)
    { Count(R, Counter); if (++Rows > SafetyRows) { return Bad(TEXT("Finite topology projection row budget reached")); } return true; }
    bool Text(const TSharedRef<FJsonObject>& J, const TCHAR* Key, const FString& V)
    {
        if (V.Len() > SafetyStringChars || V.Len() > SafetyTotalStringChars - StringChars)
        { return Bad(TEXT("Finite topology string budget reached; partial projection retained")); }
        StringChars += V.Len(); J->SetStringField(Key, V); return true;
    }
    void Dirty()
    {
        R.Data->SetBoolField(TEXT("asset_dirty_before"), bDirtyBefore);
        if (Loaded(Package.Get()) && Package->GetPathName() == PackagePath)
        {
            const bool After = Package->IsDirty(); R.Data->SetBoolField(TEXT("asset_dirty_after"), After);
            R.Data->SetBoolField(TEXT("observed_dirty_changed"), After != bDirtyBefore);
            R.Data->SetBoolField(TEXT("asset_dirty_unchanged"), After == bDirtyBefore);
        }
        else { R.Data->SetField(TEXT("asset_dirty_after"), MakeShared<FJsonValueNull>()); }
    }
};

bool SafeGraphParents(FRead& C)
{
    TSet<URigVMGraph*> Seen;
    for (URigVMGraph* G = C.Graph.Get(); G; G = G->GetParentGraph())
    {
        if (!Loaded(G) || Seen.Contains(G) || Seen.Num() >= SafetyDepth)
        { return C.Bad(TEXT("Graph parent path is cyclic, unreadable or deeper than the safety bound")); }
        Seen.Add(G); C.Hold(G);
    }
    return true;
}
struct FNodeView
{
    FNodeView(FRead& InC, URigVMNode* InNode) : C(InC), Node(InNode), Outer(InNode->GetOuter()),
        Class(InNode->GetClass()), Path(InNode->GetPathName()), NodeIndex(InC.Graph->GetNodes().Find(InNode)) { C.Hold(Node); C.Hold(Class); }
    FRead& C; URigVMNode* Node; UObject* Outer; UClass* Class; FString Path; int32 NodeIndex;
    TArray<URigVMPin*> Pins; // Requested projection prefix only.
    TArray<URigVMPin*> RootInventory; // SDK HasInput/Output/IO read the entire source list.
    int32 PinCount = 0; bool bPinsCaptured = false;
    bool Check()
    {
        if (!C.Current() || !C.SamePackage(Node) || !C.SamePackage(Class) || Class->HasAnyClassFlags(CLASS_NewerVersionExists)
            || Node->GetClass() != Class || Node->GetOuter() != Outer || Node->GetPathName() != Path
            || Node->GetGraph() != C.Graph.Get() || !C.Graph->GetNodes().IsValidIndex(NodeIndex) || C.Graph->GetNodes()[NodeIndex] != Node)
        { return C.Bad(TEXT("Projected node left its captured live default model")); }
        if (bPinsCaptured)
        {
            // GetPins is virtual. Copy first, then recheck the captured node/model before use.
            const auto& Live = Node->GetPins();
            if (Live.Num() > SafetyRows) { return C.Bad(TEXT("Node pin inventory exceeds the finite source budget")); }
            const TArray<URigVMPin*> Now = Live;
            if (!C.Current() || !C.SamePackage(Node) || !C.SamePackage(Class) || Node->GetOuter() != Outer || Node->GetPathName() != Path
                || Node->GetClass() != Class || Node->GetGraph() != C.Graph.Get()
                || !C.Graph->GetNodes().IsValidIndex(NodeIndex) || C.Graph->GetNodes()[NodeIndex] != Node || Now.Num() != PinCount)
            { return C.Bad(TEXT("Node/pin membership changed during a virtual pin getter")); }
            if (RootInventory.Num() != PinCount) { return C.Bad(TEXT("Complete root-pin source inventory was not captured")); }
            for (int32 I = 0; I < RootInventory.Num(); ++I)
            {
                URigVMPin* Pin = Now[I];
                if (Pin != RootInventory[I] || !Loaded(Pin) || !C.SamePackage(Pin)
                    || Pin->GetOuter() != Node || Pin->GetParentPin())
                { return C.Bad(TEXT("Complete root-pin slot/loaded/owner/actual-package identity changed")); }
            }
        }
        return true;
    }
    bool PinMember(URigVMPin* Pin)
    {
        if (!Check()) { return false; }
        TSet<URigVMPin*> Seen;
        for (URigVMPin* Cursor = Pin; Cursor;)
        {
            if (!Loaded(Cursor) || Seen.Contains(Cursor) || Seen.Num() >= SafetyDepth)
            { return C.Bad(TEXT("Pin parent chain is cyclic, unreadable or deeper than the safety bound")); }
            Seen.Add(Cursor); C.Hold(Cursor); if (!C.SamePackage(Cursor)) { return false; }
            URigVMPin* Parent = Cursor->GetParentPin();
            if (Parent)
            {
                if (!Loaded(Parent) || Cursor->GetOuter() != Parent || !Parent->GetSubPins().Contains(Cursor))
                { return C.Bad(TEXT("Sub-pin no longer belongs to its captured live parent")); }
            }
            else
            { return Cursor->GetOuter() == Node && Pins.Contains(Cursor) ? true : C.Bad(TEXT("Pin no longer belongs to a captured top-level slot")); }
            Cursor = Parent;
        }
        return C.Bad(TEXT("Pin owner chain has no captured node"));
    }
};

// GetDefaultValue itself recursively reads all descendants, even when projection omits subpins.
// Validate that entire closure, retain its UObject lifetimes and recheck after type-refresh callbacks.
bool SafePinTree(FNodeView& V, URigVMPin* Pin, int32 Depth, TSet<URigVMPin*>& Active)
{
    FRead& C = V.C; Count(C.R, TEXT("pin_tree_validation_attempts"));
    if (++C.TreeChecks > SafetyTreeChecks || Depth > SafetyDepth || !Loaded(Pin) || Active.Contains(Pin))
    { return C.Bad(TEXT("Pin default-value closure is cyclic, unreadable or exceeds the finite tree budget")); }
    C.Hold(Pin); Active.Add(Pin); if (!C.SamePackage(Pin)) { return false; }
    if (!V.PinMember(Pin)) { return false; }
    const bool StructPin = Pin->IsStruct(); if (!V.PinMember(Pin)) { return false; }
    if (StructPin)
    { UScriptStruct* Struct = Pin->GetScriptStruct(); C.Hold(Struct); if (!V.PinMember(Pin) || !C.SamePackage(Struct)) { return C.Bad(TEXT("Pin default-value closure has an unreadable script struct")); } }
    const auto& LiveChildren = Pin->GetSubPins();
    if (LiveChildren.Num() > SafetyTreeChecks - C.TreeChecks) { return C.Bad(TEXT("Pin default-value closure exceeds the finite tree budget")); }
    const TArray<URigVMPin*> Children = LiveChildren;
    for (URigVMPin* Child : Children) { C.Hold(Child); }
    if (Children.Num() > SafetyTreeChecks - C.TreeChecks) { return C.Bad(TEXT("Pin default-value closure exceeds the finite tree budget")); }
    for (int32 I = 0; I < Children.Num(); ++I)
    {
        if (!V.PinMember(Pin) || Pin->GetSubPins().Num() != Children.Num() || Pin->GetSubPins()[I] != Children[I])
        { return C.Bad(TEXT("Pin default-value closure changed during type callbacks")); }
        URigVMPin* Child = Children[I];
        if (!Loaded(Child) || Child->GetOuter() != Pin || Child->GetParentPin() != Pin
            || !SafePinTree(V, Child, Depth + 1, Active))
        { return C.Bad(TEXT("Pin default-value closure has invalid live child ownership")); }
    }
    Active.Remove(Pin); return true;
}

// Link path getters recurse through cached endpoint parents. Check those parents first rather than
// calling GetSourceNode/GetPinPath on an unchecked cyclic chain. Lazy endpoint resolution is an SDK effect.
bool SafeLinkEndpoints(FRead& C, URigVMLink* Link)
{
    C.Hold(Link);
    if (!C.Current() || !C.SamePackage(Link) || Link->GetOuter() != C.Graph.Get() || !C.Graph->GetLinks().Contains(Link))
    { return C.Bad(TEXT("Live model link ownership is unavailable")); }
    for (bool bSource : {true, false})
    {
        URigVMPin* Pin = bSource ? Link->GetSourcePin() : Link->GetTargetPin(); C.Hold(Pin);
        if (!C.Current() || !C.SamePackage(Link) || !C.Graph->GetLinks().Contains(Link)) { return false; }
        if (!Pin) { continue; }
        URigVMPin* Root = Pin; TSet<URigVMPin*> Seen;
        while (Root)
        {
            if (!Loaded(Root) || Seen.Contains(Root) || Seen.Num() >= SafetyDepth) { return C.Bad(TEXT("Link endpoint pin chain is unsafe")); }
            Seen.Add(Root); C.Hold(Root); if (!C.SamePackage(Root)) { return false; }
            URigVMPin* Parent = Root->GetParentPin();
            if (!Parent) { break; }
            if (!Loaded(Parent) || Root->GetOuter() != Parent || !Parent->GetSubPins().Contains(Root))
            { return C.Bad(TEXT("Link endpoint parent membership is invalid")); }
            Root = Parent;
        }
        URigVMNode* Node = Root ? Cast<URigVMNode>(Root->GetOuter()) : nullptr;
        if (!Loaded(Node)) { return C.Bad(TEXT("Link endpoint node is unreadable")); }
        FNodeView V(C, Node); if (!V.Check()) { return false; }
        const auto& LivePins = Node->GetPins();
        if (LivePins.Num() > SafetyRows) { return C.Bad(TEXT("Link endpoint pin inventory exceeds the finite budget")); }
        const TArray<URigVMPin*> Pins = LivePins;
        if (!V.Check()) { return false; }
        V.PinCount = Pins.Num(); V.Pins = Pins; V.RootInventory = Pins;
        for (URigVMPin* RootPin : Pins) { C.Hold(RootPin); }
        V.bPinsCaptured = true;
        if (!V.PinMember(Pin)) { return false; }
    }
    return true;
}

bool PinJson(FNodeView& V, URigVMPin* Pin, int32 Index, bool bExtended, bool bPinLinks,
    int32 SubLimit, int32 Depth, const TSharedRef<FJsonObject>& J)
{
    FRead& C = V.C; J->SetNumberField(TEXT("index"), Index);
    if (!C.Slot(TEXT("pin_projection_attempts"))) { return false; }
    if (!Pin) { J->SetBoolField(TEXT("valid"), false); return true; }
    if (Depth > SafetyDepth || !V.PinMember(Pin)) { return C.Bad(TEXT("Sub-pin projection safety boundary reached")); }
    TSet<URigVMPin*> Active;
    if (!SafePinTree(V, Pin, 0, Active)) { return false; }
    UClass* PinClass = Pin->GetClass(); C.Hold(PinClass);
    auto Current = [&]() { return V.PinMember(Pin) && Loaded(PinClass) && !PinClass->HasAnyClassFlags(CLASS_NewerVersionExists)
        && C.SamePackage(PinClass) && Pin->GetClass() == PinClass ? true : C.Bad(TEXT("Projected pin class changed")); };
    J->SetBoolField(TEXT("valid"), true);
#define PIN_TEXT(Key, Expr) { const FString Value = (Expr); if (!Current() || !C.Text(J, TEXT(Key), Value)) { return false; } }
#define PIN_BOOL(Key, Expr) { const bool Value = (Expr); if (!Current()) { return false; } J->SetBoolField(TEXT(Key), Value); }
    PIN_TEXT("name", Pin->GetName());
    PIN_TEXT("display_name", Pin->GetDisplayName().ToString());
    PIN_TEXT("pin_path", Pin->GetPinPath());
    PIN_TEXT("direction", Direction(Pin->GetDirection()));
    PIN_TEXT("cpp_type", Pin->GetCPPType());
    // Type refresh can invoke SDK callbacks. Validate the closure again immediately before its recursive getter.
    const bool StructPin = Pin->IsStruct(); if (!Current()) { return false; }
    if (StructPin)
    { UScriptStruct* Struct = Pin->GetScriptStruct(); C.Hold(Struct); if (!Current() || !C.SamePackage(Struct)) { return C.Bad(TEXT("Struct pin has no live reflected script struct")); } }
    Active.Reset(); if (!SafePinTree(V, Pin, 0, Active)) { return false; }
    PIN_TEXT("default_value", Pin->GetDefaultValue());
    PIN_BOOL("is_execute_context", Pin->IsExecuteContext());
    if (!bExtended) { return true; }
    PIN_TEXT("sub_pin_path", Pin->GetSubPinPath(nullptr));
    PIN_BOOL("is_array", Pin->IsArray()); PIN_BOOL("is_array_element", Pin->IsArrayElement());
    PIN_BOOL("is_struct", Pin->IsStruct()); PIN_BOOL("is_struct_member", Pin->IsStructMember());
    PIN_BOOL("is_linked", Pin->IsLinked(false)); PIN_BOOL("can_provide_default_value", Pin->CanProvideDefaultValue());
    PIN_BOOL("has_default_value_override", Pin->HasDefaultValueOverride());
    PIN_TEXT("default_value_ui", Pin->GetDefaultValueStoredByUserInterface());
    PIN_TEXT("bound_variable_path", Pin->GetBoundVariablePath());
    UObject* Type = Pin->GetCPPTypeObject(); C.Hold(Type);
    if (!Current()) { return false; }
    if (Type && (!C.SamePackage(Type) || !C.Text(J, TEXT("cpp_type_object"), Type->GetPathName())))
    { return C.Bad(TEXT("Projected pin type object became unreadable")); }
    const TArray<URigVMPin*> Children = Pin->GetSubPins();
    J->SetNumberField(TEXT("sub_pin_count"), Children.Num());
    J->SetBoolField(TEXT("sub_pins_truncated"), Children.Num() > SubLimit);
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (int32 I = 0; I < Children.Num() && I < SubLimit; ++I)
    {
        if (!Current() || Pin->GetSubPins().Num() != Children.Num() || Pin->GetSubPins()[I] != Children[I])
        { return C.Bad(TEXT("Sub-pin slot changed during recursive projection")); }
        auto Child = MakeShared<FJsonObject>(); Rows.Add(MakeShared<FJsonValueObject>(Child)); J->SetArrayField(TEXT("sub_pins"), Rows);
        if (!PinJson(V, Children[I], I, true, bPinLinks, SubLimit, Depth + 1, Child)) { return false; }
    }
    J->SetArrayField(TEXT("sub_pins"), Rows);
    if (bPinLinks)
    {
        const TArray<URigVMLink*> Links = Pin->GetLinks();
        if (Links.Num() > SafetyRows - C.Rows) { return C.Bad(TEXT("Pin-link projection exceeds the finite row budget")); }
        TArray<TSharedPtr<FJsonValue>> Paths; J->SetNumberField(TEXT("link_count"), Links.Num());
        for (int32 I = 0; I < Links.Num(); ++I)
        {
            if (!Current() || Pin->GetLinks().Num() != Links.Num() || Pin->GetLinks()[I] != Links[I])
            { return C.Bad(TEXT("Per-pin link slot changed during observation")); }
            URigVMLink* Link = Links[I]; if (!Link) { continue; } C.Hold(Link);
            if (!Loaded(Link) || !C.Graph->GetLinks().Contains(Link) || !C.Slot(TEXT("link_projection_attempts")))
            { return C.Bad(TEXT("Per-pin link left its captured live default model")); }
            if (!SafeLinkEndpoints(C, Link) || !Current()) { return false; }
            const FString Path = Link->GetPinPathRepresentation();
            if (!Current() || !SafeLinkEndpoints(C, Link) || !C.Text(J, TEXT("_bounded_link_path"), Path))
            { return false; }
            J->RemoveField(TEXT("_bounded_link_path")); Paths.Add(MakeShared<FJsonValueString>(Path)); J->SetArrayField(TEXT("link_paths"), Paths);
        }
        J->SetArrayField(TEXT("link_paths"), Paths);
    }
#undef PIN_TEXT
#undef PIN_BOOL
    return true;
}

bool NodeJson(FRead& C, URigVMNode* Node, int32 Index, bool bPins, bool bSubs, bool bPinLinks,
    int32 PinLimit, int32 SubLimit, const TSharedRef<FJsonObject>& J)
{
    J->SetNumberField(TEXT("index"), Index);
    if (!C.Slot(TEXT("node_projection_attempts"))) { return false; }
    if (!Node) { J->SetBoolField(TEXT("valid"), false); return true; }
    C.Hold(Node); if (!Loaded(Node)) { return C.Bad(TEXT("Projected node is unreadable")); }
    FNodeView V(C, Node); if (!V.Check()) { return false; } J->SetBoolField(TEXT("valid"), true);
#define NODE_TEXT(Key, Expr) { const FString Value = (Expr); if (!V.Check() || !C.Text(J, TEXT(Key), Value)) { return false; } }
#define NODE_BOOL(Key, Expr) { const bool Value = (Expr); if (!V.Check()) { return false; } J->SetBoolField(TEXT(Key), Value); }
    NODE_TEXT("name", Node->GetName()); NODE_TEXT("node_path", Node->GetNodePath());
    NODE_TEXT("node_title", Node->GetNodeTitle()); NODE_TEXT("class", Node->GetClass()->GetName()); NODE_TEXT("class_path", Node->GetClass()->GetPathName());
    const FVector2D Position = Node->GetPosition(); if (!V.Check()) { return false; } J->SetObjectField(TEXT("position"), Vector(Position));
    const FVector2D Size = Node->GetSize(); if (!V.Check()) { return false; } J->SetObjectField(TEXT("size"), Vector(Size));
    NODE_BOOL("is_event", Node->IsEvent()); NODE_TEXT("event_name", Node->GetEventName().ToString());
    NODE_BOOL("can_only_exist_once", Node->CanOnlyExistOnce()); NODE_BOOL("is_mutable", Node->IsMutable()); NODE_BOOL("is_pure", Node->IsPure());
    NODE_BOOL("is_injected", Node->IsInjected()); NODE_BOOL("is_visible_in_ui", Node->IsVisibleInUI());
    // HasInput/OutputPin dereference the whole root-pin list. Validate that source list before those SDK calls.
    const auto& RootPins = Node->GetPins();
    if (RootPins.Num() > SafetyRows) { return C.Bad(TEXT("Node root-pin inventory exceeds the finite safety budget")); }
    const TArray<URigVMPin*> RootSnapshot = RootPins; if (!V.Check()) { return false; }
    V.PinCount = RootSnapshot.Num(); V.RootInventory = RootSnapshot;
    for (int32 I = 0; I < RootSnapshot.Num(); ++I)
    { URigVMPin* Pin = RootSnapshot[I]; C.Hold(Pin);
      if (!Loaded(Pin) || Pin->GetOuter() != Node || Pin->GetParentPin()) { return C.Bad(TEXT("Node root-pin inventory has invalid live ownership")); }
      if (I < PinLimit) { V.Pins.Add(Pin); } }
    V.bPinsCaptured = true;
    NODE_BOOL("has_input_pin", Node->HasInputPin(true)); NODE_BOOL("has_output_pin", Node->HasOutputPin(true));
    NODE_BOOL("has_io_pin", Node->HasIOPin()); NODE_BOOL("has_orphaned_pins", Node->HasOrphanedPins());
    if (URigVMUnitNode* Unit = Cast<URigVMUnitNode>(Node))
    {
        NODE_TEXT("method_name", Unit->GetMethodName().ToString());
        UScriptStruct* Struct = Unit->GetScriptStruct(); C.Hold(Struct);
        if (!V.Check()) { return false; }
        if (Struct)
        { if (!C.SamePackage(Struct) || !C.Text(J, TEXT("script_struct"), Struct->GetName()) || !C.Text(J, TEXT("script_struct_path"), Struct->GetPathName())) { return false; } }
    }
    const auto& LivePins = Node->GetPins();
    if (LivePins.Num() > SafetyRows) { return C.Bad(TEXT("Node pin inventory exceeds the finite source budget")); }
    const TArray<URigVMPin*> Pins = LivePins;
    if (!V.Check()) { return false; }
    J->SetNumberField(TEXT("pin_count"), Pins.Num()); J->SetBoolField(TEXT("pins_truncated"), Pins.Num() > PinLimit);
    if (bPins)
    {
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (int32 I = 0; I < V.Pins.Num(); ++I)
        {
            auto Pin = MakeShared<FJsonObject>(); Rows.Add(MakeShared<FJsonValueObject>(Pin)); J->SetArrayField(TEXT("pins"), Rows);
            if (!PinJson(V, V.Pins[I], I, bSubs, bPinLinks, SubLimit, 0, Pin)) { return false; }
        }
        J->SetArrayField(TEXT("pins"), Rows);
    }
#undef NODE_TEXT
#undef NODE_BOOL
    return V.Check();
}

bool LinkJson(FRead& C, URigVMLink* Link, int32 Index, const TSharedRef<FJsonObject>& J)
{
    J->SetNumberField(TEXT("index"), Index);
    if (!C.Slot(TEXT("link_projection_attempts"))) { return false; }
    if (!Link) { J->SetBoolField(TEXT("valid"), false); return true; }
    C.Hold(Link);
    auto Current = [&]() { return C.Current() && C.SamePackage(Link) && C.Graph->GetLinks().Contains(Link)
        ? true : C.Bad(TEXT("Projected link left its captured live default model")); };
    if (!Current() || !SafeLinkEndpoints(C, Link)) { return false; } J->SetBoolField(TEXT("valid"), true);
    J->SetNumberField(TEXT("link_index"), Link->GetLinkIndex());
#define LINK_TEXT(Key, Expr) { if (!SafeLinkEndpoints(C, Link)) { return false; } const FString Value = (Expr); if (!Current() || !SafeLinkEndpoints(C, Link) || !C.Text(J, TEXT(Key), Value)) { return false; } }
    LINK_TEXT("pin_path_representation", Link->GetPinPathRepresentation());
    LINK_TEXT("source_pin_path", Link->GetSourcePinPath()); LINK_TEXT("target_pin_path", Link->GetTargetPinPath());
    for (bool bSource : {true, false})
    {
        URigVMPin* Pin = bSource ? Link->GetSourcePin() : Link->GetTargetPin(); C.Hold(Pin);
        if (!Current()) { return false; }
        if (Pin)
        {
            if (!Loaded(Pin)) { return C.Bad(TEXT("Link endpoint pin is unreadable")); }
            // Validate parent chain without invoking recursive GetNode/GetPinPath on corrupt data.
            URigVMPin* Root = Pin; TSet<URigVMPin*> Seen;
            while (Root && Root->GetParentPin())
            { if (!Loaded(Root) || Seen.Contains(Root) || Seen.Num() >= SafetyDepth) { return C.Bad(TEXT("Link endpoint pin chain is unsafe")); }
              Seen.Add(Root); C.Hold(Root); Root = Root->GetParentPin(); }
            URigVMNode* Node = Root && Loaded(Root) ? Cast<URigVMNode>(Root->GetOuter()) : nullptr;
            if (!Loaded(Node)) { return C.Bad(TEXT("Link endpoint node is unreadable")); }
            FNodeView V(C, Node); if (!V.Check()) { return false; }
            const auto& LivePins = Node->GetPins();
            if (LivePins.Num() > SafetyRows) { return C.Bad(TEXT("Link endpoint pin inventory exceeds the finite budget")); }
            const TArray<URigVMPin*> Pins = LivePins; if (!V.Check()) { return false; }
            V.PinCount = Pins.Num(); V.Pins = Pins; V.RootInventory = Pins;
            for (URigVMPin* RootPin : Pins) { C.Hold(RootPin); }
            V.bPinsCaptured = true;
            if (!V.PinMember(Pin)) { return false; }
            const FString Name = Pin->GetName(), D = Direction(Pin->GetDirection()), Type = Pin->GetCPPType();
            if (!Current() || !V.PinMember(Pin)) { return false; }
            if (!C.Text(J, bSource ? TEXT("source_pin_name") : TEXT("target_pin_name"), Name)
                || !C.Text(J, bSource ? TEXT("source_pin_direction") : TEXT("target_pin_direction"), D)
                || !C.Text(J, bSource ? TEXT("source_pin_cpp_type") : TEXT("target_pin_cpp_type"), Type)) { return false; }
        }
        URigVMNode* Node = bSource ? Link->GetSourceNode() : Link->GetTargetNode(); C.Hold(Node);
        if (!Current()) { return false; }
        if (Node)
        {
            if (!Loaded(Node)) { return C.Bad(TEXT("Link endpoint node is unreadable")); }
            FNodeView V(C, Node); if (!V.Check()) { return false; }
            const FString Name = Node->GetName(), Path = Node->GetNodePath();
            if (!Current() || !V.Check()) { return false; }
            if (!C.Text(J, bSource ? TEXT("source_node_name") : TEXT("target_node_name"), Name)
                || !C.Text(J, bSource ? TEXT("source_node_path") : TEXT("target_node_path"), Path)) { return false; }
        }
    }
#undef LINK_TEXT
    return Current();
}
}

Variable::FResult Inspect(const FString& ControlRigPath, const FString& ControlRigAssetPath,
    const FString& ControlRigClassPath, bool bIncludePins, bool bIncludeSubPins, bool bIncludeLinks,
    bool bIncludePinLinks, int64 MaxNodes, int64 MaxPinsPerNode, int64 MaxSubPinsPerPin, int64 MaxLinks)
{
    FResult R; Init(R);
    // Preserve received int64 values independently of JSON number/double precision and the original clamp.
    auto Received = MakeShared<FJsonObject>();
    Received->SetStringField(TEXT("max_nodes"), LexToString(MaxNodes));
    Received->SetStringField(TEXT("max_pins_per_node"), LexToString(MaxPinsPerNode));
    Received->SetStringField(TEXT("max_sub_pins_per_pin"), LexToString(MaxSubPinsPerPin));
    Received->SetStringField(TEXT("max_links"), LexToString(MaxLinks));
    R.Data->SetObjectField(TEXT("received_limits_decimal"), Received);
    const int32 NodeLimit = int32(FMath::Clamp<int64>(MaxNodes, 0, 2048));
    const int32 PinLimit = int32(FMath::Clamp<int64>(MaxPinsPerNode, 0, 512));
    const int32 SubLimit = int32(FMath::Clamp<int64>(MaxSubPinsPerPin, 0, 256));
    const int32 LinkLimit = int32(FMath::Clamp<int64>(MaxLinks, 0, 4096));
    R.Data->SetBoolField(TEXT("include_pins"), bIncludePins); R.Data->SetBoolField(TEXT("include_sub_pins"), bIncludeSubPins);
    R.Data->SetBoolField(TEXT("include_links"), bIncludeLinks); R.Data->SetBoolField(TEXT("include_pin_links"), bIncludePinLinks);
    R.Data->SetNumberField(TEXT("max_nodes"), NodeLimit); R.Data->SetNumberField(TEXT("max_pins_per_node"), PinLimit);
    R.Data->SetNumberField(TEXT("max_sub_pins_per_pin"), SubLimit); R.Data->SetNumberField(TEXT("max_links"), LinkLimit);
    R.Data->SetStringField(TEXT("requested_control_rig_path"), ControlRigPath);
    R.Data->SetStringField(TEXT("requested_control_rig_asset_path"), ControlRigAssetPath);
    R.Data->SetStringField(TEXT("requested_control_rig_class_path"), ControlRigClassPath);
    R.Data->SetStringField(TEXT("scope"), TEXT("read-only ControlRig Blueprint default RigVM model topology; no graph mutation, compile, save, or runtime solve"));
    R.Data->SetArrayField(TEXT("nodes"), {}); R.Data->SetArrayField(TEXT("links"), {}); R.Data->SetArrayField(TEXT("event_names"), {});
    if (!IsEditorGameThread() || IsPlaySessionActive() || IsEngineExitRequested() || ReadInFlight)
    { return Fail(R, TEXT("rigvm_topology_editor_unavailable"), TEXT("An idle live editor game thread outside PIE/SIE is required; no load was dispatched")); }
    TGuardValue<bool> Busy(ReadInFlight, true);
    if (!InputPath(ControlRigPath) || !InputPath(ControlRigAssetPath) || !InputPath(ControlRigClassPath))
    { return Fail(R, TEXT("rigvm_topology_path_invalid"), TEXT("Paths must have no embedded NUL, at most1Mi raw characters and a normalized name shorter than NAME_SIZE")); }
    // Preserve IsEmpty-before-trim alias precedence, including whitespace masking the asset alias.
    const FString Path = ControlRigPath.IsEmpty() ? ControlRigAssetPath : ControlRigPath;
    FString Requested, Kind;
    UObject* Object = nullptr;
    R.Stage = TEXT("resolve_control_rig_input");
    if (!Path.TrimStartAndEnd().IsEmpty()) { Requested = Path; R.Stage = TEXT("load_control_rig"); Object = LoadAsset(Path, R); Kind = TEXT("asset"); }
    else if (!ControlRigClassPath.TrimStartAndEnd().IsEmpty())
    { Requested = ControlRigClassPath; R.Stage = TEXT("load_control_rig"); Object = LoadClass(ControlRigClassPath, R); Kind = TEXT("generated_class"); }
    else { return Fail(R, TEXT("rigvm_topology_target_missing"), TEXT("Missing 'control_rig_path', 'control_rig_asset_path', or 'control_rig_class_path' parameter")); }
    R.Data->SetStringField(TEXT("requested_control_rig"), Requested);
    TStrongObjectPtr<UObject> LoadedLifetime(Object);
    if (!Loaded(Object)) { return Fail(R, TEXT("rigvm_topology_target_invalid"), TEXT("ControlRig Blueprint asset or generated class was not found or is not fully loaded")); }
    R.Stage = TEXT("resolve_control_rig");
    UClass* InputClass = Cast<UClass>(Object);
    if (UClass* Class = InputClass)
    {
        if ((!Path.TrimStartAndEnd().IsEmpty() || Class->IsChildOf(UControlRig::StaticClass())) && Loaded(Class->ClassGeneratedBy))
        { Object = Class->ClassGeneratedBy; Kind = TEXT("generated_class"); }
        else { Object = nullptr; }
    }
    if (!Loaded(Object) || (!Cast<UControlRigBlueprint>(Object) && !Cast<UControlRigEditorAsset>(Object)))
    { return Fail(R, TEXT("rigvm_topology_target_invalid"), TEXT("ControlRig class has no ControlRig Blueprint/editor asset graph to inspect, or target is not a ControlRig editor asset")); }
    if (!Loaded(Object->GetPackage())) { return Fail(R, TEXT("rigvm_topology_package_invalid"), TEXT("Resolved editor asset package is unavailable")); }
    FRead C(R, Object);
    C.InputClass.Reset(InputClass);
    R.Data->SetStringField(TEXT("resolution_kind"), Kind); R.Data->SetStringField(TEXT("control_rig_asset_path"), C.AssetPath);
    R.Data->SetStringField(TEXT("resolved_editor_asset_class"), Object->GetClass()->GetPathName()); C.Dirty();
    auto FinishFailure = [&]() { C.Dirty(); return Fail(R, TEXT("rigvm_topology_observation_incomplete"), C.Error.IsEmpty() ? TEXT("Topology observation is incomplete; partial data retained, no retry") : C.Error); };
    if (!C.RawCurrent()) { C.Bad(TEXT("Resolved editor asset changed before model acquisition")); return FinishFailure(); }
    auto* Interface = Cast<IControlRigEditorAssetInterface>(C.Asset.Get());
    if (!Interface) { C.Bad(TEXT("Public ControlRig editor interface unavailable")); return FinishFailure(); }
    C.Client = Interface->GetRigVMClient();
    if (!C.RawCurrent() || !C.Client) { C.Bad(TEXT("Public RigVM client unavailable or asset changed")); return FinishFailure(); }
    R.Stage = TEXT("resolve_default_model"); Count(R, TEXT("default_model_calls"));
    C.Graph.Reset(C.Client->GetDefaultModel());
    if (!C.RawCurrent() || !Loaded(C.Graph.Get())) { C.Bad(TEXT("ControlRig Blueprint has no live default RigVM model")); return FinishFailure(); }
    C.Hold(C.Graph.Get()); C.GraphClass = C.Graph->GetClass(); C.Hold(C.GraphClass);
    C.GraphOuter = C.Graph->GetOuter(); C.GraphPath = C.Graph->GetPathName();
    if (!C.Current() || !SafeGraphParents(C)) { return FinishFailure(); }
    const auto& Nodes = C.Graph->GetNodes(); const auto& Links = C.Graph->GetLinks();
    C.NodeCount = Nodes.Num(); C.LinkCount = Links.Num();
    if (C.NodeCount > SafetyRows || C.LinkCount > SafetyRows)
    { C.Bad(TEXT("Default model source inventory exceeds the finite observation budget")); return FinishFailure(); }
    for (URigVMNode* Node : Nodes) { C.EventNodes.Add(Node); C.Hold(Node); }
    for (int32 I = 0; I < Nodes.Num() && I < NodeLimit; ++I) { C.NodePrefix.Add(Nodes[I]); }
    for (int32 I = 0; I < Links.Num() && I < LinkLimit; ++I) { C.LinkPrefix.Add(Links[I]); C.Hold(Links[I]); }
    C.bGraphSnapshot = true;
    R.Data->SetNumberField(TEXT("node_count"), C.NodeCount); R.Data->SetNumberField(TEXT("link_count"), C.LinkCount);
    R.Data->SetBoolField(TEXT("nodes_truncated"), C.NodeCount > NodeLimit); R.Data->SetBoolField(TEXT("links_truncated"), C.LinkCount > LinkLimit);
    R.Data->SetNumberField(TEXT("included_node_count"), 0); R.Data->SetNumberField(TEXT("included_link_count"), 0);
    R.Stage = TEXT("project_rigvm_topology");
    TArray<TSharedPtr<FJsonValue>> NodeRows, LinkRows;
    for (int32 I = 0; I < C.NodePrefix.Num(); ++I)
    {
        if (!C.Current()) { return FinishFailure(); }
        auto Row = MakeShared<FJsonObject>(); NodeRows.Add(MakeShared<FJsonValueObject>(Row));
        R.Data->SetArrayField(TEXT("nodes"), NodeRows); R.Data->SetNumberField(TEXT("included_node_count"), NodeRows.Num());
        if (!NodeJson(C, C.NodePrefix[I], I, bIncludePins, bIncludeSubPins, bIncludePinLinks, PinLimit, SubLimit, Row)) { return FinishFailure(); }
    }
    if (bIncludeLinks)
    {
        for (int32 I = 0; I < C.LinkPrefix.Num(); ++I)
        {
            if (!C.Current()) { return FinishFailure(); }
            auto Row = MakeShared<FJsonObject>(); LinkRows.Add(MakeShared<FJsonValueObject>(Row));
            R.Data->SetArrayField(TEXT("links"), LinkRows); R.Data->SetNumberField(TEXT("included_link_count"), LinkRows.Num());
            if (!LinkJson(C, C.LinkPrefix[I], I, Row)) { return FinishFailure(); }
        }
    }
    // Match public GetEventNames source (top-level, non-library, node order) without keeping its live
    // Nodes iterator across virtual IsEvent/GetEventName callbacks. This is a source-equivalent projection.
    Count(R, TEXT("event_projection_calls"));
    TArray<TSharedPtr<FJsonValue>> EventRows;
    const bool EventGraph = C.Graph->IsTopLevelGraph(); if (!C.Current()) { return FinishFailure(); }
    if (EventGraph && !C.Graph->IsA<URigVMFunctionLibrary>())
    {
        for (int32 I = 0; I < C.EventNodes.Num(); ++I)
        {
            URigVMNode* Node = C.EventNodes[I];
            if (!Loaded(Node) || !C.Current() || C.Graph->GetNodes()[I] != Node)
            { C.Bad(TEXT("Event node slot is null, unreadable or changed")); return FinishFailure(); }
            FNodeView V(C, Node); if (!V.Check()) { return FinishFailure(); }
            const bool IsEvent = Node->IsEvent(); if (!V.Check()) { return FinishFailure(); }
            if (!IsEvent) { continue; }
            const FString Name = Node->GetEventName().ToString();
            if (!V.Check() || ++C.Rows > SafetyRows || !C.Text(R.Data, TEXT("_bounded_event_name"), Name))
            { C.Bad(TEXT("Event projection became unsafe or exceeded the finite budget")); return FinishFailure(); }
            R.Data->RemoveField(TEXT("_bounded_event_name")); EventRows.Add(MakeShared<FJsonValueString>(Name)); R.Data->SetArrayField(TEXT("event_names"), EventRows);
        }
    }
    R.Stage = TEXT("verify_rigvm_topology");
    if (!C.SameProjectedSlots() || !SafeGraphParents(C)) { return FinishFailure(); }
    Count(R, TEXT("asset_reference_calls"));
    const FControlRigAssetStrongReference Source = Interface->GetControlRigAssetReference();
    UClass* Class = Source.GetBlueprintClass().Get(); C.Hold(Class);
    UObject* RuntimeAsset = Source.GetControlRigAsset().Get(); C.Hold(RuntimeAsset);
    if (!C.Current()) { return FinishFailure(); }
    if (const UControlRigBlueprint* Legacy = Cast<UControlRigBlueprint>(C.Asset.Get()))
    { if (Class != Cast<URigVMBlueprintGeneratedClass>(Legacy->GeneratedClass.Get()))
      { C.Bad(TEXT("Public ControlRig asset reference differs from the original legacy generated-class result")); return FinishFailure(); } }
    // GetControlRigClass is deprecated in UE58. The public asset reference preserves the legacy class only.
    if (Class)
    {
        if (!C.SamePackage(Class) || !Cast<URigVMBlueprintGeneratedClass>(Class)) { C.Bad(TEXT("Legacy ControlRig asset reference has an invalid generated class")); return FinishFailure(); }
        R.Data->SetStringField(TEXT("control_rig_class_path"), Class->GetPathName());
    }
    if (RuntimeAsset)
    { if (!C.SamePackage(RuntimeAsset)) { C.Bad(TEXT("ControlRig runtime asset reference is unreadable")); return FinishFailure(); }
      R.Data->SetStringField(TEXT("control_rig_runtime_asset_path"), RuntimeAsset->GetPathName()); }
    R.Data->SetStringField(TEXT("class_path_source"), TEXT("public_GetControlRigAssetReference_GetBlueprintClass; runtime assets are not represented as classes"));
    const FString GraphName = C.Graph->GetGraphName(); if (!C.Current() || !C.Text(R.Data, TEXT("graph_name"), GraphName)) { return FinishFailure(); }
    const FString NodePath = C.Graph->GetNodePath(); if (!C.Current() || !C.Text(R.Data, TEXT("graph_node_path"), NodePath)) { return FinishFailure(); }
    const bool Root = C.Graph->IsRootGraph(); if (!C.Current()) { return FinishFailure(); } R.Data->SetBoolField(TEXT("is_root_graph"), Root);
    const bool Top = C.Graph->IsTopLevelGraph(); if (!C.Current()) { return FinishFailure(); } R.Data->SetBoolField(TEXT("is_top_level_graph"), Top);
    const int32 Depth = C.Graph->GetGraphDepth(); if (!C.Current()) { return FinishFailure(); } R.Data->SetNumberField(TEXT("graph_depth"), Depth);
    if (!C.SameProjectedSlots()) { return FinishFailure(); }
    // One final non-callback pass catches changes to an earlier projected object's external package.
    for (const auto& Pair : C.ActualPackages) { if (!C.SamePackage(Pair.Key)) { return FinishFailure(); } }
    R.Data->SetBoolField(TEXT("captured_actual_package_identities_verified"), true);
    R.Data->SetNumberField(TEXT("captured_object_count"), C.ActualPackages.Num());
    C.Dirty(); R.bSuccess = true; R.bPostconditionPassed = true; R.Stage = TEXT("rigvm_topology_complete");
    return R;
}
}
