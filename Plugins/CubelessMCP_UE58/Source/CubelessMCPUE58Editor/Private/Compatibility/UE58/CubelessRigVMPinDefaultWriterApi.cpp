// UE56 proposal diagnostics contract, implemented with installed UE58 public APIs.
#include "Compatibility/UE58/CubelessRigVMPinDefaultWriterApi.h"
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
#include "RigVMTypeUtils.h"
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

namespace Cubeless::UE58::RigVMPinDefaultWriter
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
        TEXT("default_model_calls"),TEXT("asset_reference_calls"),TEXT("find_pin_calls"),TEXT("pin_default_projection_calls"),
        TEXT("is_valid_default_calls"),TEXT("clamp_default_calls"),TEXT("child_default_normalize_calls"),TEXT("child_default_normalize_returns"),TEXT("getter_identity_checks"),TEXT("completed_diagnostics"),
        TEXT("get_or_create_controller_calls"),TEXT("get_controller_calls"),TEXT("set_pin_default_calls"),TEXT("set_pin_default_returns_true"),
        TEXT("auto_vm_recompile_requests"),TEXT("mark_package_dirty_calls"),TEXT("request_init_calls"),
        TEXT("explicit_compile_calls"),TEXT("explicit_save_calls"),TEXT("explicit_modify_calls"),TEXT("runtime_execution_calls")})
    { Calls->SetNumberField(Key,0); }
    R.Data->SetObjectField(TEXT("native_call_counts"),Calls);
    R.Data->SetStringField(TEXT("command"),TEXT("set_controlrig_rigvm_pin_defaults"));
    for (const TCHAR* Key : {TEXT("sample_only"),TEXT("atomic_rollback_unavailable"),TEXT("total_SDK_effects_unknown")}) { R.Data->SetBoolField(Key,true); }
    for (const TCHAR* Key : {TEXT("target_modified"),TEXT("target_asset_modified"),TEXT("partial_target_modified"),TEXT("source_modified"),
        TEXT("automatic_action_retry"),TEXT("atomic_snapshot_verified"),TEXT("observation_incomplete"),
        TEXT("packaged_runtime_verified"),TEXT("unique_external_pipeline_complete"),TEXT("target_asset_saved")}) { R.Data->SetBoolField(Key,false); }
    for (const TCHAR* Key : {TEXT("pin_found_count"),TEXT("missing_pin_count"),TEXT("can_provide_default_count"),TEXT("cannot_provide_default_count"),
        TEXT("proposed_default_valid_count"),TEXT("proposed_default_invalid_count"),TEXT("already_matching_count"),TEXT("would_set_count"),
        TEXT("defaults_set_count"),TEXT("included_pin_count"),TEXT("completed_preflight_count"),TEXT("set_pin_default_attempt_count"),
        TEXT("observed_default_value_changes"),TEXT("observed_pins_added"),TEXT("observed_pins_removed")}) { R.Data->SetNumberField(Key,0); }
    R.Data->SetArrayField(TEXT("results"),{}); R.Data->SetArrayField(TEXT("completion_errors"),{});
    R.Data->SetStringField(TEXT("save_verification_state"),TEXT("not_attempted"));
    R.Data->SetStringField(TEXT("scope"),TEXT("sample/temp or isolated CubelessMCPTests ControlRig RigVM pin-default writer; array resize and linked-pin option are explicit and disabled by default; no original asset mutation, runtime code, execution or solve"));
    R.Data->SetStringField(TEXT("change_state_scope"),TEXT("Actual SetPinDefaultValue attempts/returns, observed default/slot deltas and captured-package dirty are separate. Load/PostLoad, controller creation, import/type/UI/cache and automatic compiler callbacks have unknown total effects. There is no atomic rollback or automatic retry."));
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
// Snapshot every captured pin because linked/default recursion and array resize can
// affect more than the directly requested pin. This is an observation, not rollback.
struct FDefaultsSnapshot
{
    TMap<URigVMPin*,FString> Values;
    TArray<URigVMLink*> Links;
};
bool Snapshot(FInventory& I,FDefaultsSnapshot& Out)
{
    int64 Chars=0;
    for(const auto& V:I.Pins)
    {
        TSet<URigVMPin*> Active;
        if(!PrepareDefaults(I,V.Pin,0,Active)){return false;}
        const FString Value=V.Pin->GetDefaultValue();
        if(!I.Member(V.Pin)||Value.Len()>SafetyChars||(Chars+=Value.Len())>SafetyOutputChars)
        {return I.C.Bad(TEXT("Default snapshot exceeds the finite string budget or lost pin identity"));}
        Out.Values.Add(V.Pin,Value);
    }
    Out.Links=I.C.Graph->GetLinks(); return I.Check();
}
void Delta(FRead& C,const FDefaultsSnapshot& Before,const FDefaultsSnapshot& After,const TSharedRef<FJsonObject>& Row)
{
    int32 Added=0,Removed=0,Changed=0;
    for(const auto& V:After.Values)
    {const FString* Old=Before.Values.Find(V.Key);if(!Old){++Added;}else if(*Old!=V.Value){++Changed;}}
    for(const auto& V:Before.Values){if(!After.Values.Contains(V.Key)){++Removed;}}
    Row->SetNumberField(TEXT("observed_default_value_changes"),Changed);
    Row->SetNumberField(TEXT("observed_pins_added"),Added);Row->SetNumberField(TEXT("observed_pins_removed"),Removed);
    for(const auto& V:TArray<TPair<FString,int32>>{{TEXT("observed_default_value_changes"),Changed},{TEXT("observed_pins_added"),Added},{TEXT("observed_pins_removed"),Removed}})
    {C.R.Data->SetNumberField(V.Key,C.R.Data->GetNumberField(V.Key)+V.Value);}
    const bool LinksChanged=Before.Links!=After.Links;
    Row->SetBoolField(TEXT("observed_graph_link_slots_changed"),LinksChanged);
    C.R.bChanged|=Added>0||Removed>0||Changed>0||LinksChanged;
}
bool WriterProposal(FInventory& I,URigVMPin* P,const FString& Proposal,bool Resize)
{
    if(!I.Member(P)||!SafeProposal(I,P,Proposal)){return false;}
    const FString Type=P->GetCPPType();if(!I.Member(P)){return false;}
    if(Type==RigVMTypeUtils::FNameType&&Proposal.Len()>=NAME_SIZE)
    {return I.C.Bad(TEXT("FName default proposal exceeds the public name bound; no SetPinDefaultValue dispatched"));}
    const bool Array=P->IsArray();if(!I.Member(P)){return false;}
    // Resolve the same public object/class values before any authoring dispatch so
    // a short redirect cannot conceal a direct dependency on our Editor plugin.
    UObject* TypeObject=P->GetCPPTypeObject();if(TypeObject&&!I.Object(TypeObject)){return false;}
    if(!I.Member(P)){return false;}
    if(Cast<UClass>(TypeObject))
    {
        TArray<FString> Values;if(Array){Values=URigVMPin::SplitDefaultValue(Proposal);}else{Values.Add(Proposal);}
        if(Values.Num()>SafetyRows||!I.Member(P)){return I.C.Bad(TEXT("Object default proposal exceeds the finite value inventory"));}
        for(const FString& Value:Values)
        {
            if(!I.Step()||!I.Member(P)){return false;}
            if(Value.IsEmpty()||Value==TEXT("None")){continue;}
            UObject* Resolved=RigVMTypeUtils::FindObjectFromCPPTypeObjectPath(Value);
            if(!I.Member(P)||(Resolved&&!I.Object(Resolved))){return false;}
            if(Resolved)
            {
                TArray<UObject*> Dependencies{Resolved,Resolved->GetClass()};
                if(UClass* Class=Cast<UClass>(Resolved)){Dependencies.Add(Class);}
                for(UObject* Object:Dependencies)
                {
                    TSet<UStruct*> Seen;
                    for(UObject* Item=Object;Item;)
                    {
                        if(!I.Object(Item)||!I.Member(P)){return false;}
                        const FString Package=Item->GetPackage()->GetName();
                        if(Package.StartsWith(TEXT("/Script/Cubeless"))||Package.StartsWith(TEXT("/Script/UnrealMCP"))||Package.StartsWith(TEXT("/CubelessMCP_UE58/")))
                        {return I.C.Bad(TEXT("Resolved default object/class depends on Cubeless/MCP; no setter dispatched"));}
                        UStruct* Struct=Cast<UStruct>(Item);if(!Struct){break;}
                        if(Seen.Contains(Struct)||Seen.Num()>=SafetyDepth){return I.C.Bad(TEXT("Resolved default superclass chain is cyclic or beyond the finite bound"));}
                        Seen.Add(Struct);Item=Struct->GetSuperStruct();
                    }
                }
            }
        }
    }
    return I.Member(P);
}
// YJ: UE58 compatibility wrapper for the installed protected static SDK hook.
// No Controller instance, UObject subclass registration, base-to-derived cast or
// Engine API modification is involved; the helper is never instantiated.
struct FChildDefaultNormalizationAccess : URigVMController
{
    static void Normalize(const URigVMPin* Pin,FString& Value)
    { PostProcessDefaultValue(Pin,Value); }
};
// SDK SetPinDefaultValue normalizes each recursive child before applying it
// (RigVMController.cpp10385/10411). Reuse that hook rather than guessing the
// constructor-generated defaults of nested structs containing arrays.
bool ChildText(FInventory& I,URigVMPin* Pin,FString& Value,bool Resize)
{
    (void)Resize; // Existing recursive setter closure still enforces resize=False sizes.
    if(!I.Member(Pin))return false;
    if(!ProposalBound(Value))return I.C.Bad(TEXT("Child default normalization input exceeds the finite proposal bounds"));
    const bool IsStruct=Pin->IsStruct();if(!I.Member(Pin))return false;
    UScriptStruct* Type=IsStruct?Pin->GetScriptStruct():nullptr;
    if(!I.Member(Pin)||(Type&&!I.Object(Type)))return false;
    auto& R=I.C.R;
    R.Data->SetStringField(TEXT("child_default_normalization_scope"),TEXT("Cubeless UE58 non-instantiated forwarding wrapper for protected static URigVMController::PostProcessDefaultValue, not a new Engine public API. Recursive preflight may invoke SDK struct default constructors/export or type/default getters before the target setter; it is not pure, not a target writer, and total opaque SDK effects remain unknown. Actual setter repeats its own SDK normalization; no original input is rewritten."));
    R.Stage=TEXT("preflight_child_default_normalization");
    R.Data->SetBoolField(TEXT("child_default_normalization_entered"),true);
    R.Data->SetBoolField(TEXT("child_default_normalization_returned"),false);
    R.Data->SetBoolField(TEXT("child_default_normalization_output_adopted"),false);
    Count(R,TEXT("child_default_normalize_calls"));
    FChildDefaultNormalizationAccess::Normalize(Pin,Value);
    Count(R,TEXT("child_default_normalize_returns"));
    R.Data->SetNumberField(TEXT("last_child_normalized_text_chars"),Value.Len());
    R.Data->SetBoolField(TEXT("child_default_normalization_returned"),true);
    R.Data->SetBoolField(TEXT("child_default_normalization_output_adopted"),false);
    // The inventory owns the pin/type through the opaque call. Do not inspect the
    // receiver or normalized output as current until the existing owner fence closes.
    if(!I.Member(Pin)||(Type&&!I.Identity(Type)))return false;
    if(!ProposalBound(Value))return I.C.Bad(TEXT("SDK-normalized child default exceeds the finite text/nesting bounds; no target setter was dispatched"));
    R.Data->SetBoolField(TEXT("child_default_normalization_output_adopted"),true);
    return true;
}
bool SetterClosure(FInventory& I,FControllerView& Controller,URigVMPin* Pin,const FString& Proposal,bool Resize,
    int32 Depth,TSet<URigVMPin*>& Active,int32& Visits)
{
    if(!I.Step()||Depth>SafetyDepth||++Visits>SafetyRows||Active.Contains(Pin)||!I.Member(Pin)||!Controller.Current())
    {return I.C.Bad(TEXT("Setter proposal closure is cyclic, unreadable or exceeds finite depth/rows"));}
    Active.Add(Pin);
    auto Exit=[&](bool Result){Active.Remove(Pin);return Result;};
    if(!WriterProposal(I,Pin,Proposal,Resize)){return Exit(false);}
    bool Handled=false,Valid=false;auto ClassReport=MakeShared<FJsonObject>();
    if(!ClassProposal(I,Pin,Proposal,ClassReport,Handled,Valid)){return Exit(false);}
    if(Handled&&!Valid){return Exit(I.C.Bad(TEXT("Nested setter class proposal is invalid; assertion-bearing SDK path was not dispatched")));}
    const bool Root=Pin->IsRootPin();if(!I.Member(Pin)){return Exit(false);}
    if(Root){Count(I.C.R,TEXT("clamp_default_calls"));}
    const FString Value=Root?Pin->ClampDefaultValueFromMetaData(Proposal):Proposal;
    if(!I.Member(Pin)||!ProposalBound(Value)){return Exit(I.C.Bad(TEXT("Clamped setter proposal exceeds finite text/nesting bounds")));}
    const bool Array=Pin->IsArray();if(!I.Member(Pin)){return Exit(false);}
    if(Array)
    {
        URigVMSchema* Schema=Controller.Controller->GetSchema();
        if(!I.Object(Schema)||!I.Member(Pin)||!Controller.Current()){return Exit(false);}
        double UnfoldCalls=0;I.C.R.Data->TryGetNumberField(TEXT("setter_can_unfold_pin_calls"),UnfoldCalls);
        I.C.R.Data->SetNumberField(TEXT("setter_can_unfold_pin_calls"),UnfoldCalls+1);
        const bool Unfold=Schema->CanUnfoldPin(Controller.Controller.Get(),Pin);
        if(!I.Identity(Schema)||!I.Member(Pin)||!Controller.Current()){return Exit(false);}
        if(Unfold)
        {
            const TArray<FString> Values=URigVMPin::SplitDefaultValue(Value);
            if(!I.Member(Pin)||Values.Num()>SafetyRows){return Exit(I.C.Bad(TEXT("Setter array proposal exceeds finite rows")));}
            if(!Resize&&Values.Num()!=Pin->GetSubPins().Num())
            {return Exit(I.C.Bad(TEXT("Nested array size differs while resize_arrays=False; SDK ensure path was not dispatched")));}
            for(int32 Index=0;Index<Values.Num();++Index)
            {
                URigVMPin* Child=Pin->FindSubPin(FString::FromInt(Index));
                if(!I.Member(Pin)||(Child&&!I.Member(Child))){return Exit(false);}
                // With resize=True, the SDK may create new slots. This preflight covers
                // existing slots; all recursive setters inherit resize=True, so the
                // guarded size-equality ensure is not entered for generated slots.
                if(!Child){continue;}
                FString ChildValue=Values[Index];if(!ChildText(I,Child,ChildValue,Resize)){return Exit(false);}
                const bool Wild=Child->IsWildCard();if(!I.Member(Child)){return Exit(false);}
                const bool Object=Child->IsUObject();if(!I.Member(Child)){return Exit(false);}
                if((!ChildValue.IsEmpty()||Wild||Object)&&!SetterClosure(I,Controller,Child,ChildValue,Resize,Depth+1,Active,Visits)){return Exit(false);}
            }
        }
    }
    else
    {
        const bool Struct=Pin->IsStruct();if(!I.Member(Pin)){return Exit(false);}
        if(Struct)
        {
            const TArray<FString> Pairs=URigVMPin::SplitDefaultValue(Value);
            if(!I.Member(Pin)||Pairs.Num()>SafetyRows){return Exit(I.C.Bad(TEXT("Setter struct proposal exceeds finite rows")));}
            for(const FString& Pair:Pairs)
            {
                FString Name,ChildValue;if(!Pair.Split(TEXT("="),&Name,&ChildValue)){continue;}
                if(Name.Len()>=NAME_SIZE){return Exit(I.C.Bad(TEXT("Nested setter member path exceeds the SDK name bound")));}
                URigVMPin* Child=Pin->FindSubPin(Name);
                if(!I.Member(Pin)||(Child&&!I.Member(Child))){return Exit(false);}
                if(!Child||ChildValue.IsEmpty()){continue;}
                if(!ChildText(I,Child,ChildValue,Resize)){return Exit(false);}
                const bool Wild=Child->IsWildCard();if(!I.Member(Child)){return Exit(false);}
                const bool Object=Child->IsUObject();if(!I.Member(Child)){return Exit(false);}
                if((!ChildValue.IsEmpty()||Wild||Object)&&!SetterClosure(I,Controller,Child,ChildValue,Resize,Depth+1,Active,Visits)){return Exit(false);}
            }
        }
    }
    return Exit(I.Member(Pin)&&Controller.Current());
}
bool WriterDispatchClosure(FInventory& I,FControllerView& Controller,URigVMPin* Pin,const FString& Proposal,bool Resize)
{
    I.C.R.Stage=TEXT("preflight_setter_proposal_closure");TSet<URigVMPin*> Active;int32 Visits=0;
    if(!SetterClosure(I,Controller,Pin,Proposal,Resize,0,Active,Visits)){return false;}
    // FString overload also calls the pointer setter on GetPinForLink's injected
    // target. Validate its source endpoints before the SDK dereferences them.
    URigVMPin* Root=Pin;
    for(int32 Depth=0;;++Depth)
    {
        if(Depth>SafetyDepth||!I.Member(Root)){return I.C.Bad(TEXT("Setter pin-for-link root exceeds finite parent depth"));}
        URigVMPin* Parent=Root->GetParentPin();if(!Parent){break;}Root=Parent;
    }
    const auto Injections=Root->GetInjectedNodes();if(!I.Member(Root)){return false;}
    if(!Injections.IsEmpty())
    {
        URigVMInjectionInfo* Info=Injections.Last();if(!I.Identity(Info)){return false;}
        const ERigVMPinDirection Direction=Pin->GetDirection();if(!I.Member(Pin)){return false;}
        URigVMPin* Target=(Direction==ERigVMPinDirection::Input||Direction==ERigVMPinDirection::IO)?Info->InputPin.Get():Info->OutputPin.Get();
        if(!Target||!I.Member(Target)){return I.C.Bad(TEXT("Setter pin-for-link has no live injected endpoint"));}
    }
    URigVMPin* Other=Pin->GetPinForLink();if(!I.Member(Pin)||!Other||!I.Member(Other)||!Controller.Current())
    {return I.C.Bad(TEXT("Setter pin-for-link target changed or is unreadable"));}
    if(Other!=Pin&&!SetterClosure(I,Controller,Other,Proposal,Resize,0,Active,Visits)){return false;}
    I.C.R.Data->SetNumberField(TEXT("last_setter_proposal_closure_visits"),Visits);
    I.C.R.Data->SetBoolField(TEXT("setter_injected_target_closure_checked"),true);
    return I.Member(Pin)&&I.Member(Other)&&Controller.Current();
}
bool ReadAfter(FInventory& I,const FString& Path,const TSharedRef<FJsonObject>& Row,const TCHAR* ValueKey,const TCHAR* UIKey)
{
    Count(I.C.R,TEXT("find_pin_calls"));URigVMPin* Pin=I.C.Graph->FindPin(Path);
    if(!I.Check()||(Pin&&!I.Member(Pin))){return false;}
    if(!Pin){Row->SetBoolField(TEXT("postcondition_pin_present"),false);return true;}
    Row->SetBoolField(TEXT("postcondition_pin_present"),true);TSet<URigVMPin*> Active;
    if(!PrepareDefaults(I,Pin,0,Active)){return false;}
    const FString Value=Pin->GetDefaultValue();if(!I.Member(Pin)||!I.C.Text(Row,ValueKey,Value)){return false;}
    Active.Reset();if(!PrepareDefaults(I,Pin,0,Active)){return false;}
    const FString UI=Pin->GetDefaultValueStoredByUserInterface();return I.Member(Pin)&&I.C.Text(Row,UIKey,UI);
}
}

