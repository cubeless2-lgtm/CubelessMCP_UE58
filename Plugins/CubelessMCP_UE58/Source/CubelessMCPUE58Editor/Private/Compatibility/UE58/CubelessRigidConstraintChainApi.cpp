// Editor-only composite UE56 rigid-chain authoring contract.
// Public native SCS/body/constraint authoring only; no custom runtime class.
#include "Compatibility/UE58/CubelessRigidConstraintChainApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "Engine/World.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_Variable.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::RigidConstraintChain
{
namespace
{
constexpr int32 MaxProperties = 2048, MaxText = 1048576, MaxNodes = 256, MaxGraphs = 64, MaxPins = 16384, MaxPackages = 8192;
constexpr int64 MaxDiskBytes = 64 * 1024 * 1024;
struct FResult
{
    FString Operation, Stage = TEXT("preflight"), Code, Error;
    bool bSuccess = false, bAttempted = false, bKnown = true, bChanged = false, bPost = false;
    bool bCompileRequested = false, bCompileAttempted = false, bSaveRequested = false, bSaveAttempted = false, bSaved = false;
    TOptional<bool> Compiled;
    TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
};
FString Text(const TSharedRef<FJsonObject>& J)
{ FString S; FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&S)); return S; }
bool Fail(FResult& R, const TCHAR* Code, const FString& Error, bool Unknown = false)
{ R.Code = Code; R.Error = Error; R.bSuccess = false; R.bPost = false; if (Unknown) { R.bKnown = false; } return false; }
void Count(FResult& R, const TCHAR* Key)
{ R.Data->SetNumberField(Key, R.Data->GetNumberField(Key) + 1); }
void Init(FResult& R, bool Dry, bool Save)
{
    R.Operation = TEXT("ensure_blueprint_rigid_constraint_chain"); R.bCompileRequested = !Dry; R.bSaveRequested = !Dry && Save;
    for (const TCHAR* K : { TEXT("explicit_modify_calls"), TEXT("create_node_attempts"), TEXT("created_node_count"), TEXT("attached_node_count"), TEXT("body_configuration_calls"), TEXT("constraint_configuration_calls"), TEXT("public_setter_calls"), TEXT("direct_field_write_calls"), TEXT("add_root_calls"), TEXT("add_child_calls"), TEXT("implicit_default_root_removed_count"), TEXT("structural_refresh_calls"), TEXT("mark_package_dirty_calls"), TEXT("full_compile_attempts"), TEXT("save_attempt_count"), TEXT("asset_load_count") }) { R.Data->SetNumberField(K, 0); }
    for (const TCHAR* K : { TEXT("observations_complete"), TEXT("source_preserved"), TEXT("unselected_state_preserved"), TEXT("independent_readback_passed"), TEXT("runtime_verified"), TEXT("mcp_dependency_written") }) { R.Data->SetBoolField(K, false); }
    R.Data->SetBoolField(TEXT("dry_run"), Dry); R.Data->SetBoolField(TEXT("caller_save_requested"), Save); R.Data->SetArrayField(TEXT("save_calls"), {}); R.Data->SetArrayField(TEXT("asset_load_calls"), {}); R.Data->SetArrayField(TEXT("allocated_node_prefix"), {});
    R.Data->SetStringField(TEXT("dependency_tier"), TEXT("EngineAPI: public native SCS/body/constraint defaults, Blueprint compiler and exact EditorAssetLibrary save; no ServerRPC, runtime Cubeless class or gameplay module"));
    R.Data->SetStringField(TEXT("effect_scope"), TEXT("Loaded direct-native Actor Blueprint with one leaf exact native Scene root: existing requested root or replacement of lone DefaultSceneRoot. New native StaticMesh bodies and PhysicsConstraint components. Public CreateNode includes transient template allocation/Modify/rename and node allocation; AddNode/child and compiler/setters may dispatch implicit callbacks/cache/physics-handle work, not exhaustively measured. Counters describe explicit calls only."));
    R.Data->SetStringField(TEXT("source_load_scope"), TEXT("Explicit requested Game/Engine StaticMesh loads and PostLoad effects occur before selected authoring baseline and are separately recorded; no native CDO prewarming, world/simulation or runtime proof."));
    R.Data->SetStringField(TEXT("read_scope"), TEXT("Bounded reflected properties/current own SCS and native CDO sources, explicit mesh/body-setup references, declared topology, target file hash and other-package dirty flags; unreflected render/physics/streaming caches, all delegates and global loaded-object effects are not certified."));
    R.Data->SetBoolField(TEXT("atomic_rollback_available"), false); R.Data->SetBoolField(TEXT("auto_retry_safe"), false);
}
FString Encode(FResult& R)
{
    auto J = MakeShared<FJsonObject>(); J->SetBoolField(TEXT("success"), R.bSuccess); J->SetStringField(TEXT("operation"), R.Operation); J->SetStringField(TEXT("stage"), R.Stage);
    J->SetBoolField(TEXT("mutation_attempted"), R.bAttempted); J->SetBoolField(TEXT("change_state_known"), R.bKnown); J->SetBoolField(TEXT("outcome_unknown"), !R.bKnown);
    if (R.bKnown) { J->SetBoolField(TEXT("changed"), R.bChanged); } else { J->SetField(TEXT("changed"), MakeShared<FJsonValueNull>()); }
    J->SetBoolField(TEXT("postcondition_passed"), R.bPost); J->SetBoolField(TEXT("compile_requested"), R.bCompileRequested); J->SetBoolField(TEXT("compile_attempted"), R.bCompileAttempted);
    if (R.Compiled.IsSet()) { J->SetBoolField(TEXT("compiled"), R.Compiled.GetValue()); } else { J->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>()); }
    J->SetBoolField(TEXT("save_requested"), R.bSaveRequested); J->SetBoolField(TEXT("save_attempted"), R.bSaveAttempted); J->SetBoolField(TEXT("saved"), R.bSaved);
    J->SetBoolField(TEXT("atomic_rollback_available"), false); J->SetBoolField(TEXT("auto_retry_safe"), false);
    if (!R.Code.IsEmpty()) { J->SetStringField(TEXT("error_code"), R.Code); J->SetStringField(TEXT("error"), R.Error); }
    J->SetObjectField(TEXT("data"), R.Data); return Text(J);
}
bool Loaded(const UObject* O)
{ return IsValid(O) && !O->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_BeginDestroyed | RF_FinishDestroyed); }
bool CleanText(const FString& S)
{ for (const TCHAR* P : { TEXT("/Script/Cubeless"), TEXT("/Script/UnrealMCP"), TEXT("/Engine/PythonTypes"), TEXT("/Game/UnrealMCP"), TEXT("/CubelessMCP_UE58/"), TEXT("/UnrealMCP/"), TEXT("/MCPUnreal/"), TEXT("/MCP_Unreal/"), TEXT("UnrealMCP"), TEXT("MCPUnreal"), TEXT("mcp_unreal"), TEXT("CubelessControlRigTools") }) { if (S.Contains(P, ESearchCase::IgnoreCase)) { return false; } } return true; }
bool Name(const FString& S)
{ FText Why; return !S.IsEmpty() && S.Len() < NAME_SIZE && FName::IsValidXName(S, INVALID_OBJECTNAME_CHARACTERS, &Why) && !FName(*S).IsNone(); }
FString Path(FString S)
{ S = FPackageName::ExportTextPathToObjectPath(S).TrimStartAndEnd(); S.TrimQuotesInline(); S.ReplaceInline(TEXT("\\"), TEXT("/")); if (!S.IsEmpty() && !S.Contains(TEXT("."))) { S += TEXT(".") + FPackageName::GetShortName(S); } return S; }
bool AssetPath(const FString& S)
{ const FString P = FPackageName::ObjectPathToPackageName(S); return S.Len() < 1024 && FPackageName::IsValidObjectPath(S) && (P.StartsWith(TEXT("/Game/")) || P.StartsWith(TEXT("/Engine/"))) && !S.Contains(TEXT(":")) && CleanText(S); }
TSharedRef<FJsonObject> Identity(const UObject* O)
{
    auto J = MakeShared<FJsonObject>(); J->SetBoolField(TEXT("valid"), Loaded(O));
    J->SetStringField(TEXT("path"), Loaded(O) ? O->GetPathName() : FString()); J->SetStringField(TEXT("class"), Loaded(O) ? O->GetClass()->GetPathName() : FString());
    J->SetStringField(TEXT("outer"), Loaded(O) && O->GetOuter() ? O->GetOuter()->GetPathName() : FString()); J->SetStringField(TEXT("object_flags"), Loaded(O) ? LexToString(static_cast<uint32>(O->GetFlags())) : FString()); return J;
}
struct FContext;
bool AppendCurrent(FContext* C);
bool Properties(UStruct* Type, const void* Container, UObject* Owner, const TSet<FName>& Skip, TSharedRef<FJsonObject>& J, FContext* Context = nullptr)
{
    if (!AppendCurrent(Context) || !Type || !Container || !Loaded(Owner)) { return false; } int32 Count = 0, Chars = 0;
    TStrongObjectPtr<UObject> HeldOwner(Owner); TStrongObjectPtr<UStruct> HeldType(Type);
    TArray<FProperty*> Fields;
    for (TFieldIterator<FProperty> It(Type, EFieldIteratorFlags::IncludeSuper); It; ++It) { if (Fields.Num() >= MaxProperties) { return false; } Fields.Add(*It); }
    for (FProperty* P : Fields)
    {
        // No retained field/iterator is dereferenced after ExportText until membership/current closes.
        const auto FieldCurrent = [&]() -> bool
        {
            if (!AppendCurrent(Context) || !Loaded(Owner) || !Loaded(Type) || Owner->GetClass() != Type) { return false; }
            for (TFieldIterator<FProperty> It(Type, EFieldIteratorFlags::IncludeSuper); It; ++It) { if (*It == P) { return true; } }
            return false;
        };
        if (!FieldCurrent()) { return false; } if (Skip.Contains(P->GetFName())) { continue; } if (++Count > MaxProperties || P->ArrayDim < 1 || P->ArrayDim > 64 || !P->GetOwnerStruct()) { return false; }
        auto Row = MakeShared<FJsonObject>(); J->SetObjectField(P->GetName(), Row); Row->SetStringField(TEXT("owner"), P->GetOwnerStruct()->GetPathName()); Row->SetStringField(TEXT("class"), P->GetClass()->GetName()); Row->SetStringField(TEXT("flags"), LexToString(static_cast<uint64>(P->GetPropertyFlags())));
        const FName FieldName = P->GetFName(); const FFieldClass* FieldClass = P->GetClass(); UStruct* FieldOwner = P->GetOwnerStruct();
        const int32 Dim = P->ArrayDim, Offset = P->GetOffset_ForInternal(), Size = P->GetSize(); const EPropertyFlags Flags = P->GetPropertyFlags();
        TArray<TSharedPtr<FJsonValue>> Values;
        for (int32 I = 0; I < Dim; ++I)
        {
            if (!FieldCurrent() || P->GetFName() != FieldName || P->GetClass() != FieldClass || P->GetOwnerStruct() != FieldOwner || P->ArrayDim != Dim || P->GetOffset_ForInternal() != Offset || P->GetSize() != Size || P->GetPropertyFlags() != Flags) { return false; }
            FString S; const void* V = P->ContainerPtrToValuePtr<void>(Container, I); const bool Exported = P->ExportText_Direct(S, V, V, Owner, PPF_None);
            if (!FieldCurrent() || P->GetFName() != FieldName || P->GetClass() != FieldClass || P->GetOwnerStruct() != FieldOwner || P->ArrayDim != Dim || P->GetOffset_ForInternal() != Offset || P->GetSize() != Size || P->GetPropertyFlags() != Flags || !Exported || (Chars += S.Len()) > MaxText || !CleanText(S)) { return false; }
            Values.Add(MakeShared<FJsonValueString>(S));
        }
        Row->SetArrayField(TEXT("values"), Values);
    }
    return true;
}
bool Properties(UObject* O, const TSet<FName>& Skip, TSharedRef<FJsonObject>& J, FContext* Context = nullptr)
{ return AppendCurrent(Context) && Loaded(O) && Properties(O->GetClass(), O, O, Skip, J, Context); }
bool Equal(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
{
    if (!A || !B || A->Type != B->Type) { return false; }
    if (A->Type == EJson::Object) { if (A->AsObject()->Values.Num() != B->AsObject()->Values.Num()) { return false; } for (const auto& P : A->AsObject()->Values) { const auto Q = B->AsObject()->TryGetField(P.Key.ToView()); if (!Equal(P.Value, Q)) { return false; } } return true; }
    if (A->Type == EJson::Array) { const auto& X = A->AsArray(); const auto& Y = B->AsArray(); if (X.Num() != Y.Num()) { return false; } for (int32 I = 0; I < X.Num(); ++I) { if (!Equal(X[I], Y[I])) { return false; } } return true; }
    if (A->Type == EJson::String) { return A->AsString().Equals(B->AsString(), ESearchCase::CaseSensitive); } if (A->Type == EJson::Number) { return A->AsNumber() == B->AsNumber(); } if (A->Type == EJson::Boolean) { return A->AsBool() == B->AsBool(); } return A->Type == EJson::Null;
}
bool Equal(const TSharedRef<FJsonObject>& A, const TSharedRef<FJsonObject>& B)
{ return Equal(MakeShared<FJsonValueObject>(A), MakeShared<FJsonValueObject>(B)); }
// UO2: a newly loaded clean package is an observation addition, not a dirty-state change.
// Every pre-existing key remains mandatory, Boolean and exact; no entry is masked.
bool OtherDirtyPreserved(const TSharedRef<FJsonObject>& Before, const TSharedRef<FJsonObject>& After, FResult& R, const FString& Stage)
{
    auto Check = MakeShared<FJsonObject>(), Added = MakeShared<FJsonObject>(), Checks = MakeShared<FJsonObject>();
    if (R.Data->HasTypedField<EJson::Object>(TEXT("other_package_dirty_checks"))) { Checks->Values = R.Data->GetObjectField(TEXT("other_package_dirty_checks"))->Values; }
    Check->SetBoolField(TEXT("passed"), false); Check->SetNumberField(TEXT("before_count"), Before->Values.Num()); Check->SetNumberField(TEXT("after_count"), After->Values.Num());
    Check->SetNumberField(TEXT("normal_new_clean_package_count"), 0); Check->SetObjectField(TEXT("normal_new_clean_packages"), Added); Checks->SetObjectField(Stage, Check); R.Data->SetObjectField(TEXT("other_package_dirty_checks"), Checks);
    auto Bad = [&](const FString& Package, const TSharedPtr<FJsonValue>& Expected, const TSharedPtr<FJsonValue>& Actual, const TCHAR* Reason) -> bool
    {
        if (!R.Data->HasField(TEXT("other_package_dirty_first_mismatch")))
        {
            auto Mismatch = MakeShared<FJsonObject>(); FString Pointer = Package; Pointer.ReplaceInline(TEXT("~"), TEXT("~0")); Pointer.ReplaceInline(TEXT("/"), TEXT("~1"));
            Mismatch->SetStringField(TEXT("path"), TEXT("/other_package_dirty/") + Pointer); Mismatch->SetStringField(TEXT("package"), Package); Mismatch->SetStringField(TEXT("stage"), Stage); Mismatch->SetStringField(TEXT("reason"), Reason);
            Mismatch->SetBoolField(TEXT("before_present"), Before->HasField(Package)); Mismatch->SetBoolField(TEXT("actual_present"), After->HasField(Package));
            TSharedPtr<FJsonValue> ExpectedValue = Expected, ActualValue = Actual;
            if (!ExpectedValue.IsValid()) { ExpectedValue = MakeShared<FJsonValueNull>(); } if (!ActualValue.IsValid()) { ActualValue = MakeShared<FJsonValueNull>(); }
            Mismatch->SetField(TEXT("expected"), ExpectedValue); Mismatch->SetField(TEXT("actual"), ActualValue);
            R.Data->SetObjectField(TEXT("other_package_dirty_first_mismatch"), Mismatch);
        }
        return false;
    };
    for (const auto& Pair : Before->Values)
    {
        const FString Package(Pair.Key.ToView()); const auto Actual = After->TryGetField(Pair.Key.ToView());
        if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::Boolean) { return Bad(Package, MakeShared<FJsonValueString>(TEXT("existing Boolean dirty state")), Actual, TEXT("before_type_unknown")); }
        if (!Actual.IsValid() || Actual->Type != EJson::Boolean || Actual->AsBool() != Pair.Value->AsBool()) { return Bad(Package, Pair.Value, Actual, TEXT("existing_package_missing_type_or_dirty_changed")); }
    }
    for (const auto& Pair : After->Values)
    {
        if (Before->HasField(Pair.Key.ToView())) { continue; }
        const FString Package(Pair.Key.ToView());
        if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::Boolean || Pair.Value->AsBool()) { return Bad(Package, MakeShared<FJsonValueBoolean>(false), Pair.Value, TEXT("new_package_must_be_actual_clean_Boolean")); }
        Added->SetBoolField(Package, false); Check->SetNumberField(TEXT("normal_new_clean_package_count"), Added->Values.Num());
    }
    Check->SetNumberField(TEXT("normal_new_clean_package_count"), Added->Values.Num()); Check->SetBoolField(TEXT("passed"), true);
    Check->SetStringField(TEXT("scope"), TEXT("Existing loaded-package presence and Boolean dirty values remain exact. Only newly observed actual clean packages are admitted; loader/callback global purity and arbitrary package state are not proven."));
    return true;
}
bool Native(UClass* C)
{ return Loaded(C) && C->HasAnyClassFlags(CLASS_Native) && !C->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists) && !C->ClassGeneratedBy && C->GetOutermost()->GetName() == TEXT("/Script/Engine") && !C->GetOutermost()->IsDirty() && C->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn); }
bool Disk(const FString& Package, TSharedRef<FJsonObject>& J)
{
    FString F; if (!FPackageName::TryConvertLongPackageNameToFilename(Package, F, FPackageName::GetAssetPackageExtension())) { return false; } F = FPaths::ConvertRelativePathToFull(F);
    const int64 Size = IFileManager::Get().FileSize(*F); J->SetStringField(TEXT("path"), F); J->SetBoolField(TEXT("exists"), Size >= 0); J->SetNumberField(TEXT("bytes"), FMath::Max<int64>(0, Size)); J->SetStringField(TEXT("sha1"), TEXT("")); if (Size < 0) { return true; }
    TArray<uint8> Bytes; if (Size > MaxDiskBytes || !FFileHelper::LoadFileToArray(Bytes, *F) || Bytes.Num() != Size) { return false; } uint8 Hash[FSHA1::DigestSize]; FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Hash); J->SetStringField(TEXT("sha1"), BytesToHex(Hash, FSHA1::DigestSize)); return true;
}
bool Dirty(const FString& Package, TSharedRef<FJsonObject>& J)
{ int32 Count = 0; for (TObjectIterator<UPackage> It; It; ++It) { if (!Loaded(*It) || It->GetName() == Package) { continue; } if (++Count > MaxPackages) { return false; } J->SetBoolField(It->GetName(), It->IsDirty()); } return true; }

