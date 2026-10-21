#include "Compatibility/UE58/CubelessAssetReferenceApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Templates/UnrealTemplate.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CallFunctionOnMember.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_EditablePinBase.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionTerminator.h"
#include "K2Node_StructOperation.h"
#include "K2Node_Variable.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MaterialShared.h"
#include "MaterialShaderPrecompileMode.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Misc/CoreMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::AssetReference
{
namespace
{
using FResult = Variable::FResult;
constexpr int32 MaxCarrierChars = 1024 * 1024, MaxRows = 4096, MaxObjects = 32768;
constexpr int32 MaxNodes = 8192, MaxPins = 65536, MaxExportChars = 1024 * 1024;
constexpr int64 MaxProofBytes = 64 * 1024 * 1024;
bool WorkBusy = false;
struct FPathStringReplacement { FString Source, Target; };
bool IsLiveObjectForMCP(const UObject* O)
{
    return IsValid(O) && !O->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed | RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects)
        && !O->HasAnyInternalFlags(EInternalObjectFlags::Garbage | EInternalObjectFlags::Unreachable)
        && O->GetClass() && O->GetOutermost();
}
bool NoNul(const FString& Text)
{ for (TCHAR C : Text) { if (C == TCHAR('\0')) { return false; } } return true; }
void Strings(const TSharedRef<FJsonObject>& Data, const TCHAR* Key, const TArray<FString>& Values)
{ TArray<TSharedPtr<FJsonValue>> Rows; for (const FString& V : Values) { Rows.Add(MakeShared<FJsonValueString>(V)); } Data->SetArrayField(Key, Rows); }
void Bump(FResult& R, const TCHAR* Key, int64 By = 1)
{ double Before = 0; R.Data->TryGetNumberField(Key, Before); R.Data->SetNumberField(Key, Before + double(By)); }
bool Fail(FResult& R, const TCHAR* Code, const FString& Error)
{ R.bSuccess = false; R.ErrorCode = Code; R.Error = Error; return false; }
FString NormalizeContentRoot(FString Path)
{ Path = Path.TrimStartAndEnd(); while (Path.Len() > 1 && Path.EndsWith(TEXT("/"))) { Path.LeftChopInline(1); } return Path; }
FString NormalizePackagePathParam(FString Path)
{
    Path = Path.TrimStartAndEnd(); Path.TrimQuotesInline(); FString LongName;
    if (FPackageName::TryConvertFilenameToLongPackageName(Path, LongName)) { return NormalizeContentRoot(LongName); }
    if (Path.Contains(TEXT("."))) { Path = FPackageName::ObjectPathToPackageName(Path); }
    return NormalizeContentRoot(Path);
}
const TArray<FString>& RuntimeForbiddenPrefixes()
{
    static const TArray<FString> Cached = []()
    {
    TArray<FString> Prefixes = {TEXT("/UnrealMCP/"), TEXT("/MCPUnreal/"), TEXT("/MCP_Unreal/"), TEXT("/CubelessControlRigTools/"),
        TEXT("/Script/UnrealMCP"), TEXT("/Script/MCPUnreal"), TEXT("/Script/MCP_Unreal"), TEXT("/Script/CubelessControlRigTools")};
    for (const TCHAR* PluginName : {TEXT("CubelessMCP_UE58"), TEXT("ModelContextProtocol"), TEXT("ToolsetRegistry"), TEXT("PythonScriptPlugin"), TEXT("EditorScriptingUtilities")})
    {
        Prefixes.Add(FString::Printf(TEXT("/%s/"), PluginName));
        if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName))
        { for (const FModuleDescriptor& Module : Plugin->GetDescriptor().Modules) { Prefixes.Add(FString::Printf(TEXT("/Script/%s"), *Module.Name.ToString())); } }
    }
    return Prefixes;
    }();
    return Cached;
}
bool MCPOnly(const FString& Text)
{
    // Preserve the legacy guard and include the installed project/official MCP module namespaces.
    for (const TCHAR* Token : {TEXT("UnrealMCP"), TEXT("MCPUnreal"), TEXT("mcp_unreal"), TEXT("CubelessControlRigTools"), TEXT("/Script/CubelessMCP")})
    { if (Text.Contains(Token, ESearchCase::IgnoreCase)) { return true; } }
    for (const FString& Prefix : RuntimeForbiddenPrefixes())
    { if (Text.Contains(Prefix, ESearchCase::IgnoreCase)) { return true; } }
    return false;
}
bool RuntimeDestinationAllowed(UObject* Object, FString& Error)
{
    if (!IsLiveObjectForMCP(Object) || MCPOnly(Object->GetPathName()))
    { Error = TEXT("Replacement destination is unreadable or belongs to an MCP/Editor integration namespace"); return false; }
    auto CheckChain = [&](UClass* Class) -> bool
    {
        TSet<UClass*> Seen;
        for (int32 Depth = 0; Class; ++Depth, Class = Class->GetSuperClass())
        {
            if (Depth >= 256 || Seen.Contains(Class) || !IsLiveObjectForMCP(Class) || MCPOnly(Class->GetPathName()))
            { Error = TEXT("Replacement destination class chain is incomplete or introduces an MCP/Editor integration module"); return false; }
            Seen.Add(Class);
        }
        return true;
    };
    if (!CheckChain(Object->GetClass())) { return false; }
    if (UClass* ClassObject = Cast<UClass>(Object)) { if (!CheckChain(ClassObject)) { return false; } }
    if (UBlueprint* Blueprint = Cast<UBlueprint>(Object))
    { if (!CheckChain(Blueprint->ParentClass) || (Blueprint->GeneratedClass && !CheckChain(Blueprint->GeneratedClass))) { return false; } }
    return true;
}
bool Decode(const FString& Text, TSharedPtr<FJsonValue>& Value, FString& Error)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    if (Text.Len() > MaxCarrierChars || !NoNul(Text)
        || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[") + Text + TEXT("]")), Values)
        || Values.Num() != 1 || !Values[0].IsValid())
    { Error = TEXT("Nullable carrier must contain exactly one bounded JSON value"); return false; }
    Value = Values[0]; return true;
}
bool MapInput(const FString& Text, TSharedPtr<FJsonObject>& Out, FString& Error)
{
    TSharedPtr<FJsonValue> Value; if (!Decode(Text, Value, Error)) { return false; }
    if (Value->Type == EJson::Null) { Out = MakeShared<FJsonObject>(); return true; }
    if (Value->Type != EJson::Object || !Value->AsObject().IsValid() || Value->AsObject()->Values.Num() > MaxRows)
    { Error = TEXT("Mapping carrier requires an object or null within the finite row bound"); return false; }
    Out = Value->AsObject(); return true;
}
bool TargetInput(const FString& Text, TArray<FString>& Out, FString& Error)
{
    TSharedPtr<FJsonValue> Value; if (!Decode(Text, Value, Error)) { return false; }
    if (Value->Type == EJson::Null) { return true; }
    if (Value->Type != EJson::Array || Value->AsArray().Num() > MaxRows)
    { Error = TEXT("target_paths requires a bounded string array or null"); return false; }
    TSet<FString> Seen;
    for (const auto& Row : Value->AsArray())
    {
        if (!Row.IsValid() || Row->Type != EJson::String || !NoNul(Row->AsString()) || Row->AsString().Len() >= NAME_SIZE)
        { Error = TEXT("target_paths contains an invalid or overlong string"); return false; }
        FString RawPath = FPackageName::ExportTextPathToObjectPath(Row->AsString()).TrimStartAndEnd(); RawPath.TrimQuotesInline(); RawPath.ReplaceInline(TEXT("\\"), TEXT("/"));
        FString Path = NormalizePackagePathParam(RawPath);
        if (!Path.IsEmpty() && !Seen.Contains(Path)) { Seen.Add(Path); Out.Add(Path); }
    }
    Out.Sort(); return true;
}
struct FIdentity
{
    TStrongObjectPtr<UObject> Hold;
    UPackage* Package = nullptr; UObject* Outer = nullptr; UClass* Class = nullptr; FString Path;
    explicit FIdentity(UObject* Object) : Hold(Object), Package(Object->GetOutermost()), Outer(Object->GetOuter()), Class(Object->GetClass()), Path(Object->GetPathName()) {}
    bool Current() const
    { UObject* Object = Hold.Get(); return IsLiveObjectForMCP(Object) && Object->GetOutermost() == Package && Object->GetOuter() == Outer && Object->GetClass() == Class && Object->GetPathName() == Path
        && !Class->HasAnyClassFlags(CLASS_NewerVersionExists)
        && (!Cast<UClass>(Object) || !Cast<UClass>(Object)->HasAnyClassFlags(CLASS_NewerVersionExists)); }
};
struct FWork
{
    explicit FWork(FResult& InResult) : Result(InResult) {}
    FResult& Result;
    TMap<UObject*, TUniquePtr<FIdentity>> Identities;
    TSet<UObject*> Mutable, Prepared, ReferenceTargets;
    TSet<UPackage*> Packages;
    const TCHAR* PropertyCounter = TEXT("path_property_remap_count");
    bool Failed = false;
    bool EditorCurrent()
    {
        if (Failed) { return false; }
        if (!IsEditorGameThread() || IsPlaySessionActive() || IsEngineExitRequested())
        { Failed = true; return Fail(Result, TEXT("editor_state_changed"), TEXT("Editor/game-thread/PIE state changed in callback; partial prefix retained")); }
        return true;
    }
    bool Capture(UObject* Object, bool CanWrite)
    {
        if (!EditorCurrent()) { return false; }
        if (!IsLiveObjectForMCP(Object)) { Failed = true; return Fail(Result, TEXT("target_unreadable"), TEXT("Loaded object is unreadable before capture")); }
        if (!Identities.Contains(Object))
        {
            if (Identities.Num() >= MaxObjects) { Failed = true; return Fail(Result, TEXT("scope_bound"), TEXT("Captured object scope exceeds finite bound")); }
            Identities.Add(Object, MakeUnique<FIdentity>(Object));
        }
        if (CanWrite) { Mutable.Add(Object); }
        return true;
    }
    bool Current(UObject* Object, bool RequireMutable = true)
    {
        if (!EditorCurrent()) { return false; }
        const auto* Id = Identities.Find(Object);
        if (!Id || !(*Id)->Current() || (RequireMutable && !Mutable.Contains(Object)))
        { Failed = true; return Fail(Result, TEXT("target_identity_changed"), TEXT("Captured target/object ownership changed; prefix retained without replay")); }
        if (RequireMutable)
        {
            // Modify/MarkPackageDirty can select an external package through the outer chain.
            UPackage* DirtyPackage = Object->GetPackage(); const auto* DirtyId = Identities.Find(DirtyPackage);
            if (!DirtyId || !Mutable.Contains(DirtyPackage) || !(*DirtyId)->Current())
            { Failed = true; return Fail(Result, TEXT("dirty_package_outside_scope"), TEXT("Actual external/outer-chain package is not a captured writable target; no follow-up write dispatched")); }
        }
        if (UEdGraphNode* Node = Cast<UEdGraphNode>(Object))
        {
            UEdGraph* Graph = Node->GetGraph(); const auto* GraphId = Identities.Find(Graph);
            if (!GraphId || !(*GraphId)->Current() || !Graph->Nodes.Contains(Node))
            { Failed = true; return Fail(Result, TEXT("graph_membership_changed"), TEXT("Captured node no longer belongs to its original graph")); }
        }
        if (UEdGraph* Graph = Cast<UEdGraph>(Object))
        {
            if (UBlueprint* Blueprint = Graph->GetTypedOuter<UBlueprint>())
            {
                const auto* BlueprintId = Identities.Find(Blueprint); TArray<UEdGraph*> Graphs;
                if (!BlueprintId || !(*BlueprintId)->Current()) { Failed = true; return Fail(Result, TEXT("graph_membership_changed"), TEXT("Captured Blueprint owner changed")); }
                Blueprint->GetAllGraphs(Graphs);
                if (!Graphs.Contains(Graph)) { Failed = true; return Fail(Result, TEXT("graph_membership_changed"), TEXT("Captured graph is detached from its original Blueprint")); }
            }
        }
        return true;
    }
    bool Prepare(UObject* Object)
    {
        if (!Current(Object)) { return false; }
        FString Error;
        for (UObject* Destination : ReferenceTargets)
        { const auto* Id = Identities.Find(Destination); if (!Id || !(*Id)->Current() || !RuntimeDestinationAllowed(Destination, Error))
            { Failed = true; return Fail(Result, TEXT("runtime_dependency_refused"), Error.IsEmpty() ? TEXT("Replacement destination identity changed before write") : Error); } }
        if (Prepared.Contains(Object)) { return true; }
        Prepared.Add(Object); Result.bMutationAttempted = true;
        const bool Dirty = Object->GetOutermost()->IsDirty(); Bump(Result, TEXT("explicit_modify_calls")); Object->Modify();
        if (!Current(Object)) { return false; }
        if (!Dirty && Object->GetOutermost()->IsDirty()) { Result.bChanged = true; }
        return true;
    }
};
FWork* ActiveWork = nullptr;
bool Prepare(UObject* Object) { return ActiveWork && ActiveWork->Prepare(Object); }
bool Current(UObject* Object) { return ActiveWork && ActiveWork->Current(Object); }
void Changed() { if (ActiveWork) { ActiveWork->Result.bChanged = true; } }
bool MarkCapturedDirty(UObject* Object, bool AlsoPackage = false)
{
    if (!Current(Object)) { return false; }
    UPackage* Package = Object->GetPackage();
    if (!Current(Package)) { return false; }
    const bool WasDirty = Package->IsDirty();
    ActiveWork->Result.bMutationAttempted = true;
    Bump(ActiveWork->Result, TEXT("explicit_mark_dirty_calls")); Object->MarkPackageDirty();
    if (!Current(Object) || !Current(Package)) { return false; }
    if (!WasDirty && Package->IsDirty()) { Changed(); }
    if (AlsoPackage)
    {
        Bump(ActiveWork->Result, TEXT("explicit_mark_dirty_calls")); Package->MarkPackageDirty();
        if (!Current(Object) || !Current(Package)) { return false; }
    }
    return true;
}
void GraphChanged() { Changed(); Bump(ActiveWork->Result, TEXT("blueprint_graph_reference_remap_count")); }
FString ApplyPathReplacements(FString Text, const TArray<FPathStringReplacement>& Replacements);
UObject* FindReplacementObject(const TMap<UObject*, UObject*>& ReplacementMap, UObject* Object);
bool NodeMayChange(UEdGraphNode* Node, UBlueprint* Blueprint, const TMap<UObject*, UObject*>& Map, const TArray<FPathStringReplacement>& Paths);
bool PrepareBlueprintScope(UBlueprint* Blueprint, const TMap<UObject*, UObject*>& Map, const TArray<FPathStringReplacement>& Paths);
// Preserve the public archive traversal while validating each captured nested object and actual reflected destination slot.
class FGuardedReferenceArchive final : public FArchiveReplaceObjectRef<UObject>
{
public:
    using FArchiveReplaceObjectRef<UObject>::operator<<;
    FGuardedReferenceArchive(FWork& InWork, UObject* Root, const TMap<UObject*, UObject*>& Map)
        : FArchiveReplaceObjectRef<UObject>(Root, Map, EArchiveReplaceObjectFlags::IgnoreOuterRef | EArchiveReplaceObjectFlags::IgnoreArchetypeRef
            | EArchiveReplaceObjectFlags::IncludeClassGeneratedByRef | EArchiveReplaceObjectFlags::DelayStart), Work(InWork)
    { SerializeSearchObject(); }
    virtual FArchive& operator<<(UObject*& Object) override
    {
        if (Work.Failed) { return *this; }
        if (++ReferenceVisits > 262144 || !Work.Current(SerializingObject))
        { if (!Work.Failed) { Work.Failed = true; Fail(Work.Result, TEXT("archive_scope_bound"), TEXT("Reference serialization exceeds finite visit bound; prefix retained")); } return *this; }
        if (UObject* const* Destination = ReplacementMap.Find(Object))
        {
            FString Error;
            if (!Work.Current(*Destination, false) || !RuntimeDestinationAllowed(*Destination, Error))
            { if (!Work.Failed) { Work.Failed = true; Fail(Work.Result, TEXT("runtime_dependency_refused"), Error); } return *this; }
            FProperty* Property = GetSerializedProperty(); bool Fits = true;
            if (const auto* ObjectProperty = CastField<FObjectPropertyBase>(Property))
            { Fits = ObjectProperty->PropertyClass && (*Destination)->IsA(ObjectProperty->PropertyClass); }
            if (const auto* ClassProperty = CastField<FClassProperty>(Property))
            { UClass* Class = Cast<UClass>(*Destination); Fits = Fits && Class && ClassProperty->MetaClass && Class->IsChildOf(ClassProperty->MetaClass); }
            if (const auto* SoftClassProperty = CastField<FSoftClassProperty>(Property))
            { UClass* Class = Cast<UClass>(*Destination); Fits = Fits && Class && SoftClassProperty->MetaClass && Class->IsChildOf(SoftClassProperty->MetaClass); }
            if (const auto* InterfaceProperty = CastField<FInterfaceProperty>(Property))
            { Fits = InterfaceProperty->InterfaceClass && (*Destination)->GetClass()->ImplementsInterface(InterfaceProperty->InterfaceClass); }
            if (!Fits)
            { Work.Failed = true; Fail(Work.Result, TEXT("reference_type_incompatible"), TEXT("Replacement destination is incompatible with its reflected object/class/interface slot; prior archive prefix retained")); return *this; }
        }
        return FArchiveReplaceObjectRef<UObject>::operator<<(Object);
    }
protected:
    virtual void SerializeObject(UObject* Object) override
    {
        if (Work.Failed || ++SerializedCount > MaxObjects || !Work.Prepare(Object))
        { if (!Work.Failed) { Work.Failed = true; Fail(Work.Result, TEXT("archive_scope_bound"), TEXT("Nested serialization exceeds captured object scope")); } return; }
        FArchiveReplaceObjectRefBase::SerializeObject(Object);
        Work.Current(Object);
    }
private:
    FWork& Work; int32 ReferenceVisits = 0, SerializedCount = 0;
};

