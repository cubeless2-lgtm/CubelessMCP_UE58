// UE56 proposal diagnostics contract, implemented with installed UE58 public APIs.
#include "Compatibility/UE58/CubelessRigVMLinkWriterApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "ControlRig.h"
#include "ControlRigAssetReference.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigEditorAsset.h"
#include "ControlRigRuntimeAsset.h"
#include "Containers/UnrealString.h"
#include "Dom/JsonValue.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "HAL/FileManager.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Misc/CoreMisc.h"
#include "Misc/PackageName.h"
#include "RigVMBlueprintGeneratedClass.h"
#include "RigVMModel/Nodes/RigVMCollapseNode.h"
#include "RigVMModel/Nodes/RigVMUnitNode.h"
#include "RigVMModel/RigVMClient.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMModel/RigVMController.h"
#include "RigVMModel/RigVMSchema.h"
#include "RigVMModel/RigVMLink.h"
#include "RigVMModel/RigVMNode.h"
#include "RigVMModel/RigVMPin.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace Cubeless::UE58::RigVMLinkWriter
{
namespace
{
using Variable::FResult;
constexpr int32 SafetyChars = 1048576, SafetyOutputChars = 8388608, SafetyDepth = 64, SafetyRows = 65536;
constexpr int64 SafetyChecks = 16777216;
bool ReadInFlight = false;

bool Loaded(const UObject* O)
{
    return IsValid(O) && !O->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects
        | RF_BeginDestroyed | RF_FinishDestroyed);
}
void Count(FResult& R, const TCHAR* Key)
{
    const auto C = R.Data->GetObjectField(TEXT("native_call_counts"));
    C->SetNumberField(Key, C->GetNumberField(Key) + 1);
}
void Init(FResult& R)
{
    auto Calls = MakeShared<FJsonObject>();
    for (const TCHAR* Key : {TEXT("asset_load_calls"),TEXT("asset_load_returns"),TEXT("class_load_calls"),TEXT("class_find_calls"),
        TEXT("default_model_calls"),TEXT("asset_reference_calls"),TEXT("find_pin_calls"),TEXT("pin_summary_calls"),
        TEXT("find_link_for_pin_calls"),TEXT("contains_link_calls"),TEXT("can_link_calls"),TEXT("getter_identity_checks"),
        TEXT("get_or_create_controller_calls"),TEXT("get_controller_calls"),TEXT("add_link_calls"),TEXT("add_link_returns_true"),
        TEXT("auto_vm_recompile_requests"),TEXT("mark_package_dirty_calls"),TEXT("request_init_calls"),
        TEXT("explicit_compile_calls"),TEXT("explicit_save_calls"),TEXT("explicit_modify_calls"),TEXT("runtime_execution_calls")})
    { Calls->SetNumberField(Key,0); }
    R.Data->SetObjectField(TEXT("native_call_counts"),Calls);
    R.Data->SetStringField(TEXT("command"),TEXT("add_controlrig_rigvm_links"));
    for (const TCHAR* Key : {TEXT("sample_only"),TEXT("atomic_rollback_unavailable"),TEXT("total_SDK_effects_unknown")}) { R.Data->SetBoolField(Key,true); }
    for (const TCHAR* Key : {TEXT("target_modified"),TEXT("target_asset_modified"),TEXT("partial_target_modified"),TEXT("source_modified"),
        TEXT("cast_node_creation_supported"),TEXT("automatic_action_retry"),TEXT("atomic_snapshot_verified"),TEXT("observation_incomplete"),
        TEXT("packaged_runtime_verified"),TEXT("unique_external_pipeline_complete"),TEXT("target_asset_saved")}) { R.Data->SetBoolField(Key,false); }
    for (const TCHAR* Key : {TEXT("can_link_count"),TEXT("cannot_link_count"),TEXT("missing_pin_count"),TEXT("already_linked_count"),
        TEXT("would_add_count"),TEXT("links_added_count"),TEXT("observed_links_added_count"),TEXT("observed_links_removed_count"),
        TEXT("included_link_count"),TEXT("completed_preflight_count"),TEXT("add_link_attempt_count")}) { R.Data->SetNumberField(Key,0); }
    R.Data->SetArrayField(TEXT("results"),{}); R.Data->SetArrayField(TEXT("completion_errors"),{});
    R.Data->SetStringField(TEXT("save_verification_state"),TEXT("not_attempted"));
    R.Data->SetStringField(TEXT("scope"),TEXT("sample/temp or isolated CubelessMCPTests ControlRig RigVM link writer; no original asset mutation, runtime code, graph replacement, cast node creation, execution, or solve"));
    R.Data->SetStringField(TEXT("change_state_scope"),TEXT("Actual AddLink attempts/returns, observed graph deltas and captured-package dirty are reported separately. Load/PostLoad, controller creation, type/AST/auto-compile callbacks have unknown total effects. There is no atomic rollback or automatic retry."));
}
FResult Fail(FResult R, const TCHAR* Code, const FString& Message)
{
    R.bSuccess = false; R.bPostconditionPassed = false; R.ErrorCode = Code; R.Error = Message;
    R.Data->SetBoolField(TEXT("success"), false); R.Data->SetStringField(TEXT("message"), Message);R.Data->SetStringField(TEXT("error"),Message);
    return R;
}
FString Normalize(const FString& P)
{
    FString N = FPackageName::ExportTextPathToObjectPath(P).TrimStartAndEnd(); N.TrimQuotesInline();
    if ((N.StartsWith(TEXT("/Game/")) || N.StartsWith(TEXT("/Engine/"))) && !N.Contains(TEXT(".")))
    { N += TEXT(".") + FPackageName::GetShortName(N); }
    return N;
}
bool InputPath(const FString& P)
{
    if (P.Len() > SafetyChars) { return false; }
    for (int32 I = 0; I < P.Len(); ++I) { if (P[I] == TCHAR(0)) { return false; } }
    return Normalize(P).Len() < NAME_SIZE;
}
UObject* LoadAsset(const FString& P, FResult& R)
{
    R.bChangeStateKnown = false;
    const FString N = Normalize(P); Count(R, TEXT("asset_load_calls"));
    UObject* O = UEditorAssetLibrary::LoadAsset(N);
    if (O) { Count(R, TEXT("asset_load_returns")); return O; }
    // Original resolver's typed-load fallback, not a retry of an uncertain authoring action.
    Count(R, TEXT("asset_load_calls")); O = StaticLoadObject(UObject::StaticClass(), nullptr, *N);
    if (O) { Count(R, TEXT("asset_load_returns")); } return O;
}
UClass* LoadClass(const FString& P, FResult& R)
{
    R.bChangeStateKnown = false; Count(R, TEXT("class_load_calls"));
    if (UClass* C = LoadObject<UClass>(nullptr, *Normalize(P))) { return C; }
    Count(R, TEXT("class_find_calls")); if (UClass* C = FindFirstObject<UClass>(*P)) { return C; }
    Count(R, TEXT("class_load_calls")); return LoadObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *P));
}
bool Decode(const FString& Text, TSharedPtr<FJsonValue>& Value)
{
    if (Text.Len() > SafetyChars) { return false; }
    int32 Depth = 0; bool Quoted = false, Escaped = false;
    for (int32 I = 0; I < Text.Len(); ++I)
    {
        const TCHAR C = Text[I]; if (C == TCHAR(0)) { return false; }
        if (Quoted)
        {
            if (Escaped) { Escaped = false; }
            else if (C == TCHAR('\\')) { Escaped = true; }
            else if (C == TCHAR('"')) { Quoted = false; }
        }
        else if (C == TCHAR('"')) { Quoted = true; }
        else if (C == TCHAR('[') || C == TCHAR('{')) { if (++Depth > SafetyDepth) { return false; } }
        else if (C == TCHAR(']') || C == TCHAR('}')) { if (--Depth < 0) { return false; } }
    }
    if (Quoted || Depth != 0) { return false; }
    // Installed FJsonSerializer drops standalone scalar roots. An exactly-one array preserves null.
    TArray<TSharedPtr<FJsonValue>> One;
    const auto Reader = TJsonReaderFactory<>::Create(TEXT("[") + Text + TEXT("]"));
    if (!FJsonSerializer::Deserialize(Reader, One, FJsonSerializer::EFlags::None) || One.Num() != 1 || !One[0].IsValid()) { return false; }
    Value = One[0]; return Value->Type == EJson::Null || Value->Type == EJson::Array;
}
FString StringParam(const TSharedPtr<FJsonObject>& J, const TCHAR* Key, const FString& Default = FString())
{
    FString V; return J.IsValid() && J->TryGetStringField(Key, V) ? V : Default;
}
struct FSpec { FString Name, Source, Target; };
bool PinPath(const FString& Path)
{
    int32 Segment = 0, Segments = 1;
    for (int32 I = 0; I < Path.Len(); ++I)
    {
        if (Path[I] == TCHAR(0)) { return false; }
        if (Path[I] == TCHAR('.') || Path[I] == TCHAR('|'))
        { Segment = 0; if (++Segments > SafetyDepth) { return false; } }
        else if (++Segment >= NAME_SIZE) { return false; }
    }
    return true;
}
bool Parse(const TSharedPtr<FJsonValue>& Links, const TSharedPtr<FJsonValue>& Units, int32 Limit,
    TArray<FSpec>& Specs, FString& Field, int32& Requested, bool& Truncated, FString& Error)
{
    const auto& Chosen = Links->Type == EJson::Array ? Links : Units;
    Field = Links->Type == EJson::Array ? TEXT("links") : (Units->Type == EJson::Array ? TEXT("unit_links") : TEXT(""));
    if (Chosen->Type != EJson::Array || (Requested = Chosen->AsArray().Num()) == 0)
    { Error = TEXT("Missing non-empty 'links' array (or 'unit_links' alias)"); return false; }
    Truncated = Requested > Limit;
    for (int32 I = 0; I < Requested && I < Limit; ++I)
    {
        const auto& V = Chosen->AsArray()[I]; FSpec S; S.Name = FString::Printf(TEXT("link_%d"), I);
        if (V.IsValid() && V->Type == EJson::String)
        {
            if (!URigVMLink::SplitPinPathRepresentation(V->AsString().TrimStartAndEnd(), S.Source, S.Target))
            { Error = FString::Printf(TEXT("%s[%d] must be an object or a 'Source -> Target' link path string"), *Field, I); return false; }
        }
        else if (V.IsValid() && V->Type == EJson::Object)
        {
            const auto O = V->AsObject();
            S.Name = StringParam(O, TEXT("name"), S.Name);
            S.Name = StringParam(O, TEXT("label"), S.Name);
            S.Name = StringParam(O, TEXT("id"), S.Name);
            FString Rep = StringParam(O, TEXT("pin_path_representation"));
            if (Rep.IsEmpty()) { Rep = StringParam(O, TEXT("link_path_representation")); }
            if (Rep.IsEmpty()) { Rep = StringParam(O, TEXT("link")); }
            S.Source = StringParam(O, TEXT("source_pin_path"));
            if (S.Source.IsEmpty()) { S.Source = StringParam(O, TEXT("source")); }
            if (S.Source.IsEmpty()) { S.Source = StringParam(O, TEXT("from_pin_path")); }
            if (S.Source.IsEmpty()) { S.Source = StringParam(O, TEXT("from")); }
            S.Target = StringParam(O, TEXT("target_pin_path"));
            if (S.Target.IsEmpty()) { S.Target = StringParam(O, TEXT("target")); }
            if (S.Target.IsEmpty()) { S.Target = StringParam(O, TEXT("to_pin_path")); }
            if (S.Target.IsEmpty()) { S.Target = StringParam(O, TEXT("to")); }
            if ((S.Source.IsEmpty() || S.Target.IsEmpty()) && !Rep.IsEmpty())
            {
                FString A, B;
                if (!URigVMLink::SplitPinPathRepresentation(Rep.TrimStartAndEnd(), A, B))
                { Error = FString::Printf(TEXT("%s[%d].pin_path_representation must be 'Source -> Target'"), *Field, I); return false; }
                if (S.Source.IsEmpty()) { S.Source = A; } if (S.Target.IsEmpty()) { S.Target = B; }
            }
        }
        else { Error = FString::Printf(TEXT("%s[%d] must be an object or string"), *Field, I); return false; }
        S.Name.TrimStartAndEndInline(); S.Source.TrimStartAndEndInline(); S.Target.TrimStartAndEndInline();
        if (S.Source.IsEmpty() || S.Target.IsEmpty())
        { Error = FString::Printf(TEXT("%s[%d] missing source_pin_path/from and target_pin_path/to"), *Field, I); return false; }
        // FindNode/NameEquals construct FNames and FindSubPinView recurses. Bound before SDK dispatch.
        if (!PinPath(S.Source) || !PinPath(S.Target))
        { Error = FString::Printf(TEXT("%s[%d] pin paths require no NUL, <NAME_SIZE per segment and <=64 segments"), *Field, I); return false; }
        Specs.Add(MoveTemp(S));
    }
    return true;
}
FString Direction(ERigVMPinDirection D)
{
    switch (D)
    {
    case ERigVMPinDirection::Input: return TEXT("input"); case ERigVMPinDirection::Output: return TEXT("output");
    case ERigVMPinDirection::IO: return TEXT("io"); case ERigVMPinDirection::Visible: return TEXT("visible");
    case ERigVMPinDirection::Hidden: return TEXT("hidden"); default: return TEXT("invalid");
    }
}

