// UE56 proposal diagnostics contract, implemented with installed UE58 public APIs.
#include "Compatibility/UE58/CubelessRigVMPinDefaultValidationApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "ControlRig.h"
#include "ControlRigAssetReference.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigEditorAsset.h"
#include "ControlRigRuntimeAsset.h"
#include "Containers/UnrealString.h"
#include "Dom/JsonValue.h"
#include "EditorAssetLibrary.h"
#include "Misc/CoreMisc.h"
#include "Misc/PackageName.h"
#include "RigVMBlueprintGeneratedClass.h"
#include "RigVMTypeUtils.h"
#include "RigVMModel/Nodes/RigVMCollapseNode.h"
#include "RigVMModel/RigVMClient.h"
#include "RigVMModel/RigVMGraph.h"
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

namespace Cubeless::UE58::RigVMPinDefaultValidation
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
    auto C = MakeShared<FJsonObject>();
    for (const TCHAR* Key : {TEXT("asset_load_calls"), TEXT("asset_load_returns"), TEXT("class_load_calls"), TEXT("class_find_calls"),
        TEXT("default_model_calls"), TEXT("asset_reference_calls"), TEXT("find_pin_calls"), TEXT("pin_default_projection_calls"),
        TEXT("is_valid_default_calls"), TEXT("clamp_default_calls"), TEXT("completed_diagnostics"), TEXT("getter_identity_checks"),
        TEXT("explicit_modify_calls"), TEXT("explicit_set_pin_default_calls"), TEXT("explicit_array_resize_calls"),
        TEXT("explicit_compile_calls"), TEXT("explicit_save_calls"), TEXT("runtime_execution_calls")})
    { C->SetNumberField(Key, 0); }
    R.Data->SetObjectField(TEXT("native_call_counts"), C);
    R.Data->SetStringField(TEXT("command"), TEXT("validate_controlrig_rigvm_pin_defaults"));
    R.Data->SetStringField(TEXT("requested_control_rig"), TEXT(""));
    R.Data->SetBoolField(TEXT("read_only"), true);
    R.Data->SetBoolField(TEXT("source_modified"), false);
    R.Data->SetBoolField(TEXT("saves_assets"), false);
    R.Data->SetBoolField(TEXT("explicit_authoring_read_only"), true);
    R.Data->SetBoolField(TEXT("total_SDK_side_effects_verified"), false);
    R.Data->SetBoolField(TEXT("atomic_snapshot_verified"), false);
    R.Data->SetBoolField(TEXT("observation_incomplete"), false);
    R.Data->SetBoolField(TEXT("automatic_action_retry"), false);
    R.Data->SetBoolField(TEXT("atomic_rollback_available"), false);
    R.Data->SetBoolField(TEXT("unique_external_pipeline_complete"), false);
    R.Data->SetBoolField(TEXT("packaged_runtime_verified"), false);
    R.Data->SetNumberField(TEXT("completed_pin_count"), 0);
    R.Data->SetNumberField(TEXT("included_pin_count"), 0);
    for (const TCHAR* Key : {TEXT("pin_found_count"),TEXT("missing_pin_count"),TEXT("can_provide_default_count"),TEXT("cannot_provide_default_count"),
        TEXT("proposed_default_provided_count"),TEXT("proposed_default_valid_count"),TEXT("proposed_default_invalid_count"),TEXT("default_write_candidate_for_review_count")})
    { R.Data->SetNumberField(Key, 0); }
    R.Data->SetBoolField(TEXT("actual_default_writes_supported"), false);
    R.Data->SetArrayField(TEXT("results"), {});
    R.Data->SetStringField(TEXT("scope"), TEXT("read-only ControlRig RigVM pin default feasibility validation; no URigVMController SetPinDefaultValue, graph mutation, array resize, compile, save, execute, or solve"));
    R.Data->SetStringField(TEXT("change_state_scope"), TEXT("No explicit authoring/compile/save/execute calls. Load/PostLoad, virtual getters, type registry and default/UI/original/type caches and struct import callbacks have unknown total effects. Source_modified is the legacy captured-package dirty comparison, not total purity."));
}
FResult Fail(FResult R, const TCHAR* Code, const FString& Message)
{
    R.bSuccess = false; R.bPostconditionPassed = false; R.ErrorCode = Code; R.Error = Message;
    R.Data->SetBoolField(TEXT("success"), false); R.Data->SetStringField(TEXT("message"), Message);
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
struct FSpec { FString Name, Path, Proposed; bool bProvided = false; };
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
FString ProposedString(const TSharedPtr<FJsonValue>& V)
{
    if (!V.IsValid()) { return FString(); }
    if (V->Type == EJson::Boolean) { return V->AsBool() ? TEXT("true") : TEXT("false"); }
    if (V->Type == EJson::Number) { return FString::SanitizeFloat(V->AsNumber()); }
    if (V->Type == EJson::String) { return V->AsString(); }
    if (V->Type == EJson::Array || V->Type == EJson::Object)
    {
        FString Text; const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
        if (FJsonSerializer::Serialize(V, TEXT(""), Writer)) { return Text; }
    }
    return FString(); // Explicit JSON null is a provided empty string, not an omitted proposal.
}
bool ProposalBound(const FString& V)
{
    if (V.Len() > SafetyChars) { return false; }
    int32 Depth = 0; bool Quoted = false, Escaped = false;
    for (int32 I = 0; I < V.Len(); ++I)
    {
        const TCHAR C = V[I]; if (C == TCHAR(0)) { return false; }
        if (Quoted)
        {
            if (Escaped) { Escaped = false; } else if (C == TCHAR('\\')) { Escaped = true; }
            else if (C == TCHAR('"')) { Quoted = false; }
        }
        else if (C == TCHAR('"')) { Quoted = true; }
        else if (C == TCHAR('(') || C == TCHAR('{') || C == TCHAR('[')) { if (++Depth > SafetyDepth) { return false; } }
        else if ((C == TCHAR(')') || C == TCHAR('}') || C == TCHAR(']')) && Depth > 0) { --Depth; }
    }
    // Malformed syntax remains an ordinary IsValidDefaultValue diagnostic, not a parser failure.
    return true;
}
bool Parse(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B, const TSharedPtr<FJsonValue>& C,
    int32 Limit, TArray<FSpec>& Specs, FString& Field, int32& Requested, bool& Truncated, FString& Error)
{
    const auto& Chosen = A->Type == EJson::Array ? A : (B->Type == EJson::Array ? B : C);
    Field = A->Type == EJson::Array ? TEXT("pin_defaults") : (B->Type == EJson::Array ? TEXT("defaults") : (C->Type == EJson::Array ? TEXT("pins") : TEXT("")));
    if (Chosen->Type != EJson::Array || (Requested = Chosen->AsArray().Num()) == 0)
    { Error = TEXT("Missing non-empty 'pin_defaults' array (or 'defaults'/'pins' alias)"); return false; }
    Truncated = Requested > Limit;
    for (int32 I = 0; I < Requested && I < Limit; ++I)
    {
        const auto& V = Chosen->AsArray()[I]; FSpec S; S.Name = FString::Printf(TEXT("pin_default_%d"), I);
        if (V.IsValid() && V->Type == EJson::String) { S.Path = V->AsString(); }
        else if (V.IsValid() && V->Type == EJson::Object)
        {
            const auto O = V->AsObject(); S.Name = StringParam(O, TEXT("name"), S.Name);
            S.Name = StringParam(O, TEXT("label"), S.Name); S.Name = StringParam(O, TEXT("id"), S.Name);
            S.Path = StringParam(O, TEXT("pin_path")); if (S.Path.IsEmpty()) { S.Path = StringParam(O, TEXT("pin")); }
            if (S.Path.IsEmpty()) { S.Path = StringParam(O, TEXT("path")); }
            TSharedPtr<FJsonValue> Proposed = O->TryGetField(TEXT("default_value"));
            if (!Proposed.IsValid()) { Proposed = O->TryGetField(TEXT("value")); }
            if (!Proposed.IsValid()) { Proposed = O->TryGetField(TEXT("default")); }
            if (!Proposed.IsValid()) { Proposed = O->TryGetField(TEXT("proposed_default_value")); }
            if (Proposed.IsValid()) { S.bProvided = true; S.Proposed = ProposedString(Proposed); }
        }
        else { Error = FString::Printf(TEXT("%s[%d] must be an object or pin-path string"), *Field, I); return false; }
        S.Name.TrimStartAndEndInline(); S.Path.TrimStartAndEndInline();
        if (S.Path.IsEmpty()) { Error = FString::Printf(TEXT("%s[%d] missing pin_path/pin/path"), *Field, I); return false; }
        if (!PinPath(S.Path) || (S.bProvided && !ProposalBound(S.Proposed)))
        { Error = FString::Printf(TEXT("%s[%d] exceeds finite pin path/proposal character, NUL or nesting bounds"), *Field, I); return false; }
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
        R.Data->SetBoolField(TEXT("asset_dirty_before"), bDirtyBefore);
        R.Data->SetStringField(TEXT("captured_package_path"), PackagePath);
        R.Data->SetStringField(TEXT("dirty_observation_scope"), TEXT("Original resolved package captured after load; this does not verify current asset package dirty if identity is lost"));
        if (Loaded(Package.Get()) && Package->GetPathName() == PackagePath)
        {
            const bool After = Package->IsDirty(); R.Data->SetBoolField(TEXT("asset_dirty_after"), After);
            R.Data->SetBoolField(TEXT("asset_dirty_unchanged"), bDirtyBefore == After);
            R.Data->SetBoolField(TEXT("source_modified"), bDirtyBefore != After); R.bChanged |= bDirtyBefore != After;
        }
        else
        {
            R.Data->SetField(TEXT("asset_dirty_after"), MakeShared<FJsonValueNull>());
            R.Data->SetField(TEXT("asset_dirty_unchanged"), MakeShared<FJsonValueNull>());
            R.Data->SetField(TEXT("source_modified"), MakeShared<FJsonValueNull>());
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

FString DefaultType(ERigVMPinDefaultValueType T)
{
    switch (T)
    {
    case ERigVMPinDefaultValueType::AutoDetect: return TEXT("auto_detect");
    case ERigVMPinDefaultValueType::Unset: return TEXT("unset");
    case ERigVMPinDefaultValueType::Override: return TEXT("override");
    case ERigVMPinDefaultValueType::KeepValueType: return TEXT("keep_value_type");
    default: return TEXT("unknown");
    }
}
// GetDefaultValue/HasOriginalDefaultValue/HasDefaultValueOverride recursively read all children.
// Retain the whole selected closure and live reflected types before these SDK getters.
bool PrepareDefaults(FInventory& I, URigVMPin* P, int32 Depth, TSet<URigVMPin*>& Active)
{
    FRead& C = I.C;
    if (!I.Step() || Depth > SafetyDepth || Active.Contains(P) || !I.Member(P))
    { return C.Bad(TEXT("Default-value closure is cyclic, unreadable or exceeds the finite depth")); }
    Active.Add(P); UObject* Type = P->GetCPPTypeObject();
    if (Type && !I.Object(Type)) { return false; }
    if (!I.Member(P)) { return false; }
    const bool Struct = P->IsStruct(); if (!I.Member(P)) { return false; }
    if (Struct && !Cast<UScriptStruct>(Type)) { return C.Bad(TEXT("Struct default closure has no live script struct")); }
    const TArray<URigVMPin*> Children = P->GetSubPins();
    for (URigVMPin* Child : Children)
    {
        if (!I.Member(P) || P->GetSubPins() != Children || !PrepareDefaults(I, Child, Depth + 1, Active)) { return false; }
    }
    Active.Remove(P); return I.Member(P);
}
bool SafeProposal(FInventory& I, URigVMPin* P, const FString& Proposal)
{
    UObject* Type = P->GetCPPTypeObject(); if (Type && !I.Object(Type)) { return false; }
    if (!I.Member(P)) { return false; }
    const bool Array = P->IsArray(); if (!I.Member(P)) { return false; }
    TArray<FString> Values;
    if (Array) { Values = URigVMPin::SplitDefaultValue(Proposal); } else { Values.Add(Proposal); }
    if (!I.Member(P) || Values.Num() > SafetyRows) { return I.C.Bad(TEXT("Proposed default elements exceed the finite validation bound")); }
    if (UEnum* Enum = Cast<UEnum>(Type))
    {
        for (const FString& V : Values)
        {
            if (!I.Step() || Enum->GenerateFullEnumName(*V).Len() >= NAME_SIZE)
            { return I.C.Bad(TEXT("Proposed enum name would exceed the SDK FName bound")); }
        }
    }
    else if (Cast<UClass>(Type))
    {
        for (const FString& V : Values)
        {
            if (!I.Step() || (!V.IsEmpty() && V != TEXT("None") && Normalize(V).Len() >= NAME_SIZE))
            { return I.C.Bad(TEXT("Proposed object/class path would exceed the SDK name bound")); }
        }
    }
    return I.Member(P);
}
// IsValidDefaultValue's TSubclassOf branch CastChecked<UClass>s the result of a
// public UObject resolver. A loaded CDO/ordinary object must be an invalid proposal,
// not an assertion. Use that branch's public resolution + safe Cast/IsChildOf once,
// and do not redispatch the assertion-bearing validator after a callback can change it.
bool ClassProposal(FInventory& I, URigVMPin* P, const FString& Proposal,
    const TSharedRef<FJsonObject>& J, bool& bHandled, bool& bValid)
{
    bHandled = false; bValid = false; FRead& C = I.C;
    UObject* Type = P->GetCPPTypeObject(); if (Type && !I.Object(Type)) { return false; }
    if (!I.Member(P)) { return false; }
    const FString CPPType = P->GetCPPType(); if (!I.Member(P)) { return false; }
    UClass* Expected = Cast<UClass>(Type);
    if (!Expected || !RigVMTypeUtils::IsUClassType(CPPType)) { return true; }
    bHandled = true; C.R.Stage = TEXT("validate_class_default_proposal");
    J->SetBoolField(TEXT("class_default_proposal_guard"), true);
    J->SetStringField(TEXT("class_default_validation_source"), TEXT("public RigVMTypeUtils resolver, safe Cast<UClass>, IsChildOf; assertion-bearing IsValidDefaultValue branch not redispatched"));
    auto Current = [&]()
    {
        if (!I.Member(P) || !I.Identity(Expected) || Expected->HasAnyClassFlags(CLASS_NewerVersionExists))
        { return C.Bad(TEXT("Expected class or pin became unreadable during class-default validation")); }
        UObject* NowType = P->GetCPPTypeObject(); if (!I.Member(P)) { return false; }
        const FString NowCPP = P->GetCPPType();
        if (!I.Member(P) || NowType != Expected || NowCPP != CPPType || !I.Identity(Expected))
        { return C.Bad(TEXT("Pin reflected/C++ type changed during class-default validation")); }
        return true;
    };
    if (!Current()) { return false; }
    const bool Array = P->IsArray(); if (!Current()) { return false; }
    // Preserve SDK's pre-loop array syntax rejection and its empty-list final True.
    if (Array && (Proposal.IsEmpty() || Proposal[0] != TCHAR('(') || Proposal[Proposal.Len() - 1] != TCHAR(')')))
    { return true; }
    TArray<FString> Values;
    if (Array) { Values = URigVMPin::SplitDefaultValue(Proposal); } else { Values.Add(Proposal); }
    if (Values.Num() > SafetyRows || !Current()) { return C.Bad(TEXT("Class-default proposal exceeds the bounded value inventory")); }
    int32 Checked = 0;
    for (const FString& Value : Values)
    {
        if (!I.Step() || !Current()) { return false; }
        if (Value.IsEmpty() || Value == TEXT("None")) { bValid = true; return true; }
        // The actual resolver uses the raw string, not Normalize's export-text projection.
        if (Value.Len() >= NAME_SIZE) { return C.Bad(TEXT("Raw class-default path exceeds the public resolver name bound")); }
        double Calls = 0; C.R.Data->TryGetNumberField(TEXT("class_default_public_resolve_calls"), Calls);
        C.R.Data->SetNumberField(TEXT("class_default_public_resolve_calls"), Calls + 1);
        J->SetBoolField(TEXT("class_default_resolution_attempted"), true);
        // This uses the same exported SDK search/redirect behavior. Its callbacks, ambiguity
        // diagnostics and indirect effects are not a purity proof; total change remains unknown.
        UObject* Resolved = RigVMTypeUtils::FindObjectFromCPPTypeObjectPath(Value);
        if (Resolved && !I.Object(Resolved)) { return false; }
        if (!Current() || (Resolved && !I.Identity(Resolved))) { return false; }
        J->SetBoolField(TEXT("class_default_resolution_returned"), true);
        UClass* ResolvedClass = Cast<UClass>(Resolved);
        J->SetStringField(TEXT("class_default_last_resolution_kind"), !Resolved ? TEXT("missing") : (ResolvedClass ? TEXT("UClass") : TEXT("non_UClass")));
        if (!ResolvedClass) { return true; } // Missing/loaded CDO/ordinary UObject: normal invalid.
        if (ResolvedClass->HasAnyClassFlags(CLASS_NewerVersionExists))
        { return C.Bad(TEXT("Resolved class is a superseded class during default validation")); }
        const bool Compatible = ResolvedClass->IsChildOf(Expected);
        if (!Current() || !I.Identity(ResolvedClass)) { return false; }
        J->SetNumberField(TEXT("class_default_checked_element_count"), ++Checked);
        if (!Compatible) { return true; }
    }
    bValid = true; return Current();
}

bool Diagnostic(FInventory& I, const FSpec& S, int32 Index, const TSharedRef<FJsonObject>& J)
{
    FRead& C = I.C; J->SetNumberField(TEXT("index"), Index);
    if (!C.Text(J, TEXT("name"), S.Name) || !C.Text(J, TEXT("pin_path"), S.Path)) { return false; }
    J->SetBoolField(TEXT("proposed_default_provided"), S.bProvided);
    if (S.bProvided && !C.Text(J, TEXT("proposed_default_value"), S.Proposed)) { return false; }
    if (!I.Check()) { return false; }
    Count(C.R, TEXT("find_pin_calls")); URigVMPin* P = C.Graph->FindPin(S.Path);
    if (!I.Check() || (P && !I.Member(P))) { return false; }
    J->SetBoolField(TEXT("pin_found"), P != nullptr);
    if (!P)
    {
        for (const TCHAR* Key : {TEXT("valid"),TEXT("can_provide_default_value"),TEXT("proposed_default_is_valid"),TEXT("proposed_default_valid")})
        { J->SetBoolField(Key, false); }
        J->SetStringField(TEXT("failure_reason"), TEXT("Pin was not found in the default RigVM graph")); return true;
    }
    Count(C.R, TEXT("pin_default_projection_calls")); TSet<URigVMPin*> Active;
    if (!PrepareDefaults(I, P, 0, Active)) { return false; }
    J->SetBoolField(TEXT("valid"), true);
#define PIN_TEXT(Key, Expr) { const FString V = (Expr); if (!I.Member(P) || !C.Text(J, TEXT(Key), V)) { return false; } }
#define PIN_BOOL(Key, Expr) { const bool V = (Expr); if (!I.Member(P)) { return false; } J->SetBoolField(TEXT(Key), V); }
    PIN_TEXT("pin_name", P->GetName()); PIN_TEXT("pin_path_resolved", P->GetPinPath());
    PIN_TEXT("pin_direction", Direction(P->GetDirection())); PIN_TEXT("cpp_type", P->GetCPPType());
    UObject* Type = P->GetCPPTypeObject(); if (Type && !I.Object(Type)) { return false; }
    if (!I.Member(P)) { return false; }
    if (Type && !C.Text(J, TEXT("cpp_type_object"), Type->GetPathName())) { return false; }
    PIN_BOOL("is_execute_context", P->IsExecuteContext()); PIN_BOOL("is_array", P->IsArray());
    PIN_BOOL("is_array_element", P->IsArrayElement()); PIN_BOOL("is_struct", P->IsStruct());
    PIN_BOOL("is_struct_member", P->IsStructMember()); PIN_BOOL("is_string_type", P->IsStringType());
    PIN_BOOL("is_enum", P->IsEnum()); PIN_BOOL("is_linked", P->IsLinked(false));
    J->SetNumberField(TEXT("link_count"), P->GetLinks().Num());
    PIN_BOOL("requires_watch", P->RequiresWatch(false)); PIN_BOOL("can_provide_default_value", P->CanProvideDefaultValue());
    PIN_TEXT("default_value_type", DefaultType(P->GetDefaultValueType()));
    PIN_BOOL("has_original_default_value", P->HasOriginalDefaultValue());
    PIN_TEXT("original_default_value", P->GetOriginalDefaultValue());
    Active.Reset(); if (!PrepareDefaults(I, P, 0, Active)) { return false; }
    PIN_TEXT("current_default_value", P->GetDefaultValue());
    Active.Reset(); if (!PrepareDefaults(I, P, 0, Active)) { return false; }
    PIN_TEXT("default_value_ui", P->GetDefaultValueStoredByUserInterface());
    PIN_BOOL("has_default_value_override", P->HasDefaultValueOverride());
    PIN_TEXT("bound_variable_path", P->GetBoundVariablePath());
    PIN_BOOL("is_bound_to_variable", P->IsBoundToVariable());
    PIN_BOOL("is_bound_to_external_variable", P->IsBoundToExternalVariable());
    PIN_BOOL("is_bound_to_local_variable", P->IsBoundToLocalVariable());
    PIN_BOOL("is_bound_to_input_argument", P->IsBoundToInputArgument());
    URigVMNode* N = P->GetNode(); if (!I.Member(P)) { return false; }
    if (N)
    { PIN_TEXT("node_name", N->GetName()); PIN_TEXT("node_path", N->GetNodePath()); PIN_TEXT("node_title", N->GetNodeTitle()); }
    bool ProposedValid = false, Clamped = false; FString Reason;
    if (S.bProvided)
    {
        if (!SafeProposal(I, P, S.Proposed)) { return false; }
        bool ClassHandled = false;
        if (!ClassProposal(I, P, S.Proposed, J, ClassHandled, ProposedValid)) { return false; }
        if (!ClassHandled)
        {
            Count(C.R, TEXT("is_valid_default_calls")); ProposedValid = P->IsValidDefaultValue(S.Proposed);
            if (!I.Member(P)) { return false; }
        }
        C.R.Stage = TEXT("validate_rigvm_pin_defaults");
        Count(C.R, TEXT("clamp_default_calls")); const FString Value = P->ClampDefaultValueFromMetaData(S.Proposed);
        if (!I.Member(P) || !C.Text(J, TEXT("proposed_default_clamped_value"), Value)) { return false; }
        Clamped = Value != S.Proposed;
        const bool Can = P->CanProvideDefaultValue(); if (!I.Member(P)) { return false; }
        if (!Can) { Reason = TEXT("Pin cannot provide a default value"); }
        else if (!ProposedValid) { Reason = TEXT("Proposed default value is not valid for this RigVM pin"); }
    }
    else
    {
        const bool Can = P->CanProvideDefaultValue(); if (!I.Member(P)) { return false; }
        if (!Can) { Reason = TEXT("Pin cannot provide a default value"); }
    }
    J->SetBoolField(TEXT("proposed_default_is_valid"), ProposedValid); J->SetBoolField(TEXT("proposed_default_valid"), ProposedValid);
    J->SetBoolField(TEXT("proposed_default_clamped_by_metadata"), Clamped);
    const bool Can = P->CanProvideDefaultValue(); if (!I.Member(P)) { return false; }
    bool Candidate = Can && (!S.bProvided || ProposedValid);
    if (Candidate) { const bool Execute = P->IsExecuteContext(); if (!I.Member(P)) { return false; } Candidate = !Execute; }
    J->SetBoolField(TEXT("default_write_candidate_for_review"), Candidate);
    J->SetBoolField(TEXT("would_require_writer_review"), true);
#undef PIN_TEXT
#undef PIN_BOOL
    return C.Text(J, TEXT("failure_reason"), Reason);
}

}

Variable::FResult Validate(const FString& ControlRigPath, const FString& ControlRigAssetPath,
    const FString& ControlRigClassPath, const FString& PinDefaultsJson, const FString& DefaultsJson, const FString& PinsJson, int64 MaxPins)
{
    FResult R; Init(R);
    const int32 Limit = int32(FMath::Clamp<int64>(MaxPins, 0, 512));
    R.Data->SetStringField(TEXT("received_max_pins_decimal"), LexToString(MaxPins));
    R.Data->SetNumberField(TEXT("max_pins"), Limit);
    if (!IsEditorGameThread() || IsPlaySessionActive() || IsEngineExitRequested() || ReadInFlight)
    { return Fail(R, TEXT("rigvm_pin_defaults_editor_unavailable"), TEXT("An idle live editor game thread outside PIE/SIE is required; no load dispatched")); }
    TGuardValue<bool> Busy(ReadInFlight, true);
    R.Stage = TEXT("parse_pin_specs");
    TSharedPtr<FJsonValue> PinDefaults, Defaults, PinValues;
    if (!Decode(PinDefaultsJson, PinDefaults) || !Decode(DefaultsJson, Defaults) || !Decode(PinsJson, PinValues))
    { return Fail(R, TEXT("rigvm_pin_defaults_json_invalid"), TEXT("Each nullable array carrier must be valid JSON null/array, <=1Mi UE characters, no embedded NUL, nesting<=64")); }
    TArray<FSpec> Specs; FString Field, ParseError; int32 RequestedCount = 0; bool Truncated = false;
    const bool Parsed = Parse(PinDefaults, Defaults, PinValues, Limit, Specs, Field, RequestedCount, Truncated, ParseError);
    R.Data->SetStringField(TEXT("pin_specs_field"), Field); R.Data->SetNumberField(TEXT("requested_pin_count"), RequestedCount);
    R.Data->SetNumberField(TEXT("validated_pin_count"), Specs.Num()); R.Data->SetBoolField(TEXT("pins_truncated"), Truncated);
    if (!Parsed) { return Fail(R, TEXT("rigvm_pin_defaults_specs_invalid"), ParseError); }
    if (!InputPath(ControlRigPath) || !InputPath(ControlRigAssetPath) || !InputPath(ControlRigClassPath))
    { return Fail(R, TEXT("rigvm_pin_defaults_path_invalid"), TEXT("Paths require <=1Mi raw characters, no embedded NUL and normalized length<NAME_SIZE")); }
    // Original IsEmpty-before-trim selection. Whitespace masks later asset aliases before class fallback.
    const FString Path = ControlRigPath.IsEmpty() ? ControlRigAssetPath : ControlRigPath;
    FString Requested, Kind; UObject* Object = nullptr; R.Stage = TEXT("resolve_control_rig_input");
    if (!Path.TrimStartAndEnd().IsEmpty()) { Requested = Path; Kind = TEXT("asset"); }
    else if (!ControlRigClassPath.TrimStartAndEnd().IsEmpty()) { Requested = ControlRigClassPath; Kind = TEXT("generated_class"); }
    else { return Fail(R, TEXT("rigvm_pin_defaults_target_missing"), TEXT("Missing 'control_rig_path', 'control_rig_asset_path', or 'control_rig_class_path' parameter")); }
    R.Data->SetStringField(TEXT("requested_control_rig"), Requested); R.Stage = TEXT("load_control_rig");
    Object = Kind == TEXT("asset") ? LoadAsset(Requested, R) : LoadClass(Requested, R);
    TStrongObjectPtr<UObject> LoadedLifetime(Object);
    if (!Loaded(Object)) { return Fail(R, TEXT("rigvm_pin_defaults_target_invalid"), TEXT("ControlRig asset or generated class was not found or is not fully loaded")); }
    R.Stage = TEXT("resolve_control_rig"); UClass* InputClass = Cast<UClass>(Object);
    if (UClass* Class = InputClass)
    {
        if ((!Path.TrimStartAndEnd().IsEmpty() || Class->IsChildOf(UControlRig::StaticClass())) && Loaded(Class->ClassGeneratedBy))
        { Object = Class->ClassGeneratedBy; Kind = TEXT("generated_class"); }
        else { Object = nullptr; }
    }
    if (!Loaded(Object) || (!Cast<UControlRigBlueprint>(Object) && !Cast<UControlRigEditorAsset>(Object)))
    { return Fail(R, TEXT("rigvm_pin_defaults_target_invalid"), TEXT("Target has no ControlRig Blueprint/editor asset graph to validate")); }
    if (!Loaded(Object->GetPackage())) { return Fail(R, TEXT("rigvm_pin_defaults_package_invalid"), TEXT("Resolved editor asset package is unavailable")); }
    FRead C(R, Object); C.InputClass.Reset(InputClass);
    R.Data->SetStringField(TEXT("resolution_kind"), Kind); R.Data->SetStringField(TEXT("control_rig_asset_path"), C.AssetPath); C.Dirty();
    auto Failure = [&]() { C.Dirty(); return Fail(R, TEXT("rigvm_pin_defaults_observation_incomplete"), C.Error.IsEmpty() ? TEXT("Pin default observation is incomplete; partial data retained, no automatic retry") : C.Error); };
    if (!C.RawCurrent()) { C.Bad(TEXT("Editor asset changed before model acquisition")); return Failure(); }
    auto* Interface = Cast<IControlRigEditorAssetInterface>(C.Asset.Get());
    if (!Interface) { C.Bad(TEXT("Public ControlRig editor interface is unavailable")); return Failure(); }
    C.Client = Interface->GetRigVMClient();
    if (!C.RawCurrent() || !C.Client) { C.Bad(TEXT("RigVM client is unavailable or asset changed")); return Failure(); }
    R.Stage = TEXT("resolve_default_model"); Count(R, TEXT("default_model_calls")); C.Graph.Reset(C.Client->GetDefaultModel());
    if (!C.RawCurrent() || !Loaded(C.Graph.Get())) { C.Bad(TEXT("ControlRig Blueprint has no live default RigVM model")); return Failure(); }
    C.Hold(C.Graph.Get()); C.GraphClass = C.Graph->GetClass(); C.Hold(C.GraphClass);
    C.GraphOuter = C.Graph->GetOuter(); C.GraphPath = C.Graph->GetPathName();
    if (!C.Current()) { return Failure(); }
    FInventory Inventory(C);
    R.Stage = TEXT("capture_rigvm_pin_default_sources");
    // max_pins=0 preserves the original zero-proposal path and dispatches no default getter/validator.
    if (!Specs.IsEmpty() && !Inventory.Capture()) { return Failure(); }
    if (Specs.IsEmpty() && !Inventory.Parents(C.Graph.Get())) { return Failure(); }
    R.Stage = TEXT("validate_rigvm_pin_defaults"); TArray<TSharedPtr<FJsonValue>> Rows;
    int32 Found = 0, Missing = 0, Can = 0, Cannot = 0, Provided = 0, Valid = 0, Invalid = 0, Candidates = 0;
    for (int32 I = 0; I < Specs.Num(); ++I)
    {
        auto Row = MakeShared<FJsonObject>(); Rows.Add(MakeShared<FJsonValueObject>(Row));
        R.Data->SetArrayField(TEXT("results"), Rows); R.Data->SetNumberField(TEXT("included_pin_count"), Rows.Num());
        if (!Diagnostic(Inventory, Specs[I], I, Row)) { return Failure(); }
        bool F = false, CanDefault = false, P = false, V = false, Candidate = false;
        Row->TryGetBoolField(TEXT("pin_found"), F); Row->TryGetBoolField(TEXT("can_provide_default_value"), CanDefault);
        Row->TryGetBoolField(TEXT("proposed_default_provided"), P); Row->TryGetBoolField(TEXT("proposed_default_is_valid"), V);
        Row->TryGetBoolField(TEXT("default_write_candidate_for_review"), Candidate);
        Found += F ? 1 : 0; Missing += F ? 0 : 1; Can += CanDefault ? 1 : 0; Cannot += CanDefault ? 0 : 1;
        Provided += P ? 1 : 0; Valid += P && V ? 1 : 0; Invalid += P && !V ? 1 : 0; Candidates += Candidate ? 1 : 0;
        Count(R, TEXT("completed_diagnostics")); R.Data->SetNumberField(TEXT("completed_pin_count"), I + 1);
        R.Data->SetNumberField(TEXT("pin_found_count"), Found); R.Data->SetNumberField(TEXT("missing_pin_count"), Missing);
        R.Data->SetNumberField(TEXT("can_provide_default_count"), Can); R.Data->SetNumberField(TEXT("cannot_provide_default_count"), Cannot);
        R.Data->SetNumberField(TEXT("proposed_default_provided_count"), Provided); R.Data->SetNumberField(TEXT("proposed_default_valid_count"), Valid);
        R.Data->SetNumberField(TEXT("proposed_default_invalid_count"), Invalid); R.Data->SetNumberField(TEXT("default_write_candidate_for_review_count"), Candidates);
    }
    R.Stage = TEXT("verify_rigvm_pin_default_observation");
    if (!Inventory.Check() || !Inventory.Parents(C.Graph.Get())) { return Failure(); }
    Count(R, TEXT("asset_reference_calls")); const FControlRigAssetStrongReference Source = Interface->GetControlRigAssetReference();
    UClass* Class = Source.GetBlueprintClass().Get(); C.Hold(Class); UObject* RuntimeAsset = Source.GetControlRigAsset().Get(); C.Hold(RuntimeAsset);
    if (!Inventory.Check() || !Inventory.Parents(C.Graph.Get())) { return Failure(); }
    if (const UControlRigBlueprint* Legacy = Cast<UControlRigBlueprint>(C.Asset.Get()))
    {
        if (Class != Cast<URigVMBlueprintGeneratedClass>(Legacy->GeneratedClass.Get()))
        { C.Bad(TEXT("Public ControlRig asset reference differs from legacy generated-class result")); return Failure(); }
    }
    if (Class)
    {
        if (!C.SamePackage(Class) || !Cast<URigVMBlueprintGeneratedClass>(Class)) { C.Bad(TEXT("Generated class reference is unreadable")); return Failure(); }
        if (!C.Text(R.Data, TEXT("control_rig_class_path"), Class->GetPathName())) { return Failure(); }
    }
    if (RuntimeAsset)
    {
        if (!C.SamePackage(RuntimeAsset) || !C.Text(R.Data, TEXT("control_rig_runtime_asset_path"), RuntimeAsset->GetPathName())) { return Failure(); }
    }
    const FString GraphName = C.Graph->GetGraphName();
    if (!Inventory.Check() || !Inventory.Parents(C.Graph.Get()) || !C.Text(R.Data, TEXT("graph_name"), GraphName)) { return Failure(); }
    const FString GraphPath = C.Graph->GetNodePath();
    if (!Inventory.Check() || !Inventory.Parents(C.Graph.Get()) || !C.Text(R.Data, TEXT("graph_node_path"), GraphPath)) { return Failure(); }
    R.Data->SetBoolField(TEXT("actual_default_writes_supported"), false);
    R.Data->SetNumberField(TEXT("bounded_source_checks"), double(Inventory.Checks));
    for (const auto& Pair : C.Packages) { if (!C.SamePackage(Pair.Key)) { return Failure(); } }
    C.Dirty(); R.Data->SetBoolField(TEXT("success"), true);
    R.bSuccess = true; R.bPostconditionPassed = true; R.Stage = TEXT("rigvm_pin_default_validation_complete");
    return R;
}
}