bool ShouldSkipArchiveObjectReferenceRemap(const UObject* Object)
{
    if (!IsLiveObjectForMCP(Object))
    {
        return true;
    }

    // Blueprint CDOs validate their UberGraphFrame while serialized. Direct archive
    // replacement can trip that validation before the Blueprint compile pass repairs it.
    return Object->HasAnyFlags(RF_ClassDefaultObject) && Cast<UBlueprintGeneratedClass>(Object->GetClass()) != nullptr;
}

bool ShouldSkipAssetArchiveObjectReferenceRemap(const UObject* Object)
{
    if (ShouldSkipArchiveObjectReferenceRemap(Object))
    {
        return true;
    }

    if (Object->IsA<UBlueprint>())
    {
        return true;
    }

    return Cast<UBlueprintGeneratedClass>(Object->GetClass()) != nullptr;
}
void AddReplacementMapping(TMap<UObject*, UObject*>& ReplacementMap, UObject* SourceObject, UObject* TargetObject)
{
    if (SourceObject && TargetObject && SourceObject != TargetObject)
    {
        ReplacementMap.Add(SourceObject, TargetObject);
    }
}
void AddBlueprintReplacementMappings(TMap<UObject*, UObject*>& ReplacementMap, UObject* SourceObject, UObject* TargetObject)
{
    UBlueprint* SourceBlueprint = Cast<UBlueprint>(SourceObject);
    UBlueprint* TargetBlueprint = Cast<UBlueprint>(TargetObject);
    if (!SourceBlueprint || !TargetBlueprint)
    {
        return;
    }

    AddReplacementMapping(ReplacementMap, SourceBlueprint->GeneratedClass, TargetBlueprint->GeneratedClass);
    AddReplacementMapping(ReplacementMap, SourceBlueprint->SkeletonGeneratedClass, TargetBlueprint->SkeletonGeneratedClass);

    if (SourceBlueprint->GeneratedClass && TargetBlueprint->GeneratedClass)
    {
        AddReplacementMapping(
            ReplacementMap,
            SourceBlueprint->GeneratedClass->GetDefaultObject(false),
            TargetBlueprint->GeneratedClass->GetDefaultObject(false));
    }

    if (SourceBlueprint->SkeletonGeneratedClass && TargetBlueprint->SkeletonGeneratedClass)
    {
        AddReplacementMapping(
            ReplacementMap,
            SourceBlueprint->SkeletonGeneratedClass->GetDefaultObject(false),
            TargetBlueprint->SkeletonGeneratedClass->GetDefaultObject(false));
    }

    AddReplacementMapping(ReplacementMap, SourceBlueprint->SimpleConstructionScript, TargetBlueprint->SimpleConstructionScript);
    if (SourceBlueprint->SimpleConstructionScript && TargetBlueprint->SimpleConstructionScript)
    {
        const TArray<USCS_Node*>& SourceNodes = SourceBlueprint->SimpleConstructionScript->GetAllNodes();
        const TArray<USCS_Node*>& TargetNodes = TargetBlueprint->SimpleConstructionScript->GetAllNodes();
        if (SourceNodes.Num() > MaxRows || TargetNodes.Num() > MaxRows)
        { ActiveWork->Failed = true; Fail(ActiveWork->Result, TEXT("scs_scope_bound"), TEXT("SCS replacement expansion exceeds finite node scope")); return; }
        for (USCS_Node* SourceNode : SourceNodes)
        {
            if (!SourceNode)
            {
                continue;
            }

            for (USCS_Node* TargetNode : TargetNodes)
            {
                if (TargetNode && TargetNode->GetVariableName() == SourceNode->GetVariableName())
                {
                    AddReplacementMapping(ReplacementMap, SourceNode, TargetNode);
                    AddReplacementMapping(ReplacementMap, SourceNode->ComponentTemplate, TargetNode->ComponentTemplate);
                    break;
                }
            }
        }
    }
}
UObject* FindReplacementObject(const TMap<UObject*, UObject*>& ReplacementMap, UObject* Object)
{
    if (!IsLiveObjectForMCP(Object))
    {
        return nullptr;
    }

    if (UObject* const* Replacement = ReplacementMap.Find(Object))
    {
        if (!IsLiveObjectForMCP(*Replacement)) { return nullptr; }
        FString Error;
        if (ActiveWork && (!ActiveWork->Current(*Replacement, false) || !RuntimeDestinationAllowed(*Replacement, Error)))
        { if (!ActiveWork->Failed) { ActiveWork->Failed = true; Fail(ActiveWork->Result, TEXT("runtime_dependency_refused"), Error); } return nullptr; }
        return *Replacement;
    }
    return nullptr;
}

bool RemapSimpleMemberReference(FSimpleMemberReference& Reference, const TMap<UObject*, UObject*>& ReplacementMap)
{
    if (UObject* ReplacementParent = FindReplacementObject(ReplacementMap, Reference.MemberParent))
    {
        Reference.MemberParent = ReplacementParent;
        return true;
    }
    return false;
}

bool RemapTerminalType(FEdGraphTerminalType& TerminalType, const TMap<UObject*, UObject*>& ReplacementMap)
{
    if (UObject* Replacement = FindReplacementObject(ReplacementMap, TerminalType.TerminalSubCategoryObject.Get(true)))
    {
        TerminalType.TerminalSubCategoryObject = Replacement;
        return true;
    }
    return false;
}

