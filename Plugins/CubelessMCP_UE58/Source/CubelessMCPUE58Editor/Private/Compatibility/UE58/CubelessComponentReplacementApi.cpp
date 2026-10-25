// ADN source-only: blueprint_tools.py414 -> replace_blueprint_component_class ->
// BlueprintCommands.cpp992/1731 and native template/common-copy helpers. Original
// native class resolution has no shape-only target restriction. Exact empty native
// StaticMeshComponent targets now share the existing own-SCS shape replacement flow.
// SDK StaticMeshComponent.h132-133 and MeshComponent.h29-31 are authored references;
// public reflection reads slots/counts only. No private access, mesh/material writer,
// new source/template constructor, renderer cache invariant or engine modification.
// Exact own native shape/StaticMesh sources; generated/custom/plain Scene sources, populated
// target defaults, broad typed references and runtime propagation remain incomplete.
// Editor-only finite port of UE56 replace_blueprint_component_class.
// Own native Box/Sphere/Capsule -> native shape targets, including a finite own SCS scene-root slice; no runtime helpers.
#include "Compatibility/UE58/CubelessComponentReplacementApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessGeneratedComponentApi.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Editor.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectGlobals.h"
#include "Components/ActorComponent.h"
#include "UObject/SparseDelegate.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/ComponentDelegateBinding.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "EdGraphSchema_K2.h"
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

namespace Cubeless::UE58::ComponentReplacement
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
void Init(FResult& R, bool Dry, bool Compile, bool Save, bool Refresh)
{
    R.Operation = TEXT("replace_blueprint_component_class"); R.bCompileRequested = !Dry && Compile; R.bSaveRequested = !Dry && Compile && Save;
    for (const TCHAR* K : { TEXT("modify_calls"), TEXT("rename_calls"), TEXT("template_allocation_calls"), TEXT("common_copy_calls"), TEXT("structural_refresh_calls"), TEXT("full_compile_attempts"), TEXT("save_attempt_count") }) { R.Data->SetNumberField(K, 0); }
    for (const TCHAR* K : { TEXT("observations_complete"), TEXT("source_preserved"), TEXT("unselected_state_preserved"), TEXT("independent_readback_passed"), TEXT("runtime_verified"), TEXT("mcp_dependency_written") }) { R.Data->SetBoolField(K, false); }
    R.Data->SetBoolField(TEXT("dry_run"), Dry); R.Data->SetBoolField(TEXT("compile_asset_requested"), Compile); R.Data->SetBoolField(TEXT("save_asset_requested"), Save); R.Data->SetBoolField(TEXT("refresh_nodes_requested"), Refresh);
    R.Data->SetArrayField(TEXT("save_calls"), {}); R.Data->SetNumberField(TEXT("asset_load_count"), 0);
    R.Data->SetStringField(TEXT("dependency_tier"), TEXT("EngineAPI: public native SCS/defaults, UObject Rename/NewObject, Blueprint editor compiler and exact EditorAssetLibrary save; no ServerRPC or runtime Cubeless class"));
    R.Data->SetStringField(TEXT("effect_scope"), TEXT("Exact loaded direct-native Actor Blueprint, own non-root leaf Box/Sphere to Sphere; selected template retirement to Transient and new template allocation are real effects. Setters/Rename/structural refresh/compile/save dispatch implicit Engine callbacks, caches and reinstance work that are not exhaustively instrumented."));
    R.Data->SetStringField(TEXT("read_scope"), TEXT("Bounded current native class/CDO sources, own SCS identities/topology, full raw templates and unrelated defaults/declarations/graphs, selected common values, target disk and other dirty packages. No world/runtime/cook or whole loaded-reference proof."));
    R.Data->SetStringField(TEXT("unsupported_original_scope"), TEXT("Root/children/inherited/custom or non-shape targets, selected typed graph references, loaded live/derived instances, archetype instances and extra template-slot collisions are rejected before Modify; arbitrary original classes remain incomplete."));
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
{ for (const TCHAR* P : { TEXT("/Script/Cubeless"), TEXT("/Script/UnrealMCP"), TEXT("/Engine/PythonTypes"), TEXT("/Game/UnrealMCP"), TEXT("/CubelessMCP_UE58/"), TEXT("/UnrealMCP/"), TEXT("/MCPUnreal/"), TEXT("/MCP_Unreal/") }) { if (S.Contains(P, ESearchCase::IgnoreCase)) { return false; } } return true; }
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
struct FContext;bool NativeOverlapNode(FContext*,const UEdGraphNode*);bool ShapeCurrent(FContext& C,FResult& R);bool ShapeStop(FContext& C,FResult& R,const TCHAR* Property,const FString& Expected,const FString& Actual);
bool Properties(UStruct* Type, const void* Container, UObject* Owner, const TSet<FName>& Skip, TSharedRef<FJsonObject>& J,FContext* Guard=nullptr,FResult* Reply=nullptr)
{
    TStrongObjectPtr<UStruct> HoldType(Guard?Type:nullptr);TStrongObjectPtr<UObject> HoldOwner(Guard?Owner:nullptr);if(Guard&&!ShapeCurrent(*Guard,*Reply))return false;
    if (!Type || !Container || !Loaded(Owner)) { return false; }TArray<FProperty*> CapturedFields;if(Guard){for(TFieldIterator<FProperty> Field(Type,EFieldIteratorFlags::IncludeSuper);Field;++Field)CapturedFields.Add(*Field);if(CapturedFields.Num()>MaxProperties)return false;if(!ShapeCurrent(*Guard,*Reply))return false;} int32 Count = 0, Chars = 0;
    for (TFieldIterator<FProperty> It(Type, EFieldIteratorFlags::IncludeSuper); It; ++It)
    {
        FProperty* P = *It; if (Skip.Contains(P->GetFName())) { continue; } if (++Count > MaxProperties || P->ArrayDim < 1 || P->ArrayDim > 64 || !P->GetOwnerStruct()) { return false; }
        auto Row = MakeShared<FJsonObject>(); J->SetObjectField(P->GetName(), Row); Row->SetStringField(TEXT("owner"), P->GetOwnerStruct()->GetPathName()); Row->SetStringField(TEXT("class"), P->GetClass()->GetName()); Row->SetStringField(TEXT("flags"), LexToString(static_cast<uint64>(P->GetPropertyFlags())));
        TArray<TSharedPtr<FJsonValue>> Values;
        for (int32 I = 0; I < P->ArrayDim; ++I) { FString S; const void* V = P->ContainerPtrToValuePtr<void>(Container, I); const bool Exported=P->ExportText_Direct(S,V,V,Owner,PPF_None);if(Guard){if(!ShapeCurrent(*Guard,*Reply))return false;if(Owner->HasAnyFlags(RF_ClassDefaultObject)){auto* OwnerClass=Owner->GetClass();if(!OwnerClass->ClassDefaultObject.IsResolved()||OwnerClass->ClassDefaultObject.Get()!=Owner)return ShapeStop(*Guard,*Reply,TEXT("read_owner_cdo_current"),TEXT("held current source/host CDO"),TEXT("CDO slot changed during reflected read"));}TArray<FProperty*> CurrentFields;for(TFieldIterator<FProperty> Field(Type,EFieldIteratorFlags::IncludeSuper);Field;++Field)CurrentFields.Add(*Field);if(CurrentFields!=CapturedFields)return ShapeStop(*Guard,*Reply,TEXT("reflected_field_membership"),TEXT("same held Type field addresses"),TEXT("different reflected layout membership"));}if (!Exported || (Chars += S.Len()) > MaxText || !CleanText(S)) { return false; } Values.Add(MakeShared<FJsonValueString>(S)); }
        Row->SetArrayField(TEXT("values"), Values);
    }
    return true;
}
bool Properties(UObject* O, const TSet<FName>& Skip, TSharedRef<FJsonObject>& J,FContext* Guard=nullptr,FResult* Reply=nullptr)
{ return Loaded(O) && Properties(O->GetClass(), O, O, Skip, J,Guard,Reply); }
bool Equal(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
{
    if (!A || !B || A->Type != B->Type) { return false; }
    if (A->Type == EJson::Object) { if (A->AsObject()->Values.Num() != B->AsObject()->Values.Num()) { return false; } for (const auto& P : A->AsObject()->Values) { const auto Q = B->AsObject()->TryGetField(P.Key.ToView()); if (!Equal(P.Value, Q)) { return false; } } return true; }
    if (A->Type == EJson::Array) { const auto& X = A->AsArray(); const auto& Y = B->AsArray(); if (X.Num() != Y.Num()) { return false; } for (int32 I = 0; I < X.Num(); ++I) { if (!Equal(X[I], Y[I])) { return false; } } return true; }
    if (A->Type == EJson::String) { return A->AsString().Equals(B->AsString(), ESearchCase::CaseSensitive); } if (A->Type == EJson::Number) { return A->AsNumber() == B->AsNumber(); } if (A->Type == EJson::Boolean) { return A->AsBool() == B->AsBool(); } return A->Type == EJson::Null;
}
bool Equal(const TSharedRef<FJsonObject>& A, const TSharedRef<FJsonObject>& B)
{ return Equal(MakeShared<FJsonValueObject>(A), MakeShared<FJsonValueObject>(B)); }
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

// A successful save can load thumbnail resources. Preserve every existing
// package's dirty bit; report additional clean packages separately from edits.
// No package removal, old dirty-bit transition, new dirty package or non-bool
// observation is accepted, and no authored/source baseline is replaced.
bool SavedOtherDirty(const TSharedRef<FJsonObject>& Before,const TSharedRef<FJsonObject>& After,FResult& R)
{
    R.Data->SetBoolField(TEXT("after_save_existing_package_dirty_preserved"),false);
    auto Mismatch=[&](const FString& Package,const FString& Expected,const FString& Actual)
    {
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("object"),Package);J->SetStringField(TEXT("property"),TEXT("after_save.other_package_dirty"));J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);R.Data->SetObjectField(TEXT("after_save_other_dirty_first_mismatch"),J);return false;
    };
    for(const auto& Pair:Before->Values)
    {
        const auto Now=After->TryGetField(Pair.Key.ToView());
        if(!Pair.Value.IsValid()||Pair.Value->Type!=EJson::Boolean||!Now.IsValid()||Now->Type!=EJson::Boolean||Pair.Value->AsBool()!=Now->AsBool())
            return Mismatch(FString(Pair.Key.ToView()),Pair.Value.IsValid()&&Pair.Value->Type==EJson::Boolean?(Pair.Value->AsBool()?TEXT("dirty=true"):TEXT("dirty=false")):TEXT("boolean required"),!Now.IsValid()?TEXT("missing"):Now->Type!=EJson::Boolean?TEXT("non-boolean"):Now->AsBool()?TEXT("dirty=true"):TEXT("dirty=false"));
    }
    TArray<TSharedPtr<FJsonValue>> AddedClean;
    for(const auto& Pair:After->Values)
    {
        if(Before->HasField(Pair.Key.ToView()))continue;
        if(!Pair.Value.IsValid()||Pair.Value->Type!=EJson::Boolean||Pair.Value->AsBool())
            return Mismatch(FString(Pair.Key.ToView()),TEXT("new package may only be clean"),Pair.Value.IsValid()&&Pair.Value->Type==EJson::Boolean?TEXT("dirty=true"):TEXT("non-boolean"));
        AddedClean.Add(MakeShared<FJsonValueString>(FString(Pair.Key.ToView())));
    }
    R.Data->SetArrayField(TEXT("after_save_new_clean_packages"),AddedClean);
    R.Data->SetBoolField(TEXT("after_save_existing_package_dirty_preserved"),true);return true;
}

FString ObjectPath(const UObject* O) { return O ? O->GetPathName() : FString(); }
bool Graphs(UBlueprint* B, TSharedRef<FJsonObject>& J,FContext* Guard=nullptr,FResult* Reply=nullptr)
{
    TArray<UEdGraph*> All; B->GetAllGraphs(All); if (All.Num() > MaxGraphs) { return false; }
    TSet<UEdGraph*> Seen; int32 Nodes = 0, Pins = 0; TArray<TSharedPtr<FJsonValue>> Rows; J->SetArrayField(TEXT("graphs"), Rows);
    for (UEdGraph* G : All)
    {
        if (!Loaded(G) || !CleanText(G->GetClass()->GetPathName()) || G->GetOutermost() != B->GetOutermost() || Seen.Contains(G) || !G->GraphGuid.IsValid()) { return false; } Seen.Add(G);
        auto Row = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("identity"), Identity(G)); Row->SetStringField(TEXT("guid"), G->GraphGuid.ToString()); Rows.Add(MakeShared<FJsonValueObject>(Row)); J->SetArrayField(TEXT("graphs"), Rows);
        const auto NodeCopies=Guard?G->Nodes:TArray<TObjectPtr<UEdGraphNode>>();const auto& PhysicalNodes=Guard?NodeCopies:G->Nodes;
        TSet<UEdGraphNode*> Owned; TSet<const UEdGraphPin*> OwnedPins;
        for (UEdGraphNode* N : PhysicalNodes)
        {
            if (!Loaded(N) || !CleanText(N->GetClass()->GetPathName()) || N->GetOuter() != G || Owned.Contains(N) || !N->NodeGuid.IsValid() || ++Nodes > 4096) { return false; } Owned.Add(N);
            const auto PinCopies=Guard?N->Pins:TArray<UEdGraphPin*>();const auto& PhysicalPins=Guard?PinCopies:N->Pins;
            for (const UEdGraphPin* P : PhysicalPins) { if (!P || P->GetOwningNodeUnchecked() != N || OwnedPins.Contains(P) || !P->PinId.IsValid() || ++Pins > MaxPins) { return false; } OwnedPins.Add(P); }
        }
        TArray<TSharedPtr<FJsonValue>> NodeRows;
        for (UEdGraphNode* N : PhysicalNodes)
        {
            auto NR = MakeShared<FJsonObject>(); NR->SetObjectField(TEXT("identity"), Identity(N)); NR->SetStringField(TEXT("guid"), N->NodeGuid.ToString()); NodeRows.Add(MakeShared<FJsonValueObject>(NR)); Row->SetArrayField(TEXT("nodes"), NodeRows);
            auto Props = MakeShared<FJsonObject>(); NR->SetObjectField(TEXT("properties"), Props); if (!Properties(N, {}, Props,Guard,Reply)) { return false; }
            TArray<TSharedPtr<FJsonValue>> PinRows;
            const auto PinCopies=Guard?N->Pins:TArray<UEdGraphPin*>();const auto& PhysicalPins=Guard?PinCopies:N->Pins;
            for (const UEdGraphPin* P : PhysicalPins)
            {
                auto PR = MakeShared<FJsonObject>(); PR->SetStringField(TEXT("id"), P->PinId.ToString()); PR->SetStringField(TEXT("name"), P->PinName.ToString()); PR->SetNumberField(TEXT("direction"), static_cast<uint8>(P->Direction));
                const FEdGraphPinType CopiedType=P->PinType;FString Type; FEdGraphPinType::StaticStruct()->ExportText(Type, Guard?&CopiedType:&P->PinType, nullptr, N, PPF_None, nullptr);if(Guard&&!ShapeCurrent(*Guard,*Reply))return false; if (Type.Len() > MaxText || !CleanText(Type) || !CleanText(P->DefaultValue) || !CleanText(P->AutogeneratedDefaultValue)) { return false; }
                const FText TextCopy=P->DefaultTextValue;const FString Display=Guard?TextCopy.ToString():P->DefaultTextValue.ToString();if(Guard&&!ShapeCurrent(*Guard,*Reply))return false;if (!CleanText(ObjectPath(P->DefaultObject)) || !CleanText(Display)) { return false; }
                PR->SetStringField(TEXT("type"), Type); PR->SetStringField(TEXT("default"), P->DefaultValue); PR->SetStringField(TEXT("autogenerated_default"), P->AutogeneratedDefaultValue); PR->SetStringField(TEXT("default_object"), ObjectPath(P->DefaultObject)); PR->SetStringField(TEXT("default_text"), Guard?Display:P->DefaultTextValue.ToString());
                TArray<TSharedPtr<FJsonValue>> Links;
                for (const UEdGraphPin* Peer : P->LinkedTo) { if (!Peer || !OwnedPins.Contains(Peer) || !Peer->LinkedTo.Contains(P) || Peer->Direction == P->Direction) { return false; } Links.Add(MakeShared<FJsonValueString>(Peer->PinId.ToString())); }
                PR->SetArrayField(TEXT("links"), Links); PinRows.Add(MakeShared<FJsonValueObject>(PR)); NR->SetArrayField(TEXT("pins"), PinRows);
            }
            if(NativeOverlapNode(Guard,N)){PinRows.Sort([](const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B){return A->AsObject()->GetStringField(TEXT("id"))<B->AsObject()->GetStringField(TEXT("id"));});NR->SetArrayField(TEXT("pins"),PinRows);}
        }
    }
    J->SetNumberField(TEXT("node_count"), Nodes); J->SetNumberField(TEXT("pin_count"), Pins); return true;
}
bool Sources(UClass* Parent, TSharedRef<FJsonObject>& J,UClass* Additional=nullptr,FContext* Guard=nullptr,FResult* Reply=nullptr)
{
    TArray<UClass*> Classes = { AActor::StaticClass(), UBlueprint::StaticClass(), USCS_Node::StaticClass(), USimpleConstructionScript::StaticClass(), UActorComponent::StaticClass(), USceneComponent::StaticClass(), UPrimitiveComponent::StaticClass(), UBoxComponent::StaticClass(), USphereComponent::StaticClass() };
    if (Parent && !Classes.Contains(Parent)) { Classes.Add(Parent); }
    if(Additional==UStaticMeshComponent::StaticClass()){Classes.AddUnique(UMeshComponent::StaticClass());Classes.AddUnique(UCapsuleComponent::StaticClass());}
    if(Additional&&!Classes.Contains(Additional))Classes.Add(Additional);
    for (UClass* Class : Classes)
    {
        auto Row = Identity(Class); J->SetObjectField(Class->GetPathName(), Row);
        if (!Native(Class)) { J->SetStringField(TEXT("failed_class"), Class->GetPathName()); return false; }
        Row->SetStringField(TEXT("class_flags"), LexToString(static_cast<uint32>(Class->GetClassFlags())));
        UObject* CDO = Class->GetDefaultObject(false); Row->SetBoolField(TEXT("existing_cdo_available"), CDO != nullptr);
        if (!CDO) { if (Class == USphereComponent::StaticClass()||Class==Additional) { J->SetStringField(TEXT("failed_class"), Class->GetPathName()); return false; } continue; }
        Row->SetObjectField(TEXT("existing_cdo"), Identity(CDO)); auto Props = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("existing_cdo_properties"), Props);
        if (!Loaded(CDO) || CDO->GetOuter() != Class->GetOuter() || CDO->GetClass() != Class || !CDO->HasAnyFlags(RF_ClassDefaultObject) || !Properties(CDO, {}, Props,Guard,Reply)) { J->SetStringField(TEXT("failed_class"), Class->GetPathName()); return false; }
    }
    return true;
}
namespace GeneratedReplacement { struct FScope; }
struct FContext
{
    TSharedPtr<GeneratedReplacement::FScope> NativeGet;bool NativeChecking=false;
    TStrongObjectPtr<UBlueprint> Blueprint;
    TStrongObjectPtr<UActorComponent> OldTemplate, NewTemplate;
    FString Package, SelectedName, Slot;
    USCS_Node* Node = nullptr;
    USCS_Node* Parent = nullptr;
    AActor* CDO = nullptr;
    FGuid Guid;
    UClass* TargetClass=USphereComponent::StaticClass();bool ShapeMode=false,ShapeBound=false,ShapeRetired=false,ShapeAssigned=false;
    FVector ExpectedBox=FVector::ZeroVector;float ExpectedCapsuleRadius=0,ExpectedCapsuleHalfHeight=0;
    // ADN: exact native mesh target starts with no authored mesh/material references.
    // Descriptor-only snapshots; render/physics/lighting caches are never frozen.
    TStrongObjectPtr<UObject> StaticMeshSourceCDO;
    struct FMeshField { FProperty* P=nullptr;FFieldClass* Kind=nullptr;UStruct* Owner=nullptr;uint64 Flags=0;int32 Offset=0,Size=0,Dim=0;
     explicit FMeshField(FProperty* In):P(In),Kind(In->GetClass()),Owner(In->GetOwnerStruct()),Flags(In->GetPropertyFlags()),Offset(In->GetOffset_ForInternal()),Size(In->GetSize()),Dim(In->ArrayDim){}
     bool Exact()const{return P->GetClass()==Kind&&P->GetOwnerStruct()==Owner&&P->GetPropertyFlags()==Flags&&P->GetOffset_ForInternal()==Offset&&P->GetSize()==Size&&P->ArrayDim==Dim&&!P->HasSetterOrGetter();}};
    TArray<FMeshField> StaticMeshFields;
    // AH: retain the original mesh source through Rename/compile. These are
    // authored reference slots and saved asset files, not renderer caches.
    struct FMeshSource
    {
        TStrongObjectPtr<UActorComponent> Template;
        TStrongObjectPtr<UClass> Class;
        TArray<FMeshField> Fields;
        TArray<TStrongObjectPtr<UObject>> References;
        struct FAsset { TStrongObjectPtr<UObject> Object; TStrongObjectPtr<UClass> Class; TStrongObjectPtr<UPackage> Package; FString Path; EObjectFlags Flags=RF_NoFlags; uint32 PackageFlags=0; };
        TArray<FAsset> Assets;
        TSharedRef<FJsonObject> Files=MakeShared<FJsonObject>();
    };
    TSharedPtr<FMeshSource> MeshSource;

    TStrongObjectPtr<UClass> ShapeGenerated,ShapeSkeleton,ShapeOldClass;TStrongObjectPtr<UObject> ShapeTargetCDO,ShapeHostCDO;TStrongObjectPtr<USimpleConstructionScript> ShapeSCS;
    struct FShapeNode{TStrongObjectPtr<USCS_Node> Node;TStrongObjectPtr<UActorComponent> Template;TStrongObjectPtr<UClass> Class;FName Name;FGuid Guid;TArray<USCS_Node*> Children;};
    TArray<FShapeNode> ShapeNodes;TArray<USCS_Node*> ShapeAll,ShapeRoots;
    bool ShapeChildrenMode=false;
    bool RootMode=false,RootReady=false,RootMayCreateDefault=false;
    TStrongObjectPtr<UEditorEngine> RootEditor;decltype(GUndo) RootUndo=nullptr;
    TStrongObjectPtr<UPackage> RootPackage;TStrongObjectPtr<UClass> RootParent,RootGenerated,RootSkeleton;
    TStrongObjectPtr<UObject> RootCDO;TStrongObjectPtr<USimpleConstructionScript> RootSCS;
    TArray<USCS_Node*> RootAll,RootRoots;FString RootBlueprintPath,RootGeneratedPath,RootSkeletonPath;
    struct FRootNode{TStrongObjectPtr<USCS_Node> Node;TStrongObjectPtr<UActorComponent> Template;TStrongObjectPtr<UClass> Class;FGuid Guid;FName Name,Socket,ParentName,ParentOwner;bool NativeParent=false;TArray<USCS_Node*> Children;};
    TArray<FRootNode> RootRows;TStrongObjectPtr<USCS_Node> RootDefault;TStrongObjectPtr<UActorComponent> RootDefaultTemplate;
    FGuid RootDefaultGuid;FName RootDefaultName;TArray<USCS_Node*> RootDefaultChildren;
    TStrongObjectPtr<USimpleConstructionScript> ChildrenSCS;
    TArray<FShapeNode> ChildrenBaseline;TArray<USCS_Node*> ChildrenAll,ChildrenRoots;
    struct FShapeGraphNode{TStrongObjectPtr<UEdGraphNode> Node;FGuid Guid;TArray<UEdGraphPin*> Pins;TArray<FGuid> PinIds;TArray<FEdGraphPinType> Types;};
    struct FShapeGraph{TStrongObjectPtr<UEdGraph> Graph;TArray<TObjectPtr<UEdGraphNode>> Nodes;TArray<FShapeGraphNode> Rows;};TArray<FShapeGraph> ShapeGraphs;TArray<TStrongObjectPtr<UObject>> ShapePinObjects;
};
void ClearFinal(FResult& R);
bool NativeGetCurrent(FContext&,FResult&);
bool NativeGetAllowed(const FContext&,const UEdGraphNode*);
bool NativeGetProject(FContext&,const TSharedPtr<FJsonValue>&,TSharedPtr<FJsonValue>&);
bool NativeGetBind(FContext&,FResult&);
bool NativeGetReturn(FContext&,FResult&,bool);
bool NativeGetSaved(FContext&,FResult&);
bool NativeGetStable(FContext&,FResult&);
// OD: additional native shape operations use the existing full observations,
// plus a small held identity/topology fence between actual mutation callbacks.
// Existing held shape fences are also used by the opt-in own-root slice below; no live/derived propagation or loaders.
bool ShapeStop(FContext& C,FResult& R,const TCHAR* Property,const FString& Expected,const FString& Actual)
{
    if(!R.Data->HasField(TEXT("native_scene_first_mismatch"))){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);J->SetBoolField(TEXT("expected_actual_observed"),false);J->SetStringField(TEXT("diagnostic_kind"),TEXT("compound_current_guard_or_copied_operands"));R.Data->SetObjectField(TEXT("native_scene_first_mismatch"),J);}
    R.Data->SetBoolField(TEXT("native_scene_followup_sdk_stopped"),true);ClearFinal(R);
    return Fail(R,TEXT("component_replace_native_scene_current_changed"),TEXT("Held native Scene target/SCS/template current guard changed. Retained actual prefix; no followup SDK, rollback or retry."),true);
}
// UL: concrete own scene-root role, using the existing native shape writer.
// DefaultSceneRoot is a separate SDK cache, never an excuse to accept authored drift.
bool RootObjectSlot(UObject* Object,UClass* Declaring,const TCHAR* Name,UClass* Type,UObject*& Out)
{
    Out=nullptr;if(!Loaded(Object)||!Loaded(Declaring)||!Loaded(Type))return false;
    FProperty* Raw=FindFProperty<FProperty>(Object->GetClass(),Name);
    if(!Raw||Raw->GetClass()!=FObjectProperty::StaticClass())return false;
    auto* P=static_cast<FObjectProperty*>(Raw);
    if(P->GetOwnerClass()!=Declaring||P->ArrayDim!=1||P->ElementSize!=sizeof(FObjectPtr)||P->HasSetterOrGetter()||!P->PropertyClass.IsResolved()||P->PropertyClass.Get()!=Type||P->GetOffset_ForInternal()<0||P->GetOffset_ForInternal()>Object->GetClass()->GetPropertiesSize()-P->ElementSize)return false;
    const auto* Slot=P->ContainerPtrToValuePtr<FObjectPtr>(Object);if(!Slot->IsResolved())return false;Out=Slot->Get();return true;
}
bool RootContext(FContext& C,FResult& R,UClass* Generated=nullptr)
{
    if(!C.RootMode)return true;auto* B=C.Blueprint.Get();auto* G=Generated?Generated:C.RootGenerated.Get();
    if(R.Data->HasField(TEXT("native_scene_followup_sdk_stopped")))return false;
    if(!IsEditorGameThread()||IsPlaySessionActive()||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||GEditor!=C.RootEditor.Get()||GUndo!=C.RootUndo||!Loaded(B)||!Loaded(G)||!Loaded(C.RootParent.Get())||!Loaded(C.RootPackage.Get())||B->GetClass()!=UBlueprint::StaticClass()||B->GetPathName()!=C.RootBlueprintPath||B->GetOutermost()!=C.RootPackage.Get()||!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.Get()!=C.RootParent.Get()||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||!B->SimpleConstructionScript.IsResolved()||B->SimpleConstructionScript.Get()!=C.RootSCS.Get()||!Loaded(C.RootSCS.Get())||C.RootSCS->GetClass()!=USimpleConstructionScript::StaticClass()||C.RootSCS->GetOuter()!=G)
        return ShapeStop(C,R,TEXT("own_root_context"),TEXT("same held idle BP/parent/package/Editor/SCS ownership"),TEXT("different role or context"));
    const auto All=C.RootSCS->GetAllNodes();if(All!=C.RootAll||C.RootSCS->GetRootNodes()!=C.RootRoots||!C.RootRoots.Contains(C.Node))return ShapeStop(C,R,TEXT("own_root_membership"),TEXT("same own root and ordered AllNodes/Roots"),TEXT("different authored membership"));
    for(const auto& E:C.RootRows)
    {
        auto* N=E.Node.Get();if(!Loaded(N)||N->GetClass()!=USCS_Node::StaticClass()||N->GetOuter()!=C.RootSCS.Get()||N->VariableGuid!=E.Guid||!N->GetVariableName().ToString().Equals(E.Name.ToString(),ESearchCase::CaseSensitive)||N->GetChildNodes()!=E.Children||N->AttachToName!=E.Socket||N->ParentComponentOrVariableName!=E.ParentName||N->ParentComponentOwnerClassName!=E.ParentOwner||N->bIsParentComponentNative!=E.NativeParent)
            return ShapeStop(C,R,TEXT("own_root_authored_topology"),E.Name.ToString(),TEXT("different node/GUID/name/parent/socket/children"));
        UClass* ExpectedClass=N==C.Node&&C.ShapeAssigned?C.TargetClass:E.Class.Get();UActorComponent* Expected=N==C.Node&&C.ShapeAssigned?C.NewTemplate.Get():E.Template.Get();
        if(!N->ComponentClass.IsResolved()||!N->ComponentTemplate.IsResolved()||N->ComponentClass.Get()!=ExpectedClass||N->ComponentTemplate.Get()!=Expected||!Loaded(Expected)||Expected->GetClass()!=ExpectedClass||Expected->GetOuter()!=(N==C.Node&&!C.ShapeAssigned&&C.ShapeRetired?static_cast<UObject*>(GetTransientPackage()):static_cast<UObject*>(G)))return ShapeStop(C,R,TEXT("own_root_template_binding"),E.Name.ToString(),TEXT("different actual class/template/outer"));
    }
    return true;
}
bool RootDefaultCurrent(FContext& C,FResult& R,UClass* Generated=nullptr,bool AdoptCreation=false)
{
    if(!C.RootMode||!C.RootReady)return true;if(!RootContext(C,R,Generated))return false;
    auto* G=Generated?Generated:C.RootGenerated.Get();UObject* Object=nullptr;
    if(!RootObjectSlot(C.RootSCS.Get(),USimpleConstructionScript::StaticClass(),TEXT("DefaultSceneRootNode"),USCS_Node::StaticClass(),Object))return ShapeStop(C,R,TEXT("default_root_slot"),TEXT("resolved exact native default-root slot"),TEXT("unresolved or descriptor mismatch"));
    TStrongObjectPtr<UObject> Returned(Object);
    if(Object!=C.RootDefault.Get())
    {
        if(!AdoptCreation||C.RootDefault.IsValid()||!C.RootMayCreateDefault||!Loaded(Object)||Object->GetClass()!=USCS_Node::StaticClass())return ShapeStop(C,R,TEXT("default_root_identity"),C.RootDefault.IsValid()?TEXT("same held default root"):TEXT("null except planned validator creation"),TEXT("different current cache object"));
        auto* N=static_cast<USCS_Node*>(Object);
        if(N->GetOuter()!=C.RootSCS.Get()||C.RootAll.Contains(N)||!N->ComponentClass.IsResolved()||N->ComponentClass.Get()!=USceneComponent::StaticClass()||!N->ComponentTemplate.IsResolved()||!N->GetChildNodes().IsEmpty()||N->GetVariableName()!=USceneComponent::GetDefaultSceneRootVariableName()||!N->VariableGuid.IsValid())return ShapeStop(C,R,TEXT("default_root_creation"),TEXT("new empty native default Scene cache outside authored membership"),TEXT("unplanned default-root shape"));
        TStrongObjectPtr<UActorComponent> Template(N->ComponentTemplate.Get());
        if(!Loaded(Template.Get())||Template->GetClass()!=USceneComponent::StaticClass()||Template->GetOuter()!=G)return ShapeStop(C,R,TEXT("default_root_template_creation"),TEXT("exact own Scene template"),TEXT("different template role"));
        C.RootDefault.Reset(N);C.RootDefaultTemplate.Reset(Template.Get());C.RootDefaultGuid=N->VariableGuid;C.RootDefaultName=N->GetVariableName();C.RootDefaultChildren=N->GetChildNodes();
        R.Data->SetBoolField(TEXT("own_root_validator_created_default_cache"),true);
    }
    if(C.RootDefault.IsValid())
    {
        auto* N=C.RootDefault.Get();
        if(!Loaded(N)||N->GetClass()!=USCS_Node::StaticClass()||N->GetOuter()!=C.RootSCS.Get()||N->VariableGuid!=C.RootDefaultGuid||N->GetVariableName()!=C.RootDefaultName||N->GetChildNodes()!=C.RootDefaultChildren||!N->ComponentClass.IsResolved()||N->ComponentClass.Get()!=USceneComponent::StaticClass()||!N->ComponentTemplate.IsResolved()||N->ComponentTemplate.Get()!=C.RootDefaultTemplate.Get()||!Loaded(C.RootDefaultTemplate.Get())||C.RootDefaultTemplate->GetClass()!=USceneComponent::StaticClass()||C.RootDefaultTemplate->GetOuter()!=G||C.RootAll.Contains(N))return ShapeStop(C,R,TEXT("default_root_cache_current"),TEXT("same separate native default cache/template/GUID/children"),TEXT("different cache metadata or authored membership"));
    }
    return true;
}
bool RootCurrent(FContext& C,FResult& R)
{
    if(!C.RootMode||!C.RootReady)return true;if(!RootDefaultCurrent(C,R))return false;
    auto* B=C.Blueprint.Get();
    if(!Loaded(C.RootGenerated.Get())||!Loaded(C.RootSkeleton.Get())||!B->GeneratedClass.GetGCPtr().IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||B->GeneratedClass.Get()!=C.RootGenerated.Get()||B->SkeletonGeneratedClass.Get()!=C.RootSkeleton.Get()||!Loaded(C.RootCDO.Get())||!C.RootGenerated->ClassDefaultObject.IsResolved()||C.RootGenerated->ClassDefaultObject.Get()!=C.RootCDO.Get())return ShapeStop(C,R,TEXT("own_root_class_cdo"),TEXT("same current generated/skeleton/CDO roles outside compiler return"),TEXT("different derived role"));
    UObject* RootValue=nullptr;
    if(!RootObjectSlot(C.RootCDO.Get(),AActor::StaticClass(),TEXT("RootComponent"),USceneComponent::StaticClass(),RootValue)||RootValue!=nullptr)return ShapeStop(C,R,TEXT("own_root_cdo_root_component"),TEXT("captured null CDO RootComponent; authored scene root is in SCS"),TEXT("different or unresolved CDO root slot"));
    return true;
}
bool RootBegin(FContext& C,FResult& R)
{
    if(!C.RootMode)return true;auto* B=C.Blueprint.Get();
    C.RootEditor.Reset(GEditor);C.RootUndo=GUndo;C.RootPackage.Reset(B->GetOutermost());C.RootParent.Reset(B->ParentClass.Get());C.RootSCS.Reset(B->SimpleConstructionScript.Get());C.RootGenerated.Reset(B->GeneratedClass.Get());C.RootSkeleton.Reset(B->SkeletonGeneratedClass.Get());C.RootCDO.Reset(C.CDO);
    C.RootBlueprintPath=B->GetPathName();C.RootGeneratedPath=C.RootGenerated->GetPathName();C.RootSkeletonPath=C.RootSkeleton->GetPathName();C.RootAll=C.RootSCS->GetAllNodes();C.RootRoots=C.RootSCS->GetRootNodes();
    if(C.RootSCS->GetClass()!=USimpleConstructionScript::StaticClass()||C.RootRoots.Num()!=1||C.RootRoots[0]!=C.Node||C.Node->GetClass()!=USCS_Node::StaticClass()||!C.Node->ParentComponentOrVariableName.IsNone()||!C.Node->ParentComponentOwnerClassName.IsNone()||C.Node->bIsParentComponentNative)return Fail(R,TEXT("component_replace_own_root_scope_pending"),TEXT("One own parentless native shape scene root required; inherited/native-attached/multiple/default-selected roots remain incomplete."));
    UObject* RootValue=nullptr;
    if(!RootObjectSlot(C.RootCDO.Get(),AActor::StaticClass(),TEXT("RootComponent"),USceneComponent::StaticClass(),RootValue)||RootValue)return Fail(R,TEXT("component_replace_own_root_cdo_pending"),TEXT("First own-SCS root slice preserves an actual null generated Actor CDO RootComponent; existing CDO/native-root binding variants remain incomplete."));
    for(auto* N:C.RootAll)
    {
        if(!Loaded(N)||N->GetClass()!=USCS_Node::StaticClass()||!N->ComponentClass.IsResolved()||!N->ComponentTemplate.IsResolved())return Fail(R,TEXT("component_replace_own_root_node_pending"),TEXT("Exact loaded native own SCS nodes/resolved templates required."));
        FContext::FRootNode E;E.Node.Reset(N);E.Template.Reset(N->ComponentTemplate.Get());E.Class.Reset(N->ComponentClass.Get());E.Guid=N->VariableGuid;E.Name=N->GetVariableName();E.Children=N->GetChildNodes();E.Socket=N->AttachToName;E.ParentName=N->ParentComponentOrVariableName;E.ParentOwner=N->ParentComponentOwnerClassName;E.NativeParent=N->bIsParentComponentNative;C.RootRows.Add(MoveTemp(E));
    }
    UObject* Default=nullptr;
    if(!RootObjectSlot(C.RootSCS.Get(),USimpleConstructionScript::StaticClass(),TEXT("DefaultSceneRootNode"),USCS_Node::StaticClass(),Default))return Fail(R,TEXT("component_replace_default_root_slot_pending"),TEXT("Native resolved nullable default root descriptor required; no implicit resolve."));
    TStrongObjectPtr<UObject> Hold(Default);
    C.RootMayCreateDefault=Default==nullptr;
    if(Default)
    {
        if(!Loaded(Default)||Default->GetClass()!=USCS_Node::StaticClass())return Fail(R,TEXT("component_replace_default_root_pending"),TEXT("Actual default cache must be exact native SCS node."));
        auto* N=static_cast<USCS_Node*>(Default);if(!N->ComponentTemplate.IsResolved())return Fail(R,TEXT("component_replace_default_root_pending"),TEXT("Unresolved default cache template."));
        C.RootDefault.Reset(N);C.RootDefaultTemplate.Reset(N->ComponentTemplate.Get());C.RootDefaultGuid=N->VariableGuid;C.RootDefaultName=N->GetVariableName();C.RootDefaultChildren=N->GetChildNodes();
    }
    C.RootReady=true;if(!RootCurrent(C,R))return false;
    // This public source lookup is an observer; return/context is closed before using it.
    USCS_Node* ActualNode=nullptr;USceneComponent* Actual=C.RootSCS->GetSceneRootComponentTemplate(false,&ActualNode);TStrongObjectPtr<USceneComponent> ActualHold(Actual);TStrongObjectPtr<USCS_Node> NodeHold(ActualNode);
    if(!RootCurrent(C,R))return false;
    if(ActualNode!=C.Node||Actual!=C.OldTemplate.Get())return Fail(R,TEXT("component_replace_actual_scene_root_pending"),TEXT("Selected own SCS root must be the actual public scene-root template, without a native/inherited root alternative."));
    R.Data->SetBoolField(TEXT("own_scs_scene_root_replacement"),true);R.Data->SetBoolField(TEXT("own_root_validator_created_default_cache"),false);R.Data->SetStringField(TEXT("own_scs_scene_root_guid"),C.Guid.ToString());
    R.Data->SetStringField(TEXT("own_scs_scene_root_scope"),TEXT("Same own parentless native Box/Sphere/Capsule SCS root and ordered descendants/socket/fields. Separate SDK default cache; actual null CDO RootComponent retained. Compiler class/CDO roles are reacquired, authored values remain original. DefaultSceneRoot-selected/plain Scene/inherited/native-bound roots remain valid incomplete."));
    return true;
}
bool RootValidatorReturned(FContext& C,FResult& R)
{
    if(!C.RootMode)return ShapeCurrent(C,R);
    // Only this actual ValidateSceneRootNodes return may fill a previously-null cache.
    if(!RootDefaultCurrent(C,R,nullptr,true))return false;C.RootMayCreateDefault=false;
    return ShapeCurrent(C,R);
}
bool RootCompilerReturned(FContext& C,FResult& R)
{
    if(!C.RootMode)return true;auto* B=C.Blueprint.Get();
    if(!IsEditorGameThread()||IsPlaySessionActive()||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||GEditor!=C.RootEditor.Get()||GUndo!=C.RootUndo||!Loaded(B)||!B->GeneratedClass.GetGCPtr().IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved())return ShapeStop(C,R,TEXT("root_compiler_class_slots"),TEXT("resolved returned classes"),TEXT("unavailable slots"));
    TStrongObjectPtr<UClass> G(B->GeneratedClass.Get()),S(B->SkeletonGeneratedClass.Get());
    if(!Loaded(G.Get())||!Loaded(S.Get())||G->GetClass()!=UBlueprintGeneratedClass::StaticClass()||S->GetClass()!=UBlueprintGeneratedClass::StaticClass()||!G->ClassGeneratedBy.IsResolved()||!S->ClassGeneratedBy.IsResolved()||G->ClassGeneratedBy.Get()!=B||S->ClassGeneratedBy.Get()!=B||G->GetPathName()!=C.RootGeneratedPath||S->GetPathName()!=C.RootSkeletonPath||G->GetSuperClass()!=C.RootParent.Get()||S->GetSuperClass()!=C.RootParent.Get()||!G->ClassDefaultObject.IsResolved())return ShapeStop(C,R,TEXT("root_compiler_returned_class_roles"),TEXT("same logical own generated/skeleton/native-parent classes"),TEXT("different returned role"));
    TStrongObjectPtr<UObject> D(G->ClassDefaultObject.Get());
    if(!Loaded(D.Get())||D->GetClass()!=G.Get()||D->GetOuter()!=G->GetOuter()||!D->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject))return ShapeStop(C,R,TEXT("root_compiler_cdo"),TEXT("actual held generated Actor CDO"),TEXT("different returned CDO"));
    if(!RootDefaultCurrent(C,R,G.Get()))return false;
    UObject* RootValue=nullptr;if(!RootObjectSlot(D.Get(),AActor::StaticClass(),TEXT("RootComponent"),USceneComponent::StaticClass(),RootValue)||RootValue)return ShapeStop(C,R,TEXT("root_compiler_cdo_root"),TEXT("null CDO RootComponent as original own-SCS baseline"),TEXT("different returned value"));
    // Adopt only the returned derived roles. Original authored RootRows/observations stay fixed.
    C.RootGenerated.Reset(G.Get());C.RootSkeleton.Reset(S.Get());C.RootCDO.Reset(D.Get());C.CDO=static_cast<AActor*>(D.Get());
    R.Data->SetBoolField(TEXT("own_root_returned_class_cdo_roles_reacquired"),true);
    return RootCurrent(C,R);
}

bool ShapeChildrenCurrent(FContext& C,FResult& R)
{
    if(!C.ShapeChildrenMode||C.ChildrenBaseline.IsEmpty())return true;
    auto* B=C.Blueprint.Get();auto* S=C.ChildrenSCS.Get();
    if(!Loaded(B)||!B->SimpleConstructionScript.IsResolved()||B->SimpleConstructionScript.Get()!=S||!Loaded(S)||S->GetAllNodes()!=C.ChildrenAll||S->GetRootNodes()!=C.ChildrenRoots)return ShapeStop(C,R,TEXT("original_child_scs_membership"),TEXT("same original SCS/AllNodes/roots"),TEXT("different original SCS role"));
    for(const auto& E:C.ChildrenBaseline)
    {
        auto* N=E.Node.Get();if(!Loaded(N)||N->GetOuter()!=S||N->VariableGuid!=E.Guid||N->GetVariableName()!=E.Name||N->GetChildNodes()!=E.Children)return ShapeStop(C,R,TEXT("original_child_node_topology"),E.Name.ToString(),TEXT("different original node/GUID/name/children"));
        if(N!=C.Node&&(!N->ComponentClass.IsResolved()||!N->ComponentTemplate.IsResolved()||N->ComponentClass.Get()!=E.Class.Get()||N->ComponentTemplate.Get()!=E.Template.Get()||!Loaded(E.Template.Get())||E.Template->GetClass()!=E.Class.Get()||E.Template->GetOuter()!=B->GeneratedClass.Get()))return ShapeStop(C,R,TEXT("original_child_template_binding"),E.Name.ToString(),TEXT("different original nonselected class/template/owner"));
    }
    return true;
}
void KeepChildrenBaseline(FContext& C,FResult& R)
{
    if(!C.ShapeChildrenMode||!C.ChildrenBaseline.IsEmpty())return;
    C.ChildrenSCS.Reset(C.ShapeSCS.Get());C.ChildrenAll=C.ShapeAll;C.ChildrenRoots=C.ShapeRoots;
    for(const auto& Source:C.ShapeNodes){FContext::FShapeNode E;E.Node.Reset(Source.Node.Get());E.Template.Reset(Source.Template.Get());E.Class.Reset(Source.Class.Get());E.Name=Source.Name;E.Guid=Source.Guid;E.Children=Source.Children;C.ChildrenBaseline.Add(MoveTemp(E));}
    TArray<USCS_Node*> Queue=C.Node->GetChildNodes();for(int32 I=0;I<Queue.Num();++I)Queue.Append(Queue[I]->GetChildNodes());
    TArray<TSharedPtr<FJsonValue>> Children;for(auto* N:C.Node->GetChildNodes())Children.Add(MakeShared<FJsonValueString>(N->VariableGuid.ToString(EGuidFormats::Digits)));
    R.Data->SetBoolField(TEXT("native_scene_child_subtree_replacement"),true);R.Data->SetBoolField(TEXT("native_scene_child_subtree_preserved"),false);R.Data->SetNumberField(TEXT("selected_scs_child_count"),C.Node->GetChildNodes().Num());R.Data->SetNumberField(TEXT("selected_scs_descendant_count"),Queue.Num());R.Data->SetArrayField(TEXT("selected_scs_child_guids"),Children);
    R.Data->SetStringField(TEXT("native_scene_child_subtree_scope"),TEXT("Same original own native SCS nodes/templates/GUIDs/classes/children/order through replacement and explicit compile; original logical reflected values fenced separately. Actual selected template attachments must remain empty. Graph physical pins may be reacquired after explicit reconstruction; original logical Before remains authoritative."));
}
// ADN: public reflection observes the existing private StaticMesh UPROPERTY and
// inherited material-array storage. Non-null/custom defaults remain incomplete;
// no SetStaticMesh/SetMaterial/load is synthesized by the original common-copy path.
bool StaticMeshTargetCurrent(FContext& C,FResult& R,bool Bind=false)
{
 if(C.TargetClass!=UStaticMeshComponent::StaticClass())return true;
 auto Bad=[&](const TCHAR* Field){return ShapeStop(C,R,Field,TEXT("same held native CDO/descriptors, null StaticMesh and empty OverrideMaterials"),TEXT("current role/layout/reference differs"));};
 if(!Loaded(C.TargetClass)||!C.TargetClass->ClassDefaultObject.IsResolved()||!Loaded(C.ShapeTargetCDO.Get())||C.ShapeTargetCDO->GetClass()!=C.TargetClass||!C.ShapeTargetCDO->HasAnyFlags(RF_ClassDefaultObject)||C.ShapeTargetCDO->GetOuter()!=C.TargetClass->GetOuter())return Bad(TEXT("static_mesh_native_cdo"));
 if(Bind&&C.StaticMeshFields.IsEmpty())C.StaticMeshSourceCDO.Reset(C.ShapeTargetCDO.Get());
 if(!Loaded(C.StaticMeshSourceCDO.Get())||C.ShapeTargetCDO.Get()!=C.StaticMeshSourceCDO.Get()||C.TargetClass->ClassDefaultObject.GetHandle()!=TObjectPtr<UObject>(C.StaticMeshSourceCDO.Get()).GetHandle())return Bad(TEXT("static_mesh_source_cdo_identity"));
 auto* Mesh=FindFProperty<FObjectProperty>(C.TargetClass,TEXT("StaticMesh"));auto* Materials=FindFProperty<FArrayProperty>(C.TargetClass,TEXT("OverrideMaterials"));auto* Inner=Materials?CastField<FObjectProperty>(Materials->Inner):nullptr;
 if(!Mesh||!Materials||!Inner||Mesh->GetOwnerStruct()!=UStaticMeshComponent::StaticClass()||Materials->GetOwnerStruct()!=UMeshComponent::StaticClass()||Inner->GetOwner<FProperty>()!=Materials||Mesh->ArrayDim!=1||Materials->ArrayDim!=1||Inner->ArrayDim!=1||Mesh->GetSize()!=sizeof(TObjectPtr<UObject>)||Materials->GetSize()!=sizeof(TArray<TObjectPtr<UObject>>)||Inner->GetSize()!=sizeof(TObjectPtr<UObject>)||Mesh->HasSetterOrGetter()||Materials->HasSetterOrGetter()||Inner->HasSetterOrGetter()||!Mesh->PropertyClass.IsResolved()||Mesh->PropertyClass.GetHandle()!=TObjectPtr<UClass>(UStaticMesh::StaticClass()).GetHandle()||!Inner->PropertyClass.IsResolved()||Inner->PropertyClass.GetHandle()!=TObjectPtr<UClass>(UMaterialInterface::StaticClass()).GetHandle())return Bad(TEXT("static_mesh_field_layout"));
 if(Bind&&C.StaticMeshFields.IsEmpty()){C.StaticMeshFields.Emplace(Mesh);C.StaticMeshFields.Emplace(Materials);C.StaticMeshFields.Emplace(Inner);}
 if(C.StaticMeshFields.Num()!=3||C.StaticMeshFields[0].P!=Mesh||C.StaticMeshFields[1].P!=Materials||C.StaticMeshFields[2].P!=Inner)return Bad(TEXT("static_mesh_field_membership"));
 for(const auto& F:C.StaticMeshFields)if(!F.Exact())return Bad(TEXT("static_mesh_captured_layout"));
 for(UObject* O:{C.StaticMeshSourceCDO.Get(),static_cast<UObject*>(C.NewTemplate.Get())})if(O)
 {
  if(!Loaded(O)||O->GetClass()!=C.TargetClass)return Bad(TEXT("static_mesh_target_class"));
  const auto* Slot=Mesh->GetObjectPtrPropertyValuePtr(Mesh->ContainerPtrToValuePtr<void>(O));FScriptArrayHelper H(Materials,Materials->ContainerPtrToValuePtr<void>(O));
  if(!Slot->IsResolved()||Slot->GetHandle()!=TObjectPtr<UObject>().GetHandle()||H.Num()!=0)return Bad(TEXT("static_mesh_authored_reference"));
 }
 return true;
}
// Existing own native mesh source only. Public reflected slots do not invoke
// GetStaticMesh tracking, lazy asset resolution, material setters or render work.
// File hashing stays at whole observation boundaries, never inside per-field
// ShapeCurrent calls or each common-default setter.
bool StaticMeshSourceCurrent(FContext& C,FResult& R,bool Bind=false)
{
    if (!C.MeshSource.IsValid() && (!C.OldTemplate.IsValid() || C.OldTemplate->GetClass()!=UStaticMeshComponent::StaticClass())) return true;
    auto Bad=[&](const TCHAR* Field,const FString& Expected,const FString& Actual)
    {
        R.Data->SetBoolField(TEXT("static_mesh_source_references_preserved"),false);R.Data->SetBoolField(TEXT("static_mesh_source_files_preserved"),false);
        if(R.bAttempted)return ShapeStop(C,R,Field,Expected,Actual);
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("property"),Field);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);R.Data->SetObjectField(TEXT("static_mesh_source_first_mismatch"),J);
        return Fail(R,TEXT("component_replace_mesh_source_unsupported"),TEXT("Loaded bounded runtime mesh/material source references and saved clean source packages required; no target writer."));
    };
    UClass* K=UStaticMeshComponent::StaticClass();
    if(!Native(K)||!Loaded(C.OldTemplate.Get())||C.OldTemplate->GetClass()!=K)return Bad(TEXT("mesh_source_class"),TEXT("held exact native StaticMeshComponent"),TEXT("different source role"));
    auto* Mesh=FindFProperty<FObjectProperty>(K,TEXT("StaticMesh"));auto* Materials=FindFProperty<FArrayProperty>(K,TEXT("OverrideMaterials"));auto* Inner=Materials?CastField<FObjectProperty>(Materials->Inner):nullptr;
    if(!Mesh||!Materials||!Inner||Mesh->GetOwnerStruct()!=K||Materials->GetOwnerStruct()!=UMeshComponent::StaticClass()||Inner->GetOwner<FProperty>()!=Materials||Mesh->ArrayDim!=1||Materials->ArrayDim!=1||Inner->ArrayDim!=1||Mesh->GetSize()!=sizeof(TObjectPtr<UObject>)||Materials->GetSize()!=sizeof(TArray<TObjectPtr<UObject>>)||Inner->GetSize()!=sizeof(TObjectPtr<UObject>)||Mesh->HasSetterOrGetter()||Materials->HasSetterOrGetter()||Inner->HasSetterOrGetter()||!Mesh->PropertyClass.IsResolved()||Mesh->PropertyClass.Get()!=UStaticMesh::StaticClass()||!Inner->PropertyClass.IsResolved()||Inner->PropertyClass.Get()!=UMaterialInterface::StaticClass())return Bad(TEXT("mesh_source_fields"),TEXT("current exact authored mesh/material descriptors"),TEXT("different owner/layout/type"));
    const bool New=Bind&&!C.MeshSource.IsValid();
    if(New){C.MeshSource=MakeShared<FContext::FMeshSource>();C.MeshSource->Template.Reset(C.OldTemplate.Get());C.MeshSource->Class.Reset(K);C.MeshSource->Fields.Emplace(Mesh);C.MeshSource->Fields.Emplace(Materials);C.MeshSource->Fields.Emplace(Inner);}
    if(!C.MeshSource.IsValid())return Bad(TEXT("mesh_source_baseline"),TEXT("pre-writer held source"),TEXT("missing baseline"));
    auto& S=*C.MeshSource;
    if(S.Template.Get()!=C.OldTemplate.Get()||S.Class.Get()!=K||S.Fields.Num()!=3||S.Fields[0].P!=Mesh||S.Fields[1].P!=Materials||S.Fields[2].P!=Inner)return Bad(TEXT("mesh_source_membership"),TEXT("same original template and field addresses"),TEXT("different source/field membership"));
    for(const auto& F:S.Fields)if(!F.Exact())return Bad(TEXT("mesh_source_layout"),TEXT("same captured native descriptors"),TEXT("different field metadata"));
    const auto* MeshSlot=Mesh->GetObjectPtrPropertyValuePtr(Mesh->ContainerPtrToValuePtr<void>(S.Template.Get()));FScriptArrayHelper H(Materials,Materials->ContainerPtrToValuePtr<void>(S.Template.Get()));
    if(!MeshSlot->IsResolved()||H.Num()>256)return Bad(TEXT("mesh_source_reference_storage"),TEXT("already resolved mesh and at most256 material slots"),TEXT("unresolved or over bound"));
    TArray<UObject*> Values;Values.Add(MeshSlot->Get());
    for(int32 I=0;I<H.Num();++I){const auto* Slot=Inner->GetObjectPtrPropertyValuePtr(H.GetRawPtr(I));if(!Slot->IsResolved())return Bad(TEXT("mesh_source_material_slot"),TEXT("resolved original material slot"),LexToString(I));Values.Add(Slot->Get());}
    if(New)
    {
        for(int32 I=0;I<Values.Num();++I)
        {
            UObject* O=Values[I];S.References.Emplace(O);if(!O)continue;
            const FString ClassPath=Loaded(O)?O->GetClass()->GetPathName():FString();
            if(!Loaded(O)||!Native(O->GetClass())||(I==0?ClassPath!=TEXT("/Script/Engine.StaticMesh"):(ClassPath!=TEXT("/Script/Engine.Material")&&ClassPath!=TEXT("/Script/Engine.MaterialInstanceConstant")))||O->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject)||!AssetPath(O->GetPathName())||O->GetOuter()!=O->GetOutermost()||O->GetOutermost()->GetName()==C.Package||O->GetOutermost()->IsDirty()||O->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn|PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly|PKG_PlayInEditor))return Bad(TEXT("mesh_source_asset"),TEXT("loaded saved-clean top-level runtime StaticMesh/Material/MIC asset"),Loaded(O)?O->GetPathName():TEXT("unavailable"));
            bool Seen=false;for(const auto& A:S.Assets)Seen|=A.Object.Get()==O;if(Seen)continue;
            FContext::FMeshSource::FAsset A;A.Object.Reset(O);A.Class.Reset(O->GetClass());A.Package.Reset(O->GetOutermost());A.Path=O->GetPathName();A.Flags=O->GetFlags();A.PackageFlags=A.Package->GetPackageFlags();S.Assets.Add(MoveTemp(A));
        }
    }
    if(S.References.Num()!=Values.Num())return Bad(TEXT("mesh_source_reference_count"),LexToString(S.References.Num()),LexToString(Values.Num()));
    for(int32 I=0;I<Values.Num();++I)if(S.References[I].Get()!=Values[I])return Bad(*(TEXT("mesh_source_reference_slot[")+LexToString(I)+TEXT("]")),ObjectPath(S.References[I].Get()),ObjectPath(Values[I]));
    for(const auto& A:S.Assets)if(!Loaded(A.Object.Get())||!Loaded(A.Class.Get())||!Native(A.Class.Get())||!Loaded(A.Package.Get())||A.Object->GetClass()!=A.Class.Get()||A.Object->GetOuter()!=A.Package.Get()||A.Object->GetPackage()!=A.Package.Get()||A.Object->GetPathName()!=A.Path||A.Object->GetFlags()!=A.Flags||A.Package->GetPackageFlags()!=A.PackageFlags||A.Package->IsDirty())return Bad(TEXT("mesh_source_asset_identity"),A.Path,TEXT("different source identity/class/package/flags/dirty state"));
    if(New)
    {
        TArray<FString> Packages;
        for(const auto& A:S.Assets)if(!Packages.Contains(A.Package->GetName())){Packages.Add(A.Package->GetName());auto File=MakeShared<FJsonObject>();if(!Disk(A.Package->GetName(),File)||!File->GetBoolField(TEXT("exists")))return Bad(TEXT("mesh_source_saved_file"),A.Package->GetName(),TEXT("missing or outside existing file bound"));S.Files->SetObjectField(A.Package->GetName(),File);}
        if(!Packages.IsEmpty()){auto Audit=ReadPackageDependencies(Packages,true,4096);R.Data->SetObjectField(TEXT("static_mesh_source_dependency_audit"),Audit);bool Passed=false;if(!Audit->TryGetBoolField(TEXT("passed"),Passed)||!Passed)return Bad(TEXT("mesh_source_dependencies"),TEXT("saved runtime assets without MCP/editor dependencies"),TEXT("dependency audit refused"));}
        TArray<TSharedPtr<FJsonValue>> Refs;for(UObject* O:Values)Refs.Add(MakeShared<FJsonValueString>(ObjectPath(O)));
        R.Data->SetArrayField(TEXT("static_mesh_source_original_references"),Refs);R.Data->SetObjectField(TEXT("static_mesh_source_saved_files"),S.Files);
        R.Data->SetStringField(TEXT("static_mesh_source_scope"),TEXT("Exact own inactive native StaticMeshComponent; original authored mesh/material slots including order/nulls, held loaded runtime source identities and saved-clean files. No source loader/setter/renderer/physics work or asset cache invariant. Broader cold/custom/dirty source profiles and whole in-memory asset preservation remain incomplete."));
        return StaticMeshSourceCurrent(C,R);
    }
    R.Data->SetBoolField(TEXT("static_mesh_source_references_preserved"),true);return true;
}
bool StaticMeshSourceFiles(FContext& C,FResult& R)
{
    if(!C.MeshSource.IsValid())return true;
    if(!StaticMeshSourceCurrent(C,R))return false;
    for(const auto& Item:C.MeshSource->Files->Values){auto Now=MakeShared<FJsonObject>();const bool OK=Disk(FString(Item.Key.ToView()),Now);if(!StaticMeshSourceCurrent(C,R))return false;if(!OK||!Equal(Item.Value,MakeShared<FJsonValueObject>(Now))){R.Data->SetBoolField(TEXT("static_mesh_source_files_preserved"),false);return ShapeStop(C,R,TEXT("mesh_source_saved_file"),FString(Item.Key.ToView()),TEXT("source file changed since original pre-writer observation"));}}
    R.Data->SetBoolField(TEXT("static_mesh_source_files_preserved"),true);return true;
}

bool ShapeCurrent(FContext& C,FResult& R)
{
    if(!C.ShapeMode||!C.ShapeBound)return true;if(R.Data->HasField(TEXT("native_scene_followup_sdk_stopped")))return false;
    auto* B=C.Blueprint.Get();if(!IsEditorGameThread()||IsPlaySessionActive()||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||!Loaded(B)||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad)return ShapeStop(C,R,TEXT("editor_target_current"),TEXT("held idle Editor Blueprint"),TEXT("retired/busy/context mismatch"));
    if(!B->GeneratedClass.GetGCPtr().IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||!B->SimpleConstructionScript.IsResolved()||B->GeneratedClass.GetGCPtr()!=C.ShapeGenerated.Get()||B->SkeletonGeneratedClass.GetGCPtr()!=C.ShapeSkeleton.Get()||B->SimpleConstructionScript.Get()!=C.ShapeSCS.Get()||!Loaded(C.ShapeGenerated.Get())||!Loaded(C.ShapeSkeleton.Get())||!Loaded(C.ShapeSCS.Get()))return ShapeStop(C,R,TEXT("target_class_scs_roles"),TEXT("same held generated/skeleton/SCS"),TEXT("different current handles"));
    if(!Loaded(C.ShapeHostCDO.Get())||C.CDO!=C.ShapeHostCDO.Get()||C.ShapeHostCDO->GetClass()!=C.ShapeGenerated.Get()||!C.ShapeGenerated->ClassDefaultObject.IsResolved()||C.ShapeGenerated->ClassDefaultObject.Get()!=C.ShapeHostCDO.Get())return ShapeStop(C,R,TEXT("host_cdo_current"),TEXT("held actual current generated Actor CDO"),TEXT("different host CDO slot/class"));
    if(!Loaded(C.TargetClass)||!Native(C.TargetClass)||!C.TargetClass->ClassDefaultObject.IsResolved()||C.TargetClass->ClassDefaultObject.Get()!=C.ShapeTargetCDO.Get()||!Loaded(C.ShapeTargetCDO.Get()))return ShapeStop(C,R,TEXT("native_target_cdo"),TEXT("same loaded native target and existing CDO"),TEXT("different source role"));
    const auto All=C.ShapeSCS->GetAllNodes();if(All!=C.ShapeAll||C.ShapeSCS->GetRootNodes()!=C.ShapeRoots)return ShapeStop(C,R,TEXT("scs_membership_roots"),TEXT("same captured own nodes/roots"),TEXT("different pointer inventory"));
    for(const auto& E:C.ShapeNodes)
    {
        auto* N=E.Node.Get();if(!Loaded(N)||N->GetOuter()!=C.ShapeSCS.Get()||N->VariableGuid!=E.Guid||N->GetVariableName()!=E.Name||N->GetChildNodes()!=E.Children)return ShapeStop(C,R,TEXT("scs_node_topology_identity"),E.Name.ToString(),TEXT("different owner/GUID/name/children"));
        UClass* Class=N==C.Node&&C.ShapeAssigned?C.TargetClass:E.Class.Get();UActorComponent* Template=N==C.Node&&C.ShapeAssigned?C.NewTemplate.Get():E.Template.Get();
        if(!N->ComponentClass.IsResolved()||!N->ComponentTemplate.IsResolved()||N->ComponentClass.Get()!=Class||N->ComponentTemplate.Get()!=Template||!Loaded(Template)||!Native(Class)||Template->GetClass()!=Class||Template->GetOuter()!=(N==C.Node&&!C.ShapeAssigned&&C.ShapeRetired?static_cast<UObject*>(GetTransientPackage()):static_cast<UObject*>(C.ShapeGenerated.Get())))return ShapeStop(C,R,TEXT("scs_class_template_binding"),E.Name.ToString(),TEXT("different class/template binding"));
    }
    if(!Loaded(C.OldTemplate.Get())||C.OldTemplate->GetClass()!=C.ShapeOldClass.Get()||C.OldTemplate->GetOuter()!=(C.ShapeRetired?static_cast<UObject*>(GetTransientPackage()):static_cast<UObject*>(C.ShapeGenerated.Get())))return ShapeStop(C,R,TEXT("retired_old_template"),C.ShapeRetired?TEXT("held original template in Transient"):TEXT("held original generated-class template"),TEXT("different old role"));
    if(C.ShapeChildrenMode||C.RootMode)
    {
        auto* OldScene=Cast<USceneComponent>(C.OldTemplate.Get());
        if(!OldScene||OldScene->IsRegistered()||OldScene->IsPreRegistering()||OldScene->GetOwner()||OldScene->GetWorld()||OldScene->GetAttachParent()||!OldScene->GetAttachChildren().IsEmpty())return ShapeStop(C,R,TEXT("old_template_actual_attachment"),TEXT("held unregistered owner/world-free template without actual attachment"),TEXT("different old physical Scene role"));
        if(!ShapeChildrenCurrent(C,R))return false;
    }
    if(C.NewTemplate.IsValid()){auto* P=Cast<UPrimitiveComponent>(C.NewTemplate.Get());if(!Loaded(P)||P->GetClass()!=C.TargetClass||P->GetOuter()!=C.ShapeGenerated.Get()||P->GetName()!=C.Slot||P->IsRegistered()||P->IsPreRegistering()||P->GetOwner()||P->GetWorld()||P->GetAttachParent()||!P->GetAttachChildren().IsEmpty())return ShapeStop(C,R,TEXT("new_template_role"),C.Slot,TEXT("different new class/owner/registration/attachment role"));}
    if(!C.NativeGet)for(const auto& G:C.ShapeGraphs){if(!Loaded(G.Graph.Get())||G.Graph->GetOuter()!=B||G.Graph->Nodes!=G.Nodes)return ShapeStop(C,R,TEXT("graph_node_membership"),TEXT("same held graph/node inventory"),TEXT("different graph nodes"));for(const auto& E:G.Rows){auto* N=E.Node.Get();if(!Loaded(N)||N->GetOuter()!=G.Graph.Get()||N->NodeGuid!=E.Guid||N->Pins!=E.Pins)return ShapeStop(C,R,TEXT("graph_node_pin_membership"),TEXT("same held node/GUID/physical pins"),TEXT("different node/pin roles"));for(int32 I=0;I<E.Pins.Num();++I){const auto* P=E.Pins[I];const auto& Type=E.Types[I];if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||P->PinId!=E.PinIds[I]||!(P->PinType==Type)||P->PinType.bIsUObjectWrapper!=Type.bIsUObjectWrapper||P->PinType.bSerializeAsSinglePrecisionFloat!=Type.bSerializeAsSinglePrecisionFloat||P->PinType.PinValueType.bTerminalIsUObjectWrapper!=Type.PinValueType.bTerminalIsUObjectWrapper)return ShapeStop(C,R,TEXT("graph_physical_pin_role"),E.PinIds[I].ToString(),TEXT("different current pin role"));}}}
    if(!RootCurrent(C,R))return false;
    if(!NativeGetCurrent(C,R)||!StaticMeshSourceCurrent(C,R))return false;return StaticMeshTargetCurrent(C,R);
}
bool ShapeBind(FContext& C,FResult& R,bool Assigned=false)
{
    if(!C.ShapeMode)return true;auto* B=C.Blueprint.Get();if(!Loaded(B)||!Loaded(C.OldTemplate.Get())||!Loaded(C.TargetClass)||!C.TargetClass->ClassDefaultObject.IsResolved())return Fail(R,TEXT("component_replace_native_scene_source_unavailable"),TEXT("Loaded native source and already-resolved existing target CDO required; no CDO creation or prewarm."));
    if(C.TargetClass==UStaticMeshComponent::StaticClass()){
     const auto Slot=C.TargetClass->ClassDefaultObject;UObject* Existing=StaticFindObject(nullptr,nullptr,*Slot.GetPathName(),false);TStrongObjectPtr<UObject> Hold(Loaded(Existing)?Existing:nullptr);
     if(!Hold.IsValid()||Slot.GetHandle()!=TObjectPtr<UObject>(Hold.Get()).GetHandle()||C.TargetClass->ClassDefaultObject.GetHandle()!=Slot.GetHandle()||Hold->GetClass()!=C.TargetClass||!Hold->HasAnyFlags(RF_ClassDefaultObject))return Fail(R,TEXT("component_replace_static_mesh_cdo_unavailable"),TEXT("Existing resolved exact native StaticMeshComponent CDO required; no tracking getter or source creation."));C.ShapeTargetCDO.Reset(Hold.Get());
     if(!StaticMeshTargetCurrent(C,R,true))return false;
    }else C.ShapeTargetCDO.Reset(C.TargetClass->ClassDefaultObject.Get());if(!Loaded(C.ShapeTargetCDO.Get()))return Fail(R,TEXT("component_replace_native_scene_source_unavailable"),TEXT("Existing target CDO is absent; no implicit source initialization attempted."));
    C.ShapeGenerated.Reset(B->GeneratedClass.Get());C.ShapeSkeleton.Reset(B->SkeletonGeneratedClass.Get());C.ShapeHostCDO.Reset(C.CDO);if(!Loaded(C.ShapeGenerated.Get())||!C.ShapeGenerated->ClassDefaultObject.IsResolved()||C.ShapeGenerated->ClassDefaultObject.Get()!=C.ShapeHostCDO.Get()||!Loaded(C.ShapeHostCDO.Get())||C.ShapeHostCDO->GetClass()!=C.ShapeGenerated.Get())return Fail(R,TEXT("component_replace_native_scene_host_cdo_unavailable"),TEXT("Actual current generated Actor CDO is required; no CDO creation or stale cached CDO observation."));C.ShapeSCS.Reset(B->SimpleConstructionScript.Get());C.ShapeOldClass.Reset(C.OldTemplate->GetClass());C.ShapeAll=C.ShapeSCS->GetAllNodes();C.ShapeRoots=C.ShapeSCS->GetRootNodes();C.ShapeNodes.Reset();
    for(auto* N:C.ShapeAll){if(!Loaded(N)||!N->ComponentClass.IsResolved()||!N->ComponentTemplate.IsResolved())return Fail(R,TEXT("component_replace_native_scene_source_unavailable"),TEXT("Current local SCS roles required before the new shape path."));FContext::FShapeNode E;E.Node.Reset(N);E.Template.Reset(N->ComponentTemplate.Get());E.Class.Reset(N->ComponentClass.Get());E.Name=N->GetVariableName();E.Guid=N->VariableGuid;E.Children=N->GetChildNodes();C.ShapeNodes.Add(MoveTemp(E));}
    if(!ShapeChildrenCurrent(C,R))return false;
    C.ShapeGraphs.Reset();C.ShapePinObjects.Reset();TArray<UEdGraph*> Graphs;B->GetAllGraphs(Graphs);if(Graphs.Num()>MaxGraphs)return Fail(R,TEXT("component_replace_native_scene_observation_bound"),TEXT("Current graph observation exceeds the existing bound."));int32 PinCount=0;
    for(auto* G:Graphs){if(!Loaded(G)||G->GetOuter()!=B)return Fail(R,TEXT("component_replace_native_scene_graph_current"),TEXT("Current own graph required."));FContext::FShapeGraph Role;Role.Graph.Reset(G);Role.Nodes=G->Nodes;for(UEdGraphNode* N:Role.Nodes){if(!Loaded(N)||N->GetOuter()!=G||N->Pins.Num()+PinCount>MaxPins)return Fail(R,TEXT("component_replace_native_scene_graph_current"),TEXT("Current bounded node/pin inventory required."));FContext::FShapeGraphNode E;E.Node.Reset(N);E.Guid=N->NodeGuid;E.Pins=N->Pins;for(auto* P:E.Pins){if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N)return Fail(R,TEXT("component_replace_native_scene_graph_current"),TEXT("Current node-owned physical pin required."));E.PinIds.Add(P->PinId);E.Types.Add(P->PinType);++PinCount;}Role.Rows.Add(MoveTemp(E));}C.ShapeGraphs.Add(MoveTemp(Role));}
    C.ShapeAssigned=Assigned;C.ShapeRetired=Assigned;C.ShapeBound=true;if(!StaticMeshSourceCurrent(C,R,true)||!ShapeCurrent(C,R))return false;
    for(const auto& G:C.ShapeGraphs)for(const auto& E:G.Rows)for(auto* P:E.Pins){C.ShapePinObjects.Emplace(P->DefaultObject.Get());if(!ShapeCurrent(C,R))return false;C.ShapePinObjects.Emplace(P->PinType.PinSubCategoryObject.Get());}
    return ShapeCurrent(C,R);
}
void CopyShapeContract(const FContext& Old,FContext& Fresh)
{
 Fresh.NativeGet=Old.NativeGet;
 // RootMode is classified again by Prepare; never replace a changed classification with a new baseline.
 if(Old.RootMode&&Fresh.RootMode){
 Fresh.RootReady=Old.RootReady;Fresh.RootMayCreateDefault=Old.RootMayCreateDefault;Fresh.RootEditor.Reset(Old.RootEditor.Get());Fresh.RootUndo=Old.RootUndo;Fresh.RootPackage.Reset(Old.RootPackage.Get());Fresh.RootParent.Reset(Old.RootParent.Get());Fresh.RootGenerated.Reset(Old.RootGenerated.Get());Fresh.RootSkeleton.Reset(Old.RootSkeleton.Get());Fresh.RootCDO.Reset(Old.RootCDO.Get());Fresh.RootSCS.Reset(Old.RootSCS.Get());Fresh.RootAll=Old.RootAll;Fresh.RootRoots=Old.RootRoots;Fresh.RootBlueprintPath=Old.RootBlueprintPath;Fresh.RootGeneratedPath=Old.RootGeneratedPath;Fresh.RootSkeletonPath=Old.RootSkeletonPath;
 Fresh.RootDefault.Reset(Old.RootDefault.Get());Fresh.RootDefaultTemplate.Reset(Old.RootDefaultTemplate.Get());Fresh.RootDefaultGuid=Old.RootDefaultGuid;Fresh.RootDefaultName=Old.RootDefaultName;Fresh.RootDefaultChildren=Old.RootDefaultChildren;
 for(const auto& Source:Old.RootRows){FContext::FRootNode E;E.Node.Reset(Source.Node.Get());E.Template.Reset(Source.Template.Get());E.Class.Reset(Source.Class.Get());E.Guid=Source.Guid;E.Name=Source.Name;E.Socket=Source.Socket;E.ParentName=Source.ParentName;E.ParentOwner=Source.ParentOwner;E.NativeParent=Source.NativeParent;E.Children=Source.Children;Fresh.RootRows.Add(MoveTemp(E));}
 }
 Fresh.TargetClass=Old.TargetClass;Fresh.ShapeMode=Old.ShapeMode;Fresh.ExpectedBox=Old.ExpectedBox;Fresh.ExpectedCapsuleRadius=Old.ExpectedCapsuleRadius;Fresh.ExpectedCapsuleHalfHeight=Old.ExpectedCapsuleHalfHeight;
 Fresh.StaticMeshSourceCDO.Reset(Old.StaticMeshSourceCDO.Get());Fresh.StaticMeshFields=Old.StaticMeshFields;Fresh.MeshSource=Old.MeshSource;
 Fresh.ShapeChildrenMode=Old.ShapeChildrenMode;Fresh.ChildrenSCS.Reset(Old.ChildrenSCS.Get());Fresh.ChildrenAll=Old.ChildrenAll;Fresh.ChildrenRoots=Old.ChildrenRoots;
 for(const auto& Source:Old.ChildrenBaseline){FContext::FShapeNode E;E.Node.Reset(Source.Node.Get());E.Template.Reset(Source.Template.Get());E.Class.Reset(Source.Class.Get());E.Name=Source.Name;E.Guid=Source.Guid;E.Children=Source.Children;Fresh.ChildrenBaseline.Add(MoveTemp(E));}
}
bool ShapeDefaults(const FContext& C,const UPrimitiveComponent* P)
{
    if(C.TargetClass==UBoxComponent::StaticClass()){const auto* B=Cast<UBoxComponent>(P);return B&&B->GetUnscaledBoxExtent()==C.ExpectedBox;}
    if(C.TargetClass==UCapsuleComponent::StaticClass()){const auto* B=Cast<UCapsuleComponent>(P);return B&&B->GetUnscaledCapsuleRadius()==C.ExpectedCapsuleRadius&&B->GetUnscaledCapsuleHalfHeight()==C.ExpectedCapsuleHalfHeight;}
    return true;
}
bool CopyAdditionalShapeDefaults(FContext& C,FResult& R,UPrimitiveComponent* Old,UPrimitiveComponent* New,float Radius)
{
    if(!ShapeCurrent(C,R))return false;
    TArray<TSharedPtr<FJsonValue>> Calls;
    auto Call=[&](const TCHAR* Stage,auto&& Action)->bool{auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),Stage);J->SetBoolField(TEXT("attempted"),true);J->SetBoolField(TEXT("returned"),false);Calls.Add(MakeShared<FJsonValueObject>(J));R.Data->SetArrayField(TEXT("native_scene_default_calls"),Calls);Action();J->SetBoolField(TEXT("returned"),true);return ShapeCurrent(C,R);};
    New->ComponentTags=Old->ComponentTags;
    if(!Call(TEXT("relative_location"),[&](){New->SetRelativeLocation(Old->GetRelativeLocation());}))return false;
    if(!Call(TEXT("relative_rotation"),[&](){New->SetRelativeRotation(Old->GetRelativeRotation());}))return false;
    if(!Call(TEXT("relative_scale"),[&](){New->SetRelativeScale3D(Old->GetRelativeScale3D());}))return false;
    if(!Call(TEXT("mobility"),[&](){New->SetMobility(Old->Mobility);}))return false;
    if(!Call(TEXT("visibility"),[&](){New->SetVisibility(Old->IsVisible(),false);}))return false;
    if(!Call(TEXT("hidden_in_game"),[&](){New->SetHiddenInGame(Old->bHiddenInGame,false);}))return false;
    if(!Call(TEXT("collision_profile"),[&](){New->SetCollisionProfileName(Old->GetCollisionProfileName(),false);}))return false;
    if(!Call(TEXT("collision_enabled"),[&](){New->SetCollisionEnabled(Old->GetCollisionEnabled());}))return false;
    if(!Call(TEXT("overlap_events"),[&](){New->SetGenerateOverlapEvents(Old->GetGenerateOverlapEvents());}))return false;
    New->CanCharacterStepUpOn=Old->CanCharacterStepUpOn;
    if(auto* B=Cast<UBoxComponent>(New);B&&Cast<UBoxComponent>(Old)){if(!Call(TEXT("box_extent"),[&](){B->SetBoxExtent(C.ExpectedBox,false);}))return false;}
    if(auto* S=Cast<USphereComponent>(New)){if(!Call(TEXT("sphere_radius"),[&](){S->SetSphereRadius(Radius,false);}))return false;}
    // Original CopyCommon has no Capsule size branch: its new native CDO
    // dimensions remain intact; do not invent a radius/half-height transfer.
    return ShapeCurrent(C,R);
}

bool Prepare(const FString& RequestedPath, const FString& ComponentName, FContext& C, FResult& R,UClass* ReturnedTarget=nullptr)
{
    if (!IsEditorGameThread() || IsPlaySessionActive()) { return Fail(R, TEXT("component_replace_editor_context_required"), TEXT("Editor game thread without PIE/SIE required.")); }
    const FString P = Path(RequestedPath); C.Package = FPackageName::ObjectPathToPackageName(P); C.SelectedName = ComponentName;
    R.Data->SetStringField(TEXT("asset_path"), P); R.Data->SetStringField(TEXT("component_name"), ComponentName);
    if (!AssetPath(P) || !C.Package.StartsWith(TEXT("/Game/")) || !Name(ComponentName)) { return Fail(R, TEXT("component_replace_inputs_invalid"), TEXT("Exact Game Blueprint object path and bounded local component name required.")); }
    C.Blueprint.Reset(FindObject<UBlueprint>(nullptr, *P)); UBlueprint* B = C.Blueprint.Get();
    if (!Loaded(B) || B->GetClass() != UBlueprint::StaticClass() || B->BlueprintType != BPTYPE_Normal || B->GetOuter() != B->GetOutermost() || B->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject)
        || B->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn | PKG_EditorOnly | PKG_PlayInEditor) || !Native(B->ParentClass) || !B->ParentClass->IsChildOf(AActor::StaticClass())
        || !Loaded(B->GeneratedClass) || !Loaded(B->SkeletonGeneratedClass) || B->GeneratedClass->ClassGeneratedBy != B || B->SkeletonGeneratedClass->ClassGeneratedBy != B || B->GeneratedClass->GetSuperClass() != B->ParentClass
        || B->GeneratedClass->GetOutermost() != B->GetOutermost() || B->SkeletonGeneratedClass->GetOutermost() != B->GetOutermost() || B->bBeingCompiled || B->bQueuedForCompilation || B->bSuppressStructurallyModified || B->bIsRegeneratingOnLoad || B->Status == BS_Error || B->Status == BS_BeingCreated)
    { return Fail(R, TEXT("component_replace_blueprint_unsupported"), TEXT("Loaded current normal direct-native Engine Actor Blueprint required; no Blueprint loading/CDO prewarming.")); }
    C.CDO = Cast<AActor>(B->GeneratedClass->GetDefaultObject(false));
    if (!Loaded(C.CDO) || C.CDO->GetClass() != B->GeneratedClass || C.CDO->GetOuter() != B->GeneratedClass->GetOuter() || !C.CDO->HasAnyFlags(RF_ClassDefaultObject)) { return Fail(R, TEXT("component_replace_cdo_unsupported"), TEXT("Existing exact generated Actor CDO required.")); }
    int32 Scanned = 0;
    for (TObjectIterator<AActor> It; It; ++It) { if (++Scanned > 32768) { return Fail(R, TEXT("component_replace_instance_scan_bound"), TEXT("Loaded Actor scan exceeds finite bound.")); } if (Loaded(*It) && !It->IsTemplate() && It->IsA(B->GeneratedClass)) { return Fail(R, TEXT("component_replace_live_instance_unsupported"), TEXT("Loaded live selected Actor instance is outside finite template replacement propagation proof.")); } }
    Scanned = 0;
    for (TObjectIterator<UBlueprintGeneratedClass> It; It; ++It) { if (++Scanned > 32768) { return Fail(R, TEXT("component_replace_instance_scan_bound"), TEXT("Loaded generated-class scan exceeds finite bound.")); } if (Loaded(*It) && *It != B->GeneratedClass && *It != B->SkeletonGeneratedClass && It->IsChildOf(B->GeneratedClass)) { return Fail(R, TEXT("component_replace_derived_class_unsupported"), TEXT("Loaded derived Blueprint classes are outside the finite compile propagation scope.")); } }
    USimpleConstructionScript* SCS = B->SimpleConstructionScript.Get();
    if (!Loaded(SCS) || SCS->GetOuter() != B->GeneratedClass || SCS->GetAllNodes().Num() > MaxNodes) { return Fail(R, TEXT("component_replace_scs_unsupported"), TEXT("Bounded own local SCS required.")); }
    TSet<USCS_Node*> Nodes; TSet<UActorComponent*> Templates; TSet<FGuid> Guids; TSet<FName> Names;
    for (USCS_Node* Node : SCS->GetAllNodes())
    {
        UActorComponent* Template = Loaded(Node) ? Node->ComponentTemplate.Get() : nullptr;
        if (!Loaded(Node) || Node->GetOuter() != SCS || Nodes.Contains(Node) || !Node->VariableGuid.IsValid() || Guids.Contains(Node->VariableGuid) || Node->GetVariableName().IsNone() || Names.Contains(Node->GetVariableName())
            || !Native(Node->ComponentClass) || !Loaded(Template) || Templates.Contains(Template) || Template->GetClass() != Node->ComponentClass || Template->GetOuter() != B->GeneratedClass || !Template->HasAnyFlags(RF_ArchetypeObject))
        { return Fail(R, TEXT("component_replace_scs_ownership_unsupported"), TEXT("Unique own native nodes/names/GUIDs/templates with exact current generated-class ownership required.")); }
        Nodes.Add(Node); Templates.Add(Template); Guids.Add(Node->VariableGuid); Names.Add(Node->GetVariableName());
        if (Node->GetVariableName().ToString() == ComponentName) { C.Node = Node; C.Guid = Node->VariableGuid; }
    }
    if (!C.Node) { return Fail(R, TEXT("component_replace_component_not_found"), TEXT("Exact named own local component not found.")); }
    int32 ParentCount = 0; TMap<USCS_Node*, int32> Incoming;
    for (USCS_Node* Node : SCS->GetAllNodes())
    {
        TSet<USCS_Node*> Children;
        for (USCS_Node* Child : Node->GetChildNodes())
        {
            if (!Nodes.Contains(Child) || Child == Node || Children.Contains(Child)) { return Fail(R, TEXT("component_replace_scs_topology_unsupported"), TEXT("Own reciprocal acyclic SCS tree required.")); }
            Children.Add(Child); ++Incoming.FindOrAdd(Child); if (Child == C.Node) { C.Parent = Node; ++ParentCount; }
        }
    }
    TSet<USCS_Node*> Roots, Reachable; TArray<USCS_Node*> Pending;
    for (USCS_Node* Root : SCS->GetRootNodes()) { if (!Nodes.Contains(Root) || Roots.Contains(Root) || Incoming.FindRef(Root) != 0) { return Fail(R, TEXT("component_replace_scs_topology_unsupported"), TEXT("Exact own SCS roots required.")); } Roots.Add(Root); Pending.Add(Root); }
    for (int32 I = 0; I < Pending.Num(); ++I)
    {
        USCS_Node* Node = Pending[I]; if (Reachable.Contains(Node)) { return Fail(R, TEXT("component_replace_scs_topology_unsupported"), TEXT("Repeated/cyclic own SCS topology rejected.")); } Reachable.Add(Node);
        for (USCS_Node* Child : Node->GetChildNodes()) { if (Incoming.FindRef(Child) != 1 || Pending.Num() >= MaxNodes) { return Fail(R, TEXT("component_replace_scs_topology_unsupported"), TEXT("Single-parent bounded local SCS tree required.")); } Pending.Add(Child); }
    }
    C.RootMode=ParentCount==0&&Roots.Contains(C.Node)&&C.Parent==nullptr;
    if (Reachable.Num() != Nodes.Num() || (!C.RootMode&&(ParentCount != 1 || Roots.Contains(C.Node) || !C.Parent || !C.Parent->ComponentClass->IsChildOf(USceneComponent::StaticClass()))))
    { return Fail(R, TEXT("component_replace_root_or_children_unsupported"), TEXT("Reachable own native shape with one local Scene parent or one own parentless scene root required; inherited/native-bound variants remain incomplete.")); }
    USceneComponent* Scene = Cast<USceneComponent>(C.Node->ComponentTemplate);
    if (!Loaded(Scene) || (Scene->GetClass() != UBoxComponent::StaticClass() && Scene->GetClass() != USphereComponent::StaticClass() && Scene->GetClass()!=UCapsuleComponent::StaticClass() && Scene->GetClass()!=UStaticMeshComponent::StaticClass()) || Scene->IsRegistered() || Scene->IsPreRegistering() || Scene->GetOwner() || Scene->GetWorld() || Scene->GetAttachParent() || Scene->GetAttachChildren().Num() != 0)
    { return Fail(R, TEXT("component_replace_source_class_unsupported"), TEXT("Exact native unregistered/non-preregistering own Box/Sphere/Capsule/StaticMesh template without owner/world/template attachments required.")); }
    C.ShapeChildrenMode = !C.Node->GetChildNodes().IsEmpty();
    C.Slot = C.Node->GetVariableName().ToString() + USimpleConstructionScript::ComponentTemplateNameSuffix;
    if (Scene->GetName() != C.Slot || FindObject<UObject>(B->GeneratedClass, *C.Slot) != Scene) { return Fail(R, TEXT("component_replace_template_slot_collision"), TEXT("Selected template must be the sole exact standard generated-class template slot; no repeated collision Rename is admitted.")); }
    return true;
}
bool GraphReferences(FContext& C, FResult& R)
{
    TArray<UEdGraph*> Graphs; C.Blueprint->GetAllGraphs(Graphs); int32 Count = 0;
    if (Graphs.Num() > MaxGraphs) { return Fail(R, TEXT("component_replace_graph_unobservable"), TEXT("Stored graph count exceeds finite bound.")); }
    for (UEdGraph* Graph : Graphs)
    {
        if (!Loaded(Graph)) { return Fail(R, TEXT("component_replace_graph_unobservable"), TEXT("A stored graph is not readable.")); }
        const auto NodeCopies=C.ShapeMode?Graph->Nodes:TArray<TObjectPtr<UEdGraphNode>>();const auto& NodeRows=C.ShapeMode?NodeCopies:Graph->Nodes;for (UEdGraphNode* Node : NodeRows)
        {
            if (!Loaded(Node) || ++Count > 4096) { return Fail(R, TEXT("component_replace_graph_unobservable"), TEXT("Stored graph nodes exceed readable finite bound.")); }
            if (UK2Node_Variable* Variable = Cast<UK2Node_Variable>(Node); Variable && (Variable->VariableReference.GetMemberGuid() == C.Guid || (Variable->VariableReference.IsSelfContext() && Variable->VariableReference.GetMemberName() == FName(*C.SelectedName))))
            { if(!NativeGetAllowed(C,Node)){R.Data->SetStringField(TEXT("selected_graph_reference"), Node->GetPathName()); return Fail(R, TEXT("component_replace_graph_reference_unsupported"), TEXT("Selected SCS variable graph references require a separate typed pin/signature migration; rejected before Modify."));} }
            if (const UK2Node_ComponentBoundEvent* Event = Cast<UK2Node_ComponentBoundEvent>(Node); Event && Event->GetComponentPropertyName() == FName(*C.SelectedName))
            { if(!NativeGetAllowed(C,Node)){ R.Data->SetStringField(TEXT("selected_graph_reference"), Node->GetPathName()); R.Data->SetStringField(TEXT("selected_graph_reference_kind"), TEXT("component_bound_event")); return Fail(R, TEXT("component_replace_graph_reference_unsupported"), TEXT("Selected component bound-event references require a separate delegate/type migration; rejected before Modify.")); } }
            const auto PinCopies=C.ShapeMode?Node->Pins:TArray<UEdGraphPin*>();const auto& PinRows=C.ShapeMode?PinCopies:Node->Pins;for (const UEdGraphPin* Pin : PinRows) { if(!ShapeCurrent(C,R))return false;TStrongObjectPtr<UObject> ReturnedDefault(C.ShapeMode&&Pin?Pin->DefaultObject.Get():nullptr);if(!ShapeCurrent(C,R))return false;if (!Pin || Pin->DefaultObject == C.Node->ComponentTemplate) { return Fail(R, TEXT("component_replace_graph_reference_unsupported"), TEXT("Missing pin or selected template literal reference is outside finite replacement proof.")); } }
        }
    }
    TArray<UObject*> Instances; C.Node->ComponentTemplate->GetArchetypeInstances(Instances);TArray<TStrongObjectPtr<UObject>> ReturnedInstanceHolds;if(C.NativeGet||C.RootMode){for(UObject* O:Instances)ReturnedInstanceHolds.Emplace(O);if(!ShapeCurrent(C,R))return false;} TArray<TSharedPtr<FJsonValue>> Rows;
    for (UObject* Instance : Instances) { Rows.Add(MakeShared<FJsonValueObject>(Identity(Instance))); if (Rows.Num() > MaxNodes) { break; } }
    R.Data->SetArrayField(TEXT("selected_template_archetype_instances"), Rows); R.Data->SetNumberField(TEXT("selected_template_archetype_instance_count"), Instances.Num());
    return Instances.Num() == 0 || Fail(R, TEXT("component_replace_archetype_instances_unsupported"), TEXT("Existing selected-template archetype instances require separate propagation proof; no template Rename attempted."));
}
TArray<TSharedPtr<FJsonValue>> Vector(const FVector& V) { return { MakeShared<FJsonValueNumber>(V.X), MakeShared<FJsonValueNumber>(V.Y), MakeShared<FJsonValueNumber>(V.Z) }; }
TArray<TSharedPtr<FJsonValue>> Rotation(const FRotator& V) { return { MakeShared<FJsonValueNumber>(V.Pitch), MakeShared<FJsonValueNumber>(V.Yaw), MakeShared<FJsonValueNumber>(V.Roll) }; }
TSharedRef<FJsonObject> Common(UPrimitiveComponent* Primitive)
{
    auto J = MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> Tags;
    for (const FName& Tag : Primitive->ComponentTags) { Tags.Add(MakeShared<FJsonValueString>(Tag.ToString())); } J->SetArrayField(TEXT("component_tags"), Tags);
    auto Transform = MakeShared<FJsonObject>(); Transform->SetArrayField(TEXT("location"), Vector(Primitive->GetRelativeLocation())); Transform->SetArrayField(TEXT("rotation"), Rotation(Primitive->GetRelativeRotation())); Transform->SetArrayField(TEXT("scale"), Vector(Primitive->GetRelativeScale3D())); J->SetObjectField(TEXT("relative_transform"), Transform);
    J->SetNumberField(TEXT("mobility_value"), static_cast<uint8>(Primitive->Mobility.GetValue())); J->SetBoolField(TEXT("visible"), Primitive->IsVisible()); J->SetBoolField(TEXT("hidden_in_game"), Primitive->bHiddenInGame);
    J->SetStringField(TEXT("collision_profile"), Primitive->GetCollisionProfileName().ToString()); J->SetNumberField(TEXT("collision_enabled_value"), static_cast<uint8>(Primitive->GetCollisionEnabled())); J->SetBoolField(TEXT("generate_overlap_events"), Primitive->GetGenerateOverlapEvents()); J->SetNumberField(TEXT("can_character_step_up_on_value"), static_cast<uint8>(Primitive->CanCharacterStepUpOn.GetValue())); return J;
}
// OD Repair1: SetRelativeRotation explicitly converts through FQuat in the
// installed SDK. Only this semantic rotation field uses the same public scene
// tolerance; every other copied common field remains JSON-exact.
bool CommonMismatch(FResult& R,const FString& ObjectPath,const FString& Property,
    const TSharedPtr<FJsonValue>& Expected,const TSharedPtr<FJsonValue>& Actual)
{
    if(!R.Data->HasField(TEXT("native_scene_common_first_mismatch"))){
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("object_path"),ObjectPath);J->SetStringField(TEXT("property_path"),Property);
        J->SetBoolField(TEXT("expected_exists"),Expected.IsValid());J->SetBoolField(TEXT("actual_exists"),Actual.IsValid());
        J->SetField(TEXT("expected"),Expected.IsValid()?Expected:MakeShared<FJsonValueNull>());J->SetField(TEXT("actual"),Actual.IsValid()?Actual:MakeShared<FJsonValueNull>());
        J->SetBoolField(TEXT("expected_actual_observed"),true);J->SetBoolField(TEXT("independent_engine_reread"),false);J->SetStringField(TEXT("observation_kind"),TEXT("copied_native_common_snapshots"));
        J->SetStringField(TEXT("comparison_scope"),Property.StartsWith(TEXT("/relative_transform/rotation"))?TEXT("FRotator normalized axes with installed SCENECOMPONENT_ROTATOR_TOLERANCE degrees"):TEXT("Exact copied JSON field comparison; no tolerance"));
        R.Data->SetObjectField(TEXT("native_scene_common_first_mismatch"),J);
    }
    return false;
}
bool CommonFieldEqual(const TSharedPtr<FJsonValue>& Expected,const TSharedPtr<FJsonValue>& Actual,
    const FString& Property,const FString& ObjectPath,FResult& R)
{
    if(Equal(Expected,Actual))return true;
    if(!Expected.IsValid()||!Actual.IsValid()||Expected->Type!=Actual->Type)return CommonMismatch(R,ObjectPath,Property,Expected,Actual);
    if(Property==TEXT("/relative_transform/rotation")){
        if(Expected->Type!=EJson::Array||Expected->AsArray().Num()!=3||Actual->AsArray().Num()!=3)return CommonMismatch(R,ObjectPath,Property,Expected,Actual);
        double Left[3],Right[3];
        for(int32 Axis=0;Axis<3;++Axis){const auto& E=Expected->AsArray()[Axis];const auto& A=Actual->AsArray()[Axis];if(!E.IsValid()||!A.IsValid()||E->Type!=EJson::Number||A->Type!=EJson::Number||!FMath::IsFinite(E->AsNumber())||!FMath::IsFinite(A->AsNumber()))return CommonMismatch(R,ObjectPath,Property+TEXT("/")+LexToString(Axis),E,A);Left[Axis]=E->AsNumber();Right[Axis]=A->AsNumber();}
        const FRotator Before(Left[0],Left[1],Left[2]),After(Right[0],Right[1],Right[2]);
        constexpr double Tolerance=double(SCENECOMPONENT_ROTATOR_TOLERANCE);
        const bool Same=Before.Equals(After,Tolerance);auto Observation=MakeShared<FJsonObject>();Observation->SetStringField(TEXT("stage"),R.Stage);Observation->SetStringField(TEXT("object_path"),ObjectPath);Observation->SetField(TEXT("expected"),Expected);Observation->SetField(TEXT("actual"),Actual);Observation->SetNumberField(TEXT("tolerance_degrees"),Tolerance);Observation->SetBoolField(TEXT("semantic_match"),Same);Observation->SetBoolField(TEXT("exact_match"),false);Observation->SetStringField(TEXT("basis"),TEXT("Installed SceneComponent SetRelativeRotation and FRotator::Equals normalized axis semantics; no tolerance on location/scale/other fields"));R.Data->SetObjectField(TEXT("native_scene_common_rotation_comparison"),Observation);
        if(Same)return true;
        for(int32 Axis=0;Axis<3;++Axis)if(FMath::Abs(FRotator::NormalizeAxis(Left[Axis]-Right[Axis]))>Tolerance)return CommonMismatch(R,ObjectPath,Property+TEXT("/")+LexToString(Axis),Expected->AsArray()[Axis],Actual->AsArray()[Axis]);
        return CommonMismatch(R,ObjectPath,Property,Expected,Actual);
    }
    if(Expected->Type==EJson::Object){
        const auto Left=Expected->AsObject(),Right=Actual->AsObject();if(!Left.IsValid()||!Right.IsValid())return CommonMismatch(R,ObjectPath,Property,Expected,Actual);
        TArray<FString> Names;for(const auto& Entry:Left->Values)Names.Add(FString(Entry.Key.ToView()));for(const auto& Entry:Right->Values)Names.AddUnique(FString(Entry.Key.ToView()));Names.Sort();
        for(const FString& Field:Names)if(!CommonFieldEqual(Left->TryGetField(Field),Right->TryGetField(Field),Property+TEXT("/")+Field,ObjectPath,R))return false;
        return true;
    }
    if(Expected->Type==EJson::Array){
        if(Expected->AsArray().Num()!=Actual->AsArray().Num())return CommonMismatch(R,ObjectPath,Property,Expected,Actual);
        for(int32 Index=0;Index<Expected->AsArray().Num();++Index)if(!CommonFieldEqual(Expected->AsArray()[Index],Actual->AsArray()[Index],Property+TEXT("/")+LexToString(Index),ObjectPath,R))return false;
        return true;
    }
    return CommonMismatch(R,ObjectPath,Property,Expected,Actual);
}
bool CommonEqual(const TSharedRef<FJsonObject>& Expected,const TSharedRef<FJsonObject>& Actual,
    const FString& ObjectPath,FResult& R)
{return CommonFieldEqual(MakeShared<FJsonValueObject>(Expected),MakeShared<FJsonValueObject>(Actual),FString(),ObjectPath,R);}
TSharedRef<FJsonObject> Summary(USCS_Node* Node)
{
    auto J = MakeShared<FJsonObject>(); if (!Loaded(Node) || !Loaded(Node->ComponentTemplate)) { return J; }
    auto* Primitive = Cast<UPrimitiveComponent>(Node->ComponentTemplate); if (!Primitive) { return J; }
    J->SetStringField(TEXT("component_name"), Node->GetVariableName().ToString()); J->SetStringField(TEXT("variable_name"), Node->GetVariableName().ToString()); J->SetStringField(TEXT("node_guid"), Node->VariableGuid.ToString());
    J->SetStringField(TEXT("component_class"), Node->ComponentClass->GetName()); J->SetStringField(TEXT("component_class_path"), Node->ComponentClass->GetPathName()); J->SetStringField(TEXT("template_name"), Primitive->GetName()); J->SetStringField(TEXT("template_path"), Primitive->GetPathName());
    const auto Values = Common(Primitive); for (const auto& P : Values->Values) { J->SetField(P.Key, P.Value); }
    UEnum* MobilityEnum = FindObject<UEnum>(nullptr, TEXT("/Script/Engine.EComponentMobility")); UEnum* CollisionEnum = FindObject<UEnum>(nullptr, TEXT("/Script/Engine.ECollisionEnabled"));
    J->SetStringField(TEXT("mobility"), Loaded(MobilityEnum) ? MobilityEnum->GetNameStringByValue(static_cast<uint8>(Primitive->Mobility.GetValue())) : TEXT("")); J->SetStringField(TEXT("collision_enabled"), Loaded(CollisionEnum) ? CollisionEnum->GetNameStringByValue(static_cast<uint8>(Primitive->GetCollisionEnabled())) : TEXT(""));
    if (const UBoxComponent* Box = Cast<UBoxComponent>(Primitive)) { J->SetStringField(TEXT("shape"), TEXT("box")); J->SetArrayField(TEXT("box_extent"), Vector(Box->GetUnscaledBoxExtent())); }
    else if (const USphereComponent* Sphere = Cast<USphereComponent>(Primitive)) { J->SetStringField(TEXT("shape"), TEXT("sphere")); J->SetNumberField(TEXT("sphere_radius"), Sphere->GetUnscaledSphereRadius()); }
    else if(const UCapsuleComponent* Capsule=Cast<UCapsuleComponent>(Primitive)){J->SetStringField(TEXT("shape"),TEXT("capsule"));J->SetNumberField(TEXT("capsule_radius"),Capsule->GetUnscaledCapsuleRadius());J->SetNumberField(TEXT("capsule_half_height"),Capsule->GetUnscaledCapsuleHalfHeight());}if(Primitive->GetClass()==UStaticMeshComponent::StaticClass()){auto* P=FindFProperty<FObjectProperty>(UStaticMeshComponent::StaticClass(),TEXT("StaticMesh"));if(P){const auto* Slot=P->GetObjectPtrPropertyValuePtr(P->ContainerPtrToValuePtr<void>(Primitive));J->SetStringField(TEXT("static_mesh"),Slot->IsResolved()?Slot->GetPathName():TEXT("<unresolved>"));}}return J;
}
bool FiniteCommon(UPrimitiveComponent* P)
{
    if (!Loaded(P) || P->GetRelativeLocation().ContainsNaN() || P->GetRelativeRotation().ContainsNaN() || P->GetRelativeScale3D().ContainsNaN() || P->ComponentTags.Num() > 256) { return false; }
    UEnum* MobilityEnum = FindObject<UEnum>(nullptr, TEXT("/Script/Engine.EComponentMobility")); UEnum* CollisionEnum = FindObject<UEnum>(nullptr, TEXT("/Script/Engine.ECollisionEnabled"));
    if (!Loaded(MobilityEnum) || !Loaded(CollisionEnum) || MobilityEnum->GetNameStringByValue(static_cast<uint8>(P->Mobility.GetValue())).IsEmpty() || CollisionEnum->GetNameStringByValue(static_cast<uint8>(P->GetCollisionEnabled())).IsEmpty()) { return false; }
    const auto C = Common(P); return CleanText(Text(C)) && Text(C).Len() <= MaxText;
}
struct FObservation
{
    TSharedRef<FJsonObject> State = MakeShared<FJsonObject>(), NativeSources = MakeShared<FJsonObject>(), DiskState = MakeShared<FJsonObject>(), OtherDirty = MakeShared<FJsonObject>();
    bool bComplete = false;
};
TSharedRef<FJsonObject> Observation(const FObservation& O)
{
    auto J = MakeShared<FJsonObject>(); J->SetBoolField(TEXT("complete"), O.bComplete); J->SetObjectField(TEXT("state"), O.State); J->SetObjectField(TEXT("native_sources"), O.NativeSources); J->SetObjectField(TEXT("disk"), O.DiskState); J->SetObjectField(TEXT("other_package_dirty"), O.OtherDirty); return J;
}
bool Capture(FContext& C, FObservation& O, FResult& R, const TCHAR* Key)
{
    O = FObservation{}; // Each phase owns fresh maps; earlier published prefixes remain immutable.
    auto J = Observation(O); R.Data->SetObjectField(Key, J); UBlueprint* B = C.Blueprint.Get();
    if (!Loaded(B) || !Loaded(B->GeneratedClass) || !Loaded(C.CDO) || B->GeneratedClass->GetDefaultObject(false) != C.CDO || !Loaded(C.Node)) { return false; }
    O.State->SetObjectField(TEXT("identity"), Identity(B)); O.State->SetObjectField(TEXT("cdo_identity"), Identity(C.CDO)); O.State->SetStringField(TEXT("parent_class"), B->ParentClass->GetPathName()); O.State->SetStringField(TEXT("generated_class"), B->GeneratedClass->GetPathName()); O.State->SetStringField(TEXT("skeleton_class"), B->SkeletonGeneratedClass->GetPathName()); O.State->SetBoolField(TEXT("package_dirty"), B->GetOutermost()->IsDirty()); O.State->SetNumberField(TEXT("blueprint_status"), static_cast<uint8>(B->Status.GetValue()));
    auto Declarations = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("blueprint_declarations"), Declarations);
    for (const FName Field : { FName(TEXT("NewVariables")), FName(TEXT("ImplementedInterfaces")), FName(TEXT("ParentClass")) })
    { FProperty* P = FindFProperty<FProperty>(B->GetClass(), Field); FString S; if (!P)return false;const bool Exported=P->ExportText_Direct(S,P->ContainerPtrToValuePtr<void>(B),P->ContainerPtrToValuePtr<void>(B),B,PPF_None);if(!ShapeCurrent(C,R))return false;if (!Exported || S.Len() > MaxText || !CleanText(S)) { return false; } Declarations->SetStringField(Field.ToString(), S); }
    auto CDOProps = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("unselected_cdo_properties"), CDOProps); if (!Properties(C.CDO, { FName(*C.SelectedName) }, CDOProps,C.ShapeMode?&C:nullptr,&R)) { return false; }
    auto Member = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("selected_scs_cdo_member"), Member); FProperty* P = FindFProperty<FProperty>(C.CDO->GetClass(), FName(*C.SelectedName)); Member->SetBoolField(TEXT("present"), P != nullptr);
    if (P)
    {
        FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(P); Member->SetStringField(TEXT("path"), P->GetPathName()); Member->SetStringField(TEXT("owner"), P->GetOwnerStruct()->GetPathName()); Member->SetStringField(TEXT("property_flags"), LexToString(static_cast<uint64>(P->GetPropertyFlags()))); Member->SetNumberField(TEXT("array_dim"), P->ArrayDim);
        if (!Object || P->ArrayDim != 1 || P->GetOwnerClass() != C.CDO->GetClass() || (Object->PropertyClass != UBoxComponent::StaticClass() && Object->PropertyClass != USphereComponent::StaticClass() && Object->PropertyClass!=UCapsuleComponent::StaticClass()&&Object->PropertyClass!=UStaticMeshComponent::StaticClass()) || !P->HasAnyPropertyFlags(CPF_BlueprintVisible)) { return false; }
        Member->SetStringField(TEXT("object_class"), Object->PropertyClass->GetPathName()); Member->SetStringField(TEXT("value"), ObjectPath(Object->GetObjectPropertyValue_InContainer(C.CDO)));
    }
    USimpleConstructionScript* SCS = B->SimpleConstructionScript.Get(); if (!Loaded(SCS) || SCS->GetAllNodes().Num() > MaxNodes) { return false; }
    O.State->SetObjectField(TEXT("scs_identity"), Identity(SCS)); TSet<USCS_Node*> Nodes;
    for (USCS_Node* Node : SCS->GetAllNodes()) { if (!Loaded(Node) || Node->GetOuter() != SCS || Nodes.Contains(Node) || !Node->VariableGuid.IsValid() || !Native(Node->ComponentClass) || !Loaded(Node->ComponentTemplate) || Node->ComponentTemplate->GetClass() != Node->ComponentClass || Node->ComponentTemplate->GetOuter() != B->GeneratedClass) { return false; } Nodes.Add(Node); }
    TArray<TSharedPtr<FJsonValue>> Rows; O.State->SetArrayField(TEXT("local_scs"), Rows);
    for (USCS_Node* Node : SCS->GetAllNodes())
    {
        auto Row = MakeShared<FJsonObject>(); Rows.Add(MakeShared<FJsonValueObject>(Row)); O.State->SetArrayField(TEXT("local_scs"), Rows); Row->SetObjectField(TEXT("identity"), Identity(Node)); Row->SetStringField(TEXT("name"), Node->GetVariableName().ToString()); Row->SetStringField(TEXT("guid"), Node->VariableGuid.ToString()); Row->SetStringField(TEXT("component_class"), Node->ComponentClass->GetPathName()); Row->SetObjectField(TEXT("template_identity"), Identity(Node->ComponentTemplate));
        auto NodeProps = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("node_properties"), NodeProps); if (!Properties(Node, Node == C.Node ? TSet<FName>{ TEXT("ComponentClass"), TEXT("ComponentTemplate") } : TSet<FName>{}, NodeProps,C.ShapeMode?&C:nullptr,&R)) { return false; }
        auto TemplateProps = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("template_properties"), TemplateProps); if (!Properties(Node->ComponentTemplate, {}, TemplateProps,C.ShapeMode?&C:nullptr,&R)) { return false; }
        TArray<TSharedPtr<FJsonValue>> Children; for (USCS_Node* Child : Node->GetChildNodes()) { if (!Nodes.Contains(Child)) { return false; } Children.Add(MakeShared<FJsonValueString>(Child->VariableGuid.ToString())); } Row->SetArrayField(TEXT("children"), Children);
    }
    TArray<TSharedPtr<FJsonValue>> Roots; for (USCS_Node* Root : SCS->GetRootNodes()) { if (!Nodes.Contains(Root)) { return false; } Roots.Add(MakeShared<FJsonValueString>(Root->VariableGuid.ToString())); } O.State->SetArrayField(TEXT("scs_roots"), Roots);
    if (!Nodes.Contains(C.Node) || (!C.RootMode&&!Nodes.Contains(C.Parent)) || (C.RootMode&&C.Parent)) { return false; }
    auto Selected = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("selected"), Selected); Selected->SetObjectField(TEXT("summary"), Summary(C.Node)); Selected->SetObjectField(TEXT("common"), Common(CastChecked<UPrimitiveComponent>(C.Node->ComponentTemplate))); Selected->SetStringField(TEXT("parent_guid"), C.RootMode?FString():C.Parent->VariableGuid.ToString()); Selected->SetStringField(TEXT("parent_name"), C.RootMode?FString():C.Parent->GetVariableName().ToString());
    Selected->SetStringField(TEXT("slot"), C.Slot); Selected->SetBoolField(TEXT("current_slot_matches_selected"), FindObject<UObject>(B->GeneratedClass, *C.Slot) == C.Node->ComponentTemplate); Selected->SetStringField(TEXT("archetype_path"), ObjectPath(C.Node->ComponentTemplate->GetArchetype()));
    auto Graph = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("graphs"), Graph);
    if (!Graphs(B, Graph,C.ShapeMode?&C:nullptr,&R) || !Sources(B->ParentClass, O.NativeSources,C.MeshSource.IsValid()?UStaticMeshComponent::StaticClass():C.ShapeMode?(C.TargetClass==UCapsuleComponent::StaticClass()||C.TargetClass==UStaticMeshComponent::StaticClass())?C.TargetClass:C.OldTemplate.IsValid()&&C.OldTemplate->GetClass()==UCapsuleComponent::StaticClass()?UCapsuleComponent::StaticClass():nullptr:nullptr,C.ShapeMode?&C:nullptr,&R) || !Disk(C.Package, O.DiskState) || !Dirty(C.Package, O.OtherDirty)) { return false; }
    if(!ShapeCurrent(C,R)||!StaticMeshSourceFiles(C,R))return false;O.bComplete = true; J->SetBoolField(TEXT("complete"), true); return true;
}
// Only a confirmed successful full compile may replace this exact owned CDO
// with the same identity and the precise constructor-cleared load bits.
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
bool SameUnselected(const FObservation& A, const FObservation& B, const FString& Name, bool AllowCompileCdo)
{
    if (!A.bComplete || !B.bComplete) { return false; }
    for (const auto& P : A.State->Values)
    {
        const FString Key(P.Key.ToView()); const auto Other = B.State->TryGetField(P.Key.ToView());
        if (Key == TEXT("selected") || Key == TEXT("selected_scs_cdo_member") || Key == TEXT("package_dirty") || Key == TEXT("blueprint_status")) { continue; }
        if (AllowCompileCdo && Key == TEXT("cdo_identity")) { if (!PostcompileCdoEqual(P.Value, Other)) { return false; } continue; }
        if (Key != TEXT("local_scs")) { if (!Equal(P.Value, Other)) { return false; } continue; }
        if (!Other || Other->Type != EJson::Array || P.Value->AsArray().Num() != Other->AsArray().Num()) { return false; }
        for (int32 I = 0; I < P.Value->AsArray().Num(); ++I)
        {
            const auto Left = P.Value->AsArray()[I]->AsObject(), Right = Other->AsArray()[I]->AsObject(); if (Left->Values.Num() != Right->Values.Num()) { return false; }
            for (const auto& Field : Left->Values)
            { if (Left->GetStringField(TEXT("name")) == Name && (Field.Key.ToView() == TEXT("component_class") || Field.Key.ToView() == TEXT("template_identity") || Field.Key.ToView() == TEXT("template_properties"))) { continue; } if (!Equal(Field.Value, Right->TryGetField(Field.Key.ToView()))) { return false; } }
        }
    }
    return A.State->Values.Num() == B.State->Values.Num();
}
void ClearFinal(FResult& R)
{ for (const TCHAR* K : { TEXT("observations_complete"), TEXT("source_preserved"), TEXT("unselected_state_preserved"), TEXT("independent_readback_passed") }) { R.Data->SetBoolField(K, false); } for(const TCHAR* K:{TEXT("static_mesh_source_references_preserved"),TEXT("static_mesh_source_files_preserved")})if(R.Data->HasField(K))R.Data->SetBoolField(K,false); }
bool Fences(const FObservation& Before, const FObservation& After, FContext& C, FResult& R)
{
    const bool Complete = Before.bComplete && After.bComplete; R.Data->SetBoolField(TEXT("observations_complete"), Complete);
    if (!Complete) { return Fail(R, TEXT("component_replace_observation_incomplete"), TEXT("Actual bounded prefix retained; no next mutation or save."), R.bAttempted); }
    const bool Compiled = R.bCompileAttempted && R.Compiled.IsSet() && R.Compiled.GetValue();
    FObservation Expected=Before;
    if(C.NativeGet){TSharedPtr<FJsonValue> Projected;if(!NativeGetProject(C,Before.State->TryGetField(TEXT("graphs")),Projected))return ShapeStop(C,R,TEXT("selected_getter_graph_projection"),TEXT("original JSON plus planned selected output type"),TEXT("projection failed"));Expected.State=MakeShared<FJsonObject>(*Before.State);Expected.State->SetField(TEXT("graphs"),Projected);}
    const bool Source = Equal(Before.NativeSources, After.NativeSources), State = SameUnselected(Expected, After, C.SelectedName, Compiled), Other = Equal(Before.OtherDirty, After.OtherDirty), D = Equal(Before.DiskState, After.DiskState);
    R.Data->SetBoolField(TEXT("postcompile_owned_cdo_identity_verified"), Compiled && PostcompileCdoEqual(Before.State->TryGetField(TEXT("cdo_identity")), After.State->TryGetField(TEXT("cdo_identity"))));
    const bool ReplacedIdentity = C.OldTemplate.IsValid() && C.NewTemplate.IsValid() && C.OldTemplate.Get() != C.NewTemplate.Get() && Loaded(C.Node) && C.Node->ComponentTemplate == C.NewTemplate.Get();
    R.bKnown = true; R.bChanged = ReplacedIdentity || !Equal(Before.State, After.State) || !Source || !Other || !D;
    R.Data->SetBoolField(TEXT("selected_template_object_identity_replaced"), ReplacedIdentity);
    R.Data->SetBoolField(TEXT("source_preserved"), Source); R.Data->SetBoolField(TEXT("unselected_state_preserved"), State && Other); R.Data->SetBoolField(TEXT("disk_preserved_before_save"), D);
    return (Source && State && Other && D) || Fail(R, TEXT("component_replace_preservation_failed"), TEXT("Exact unrelated SCS/CDO/declarations/graphs, native source, other-dirty or pre-save disk fence differs; actual complete prefix retained, no save."));
}
void Retirement(FContext& C, FResult& R, const TCHAR* Key)
{
    auto J = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("old_template"), Identity(C.OldTemplate.Get())); J->SetObjectField(TEXT("new_template"), Identity(C.NewTemplate.Get()));
    J->SetBoolField(TEXT("old_template_held_alive"), C.OldTemplate.IsValid()); J->SetBoolField(TEXT("old_template_in_transient_package"), Loaded(C.OldTemplate.Get()) && C.OldTemplate->GetOutermost() == GetTransientPackage()); J->SetBoolField(TEXT("destruction_or_gc_requested"), false);
    int32 OldCount = 0, NewCount = 0; UBlueprint* B = C.Blueprint.Get(); USimpleConstructionScript* SCS = Loaded(B) ? B->SimpleConstructionScript.Get() : nullptr;
    bool CountsObserved = Loaded(SCS) && SCS->GetAllNodes().Num() <= MaxNodes;
    if (CountsObserved) { for (USCS_Node* Node : SCS->GetAllNodes()) { if (!Loaded(Node)) { CountsObserved = false; break; } OldCount += Node->ComponentTemplate == C.OldTemplate.Get(); NewCount += C.NewTemplate.IsValid() && Node->ComponentTemplate == C.NewTemplate.Get(); } }
    J->SetBoolField(TEXT("scs_reference_count_observed"), CountsObserved);
    if (CountsObserved) { J->SetNumberField(TEXT("old_template_scs_reference_count"), OldCount); J->SetNumberField(TEXT("new_template_scs_reference_count"), NewCount); }
    else { J->SetField(TEXT("old_template_scs_reference_count"), MakeShared<FJsonValueNull>()); J->SetField(TEXT("new_template_scs_reference_count"), MakeShared<FJsonValueNull>()); }
    J->SetBoolField(TEXT("current_standard_slot_is_new"), Loaded(B) && Loaded(B->GeneratedClass) && C.NewTemplate.IsValid() && FindObject<UObject>(B->GeneratedClass, *C.Slot) == C.NewTemplate.Get());
    J->SetObjectField(TEXT("selected_node"), Identity(C.Node)); J->SetObjectField(TEXT("parent_node"), Identity(C.Parent)); J->SetStringField(TEXT("standard_slot"), C.Slot);
    if (Loaded(C.Node)) { J->SetStringField(TEXT("variable_name"), C.Node->GetVariableName().ToString()); J->SetStringField(TEXT("node_guid"), C.Node->VariableGuid.ToString()); J->SetObjectField(TEXT("node_current_template"), Identity(C.Node->ComponentTemplate)); J->SetObjectField(TEXT("node_current_class"), Identity(C.Node->ComponentClass)); }
    if (Loaded(B) && Loaded(B->GeneratedClass)) { J->SetObjectField(TEXT("actual_slot_object"), Identity(FindObject<UObject>(B->GeneratedClass, *C.Slot))); J->SetBoolField(TEXT("target_package_dirty"), B->GetOutermost()->IsDirty()); }
    J->SetStringField(TEXT("duplicate_scope"), TEXT("Exact generated-class standard template slot and all admitted local SCS template pointers; broader object aliases are not exhaustively scanned. Old object is retired, not deleted/GCed.")); R.Data->SetObjectField(Key, J);
}
bool Readback(FContext& C, const FObservation& Before, const FObservation& After, float Radius, FResult& R, bool RequireCompiledMember)
{
    const auto Prior = Before.State->GetObjectField(TEXT("selected")), Current = After.State->GetObjectField(TEXT("selected"));
    const auto SummaryNow = Current->GetObjectField(TEXT("summary")); const auto SummaryBefore = Prior->GetObjectField(TEXT("summary")); const auto Member = After.State->GetObjectField(TEXT("selected_scs_cdo_member"));
    const bool MemberOK = !RequireCompiledMember || (Member->GetBoolField(TEXT("present")) && Member->GetStringField(TEXT("object_class")) == C.TargetClass->GetPathName());
    const bool Passed = CommonEqual(Prior->GetObjectField(TEXT("common")).ToSharedRef(), Current->GetObjectField(TEXT("common")).ToSharedRef(),SummaryNow->GetStringField(TEXT("template_path")),R) && SummaryNow->GetStringField(TEXT("component_class_path")) == C.TargetClass->GetPathName()
        && (C.TargetClass==USphereComponent::StaticClass()?SummaryNow->GetNumberField(TEXT("sphere_radius"))==Radius:ShapeDefaults(C,Cast<UPrimitiveComponent>(C.NewTemplate.Get()))) && SummaryNow->GetStringField(TEXT("variable_name")) == SummaryBefore->GetStringField(TEXT("variable_name")) && SummaryNow->GetStringField(TEXT("node_guid")) == SummaryBefore->GetStringField(TEXT("node_guid"))
        && SummaryNow->GetStringField(TEXT("template_name")) == C.Slot && SummaryNow->GetStringField(TEXT("template_path")) == SummaryBefore->GetStringField(TEXT("template_path")) && Current->GetStringField(TEXT("parent_guid")) == Prior->GetStringField(TEXT("parent_guid")) && Current->GetStringField(TEXT("parent_name")) == Prior->GetStringField(TEXT("parent_name"))
        && Current->GetBoolField(TEXT("current_slot_matches_selected")) && Loaded(C.NewTemplate.Get()) && C.Node->ComponentTemplate == C.NewTemplate.Get() && MemberOK;
    R.Data->SetBoolField(TEXT("independent_readback_passed"), Passed); R.Data->SetBoolField(TEXT("compiled_selected_member_verified"), RequireCompiledMember && MemberOK);
    return Passed || Fail(R, TEXT("component_replace_typed_readback_failed"), TEXT("Native shape/common fields/dimensions, exact standard slot, variable/GUID/parent or compiled member differs; actual prefix retained, no next save."));
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
        }
        return true;
    }
    if (A->Type == EJson::Array) { if (A->AsArray().Num() != B->AsArray().Num()) { return false; } for (int32 I = 0; I < A->AsArray().Num(); ++I) { if (!SavedEqual(A->AsArray()[I], B->AsArray()[I], Package, At + TEXT("/") + LexToString(I))) { return false; } } return true; } return Equal(A, B);
}
// LE GeneratedReplacement_Begin

namespace GeneratedReplacement
{
struct FRole
{
    TStrongObjectPtr<UObject> Object;TStrongObjectPtr<UClass> Class;TStrongObjectPtr<UObject> Outer;TStrongObjectPtr<UPackage> Package;
    FString Path;EObjectFlags Flags=RF_NoFlags;uint32 PackageFlags=0,ClassFlags=0;int32 ClassSize=0;bool IsClass=false;
};
struct FField
{
    FProperty* Property=nullptr;UStruct* Owner=nullptr;FFieldClass* Kind=nullptr;FName Name;EPropertyFlags Flags=CPF_None;int32 Size=0,Offset=0,Dim=0;
};
struct FPin
{
    UEdGraphPin* Pin=nullptr;FGuid Id;FName Name;EEdGraphPinDirection Direction;FEdGraphPinType Type;
    FString Default,Auto;FText Text;TObjectPtr<UObject> Object;TArray<UEdGraphPin*> Links,Children;UEdGraphPin* Parent=nullptr;
    bool Hidden=false,Orphan=false,Advanced=false,NotConnectable=false,DefaultReadOnly=false,DefaultIgnored=false,SaveOrphan=false;
    bool Deprecated=false,MutableRef=false,FriendlyName=false;
    FText NativeFriendly;FString NativeTextDisplay,NativeFriendlyDisplay; // QD logical text at explicit rebind only
};
struct FGraphNode
{
    TStrongObjectPtr<UEdGraphNode> Node;FGuid Guid;TArray<UEdGraphPin*> Pins;TArray<FPin> PinRows;FMemberReference Reference;bool Variable=false,Deprecated=false;
    bool SelectedOverlap=false;
    bool SelectedGetter=false,GetterRetyped=false;int32 GetterValueIndex=INDEX_NONE;FEdGraphPinType GetterBeforeType,GetterAfterType;FString GetterBeforeTypeText,GetterAfterTypeText;
};
struct FGraph
{
    TStrongObjectPtr<UEdGraph> Graph;FGuid Guid;TArray<FGraphNode> Nodes;
};
struct FScsNode
{
    TStrongObjectPtr<USCS_Node> Node;TStrongObjectPtr<UClass> Class;TStrongObjectPtr<UActorComponent> Template;
    FName Name;FGuid Guid;TArray<USCS_Node*> Children;
};
struct FNativeEmptyDelegate
{
    FMulticastSparseDelegateProperty* Property=nullptr;FName Name;int32 Offset=0,Size=0;
    TStrongObjectPtr<USparseDelegateFunction> Signature;TObjectPtr<UFunction> SignatureHandle;EFunctionFlags FunctionFlags=FUNC_None;int32 NumParms=0,ParmsSize=0;
};
struct FScope;
// RA: source/native signature and compiler-derived binding objects are separate authorities.
struct FOverlapParam
{
    FField Field;TStrongObjectPtr<UObject> TypeObject;FEdGraphPinType PinType;
};
struct FNativeOverlap
{
    TStrongObjectPtr<UK2Node_ComponentBoundEvent> Node;FMemberReference Reference;
    FName Component,Delegate,Function;uint32 Flags=0;bool Override=false,Internal=false,Ready=false,DerivedReady=false;
    FField DelegateField;TStrongObjectPtr<USparseDelegateFunction> Signature;TArray<FOverlapParam> Params;EFunctionFlags NativeFlags=FUNC_None,OwnFlags=FUNC_None;int32 NativeParmsSize=0,NativeNumParms=0,OwnParmsSize=0,OwnNumParms=0;FName SparseOwner,SparseDelegate;
    TStrongObjectPtr<UEdGraphSchema_K2> Schema;TStrongObjectPtr<UBlueprintGeneratedClass> BindingOwner;
    TArray<TStrongObjectPtr<UComponentDelegateBinding>> Bindings;TArray<TArray<FBlueprintComponentDelegateBinding>> BindingValues;
    TStrongObjectPtr<UFunction> OwnSignature;TStrongObjectPtr<UClass> OwnOwner;TArray<FField> OwnFields;
    TArray<FBlueprintComponentDelegateBinding> ExpectedBindings;FEdGraphPinType DelegateType;
};
bool NativeOverlapCurrent(FScope&,bool);
bool NativeOverlapPrepare(FScope&);
bool NativeOverlapReturn(FScope&,UBlueprintGeneratedClass*,UBlueprintGeneratedClass*);

bool NativeSceneCurrent(FScope&,bool,bool);
bool NativeAdoptPins(FScope&,bool);
struct FScope
{
    FResult& R;TStrongObjectPtr<UBlueprint> Host,OldBP,NewBP;TStrongObjectPtr<UBlueprintGeneratedClass> HostClass,HostSkeleton;
    TStrongObjectPtr<UClass> OldClass,NewClass,OldSkeleton,NewSkeleton;TStrongObjectPtr<UActorComponent> OldCDO,NewCDO,OldTemplate,NewTemplate;
    TStrongObjectPtr<UObject> HostCDO;TStrongObjectPtr<USimpleConstructionScript> SCS;TStrongObjectPtr<USCS_Node> Selected;TStrongObjectPtr<UEditorEngine> Editor;
    TArray<FRole> Roles;TArray<TStrongObjectPtr<UObject>> RetiredHostRoles;TArray<FScsNode> Nodes;TArray<USCS_Node*> All,Roots;TArray<FGraph> GraphRows;
    TArray<TArray<TObjectPtr<UEdGraph>>> HostArrays,OldArrays,NewArrays;
    TSharedPtr<FJsonObject> OldFieldsBefore;TArray<FNativeEmptyDelegate> NativeEmptyDelegates;bool NativeEmptyMetadataReady=false,NativeEmptyGuardReady=false;
    FString HostPath,HostPackage,SelectedName,Slot;FGuid Guid;decltype(GUndo) Undo=nullptr;
    uint32 HostPackageFlags=0;bool Dirty=false,Stopped=false,GraphsReady=false,Assigned=false,Retired=false;EBlueprintStatus Status=BS_Unknown,OldStatus=BS_Unknown,NewStatus=BS_Unknown;
    TStrongObjectPtr<UEdGraphSchema_K2> GetterSchema;bool GetterPropertiesReady=false;FField GetterSkeletonField,GetterCompiledField;UClass* GetterSkeletonType=nullptr;UClass* GetterCompiledType=nullptr;
    struct FCrossSparse{FField Field;TStrongObjectPtr<UClass> Owner;TStrongObjectPtr<USparseDelegateFunction> Signature;EFunctionFlags Flags=FUNC_None;FName OwnerName,DelegateName;};
    bool CrossNativeOld=false,RequireSceneCompatibility=true,CrossSparseReady=false;TArray<FCrossSparse> CrossSparse;
    TArray<TStrongObjectPtr<UClass>> CrossNativeClasses;TArray<TStrongObjectPtr<UObject>> CrossNativeDefaults;TArray<UClass*> CrossNativeSupers;
    TStrongObjectPtr<USCS_Node> CrossRoot,CrossParent,CrossDefaultRoot;TStrongObjectPtr<UActorComponent> CrossDefaultTemplate;
    bool CrossPromoted=false;TSharedPtr<FJsonValue> CrossParentChildrenBefore;
    bool NativeScene=false,NativeReturning=false,NativeCandidatesReady=false,NativeSaved=false;FContext* NativeContext=nullptr;
    TSharedPtr<FNativeOverlap> Overlap;
    TArray<FGraph> NativeCandidates;TArray<TStrongObjectPtr<UObject>> NativeReturnedHolds;
    explicit FScope(FResult& In):R(In){}
    static bool Globals(){return IsEditorGameThread()&&GEditor&&!IsPlaySessionActive()&&!GIsImportingT3D&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage();}
    bool Stop(const TCHAR* Why,const FString& Message)
    {if(NativeScene&&!R.Data->HasField(TEXT("native_get_first_current_failure"))){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("error_code"),Why);J->SetStringField(TEXT("reason"),Message);J->SetBoolField(TEXT("additional_observation"),false);R.Data->SetObjectField(TEXT("native_get_first_current_failure"),J);}Stopped=true;R.bKnown=false;R.Data->SetBoolField(TEXT("observation_incomplete"),true);return Fail(R,Why,Message,true);}
    int32 Role(UObject* O)
    {
        for(int32 I=0;I<Roles.Num();++I)if(Roles[I].Object.Get()==O)return I;
        if(!Loaded(O)||!Loaded(O->GetClass())||!Loaded(O->GetOutermost()))return INDEX_NONE;
        FRole A;A.Object.Reset(O);A.Class.Reset(O->GetClass());A.Outer.Reset(O->GetOuter());A.Package.Reset(O->GetOutermost());A.Path=O->GetPathName();A.Flags=O->GetFlags();A.PackageFlags=A.Package->GetPackageFlags();if(const auto* C=Cast<UClass>(O)){A.IsClass=true;A.ClassFlags=C->GetClassFlags();A.ClassSize=C->GetStructureSize();}return Roles.Add(MoveTemp(A));
    }
    static TArray<TArray<TObjectPtr<UEdGraph>>> Arrays(const UBlueprint* B)
    {return {B->UbergraphPages,B->FunctionGraphs,B->MacroGraphs,B->DelegateSignatureGraphs,B->IntermediateGeneratedGraphs,B->EventGraphs};}
    static bool SameArrays(const TArray<TArray<TObjectPtr<UEdGraph>>>& A,const TArray<TArray<TObjectPtr<UEdGraph>>>& B)
    {
        if(A.Num()!=B.Num())return false;for(int32 I=0;I<A.Num();++I){if(A[I].Num()!=B[I].Num())return false;for(int32 J=0;J<A[I].Num();++J)if(!A[I][J].IsResolved()||!B[I][J].IsResolved()||A[I][J]!=B[I][J])return false;}return true;
    }
    static bool PinTypeSame(const FEdGraphPinType& A,const FEdGraphPinType& B)
    {return A==B&&A.bIsUObjectWrapper==B.bIsUObjectWrapper&&A.bSerializeAsSinglePrecisionFloat==B.bSerializeAsSinglePrecisionFloat&&A.PinValueType.bTerminalIsUObjectWrapper==B.PinValueType.bTerminalIsUObjectWrapper;}
    bool PinSame(const FGraphNode& N,const FPin& P)const
    {
        UEdGraphPin* Q=P.Pin;if(!N.Node->Pins.Contains(Q)||!Q||Q->WasTrashed()||Q->GetOwningNodeUnchecked()!=N.Node.Get()||Q->PinId!=P.Id||!Q->PinId.IsValid()
            ||Q->PinName!=P.Name||Q->Direction!=P.Direction||!PinTypeSame(Q->PinType,P.Type)||Q->DefaultValue!=P.Default||Q->AutogeneratedDefaultValue!=P.Auto
            ||!Q->DefaultTextValue.IdenticalTo(P.Text)||!Q->DefaultObject.IsResolved()||!P.Object.IsResolved()||Q->DefaultObject!=P.Object
            ||Q->LinkedTo!=P.Links||Q->SubPins!=P.Children||Q->ParentPin!=P.Parent||Q->bHidden!=P.Hidden||Q->bOrphanedPin!=P.Orphan||Q->bAdvancedView!=P.Advanced
            ||Q->bNotConnectable!=P.NotConnectable||Q->bDefaultValueIsReadOnly!=P.DefaultReadOnly||Q->bDefaultValueIsIgnored!=P.DefaultIgnored||Q->ShouldSavePinIfOrphaned()!=P.SaveOrphan)return false;
        if(N.SelectedGetter&&(Q->bDeprecated!=P.Deprecated||Q->bDisplayAsMutableRef!=P.MutableRef||Q->bAllowFriendlyName!=P.FriendlyName))return false;
        return true;
    }
    static FString PointerLabel(const void* P){return FString::Printf(TEXT("%p"),P);}
    FString CapturedRolePath(const UObject* O)const
    {for(const auto& A:Roles)if(A.Object.Get()==O)return A.Path;return FString();}
    bool GraphMismatch(const FGraph& G,int32 NodeIndex,int32 PinIndex,const TCHAR* Field,const FString& Expected,const FString& Actual,const TCHAR* Code,const TCHAR* Message)
    {
        if(!R.Data->HasField(TEXT("compiler_graph_first_mismatch")))
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("graph_captured_path"),CapturedRolePath(G.Graph.Get()));J->SetStringField(TEXT("graph_guid"),G.Guid.ToString());
            J->SetNumberField(TEXT("node_index"),NodeIndex);J->SetNumberField(TEXT("pin_index"),PinIndex);J->SetStringField(TEXT("first_failed_condition"),Field);J->SetStringField(TEXT("expected_value"),Expected);J->SetStringField(TEXT("actual_value"),Actual);
            if(G.Nodes.IsValidIndex(NodeIndex)){J->SetStringField(TEXT("node_captured_path"),CapturedRolePath(G.Nodes[NodeIndex].Node.Get()));J->SetStringField(TEXT("node_guid_expected"),G.Nodes[NodeIndex].Guid.ToString());if(G.Nodes[NodeIndex].PinRows.IsValidIndex(PinIndex))J->SetStringField(TEXT("pin_id_expected"),G.Nodes[NodeIndex].PinRows[PinIndex].Id.ToString());}
            J->SetBoolField(TEXT("diagnostic_additional_uobject_queries"),false);J->SetBoolField(TEXT("later_conditions_unobserved"),true);J->SetStringField(TEXT("scope"),TEXT("First failing ordered comparison only. Captured role paths and old pin IDs are prior plain values, not new queries or dereferences of retired pins."));R.Data->SetObjectField(TEXT("compiler_graph_first_mismatch"),J);
        }
        return Stop(Code,Message);
    }
    bool CompilerGraphPinsSame(const FGraph& G)
    {
        constexpr const TCHAR* Code=TEXT("component_replace_generated_refreshed_pin_changed");constexpr const TCHAR* Message=TEXT("Successful explicit refresh changed a host pin semantic/default/link/whole-pin contract; actual compile prefix retained without save or retry.");
        TMap<UEdGraphPin*,UEdGraphPin*> CurrentPins;TSet<UEdGraphPin*> UniquePins;
        for(int32 I=0;I<G.Nodes.Num();++I)
        {
            const auto& N=G.Nodes[I];const auto& Now=N.Node->Pins;
            if(Now.Num()!=N.PinRows.Num())return GraphMismatch(G,I,INDEX_NONE,TEXT("pin_count"),LexToString(N.PinRows.Num()),LexToString(Now.Num()),Code,Message);
            for(int32 J=0;J<N.PinRows.Num();++J)
            {
                const auto& P=N.PinRows[J];UEdGraphPin* Q=Now[J];
                if(!Q||UniquePins.Contains(Q)||Q->WasTrashed()||Q->GetOwningNodeUnchecked()!=N.Node.Get())return GraphMismatch(G,I,J,TEXT("current_pin_owner_or_lifetime"),TEXT("unique live owned pin"),PointerLabel(Q),Code,Message);
                UniquePins.Add(Q);CurrentPins.Add(P.Pin,Q);
                if(Q->PinId!=P.Id||!Q->PinId.IsValid())return GraphMismatch(G,I,J,TEXT("pin_id"),P.Id.ToString(),Q->PinId.ToString(),Code,Message);
                if(Q->PinName!=P.Name)return GraphMismatch(G,I,J,TEXT("pin_name"),P.Name.ToString(),Q->PinName.ToString(),Code,Message);
                if(Q->Direction!=P.Direction)return GraphMismatch(G,I,J,TEXT("pin_direction"),LexToString(static_cast<int32>(P.Direction)),LexToString(static_cast<int32>(Q->Direction)),Code,Message);
                if(!PinTypeSame(Q->PinType,P.Type))return GraphMismatch(G,I,J,TEXT("full_pin_type"),TEXT("exact captured type including wrapper/precision bits"),TEXT("different"),Code,Message);
                if(Q->DefaultValue!=P.Default)return GraphMismatch(G,I,J,TEXT("default_value"),P.Default,Q->DefaultValue,Code,Message);
                if(Q->AutogeneratedDefaultValue!=P.Auto)return GraphMismatch(G,I,J,TEXT("autogenerated_default_value"),P.Auto,Q->AutogeneratedDefaultValue,Code,Message);
                if(!Q->DefaultTextValue.IdenticalTo(P.Text))return GraphMismatch(G,I,J,TEXT("default_text_identity"),TEXT("identical captured FText"),TEXT("different"),Code,Message);
                if(!Q->DefaultObject.IsResolved()||!P.Object.IsResolved()||Q->DefaultObject!=P.Object)return GraphMismatch(G,I,J,TEXT("resolved_default_object"),TEXT("exact captured resolved object handle"),TEXT("different or unresolved"),Code,Message);
                if(Q->bHidden!=P.Hidden||Q->bOrphanedPin!=P.Orphan||Q->bAdvancedView!=P.Advanced||Q->bNotConnectable!=P.NotConnectable||Q->bDefaultValueIsReadOnly!=P.DefaultReadOnly||Q->bDefaultValueIsIgnored!=P.DefaultIgnored||Q->ShouldSavePinIfOrphaned()!=P.SaveOrphan||(N.SelectedGetter&&(Q->bDeprecated!=P.Deprecated||Q->bDisplayAsMutableRef!=P.MutableRef||Q->bAllowFriendlyName!=P.FriendlyName)))return GraphMismatch(G,I,J,TEXT("whole_pin_flags"),TEXT("exact captured flags"),TEXT("different"),Code,Message);
            }
        }
        // Old pointer values are map keys only. Neither an old pin nor an old linked peer is dereferenced after compiler dispatch.
        for(int32 I=0;I<G.Nodes.Num();++I)for(int32 J=0;J<G.Nodes[I].PinRows.Num();++J)
        {
            const auto& P=G.Nodes[I].PinRows[J];UEdGraphPin* Q=CurrentPins.FindRef(P.Pin);
            if(Q->LinkedTo.Num()!=P.Links.Num())return GraphMismatch(G,I,J,TEXT("link_count"),LexToString(P.Links.Num()),LexToString(Q->LinkedTo.Num()),Code,Message);
            for(int32 K=0;K<P.Links.Num();++K){UEdGraphPin* Expected=CurrentPins.FindRef(P.Links[K]);if(!Expected||Q->LinkedTo[K]!=Expected)return GraphMismatch(G,I,J,TEXT("ordered_link_endpoint"),PointerLabel(Expected),PointerLabel(Q->LinkedTo[K]),Code,Message);}
            UEdGraphPin* Parent=P.Parent?CurrentPins.FindRef(P.Parent):nullptr;
            if((P.Parent&&!Parent)||Q->ParentPin!=Parent)return GraphMismatch(G,I,J,TEXT("split_parent_endpoint"),PointerLabel(Parent),PointerLabel(Q->ParentPin),Code,Message);
            if(Q->SubPins.Num()!=P.Children.Num())return GraphMismatch(G,I,J,TEXT("split_child_count"),LexToString(P.Children.Num()),LexToString(Q->SubPins.Num()),Code,Message);
            for(int32 K=0;K<P.Children.Num();++K){UEdGraphPin* Expected=CurrentPins.FindRef(P.Children[K]);if(!Expected||Q->SubPins[K]!=Expected)return GraphMismatch(G,I,J,TEXT("ordered_split_child_endpoint"),PointerLabel(Expected),PointerLabel(Q->SubPins[K]),Code,Message);}
        }
        return true;
    }
    bool AdoptCompilerGraphPins()
    {
        if(!Current(true,true))return false;int32 Changes=0,NodesObserved=0;
        for(auto& G:GraphRows)if(G.Graph->GetOutermost()==Host->GetPackage())
        {
            TMap<UEdGraphPin*,UEdGraphPin*> CurrentPins;for(const auto& N:G.Nodes)for(int32 J=0;J<N.PinRows.Num();++J)CurrentPins.Add(N.PinRows[J].Pin,N.Node->Pins[J]);
            for(auto& N:G.Nodes)
            {
                ++NodesObserved;for(auto& P:N.PinRows){UEdGraphPin* Q=CurrentPins.FindRef(P.Pin);Changes+=Q!=P.Pin;P.Pin=Q;for(auto*& Link:P.Links)Link=CurrentPins.FindRef(Link);for(auto*& Child:P.Children)Child=CurrentPins.FindRef(Child);if(P.Parent)P.Parent=CurrentPins.FindRef(P.Parent);}N.Pins=N.Node->Pins;
            }
        }
        R.Data->SetBoolField(TEXT("compiler_host_pin_semantics_verified"),true);R.Data->SetNumberField(TEXT("compiler_host_pin_pointer_replacements"),Changes);R.Data->SetNumberField(TEXT("compiler_host_pin_nodes_observed"),NodesObserved);
        R.Data->SetStringField(TEXT("compiler_host_pin_adoption_scope"),TEXT("Successful compiled=true and requested/refreshed_nodes=true explicit return only. Exact held host graph/node/GUID, same ordered persistent PinIds/name/direction/full type/default/text/object/whole flags and mapped link/split endpoints; physical pointers only are refreshed. Source graphs and all other current roles remain exact."));return Current();
    }
    // LT: direct own-field inventory is compared before dereferencing a captured FProperty.
    static FObjectProperty* GetterField(UClass* Owner,FName Name)
    {
        for(TFieldIterator<FProperty> It(Owner,EFieldIteratorFlags::ExcludeSuper);It;++It)if(It->GetFName()==Name)return CastField<FObjectProperty>(*It);
        return nullptr;
    }
    static TArray<FField> GetterFields(UClass* Owner)
    {
        TArray<FField> Rows;for(TFieldIterator<FProperty> It(Owner,EFieldIteratorFlags::ExcludeSuper);It;++It){FProperty* P=*It;Rows.Add({P,P->GetOwnerStruct(),P->GetClass(),P->GetFName(),P->GetPropertyFlags(),P->GetSize(),P->GetOffset_ForInternal(),P->ArrayDim});}return Rows;
    }
    static bool GetterFieldsSame(UClass* Owner,const TArray<FField>& Before)
    {
        const auto Now=GetterFields(Owner);if(Now.Num()!=Before.Num())return false;for(int32 I=0;I<Now.Num();++I){const auto& A=Before[I];const auto& B=Now[I];if(A.Property!=B.Property||A.Owner!=B.Owner||A.Kind!=B.Kind||A.Name!=B.Name||A.Flags!=B.Flags||A.Size!=B.Size||A.Offset!=B.Offset||A.Dim!=B.Dim)return false;}return true;
    }
    static bool GetterFieldSame(const FField& Before,UClass* Owner,UClass* ValueClass)
    {
        FObjectProperty* P=GetterField(Owner,Before.Name);
        return P&&P==Before.Property&&P->GetOwnerStruct()==Before.Owner&&P->GetClass()==Before.Kind&&P->GetPropertyFlags()==Before.Flags&&P->GetSize()==Before.Size&&P->GetOffset_ForInternal()==Before.Offset&&P->ArrayDim==Before.Dim&&!P->HasSetterOrGetter()&&P->PropertyClass.IsResolved()&&P->PropertyClass==ValueClass;
    }
    bool GetterPropertiesCurrent()
    {
        if(!GetterPropertiesReady)return true;
        if(!Loaded(GetterSchema.Get())||GetterSchema->GetClass()!=UEdGraphSchema_K2::StaticClass()||!GetterFieldSame(GetterSkeletonField,HostSkeleton.Get(),GetterSkeletonType)||!GetterFieldSame(GetterCompiledField,HostClass.Get(),GetterCompiledType))return Stop(TEXT("component_replace_selected_getter_property_changed"),TEXT("Selected own skeleton/compiled field pointer, full descriptor or class role changed outside its explicit compiler boundary."));
        const FGuid SkeletonGuid=HostSkeleton->FindBlueprintPropertyGuidFromName(FName(*SelectedName)),CompiledGuid=HostClass->FindBlueprintPropertyGuidFromName(FName(*SelectedName));
        for(const auto& G:GraphRows)for(const auto& N:G.Nodes)if(N.SelectedGetter)
        {
            if((!NativeScene&&!CrossNativeOld&&(N.Reference.GetMemberGuid()!=SkeletonGuid||N.Reference.GetMemberGuid()!=CompiledGuid))||!G.Graph->Schema.GetGCPtr().IsResolved()||G.Graph->Schema.GetGCPtr()!=UEdGraphSchema_K2::StaticClass()||N.Node->GetClass()!=UK2Node_VariableGet::StaticClass()||!static_cast<UK2Node_VariableGet*>(N.Node.Get())->IsNodePure())return Stop(TEXT("component_replace_selected_getter_role_changed"),TEXT("Exact own pure getter/schema/member GUID roles changed; SCS GUID is separately preserved and is not substituted for the member GUID."));
        }
        return true;
    }
    bool AdoptGetterProperties(bool FullCompile)
    {
        if(!GetterPropertiesReady)return true;
        FField Next[2]={GetterSkeletonField,GetterCompiledField};UClass* Owners[2]={HostSkeleton.Get(),HostClass.Get()};UClass* Types[2]={NewClass.Get(),FullCompile?NewClass.Get():GetterCompiledType};
        for(int32 I=0;I<2;++I)
        {
            FObjectProperty* P=GetterField(Owners[I],FName(*SelectedName));const auto& Before=Next[I];
            if(!P||P->GetClass()!=FObjectProperty::StaticClass()||P->GetOwnerClass()!=Owners[I]||P->GetPropertyFlags()!=Before.Flags||P->GetSize()!=Before.Size||P->GetOffset_ForInternal()!=Before.Offset||P->ArrayDim!=Before.Dim||P->HasSetterOrGetter()||!P->PropertyClass.IsResolved()||P->PropertyClass!=Types[I])return Stop(TEXT("component_replace_selected_getter_property_transition_failed"),TEXT("Explicit skeleton/full compiler return did not leave the exact selected own object property descriptor and expected old/new component type."));
            Next[I].Property=P;Next[I].Owner=Owners[I];
        }
        GetterSkeletonField=Next[0];GetterCompiledField=Next[1];GetterSkeletonType=Types[0];GetterCompiledType=Types[1];return true;
    }
    bool AdoptGetterPinsOnly(UEdGraphNode* Reconstructed)
    {
        if(NativeScene)return NativeAdoptPins(*this,false);
        if(!Current(false,true))return false;
        for(const auto& G:GraphRows)if(G.Graph->GetOutermost()==Host->GetPackage())for(const auto& N:G.Nodes)if(N.Node.Get()!=Reconstructed&&N.Node->Pins!=N.Pins)return Stop(TEXT("component_replace_selected_getter_peer_pins_changed"),TEXT("Getter reconstruction changed a nonselected physical pin inventory; no adoption or next SDK."));
        int32 Replaced=0;
        for(auto& G:GraphRows)if(G.Graph->GetOutermost()==Host->GetPackage())
        {
            TMap<UEdGraphPin*,UEdGraphPin*> Now;for(const auto& N:G.Nodes)for(int32 I=0;I<N.PinRows.Num();++I)Now.Add(N.PinRows[I].Pin,N.Node->Pins[I]);
            // Retired pin addresses below are opaque map keys only. Strict semantic/ID/link verification above used current pins.
            for(auto& N:G.Nodes){for(auto& P:N.PinRows){UEdGraphPin* Q=Now.FindRef(P.Pin);Replaced+=Q!=P.Pin;P.Pin=Q;for(auto*& Link:P.Links)Link=Now.FindRef(Link);for(auto*& Child:P.Children)Child=Now.FindRef(Child);if(P.Parent)P.Parent=Now.FindRef(P.Parent);}N.Pins=N.Node->Pins;}
        }
        R.Data->SetNumberField(TEXT("selected_getter_pin_pointer_replacements"),R.Data->GetNumberField(TEXT("selected_getter_pin_pointer_replacements"))+Replaced);R.Data->SetBoolField(TEXT("selected_getter_semantic_links_verified"),true);return Current();
    }
    bool NativeEmptyCurrent()
    {
        if(!NativeEmptyMetadataReady)return true;
        if(NativeEmptyDelegates.Num()!=2)return Stop(TEXT("component_replace_generated_native_delegate_metadata_changed"),TEXT("Exactly two prepared native sparse delegate roles are required."));
        constexpr EPropertyFlags ExpectedFlags=CPF_NativeAccessSpecifierPublic|CPF_NoDestructor|CPF_BlueprintAssignable|CPF_InstancedReference;
        for(const auto& A:NativeEmptyDelegates)
        {
            FMulticastSparseDelegateProperty* P=nullptr;
            // Exact owner-only field traversal does not follow a tracked superclass pointer.
            for(TFieldIterator<FProperty> It(UActorComponent::StaticClass(),EFieldIteratorFlags::ExcludeSuper);It;++It)if(It->GetFName()==A.Name){P=CastField<FMulticastSparseDelegateProperty>(*It);break;}
            USparseDelegateFunction* F=A.Signature.Get();
            if(!P||P!=A.Property||P->GetClass()!=FMulticastSparseDelegateProperty::StaticClass()||P->GetOwnerClass()!=UActorComponent::StaticClass()||P->GetPropertyFlags()!=ExpectedFlags||P->ArrayDim!=1||P->GetSize()!=A.Size||P->GetOffset_ForInternal()!=A.Offset||P->HasSetterOrGetter()
                ||!P->SignatureFunction.IsResolved()||P->SignatureFunction!=F||!Loaded(F)||F->GetClass()!=USparseDelegateFunction::StaticClass()||F->OwningClassName!=TEXT("ActorComponent")||F->DelegateName!=A.Name||F->FunctionFlags!=A.FunctionFlags||F->NumParms!=A.NumParms||F->ParmsSize!=A.ParmsSize)return Stop(TEXT("component_replace_generated_native_delegate_metadata_changed"),TEXT("Exact native sparse property/layout/resolved held signature role changed; no delegate getter or next writer."));
        }
        if(NativeEmptyGuardReady)
        {
            for(UActorComponent* O:{OldCDO.Get(),NewCDO.Get(),OldTemplate.Get(),NewTemplate.Get()})if(O)
            {
                if(!Loaded(O))return Stop(TEXT("component_replace_generated_native_delegate_owner_changed"),TEXT("Held native sparse delegate owner is not loaded."));
                // Public typed GetShared uses the native member offset and a storage shared copy only; no tracked SignatureFunction read.
                const auto Activated=O->OnComponentActivated.GetShared();const auto Deactivated=O->OnComponentDeactivated.GetShared();
                if((Activated.IsValid()&&Activated->IsBound())||(Deactivated.IsValid()&&Deactivated->IsBound()))return Stop(TEXT("component_replace_generated_native_delegate_became_nonempty"),TEXT("A selected/source native sparse delegate invocation list became nonempty; expired entries are not discarded and no next writer is dispatched."));
            }
        }
        return true;
    }
    // Native scene inheritance terminates at the exact compiled CoreUObject root;
    // keep the Engine-only Native predicate unchanged for all existing profiles.
    static bool CrossNativeSourceClass(UClass* K)
    {
        return Native(K)||(K==UObject::StaticClass()&&Loaded(K)&&K->HasAnyClassFlags(CLASS_Native)
            &&!K->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists)&&K->ClassGeneratedBy.IsResolved()&&!K->ClassGeneratedBy.Get()
            &&K->GetOutermost()->GetName()==TEXT("/Script/CoreUObject")&&!K->GetOutermost()->IsDirty()&&K->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn));
    }
    bool CrossOldCurrent()
    {
        if(!CrossNativeOld)return true;
        if(CrossNativeClasses.IsEmpty()||CrossNativeClasses.Num()!=CrossNativeDefaults.Num()||CrossNativeClasses.Num()!=CrossNativeSupers.Num())return Stop(TEXT("component_replace_cross_source_changed"),TEXT("Native source hierarchy role inventory changed."));
        for(int32 I=0;I<CrossNativeClasses.Num();++I){auto* K=CrossNativeClasses[I].Get();auto* D=CrossNativeDefaults[I].Get();if(!CrossNativeSourceClass(K)||K->GetSuperClass()!=CrossNativeSupers[I]||!K->ClassDefaultObject.IsResolved()||K->ClassDefaultObject.Get()!=D||(D&&(!Loaded(D)||D->GetClass()!=K||D->GetOuter()!=K->GetOuter())))return Stop(TEXT("component_replace_cross_source_changed"),TEXT("Existing native source hierarchy/CDO role changed."));}
        if(!Loaded(CrossRoot.Get())||!Loaded(CrossDefaultRoot.Get())||!Loaded(CrossDefaultTemplate.Get())||SCS->GetDefaultSceneRootNode()!=CrossDefaultRoot.Get()
            ||!CrossDefaultRoot->ComponentClass.IsResolved()||CrossDefaultRoot->ComponentClass.Get()!=USceneComponent::StaticClass()||!CrossDefaultRoot->ComponentTemplate.IsResolved()||CrossDefaultRoot->ComponentTemplate!=CrossDefaultTemplate.Get()||CrossDefaultTemplate->GetClass()!=USceneComponent::StaticClass()||!CrossDefaultRoot->GetChildNodes().IsEmpty()
            ||!CrossDefaultRoot->ParentComponentOrVariableName.IsNone()||!CrossDefaultRoot->ParentComponentOwnerClassName.IsNone()||CrossDefaultRoot->bIsParentComponentNative
            ||Roots.Contains(CrossDefaultRoot.Get())||All.Contains(CrossDefaultRoot.Get())||!Roots.Contains(CrossRoot.Get())||CrossRoot.Get()==Selected.Get())return Stop(TEXT("component_replace_cross_root_changed"),TEXT("Existing non-default Scene root/default-root cache changed; no topology normalization is adopted."));
        if(CrossSparseReady)for(const auto& E:CrossSparse)
        {
            auto* P=FindFProperty<FMulticastSparseDelegateProperty>(E.Owner.Get(),E.Field.Name);auto* F=E.Signature.Get();
            if(!P||P!=E.Field.Property||P->GetOwnerStruct()!=E.Owner.Get()||P->GetClass()!=E.Field.Kind||P->GetPropertyFlags()!=E.Field.Flags||P->GetSize()!=E.Field.Size||P->GetOffset_ForInternal()!=E.Field.Offset||P->ArrayDim!=1||P->HasSetterOrGetter()||!P->SignatureFunction.IsResolved()||P->SignatureFunction.Get()!=F
                ||!Loaded(F)||F->GetClass()!=USparseDelegateFunction::StaticClass()||F->FunctionFlags!=E.Flags||F->OwningClassName!=E.OwnerName||F->DelegateName!=E.DelegateName)return Stop(TEXT("component_replace_cross_delegate_changed"),TEXT("Native Scene/Primitive sparse property/current held signature role changed before getter."));
            if(NativeEmptyGuardReady)for(UActorComponent* O:{OldCDO.Get(),OldTemplate.Get()}){if(!Loaded(O))return Stop(TEXT("component_replace_cross_delegate_owner_changed"),TEXT("Held old native sparse delegate owner is unavailable."));const auto* D=P->GetMulticastDelegate(P->ContainerPtrToValuePtr<void>(O));const bool Empty=!D||!D->IsBound();if(!Empty)return Stop(TEXT("component_replace_cross_delegate_became_bound"),TEXT("Old native Scene/Primitive invocation list changed; no compaction/invocation/propagation or next writer."));}
        }
        return true;
    }
    TArray<TSharedPtr<FJsonValue>> ScsLabels(const TArray<USCS_Node*>& List) const
    {
        TArray<TSharedPtr<FJsonValue>> Out;for(auto* N:List){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("pointer"),PointerLabel(N));const auto* Captured=Nodes.FindByPredicate([&](const FScsNode& E){return E.Node.Get()==N;});J->SetBoolField(TEXT("captured_role"),Captured!=nullptr);J->SetStringField(TEXT("captured_name"),Captured?Captured->Name.ToString():FString());J->SetStringField(TEXT("captured_guid"),Captured?Captured->Guid.ToString():FString());Out.Add(MakeShared<FJsonValueObject>(J));}return Out;
    }
    TArray<USCS_Node*> ExpectedRoots() const
    {auto Expected=Roots;if(CrossPromoted)Expected.Add(Selected.Get());return Expected;}
    TArray<USCS_Node*> ExpectedChildren(const FScsNode& Row) const
    {auto Expected=Row.Children;if(CrossPromoted&&Row.Node.Get()==CrossParent.Get())Expected.RemoveSingle(Selected.Get());return Expected;}
    bool ScsMismatch(const TCHAR* Field,const TArray<USCS_Node*>& Expected,const TArray<USCS_Node*>& Actual)
    {
        if(!R.Data->HasField(TEXT("scs_topology_first_mismatch")))
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("object"),HostPath);J->SetStringField(TEXT("property"),Field);J->SetArrayField(TEXT("expected"),ScsLabels(Expected));J->SetArrayField(TEXT("actual"),ScsLabels(Actual));J->SetArrayField(TEXT("captured_all_nodes"),ScsLabels(All));J->SetArrayField(TEXT("captured_roots"),ScsLabels(Roots));J->SetArrayField(TEXT("actual_all_nodes"),ScsLabels(SCS->GetAllNodes()));J->SetArrayField(TEXT("actual_roots"),ScsLabels(SCS->GetRootNodes()));J->SetBoolField(TEXT("diagnostic_read_scope_held_scs_and_node_arrays_only"),true);
            TArray<USCS_Node*> Parents;for(const auto& N:Nodes)if(N.Node->GetChildNodes().Contains(Selected.Get()))Parents.Add(N.Node.Get());TArray<USCS_Node*> ExpectedParents;if(CrossNativeOld&&!CrossPromoted)ExpectedParents.Add(CrossParent.Get());J->SetArrayField(TEXT("expected_selected_local_parents"),ScsLabels(ExpectedParents));J->SetArrayField(TEXT("actual_selected_local_parents"),ScsLabels(Parents));J->SetBoolField(TEXT("new_unknown_node_dereference_performed"),false);R.Data->SetObjectField(TEXT("scs_topology_first_mismatch"),J);
        }
        return Stop(TEXT("component_replace_generated_scs_changed"),TEXT("Current SCS topology differs from the exact captured or admitted selected-leaf compiler transition; actual prefix retained."));
    }
    bool AdoptCrossCompilerTopology()
    {
        if(!CrossNativeOld||!Assigned||CrossPromoted)return true;
        // BlueprintCompilationManager's full-compile fixup moves non-Scene
        // nodes out of scene children into the root set. No other node/layout
        // change is inferred from this successful compiler return.
        const auto ActualAll=SCS->GetAllNodes();const auto ActualRoots=SCS->GetRootNodes();
        if(ActualAll!=All)return ScsMismatch(TEXT("AllNodes"),All,ActualAll);
        if(ActualRoots==Roots)return true;
        auto PromotedRoots=Roots;PromotedRoots.Add(Selected.Get());if(ActualRoots!=PromotedRoots)return ScsMismatch(TEXT("RootNodes"),PromotedRoots,ActualRoots);
        if(RequireSceneCompatibility||!CrossParent.IsValid()||!NewClass.IsValid()||NewClass->IsChildOf(USceneComponent::StaticClass())||!Selected->GetChildNodes().IsEmpty())return Stop(TEXT("component_replace_cross_compiler_scope_changed"),TEXT("Only the admitted assigned non-Scene selected leaf can move to the root set."));
        int32 ParentMatches=0;
        for(const auto& Row:Nodes)
        {
            auto* N=Row.Node.Get();if(!Loaded(N)||N->GetOuter()!=SCS.Get()||N->GetVariableName()!=Row.Name||N->VariableGuid!=Row.Guid||!N->ComponentClass.IsResolved()||N->ComponentClass!=(N==Selected.Get()?NewClass.Get():Row.Class.Get())||!N->ComponentTemplate.IsResolved()||N->ComponentTemplate!=(N==Selected.Get()?NewTemplate.Get():Row.Template.Get()))return Stop(TEXT("component_replace_cross_compiler_node_changed"),TEXT("Compiler topology return changed a node identity/class/template role."));
            auto Children=Row.Children;if(N==CrossParent.Get()){if(Children.RemoveSingle(Selected.Get())!=1)return Stop(TEXT("component_replace_cross_compiler_parent_changed"),TEXT("Captured exact local parent must contain the selected leaf once."));++ParentMatches;}
            if(N->GetChildNodes()!=Children)return ScsMismatch(TEXT("ChildNodes_after_full_compile"),Children,N->GetChildNodes());
        }
        if(ParentMatches!=1||!Selected->ParentComponentOrVariableName.IsNone()||!Selected->ParentComponentOwnerClassName.IsNone()||Selected->bIsParentComponentNative)return Stop(TEXT("component_replace_cross_compiler_parent_changed"),TEXT("Selected promoted leaf must retain empty inherited/native parent roles."));
        CrossPromoted=true;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("boundary"),TEXT("successful_full_compile"));J->SetArrayField(TEXT("original_roots"),ScsLabels(Roots));J->SetArrayField(TEXT("actual_roots"),ScsLabels(ActualRoots));J->SetArrayField(TEXT("preserved_all_nodes"),ScsLabels(ActualAll));J->SetBoolField(TEXT("selected_leaf_only_promoted"),true);J->SetBoolField(TEXT("other_topology_adopted"),false);R.Data->SetObjectField(TEXT("cross_scene_leaf_compiler_normalization"),J);return true;
    }
    bool Current(bool Rebinding=false,bool CompilerRefreshedPins=false,bool AllowCrossCompilerTopology=false)
    {
        if(NativeScene)return NativeSceneCurrent(*this,Rebinding,CompilerRefreshedPins);
        if(Stopped)return false;
        if(!Globals()||GEditor!=Editor.Get()||!Loaded(Editor.Get())||GUndo!=Undo)return Stop(TEXT("component_replace_generated_context_changed"),TEXT("Editor/game-thread/Undo context changed; no next SDK, rollback or retry."));
        for(const auto& A:Roles)
        {
            UObject* O=A.Object.Get();if(Rebinding&&(O==HostClass.Get()||O==HostSkeleton.Get()||O==HostCDO.Get()))continue;
            if(!Loaded(O)||!Loaded(A.Class.Get())||!Loaded(A.Package.Get())||(A.Outer.IsValid()&&!Loaded(A.Outer.Get()))||O->GetClass()!=A.Class.Get()||O->GetOuter()!=A.Outer.Get()||O->GetOutermost()!=A.Package.Get()||O->GetPathName()!=A.Path||O->GetFlags()!=A.Flags
                ||A.Package->GetPackageFlags()!=(A.Package.Get()==Host->GetPackage()?HostPackageFlags:A.PackageFlags)||(A.IsClass&&(static_cast<UClass*>(O)->GetClassFlags()!=A.ClassFlags||static_cast<UClass*>(O)->GetStructureSize()!=A.ClassSize)))return Stop(TEXT("component_replace_generated_identity_changed"),TEXT("Held object/class/outer/path/flags/package identity changed."));
        }
        if(!Loaded(Host.Get())||!Loaded(SCS.Get())||!Host->ParentClass.GetGCPtr().IsResolved()||Host->ParentClass.GetGCPtr()!=AActor::StaticClass()
            ||!Host->SimpleConstructionScript.IsResolved()||Host->SimpleConstructionScript!=SCS.Get()||Host->GetPathName()!=HostPath||Host->GetPackage()->IsDirty()!=Dirty
            ||Host->bBeingCompiled||Host->bQueuedForCompilation||Host->bIsRegeneratingOnLoad||Host->bSuppressStructurallyModified||(!Rebinding&&Host->Status!=Status))return Stop(TEXT("component_replace_generated_host_changed"),TEXT("Current direct-native Actor host scope changed."));
        if(!Rebinding&&(!Host->GeneratedClass.GetGCPtr().IsResolved()||Host->GeneratedClass.GetGCPtr()!=HostClass.Get()||!Host->SkeletonGeneratedClass.GetGCPtr().IsResolved()||Host->SkeletonGeneratedClass.GetGCPtr()!=HostSkeleton.Get()))return Stop(TEXT("component_replace_generated_host_class_changed"),TEXT("Current host generated/skeleton classes changed outside the explicit compiler return."));
        if(!Rebinding)
        {
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            const auto HostDefault=HostClass->ClassDefaultObject;
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
            if(!HostDefault.IsResolved()||HostDefault!=HostCDO.Get()||HostCDO->GetClass()!=HostClass.Get())return Stop(TEXT("component_replace_generated_host_cdo_changed"),TEXT("Current host CDO shallow handle/class changed outside the explicit compiler return."));
        }
        if(!CrossOldCurrent())return false;
        for(int32 I=0;I<2;++I)
        {
            if(CrossNativeOld&&I==0)continue;
            UBlueprint* B=I?NewBP.Get():OldBP.Get();UClass* C=I?NewClass.Get():OldClass.Get();UClass* K=I?NewSkeleton.Get():OldSkeleton.Get();UObject* D=I?NewCDO.Get():OldCDO.Get();
            if(!Loaded(B)||!Loaded(C)||!Loaded(K)||!Loaded(D)||B->GetPackage()->IsDirty()||!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.GetGCPtr()!=UActorComponent::StaticClass()
                ||!B->GeneratedClass.GetGCPtr().IsResolved()||B->GeneratedClass.GetGCPtr()!=C||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||B->SkeletonGeneratedClass.GetGCPtr()!=K
                ||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy!=B||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||B->bSuppressStructurallyModified
                ||B->Status!=(I?NewStatus:OldStatus)||!K->ClassGeneratedBy.IsResolved()||K->ClassGeneratedBy!=B)return Stop(TEXT("component_replace_generated_source_changed"),TEXT("Saved-clean direct ActorComponent source BP/class/skeleton roles changed."));
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            const auto Handle=C->ClassDefaultObject;
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
            if(!Handle.IsResolved()||Handle!=D||D->GetClass()!=C)return Stop(TEXT("component_replace_generated_source_cdo_changed"),TEXT("Existing held source CDO handle/class changed; no CDO creation."));
        }
        if(Rebinding&&AllowCrossCompilerTopology&&!AdoptCrossCompilerTopology())return false;
        const auto ActualAll=SCS->GetAllNodes();if(ActualAll!=All)return ScsMismatch(TEXT("AllNodes"),All,ActualAll);const auto ActualRoots=SCS->GetRootNodes();const auto WantedRoots=ExpectedRoots();if(ActualRoots!=WantedRoots)return ScsMismatch(TEXT("RootNodes"),WantedRoots,ActualRoots);
        for(const auto& A:Nodes)
        {
            USCS_Node* N=A.Node.Get();UClass* C=N==Selected.Get()&&Assigned?NewClass.Get():A.Class.Get();UActorComponent* T=N==Selected.Get()&&Assigned?NewTemplate.Get():A.Template.Get();
            if(!Loaded(N)||N->GetOuter()!=SCS.Get()||N->GetVariableName()!=A.Name||N->VariableGuid!=A.Guid||!N->ComponentClass.IsResolved()||N->ComponentClass!=C||!N->ComponentTemplate.IsResolved()||N->ComponentTemplate!=T)return Stop(TEXT("component_replace_generated_node_changed"),TEXT("Own SCS node/name/GUID/class/template scope changed."));
            const auto Children=ExpectedChildren(A);if(N->GetChildNodes()!=Children)return ScsMismatch(TEXT("ChildNodes"),Children,N->GetChildNodes());
        }
        if(GraphsReady)
        {
            if(!SameArrays(HostArrays,Arrays(Host.Get()))||(!CrossNativeOld&&!SameArrays(OldArrays,Arrays(OldBP.Get())))||!SameArrays(NewArrays,Arrays(NewBP.Get())))return Stop(TEXT("component_replace_generated_graphs_changed"),TEXT("Stored graph arrays changed."));
            for(const auto& G:GraphRows)
            {
                if(!Loaded(G.Graph.Get())||G.Graph->GraphGuid!=G.Guid||G.Graph->Nodes.Num()!=G.Nodes.Num()||!G.Graph->SubGraphs.IsEmpty())return Stop(TEXT("component_replace_generated_graph_changed"),TEXT("Current flat graph/node inventory changed."));
                const bool ReconstructHostPins=CompilerRefreshedPins&&G.Graph->GetOutermost()==Host->GetPackage();
                for(int32 I=0;I<G.Nodes.Num();++I)
                {
                    const auto& N=G.Nodes[I];
                    if(!G.Graph->Nodes[I].IsResolved())return GraphMismatch(G,I,INDEX_NONE,TEXT("node_handle_resolved"),TEXT("true"),TEXT("false"),TEXT("component_replace_generated_graph_node_changed"),TEXT("Current graph node/GUID/pin inventory changed."));
                    if(G.Graph->Nodes[I]!=N.Node.Get())return GraphMismatch(G,I,INDEX_NONE,TEXT("node_pointer"),PointerLabel(N.Node.Get()),TEXT("different resolved handle"),TEXT("component_replace_generated_graph_node_changed"),TEXT("Current graph node/GUID/pin inventory changed."));
                    if(!Loaded(N.Node.Get()))return GraphMismatch(G,I,INDEX_NONE,TEXT("held_node_loaded"),TEXT("true"),TEXT("false"),TEXT("component_replace_generated_graph_node_changed"),TEXT("Current graph node/GUID/pin inventory changed."));
                    if(N.Node->NodeGuid!=N.Guid)return GraphMismatch(G,I,INDEX_NONE,TEXT("node_guid"),N.Guid.ToString(),N.Node->NodeGuid.ToString(),TEXT("component_replace_generated_graph_node_changed"),TEXT("Current graph node/GUID/pin inventory changed."));
                    if(!ReconstructHostPins&&N.Node->Pins!=N.Pins)
                    {
                        if(N.Node->Pins.Num()!=N.Pins.Num())return GraphMismatch(G,I,INDEX_NONE,TEXT("physical_pin_count"),LexToString(N.Pins.Num()),LexToString(N.Node->Pins.Num()),TEXT("component_replace_generated_graph_node_changed"),TEXT("Current graph node/GUID/pin inventory changed."));
                        for(int32 J=0;J<N.Pins.Num();++J)if(N.Node->Pins[J]!=N.Pins[J])return GraphMismatch(G,I,J,TEXT("physical_pin_pointer"),PointerLabel(N.Pins[J]),PointerLabel(N.Node->Pins[J]),TEXT("component_replace_generated_graph_node_changed"),TEXT("Current graph node/GUID/pin inventory changed."));
                    }
                    if(N.Variable){const auto* V=CastChecked<UK2Node_Variable>(N.Node.Get());if(!V->VariableReference.IsSameReference(N.Reference)||V->VariableReference.IsDeprecated()!=N.Deprecated)return Stop(TEXT("component_replace_generated_graph_member_changed"),TEXT("Existing member reference changed."));}
                    if(!ReconstructHostPins)for(const auto& P:N.PinRows)if(!PinSame(N,P))return Stop(TEXT("component_replace_generated_graph_pin_changed"),TEXT("Existing physical pin/type/default/link/whole-pin state changed."));
                }
                if(ReconstructHostPins&&!CompilerGraphPinsSame(G))return false;
            }
        }
        // These public scalar accessors are callback-free; Fresh() must not be called here because it calls Current().
        for(UActorComponent* T:{OldTemplate.Get(),NewTemplate.Get()})if(T&&(!Loaded(T)||T->IsRegistered()||T->IsPreRegistering()||T->IsActive()||T->HasBegunPlay()||T->IsRenderStateCreated()||T->IsPhysicsStateCreated()))return Stop(TEXT("component_replace_generated_template_became_live"),TEXT("Held selected old/new authoring template acquired live registration/active/begun/render/physics state; no next SDK."));
        if(!Rebinding&&!GetterPropertiesCurrent())return false;return NativeEmptyCurrent();
    }
    static TArray<FField> Fields(UStruct* Type)
    {
        TArray<FField> Out;for(TFieldIterator<FProperty> It(Type,EFieldIteratorFlags::IncludeSuper);It;++It){FProperty* P=*It;Out.Add({P,P->GetOwnerStruct(),P->GetClass(),P->GetFName(),P->GetPropertyFlags(),P->GetSize(),P->GetOffset_ForInternal(),P->ArrayDim});}return Out;
    }
    static bool FieldsSame(UStruct* Type,const TArray<FField>& Before)
    {
        const auto Now=Fields(Type);if(Now.Num()!=Before.Num())return false;for(int32 I=0;I<Now.Num();++I){const auto& A=Before[I];const auto& B=Now[I];if(A.Property!=B.Property||A.Owner!=B.Owner||A.Kind!=B.Kind||A.Name!=B.Name||A.Flags!=B.Flags||A.Size!=B.Size||A.Offset!=B.Offset||A.Dim!=B.Dim)return false;}return true;
    }
    bool Props(UObject* O,const TSet<FName>& Skip,const TSharedRef<FJsonObject>& Out)
    {
        if(!Current()||!Loaded(O))return false;TStrongObjectPtr<UObject> Hold(O);TStrongObjectPtr<UClass> Type(O->GetClass());const auto Layout=Fields(Type.Get());if(Layout.Num()>MaxProperties)return Stop(TEXT("component_replace_generated_property_bound"),TEXT("Property inventory exceeds the finite bound."));
        int32 Chars=0;for(const auto& F:Layout)
        {
            if(Skip.Contains(F.Name))continue;if(F.Dim<1||F.Dim>64)return Stop(TEXT("component_replace_generated_property_layout"),TEXT("Unsupported static property dimension."));
            const FString Cpp=F.Property->GetCPPType();if(!Current())return false;if(!FieldsSame(Type.Get(),Layout))return Stop(TEXT("component_replace_generated_property_layout_changed"),TEXT("Current property layout changed after type export."));
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("type"),Cpp);Row->SetStringField(TEXT("owner"),F.Owner->GetPathName());Row->SetStringField(TEXT("flags"),LexToString(static_cast<uint64>(F.Flags)));Row->SetNumberField(TEXT("size"),F.Size);Row->SetNumberField(TEXT("offset"),F.Offset);TArray<TSharedPtr<FJsonValue>> Values;
            for(int32 I=0;I<F.Dim;++I)
            {
                FString Value;const void* Address=F.Property->ContainerPtrToValuePtr<void>(O,I);const bool Exported=F.Property->ExportText_Direct(Value,Address,Address,O,PPF_IncludeTransient);if(!Current())return false;
                if(!FieldsSame(Type.Get(),Layout))return Stop(TEXT("component_replace_generated_property_layout_changed"),TEXT("Current property layout changed after value export; no stale property is reused."));
                if(!Exported||(Chars+=Value.Len())>MaxText||!CleanText(Value))return Stop(TEXT("component_replace_generated_property_unobservable"),TEXT("Bounded clean property export is incomplete."));Values.Add(MakeShared<FJsonValueString>(Value));
            }
            Row->SetArrayField(TEXT("values"),Values);Out->SetObjectField(F.Name.ToString(),Row);
        }
        return true;
    }
};

template<typename T> UObject* FromHandle(const TObjectPtr<T>& H)
{if(!H.IsResolved()||H==nullptr)return nullptr;UObject* O=FindObject<UObject>(nullptr,*H.GetPathName());return H==O?O:nullptr;}
UClass* ClassHandle(const TObjectPtr<UClass>& H)
{if(!H.IsResolved()||H==nullptr)return nullptr;UObject* O=FindObject<UObject>(nullptr,*H.GetPathName());return H==O&&Loaded(O)&&O->GetClass()==UBlueprintGeneratedClass::StaticClass()?static_cast<UClass*>(O):nullptr;}
UObject* ExistingCDO(UClass* C)
{
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    const auto H=C->ClassDefaultObject;
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
    return FromHandle(H);
}
bool Fresh(FScope& Scope,UActorComponent* C)
{
    if(!Scope.Current()||!Loaded(C))return false;AActor* Owner=C->GetOwner();if(!Scope.Current())return false;UWorld* World=C->GetWorld();if(!Scope.Current())return false;
    return !Owner&&!World&&!C->IsRegistered()&&!C->IsPreRegistering()&&!C->IsActive()&&!C->HasBegunPlay()&&!C->IsRenderStateCreated()&&!C->IsPhysicsStateCreated();
}
bool FreezeGraphs(FScope& C,UBlueprint* B)
{
    TArray<UEdGraph*> Graphs;B->GetAllGraphs(Graphs);if(!C.Current())return false;if(Graphs.Num()>MaxGraphs)return Fail(C.R,TEXT("component_replace_generated_graph_bound"),TEXT("Stored graph bound exceeded."));
    for(UEdGraph* G:Graphs)
    {
        if(!Loaded(G)||G->GetOutermost()!=B->GetPackage()||!G->GraphGuid.IsValid()||!G->SubGraphs.IsEmpty()||C.Role(G)==INDEX_NONE)return Fail(C.R,TEXT("component_replace_generated_graph_scope"),TEXT("Only finite flat stored graphs are admitted."));
        FGraph Row;Row.Graph.Reset(G);Row.Guid=G->GraphGuid;
        for(const auto& H:G->Nodes)
        {
            if(!H.IsResolved())return Fail(C.R,TEXT("component_replace_generated_graph_scope"),TEXT("Stored node handles must already be resolved."));
            UObject* O=FindObject<UObject>(nullptr,*H.GetPathName());if(!C.Current())return false;
            if(H!=O||!Loaded(O)||O->GetOuter()!=G)return Fail(C.R,TEXT("component_replace_generated_graph_scope"),TEXT("Current graph node lookup differs from the resolved handle."));
            UEdGraphNode* N=Cast<UEdGraphNode>(O);if(!C.Current())return false;if(!Loaded(N)||!N->NodeGuid.IsValid()||C.Role(N)==INDEX_NONE||Row.Nodes.Num()>4096)return Fail(C.R,TEXT("component_replace_generated_graph_scope"),TEXT("Graph node/GUID/count is unsupported."));
            FGraphNode Node;Node.Node.Reset(N);Node.Guid=N->NodeGuid;Node.Pins=N->Pins;
            if(const auto* V=Cast<UK2Node_Variable>(N))
            {
                Node.Variable=true;Node.Reference=V->VariableReference;Node.Deprecated=V->VariableReference.IsDeprecated();
                if(B==C.Host.Get()&&(Node.Reference.GetMemberGuid()==C.Guid||(Node.Reference.IsSelfContext()&&Node.Reference.GetMemberName()==FName(*C.SelectedName))))
                {
                    if(N->GetClass()!=UK2Node_VariableGet::StaticClass()||!Node.Reference.IsSelfContext()||Node.Reference.IsLocalScope()||Node.Reference.GetMemberName()!=FName(*C.SelectedName)||Node.Deprecated||!static_cast<UK2Node_VariableGet*>(N)->IsNodePure())
                    {C.R.Data->SetStringField(TEXT("selected_graph_reference"),N->GetPathName());return Fail(C.R,TEXT("component_replace_graph_reference_unsupported"),TEXT("Only selected own pure VariableGet references are admitted; setters, external/local/validated references remain refused before Modify."));}
                    Node.SelectedGetter=true;
                }
            }
            if(const auto* E=Cast<UK2Node_ComponentBoundEvent>(N);E&&B==C.Host.Get()&&E->GetComponentPropertyName()==FName(*C.SelectedName))
            {if(!C.NativeScene)return Fail(C.R,TEXT("component_replace_graph_reference_unsupported"),TEXT("Selected component-bound event migration is incomplete on generated replacement."));Node.SelectedOverlap=true;}
            for(UEdGraphPin* P:N->Pins)
            {
                if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||!P->DefaultObject.IsResolved())return Fail(C.R,TEXT("component_replace_generated_graph_pin_scope"),TEXT("Physical pin/owner/ID/default handle is incomplete."));
                if(B==C.Host.Get()&&P->DefaultObject==C.OldTemplate.Get())return Fail(C.R,TEXT("component_replace_graph_reference_unsupported"),TEXT("Selected template literals require independent migration; refused before Modify."));
                FPin Pin;Pin.Pin=P;Pin.Id=P->PinId;Pin.Name=P->PinName;Pin.Direction=P->Direction;Pin.Type=P->PinType;Pin.Default=P->DefaultValue;Pin.Auto=P->AutogeneratedDefaultValue;Pin.Text=P->DefaultTextValue;Pin.Object=P->DefaultObject;
                Pin.Links=P->LinkedTo;Pin.Children=P->SubPins;Pin.Parent=P->ParentPin;Pin.Hidden=P->bHidden;Pin.Orphan=P->bOrphanedPin;Pin.Advanced=P->bAdvancedView;Pin.NotConnectable=P->bNotConnectable;Pin.DefaultReadOnly=P->bDefaultValueIsReadOnly;Pin.DefaultIgnored=P->bDefaultValueIsIgnored;Pin.SaveOrphan=P->ShouldSavePinIfOrphaned();if(Node.SelectedGetter){Pin.Deprecated=P->bDeprecated;Pin.MutableRef=P->bDisplayAsMutableRef;Pin.FriendlyName=P->bAllowFriendlyName;}
                for(UObject* Type:{P->PinType.PinSubCategoryObject.Get(),P->PinType.PinValueType.TerminalSubCategoryObject.Get()})if(Type&&C.Role(Type)==INDEX_NONE)return Fail(C.R,TEXT("component_replace_generated_pin_type_unavailable"),TEXT("Referenced physical pin type object is unavailable."));
                Node.PinRows.Add(MoveTemp(Pin));
            }
            Row.Nodes.Add(MoveTemp(Node));
        }
        TSet<UEdGraphPin*> OwnedPins;for(const auto& N:Row.Nodes)for(auto* P:N.Pins)OwnedPins.Add(P);
        if(OwnedPins.Num()>MaxPins)return Fail(C.R,TEXT("component_replace_generated_graph_pin_bound"),TEXT("Stored physical pin bound exceeded."));
        for(const auto& N:Row.Nodes)for(const auto& P:N.PinRows)
        {
            for(auto* Peer:P.Links)if(!Peer||!OwnedPins.Contains(Peer)||!Peer->LinkedTo.Contains(P.Pin)||Peer->Direction==P.Direction)return Fail(C.R,TEXT("component_replace_generated_graph_link_scope"),TEXT("Stored links must be reciprocal within the admitted graph inventory."));
            if(P.Parent&&!OwnedPins.Contains(P.Parent))return Fail(C.R,TEXT("component_replace_generated_graph_pin_scope"),TEXT("Split parent pin is outside the stored inventory."));for(auto* Child:P.Children)if(!OwnedPins.Contains(Child))return Fail(C.R,TEXT("component_replace_generated_graph_pin_scope"),TEXT("Split child pin is outside the stored inventory."));
        }
        C.GraphRows.Add(MoveTemp(Row));
    }
    return true;
}
bool NoConsumers(FScope& C)
{
    TArray<UObject*> Objects;GetObjectsOfClass(C.HostClass.Get(),Objects,true,RF_NoFlags);if(!C.Current())return false;
    if(Objects.Num()>32768)return Fail(C.R,TEXT("component_replace_instance_scan_bound"),TEXT("Loaded host instance scan exceeds the finite bound."));
    for(UObject* O:Objects)if(Loaded(O)&&!O->IsTemplate())return Fail(C.R,TEXT("component_replace_live_instance_unsupported"),TEXT("Live host instances require propagation not covered by this template-only slice."));
    TArray<UClass*> Derived;GetDerivedClasses(C.HostClass.Get(),Derived,true);if(!C.Current())return false;
    for(UClass* D:Derived)if(D!=C.HostClass.Get()&&D!=C.HostSkeleton.Get())return Fail(C.R,TEXT("component_replace_derived_class_unsupported"),TEXT("Loaded host subclasses require separate propagation proof."));
    auto* Module=FModuleManager::GetModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry"));if(!Module)return Fail(C.R,TEXT("component_replace_derived_class_unverified"),TEXT("Existing AssetRegistry discovery is required; no module load fallback."));
    auto& Registry=Module->Get();const bool Loading=Registry.IsLoadingAssets();if(!C.Current())return false;if(Loading)return Fail(C.R,TEXT("component_replace_derived_class_unverified"),TEXT("AssetRegistry discovery is incomplete."));
    TArray<FTopLevelAssetPath> Roots={C.HostClass->GetClassPathName()};TSet<FTopLevelAssetPath> Excluded,Found;Registry.GetDerivedClassNames(Roots,Excluded,Found);if(!C.Current())return false;const bool LoadingAfter=Registry.IsLoadingAssets();if(!C.Current())return false;if(LoadingAfter)return Fail(C.R,TEXT("component_replace_derived_class_unverified"),TEXT("Registry discovery resumed during the derived-class query."));Found.Remove(C.HostClass->GetClassPathName());
    C.R.Data->SetNumberField(TEXT("discovered_host_derived_class_count"),Found.Num());if(!Found.IsEmpty())return Fail(C.R,TEXT("component_replace_derived_class_unsupported"),TEXT("Discovered unloaded host subclasses remain outside the finite propagation scope."));
    TArray<UObject*> Instances;C.OldTemplate->GetArchetypeInstances(Instances);if(!C.Current())return false;C.R.Data->SetNumberField(TEXT("selected_template_archetype_instance_count"),Instances.Num());
    if(!Instances.IsEmpty())return Fail(C.R,TEXT("component_replace_archetype_instances_unsupported"),TEXT("Selected archetype consumers require separate propagation proof."));return true;
}
bool Bind(FScope& C,const FString& BPPath,const FString& ComponentName,const FString& ClassPath)
{
    C.R.Data->SetStringField(TEXT("requested_blueprint_path"),BPPath);C.R.Data->SetStringField(TEXT("normalized_blueprint_object_path"),FString());
    if(!FScope::Globals()||!Name(ComponentName)||!AssetPath(BPPath)||!BPPath.StartsWith(TEXT("/Game/"))||ClassPath.Len()>=1024||!FPackageName::IsValidObjectPath(ClassPath)||!ClassPath.StartsWith(TEXT("/Game/"))||ClassPath.Contains(TEXT(":"))||!ClassPath.EndsWith(TEXT("_C"))||!CleanText(ClassPath))return Fail(C.R,TEXT("component_replace_generated_input_invalid"),TEXT("Exact loaded /Game Actor Blueprint, own name and generated BPC class object path required."));
    // Preserve the raw admission before the shared native package-to-object normalization; export/alias/loader inputs are not added.
    const FString ObjectPath=Path(BPPath);C.R.Data->SetStringField(TEXT("normalized_blueprint_object_path"),ObjectPath);
    if(!AssetPath(ObjectPath)||!ObjectPath.StartsWith(TEXT("/Game/")))return Fail(C.R,TEXT("component_replace_generated_input_invalid"),TEXT("Normalized target object path exceeds the existing bounded /Game object-path contract."));
    C.Editor.Reset(GEditor);C.Undo=GUndo;UObject* B=FindObject<UObject>(nullptr,*ObjectPath);if(!FScope::Globals()||GEditor!=C.Editor.Get()||GUndo!=C.Undo)return Fail(C.R,TEXT("component_replace_generated_context_invalid"),TEXT("Editor context changed during initial no-load lookup."));
    if(!Loaded(B)||B->GetClass()!=UBlueprint::StaticClass()||B->GetPathName()!=ObjectPath)return Fail(C.R,TEXT("component_replace_blueprint_unsupported"),TEXT("The exact target Blueprint must already be loaded; no implicit asset/class load."));
    C.Host.Reset(static_cast<UBlueprint*>(B));C.HostPath=ObjectPath;C.HostPackage=B->GetOutermost()->GetName();C.SelectedName=ComponentName;C.Slot=ComponentName+USimpleConstructionScript::ComponentTemplateNameSuffix;
    if(!C.Host->ParentClass.GetGCPtr().IsResolved()||C.Host->ParentClass.GetGCPtr()!=AActor::StaticClass()||C.Host->GetPackage()->IsDirty()||C.Host->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn|PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly|PKG_PlayInEditor)||C.Host->BlueprintType!=BPTYPE_Normal||C.Host->bBeingCompiled||C.Host->bQueuedForCompilation||C.Host->bIsRegeneratingOnLoad||C.Host->bSuppressStructurallyModified||(C.Host->Status!=BS_UpToDate&&C.Host->Status!=BS_UpToDateWithWarnings))return Fail(C.R,TEXT("component_replace_blueprint_unsupported"),TEXT("Saved-clean, compiled normal direct-native Actor host required."));
    C.HostClass.Reset(Cast<UBlueprintGeneratedClass>(ClassHandle(C.Host->GeneratedClass.GetGCPtr())));C.HostSkeleton.Reset(Cast<UBlueprintGeneratedClass>(ClassHandle(C.Host->SkeletonGeneratedClass.GetGCPtr())));
    if(!Loaded(C.HostClass.Get())||!Loaded(C.HostSkeleton.Get())||!C.Host->SimpleConstructionScript.IsResolved())return Fail(C.R,TEXT("component_replace_generated_host_class_unsupported"),TEXT("Current resolved host generated/skeleton/SCS roles required."));
    C.SCS.Reset(Cast<USimpleConstructionScript>(FromHandle(C.Host->SimpleConstructionScript)));C.HostCDO.Reset(ExistingCDO(C.HostClass.Get()));
    if(!Loaded(C.SCS.Get())||!Loaded(C.HostCDO.Get())||C.HostCDO->GetClass()!=C.HostClass.Get()||!C.HostCDO->HasAnyFlags(RF_ClassDefaultObject))return Fail(C.R,TEXT("component_replace_generated_host_cdo_unsupported"),TEXT("Existing host SCS/CDO roles must be present; no CDO creation."));
    C.All=C.SCS->GetAllNodes();C.Roots=C.SCS->GetRootNodes();if(C.All.Num()>MaxNodes)return Fail(C.R,TEXT("component_replace_generated_scs_bound"),TEXT("Own SCS node bound exceeded."));
    for(USCS_Node* N:C.All)
    {
        if(!Loaded(N)||N->GetOuter()!=C.SCS.Get()||!N->ComponentClass.IsResolved()||!N->ComponentTemplate.IsResolved()||!N->VariableGuid.IsValid())return Fail(C.R,TEXT("component_replace_generated_node_unsupported"),TEXT("Current own SCS node/class/template/GUID required."));
        UObject* T=FromHandle(N->ComponentTemplate);UObject* K=FindObject<UObject>(nullptr,*N->ComponentClass.GetPathName());
        if(!Loaded(T)||!Loaded(K)||N->ComponentClass!=K||T->GetClass()!=K||T->GetOuter()!=C.HostClass.Get()||!T->HasAnyFlags(RF_ArchetypeObject))return Fail(C.R,TEXT("component_replace_generated_node_unsupported"),TEXT("Current unique generated-class template ownership required."));
        FScsNode Row;Row.Node.Reset(N);Row.Class.Reset(static_cast<UClass*>(K));Row.Template.Reset(static_cast<UActorComponent*>(T));Row.Name=N->GetVariableName();Row.Guid=N->VariableGuid;Row.Children=N->GetChildNodes();
        for(const auto& Old:C.Nodes)if(Old.Name==Row.Name||Old.Guid==Row.Guid||Old.Template.Get()==T||Old.Node.Get()==N)return Fail(C.R,TEXT("component_replace_generated_node_ambiguous"),TEXT("Duplicate own component name/GUID/template/node is unsupported."));
        if(Row.Name.ToString()==ComponentName){C.Selected.Reset(N);C.Guid=Row.Guid;C.OldClass.Reset(Row.Class.Get());C.OldTemplate.Reset(Row.Template.Get());}
        C.Nodes.Add(MoveTemp(Row));
    }
    TMap<USCS_Node*,int32> Parents;TSet<USCS_Node*> RootSet,Reached;TArray<USCS_Node*> Queue;
    for(auto* Root:C.Roots){if(!C.All.Contains(Root)||RootSet.Contains(Root))return Fail(C.R,TEXT("component_replace_generated_scs_topology_unsupported"),TEXT("Unique local roots required."));RootSet.Add(Root);Queue.Add(Root);}
    for(const auto& N:C.Nodes)for(auto* Child:N.Children){if(!C.All.Contains(Child)||Child==N.Node.Get()||++Parents.FindOrAdd(Child)>1)return Fail(C.R,TEXT("component_replace_generated_scs_topology_unsupported"),TEXT("Unique acyclic local SCS parents required."));}
    for(auto* Root:C.Roots)if(Parents.Contains(Root))return Fail(C.R,TEXT("component_replace_generated_scs_topology_unsupported"),TEXT("Local roots may not also be children."));
    for(int32 I=0;I<Queue.Num();++I){if(Reached.Contains(Queue[I]))return Fail(C.R,TEXT("component_replace_generated_scs_topology_unsupported"),TEXT("Cyclic or duplicate SCS reachability."));Reached.Add(Queue[I]);for(auto* Child:Queue[I]->GetChildNodes())Queue.Add(Child);}
    if(Reached.Num()!=C.All.Num())return Fail(C.R,TEXT("component_replace_generated_scs_topology_unsupported"),TEXT("Every own node must be reachable from one admitted root."));
    if(C.Selected.IsValid()&&!C.RequireSceneCompatibility&&(C.OldClass.Get()==UBoxComponent::StaticClass()||C.OldClass.Get()==USphereComponent::StaticClass()||C.OldClass.Get()==UCapsuleComponent::StaticClass()))
    {
        C.CrossNativeOld=true;
        USCS_Node* Parent=nullptr;int32 Incoming=0;for(const auto& N:C.Nodes)if(N.Children.Contains(C.Selected.Get())){Parent=N.Node.Get();++Incoming;}
        if(C.Roots.Num()!=1||C.Roots.Contains(C.Selected.Get())||Incoming!=1||!Parent||!C.Selected->GetChildNodes().IsEmpty()||!Parent->ComponentClass->IsChildOf(USceneComponent::StaticClass())||!C.Roots[0]->ComponentClass->IsChildOf(USceneComponent::StaticClass()))return Fail(C.R,TEXT("component_replace_cross_leaf_scope_pending"),TEXT("False cross-class slice needs one own native Scene root and a non-root leaf shape with one Scene parent; root/child-bearing/reverse transitions remain incomplete."));
        C.CrossRoot.Reset(C.Roots[0]);C.CrossParent.Reset(Parent);C.CrossDefaultRoot.Reset(C.SCS->GetDefaultSceneRootNode());
        if(!Loaded(C.CrossDefaultRoot.Get())||C.All.Contains(C.CrossDefaultRoot.Get())||!C.CrossDefaultRoot->ComponentTemplate.IsResolved())return Fail(C.R,TEXT("component_replace_cross_default_root_pending"),TEXT("Existing resolved inactive default-root cache required; no cache creation/normalization is pre-approved."));
        C.CrossDefaultTemplate.Reset(C.CrossDefaultRoot->ComponentTemplate.Get());
        auto* Scene=Cast<USceneComponent>(C.OldTemplate.Get());if(!Scene||Scene->GetAttachParent()||!Scene->GetAttachChildren().IsEmpty())return Fail(C.R,TEXT("component_replace_cross_template_attachment_pending"),TEXT("Only unregistered/unattached template roles; SCS parent is separate from live template attachments."));
        for(UClass* K=C.OldClass.Get();K;K=K->GetSuperClass()){if(!FScope::CrossNativeSourceClass(K)||!K->ClassDefaultObject.IsResolved())return Fail(C.R,TEXT("component_replace_cross_native_source_pending"),TEXT("Loaded resolved native Engine hierarchy and exact CoreUObject UObject root/CDO slots required."));C.CrossNativeClasses.Emplace(K);C.CrossNativeSupers.Add(K->GetSuperClass());C.CrossNativeDefaults.Emplace(K->ClassDefaultObject.Get());}
        C.OldCDO.Reset(Cast<UActorComponent>(C.CrossNativeDefaults[0].Get()));if(!Loaded(C.OldCDO.Get()))return Fail(C.R,TEXT("component_replace_cross_native_cdo_pending"),TEXT("Existing native shape CDO required without warming."));
    }
    if(!C.Selected.IsValid()||(!C.CrossNativeOld&&!C.Roots.Contains(C.Selected.Get()))||!C.Selected->GetChildNodes().IsEmpty()||!C.Selected->ParentComponentOrVariableName.IsNone()||!C.Selected->ParentComponentOwnerClassName.IsNone())return Fail(C.R,TEXT("component_replace_generated_flat_root_required"),TEXT("One own flat non-scene root with no children/inherited parent attachment is required."));
    UObject* Requested=FindObject<UObject>(nullptr,*ClassPath);if(!Loaded(Requested)||Requested->GetClass()!=UBlueprintGeneratedClass::StaticClass()||Requested->GetPathName()!=ClassPath)return Fail(C.R,TEXT("component_replace_generated_class_unavailable"),TEXT("Replacement exact generated class must already be loaded."));C.NewClass.Reset(static_cast<UClass*>(Requested));
    if(C.RequireSceneCompatibility&&C.OldClass->IsChildOf(USceneComponent::StaticClass())!=C.NewClass->IsChildOf(USceneComponent::StaticClass()))return Fail(C.R,TEXT("component_replace_scene_incompatible"),TEXT("Original require_scene_compatibility=true rejects Scene/non-Scene transitions before Modify."));
    for(int32 I=0;I<2;++I)
    {
        if(C.CrossNativeOld&&I==0)continue;
        UClass* K=I?C.NewClass.Get():C.OldClass.Get();if(!Loaded(K)||K->GetClass()!=UBlueprintGeneratedClass::StaticClass()||!K->ClassGeneratedBy.IsResolved())return Fail(C.R,TEXT("component_replace_generated_class_unsupported"),TEXT("Both component classes must be current generated BPC classes."));
        UObject* Source=FromHandle(K->ClassGeneratedBy);if(!Loaded(Source)||Source->GetClass()!=UBlueprint::StaticClass())return Fail(C.R,TEXT("component_replace_generated_class_unsupported"),TEXT("Exact source Blueprint identity required."));
        UBlueprint* BP=static_cast<UBlueprint*>(Source);UClass* Skeleton=ClassHandle(BP->SkeletonGeneratedClass.GetGCPtr());UObject* D=ExistingCDO(K);
        if(!Loaded(Skeleton)||!Loaded(D)||D->GetClass()!=K||!D->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject)||!BP->ParentClass.GetGCPtr().IsResolved()||BP->ParentClass.GetGCPtr()!=UActorComponent::StaticClass())return Fail(C.R,TEXT("component_replace_generated_class_unsupported"),TEXT("Direct ActorComponent source parent and existing source CDO/skeleton required."));
        if(I){C.NewBP.Reset(BP);C.NewSkeleton.Reset(Skeleton);C.NewCDO.Reset(static_cast<UActorComponent*>(D));C.NewStatus=BP->Status;}else{C.OldBP.Reset(BP);C.OldSkeleton.Reset(Skeleton);C.OldCDO.Reset(static_cast<UActorComponent*>(D));C.OldStatus=BP->Status;}
    }
    if((!C.CrossNativeOld&&C.OldBP->GetPackage()==C.Host->GetPackage())||C.NewBP->GetPackage()==C.Host->GetPackage())return Fail(C.R,TEXT("component_replace_generated_source_target_package_shared"),TEXT("Source class packages must be distinct from the selected target package."));
    C.Status=C.Host->Status;C.Dirty=false;C.HostPackageFlags=C.Host->GetPackage()->GetPackageFlags();
    for(UObject* O:{static_cast<UObject*>(C.Host.Get()),static_cast<UObject*>(C.Host->GetOutermost()),static_cast<UObject*>(C.HostClass.Get()),static_cast<UObject*>(C.HostSkeleton.Get()),C.HostCDO.Get(),static_cast<UObject*>(C.SCS.Get()),static_cast<UObject*>(C.OldBP.Get()),static_cast<UObject*>(C.NewBP.Get()),static_cast<UObject*>(C.OldClass.Get()),static_cast<UObject*>(C.NewClass.Get()),static_cast<UObject*>(C.OldSkeleton.Get()),static_cast<UObject*>(C.NewSkeleton.Get()),static_cast<UObject*>(C.OldCDO.Get()),static_cast<UObject*>(C.NewCDO.Get()),static_cast<UObject*>(AActor::StaticClass()),static_cast<UObject*>(UActorComponent::StaticClass())})if((O||!C.CrossNativeOld)&&C.Role(O)==INDEX_NONE)return Fail(C.R,TEXT("component_replace_generated_role_unavailable"),TEXT("An initial held object identity is incomplete."));
    if(C.CrossNativeOld){for(int32 I=0;I<C.CrossNativeClasses.Num();++I){if(C.Role(C.CrossNativeClasses[I].Get())==INDEX_NONE||(C.CrossNativeDefaults[I].IsValid()&&C.Role(C.CrossNativeDefaults[I].Get())==INDEX_NONE))return false;}for(UObject* O:{static_cast<UObject*>(C.CrossDefaultRoot.Get()),static_cast<UObject*>(C.CrossDefaultTemplate.Get())})if(C.Role(O)==INDEX_NONE)return false;}
    for(const auto& Row:C.Nodes){if(C.Role(Row.Node.Get())==INDEX_NONE||C.Role(Row.Template.Get())==INDEX_NONE||C.Role(Row.Class.Get())==INDEX_NONE)return Fail(C.R,TEXT("component_replace_generated_role_unavailable"),TEXT("Own SCS node/class/template role cannot be held."));if(Row.Node.Get()!=C.Selected.Get()&&!Native(Row.Class.Get()))return Fail(C.R,TEXT("component_replace_generated_unselected_class_pending"),TEXT("Other generated SCS component classes remain outside this first finite source-protection slice."));}
    if(!C.Current())return false;
    for(const auto& V:C.Host->NewVariables)if(!V.DefaultValue.IsEmpty())return Fail(C.R,TEXT("component_replace_generated_pending_defaults"),TEXT("Host pending descriptor defaults must be compiled/saved before replacement; no unselected default consumption is admitted."));
    auto HostInspection=GeneratedComponent::InspectSavedActorParent(C.HostClass.Get());if(!C.Current())return false;C.R.Data->SetObjectField(TEXT("host_parent_inspection"),HostInspection);bool HostPassed=false;if(!HostInspection->TryGetBoolField(TEXT("success"),HostPassed)||!HostPassed)return Fail(C.R,TEXT("component_replace_generated_host_rejected"),TEXT("Saved-clean runtime host dependency inspection failed; no Modify."));
    for(UClass* K:{C.OldClass.Get(),C.NewClass.Get()}){if(C.CrossNativeOld&&K==C.OldClass.Get())continue;auto Inspection=GeneratedComponent::InspectSavedClass(K);if(!C.Current())return false;C.R.Data->SetObjectField(K==C.OldClass.Get()?TEXT("old_generated_component_inspection"):TEXT("new_generated_component_inspection"),Inspection);bool Passed=false;if(!Inspection->TryGetBoolField(TEXT("success"),Passed)||!Passed)return Fail(C.R,TEXT("component_replace_generated_source_rejected"),TEXT("Saved-clean generated component inspection failed; no Modify."));}
    if(!Fresh(C,C.OldTemplate.Get())||C.OldTemplate->GetClass()!=C.OldClass.Get()||C.OldTemplate->GetName()!=C.Slot||FindObject<UObject>(C.HostClass.Get(),*C.Slot)!=C.OldTemplate.Get())return Fail(C.R,TEXT("component_replace_generated_template_unsupported"),TEXT("Exact fresh inactive/unregistered/noWorld selected standard template slot required."));
    for(UObject* D:{static_cast<UObject*>(C.OldCDO.Get()),static_cast<UObject*>(C.NewCDO.Get()),static_cast<UObject*>(C.OldTemplate.Get())}){TArray<UObject*> Owned;GetObjectsWithOuter(D,Owned,EGetObjectsFlags::IncludeNestedObjects);if(!C.Current())return false;if(!Owned.IsEmpty())return Fail(C.R,TEXT("component_replace_generated_owned_defaults_pending"),TEXT("Owned/default subobjects require separate instancing propagation proof."));}
    C.HostArrays=FScope::Arrays(C.Host.Get());if(!C.CrossNativeOld)C.OldArrays=FScope::Arrays(C.OldBP.Get());C.NewArrays=FScope::Arrays(C.NewBP.Get());
    if(!FreezeGraphs(C,C.Host.Get())||(!C.CrossNativeOld&&!FreezeGraphs(C,C.OldBP.Get()))||(C.NewBP.Get()!=C.OldBP.Get()&&!FreezeGraphs(C,C.NewBP.Get())))return false;C.GraphsReady=true;
    return C.Current()&&NoConsumers(C);
}

bool PrepareNativeEmptyDelegates(FScope& C)
{
    if(!C.Current())return false;constexpr EPropertyFlags ExpectedFlags=CPF_NativeAccessSpecifierPublic|CPF_NoDestructor|CPF_BlueprintAssignable|CPF_InstancedReference;
    auto Prepared=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;C.R.Data->SetObjectField(TEXT("native_empty_delegate_metadata"),Prepared);Prepared->SetBoolField(TEXT("complete"),false);Prepared->SetStringField(TEXT("scope"),TEXT("Only exact native ActorComponent activation/deactivation sparse delegates with an actually empty multicast invocation list; IsBound counts expired entries too. Public getter/plain returned bools plus callback-free typed GetShared current guard; no delegate invocation, compaction, cleanup or instancing propagation."));C.R.Data->SetBoolField(TEXT("native_delegate_cleanup_performed"),false);C.R.Data->SetNumberField(TEXT("native_empty_delegate_getter_call_count"),0);C.R.Data->SetArrayField(TEXT("native_empty_delegate_observations"),{});
    for(const FName Name:{FName(TEXT("OnComponentActivated")),FName(TEXT("OnComponentDeactivated"))})
    {
        FMulticastSparseDelegateProperty* P=nullptr;for(TFieldIterator<FProperty> It(UActorComponent::StaticClass(),EFieldIteratorFlags::ExcludeSuper);It;++It)if(It->GetFName()==Name){P=CastField<FMulticastSparseDelegateProperty>(*It);break;}
        const int32 ExpectedOffset=Name==TEXT("OnComponentActivated")?STRUCT_OFFSET(UActorComponent,OnComponentActivated):STRUCT_OFFSET(UActorComponent,OnComponentDeactivated);
        const int32 ExpectedSize=Name==TEXT("OnComponentActivated")?sizeof(FActorComponentActivatedSignature):sizeof(FActorComponentDeactivateSignature);
        if(!P||P->GetClass()!=FMulticastSparseDelegateProperty::StaticClass()||P->GetOwnerClass()!=UActorComponent::StaticClass()||P->GetPropertyFlags()!=ExpectedFlags||P->ArrayDim!=1||P->GetSize()!=ExpectedSize||P->GetOffset_ForInternal()!=ExpectedOffset||P->HasSetterOrGetter()||!P->SignatureFunction.IsResolved())return Fail(C.R,TEXT("component_replace_generated_native_delegate_metadata_unsupported"),TEXT("Exact two native public sparse delegate fields with installed flags/layout and resolved signatures are required; no loader fallback."));
        const auto SignatureHandle=P->SignatureFunction;UObject* Raw=FromHandle(SignatureHandle);if(!C.Current())return false;
        FMulticastSparseDelegateProperty* Current=nullptr;for(TFieldIterator<FProperty> It(UActorComponent::StaticClass(),EFieldIteratorFlags::ExcludeSuper);It;++It)if(It->GetFName()==Name){Current=CastField<FMulticastSparseDelegateProperty>(*It);break;}
        if(Current!=P||!Loaded(Raw)||Raw->GetClass()!=USparseDelegateFunction::StaticClass()||!Current->SignatureFunction.IsResolved()||Current->SignatureFunction!=Raw)return Fail(C.R,TEXT("component_replace_generated_native_delegate_signature_unsupported"),TEXT("Already-loaded native sparse signature identity is not exact/current; no getter or writer."));
        auto* F=static_cast<USparseDelegateFunction*>(Raw);if(F->GetPackage()->GetName()!=TEXT("/Script/Engine")||F->GetPackage()->GetPackageFlags()!=16U||F->GetPackage()->IsDirty()||F->OwningClassName!=TEXT("ActorComponent")||F->DelegateName!=Name||F->IsEditorOnly())return Fail(C.R,TEXT("component_replace_generated_native_delegate_signature_unsupported"),TEXT("Native sparse signature owner/member/package metadata is unsupported."));
        FNativeEmptyDelegate A;A.Property=P;A.Name=Name;A.Offset=ExpectedOffset;A.Size=ExpectedSize;A.Signature.Reset(F);A.SignatureHandle=SignatureHandle;A.FunctionFlags=F->FunctionFlags;A.NumParms=F->NumParms;A.ParmsSize=F->ParmsSize;if(C.Role(F)==INDEX_NONE)return C.Stop(TEXT("component_replace_generated_native_delegate_signature_unobservable"),TEXT("Native sparse signature cannot be strongly held."));C.NativeEmptyDelegates.Add(MoveTemp(A));
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("property_name"),Name.ToString());Row->SetStringField(TEXT("owner"),TEXT("/Script/Engine.ActorComponent"));Row->SetStringField(TEXT("field_class"),P->GetClass()->GetName());Row->SetStringField(TEXT("property_flags_string"),LexToString(static_cast<uint64>(ExpectedFlags)));Row->SetNumberField(TEXT("size"),ExpectedSize);Row->SetNumberField(TEXT("offset"),ExpectedOffset);Row->SetObjectField(TEXT("signature"),Identity(F));Row->SetStringField(TEXT("signature_function_flags_string"),LexToString(static_cast<uint32>(F->FunctionFlags)));Row->SetNumberField(TEXT("signature_num_parms"),F->NumParms);Row->SetNumberField(TEXT("signature_parms_size"),F->ParmsSize);Row->SetStringField(TEXT("signature_owning_class"),F->OwningClassName.ToString());Row->SetStringField(TEXT("signature_delegate_name"),F->DelegateName.ToString());Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    C.NativeEmptyMetadataReady=true;if(!C.Current())return false;Prepared->SetArrayField(TEXT("fields"),Rows);Prepared->SetBoolField(TEXT("complete"),true);return true;
}
bool PrepareCrossSparse(FScope& C)
{
    if(!C.CrossNativeOld)return true;if(!C.Current())return false;
    for(UClass* Owner:{USceneComponent::StaticClass(),UPrimitiveComponent::StaticClass()})
    {
        const auto Layout=FScope::Fields(Owner);
        for(const auto& E:Layout)if(E.Owner==Owner&&E.Kind==FMulticastSparseDelegateProperty::StaticClass())
        {
            auto* P=static_cast<FMulticastSparseDelegateProperty*>(E.Property);
            if(P->ArrayDim!=1||P->HasSetterOrGetter()||!P->HasAnyPropertyFlags(CPF_BlueprintAssignable)||!P->SignatureFunction.IsResolved())return Fail(C.R,TEXT("component_replace_cross_sparse_source_pending"),TEXT("Exact native public sparse property/signature roles required."));
            const auto Slot=P->SignatureFunction;auto* Raw=FromHandle(Slot);if(!C.Current()||!FScope::FieldsSame(Owner,Layout))return false;
            auto* F=Cast<USparseDelegateFunction>(Raw);if(!Loaded(F)||F->GetPackage()->GetName()!=TEXT("/Script/Engine")||F->IsEditorOnly()||F->OwningClassName!=Owner->GetFName()||F->DelegateName!=E.Name)return Fail(C.R,TEXT("component_replace_cross_sparse_signature_pending"),TEXT("Native Scene/Primitive owning-class/member signature required; no source lookup fallback."));
            FScope::FCrossSparse S;S.Field=E;S.Owner.Reset(Owner);S.Signature.Reset(F);S.Flags=F->FunctionFlags;S.OwnerName=F->OwningClassName;S.DelegateName=F->DelegateName;if(C.Role(F)==INDEX_NONE)return false;C.CrossSparse.Add(MoveTemp(S));
        }
    }
    C.CrossSparseReady=true;C.R.Data->SetNumberField(TEXT("cross_native_sparse_source_count"),C.CrossSparse.Num());return C.Current();
}
// The raw public getter result is reduced to plain bools before any later SDK/current/layout query.
bool ObserveNativeEmptyDelegate(FScope& C,UActorComponent* O,const FField& Field,const TArray<FField>& Layout,const TSharedRef<FJsonObject>& Metadata,bool& Empty)
{
    Empty=false;if(!C.Current())return false;const FNativeEmptyDelegate* Role=nullptr;for(const auto& A:C.NativeEmptyDelegates)if(A.Property==Field.Property&&A.Name==Field.Name){Role=&A;break;}
    if(!Role)return C.Stop(TEXT("component_replace_generated_native_delegate_role_changed"),TEXT("Approved native sparse property role is missing."));
    auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("target_path"),O->GetPathName());Row->SetArrayField(TEXT("target_roles"),Metadata->GetArrayField(TEXT("target_roles")));Row->SetStringField(TEXT("property_name"),Field.Name.ToString());Row->SetBoolField(TEXT("getter_attempted"),false);Row->SetBoolField(TEXT("getter_returned"),false);Row->SetBoolField(TEXT("empty_observed"),false);
    auto Rows=C.R.Data->GetArrayField(TEXT("native_empty_delegate_observations"));Rows.Add(MakeShared<FJsonValueObject>(Row));C.R.Data->SetArrayField(TEXT("native_empty_delegate_observations"),Rows);
    const void* Address=Role->Property->ContainerPtrToValuePtr<void>(O);if(!C.Current())return false;const bool BeforeLayout=FScope::FieldsSame(O->GetClass(),Layout);if(!C.Current())return false;if(!BeforeLayout)return C.Stop(TEXT("component_replace_generated_native_delegate_layout_changed"),TEXT("Full current property inventory changed before public sparse getter."));
    Row->SetBoolField(TEXT("getter_attempted"),true);Count(C.R,TEXT("native_empty_delegate_getter_call_count"));
    const FMulticastScriptDelegate* Returned=Role->Property->GetMulticastDelegate(Address);const bool Present=Returned!=nullptr;const bool ObservedEmpty=!Returned||!Returned->IsBound();
    Row->SetBoolField(TEXT("getter_returned"),true);Row->SetBoolField(TEXT("returned_delegate_present"),Present);Row->SetBoolField(TEXT("empty_observed"),true);Row->SetBoolField(TEXT("invocation_list_empty"),ObservedEmpty);Metadata->SetBoolField(TEXT("value_observed_by_diagnostic"),true);Metadata->SetBoolField(TEXT("native_delegate_invocation_list_empty"),ObservedEmpty);
    if(!C.Current())return false;const bool Same=FScope::FieldsSame(O->GetClass(),Layout);
    if(!C.Current())return false;if(!Same)return C.Stop(TEXT("component_replace_generated_native_delegate_layout_changed"),TEXT("Full current property inventory changed after public getter; raw returned pointer is never reused."));
    Empty=ObservedEmpty;return true;
}
bool NoInstancedDefaults(FScope& C,UActorComponent* O)
{
    if(!C.Current())return false;const auto Fields=FScope::Fields(O->GetClass());
    const FString TargetPath=O->GetPathName();TArray<TSharedPtr<FJsonValue>> TargetRoles;
    if(O==C.OldCDO.Get())TargetRoles.Add(MakeShared<FJsonValueString>(TEXT("old_source_cdo")));
    if(O==C.NewCDO.Get())TargetRoles.Add(MakeShared<FJsonValueString>(TEXT("new_source_cdo")));
    if(O==C.OldTemplate.Get())TargetRoles.Add(MakeShared<FJsonValueString>(TEXT("old_selected_template")));
    if(O==C.NewTemplate.Get())TargetRoles.Add(MakeShared<FJsonValueString>(TEXT("new_replacement_template")));
    auto RejectPending=[&](const TSharedRef<FJsonObject>& Metadata,const TCHAR* Reason,const TCHAR* Message)->bool
    {
        Metadata->SetStringField(TEXT("failed_condition"),Reason);
        if(!C.R.Data->HasField(TEXT("owned_defaults_preflight_failure")))C.R.Data->SetObjectField(TEXT("owned_defaults_preflight_failure"),Metadata);
        return Fail(C.R,TEXT("component_replace_generated_owned_defaults_pending"),Message);
    };
    for(const auto& F:Fields)if(F.Flags&(CPF_InstancedReference|CPF_ContainsInstancedReference|CPF_PersistentInstance))
    {
        // Capture only already-current metadata before the existing checks. The failure helper performs no UObject/SDK read.
        auto Metadata=MakeShared<FJsonObject>();Metadata->SetStringField(TEXT("target_path"),TargetPath);Metadata->SetArrayField(TEXT("target_roles"),TargetRoles);
        Metadata->SetStringField(TEXT("property_name"),F.Name.ToString());Metadata->SetStringField(TEXT("field_class"),F.Kind?F.Kind->GetName():FString());
        const bool OwnerObserved=Loaded(F.Owner);Metadata->SetBoolField(TEXT("owner_path_observed"),OwnerObserved);Metadata->SetStringField(TEXT("property_owner_path"),OwnerObserved?F.Owner->GetPathName():FString());
        Metadata->SetStringField(TEXT("property_flags_string"),LexToString(static_cast<uint64>(F.Flags)));Metadata->SetStringField(TEXT("instancing_mask_overlap_string"),LexToString(static_cast<uint64>(F.Flags&(CPF_InstancedReference|CPF_ContainsInstancedReference|CPF_PersistentInstance))));
        Metadata->SetNumberField(TEXT("array_dim"),F.Dim);Metadata->SetNumberField(TEXT("size"),F.Size);Metadata->SetNumberField(TEXT("offset"),F.Offset);Metadata->SetBoolField(TEXT("value_observed_by_diagnostic"),false);
        if(F.Property->HasSetterOrGetter()||F.Dim!=1)return RejectPending(Metadata,TEXT("accessor_or_static_dimension"),TEXT("Instanced callback/static-array defaults are outside this slice."));
        if(const auto* A=CastField<FArrayProperty>(F.Property))
        {if(FScriptArrayHelper(A,A->ContainerPtrToValuePtr<void>(O)).Num()!=0)return RejectPending(Metadata,TEXT("nonempty_instanced_array"),TEXT("Nonempty instanced arrays require separate ownership propagation proof."));}
        else if(const auto* P=CastField<FObjectPropertyBase>(F.Property))
        {UObject* Value=P->GetObjectPropertyValue_InContainer(O);if(!C.Current())return false;if(!FScope::FieldsSame(O->GetClass(),Fields))return C.Stop(TEXT("component_replace_generated_property_layout_changed"),TEXT("Instanced default inspection changed its current descriptor layout."));if(Value)return RejectPending(Metadata,TEXT("non_null_instanced_object"),TEXT("Non-null instanced objects require separate ownership propagation proof."));}
        else if(F.Owner==UActorComponent::StaticClass()&&F.Kind==FMulticastSparseDelegateProperty::StaticClass()&&(F.Name==TEXT("OnComponentActivated")||F.Name==TEXT("OnComponentDeactivated")))
        {bool Empty=false;if(!ObserveNativeEmptyDelegate(C,O,F,Fields,Metadata,Empty))return false;if(!Empty)return RejectPending(Metadata,TEXT("nonempty_native_sparse_delegate"),TEXT("Native sparse delegate invocation list is nonempty; expired entries are retained and no cleanup or instancing is attempted."));}
        else if(C.CrossNativeOld&&(O==C.OldCDO.Get()||O==C.OldTemplate.Get())&&F.Kind==FMulticastSparseDelegateProperty::StaticClass()&&(F.Owner==USceneComponent::StaticClass()||F.Owner==UPrimitiveComponent::StaticClass()))
        {const auto* S=C.CrossSparse.FindByPredicate([&](const FScope::FCrossSparse& X){return X.Field.Property==F.Property;});if(!S||!C.CrossOldCurrent())return false;const auto* D=static_cast<FMulticastSparseDelegateProperty*>(F.Property)->GetMulticastDelegate(F.Property->ContainerPtrToValuePtr<void>(O));const bool Empty=!D||!D->IsBound();if(!C.Current()||!FScope::FieldsSame(O->GetClass(),Fields))return false;if(!Empty)return RejectPending(Metadata,TEXT("nonempty_native_scene_sparse_delegate"),TEXT("Native Scene/Primitive delegate bindings require propagation outside this leaf slice."));}
        else return RejectPending(Metadata,TEXT("unsupported_nested_instanced_kind"),TEXT("Nested instanced defaults are not admitted."));
    }
    return C.Current();
}
bool Declarations(FScope& C,UBlueprint* B,const TSharedRef<FJsonObject>& Out)
{
    const TSet<FName> Keep={TEXT("NewVariables"),TEXT("ImplementedInterfaces"),TEXT("ParentClass"),TEXT("CategorySorting")};TSet<FName> Skip;
    for(const auto& F:FScope::Fields(B->GetClass()))if(!Keep.Contains(F.Name))Skip.Add(F.Name);return C.Props(B,Skip,Out);
}
bool GraphState(FScope& C,const TSharedRef<FJsonObject>& Out,bool Sources)
{
    TArray<TSharedPtr<FJsonValue>> Rows;
    for(const auto& G:C.GraphRows)
    {
        const bool IsSource=G.Graph->GetOutermost()!=C.Host->GetPackage();if(IsSource!=Sources)continue;
        auto J=MakeShared<FJsonObject>();J->SetObjectField(TEXT("identity"),Identity(G.Graph.Get()));J->SetStringField(TEXT("guid"),G.Guid.ToString());TArray<TSharedPtr<FJsonValue>> Nodes;
        for(const auto& N:G.Nodes)
        {
            auto Node=MakeShared<FJsonObject>();Node->SetObjectField(TEXT("identity"),Identity(N.Node.Get()));Node->SetStringField(TEXT("guid"),N.Guid.ToString());auto Props=MakeShared<FJsonObject>();if(!C.Props(N.Node.Get(),{},Props))return false;Node->SetObjectField(TEXT("properties"),Props);TArray<TSharedPtr<FJsonValue>> Pins;
            for(const auto& P:N.PinRows)
            {
                FString Type;FEdGraphPinType::StaticStruct()->ExportText(Type,&P.Type,nullptr,N.Node.Get(),PPF_None,nullptr);if(!C.Current())return false;
                if(Type.Len()>MaxText||!CleanText(Type))return C.Stop(TEXT("component_replace_generated_pin_unobservable"),TEXT("Bounded physical pin type export is incomplete."));
                auto Pin=MakeShared<FJsonObject>();Pin->SetStringField(TEXT("id"),P.Id.ToString());Pin->SetStringField(TEXT("name"),P.Name.ToString());Pin->SetStringField(TEXT("type"),Type);Pin->SetStringField(TEXT("default"),P.Default);Pin->SetStringField(TEXT("auto"),P.Auto);Pin->SetStringField(TEXT("default_object"),P.Object.GetPathName());Pin->SetStringField(TEXT("text"),P.Text.ToString());TArray<TSharedPtr<FJsonValue>> Links;for(auto* L:P.Links)Links.Add(MakeShared<FJsonValueString>(L->PinId.ToString()));Pin->SetArrayField(TEXT("links"),Links);Pins.Add(MakeShared<FJsonValueObject>(Pin));
            }
            Node->SetArrayField(TEXT("pins"),Pins);Nodes.Add(MakeShared<FJsonValueObject>(Node));
        }
        J->SetArrayField(TEXT("nodes"),Nodes);Rows.Add(MakeShared<FJsonValueObject>(J));
    }
    Out->SetArrayField(TEXT("graphs"),Rows);return C.Current();
}
bool SourceState(FScope& C,const TSharedRef<FJsonObject>& Out)
{
    for(int32 I=0;I<2;++I)
    {
        if(C.CrossNativeOld&&I==0){auto NativeSource=MakeShared<FJsonObject>();for(int32 J=0;J<C.CrossNativeClasses.Num();++J){auto* K=C.CrossNativeClasses[J].Get();auto Row=Identity(K);Row->SetStringField(TEXT("class_flags"),LexToString(uint32(K->GetClassFlags())));auto* D=C.CrossNativeDefaults[J].Get();Row->SetObjectField(TEXT("existing_cdo"),Identity(D));if(D){auto Values=MakeShared<FJsonObject>();if(!C.Props(D,{},Values))return false;Row->SetObjectField(TEXT("default_properties"),Values);}NativeSource->SetObjectField(K->GetPathName(),Row);}Out->SetObjectField(TEXT("old_native_source"),NativeSource);continue;}
        UBlueprint* B=I?C.NewBP.Get():C.OldBP.Get();UActorComponent* D=I?C.NewCDO.Get():C.OldCDO.Get();auto J=MakeShared<FJsonObject>();J->SetObjectField(TEXT("blueprint"),Identity(B));J->SetObjectField(TEXT("class"),Identity(I?C.NewClass.Get():C.OldClass.Get()));J->SetObjectField(TEXT("skeleton"),Identity(I?C.NewSkeleton.Get():C.OldSkeleton.Get()));J->SetObjectField(TEXT("cdo"),Identity(D));
        auto Values=MakeShared<FJsonObject>(),Declaration=MakeShared<FJsonObject>(),File=MakeShared<FJsonObject>();if(!C.Props(D,{},Values)||!Declarations(C,B,Declaration)||!Disk(B->GetOutermost()->GetName(),File)||!C.Current())return false;J->SetObjectField(TEXT("default_properties"),Values);J->SetObjectField(TEXT("declarations"),Declaration);J->SetObjectField(TEXT("disk"),File);Out->SetObjectField(I?TEXT("new_source"):TEXT("old_source"),J);
    }
    auto Graphs=MakeShared<FJsonObject>();if(!GraphState(C,Graphs,true))return false;Out->SetObjectField(TEXT("source_graphs"),Graphs);return C.Current();
}
bool TargetState(FScope& C,const TSharedRef<FJsonObject>& Out)
{
    auto Declaration=MakeShared<FJsonObject>(),CDO=MakeShared<FJsonObject>(),Graphs=MakeShared<FJsonObject>();if(!Declarations(C,C.Host.Get(),Declaration)||!C.Props(C.HostCDO.Get(),{FName(*C.SelectedName)},CDO)||!GraphState(C,Graphs,false))return false;
    Out->SetObjectField(TEXT("blueprint"),Identity(C.Host.Get()));Out->SetObjectField(TEXT("host_cdo"),Identity(C.HostCDO.Get()));Out->SetObjectField(TEXT("declarations"),Declaration);Out->SetObjectField(TEXT("unselected_cdo"),CDO);Out->SetObjectField(TEXT("graphs"),Graphs);TArray<TSharedPtr<FJsonValue>> Nodes;
    for(const auto& N:C.Nodes)
    {
        auto J=MakeShared<FJsonObject>();J->SetObjectField(TEXT("node"),Identity(N.Node.Get()));J->SetStringField(TEXT("name"),N.Name.ToString());J->SetStringField(TEXT("guid"),N.Guid.ToString());
        auto Props=MakeShared<FJsonObject>();if(!C.Props(N.Node.Get(),N.Node.Get()==C.Selected.Get()?TSet<FName>{TEXT("ComponentClass"),TEXT("ComponentTemplate")}:TSet<FName>{},Props))return false;
        if(C.CrossNativeOld&&!C.Assigned&&N.Node.Get()==C.CrossParent.Get()){const auto ChildField=Props->TryGetField(TEXT("ChildNodes"));if(!ChildField||ChildField->Type!=EJson::Object)return C.Stop(TEXT("component_replace_cross_parent_field_unobservable"),TEXT("Original local parent ChildNodes descriptor/value could not be observed."));if(C.CrossParentChildrenBefore.IsValid()&&!Equal(C.CrossParentChildrenBefore,ChildField))return C.Stop(TEXT("component_replace_cross_parent_field_changed"),TEXT("Original parent ChildNodes export changed before assignment."));C.CrossParentChildrenBefore=ChildField;}
        J->SetObjectField(TEXT("node_properties"),Props);
        if(N.Node.Get()!=C.Selected.Get()){auto Values=MakeShared<FJsonObject>();if(!C.Props(N.Template.Get(),{},Values))return false;J->SetObjectField(TEXT("template"),Identity(N.Template.Get()));J->SetObjectField(TEXT("template_properties"),Values);}
        Nodes.Add(MakeShared<FJsonValueObject>(J));
    }
    Out->SetArrayField(TEXT("scs"),Nodes);return C.Current();
}
bool SelectedGetterGraphExpectation(FScope& C,const TSharedPtr<FJsonValue>& Before,TSharedPtr<FJsonValue>& Projected);
bool CrossScsExpectation(FScope& C,const TSharedPtr<FJsonValue>& Before,const TSharedPtr<FJsonValue>& Actual,TSharedPtr<FJsonValue>& Projected)
{
    Projected=Before;if(!C.CrossPromoted)return true;if(!C.Current()||!Before||!Actual||Before->Type!=EJson::Array||Actual->Type!=EJson::Array||!C.CrossParentChildrenBefore.IsValid())return false;
    const auto& A=Before->AsArray();const auto& B=Actual->AsArray();if(A.Num()!=C.Nodes.Num()||B.Num()!=A.Num())return false;TArray<TSharedPtr<FJsonValue>> Rows=A;int32 Matched=0;
    for(int32 I=0;I<A.Num();++I)if(C.Nodes[I].Node.Get()==C.CrossParent.Get())
    {
        ++Matched;if(!A[I]||!B[I]||A[I]->Type!=EJson::Object||B[I]->Type!=EJson::Object)return false;auto Original=A[I]->AsObject();auto ActualRow=B[I]->AsObject();
        const auto* P=Original->TryGetField(TEXT("node_properties")).Get();const auto* Q=ActualRow->TryGetField(TEXT("node_properties")).Get();if(!P||!Q||P->Type!=EJson::Object||Q->Type!=EJson::Object)return false;
        auto Properties=P->AsObject();auto ActualProperties=Q->AsObject();const auto OldChildren=Properties->TryGetField(TEXT("ChildNodes")),NewChildren=ActualProperties->TryGetField(TEXT("ChildNodes"));if(!OldChildren||!NewChildren||OldChildren->Type!=EJson::Object||NewChildren->Type!=EJson::Object)return false;
        if(Equal(OldChildren,NewChildren))continue;
        if(!Equal(OldChildren,C.CrossParentChildrenBefore))return false;
        const auto ExpectedField=OldChildren->AsObject(),ActualField=NewChildren->AsObject();if(ExpectedField->Values.Num()!=ActualField->Values.Num())return false;
        const auto OldValues=ExpectedField->TryGetField(TEXT("values")),NewValues=ActualField->TryGetField(TEXT("values"));if(!OldValues||!NewValues||OldValues->Type!=EJson::Array||NewValues->Type!=EJson::Array)return false;
        for(const auto& E:ExpectedField->Values)if(E.Key.ToView()!=TEXT("values")&&!Equal(E.Value,ActualField->TryGetField(E.Key.ToView())))return false;
        // Current() separately proves the original parent child-pointer list
        // minus exactly Selected. Only its serialized values cell is projected;
        // descriptor identity/layout and every other node/property stay exact.
        auto CopyField=MakeShared<FJsonObject>();CopyField->Values=ExpectedField->Values;CopyField->SetField(TEXT("values"),NewValues);auto CopyProperties=MakeShared<FJsonObject>();CopyProperties->Values=Properties->Values;CopyProperties->SetObjectField(TEXT("ChildNodes"),CopyField);auto CopyRow=MakeShared<FJsonObject>();CopyRow->Values=Original->Values;CopyRow->SetObjectField(TEXT("node_properties"),CopyProperties);Rows[I]=MakeShared<FJsonValueObject>(CopyRow);
    }
    if(Matched!=1)return false;Projected=MakeShared<FJsonValueArray>(Rows);return C.Current();
}
bool Boundary(FScope& C,const TSharedRef<FJsonObject>& SourceBefore,const TSharedRef<FJsonObject>& TargetBefore,const TSharedRef<FJsonObject>& DiskBefore,const TSharedRef<FJsonObject>& DirtyBefore,const TCHAR* Key,bool SavePhase=false,bool Compiled=false)
{
    auto Sources=MakeShared<FJsonObject>(),Target=MakeShared<FJsonObject>(),File=MakeShared<FJsonObject>(),Others=MakeShared<FJsonObject>(),J=MakeShared<FJsonObject>();C.R.Data->SetObjectField(Key,J);J->SetBoolField(TEXT("complete"),false);
    if(!SourceState(C,Sources)||!TargetState(C,Target)||!Disk(C.HostPackage,File)||!Dirty(C.HostPackage,Others)||!C.Current())return C.Stopped?false:C.Stop(TEXT("component_replace_generated_observation_incomplete"),TEXT("Phase observation returned incomplete; no next SDK/save/retry."));
    J->SetObjectField(TEXT("sources"),Sources);J->SetObjectField(TEXT("target"),Target);J->SetObjectField(TEXT("disk"),File);J->SetObjectField(TEXT("other_dirty"),Others);J->SetBoolField(TEXT("complete"),true);
    bool SameTarget=true;
    for(const auto& Field:TargetBefore->Values)
    {
        TSharedPtr<FJsonValue> Expected=Field.Value;
        if(Field.Key.ToView()==TEXT("graphs")&&!SelectedGetterGraphExpectation(C,Field.Value,Expected))return C.Stop(TEXT("component_replace_selected_getter_projection_failed"),TEXT("Selected getter comparison-only type projection was not unique and exact."));
        if(Field.Key.ToView()==TEXT("scs")&&!CrossScsExpectation(C,Field.Value,Target->TryGetField(Field.Key.ToView()),Expected))return C.Stopped?false:C.Stop(TEXT("component_replace_cross_compiler_scs_projection_failed"),TEXT("Selected full-compile leaf promotion projection was not exact; no save/retry."));
        if(Field.Key.ToView()==TEXT("host_cdo")&&Compiled){if(!PostcompileCdoEqual(Field.Value,Target->TryGetField(Field.Key.ToView())))SameTarget=false;}
        else if(SavePhase){if(!SavedEqual(Expected,Target->TryGetField(Field.Key.ToView()),C.HostPackage))SameTarget=false;}
        else if(!Equal(Expected,Target->TryGetField(Field.Key.ToView())))SameTarget=false;
    }
    const bool SourceOK=Equal(SourceBefore,Sources),OtherOK=Equal(DirtyBefore,Others),DiskOK=SavePhase?(File->GetBoolField(TEXT("exists"))&&File->GetNumberField(TEXT("bytes"))>0):Equal(DiskBefore,File);
    C.R.Data->SetBoolField(TEXT("observations_complete"),true);C.R.Data->SetBoolField(TEXT("source_preserved"),SourceOK);C.R.Data->SetBoolField(TEXT("unselected_state_preserved"),SameTarget&&OtherOK);C.R.Data->SetBoolField(TEXT("disk_preserved_before_save"),SavePhase?false:DiskOK);
    if(!SourceOK||!OtherOK||!SameTarget||!DiskOK)return Fail(C.R,TEXT("component_replace_generated_preservation_failed"),TEXT("Observed source/default/declaration/graph/unselected state/disk differs; actual prefix is retained without rollback/retry."),C.R.bAttempted);
    C.R.bKnown=true;C.R.bChanged=C.R.bAttempted;return true;
}
bool ModifyOne(FScope& C,UObject* Object,const TCHAR* Key)
{
    if(!C.Current())return false;C.R.bAttempted=true;C.R.bKnown=false;const bool BeforeDirty=C.Dirty;const int32 PackageRole=C.Role(C.Host->GetPackage());const EObjectFlags BeforePackageFlags=C.Roles[PackageRole].Flags;
    Count(C.R,TEXT("modify_calls"));const bool Returned=Object->Modify();C.R.Data->SetBoolField(Key,Returned);
    if(!FScope::Globals()||GEditor!=C.Editor.Get()||GUndo!=C.Undo||!Loaded(C.Host.Get())||!Loaded(C.Host->GetPackage()))return C.Stop(TEXT("component_replace_generated_modify_context_changed"),TEXT("Modify returned into a different current context."));
    C.Dirty=C.Host->GetPackage()->IsDirty();if(!C.Dirty)return C.Stop(TEXT("component_replace_generated_modify_dirty_unobserved"),TEXT("Modify returned without the required current target dirty state; no retirement is dispatched."));const EObjectFlags Actual=C.Host->GetPackage()->GetFlags();bool Adopted=false;
    if(Actual!=BeforePackageFlags&&Returned&&C.Undo&&!BeforeDirty&&C.Dirty&&Actual==(BeforePackageFlags|RF_Transactional)){C.Roles[PackageRole].Flags=Actual;Adopted=true;}
    C.R.Data->SetBoolField(TEXT("selected_modify_transactional_package_transition_adopted"),Adopted);
    if(!C.Current()){C.Roles[PackageRole].Flags=BeforePackageFlags;return false;}return true;
}
bool BasicReturned(FScope& C)
{
    return FScope::Globals()&&GEditor==C.Editor.Get()&&GUndo==C.Undo&&Loaded(C.Host.Get())&&Loaded(C.HostClass.Get())&&Loaded(C.SCS.Get())&&Loaded(C.Selected.Get());
}
bool AdoptRetirement(FScope& C,bool Returned)
{
    C.R.Data->SetBoolField(TEXT("retired_template_identity_observed"),false);
    if(!IsInGameThread()||IsGarbageCollecting()||UE::IsSavingPackage()||GExitPurge||IsEngineExitRequested())return C.Stop(TEXT("component_replace_generated_retirement_unobservable"),TEXT("Rename returned outside a safe lifetime-only observation context; no UObject dereference follows."));
    UActorComponent* O=C.OldTemplate.Get();
    if(!Loaded(O)||!Loaded(O->GetClass())||!Loaded(O->GetOuter())||!Loaded(O->GetOutermost()))return C.Stop(TEXT("component_replace_generated_retirement_unobservable"),TEXT("Held returned old template/class/outer/package is not safely observable."));
    C.R.Data->SetObjectField(TEXT("retired_template_returned_identity"),Identity(O));C.R.Data->SetBoolField(TEXT("retired_template_identity_observed"),true);
    if(!BasicReturned(C))return C.Stop(TEXT("component_replace_generated_retirement_context_changed"),TEXT("Actual retirement identity prefix is retained, but current editor/Undo/target roles changed; no next SDK."));
    if(!Returned||O->GetClass()!=C.OldClass.Get()||O->GetOutermost()!=GetTransientPackage()||O->GetOuter()!=GetTransientPackage())return C.Stop(TEXT("component_replace_generated_retirement_failed"),TEXT("One Rename did not return the exact admitted retirement; no allocation, rollback or retry."));
    const int32 Index=C.Role(O);if(Index==INDEX_NONE)return C.Stop(TEXT("component_replace_generated_retirement_unobservable"),TEXT("Held retirement role is absent."));
    auto& Role=C.Roles[Index];const auto Before=Role.Path;
    if(O->GetFlags()!=Role.Flags)return C.Stop(TEXT("component_replace_generated_retirement_flags_changed"),TEXT("Rename changed flags outside the admitted path/outer/package transition."));
    Role.Path=O->GetPathName();Role.Outer.Reset(GetTransientPackage());Role.Package.Reset(GetTransientPackage());Role.PackageFlags=GetTransientPackage()->GetPackageFlags();C.Retired=true;
    C.R.Data->SetStringField(TEXT("retired_template_original_path"),Before);C.R.Data->SetBoolField(TEXT("retirement_identity_transition_adopted"),true);
    if(!C.Current())return false;
    UObject* Slot=FindObject<UObject>(C.HostClass.Get(),*C.Slot);if(!C.Current())return false;
    C.R.Data->SetBoolField(TEXT("standard_template_slot_empty_after_retirement"),Slot==nullptr);
    return !Slot||C.Stop(TEXT("component_replace_generated_template_slot_collision"),TEXT("Standard slot is no longer empty after one retirement; no repeated Rename or allocation."));
}
bool AdoptAllocation(FScope& C,UActorComponent* Returned)
{
    C.R.Data->SetBoolField(TEXT("replacement_allocation_returned_nonnull"),Returned!=nullptr);C.R.Data->SetBoolField(TEXT("replacement_identity_observed"),false);
    if(!IsInGameThread()||IsGarbageCollecting()||UE::IsSavingPackage()||GExitPurge||IsEngineExitRequested())return C.Stop(TEXT("component_replace_generated_allocation_unobservable"),TEXT("Allocation returned outside a safe lifetime-only observation context; only raw nonnull is retained."));
    if(!Loaded(Returned))return C.Stop(TEXT("component_replace_generated_allocation_unobservable"),TEXT("Returned replacement is not safely loaded; no later SDK or cleanup."));
    C.NewTemplate.Reset(Returned);
    if(!Loaded(Returned->GetClass())||!Loaded(Returned->GetOuter())||!Loaded(Returned->GetOutermost()))return C.Stop(TEXT("component_replace_generated_allocation_unobservable"),TEXT("Returned object is held but class/outer/package identity is unobservable; no next SDK."));
    C.R.Data->SetObjectField(TEXT("replacement_returned_identity"),Identity(Returned));C.R.Data->SetBoolField(TEXT("replacement_identity_observed"),true);
    if(!BasicReturned(C))return C.Stop(TEXT("component_replace_generated_allocation_context_changed"),TEXT("Actual held returned identity prefix is retained, but editor/Undo/target roles changed; no next SDK."));
    if(Returned->GetClass()!=C.NewClass.Get()||Returned->GetOuter()!=C.HostClass.Get()||Returned->GetName()!=C.Slot||!Returned->HasAllFlags(RF_ArchetypeObject|RF_Transactional|RF_Public)||Returned->HasAnyFlags(RF_ClassDefaultObject|RF_DefaultSubObject|RF_Transient))return C.Stop(TEXT("component_replace_generated_allocation_role_changed"),TEXT("Observed returned allocation differs from the exact native public NewObject request."));
    if(C.Role(Returned)==INDEX_NONE||!C.Current())return C.Stopped?false:C.Stop(TEXT("component_replace_generated_allocation_unobservable"),TEXT("Returned replacement role cannot be held."));
    if(!Fresh(C,Returned))return C.Stopped?false:C.Stop(TEXT("component_replace_generated_allocation_not_fresh"),TEXT("Replacement must remain ownerless/noWorld/unregistered/inactive with no live render/physics state."));
    TArray<UObject*> Owned;GetObjectsWithOuter(Returned,Owned,EGetObjectsFlags::IncludeNestedObjects);if(!C.Current())return false;
    C.R.Data->SetNumberField(TEXT("replacement_owned_object_count"),Owned.Num());
    if(!Owned.IsEmpty())return C.Stop(TEXT("component_replace_generated_owned_allocation_pending"),TEXT("Constructor-created owned subobjects are outside this first replacement slice."));
    return NoInstancedDefaults(C,Returned);
}
bool RebindHost(FScope& C,bool FullCompile,bool CompilerRefreshedPins=false)
{
    if(!C.Current(true,CompilerRefreshedPins,FullCompile))return false;
    UClass* RawClass=ClassHandle(C.Host->GeneratedClass.GetGCPtr());if(!C.Current(true,CompilerRefreshedPins))return false;
    UClass* RawSkeleton=ClassHandle(C.Host->SkeletonGeneratedClass.GetGCPtr());if(!C.Current(true,CompilerRefreshedPins))return false;
    if(!Loaded(RawClass)||!Loaded(RawSkeleton))return C.Stop(TEXT("component_replace_generated_compiler_class_unobservable"),TEXT("Compiler returned without resolved exact current generated/skeleton roles."));
    TStrongObjectPtr<UClass> HoldClass(RawClass),HoldSkeleton(RawSkeleton);UObject* RawCDO=ExistingCDO(RawClass);if(!C.Current(true,CompilerRefreshedPins))return false;
    if(!Loaded(RawCDO))return C.Stop(TEXT("component_replace_generated_compiler_cdo_unobservable"),TEXT("Compiler did not leave an already existing loaded host CDO."));TStrongObjectPtr<UObject> HoldCDO(RawCDO);
    const UObject* OldObjects[]={C.HostClass.Get(),C.HostSkeleton.Get(),C.HostCDO.Get()};UObject* NewObjects[]={RawClass,RawSkeleton,RawCDO};
    for(int32 I=0;I<3;++I)
    {
        const FRole* Before=nullptr;for(const auto& A:C.Roles)if(A.Object.Get()==OldObjects[I]){Before=&A;break;}
        UObject* O=NewObjects[I];
        // Preserve the original short-circuit order. Failure diagnostics only serialize copied values and held role paths.
        const auto PointerText=[](const void* P){return FString::Printf(TEXT("%p"),P);};
        const auto HeldPath=[&](const UObject* P){for(const auto& A:C.Roles)if(A.Object.Get()==P)return A.Path;return FString();};
        const auto RejectIdentity=[&](const TCHAR* Field,const FString& ExpectedValue,const FString& ActualValue,const TCHAR* ValueKind,const UObject* ExpectedReference,const UObject* ActualReference)
        {
            if(!C.R.Data->HasField(TEXT("compiler_host_role_first_mismatch")))
            {
                auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("return_boundary"),FullCompile?TEXT("full_compile"):TEXT("structural_refresh"));
                J->SetNumberField(TEXT("role_index"),I);J->SetStringField(TEXT("role"),I==0?TEXT("host_generated_class"):(I==1?TEXT("host_skeleton_class"):TEXT("host_cdo")));
                J->SetStringField(TEXT("first_failed_condition"),Field);J->SetStringField(TEXT("value_kind"),ValueKind);J->SetStringField(TEXT("expected_value"),ExpectedValue);J->SetStringField(TEXT("actual_value"),ActualValue);
                J->SetStringField(TEXT("returned_object_pointer"),PointerText(O));J->SetBoolField(TEXT("held_before_role_observed"),Before!=nullptr);J->SetStringField(TEXT("held_before_object_path"),Before?Before->Path:FString());
                const FString ExpectedPath=HeldPath(ExpectedReference),ActualPath=HeldPath(ActualReference);
                J->SetBoolField(TEXT("expected_reference_captured_path_available"),!ExpectedPath.IsEmpty());J->SetStringField(TEXT("expected_reference_captured_path"),ExpectedPath);
                J->SetBoolField(TEXT("actual_reference_captured_path_available"),!ActualPath.IsEmpty());J->SetStringField(TEXT("actual_reference_captured_path"),ActualPath);
                const bool ComparedObjectPath=FCString::Strcmp(Field,TEXT("object_path"))==0;J->SetBoolField(TEXT("returned_object_current_path_observed"),ComparedObjectPath);
                if(ComparedObjectPath)J->SetStringField(TEXT("returned_object_current_path"),ActualValue);else J->SetField(TEXT("returned_object_current_path"),MakeShared<FJsonValueNull>());
                J->SetBoolField(TEXT("later_identity_conditions_unobserved"),!ComparedObjectPath);J->SetBoolField(TEXT("diagnostic_performed_additional_uobject_queries"),false);
                J->SetStringField(TEXT("path_scope"),TEXT("Reference paths are previously captured held-role labels, not new current UObject queries. Only the last object_path comparison observes the returned object's current path."));
                C.R.Data->SetObjectField(TEXT("compiler_host_role_first_mismatch"),J);
            }
            return C.Stop(TEXT("component_replace_generated_compiler_identity_changed"),TEXT("Compiler changed a host generated/skeleton/CDO role beyond the selected exact path/owner/class transition."));
        };
        if(!Before)return RejectIdentity(TEXT("held_before_role"),TEXT("present"),TEXT("absent"),TEXT("presence"),nullptr,nullptr);
        UClass* ExpectedClass=I==2?RawClass:Before->Class.Get();UClass* ActualClass=O->GetClass();
        if(ActualClass!=ExpectedClass)return RejectIdentity(TEXT("class"),PointerText(ExpectedClass),PointerText(ActualClass),TEXT("object_pointer"),ExpectedClass,ActualClass);
        // UClass::CreateDefaultObject allocates its CDO in the class outer (the held package), not inside the class object.
        UObject* ExpectedOuter=Before->Outer.Get();UObject* ActualOuter=O->GetOuter();
        if(ActualOuter!=ExpectedOuter)return RejectIdentity(TEXT("outer"),PointerText(ExpectedOuter),PointerText(ActualOuter),TEXT("object_pointer"),ExpectedOuter,ActualOuter);
        UPackage* ExpectedPackage=Before->Package.Get();UPackage* ActualPackage=O->GetOutermost();
        if(ActualPackage!=ExpectedPackage)return RejectIdentity(TEXT("package"),PointerText(ExpectedPackage),PointerText(ActualPackage),TEXT("object_pointer"),ExpectedPackage,ActualPackage);
        const FString ActualPath=O->GetPathName();if(C.NativeScene?!ActualPath.Equals(Before->Path,ESearchCase::CaseSensitive):ActualPath!=Before->Path)return RejectIdentity(TEXT("object_path"),Before->Path,ActualPath,TEXT("object_path"),nullptr,nullptr);
        const EObjectFlags F=O->GetFlags(),Expected=Before->Flags;
        if(F!=Expected&&F!=(Expected&~(RF_WasLoaded|RF_LoadCompleted)))return C.Stop(TEXT("component_replace_generated_compiler_flags_changed"),TEXT("Only native compiler clearing of host load-completion flags is admitted at this explicit return."));
        if(!FullCompile&&I!=1&&O!=OldObjects[I])return C.Stop(TEXT("component_replace_generated_structural_identity_changed"),TEXT("Skeleton-only refresh may not replace generated class/CDO in this slice."));
    }
    if(RawCDO->GetClass()!=RawClass||!RawCDO->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject)||!RawClass->ClassGeneratedBy.IsResolved()||RawClass->ClassGeneratedBy!=C.Host.Get()||!RawSkeleton->ClassGeneratedBy.IsResolved()||RawSkeleton->ClassGeneratedBy!=C.Host.Get())return C.Stop(TEXT("component_replace_generated_compiler_role_changed"),TEXT("Compiler current generated/skeleton/CDO ownership differs."));
    for(int32 I=C.Roles.Num()-1;I>=0;--I)if(C.Roles[I].Object.Get()==OldObjects[0]||C.Roles[I].Object.Get()==OldObjects[1]||C.Roles[I].Object.Get()==OldObjects[2]){C.RetiredHostRoles.Add(MoveTemp(C.Roles[I].Object));C.Roles.RemoveAt(I);}
    C.HostClass.Reset(static_cast<UBlueprintGeneratedClass*>(RawClass));C.HostSkeleton.Reset(static_cast<UBlueprintGeneratedClass*>(RawSkeleton));C.HostCDO.Reset(RawCDO);C.Status=C.Host->Status;
    for(UObject* O:NewObjects)if(C.Role(O)==INDEX_NONE)return C.Stop(TEXT("component_replace_generated_compiler_role_unobservable"),TEXT("Current compiler role cannot be held."));
    C.R.Data->SetObjectField(FullCompile?TEXT("compiled_host_cdo_identity"):TEXT("structural_host_cdo_identity"),Identity(RawCDO));if(!C.AdoptGetterProperties(FullCompile))return false;if(CompilerRefreshedPins&&!C.AdoptCompilerGraphPins())return false;return C.Current();
}
bool SelectedMember(FScope& C,UClass* Owner,const TCHAR* Key)
{
    if(!C.Current())return false;const auto Fields=FScope::Fields(Owner);const FObjectProperty* P=FindFProperty<FObjectProperty>(Owner,FName(*C.SelectedName));
    const bool OK=P&&P->GetOwnerClass()==Owner&&P->ArrayDim==1&&!P->HasSetterOrGetter()&&P->HasAnyPropertyFlags(CPF_BlueprintVisible)&&P->PropertyClass.IsResolved()&&P->PropertyClass==C.NewClass.Get();C.R.Data->SetBoolField(Key,OK);
    if(!C.Current()||!FScope::FieldsSame(Owner,Fields))return C.Stopped?false:C.Stop(TEXT("component_replace_generated_selected_property_layout_changed"),TEXT("Selected current property layout changed."));
    return OK||C.Stop(TEXT("component_replace_generated_selected_property_mismatch"),TEXT("Selected current skeleton/compiled member is not the exact new Blueprint component class."));
}
// LT SelectedGetter_Begin
bool GetterType(FScope& C,UBlueprintGeneratedClass* Owner,UClass* ValueClass,FEdGraphPinType& Out)
{
    if(!C.Current())return false;const auto Layout=FScope::GetterFields(Owner);FObjectProperty* P=FScope::GetterField(Owner,FName(*C.SelectedName));
    if(!P||P->GetClass()!=FObjectProperty::StaticClass()||P->GetOwnerClass()!=Owner||P->ArrayDim!=1||P->HasSetterOrGetter()||!P->HasAnyPropertyFlags(CPF_BlueprintVisible)||!P->PropertyClass.IsResolved()||P->PropertyClass!=ValueClass)return Fail(C.R,TEXT("component_replace_selected_getter_property_unsupported"),TEXT("Selected own scalar visible hard object property must name the exact held component class."),C.R.bAttempted);
    const bool Converted=C.GetterSchema->ConvertPropertyToPinType(P,Out);
    if(!C.Current())return false;if(!FScope::GetterFieldsSame(Owner,Layout)||FScope::GetterField(Owner,FName(*C.SelectedName))!=P)return C.Stop(TEXT("component_replace_selected_getter_property_changed"),TEXT("Property conversion changed its current field inventory; no old field is reused."));
    return (Converted&&Out.PinCategory==UEdGraphSchema_K2::PC_Object&&Out.ContainerType==EPinContainerType::None&&!Out.bIsReference&&Out.PinSubCategoryObject==ValueClass)||Fail(C.R,TEXT("component_replace_selected_getter_type_unsupported"),TEXT("Selected property did not convert to the exact whole scalar object output."),C.R.bAttempted);
}
bool GetterLinks(FScope& C,const FGraphNode& N,const FEdGraphPinType& NewType)
{
    if(!C.Current()||!N.PinRows.IsValidIndex(N.GetterValueIndex))return false;
    // Every peer address was proved to belong to the frozen graph inventory before any peer dereference in FreezeGraphs.
    const auto Peers=N.PinRows[N.GetterValueIndex].Links;
    for(UEdGraphPin* Peer:Peers)
    {
        if(!C.Current())return false;
        if(Peer->WasTrashed()||Peer->Direction!=EGPD_Input||Peer->ParentPin||!Peer->SubPins.IsEmpty()||Peer->bHidden||Peer->bOrphanedPin||Peer->bDeprecated||Peer->bNotConnectable)return Fail(C.R,TEXT("component_replace_selected_getter_link_unsupported"),TEXT("Selected getter supports whole current directly connected input peers only."),C.R.bAttempted);
        const FEdGraphPinType PeerType=Peer->PinType;
        const bool Compatible=C.GetterSchema->ArePinTypesCompatible(NewType,PeerType,C.HostClass.Get());
        if(!C.Current())return false;
        if(!Compatible)return Fail(C.R,TEXT("component_replace_selected_getter_link_incompatible"),TEXT("New getter type is incompatible with an existing direct peer; no converter, break-others, old-specific member migration or retry is authorized."),C.R.bAttempted);
    }
    return true;
}
bool PrepareSelectedGetters(FScope& C)
{
    int32 CountGetters=0;for(const auto& G:C.GraphRows)for(const auto& N:G.Nodes)CountGetters+=N.SelectedGetter;
    C.R.Data->SetNumberField(TEXT("selected_own_getter_count"),CountGetters);if(!CountGetters)return true;
    if(!C.Current())return false;UClass* SchemaClass=UEdGraphSchema_K2::StaticClass();UObject* SchemaObject=ExistingCDO(SchemaClass);if(!C.Current())return false;
    if(!Loaded(SchemaObject)||SchemaObject->GetClass()!=SchemaClass||!SchemaObject->HasAnyFlags(RF_ClassDefaultObject))return Fail(C.R,TEXT("component_replace_selected_getter_schema_unavailable"),TEXT("Exact already-existing native K2 schema CDO is required; no schema creation or loader fallback."));
    C.GetterSchema.Reset(static_cast<UEdGraphSchema_K2*>(SchemaObject));if(C.Role(SchemaClass)==INDEX_NONE||C.Role(SchemaObject)==INDEX_NONE)return false;
    const auto SkeletonLayout=FScope::GetterFields(C.HostSkeleton.Get()),CompiledLayout=FScope::GetterFields(C.HostClass.Get());
    FEdGraphPinType SkeletonType,CompiledType;if(!GetterType(C,C.HostSkeleton.Get(),C.OldClass.Get(),SkeletonType)||!GetterType(C,C.HostClass.Get(),C.OldClass.Get(),CompiledType))return false;
    if(!FScope::GetterFieldsSame(C.HostSkeleton.Get(),SkeletonLayout)||!FScope::GetterFieldsSame(C.HostClass.Get(),CompiledLayout))return C.Stop(TEXT("component_replace_selected_getter_property_changed"),TEXT("Selected preflight property inventories changed during schema conversion."));
    if(!FScope::PinTypeSame(SkeletonType,CompiledType))return Fail(C.R,TEXT("component_replace_selected_getter_property_unsupported"),TEXT("Selected skeleton/compiled property full types differ before Modify."));
    FObjectProperty* Skeleton=FScope::GetterField(C.HostSkeleton.Get(),FName(*C.SelectedName));FObjectProperty* Compiled=FScope::GetterField(C.HostClass.Get(),FName(*C.SelectedName));
    C.GetterSkeletonField={Skeleton,Skeleton->GetOwnerStruct(),Skeleton->GetClass(),Skeleton->GetFName(),Skeleton->GetPropertyFlags(),Skeleton->GetSize(),Skeleton->GetOffset_ForInternal(),Skeleton->ArrayDim};
    C.GetterCompiledField={Compiled,Compiled->GetOwnerStruct(),Compiled->GetClass(),Compiled->GetFName(),Compiled->GetPropertyFlags(),Compiled->GetSize(),Compiled->GetOffset_ForInternal(),Compiled->ArrayDim};
    C.GetterSkeletonType=C.OldClass.Get();C.GetterCompiledType=C.OldClass.Get();
    const FGuid MemberGuid=C.HostSkeleton->FindBlueprintPropertyGuidFromName(FName(*C.SelectedName));
    for(const auto& G:C.GraphRows)for(const auto& N:G.Nodes)if(N.SelectedGetter)
    {
        if((!C.NativeScene&&!C.CrossNativeOld&&(N.Reference.GetMemberGuid()!=MemberGuid||C.HostClass->FindBlueprintPropertyGuidFromName(FName(*C.SelectedName))!=MemberGuid))||!G.Graph->Schema.GetGCPtr().IsResolved()||G.Graph->Schema.GetGCPtr()!=SchemaClass)return Fail(C.R,TEXT("component_replace_selected_getter_reference_unsupported"),TEXT("Own getter member GUID must equal both current public property GUID maps; exact K2 schema required. SCS GUID is a separate identity."));
    }
    C.GetterPropertiesReady=true;
    C.R.Data->SetStringField(TEXT("selected_getter_member_guid"),MemberGuid.ToString());C.R.Data->SetStringField(TEXT("selected_scs_variable_guid"),C.Guid.ToString());C.R.Data->SetBoolField(TEXT("scs_guid_substituted_for_member_guid"),false);
    C.R.Data->SetNumberField(TEXT("selected_getter_pin_pointer_replacements"),0);
    for(auto& G:C.GraphRows)for(auto& N:G.Nodes)if(N.SelectedGetter)
    {
        if(!C.Current())return false;
        if(!C.R.Data->HasField(TEXT("selected_getter_entry_object_flags"))){C.R.Data->SetStringField(TEXT("selected_getter_entry_object_flags"),LexToString(static_cast<uint32>(N.Node->GetFlags())));C.R.Data->SetNumberField(TEXT("selected_getter_entry_pin_count"),N.PinRows.Num());C.R.Data->SetBoolField(TEXT("selected_getter_entry_native_scene"),C.NativeScene);C.R.Data->SetStringField(TEXT("selected_getter_entry_node_guid"),N.Guid.ToString());}
        if((!C.NativeScene&&!C.CrossNativeOld&&!N.Node->HasAnyFlags(RF_Transactional))||N.PinRows.Num()!=2)return Fail(C.R,TEXT("component_replace_selected_getter_shape_unsupported"),TEXT("Pure getter with one own value output and one hidden unlinked self input required; generated non-scene entry additionally requires its original transactional flag."));
        int32 SelfCount=0,ValueCount=0;
        for(int32 I=0;I<N.PinRows.Num();++I)
        {
            const auto& Pin=N.PinRows[I];
            if(Pin.Orphan||Pin.Deprecated||Pin.NotConnectable||Pin.Parent||!Pin.Children.IsEmpty()||Pin.Object!=nullptr||!Pin.Default.IsEmpty()||!Pin.Auto.IsEmpty()||!Pin.Text.IsEmpty())return Fail(C.R,TEXT("component_replace_selected_getter_shape_unsupported"),TEXT("Getter literal, orphan, split or deprecated pin migration is outside this first slice."));
            if(Pin.Direction==EGPD_Output&&Pin.Name==FName(*C.SelectedName)&&!Pin.Hidden&&FScope::PinTypeSame(Pin.Type,SkeletonType)){N.GetterValueIndex=I;++ValueCount;}
            else if(Pin.Direction==EGPD_Input&&Pin.Name==UEdGraphSchema_K2::PN_Self&&Pin.Hidden&&Pin.Links.IsEmpty()&&Pin.Type.PinCategory==UEdGraphSchema_K2::PC_Object&&Pin.Type.ContainerType==EPinContainerType::None&&!Pin.Type.bIsReference)++SelfCount;
            else return Fail(C.R,TEXT("component_replace_selected_getter_shape_unsupported"),TEXT("Getter has an unsupported extra/exec pin, output type or explicit self link."));
        }
        if(ValueCount!=1||SelfCount!=1)return Fail(C.R,TEXT("component_replace_selected_getter_shape_unsupported"),TEXT("Unique selected scalar output and hidden own self pin required."));
        N.GetterBeforeType=SkeletonType;N.GetterAfterType=SkeletonType;N.GetterAfterType.PinSubCategoryObject=C.NewClass.Get();
        FEdGraphPinType::StaticStruct()->ExportText(N.GetterBeforeTypeText,&N.GetterBeforeType,nullptr,N.Node.Get(),PPF_None,nullptr);if(!C.Current())return false;
        FEdGraphPinType::StaticStruct()->ExportText(N.GetterAfterTypeText,&N.GetterAfterType,nullptr,N.Node.Get(),PPF_None,nullptr);if(!C.Current())return false;
        if(!GetterLinks(C,N,N.GetterAfterType))return false;
    }
    C.R.Data->SetBoolField(TEXT("selected_getter_direct_links_preflight_verified"),true);C.R.Data->SetBoolField(TEXT("selected_getter_repair_planned"),true);return C.Current();
}
bool RepairSelectedGetters(FScope& C)
{
    if(!C.GetterPropertiesReady)return true;
    FEdGraphPinType ActualNewType;if(!GetterType(C,C.HostSkeleton.Get(),C.NewClass.Get(),ActualNewType))return false;
    TArray<TSharedPtr<FJsonValue>> Receipts;C.R.Data->SetArrayField(TEXT("selected_getter_repairs"),Receipts);
    for(auto& G:C.GraphRows)for(auto& N:G.Nodes)if(N.SelectedGetter)
    {
        if(!C.Current()||!FScope::PinTypeSame(ActualNewType,N.GetterAfterType))return C.Stopped?false:C.Stop(TEXT("component_replace_selected_getter_new_type_mismatch"),TEXT("Current new skeleton property differs from the exact planned output type."));
        if(!GetterLinks(C,N,N.GetterAfterType)||!ModifyOne(C,N.Node.Get(),TEXT("selected_getter_modify_returned")))return false;
        if(!C.Current())return false;FEdGraphPinType BeforeWriteType;if(!GetterType(C,C.HostSkeleton.Get(),C.NewClass.Get(),BeforeWriteType)||!FScope::PinTypeSame(BeforeWriteType,N.GetterAfterType))return C.Stopped?false:C.Stop(TEXT("component_replace_selected_getter_new_type_mismatch"),TEXT("Selected property changed before the output type writer."));
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("node_path"),C.CapturedRolePath(N.Node.Get()));J->SetStringField(TEXT("node_guid"),N.Guid.ToString());J->SetStringField(TEXT("output_pin_id"),N.PinRows[N.GetterValueIndex].Id.ToString());J->SetStringField(TEXT("before_type"),N.GetterBeforeTypeText);J->SetStringField(TEXT("expected_type"),N.GetterAfterTypeText);J->SetBoolField(TEXT("type_assignment_attempted"),true);J->SetBoolField(TEXT("type_assignment_performed"),false);J->SetBoolField(TEXT("reconstruct_attempted"),false);J->SetBoolField(TEXT("reconstruct_returned"),false);Receipts.Add(MakeShared<FJsonValueObject>(J));C.R.Data->SetArrayField(TEXT("selected_getter_repairs"),Receipts);
        C.R.Stage=TEXT("retarget_selected_getter");C.R.bAttempted=true;C.R.bKnown=false;Count(C.R,TEXT("selected_getter_type_assignment_calls"));
        // Current has checked exact whole pins immediately before this callback-free public data assignment.
        UEdGraphPin* Output=N.Node->Pins[N.GetterValueIndex];Output->PinType=N.GetterAfterType;J->SetBoolField(TEXT("type_assignment_performed"),true);
        // Accept only the planned full type before broad checks and before ReconstructNode. No SDK executes between assignment and this plain comparison.
        if(!FScope::PinTypeSame(Output->PinType,N.GetterAfterType))return C.Stop(TEXT("component_replace_selected_getter_type_assignment_failed"),TEXT("Output type assignment did not retain its exact requested type."));
        N.PinRows[N.GetterValueIndex].Type=N.GetterAfterType;N.GetterRetyped=true;J->SetBoolField(TEXT("assigned_type_observed_exact"),true);C.R.Data->SetBoolField(TEXT("graph_member_migration_performed"),true);
        if(!C.Current()||!GetterLinks(C,N,N.GetterAfterType))return false;
        C.R.Stage=TEXT("reconstruct_selected_getter");C.R.bKnown=false;J->SetBoolField(TEXT("reconstruct_attempted"),true);Count(C.R,TEXT("selected_getter_reconstruct_calls"));static_cast<UK2Node_VariableGet*>(N.Node.Get())->ReconstructNode();J->SetBoolField(TEXT("reconstruct_returned"),true);
        // Previous raw pin values are opaque keys only until all current semantic/ID/link checks have passed.
        if(!C.AdoptGetterPinsOnly(N.Node.Get())||!GetterLinks(C,N,N.GetterAfterType))return false;J->SetBoolField(TEXT("current_output_and_links_verified"),true);
    }
    C.R.Data->SetBoolField(TEXT("selected_getter_repairs_complete"),true);return C.Current();
}
bool SelectedGetterGraphExpectation(FScope& C,const TSharedPtr<FJsonValue>& Before,TSharedPtr<FJsonValue>& Projected)
{
    Projected=Before;bool Any=false;for(const auto& G:C.GraphRows)for(const auto& N:G.Nodes)Any|=N.GetterRetyped;if(!Any)return true;
    if(!Before.IsValid()||Before->Type!=EJson::Object)return false;TSharedPtr<FJsonObject> Clone;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text(Before->AsObject().ToSharedRef())),Clone)||!Clone.IsValid())return false;
    for(const auto& G:C.GraphRows)for(const auto& N:G.Nodes)if(N.GetterRetyped)
    {
        int32 Matches=0;
        for(const auto& GraphValue:Clone->GetArrayField(TEXT("graphs")))
        {
            const auto Graph=GraphValue->AsObject();if(Graph->GetStringField(TEXT("guid"))!=G.Guid.ToString())continue;
            for(const auto& NodeValue:Graph->GetArrayField(TEXT("nodes")))
            {
                const auto Node=NodeValue->AsObject();if(Node->GetStringField(TEXT("guid"))!=N.Guid.ToString())continue;
                for(const auto& PinValue:Node->GetArrayField(TEXT("pins")))
                {
                    const auto Pin=PinValue->AsObject();if(Pin->GetStringField(TEXT("id"))!=N.PinRows[N.GetterValueIndex].Id.ToString())continue;
                    const FString BeforeType=Pin->GetStringField(TEXT("type"));if(BeforeType!=N.GetterBeforeTypeText&&BeforeType!=N.GetterAfterTypeText)return false;
                    Pin->SetStringField(TEXT("type"),N.GetterAfterTypeText);++Matches;
                }
            }
        }
        if(Matches!=1)return false;
    }
    Projected=MakeShared<FJsonValueObject>(Clone);return true;
}
// LT SelectedGetter_End
bool SelectedDefaults(FScope& C,const TArray<FName>& Tags,const TSharedRef<FJsonObject>& NewDefaultValues,const TCHAR* Key)
{
    if(!C.Current()||!Fresh(C,C.NewTemplate.Get()))return C.Stopped?false:C.Stop(TEXT("component_replace_generated_selected_not_fresh"),TEXT("Assigned replacement lost its admitted fresh authoring roles."));
    auto J=MakeShared<FJsonObject>(),Values=MakeShared<FJsonObject>(),Old=MakeShared<FJsonObject>();C.R.Data->SetObjectField(Key,J);
    if(!C.Props(C.NewTemplate.Get(),{TEXT("ComponentTags")},Values)||!C.Props(C.OldTemplate.Get(),{},Old))return false;
    const bool ValuesOK=Equal(NewDefaultValues,Values),TagsOK=C.NewTemplate->ComponentTags==Tags,OldOK=C.OldFieldsBefore.IsValid()&&Equal(C.OldFieldsBefore.ToSharedRef(),Old);
    J->SetObjectField(TEXT("replacement_identity"),Identity(C.NewTemplate.Get()));J->SetObjectField(TEXT("retired_identity"),Identity(C.OldTemplate.Get()));J->SetObjectField(TEXT("replacement_default_properties"),Values);J->SetObjectField(TEXT("retired_properties"),Old);J->SetBoolField(TEXT("new_class_cdo_defaults_verified"),ValuesOK);J->SetBoolField(TEXT("component_tags_copied"),TagsOK);J->SetBoolField(TEXT("retired_properties_preserved"),OldOK);
    const bool Binding=C.Assigned&&C.Selected->GetVariableName()==FName(*C.SelectedName)&&C.Selected->VariableGuid==C.Guid&&C.NewTemplate->GetClass()==C.NewClass.Get()&&C.NewTemplate->GetOuter()==C.HostClass.Get()&&C.NewTemplate->GetName()==C.Slot;
    J->SetBoolField(TEXT("selected_name_guid_class_slot_verified"),Binding);return (ValuesOK&&TagsOK&&OldOK&&Binding)||C.Stop(TEXT("component_replace_generated_selected_readback_failed"),TEXT("Actual selected tags/new-class defaults/retired defaults/name/GUID/class/slot differ; no retry or rollback."));
}
bool AdoptSaveFlags(FScope& C)
{
    if(!BasicReturned(C)||!Loaded(C.Host->GetPackage()))return C.Stop(TEXT("component_replace_generated_save_context_changed"),TEXT("Save returned without safe held current target roles."));
    for(auto& A:C.Roles)
    {
        UObject* O=A.Object.Get();if(A.Package.Get()!=C.Host->GetPackage())continue;
        if(!Loaded(O)||O->GetClass()!=A.Class.Get()||O->GetOuter()!=A.Outer.Get()||O->GetPathName()!=A.Path)return C.Stop(TEXT("component_replace_generated_save_identity_changed"),TEXT("Save changed an owned identity before load-flag observation."));
        const EObjectFlags Actual=O->GetFlags();if(Actual!=A.Flags&&Actual!=(A.Flags|RF_WasLoaded|RF_LoadCompleted))return C.Stop(TEXT("component_replace_generated_save_flags_changed"),TEXT("Only package-owned successful-save load-completion flag additions are admitted."));A.Flags=Actual;
    }
    C.Dirty=C.Host->GetPackage()->IsDirty();return C.Current();
}
FString Replace(const FString& BlueprintPath,const FString& ComponentName,const FString& NewComponentClass,double SphereRadius,bool DryRun,bool Compile,bool Save,bool Refresh,bool RequireSceneCompatibility=true)
{
    FResult R;Init(R,DryRun,Compile,Save,Refresh);FScope C(R);C.RequireSceneCompatibility=RequireSceneCompatibility;
    R.Data->SetBoolField(TEXT("generated_non_scene_component_replacement"),true);R.Data->SetBoolField(TEXT("native_transaction_scope_created"),false);R.Data->SetBoolField(TEXT("full_legacy_contract_completed"),false);R.Data->SetBoolField(TEXT("graph_member_migration_performed"),false);
    R.Data->SetStringField(TEXT("blueprint_name"),BlueprintPath);R.Data->SetStringField(TEXT("requested_new_component_class"),NewComponentClass);R.Data->SetBoolField(TEXT("require_scene_compatibility"),RequireSceneCompatibility);
    R.Data->SetStringField(TEXT("dependency_tier"),TEXT("EngineAPI: existing saved generated BPC/native Actor source inspection, public UObject Rename/NewObject and SCS/editor compile/save APIs; no ServerRPC, custom runtime component or external bridge."));
    R.Data->SetStringField(TEXT("effect_scope"),TEXT("Exact loaded saved-clean direct-native Actor host, one own flat leaf non-scene generated BPC root to a saved-clean direct ActorComponent BPC class. One retirement/allocation preserves node name/GUID and copies only ComponentTags, as the original non-scene helper did; remaining selected defaults originate from the new class CDO. Public Modify/Rename/constructor/structural/compiler/save callbacks may have implicit engine effects; no atomic rollback."));
    R.Data->SetStringField(TEXT("read_scope"),TEXT("Held current source classes/skeletons/existing CDOs, bounded source defaults/declarations/stored graphs/disk, host own SCS/name/GUID/class/template/topology, selected defaults and bounded unselected CDO/declaration/graph/template/disk/dirty-package state. No complete loaded-reference, delegate purity, runtime or cook proof."));
    R.Data->SetStringField(TEXT("unsupported_original_scope"),TEXT("Selected own pure getter outputs and directly compatible existing links have a finite retarget/reconstruct path. Selected setters, external/local/validated members, old-class-specific incompatible consumers, bound events and template literals remain refused before Modify, as do inherited/scene/child-bearing selected components, other generated SCS classes, pending host descriptor defaults, derived/live/archetype consumers and instanced/default-subobject propagation. Complete Touch/Offset pipelines remain unverified."));
    R.Data->SetStringField(TEXT("retry_risk"),TEXT("A write request may retire/allocate again; any unknown callback/compile/save outcome must be inspected and must not be automatically retried."));
    R.Data->SetStringField(TEXT("asset_load_count_scope"),TEXT("Explicit loader calls only: no asset/class loader fallback is dispatched. Reflected export and public engine callbacks can resolve references or cause uninstrumented implicit work; zero explicit loads is not global reference-load purity."));
    const bool Passed=[&]()->bool
    {
        if(!FMath::IsFinite(SphereRadius))return Fail(R,TEXT("component_replace_radius_invalid"),TEXT("Finite public radius required; this non-scene branch does not consume sphere_radius."));
        if(!Bind(C,BlueprintPath,ComponentName,NewComponentClass)||!PrepareSelectedGetters(C))return false;
        if(!PrepareNativeEmptyDelegates(C)||!PrepareCrossSparse(C)||!NoInstancedDefaults(C,C.OldCDO.Get())||!NoInstancedDefaults(C,C.NewCDO.Get())||!NoInstancedDefaults(C,C.OldTemplate.Get()))return false;
        if(C.CrossNativeOld){USCS_Node* Root=nullptr;const auto* SceneRoot=C.SCS->GetSceneRootComponentTemplate(false,&Root);if(!C.Current())return false;if(Root!=C.CrossRoot.Get()||SceneRoot!=C.CrossRoot->ComponentTemplate.Get())return Fail(R,TEXT("component_replace_cross_root_selection_pending"),TEXT("Existing actual own non-default Scene root required; no default-root cache creation or normalization is allowed."));R.Data->SetBoolField(TEXT("cross_scene_to_non_scene_leaf"),true);R.Data->SetStringField(TEXT("unsupported_original_scope"),TEXT("Cross Scene root/child-bearing/reverse/mixed native targets, nonempty sparse bindings/owned subobjects, selected setters/bound events/literals and incompatible typed consumers remain incomplete. Native Scene leaf to direct ActorComponent BPC is implemented using the existing writer; usage/compile/save validation pending."));R.Data->SetStringField(TEXT("effect_scope"),TEXT("Raw require_scene_compatibility=false: own non-root native Box/Sphere/Capsule leaf below an existing non-default Scene root to saved-clean direct ActorComponent BPC. Existing node identities/default-root cache remain; successful full compile may move exactly the selected non-Scene leaf from its one local parent to the root set; original cross-class copy is ComponentTags only, target defaults originate from its CDO. Exact source/template/delegate/getter/current/partial/compiler/save guards retained. Root, child-bearing and reverse cross transitions remain incomplete; actual usage validation pending."));}
        C.NativeEmptyGuardReady=true;if(!C.Current())return false;R.Data->SetBoolField(TEXT("native_empty_delegate_current_guard_active"),true);
        auto SourceBefore=MakeShared<FJsonObject>(),TargetBefore=MakeShared<FJsonObject>(),DiskBefore=MakeShared<FJsonObject>(),OtherDirty=MakeShared<FJsonObject>(),OldValues=MakeShared<FJsonObject>(),NewDefaults=MakeShared<FJsonObject>();
        if(!SourceState(C,SourceBefore)||!TargetState(C,TargetBefore)||!C.Props(C.OldTemplate.Get(),{},OldValues)||!C.Props(C.NewCDO.Get(),{TEXT("ComponentTags")},NewDefaults)||!Disk(C.HostPackage,DiskBefore)||!Dirty(C.HostPackage,OtherDirty)||!C.Current())return C.Stopped?false:C.Stop(TEXT("component_replace_generated_before_unobservable"),TEXT("Bounded actual source/target/default/disk observation incomplete; no Modify."));
        C.OldFieldsBefore=OldValues;const TArray<FName> Tags=C.OldTemplate->ComponentTags;const FString OldPath=C.OldTemplate->GetPathName();
        if(!DiskBefore->GetBoolField(TEXT("exists")))return Fail(R,TEXT("component_replace_generated_host_not_saved"),TEXT("Host must have an existing saved package file."));
        if(Text(TargetBefore).Contains(OldPath,ESearchCase::CaseSensitive))return Fail(R,TEXT("component_replace_generated_unselected_reference_pending"),TEXT("A bounded unselected field references the selected template; propagation is not part of this slice and is refused before Modify."));
        auto Before=MakeShared<FJsonObject>();Before->SetObjectField(TEXT("component"),Identity(C.OldTemplate.Get()));Before->SetStringField(TEXT("variable_name"),C.SelectedName);Before->SetStringField(TEXT("variable_guid"),C.Guid.ToString());Before->SetStringField(TEXT("component_class_path"),C.OldClass->GetPathName());Before->SetObjectField(TEXT("selected_properties"),OldValues);R.Data->SetObjectField(TEXT("before"),Before);
        R.Data->SetObjectField(TEXT("source_before"),SourceBefore);R.Data->SetObjectField(TEXT("unselected_target_before"),TargetBefore);R.Data->SetObjectField(TEXT("disk_before"),DiskBefore);R.Data->SetObjectField(TEXT("other_dirty_before"),OtherDirty);R.Data->SetStringField(TEXT("new_component_class"),C.NewClass->GetName());R.Data->SetStringField(TEXT("new_component_class_path"),C.NewClass->GetPathName());
        R.Data->SetStringField(TEXT("non_scene_default_copy_policy"),TEXT("Original non-scene behavior: new class CDO defaults, then old ComponentTags only. Old custom variable overrides are not migrated."));R.Data->SetObjectField(TEXT("new_class_default_properties_without_tags"),NewDefaults);R.Data->SetBoolField(TEXT("sphere_radius_consumed"),false);
        if(DryRun)
        {
            auto Preview=MakeShared<FJsonObject>();Preview->SetStringField(TEXT("variable_name"),C.SelectedName);Preview->SetStringField(TEXT("variable_guid"),C.Guid.ToString());Preview->SetStringField(TEXT("component_class_path"),C.NewClass->GetPathName());Preview->SetStringField(TEXT("expected_template_slot"),C.Slot);Preview->SetObjectField(TEXT("new_class_default_properties_without_tags"),NewDefaults);Preview->SetBoolField(TEXT("component_tags_copy_planned"),true);R.Data->SetObjectField(TEXT("after_preview"),Preview);R.Data->SetStringField(TEXT("write_scope"),TEXT("dry_run_no_asset_mutation"));
            if(!Boundary(C,SourceBefore,TargetBefore,DiskBefore,OtherDirty,TEXT("after_preview_observation")))return false;
            auto ActualOld=MakeShared<FJsonObject>();if(!C.Props(C.OldTemplate.Get(),{},ActualOld))return false;
            const bool Same=Equal(OldValues,ActualOld);R.Data->SetBoolField(TEXT("independent_readback_passed"),Same);if(!Same)return C.Stop(TEXT("component_replace_generated_preview_changed"),TEXT("Actual selected defaults changed during preview; no writer was dispatched."));R.Stage=TEXT("complete");return true;
        }
        R.Stage=TEXT("modify_selected_scope");if(!ModifyOne(C,C.Host.Get(),TEXT("host_modify_returned"))||!ModifyOne(C,C.SCS.Get(),TEXT("scs_modify_returned"))||!ModifyOne(C,C.Selected.Get(),TEXT("node_modify_returned"))||!ModifyOne(C,C.OldTemplate.Get(),TEXT("old_template_modify_returned")))return false;
        auto PreRetireSources=MakeShared<FJsonObject>(),PreRetireOld=MakeShared<FJsonObject>();if(!SourceState(C,PreRetireSources)||!C.Props(C.OldTemplate.Get(),{},PreRetireOld)||!NoConsumers(C))return false;
        if(!Equal(SourceBefore,PreRetireSources)||!Equal(OldValues,PreRetireOld))return C.Stop(TEXT("component_replace_generated_pre_retirement_changed"),TEXT("Source or selected defaults changed during Modify; no retirement dispatched."));
        if(!Fresh(C,C.OldTemplate.Get()))return C.Stopped?false:C.Stop(TEXT("component_replace_generated_pre_retirement_not_fresh"),TEXT("Selected owner/noWorld/fresh scope changed before Rename."));
        R.Stage=TEXT("retire_old_template");R.bKnown=false;Count(R,TEXT("rename_calls"));const bool Renamed=C.OldTemplate->Rename(nullptr,GetTransientPackage(),REN_DoNotDirty|REN_DontCreateRedirectors);R.Data->SetBoolField(TEXT("old_template_rename_returned"),Renamed);
        if(!AdoptRetirement(C,Renamed))return false;
        if(!Fresh(C,C.OldTemplate.Get()))return C.Stopped?false:C.Stop(TEXT("component_replace_generated_pre_allocation_not_fresh"),TEXT("Retired held template lost its ownerless/noWorld fresh role; allocation suppressed."));
        R.Stage=TEXT("allocate_replacement_template");Count(R,TEXT("template_allocation_calls"));UActorComponent* Returned=NewObject<UActorComponent>(C.HostClass.Get(),C.NewClass.Get(),FName(*C.Slot),RF_ArchetypeObject|RF_Transactional|RF_Public,C.NewCDO.Get());
        if(!AdoptAllocation(C,Returned)||!ModifyOne(C,C.NewTemplate.Get(),TEXT("new_template_modify_returned")))return false;
        if(!Fresh(C,C.NewTemplate.Get()))return C.Stopped?false:C.Stop(TEXT("component_replace_generated_pre_copy_not_fresh"),TEXT("Returned replacement lost its ownerless/noWorld fresh role; tags writer suppressed."));
        R.Stage=TEXT("copy_non_scene_defaults");Count(R,TEXT("common_copy_calls"));C.NewTemplate->ComponentTags=Tags;R.Data->SetBoolField(TEXT("component_tags_assignment_performed"),true);if(!C.Current())return false;
        auto PreAssignSources=MakeShared<FJsonObject>();if(!SourceState(C,PreAssignSources)||!NoConsumers(C))return false;if(!Equal(SourceBefore,PreAssignSources))return C.Stop(TEXT("component_replace_generated_source_changed_before_assignment"),TEXT("Source observations changed after constructor callbacks; no node assignment."));
        if(!Fresh(C,C.NewTemplate.Get()))return C.Stopped?false:C.Stop(TEXT("component_replace_generated_pre_assignment_not_fresh"),TEXT("Replacement lost its ownerless/noWorld fresh role; SCS assignment suppressed."));
        R.Stage=TEXT("assign_scs_template");C.Selected->ComponentClass=C.NewClass.Get();C.Selected->ComponentTemplate=C.NewTemplate.Get();C.Assigned=true;R.Data->SetBoolField(TEXT("scs_assignment_performed"),true);if(!C.Current())return false;
        if(!Fresh(C,C.NewTemplate.Get()))return C.Stopped?false:C.Stop(TEXT("component_replace_generated_pre_root_validation_not_fresh"),TEXT("Replacement lost its ownerless/noWorld fresh role; scene-root validation suppressed."));
        C.SCS->ValidateSceneRootNodes();R.Data->SetBoolField(TEXT("scene_root_validation_returned"),true);if(!C.Current())return false;
        if(!SelectedDefaults(C,Tags,NewDefaults,TEXT("after_assignment_selected"))||!Boundary(C,SourceBefore,TargetBefore,DiskBefore,OtherDirty,TEXT("after_assignment_observation")))return false;
        R.Data->SetBoolField(TEXT("after_assignment_preserved"),true);R.Data->SetStringField(TEXT("write_scope"),TEXT("blueprint_scs_component_template_replacement"));
        if(!Fresh(C,C.NewTemplate.Get()))return C.Stopped?false:C.Stop(TEXT("component_replace_generated_pre_structural_not_fresh"),TEXT("Replacement lost its ownerless/noWorld fresh role; structural refresh suppressed."));
        R.Stage=TEXT("structural_refresh");R.bKnown=false;Count(R,TEXT("structural_refresh_calls"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(C.Host.Get());R.Data->SetBoolField(TEXT("structural_refresh_returned"),true);
        if(!RebindHost(C,false)||!SelectedMember(C,C.HostSkeleton.Get(),TEXT("skeleton_selected_member_verified"))||!RepairSelectedGetters(C)||!SelectedDefaults(C,Tags,NewDefaults,TEXT("after_structural_selected"))||!Boundary(C,SourceBefore,TargetBefore,DiskBefore,OtherDirty,TEXT("after_structural_observation")))return false;
        R.Data->SetBoolField(TEXT("independent_readback_passed"),true);auto After=MakeShared<FJsonObject>();After->SetStringField(TEXT("variable_name"),C.SelectedName);After->SetStringField(TEXT("variable_guid"),C.Guid.ToString());After->SetStringField(TEXT("component_class_path"),C.NewClass->GetPathName());After->SetObjectField(TEXT("component"),Identity(C.NewTemplate.Get()));R.Data->SetObjectField(TEXT("after"),After);
        if(!Compile){R.Data->SetBoolField(TEXT("saved"),false);R.Stage=TEXT("complete");return true;}
        if(!Fresh(C,C.NewTemplate.Get()))return C.Stopped?false:C.Stop(TEXT("component_replace_generated_pre_compile_not_fresh"),TEXT("Replacement lost its ownerless/noWorld fresh role; compiler suppressed."));
        R.Stage=TEXT("compile_blueprint");R.bKnown=false;R.bCompileAttempted=true;Count(R,TEXT("full_compile_attempts"));auto Compiler=Cubeless::UE58::CompileBlueprint(C.HostPath,false,Refresh);R.Data->SetObjectField(TEXT("compile_result"),Compiler);bool Compiled=false;if(Compiler->TryGetBoolField(TEXT("compiled"),Compiled))R.Compiled=Compiled;
        // Failed/unknown compiler dispatch may already have retired raw pins. Preserve its receipt and stop without old host pin readback.
        if(!R.Compiled.IsSet())return C.Stop(TEXT("component_replace_compile_outcome_unobservable"),TEXT("Compiler outcome missing; actual prefix retained, no save or retry."));
        if(!Compiled)return C.Stop(TEXT("component_replace_compile_failed"),TEXT("Compiler reported failure; actual mutation/compile prefix retained, pin endpoint observation incomplete, no further readback/save/retry."));
        bool Refreshed=false,RefreshRequested=false;Compiler->TryGetBoolField(TEXT("refreshed_nodes"),Refreshed);Compiler->TryGetBoolField(TEXT("refresh_nodes_requested"),RefreshRequested);
        const bool CompilerRefreshedPins=Compiled&&Refresh&&RefreshRequested&&Refreshed;R.Data->SetBoolField(TEXT("compiler_host_pin_refresh_admission_requested"),CompilerRefreshedPins);
        if(!RebindHost(C,Compiled,CompilerRefreshedPins)||!SelectedMember(C,C.HostSkeleton.Get(),TEXT("skeleton_selected_member_verified"))||!SelectedDefaults(C,Tags,NewDefaults,TEXT("after_compile_selected"))||!Boundary(C,SourceBefore,TargetBefore,DiskBefore,OtherDirty,TEXT("after_compile_observation"),false,Compiled))return false;
        if(!SelectedMember(C,C.HostClass.Get(),TEXT("compiled_selected_member_verified")))return false;
        if(!Save){R.Stage=TEXT("complete");return true;}
        auto SaveBefore=MakeShared<FJsonObject>();if(!TargetState(C,SaveBefore))return false;
        if(!Fresh(C,C.NewTemplate.Get()))return C.Stopped?false:C.Stop(TEXT("component_replace_generated_pre_save_not_fresh"),TEXT("Replacement lost its ownerless/noWorld fresh role; save suppressed."));
        R.Stage=TEXT("save_asset");R.bKnown=false;R.bSaveAttempted=true;Count(R,TEXT("save_attempt_count"));auto SaveReceipt=MakeShared<FJsonObject>();SaveReceipt->SetStringField(TEXT("asset_path"),C.HostPath);SaveReceipt->SetBoolField(TEXT("attempted"),true);R.Data->SetArrayField(TEXT("save_calls"),{MakeShared<FJsonValueObject>(SaveReceipt)});
        R.bSaved=UEditorAssetLibrary::SaveLoadedAsset(C.Host.Get(),false);SaveReceipt->SetBoolField(TEXT("success"),R.bSaved);R.Data->SetBoolField(TEXT("saved"),R.bSaved);
        if(!R.bSaved)return C.Stop(TEXT("component_replace_save_failed_or_unobservable"),TEXT("Save returned false; actual disk effects are uncertain and are never retried automatically."));
        if(!AdoptSaveFlags(C)||!SelectedDefaults(C,Tags,NewDefaults,TEXT("after_save_selected"))||!Boundary(C,SourceBefore,SaveBefore,DiskBefore,OtherDirty,TEXT("after_save_observation"),true))return false;
        if(C.Dirty)return Fail(R,TEXT("component_replace_saved_postcondition_failed"),TEXT("Save returned true but current target remains dirty; actual saved prefix retained."));
        R.Data->SetBoolField(TEXT("saved_state_preserved"),true);R.Data->SetBoolField(TEXT("independent_readback_passed"),true);R.Stage=TEXT("complete");return true;
    }();
    R.bSuccess=Passed;R.bPost=Passed;return Encode(R);
}


}

// LE GeneratedReplacement_End

// QD native Box->Sphere: reuse the existing concrete getter plan/writer, not its generated source admission.
namespace GeneratedReplacement
{
TArray<FField> OverlapFields(UStruct* Owner)
{
    TArray<FField> Out;for(TFieldIterator<FProperty> It(Owner,EFieldIterationFlags::IncludeDeprecated);It;++It){auto* P=*It;Out.Add({P,P->GetOwnerStruct(),P->GetClass(),P->GetFName(),P->GetPropertyFlags(),P->GetSize(),P->GetOffset_ForInternal(),P->ArrayDim});}return Out;
}
bool OverlapFieldsCurrent(UStruct* Owner,const TArray<FField>& Before)
{
    const auto Now=OverlapFields(Owner);if(Now.Num()!=Before.Num())return false;
    for(int32 I=0;I<Now.Num();++I){const auto& A=Before[I];const auto& B=Now[I];if(A.Property!=B.Property||A.Owner!=B.Owner||A.Kind!=B.Kind||A.Name!=B.Name||A.Flags!=B.Flags||A.Size!=B.Size||A.Offset!=B.Offset||A.Dim!=B.Dim)return false;}return true;
}
FMulticastSparseDelegateProperty* OverlapProperty()
{
    for(TFieldIterator<FProperty> It(UPrimitiveComponent::StaticClass(),EFieldIterationFlags::IncludeDeprecated);It;++It)if(It->GetFName()==TEXT("OnComponentBeginOverlap"))return CastField<FMulticastSparseDelegateProperty>(*It);return nullptr;
}
bool OverlapTupleSame(const FBlueprintComponentDelegateBinding& A,const FBlueprintComponentDelegateBinding& B)
{return A.ComponentPropertyName==B.ComponentPropertyName&&A.DelegatePropertyName==B.DelegatePropertyName&&A.FunctionNameToBind==B.FunctionNameToBind;}
bool OverlapTuplesSame(const TArray<FBlueprintComponentDelegateBinding>& A,const TArray<FBlueprintComponentDelegateBinding>& B)
{
    if(A.Num()!=B.Num())return false;TSet<int32> Used;for(const auto& E:A){int32 Match=INDEX_NONE;for(int32 I=0;I<B.Num();++I)if(!Used.Contains(I)&&OverlapTupleSame(E,B[I])){Match=I;break;}if(Match==INDEX_NONE)return false;Used.Add(Match);}return true;
}
bool NativeOverlapCurrent(FScope& C,bool Rebinding)
{
    if(!C.Overlap)return true;const auto& E=*C.Overlap;auto* N=E.Node.Get();
    const auto Bad=[&](const TCHAR* Field,const FString& Expected=TEXT("captured overlap role"),const FString& Actual=TEXT("different")){if(!C.R.Data->HasField(TEXT("selected_overlap_first_mismatch"))){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),C.R.Stage);J->SetStringField(TEXT("field"),Field);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);J->SetBoolField(TEXT("additional_uobject_queries"),false);C.R.Data->SetObjectField(TEXT("selected_overlap_first_mismatch"),J);}return C.Stop(TEXT("component_replace_overlap_current_changed"),TEXT("Captured overlap source/event/binding role changed; no next SDK or retry."));};
    if(!Loaded(N)||N->GetClass()!=UK2Node_ComponentBoundEvent::StaticClass())return Bad(TEXT("event_lifetime_exact_class"));
    if(N->ComponentPropertyName!=E.Component)return Bad(TEXT("component_property_name"),E.Component.ToString(),N->ComponentPropertyName.ToString());
    if(N->DelegatePropertyName!=E.Delegate)return Bad(TEXT("delegate_property_name"),E.Delegate.ToString(),N->DelegatePropertyName.ToString());
    if(N->CustomFunctionName!=E.Function)return Bad(TEXT("custom_function_name"),E.Function.ToString(),N->CustomFunctionName.ToString());
    if(!N->DelegateOwnerClass.IsResolved()||N->DelegateOwnerClass.Get()!=UPrimitiveComponent::StaticClass())return Bad(TEXT("delegate_owner_binding"));
    if(!N->EventReference.IsSameReference(E.Reference))return Bad(TEXT("event_reference"),E.Reference.GetMemberName().ToString(),N->EventReference.GetMemberName().ToString());
    if(N->FunctionFlags!=E.Flags)return Bad(TEXT("function_flags"),LexToString(E.Flags),LexToString(N->FunctionFlags));
    if(bool(N->bOverrideFunction)!=E.Override||bool(N->bInternalEvent)!=E.Internal||!N->UserDefinedPins.IsEmpty())return Bad(TEXT("override_internal_user_pins"));
    if(!E.Ready)return true;
    if(FindFProperty<FMulticastDelegateProperty>(UBoxComponent::StaticClass(),E.Delegate)!=E.DelegateField.Property||FindFProperty<FMulticastDelegateProperty>(USphereComponent::StaticClass(),E.Delegate)!=E.DelegateField.Property)return Bad(TEXT("common_native_delegate_lookup"));
    auto* P=OverlapProperty();const auto& RADelegateField=E.DelegateField;
    if(!P||P!=RADelegateField.Property||P->GetClass()!=RADelegateField.Kind||P->GetOwnerStruct()!=RADelegateField.Owner||P->GetPropertyFlags()!=RADelegateField.Flags||P->GetSize()!=RADelegateField.Size||P->GetOffset_ForInternal()!=RADelegateField.Offset||P->ArrayDim!=RADelegateField.Dim||!P->SignatureFunction.IsResolved()||P->SignatureFunction.Get()!=E.Signature.Get())return Bad(TEXT("native_sparse_delegate_descriptor"));
    auto* S=E.Signature.Get();if(!Loaded(S)||S->GetClass()!=USparseDelegateFunction::StaticClass()||S->GetOutermost()!=UPrimitiveComponent::StaticClass()->GetOutermost()||S->FunctionFlags!=E.NativeFlags||S->NumParms!=E.NativeNumParms||S->ParmsSize!=E.NativeParmsSize||S->OwningClassName!=E.SparseOwner||S->DelegateName!=E.SparseDelegate)return Bad(TEXT("native_signature_role"));
    TArray<FField> Fields;for(const auto& A:E.Params)Fields.Add(A.Field);if(!OverlapFieldsCurrent(S,Fields))return Bad(TEXT("native_signature_field_inventory_layout"));
    for(const auto& A:E.Params)
    {
        auto* Q=A.Field.Property;
        if(auto* O=CastField<FObjectProperty>(Q)){if(!O->PropertyClass.IsResolved()||O->PropertyClass.Get()!=A.TypeObject.Get())return Bad(TEXT("native_parameter_object_binding"));}
        else if(auto* V=CastField<FStructProperty>(Q)){if(!V->Struct.IsResolved()||V->Struct.Get()!=A.TypeObject.Get())return Bad(TEXT("native_parameter_struct_binding"));}
    }
    if(!E.DerivedReady)return Rebinding?true:Bad(TEXT("derived_return_not_ready"));
    {
        if(!Loaded(E.OwnOwner.Get())||!Loaded(E.OwnSignature.Get())||(!Rebinding&&E.OwnOwner.Get()!=C.HostSkeleton.Get())||E.OwnSignature->GetOuter()!=E.OwnOwner.Get()||E.OwnSignature->GetFName()!=E.Function||E.OwnSignature->FunctionFlags!=E.OwnFlags||E.OwnSignature->NumParms!=E.OwnNumParms||E.OwnSignature->ParmsSize!=E.OwnParmsSize||!OverlapFieldsCurrent(E.OwnSignature.Get(),E.OwnFields))return Bad(TEXT("current_own_event_signature"));
        if(!C.Host->SkeletonGeneratedClass.GetGCPtr().IsResolved()||C.Host->SkeletonGeneratedClass.Get()!=E.OwnOwner.Get())return Bad(TEXT("returned_own_signature_owner_slot"));
        for(const auto& F:E.OwnFields)
        {
            const auto* A=E.Params.FindByPredicate([&](const FOverlapParam& V){return V.Field.Name==F.Name;});if(!A)return Bad(TEXT("own_parameter_name"));
            if(auto* O=CastField<FObjectProperty>(F.Property)){if(!O->PropertyClass.IsResolved()||O->PropertyClass.Get()!=A->TypeObject.Get())return Bad(TEXT("own_parameter_object_binding"));}
            else if(auto* V=CastField<FStructProperty>(F.Property)){if(!V->Struct.IsResolved()||V->Struct.Get()!=A->TypeObject.Get())return Bad(TEXT("own_parameter_struct_binding"));}
        }
        if(!Loaded(E.BindingOwner.Get())||(!Rebinding&&E.BindingOwner.Get()!=C.HostClass.Get())||E.BindingOwner->DynamicBindingObjects.Num()!=E.Bindings.Num())return Bad(TEXT("current_binding_owner_inventory"));
        if(!C.Host->GeneratedClass.GetGCPtr().IsResolved()||C.Host->GeneratedClass.Get()!=E.BindingOwner.Get())return Bad(TEXT("returned_binding_owner_slot"));
        for(int32 I=0;I<E.Bindings.Num();++I)
        {
            const auto& H=E.BindingOwner->DynamicBindingObjects[I];auto* B=E.Bindings[I].Get();if(!H.IsResolved()||H.Get()!=B||!Loaded(B)||B->GetClass()!=UComponentDelegateBinding::StaticClass()||B->GetOuter()!=E.BindingOwner.Get()||B->ComponentDelegateBindings.Num()!=E.BindingValues[I].Num())return Bad(TEXT("current_component_binding_object_role"));
            for(int32 J=0;J<E.BindingValues[I].Num();++J)if(!OverlapTupleSame(B->ComponentDelegateBindings[J],E.BindingValues[I][J]))return Bad(TEXT("current_component_binding_tuple"));
        }
    }
    return true;
}
bool NativeOverlapReturn(FScope& C,UBlueprintGeneratedClass* Generated,UBlueprintGeneratedClass* Skeleton)
{
    if(!C.Overlap)return true;auto& E=*C.Overlap;
    if(!NativeOverlapCurrent(C,true))return false;
    // Current returned owners are already strongly held by the caller. Retired derived objects remain held, but are never an identity invariant.
    TStrongObjectPtr<UFunction> Own;for(TFieldIterator<UFunction> It(Skeleton,EFieldIterationFlags::IncludeDeprecated);It;++It)if(It->GetFName()==E.Function){Own.Reset(*It);break;}
    if(!Loaded(Own.Get())||Own->GetOuter()!=Skeleton)return C.Stop(TEXT("component_replace_overlap_own_signature_missing"),TEXT("Returned skeleton has no direct event function with the preserved CustomFunctionName."));
    const auto OwnFields=OverlapFields(Own.Get());if(OwnFields.Num()!=E.Params.Num())return C.Stop(TEXT("component_replace_overlap_own_signature_changed"),TEXT("Returned own event parameter count differs from native delegate signature."));
    for(int32 I=0;I<OwnFields.Num();++I)
    {
        const auto& F=OwnFields[I];const auto* A=E.Params.FindByPredicate([&](const FOverlapParam& V){return V.Field.Name==F.Name;});
        if(!A)return C.Stop(TEXT("component_replace_overlap_own_signature_changed"),TEXT("Unexpected returned own parameter name."));const auto& Source=A->Field;
        if(auto* O=CastField<FObjectProperty>(F.Property)){if(!O->PropertyClass.IsResolved()||O->PropertyClass.Get()!=A->TypeObject.Get())return C.Stop(TEXT("component_replace_overlap_own_signature_changed"),TEXT("Returned own object parameter binding differs."));}
        else if(auto* V=CastField<FStructProperty>(F.Property)){if(!V->Struct.IsResolved()||V->Struct.Get()!=A->TypeObject.Get())return C.Stop(TEXT("component_replace_overlap_own_signature_changed"),TEXT("Returned own struct parameter binding differs."));}
        if(F.Name!=Source.Name||F.Kind!=Source.Kind||F.Dim!=Source.Dim||!F.Property->HasAnyPropertyFlags(CPF_Parm)||!F.Property->SameType(Source.Property))return C.Stop(TEXT("component_replace_overlap_own_signature_changed"),TEXT("Returned own event parameter names/types differ from its held native signature."));
    }
    TArray<TStrongObjectPtr<UComponentDelegateBinding>> Objects;TArray<TArray<FBlueprintComponentDelegateBinding>> Values;TArray<FBlueprintComponentDelegateBinding> Flat;
    for(const auto& H:Generated->DynamicBindingObjects)
    {
        if(!H.IsResolved())return C.Stop(TEXT("component_replace_overlap_binding_unresolved"),TEXT("Unresolved generated binding handle; no loader fallback."));
        TStrongObjectPtr<UDynamicBlueprintBinding> Hold(H.Get());auto* B=Cast<UComponentDelegateBinding>(Hold.Get());
        if(!Loaded(B)||B->GetClass()!=UComponentDelegateBinding::StaticClass()||B->GetOuter()!=Generated)return C.Stop(TEXT("component_replace_overlap_binding_scope"),TEXT("First overlap slice captures exact component delegate binding objects only."));
        Objects.Emplace(B);Values.Add(B->ComponentDelegateBindings);Flat.Append(B->ComponentDelegateBindings);
    }
    if(E.Ready&&!OverlapTuplesSame(E.ExpectedBindings,Flat))return C.Stop(TEXT("component_replace_overlap_binding_tuple_changed"),TEXT("Compiler-derived binding addresses may change, but original component/delegate/function tuple multiset must remain exact."));
    if(!E.Ready)
    {
        int32 SelectedCount=0;for(const auto& B:Flat)if(B.ComponentPropertyName==E.Component&&B.DelegatePropertyName==E.Delegate&&B.FunctionNameToBind==E.Function)++SelectedCount;
        if(SelectedCount!=1)return Fail(C.R,TEXT("component_replace_overlap_binding_not_compiled"),TEXT("First overlap profile requires exactly one actual selected component/delegate/function binding tuple from the actual compiled source host."));
        E.ExpectedBindings=Flat;
    }
    if(E.OwnSignature)C.NativeReturnedHolds.Emplace(E.OwnSignature.Get());for(const auto& B:E.Bindings)C.NativeReturnedHolds.Emplace(B.Get());
    E.OwnSignature.Reset(Own.Get());E.OwnOwner.Reset(Skeleton);E.OwnFields=OwnFields;E.OwnFlags=Own->FunctionFlags;E.OwnNumParms=Own->NumParms;E.OwnParmsSize=Own->ParmsSize;E.BindingOwner.Reset(Generated);E.Bindings=MoveTemp(Objects);E.BindingValues=MoveTemp(Values);
    E.DelegateType=FEdGraphPinType();E.DelegateType.PinCategory=UEdGraphSchema_K2::PC_Delegate;
    FMemberReference::FillSimpleMemberReference<UFunction>(Own.Get(),E.DelegateType.PinSubCategoryMemberReference);
    E.DerivedReady=true;C.R.Data->SetNumberField(TEXT("selected_overlap_generated_binding_tuple_count"),E.ExpectedBindings.Num());return true;
}
bool NativeOverlapPrepare(FScope& C)
{
    FGraphNode* Selected=nullptr;for(auto& G:C.GraphRows)for(auto& N:G.Nodes)if(N.SelectedOverlap){if(Selected)return Fail(C.R,TEXT("component_replace_overlap_count_unsupported"),TEXT("First overlap profile admits exactly one selected bound event."));Selected=&N;}
    if(!Selected)return true;
    auto Work=MakeShared<FNativeOverlap>();auto& E=*Work;auto* N=static_cast<UK2Node_ComponentBoundEvent*>(Selected->Node.Get());E.Node.Reset(N);E.Reference=N->EventReference;E.Component=N->ComponentPropertyName;E.Delegate=N->DelegatePropertyName;E.Function=N->CustomFunctionName;E.Flags=N->FunctionFlags;E.Override=N->bOverrideFunction;E.Internal=N->bInternalEvent;
    C.R.Data->SetStringField(TEXT("selected_overlap_observed_delegate"),E.Delegate.ToString());C.R.Data->SetStringField(TEXT("selected_overlap_observed_component"),E.Component.ToString());C.R.Data->SetStringField(TEXT("selected_overlap_expected_delegate"),TEXT("OnComponentBeginOverlap"));
    if(N->GetClass()!=UK2Node_ComponentBoundEvent::StaticClass()||E.Delegate!=TEXT("OnComponentBeginOverlap")||!N->DelegateOwnerClass.IsResolved()||N->DelegateOwnerClass.Get()!=UPrimitiveComponent::StaticClass()||E.Function.IsNone()||E.Override||!E.Internal||!N->UserDefinedPins.IsEmpty())return Fail(C.R,TEXT("component_replace_overlap_event_unsupported"),TEXT("One own exact UPrimitiveComponent.OnComponentBeginOverlap bound event with its actual nonoverride internal function/reference is required."));
    C.Overlap=Work;
    auto* P=OverlapProperty();if(!P||P->GetClass()!=FMulticastSparseDelegateProperty::StaticClass()||P->GetOwnerClass()!=UPrimitiveComponent::StaticClass()||!P->SignatureFunction.IsResolved())return Fail(C.R,TEXT("component_replace_overlap_delegate_unavailable"),TEXT("Existing exact native sparse delegate property/signature required; no load or CDO warm."));
    E.Signature.Reset(Cast<USparseDelegateFunction>(P->SignatureFunction.Get()));if(!Loaded(E.Signature.Get())||E.Signature->GetOutermost()!=UPrimitiveComponent::StaticClass()->GetOutermost()||E.Signature->OwningClassName!=TEXT("PrimitiveComponent")||E.Signature->DelegateName!=TEXT("OnComponentBeginOverlap")||!P->HasAnyPropertyFlags(CPF_BlueprintAssignable))return Fail(C.R,TEXT("component_replace_overlap_signature_unavailable"),TEXT("Loaded native assignable BeginOverlap sparse delegate signature required."));
    E.NativeFlags=E.Signature->FunctionFlags;E.NativeNumParms=E.Signature->NumParms;E.NativeParmsSize=E.Signature->ParmsSize;E.SparseOwner=E.Signature->OwningClassName;E.SparseDelegate=E.Signature->DelegateName;
    E.DelegateField={P,P->GetOwnerStruct(),P->GetClass(),P->GetFName(),P->GetPropertyFlags(),P->GetSize(),P->GetOffset_ForInternal(),P->ArrayDim};
    FMemberReference NativeReference;NativeReference.SetFromField<UFunction>(E.Signature.Get(),false);if(!E.Reference.IsSameReference(NativeReference))return Fail(C.R,TEXT("component_replace_overlap_reference_unsupported"),TEXT("Actual event reference does not name the held native delegate signature."));
    const auto Fields=OverlapFields(E.Signature.Get());if(Fields.Num()!=6)return Fail(C.R,TEXT("component_replace_overlap_signature_unsupported"),TEXT("Actual native BeginOverlap six-parameter signature required."));
    for(const auto& F:Fields)
    {
        if(!F.Property->HasAnyPropertyFlags(CPF_Parm)||F.Property->HasAnyPropertyFlags(CPF_ReturnParm)||F.Dim!=1)return Fail(C.R,TEXT("component_replace_overlap_signature_unsupported"),TEXT("Native delegate parameter shape unsupported."));
        FOverlapParam A;A.Field=F;if(auto* O=CastField<FObjectProperty>(F.Property)){if(!O->PropertyClass.IsResolved())return Fail(C.R,TEXT("component_replace_overlap_parameter_unresolved"),TEXT("Native parameter class must already be resolved."));A.TypeObject.Reset(O->PropertyClass.Get());}
        else if(auto* V=CastField<FStructProperty>(F.Property)){if(!V->Struct.IsResolved())return Fail(C.R,TEXT("component_replace_overlap_parameter_unresolved"),TEXT("Native parameter struct must already be resolved."));A.TypeObject.Reset(V->Struct.Get());}
        else if(F.Kind!=FIntProperty::StaticClass()&&F.Kind!=FBoolProperty::StaticClass())return Fail(C.R,TEXT("component_replace_overlap_signature_unsupported"),TEXT("Only actual native BeginOverlap parameter descriptors are admitted."));
        if(A.TypeObject&&C.Role(A.TypeObject.Get())==INDEX_NONE)return false;E.Params.Add(MoveTemp(A));
    }
    for(UObject* O:{static_cast<UObject*>(UPrimitiveComponent::StaticClass()),static_cast<UObject*>(E.Signature.Get())})if(C.Role(O)==INDEX_NONE)return false;
    UObject* Schema=ExistingCDO(UEdGraphSchema_K2::StaticClass());E.Schema.Reset(Cast<UEdGraphSchema_K2>(Schema));if(!Loaded(E.Schema.Get())||C.Role(E.Schema.Get())==INDEX_NONE)return Fail(C.R,TEXT("component_replace_overlap_schema_unavailable"),TEXT("Already-existing native K2 schema CDO required."));
    // Hold source metadata and derived roles before any schema callback. Ready turns on only after the complete plain source inventory exists.
    if(!NativeOverlapReturn(C,C.HostClass.Get(),C.HostSkeleton.Get()))return false;E.Ready=true;if(!C.Current())return false;
    TSet<FName> Names;TSet<FGuid> PinIds;int32 Exec=0,Delegate=0;
    for(auto& A:E.Params){if(!E.Schema->ConvertPropertyToPinType(A.Field.Property,A.PinType)||!C.Current())return false;}
    if(Selected->PinRows.Num()!=E.Params.Num()+2)return Fail(C.R,TEXT("component_replace_overlap_pin_shape_unsupported"),TEXT("Actual overlap event must have delegate, Then and native parameter pins."));
    for(const auto& Pin:Selected->PinRows)
    {
        if(PinIds.Contains(Pin.Id)||Names.Contains(Pin.Name)||Pin.Direction!=EGPD_Output||Pin.Parent||!Pin.Children.IsEmpty()||Pin.Orphan)return Fail(C.R,TEXT("component_replace_overlap_pin_shape_unsupported"),TEXT("Unique whole nonorphan output pins required; split event parameters remain incomplete."));Names.Add(Pin.Name);PinIds.Add(Pin.Id);
        FEdGraphPinType Expected;
        if(Pin.Name==UEdGraphSchema_K2::PN_Then){Expected.PinCategory=UEdGraphSchema_K2::PC_Exec;++Exec;}
        else if(Pin.Name==UK2Node_Event::DelegateOutputName){Expected=E.DelegateType;++Delegate;if(!Pin.Links.IsEmpty())return Fail(C.R,TEXT("component_replace_overlap_delegate_consumer_pending"),TEXT("Event delegate-output consumers remain incomplete; exec/data links are preserved."));}
        else{const auto* A=E.Params.FindByPredicate([&](const FOverlapParam& V){return V.Field.Name==Pin.Name;});if(!A)return Fail(C.R,TEXT("component_replace_overlap_pin_shape_unsupported"),TEXT("Unexpected event parameter pin."));Expected=A->PinType;}
        if(!FScope::PinTypeSame(Pin.Type,Expected))return Fail(C.R,TEXT("component_replace_overlap_pin_signature_mismatch"),TEXT("Actual event full pin type differs from public native signature conversion."));
    }
    if(Exec!=1||Delegate!=1)return Fail(C.R,TEXT("component_replace_overlap_pin_shape_unsupported"),TEXT("Exactly one Then and delegate output required."));
    // GetTargetDelegateProperty is public and follows the event owner/name. Treat its return as a descriptor address until Current succeeds.
    auto* Actual=N->GetTargetDelegateProperty();if(!C.Current())return false;if(Actual!=P)return Fail(C.R,TEXT("component_replace_overlap_target_delegate_mismatch"),TEXT("Public event resolution differs from held native common delegate."));
    C.R.Data->SetBoolField(TEXT("native_scene_selected_overlap_replacement"),true);C.R.Data->SetNumberField(TEXT("selected_overlap_event_count"),1);C.R.Data->SetStringField(TEXT("selected_overlap_delegate"),TEXT("/Script/Engine.PrimitiveComponent:OnComponentBeginOverlap"));C.R.Data->SetStringField(TEXT("selected_overlap_function_name"),E.Function.ToString());C.R.Data->SetNumberField(TEXT("selected_overlap_parameter_pin_count"),E.Params.Num());C.R.Data->SetStringField(TEXT("selected_overlap_node_guid"),Selected->Guid.ToString());C.R.Data->SetBoolField(TEXT("selected_overlap_runtime_verified"),false);return C.Current();
}

bool NativeGraphRowsCurrent(FScope& C,const TArray<FGraph>& Rows)
{
    const auto Bad=[&](const FGraph& G,int32 N,int32 P,const TCHAR* Field,const FString& E,const FString& A){return C.GraphMismatch(G,N,P,Field,E,A,TEXT("component_replace_native_get_current_changed"),TEXT("Held native shape getter graph role changed; no next SDK or retry."));};
    for(const auto& G:Rows)
    {
        if(!Loaded(G.Graph.Get())||G.Graph->GetOuter()!=C.Host.Get()||G.Graph->GraphGuid!=G.Guid||!G.Graph->Schema.GetGCPtr().IsResolved()||G.Graph->Schema.GetGCPtr()!=UEdGraphSchema_K2::StaticClass()||!G.Graph->SubGraphs.IsEmpty()||G.Graph->Nodes.Num()!=G.Nodes.Num())return Bad(G,INDEX_NONE,INDEX_NONE,TEXT("graph_role"),TEXT("held own flat K2 graph/GUID/node count"),TEXT("different"));
        TSet<UEdGraphPin*> Inventory;
        for(int32 I=0;I<G.Nodes.Num();++I)
        {
            const auto& N=G.Nodes[I];
            if(!G.Graph->Nodes[I].IsResolved()||G.Graph->Nodes[I].Get()!=N.Node.Get()||!Loaded(N.Node.Get())||N.Node->GetOuter()!=G.Graph.Get()||N.Node->NodeGuid!=N.Guid||N.Node->Pins!=N.Pins)return Bad(G,I,INDEX_NONE,TEXT("node_pin_membership"),TEXT("same held node/GUID/ordered pins"),TEXT("different"));
            if(N.Variable){const auto* V=static_cast<UK2Node_Variable*>(N.Node.Get());if(!V->VariableReference.IsSameReference(N.Reference)||V->VariableReference.IsDeprecated()!=N.Deprecated)return Bad(G,I,INDEX_NONE,TEXT("member_reference"),TEXT("original self/name/GUID/reference"),TEXT("different"));}
            for(auto* P:N.Pins)if(!P||Inventory.Contains(P)||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N.Node.Get())return Bad(G,I,INDEX_NONE,TEXT("pin_lifetime_owner"),TEXT("unique current owned pin"),FScope::PointerLabel(P));else Inventory.Add(P);
        }
        for(int32 I=0;I<G.Nodes.Num();++I)for(int32 J=0;J<G.Nodes[I].PinRows.Num();++J)
        {
            const auto& P=G.Nodes[I].PinRows[J];UEdGraphPin* Q=P.Pin;
            if(!Inventory.Contains(Q))return Bad(G,I,J,TEXT("pin_inventory"),TEXT("present"),TEXT("absent"));
            if(Q->PinId!=P.Id||Q->PinName!=P.Name||Q->Direction!=P.Direction||!FScope::PinTypeSame(Q->PinType,P.Type))return Bad(G,I,J,TEXT("pin_id_name_direction_fulltype"),P.Id.ToString(),TEXT("different"));
            if(!Q->DefaultValue.Equals(P.Default,ESearchCase::CaseSensitive))return Bad(G,I,J,TEXT("raw_default"),P.Default,Q->DefaultValue);
            if(!Q->AutogeneratedDefaultValue.Equals(P.Auto,ESearchCase::CaseSensitive))return Bad(G,I,J,TEXT("autogenerated_default"),P.Auto,Q->AutogeneratedDefaultValue);
            if(!Q->DefaultObject.IsResolved()||!P.Object.IsResolved()||Q->DefaultObject!=P.Object||Q->PinType.PinSubCategoryObject.IsStale()||Q->PinType.PinValueType.TerminalSubCategoryObject.IsStale())return Bad(G,I,J,TEXT("resolved_pin_objects"),TEXT("held resolved objects"),TEXT("different or unresolved"));
            if(FTextInspector::GetSharedDataId(Q->DefaultTextValue)!=FTextInspector::GetSharedDataId(P.Text)||FTextInspector::GetFlags(Q->DefaultTextValue)!=FTextInspector::GetFlags(P.Text)||FTextInspector::GetSharedDataId(Q->PinFriendlyName)!=FTextInspector::GetSharedDataId(P.NativeFriendly)||FTextInspector::GetFlags(Q->PinFriendlyName)!=FTextInspector::GetFlags(P.NativeFriendly))return Bad(G,I,J,TEXT("physical_text_identity_flags"),TEXT("captured physical text identity/flags"),TEXT("different"));
            if(Q->LinkedTo!=P.Links||Q->ParentPin!=P.Parent||Q->SubPins!=P.Children||Q->bHidden!=P.Hidden||Q->bOrphanedPin!=P.Orphan||Q->bAdvancedView!=P.Advanced||Q->bNotConnectable!=P.NotConnectable||Q->bDefaultValueIsReadOnly!=P.DefaultReadOnly||Q->bDefaultValueIsIgnored!=P.DefaultIgnored||Q->ShouldSavePinIfOrphaned()!=P.SaveOrphan||Q->bDeprecated!=P.Deprecated||Q->bDisplayAsMutableRef!=P.MutableRef||Q->bAllowFriendlyName!=P.FriendlyName)return Bad(G,I,J,TEXT("ordered_links_split_wholeflags"),TEXT("captured endpoints/flags"),TEXT("different"));
            for(auto* L:P.Links)if(!Inventory.Contains(L)||!L->LinkedTo.Contains(Q)||L->Direction==Q->Direction)return Bad(G,I,J,TEXT("reciprocal_peer_membership"),TEXT("same graph reciprocal opposite direction"),TEXT("different"));
        }
    }
    return true;
}
bool NativeSceneCurrent(FScope& C,bool Rebinding,bool)
{
    if(C.Stopped)return false;
    if(!FScope::Globals()||GEditor!=C.Editor.Get()||!Loaded(C.Editor.Get())||GUndo!=C.Undo)return C.Stop(TEXT("component_replace_native_get_context_changed"),TEXT("Held Editor/Undo context changed."));
    for(const auto& A:C.Roles)
    {
        UObject* O=A.Object.Get();if(Rebinding&&(O==C.HostClass.Get()||O==C.HostSkeleton.Get()||O==C.HostCDO.Get()))continue;
        const auto RoleBad=[&](const TCHAR* Field,const FString& Expected,const FString& Actual){if(!C.R.Data->HasField(TEXT("native_get_role_first_mismatch"))){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),C.R.Stage);J->SetStringField(TEXT("object_captured_path"),A.Path);J->SetStringField(TEXT("property"),Field);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);J->SetBoolField(TEXT("later_conditions_unobserved"),true);C.R.Data->SetObjectField(TEXT("native_get_role_first_mismatch"),J);}return C.Stop(TEXT("component_replace_native_get_identity_changed"),TEXT("Held native getter role changed; first operands copied without followup queries."));};
        if(!Loaded(O)||!Loaded(A.Class.Get())||!Loaded(A.Package.Get()))return RoleBad(TEXT("held_lifetime"),TEXT("loaded held object/class/package"),TEXT("unavailable"));
        if(O->GetClass()!=A.Class.Get())return RoleBad(TEXT("class"),FScope::PointerLabel(A.Class.Get()),FScope::PointerLabel(O->GetClass()));
        if(O->GetOuter()!=A.Outer.Get())return RoleBad(TEXT("outer"),FScope::PointerLabel(A.Outer.Get()),FScope::PointerLabel(O->GetOuter()));
        if(O->GetOutermost()!=A.Package.Get())return RoleBad(TEXT("package"),FScope::PointerLabel(A.Package.Get()),FScope::PointerLabel(O->GetOutermost()));
        const FString NowPath=O->GetPathName();if(!NowPath.Equals(A.Path,ESearchCase::CaseSensitive))return RoleBad(TEXT("path"),A.Path,NowPath);
        const EObjectFlags Expected=C.NativeSaved&&A.Package.Get()==C.Host->GetPackage()?A.Flags|RF_WasLoaded|RF_LoadCompleted:A.Flags;
        const EObjectFlags Actual=O->GetFlags();if(Actual!=A.Flags&&Actual!=Expected)return RoleBad(TEXT("object_flags"),LexToString(static_cast<uint32>(Expected)),LexToString(static_cast<uint32>(Actual)));
        if(A.Package->GetPackageFlags()!=A.PackageFlags||(A.IsClass&&(static_cast<UClass*>(O)->GetClassFlags()!=A.ClassFlags||static_cast<UClass*>(O)->GetStructureSize()!=A.ClassSize)))return C.Stop(TEXT("component_replace_native_get_layout_changed"),TEXT("Held source package/class layout changed."));
    }
    auto* B=C.Host.Get();if(!Loaded(B)||!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.GetGCPtr()!=AActor::StaticClass()||!B->SimpleConstructionScript.IsResolved()||B->SimpleConstructionScript.Get()!=C.SCS.Get()||!Loaded(C.SCS.Get())||C.SCS->GetAllNodes()!=C.All||C.SCS->GetRootNodes()!=C.Roots||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad)return C.Stop(TEXT("component_replace_native_get_host_changed"),TEXT("Held direct Actor authoring scope changed."));
    // Authored graph categories remain exact. EventGraphs are compiler stubs and are not traversed as authored graphs here.
    const auto Arrays=FScope::Arrays(B);for(int32 I=0;I<4;++I)if(!FScope::SameArrays({C.HostArrays[I]},{Arrays[I]}))return C.Stop(TEXT("component_replace_native_get_authored_graphs_changed"),TEXT("Original authored graph category membership/order changed."));
    if(!Rebinding)
    {
        if(!B->GeneratedClass.GetGCPtr().IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||B->GeneratedClass.Get()!=C.HostClass.Get()||B->SkeletonGeneratedClass.Get()!=C.HostSkeleton.Get()||!C.HostClass->ClassDefaultObject.IsResolved()||C.HostClass->ClassDefaultObject.Get()!=C.HostCDO.Get()||!C.HostClass->ClassGeneratedBy.IsResolved()||C.HostClass->ClassGeneratedBy.Get()!=B||!C.HostSkeleton->ClassGeneratedBy.IsResolved()||C.HostSkeleton->ClassGeneratedBy.Get()!=B||C.HostClass->GetSuperClass()!=AActor::StaticClass()||C.HostSkeleton->GetSuperClass()!=AActor::StaticClass())return C.Stop(TEXT("component_replace_native_get_class_slots_changed"),TEXT("Current generated/skeleton/CDO and GeneratedBy/parent roles differ from the verified compiler return."));
    }
    if(!Rebinding&&!C.NativeReturning)
    {
        C.NativeContext->NativeChecking=true;const bool ShapeOK=ShapeCurrent(*C.NativeContext,C.R);C.NativeContext->NativeChecking=false;if(!ShapeOK){C.Stopped=true;return false;}
    }
    if(!Rebinding&&C.GetterPropertiesReady&&!C.GetterPropertiesCurrent())return false;
    if(!NativeOverlapCurrent(C,Rebinding))return false;
    if(C.GraphsReady&&!NativeGraphRowsCurrent(C,C.NativeCandidatesReady?C.NativeCandidates:C.GraphRows))return false;
    return true;
}
FPin NativePin(UEdGraphPin* Q)
{
    FPin P;P.Pin=Q;P.Id=Q->PinId;P.Name=Q->PinName;P.Direction=Q->Direction;P.Type=Q->PinType;P.Default=Q->DefaultValue;P.Auto=Q->AutogeneratedDefaultValue;P.Text=Q->DefaultTextValue;P.NativeFriendly=Q->PinFriendlyName;P.Object=Q->DefaultObject;P.Links=Q->LinkedTo;P.Children=Q->SubPins;P.Parent=Q->ParentPin;P.Hidden=Q->bHidden;P.Orphan=Q->bOrphanedPin;P.Advanced=Q->bAdvancedView;P.NotConnectable=Q->bNotConnectable;P.DefaultReadOnly=Q->bDefaultValueIsReadOnly;P.DefaultIgnored=Q->bDefaultValueIsIgnored;P.SaveOrphan=Q->ShouldSavePinIfOrphaned();P.Deprecated=Q->bDeprecated;P.MutableRef=Q->bDisplayAsMutableRef;P.FriendlyName=Q->bAllowFriendlyName;return P;
}
bool NativeDisplays(FScope& C,TArray<FGraph>& Rows,bool Rebinding=false)
{
    for(auto& G:Rows)for(auto& N:G.Nodes)for(auto& P:N.PinRows)
    {
        const FText T=P.Text,F=P.NativeFriendly;P.NativeTextDisplay=T.ToString();if(!C.Current(Rebinding))return false;P.NativeFriendlyDisplay=F.ToString();if(!C.Current(Rebinding))return false;
    }
    return true;
}
bool NativeAdoptPins(FScope& C,bool CompilerReturn)
{
    // Close context/source/returned class roles before reading any returned pin. The old physical pin inventory is deliberately not visited.
    const bool WasReady=C.GraphsReady;C.GraphsReady=false;const bool ReturnCurrent=C.Current(CompilerReturn);C.GraphsReady=WasReady;if(!ReturnCurrent)return false;
    // Current graph/node membership first. Never dereference an old pin after structural/compiler/reconstruct dispatch.
    C.NativeCandidates.Reset();C.NativeCandidatesReady=false;TMap<UEdGraphPin*,UEdGraphPin*> Map;
    for(const auto& G:C.GraphRows)
    {
        if(!Loaded(G.Graph.Get())||G.Graph->Nodes.Num()!=G.Nodes.Num())return C.Stop(TEXT("component_replace_native_get_rebind_inventory"),TEXT("Returned graph/node inventory changed."));
        FGraph Next;Next.Graph.Reset(G.Graph.Get());Next.Guid=G.Guid;
        for(int32 I=0;I<G.Nodes.Num();++I)
        {
            const auto& N=G.Nodes[I];if(!G.Graph->Nodes[I].IsResolved()||G.Graph->Nodes[I].Get()!=N.Node.Get()||!Loaded(N.Node.Get())||N.Node->NodeGuid!=N.Guid||N.Node->Pins.Num()!=N.PinRows.Num())return C.Stop(TEXT("component_replace_native_get_rebind_inventory"),TEXT("Returned node/GUID/ordered pin count differs."));
            FGraphNode V;V.Node.Reset(N.Node.Get());V.Guid=N.Guid;V.Pins=N.Node->Pins;V.Variable=N.Variable;V.SelectedOverlap=N.SelectedOverlap;V.Reference=N.Reference;V.Deprecated=N.Deprecated;V.SelectedGetter=N.SelectedGetter;V.GetterRetyped=N.GetterRetyped;V.GetterValueIndex=N.GetterValueIndex;V.GetterBeforeType=N.GetterBeforeType;V.GetterAfterType=N.GetterAfterType;V.GetterBeforeTypeText=N.GetterBeforeTypeText;V.GetterAfterTypeText=N.GetterAfterTypeText;
            for(int32 J=0;J<V.Pins.Num();++J){UEdGraphPin* Q=V.Pins[J];if(!Q||Q->WasTrashed()||Q->GetOwningNodeUnchecked()!=N.Node.Get()||!Q->DefaultObject.IsResolved()||Q->PinType.PinSubCategoryObject.IsStale()||Q->PinType.PinValueType.TerminalSubCategoryObject.IsStale())return C.Stop(TEXT("component_replace_native_get_rebind_pin"),TEXT("Returned current pin role unavailable."));V.PinRows.Add(NativePin(Q));
                if(N.SelectedOverlap){const auto* Prior=N.PinRows.FindByPredicate([&](const FPin& A){return A.Id==Q->PinId;});if(!Prior||Map.Contains(Prior->Pin))return C.Stop(TEXT("component_replace_overlap_pin_identity_changed"),TEXT("Returned event persistent PinId missing or duplicate."));Map.Add(Prior->Pin,Q);}
                else Map.Add(N.PinRows[J].Pin,Q);if(Q->DefaultObject)C.NativeReturnedHolds.Emplace(Q->DefaultObject.Get());if(Q->PinType.PinSubCategoryObject.Get())C.NativeReturnedHolds.Emplace(Q->PinType.PinSubCategoryObject.Get());if(Q->PinType.PinValueType.TerminalSubCategoryObject.Get())C.NativeReturnedHolds.Emplace(Q->PinType.PinValueType.TerminalSubCategoryObject.Get());}
            Next.Nodes.Add(MoveTemp(V));
        }
        C.NativeCandidates.Add(MoveTemp(Next));
    }
    C.NativeCandidatesReady=true;if(!C.Current(CompilerReturn)||!NativeDisplays(C,C.NativeCandidates,CompilerReturn))return false;
    for(int32 GI=0;GI<C.GraphRows.Num();++GI)for(int32 NI=0;NI<C.GraphRows[GI].Nodes.Num();++NI)
    {
        const auto& N=C.GraphRows[GI].Nodes[NI];auto& Next=C.NativeCandidates[GI].Nodes[NI];
        for(int32 NativePinIndex=0;NativePinIndex<N.PinRows.Num();++NativePinIndex)
        {
            const auto& P=N.PinRows[NativePinIndex];const int32 CurrentIndex=N.SelectedOverlap?Next.PinRows.IndexOfByPredicate([&](const FPin& A){return A.Id==P.Id;}):NativePinIndex;
            if(CurrentIndex==INDEX_NONE)return C.Stop(TEXT("component_replace_overlap_pin_identity_changed"),TEXT("Original event PinId absent from fresh owning inventory."));auto& Q=Next.PinRows[CurrentIndex];FEdGraphPinType Expected=P.Type;
            if(CompilerReturn&&N.SelectedOverlap&&P.Name==UK2Node_Event::DelegateOutputName&&C.Overlap)Expected=C.Overlap->DelegateType;
            if(CompilerReturn&&N.SelectedGetter&&NativePinIndex==N.GetterValueIndex&&FScope::PinTypeSame(Q.Type,N.GetterAfterType)){Expected=N.GetterAfterType;Next.GetterRetyped=true;}
            if(P.Id!=Q.Id||P.Name!=Q.Name||P.Direction!=Q.Direction||!FScope::PinTypeSame(Expected,Q.Type)||!P.Default.Equals(Q.Default,ESearchCase::CaseSensitive)||!P.Auto.Equals(Q.Auto,ESearchCase::CaseSensitive)||P.Object!=Q.Object||!P.NativeTextDisplay.Equals(Q.NativeTextDisplay,ESearchCase::CaseSensitive)||!P.NativeFriendlyDisplay.Equals(Q.NativeFriendlyDisplay,ESearchCase::CaseSensitive)||FTextInspector::GetFlags(P.Text)!=FTextInspector::GetFlags(Q.Text)||FTextInspector::GetFlags(P.NativeFriendly)!=FTextInspector::GetFlags(Q.NativeFriendly)||P.Hidden!=Q.Hidden||P.Orphan!=Q.Orphan||P.Advanced!=Q.Advanced||P.NotConnectable!=Q.NotConnectable||P.DefaultReadOnly!=Q.DefaultReadOnly||P.DefaultIgnored!=Q.DefaultIgnored||P.SaveOrphan!=Q.SaveOrphan||P.Deprecated!=Q.Deprecated||P.MutableRef!=Q.MutableRef||P.FriendlyName!=Q.FriendlyName)return C.GraphMismatch(C.GraphRows[GI],NI,NativePinIndex,TEXT("returned_pin_semantic"),TEXT("original logical fields; only planned selected type"),TEXT("different"),TEXT("component_replace_native_get_rebind_semantic"),TEXT("Returned pin logical state changed; candidate baseline is not adopted."));
            if(P.Links.Num()!=Q.Links.Num()||P.Children.Num()!=Q.Children.Num()||(P.Parent?Map.FindRef(P.Parent):nullptr)!=Q.Parent)return C.Stop(TEXT("component_replace_native_get_rebind_links"),TEXT("Returned link/split inventory differs."));
            for(int32 K=0;K<P.Links.Num();++K)if(!Map.Contains(P.Links[K])||Map.FindRef(P.Links[K])!=Q.Links[K])return C.Stop(TEXT("component_replace_native_get_rebind_links"),TEXT("Original ordered reciprocal link endpoint changed."));
            for(int32 K=0;K<P.Children.Num();++K)if(!Map.Contains(P.Children[K])||Map.FindRef(P.Children[K])!=Q.Children[K])return C.Stop(TEXT("component_replace_native_get_rebind_links"),TEXT("Original ordered split endpoint changed."));
        }
    }
    // Only physical handles/text identity are replaced after original logical equality; original graph JSON remains the comparison authority.
    for(int32 GI=0;GI<C.GraphRows.Num();++GI)for(int32 NI=0;NI<C.GraphRows[GI].Nodes.Num();++NI){auto& Stable=C.GraphRows[GI].Nodes[NI];auto& Candidate=C.NativeCandidates[GI].Nodes[NI];Stable.Pins=MoveTemp(Candidate.Pins);Stable.PinRows=MoveTemp(Candidate.PinRows);Stable.GetterRetyped=Candidate.GetterRetyped;}C.NativeCandidatesReady=false;C.NativeCandidates.Reset();C.R.Data->SetBoolField(TEXT("selected_getter_semantic_links_verified"),true);return C.Current(CompilerReturn);
}
bool NativeGetterResolved(FScope& C)
{
    for(const auto& G:C.GraphRows)for(const auto& N:G.Nodes)if(N.SelectedGetter)
    {
        if(!C.Current())return false;auto* V=static_cast<UK2Node_VariableGet*>(N.Node.Get());FProperty* P=V->GetPropertyForVariable();if(!C.Current())return false;
        // SDK prefers generated property (and can use skeleton during compilation). Both are direct held own-property roles.
        if(P!=C.GetterCompiledField.Property&&P!=C.GetterSkeletonField.Property)return C.Stop(TEXT("component_replace_native_get_resolved_property"),TEXT("Actual getter resolution is outside the held direct compiled/skeleton property binding."));
        FProperty* S=V->GetPropertyForVariableFromSkeleton();if(!C.Current())return false;if(S!=C.GetterSkeletonField.Property)return C.Stop(TEXT("component_replace_native_get_skeleton_property"),TEXT("Actual skeleton getter resolution differs from held direct property."));
    }
    return true;
}
}

bool NativeOverlapNode(FContext* C,const UEdGraphNode* Node)
{return C&&C->NativeGet&&C->NativeGet->Overlap&&C->NativeGet->Overlap->Node.Get()==Node;}

bool NativeGetCurrent(FContext& C,FResult&)
{
    if(!C.NativeGet||C.NativeChecking)return true;C.NativeGet->NativeContext=&C;return C.NativeGet->Current();
}
bool NativeGetAllowed(const FContext& C,const UEdGraphNode* Node)
{if(!C.NativeGet)return false;for(const auto& G:C.NativeGet->GraphRows)for(const auto& N:G.Nodes)if(N.Node.Get()==Node&&(N.SelectedGetter||N.SelectedOverlap))return true;return false;}
bool NativeGetProject(FContext& C,const TSharedPtr<FJsonValue>& Before,TSharedPtr<FJsonValue>& Out)
{Out=Before;return !C.NativeGet||GeneratedReplacement::SelectedGetterGraphExpectation(*C.NativeGet,Before,Out);}
bool NativeGetBind(FContext& C,FResult& R)
{
    using namespace GeneratedReplacement;
    TArray<UEdGraph*> Graphs;C.Blueprint->GetAllGraphs(Graphs);bool Any=false;
    if(Graphs.Num()>MaxGraphs)return true;int32 Scanned=0;
    for(auto* G:Graphs)if(Loaded(G))for(const auto& H:G->Nodes){if(++Scanned>4096||!H.IsResolved())return true;UEdGraphNode* N=H.Get();if(Loaded(N)){if(const auto* V=Cast<UK2Node_Variable>(N);V&&V->VariableReference.IsSelfContext()&&V->VariableReference.GetMemberName()==FName(*C.SelectedName))Any=true;if(const auto* E=Cast<UK2Node_ComponentBoundEvent>(N);E&&E->GetComponentPropertyName()==FName(*C.SelectedName))Any=true;}}
    if(!Any||C.TargetClass!=USphereComponent::StaticClass()||C.OldTemplate->GetClass()!=UBoxComponent::StaticClass())return true;
    C.ShapeMode=true;if(!ShapeBind(C,R))return false;
    auto Work=MakeShared<FScope>(R);auto& W=*Work;W.NativeScene=true;W.NativeContext=&C;W.Host.Reset(C.Blueprint.Get());W.HostClass.Reset(static_cast<UBlueprintGeneratedClass*>(C.ShapeGenerated.Get()));W.HostSkeleton.Reset(static_cast<UBlueprintGeneratedClass*>(C.ShapeSkeleton.Get()));W.HostCDO.Reset(C.CDO);W.OldClass.Reset(UBoxComponent::StaticClass());W.NewClass.Reset(USphereComponent::StaticClass());W.OldTemplate.Reset(C.OldTemplate.Get());W.SCS.Reset(C.ShapeSCS.Get());W.Selected.Reset(C.Node);W.SelectedName=C.SelectedName;W.Guid=C.Guid;W.HostPath=C.Blueprint->GetPathName();W.HostPackage=C.Package;W.Editor.Reset(GEditor);W.Undo=GUndo;W.HostArrays=FScope::Arrays(C.Blueprint.Get());W.All=C.ShapeAll;W.Roots=C.ShapeRoots;
    for(UObject* O:{static_cast<UObject*>(W.Host.Get()),static_cast<UObject*>(W.Host->GetPackage()),static_cast<UObject*>(W.HostClass.Get()),static_cast<UObject*>(W.HostSkeleton.Get()),W.HostCDO.Get(),static_cast<UObject*>(W.SCS.Get()),static_cast<UObject*>(W.OldClass.Get()),static_cast<UObject*>(W.NewClass.Get())})if(W.Role(O)==INDEX_NONE)return Fail(R,TEXT("component_replace_native_get_source_unavailable"),TEXT("Loaded source/host roles required before getter migration."));
    C.NativeGet=Work;
    if(!FreezeGraphs(W,W.Host.Get()))return false;
    for(auto& G:W.GraphRows)for(auto& N:G.Nodes)for(auto& P:N.PinRows){P.NativeFriendly=P.Pin->PinFriendlyName;P.Deprecated=P.Pin->bDeprecated;P.MutableRef=P.Pin->bDisplayAsMutableRef;P.FriendlyName=P.Pin->bAllowFriendlyName;if(P.Object)W.NativeReturnedHolds.Emplace(P.Object.Get());}
    W.GraphsReady=true;if(!W.Current()||!NativeOverlapPrepare(W)||!NativeDisplays(W,W.GraphRows)||!PrepareSelectedGetters(W)||!NativeGetterResolved(W))return false;
    // Bound direct input peers must use the native common Scene/Primitive object surface; no Box-specific retargeting/converter.
    for(const auto& G:W.GraphRows)for(const auto& N:G.Nodes)if(N.SelectedGetter)for(auto* Peer:N.PinRows[N.GetterValueIndex].Links)
    {
        if(!W.Current())return false;
        if(Peer->PinType.PinCategory!=UEdGraphSchema_K2::PC_Object||Peer->PinType.ContainerType!=EPinContainerType::None||Peer->PinType.bIsReference||(Peer->PinType.PinSubCategoryObject.Get()!=USceneComponent::StaticClass()&&Peer->PinType.PinSubCategoryObject.Get()!=UPrimitiveComponent::StaticClass()))return Fail(R,TEXT("component_replace_native_get_consumer_pending"),TEXT("First native Scene getter slice admits directly compatible whole Scene/Primitive object input peers only."));
    }
    R.Data->SetBoolField(TEXT("native_scene_selected_getter_replacement"),R.Data->GetNumberField(TEXT("selected_own_getter_count"))>0);R.Data->SetBoolField(TEXT("selected_getter_member_guid_may_be_zero"),true);R.Data->SetBoolField(TEXT("full_legacy_contract_completed"),false);
    R.Data->SetStringField(TEXT("native_scene_selected_getter_scope"),TEXT("Own exact pure self VariableGet, captured name/reference (zero member GUID permitted), actual direct compiled/skeleton property binding; Box->Sphere output and existing directly compatible whole Scene/Primitive object input peers. Original ordered reciprocal links, GUIDs and nonselected captured state remain authoritative. Setters, incompatible consumers, runtime/derived propagation remain incomplete. One selected native PrimitiveComponent.BeginOverlap event is separately verified when present; other bound events remain incomplete."));return W.Current();
}
bool NativeGetReturn(FContext& C,FResult& R,bool FullCompile)
{
    if(!C.NativeGet)return true;auto& W=*C.NativeGet;W.NativeContext=&C;W.NativeReturning=true;if(W.Overlap)W.Overlap->DerivedReady=false;
    // Return-time hold before any observer or old physical pin access.
    auto GC=C.Blueprint->GeneratedClass,SK=C.Blueprint->SkeletonGeneratedClass;
    if(!GC.GetGCPtr().IsResolved()||!SK.GetGCPtr().IsResolved())return W.Stop(TEXT("component_replace_native_get_class_return"),TEXT("Compiler returned unresolved class slots."));
    TStrongObjectPtr<UClass> G(GC.Get()),S(SK.Get());if(!Loaded(G.Get())||!Loaded(S.Get())||!G->ClassDefaultObject.IsResolved())return W.Stop(TEXT("component_replace_native_get_class_return"),TEXT("Compiler returned unavailable held class/CDO roles."));TStrongObjectPtr<UObject> D(G->ClassDefaultObject.Get());if(G->GetClass()!=UBlueprintGeneratedClass::StaticClass()||S->GetClass()!=UBlueprintGeneratedClass::StaticClass()||!Loaded(D.Get())||D->GetClass()!=G.Get()||D->GetOuter()!=G->GetOuter()||!D->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject)||!G->ClassGeneratedBy.IsResolved()||G->ClassGeneratedBy.Get()!=C.Blueprint.Get()||!S->ClassGeneratedBy.IsResolved()||S->ClassGeneratedBy.Get()!=C.Blueprint.Get())return W.Stop(TEXT("component_replace_native_get_class_return"),TEXT("Returned classes/CDO are not the planned own generated/skeleton/default roles."));W.NativeReturnedHolds.Emplace(G.Get());W.NativeReturnedHolds.Emplace(S.Get());W.NativeReturnedHolds.Emplace(D.Get());
    if(!GeneratedReplacement::NativeOverlapReturn(W,static_cast<UBlueprintGeneratedClass*>(G.Get()),static_cast<UBlueprintGeneratedClass*>(S.Get()))||!GeneratedReplacement::NativeAdoptPins(W,true)||!GeneratedReplacement::RebindHost(W,FullCompile,false))return false;
    // Base Scene scope is refreshed only after the shared explicit class-role proof succeeded.
    C.ShapeGenerated.Reset(G.Get());C.ShapeSkeleton.Reset(S.Get());C.ShapeHostCDO.Reset(D.Get());C.CDO=static_cast<AActor*>(D.Get());
    if(!GeneratedReplacement::RepairSelectedGetters(W)||!GeneratedReplacement::NativeGetterResolved(W))return false;
    return W.Current();
}
bool NativeGetStable(FContext& C,FResult& R)
{if(!C.NativeGet)return true;C.NativeGet->NativeContext=&C;C.NativeGet->NativeReturning=false;return ShapeCurrent(C,R);}
bool NativeGetSaved(FContext& C,FResult& R)
{if(!C.NativeGet)return true;if(!R.bSaved)return C.NativeGet->Stop(TEXT("component_replace_save_failed_or_unobservable"),TEXT("Save false stops before followup getter/source SDK."));C.NativeGet->NativeSaved=true;return C.NativeGet->Current();}

}
FString ReplaceComponentClassRaw(const FString& BlueprintPath, const FString& ComponentName, const FString& NewComponentClass,
    double SphereRadius, bool bDryRun, bool bCompile, bool bSave, bool bRefreshNodes, bool bRequireSceneCompatibility)
{
    if (NewComponentClass.StartsWith(TEXT("/Game/")))
    {
        return GeneratedReplacement::Replace(BlueprintPath, ComponentName, NewComponentClass, SphereRadius, bDryRun, bCompile, bSave, bRefreshNodes,bRequireSceneCompatibility);
    }
    FResult R; Init(R, bDryRun, bCompile, bSave, bRefreshNodes); R.Data->SetStringField(TEXT("blueprint_name"), BlueprintPath); R.Data->SetStringField(TEXT("requested_new_component_class"), NewComponentClass); R.Data->SetBoolField(TEXT("require_scene_compatibility"), bRequireSceneCompatibility); FContext C; FObservation Before, Current; float Radius = 0;
    const bool Passed = [&]() -> bool
    {
        FString Target = NewComponentClass.TrimStartAndEnd(); Target.TrimQuotesInline();
        const auto Alias=[&](const TCHAR* Short,const TCHAR* Full,const TCHAR* UName,const TCHAR* ExactPath){return Target.Equals(Short,ESearchCase::IgnoreCase)||Target.Equals(Full,ESearchCase::IgnoreCase)||Target.Equals(UName,ESearchCase::IgnoreCase)||Target.Equals(ExactPath,ESearchCase::IgnoreCase);};
        if(Alias(TEXT("Sphere"),TEXT("SphereComponent"),TEXT("USphereComponent"),TEXT("/Script/Engine.SphereComponent")))C.TargetClass=USphereComponent::StaticClass();
        else if(Alias(TEXT("Box"),TEXT("BoxComponent"),TEXT("UBoxComponent"),TEXT("/Script/Engine.BoxComponent")))C.TargetClass=UBoxComponent::StaticClass();
        else if(Alias(TEXT("Capsule"),TEXT("CapsuleComponent"),TEXT("UCapsuleComponent"),TEXT("/Script/Engine.CapsuleComponent")))C.TargetClass=UCapsuleComponent::StaticClass();
        else if(Alias(TEXT("StaticMeshComponent"),TEXT("StaticMeshComponent"),TEXT("UStaticMeshComponent"),TEXT("/Script/Engine.StaticMeshComponent")))C.TargetClass=UStaticMeshComponent::StaticClass();
        else return Fail(R,TEXT("component_replace_target_class_unsupported"),TEXT("Exact native Sphere/Box/Capsule and bounded null-mesh StaticMeshComponent targets are admitted; other native/custom Scene targets remain incomplete."));
        if (C.TargetClass==USphereComponent::StaticClass()&&!FMath::IsFinite(SphereRadius)) { return Fail(R, TEXT("component_replace_radius_invalid"), TEXT("Finite radius required; negative public values mean original omitted/derived radius.")); }
        if (!Prepare(BlueprintPath, ComponentName, C, R)) { return false; }
        C.OldTemplate.Reset(C.Node->ComponentTemplate); auto* Old = CastChecked<UPrimitiveComponent>(C.OldTemplate.Get());
        C.ShapeMode=C.RootMode||C.ShapeChildrenMode||C.TargetClass!=USphereComponent::StaticClass()||Old->GetClass()==UCapsuleComponent::StaticClass()||Old->GetClass()==UStaticMeshComponent::StaticClass();
        if(!RootBegin(C,R))return false;
        if(C.ShapeMode){C.SelectedName=C.Node->GetVariableName().ToString();R.Data->SetBoolField(TEXT("native_scene_shape_replacement"),true);R.Data->SetStringField(TEXT("unsupported_original_scope"),TEXT("Root/inherited/typed-reference/live/derived and other native/custom targets remain incomplete; own non-root native shape preserves the existing SCS child subtree with original common/shape semantics."));if(C.RootMode)R.Data->SetStringField(TEXT("unsupported_original_scope"),TEXT("Own native shape root is supported with the existing common values and ordered SCS children; plain Scene/default-selected/native-bound/inherited roots, other classes, live/derived propagation and unsupported typed references remain valid incomplete."));if(!ShapeBind(C,R))return false;KeepChildrenBaseline(C,R);if(!ShapeCurrent(C,R))return false;}
        if(!NativeGetBind(C,R))return false;
        if (!FiniteCommon(Old)) { return Fail(R, TEXT("component_replace_common_defaults_unobservable"), TEXT("Bounded finite common component defaults required.")); }
        if (!GraphReferences(C, R)) { return false; }
        double Requested = C.TargetClass==USphereComponent::StaticClass()?SphereRadius:0.0;
        if (C.TargetClass==USphereComponent::StaticClass()&&Requested < 0) { if (auto* Box = Cast<UBoxComponent>(Old)) { const FVector E = Box->GetUnscaledBoxExtent(); if (E.ContainsNaN()) { return Fail(R, TEXT("component_replace_radius_invalid"), TEXT("Source Box extent is not finite.")); } Requested = FMath::Max3(FMath::Abs(E.X), FMath::Abs(E.Y), FMath::Abs(E.Z)); } else if(auto* S=Cast<USphereComponent>(Old)){Requested=S->GetUnscaledSphereRadius();}else Requested=100.0; }
        Radius = FMath::Max(0.0f, static_cast<float>(Requested)); if (!FMath::IsFinite(Requested) || !FMath::IsFinite(Radius)) { return Fail(R, TEXT("component_replace_radius_invalid"), TEXT("Effective radius must be finite in native float representation.")); }
        R.Data->SetStringField(TEXT("radius_source"), SphereRadius >= 0 ? TEXT("explicit") : Old->GetClass() == UBoxComponent::StaticClass() ? TEXT("old_box_max_absolute_extent") : TEXT("old_sphere_unscaled_radius")); R.Data->SetNumberField(TEXT("effective_sphere_radius"), Radius);
        if(C.TargetClass!=USphereComponent::StaticClass()){R.Data->RemoveField(TEXT("effective_sphere_radius"));R.Data->SetStringField(TEXT("radius_source"),TEXT("ignored_non_sphere_target"));}
        else if(SphereRadius<0&&(Old->GetClass()==UCapsuleComponent::StaticClass()||Old->GetClass()==UStaticMeshComponent::StaticClass()))R.Data->SetStringField(TEXT("radius_source"),TEXT("original_other_scene_fallback100"));
        if(C.ShapeMode)
        {
            if(auto* SourceCapsule=Cast<UCapsuleComponent>(Old);SourceCapsule&&(!FMath::IsFinite(SourceCapsule->GetUnscaledCapsuleRadius())||!FMath::IsFinite(SourceCapsule->GetUnscaledCapsuleHalfHeight())))return Fail(R,TEXT("component_replace_shape_defaults_unobservable"),TEXT("Finite source Capsule dimensions required."));
            if(C.TargetClass==UBoxComponent::StaticClass()){const auto* Default=Cast<UBoxComponent>(C.ShapeTargetCDO.Get());if(!Default)return Fail(R,TEXT("component_replace_shape_defaults_unobservable"),TEXT("Actual target Box existing CDO required."));C.ExpectedBox=Cast<UBoxComponent>(Old)?CastChecked<UBoxComponent>(Old)->GetUnscaledBoxExtent():Default->GetUnscaledBoxExtent();if(C.ExpectedBox.ContainsNaN())return Fail(R,TEXT("component_replace_shape_defaults_unobservable"),TEXT("Finite effective Box extent required."));R.Data->SetArrayField(TEXT("effective_box_extent"),Vector(C.ExpectedBox));R.Data->SetStringField(TEXT("shape_defaults_origin"),Cast<UBoxComponent>(Old)?TEXT("original_box_to_box_extent_copy"):TEXT("new_target_existing_cdo"));}
            if(C.TargetClass==UCapsuleComponent::StaticClass()){const auto* Default=Cast<UCapsuleComponent>(C.ShapeTargetCDO.Get());if(!Default)return Fail(R,TEXT("component_replace_shape_defaults_unobservable"),TEXT("Actual target Capsule existing CDO required."));C.ExpectedCapsuleRadius=Default->GetUnscaledCapsuleRadius();C.ExpectedCapsuleHalfHeight=Default->GetUnscaledCapsuleHalfHeight();if(!FMath::IsFinite(C.ExpectedCapsuleRadius)||!FMath::IsFinite(C.ExpectedCapsuleHalfHeight))return Fail(R,TEXT("component_replace_shape_defaults_unobservable"),TEXT("Finite native Capsule CDO dimensions required."));R.Data->SetNumberField(TEXT("effective_capsule_radius"),C.ExpectedCapsuleRadius);R.Data->SetNumberField(TEXT("effective_capsule_half_height"),C.ExpectedCapsuleHalfHeight);R.Data->SetStringField(TEXT("shape_defaults_origin"),TEXT("new_target_existing_cdo_original_copy_has_no_capsule_dimension_branch"));}
            R.Data->SetBoolField(TEXT("sphere_radius_applied"),C.TargetClass==USphereComponent::StaticClass());R.Data->SetStringField(TEXT("effect_scope"),TEXT("Own non-root native Box/Sphere/Capsule replacement with preserved SCS children; tags/Scene/Primitive common values plus original target shape semantics. Actual template attachments remain empty; callback prefix/current fences, no atomic rollback/runtime propagation."));
            if(C.RootMode)R.Data->SetStringField(TEXT("effect_scope"),TEXT("Own parentless native Box/Sphere/Capsule root template replacement; original tags/relative transform/mobility/visibility/collision and target-shape semantics, ordered descendants/socket/GUID/variable references preserved in editor. No runtime world-transform or live-instance propagation claim."));
            if(!ShapeCurrent(C,R))return false;
        }
        if(C.TargetClass==UStaticMeshComponent::StaticClass()){
         R.Data->SetBoolField(TEXT("native_static_mesh_target"),true);R.Data->SetBoolField(TEXT("static_mesh_reference_written"),false);R.Data->SetBoolField(TEXT("static_mesh_runtime_verified"),false);
         R.Data->SetStringField(TEXT("effect_scope"),TEXT("Own native shape -> exact native StaticMeshComponent using original Scene/Primitive common-copy and existing SCS root/children/retirement/compile/save flow. Target CDO and returned templates keep null StaticMesh and empty OverrideMaterials; no mesh/material assignment, load, renderer setup or runtime proof. Broader original native component/source/asset profiles remain incomplete; implemented/usage validation pending."));
        }
        if(C.MeshSource.IsValid())
        {
            R.Data->SetBoolField(TEXT("native_static_mesh_source"),true);R.Data->SetBoolField(TEXT("static_mesh_reference_written"),false);R.Data->SetBoolField(TEXT("static_mesh_runtime_verified"),false);
            R.Data->SetStringField(TEXT("effect_scope"),TEXT("Own inactive native StaticMeshComponent -> admitted native Primitive target through original common defaults, template retirement, SCS root/children, compile and optional exact save. Original mesh/material references remain on the retained old template; no source asset setter or reference transfer to shape targets. No runtime propagation or whole in-memory asset/cache proof."));
        }
        R.Data->SetStringField(TEXT("new_component_class"), C.TargetClass->GetName()); R.Data->SetStringField(TEXT("new_component_class_path"), C.TargetClass->GetPathName());
        if (!Capture(C, Before, R, TEXT("before_observation"))) { return Fail(R, TEXT("component_replace_before_unobservable"), TEXT("Preflight bounded target/source/file observation incomplete; no Modify/allocation.")); }
        R.Data->SetObjectField(TEXT("before"), Summary(C.Node));
        if (bDryRun)
        {
            auto Preview = Summary(C.Node); Preview->SetStringField(TEXT("component_class"), C.TargetClass->GetName());Preview->SetStringField(TEXT("component_class_path"),C.TargetClass->GetPathName());if(C.ShapeMode){if(C.MeshSource.IsValid()&&C.TargetClass!=UStaticMeshComponent::StaticClass())Preview->RemoveField(TEXT("static_mesh"));Preview->RemoveField(TEXT("box_extent"));Preview->RemoveField(TEXT("sphere_radius"));Preview->RemoveField(TEXT("capsule_radius"));Preview->RemoveField(TEXT("capsule_half_height"));}
            if(C.TargetClass==UBoxComponent::StaticClass()){Preview->SetStringField(TEXT("shape"),TEXT("box"));Preview->SetArrayField(TEXT("box_extent"),Vector(C.ExpectedBox));}else if(C.TargetClass==UCapsuleComponent::StaticClass()){Preview->SetStringField(TEXT("shape"),TEXT("capsule"));Preview->SetNumberField(TEXT("capsule_radius"),C.ExpectedCapsuleRadius);Preview->SetNumberField(TEXT("capsule_half_height"),C.ExpectedCapsuleHalfHeight);}else if(C.TargetClass==UStaticMeshComponent::StaticClass()){Preview->RemoveField(TEXT("shape"));Preview->SetStringField(TEXT("static_mesh"),TEXT(""));}else{Preview->SetStringField(TEXT("shape"),TEXT("sphere"));Preview->SetNumberField(TEXT("sphere_radius"),Radius);} R.Data->SetObjectField(TEXT("after_preview"), Preview); R.Data->SetStringField(TEXT("write_scope"), TEXT("dry_run_no_asset_mutation"));
            if (!Capture(C, Current, R, TEXT("after_observation"))) { return Fail(R, TEXT("component_replace_preview_unobservable"), TEXT("Preview returned but exact repeated current observation incomplete."), true); }
            const bool EqualAll = Equal(Before.State, Current.State) && Equal(Before.NativeSources, Current.NativeSources) && Equal(Before.DiskState, Current.DiskState) && Equal(Before.OtherDirty, Current.OtherDirty);
            R.Data->SetBoolField(TEXT("observations_complete"), true); R.Data->SetBoolField(TEXT("source_preserved"), Equal(Before.NativeSources, Current.NativeSources)); R.Data->SetBoolField(TEXT("unselected_state_preserved"), EqualAll); R.Data->SetBoolField(TEXT("independent_readback_passed"), EqualAll); R.bChanged = !EqualAll;
            if (!EqualAll) { return Fail(R, TEXT("component_replace_preview_state_changed"), TEXT("Repeated actual observation differs during preview; desired fields remain explicitly separate.")); }
            R.Stage = TEXT("complete"); return true;
        }
        R.Stage = TEXT("retire_old_template"); R.bAttempted = true; R.bKnown = false; ClearFinal(R);
        C.Blueprint->Modify(); Count(R, TEXT("modify_calls"));if(!ShapeCurrent(C,R))return false; C.Blueprint->SimpleConstructionScript->Modify(); Count(R, TEXT("modify_calls"));if(!ShapeCurrent(C,R))return false; C.Node->Modify(); Count(R, TEXT("modify_calls"));if(!ShapeCurrent(C,R))return false; C.OldTemplate->Modify(); Count(R, TEXT("modify_calls"));if(!ShapeCurrent(C,R))return false;
        Count(R, TEXT("rename_calls")); const bool Renamed = C.OldTemplate->Rename(nullptr, GetTransientPackage(), REN_DoNotDirty | REN_DontCreateRedirectors); R.Data->SetBoolField(TEXT("old_template_rename_returned"), Renamed);if(C.ShapeMode&&Renamed)C.ShapeRetired=true;if(!ShapeCurrent(C,R))return false; Retirement(C, R, TEXT("after_retirement"));
        if (!Renamed || !Loaded(C.OldTemplate.Get()) || C.OldTemplate->GetOutermost() != GetTransientPackage() || FindObject<UObject>(C.Blueprint->GeneratedClass, *C.Slot)) { return Fail(R, TEXT("component_replace_retirement_failed"), TEXT("One exact retirement Rename failed or old slot not empty; retained actual prefix, no allocation/retry."), true); }
        R.Stage = TEXT("allocate_replacement_template"); Count(R, TEXT("template_allocation_calls")); C.NewTemplate.Reset(NewObject<UActorComponent>(C.Blueprint->GeneratedClass, C.TargetClass, FName(*C.Slot), RF_ArchetypeObject | RF_Transactional | RF_Public));if(!ShapeCurrent(C,R))return false; Retirement(C, R, TEXT("after_allocation"));
        auto* Sphere = Cast<UPrimitiveComponent>(C.NewTemplate.Get());
        if (!Loaded(Sphere) || Sphere->GetClass() != C.TargetClass || Sphere->GetOuter() != C.Blueprint->GeneratedClass || Sphere->GetName() != C.Slot || Sphere->IsRegistered() || Sphere->IsPreRegistering() || Sphere->GetOwner() || Sphere->GetWorld()) { return Fail(R, TEXT("component_replace_allocation_failed"), TEXT("Exact new native unregistered Primitive target template not observed; old node prefix remains, no rollback/retry."), true); }
        Sphere->Modify(); Count(R, TEXT("modify_calls"));if(!ShapeCurrent(C,R))return false; R.Stage = TEXT("copy_common_defaults"); Count(R, TEXT("common_copy_calls"));
        if(C.ShapeMode){if(!CopyAdditionalShapeDefaults(C,R,Old,Sphere,Radius))return false;}
        else{auto* NativeSphere=CastChecked<USphereComponent>(Sphere);
        Sphere->ComponentTags = Old->ComponentTags; Sphere->SetRelativeLocation(Old->GetRelativeLocation()); Sphere->SetRelativeRotation(Old->GetRelativeRotation()); Sphere->SetRelativeScale3D(Old->GetRelativeScale3D()); Sphere->SetMobility(Old->Mobility); Sphere->SetVisibility(Old->IsVisible(), false); Sphere->SetHiddenInGame(Old->bHiddenInGame, false);
        Sphere->SetCollisionProfileName(Old->GetCollisionProfileName(), false); Sphere->SetCollisionEnabled(Old->GetCollisionEnabled()); Sphere->SetGenerateOverlapEvents(Old->GetGenerateOverlapEvents()); Sphere->CanCharacterStepUpOn = Old->CanCharacterStepUpOn; NativeSphere->SetSphereRadius(Radius, false);
        }
        const auto CopiedCommon=Common(Sphere);R.Data->SetObjectField(TEXT("copied_common_readback"), CopiedCommon); R.Data->SetObjectField(TEXT("new_template_after_common_copy"), Identity(Sphere));
        if (!FiniteCommon(Sphere) || !CommonEqual(Before.State->GetObjectField(TEXT("selected"))->GetObjectField(TEXT("common")).ToSharedRef(), CopiedCommon,Sphere->GetPathName(),R) || (C.TargetClass==USphereComponent::StaticClass()?CastChecked<USphereComponent>(Sphere)->GetUnscaledSphereRadius()!=Radius:!ShapeDefaults(C,Sphere))) { return Fail(R, TEXT("component_replace_common_copy_failed"), TEXT("Actual native common-copy/radius differs after public callbacks; no node assignment/compile/save, retained objects are not rolled back."), true); }
        R.Stage = TEXT("assign_scs_template"); C.ShapeAssigned=true;C.Node->ComponentClass = C.TargetClass; C.Node->ComponentTemplate = Sphere; C.Node->SetVariableName(FName(*C.SelectedName), false);if(!ShapeCurrent(C,R))return false; C.Node->ValidateGuid();if(!ShapeCurrent(C,R))return false; Retirement(C, R, TEXT("after_node_assignment"));
        C.Blueprint->SimpleConstructionScript->ValidateSceneRootNodes();if(!RootValidatorReturned(C,R))return false;
        // Capture the fully assigned authoring state before structural callbacks, without calling the public compiler yet.
        if (!Capture(C, Current, R, TEXT("after_assignment_observation"))) { return Fail(R, TEXT("component_replace_assignment_unobservable"), TEXT("Actual selected replacement prefix incomplete; structural refresh and full compile suppressed."), true); }
        if (!Fences(Before, Current, C, R) || !Readback(C, Before, Current, Radius, R, false)) { return false; }
        R.Data->SetBoolField(TEXT("after_assignment_preserved"), true); R.Stage = TEXT("structural_refresh"); ClearFinal(R); R.bKnown = false; Count(R, TEXT("structural_refresh_calls")); FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(C.Blueprint.Get());
        if(!RootCompilerReturned(C,R)||!NativeGetReturn(C,R,false))return false;
        FContext Refreshed; const bool ContextOK = Prepare(C.Blueprint->GetPathName(), C.SelectedName, Refreshed, R,C.TargetClass==UStaticMeshComponent::StaticClass()?C.TargetClass:nullptr); if((C.RootMode||C.ShapeChildrenMode||C.NativeGet||C.MeshSource.IsValid())&&(!ContextOK||C.RootMode!=Refreshed.RootMode)){R.bKnown=false;ClearFinal(R);R.Data->SetBoolField(TEXT("native_scene_child_followup_sdk_stopped"),true);return false;} if (ContextOK) { Refreshed.OldTemplate = MoveTemp(C.OldTemplate); Refreshed.NewTemplate = MoveTemp(C.NewTemplate);CopyShapeContract(C,Refreshed); C = MoveTemp(Refreshed);if(!ShapeBind(C,R,true)||!NativeGetStable(C,R))return false; }
        if (!Capture(C, Current, R, TEXT("after_structural_observation")) || !ContextOK) { return Fail(R, TEXT("component_replace_structural_unobservable"), TEXT("Structural refresh returned but current admitted ownership/defaults incomplete; retained prefix, no full compile/save."), true); }
        const FObjectPropertyBase* SkeletonMember = FindFProperty<FObjectPropertyBase>(C.Blueprint->SkeletonGeneratedClass, FName(*C.SelectedName)); const bool SkeletonOK = SkeletonMember && SkeletonMember->GetOwnerClass() == C.Blueprint->SkeletonGeneratedClass && SkeletonMember->ArrayDim == 1 && SkeletonMember->PropertyClass == C.TargetClass && SkeletonMember->HasAnyPropertyFlags(CPF_BlueprintVisible);
        R.Data->SetBoolField(TEXT("skeleton_selected_member_verified"), SkeletonOK); R.Data->SetObjectField(TEXT("after"), Summary(C.Node)); R.Data->SetStringField(TEXT("write_scope"), TEXT("blueprint_scs_component_template_replacement")); Retirement(C, R, TEXT("after_structural_retirement"));
        if (!Fences(Before, Current, C, R) || !Readback(C, Before, Current, Radius, R, false)) { return false; }
        if (!SkeletonOK) { return Fail(R, TEXT("component_replace_skeleton_member_mismatch"), TEXT("Exact current skeleton Sphere member not verified; no full compile/save.")); }
        if (!bCompile) { R.Data->SetBoolField(TEXT("saved"), false); R.Data->SetBoolField(TEXT("compile_requested"), false); R.Stage = TEXT("complete"); return true; }
        R.Stage = TEXT("compile_blueprint"); R.Data->SetBoolField(TEXT("after_configuration_observations_complete"), true); R.Data->SetBoolField(TEXT("after_configuration_source_preserved"), true); R.Data->SetBoolField(TEXT("after_configuration_unselected_state_preserved"), true); ClearFinal(R); R.bKnown = false; R.bCompileAttempted = true; Count(R, TEXT("full_compile_attempts"));
        auto CompileResult = Cubeless::UE58::CompileBlueprint(C.Blueprint->GetPathName(), false, bRefreshNodes); R.Data->SetObjectField(TEXT("compile_result"), CompileResult); bool Compiled = false; if (CompileResult->TryGetBoolField(TEXT("compiled"), Compiled)) { R.Compiled = Compiled; }
        if(C.RootMode||C.ShapeChildrenMode||C.NativeGet||C.MeshSource.IsValid())
        {
            bool Known=true,Unknown=false;CompileResult->TryGetBoolField(TEXT("change_state_known"),Known);CompileResult->TryGetBoolField(TEXT("outcome_unknown"),Unknown);
            if(!Known||Unknown||!R.Compiled.IsSet()){ClearFinal(R);R.Data->SetBoolField(TEXT("native_scene_child_followup_sdk_stopped"),true);return Fail(R,TEXT("component_replace_compile_outcome_unobservable"),TEXT("Actual compiler receipt retained; explicit unknown or missing typed compiled status stops before Prepare/readback/source SDK."),true);}
        }
        if(C.MeshSource.IsValid()&&!Compiled)return Fail(R,TEXT("component_replace_compile_failed"),TEXT("Known compiler failure retained; source identities remain held, no source/readback/save SDK or retry follows."),true);
        if(C.RootMode){if(!Compiled)return Fail(R,TEXT("component_replace_compile_failed"),TEXT("Known compiler failure retained; no old role readback/save/retry."),true);if(!RootCompilerReturned(C,R))return false;}
        if(C.NativeGet){if(!Compiled)return Fail(R,TEXT("component_replace_compile_failed"),TEXT("Known compiler failure retained; no old pin readback/save/retry."),true);if(!NativeGetReturn(C,R,true))return false;}
        FContext CompiledContext; const bool CompiledContextOK = Prepare(C.Blueprint->GetPathName(), C.SelectedName, CompiledContext, R,C.TargetClass==UStaticMeshComponent::StaticClass()?C.TargetClass:nullptr); if((C.RootMode||C.ShapeChildrenMode||C.NativeGet||C.MeshSource.IsValid())&&(!CompiledContextOK||C.RootMode!=CompiledContext.RootMode)){R.bKnown=false;ClearFinal(R);R.Data->SetBoolField(TEXT("native_scene_child_followup_sdk_stopped"),true);return false;} if (CompiledContextOK) { CompiledContext.OldTemplate = MoveTemp(C.OldTemplate); CompiledContext.NewTemplate = MoveTemp(C.NewTemplate);CopyShapeContract(C,CompiledContext); C = MoveTemp(CompiledContext);if(!ShapeBind(C,R,true)||!NativeGetStable(C,R))return false; }
        if (!Capture(C, Current, R, TEXT("after_observation")) || !CompiledContextOK) { return Fail(R, TEXT("component_replace_postcompile_unobservable"), TEXT("Full compiler attempted once; current bounded actual prefix incomplete, save suppressed."), true); }
        Retirement(C, R, TEXT("after_compile_retirement")); if (!Fences(Before, Current, C, R) || !Readback(C, Before, Current, Radius, R, Compiled)) { return false; }
        if (!R.Compiled.IsSet()) { return Fail(R, TEXT("component_replace_compile_outcome_unobservable"), TEXT("Missing typed compiler outcome; actual prefix retained, save suppressed."), true); }
        if (!Compiled) { return Fail(R, TEXT("component_replace_compile_failed"), TEXT("Compiler reported failure; exact returned diagnostics and actual prefix retained, no save.")); }
        R.Data->SetObjectField(TEXT("after"), Summary(C.Node)); R.Data->SetBoolField(TEXT("saved"), false); if (!bSave) { R.Stage = TEXT("complete"); return true; }
        R.Stage = TEXT("save_asset"); R.Data->SetBoolField(TEXT("before_save_observations_complete"), true); R.Data->SetBoolField(TEXT("before_save_source_preserved"), true); R.Data->SetBoolField(TEXT("before_save_unselected_state_preserved"), true); ClearFinal(R); R.bKnown = false; R.bSaveAttempted = true; Count(R, TEXT("save_attempt_count"));
        auto Receipt = MakeShared<FJsonObject>(); Receipt->SetStringField(TEXT("asset_path"), C.Blueprint->GetPathName()); Receipt->SetBoolField(TEXT("attempted"), true); R.Data->SetArrayField(TEXT("save_calls"), { MakeShared<FJsonValueObject>(Receipt) }); R.bSaved = UEditorAssetLibrary::SaveLoadedAsset(C.Blueprint.Get(), false); Receipt->SetBoolField(TEXT("success"), R.bSaved); R.Data->SetBoolField(TEXT("saved"), R.bSaved);
        if((C.RootMode||C.ShapeChildrenMode||C.NativeGet||C.MeshSource.IsValid())&&!R.bSaved){ClearFinal(R);R.Data->SetBoolField(TEXT("native_scene_child_followup_sdk_stopped"),true);return Fail(R,TEXT("component_replace_save_failed_or_unobservable"),TEXT("Actual saveFalse receipt retained; no Capture/retirement/source SDK or retry follows."),true);}
        if(!RootCurrent(C,R)||!NativeGetSaved(C,R))return false;
        FObservation Saved; const bool Complete = Capture(C, Saved, R, TEXT("after_save")); Retirement(C, R, TEXT("after_save_retirement")); R.Data->SetBoolField(TEXT("observations_complete"), Complete);
        if (!R.bSaved || !Complete) { return Fail(R, TEXT("component_replace_save_failed_or_unobservable"), TEXT("Single exact save receipt retained; successful disk writes must never be replayed because observer fails."), true); }
        const bool State = SavedEqual(MakeShared<FJsonValueObject>(Current.State), MakeShared<FJsonValueObject>(Saved.State), C.Package), Source = Equal(Current.NativeSources, Saved.NativeSources), Other = SavedOtherDirty(Current.OtherDirty, Saved.OtherDirty, R);
        R.bKnown = true; R.bChanged = true; R.Data->SetBoolField(TEXT("source_preserved"), Source); R.Data->SetBoolField(TEXT("unselected_state_preserved"), State && Other); R.Data->SetBoolField(TEXT("independent_readback_passed"), State); R.Data->SetBoolField(TEXT("saved_state_preserved"), State);
        if (!State || !Source || !Other || !Saved.DiskState->GetBoolField(TEXT("exists")) || Saved.DiskState->GetNumberField(TEXT("bytes")) <= 0 || C.Blueprint->GetOutermost()->IsDirty()) { return Fail(R, TEXT("component_replace_saved_postcondition_failed"), TEXT("Save receipt succeeded, but exact owned flag transition/state/source/disk/dirty proof differs; no retry or rollback.")); }
        R.Stage = TEXT("complete"); return true;
    }();
    R.bSuccess = Passed; R.bPost = Passed;
    if(C.RootMode)R.Data->SetBoolField(TEXT("own_scs_scene_root_replacement_preserved"),Passed&&R.Data->GetBoolField(TEXT("unselected_state_preserved"))&&R.Data->GetBoolField(TEXT("source_preserved")));
    if(C.ShapeChildrenMode){R.Data->SetBoolField(TEXT("native_scene_child_subtree_preserved"),Passed&&R.Data->GetBoolField(TEXT("unselected_state_preserved"))&&R.Data->GetBoolField(TEXT("source_preserved")));R.Data->SetBoolField(TEXT("native_scene_child_replacement_applied"),Passed&&!bDryRun);R.Data->SetBoolField(TEXT("native_scene_child_runtime_verified"),false);}
    return Encode(R);
}
FString ReplaceComponentClass(const FString& BlueprintPath,const FString& ComponentName,const FString& NewComponentClass,
    double SphereRadius,bool bDryRun,bool bCompile,bool bSave,bool bRefreshNodes)
{ return ReplaceComponentClassRaw(BlueprintPath,ComponentName,NewComponentClass,SphereRadius,bDryRun,bCompile,bSave,bRefreshNodes,true); }

}
