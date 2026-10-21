// UE56 proposal diagnostics contract, implemented with installed UE58 public APIs.
#include "Compatibility/UE58/CubelessRigVMLinkValidationApi.h"
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
#include "RigVMModel/Nodes/RigVMCollapseNode.h"
#include "RigVMModel/RigVMClient.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMModel/RigVMLink.h"
#include "RigVMModel/RigVMNode.h"
#include "RigVMModel/RigVMPin.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace Cubeless::UE58::RigVMLinkValidation
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
    for (const TCHAR* Key : {TEXT("asset_load_calls"), TEXT("asset_load_returns"), TEXT("class_load_calls"),
        TEXT("class_find_calls"), TEXT("default_model_calls"), TEXT("asset_reference_calls"), TEXT("find_pin_calls"),
        TEXT("pin_summary_calls"), TEXT("find_link_for_pin_calls"), TEXT("contains_link_calls"), TEXT("can_link_calls"),
        TEXT("completed_diagnostics"), TEXT("getter_identity_checks"), TEXT("explicit_modify_calls"),
        TEXT("explicit_add_link_calls"), TEXT("explicit_compile_calls"), TEXT("explicit_save_calls"), TEXT("runtime_execution_calls")})
    { C->SetNumberField(Key, 0); }
    R.Data->SetObjectField(TEXT("native_call_counts"), C);
    R.Data->SetStringField(TEXT("command"), TEXT("validate_controlrig_rigvm_links"));
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
    R.Data->SetNumberField(TEXT("completed_link_count"), 0);
    R.Data->SetNumberField(TEXT("included_link_count"), 0);
    R.Data->SetNumberField(TEXT("can_link_count"), 0);
    R.Data->SetNumberField(TEXT("cannot_link_count"), 0);
    R.Data->SetNumberField(TEXT("missing_pin_count"), 0);
    R.Data->SetNumberField(TEXT("already_linked_count"), 0);
    R.Data->SetArrayField(TEXT("results"), {});
    R.Data->SetStringField(TEXT("scope"), TEXT("read-only ControlRig RigVM link feasibility validation; no URigVMController AddLink, graph mutation, compile, save, execute, or solve"));
    R.Data->SetStringField(TEXT("change_state_scope"), TEXT("No explicit authoring/compile/save/execute calls. Load/PostLoad, virtual getters, type registry and CanLink DiagnosticsAST caches have unknown total effects. Source_modified is the legacy captured-package dirty comparison, not total purity."));
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