bool RemapPinType(FEdGraphPinType& PinType, const TMap<UObject*, UObject*>& ReplacementMap)
{
    bool bChanged = false;
    if (UObject* Replacement = FindReplacementObject(ReplacementMap, PinType.PinSubCategoryObject.Get(true)))
    {
        PinType.PinSubCategoryObject = Replacement;
        bChanged = true;
    }

    bChanged |= RemapSimpleMemberReference(PinType.PinSubCategoryMemberReference, ReplacementMap);
    bChanged |= RemapTerminalType(PinType.PinValueType, ReplacementMap);
    return bChanged;
}

bool RemapMemberReferenceParent(FMemberReference& Reference, const TMap<UObject*, UObject*>& ReplacementMap)
{
    if (Reference.IsSelfContext() || Reference.IsLocalScope())
    {
        return false;
    }

    UClass* ParentClass = Reference.GetMemberParentClass();
    UClass* ReplacementClass = Cast<UClass>(FindReplacementObject(ReplacementMap, ParentClass));
    if (!ReplacementClass)
    {
        return false;
    }

    Reference.SetExternalMember(Reference.GetMemberName(), ReplacementClass, Reference.GetMemberGuid());
    return true;
}

FGuid FindBlueprintSelfVariableGuid(UBlueprint* Blueprint, const FName MemberName)
{
    if (!Blueprint || MemberName.IsNone())
    {
        return FGuid();
    }

    FGuid Guid = FBlueprintEditorUtils::FindMemberVariableGuidByName(Blueprint, MemberName);
    if (Guid.IsValid())
    {
        return Guid;
    }

    if (Blueprint->SimpleConstructionScript)
    {
        const TArray<USCS_Node*>& SCSNodes = Blueprint->SimpleConstructionScript->GetAllNodes();
        for (USCS_Node* SCSNode : SCSNodes)
        {
            if (SCSNode && SCSNode->GetVariableName() == MemberName && SCSNode->VariableGuid.IsValid())
            {
                return SCSNode->VariableGuid;
            }
        }
    }

    for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
    {
        if (Variable.VarName == MemberName && Variable.VarGuid.IsValid())
        {
            return Variable.VarGuid;
        }
    }

    for (const FBPVariableDescription& Variable : Blueprint->GeneratedVariables)
    {
        if (Variable.VarName == MemberName && Variable.VarGuid.IsValid())
        {
            return Variable.VarGuid;
        }
    }

    return FGuid();
}

FProperty* FindBlueprintPropertyByName(UBlueprint* Blueprint, const FName MemberName)
{
    if (!Blueprint || MemberName.IsNone())
    {
        return nullptr;
    }

    if (Blueprint->SkeletonGeneratedClass)
    {
        if (FProperty* Property = FindFProperty<FProperty>(Blueprint->SkeletonGeneratedClass, MemberName))
        {
            return Property;
        }
    }

    if (Blueprint->GeneratedClass)
    {
        if (FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, MemberName))
        {
            return Property;
        }
    }

    return nullptr;
}

UFunction* FindBlueprintFunctionByName(UBlueprint* Blueprint, const FName MemberName)
{
    if (!Blueprint || MemberName.IsNone())
    {
        return nullptr;
    }

    if (Blueprint->SkeletonGeneratedClass)
    {
        if (UFunction* Function = FindUField<UFunction>(Blueprint->SkeletonGeneratedClass, MemberName))
        {
            return Function;
        }
    }

    if (Blueprint->GeneratedClass)
    {
        if (UFunction* Function = FindUField<UFunction>(Blueprint->GeneratedClass, MemberName))
        {
            return Function;
        }
    }

    return nullptr;
}

bool RemapSelfMemberReference(FMemberReference& Reference, UBlueprint* Blueprint)
{
    if (!Blueprint || !Reference.IsSelfContext() || Reference.IsLocalScope())
    {
        return false;
    }

    const FName MemberName = Reference.GetMemberName();
    const FGuid Guid = FindBlueprintSelfVariableGuid(Blueprint, MemberName);
    if (Guid.IsValid() && Guid != Reference.GetMemberGuid())
    {
        Reference.SetSelfMember(MemberName, Guid);
        return true;
    }

    return false;
}

bool RemapVariableReference(UK2Node_Variable* VariableNode, UBlueprint* Blueprint, const TMap<UObject*, UObject*>& ReplacementMap)
{
    if (!VariableNode)
    {
        return false;
    }

    const FName MemberName = VariableNode->VariableReference.GetMemberName();
    if (VariableNode->VariableReference.IsLocalScope())
    {
        return false;
    }

    if (VariableNode->VariableReference.IsSelfContext())
    {
        if (!Prepare(VariableNode)) { return false; }
        if (FProperty* ReplacementProperty = FindBlueprintPropertyByName(Blueprint, MemberName))
        {
            VariableNode->SetFromProperty(ReplacementProperty, true, ReplacementProperty->GetOwnerClass());
            return true;
        }

        const FGuid Guid = FindBlueprintSelfVariableGuid(Blueprint, MemberName);
        if (Guid.IsValid() && Guid != VariableNode->VariableReference.GetMemberGuid())
        {
            VariableNode->VariableReference.SetSelfMember(MemberName, Guid);
            return true;
        }

        return false;
    }

    UClass* ParentClass = VariableNode->VariableReference.GetMemberParentClass();
    UClass* ReplacementClass = Cast<UClass>(FindReplacementObject(ReplacementMap, ParentClass));
    if (!ReplacementClass)
    {
        return false;
    }

    if (!Prepare(VariableNode)) { return false; }
    if (const FProperty* ReplacementProperty = FindFProperty<FProperty>(ReplacementClass, MemberName))
    {
        VariableNode->SetFromProperty(ReplacementProperty, false, ReplacementClass);
    }
    else
    {
        VariableNode->VariableReference.SetExternalMember(
            MemberName,
            ReplacementClass,
            VariableNode->VariableReference.GetMemberGuid());
    }
    return true;
}

bool RemapFunctionReference(UK2Node_CallFunction* FunctionNode, UBlueprint* Blueprint, const TMap<UObject*, UObject*>& ReplacementMap)
{
    if (!FunctionNode || FunctionNode->FunctionReference.IsLocalScope())
    {
        return false;
    }

    if (FunctionNode->FunctionReference.IsSelfContext())
    {
        if (const UFunction* ReplacementFunction = FindBlueprintFunctionByName(Blueprint, FunctionNode->FunctionReference.GetMemberName()))
        {
            if (!Prepare(FunctionNode)) { return false; }
            FunctionNode->SetFromFunction(ReplacementFunction);
            return true;
        }

        return false;
    }

    UClass* ParentClass = FunctionNode->FunctionReference.GetMemberParentClass();
    UClass* ReplacementClass = Cast<UClass>(FindReplacementObject(ReplacementMap, ParentClass));
    if (!ReplacementClass)
    {
        return false;
    }

    if (!Prepare(FunctionNode)) { return false; }
    if (const UFunction* ReplacementFunction = FindUField<UFunction>(ReplacementClass, FunctionNode->FunctionReference.GetMemberName()))
    {
        FunctionNode->SetFromFunction(ReplacementFunction);
    }
    else
    {
        FunctionNode->FunctionReference.SetExternalMember(
            FunctionNode->FunctionReference.GetMemberName(),
            ReplacementClass,
            FunctionNode->FunctionReference.GetMemberGuid());
    }
    return true;
}

bool RemapDelegateReference(UK2Node_BaseMCDelegate* DelegateNode, UBlueprint* Blueprint, const TMap<UObject*, UObject*>& ReplacementMap)
{
    if (!DelegateNode || DelegateNode->DelegateReference.IsLocalScope())
    {
        return false;
    }

    const FName MemberName = DelegateNode->DelegateReference.GetMemberName();
    if (DelegateNode->DelegateReference.IsSelfContext())
    {
        if (FProperty* ReplacementProperty = FindBlueprintPropertyByName(Blueprint, MemberName))
        {
            if (!Prepare(DelegateNode)) { return false; }
            DelegateNode->SetFromProperty(ReplacementProperty, true, ReplacementProperty->GetOwnerClass());
            return true;
        }

        return RemapSelfMemberReference(DelegateNode->DelegateReference, Blueprint);
    }

    UClass* ParentClass = DelegateNode->DelegateReference.GetMemberParentClass();
    UClass* ReplacementClass = Cast<UClass>(FindReplacementObject(ReplacementMap, ParentClass));
    if (!ReplacementClass)
    {
        return false;
    }

    if (!Prepare(DelegateNode)) { return false; }
    if (const FProperty* ReplacementProperty = FindFProperty<FProperty>(ReplacementClass, MemberName))
    {
        DelegateNode->SetFromProperty(ReplacementProperty, false, ReplacementClass);
    }
    else
    {
        DelegateNode->DelegateReference.SetExternalMember(MemberName, ReplacementClass, DelegateNode->DelegateReference.GetMemberGuid());
    }
    return true;
}

bool RemapEditableUserPins(UK2Node_EditablePinBase* EditableNode, const TMap<UObject*, UObject*>& ReplacementMap, const TArray<FPathStringReplacement>& PathReplacements)
{
    if (!EditableNode)
    {
        return false;
    }

    bool bChanged = false;
    for (TSharedPtr<FUserPinInfo>& UserPin : EditableNode->UserDefinedPins)
    {
        if (!UserPin.IsValid())
        {
            continue;
        }

        bChanged |= RemapPinType(UserPin->PinType, ReplacementMap);
        const FString NewDefaultValue = ApplyPathReplacements(UserPin->PinDefaultValue, PathReplacements);
        if (NewDefaultValue != UserPin->PinDefaultValue)
        {
            UserPin->PinDefaultValue = NewDefaultValue;
            bChanged = true;
        }
    }

    if (bChanged)
    {
        if (!Prepare(EditableNode)) { return false; }
    }
    return bChanged;
}

bool RemapFunctionEntryLocalVariables(UK2Node_FunctionEntry* FunctionEntry, const TMap<UObject*, UObject*>& ReplacementMap, const TArray<FPathStringReplacement>& PathReplacements)
{
    if (!FunctionEntry)
    {
        return false;
    }

    bool bChanged = false;
    for (FBPVariableDescription& LocalVariable : FunctionEntry->LocalVariables)
    {
        bChanged |= RemapPinType(LocalVariable.VarType, ReplacementMap);
        const FString NewDefaultValue = ApplyPathReplacements(LocalVariable.DefaultValue, PathReplacements);
        if (NewDefaultValue != LocalVariable.DefaultValue)
        {
            LocalVariable.DefaultValue = NewDefaultValue;
            bChanged = true;
        }
    }

    if (bChanged)
    {
        if (!Prepare(FunctionEntry)) { return false; }
    }
    return bChanged;
}

bool RemapStructOperation(UK2Node_StructOperation* StructNode, const TMap<UObject*, UObject*>& ReplacementMap)
{
    if (!StructNode || !StructNode->StructType)
    {
        return false;
    }

    UScriptStruct* ReplacementStruct = Cast<UScriptStruct>(FindReplacementObject(ReplacementMap, StructNode->StructType));
    if (!ReplacementStruct || ReplacementStruct == StructNode->StructType)
    {
        return false;
    }

    if (!Prepare(StructNode)) { return false; }
    StructNode->StructType = ReplacementStruct;
    return true;
}

bool RemapDynamicCastTarget(UK2Node_DynamicCast* CastNode, const TMap<UObject*, UObject*>& ReplacementMap)
{
    if (!CastNode || !CastNode->TargetType)
    {
        return false;
    }

    UClass* ReplacementClass = Cast<UClass>(FindReplacementObject(ReplacementMap, CastNode->TargetType));
    if (!ReplacementClass || ReplacementClass == CastNode->TargetType)
    {
        return false;
    }

    if (!Prepare(CastNode)) { return false; }
    CastNode->TargetType = ReplacementClass;
    return true;
}