struct FRead
{
    FRead(FResult& InR, UObject* O) : R(InR), Asset(O), Package(O->GetPackage()), AssetClass(O->GetClass()),
        AssetOuter(O->GetOuter()), AssetPath(O->GetPathName()), PackagePath(Package->GetPathName()), bDirtyBefore(Package->IsDirty()) {}
    FResult& R; TStrongObjectPtr<UObject> Asset; TStrongObjectPtr<UPackage> Package;
    TStrongObjectPtr<UClass> AssetClass, InputClass; UObject* AssetOuter; FString AssetPath, PackagePath; bool bDirtyBefore;
    TStrongObjectPtr<URigVMGraph> Graph; UClass* GraphClass = nullptr; UObject* GraphOuter = nullptr; FString GraphPath;
    FRigVMClient* Client = nullptr; FString Error; int32 StringChars = 0;
    TSet<UObject*> Held; TArray<TStrongObjectPtr<UObject>> Lifetimes;
    TMap<UObject*, TPair<UPackage*, FString>> Packages;
    bool Bad(const FString& E)
    { if (Error.IsEmpty()) { Error = E; } R.Data->SetBoolField(TEXT("observation_incomplete"), true); return false; }
    void Hold(UObject* O)
    {
        if (O && !Held.Contains(O))
        {
            Held.Add(O); Lifetimes.Emplace(O); UPackage* P = O->GetPackage();
            Packages.Emplace(O, TPair<UPackage*, FString>(P, P ? P->GetPathName() : FString()));
            if (P && P != O) { Hold(P); }
        }
    }
    bool SamePackage(UObject* O)
    {
        const auto* P = Packages.Find(O);
        return P && Loaded(O) && Loaded(P->Key) && O->GetPackage() == P->Key && P->Key->GetPathName() == P->Value
            ? true : Bad(TEXT("Captured object's actual package identity changed"));
    }
    bool RawCurrent()
    {
        return IsEditorGameThread() && !IsPlaySessionActive() && !IsEngineExitRequested()
            && Loaded(Asset.Get()) && Loaded(Package.Get()) && Loaded(AssetClass.Get())
            && !AssetClass->HasAnyClassFlags(CLASS_NewerVersionExists)
            && Asset->GetClass() == AssetClass.Get() && Asset->GetOuter() == AssetOuter && Asset->GetPathName() == AssetPath
            && Asset->GetPackage() == Package.Get() && Package->GetPathName() == PackagePath
            && (!InputClass.IsValid() || (Loaded(InputClass.Get()) && !InputClass->HasAnyClassFlags(CLASS_NewerVersionExists)
                && InputClass->ClassGeneratedBy == Asset.Get()));
    }
    bool Current()
    {
        Count(R, TEXT("getter_identity_checks"));
        if (!RawCurrent()) { return Bad(TEXT("Captured editor asset/package/class is no longer current")); }
        auto* I = Cast<IControlRigEditorAssetInterface>(Asset.Get());
        if (!I) { return Bad(TEXT("Captured ControlRig editor interface is unavailable")); }
        FRigVMClient* Now = I->GetRigVMClient();
        if (!RawCurrent() || !Now || Now != Client) { return Bad(TEXT("RigVM client changed during interface callback")); }
        Count(R, TEXT("default_model_calls")); URigVMGraph* Model = Now->GetDefaultModel();
        if (!RawCurrent() || Model != Graph.Get() || !Loaded(Model) || !SamePackage(Model) || !SamePackage(GraphClass)
            || GraphClass->HasAnyClassFlags(CLASS_NewerVersionExists) || Model->GetClass() != GraphClass
            || Model->GetOuter() != GraphOuter || Model->GetPathName() != GraphPath)
        { return Bad(TEXT("Default RigVM model identity/package changed during observation")); }
        return true;
    }
    bool Text(const TSharedRef<FJsonObject>& J, const FString& Key, const FString& V)
    {
        if (V.Len() > SafetyChars || V.Len() > SafetyOutputChars - StringChars)
        { return Bad(TEXT("Finite diagnostic string budget reached; partial result retained")); }
        StringChars += V.Len(); J->SetStringField(Key, V); return true;
    }
    void Dirty()
    {
        R.Data->SetBoolField(TEXT("target_dirty_before"), bDirtyBefore);
        R.Data->SetStringField(TEXT("captured_package_path"), PackagePath);
        R.Data->SetStringField(TEXT("dirty_observation_scope"), TEXT("Original resolved package captured after load; this does not verify current asset package dirty if identity is lost"));
        if (Loaded(Package.Get()) && Package->GetPathName() == PackagePath)
        {
            const bool After = Package->IsDirty(); R.Data->SetBoolField(TEXT("target_dirty_after"), After);
            R.Data->SetBoolField(TEXT("target_dirty_unchanged"), bDirtyBefore == After);
            R.Data->SetBoolField(TEXT("observed_target_dirty_changed"), bDirtyBefore != After); R.bChanged |= bDirtyBefore != After;
        }
        else
        {
            R.Data->SetField(TEXT("target_dirty_after"), MakeShared<FJsonValueNull>());
            R.Data->SetField(TEXT("target_dirty_unchanged"), MakeShared<FJsonValueNull>());
            R.Data->SetBoolField(TEXT("source_modified"), false);
        }
    }
};