FString ObjectPath(const UObject* O) { return O ? O->GetPathName() : FString(); }
bool Graphs(UBlueprint* B, TSharedRef<FJsonObject>& J, FContext* Context = nullptr)
{
    if (!AppendCurrent(Context)) { return false; }
    TArray<UEdGraph*> All; B->GetAllGraphs(All); TArray<TStrongObjectPtr<UEdGraph>> HeldGraphs; for (UEdGraph* G : All) { HeldGraphs.Emplace(G); } if (!AppendCurrent(Context) || All.Num() > MaxGraphs) { return false; }
    TSet<UEdGraph*> Seen; int32 Nodes = 0, Pins = 0; TArray<TSharedPtr<FJsonValue>> Rows; J->SetArrayField(TEXT("graphs"), Rows);
    for (UEdGraph* G : All)
    {
        if (!Loaded(G) || !CleanText(G->GetClass()->GetPathName()) || G->GetOutermost() != B->GetOutermost() || Seen.Contains(G) || !G->GraphGuid.IsValid()) { return false; } Seen.Add(G);
        auto Row = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("identity"), Identity(G)); Row->SetStringField(TEXT("guid"), G->GraphGuid.ToString()); Rows.Add(MakeShared<FJsonValueObject>(Row)); J->SetArrayField(TEXT("graphs"), Rows);
        const auto PhysicalNodes = G->Nodes; TArray<TStrongObjectPtr<UEdGraphNode>> HeldNodes;
        TMap<UEdGraphNode*, TArray<UEdGraphPin*>> PhysicalPins;
        for (UEdGraphNode* N : PhysicalNodes) { HeldNodes.Emplace(N); if (!Loaded(N)) { return false; } PhysicalPins.Add(N, N->Pins); }
        const auto GraphCurrent = [&]() -> bool
        {
            if (!AppendCurrent(Context) || !Loaded(G) || G->Nodes != PhysicalNodes) { return false; }
            for (UEdGraphNode* N : PhysicalNodes) { if (!Loaded(N) || N->GetOuter() != G || N->Pins != PhysicalPins.FindChecked(N)) { return false; } }
            return true;
        };
        if (!GraphCurrent()) { return false; }
        TSet<UEdGraphNode*> Owned; TSet<const UEdGraphPin*> OwnedPins;
        for (UEdGraphNode* N : PhysicalNodes)
        {
            if (!Loaded(N) || !CleanText(N->GetClass()->GetPathName()) || N->GetOuter() != G || Owned.Contains(N) || !N->NodeGuid.IsValid() || ++Nodes > 4096) { return false; } Owned.Add(N);
            for (const UEdGraphPin* P : N->Pins) { if (!P || P->GetOwningNodeUnchecked() != N || OwnedPins.Contains(P) || !P->PinId.IsValid() || ++Pins > MaxPins) { return false; } OwnedPins.Add(P); }
        }
        TArray<TSharedPtr<FJsonValue>> NodeRows;
        for (UEdGraphNode* N : PhysicalNodes)
        {
            auto NR = MakeShared<FJsonObject>(); NR->SetObjectField(TEXT("identity"), Identity(N)); NR->SetStringField(TEXT("guid"), N->NodeGuid.ToString()); NodeRows.Add(MakeShared<FJsonValueObject>(NR)); Row->SetArrayField(TEXT("nodes"), NodeRows);
            auto Props = MakeShared<FJsonObject>(); NR->SetObjectField(TEXT("properties"), Props); if (!Properties(N, {}, Props, Context) || !GraphCurrent()) { return false; }
            TArray<TSharedPtr<FJsonValue>> PinRows;
            for (const UEdGraphPin* P : N->Pins)
            {
                auto PR = MakeShared<FJsonObject>(); PR->SetStringField(TEXT("id"), P->PinId.ToString()); PR->SetStringField(TEXT("name"), P->PinName.ToString()); PR->SetNumberField(TEXT("direction"), static_cast<uint8>(P->Direction));
                FString Type; FEdGraphPinType::StaticStruct()->ExportText(Type, &P->PinType, nullptr, N, PPF_None, nullptr); if (!GraphCurrent() || Type.Len() > MaxText || !CleanText(Type) || !CleanText(P->DefaultValue) || !CleanText(P->AutogeneratedDefaultValue)) { return false; }
                TStrongObjectPtr<UObject> HeldDefault(P->DefaultObject); if (!GraphCurrent()) { return false; } const FString DefaultPath = ObjectPath(HeldDefault.Get());
                const FText TextValue = P->DefaultTextValue; const FString DefaultText = TextValue.ToString(); if (!GraphCurrent() || !CleanText(DefaultPath) || !CleanText(DefaultText)) { return false; }
                PR->SetStringField(TEXT("type"), Type); PR->SetStringField(TEXT("default"), P->DefaultValue); PR->SetStringField(TEXT("autogenerated_default"), P->AutogeneratedDefaultValue); PR->SetStringField(TEXT("default_object"), DefaultPath); PR->SetStringField(TEXT("default_text"), DefaultText);
                TArray<TSharedPtr<FJsonValue>> Links;
                for (const UEdGraphPin* Peer : P->LinkedTo) { if (!Peer || !OwnedPins.Contains(Peer) || !Peer->LinkedTo.Contains(P) || Peer->Direction == P->Direction) { return false; } Links.Add(MakeShared<FJsonValueString>(Peer->PinId.ToString())); }
                PR->SetArrayField(TEXT("links"), Links); PinRows.Add(MakeShared<FJsonValueObject>(PR)); NR->SetArrayField(TEXT("pins"), PinRows);
            }
        }
    }
    J->SetNumberField(TEXT("node_count"), Nodes); J->SetNumberField(TEXT("pin_count"), Pins); return true;
}
struct FBodySpec
{
    FString ComponentName;
    FName ComponentFName = NAME_None;
    FString StaticMeshPath;
    UStaticMesh* StaticMesh = nullptr;
    UBodySetup* BodySetup = nullptr;
    FVector RelativeLocation = FVector::ZeroVector;
    FRotator RelativeRotation = FRotator::ZeroRotator;
    float MassKg = 0.35f;
    bool bSimulatePhysics = false;
};

struct FConstraintSpec
{
    FString ComponentName;
    FName ComponentFName = NAME_None;
    int32 ParentBodyIndex = INDEX_NONE;
    int32 ChildBodyIndex = INDEX_NONE;
};

bool TryReadVector3(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* FieldName,
    FVector& OutValue,
    bool bRequired,
    FString& OutError)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Object.IsValid() || !Object->TryGetArrayField(FieldName, Values))
    {
        if (bRequired)
        {
            OutError = FString::Printf(TEXT("Missing '%s' three-number array"), FieldName);
            return false;
        }
        return true;
    }
    if (!Values || Values->Num() != 3)
    {
        OutError = FString::Printf(TEXT("'%s' must be a three-number array"), FieldName);
        return false;
    }

    double X = 0.0;
    double Y = 0.0;
    double Z = 0.0;
    if (!(*Values)[0].IsValid() || !(*Values)[0]->TryGetNumber(X) ||
        !(*Values)[1].IsValid() || !(*Values)[1]->TryGetNumber(Y) ||
        !(*Values)[2].IsValid() || !(*Values)[2]->TryGetNumber(Z))
    {
        OutError = FString::Printf(TEXT("'%s' must contain only numbers"), FieldName);
        return false;
    }
    if (!FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z))
    {
        OutError = FString::Printf(TEXT("'%s' must contain only finite numbers"), FieldName);
        return false;
    }
    OutValue = FVector(X, Y, Z);
    return true;
}

bool TryParseComponentName(
    const FString& RequestedName,
    const TCHAR* FieldName,
    FName& OutName,
    FString& OutError)
{
    if (RequestedName.IsEmpty())
    {
        OutError = FString::Printf(TEXT("'%s' must not be empty"), FieldName);
        return false;
    }
    if (RequestedName.Len() >= NAME_SIZE)
    {
        OutError = FString::Printf(
            TEXT("'%s' is too long for an Unreal component name (maximum %d characters)"),
            FieldName,
            NAME_SIZE - 1);
        return false;
    }

    FText InvalidReason;
    if (!FName::IsValidXName(RequestedName, INVALID_OBJECTNAME_CHARACTERS, &InvalidReason))
    {
        OutError = FString::Printf(
            TEXT("Invalid '%s' component name '%s': %s"),
            FieldName,
            *RequestedName,
            *InvalidReason.ToString());
        return false;
    }

    OutName = FName(*RequestedName);
    if (OutName.IsNone())
    {
        OutError = FString::Printf(
            TEXT("Invalid '%s' component name '%s': name resolves to None"),
            FieldName,
            *RequestedName);
        return false;
    }
    return true;
}

TArray<TSharedPtr<FJsonValue>> VectorJson(const FVector& Value)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    Result.Add(MakeShared<FJsonValueNumber>(Value.X));
    Result.Add(MakeShared<FJsonValueNumber>(Value.Y));
    Result.Add(MakeShared<FJsonValueNumber>(Value.Z));
    return Result;
}

TArray<TSharedPtr<FJsonValue>> RotatorJson(const FRotator& Value)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    Result.Add(MakeShared<FJsonValueNumber>(Value.Pitch));
    Result.Add(MakeShared<FJsonValueNumber>(Value.Yaw));
    Result.Add(MakeShared<FJsonValueNumber>(Value.Roll));
    return Result;
}

USCS_Node* FindNode(UBlueprint* Blueprint, const FName ComponentName)
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript)
    {
        return nullptr;
    }
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName() == ComponentName)
        {
            return Node;
        }
    }
    return nullptr;
}

bool HasUniformIgnoreResponse(const UPrimitiveComponent* Component)
{
    if (!Component)
    {
        return false;
    }
    for (int32 ChannelIndex = static_cast<int32>(ECC_WorldStatic);
         ChannelIndex < static_cast<int32>(ECC_MAX);
         ++ChannelIndex)
    {
        if (Component->GetCollisionResponseToChannel(static_cast<ECollisionChannel>(ChannelIndex)) != ECR_Ignore)
        {
            return false;
        }
    }
    return true;
}

FString MakeConstraintName(const FString& Prefix, const FString& Body1, const FString& Body2)
{
    return FString::Printf(TEXT("%s_%s_%s"), *Prefix, *Body1, *Body2);
}