bool RemapGraphPin(UEdGraphPin* Pin, const TMap<UObject*, UObject*>& ReplacementMap, const TArray<FPathStringReplacement>& PathReplacements)
{
    if (!Pin)
    {
        return false;
    }

    bool bChanged = RemapPinType(Pin->PinType, ReplacementMap);
    if (UObject* Replacement = FindReplacementObject(ReplacementMap, Pin->DefaultObject))
    {
        Pin->DefaultObject = Replacement;
        bChanged = true;
    }

    const FString NewDefaultValue = ApplyPathReplacements(Pin->DefaultValue, PathReplacements);
    if (NewDefaultValue != Pin->DefaultValue)
    {
        Pin->DefaultValue = NewDefaultValue;
        bChanged = true;
    }

    const FString NewAutogeneratedDefaultValue = ApplyPathReplacements(Pin->AutogeneratedDefaultValue, PathReplacements);
    if (NewAutogeneratedDefaultValue != Pin->AutogeneratedDefaultValue)
    {
        Pin->AutogeneratedDefaultValue = NewAutogeneratedDefaultValue;
        bChanged = true;
    }

    if (bChanged)
    {
        if (!Current(Pin->GetOwningNode())) { return false; }
    }
    return bChanged;
}

bool NodeHasSplitPins(const UEdGraphNode* Node)
{
    if (!Node)
    {
        return false;
    }

    for (const UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->SubPins.Num() > 0)
        {
            return true;
        }
    }
    return false;
}

int32 RemapBlueprintGraphReferences(UBlueprint* Blueprint, const TMap<UObject*, UObject*>& ReplacementMap, const TArray<FPathStringReplacement>& PathReplacements)
{
    if (!Blueprint || ReplacementMap.IsEmpty())
    {
        return 0;
    }

    if (!PrepareBlueprintScope(Blueprint, ReplacementMap, PathReplacements)) { return 0; }
    int32 RemapCount = 0;
    bool bBlueprintChanged = false;

    for (FBPVariableDescription& Variable : Blueprint->NewVariables)
    {
        bool bVariableChanged = RemapPinType(Variable.VarType, ReplacementMap);
        const FString NewDefaultValue = ApplyPathReplacements(Variable.DefaultValue, PathReplacements);
        if (NewDefaultValue != Variable.DefaultValue)
        {
            Variable.DefaultValue = NewDefaultValue;
            bVariableChanged = true;
        }

        if (bVariableChanged)
        {
            ++RemapCount; GraphChanged();
            bBlueprintChanged = true;
        }
    }

    for (FBPVariableDescription& Variable : Blueprint->GeneratedVariables)
    {
        bool bVariableChanged = RemapPinType(Variable.VarType, ReplacementMap);
        const FString NewDefaultValue = ApplyPathReplacements(Variable.DefaultValue, PathReplacements);
        if (NewDefaultValue != Variable.DefaultValue)
        {
            Variable.DefaultValue = NewDefaultValue;
            bVariableChanged = true;
        }

        if (bVariableChanged)
        {
            ++RemapCount; GraphChanged();
            bBlueprintChanged = true;
        }
    }

    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    TArray<UEdGraphNode*> NodesToReconstruct;
    for (UEdGraph* Graph : Graphs)
    {
        if (!Graph)
        {
            continue;
        }

        const TArray<TObjectPtr<UEdGraphNode>> Nodes = Graph->Nodes;
        for (UEdGraphNode* Node : Nodes)
        {
            if (!Node)
            {
                continue;
            }

            if (!Current(Graph) || !Current(Node)) { return RemapCount; }
            if (NodeMayChange(Node, Blueprint, ReplacementMap, PathReplacements) && !Prepare(Node)) { return RemapCount; }
            bool bNodeChanged = false;
            bool bNeedsReconstruct = false;
            const bool bHasSplitPins = NodeHasSplitPins(Node);
            const bool bIsStructOperation = Node->IsA<UK2Node_StructOperation>();
            if (UK2Node_CallFunction* FunctionNode = Cast<UK2Node_CallFunction>(Node))
            {
                const bool bFunctionChanged = RemapFunctionReference(FunctionNode, Blueprint, ReplacementMap);
                    if (!Current(Node)) { return RemapCount; }
                bNodeChanged |= bFunctionChanged;
                bNeedsReconstruct |= bFunctionChanged;
                if (UK2Node_CallFunctionOnMember* FunctionOnMemberNode = Cast<UK2Node_CallFunctionOnMember>(FunctionNode))
                {
                    const bool bMemberParentChanged = RemapMemberReferenceParent(FunctionOnMemberNode->MemberVariableToCallOn, ReplacementMap);
                    if (!Current(Node)) { return RemapCount; }
                    const bool bSelfMemberChanged = RemapSelfMemberReference(FunctionOnMemberNode->MemberVariableToCallOn, Blueprint);
                    if (!Current(Node)) { return RemapCount; }
                    bNodeChanged |= bMemberParentChanged || bSelfMemberChanged;
                    bNeedsReconstruct |= bMemberParentChanged || bSelfMemberChanged;
                }
            }
            if (UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(Node))
            {
                const bool bVariableChanged = RemapVariableReference(VariableNode, Blueprint, ReplacementMap);
                    if (!Current(Node)) { return RemapCount; }
                bNodeChanged |= bVariableChanged;
                bNeedsReconstruct |= bVariableChanged;
            }
            if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
            {
                const bool bEventParentChanged = RemapMemberReferenceParent(EventNode->EventReference, ReplacementMap);
                    if (!Current(Node)) { return RemapCount; }
                const bool bSelfEventChanged = RemapSelfMemberReference(EventNode->EventReference, Blueprint);
                    if (!Current(Node)) { return RemapCount; }
                bNodeChanged |= bEventParentChanged || bSelfEventChanged;
                bNeedsReconstruct |= bEventParentChanged || bSelfEventChanged;
            }
            if (UK2Node_BaseMCDelegate* DelegateNode = Cast<UK2Node_BaseMCDelegate>(Node))
            {
                const bool bDelegateChanged = RemapDelegateReference(DelegateNode, Blueprint, ReplacementMap);
                    if (!Current(Node)) { return RemapCount; }
                bNodeChanged |= bDelegateChanged;
                bNeedsReconstruct |= bDelegateChanged;
            }
            if (UK2Node_FunctionTerminator* FunctionTerminator = Cast<UK2Node_FunctionTerminator>(Node))
            {
                const bool bTerminatorParentChanged = RemapMemberReferenceParent(FunctionTerminator->FunctionReference, ReplacementMap);
                    if (!Current(Node)) { return RemapCount; }
                const bool bSelfTerminatorChanged = RemapSelfMemberReference(FunctionTerminator->FunctionReference, Blueprint);
                    if (!Current(Node)) { return RemapCount; }
                bNodeChanged |= bTerminatorParentChanged || bSelfTerminatorChanged;
                bNeedsReconstruct |= bTerminatorParentChanged || bSelfTerminatorChanged;
            }
            if (UK2Node_EditablePinBase* EditableNode = Cast<UK2Node_EditablePinBase>(Node))
            {
                bNodeChanged |= RemapEditableUserPins(EditableNode, ReplacementMap, PathReplacements);
                if (!Current(Node)) { return RemapCount; }
            }
            if (UK2Node_FunctionEntry* FunctionEntry = Cast<UK2Node_FunctionEntry>(Node))
            {
                bNodeChanged |= RemapFunctionEntryLocalVariables(FunctionEntry, ReplacementMap, PathReplacements);
                if (!Current(Node)) { return RemapCount; }
            }
            if (UK2Node_StructOperation* StructNode = Cast<UK2Node_StructOperation>(Node))
            {
                bNodeChanged |= RemapStructOperation(StructNode, ReplacementMap);
                if (!Current(Node)) { return RemapCount; }
            }
            if (UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node))
            {
                const bool bCastTargetChanged = RemapDynamicCastTarget(CastNode, ReplacementMap);
                    if (!Current(Node)) { return RemapCount; }
                bNodeChanged |= bCastTargetChanged;
                bNeedsReconstruct |= bCastTargetChanged;
            }

            for (UEdGraphPin* Pin : Node->Pins)
            {
                bNodeChanged |= RemapGraphPin(Pin, ReplacementMap, PathReplacements);
                if (!Current(Node)) { return RemapCount; }
            }

            if (bNodeChanged)
            {
                ++RemapCount; GraphChanged();
                bBlueprintChanged = true;
                if (bNeedsReconstruct && !bHasSplitPins && !bIsStructOperation)
                {
                    NodesToReconstruct.AddUnique(Node);
                }
            }
        }
    }

    for (UEdGraphNode* Node : NodesToReconstruct)
    {
        if (!Node)
        {
            continue;
        }

        if (!Prepare(Node)) { return RemapCount; }
        Bump(ActiveWork->Result, TEXT("node_reconstruct_calls")); Node->ReconstructNode();
        if (!Current(Blueprint) || !Current(Node) || Node->Pins.Num() > MaxPins)
        { if (!ActiveWork->Failed) { ActiveWork->Failed = true; Fail(ActiveWork->Result, TEXT("pin_scope_bound"), TEXT("Reconstructed pin scope exceeds finite bound")); } return RemapCount; }
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (RemapGraphPin(Pin, ReplacementMap, PathReplacements))
            {
                ++RemapCount; GraphChanged();
            }
        }
    }

    if (bBlueprintChanged)
    {
        if (!Current(Blueprint)) { return RemapCount; }
        Bump(ActiveWork->Result, TEXT("mark_blueprint_structural_calls")); FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        if (!Current(Blueprint)) { return RemapCount; }
        if (!MarkCapturedDirty(Blueprint)) { return RemapCount; }
    }

    return RemapCount;
}
void AddPathReplacement(TArray<FPathStringReplacement>& Replacements, const FString& Source, const FString& Target)
{
    if (!Source.IsEmpty() && !Target.IsEmpty() && Source != Target)
    {
        Replacements.Add({Source, Target});
    }
}

void AddObjectPathReplacements(TArray<FPathStringReplacement>& Replacements, UObject* SourceObject, UObject* TargetObject)
{
    if (!IsValid(SourceObject) || !IsValid(TargetObject))
    {
        return;
    }

    UPackage* SourcePackage = SourceObject->GetOutermost();
    UPackage* TargetPackage = TargetObject->GetOutermost();
    if (!SourcePackage || !TargetPackage)
    {
        return;
    }

    AddPathReplacement(Replacements, SourceObject->GetPathName(), TargetObject->GetPathName());
    AddPathReplacement(Replacements, SourcePackage->GetName(), TargetPackage->GetName());

    UBlueprint* SourceBlueprint = Cast<UBlueprint>(SourceObject);
    UBlueprint* TargetBlueprint = Cast<UBlueprint>(TargetObject);
    if (SourceBlueprint && TargetBlueprint)
    {
        AddPathReplacement(Replacements,
            SourceObject->GetPathName() + TEXT("_C"),
            TargetObject->GetPathName() + TEXT("_C"));
        AddPathReplacement(Replacements,
            SourcePackage->GetName() + TEXT(".") + SourceObject->GetName() + TEXT("_C"),
            TargetPackage->GetName() + TEXT(".") + TargetObject->GetName() + TEXT("_C"));
        if (IsValid(SourceBlueprint->GeneratedClass) && IsValid(TargetBlueprint->GeneratedClass))
        {
            AddPathReplacement(Replacements, SourceBlueprint->GeneratedClass->GetPathName(), TargetBlueprint->GeneratedClass->GetPathName());
        }
        if (IsValid(SourceBlueprint->SkeletonGeneratedClass) && IsValid(TargetBlueprint->SkeletonGeneratedClass))
        {
            AddPathReplacement(Replacements, SourceBlueprint->SkeletonGeneratedClass->GetPathName(), TargetBlueprint->SkeletonGeneratedClass->GetPathName());
        }
    }
}

FString ApplyPathReplacements(FString Text, const TArray<FPathStringReplacement>& Replacements)
{
    for (const FPathStringReplacement& Replacement : Replacements)
    {
        if (Text.Contains(Replacement.Source, ESearchCase::CaseSensitive))
        {
            Text.ReplaceInline(*Replacement.Source, *Replacement.Target, ESearchCase::CaseSensitive);
        }
    }
    return Text;
}