Variable::FResult Set(const FString& ControlRigPath,const FString& ControlRigAssetPath,const FString& ControlRigClassPath,
    const FString& PinDefaultsJson,const FString& DefaultsJson,bool bResizeArrays,bool bSetValueOnLinkedPins,
    bool bDryRun,bool bCompile,bool bSaveAsset,int64 MaxPins)
{
    FResult R;Init(R);R.bCompileRequested=bCompile;R.bSaveRequested=bSaveAsset;
    const int32 Limit=int32(FMath::Clamp<int64>(MaxPins,0,512));
    R.Data->SetStringField(TEXT("received_max_pins_decimal"),LexToString(MaxPins));R.Data->SetNumberField(TEXT("max_pins"),Limit);
    R.Data->SetBoolField(TEXT("dry_run"),bDryRun);R.Data->SetBoolField(TEXT("compile_requested"),bCompile);R.Data->SetBoolField(TEXT("save_requested"),bSaveAsset);
    R.Data->SetBoolField(TEXT("resize_arrays"),bResizeArrays);R.Data->SetBoolField(TEXT("set_value_on_linked_pins"),bSetValueOnLinkedPins);
    if(!IsEditorGameThread()||IsPlaySessionActive()||IsEngineExitRequested()||ReadInFlight)
    {return Fail(R,TEXT("rigvm_pin_default_writer_editor_unavailable"),TEXT("Idle live editor game thread outside PIE/SIE required; no SDK load/writer dispatched"));}
    TGuardValue<bool> Busy(ReadInFlight,true);R.Stage=TEXT("parse_pin_specs");TSharedPtr<FJsonValue> PinDefaults,Defaults;
    if(!Decode(PinDefaultsJson,PinDefaults)||!Decode(DefaultsJson,Defaults))
    {return Fail(R,TEXT("rigvm_pin_default_writer_json_invalid"),TEXT("Nullable array carriers require JSON null/array, each<=1Mi UE chars/no NUL/nesting<=64"));}
    TArray<FSpec> Specs;FString Field,ParseError;int32 Requested=0;bool Truncated=false;
    const bool Parsed=Parse(PinDefaults,Defaults,MakeShared<FJsonValueNull>(),Limit,Specs,Field,Requested,Truncated,ParseError);
    R.Data->SetStringField(TEXT("pin_specs_field"),Field);R.Data->SetNumberField(TEXT("requested_pin_count"),Requested);
    R.Data->SetNumberField(TEXT("validated_pin_count"),Specs.Num());R.Data->SetBoolField(TEXT("pins_truncated"),Truncated);
    if(!Parsed){return Fail(R,TEXT("rigvm_pin_default_writer_specs_invalid"),ParseError);}
    for(int32 I=0;I<Specs.Num();++I)
    {
        if(!Specs[I].bProvided){return Fail(R,TEXT("rigvm_pin_default_writer_proposal_missing"),FString::Printf(TEXT("%s[%d] must include default_value/value/default for set_controlrig_rigvm_pin_defaults"),*Field,I));}
        // Conservative dependency admission: arbitrary struct/object export strings must not
        // introduce a direct Cubeless/MCP type or plugin-content reference into saved game data.
        const FString& Value=Specs[I].Proposed;
        if(Value.Contains(TEXT("/Script/Cubeless"),ESearchCase::IgnoreCase)||Value.Contains(TEXT("/Script/UnrealMCP"),ESearchCase::IgnoreCase)||Value.Contains(TEXT("/CubelessMCP_UE58/"),ESearchCase::IgnoreCase))
        {return Fail(R,TEXT("rigvm_pin_default_writer_runtime_dependency_forbidden"),TEXT("Proposal contains a Cubeless/MCP type or plugin-content path; no load or authoring dispatched"));}
    }
    if(!InputPath(ControlRigPath)||!InputPath(ControlRigAssetPath)||!InputPath(ControlRigClassPath))
    {return Fail(R,TEXT("rigvm_pin_default_writer_path_invalid"),TEXT("Target paths exceed finite NUL/length bounds"));}
    const FString Path=ControlRigPath.IsEmpty()?ControlRigAssetPath:ControlRigPath;FString RequestedPath,Kind;UObject* LoadedObject=nullptr;
    if(!Path.TrimStartAndEnd().IsEmpty()){RequestedPath=Path;Kind=TEXT("asset");}
    else if(!ControlRigClassPath.TrimStartAndEnd().IsEmpty()){RequestedPath=ControlRigClassPath;Kind=TEXT("generated_class");}
    else{return Fail(R,TEXT("rigvm_pin_default_writer_target_missing"),TEXT("Missing 'control_rig_path', 'control_rig_asset_path', or 'control_rig_class_path' parameter"));}
    R.Data->SetStringField(TEXT("requested_control_rig"),RequestedPath);R.Stage=TEXT("load_control_rig");
    LoadedObject=Kind==TEXT("asset")?LoadAsset(RequestedPath,R):LoadClass(RequestedPath,R);TStrongObjectPtr<UObject> LoadLifetime(LoadedObject);
    if(!Loaded(LoadedObject)){return Fail(R,TEXT("rigvm_pin_default_writer_target_invalid"),TEXT("ControlRig asset or generated class was not found or is not fully loaded"));}
    UObject* Asset=LoadedObject;UClass* InputClass=Cast<UClass>(LoadedObject);
    if(InputClass)
    {Asset=((!Path.TrimStartAndEnd().IsEmpty()||InputClass->IsChildOf(UControlRig::StaticClass()))&&Loaded(InputClass->ClassGeneratedBy))?InputClass->ClassGeneratedBy:nullptr;Kind=TEXT("generated_class");}
    R.Stage=TEXT("admit_legacy_writer_target");UControlRigBlueprint* Blueprint=Cast<UControlRigBlueprint>(Asset);
    if(!Loaded(Asset)||(!Blueprint&&!Cast<UControlRigEditorAsset>(Asset)))
    {return Fail(R,TEXT("rigvm_pin_default_writer_target_invalid"),TEXT("Resolved input is not a live ControlRig Blueprint/editor asset"));}
    if(!Loaded(Blueprint))
    {R.Data->SetBoolField(TEXT("new_editor_asset_writer_supported"),false);return Fail(R,TEXT("rigvm_pin_default_writer_legacy_blueprint_required"),TEXT("Writer requires a legacy ControlRigBlueprint. New ControlRigEditorAsset protected compile contract is not assumed equivalent; no explicit writer/compile/save dispatched, load effects unknown"));}
    if(!Loaded(Blueprint->GetPackage())){return Fail(R,TEXT("rigvm_pin_default_writer_package_invalid"),TEXT("Target package is unreadable"));}
    const FString TargetPackage=PackagePath(Blueprint->GetPackage()->GetName());R.Data->SetStringField(TEXT("target_package_path"),TargetPackage);
    if(!Allowed(TargetPackage)||!Allowed(PackagePath(Blueprint->GetPathName()))||Blueprint->GetOuter()!=Blueprint->GetPackage())
    {return Fail(R,TEXT("rigvm_pin_default_writer_target_forbidden"),TEXT("Asset path and actual top-level owning package must be admitted sample/temp/test roots"));}
    FRead C(R,Blueprint);C.InputClass.Reset(InputClass);R.Data->SetStringField(TEXT("control_rig_asset_path"),C.AssetPath);R.Data->SetStringField(TEXT("resolution_kind"),Kind);C.Dirty();
    int32 SetCount=0;bool AllApplied=true,AllWritable=true,CompileOK=true,SaveOK=true;
    auto CompileResult=MakeShared<FJsonObject>();CompileResult->SetBoolField(TEXT("requested"),false);R.Data->SetObjectField(TEXT("compile_result"),CompileResult);
    TArray<TSharedPtr<FJsonValue>> CompletionErrors;
    auto Prefix=[&]()
    {
        C.Dirty();R.Data->SetNumberField(TEXT("defaults_set_count"),SetCount);const bool Modified=SetCount>0||R.bChanged;
        R.Data->SetBoolField(TEXT("target_modified"),Modified);R.Data->SetBoolField(TEXT("target_asset_modified"),Modified);
        R.Data->SetBoolField(TEXT("partial_target_modified"),Modified&&(!AllApplied||!CompileOK||!SaveOK||!C.Error.IsEmpty()));
        R.Data->SetBoolField(TEXT("target_asset_saved"),R.bSaved);R.Data->SetArrayField(TEXT("completion_errors"),CompletionErrors);
    };
    auto Failure=[&](const TCHAR* Code,const FString& Message){AllApplied=false;Prefix();return Fail(R,Code,Message);};
    auto ObservationFailure=[&](){return Failure(TEXT("rigvm_pin_default_writer_observation_incomplete"),C.Error.IsEmpty()?TEXT("Writer observation incomplete; partial prefix retained, no rollback/retry"):C.Error);};
    if(!C.RawCurrent()){C.Bad(TEXT("Target changed before model acquisition"));return ObservationFailure();}
    auto* Interface=Cast<IControlRigEditorAssetInterface>(Blueprint);C.Client=Interface?Interface->GetRigVMClient():nullptr;
    if(!C.RawCurrent()||!C.Client){C.Bad(TEXT("Public ControlRig client unavailable"));return ObservationFailure();}
    Count(R,TEXT("default_model_calls"));C.Graph.Reset(C.Client->GetDefaultModel());
    if(!C.RawCurrent()||!Loaded(C.Graph.Get())){C.Bad(TEXT("ControlRig Blueprint has no default RigVM model"));return ObservationFailure();}
    C.Hold(C.Graph.Get());C.GraphClass=C.Graph->GetClass();C.Hold(C.GraphClass);C.GraphOuter=C.Graph->GetOuter();C.GraphPath=C.Graph->GetPathName();
    if(!C.Current()){return ObservationFailure();}
    auto FileBefore=MakeShared<FJsonObject>();const bool DiskBeforeKnown=Disk(TargetPackage,FileBefore);R.Data->SetObjectField(TEXT("target_file_before"),FileBefore);R.Data->SetBoolField(TEXT("target_file_before_known"),DiskBeforeKnown);
    R.Stage=TEXT("acquire_rigvm_controller");Count(R,TEXT("get_or_create_controller_calls"));R.Data->SetBoolField(TEXT("controller_acquisition_may_create_editor_object"),true);
    URigVMController* RawController=Blueprint->GetOrCreateController(C.Graph.Get());C.Hold(RawController);
    if(!C.Current()||!Loaded(RawController)){C.Bad(TEXT("Default model controller unavailable or target changed"));return ObservationFailure();}
    FControllerView Controller(C,RawController);if(!Controller.Current()){return ObservationFailure();}
    R.Stage=TEXT("preflight_all_rigvm_pin_defaults");FInventory Preflight(C);
    if(!Specs.IsEmpty()&&(!Preflight.Capture()||!WritableInventory(Preflight))){return ObservationFailure();}
    if(Specs.IsEmpty()&&!Preflight.Parents(C.Graph.Get())){return ObservationFailure();}
    TArray<TSharedRef<FJsonObject>> PinRows;TArray<TSharedPtr<FJsonValue>> Rows;
    int32 Found=0,Missing=0,CanDefaultCount=0,CannotDefaultCount=0,ValidCount=0,InvalidCount=0,Already=0,WouldSet=0;
    for(int32 I=0;I<Specs.Num();++I)
    {
        auto Row=MakeShared<FJsonObject>();PinRows.Add(Row);Rows.Add(MakeShared<FJsonValueObject>(Row));R.Data->SetArrayField(TEXT("results"),Rows);R.Data->SetNumberField(TEXT("included_pin_count"),Rows.Num());
        if(!Diagnostic(Preflight,Specs[I],I,Row)||!Controller.Current()){return ObservationFailure();}
        const bool PinFound=Row->GetBoolField(TEXT("pin_found"));bool CanDefault=false,Valid=false,Candidate=false;FString Value;
        Row->TryGetBoolField(TEXT("can_provide_default_value"),CanDefault);Row->TryGetBoolField(TEXT("proposed_default_is_valid"),Valid);
        Row->TryGetBoolField(TEXT("default_write_candidate_for_review"),Candidate);Row->TryGetStringField(TEXT("current_default_value"),Value);
        const bool Matching=PinFound&&Value==Specs[I].Proposed,WillSet=Candidate&&!Matching,Writable=Candidate||Matching;
        Row->SetBoolField(TEXT("already_matching"),Matching);Row->SetBoolField(TEXT("will_set_default"),WillSet);Row->SetBoolField(TEXT("writer_candidate"),Writable);Row->SetBoolField(TEXT("applied"),false);
        if(WillSet)
        {
            Count(R,TEXT("find_pin_calls"));URigVMPin* Pin=C.Graph->FindPin(Specs[I].Path);
            if(!Preflight.Check()||!Pin||!Preflight.Member(Pin)||!WriterDispatchClosure(Preflight,Controller,Pin,Specs[I].Proposed,bResizeArrays)||!Controller.Current()){return ObservationFailure();}
        }
        Found+=PinFound?1:0;Missing+=PinFound?0:1;CanDefaultCount+=CanDefault?1:0;CannotDefaultCount+=CanDefault?0:1;
        ValidCount+=Valid?1:0;InvalidCount+=Valid?0:1;Already+=Matching?1:0;WouldSet+=WillSet?1:0;AllWritable&=Writable;
        R.Data->SetNumberField(TEXT("pin_found_count"),Found);R.Data->SetNumberField(TEXT("missing_pin_count"),Missing);
        R.Data->SetNumberField(TEXT("can_provide_default_count"),CanDefaultCount);R.Data->SetNumberField(TEXT("cannot_provide_default_count"),CannotDefaultCount);
        R.Data->SetNumberField(TEXT("proposed_default_valid_count"),ValidCount);R.Data->SetNumberField(TEXT("proposed_default_invalid_count"),InvalidCount);
        R.Data->SetNumberField(TEXT("already_matching_count"),Already);R.Data->SetNumberField(TEXT("would_set_count"),WouldSet);R.Data->SetNumberField(TEXT("completed_preflight_count"),I+1);
    }
    R.Data->SetBoolField(TEXT("all_specs_writable"),AllWritable);
    if(!AllWritable){CompletionErrors.Add(MakeShared<FJsonValueString>(TEXT("preflight_not_writable")));return Failure(TEXT("rigvm_pin_default_writer_not_writable"),TEXT("One or more requested RigVM pin defaults are not writable; no defaults were set"));}
    if(!bDryRun)
    {
        if(C.InputClass.IsValid()){R.Data->SetStringField(TEXT("resolved_input_class_before_writer"),C.InputClass->GetPathName());}C.InputClass.Reset();
        for(int32 I=0;I<Specs.Num();++I)
        {
            auto Row=PinRows[I];if(!Row->GetBoolField(TEXT("will_set_default"))){continue;}
            R.Stage=TEXT("apply_rigvm_pin_default");FInventory CurrentInventory(C);
            if(!Controller.Current()||!CurrentInventory.Capture()||!WritableInventory(CurrentInventory)){return ObservationFailure();}
            Count(R,TEXT("find_pin_calls"));URigVMPin* Pin=C.Graph->FindPin(Specs[I].Path);
            if(!CurrentInventory.Check()||(Pin&&!CurrentInventory.Member(Pin))){return ObservationFailure();}
            if(!Pin){AllApplied=false;Row->SetStringField(TEXT("apply_failure_reason"),TEXT("Pin disappeared after preflight; no setter dispatched"));continue;}
            if(!WriterDispatchClosure(CurrentInventory,Controller,Pin,Specs[I].Proposed,bResizeArrays)){return ObservationFailure();}
            // Revalidate the current pin/type immediately before the SDK setter. This closes
            // observed inter-row replacement and rejects loaded non-UClass fatal proposals.
            auto Recheck=MakeShared<FJsonObject>();if(!Diagnostic(CurrentInventory,Specs[I],I,Recheck)||!Controller.Current()){return ObservationFailure();}
            if(!Recheck->GetBoolField(TEXT("default_write_candidate_for_review")))
            {AllApplied=false;Row->SetStringField(TEXT("apply_failure_reason"),TEXT("Pin default is no longer writable after earlier callbacks; no setter dispatched"));continue;}
            FDefaultsSnapshot Before;if(!Snapshot(CurrentInventory,Before)||!Controller.Current()){return ObservationFailure();}
            R.bMutationAttempted=true;Count(R,TEXT("set_pin_default_calls"));R.Data->SetNumberField(TEXT("set_pin_default_attempt_count"),R.Data->GetObjectField(TEXT("native_call_counts"))->GetNumberField(TEXT("set_pin_default_calls")));
            Row->SetBoolField(TEXT("set_pin_default_dispatched"),true);
            const bool Applied=Controller.Controller->SetPinDefaultValue(Specs[I].Path,Specs[I].Proposed,bResizeArrays,false,false,false,bSetValueOnLinkedPins);
            Row->SetBoolField(TEXT("set_pin_default_returned"),true);Row->SetBoolField(TEXT("applied"),Applied);
            if(Applied){++SetCount;Count(R,TEXT("set_pin_default_returns_true"));}else{AllApplied=false;Row->SetStringField(TEXT("apply_failure_reason"),TEXT("URigVMController::SetPinDefaultValue returned false"));}
            R.Data->SetNumberField(TEXT("defaults_set_count"),SetCount);
            if(!Controller.Current()){return ObservationFailure();}FInventory After(C);FDefaultsSnapshot AfterValues;
            if(!After.Capture()||!WritableInventory(After)||!Snapshot(After,AfterValues)){return ObservationFailure();}
            Delta(C,Before,AfterValues,Row);
            if(!ReadAfter(After,Specs[I].Path,Row,TEXT("default_value_after"),TEXT("default_value_ui_after"))){return ObservationFailure();}
            if(Applied&&!Row->GetBoolField(TEXT("postcondition_pin_present"))){AllApplied=false;Row->SetStringField(TEXT("postcondition_error"),TEXT("SDK returned true but the requested pin disappeared after callbacks"));}
            Prefix();
        }
        if(SetCount>0)
        {
            R.Stage=TEXT("request_auto_vm_recompilation");if(!Controller.Current()){return ObservationFailure();}
            Count(R,TEXT("auto_vm_recompile_requests"));Blueprint->RequestAutoVMRecompilation();if(!Controller.Current()){return ObservationFailure();}
            R.Stage=TEXT("mark_target_package_dirty");Count(R,TEXT("mark_package_dirty_calls"));R.Data->SetBoolField(TEXT("mark_package_dirty_returned"),Blueprint->MarkPackageDirty());
            if(!Controller.Current()){return ObservationFailure();}Prefix();
        }
    }
    const bool EffectiveCompile=bCompile&&!bDryRun&&SetCount>0,EffectiveSave=bSaveAsset&&!bDryRun&&SetCount>0;
    CompileResult->SetBoolField(TEXT("requested"),EffectiveCompile);
    if(EffectiveCompile)
    {
        R.Stage=TEXT("request_control_rig_init");if(!Controller.Current()){return ObservationFailure();}
        Count(R,TEXT("request_init_calls"));Blueprint->RequestRigVMInit();CompileResult->SetBoolField(TEXT("init_request_returned"),true);if(!Controller.Current()){return ObservationFailure();}
        R.Stage=TEXT("compile_control_rig_blueprint");R.bCompileAttempted=true;Count(R,TEXT("explicit_compile_calls"));FCompilerResultsLog Log;
        FKismetEditorUtilities::CompileBlueprint(Blueprint,EBlueprintCompileOptions::None,&Log);CompileResult->SetBoolField(TEXT("compiler_returned"),true);
        CompileResult->SetNumberField(TEXT("num_errors"),Log.NumErrors);CompileResult->SetNumberField(TEXT("num_warnings"),Log.NumWarnings);if(!Controller.Current()){return ObservationFailure();}
        UClass* Generated=Blueprint->GeneratedClass.Get();C.Hold(Generated);CompileOK=Log.NumErrors==0&&Blueprint->Status!=BS_Error&&Loaded(Generated)&&!Generated->HasAnyClassFlags(CLASS_NewerVersionExists);
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
        R.Data->SetStringField(TEXT("save_verification_state"),SaveOK?TEXT("API_file_and_dirty_verified_no_reload"):TEXT("failed_or_unverified"));R.Data->SetBoolField(TEXT("saved_default_reload_verified"),false);
        if(!SaveOK){CompletionErrors.Add(MakeShared<FJsonValueString>(TEXT("save_failed")));}
    }
    R.Stage=TEXT("verify_rigvm_pin_default_writer");if(!Controller.Current()){return ObservationFailure();}
    FInventory Final(C);if(!Specs.IsEmpty()&&(!Final.Capture()||!WritableInventory(Final))){return ObservationFailure();}
    if(Specs.IsEmpty()&&!Final.Parents(C.Graph.Get())){return ObservationFailure();}
    if(!bDryRun)
    {
        for(int32 I=0;I<Specs.Num();++I)
        {
            auto Row=PinRows[I];if(!Row->GetBoolField(TEXT("already_matching"))&&!Row->GetBoolField(TEXT("applied"))){continue;}
            if(!ReadAfter(Final,Specs[I].Path,Row,TEXT("final_default_value"),TEXT("final_default_value_ui"))){return ObservationFailure();}
            if(!Row->GetBoolField(TEXT("postcondition_pin_present"))){AllApplied=false;Row->SetStringField(TEXT("final_postcondition_error"),TEXT("A later write/compiler callback removed the requested pin"));}
        }
    }
    Count(R,TEXT("asset_reference_calls"));const FControlRigAssetStrongReference AssetSource=Interface->GetControlRigAssetReference();UClass* Generated=AssetSource.GetBlueprintClass().Get();C.Hold(Generated);
    if(!Controller.Current()||Generated!=Cast<URigVMBlueprintGeneratedClass>(Blueprint->GeneratedClass.Get())){C.Bad(TEXT("Public generated-class reference changed or differs from legacy target"));return ObservationFailure();}
    if(Generated){if(!C.SamePackage(Generated)){return ObservationFailure();}R.Data->SetStringField(TEXT("control_rig_class_path"),Generated->GetPathName());}
    const FString GraphName=C.Graph->GetGraphName();if(!Controller.Current()){return ObservationFailure();}const FString GraphPath=C.Graph->GetNodePath();if(!Controller.Current()||!Final.Check()){return ObservationFailure();}
    R.Data->SetStringField(TEXT("graph_name"),GraphName);R.Data->SetStringField(TEXT("graph_node_path"),GraphPath);
    R.Data->SetBoolField(TEXT("compile_succeeded"),CompileOK);R.Data->SetBoolField(TEXT("save_succeeded"),SaveOK);
    R.Data->SetStringField(TEXT("postcondition_scope"),TEXT("SDK setter returns and current pin existence/default observations; metadata clamping, canonical text and linked/array propagation are not asserted equal to raw proposals or independently reloaded"));Prefix();
    if(!AllApplied){return Failure(TEXT("rigvm_pin_default_writer_apply_failed"),TEXT("One or more RigVM pin defaults failed to apply"));}
    if(!CompileOK||!SaveOK){return Failure(TEXT("rigvm_pin_default_writer_completion_failed"),TEXT("RigVM default changes retained but compile/save completion failed"));}
    R.bSuccess=true;R.bPostconditionPassed=true;R.Stage=TEXT("rigvm_pin_default_writer_complete");R.Data->SetBoolField(TEXT("success"),true);return R;
}
}