TSharedPtr<FJsonObject> CaptureChainState(
    UBlueprint* Blueprint,
    const FName RootComponentName,
    const TArray<FBodySpec>& Bodies,
    const TArray<FConstraintSpec>& Constraints)
{
    TSharedPtr<FJsonObject> State = MakeShared<FJsonObject>();
    State->SetStringField(TEXT("blueprint"), Blueprint ? Blueprint->GetPathName() : FString());
    State->SetBoolField(TEXT("root_exists"), FindNode(Blueprint, RootComponentName) != nullptr);
    State->SetNumberField(
        TEXT("scs_node_count"),
        Blueprint && Blueprint->SimpleConstructionScript
            ? Blueprint->SimpleConstructionScript->GetAllNodes().Num()
            : 0);

    int32 ExistingBodyCount = 0;
    for (const FBodySpec& Body : Bodies)
    {
        ExistingBodyCount += FindNode(Blueprint, Body.ComponentFName) ? 1 : 0;
    }
    int32 ExistingConstraintCount = 0;
    for (const FConstraintSpec& Constraint : Constraints)
    {
        ExistingConstraintCount += FindNode(Blueprint, Constraint.ComponentFName) ? 1 : 0;
    }
    State->SetNumberField(TEXT("requested_body_count_present"), ExistingBodyCount);
    State->SetNumberField(TEXT("requested_constraint_count_present"), ExistingConstraintCount);
    State->SetBoolField(
        TEXT("package_dirty"),
        Blueprint && Blueprint->GetOutermost() && Blueprint->GetOutermost()->IsDirty());
    return State;
}
struct FContext
{
    TStrongObjectPtr<UBlueprint> Blueprint;
    TArray<TStrongObjectPtr<UObject>> Assets;
    TArray<TStrongObjectPtr<USCS_Node>> Allocated;
    USCS_Node* Root = nullptr;
    USCS_Node* ReplacedDefault = nullptr;
    TStrongObjectPtr<USCS_Node> ReplacedDefaultKeepAlive;
    FString Package;
    FName RootName;
    TSet<FName> Names;
    bool AppendExisting = false, Stopped = false;
    // UO: Root retains the legacy named attachment-pivot meaning, not necessarily the top root.
    bool NonRootPivot = false, PivotMobilityPlanned = false;
    TStrongObjectPtr<UEditorEngine> PivotEditor;
    TStrongObjectPtr<UWorld> PivotWorld;
    TStrongObjectPtr<UPackage> PivotPackage;
    TArray<USCS_Node*> ExistingRoots;
    FString PivotGeneratedPath, PivotSkeletonPath;
    TStrongObjectPtr<USimpleConstructionScript> AppendSCS;
    TStrongObjectPtr<UClass> AppendGenerated, AppendSkeleton;
    TStrongObjectPtr<UObject> AppendCDO;
    struct FExistingNode
    {
        TStrongObjectPtr<USCS_Node> Node;
        TStrongObjectPtr<UActorComponent> Template;
        UClass* Class = nullptr;
        FGuid Guid;
        FName Name;
        TArray<USCS_Node*> Children;
        // Copied authored roles for the complete pre-existing own SCS tree in UO only.
        TStrongObjectPtr<UClass> HeldClass;
        USCS_Node* Parent = nullptr;
        FString Socket, ParentName, ParentOwner;
        bool NativeParent = false, SceneRole = false;
        FVector Location = FVector::ZeroVector, Scale = FVector::OneVector;
        FRotator Rotation = FRotator::ZeroRotator;
        EComponentMobility::Type Mobility = EComponentMobility::Static;
        bool AbsoluteLocation = false, AbsoluteRotation = false, AbsoluteScale = false;
        bool Visible = false, Hidden = false;
        TArray<FString> Tags;
    };
    TArray<FExistingNode> ExistingNodes, CreatedBindings;
    TArray<USCS_Node*> ExistingChildren, AppendedChildren;
    TArray<TStrongObjectPtr<UObject>> ReturnedTemplates, HeldSourceCDOs;
    TMap<UClass*, UObject*> SourceCDOs;
    TMap<UStaticMesh*, UBodySetup*> MeshSetups;
    TArray<TStrongObjectPtr<UBodySetup>> HeldBodySetups;
    USCS_Node* PendingChild = nullptr;
};
bool AppendCurrent(FContext* C)
{
    if (!C || !C->AppendExisting) { return true; }
    if (C->Stopped) { return false; }
    if (C->NonRootPivot)
    {
        // No UObject getter precedes the non-object GT/GC/save/exit checks on callback return.
        if (!IsEditorGameThread() || IsGarbageCollecting() || UE::IsSavingPackage() || GExitPurge || IsEngineExitRequested() || GEditor != C->PivotEditor.Get()) { C->Stopped = true; return false; }
        if (!Loaded(GEditor) || IsPlaySessionActive() || !Loaded(C->PivotWorld.Get()) || C->PivotWorld->bIsTearingDown || C->PivotWorld->WorldType != EWorldType::Editor || GEditor->GetEditorWorldContext().World() != C->PivotWorld.Get()) { C->Stopped = true; return false; }
        auto* BP = C->Blueprint.Get(); auto* Generated = C->AppendGenerated.Get(); auto* Skeleton = C->AppendSkeleton.Get();
        if (!Loaded(BP) || !Loaded(C->PivotPackage.Get()) || BP->GetOutermost() != C->PivotPackage.Get() || !BP->SimpleConstructionScript.IsResolved() || !BP->GeneratedClass.GetGCPtr().IsResolved() || !BP->SkeletonGeneratedClass.GetGCPtr().IsResolved()
            || !Loaded(Generated) || !Loaded(Skeleton) || !Generated->ClassGeneratedBy.IsResolved() || !Skeleton->ClassGeneratedBy.IsResolved()) { C->Stopped = true; return false; }
    }
    UBlueprint* B = C->Blueprint.Get(); USimpleConstructionScript* S = C->AppendSCS.Get();
    bool Good = Loaded(B) && Loaded(S) && B->SimpleConstructionScript == S && B->GeneratedClass == C->AppendGenerated.Get() && B->SkeletonGeneratedClass == C->AppendSkeleton.Get()
        && Loaded(C->AppendGenerated.Get()) && Loaded(C->AppendSkeleton.Get()) && S->GetOuter() == B->GeneratedClass && B->GeneratedClass->ClassGeneratedBy == B && B->SkeletonGeneratedClass->ClassGeneratedBy == B
        && B->GeneratedClass->ClassDefaultObject.IsResolved() && B->GeneratedClass->ClassDefaultObject.Get() == C->AppendCDO.Get() && Loaded(C->AppendCDO.Get()) && C->AppendCDO->GetClass() == B->GeneratedClass;
    for (const auto& Source : C->SourceCDOs)
    {
        if (!Good || !Loaded(Source.Key) || !Source.Key->ClassDefaultObject.IsResolved() || Source.Key->ClassDefaultObject.Get() != Source.Value || (Source.Value && (!Loaded(Source.Value) || Source.Value->GetClass() != Source.Key))) { Good = false; break; }
    }
    if (Good)
    {
        const auto& All = S->GetAllNodes();
        int32 Extra = C->AppendedChildren.Num(); if (C->PendingChild && All.Num() == C->ExistingNodes.Num() + Extra + 1) { ++Extra; }
        Good = (C->NonRootPivot ? S->GetRootNodes() == C->ExistingRoots : S->GetRootNodes().Num() == 1 && S->GetRootNodes()[0] == C->Root) && All.Num() == C->ExistingNodes.Num() + Extra;
        for (int32 I = 0; Good && I < C->ExistingNodes.Num(); ++I)
        {
            const auto& E = C->ExistingNodes[I]; USCS_Node* N = E.Node.Get(); UActorComponent* T = E.Template.Get();
            Good = All[I] == N && Loaded(N) && N->GetOuter() == S && N->VariableGuid == E.Guid && N->GetVariableName() == E.Name && (!C->NonRootPivot || N->ComponentClass.IsResolved()) && N->ComponentClass == E.Class
                && N->ComponentTemplate.IsResolved() && N->ComponentTemplate.Get() == T && Loaded(T) && T->GetOuter() == B->GeneratedClass && T->GetClass() == E.Class && !T->IsRegistered() && !T->IsPreRegistering() && !T->GetOwner() && !T->GetWorld();
            if (Good && N == C->Root) { const USceneComponent* Scene = Cast<USceneComponent>(T); Good = Scene && !Scene->GetAttachParent() && Scene->GetAttachChildren().Num() == 0; }
            if (Good && C->NonRootPivot)
            {
                Good = Loaded(E.HeldClass.Get()) && E.HeldClass.Get() == E.Class && N->ComponentClass.IsResolved() && N->GetVariableName().ToString().Equals(E.Name.ToString(), ESearchCase::CaseSensitive)
                    && N->AttachToName.ToString().Equals(E.Socket, ESearchCase::CaseSensitive)
                    && N->ParentComponentOrVariableName.ToString().Equals(E.ParentName, ESearchCase::CaseSensitive) && N->ParentComponentOwnerClassName.ToString().Equals(E.ParentOwner, ESearchCase::CaseSensitive) && N->bIsParentComponentNative == E.NativeParent
                    && T->ComponentTags.Num() == E.Tags.Num();
                for (int32 Tag = 0; Good && Tag < E.Tags.Num(); ++Tag) { Good = T->ComponentTags[Tag].ToString().Equals(E.Tags[Tag], ESearchCase::CaseSensitive); }
                if (Good && E.SceneRole)
                {
                    const auto* Scene = static_cast<const USceneComponent*>(T);
                    Good = Scene->GetRelativeLocation() == E.Location && Scene->GetRelativeRotation() == E.Rotation && Scene->GetRelativeScale3D() == E.Scale
                        && Scene->Mobility == (N == C->Root && C->PivotMobilityPlanned ? EComponentMobility::Movable : E.Mobility)
                        && Scene->IsUsingAbsoluteLocation() == E.AbsoluteLocation && Scene->IsUsingAbsoluteRotation() == E.AbsoluteRotation && Scene->IsUsingAbsoluteScale() == E.AbsoluteScale
                        && Scene->GetVisibleFlag() == E.Visible && Scene->bHiddenInGame == E.Hidden && !Scene->GetAttachParent() && Scene->GetAttachChildren().Num() == 0;
                }
            }
            if (!Good) { break; } const auto& Children = N->GetChildNodes(); const int32 Suffix = N == C->Root ? Extra : 0;
            Good = Children.Num() == E.Children.Num() + Suffix;
            for (int32 J = 0; Good && J < E.Children.Num(); ++J) { Good = Children[J] == E.Children[J]; }
            for (int32 J = 0; Good && J < Suffix; ++J) { Good = Children[E.Children.Num() + J] == (J < C->AppendedChildren.Num() ? C->AppendedChildren[J] : C->PendingChild); }
        }
        for (int32 I = 0; Good && I < Extra; ++I) { Good = All[C->ExistingNodes.Num() + I] == (I < C->AppendedChildren.Num() ? C->AppendedChildren[I] : C->PendingChild); }
        for (const auto& E : C->CreatedBindings)
        {
            USCS_Node* N = E.Node.Get(); UActorComponent* T = E.Template.Get();
            if (!Good || !Loaded(N) || N->GetOuter() != S || N->VariableGuid != E.Guid || N->GetVariableName() != E.Name || N->ComponentClass != E.Class || !N->ComponentTemplate.IsResolved() || N->ComponentTemplate.Get() != T || !Loaded(T) || T->GetOuter() != B->GeneratedClass || T->GetClass() != E.Class || N->GetChildNodes().Num() != 0) { Good = false; break; }
        }
        for (const auto& H : C->Allocated)
        {
            USCS_Node* N = H.Get(); if (!Good || !Loaded(N) || N->GetOuter() != S || !N->ComponentTemplate.IsResolved()) { Good = false; break; }
            UActorComponent* T = N->ComponentTemplate.Get(); Good = Loaded(T) && T->GetOuter() == B->GeneratedClass && T->GetClass() == N->ComponentClass && !T->IsRegistered() && !T->IsPreRegistering() && !T->GetOwner() && !T->GetWorld();
        }
    }
    if (!Good) { C->Stopped = true; } return Good;
}
bool AppendCheck(FContext& C, FResult& R)
{
    if (AppendCurrent(&C)) { return true; }
    if (!R.Data->HasField(TEXT("existing_root_first_failure"))) { auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("stage"), R.Stage); J->SetStringField(TEXT("role"), TEXT("held target/SCS/old subtree/new attachment")); J->SetStringField(TEXT("expected"), TEXT("exact current bindings and original topology plus planned suffix")); J->SetStringField(TEXT("actual"), TEXT("compound current mismatch; stopped without further source read")); J->SetBoolField(TEXT("expected_actual_observed"), false); R.Data->SetObjectField(TEXT("existing_root_first_failure"), J); }
    return Fail(R, TEXT("rigid_chain_existing_root_current_lost"), TEXT("Existing-root current fence failed; retained actual prefix, no further SDK/readback/retry."), true);
}
bool BindAppendPhase(FContext& C, FResult& R)
{
    if (!C.AppendExisting) { return true; } if (C.Stopped) { return false; }
    if (C.NonRootPivot && (!IsEditorGameThread() || IsGarbageCollecting() || UE::IsSavingPackage() || GExitPurge || IsEngineExitRequested() || GEditor != C.PivotEditor.Get())) { C.Stopped = true; return AppendCheck(C, R); }
    UBlueprint* B = C.Blueprint.Get(); if (!Loaded(B) || (C.NonRootPivot && !B->SimpleConstructionScript.IsResolved()) || B->SimpleConstructionScript != C.AppendSCS.Get()) { C.Stopped = true; return AppendCheck(C, R); }
    // Only explicit structural/full-compile boundaries may reacquire generated classes/CDO.
    if (C.NonRootPivot)
    {
        if (!IsEditorGameThread() || IsGarbageCollecting() || UE::IsSavingPackage() || GExitPurge || IsEngineExitRequested() || GEditor != C.PivotEditor.Get()) { C.Stopped = true; return AppendCheck(C, R); }
        if (!Loaded(GEditor) || IsPlaySessionActive() || !Loaded(C.PivotWorld.Get()) || C.PivotWorld->bIsTearingDown || GEditor->GetEditorWorldContext().World() != C.PivotWorld.Get() || !B->GeneratedClass.GetGCPtr().IsResolved() || !B->SkeletonGeneratedClass.GetGCPtr().IsResolved()) { C.Stopped = true; return AppendCheck(C, R); }
    }
    C.AppendGenerated.Reset(B->GeneratedClass); C.AppendSkeleton.Reset(B->SkeletonGeneratedClass);
    if (C.NonRootPivot)
    {
        auto* Generated = C.AppendGenerated.Get(); auto* Skeleton = C.AppendSkeleton.Get();
        if (!Loaded(Generated) || !Loaded(Skeleton) || Generated->GetClass() != UBlueprintGeneratedClass::StaticClass() || Skeleton->GetClass() != UBlueprintGeneratedClass::StaticClass() || !Generated->ClassGeneratedBy.IsResolved() || !Skeleton->ClassGeneratedBy.IsResolved()
            || Generated->ClassGeneratedBy.Get() != B || Skeleton->ClassGeneratedBy.Get() != B || Generated->GetSuperClass() != AActor::StaticClass() || Skeleton->GetSuperClass() != AActor::StaticClass()
            || Generated->GetPathName() != C.PivotGeneratedPath || Skeleton->GetPathName() != C.PivotSkeletonPath) { C.Stopped = true; return AppendCheck(C, R); }
    }
    if (!Loaded(C.AppendGenerated.Get()) || !Loaded(C.AppendSkeleton.Get()) || !B->GeneratedClass->ClassDefaultObject.IsResolved()) { C.Stopped = true; return AppendCheck(C, R); }
    C.AppendCDO.Reset(B->GeneratedClass->ClassDefaultObject.Get());
    if (C.NonRootPivot && (!Loaded(C.AppendCDO.Get()) || C.AppendCDO->GetClass() != C.AppendGenerated.Get() || C.AppendCDO->GetOuter() != C.AppendGenerated->GetOuter() || C.AppendCDO->GetPackage() != C.PivotPackage.Get() || !C.AppendCDO->HasAllFlags(RF_ClassDefaultObject | RF_ArchetypeObject))) { C.Stopped = true; return AppendCheck(C, R); }
    return AppendCheck(C, R);
}
bool Sources(TSharedRef<FJsonObject>& J, FContext* Context = nullptr)
{
    for (UClass* Class : { AActor::StaticClass(), UBlueprint::StaticClass(), USCS_Node::StaticClass(), USimpleConstructionScript::StaticClass(), USceneComponent::StaticClass(), UStaticMeshComponent::StaticClass(), UPhysicsConstraintComponent::StaticClass(), UStaticMesh::StaticClass(), UBodySetup::StaticClass() })
    {
        auto Row = Identity(Class); J->SetObjectField(Class->GetPathName(), Row); Row->SetStringField(TEXT("class_flags"), LexToString(static_cast<uint32>(Class->GetClassFlags())));
        if (!Native(Class)) { J->SetStringField(TEXT("failed_class"), Class->GetPathName()); return false; }
        UObject* CDO = Class->GetDefaultObject(false); TStrongObjectPtr<UObject> HeldCDO(CDO);
        if (Context && Context->AppendExisting && !Context->SourceCDOs.Contains(Class)) { Context->SourceCDOs.Add(Class, CDO); Context->HeldSourceCDOs.Emplace(CDO); }
        if (!AppendCurrent(Context)) { return false; } Row->SetBoolField(TEXT("existing_cdo_available"), CDO != nullptr);
        if (!CDO) { if (Class == USceneComponent::StaticClass() || Class == UStaticMeshComponent::StaticClass() || Class == UPhysicsConstraintComponent::StaticClass()) { J->SetStringField(TEXT("failed_class"), Class->GetPathName()); return false; } continue; }
        auto Props = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("existing_cdo"), Identity(CDO)); Row->SetObjectField(TEXT("existing_cdo_properties"), Props);
        if (!Loaded(CDO) || CDO->GetClass() != Class || CDO->GetOuter() != Class->GetOuter() || !CDO->HasAnyFlags(RF_ClassDefaultObject) || !Properties(CDO, {}, Props, Context)) { J->SetStringField(TEXT("failed_class"), Class->GetPathName()); return false; }
    }
    return true;
}
bool Prepare(const FString& Input, FContext& C, FResult& R)
{
    if (!IsEditorGameThread() || IsPlaySessionActive()) { return Fail(R, TEXT("rigid_chain_editor_context_required"), TEXT("Editor GT without PIE/SIE required.")); }
    const FString P = Path(Input); C.Package = FPackageName::ObjectPathToPackageName(P); C.Blueprint.Reset(FindObject<UBlueprint>(nullptr, *P)); UBlueprint* B = C.Blueprint.Get(); R.Data->SetStringField(TEXT("blueprint"), P);
    if (!AssetPath(P) || !C.Package.StartsWith(TEXT("/Game/")) || !Loaded(B) || B->GetClass() != UBlueprint::StaticClass() || B->BlueprintType != BPTYPE_Normal || B->ParentClass != AActor::StaticClass()
        || B->GetOuter() != B->GetOutermost() || B->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject) || B->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn | PKG_EditorOnly | PKG_PlayInEditor)
        || !Loaded(B->GeneratedClass) || !Loaded(B->SkeletonGeneratedClass) || B->GeneratedClass->ClassGeneratedBy != B || B->SkeletonGeneratedClass->ClassGeneratedBy != B || B->GeneratedClass->GetSuperClass() != AActor::StaticClass()
        || B->bBeingCompiled || B->bQueuedForCompilation || B->bSuppressStructurallyModified || B->bIsRegeneratingOnLoad || B->Status == BS_Error || B->Status == BS_BeingCreated)
    { return Fail(R, TEXT("rigid_chain_blueprint_unsupported"), TEXT("Loaded current normal direct-native Actor Blueprint required; inherited/custom/live propagation remains incomplete.")); }
    AActor* CDO = Cast<AActor>(B->GeneratedClass->GetDefaultObject(false)); if (!Loaded(CDO) || CDO->GetClass() != B->GeneratedClass || CDO->GetOuter() != B->GeneratedClass->GetOuter()) { return Fail(R, TEXT("rigid_chain_cdo_unsupported"), TEXT("Existing current generated Actor CDO required.")); }
    int32 Scanned = 0;
    for (TObjectIterator<AActor> It; It; ++It) { if (++Scanned > 32768) { return Fail(R, TEXT("rigid_chain_instance_scan_bound"), TEXT("Loaded Actor scan exceeds bound.")); } if (Loaded(*It) && !It->IsTemplate() && It->IsA(B->GeneratedClass)) { return Fail(R, TEXT("rigid_chain_live_instance_unsupported"), TEXT("Loaded live selected Actor instance unsupported.")); } }
    Scanned = 0;
    for (TObjectIterator<UBlueprintGeneratedClass> It; It; ++It) { if (++Scanned > 32768) { return Fail(R, TEXT("rigid_chain_instance_scan_bound"), TEXT("Loaded class scan exceeds bound.")); } if (Loaded(*It) && *It != B->GeneratedClass && *It != B->SkeletonGeneratedClass && It->IsChildOf(B->GeneratedClass)) { return Fail(R, TEXT("rigid_chain_derived_class_unsupported"), TEXT("Loaded derived class unsupported.")); } }
    USimpleConstructionScript* SCS = B->SimpleConstructionScript.Get(); if (!Loaded(SCS) || SCS->GetOuter() != B->GeneratedClass || SCS->GetAllNodes().Num() > MaxNodes) { return Fail(R, TEXT("rigid_chain_scs_unsupported"), TEXT("Bounded own current SCS required.")); }
    TSet<USCS_Node*> Nodes; TSet<FName> Names; TSet<FGuid> Guids;
    for (USCS_Node* Node : SCS->GetAllNodes())
    {
        UActorComponent* Template = Loaded(Node) ? Node->ComponentTemplate.Get() : nullptr;
        if (!Loaded(Node) || Node->GetOuter() != SCS || Nodes.Contains(Node) || !Node->VariableGuid.IsValid() || Guids.Contains(Node->VariableGuid) || Node->GetVariableName().IsNone() || Names.Contains(Node->GetVariableName()) || !Native(Node->ComponentClass)
            || !Loaded(Template) || Template->GetClass() != Node->ComponentClass || Template->GetOuter() != B->GeneratedClass || !Template->HasAnyFlags(RF_ArchetypeObject) || Template->IsRegistered() || Template->IsPreRegistering() || Template->GetOwner() || Template->GetWorld())
        { return Fail(R, TEXT("rigid_chain_scs_ownership_unsupported"), TEXT("Unique own native unregistered/non-preregistering current SCS nodes/templates/GUIDs without owner/world required.")); }
        Nodes.Add(Node); Names.Add(Node->GetVariableName()); Guids.Add(Node->VariableGuid);
    }
    TSet<USCS_Node*> Visited; TArray<USCS_Node*> Pending;
    for (USCS_Node* Root : SCS->GetRootNodes()) { if (!Nodes.Contains(Root)) { return Fail(R, TEXT("rigid_chain_scs_topology_unsupported"), TEXT("Exact own roots required.")); } Pending.Add(Root); }
    for (int32 I = 0; I < Pending.Num(); ++I) { USCS_Node* Node = Pending[I]; if (Visited.Contains(Node)) { return Fail(R, TEXT("rigid_chain_scs_topology_unsupported"), TEXT("Repeated/cyclic SCS topology rejected.")); } Visited.Add(Node); for (USCS_Node* Child : Node->GetChildNodes()) { if (!Nodes.Contains(Child) || Pending.Num() >= MaxNodes) { return Fail(R, TEXT("rigid_chain_scs_topology_unsupported"), TEXT("Bounded own tree required.")); } Pending.Add(Child); } }
    return Visited.Num() == Nodes.Num() || Fail(R, TEXT("rigid_chain_scs_topology_unsupported"), TEXT("All own SCS nodes must be reachable."));
}
bool References(FContext& C, TSharedRef<FJsonObject>& J)
{
    for (const auto& Strong : C.Assets)
    {
        UObject* Asset = Strong.Get(); auto Row = MakeShared<FJsonObject>(); J->SetObjectField(ObjectPath(Asset), Row); Row->SetObjectField(TEXT("identity"), Identity(Asset)); auto Props = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("properties"), Props); auto D = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("disk"), D);
        if (!Loaded(Asset) || Asset->GetOutermost()->IsDirty() || !Properties(Asset, {}, Props, &C) || !Disk(Asset->GetOutermost()->GetName(), D) || !D->GetBoolField(TEXT("exists")) || D->GetNumberField(TEXT("bytes")) <= 0) { return false; }
        if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset))
        {
            UBodySetup* Setup = Mesh->GetBodySetup(); TStrongObjectPtr<UBodySetup> HeldSetup(Setup); if (!AppendCurrent(&C) || (C.AppendExisting && C.MeshSetups.FindRef(Mesh) != Setup)) { return false; } auto Shape = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("body_setup"), Shape); Shape->SetObjectField(TEXT("identity"), Identity(Setup)); auto ShapeProps = MakeShared<FJsonObject>(); Shape->SetObjectField(TEXT("properties"), ShapeProps);
            if (!Loaded(Setup) || Setup->GetClass() != UBodySetup::StaticClass() || Setup->GetOuter() != Mesh || !Properties(Setup, {}, ShapeProps, &C)) { return false; }
            // Public accessor performs the required async-property wait; never bypass private BodySetup storage.
            UBodySetup* RequeriedSetup = Mesh->GetBodySetup(); TStrongObjectPtr<UBodySetup> HeldRequeriedSetup(RequeriedSetup);
            if (!AppendCurrent(&C) || RequeriedSetup != Setup || Setup->AggGeom.GetElementCount() <= 0) { return false; }
            Shape->SetNumberField(TEXT("simple_collision_shape_count"), Setup->AggGeom.GetElementCount());
        }
    }
    return true;
}
struct FObservation
{
    TSharedRef<FJsonObject> State = MakeShared<FJsonObject>(), SourcesState = MakeShared<FJsonObject>(), ReferencesState = MakeShared<FJsonObject>(), DiskState = MakeShared<FJsonObject>(), OtherDirty = MakeShared<FJsonObject>(); bool bComplete = false;
};
bool Capture(FContext& C, FObservation& O, FResult& R, const TCHAR* Key)
{
    if (!AppendCheck(C, R)) { return false; }
    O = FObservation{}; auto J = MakeShared<FJsonObject>(); J->SetBoolField(TEXT("complete"), false); J->SetObjectField(TEXT("state"), O.State); J->SetObjectField(TEXT("native_sources"), O.SourcesState); J->SetObjectField(TEXT("references"), O.ReferencesState); J->SetObjectField(TEXT("disk"), O.DiskState); J->SetObjectField(TEXT("other_package_dirty"), O.OtherDirty); R.Data->SetObjectField(Key, J);
    UBlueprint* B = C.Blueprint.Get(); if (!Loaded(B) || !Loaded(B->GeneratedClass)) { return false; } UObject* CDO = B->GeneratedClass->GetDefaultObject(false); if (!Loaded(CDO)) { return false; }
    O.State->SetObjectField(TEXT("identity"), Identity(B)); O.State->SetObjectField(TEXT("cdo_identity"), Identity(CDO)); O.State->SetBoolField(TEXT("package_dirty"), B->GetOutermost()->IsDirty()); O.State->SetNumberField(TEXT("blueprint_status"), static_cast<uint8>(B->Status.GetValue()));
    auto Declarations = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("declarations"), Declarations);
    for (const FName Field : { FName(TEXT("NewVariables")), FName(TEXT("ImplementedInterfaces")), FName(TEXT("ParentClass")) }) { FProperty* P = FindFProperty<FProperty>(B->GetClass(), Field); FString S; if (!P || !P->ExportText_Direct(S, P->ContainerPtrToValuePtr<void>(B), P->ContainerPtrToValuePtr<void>(B), B, PPF_None) || !AppendCheck(C, R) || S.Len() > MaxText || !CleanText(S)) { return false; } Declarations->SetStringField(Field.ToString(), S); }
    auto Props = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("unselected_cdo_properties"), Props); TSet<FName> Skip = C.Names; if (C.ReplacedDefault) { Skip.Add(C.ReplacedDefault->GetVariableName()); } if (!Properties(CDO, Skip, Props, &C)) { return false; }
    auto Members = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("selected_cdo_members"), Members);
    for (const FName Name : C.Names) { auto Row = MakeShared<FJsonObject>(); Members->SetObjectField(Name.ToString(), Row); FObjectPropertyBase* P = FindFProperty<FObjectPropertyBase>(CDO->GetClass(), Name); Row->SetBoolField(TEXT("present"), P != nullptr); if (P) { if (!Loaded(P->PropertyClass) || !P->GetOwnerStruct() || P->ArrayDim != 1) { return false; } Row->SetStringField(TEXT("class"), P->PropertyClass->GetPathName()); Row->SetStringField(TEXT("owner"), P->GetOwnerStruct()->GetPathName()); Row->SetNumberField(TEXT("array_dim"), P->ArrayDim); Row->SetStringField(TEXT("property_flags"), LexToString(static_cast<uint64>(P->GetPropertyFlags()))); Row->SetStringField(TEXT("value"), ObjectPath(P->GetObjectPropertyValue_InContainer(CDO))); } }
    USimpleConstructionScript* SCS = B->SimpleConstructionScript.Get(); if (!Loaded(SCS) || SCS->GetAllNodes().Num() > MaxNodes) { return false; } O.State->SetObjectField(TEXT("scs_identity"), Identity(SCS));
    auto Cache = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("default_scene_root_cache"), Cache); USCS_Node* Default = SCS->GetDefaultSceneRootNode(); Cache->SetObjectField(TEXT("node"), Identity(Default)); Cache->SetObjectField(TEXT("template"), Identity(Loaded(Default) ? Default->ComponentTemplate.Get() : nullptr)); auto CacheProps = MakeShared<FJsonObject>(); Cache->SetObjectField(TEXT("template_properties"), CacheProps); auto CacheNodeProps = MakeShared<FJsonObject>(); Cache->SetObjectField(TEXT("node_properties"), CacheNodeProps);
    if (!Loaded(Default) || Default->GetOuter() != SCS || Default->ComponentClass != USceneComponent::StaticClass() || !Loaded(Default->ComponentTemplate) || !Properties(Default, { TEXT("ChildNodes") }, CacheNodeProps, &C) || !Properties(Default->ComponentTemplate, Default == C.Root ? TSet<FName>{ TEXT("Mobility") } : TSet<FName>{}, CacheProps, &C)) { return false; }
    TSet<USCS_Node*> Nodes; for (USCS_Node* Node : SCS->GetAllNodes()) { if (!Loaded(Node) || Node->GetOuter() != SCS || Nodes.Contains(Node) || !Loaded(Node->ComponentTemplate) || Node->ComponentTemplate->GetOuter() != B->GeneratedClass || Node->ComponentTemplate->GetClass() != Node->ComponentClass || !Node->VariableGuid.IsValid()) { return false; } Nodes.Add(Node); }
    auto Rows = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("local_scs"), Rows); TArray<TSharedPtr<FJsonValue>> Order, Roots;
    for (USCS_Node* Node : SCS->GetAllNodes())
    {
        auto Row = MakeShared<FJsonObject>(); Rows->SetObjectField(Node->VariableGuid.ToString(), Row); Row->SetObjectField(TEXT("identity"), Identity(Node)); Row->SetStringField(TEXT("name"), Node->GetVariableName().ToString()); Row->SetStringField(TEXT("class"), Node->ComponentClass->GetPathName()); Row->SetObjectField(TEXT("template_identity"), Identity(Node->ComponentTemplate)); auto NodeProps = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("properties"), NodeProps); if (!Properties(Node, { TEXT("ChildNodes") }, NodeProps, &C)) { return false; }
        auto TemplateProps = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("template_properties"), TemplateProps); if (!Properties(Node->ComponentTemplate, Node == C.Root ? TSet<FName>{ TEXT("Mobility") } : TSet<FName>{}, TemplateProps, &C)) { return false; }
        TArray<TSharedPtr<FJsonValue>> Children; for (USCS_Node* Child : Node->GetChildNodes()) { if (!Nodes.Contains(Child)) { return false; } Children.Add(MakeShared<FJsonValueString>(Child->VariableGuid.ToString())); } Row->SetArrayField(TEXT("children"), Children); Order.Add(MakeShared<FJsonValueString>(Node->VariableGuid.ToString()));
    }
    for (USCS_Node* Root : SCS->GetRootNodes()) { if (!Nodes.Contains(Root)) { return false; } Roots.Add(MakeShared<FJsonValueString>(Root->VariableGuid.ToString())); } O.State->SetArrayField(TEXT("order"), Order); O.State->SetArrayField(TEXT("roots"), Roots);
    auto Graph = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("graphs"), Graph); if (!Graphs(B, Graph, &C) || !Sources(O.SourcesState, &C) || !References(C, O.ReferencesState) || !Disk(C.Package, O.DiskState) || !Dirty(C.Package, O.OtherDirty)) { return false; }
    if (!AppendCheck(C, R)) { return false; }
    O.bComplete = true; J->SetBoolField(TEXT("complete"), true); return true;
}
bool PostcompileCdoEqual(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
{
    if (!A || !B || A->Type != EJson::Object || B->Type != EJson::Object) { return false; }
    const auto X = A->AsObject(), Y = B->AsObject(); if (X->Values.Num() != Y->Values.Num() || !X->GetBoolField(TEXT("valid")) || !Y->GetBoolField(TEXT("valid"))) { return false; }
    const FString Outer = X->GetStringField(TEXT("outer")), Class = X->GetStringField(TEXT("class"));
    if (!Outer.StartsWith(TEXT("/Game/")) || !Class.StartsWith(Outer + TEXT(".")) || X->GetStringField(TEXT("path")) != Outer + TEXT(".Default__") + FPackageName::ObjectPathToObjectName(Class)) { return false; }
    const FString Old = X->GetStringField(TEXT("object_flags")), Now = Y->GetStringField(TEXT("object_flags")); const uint64 Flags = FCString::Strtoui64(*Old, nullptr, 10);
    const uint64 CdoFlags = static_cast<uint64>(RF_ClassDefaultObject | RF_ArchetypeObject), LoadFlags = static_cast<uint64>(RF_WasLoaded | RF_LoadCompleted);
    if (LexToString(Flags) != Old || (Flags & CdoFlags) != CdoFlags || (Now != Old && Now != LexToString(Flags & ~LoadFlags))) { return false; }
    for (const auto& Field : X->Values) { if (Field.Key.ToView() != TEXT("object_flags") && !Equal(Field.Value, Y->TryGetField(Field.Key.ToView()))) { return false; } } return true;
}
bool Preserve(const FObservation& Before, const FObservation& After, FContext& C, FResult& R)
{
    const bool Complete = Before.bComplete && After.bComplete; R.Data->SetBoolField(TEXT("observations_complete"), Complete); if (!Complete) { return Fail(R, TEXT("rigid_chain_observation_incomplete"), TEXT("Bounded actual prefix incomplete; no subsequent action/save."), R.bAttempted); }
    bool State = true;
    const bool RetiredDefault = C.ReplacedDefault && R.Data->GetNumberField(TEXT("add_root_calls")) > 0;
    const bool Compiled = R.bCompileAttempted && R.Compiled.IsSet() && R.Compiled.GetValue();
    for (const TCHAR* K : { TEXT("identity"), TEXT("scs_identity"), TEXT("default_scene_root_cache"), TEXT("declarations"), TEXT("unselected_cdo_properties"), TEXT("graphs") }) { State &= Equal(Before.State->TryGetField(K), After.State->TryGetField(K)); }
    State &= Compiled ? PostcompileCdoEqual(Before.State->TryGetField(TEXT("cdo_identity")), After.State->TryGetField(TEXT("cdo_identity"))) : Equal(Before.State->TryGetField(TEXT("cdo_identity")), After.State->TryGetField(TEXT("cdo_identity")));
    R.Data->SetBoolField(TEXT("postcompile_owned_cdo_identity_verified"), Compiled && PostcompileCdoEqual(Before.State->TryGetField(TEXT("cdo_identity")), After.State->TryGetField(TEXT("cdo_identity"))));
    const auto Old = Before.State->GetObjectField(TEXT("local_scs")), Now = After.State->GetObjectField(TEXT("local_scs"));
    for (const auto& P : Old->Values)
    {
        const auto A = P.Value->AsObject(); if (RetiredDefault && A->GetStringField(TEXT("name")) == C.ReplacedDefault->GetVariableName().ToString()) { State &= !Now->HasField(P.Key.ToView()); continue; }
        const auto Q = Now->TryGetField(P.Key.ToView()); if (!Q || Q->Type != EJson::Object) { State = false; continue; } const auto B = Q->AsObject();
        for (const auto& Field : A->Values) { if (A->GetStringField(TEXT("name")) == C.RootName.ToString() && Field.Key.ToView() == TEXT("children")) { const auto& X = Field.Value->AsArray(), Y = B->GetArrayField(TEXT("children")); if (Y.Num() < X.Num()) { State = false; } else { for (int32 I = 0; I < X.Num(); ++I) { State &= Equal(X[I], Y[I]); } } } else { State &= Equal(Field.Value, B->TryGetField(Field.Key.ToView())); } }
    }
    TArray<TSharedPtr<FJsonValue>> OldOrder = Before.State->GetArrayField(TEXT("order")), NewOrder = After.State->GetArrayField(TEXT("order")); if (RetiredDefault) { OldOrder.RemoveAll([&C](const auto& V) { return V->AsString() == C.ReplacedDefault->VariableGuid.ToString(); }); }
    if (NewOrder.Num() < OldOrder.Num()) { State = false; } else { for (int32 I = 0; I < OldOrder.Num(); ++I) { State &= Equal(OldOrder[I], NewOrder[I]); } }
    if (!RetiredDefault) { State &= Equal(Before.State->TryGetField(TEXT("roots")), After.State->TryGetField(TEXT("roots"))); }
    const bool Source = Equal(Before.SourcesState, After.SourcesState) && Equal(Before.ReferencesState, After.ReferencesState), Other = OtherDirtyPreserved(Before.OtherDirty, After.OtherDirty, R, FString(TEXT("preserve/")) + R.Stage), D = Equal(Before.DiskState, After.DiskState);
    R.bKnown = true; R.bChanged = R.Data->GetNumberField(TEXT("attached_node_count")) > 0 || !Equal(Before.State, After.State) || !Source || !Other || !D;
    R.Data->SetBoolField(TEXT("source_preserved"), Source); R.Data->SetBoolField(TEXT("unselected_state_preserved"), State && Other); R.Data->SetBoolField(TEXT("disk_preserved_before_save"), D);
    return (State && Source && Other && D) || Fail(R, TEXT("rigid_chain_preservation_failed"), TEXT("Complete unrelated SCS/CDO/graphs/declarations/source/other-dirty or disk fence differs; no save/retry."));
}
void ClearFinal(FResult& R) { for (const TCHAR* K : { TEXT("observations_complete"), TEXT("source_preserved"), TEXT("unselected_state_preserved"), TEXT("independent_readback_passed") }) { R.Data->SetBoolField(K, false); } }
void Prefix(FContext& C, FResult& R, USCS_Node* Node)
{
    TStrongObjectPtr<USCS_Node> ReturnedNode(Node);
    if (Node) { C.Allocated.Emplace(Node); }
    if (!AppendCheck(C, R)) { return; }
    if (Loaded(Node) && Node->ComponentTemplate.IsResolved())
    {
        C.ReturnedTemplates.Emplace(Node->ComponentTemplate.Get());
        if (C.AppendExisting) { FContext::FExistingNode E; E.Node.Reset(Node); E.Template.Reset(Node->ComponentTemplate.Get()); E.Class = Node->ComponentClass; E.Guid = Node->VariableGuid; E.Name = Node->GetVariableName(); C.CreatedBindings.Add(MoveTemp(E)); }
    }
    auto Rows = R.Data->GetArrayField(TEXT("allocated_node_prefix")); auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("stage"), R.Stage); Row->SetObjectField(TEXT("node"), Identity(Node));
    if (Loaded(Node)) { Row->SetStringField(TEXT("name"), Node->GetVariableName().ToString()); Row->SetStringField(TEXT("guid"), Node->VariableGuid.ToString()); Row->SetObjectField(TEXT("template"), Identity(Node->ComponentTemplate)); auto Props = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("template_properties"), Props); const bool Read = Properties(Node->ComponentTemplate, {}, Props, &C); Row->SetBoolField(TEXT("template_properties_complete"), Read); if (C.AppendExisting && !Read) { C.Stopped = true; } }
    Rows.Add(MakeShared<FJsonValueObject>(Row)); R.Data->SetArrayField(TEXT("allocated_node_prefix"), Rows);
}
void AllocatedFailurePrefix(FContext& C, FResult& R)
{
    TArray<TSharedPtr<FJsonValue>> Rows; R.Data->SetArrayField(TEXT("allocated_nodes_at_failure"), Rows); bool Complete = C.Allocated.Num() <= 32;
    for (const auto& Strong : C.Allocated)
    {
        USCS_Node* Node = Strong.Get(); auto Row = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("node"), Identity(Node)); Row->SetObjectField(TEXT("template"), Identity(Loaded(Node) ? Node->ComponentTemplate.Get() : nullptr)); auto P = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("template_properties"), P);
        const bool Read = Loaded(Node) && Properties(Node->ComponentTemplate, {}, P); Row->SetBoolField(TEXT("template_properties_complete"), Read); Complete &= Read; Rows.Add(MakeShared<FJsonValueObject>(Row)); R.Data->SetArrayField(TEXT("allocated_nodes_at_failure"), Rows);
    }
    R.Data->SetBoolField(TEXT("allocated_nodes_at_failure_complete"), Complete); R.Data->SetStringField(TEXT("allocated_failure_scope"), TEXT("Actual strongly-held factory-returned nodes/templates including unattached ones, read without mutation. Null factory return cannot certify all unreturned/transient allocation effects; top unknown is preserved."));
}
struct FSettings
{
    float Swing1, Swing2, Twist, LinearDamping, AngularDamping, ProjectionLinearAlpha, ProjectionAngularAlpha, ProjectionLinearTolerance, ProjectionAngularTolerance;
};
FString NormalizeRoot(FString S)
{
    S = FPackageName::ExportTextPathToObjectPath(S).TrimStartAndEnd(); S.TrimQuotesInline(); S.ReplaceInline(TEXT("\\"), TEXT("/"));
    if (S.EndsWith(TEXT(".uasset"), ESearchCase::IgnoreCase)) { S.LeftChopInline(7); } if (S.Contains(TEXT("."))) { S = FPackageName::ObjectPathToPackageName(S); }
    while (S.Len() > 1 && S.EndsWith(TEXT("/"))) { S.LeftChopInline(1); } return S;
}
bool NoNull(const FString& S) { for (TCHAR C : S) { if (C == 0) { return false; } } return true; }
bool FreshTemplate(FContext& C, USCS_Node* Node, UClass* Class, FName Requested)
{
    UActorComponent* T = Loaded(Node) ? Node->ComponentTemplate.Get() : nullptr;
    USceneComponent* Scene = Cast<USceneComponent>(T);
    return Loaded(Node) && Node->GetOuter() == C.Blueprint->SimpleConstructionScript && Node->GetVariableName() == Requested && Node->VariableGuid.IsValid() && Node->ComponentClass == Class && Node->GetChildNodes().Num() == 0
        && Loaded(T) && T->GetClass() == Class && T->GetOuter() == C.Blueprint->GeneratedClass && T->GetFName() == FName(*(Requested.ToString() + USimpleConstructionScript::ComponentTemplateNameSuffix)) && T->HasAnyFlags(RF_ArchetypeObject) && !T->HasAnyFlags(RF_ClassDefaultObject | RF_Transient)
        && !T->IsRegistered() && !T->IsPreRegistering() && !T->GetOwner() && !T->GetWorld() && Scene && !Scene->GetAttachParent() && Scene->GetAttachChildren().Num() == 0;
}
void Delta(FResult& R, int32 RequestedCount, bool ExistingRoot)
{
    const int32 Created = static_cast<int32>(R.Data->GetNumberField(TEXT("attached_node_count"))); auto J = MakeShared<FJsonObject>();
    const int32 Updated = R.bAttempted && ExistingRoot ? 1 : 0;
    J->SetNumberField(TEXT("affected_count"), Created + Updated); J->SetNumberField(TEXT("created_count"), Created); J->SetNumberField(TEXT("updated_count"), Updated); J->SetNumberField(TEXT("removed_count"), 0);
    J->SetNumberField(TEXT("skipped_count"), R.Data->GetBoolField(TEXT("dry_run")) ? RequestedCount + 1 : FMath::Max(0, RequestedCount - Created)); J->SetNumberField(TEXT("failed_count"), R.bSuccess ? 0 : 1); R.Data->SetObjectField(TEXT("delta"), J);
    R.Data->SetStringField(TEXT("delta_scope"), TEXT("Legacy counts attached requested components and existing-root update; removed_count remains original zero. Additive created_node_count records factory-returned nodes even unattached; implicit_default_root_removed_count separately records public AddNode retirement, not object destruction."));
}
bool Readback(FContext& C, const TArray<FBodySpec>& Bodies, const TArray<FConstraintSpec>& Constraints, const FSettings& Settings, bool RequireMembers, FResult& R)
{
    if (!AppendCheck(C, R)) { return false; }
    UBlueprint* B = C.Blueprint.Get(); USCS_Node* Root = FindNode(B, C.RootName); USceneComponent* RT = Loaded(Root) ? Cast<USceneComponent>(Root->ComponentTemplate) : nullptr;
    const bool RootOK = Root == C.Root && Loaded(RT) && RT->GetClass() == USceneComponent::StaticClass() && RT->Mobility == EComponentMobility::Movable && !RT->IsRegistered() && !RT->IsPreRegistering() && !RT->GetOwner() && !RT->GetWorld() && !RT->GetAttachParent() && RT->GetAttachChildren().Num() == 0 && (C.NonRootPivot ? B->SimpleConstructionScript->GetRootNodes() == C.ExistingRoots && !C.ExistingRoots.Contains(Root) : B->SimpleConstructionScript->GetRootNodes().Num() == 1 && B->SimpleConstructionScript->GetRootNodes().Contains(Root));
    R.Data->SetBoolField(TEXT("root_readback_pass"), RootOK); bool Passed = RootOK;
    TArray<TSharedPtr<FJsonValue>> BodyRows, ConstraintRows; R.Data->SetArrayField(TEXT("body_readback"), BodyRows); R.Data->SetArrayField(TEXT("constraint_readback"), ConstraintRows);
    UObject* CDO = B->GeneratedClass->GetDefaultObject(false); const auto MemberOK = [&](FName N, UClass* Class) -> bool
    { FObjectPropertyBase* P = Loaded(CDO) ? FindFProperty<FObjectPropertyBase>(CDO->GetClass(), N) : nullptr; return !RequireMembers || (P && P->GetOwnerStruct() == B->GeneratedClass && P->PropertyClass == Class && P->ArrayDim == 1 && !P->HasAnyPropertyFlags(CPF_Parm | CPF_ReferenceParm | CPF_OutParm)); };
    Passed &= MemberOK(C.RootName, USceneComponent::StaticClass());
    TArray<FName> ExpectedChildren; if (C.AppendExisting) { for (USCS_Node* Child : C.ExistingChildren) { ExpectedChildren.Add(Child->GetVariableName()); } } for (const FBodySpec& Body : Bodies) { ExpectedChildren.Add(Body.ComponentFName); } for (const FConstraintSpec& Constraint : Constraints) { ExpectedChildren.Add(Constraint.ComponentFName); }
    if (!Loaded(Root) || Root->GetChildNodes().Num() != ExpectedChildren.Num()) { Passed = false; }
    else { for (int32 I = 0; I < ExpectedChildren.Num(); ++I) { Passed &= Loaded(Root->GetChildNodes()[I]) && Root->GetChildNodes()[I]->GetVariableName() == ExpectedChildren[I]; } }
    for (const FBodySpec& Body : Bodies)
    {
        USCS_Node* Node = FindNode(B, Body.ComponentFName); UStaticMeshComponent* T = Loaded(Node) ? Cast<UStaticMeshComponent>(Node->ComponentTemplate) : nullptr; auto J = MakeShared<FJsonObject>();
        J->SetStringField(TEXT("component_name"), Body.ComponentName); J->SetObjectField(TEXT("node"), Identity(Node)); J->SetObjectField(TEXT("template"), Identity(T)); J->SetStringField(TEXT("component_class"), Loaded(T) ? T->GetClass()->GetPathName() : FString()); J->SetStringField(TEXT("static_mesh"), Loaded(T) ? ObjectPath(T->GetStaticMesh()) : FString());
        J->SetArrayField(TEXT("relative_location"), VectorJson(Loaded(T) ? T->GetRelativeLocation() : FVector::ZeroVector)); J->SetArrayField(TEXT("relative_rotation"), RotatorJson(Loaded(T) ? T->GetRelativeRotation() : FRotator::ZeroRotator));
        J->SetBoolField(TEXT("simulate_physics"), Loaded(T) && T->BodyInstance.bSimulatePhysics); J->SetBoolField(TEXT("gravity_enabled"), Loaded(T) && T->BodyInstance.bEnableGravity); J->SetBoolField(TEXT("auto_weld"), Loaded(T) && T->BodyInstance.bAutoWeld); J->SetBoolField(TEXT("start_awake"), Loaded(T) && T->BodyInstance.bStartAwake); J->SetBoolField(TEXT("mass_override_enabled"), Loaded(T) && T->BodyInstance.bOverrideMass);
        J->SetNumberField(TEXT("mass_kg"), Loaded(T) ? T->BodyInstance.GetMassOverride() : 0.0); J->SetNumberField(TEXT("linear_damping"), Loaded(T) ? T->BodyInstance.LinearDamping : 0.0); J->SetNumberField(TEXT("angular_damping"), Loaded(T) ? T->BodyInstance.AngularDamping : 0.0);
        J->SetStringField(TEXT("collision_enabled"), Loaded(T) && T->BodyInstance.GetCollisionEnabled(false) == ECollisionEnabled::PhysicsOnly ? TEXT("PhysicsOnly") : TEXT("Unexpected")); J->SetStringField(TEXT("collision_response"), HasUniformIgnoreResponse(T) ? TEXT("Ignore") : TEXT("Unexpected")); J->SetBoolField(TEXT("overlap_events"), Loaded(T) && T->GetGenerateOverlapEvents()); J->SetBoolField(TEXT("notify_rigid_body_collision"), Loaded(T) && T->BodyInstance.bNotifyRigidBodyCollision);
        TStrongObjectPtr<UStaticMesh> HeldMesh(Body.StaticMesh);
        if (C.AppendExisting && (!Loaded(T) || T->GetStaticMesh() != HeldMesh.Get())) { C.Stopped = true; return Fail(R, TEXT("rigid_chain_mesh_source_changed"), TEXT("Selected body mesh differs from held plan before public BodySetup query."), true); }
        UBodySetup* Setup = C.AppendExisting ? HeldMesh->GetBodySetup() : (Loaded(T) && T->GetStaticMesh() ? T->GetStaticMesh()->GetBodySetup() : nullptr);
        TStrongObjectPtr<UBodySetup> ReturnedSetup(Setup); if (!AppendCheck(C, R)) { return false; }
        if (C.AppendExisting && Setup != Body.BodySetup) { C.Stopped = true; return Fail(R, TEXT("rigid_chain_mesh_source_changed"), TEXT("Public BodySetup returned a different exact held source; no subsequent source dereference."), true); }
        const int32 Shapes = Loaded(Setup) ? Setup->AggGeom.GetElementCount() : 0; J->SetNumberField(TEXT("simple_collision_shape_count"), Shapes);
        const bool OK = Loaded(T) && T->GetClass() == UStaticMeshComponent::StaticClass() && FreshTemplate(C, Node, UStaticMeshComponent::StaticClass(), Body.ComponentFName) && Root && Root->GetChildNodes().Contains(Node) && MemberOK(Body.ComponentFName, UStaticMeshComponent::StaticClass())
            && T->GetStaticMesh() == Body.StaticMesh && T->GetRelativeLocation().Equals(Body.RelativeLocation, 0.01f) && T->GetRelativeRotation().Equals(Body.RelativeRotation, 0.01f) && T->Mobility == EComponentMobility::Movable
            && T->BodyInstance.bSimulatePhysics == Body.bSimulatePhysics && T->BodyInstance.bEnableGravity == Body.bSimulatePhysics && !T->BodyInstance.bAutoWeld && T->BodyInstance.bStartAwake && T->BodyInstance.bOverrideMass
            && FMath::IsNearlyEqual(T->BodyInstance.GetMassOverride(), Body.MassKg, 0.001f) && FMath::IsNearlyEqual(T->BodyInstance.LinearDamping, Settings.LinearDamping, 0.001f) && FMath::IsNearlyEqual(T->BodyInstance.AngularDamping, Settings.AngularDamping, 0.001f)
            && T->BodyInstance.GetCollisionEnabled(false) == ECollisionEnabled::PhysicsOnly && T->GetCollisionObjectType() == ECC_PhysicsBody && HasUniformIgnoreResponse(T) && !T->GetGenerateOverlapEvents() && !T->BodyInstance.bNotifyRigidBodyCollision && T->CanCharacterStepUpOn == ECB_No && Shapes > 0;
        J->SetBoolField(TEXT("pass"), OK); BodyRows.Add(MakeShared<FJsonValueObject>(J)); R.Data->SetArrayField(TEXT("body_readback"), BodyRows); Passed &= OK;
    }
    for (const FConstraintSpec& Spec : Constraints)
    {
        USCS_Node* Node = FindNode(B, Spec.ComponentFName); UPhysicsConstraintComponent* T = Loaded(Node) ? Cast<UPhysicsConstraintComponent>(Node->ComponentTemplate) : nullptr; const FConstraintInstance* CI = Loaded(T) ? &T->ConstraintInstance : nullptr;
        float LA = 0, AA = 0, LT = 0, AT = 0; if (CI) { CI->GetProjectionParams(LA, AA, LT, AT); } auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("component_name"), Spec.ComponentName); J->SetObjectField(TEXT("node"), Identity(Node)); J->SetObjectField(TEXT("template"), Identity(T));
        J->SetStringField(TEXT("body1"), Loaded(T) ? T->ComponentName1.ComponentName.ToString() : FString()); J->SetStringField(TEXT("body2"), Loaded(T) ? T->ComponentName2.ComponentName.ToString() : FString()); J->SetArrayField(TEXT("relative_location"), VectorJson(Loaded(T) ? T->GetRelativeLocation() : FVector::ZeroVector)); J->SetArrayField(TEXT("relative_rotation"), RotatorJson(Loaded(T) ? T->GetRelativeRotation() : FRotator::ZeroRotator));
        J->SetNumberField(TEXT("linear_x_motion"), CI ? static_cast<uint8>(CI->GetLinearXMotion()) : -1); J->SetNumberField(TEXT("linear_y_motion"), CI ? static_cast<uint8>(CI->GetLinearYMotion()) : -1); J->SetNumberField(TEXT("linear_z_motion"), CI ? static_cast<uint8>(CI->GetLinearZMotion()) : -1);
        J->SetNumberField(TEXT("swing1_motion"), CI ? static_cast<uint8>(CI->GetAngularSwing1Motion()) : -1); J->SetNumberField(TEXT("swing2_motion"), CI ? static_cast<uint8>(CI->GetAngularSwing2Motion()) : -1); J->SetNumberField(TEXT("twist_motion"), CI ? static_cast<uint8>(CI->GetAngularTwistMotion()) : -1);
        J->SetNumberField(TEXT("linear_limit"), CI ? CI->GetLinearLimit() : 0); J->SetNumberField(TEXT("swing1_limit_degrees"), CI ? CI->GetAngularSwing1Limit() : 0); J->SetNumberField(TEXT("swing2_limit_degrees"), CI ? CI->GetAngularSwing2Limit() : 0); J->SetNumberField(TEXT("twist_limit_degrees"), CI ? CI->GetAngularTwistLimit() : 0);
        J->SetBoolField(TEXT("disable_collision"), CI && CI->IsCollisionDisabled()); J->SetBoolField(TEXT("projection_enabled"), CI && CI->IsProjectionEnabled()); J->SetNumberField(TEXT("projection_linear_alpha"), LA); J->SetNumberField(TEXT("projection_angular_alpha"), AA); J->SetNumberField(TEXT("projection_linear_tolerance"), LT); J->SetNumberField(TEXT("projection_angular_tolerance"), AT); J->SetBoolField(TEXT("mass_conditioning"), CI && CI->IsMassConditioningEnabled());
        const bool NullActors = Loaded(T) && !T->ConstraintActor1 && !T->ConstraintActor2 && T->OverrideComponent1.IsExplicitlyNull() && T->OverrideComponent2.IsExplicitlyNull(); J->SetBoolField(TEXT("fresh_default_actor_overrides_null"), NullActors);
        const bool OK = Loaded(T) && T->GetClass() == UPhysicsConstraintComponent::StaticClass() && FreshTemplate(C, Node, UPhysicsConstraintComponent::StaticClass(), Spec.ComponentFName) && Root && Root->GetChildNodes().Contains(Node) && MemberOK(Spec.ComponentFName, UPhysicsConstraintComponent::StaticClass()) && CI && NullActors
            && T->ComponentName1.ComponentName == Bodies[Spec.ParentBodyIndex].ComponentFName && T->ComponentName2.ComponentName == Bodies[Spec.ChildBodyIndex].ComponentFName && T->GetRelativeLocation().Equals(Bodies[Spec.ChildBodyIndex].RelativeLocation, 0.01f) && T->GetRelativeRotation().Equals(FRotator::ZeroRotator, 0.01f) && T->Mobility == EComponentMobility::Movable
            && CI->GetLinearXMotion() == LCM_Locked && CI->GetLinearYMotion() == LCM_Locked && CI->GetLinearZMotion() == LCM_Locked && CI->GetLinearLimit() == 0
            && CI->GetAngularSwing1Motion() == ACM_Limited && CI->GetAngularSwing2Motion() == ACM_Limited && CI->GetAngularTwistMotion() == ACM_Limited && FMath::IsNearlyEqual(CI->GetAngularSwing1Limit(), Settings.Swing1, 0.01f) && FMath::IsNearlyEqual(CI->GetAngularSwing2Limit(), Settings.Swing2, 0.01f) && FMath::IsNearlyEqual(CI->GetAngularTwistLimit(), Settings.Twist, 0.01f)
            && CI->IsCollisionDisabled() && CI->IsProjectionEnabled() && CI->IsMassConditioningEnabled() && FMath::IsNearlyEqual(LA, Settings.ProjectionLinearAlpha, 0.001f) && FMath::IsNearlyEqual(AA, Settings.ProjectionAngularAlpha, 0.001f) && FMath::IsNearlyEqual(LT, Settings.ProjectionLinearTolerance, 0.001f) && FMath::IsNearlyEqual(AT, Settings.ProjectionAngularTolerance, 0.001f);
        J->SetBoolField(TEXT("pass"), OK); ConstraintRows.Add(MakeShared<FJsonValueObject>(J)); R.Data->SetArrayField(TEXT("constraint_readback"), ConstraintRows); Passed &= OK;
    }
    R.Data->SetBoolField(TEXT("independent_readback_passed"), Passed); R.Data->SetBoolField(TEXT("compiled_selected_members_verified"), RequireMembers && Passed);
    return Passed || Fail(R, TEXT("rigid_chain_typed_readback_failed"), TEXT("Actual root/body/constraint defaults, own node/template/ordered parent topology or compiled scalar members differ; no save/retry."));
}
bool SavedEqual(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B, const FString& Package, const FString& At = FString())
{
    if (!A || !B || A->Type != B->Type) { return false; } if (At == TEXT("/package_dirty")) { return !B->AsBool(); }
    if (A->Type == EJson::Object)
    {
        const auto X = A->AsObject(), Y = B->AsObject(); if (X->Values.Num() != Y->Values.Num()) { return false; } FString Object; const bool Owned = X->TryGetStringField(TEXT("path"), Object) && Object.StartsWith(Package + TEXT("."));
        for (const auto& P : X->Values)
        {
            const auto Q = Y->TryGetField(P.Key.ToView()); if (!Q) { return false; }
            if (Owned && P.Key.ToView() == TEXT("object_flags")) { if (P.Value->Type != EJson::String || Q->Type != EJson::String) { return false; } const uint64 Flags = FCString::Strtoui64(*P.Value->AsString(), nullptr, 10); if (LexToString(Flags) != P.Value->AsString() || Q->AsString() != LexToString(Flags | static_cast<uint64>(RF_WasLoaded | RF_LoadCompleted))) { return false; } }
            else if (!SavedEqual(P.Value, Q, Package, At + TEXT("/") + FString(P.Key.ToView()))) { return false; }
        } return true;
    }
    if (A->Type == EJson::Array) { if (A->AsArray().Num() != B->AsArray().Num()) { return false; } for (int32 I = 0; I < A->AsArray().Num(); ++I) { if (!SavedEqual(A->AsArray()[I], B->AsArray()[I], Package, At + TEXT("/") + LexToString(I))) { return false; } } return true; } return Equal(A, B);
}
}
FString EnsureChain(const FString& BlueprintPath, const FString& BodyComponentsJson, const FString& RootComponentName, const FString& ConstraintPrefix,
    double Swing1LimitDegrees, double Swing2LimitDegrees, double TwistLimitDegrees, double LinearDamping, double AngularDamping, double ProjectionLinearAlpha, double ProjectionAngularAlpha,
    double ProjectionLinearTolerance, double ProjectionAngularTolerance, const FString& RequiredRoot, bool bSave, bool bDryRun)
{
    FResult R; Init(R, bDryRun, bSave); FContext C; FObservation Before, Current; TArray<FBodySpec> Bodies; TArray<FConstraintSpec> Constraints; FSettings Settings{}; bool ExistingRoot = false;
    R.Data->SetStringField(TEXT("blueprint_name"), BlueprintPath); R.Data->SetStringField(TEXT("root_component_name"), RootComponentName); R.Data->SetStringField(TEXT("required_root"), RequiredRoot);
    const bool Success = [&]() -> bool
    {
        FString Error; if (!NoNull(BlueprintPath) || !CleanText(BlueprintPath) || !NoNull(RequiredRoot) || !NoNull(RootComponentName) || !NoNull(ConstraintPrefix) || !Name(RootComponentName) || ConstraintPrefix.IsEmpty() || !CleanText(ConstraintPrefix) || !CleanText(RootComponentName)) { return Fail(R, TEXT("rigid_chain_names_invalid"), TEXT("Original nonempty root/prefix and valid bounded component names required.")); }
        if (!FMath::IsFinite(Swing1LimitDegrees) || !FMath::IsFinite(Swing2LimitDegrees) || !FMath::IsFinite(TwistLimitDegrees) || !FMath::IsFinite(LinearDamping) || !FMath::IsFinite(AngularDamping) || !FMath::IsFinite(ProjectionLinearAlpha) || !FMath::IsFinite(ProjectionAngularAlpha) || !FMath::IsFinite(ProjectionLinearTolerance) || !FMath::IsFinite(ProjectionAngularTolerance)
            || Swing1LimitDegrees <= 0 || Swing1LimitDegrees > 179 || Swing2LimitDegrees <= 0 || Swing2LimitDegrees > 179 || TwistLimitDegrees <= 0 || TwistLimitDegrees > 179 || LinearDamping < 0 || AngularDamping < 0 || ProjectionLinearAlpha < 0 || ProjectionLinearAlpha > 1 || ProjectionAngularAlpha < 0 || ProjectionAngularAlpha > 1 || ProjectionLinearTolerance <= 0 || ProjectionAngularTolerance <= 0
            || !FMath::IsFinite(static_cast<float>(LinearDamping)) || !FMath::IsFinite(static_cast<float>(AngularDamping)) || !FMath::IsFinite(static_cast<float>(ProjectionLinearTolerance)) || !FMath::IsFinite(static_cast<float>(ProjectionAngularTolerance))) { return Fail(R, TEXT("rigid_chain_numeric_inputs_invalid"), TEXT("Original finite angular(0,179], damping>=0, projection alpha[0,1], positive finite float tolerances required.")); }
        Settings = { static_cast<float>(Swing1LimitDegrees), static_cast<float>(Swing2LimitDegrees), static_cast<float>(TwistLimitDegrees), static_cast<float>(LinearDamping), static_cast<float>(AngularDamping), static_cast<float>(ProjectionLinearAlpha), static_cast<float>(ProjectionAngularAlpha), static_cast<float>(ProjectionLinearTolerance), static_cast<float>(ProjectionAngularTolerance) };
        TArray<TSharedPtr<FJsonValue>> Rows; if (BodyComponentsJson.Len() > MaxText || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(BodyComponentsJson), Rows) || Rows.Num() < 2 || Rows.Num() > 16) { return Fail(R, TEXT("rigid_chain_body_inputs_invalid"), TEXT("Bounded JSON array of original two through sixteen body objects required.")); }
        C.RootName = FName(*RootComponentName); C.Names.Add(C.RootName);
        for (int32 I = 0; I < Rows.Num(); ++I)
        {
            if (!Rows[I] || Rows[I]->Type != EJson::Object) { return Fail(R, TEXT("rigid_chain_body_inputs_invalid"), TEXT("Every body row must be an object.")); } const auto J = Rows[I]->AsObject(); FBodySpec Body;
            if (!J->TryGetStringField(TEXT("component_name"), Body.ComponentName) || !J->TryGetStringField(TEXT("static_mesh"), Body.StaticMeshPath) || !NoNull(Body.ComponentName) || !NoNull(Body.StaticMeshPath) || !CleanText(Body.ComponentName) || !CleanText(Body.StaticMeshPath) || !TryParseComponentName(Body.ComponentName, TEXT("component_name"), Body.ComponentFName, Error) || C.Names.Contains(Body.ComponentFName)) { return Fail(R, TEXT("rigid_chain_body_inputs_invalid"), TEXT("Unique valid component_name and StaticMesh reference required; no MCP runtime reference.")); }
            if (!TryReadVector3(J, TEXT("relative_location"), Body.RelativeLocation, true, Error)) { return Fail(R, TEXT("rigid_chain_body_inputs_invalid"), Error); } FVector Rotation = FVector::ZeroVector;
            if (!TryReadVector3(J, TEXT("relative_rotation"), Rotation, false, Error)) { return Fail(R, TEXT("rigid_chain_body_inputs_invalid"), Error); } Body.RelativeRotation = FRotator(Rotation.X, Rotation.Y, Rotation.Z);
            double Mass = I == Rows.Num() - 1 ? 2.0 : 0.35; J->TryGetNumberField(TEXT("mass_kg"), Mass); if (!FMath::IsFinite(Mass) || Mass <= 0 || Mass > 100000) { return Fail(R, TEXT("rigid_chain_body_inputs_invalid"), TEXT("Original positive mass_kg <=100000 required.")); } Body.MassKg = static_cast<float>(Mass); Body.bSimulatePhysics = I > 0;
            bool Simulate = Body.bSimulatePhysics; if (J->TryGetBoolField(TEXT("simulate_physics"), Simulate) && Simulate != Body.bSimulatePhysics) { return Fail(R, TEXT("rigid_chain_body_inputs_invalid"), TEXT("Original anchored first false/rest true simulation contract required.")); }
            Body.StaticMeshPath = Path(Body.StaticMeshPath); if (!AssetPath(Body.StaticMeshPath)) { return Fail(R, TEXT("rigid_chain_mesh_reference_unsupported"), TEXT("Explicit native Game/Engine StaticMesh asset path required.")); } C.Names.Add(Body.ComponentFName); Bodies.Add(Body);
        }
        for (int32 I = 0; I + 1 < Bodies.Num(); ++I)
        {
            FConstraintSpec Spec; Spec.ParentBodyIndex = I; Spec.ChildBodyIndex = I + 1; Spec.ComponentName = MakeConstraintName(ConstraintPrefix, Bodies[I].ComponentName, Bodies[I + 1].ComponentName);
            if (!TryParseComponentName(Spec.ComponentName, TEXT("derived_constraint_component_name"), Spec.ComponentFName, Error) || C.Names.Contains(Spec.ComponentFName)) { return Fail(R, TEXT("rigid_chain_constraint_names_invalid"), TEXT("Derived constraint names must be unique valid FNames.")); } C.Names.Add(Spec.ComponentFName); Constraints.Add(Spec);
        }
        if (!Prepare(BlueprintPath, C, R)) { return false; } const FString Root = NormalizeRoot(RequiredRoot); if (Root.IsEmpty() || Root.Len() > 1024 || !CleanText(Root) || !(C.Package.Equals(Root, ESearchCase::IgnoreCase) || C.Package.StartsWith(Root + TEXT("/"), ESearchCase::IgnoreCase))) { return Fail(R, TEXT("rigid_chain_required_root_mismatch"), TEXT("Target Blueprint must lie under original normalized required_root.")); }
        USimpleConstructionScript* SCS = C.Blueprint->SimpleConstructionScript.Get(); C.Root = FindNode(C.Blueprint.Get(), C.RootName); ExistingRoot = C.Root != nullptr;
        USCS_Node* Only = SCS->GetAllNodes().Num() == 1 ? SCS->GetAllNodes()[0] : nullptr;
        USCS_Node* SelectedRoot = ExistingRoot ? C.Root : Only;
        USCS_Node* TopRoot = SCS->GetRootNodes().Num() == 1 ? SCS->GetRootNodes()[0] : nullptr;
        C.NonRootPivot = ExistingRoot && Loaded(TopRoot) && TopRoot != C.Root;
        if (!Loaded(SelectedRoot) || SCS->GetRootNodes().Num() != 1 || (!C.NonRootPivot && !SCS->GetRootNodes().Contains(SelectedRoot)) || (!ExistingRoot && SelectedRoot->GetChildNodes().Num() != 0) || SelectedRoot->ComponentClass != USceneComponent::StaticClass() || (!C.NonRootPivot && (!SelectedRoot->ParentComponentOrVariableName.IsNone() || !SelectedRoot->ParentComponentOwnerClassName.IsNone() || SelectedRoot->bIsParentComponentNative || !SelectedRoot->AttachToName.IsNone()))) { return Fail(R, TEXT("rigid_chain_existing_tree_unsupported"), TEXT("One own exact native Scene root required; existing native subtree may be preserved, while new-root replacement remains lone leaf DefaultSceneRoot only.")); }
        if (C.NonRootPivot)
        {
            if (!IsEditorGameThread() || IsGarbageCollecting() || UE::IsSavingPackage() || GExitPurge || IsEngineExitRequested() || !Loaded(GEditor) || IsPlaySessionActive()) { return Fail(R, TEXT("rigid_chain_pivot_context_unavailable"), TEXT("Idle Editor GT required before enrolling the existing nonroot pivot roles.")); }
            if (!Loaded(TopRoot) || TopRoot->GetClass() != USCS_Node::StaticClass() || !TopRoot->ComponentClass.IsResolved() || TopRoot->ComponentClass.Get() != USceneComponent::StaticClass() || !TopRoot->ParentComponentOrVariableName.IsNone() || !TopRoot->ParentComponentOwnerClassName.IsNone() || TopRoot->bIsParentComponentNative || !TopRoot->AttachToName.IsNone() || !SCS->FindParentNode(C.Root)) { return Fail(R, TEXT("rigid_chain_pivot_tree_pending"), TEXT("A named own nonroot Scene pivot below the existing parentless exact Scene root required; inherited/native-bound or multiple-root variants remain incomplete.")); }
            C.PivotEditor.Reset(GEditor); C.PivotWorld.Reset(GEditor->GetEditorWorldContext().World()); C.PivotPackage.Reset(C.Blueprint->GetOutermost()); C.ExistingRoots = SCS->GetRootNodes(); C.PivotGeneratedPath = C.Blueprint->GeneratedClass->GetPathName(); C.PivotSkeletonPath = C.Blueprint->SkeletonGeneratedClass->GetPathName();
            if (!Loaded(C.PivotWorld.Get()) || C.PivotWorld->bIsTearingDown || C.PivotWorld->WorldType != EWorldType::Editor) { return Fail(R, TEXT("rigid_chain_pivot_world_unavailable"), TEXT("Held live Editor world required before source observation.")); }
        }
        C.AppendExisting = ExistingRoot && SCS->GetAllNodes().Num() > 1;
        if (C.AppendExisting)
        {
            if (SCS->GetAllNodes().Num() + Bodies.Num() + Constraints.Num() > MaxNodes) { return Fail(R, TEXT("rigid_chain_existing_tree_bound"), TEXT("Existing subtree plus requested suffix exceeds bounded SCS inventory.")); }
            USceneComponent* ExistingRootTemplate = Cast<USceneComponent>(C.Root->ComponentTemplate.Get());
            if (!ExistingRootTemplate || ExistingRootTemplate->GetAttachParent() || ExistingRootTemplate->GetAttachChildren().Num() != 0) { return Fail(R, TEXT("rigid_chain_existing_root_attachment_unsupported"), TEXT("Selected SCS root template must have no actual SceneComponent attachments before SetMobility; SCS child nodes are separately preserved.")); }
            C.AppendSCS.Reset(SCS); C.ExistingChildren = C.Root->GetChildNodes();
            for (USCS_Node* N : SCS->GetAllNodes())
            {
                if (C.NonRootPivot && (N->GetClass() != USCS_Node::StaticClass() || !N->ComponentClass.IsResolved() || !N->ComponentTemplate.IsResolved())) { return Fail(R, TEXT("rigid_chain_pivot_node_pending"), TEXT("Exact native own SCS nodes and resolved class/template slots required before existing-role capture.")); }
                FContext::FExistingNode E; E.Node.Reset(N); E.Template.Reset(N->ComponentTemplate.Get()); E.Class = N->ComponentClass; E.Guid = N->VariableGuid; E.Name = N->GetVariableName(); E.Children = N->GetChildNodes();
                if (C.NonRootPivot)
                {
                    E.HeldClass.Reset(E.Class); E.Parent = SCS->FindParentNode(N); E.Socket = N->AttachToName.ToString(); E.ParentName = N->ParentComponentOrVariableName.ToString(); E.ParentOwner = N->ParentComponentOwnerClassName.ToString(); E.NativeParent = N->bIsParentComponentNative;
                    if (E.Template->ComponentTags.Num() > MaxProperties) { return Fail(R, TEXT("rigid_chain_pivot_tag_bound"), TEXT("Bounded existing authored component tags required before source observation.")); }
                    for (FName Tag : E.Template->ComponentTags) { E.Tags.Add(Tag.ToString()); }
                    E.SceneRole = E.Class->IsChildOf(USceneComponent::StaticClass());
                    if (E.SceneRole)
                    {
                        const auto* Scene = static_cast<const USceneComponent*>(E.Template.Get());
                        if (Scene->GetAttachParent() || Scene->GetAttachChildren().Num() != 0) { return Fail(R, TEXT("rigid_chain_pivot_template_attachment_pending"), TEXT("Serialized SCS ancestry is supported; physically attached templates remain incomplete before SetMobility callback propagation.")); }
                        E.Location = Scene->GetRelativeLocation(); E.Rotation = Scene->GetRelativeRotation(); E.Scale = Scene->GetRelativeScale3D(); E.Mobility = Scene->Mobility; E.AbsoluteLocation = Scene->IsUsingAbsoluteLocation(); E.AbsoluteRotation = Scene->IsUsingAbsoluteRotation(); E.AbsoluteScale = Scene->IsUsingAbsoluteScale(); E.Visible = Scene->GetVisibleFlag(); E.Hidden = Scene->bHiddenInGame;
                    }
                }
                C.ExistingNodes.Add(MoveTemp(E));
            }
            if (!BindAppendPhase(C, R)) { return false; }
            R.Data->SetBoolField(TEXT("native_existing_root_rigid_chain_append"), true); R.Data->SetNumberField(TEXT("existing_scs_node_count"), C.ExistingNodes.Num()); R.Data->SetNumberField(TEXT("existing_root_child_count"), C.ExistingChildren.Num());
            TArray<TSharedPtr<FJsonValue>> PrefixNames; for (USCS_Node* N : C.ExistingChildren) { PrefixNames.Add(MakeShared<FJsonValueString>(N->GetVariableName().ToString())); } R.Data->SetArrayField(TEXT("existing_root_child_prefix"), PrefixNames);
            if (C.NonRootPivot)
            {
                R.Data->SetBoolField(TEXT("native_existing_nonroot_rigid_chain_append"), true); R.Data->SetStringField(TEXT("existing_pivot_guid"), C.Root->VariableGuid.ToString()); R.Data->SetStringField(TEXT("existing_top_root_guid"), TopRoot->VariableGuid.ToString());
                R.Data->SetStringField(TEXT("effect_scope"), TEXT("Named existing own exact Scene nonroot pivot below one own Scene top root. Original root/ancestor/sibling/node GUID/name/socket/relative/absolute/tags/visibility and prior children are captured; only selected pivot Mobility and planned child suffix may change. Existing full property/graph/source/disk snapshots remain authoritative. Serialized SCS ancestry is distinct from physical template attachment. No reparenting/runtime simulation/overwrite/rollback/retry."));
            }
            else R.Data->SetStringField(TEXT("effect_scope"), TEXT("Existing own exact native Scene root with bounded native unregistered subtree: preserve prior node/template/GUID/order/properties except selected root Mobility; append only planned StaticMesh bodies and PhysicsConstraint siblings. Explicit compile/save retained; runtime/live/derived/custom propagation incomplete."));
        }
        if (!ExistingRoot)
        {
            if (Only != SCS->GetDefaultSceneRootNode()) { return Fail(R, TEXT("rigid_chain_root_creation_unsupported"), TEXT("New root supports only public replacement of lone leaf DefaultSceneRoot.")); } C.ReplacedDefault = Only; C.ReplacedDefaultKeepAlive.Reset(Only);
            TArray<UEdGraph*> GraphList; C.Blueprint->GetAllGraphs(GraphList); if (GraphList.Num() > MaxGraphs) { return Fail(R, TEXT("rigid_chain_root_references_unsupported"), TEXT("Current graph count exceeds finite bound before default-root retirement.")); } for (UEdGraph* G : GraphList) { if (!Loaded(G) || G->Nodes.Num() > 4096) { return Fail(R, TEXT("rigid_chain_root_references_unsupported"), TEXT("Bounded existing graph proof required before default-root retirement.")); } for (UEdGraphNode* N : G->Nodes) { if (!Loaded(N) || N->Pins.Num() > MaxPins) { return Fail(R, TEXT("rigid_chain_root_references_unsupported"), TEXT("Current bounded graph node required.")); }
                if (const UK2Node_Variable* V = Cast<UK2Node_Variable>(N)) { if (V->VariableReference.GetMemberGuid() == Only->VariableGuid || (V->VariableReference.IsSelfContext() && V->VariableReference.GetMemberName() == Only->GetVariableName())) { return Fail(R, TEXT("rigid_chain_root_references_unsupported"), TEXT("Default root has selected variable graph reference; no graph migration.")); } }
                if (const UK2Node_ComponentBoundEvent* E = Cast<UK2Node_ComponentBoundEvent>(N)) { if (E->GetComponentPropertyName() == Only->GetVariableName()) { return Fail(R, TEXT("rigid_chain_root_references_unsupported"), TEXT("Default root has selected bound event; no graph migration.")); } }
                for (const UEdGraphPin* Pin : N->Pins) { if (!Pin || Pin->GetOwningNodeUnchecked() != N || Pin->DefaultObject == Only->ComponentTemplate) { return Fail(R, TEXT("rigid_chain_root_references_unsupported"), TEXT("Default root literal/malformed pin reference unsupported.")); } }
            } }
            TArray<UObject*> Instances; Only->ComponentTemplate->GetArchetypeInstances(Instances); if (Instances.Num()) { return Fail(R, TEXT("rigid_chain_root_archetype_instances_unsupported"), TEXT("Existing default-root archetype instances unsupported before retirement.")); }
        }
        else if (C.Root != SelectedRoot) { return Fail(R, TEXT("rigid_chain_existing_tree_unsupported"), TEXT("Requested existing root must be the current own Scene root.")); }
        for (const FName Requested : C.Names)
        {
            if (ExistingRoot && Requested == C.RootName) { continue; } const FString Slot = Requested.ToString() + USimpleConstructionScript::ComponentTemplateNameSuffix;
            if (FindNode(C.Blueprint.Get(), Requested) || FindFProperty<FProperty>(C.Blueprint->GeneratedClass, Requested) || FindFProperty<FProperty>(C.Blueprint->SkeletonGeneratedClass, Requested) || C.Blueprint->GeneratedClass->FindFunctionByName(Requested) || C.Blueprint->SkeletonGeneratedClass->FindFunctionByName(Requested) || FindObject<UObject>(C.Blueprint->GeneratedClass, *Slot) || FindObject<UObject>(C.Blueprint->SkeletonGeneratedClass, *Slot)) { return Fail(R, TEXT("rigid_chain_component_exists"), TEXT("Original fresh-body/constraint refusal plus exact generated/skeleton field/function/standard-slot collision guard; no public CreateNode implicit eviction.")); }
        }
        auto SourcePreflight = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("native_source_preflight"), SourcePreflight); if (!Sources(SourcePreflight, &C)) { return Fail(R, TEXT("rigid_chain_native_source_unobservable"), TEXT("Current native class/existing CDO source cannot be read without warming; retained finite source prefix.")); }
        for (FBodySpec& Body : Bodies)
        {
            R.Stage = TEXT("load_static_mesh"); auto Receipts = R.Data->GetArrayField(TEXT("asset_load_calls")); auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("requested_path"), Body.StaticMeshPath); Row->SetBoolField(TEXT("attempted"), true); Receipts.Add(MakeShared<FJsonValueObject>(Row)); R.Data->SetArrayField(TEXT("asset_load_calls"), Receipts); Count(R, TEXT("asset_load_count")); if (C.AppendExisting) { R.bKnown = false; R.Data->SetBoolField(TEXT("asset_load_global_effects_observed"), false); } UObject* Asset = UEditorAssetLibrary::LoadAsset(Body.StaticMeshPath); TStrongObjectPtr<UObject> ReturnedAsset(Asset); if (!AppendCheck(C, R)) { return false; } Row->SetObjectField(TEXT("returned_object"), Identity(Asset));
            UStaticMesh* Mesh = Cast<UStaticMesh>(Asset); if (!Loaded(Mesh) || Mesh->GetClass() != UStaticMesh::StaticClass() || Mesh->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject) || Mesh->GetPathName() != Body.StaticMeshPath || Mesh->GetOutermost()->IsDirty()) { return Fail(R, TEXT("rigid_chain_mesh_unavailable"), TEXT("Exact requested clean native StaticMesh required; explicit loader/PostLoad prefix retained.")); }
            UBodySetup* Setup = Mesh->GetBodySetup(); TStrongObjectPtr<UBodySetup> ReturnedSetup(Setup); if (!AppendCheck(C, R)) { return false; } if (!Loaded(Setup) || Setup->GetClass() != UBodySetup::StaticClass() || Setup->GetOuter() != Mesh || Setup->AggGeom.GetElementCount() <= 0) { return Fail(R, TEXT("rigid_chain_simple_collision_required"), TEXT("Original positive simple-collision shape count required for each mesh; no collision generation/runtime simulation.")); }
            if (C.AppendExisting && C.MeshSetups.Contains(Mesh) && C.MeshSetups.FindRef(Mesh) != Setup) { C.Stopped = true; return Fail(R, TEXT("rigid_chain_mesh_source_changed"), TEXT("Repeated planned mesh BodySetup binding changed after public load/accessor; no further source SDK."), true); }
            Body.StaticMesh = Mesh; Body.BodySetup = Setup; C.Assets.Emplace(Mesh); C.HeldBodySetups.Emplace(Setup); C.MeshSetups.Add(Mesh, Setup);
        }
        R.Stage = TEXT("selected_preflight"); if (!Capture(C, Before, R, TEXT("before_observation"))) { return Fail(R, TEXT("rigid_chain_before_unobservable"), TEXT("After explicit mesh loads, complete selected target/source/disk/dirty baseline required before authoring.")); }
        R.Data->SetObjectField(TEXT("before"), CaptureChainState(C.Blueprint.Get(), C.RootName, Bodies, Constraints)); R.Data->SetNumberField(TEXT("body_count"), Bodies.Num()); R.Data->SetNumberField(TEXT("constraint_count"), Constraints.Num()); R.Data->SetBoolField(TEXT("root_will_be_created"), !ExistingRoot); R.Data->SetBoolField(TEXT("collision_disabled"), true); R.Data->SetBoolField(TEXT("anchor_first_body"), true);
        auto Plan = MakeShared<FJsonObject>(); Plan->SetStringField(TEXT("blueprint"), C.Blueprint->GetPathName()); Plan->SetBoolField(TEXT("root_exists"), true); Plan->SetNumberField(TEXT("requested_body_count_present"), Bodies.Num()); Plan->SetNumberField(TEXT("requested_constraint_count_present"), Constraints.Num()); Plan->SetNumberField(TEXT("scs_node_count"), SCS->GetAllNodes().Num() + Bodies.Num() + Constraints.Num() + (ExistingRoot ? 0 : 1)); Plan->SetBoolField(TEXT("package_dirty"), true); R.Data->SetObjectField(TEXT("planned_after"), Plan); R.Data->SetNumberField(TEXT("expected_actual_scs_node_count"), (C.AppendExisting ? C.ExistingNodes.Num() : 1) + Bodies.Num() + Constraints.Num());
        R.Data->SetStringField(TEXT("planned_after_scope"), TEXT("Original legacy prediction retained; its scs_node_count includes the old DefaultSceneRoot. expected_actual_scs_node_count separately accounts for public implicit retirement."));
        if (bDryRun)
        {
            if (!Capture(C, Current, R, TEXT("after_observation")) || !Preserve(Before, Current, C, R)) { return false; } R.bChanged = false; R.bPost = true; R.Data->SetBoolField(TEXT("can_apply"), true); R.Data->SetBoolField(TEXT("verification_pass"), true); R.Data->SetStringField(TEXT("verification_level"), TEXT("rigid_constraint_chain_preflight")); R.Data->SetBoolField(TEXT("independent_readback_passed"), true); R.Data->SetObjectField(TEXT("after"), CaptureChainState(C.Blueprint.Get(), C.RootName, Bodies, Constraints)); return true;
        }
        R.bAttempted = true; R.bKnown = false; ClearFinal(R); Count(R, TEXT("explicit_modify_calls")); C.Blueprint->Modify(); if (!AppendCheck(C, R)) { return false; } Count(R, TEXT("explicit_modify_calls")); SCS->Modify(); if (!AppendCheck(C, R)) { return false; }
        if (!ExistingRoot)
        {
            R.Stage = TEXT("create_root_component"); Count(R, TEXT("create_node_attempts")); C.Root = SCS->CreateNode(USceneComponent::StaticClass(), C.RootName); Prefix(C, R, C.Root); if (Loaded(C.Root)) { Count(R, TEXT("created_node_count")); }
            if (!FreshTemplate(C, C.Root, USceneComponent::StaticClass(), C.RootName)) { return Fail(R, TEXT("rigid_chain_root_factory_failed"), TEXT("Actual factory prefix retained; exact fresh requested native root/template required, no attachment/retry."), true); }
            Count(R, TEXT("add_root_calls")); SCS->AddNode(C.Root); Count(R, TEXT("attached_node_count")); const bool Removed = !SCS->GetAllNodes().Contains(C.ReplacedDefault) && !SCS->GetRootNodes().Contains(C.ReplacedDefault); R.Data->SetBoolField(TEXT("default_root_no_longer_reachable"), Removed); if (Removed) { Count(R, TEXT("implicit_default_root_removed_count")); }
            R.Data->SetObjectField(TEXT("retained_default_root"), Identity(C.ReplacedDefault)); R.Data->SetBoolField(TEXT("retained_default_root_held_alive"), C.ReplacedDefaultKeepAlive.IsValid()); R.Data->SetBoolField(TEXT("default_root_destruction_or_gc_requested"), false);
            if (!Removed || FindNode(C.Blueprint.Get(), C.RootName) != C.Root) { return Fail(R, TEXT("rigid_chain_root_attachment_failed"), TEXT("Public AddNode did not produce the exact new root/old-default reachable disposition; actual prefix retained."), true); }
        }
        R.Stage = TEXT("configure_root_component"); Count(R, TEXT("explicit_modify_calls")); C.Root->Modify(); if (!AppendCheck(C, R)) { return false; } USceneComponent* RootTemplate = Cast<USceneComponent>(C.Root->ComponentTemplate); Count(R, TEXT("explicit_modify_calls")); RootTemplate->Modify(); if (!AppendCheck(C, R)) { return false; } Count(R, TEXT("public_setter_calls")); if (C.NonRootPivot) { C.PivotMobilityPlanned = true; } RootTemplate->SetMobility(EComponentMobility::Movable); if (!AppendCheck(C, R)) { return false; }
        for (const FBodySpec& Body : Bodies)
        {
            R.Stage = TEXT("create_body_component"); Count(R, TEXT("create_node_attempts")); USCS_Node* Node = SCS->CreateNode(UStaticMeshComponent::StaticClass(), Body.ComponentFName); TStrongObjectPtr<USCS_Node> ReturnedNode(Node); if (Loaded(Node)) { Count(R, TEXT("created_node_count")); } Prefix(C, R, Node); if (!AppendCheck(C, R)) { return false; }
            if (!FreshTemplate(C, Node, UStaticMeshComponent::StaticClass(), Body.ComponentFName)) { return Fail(R, TEXT("rigid_chain_body_factory_failed"), TEXT("Actual node/template factory prefix retained; no further body/constraint creation."), true); }
            UStaticMeshComponent* T = Cast<UStaticMeshComponent>(Node->ComponentTemplate); R.Stage = TEXT("configure_body_component"); Count(R, TEXT("explicit_modify_calls")); T->Modify(); if (!AppendCheck(C, R)) { return false; } Count(R, TEXT("body_configuration_calls"));
            Count(R, TEXT("public_setter_calls")); const bool MeshSet = T->SetStaticMesh(Body.StaticMesh); R.Data->SetBoolField(TEXT("last_static_mesh_setter_returned"), MeshSet); if (!AppendCheck(C, R)) { return false; } if (!MeshSet && T->GetStaticMesh() != Body.StaticMesh) { return Fail(R, TEXT("rigid_chain_mesh_setter_failed"), TEXT("Native body SetStaticMesh returned false without the requested value; retained unattached factory/setter prefix."), true); }
            Count(R, TEXT("public_setter_calls")); T->SetRelativeLocation(Body.RelativeLocation); if (!AppendCheck(C, R)) { return false; } Count(R, TEXT("public_setter_calls")); T->SetRelativeRotation(Body.RelativeRotation); if (!AppendCheck(C, R)) { return false; } Count(R, TEXT("public_setter_calls")); T->SetMobility(EComponentMobility::Movable); if (!AppendCheck(C, R)) { return false; }
            Count(R, TEXT("public_setter_calls")); T->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly); if (!AppendCheck(C, R)) { return false; } Count(R, TEXT("public_setter_calls")); T->SetCollisionObjectType(ECC_PhysicsBody); if (!AppendCheck(C, R)) { return false; } Count(R, TEXT("public_setter_calls")); T->SetCollisionResponseToAllChannels(ECR_Ignore); if (!AppendCheck(C, R)) { return false; } Count(R, TEXT("public_setter_calls")); T->SetGenerateOverlapEvents(false); if (!AppendCheck(C, R)) { return false; } Count(R, TEXT("public_setter_calls")); T->SetNotifyRigidBodyCollision(false); if (!AppendCheck(C, R)) { return false; }
            Count(R, TEXT("direct_field_write_calls")); T->CanCharacterStepUpOn = ECB_No; Count(R, TEXT("direct_field_write_calls")); T->BodyInstance.bAutoWeld = false; Count(R, TEXT("direct_field_write_calls")); T->BodyInstance.bStartAwake = true; Count(R, TEXT("direct_field_write_calls")); T->BodyInstance.bEnableGravity = Body.bSimulatePhysics; Count(R, TEXT("direct_field_write_calls")); T->BodyInstance.bSimulatePhysics = Body.bSimulatePhysics; Count(R, TEXT("direct_field_write_calls")); T->BodyInstance.LinearDamping = Settings.LinearDamping; Count(R, TEXT("direct_field_write_calls")); T->BodyInstance.AngularDamping = Settings.AngularDamping; Count(R, TEXT("public_setter_calls")); T->BodyInstance.SetMassOverride(Body.MassKg, true); if (!AppendCheck(C, R)) { return false; }
            Count(R, TEXT("add_child_calls")); C.PendingChild = C.AppendExisting ? Node : nullptr; C.Root->AddChildNode(Node); if (!AppendCheck(C, R)) { return false; } if (!C.Root->GetChildNodes().Contains(Node) || !SCS->GetAllNodes().Contains(Node)) { return Fail(R, TEXT("rigid_chain_body_attachment_failed"), TEXT("Actual public child-attachment prefix differs; no subsequent creation."), true); } Count(R, TEXT("attached_node_count")); if (C.AppendExisting) { C.AppendedChildren.Add(Node); C.PendingChild = nullptr; }
        }
        for (const FConstraintSpec& Spec : Constraints)
        {
            R.Stage = TEXT("create_constraint_component"); Count(R, TEXT("create_node_attempts")); USCS_Node* Node = SCS->CreateNode(UPhysicsConstraintComponent::StaticClass(), Spec.ComponentFName); TStrongObjectPtr<USCS_Node> ReturnedNode(Node); if (Loaded(Node)) { Count(R, TEXT("created_node_count")); } Prefix(C, R, Node); if (!AppendCheck(C, R)) { return false; }
            if (!FreshTemplate(C, Node, UPhysicsConstraintComponent::StaticClass(), Spec.ComponentFName)) { return Fail(R, TEXT("rigid_chain_constraint_factory_failed"), TEXT("Actual native factory prefix retained; no next constraint creation."), true); }
            UPhysicsConstraintComponent* T = Cast<UPhysicsConstraintComponent>(Node->ComponentTemplate); if (T->ConstraintActor1 || T->ConstraintActor2 || !T->OverrideComponent1.IsExplicitlyNull() || !T->OverrideComponent2.IsExplicitlyNull()) { return Fail(R, TEXT("rigid_chain_constraint_fresh_defaults_unsupported"), TEXT("Fresh native actor/override pointers must already be null; no EditInstanceOnly or unreflected pointer writes."), true); }
            R.Stage = TEXT("configure_constraint_component"); Count(R, TEXT("explicit_modify_calls")); T->Modify(); if (!AppendCheck(C, R)) { return false; } Count(R, TEXT("constraint_configuration_calls")); Count(R, TEXT("public_setter_calls")); T->SetMobility(EComponentMobility::Movable); if (!AppendCheck(C, R)) { return false; } Count(R, TEXT("public_setter_calls")); T->SetRelativeLocation(Bodies[Spec.ChildBodyIndex].RelativeLocation); if (!AppendCheck(C, R)) { return false; } Count(R, TEXT("public_setter_calls")); T->SetRelativeRotation(FRotator::ZeroRotator); if (!AppendCheck(C, R)) { return false; }
            Count(R, TEXT("direct_field_write_calls")); T->ComponentName1.ComponentName = Bodies[Spec.ParentBodyIndex].ComponentFName; Count(R, TEXT("direct_field_write_calls")); T->ComponentName2.ComponentName = Bodies[Spec.ChildBodyIndex].ComponentFName;
            FConstraintInstance& CI = T->ConstraintInstance; Count(R, TEXT("public_setter_calls")); CI.SetLinearLimits(LCM_Locked, LCM_Locked, LCM_Locked, 0); if (!AppendCheck(C, R)) { return false; } Count(R, TEXT("public_setter_calls")); CI.SetAngularSwing1Limit(ACM_Limited, Settings.Swing1); if (!AppendCheck(C, R)) { return false; } Count(R, TEXT("public_setter_calls")); CI.SetAngularSwing2Limit(ACM_Limited, Settings.Swing2); if (!AppendCheck(C, R)) { return false; } Count(R, TEXT("public_setter_calls")); CI.SetAngularTwistLimit(ACM_Limited, Settings.Twist); if (!AppendCheck(C, R)) { return false; } Count(R, TEXT("public_setter_calls")); CI.SetDisableCollision(true); if (!AppendCheck(C, R)) { return false; } Count(R, TEXT("public_setter_calls")); CI.SetProjectionParams(true, Settings.ProjectionLinearAlpha, Settings.ProjectionAngularAlpha, Settings.ProjectionLinearTolerance, Settings.ProjectionAngularTolerance); if (!AppendCheck(C, R)) { return false; } Count(R, TEXT("public_setter_calls")); CI.EnableMassConditioning(); if (!AppendCheck(C, R)) { return false; }
            Count(R, TEXT("add_child_calls")); C.PendingChild = C.AppendExisting ? Node : nullptr; C.Root->AddChildNode(Node); if (!AppendCheck(C, R)) { return false; } if (!C.Root->GetChildNodes().Contains(Node) || !SCS->GetAllNodes().Contains(Node)) { return Fail(R, TEXT("rigid_chain_constraint_attachment_failed"), TEXT("Actual constraint child-attachment prefix differs; no further creation."), true); } Count(R, TEXT("attached_node_count")); if (C.AppendExisting) { C.AppendedChildren.Add(Node); C.PendingChild = nullptr; }
        }
        R.Stage = TEXT("configuration_readback"); if (!Capture(C, Current, R, TEXT("after_configuration"))) { return Fail(R, TEXT("rigid_chain_configuration_unobservable"), TEXT("Raw created/attached prefix retained; configuration observation incomplete, no structural/compiler/save."), true); }
        if (!Preserve(Before, Current, C, R) || !Readback(C, Bodies, Constraints, Settings, false, R)) { return false; } R.Data->SetBoolField(TEXT("after_configuration_source_preserved"), true); R.Data->SetBoolField(TEXT("after_configuration_unselected_state_preserved"), true);
        ClearFinal(R); R.bKnown = false; R.Stage = TEXT("structural_refresh"); Count(R, TEXT("structural_refresh_calls")); FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(C.Blueprint.Get()); if (!BindAppendPhase(C, R)) { return false; } Count(R, TEXT("mark_package_dirty_calls")); C.Blueprint->MarkPackageDirty(); if (!AppendCheck(C, R)) { return false; }
        if (!Capture(C, Current, R, TEXT("after_structural_observation"))) { return Fail(R, TEXT("rigid_chain_structural_unobservable"), TEXT("Actual structural callback prefix retained; no full compiler/save."), true); } if (!Preserve(Before, Current, C, R) || !Readback(C, Bodies, Constraints, Settings, false, R)) { return false; }
        R.Data->SetBoolField(TEXT("after_structural_source_preserved"), true); R.Data->SetBoolField(TEXT("after_structural_unselected_state_preserved"), true); ClearFinal(R); R.bKnown = false; R.Stage = TEXT("compile_blueprint"); R.bCompileAttempted = true; Count(R, TEXT("full_compile_attempts")); auto Compiler = Cubeless::UE58::CompileBlueprint(C.Blueprint->GetPathName(), false, false); R.Data->SetObjectField(TEXT("compile_result"), Compiler); bool Compiled = false; if (Compiler->TryGetBoolField(TEXT("compiled"), Compiled)) { R.Compiled = Compiled; R.Data->SetBoolField(TEXT("compiled"), Compiled); }
        for (const TCHAR* Field : { TEXT("compile_error_count"), TEXT("compile_warning_count") }) { double Value = 0; if (Compiler->TryGetNumberField(Field, Value)) { R.Data->SetNumberField(Field, Value); } }
        if (C.AppendExisting)
        {
            bool Known = true, Unknown = false; Compiler->TryGetBoolField(TEXT("change_state_known"), Known); Compiler->TryGetBoolField(TEXT("outcome_unknown"), Unknown);
            if (!R.Compiled.IsSet() || !Known || Unknown) { C.Stopped = true; return Fail(R, TEXT("rigid_chain_compile_outcome_unobservable"), TEXT("Actual compiler receipt retained; unknown/missing compile outcome stops without further source SDK."), true); }
        }
        if (!BindAppendPhase(C, R)) { return false; }
        if (!Capture(C, Current, R, TEXT("after_observation"))) { return Fail(R, TEXT("rigid_chain_postcompile_unobservable"), TEXT("Compiler receipt and actual raw bounded prefix retained; no save/retry."), true); } if (!R.Compiled.IsSet()) { return Fail(R, TEXT("rigid_chain_compile_outcome_unobservable"), TEXT("Typed compiled status absent; no save/retry."), true); }
        if (!Preserve(Before, Current, C, R)) { return false; } if (!Compiled) { return Fail(R, TEXT("rigid_chain_compile_failed"), TEXT("Full compiler returned failure; retained actual state/diagnostics, save suppressed.")); } if (!Readback(C, Bodies, Constraints, Settings, true, R)) { return false; }
        if (bSave)
        {
            const FObservation BeforeSave = Current; R.Data->SetBoolField(TEXT("before_save_observations_complete"), true); R.Data->SetBoolField(TEXT("before_save_source_preserved"), true); R.Data->SetBoolField(TEXT("before_save_unselected_state_preserved"), true); R.Data->SetBoolField(TEXT("before_save_independent_readback_passed"), true); ClearFinal(R); R.bKnown = false; R.Stage = TEXT("save_blueprint"); R.bSaveAttempted = true; Count(R, TEXT("save_attempt_count")); auto Receipts = R.Data->GetArrayField(TEXT("save_calls")); auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("asset_path"), C.Blueprint->GetPathName()); Row->SetBoolField(TEXT("attempted"), true); Row->SetBoolField(TEXT("only_if_dirty"), false); Receipts.Add(MakeShared<FJsonValueObject>(Row)); R.Data->SetArrayField(TEXT("save_calls"), Receipts);
            R.bSaved = UEditorAssetLibrary::SaveLoadedAsset(C.Blueprint.Get(), false); Row->SetBoolField(TEXT("saved"), R.bSaved); R.Data->SetBoolField(TEXT("saved"), R.bSaved); if (C.AppendExisting && !R.bSaved) { C.Stopped = true; return Fail(R, TEXT("rigid_chain_save_failed"), TEXT("Actual save false retained; no additional UObject/source read or retry."), true); } if (!AppendCheck(C, R)) { return false; } FObservation Saved;
            if (!Capture(C, Saved, R, TEXT("after_save"))) { return Fail(R, TEXT("rigid_chain_after_save_unobservable"), TEXT("Exact save attempted once; raw saved return/prefix retained, no retry."), true); } Current = Saved; if (!R.bSaved) { return Fail(R, TEXT("rigid_chain_save_failed"), TEXT("Exact forced save returned false; disk outcome remains unknown, no retry."), true); }
            const bool State = SavedEqual(MakeShared<FJsonValueObject>(BeforeSave.State), MakeShared<FJsonValueObject>(Saved.State), C.Package), Source = Equal(BeforeSave.SourcesState, Saved.SourcesState) && Equal(BeforeSave.ReferencesState, Saved.ReferencesState), Other = OtherDirtyPreserved(BeforeSave.OtherDirty, Saved.OtherDirty, R, TEXT("save")), Persisted = Saved.DiskState->GetBoolField(TEXT("exists")) && Saved.DiskState->GetNumberField(TEXT("bytes")) > 0 && !C.Blueprint->GetOutermost()->IsDirty();
            R.bKnown = true; R.bChanged = true; R.Data->SetBoolField(TEXT("observations_complete"), true); R.Data->SetBoolField(TEXT("source_preserved"), Source); R.Data->SetBoolField(TEXT("unselected_state_preserved"), State && Other); R.Data->SetBoolField(TEXT("save_state_preserved"), State); R.Data->SetBoolField(TEXT("persistence_verified"), Persisted); if (!(State && Source && Other && Persisted)) { return Fail(R, TEXT("rigid_chain_save_postcondition_failed"), TEXT("Save returned success but exact owned save-flags/state/source/other-dirty/persistence differs; retained raw saved prefix, no retry.")); } if (!Readback(C, Bodies, Constraints, Settings, true, R)) { return false; }
        }
        R.bPost = true; R.bKnown = true; R.bChanged = true; R.Data->SetBoolField(TEXT("verification_pass"), true); R.Data->SetStringField(TEXT("verification_level"), TEXT("independent_readback")); return true;
    }();
    R.bSuccess = Success;
    if (C.AppendExisting && C.Stopped) { R.bKnown = false; R.Data->SetBoolField(TEXT("existing_root_current_guard_stopped"), true); R.Data->SetStringField(TEXT("terminal_observation_scope"), TEXT("Sticky current loss, including prewriter source observation: metadata-only terminal; original error/copied prefix retained.")); }
    if (Success) { R.Stage = TEXT("complete"); }
    if (!Success && R.bAttempted)
    {
        const FString FailedStage = R.Stage; R.Data->SetStringField(TEXT("failure_stage"), FailedStage); R.Data->SetBoolField(TEXT("partial_target_modified"), true); R.Data->SetBoolField(TEXT("do_not_retry"), true);
        if (!C.AppendExisting) { AllocatedFailurePrefix(C, R); FObservation Failure; const bool Complete = Capture(C, Failure, R, TEXT("failure_observation")); R.Data->SetBoolField(TEXT("failure_observations_complete"), Complete); R.Data->SetStringField(TEXT("failure_observation_scope"), TEXT("Additional bounded selected-state snapshot only; cannot turn an unknown allocation/callback/save outcome into known success or authorize retry.")); R.Stage = FailedStage; } else { R.Data->SetBoolField(TEXT("failure_observations_complete"), false); R.Data->SetStringField(TEXT("failure_observation_scope"), TEXT("Existing-root failure is metadata-only: retained actual stage/counters/copied rows; no additional UObject/field SDK, retry or rollback.")); C.Stopped = true; }
    }
    if ((!C.AppendExisting || (Success && !C.Stopped)) && Loaded(C.Blueprint.Get()) && C.Blueprint->SimpleConstructionScript) { R.Data->SetObjectField(TEXT("after"), CaptureChainState(C.Blueprint.Get(), C.RootName, Bodies, Constraints)); }
    if (C.AppendExisting && Success) { R.Data->SetBoolField(TEXT("existing_root_subtree_preserved"), true); R.Data->SetBoolField(TEXT("existing_root_append_readback_verified"), R.bPost); R.Data->SetBoolField(TEXT("runtime_verified"), false); }
    Delta(R, Bodies.Num() + Constraints.Num(), ExistingRoot); R.Data->SetBoolField(TEXT("save_requested"), R.bSaveRequested); R.Data->SetBoolField(TEXT("saved"), R.bSaved); R.Data->SetBoolField(TEXT("mutation_performed"), R.bKnown && R.bChanged); R.Data->SetBoolField(TEXT("mutation_state_unknown"), !R.bKnown); R.Data->SetBoolField(TEXT("readback_confirmed"), R.bPost); return Encode(R);
}
}