bool ContainsReplacementSource(const FString& Text, const TArray<FPathStringReplacement>& Replacements)
{
    for (const FPathStringReplacement& Replacement : Replacements)
    {
        if (Text.Contains(Replacement.Source, ESearchCase::CaseSensitive))
        {
            return true;
        }
    }
    return false;
}

struct FPropertySnapshot
{
    FName Name; FProperty* Expected = nullptr; UStruct* Owner = nullptr;
};
bool SnapshotProperties(UObject* Object, TArray<FPropertySnapshot>& Rows)
{
    if (!Current(Object)) { return false; }
    // No virtual export/import or mutation callback runs while this live iterator exists.
    for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
    {
        FProperty* Property = *It;
        if (!Property || Property->HasAnyPropertyFlags(CPF_Transient)) { continue; }
        UStruct* Owner = Property->GetOwnerStruct();
        if (Rows.Num() >= MaxRows)
        { ActiveWork->Failed = true; return Fail(ActiveWork->Result, TEXT("property_scope_bound"), TEXT("Property snapshot exceeds finite scope")); }
        if (!Owner) { ActiveWork->Failed = true; return Fail(ActiveWork->Result, TEXT("property_owner_missing"), TEXT("Property snapshot has no readable owner")); }
        if (!ActiveWork->Capture(Owner, false)) { return false; }
        Rows.Add({Property->GetFName(), Property, Owner});
    }
    return Current(Object);
}
FProperty* ReacquireProperty(UObject* Object, const FPropertySnapshot& Row)
{
    if (!Current(Object) || !ActiveWork->Current(Row.Owner, false)) { return nullptr; }
    FProperty* Property = Object->GetClass()->FindPropertyByName(Row.Name);
    // Only dereference the descriptor after the current class lookup returns the expected pointer.
    if (!Property || Property != Row.Expected || Property->GetOwnerStruct() != Row.Owner || Property->HasAnyPropertyFlags(CPF_Transient))
    { ActiveWork->Failed = true; Fail(ActiveWork->Result, TEXT("property_identity_changed"), TEXT("Property/owner identity changed in callback; stale descriptor is not reused")); return nullptr; }
    return Property;
}

int32 ReplaceExportedPropertyPaths(UObject* Object, const TArray<FPathStringReplacement>& Replacements, const FString& SourceRoot)
{
    if (!Object || Replacements.IsEmpty())
    {
        return 0;
    }

    int32 ReplaceCount = 0;
    bool bObjectModified = false;
    TArray<FPropertySnapshot> Properties;
    if (!SnapshotProperties(Object, Properties)) { return ReplaceCount; }
    for (const FPropertySnapshot& Row : Properties)
    {
        FProperty* Property = ReacquireProperty(Object, Row);
        if (!Property) { return ReplaceCount; }
        FString ExportedValue;
        Property->ExportText_InContainer(0, ExportedValue, Object, Object, Object, PPF_SerializedAsImportText);
        Property = ReacquireProperty(Object, Row); if (!Property) { return ReplaceCount; }
        if (!ExportedValue.Contains(SourceRoot, ESearchCase::CaseSensitive))
        {
            continue;
        }

        if (ExportedValue.Len() > MaxExportChars) { ActiveWork->Failed = true; Fail(ActiveWork->Result, TEXT("export_bound"), TEXT("Property export exceeded finite bound; scope is incomplete")); return ReplaceCount; }
        const FString ReplacedValue = ApplyPathReplacements(ExportedValue, Replacements);
        if (ReplacedValue == ExportedValue)
        {
            continue;
        }

        if (!bObjectModified)
        {
            if (!Prepare(Object)) { return ReplaceCount; }
            bObjectModified = true;
        }

        Property = ReacquireProperty(Object, Row); if (!Property) { return ReplaceCount; }
        FString CurrentValue; Property->ExportText_InContainer(0, CurrentValue, Object, Object, Object, PPF_SerializedAsImportText);
        Property = ReacquireProperty(Object, Row); if (!Property) { return ReplaceCount; }
        if (CurrentValue != ExportedValue)
        { ActiveWork->Failed = true; Fail(ActiveWork->Result, TEXT("property_value_changed"), TEXT("Property value changed in Modify callback; no stale import dispatched")); return ReplaceCount; }
        void* ValueAddress = Property->ContainerPtrToValuePtr<void>(Object);
        ActiveWork->Result.bMutationAttempted = true; Bump(ActiveWork->Result, TEXT("property_import_calls"));
        const TCHAR* ImportEnd = Property->ImportText_Direct(
            *ReplacedValue,
            ValueAddress,
            Object,
            PPF_SerializedAsImportText,
            GWarn);

        Property = ReacquireProperty(Object, Row); if (!Property) { return ReplaceCount; }
        if (ImportEnd)
        {
            ++ReplaceCount; Bump(ActiveWork->Result, ActiveWork->PropertyCounter);
            FString AfterValue; Property->ExportText_InContainer(0, AfterValue, Object, Object, Object, PPF_SerializedAsImportText);
            Property = ReacquireProperty(Object, Row); if (!Property) { return ReplaceCount; }
            if (AfterValue.Len() > MaxExportChars)
            { ActiveWork->Failed = true; Fail(ActiveWork->Result, TEXT("property_readback_incomplete"), TEXT("Property readback failed/bounded; actual import prefix retained")); return ReplaceCount; }
            if (AfterValue != ExportedValue) { Changed(); }
        }
        else
        {
            ActiveWork->Failed = true; Fail(ActiveWork->Result, TEXT("property_import_failed"), TEXT("Property import failed or target identity changed; a partial import may remain")); return ReplaceCount;
        }
    }

    if (ReplaceCount > 0)
    {
        if (!MarkCapturedDirty(Object, true)) { return ReplaceCount; }
    }
    return ReplaceCount;
}