// Capture the full source lists used by recursive SDK lookup/default/binding code, not just proposals.
// These checks bound our traversal and detect observed callback invalidation. They cannot make the
// internals of SDK import/default/GetPins callbacks atomic or prove all SDK side effects absent.
struct FInventory
{
    explicit FInventory(FRead& InC) : C(InC) {}
    struct FObject { UObject* O; UClass* Class; UObject* Outer; FString Path; };
    struct FGraph { URigVMGraph* Graph; URigVMGraph* Parent; TArray<URigVMNode*> Nodes; TArray<URigVMLink*> Links; };
    struct FNode { URigVMNode* Node; URigVMGraph* Graph; TArray<URigVMPin*> Roots, Orphans; URigVMGraph* Contained; };
    struct FPin { URigVMPin* Pin; URigVMNode* Node; URigVMPin* Parent; TArray<URigVMPin*> Children; TArray<URigVMLink*> Links; TArray<URigVMInjectionInfo*> Injections; };
    struct FLink { URigVMLink* Link; URigVMGraph* Graph; URigVMPin* Source; URigVMPin* Target; };
    struct FInjection { URigVMInjectionInfo* Info; URigVMPin* OwnerPin; URigVMNode* Node; URigVMPin* Input; URigVMPin* Output; bool bAsInput; };
    FRead& C; TArray<FObject> Objects; TArray<FGraph> Graphs; TArray<FNode> Nodes; TArray<FPin> Pins; TArray<FLink> Links;
    TArray<FInjection> Injections; TMap<URigVMNode*, URigVMGraph*> NodeGraphs;
    TSet<UObject*> SeenObjects; TMap<UObject*, int32> ObjectIndices;
    TSet<URigVMGraph*> SeenGraphs; TSet<URigVMPin*> SeenPins; TMap<URigVMPin*, URigVMGraph*> PinGraphs;
    int32 Rows = 0; int64 Checks = 0;
    bool Step()
    { return ++Checks <= SafetyChecks ? true : C.Bad(TEXT("Finite diagnostic identity-check budget reached")); }
    bool Object(UObject* O)
    {
        if (!Loaded(O) || !Loaded(O->GetClass()) || O->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists))
        { return C.Bad(TEXT("Source inventory contains an unreadable/reinstanced object")); }
        if (!SeenObjects.Contains(O))
        {
            if (++Rows > SafetyRows) { return C.Bad(TEXT("Finite diagnostic source inventory budget reached")); }
            SeenObjects.Add(O); C.Hold(O); C.Hold(O->GetClass());
            ObjectIndices.Add(O, Objects.Add({O, O->GetClass(), O->GetOuter(), O->GetPathName()}));
        }
        return C.SamePackage(O) && C.SamePackage(O->GetClass());
    }
    bool Identity(UObject* O)
    {
        const int32* Index = ObjectIndices.Find(O);
        if (!Index || !Step()) { return C.Bad(TEXT("Source object has no finite identity capture")); }
        const FObject& V = Objects[*Index];
        return C.SamePackage(V.O) && C.SamePackage(V.Class) && !V.Class->HasAnyClassFlags(CLASS_NewerVersionExists)
            && V.O->GetClass() == V.Class && V.O->GetOuter() == V.Outer && V.O->GetPathName() == V.Path
            ? true : C.Bad(TEXT("Captured source object/class/outer/path/package identity changed"));
    }
    bool Identities()
    {
        if (!C.RawCurrent()) { return C.Bad(TEXT("Editor/asset identity changed during diagnostic callback")); }
        for (const FObject& V : Objects)
        {
            if (!Identity(V.O)) { return false; }
        }
        return true;
    }
    // Node.GetGraph follows InjectionInfo->owning pin->owning node recursively. Bound this
    // ownership path iteratively before any SDK default/binding getter can follow it.
    bool OwnerGraph(URigVMNode* Node, URigVMGraph*& Out)
    {
        Out = nullptr; TSet<UObject*> Seen; int32 Depth = 0;
        for (URigVMNode* Cursor = Node; Cursor;)
        {
            if (!Step() || ++Depth > SafetyDepth || Seen.Contains(Cursor) || !Object(Cursor))
            { return C.Bad(TEXT("Injected node ownership is cyclic, unreadable or deeper than64")); }
            Seen.Add(Cursor); UObject* Outer = Cursor->GetOuter();
            if (URigVMGraph* Graph = Cast<URigVMGraph>(Outer)) { Out = Graph; return Object(Graph); }
            URigVMInjectionInfo* Info = Cast<URigVMInjectionInfo>(Outer);
            if (!Info || !Object(Info) || Info->Node.Get() != Cursor)
            { return C.Bad(TEXT("Injected node has no exact live injection owner")); }
            URigVMPin* Pin = Cast<URigVMPin>(Info->GetOuter());
            if (!Pin || !Object(Pin) || !Pin->GetInjectedNodes().Contains(Info))
            { return C.Bad(TEXT("Injection owner has no live owning pin/inventory association")); }
            for (;;)
            {
                if (!Step() || ++Depth > SafetyDepth || Seen.Contains(Pin) || !Object(Pin))
                { return C.Bad(TEXT("Injected pin owner chain is cyclic or beyond the finite depth")); }
                Seen.Add(Pin); URigVMPin* Parent = Pin->GetParentPin();
                if (!Parent) { break; }
                if (!Object(Parent) || !Parent->GetSubPins().Contains(Pin))
                { return C.Bad(TEXT("Injected pin owner chain lost its parent membership")); }
                Pin = Parent;
            }
            Cursor = Cast<URigVMNode>(Pin->GetOuter());
            if (!Cursor) { return C.Bad(TEXT("Injection owner chain has no root node")); }
        }
        return false;
    }
    bool GraphParent(URigVMGraph* G, URigVMGraph*& Out)
    {
        Out = nullptr;
        if (!Object(G)) { return false; }
        if (URigVMNode* Node = Cast<URigVMNode>(G->GetOuter())) { return OwnerGraph(Node, Out); }
        return true;
    }
    bool RawShapes()
    {
        if (!Identities()) { return false; }
        for (const FGraph& V : Graphs)
        {
            URigVMGraph* Parent = nullptr;
            if (!Step() || !GraphParent(V.Graph, Parent) || Parent != V.Parent || V.Graph->GetNodes() != V.Nodes || V.Graph->GetLinks() != V.Links)
            { return C.Bad(TEXT("Captured graph source slots or parent changed")); }
        }
        for (const FPin& V : Pins)
        {
            if (!Step() || V.Pin->GetParentPin() != V.Parent || V.Pin->GetSubPins() != V.Children || V.Pin->GetLinks() != V.Links
                || V.Pin->GetInjectedNodes() != V.Injections
                || V.Pin->GetOuter() != (V.Parent ? static_cast<UObject*>(V.Parent) : static_cast<UObject*>(V.Node)))
            { return C.Bad(TEXT("Captured pin parent/child/link slots changed")); }
        }
        for (const FInjection& V : Injections)
        {
            if (!Step() || !Identity(V.Info) || V.Info->GetOuter() != V.OwnerPin || V.Info->Node.Get() != V.Node
                || V.Info->InputPin.Get() != V.Input || V.Info->OutputPin.Get() != V.Output || V.Info->bInjectedAsInput != V.bAsInput)
            { return C.Bad(TEXT("Captured injected-node/input/output association changed")); }
        }
        return true;
    }
    bool Parents(URigVMGraph* Graph)
    {
        TSet<URigVMGraph*> Seen;
        for (URigVMGraph* G = Graph; G;)
        {
            if (!Step() || Seen.Contains(G) || Seen.Num() >= SafetyDepth || !Object(G))
            { return C.Bad(TEXT("Graph parent chain is unreadable, cyclic or beyond the finite depth")); }
            Seen.Add(G);
            URigVMGraph* Parent = nullptr; if (!GraphParent(G, Parent)) { return false; } G = Parent;
        }
        return true;
    }
    bool Pin(URigVMPin* P, URigVMNode* N, URigVMPin* Parent, int32 Depth)
    {
        if (!Step() || Depth > SafetyDepth || SeenPins.Contains(P) || !Object(P)
            || P->GetParentPin() != Parent || P->GetOuter() != (Parent ? static_cast<UObject*>(Parent) : static_cast<UObject*>(N)))
        { return C.Bad(TEXT("Pin tree is cyclic, duplicated, unreadable or has invalid live ownership")); }
        SeenPins.Add(P); PinGraphs.Add(P, NodeGraphs.FindRef(N));
        const auto& Children = P->GetSubPins(); const auto& PinLinks = P->GetLinks();
        if (Children.Num() > SafetyRows - Rows || PinLinks.Num() > SafetyRows)
        { return C.Bad(TEXT("Pin child/link source list exceeds the finite observation budget")); }
        const auto Injected = P->GetInjectedNodes();
        if (Injected.Num() > SafetyRows - Rows) { return C.Bad(TEXT("Injection source list exceeds the finite observation budget")); }
        FPin V{P, N, Parent, Children, PinLinks, Injected}; Pins.Add(V);
        for (URigVMInjectionInfo* Info : Injected)
        {
            if (!Object(Info) || Info->GetOuter() != P) { return C.Bad(TEXT("Injection source has invalid live pin ownership")); }
            URigVMNode* InjectedNode = Info->Node.Get(); URigVMPin* Input = Info->InputPin.Get(); URigVMPin* Output = Info->OutputPin.Get();
            if ((InjectedNode && !Object(InjectedNode)) || (Input && !Object(Input)) || (Output && !Object(Output))) { return false; }
            Injections.Add({Info, P, InjectedNode, Input, Output, Info->bInjectedAsInput});
        }
        for (URigVMLink* L : V.Links) { if (!Object(L)) { return false; } }
        for (URigVMPin* Child : V.Children) { if (!Pin(Child, N, P, Depth + 1)) { return false; } }
        return true;
    }
    bool Graph(URigVMGraph* G, int32 Depth)
    {
        if (!Step() || Depth > SafetyDepth || SeenGraphs.Contains(G) || !Object(G) || !Parents(G))
        { return C.Bad(TEXT("Contained graph closure is cyclic, duplicated or exceeds the finite depth")); }
        SeenGraphs.Add(G);
        if (G->GetNodes().Num() > SafetyRows - Rows || G->GetLinks().Num() > SafetyRows - Rows)
        { return C.Bad(TEXT("Graph node/link source list exceeds the finite observation budget")); }
        URigVMGraph* Parent = nullptr; if (!GraphParent(G, Parent)) { return false; }
        const FGraph V{G, Parent, G->GetNodes(), G->GetLinks()}; Graphs.Add(V);
        for (URigVMLink* L : V.Links)
        { if (!Object(L) || L->GetOuter() != G) { return C.Bad(TEXT("Graph link source ownership is invalid")); } }
        for (URigVMNode* N : V.Nodes)
        {
            URigVMGraph* Owner = nullptr;
            if (!Object(N) || !OwnerGraph(N, Owner) || Owner != G || !C.Current() || !RawShapes()) { return C.Bad(TEXT("Graph node ownership changed")); }
            NodeGraphs.Add(N, G);
            const auto& LiveRoots = N->GetPins();
            if (LiveRoots.Num() > SafetyRows - Rows) { return C.Bad(TEXT("Root pin inventory exceeds the finite budget")); }
            const TArray<URigVMPin*> Roots = LiveRoots;
            if (!C.Current() || !RawShapes()) { return false; }
            const auto& LiveOrphans = N->GetOrphanedPins();
            if (LiveOrphans.Num() > SafetyRows - Rows) { return C.Bad(TEXT("Orphan pin inventory exceeds the finite budget")); }
            const TArray<URigVMPin*> Orphans = LiveOrphans;
            if (!C.Current() || !RawShapes()) { return false; }
            // Node::FindPin searches the complete root and orphan lists, even if the requested limit is zero elsewhere.
            for (URigVMPin* P : Roots) { if (!Pin(P, N, nullptr, 0)) { return false; } }
            for (URigVMPin* P : Orphans) { if (!Pin(P, N, nullptr, 0)) { return false; } }
            URigVMGraph* Contained = nullptr;
            if (URigVMCollapseNode* Collapse = Cast<URigVMCollapseNode>(N))
            {
                Contained = Collapse->GetContainedGraph();
                if (!C.Current() || !RawShapes()) { return false; }
                if (Contained && !Graph(Contained, Depth + 1)) { return false; }
            }
            Nodes.Add({N, G, Roots, Orphans, Contained});
        }
        return RawShapes();
    }
    bool Check(bool bEndpoints = true)
    {
        if (!Step() || !C.Current() || !RawShapes()) { return false; }
        for (const FNode& V : Nodes)
        {
            auto NodeCurrent = [&]() { URigVMGraph* Owner = nullptr;
                return Identity(V.Node) && Identity(V.Graph) && OwnerGraph(V.Node, Owner)
                && Owner == V.Graph && V.Graph->GetNodes().Contains(V.Node); };
            if (!Step() || !NodeCurrent()) { return C.Bad(TEXT("Captured node left its model")); }
            const auto& LiveRoots = V.Node->GetPins();
            if (LiveRoots.Num() > SafetyRows) { return C.Bad(TEXT("Root pin source changed beyond the finite bound")); }
            const TArray<URigVMPin*> Roots = LiveRoots;
            if (!C.Current() || !NodeCurrent() || Roots != V.Roots) { return C.Bad(TEXT("Complete root pin slot inventory changed after callback")); }
            for (URigVMPin* P : Roots)
            { if (!Identity(P) || P->GetOuter() != V.Node || P->GetParentPin()) { return C.Bad(TEXT("Root pin loaded/owner/package identity changed")); } }
            const auto& LiveOrphans = V.Node->GetOrphanedPins();
            if (LiveOrphans.Num() > SafetyRows) { return C.Bad(TEXT("Orphan pin source changed beyond the finite bound")); }
            const TArray<URigVMPin*> Orphans = LiveOrphans;
            if (!C.Current() || !NodeCurrent() || Orphans != V.Orphans) { return C.Bad(TEXT("Complete orphan pin inventory changed after callback")); }
            for (URigVMPin* P : Orphans)
            { if (!Identity(P) || P->GetOuter() != V.Node || P->GetParentPin()) { return C.Bad(TEXT("Orphan pin loaded/owner/package identity changed")); } }
            if (URigVMCollapseNode* Collapse = Cast<URigVMCollapseNode>(V.Node))
            {
                URigVMGraph* Now = Collapse->GetContainedGraph();
                if (!C.Current() || !NodeCurrent() || Now != V.Contained) { return C.Bad(TEXT("Contained graph identity changed after callback")); }
            }
        }
        if (bEndpoints)
        {
            for (const FLink& V : Links)
            {
                if (!Step() || !RawShapes()) { return false; }
                URigVMPin* Source = V.Link->GetSourcePin();
                if (!C.Current() || !RawShapes() || Source != V.Source) { return C.Bad(TEXT("Live link source endpoint changed")); }
                URigVMPin* Target = V.Link->GetTargetPin();
                if (!C.Current() || !RawShapes() || Target != V.Target) { return C.Bad(TEXT("Live link target endpoint changed")); }
            }
        }
        return RawShapes();
    }
    bool Capture()
    {
        if (!Graph(C.Graph.Get(), 0) || !Check(false)) { return false; }
        for (const FGraph& G : Graphs)
        {
            for (URigVMLink* L : G.Links)
            {
                if (!Check(false)) { return false; }
                URigVMPin* S = L->GetSourcePin();
                if (!Check(false) || !SeenPins.Contains(S) || PinGraphs.FindRef(S) != G.Graph)
                { return C.Bad(TEXT("Graph link source is not a captured pin of that live graph")); }
                URigVMPin* T = L->GetTargetPin();
                if (!Check(false) || !SeenPins.Contains(T) || PinGraphs.FindRef(T) != G.Graph)
                { return C.Bad(TEXT("Graph link target is not a captured pin of that live graph")); }
                Links.Add({L, G.Graph, S, T});
            }
        }
        for (const FPin& P : Pins)
        {
            for (URigVMLink* L : P.Links)
            {
                const FLink* V = Links.FindByPredicate([&](const FLink& V) { return V.Link == L; });
                if (!V || (V->Source != P.Pin && V->Target != P.Pin)) { return C.Bad(TEXT("Pin link reference is outside its captured graph/endpoints")); }
            }
        }
        for (const FInjection& V : Injections)
        {
            if ((V.Node && !NodeGraphs.Contains(V.Node)) || (V.Input && !SeenPins.Contains(V.Input))
                || (V.Output && !SeenPins.Contains(V.Output)))
            { return C.Bad(TEXT("Injected node/input/output is outside the captured graph source inventory")); }
        }
        C.R.Data->SetNumberField(TEXT("captured_source_object_count"), Objects.Num());
        C.R.Data->SetNumberField(TEXT("captured_source_graph_count"), Graphs.Num());
        return Check();
    }
    bool Member(URigVMPin* P)
    { return P && SeenPins.Contains(P) && Check() ? true : C.Bad(TEXT("Resolved pin is outside the captured live model closure")); }
};