// Capture the full source lists used by recursive SDK lookup/AST code, not just requested proposals.
// These checks bound our traversal and detect observed callback invalidation. They cannot make the
// internals of a virtual SDK CanLink/GetPins callback atomic or prove all SDK side effects absent.
struct FInventory
{
    explicit FInventory(FRead& InC) : C(InC) {}
    struct FObject { UObject* O; UClass* Class; UObject* Outer; FString Path; };
    struct FGraph { URigVMGraph* Graph; URigVMGraph* Parent; TArray<URigVMNode*> Nodes; TArray<URigVMLink*> Links; };
    struct FNode { URigVMNode* Node; URigVMGraph* Graph; TArray<URigVMPin*> Roots, Orphans; URigVMGraph* Contained; };
    struct FPin { URigVMPin* Pin; URigVMNode* Node; URigVMPin* Parent; TArray<URigVMPin*> Children; TArray<URigVMLink*> Links; };
    struct FLink { URigVMLink* Link; URigVMGraph* Graph; URigVMPin* Source; URigVMPin* Target; };
    FRead& C; TArray<FObject> Objects; TArray<FGraph> Graphs; TArray<FNode> Nodes; TArray<FPin> Pins; TArray<FLink> Links;
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
    bool RawShapes()
    {
        if (!Identities()) { return false; }
        for (const FGraph& V : Graphs)
        {
            if (!Step() || V.Graph->GetParentGraph() != V.Parent || V.Graph->GetNodes() != V.Nodes || V.Graph->GetLinks() != V.Links)
            { return C.Bad(TEXT("Captured graph source slots or parent changed")); }
        }
        for (const FPin& V : Pins)
        {
            if (!Step() || V.Pin->GetParentPin() != V.Parent || V.Pin->GetSubPins() != V.Children || V.Pin->GetLinks() != V.Links
                || V.Pin->GetOuter() != (V.Parent ? static_cast<UObject*>(V.Parent) : static_cast<UObject*>(V.Node)))
            { return C.Bad(TEXT("Captured pin parent/child/link slots changed")); }
        }
        return true;
    }
    bool Parents(URigVMGraph* Graph)
    {
        TSet<URigVMGraph*> Seen;
        for (URigVMGraph* G = Graph; G; G = G->GetParentGraph())
        {
            if (!Step() || Seen.Contains(G) || Seen.Num() >= SafetyDepth || !Object(G))
            { return C.Bad(TEXT("Graph parent chain is unreadable, cyclic or beyond the finite depth")); }
            Seen.Add(G);
            if (URigVMNode* OuterNode = Cast<URigVMNode>(G->GetOuter()))
            { if (!Object(OuterNode)) { return false; } }
        }
        return true;
    }
    bool Pin(URigVMPin* P, URigVMNode* N, URigVMPin* Parent, int32 Depth)
    {
        if (!Step() || Depth > SafetyDepth || SeenPins.Contains(P) || !Object(P)
            || P->GetParentPin() != Parent || P->GetOuter() != (Parent ? static_cast<UObject*>(Parent) : static_cast<UObject*>(N)))
        { return C.Bad(TEXT("Pin tree is cyclic, duplicated, unreadable or has invalid live ownership")); }
        SeenPins.Add(P); PinGraphs.Add(P, N->GetGraph());
        const auto& Children = P->GetSubPins(); const auto& PinLinks = P->GetLinks();
        if (Children.Num() > SafetyRows - Rows || PinLinks.Num() > SafetyRows)
        { return C.Bad(TEXT("Pin child/link source list exceeds the finite observation budget")); }
        FPin V{P, N, Parent, Children, PinLinks}; Pins.Add(V);
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
        const FGraph V{G, G->GetParentGraph(), G->GetNodes(), G->GetLinks()}; Graphs.Add(V);
        for (URigVMLink* L : V.Links)
        { if (!Object(L) || L->GetOuter() != G) { return C.Bad(TEXT("Graph link source ownership is invalid")); } }
        for (URigVMNode* N : V.Nodes)
        {
            if (!Object(N) || N->GetGraph() != G || !C.Current() || !RawShapes()) { return C.Bad(TEXT("Graph node ownership changed")); }
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
            auto NodeCurrent = [&]() { return Identity(V.Node) && Identity(V.Graph)
                && V.Node->GetGraph() == V.Graph && V.Graph->GetNodes().Contains(V.Node); };
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
}

Variable::FResult Validate(const FString& ControlRigPath, const FString& ControlRigAssetPath,
    const FString& ControlRigClassPath, const FString& LinksJson, const FString& UnitLinksJson,
    bool bAllowTypeCasting, int64 MaxLinks)
{
    FResult R; Init(R);
    const int32 Limit = int32(FMath::Clamp<int64>(MaxLinks, 0, 256));
    R.Data->SetStringField(TEXT("received_max_links_decimal"), LexToString(MaxLinks));
    R.Data->SetNumberField(TEXT("max_links"), Limit);
    R.Data->SetBoolField(TEXT("enable_type_casting"), bAllowTypeCasting); R.Data->SetBoolField(TEXT("allow_type_casting"), bAllowTypeCasting);
    if (!IsEditorGameThread() || IsPlaySessionActive() || IsEngineExitRequested() || ReadInFlight)
    { return Fail(R, TEXT("rigvm_links_editor_unavailable"), TEXT("An idle live editor game thread outside PIE/SIE is required; no load dispatched")); }
    TGuardValue<bool> Busy(ReadInFlight, true);
    R.Stage = TEXT("parse_link_specs");
    TSharedPtr<FJsonValue> Links, Units;
    if (!Decode(LinksJson, Links) || !Decode(UnitLinksJson, Units))
    { return Fail(R, TEXT("rigvm_links_json_invalid"), TEXT("Each nullable array carrier must be valid JSON null/array, <=1Mi UE characters, no embedded NUL, nesting<=64")); }
    TArray<FSpec> Specs; FString Field, ParseError; int32 RequestedCount = 0; bool Truncated = false;
    const bool Parsed = Parse(Links, Units, Limit, Specs, Field, RequestedCount, Truncated, ParseError);
    R.Data->SetStringField(TEXT("link_specs_field"), Field); R.Data->SetNumberField(TEXT("requested_link_count"), RequestedCount);
    R.Data->SetNumberField(TEXT("validated_link_count"), Specs.Num()); R.Data->SetBoolField(TEXT("links_truncated"), Truncated);
    if (!Parsed) { return Fail(R, TEXT("rigvm_links_specs_invalid"), ParseError); }
    if (!InputPath(ControlRigPath) || !InputPath(ControlRigAssetPath) || !InputPath(ControlRigClassPath))
    { return Fail(R, TEXT("rigvm_links_path_invalid"), TEXT("Paths require <=1Mi raw characters, no embedded NUL and normalized length<NAME_SIZE")); }
    // Original IsEmpty-before-trim selection. Whitespace masks later asset aliases before class fallback.
    const FString Path = ControlRigPath.IsEmpty() ? ControlRigAssetPath : ControlRigPath;
    FString Requested, Kind; UObject* Object = nullptr; R.Stage = TEXT("resolve_control_rig_input");
    if (!Path.TrimStartAndEnd().IsEmpty()) { Requested = Path; Kind = TEXT("asset"); }
    else if (!ControlRigClassPath.TrimStartAndEnd().IsEmpty()) { Requested = ControlRigClassPath; Kind = TEXT("generated_class"); }
    else { return Fail(R, TEXT("rigvm_links_target_missing"), TEXT("Missing 'control_rig_path', 'control_rig_asset_path', or 'control_rig_class_path' parameter")); }
    R.Data->SetStringField(TEXT("requested_control_rig"), Requested); R.Stage = TEXT("load_control_rig");
    Object = Kind == TEXT("asset") ? LoadAsset(Requested, R) : LoadClass(Requested, R);
    TStrongObjectPtr<UObject> LoadedLifetime(Object);
    if (!Loaded(Object)) { return Fail(R, TEXT("rigvm_links_target_invalid"), TEXT("ControlRig asset or generated class was not found or is not fully loaded")); }
    R.Stage = TEXT("resolve_control_rig"); UClass* InputClass = Cast<UClass>(Object);
    if (UClass* Class = InputClass)
    {
        if ((!Path.TrimStartAndEnd().IsEmpty() || Class->IsChildOf(UControlRig::StaticClass())) && Loaded(Class->ClassGeneratedBy))
        { Object = Class->ClassGeneratedBy; Kind = TEXT("generated_class"); }
        else { Object = nullptr; }
    }
    if (!Loaded(Object) || (!Cast<UControlRigBlueprint>(Object) && !Cast<UControlRigEditorAsset>(Object)))
    { return Fail(R, TEXT("rigvm_links_target_invalid"), TEXT("Target has no ControlRig Blueprint/editor asset graph to validate")); }
    if (!Loaded(Object->GetPackage())) { return Fail(R, TEXT("rigvm_links_package_invalid"), TEXT("Resolved editor asset package is unavailable")); }
    FRead C(R, Object); C.InputClass.Reset(InputClass);
    R.Data->SetStringField(TEXT("resolution_kind"), Kind); R.Data->SetStringField(TEXT("control_rig_asset_path"), C.AssetPath); C.Dirty();
    auto Failure = [&]() { C.Dirty(); return Fail(R, TEXT("rigvm_links_observation_incomplete"), C.Error.IsEmpty() ? TEXT("Link observation is incomplete; partial data retained, no automatic retry") : C.Error); };
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
    R.Stage = TEXT("capture_rigvm_link_sources");
    // max_links=0 preserves the original zero-proposal path and does not create a diagnostics AST.
    if (!Specs.IsEmpty() && !Inventory.Capture()) { return Failure(); }
    if (Specs.IsEmpty() && !Inventory.Parents(C.Graph.Get())) { return Failure(); }
    R.Stage = TEXT("validate_rigvm_links"); TArray<TSharedPtr<FJsonValue>> Rows;
    int32 Can = 0, Cannot = 0, Missing = 0, Already = 0;
    for (int32 I = 0; I < Specs.Num(); ++I)
    {
        auto Row = MakeShared<FJsonObject>(); Rows.Add(MakeShared<FJsonValueObject>(Row));
        R.Data->SetArrayField(TEXT("results"), Rows); R.Data->SetNumberField(TEXT("included_link_count"), Rows.Num());
        if (!Diagnostic(Inventory, Specs[I], I, bAllowTypeCasting, Row)) { return Failure(); }
        const bool B = Row->GetBoolField(TEXT("can_link")); Can += B ? 1 : 0; Cannot += B ? 0 : 1;
        Missing += (!Row->GetBoolField(TEXT("source_pin_found")) || !Row->GetBoolField(TEXT("target_pin_found"))) ? 1 : 0;
        Already += Row->GetBoolField(TEXT("already_linked")) ? 1 : 0;
        Count(R, TEXT("completed_diagnostics")); R.Data->SetNumberField(TEXT("completed_link_count"), I + 1);
        R.Data->SetNumberField(TEXT("can_link_count"), Can); R.Data->SetNumberField(TEXT("cannot_link_count"), Cannot);
        R.Data->SetNumberField(TEXT("missing_pin_count"), Missing); R.Data->SetNumberField(TEXT("already_linked_count"), Already);
    }
    R.Stage = TEXT("verify_rigvm_link_observation");
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
    R.Data->SetNumberField(TEXT("graph_link_count"), C.Graph->GetLinks().Num());
    R.Data->SetBoolField(TEXT("all_links_can_link"), Cannot == 0);
    R.Data->SetNumberField(TEXT("bounded_source_checks"), double(Inventory.Checks));
    for (const auto& Pair : C.Packages) { if (!C.SamePackage(Pair.Key)) { return Failure(); } }
    C.Dirty(); R.Data->SetBoolField(TEXT("success"), true);
    R.bSuccess = true; R.bPostconditionPassed = true; R.Stage = TEXT("rigvm_link_validation_complete");
    return R;
}
}