int32 ReplaceExportedPropertyTextValues(UObject* Object, const TArray<FPathStringReplacement>& Replacements)
{
    if (!Object || Replacements.IsEmpty())
    {
        return 0;
    }

    int32 ReplaceCount = 0;
    bool bObjectModified = false;
    TArray<FPropertySnapshot> Properties;
    if (!SnapshotProperties(Object, Properties)) { return ReplaceCount; }
    for (const FPropertySnapshot& Row : Properties)
    {
        FProperty* Property = ReacquireProperty(Object, Row);
        if (!Property) { return ReplaceCount; }
        FString ExportedValue;
        Property->ExportText_InContainer(0, ExportedValue, Object, Object, Object, PPF_SerializedAsImportText);
        Property = ReacquireProperty(Object, Row); if (!Property) { return ReplaceCount; }
        if (!ContainsReplacementSource(ExportedValue, Replacements))
        {
            continue;
        }

        if (ExportedValue.Len() > MaxExportChars) { ActiveWork->Failed = true; Fail(ActiveWork->Result, TEXT("export_bound"), TEXT("Property export exceeded finite bound; scope is incomplete")); return ReplaceCount; }
        const FString ReplacedValue = ApplyPathReplacements(ExportedValue, Replacements);
        if (ReplacedValue == ExportedValue)
        {
            continue;
        }

        if (!bObjectModified)
        {
            if (!Prepare(Object)) { return ReplaceCount; }
            bObjectModified = true;
        }

        Property = ReacquireProperty(Object, Row); if (!Property) { return ReplaceCount; }
        FString CurrentValue; Property->ExportText_InContainer(0, CurrentValue, Object, Object, Object, PPF_SerializedAsImportText);
        Property = ReacquireProperty(Object, Row); if (!Property) { return ReplaceCount; }
        if (CurrentValue != ExportedValue)
        { ActiveWork->Failed = true; Fail(ActiveWork->Result, TEXT("property_value_changed"), TEXT("Property value changed in Modify callback; no stale import dispatched")); return ReplaceCount; }
        void* ValueAddress = Property->ContainerPtrToValuePtr<void>(Object);
        ActiveWork->Result.bMutationAttempted = true; Bump(ActiveWork->Result, TEXT("property_import_calls"));
        const TCHAR* ImportEnd = Property->ImportText_Direct(
            *ReplacedValue,
            ValueAddress,
            Object,
            PPF_SerializedAsImportText,
            GWarn);

        Property = ReacquireProperty(Object, Row); if (!Property) { return ReplaceCount; }
        if (ImportEnd)
        {
            ++ReplaceCount; Bump(ActiveWork->Result, ActiveWork->PropertyCounter);
            FString AfterValue; Property->ExportText_InContainer(0, AfterValue, Object, Object, Object, PPF_SerializedAsImportText);
            Property = ReacquireProperty(Object, Row); if (!Property) { return ReplaceCount; }
            if (AfterValue.Len() > MaxExportChars)
            { ActiveWork->Failed = true; Fail(ActiveWork->Result, TEXT("property_readback_incomplete"), TEXT("Property readback failed/bounded; actual import prefix retained")); return ReplaceCount; }
            if (AfterValue != ExportedValue) { Changed(); }
        }
        else
        {
            ActiveWork->Failed = true; Fail(ActiveWork->Result, TEXT("property_import_failed"), TEXT("Property import failed or target identity changed; a partial import may remain")); return ReplaceCount;
        }
    }

    if (ReplaceCount > 0)
    {
        if (!MarkCapturedDirty(Object, true)) { return ReplaceCount; }
    }

    return ReplaceCount;
}
void RefreshMaterialReferencesAndCaches(
    const TSet<UPackage*>& TargetPackages,
    const TMap<UObject*, UObject*>& ReplacementMap,
    int32& OutTextureReferenceRepairCount,
    int32& OutMaterialCacheRefreshCount)
{
    if (TargetPackages.IsEmpty())
    {
        return;
    }

    TSet<UMaterial*> Materials;
    TSet<UMaterialFunction*> MaterialFunctions;
    TSet<UMaterialInstance*> MaterialInstances;
    TSet<UPackage*> DirtyPackages;

    for (UPackage* TargetPackage : TargetPackages)
    {
        if (!TargetPackage)
        {
            continue;
        }

        TArray<UObject*> PackageObjects;
        GetObjectsWithPackage(TargetPackage, PackageObjects, EGetObjectsFlags::IncludeNestedObjects, RF_Transient, EInternalObjectFlags::Garbage);
        if (PackageObjects.Num() > MaxObjects)
        { ActiveWork->Failed = true; Fail(ActiveWork->Result, TEXT("material_scope_bound"), TEXT("Material package inventory exceeds finite scope")); return; }
        for (UObject* PackageObject : PackageObjects)
        {
            if (!IsLiveObjectForMCP(PackageObject))
            {
                continue;
            }

            if (!Current(PackageObject)) { return; }
            if (UMaterialExpressionTextureBase* TextureExpression = Cast<UMaterialExpressionTextureBase>(PackageObject))
            {
                if (UTexture* ReplacementTexture = Cast<UTexture>(FindReplacementObject(ReplacementMap, TextureExpression->Texture)))
                {
                    if (ReplacementTexture != TextureExpression->Texture)
                    {
                        if (!Prepare(TextureExpression)) { return; }
                        TextureExpression->Texture = ReplacementTexture; Changed();
                        ActiveWork->Result.bMutationAttempted = true;
                        Bump(ActiveWork->Result, TEXT("auto_set_sample_type_calls")); TextureExpression->AutoSetSampleType();
                        if (!Current(TextureExpression)) { return; }
                        if (!MarkCapturedDirty(TextureExpression)) { return; }
                        DirtyPackages.Add(TextureExpression->GetOutermost());
                        ++OutTextureReferenceRepairCount;
                    }
                }
                else if (TextureExpression->Texture)
                {
                    if (!Prepare(TextureExpression)) { return; }
                    const auto BeforeSampler = TextureExpression->SamplerType;
                    ActiveWork->Result.bMutationAttempted = true; Bump(ActiveWork->Result, TEXT("auto_set_sample_type_calls")); TextureExpression->AutoSetSampleType();
                    if (!Current(TextureExpression)) { return; }
                    if (BeforeSampler != TextureExpression->SamplerType) { Changed(); }
                }
            }

            if (UMaterial* Material = Cast<UMaterial>(PackageObject))
            {
                Materials.Add(Material);
            }
            else if (UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(PackageObject))
            {
                MaterialFunctions.Add(MaterialFunction);
            }
            else if (UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(PackageObject))
            {
                MaterialInstances.Add(MaterialInstance);
            }
        }
    }

    for (UMaterialInstance* MaterialInstance : MaterialInstances)
    {
        if (!MaterialInstance)
        {
            continue;
        }

        if (!Current(MaterialInstance)) { return; }
        bool bChanged = false;
        if (MaterialInstance->TextureParameterValues.Num() > MaxRows)
        { ActiveWork->Failed = true; Fail(ActiveWork->Result, TEXT("material_scope_bound"), TEXT("Texture parameter inventory exceeds finite scope")); return; }
        const TArray<FTextureParameterValue> BeforeParameters = MaterialInstance->TextureParameterValues;
        for (int32 ParameterIndex = 0; ParameterIndex < BeforeParameters.Num(); ++ParameterIndex)
        {
            if (UTexture* ReplacementTexture = Cast<UTexture>(FindReplacementObject(ReplacementMap, BeforeParameters[ParameterIndex].ParameterValue)))
            {
                if (ReplacementTexture != BeforeParameters[ParameterIndex].ParameterValue)
                {
                    if (!bChanged)
                    {
                        if (!Prepare(MaterialInstance)) { return; }
                    }
                    if (MaterialInstance->TextureParameterValues.Num() != BeforeParameters.Num())
                    { ActiveWork->Failed = true; Fail(ActiveWork->Result, TEXT("material_parameters_changed"), TEXT("Texture parameter array shape changed in Modify callback")); return; }
                    for (int32 CheckIndex = ParameterIndex; CheckIndex < BeforeParameters.Num(); ++CheckIndex)
                    {
                        const auto& CurrentParameter = MaterialInstance->TextureParameterValues[CheckIndex];
                        const auto& ExpectedParameter = BeforeParameters[CheckIndex];
                        if (CurrentParameter.ParameterInfo != ExpectedParameter.ParameterInfo
                            || CurrentParameter.ParameterValue != ExpectedParameter.ParameterValue
                            || CurrentParameter.ExpressionGUID != ExpectedParameter.ExpressionGUID)
                        { ActiveWork->Failed = true; Fail(ActiveWork->Result, TEXT("material_parameters_changed"), TEXT("Texture parameter identity/value changed in callback; previous writes retained")); return; }
                    }
                    MaterialInstance->TextureParameterValues[ParameterIndex].ParameterValue = ReplacementTexture; Changed();
                    bChanged = true;
                    ++OutTextureReferenceRepairCount;
                }
            }
        }

        if (bChanged)
        {
            if (!MarkCapturedDirty(MaterialInstance)) { return; }
            DirtyPackages.Add(MaterialInstance->GetOutermost());
        }
    }

    FMaterialUpdateContext MaterialUpdateContext(FMaterialUpdateContext::EOptions::Default);

    for (UMaterialFunction* MaterialFunction : MaterialFunctions)
    {
        if (!MaterialFunction)
        {
            continue;
        }

        if (!Prepare(MaterialFunction)) { return; }
        Bump(ActiveWork->Result, TEXT("material_function_update_calls")); MaterialFunction->UpdateFromFunctionResource();
        if (!Current(MaterialFunction)) { return; }
        Bump(ActiveWork->Result, TEXT("material_function_update_calls")); MaterialFunction->UpdateInputOutputTypes();
        if (!Current(MaterialFunction)) { return; }
        Bump(ActiveWork->Result, TEXT("material_function_update_calls")); MaterialFunction->UpdateDependentFunctionCandidates();
        if (!Current(MaterialFunction)) { return; }
        ActiveWork->Result.bCompileRequested = true; ActiveWork->Result.bCompileAttempted = true;
        Bump(ActiveWork->Result, TEXT("material_force_recompile_calls")); MaterialFunction->ForceRecompileForRendering(MaterialUpdateContext, nullptr);
        if (!Current(MaterialFunction)) { return; }
        if (!MarkCapturedDirty(MaterialFunction)) { return; }
        DirtyPackages.Add(MaterialFunction->GetOutermost());
        ++OutMaterialCacheRefreshCount;
    }

    for (UMaterial* Material : Materials)
    {
        if (!Material)
        {
            continue;
        }

        if (!Prepare(Material)) { return; }
        Bump(ActiveWork->Result, TEXT("material_update_cached_calls")); Material->UpdateCachedExpressionData();
        if (!Current(Material)) { return; }
        Bump(ActiveWork->Result, TEXT("material_parameter_list_calls")); Material->BuildEditorParameterList();
        if (!Current(Material)) { return; }
        ActiveWork->Result.bCompileRequested = true; ActiveWork->Result.bCompileAttempted = true;
        Bump(ActiveWork->Result, TEXT("material_force_recompile_calls")); Material->ForceRecompileForRendering(EMaterialShaderPrecompileMode::None);
        if (!Current(Material)) { return; }
        if (!MarkCapturedDirty(Material)) { return; }
        DirtyPackages.Add(Material->GetOutermost());
        ++OutMaterialCacheRefreshCount;
    }

    for (UMaterialInstance* MaterialInstance : MaterialInstances)
    {
        if (!MaterialInstance)
        {
            continue;
        }

        if (!Prepare(MaterialInstance)) { return; }
        ActiveWork->Result.bCompileRequested = true; ActiveWork->Result.bCompileAttempted = true;
        Bump(ActiveWork->Result, TEXT("material_force_recompile_calls")); MaterialInstance->ForceRecompileForRendering(EMaterialShaderPrecompileMode::None);
        if (!Current(MaterialInstance)) { return; }
        if (!MarkCapturedDirty(MaterialInstance)) { return; }
        DirtyPackages.Add(MaterialInstance->GetOutermost());
        ++OutMaterialCacheRefreshCount;
    }

    for (UPackage* DirtyPackage : DirtyPackages)
    {
        if (DirtyPackage)
        {
            if (!MarkCapturedDirty(DirtyPackage)) { return; }
        }
    }
}

bool PinMayChange(UEdGraphPin* Pin, const TMap<UObject*, UObject*>& Map, const TArray<FPathStringReplacement>& Paths)
{
    if (!Pin) { return false; }
    auto Type = Pin->PinType;
    return RemapPinType(Type, Map) || FindReplacementObject(Map, Pin->DefaultObject)
        || ApplyPathReplacements(Pin->DefaultValue, Paths) != Pin->DefaultValue
        || ApplyPathReplacements(Pin->AutogeneratedDefaultValue, Paths) != Pin->AutogeneratedDefaultValue;
}
bool VariableMayChange(const FBPVariableDescription& Variable, const TMap<UObject*, UObject*>& Map, const TArray<FPathStringReplacement>& Paths)
{ auto Type = Variable.VarType; return RemapPinType(Type, Map) || ApplyPathReplacements(Variable.DefaultValue, Paths) != Variable.DefaultValue; }
bool ReferenceMayChange(const FMemberReference& Source, UBlueprint* Blueprint, const TMap<UObject*, UObject*>& Map)
{ auto Ref = Source; return RemapMemberReferenceParent(Ref, Map) || RemapSelfMemberReference(Ref, Blueprint); }
bool NodeMayChange(UEdGraphNode* Node, UBlueprint* Blueprint, const TMap<UObject*, UObject*>& Map, const TArray<FPathStringReplacement>& Paths)
{
    if (auto* N = Cast<UK2Node_CallFunction>(Node))
    {
        if (!N->FunctionReference.IsLocalScope() && ((N->FunctionReference.IsSelfContext() && FindBlueprintFunctionByName(Blueprint, N->FunctionReference.GetMemberName()))
            || FindReplacementObject(Map, N->FunctionReference.GetMemberParentClass()))) { return true; }
        if (auto* M = Cast<UK2Node_CallFunctionOnMember>(Node))
        { if (ReferenceMayChange(M->MemberVariableToCallOn, Blueprint, Map)) { return true; } }
    }
    if (auto* N = Cast<UK2Node_Variable>(Node))
    {
        if (!N->VariableReference.IsLocalScope() && ((N->VariableReference.IsSelfContext() && (FindBlueprintPropertyByName(Blueprint, N->VariableReference.GetMemberName())
            || ReferenceMayChange(N->VariableReference, Blueprint, Map))) || FindReplacementObject(Map, N->VariableReference.GetMemberParentClass()))) { return true; }
    }
    if (auto* N = Cast<UK2Node_Event>(Node)) { if (ReferenceMayChange(N->EventReference, Blueprint, Map)) { return true; } }
    if (auto* N = Cast<UK2Node_BaseMCDelegate>(Node))
    {
        if (!N->DelegateReference.IsLocalScope() && ((N->DelegateReference.IsSelfContext() && (FindBlueprintPropertyByName(Blueprint, N->DelegateReference.GetMemberName())
            || ReferenceMayChange(N->DelegateReference, Blueprint, Map))) || FindReplacementObject(Map, N->DelegateReference.GetMemberParentClass()))) { return true; }
    }
    if (auto* N = Cast<UK2Node_FunctionTerminator>(Node)) { if (ReferenceMayChange(N->FunctionReference, Blueprint, Map)) { return true; } }
    if (auto* N = Cast<UK2Node_EditablePinBase>(Node))
    {
        for (const auto& Pin : N->UserDefinedPins)
        { if (Pin.IsValid()) { auto Type = Pin->PinType; if (RemapPinType(Type, Map) || ApplyPathReplacements(Pin->PinDefaultValue, Paths) != Pin->PinDefaultValue) { return true; } } }
    }
    if (auto* N = Cast<UK2Node_FunctionEntry>(Node))
    { for (const auto& Variable : N->LocalVariables) { if (VariableMayChange(Variable, Map, Paths)) { return true; } } }
    if (auto* N = Cast<UK2Node_StructOperation>(Node)) { if (FindReplacementObject(Map, N->StructType)) { return true; } }
    if (auto* N = Cast<UK2Node_DynamicCast>(Node)) { if (FindReplacementObject(Map, N->TargetType)) { return true; } }
    for (UEdGraphPin* Pin : Node->Pins) { if (PinMayChange(Pin, Map, Paths)) { return true; } }
    return false;
}
bool PrepareBlueprintScope(UBlueprint* Blueprint, const TMap<UObject*, UObject*>& Map, const TArray<FPathStringReplacement>& Paths)
{
    if (!Current(Blueprint)) { return false; }
    TArray<UEdGraph*> Graphs; Blueprint->GetAllGraphs(Graphs);
    TArray<TStrongObjectPtr<UEdGraphNode>> Planned;
    bool Changes = false; int32 Nodes = 0, Pins = 0;
    if (Blueprint->NewVariables.Num() + Blueprint->GeneratedVariables.Num() > MaxRows)
    { ActiveWork->Failed = true; return Fail(ActiveWork->Result, TEXT("blueprint_scope_bound"), TEXT("Blueprint variable inventory exceeds finite scope")); }
    for (const auto& V : Blueprint->NewVariables) { Changes |= VariableMayChange(V, Map, Paths); }
    for (const auto& V : Blueprint->GeneratedVariables) { Changes |= VariableMayChange(V, Map, Paths); }
    for (UEdGraph* Graph : Graphs)
    {
        if (!Graph) { continue; } if (!Current(Graph)) { return false; }
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node) { continue; } if (!Current(Node)) { return false; }
            if (++Nodes > MaxNodes || (Pins += Node->Pins.Num()) > MaxPins)
            { ActiveWork->Failed = true; return Fail(ActiveWork->Result, TEXT("blueprint_scope_bound"), TEXT("Blueprint node/pin inventory exceeds finite scope")); }
            if (NodeMayChange(Node, Blueprint, Map, Paths)) { Changes = true; Planned.Emplace(Node); }
        }
    }
    if (!Changes) { return true; }
    if (!Prepare(Blueprint)) { return false; }
    for (const auto& Node : Planned) { if (!Prepare(Node.Get())) { return false; } }
    return true;
}
bool Disk(UObject* Asset, TSharedRef<FJsonObject> Snapshot, FString& Error)
{
    if (!IsLiveObjectForMCP(Asset)) { Error = TEXT("Asset unavailable for disk proof"); return false; }
    UPackage* Package = Asset->GetOutermost(); FString Filename;
    const bool Map = Cast<UWorld>(Asset) || UWorld::FindWorldInPackage(Package);
    if (Package == GetTransientPackage() || !FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), Filename,
        Map ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension()))
    { Error = TEXT("Package has no valid physical asset filename"); return false; }
    Filename = FPaths::ConvertRelativePathToFull(Filename); const int64 Size = IFileManager::Get().FileSize(*Filename);
    Snapshot->SetStringField(TEXT("filename"), Filename); Snapshot->SetBoolField(TEXT("exists"), Size >= 0); Snapshot->SetNumberField(TEXT("bytes"), double(Size));
    if (Size > MaxProofBytes) { Error = TEXT("Package disk proof exceeds finite byte bound"); return false; }
    if (Size >= 0)
    {
        TArray<uint8> Bytes; if (!FFileHelper::LoadFileToArray(Bytes, *Filename) || Bytes.Num() != Size)
        { Error = TEXT("Independent package byte read failed"); return false; }
        Snapshot->SetStringField(TEXT("sha1"), FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num()).ToString());
    }
    return true;
}
bool Save(FWork& Work, UObject* Asset)
{
    FResult& R = Work.Result; R.Stage = TEXT("save_assets");
    auto Row = MakeShared<FJsonObject>(), Before = MakeShared<FJsonObject>(), After = MakeShared<FJsonObject>(); FString Error;
    const auto* Captured = Work.Identities.Find(Asset);
    Row->SetStringField(TEXT("asset_path"), Captured ? (*Captured)->Path : TEXT("<uncaptured>")); Row->SetBoolField(TEXT("attempted"), false); Row->SetBoolField(TEXT("saved"), false);
    auto Rows = R.Data->GetArrayField(TEXT("save_calls")); Rows.Add(MakeShared<FJsonValueObject>(Row)); R.Data->SetArrayField(TEXT("save_calls"), Rows);
    if (!Work.Current(Asset)) { return false; }
    Row->SetObjectField(TEXT("before"), Before);
    if (!Disk(Asset, Before, Error)) { Work.Failed = true; return Fail(R, TEXT("save_preflight_failed"), Error); }
    UPackage* Package = Asset->GetOutermost(); if (!Work.Current(Package)) { return false; }
    UObject* Top = Asset; if (auto* World = UWorld::FindWorldInPackage(Package)) { Top = World; }
    if (!Work.Current(Top)) { return false; }
    const FString Filename = Before->GetStringField(TEXT("filename"));
    FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError; Args.bSlowTask = false;
    Row->SetObjectField(TEXT("before"), Before); Row->SetBoolField(TEXT("attempted"), true); R.bSaveAttempted = true;
    Bump(R, TEXT("save_attempt_count")); const bool Returned = UPackage::SavePackage(Package, Top, *Filename, Args);
    Row->SetBoolField(TEXT("return_value"), Returned); Bump(R, TEXT("save_return_count"));
    if (!Work.Current(Asset) || !Work.Current(Package)) { return false; }
    if (!Disk(Asset, After, Error)) { Work.Failed = true; return Fail(R, TEXT("save_readback_failed"), Error); }
    Row->SetObjectField(TEXT("after"), After);
    FString BeforeHash, AfterHash; Before->TryGetStringField(TEXT("sha1"), BeforeHash); After->TryGetStringField(TEXT("sha1"), AfterHash);
    if (Before->GetBoolField(TEXT("exists")) != After->GetBoolField(TEXT("exists")) || BeforeHash != AfterHash) { R.bChanged = true; }
    if (!Returned || !After->GetBoolField(TEXT("exists")) || After->GetNumberField(TEXT("bytes")) <= 0 || Package->IsDirty())
    { Work.Failed = true; return Fail(R, TEXT("save_unconfirmed"), TEXT("Package save return/file/dirty proof failed; previous changes and save prefix retained")); }
    Row->SetBoolField(TEXT("saved"), true); Bump(R, TEXT("saved_count")); return true;
}
}