// Graph::CanLink calls a recursive node-edge cycle walk. Mirror its exact source-list,
// visited and early-target order with an iterative stack before entering that SDK recursion.
// The bound concerns the captured graph; it does not make a custom virtual callback atomic.
bool SafeNodeEdgeDepth(FInventory& I, URigVMPin* Source, URigVMPin* Target)
{
    FRead& C = I.C; double Calls = 0;
    C.R.Data->TryGetNumberField(TEXT("sdk_node_edge_depth_preflight_calls"), Calls);
    C.R.Data->SetNumberField(TEXT("sdk_node_edge_depth_preflight_calls"), Calls + 1);
    C.R.Data->SetBoolField(TEXT("sdk_node_edge_depth_guard_passed"), false);
    if (!I.Check()) { return false; }
    TMap<URigVMPin*, const FInventory::FPin*> PinRows;
    TMap<URigVMLink*, const FInventory::FLink*> LinkRows;
    for (const auto& P : I.Pins) { PinRows.Add(P.Pin, &P); }
    for (const auto& L : I.Links) { LinkRows.Add(L.Link, &L); }
    const auto* const* SourceRow = PinRows.Find(Source);
    const auto* const* TargetRow = PinRows.Find(Target);
    if (!SourceRow || !TargetRow) { return C.Bad(TEXT("CanLink endpoints have no captured node ownership")); }
    URigVMNode* SourceNode = (*SourceRow)->Node; URigVMNode* TargetNode = (*TargetRow)->Node;
    // URigVMPin::CanLink returns its ordinary same-node diagnostic before Graph's cycle check.
    if (SourceNode == TargetNode)
    { C.R.Data->SetBoolField(TEXT("sdk_node_edge_depth_guard_passed"), true); return true; }
    TMap<URigVMNode*, TArray<URigVMNode*>> Upstream, Downstream;
    int32 EdgeSlots = 0;
    for (const auto& N : I.Nodes)
    {
        TArray<URigVMPin*> Work;
        for (int32 J = N.Roots.Num() - 1; J >= 0; --J) { Work.Add(N.Roots[J]); }
        while (!Work.IsEmpty())
        {
            if (!I.Step()) { return false; }
            URigVMPin* P = Work.Pop(EAllowShrinking::No);
            const auto* const* Row = PinRows.Find(P);
            if (!Row || (*Row)->Node != N.Node) { return C.Bad(TEXT("Cycle preflight pin left its captured node")); }
            // Node::GetLinks appends this pin's link slots, then recurses through subpins.
            // Preserve duplicates and order; orphan pins are not part of GetLinks' source list.
            for (URigVMLink* L : (*Row)->Links)
            {
                if (!I.Step() || ++EdgeSlots > SafetyRows) { return C.Bad(TEXT("Finite SDK node-edge source-slot budget reached before CanLink")); }
                const auto* const* Link = LinkRows.Find(L);
                if (!Link) { return C.Bad(TEXT("Cycle preflight link is outside the captured live inventory")); }
                const auto* const* S = PinRows.Find((*Link)->Source);
                const auto* const* T = PinRows.Find((*Link)->Target);
                if (!S || !T) { return C.Bad(TEXT("Cycle preflight link endpoint ownership is unavailable")); }
                Upstream.FindOrAdd(N.Node).Add((*S)->Node);
                Downstream.FindOrAdd(N.Node).Add((*T)->Node);
            }
            for (int32 J = (*Row)->Children.Num() - 1; J >= 0; --J) { Work.Add((*Row)->Children[J]); }
        }
    }
    int32 MaxDepth = 0;
    auto Trace = [&](URigVMNode* Start, URigVMNode* End, const TMap<URigVMNode*, TArray<URigVMNode*>>& Adjacency, bool& bReachedTarget)
    {
        bReachedTarget = false;
        struct FFrame { URigVMNode* Node; int32 Next = 0; };
        TArray<FFrame> Stack; Stack.Add({Start, 0}); TSet<URigVMNode*> Visited;
        // SDK does not initially mark Start visited and checks End before inserting it.
        while (!Stack.IsEmpty())
        {
            if (!I.Step()) { return false; }
            MaxDepth = FMath::Max(MaxDepth, Stack.Num());
            FFrame& Frame = Stack.Last(); const auto* Edges = Adjacency.Find(Frame.Node);
            if (!Edges || Frame.Next >= Edges->Num()) { Stack.Pop(EAllowShrinking::No); continue; }
            URigVMNode* Next = (*Edges)[Frame.Next++];
            if (Next == Frame.Node || Visited.Contains(Next)) { continue; }
            if (Next == End) { bReachedTarget = true; return true; }
            Visited.Add(Next);
            if (Stack.Num() >= SafetyDepth)
            { return C.Bad(TEXT("SDK CanLink node-edge recursion would exceed captured depth64; no CanLink query dispatched for this row")); }
            Stack.Add({Next, 0});
        }
        return true;
    };
    bool ReachedTarget = false;
    const bool Safe = Trace(SourceNode, TargetNode, Upstream, ReachedTarget)
        && (ReachedTarget || Trace(TargetNode, SourceNode, Downstream, ReachedTarget));
    C.R.Data->SetNumberField(TEXT("sdk_node_edge_depth_observed"), MaxDepth);
    C.R.Data->SetNumberField(TEXT("sdk_node_edge_source_slots"), EdgeSlots);
    C.R.Data->SetNumberField(TEXT("sdk_node_edge_depth_limit"), SafetyDepth);
    if (!Safe || !I.Check()) { return false; }
    C.R.Data->SetBoolField(TEXT("sdk_node_edge_depth_guard_passed"), true); return true;
}