FResult Replace(const FString& ReplacementsJson, const FString& TargetRootInput, const FString& TargetPathsJson,
    const FString& SourceRootInput, const FString& StringReplacementsJson, const FString& NameReplacementsJson,
    bool bDryRun, bool bSaveAssets, bool bScanPaths, bool bRefreshMaterials, bool bIncludePackageObjects, bool bAllowNonGameTargets)
{
    FResult R; R.bSaveRequested = bSaveAssets; R.Data->SetStringField(TEXT("operation"), TEXT("replace_asset_references_mcp"));
    for (const TCHAR* Key : { TEXT("replacement_pair_count"), TEXT("target_asset_count"), TEXT("target_package_count"), TEXT("package_object_count"),
        TEXT("remapped_object_count"), TEXT("remapped_reference_count"), TEXT("archive_object_remap_skip_count"), TEXT("path_property_remap_count"),
        TEXT("text_property_remap_count"), TEXT("blueprint_graph_reference_remap_count"), TEXT("material_texture_reference_repair_count"),
        TEXT("material_cache_refresh_count"), TEXT("saved_count"), TEXT("asset_load_calls"), TEXT("asset_load_returns"), TEXT("load_object_fallback_calls"),
        TEXT("scan_calls"), TEXT("registry_enumeration_calls"), TEXT("explicit_modify_calls"), TEXT("archive_remap_calls"), TEXT("property_import_calls"),
        TEXT("node_reconstruct_calls"), TEXT("explicit_mark_dirty_calls"), TEXT("mark_blueprint_structural_calls"), TEXT("auto_set_sample_type_calls"), TEXT("material_function_update_calls"),
        TEXT("material_force_recompile_calls"), TEXT("material_update_cached_calls"), TEXT("material_parameter_list_calls"), TEXT("save_attempt_count"), TEXT("save_return_count") })
    { R.Data->SetNumberField(Key, 0); }
    R.Data->SetArrayField(TEXT("save_calls"), {});
    R.Data->SetBoolField(TEXT("dry_run"), bDryRun); R.Data->SetBoolField(TEXT("save_assets"), bSaveAssets);
    R.Data->SetBoolField(TEXT("refresh_materials"), bRefreshMaterials); R.Data->SetBoolField(TEXT("include_package_objects"), bIncludePackageObjects);
    R.Data->SetBoolField(TEXT("allow_non_game_targets"), bAllowNonGameTargets); R.Data->SetBoolField(TEXT("atomic_rollback"), false);
    R.Data->SetBoolField(TEXT("automatic_retry"), false); R.Data->SetBoolField(TEXT("dry_run_predicts_write_counts"), false);
    R.Data->SetBoolField(TEXT("all_implicit_sdk_effects_observed"), false);
    R.Data->SetBoolField(TEXT("observed_explicit_change"), false);
    R.Data->SetBoolField(TEXT("runtime_dependency_packaging_verified"), false);
    R.Data->SetStringField(TEXT("runtime_dependency_guard_scope"), TEXT("Legacy text/path tokens, installed MCP/integration plugin module namespaces, loaded mapped-destination identity/class/Blueprint parent chains before explicit writes. Unrelated custom serialized or transitive dirty RAM references are not a full packaging proof.")); R.Data->SetBoolField(TEXT("disk_reload_verified"), false);
    R.Data->SetStringField(TEXT("compile_observation"), TEXT("No explicit Blueprint compile or blocking shader wait; material refresh may schedule Engine-owned compile/fanout."));
    R.Data->SetStringField(TEXT("change_observation_scope"), TEXT("Explicit assignments/counts and captured target identity/dirty/package bytes. Loads, Modify, reconstruction, import, structural refresh, Material and Save callbacks may change unobserved SDK state."));
    if (!IsEditorGameThread() || IsPlaySessionActive() || IsEngineExitRequested() || WorkBusy)
    { Fail(R, TEXT("editor_state_invalid"), TEXT("Requires an idle Editor game thread outside PIE/SIE; no request work dispatched")); return R; }
    TGuardValue<bool> Busy(WorkBusy, true); FWork Work(R); TGuardValue<FWork*> Context(ActiveWork, &Work); FString Error;
    FString TargetRoot = NormalizeContentRoot(TargetRootInput), SourceRoot = NormalizeContentRoot(SourceRootInput);
    R.Data->SetStringField(TEXT("target_root"), TargetRoot); R.Data->SetStringField(TEXT("source_root"), SourceRoot);
    if (!NoNul(TargetRoot) || !NoNul(SourceRoot) || TargetRoot.Len() >= NAME_SIZE || SourceRoot.Len() >= NAME_SIZE
        || (!TargetRoot.IsEmpty() && (!FPackageName::IsValidLongPackageName(TargetRoot) || !TargetRoot.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase)))
        || (!SourceRoot.IsEmpty() && !FPackageName::IsValidLongPackageName(SourceRoot)) || MCPOnly(SourceRoot))
    { Fail(R, TEXT("scope_invalid"), TEXT("Invalid target_root/source_root or forbidden MCP-only source root")); return R; }
    TArray<FString> TargetPaths;
    TSharedPtr<FJsonObject> Replacements, StringMap, NameMap;
    if (!TargetInput(TargetPathsJson, TargetPaths, Error) || !MapInput(ReplacementsJson, Replacements, Error)
        || !MapInput(StringReplacementsJson, StringMap, Error) || !MapInput(NameReplacementsJson, NameMap, Error))
    { Fail(R, TEXT("input_invalid"), Error); return R; }
    for (const auto& Path : TargetPaths)
    { if (!bAllowNonGameTargets && !Path.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase)) { Fail(R, TEXT("target_mount_refused"), TEXT("Explicit non-/Game target requires allow_non_game_targets=true")); return R; } }
    R.Stage = TEXT("scan_targets"); R.bChangeStateKnown = false;
    auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    if (bScanPaths)
    {
        TArray<FString> Scan; if (!TargetRoot.IsEmpty()) { Scan.Add(TargetRoot); }
        for (const auto& Path : TargetPaths) { FString Dir = FPackageName::GetLongPackagePath(Path); if (!Dir.IsEmpty()) { Scan.AddUnique(Dir); } }
        if (!Scan.IsEmpty()) { Bump(R, TEXT("scan_calls")); Registry.ScanPathsSynchronous(Scan, true); }
    }
    if (!TargetRoot.IsEmpty())
    {
        FARFilter Filter; Filter.PackagePaths.Add(FName(*TargetRoot)); Filter.bRecursivePaths = true;
        bool Bound = false; Bump(R, TEXT("registry_enumeration_calls"));
        Registry.EnumerateAssets(Filter, [&](const FAssetData& Data)
        { if (TargetPaths.Num() >= MaxRows && !TargetPaths.Contains(Data.PackageName.ToString())) { Bound = true; return false; } TargetPaths.AddUnique(Data.PackageName.ToString()); return true; });
        if (Bound) { Fail(R, TEXT("target_inventory_bound"), TEXT("Target root inventory exceeds finite bound; no subset declared complete")); return R; }
    }
    if (TargetPaths.IsEmpty()) { Fail(R, TEXT("targets_missing"), TEXT("Provide target_paths or a target_root containing assets")); return R; }
    TMap<UObject*, UObject*> ReplacementMap;
    TArray<FPathStringReplacement> Paths, Texts; TArray<FString> FailedReplacements, Samples, TextSamples, SourceRoots, FailedTargets, ChangedSamples;
    if (!SourceRoot.IsEmpty()) { SourceRoots.Add(SourceRoot); }
    auto Publish = [&]()
    {
        R.Data->SetBoolField(TEXT("observed_explicit_change"), R.bChanged);
        R.Data->SetBoolField(TEXT("total_change_state_known"), R.bChangeStateKnown);
        Strings(R.Data, TEXT("failed_replacements"), FailedReplacements); Strings(R.Data, TEXT("failed_target_paths"), FailedTargets);
        Strings(R.Data, TEXT("replacement_samples"), Samples); Strings(R.Data, TEXT("text_replacement_samples"), TextSamples);
        Strings(R.Data, TEXT("remapped_object_samples"), ChangedSamples); Strings(R.Data, TEXT("source_roots_for_path_remap"), SourceRoots);
    };
    auto ParseTexts = [&](const TSharedPtr<FJsonObject>& Map, const TCHAR* Field)
    {
        for (const auto& Pair : Map->Values)
        {
            const FString Key(Pair.Key.ToView());
            if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::String)
            { FailedReplacements.Add(FString::Printf(TEXT("%s.%s -> <non-string target>"), Field, *Key)); continue; }
            const FString Value = Pair.Value->AsString();
            if (Key.IsEmpty() || Value.IsEmpty() || !NoNul(Key) || !NoNul(Value) || Key.Len() > 4096 || Value.Len() > 4096 || MCPOnly(Value))
            { FailedReplacements.Add(FString::Printf(TEXT("%s.%s -> %s (invalid/empty/MCP-only value)"), Field, *Key, *Value)); continue; }
            if (Key == Value) { continue; }
            Texts.Add({Key, Value}); if (TextSamples.Num() < 50) { TextSamples.Add(Key + TEXT(" -> ") + Value); }
        }
    };
    ParseTexts(StringMap, TEXT("string_replacements")); ParseTexts(NameMap, TEXT("name_replacements"));
    if (!FailedReplacements.IsEmpty())
    { Publish(); R.Stage = TEXT("validate_replacement_values");
      Fail(R, TEXT("replacement_values_invalid"), TEXT("Invalid or forbidden mapping values; see failed_replacements. No target load or write dispatched.")); return R; }
    if (Replacements->Values.IsEmpty() && Texts.IsEmpty())
    { Publish(); Fail(R, TEXT("replacements_missing"), TEXT("Provide replacements, string_replacements or name_replacements")); return R; }
    auto Load = [&](const FString& Path) -> UObject*
    {
        Bump(R, TEXT("asset_load_calls")); UObject* Object = UEditorAssetLibrary::LoadAsset(Path); Bump(R, TEXT("asset_load_returns"));
        if (!Object) { Bump(R, TEXT("load_object_fallback_calls")); const FString ObjectPath = Path + TEXT(".") + FPackageName::GetShortName(Path); Object = LoadObject<UObject>(nullptr, *ObjectPath); }
        if (!Work.EditorCurrent()) { return nullptr; }
        return Object;
    };
    R.Stage = TEXT("resolve_replacements");
    for (const auto& Pair : Replacements->Values)
    {
        const FString Key(Pair.Key.ToView());
        if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::String)
        { FailedReplacements.Add(Key + TEXT(" -> <non-string target>")); continue; }
        const FString RawTarget = Pair.Value->AsString();
        if (!NoNul(Key) || !NoNul(RawTarget) || Key.Len() > 4096 || RawTarget.Len() > 4096)
        { FailedReplacements.Add(Key + TEXT(" -> <invalid/overlong raw path>")); continue; }
        FString Source = NormalizePackagePathParam(Key), Target = NormalizePackagePathParam(RawTarget);
        if (!NoNul(Source) || !NoNul(Target) || Source.Len() >= NAME_SIZE || Target.Len() >= NAME_SIZE || MCPOnly(Source) || MCPOnly(Target)
            || !FPackageName::IsValidLongPackageName(Source) || !FPackageName::IsValidLongPackageName(Target))
        { FailedReplacements.Add(Source + TEXT(" -> ") + Target + TEXT(" (invalid package/MCP-only path)")); continue; }
        UObject* SourceObject = Load(Source), *TargetObject = Load(Target);
        if (Work.Failed) { Publish(); return R; }
        if (!IsLiveObjectForMCP(SourceObject) || !IsLiveObjectForMCP(TargetObject))
        { FailedReplacements.Add(Source + TEXT(" -> ") + Target + TEXT(" (failed to load source or target)")); continue; }
        if (!Work.Capture(SourceObject, false) || !Work.Capture(TargetObject, false)) { Publish(); return R; }
        if (!RuntimeDestinationAllowed(TargetObject, Error))
        { FailedReplacements.Add(Source + TEXT(" -> ") + Target + TEXT(" (") + Error + TEXT(")")); continue; }
        AddReplacementMapping(ReplacementMap, SourceObject, TargetObject); AddObjectPathReplacements(Paths, SourceObject, TargetObject);
        AddBlueprintReplacementMappings(ReplacementMap, SourceObject, TargetObject);
        if (Work.Failed) { Publish(); return R; }
        Bump(R, TEXT("replacement_pair_count"));
        SourceRoots.AddUnique(SourceObject->GetOutermost()->GetName());
        if (Samples.Num() < 50) { Samples.Add(SourceObject->GetPathName() + TEXT(" -> ") + TargetObject->GetPathName()); }
    }
    Publish();
    if (!FailedReplacements.IsEmpty()) { Fail(R, TEXT("replacement_resolution_failed"), TEXT("One or more mappings failed before explicit target writes; load/registry effects may remain")); return R; }
    if (ReplacementMap.IsEmpty() && Texts.IsEmpty()) { Fail(R, TEXT("no_valid_replacements"), TEXT("No valid replacements resolved")); return R; }
    for (const auto& Pair : ReplacementMap)
    { if (!Work.Capture(Pair.Key, false) || !Work.Capture(Pair.Value, false)) { return R; }
        if (!RuntimeDestinationAllowed(Pair.Value, Error)) { Fail(R, TEXT("runtime_dependency_refused"), Error); return R; }
        Work.ReferenceTargets.Add(Pair.Value); }
    Paths.Sort([](const auto& A, const auto& B) { return A.Source.Len() > B.Source.Len(); });
    R.Stage = TEXT("resolve_targets"); TArray<UObject*> Assets; TArray<UBlueprint*> Blueprints; TSet<UObject*> Objects;
    for (const auto& Raw : TargetPaths)
    {
        FString Path = NormalizePackagePathParam(Raw);
        if (MCPOnly(Path) || !FPackageName::IsValidLongPackageName(Path))
        { FailedTargets.Add(Path + TEXT(" (invalid/MCP-only target)")); continue; }
        UObject* Asset = Load(Path);
        if (Work.Failed) { Publish(); return R; }
        if (!IsLiveObjectForMCP(Asset)) { FailedTargets.Add(Path + TEXT(" (target load failed)")); continue; }
        if (!Work.Capture(Asset, true) || !Work.Capture(Asset->GetOutermost(), true)) { Publish(); return R; }
        Assets.AddUnique(Asset); Work.Packages.Add(Asset->GetOutermost()); Objects.Add(Asset);
        if (auto* BP = Cast<UBlueprint>(Asset)) { Blueprints.AddUnique(BP); }
    }
    R.Data->SetNumberField(TEXT("target_asset_count"), Assets.Num()); R.Data->SetNumberField(TEXT("target_package_count"), Work.Packages.Num()); Publish();
    if (!FailedTargets.IsEmpty()) { Fail(R, TEXT("target_resolution_failed"), TEXT("One or more targets failed before explicit writes; load prefix retained")); return R; }
    // Pin every loaded package object used by graph/material helpers before any writer.
    for (UPackage* Package : Work.Packages)
    {
        TArray<UObject*> PackageObjects; GetObjectsWithPackage(Package, PackageObjects, EGetObjectsFlags::IncludeNestedObjects, RF_Transient, EInternalObjectFlags::Garbage);
        if (PackageObjects.Num() > MaxObjects) { Fail(R, TEXT("package_object_bound"), TEXT("Package object inventory exceeds finite bound")); return R; }
        for (UObject* Object : PackageObjects)
        {
            if (!IsLiveObjectForMCP(Object)) { continue; }
            if (!Work.Capture(Object, true)) { return R; }
            if (bIncludePackageObjects) { Objects.Add(Object); if (auto* BP = Cast<UBlueprint>(Object)) { Blueprints.AddUnique(BP); } }
        }
    }
    R.Data->SetNumberField(TEXT("package_object_count"), Objects.Num()); Publish();
    if (bDryRun)
    {
        R.Stage = TEXT("dry_run_resolved"); R.Data->SetBoolField(TEXT("explicit_target_writes_skipped"), true);
        R.Data->SetBoolField(TEXT("save_skipped_due_to_dry_run"), bSaveAssets); R.bSuccess = true; R.bPostconditionPassed = true; return R;
    }
    R.Stage = TEXT("archive_object_references");
    for (UObject* Object : Objects)
    {
        if (ShouldSkipAssetArchiveObjectReferenceRemap(Object)) { Bump(R, TEXT("archive_object_remap_skip_count")); continue; }
        if (!Work.Prepare(Object)) { Publish(); return R; }
        Bump(R, TEXT("archive_remap_calls"));
        FGuardedReferenceArchive Archive(Work, Object, ReplacementMap);
        const int64 Count = Archive.GetCount();
        if (Count > 0) { Bump(R, TEXT("remapped_object_count")); Bump(R, TEXT("remapped_reference_count"), Count); R.bChanged = true;
            if (ChangedSamples.Num() < 50) { ChangedSamples.Add(Object->GetPathName()); } }
        if (!Work.Current(Object)) { Publish(); return R; }
        if (Count > 0 && !MarkCapturedDirty(Object, true)) { Publish(); return R; }
    }
    R.Stage = TEXT("blueprint_graph_references");
    for (UBlueprint* Blueprint : Blueprints)
    {
        RemapBlueprintGraphReferences(Blueprint, ReplacementMap, Paths);
        if (Work.Failed || !Work.Current(Blueprint)) { Publish(); return R; }
    }
    R.Stage = TEXT("exported_property_references");
    for (UObject* Object : Objects)
    {
        if (!Work.Current(Object)) { Publish(); return R; }
        Work.PropertyCounter = TEXT("path_property_remap_count");
        for (const auto& Source : SourceRoots)
        { ReplaceExportedPropertyPaths(Object, Paths, Source); if (Work.Failed) { Publish(); return R; } }
        Work.PropertyCounter = TEXT("text_property_remap_count"); ReplaceExportedPropertyTextValues(Object, Texts);
        if (Work.Failed) { Publish(); return R; }
    }
    if (bRefreshMaterials)
    {
        R.Stage = TEXT("material_refresh"); int32 TextureRepairs = 0, CacheRefreshes = 0;
        RefreshMaterialReferencesAndCaches(Work.Packages, ReplacementMap, TextureRepairs, CacheRefreshes);
        R.Data->SetNumberField(TEXT("material_texture_reference_repair_count"), TextureRepairs);
        R.Data->SetNumberField(TEXT("material_cache_refresh_count"), CacheRefreshes);
        if (Work.Failed) { Publish(); return R; }
    }
    if (bSaveAssets)
    {
        for (UObject* Asset : Assets) { if (!Save(Work, Asset)) { Publish(); return R; } }
        R.bSaved = R.Data->GetIntegerField(TEXT("saved_count")) == Assets.Num();
    }
    for (UObject* Asset : Assets) { if (!Work.Current(Asset)) { Publish(); return R; } }
    Publish(); R.Stage = TEXT("replace_completed"); R.bSuccess = true; R.bPostconditionPassed = true; return R;
}
}