bool Summary(FInventory& I, URigVMPin* P, const FString& Prefix, const TSharedRef<FJsonObject>& J)
{
    FRead& C = I.C; J->SetBoolField(Prefix + TEXT("_pin_found"), P != nullptr);
    if (!P) { return true; }
    Count(C.R, TEXT("pin_summary_calls")); if (!I.Member(P)) { return false; }
#define SUMMARY_TEXT(Suffix, Expr) { const FString V = (Expr); if (!I.Member(P) || !C.Text(J, Prefix + TEXT(Suffix), V)) { return false; } }
    SUMMARY_TEXT("_pin_name", P->GetName());
    SUMMARY_TEXT("_pin_direction", Direction(P->GetDirection()));
    SUMMARY_TEXT("_pin_cpp_type", P->GetCPPType());
    const bool Execute = P->IsExecuteContext(); if (!I.Member(P)) { return false; }
    J->SetBoolField(Prefix + TEXT("_pin_is_execute_context"), Execute);
    SUMMARY_TEXT("_pin_path_resolved", P->GetPinPath());
    URigVMNode* N = P->GetNode(); if (!I.Member(P)) { return false; }
    if (N)
    {
        SUMMARY_TEXT("_node_name", N->GetName()); SUMMARY_TEXT("_node_path", N->GetNodePath());
        SUMMARY_TEXT("_node_title", N->GetNodeTitle());
    }
#undef SUMMARY_TEXT
    return true;
}
bool Diagnostic(FInventory& I, const FSpec& S, int32 Index, bool TypeCasting, const TSharedRef<FJsonObject>& J)
{
    FRead& C = I.C; J->SetNumberField(TEXT("index"), Index);
    if (!C.Text(J, TEXT("name"), S.Name) || !C.Text(J, TEXT("source_pin_path"), S.Source)
        || !C.Text(J, TEXT("target_pin_path"), S.Target)) { return false; }
    const FString Rep = URigVMLink::GetPinPathRepresentation(S.Source, S.Target);
    if (!C.Text(J, TEXT("link_path_representation"), Rep)) { return false; }
    J->SetBoolField(TEXT("enable_type_casting"), TypeCasting);
    if (!I.Check()) { return false; }
    Count(C.R, TEXT("find_pin_calls")); URigVMPin* Source = C.Graph->FindPin(S.Source);
    if (!I.Check() || (Source && !I.Member(Source))) { return false; }
    Count(C.R, TEXT("find_pin_calls")); URigVMPin* Target = C.Graph->FindPin(S.Target);
    if (!I.Check() || (Target && !I.Member(Target))) { return false; }
    if (!Summary(I, Source, TEXT("source"), J) || !Summary(I, Target, TEXT("target"), J)) { return false; }
    const bool Found = Source && Target; J->SetBoolField(TEXT("valid"), Found);
    bool Already = false;
    if (Found)
    {
        Count(C.R, TEXT("find_link_for_pin_calls")); URigVMLink* Link = Source->FindLinkForPin(Target);
        if (!I.Check()) { return false; } Already = Link != nullptr;
    }
    J->SetBoolField(TEXT("already_linked"), Already);
    Count(C.R, TEXT("contains_link_calls")); const bool Contains = C.Graph->ContainsLink(Rep);
    if (!I.Check()) { return false; } J->SetBoolField(TEXT("contains_link_path"), Contains);
    FString Reason; bool CanLink = false;
    if (Found)
    {
        if (!SafeNodeEdgeDepth(I, Source, Target)) { return false; }
        Count(C.R, TEXT("can_link_calls"));
        CanLink = C.Graph->CanLink(Source, Target, &Reason, nullptr, ERigVMPinDirection::IO, TypeCasting);
        // CanLink may refresh DiagnosticsAST/type caches and invoke virtual callbacks. Never infer purity.
        if (!I.Check()) { return false; }
    }
    else if (!Source && !Target) { Reason = TEXT("Source and target pins were not found in the default RigVM graph"); }
    else if (!Source) { Reason = TEXT("Source pin was not found in the default RigVM graph"); }
    else { Reason = TEXT("Target pin was not found in the default RigVM graph"); }
    J->SetBoolField(TEXT("can_link"), CanLink);
    return C.Text(J, TEXT("failure_reason"), Reason);
}
FString PackagePath(const FString& Raw)
{
    FString P=FPackageName::ExportTextPathToObjectPath(Raw).TrimStartAndEnd();P.TrimQuotesInline();P.ReplaceInline(TEXT("\\"),TEXT("/"));
    int32 Dot=INDEX_NONE;if(P.FindChar(TCHAR('.'),Dot)){P.LeftInline(Dot);}
    while(P.Contains(TEXT("//"))){P.ReplaceInline(TEXT("//"),TEXT("/"));}
    while(P.EndsWith(TEXT("/"))){P.LeftChopInline(1);}return P;
}
bool Allowed(const FString& P)
{
    for(const TCHAR* Root:{TEXT("/Game/_MCP_Sample"),TEXT("/Game/_MCP_Temp"),TEXT("/Game/CubelessMCPTests")})
    { if(P.Equals(Root,ESearchCase::IgnoreCase)||P.StartsWith(FString(Root)+TEXT("/"),ESearchCase::IgnoreCase)){return true;} }
    return false;
}
bool Disk(const FString& Package,const TSharedRef<FJsonObject>& Out)
{
    FString File;
    if(!FPackageName::TryConvertLongPackageNameToFilename(Package,File,FPackageName::GetAssetPackageExtension())){return false;}
    File=FPaths::ConvertRelativePathToFull(File);const int64 Size=IFileManager::Get().FileSize(*File);
    Out->SetStringField(TEXT("path"),File);Out->SetBoolField(TEXT("exists"),Size>=0);Out->SetNumberField(TEXT("bytes"),FMath::Max<int64>(Size,0));
    if(Size<0){Out->SetStringField(TEXT("sha1"),TEXT(""));return true;}
    TArray<uint8> Bytes;
    if(Size>268435456||!FFileHelper::LoadFileToArray(Bytes,*File)||Bytes.Num()!=Size){return false;}
    uint8 Hash[FSHA1::DigestSize];FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),Hash);Out->SetStringField(TEXT("sha1"),BytesToHex(Hash,FSHA1::DigestSize));return true;
}
bool WritableInventory(FInventory& I)
{
    if(!I.Check()||!Allowed(PackagePath(I.C.PackagePath))||!Allowed(PackagePath(I.C.AssetPath))||I.C.AssetOuter!=I.C.Package.Get()){return I.C.Bad(TEXT("Writer target identity/root is no longer admitted"));}
    for(const auto& V:I.Objects)
    {
        UObject* O=V.O;
        if((O->IsA<URigVMGraph>()||O->IsA<URigVMNode>()||O->IsA<URigVMPin>()||O->IsA<URigVMLink>()||O->IsA<URigVMInjectionInfo>())
            && O->GetPackage()!=I.C.Package.Get())
        {return I.C.Bad(TEXT("Writer graph/node/pin/link/injection has an external writable package; no authoring dispatch"));}
        const FString ClassPath=O->GetClass()->GetPathName();
        if(ClassPath.StartsWith(TEXT("/Script/Cubeless"))||ClassPath.StartsWith(TEXT("/Script/UnrealMCP")))
        {return I.C.Bad(TEXT("Writer would involve a Cubeless/MCP object class; runtime dependency admission refused"));}
    }
    auto SafeType=[&](UObject* Type)
    {
        if(!Type){return true;}
        if(!I.Object(Type)||!I.Check()){return false;}
        auto PackageAllowed=[](UObject* O){const FString Package=O->GetPackage()->GetName();
            return !Package.StartsWith(TEXT("/Script/Cubeless"))&&!Package.StartsWith(TEXT("/Script/UnrealMCP"))
            &&!Package.StartsWith(TEXT("/CubelessMCP_UE58/"))
            ;};
        if(!PackageAllowed(Type)){return I.C.Bad(TEXT("Runtime type belongs to Cubeless/MCP; writer admission refused"));}
        TSet<UStruct*> Seen;
        for(UStruct* Struct=Cast<UStruct>(Type);Struct;Struct=Struct->GetSuperStruct())
        {
            if(Seen.Contains(Struct)||Seen.Num()>=SafetyDepth||!I.Object(Struct)||!PackageAllowed(Struct))
            {return I.C.Bad(TEXT("Runtime type superclass/struct dependency is unreadable, cyclic or belongs to Cubeless/MCP"));}
            Seen.Add(Struct);
        }
        return I.Check();
    };
    TArray<UClass*> SourceClasses;for(const auto& V:I.Objects){SourceClasses.AddUnique(V.Class);}
    for(UClass* Class:SourceClasses){if(!SafeType(Class)){return false;}}
    if(UBlueprint* Target=Cast<UBlueprint>(I.C.Asset.Get()))
    {if((Target->ParentClass&&!SafeType(Target->ParentClass.Get()))||(Target->GeneratedClass&&!SafeType(Target->GeneratedClass.Get()))){return false;}}
    for(const auto& P:I.Pins)
    {UObject* Type=P.Pin->GetCPPTypeObject();if(!SafeType(Type)||!I.Member(P.Pin)){return false;}}
    for(const auto& N:I.Nodes)
    {if(URigVMUnitNode* Unit=Cast<URigVMUnitNode>(N.Node)){UScriptStruct* Struct=Unit->GetScriptStruct();if(!SafeType(Struct)||!I.Check()){return false;}}}
    return true;
}
struct FControllerView
{
    FControllerView(FRead& InC,URigVMController* InController):C(InC),Controller(InController),Class(InController->GetClass()),
        Outer(InController->GetOuter()),Path(InController->GetPathName()) {C.Hold(InController);C.Hold(Class);}
    FRead& C;TStrongObjectPtr<URigVMController> Controller;UClass* Class;UObject* Outer;FString Path;
    bool Current()
    {
        if(!C.Current()||!Allowed(PackagePath(C.PackagePath))||!C.SamePackage(Controller.Get())||!C.SamePackage(Class)
            ||Class->HasAnyClassFlags(CLASS_NewerVersionExists)||Controller->GetClass()!=Class
            ||Controller->GetOuter()!=Outer||Controller->GetPathName()!=Path||Controller->GetGraph()!=C.Graph.Get()||Controller->IsTransacting())
        {return C.Bad(TEXT("Captured controller/graph/editor ownership changed or undo/redo is active"));}
        Count(C.R,TEXT("get_controller_calls"));URigVMController* Now=C.Client->GetController(C.Graph.Get());
        if(!C.Current()||Now!=Controller.Get()){return C.Bad(TEXT("Default model no longer retains the captured controller"));}
        URigVMSchema* Schema=Controller->GetSchema();C.Hold(Schema);
        if(!C.Current()||!C.SamePackage(Schema)){return C.Bad(TEXT("Controller schema is unreadable after callback"));}
        return true;
    }
};
bool ObserveLinkDelta(FRead& C,const TArray<URigVMLink*>& Before,const TSharedRef<FJsonObject>& Row,int32& Added,int32& Removed)
{
    if(!C.Current()){return false;}
    const auto& Live=C.Graph->GetLinks();
    if(Live.Num()>SafetyRows){return C.Bad(TEXT("Post-AddLink source list exceeds the finite bound"));}
    const TArray<URigVMLink*> After=Live;
    int32 A=0,D=0;
    for(URigVMLink* L:After){if(!Before.Contains(L)){++A;}}
    for(URigVMLink* L:Before){if(!After.Contains(L)){++D;}}
    Added+=A;Removed+=D;
    C.R.Data->SetNumberField(TEXT("observed_links_added_count"),Added);C.R.Data->SetNumberField(TEXT("observed_links_removed_count"),Removed);
    C.R.Data->SetNumberField(TEXT("graph_link_count_after"),After.Num());
    Row->SetNumberField(TEXT("observed_graph_links_added"),A);Row->SetNumberField(TEXT("observed_graph_links_removed"),D);
    Row->SetBoolField(TEXT("graph_delta_observed"),true);C.R.bChanged|=A>0||D>0;return true;
}
}

Variable::FResult Add(const FString& ControlRigPath,const FString& ControlRigAssetPath,const FString& ControlRigClassPath,
    const FString& LinksJson,const FString& UnitLinksJson,bool bAllowTypeCasting,bool bDryRun,bool bCompile,bool bSaveAsset,int64 MaxLinks)
{
    FResult R;Init(R);R.bCompileRequested=bCompile;R.bSaveRequested=bSaveAsset;
    const int32 Limit=int32(FMath::Clamp<int64>(MaxLinks,0,256));
    R.Data->SetStringField(TEXT("received_max_links_decimal"),LexToString(MaxLinks));R.Data->SetNumberField(TEXT("max_links"),Limit);
    R.Data->SetBoolField(TEXT("dry_run"),bDryRun);R.Data->SetBoolField(TEXT("allow_type_casting"),bAllowTypeCasting);R.Data->SetBoolField(TEXT("enable_type_casting"),bAllowTypeCasting);
    R.Data->SetBoolField(TEXT("compile_requested"),bCompile);R.Data->SetBoolField(TEXT("save_requested"),bSaveAsset);
    if(!IsEditorGameThread()||IsPlaySessionActive()||IsEngineExitRequested()||ReadInFlight)
    {return Fail(R,TEXT("rigvm_link_writer_editor_unavailable"),TEXT("An idle live editor game thread outside PIE/SIE is required; no load dispatched"));}
    TGuardValue<bool> Busy(ReadInFlight,true);R.Stage=TEXT("parse_link_specs");
    TSharedPtr<FJsonValue> Links,Units;
    if(!Decode(LinksJson,Links)||!Decode(UnitLinksJson,Units))
    {return Fail(R,TEXT("rigvm_link_writer_json_invalid"),TEXT("Nullable array carriers require valid null/array JSON, each<=1Mi UE chars/no NUL/nesting<=64"));}
    TArray<FSpec> Specs;FString Field,ParseError;int32 Requested=0;bool Truncated=false;
    const bool Parsed=Parse(Links,Units,Limit,Specs,Field,Requested,Truncated,ParseError);
    R.Data->SetStringField(TEXT("link_specs_field"),Field);R.Data->SetNumberField(TEXT("requested_link_count"),Requested);
    R.Data->SetNumberField(TEXT("validated_link_count"),Specs.Num());R.Data->SetBoolField(TEXT("links_truncated"),Truncated);
    if(!Parsed){return Fail(R,TEXT("rigvm_link_writer_specs_invalid"),ParseError);}
    if(!InputPath(ControlRigPath)||!InputPath(ControlRigAssetPath)||!InputPath(ControlRigClassPath))
    {return Fail(R,TEXT("rigvm_link_writer_path_invalid"),TEXT("Target paths exceed finite NUL/length bounds"));}
    const FString Path=ControlRigPath.IsEmpty()?ControlRigAssetPath:ControlRigPath;FString RequestedPath,Kind;UObject* LoadedObject=nullptr;
    if(!Path.TrimStartAndEnd().IsEmpty()){RequestedPath=Path;Kind=TEXT("asset");}
    else if(!ControlRigClassPath.TrimStartAndEnd().IsEmpty()){RequestedPath=ControlRigClassPath;Kind=TEXT("generated_class");}
    else{return Fail(R,TEXT("rigvm_link_writer_target_missing"),TEXT("Missing 'control_rig_path', 'control_rig_asset_path', or 'control_rig_class_path' parameter"));}
    R.Data->SetStringField(TEXT("requested_control_rig"),RequestedPath);R.Stage=TEXT("load_control_rig");
    LoadedObject=Kind==TEXT("asset")?LoadAsset(RequestedPath,R):LoadClass(RequestedPath,R);TStrongObjectPtr<UObject> LoadLifetime(LoadedObject);
    if(!Loaded(LoadedObject)){return Fail(R,TEXT("rigvm_link_writer_target_invalid"),TEXT("ControlRig asset or generated class was not found or is not fully loaded"));}
    UObject* Asset=LoadedObject;UClass* InputClass=Cast<UClass>(LoadedObject);
    if(InputClass)
    {
        Asset=((!Path.TrimStartAndEnd().IsEmpty()||InputClass->IsChildOf(UControlRig::StaticClass()))&&Loaded(InputClass->ClassGeneratedBy))?InputClass->ClassGeneratedBy:nullptr;
        Kind=TEXT("generated_class");
    }
    R.Stage=TEXT("admit_legacy_writer_target");
    UControlRigBlueprint* Blueprint=Cast<UControlRigBlueprint>(Asset);
    if(!Loaded(Asset)||(!Blueprint&&!Cast<UControlRigEditorAsset>(Asset)))
    {return Fail(R,TEXT("rigvm_link_writer_target_invalid"),TEXT("Resolved input is not a live ControlRig Blueprint/editor asset"));}
    if(!Loaded(Blueprint))
    {
        R.Data->SetBoolField(TEXT("new_editor_asset_writer_supported"),false);
        return Fail(R,TEXT("rigvm_link_writer_legacy_blueprint_required"),TEXT("Writer requires a legacy ControlRigBlueprint. New ControlRigEditorAsset has a protected compile contract that is not treated as equivalent; no explicit writer/compile/save dispatched (load effects remain unknown)"));
    }
    if(!Loaded(Blueprint->GetPackage())){return Fail(R,TEXT("rigvm_link_writer_package_invalid"),TEXT("Target package is unreadable"));}
    const FString TargetPackage=PackagePath(Blueprint->GetPackage()->GetName());
    R.Data->SetStringField(TEXT("target_package_path"),TargetPackage);
    if(!Allowed(TargetPackage)||!Allowed(PackagePath(Blueprint->GetPathName()))||Blueprint->GetOuter()!=Blueprint->GetPackage())
    {return Fail(R,TEXT("rigvm_link_writer_target_forbidden"),TEXT("Target asset path and actual package must both be admitted sample/temp/test roots with one top-level owning package; external/nested Blueprint write admission refused"));}
    FRead C(R,Blueprint);C.InputClass.Reset(InputClass);R.Data->SetStringField(TEXT("control_rig_asset_path"),C.AssetPath);R.Data->SetStringField(TEXT("resolution_kind"),Kind);C.Dirty();
    int32 AddedCount=0,ObservedAdded=0,ObservedRemoved=0;bool AllApplied=true,AllWritable=true,CompileOK=true,SaveOK=true;
    auto CompileResult=MakeShared<FJsonObject>();CompileResult->SetBoolField(TEXT("requested"),false);R.Data->SetObjectField(TEXT("compile_result"),CompileResult);
    TArray<TSharedPtr<FJsonValue>> CompletionErrors;
    auto Prefix=[&]()
    {
        C.Dirty();R.Data->SetNumberField(TEXT("links_added_count"),AddedCount);
        const bool Modified=AddedCount>0||ObservedAdded>0||ObservedRemoved>0||C.R.bChanged;
        R.Data->SetBoolField(TEXT("target_modified"),Modified);R.Data->SetBoolField(TEXT("target_asset_modified"),Modified);
        R.Data->SetBoolField(TEXT("partial_target_modified"),Modified&&(!AllApplied||!CompileOK||!SaveOK||!C.Error.IsEmpty()));
        R.Data->SetBoolField(TEXT("target_asset_saved"),R.bSaved);R.Data->SetArrayField(TEXT("completion_errors"),CompletionErrors);
    };
    auto Failure=[&](const TCHAR* Code,const FString& Message)
    {AllApplied=false;Prefix();return Fail(R,Code,Message);};
    auto ObservationFailure=[&]() {return Failure(TEXT("rigvm_link_writer_observation_incomplete"),C.Error.IsEmpty()?TEXT("Writer observation is incomplete; partial prefix retained, no rollback/retry"):C.Error);};
    if(!C.RawCurrent()){C.Bad(TEXT("Target changed before model acquisition"));return ObservationFailure();}
    auto* Interface=Cast<IControlRigEditorAssetInterface>(Blueprint);
    C.Client=Interface?Interface->GetRigVMClient():nullptr;
    if(!C.RawCurrent()||!C.Client){C.Bad(TEXT("Public ControlRig client is unavailable"));return ObservationFailure();}
    Count(R,TEXT("default_model_calls"));C.Graph.Reset(C.Client->GetDefaultModel());
    if(!C.RawCurrent()||!Loaded(C.Graph.Get())){C.Bad(TEXT("ControlRig Blueprint has no default RigVM model"));return ObservationFailure();}
    C.Hold(C.Graph.Get());C.GraphClass=C.Graph->GetClass();C.Hold(C.GraphClass);C.GraphOuter=C.Graph->GetOuter();C.GraphPath=C.Graph->GetPathName();
    if(!C.Current()){return ObservationFailure();}
    const int32 GraphBefore=C.Graph->GetLinks().Num();R.Data->SetNumberField(TEXT("graph_link_count_before"),GraphBefore);R.Data->SetNumberField(TEXT("graph_link_count_after"),GraphBefore);
    auto FileBefore=MakeShared<FJsonObject>();const bool DiskBeforeKnown=Disk(TargetPackage,FileBefore);R.Data->SetObjectField(TEXT("target_file_before"),FileBefore);R.Data->SetBoolField(TEXT("target_file_before_known"),DiskBeforeKnown);
    R.Stage=TEXT("acquire_rigvm_controller");Count(R,TEXT("get_or_create_controller_calls"));
    R.Data->SetBoolField(TEXT("controller_acquisition_may_create_editor_object"),true);
    URigVMController* RawController=Blueprint->GetOrCreateController(C.Graph.Get());C.Hold(RawController);
    if(!C.Current()||!Loaded(RawController)){C.Bad(TEXT("Default model controller is unavailable or target changed"));return ObservationFailure();}
    FControllerView Controller(C,RawController);if(!Controller.Current()){return ObservationFailure();}
    R.Stage=TEXT("preflight_all_rigvm_links");FInventory Preflight(C);
    if(!Specs.IsEmpty()&&(!Preflight.Capture()||!WritableInventory(Preflight))){return ObservationFailure();}
    if(Specs.IsEmpty()&&!Preflight.Parents(C.Graph.Get())){return ObservationFailure();}
    TArray<TSharedRef<FJsonObject>> LinkRows;TArray<TSharedPtr<FJsonValue>> Rows;
    int32 Can=0,Cannot=0,Missing=0,Already=0,WouldAdd=0;
    for(int32 I=0;I<Specs.Num();++I)
    {
        auto Row=MakeShared<FJsonObject>();LinkRows.Add(Row);Rows.Add(MakeShared<FJsonValueObject>(Row));R.Data->SetArrayField(TEXT("results"),Rows);R.Data->SetNumberField(TEXT("included_link_count"),Rows.Num());
        if(!Diagnostic(Preflight,Specs[I],I,bAllowTypeCasting,Row)||!Controller.Current()){return ObservationFailure();}
        const bool CanLink=Row->GetBoolField(TEXT("can_link")),FoundSource=Row->GetBoolField(TEXT("source_pin_found")),FoundTarget=Row->GetBoolField(TEXT("target_pin_found")),Existing=Row->GetBoolField(TEXT("already_linked"));
        const bool WillAdd=CanLink&&!Existing,Writable=Existing||WillAdd;
        Row->SetBoolField(TEXT("will_add_link"),WillAdd);Row->SetBoolField(TEXT("writer_candidate"),Writable);Row->SetBoolField(TEXT("applied"),false);
        Can+=CanLink?1:0;Cannot+=CanLink?0:1;Missing+=(!FoundSource||!FoundTarget)?1:0;Already+=Existing?1:0;WouldAdd+=WillAdd?1:0;AllWritable&=Writable;
        R.Data->SetNumberField(TEXT("can_link_count"),Can);R.Data->SetNumberField(TEXT("cannot_link_count"),Cannot);R.Data->SetNumberField(TEXT("missing_pin_count"),Missing);
        R.Data->SetNumberField(TEXT("already_linked_count"),Already);R.Data->SetNumberField(TEXT("would_add_count"),WouldAdd);R.Data->SetNumberField(TEXT("completed_preflight_count"),I+1);
    }
    R.Data->SetBoolField(TEXT("all_specs_writable"),AllWritable);
    if(!AllWritable){CompletionErrors.Add(MakeShared<FJsonValueString>(TEXT("preflight_not_writable")));return Failure(TEXT("rigvm_link_writer_not_writable"),TEXT("One or more requested RigVM links are not writable; no links were added"));}
    if(!bDryRun)
    {
        // Auto-recompile may replace a generated input class. Its pre-writer association was checked;
        // thereafter retain the target asset/model/controller, not the superseded generated class.
        if(C.InputClass.IsValid()){R.Data->SetStringField(TEXT("resolved_input_class_before_writer"),C.InputClass->GetPathName());}C.InputClass.Reset();
        for(int32 I=0;I<Specs.Num();++I)
        {
            auto Row=LinkRows[I];if(!Row->GetBoolField(TEXT("will_add_link"))){continue;}
            R.Stage=TEXT("apply_rigvm_link");FInventory CurrentInventory(C);
            if(!Controller.Current()||!CurrentInventory.Capture()||!WritableInventory(CurrentInventory)){return ObservationFailure();}
            Count(R,TEXT("find_pin_calls"));URigVMPin* Source=C.Graph->FindPin(Specs[I].Source);
            if(!CurrentInventory.Check()||(Source&&!CurrentInventory.Member(Source))){return ObservationFailure();}
            Count(R,TEXT("find_pin_calls"));URigVMPin* Target=C.Graph->FindPin(Specs[I].Target);
            if(!CurrentInventory.Check()||(Target&&!CurrentInventory.Member(Target))){return ObservationFailure();}
            if(!Source||!Target)
            {AllApplied=false;Row->SetStringField(TEXT("apply_failure_reason"),TEXT("Pin disappeared after preflight; no AddLink dispatched"));continue;}
            URigVMLink* ExistingNow=Source->FindLinkForPin(Target);
            if(!CurrentInventory.Check()){return ObservationFailure();}
            if(ExistingNow)
            {AllApplied=false;Row->SetStringField(TEXT("apply_failure_reason"),TEXT("Link became existing after preflight; duplicate AddLink not dispatched"));Row->SetBoolField(TEXT("duplicate_dispatch_avoided"),true);continue;}
            if(!SafeNodeEdgeDepth(CurrentInventory,Source,Target)||!Controller.Current()){return ObservationFailure();}
            const TArray<URigVMLink*> Before=C.Graph->GetLinks();FString Reason;
            R.bMutationAttempted=true;Count(R,TEXT("add_link_calls"));R.Data->SetNumberField(TEXT("add_link_attempt_count"),R.Data->GetObjectField(TEXT("native_call_counts"))->GetNumberField(TEXT("add_link_calls")));
            Row->SetBoolField(TEXT("add_link_dispatched"),true);
            const bool Added=Controller.Controller->AddLink(Source,Target,false,ERigVMPinDirection::IO,false,false,&Reason);
            Row->SetBoolField(TEXT("add_link_returned"),true);Row->SetBoolField(TEXT("applied"),Added);Row->SetStringField(TEXT("apply_failure_reason"),Reason);
            if(Added){++AddedCount;Count(R,TEXT("add_link_returns_true"));}else{AllApplied=false;}
            R.Data->SetNumberField(TEXT("links_added_count"),AddedCount);
            if(!ObserveLinkDelta(C,Before,Row,ObservedAdded,ObservedRemoved)||!Controller.Current()){return ObservationFailure();}
            FInventory After(C);if(!After.Capture()||!WritableInventory(After)){return ObservationFailure();}
            URigVMPin* AfterSource=C.Graph->FindPin(Specs[I].Source);if(!After.Check()||(AfterSource&&!After.Member(AfterSource))){return ObservationFailure();}
            URigVMPin* AfterTarget=C.Graph->FindPin(Specs[I].Target);if(!After.Check()||(AfterTarget&&!After.Member(AfterTarget))){return ObservationFailure();}
            const bool Present=AfterSource&&AfterTarget&&AfterSource->FindLinkForPin(AfterTarget)!=nullptr;
            if(!After.Check()){return ObservationFailure();}Row->SetBoolField(TEXT("postcondition_link_present"),Present);
            if(Added&&!Present){AllApplied=false;Row->SetStringField(TEXT("postcondition_error"),TEXT("SDK returned true but requested link was not present after callbacks"));}
            Prefix();
        }
        if(AddedCount>0)
        {
            R.Stage=TEXT("request_auto_vm_recompilation");if(!Controller.Current()){return ObservationFailure();}
            Count(R,TEXT("auto_vm_recompile_requests"));Blueprint->RequestAutoVMRecompilation();
            if(!Controller.Current()){return ObservationFailure();}
            R.Stage=TEXT("mark_target_package_dirty");Count(R,TEXT("mark_package_dirty_calls"));const bool DirtyResult=Blueprint->MarkPackageDirty();R.Data->SetBoolField(TEXT("mark_package_dirty_returned"),DirtyResult);
            if(!Controller.Current()){return ObservationFailure();}Prefix();
        }
    }
    const bool EffectiveCompile=bCompile&&!bDryRun&&AddedCount>0,EffectiveSave=bSaveAsset&&!bDryRun&&AddedCount>0;
    CompileResult->SetBoolField(TEXT("requested"),EffectiveCompile);
    if(EffectiveCompile)
    {
        R.Stage=TEXT("request_control_rig_init");if(!Controller.Current()){return ObservationFailure();}
        Count(R,TEXT("request_init_calls"));Blueprint->RequestRigVMInit();CompileResult->SetBoolField(TEXT("init_request_returned"),true);
        if(!Controller.Current()){return ObservationFailure();}
        R.Stage=TEXT("compile_control_rig_blueprint");R.bCompileAttempted=true;Count(R,TEXT("explicit_compile_calls"));FCompilerResultsLog Log;
        FKismetEditorUtilities::CompileBlueprint(Blueprint,EBlueprintCompileOptions::None,&Log);CompileResult->SetBoolField(TEXT("compiler_returned"),true);
        CompileResult->SetNumberField(TEXT("num_errors"),Log.NumErrors);CompileResult->SetNumberField(TEXT("num_warnings"),Log.NumWarnings);
        if(!Controller.Current()){return ObservationFailure();}
        UClass* Generated=Blueprint->GeneratedClass.Get();C.Hold(Generated);
        CompileOK=Log.NumErrors==0&&Blueprint->Status!=BS_Error&&Loaded(Generated)&&!Generated->HasAnyClassFlags(CLASS_NewerVersionExists);
        CompileResult->SetBoolField(TEXT("legacy_generated_class_exists"),Generated!=nullptr);CompileResult->SetBoolField(TEXT("success"),CompileOK);
        CompileResult->SetStringField(TEXT("blueprint"),C.AssetPath);CompileResult->SetStringField(TEXT("generated_class"),Generated?Generated->GetPathName():FString());R.Compiled=CompileOK;
        if(!CompileOK){CompletionErrors.Add(MakeShared<FJsonValueString>(TEXT("compile_failed")));}
    }
    if(EffectiveSave)
    {
        R.Stage=TEXT("save_target_control_rig");if(!Controller.Current()){return ObservationFailure();}
        R.bSaveAttempted=true;R.Data->SetStringField(TEXT("save_verification_state"),TEXT("pending"));Count(R,TEXT("explicit_save_calls"));const bool Saved=UEditorAssetLibrary::SaveLoadedAsset(Blueprint,false);R.Data->SetBoolField(TEXT("save_api_returned"),Saved);
        auto FileAfter=MakeShared<FJsonObject>();const bool Known=Disk(TargetPackage,FileAfter);R.Data->SetObjectField(TEXT("target_file_after"),FileAfter);R.Data->SetBoolField(TEXT("target_file_after_known"),Known);
        if(!Controller.Current()){R.Data->SetStringField(TEXT("save_verification_state"),TEXT("outcome_unverified_identity_lost"));return ObservationFailure();}
        SaveOK=Saved&&Known&&FileAfter->GetBoolField(TEXT("exists"))&&!C.Package->IsDirty();R.bSaved=SaveOK;
        R.Data->SetStringField(TEXT("save_verification_state"),SaveOK?TEXT("API_file_and_dirty_verified_no_reload"):TEXT("failed_or_unverified"));
        R.Data->SetBoolField(TEXT("saved_link_reload_verified"),false);
        if(!SaveOK){CompletionErrors.Add(MakeShared<FJsonValueString>(TEXT("save_failed")));}
    }
    R.Stage=TEXT("verify_rigvm_link_writer");if(!Controller.Current()){return ObservationFailure();}
    FInventory Final(C);if(!Specs.IsEmpty()&&(!Final.Capture()||!WritableInventory(Final))){return ObservationFailure();}
    if(Specs.IsEmpty()&&!Final.Parents(C.Graph.Get())){return ObservationFailure();}
    if(!bDryRun)
    {
        for(int32 I=0;I<Specs.Num();++I)
        {
            auto Row=LinkRows[I];
            if(!Row->GetBoolField(TEXT("already_linked"))&&!Row->GetBoolField(TEXT("applied"))){continue;}
            Count(R,TEXT("find_pin_calls"));URigVMPin* Source=C.Graph->FindPin(Specs[I].Source);
            if(!Final.Check()||(Source&&!Final.Member(Source))){return ObservationFailure();}
            Count(R,TEXT("find_pin_calls"));URigVMPin* Target=C.Graph->FindPin(Specs[I].Target);
            if(!Final.Check()||(Target&&!Final.Member(Target))){return ObservationFailure();}
            const bool Present=Source&&Target&&Source->FindLinkForPin(Target)!=nullptr;
            if(!Final.Check()){return ObservationFailure();}Row->SetBoolField(TEXT("final_link_present"),Present);
            if(!Present){AllApplied=false;Row->SetStringField(TEXT("final_postcondition_error"),TEXT("A later write/compiler callback removed an expected link; retained changes are partial"));}
        }
    }
    Count(R,TEXT("asset_reference_calls"));const FControlRigAssetStrongReference AssetSource=Interface->GetControlRigAssetReference();UClass* Generated=AssetSource.GetBlueprintClass().Get();C.Hold(Generated);
    if(!Controller.Current()||Generated!=Cast<URigVMBlueprintGeneratedClass>(Blueprint->GeneratedClass.Get())){C.Bad(TEXT("Public generated-class reference changed or differs from legacy target"));return ObservationFailure();}
    if(Generated){if(!C.SamePackage(Generated)){return ObservationFailure();}R.Data->SetStringField(TEXT("control_rig_class_path"),Generated->GetPathName());}
    const FString GraphName=C.Graph->GetGraphName();if(!Controller.Current()){return ObservationFailure();}
    const FString GraphPath=C.Graph->GetNodePath();if(!Controller.Current()){return ObservationFailure();}
    if(!Final.Check()){return ObservationFailure();}
    R.Data->SetStringField(TEXT("graph_name"),GraphName);R.Data->SetStringField(TEXT("graph_node_path"),GraphPath);R.Data->SetNumberField(TEXT("graph_link_count_after"),C.Graph->GetLinks().Num());
    R.Data->SetBoolField(TEXT("compile_succeeded"),CompileOK);R.Data->SetBoolField(TEXT("save_succeeded"),SaveOK);Prefix();
    if(!AllApplied){return Failure(TEXT("rigvm_link_writer_apply_failed"),TEXT("One or more RigVM links failed to apply"));}
    if(!CompileOK||!SaveOK){return Failure(TEXT("rigvm_link_writer_completion_failed"),TEXT("RigVM link changes were retained but compile/save completion failed"));}
    R.bSuccess=true;R.bPostconditionPassed=true;R.Stage=TEXT("rigvm_link_writer_complete");R.Data->SetBoolField(TEXT("success"),true);return R;
}
}
