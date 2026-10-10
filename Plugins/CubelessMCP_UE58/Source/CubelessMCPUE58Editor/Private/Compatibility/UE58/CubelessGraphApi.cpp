#include "Compatibility/UE58/CubelessGraphApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessConnectionApi.h"
#include "Compatibility/UE58/CubelessComponentApi.h"
#include "Compatibility/UE58/CubelessGeneratedComponentApi.h"
#include "Compatibility/UE58/CubelessRuntimeObjectApi.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "K2Node.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Variable.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::Graph
{
namespace
{
TSharedRef<FJsonObject> Result()
{
    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), false);
    R->SetBoolField(TEXT("mutation_attempted"), false);
    R->SetBoolField(TEXT("saved"), false);
    R->SetBoolField(TEXT("compiled"), false);
    R->SetBoolField(TEXT("loaded_during_read"), false);
    R->SetStringField(TEXT("stage"), TEXT("preflight"));
    return R;
}
bool Fail(const TSharedRef<FJsonObject>& R, const TCHAR* Code, const TCHAR* Message)
{
    R->SetStringField(TEXT("error_code"), Code);
    R->SetStringField(TEXT("error"), Message);
    return false;
}
bool SafeSelector(const FString& Text)
{
    if (Text.IsEmpty() || Text.Len() >= NAME_SIZE) { return false; }
    for (const TCHAR C : Text) { if (C < 32 || C == 127) { return false; } }
    return true;
}
TSharedRef<FJsonObject> PinTypeObservation(const FEdGraphPinType& Type)
{
    auto Value = MakeShared<FJsonObject>();
    Value->SetStringField(TEXT("category"), Type.PinCategory.ToString());
    Value->SetStringField(TEXT("subcategory"), Type.PinSubCategory.ToString());
    Value->SetStringField(TEXT("type_object"), Type.PinSubCategoryObject.IsValid() ? Type.PinSubCategoryObject->GetPathName() : FString());
    Value->SetNumberField(TEXT("container_type"), static_cast<int32>(Type.ContainerType));
    Value->SetBoolField(TEXT("is_reference"), Type.bIsReference);
    Value->SetBoolField(TEXT("is_const"), Type.bIsConst);
    return Value;
}
UEdGraph* ResolveLoadedGraph(const FString& Path, const TSharedRef<FJsonObject>& R)
{
    if (!IsEditorGameThread() || IsPlaySessionActive())
    { Fail(R, TEXT("graph_editor_state_invalid"), TEXT("Requires the editor game thread outside PIE/SIE.")); return nullptr; }
    if (!SafeSelector(Path) || !Path.StartsWith(TEXT("/Game/")))
    { Fail(R, TEXT("graph_path_invalid"), TEXT("Use the exact loaded /Game graph object path.")); return nullptr; }
    UEdGraph* Graph = FindObject<UEdGraph>(nullptr, *Path);
    UBlueprint* Blueprint = Graph ? FBlueprintEditorUtils::FindBlueprintForGraph(Graph) : nullptr;
    if (!Graph || !Blueprint || !Graph->GetSchema() || !Graph->GetSchema()->IsA<UEdGraphSchema_K2>())
    { Fail(R, TEXT("graph_not_found"), TEXT("A loaded Blueprint K2 graph was not found; this helper never loads assets.")); return nullptr; }
    if (Graph->Nodes.Num() > 2048)
    { Fail(R, TEXT("graph_too_large"), TEXT("Native graph observation is limited to 2048 total nodes, including comments.")); return nullptr; }
    R->SetStringField(TEXT("graph_path"), Graph->GetPathName());
    R->SetStringField(TEXT("blueprint_path"), Blueprint->GetPathName());
    R->SetBoolField(TEXT("dirty_before"), Blueprint->GetOutermost()->IsDirty());
    R->SetNumberField(TEXT("total_node_count"), Graph->Nodes.Num());
    return Graph;
}
bool Complete(const TSharedRef<FJsonObject>& R, UEdGraph* Graph)
{
    UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
    if (!Blueprint) { return Fail(R, TEXT("graph_owner_changed"), TEXT("Graph owner is unavailable after observation.")); }
    const bool DirtyAfter = Blueprint->GetOutermost()->IsDirty();
    R->SetBoolField(TEXT("dirty_after"), DirtyAfter);
    if (DirtyAfter != R->GetBoolField(TEXT("dirty_before")))
    { return Fail(R, TEXT("graph_read_changed_dirty_state"), TEXT("Graph observation changed package dirty state; inspect the asset.")); }
    R->SetBoolField(TEXT("success"), true); R->SetStringField(TEXT("stage"), TEXT("complete")); return true;
}
}

TSharedRef<FJsonObject> ReadNodeIdentities(const FString& GraphPath)
{
    auto R = Result(); UEdGraph* Graph = ResolveLoadedGraph(GraphPath, R); if (!Graph) { return R; }
    TArray<TSharedPtr<FJsonValue>> Rows; TSet<FGuid> Seen;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!IsValid(Node) || !Node->IsA<UK2Node>()) { continue; }
        if (!Node->NodeGuid.IsValid() || Seen.Contains(Node->NodeGuid))
        { Fail(R, TEXT("graph_node_guid_invalid"), TEXT("A K2 node GUID is invalid or duplicated; no GUID was repaired.")); return R; }
        Seen.Add(Node->NodeGuid);
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("object_path"), Node->GetPathName());
        Row->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString(EGuidFormats::Digits));
        if (const UK2Node_Event* Event = Cast<UK2Node_Event>(Node))
        {
            Row->SetStringField(TEXT("event_name"), Event->EventReference.GetMemberName().ToString());
            const UClass* Parent = Event->EventReference.GetMemberParentClass();
            Row->SetStringField(TEXT("event_member_parent_path"), Parent ? Parent->GetPathName() : FString());
        }
        if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
        {
            const UFunction* Function = Call->GetTargetFunction();
            Row->SetStringField(TEXT("function_name"), Call->GetFunctionName().ToString());
            Row->SetStringField(TEXT("function_path"), Function ? Function->GetPathName() : FString());
            Row->SetStringField(TEXT("function_owner_path"), Function && Function->GetOwnerClass() ? Function->GetOwnerClass()->GetPathName() : FString());
        }
        if (const UK2Node_Variable* Variable = Cast<UK2Node_Variable>(Node))
        {
            Row->SetStringField(TEXT("variable_name"), Variable->GetVarName().ToString());
            Row->SetBoolField(TEXT("self_context"), Variable->VariableReference.IsSelfContext());
            const UClass* MemberParent = Variable->VariableReference.GetMemberParentClass();
            Row->SetStringField(TEXT("variable_member_parent_path"), MemberParent ? MemberParent->GetPathName() : FString());
            Row->SetStringField(TEXT("variable_member_guid"), Variable->VariableReference.GetMemberGuid().ToString(EGuidFormats::Digits));
            const FProperty* Property = Variable->GetPropertyForVariableFromSkeleton();
            Row->SetStringField(TEXT("variable_property_path"), Property ? Property->GetPathName() : FString());
        }
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    R->SetArrayField(TEXT("nodes"), Rows); R->SetStringField(TEXT("scope"), TEXT("loaded K2 nodes; public NodeGuid values, no mutation or GUID repair"));
    Complete(R, Graph); return R;
}

TSharedRef<FJsonObject> ReadMemberVariablePlan(const FString& GraphPath, const FString& VariableName, bool bSetter)
{
    auto R = Result(); UEdGraph* Graph = ResolveLoadedGraph(GraphPath, R); if (!Graph) { return R; }
    if (!SafeSelector(VariableName))
    { Fail(R, TEXT("variable_name_invalid"), TEXT("Use one exact bounded member name.")); return R; }
    UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
    const int32 Index = FBlueprintEditorUtils::FindNewVariableIndex(BP, FName(*VariableName));
    if (!BP->SkeletonGeneratedClass || (Index == INDEX_NONE && bSetter))
    { Fail(R, TEXT("graph_member_not_declared"), TEXT("Setters require an own declared member; getters also support an audited generated component in the reachable local SCS. A current skeleton is required.")); return R; }
    const FProperty* InheritedProperty = Index == INDEX_NONE && !bSetter
        ? BP->SkeletonGeneratedClass->FindPropertyByName(FName(*VariableName)) : nullptr;
    UClass* DeclaringClass = InheritedProperty ? InheritedProperty->GetOwnerClass() : nullptr;
    UBlueprint* DeclaringBP = DeclaringClass && DeclaringClass != BP->SkeletonGeneratedClass
        ? Cast<UBlueprint>(DeclaringClass->ClassGeneratedBy) : nullptr;
    const bool bInheritedGetter = DeclaringBP && DeclaringBP != BP;
    const bool bComponentGetter = Index == INDEX_NONE && !bInheritedGetter;
    const FBPVariableDescription* Description = Index != INDEX_NONE ? &BP->NewVariables[Index] : nullptr;
    USCS_Node* ComponentNode = nullptr;
    FName MemberName(*VariableName);
    if (bInheritedGetter)
    {
        const auto Inspection = GeneratedComponent::InspectSavedActorParent(BP->ParentClass.Get());
        R->SetObjectField(TEXT("declaring_parent_validation"), Inspection);
        const int32 ParentIndex = FBlueprintEditorUtils::FindNewVariableIndex(DeclaringBP, MemberName);
        if (!Inspection->GetBoolField(TEXT("success")) || !DeclaringBP->GeneratedClass || !BP->ParentClass
            || !BP->ParentClass->IsChildOf(DeclaringBP->GeneratedClass) || ParentIndex == INDEX_NONE
            || (DeclaringClass != DeclaringBP->GeneratedClass && DeclaringClass != DeclaringBP->SkeletonGeneratedClass))
        { Fail(R, TEXT("graph_inherited_member_unsupported"), TEXT("Inherited getter requires an exact declared variable on the saved clean compiled Actor Blueprint parent chain.")); return R; }
        Description = &DeclaringBP->NewVariables[ParentIndex];
        const FProperty* CompiledProperty = DeclaringBP->GeneratedClass->FindPropertyByName(MemberName);
        FEdGraphPinType CompiledType;
        if (Description->VarName.ToString() != VariableName || !Description->VarGuid.IsValid()
            || Description->VarType.PinCategory != UEdGraphSchema_K2::PC_Boolean || Description->VarType.IsContainer()
            || Description->VarType.bIsReference || !CompiledProperty || CompiledProperty->GetOwnerClass() != DeclaringBP->GeneratedClass
            || !GetDefault<UEdGraphSchema_K2>()->ConvertPropertyToPinType(CompiledProperty, CompiledType) || CompiledType != Description->VarType
            || FBlueprintEditorUtils::IsPropertyPrivate(InheritedProperty)
            || FBlueprintEditorUtils::IsPropertyReadableInBlueprint(BP, InheritedProperty) != FBlueprintEditorUtils::EPropertyReadableState::Readable)
        { Fail(R, TEXT("graph_inherited_member_type_pending"), TEXT("This slice admits readable scalar Boolean declarations only; inherited native/SCS/object/other-type getters and all inherited setters remain unsupported.")); return R; }
        MemberName = Description->VarName;
        R->SetStringField(TEXT("member_scope"), TEXT("inherited_actor_boolean"));
        R->SetStringField(TEXT("declaring_blueprint_path"), DeclaringBP->GetPathName());
        R->SetStringField(TEXT("declaring_class_path"), DeclaringBP->GeneratedClass->GetPathName());
    }
    else if (bComponentGetter)
    {
        Component::FReadResult Components; FString Code, Error;
        if (!Component::ReadLocalComponents(BP->GetPathName(), VariableName, FString(), false, 10000, 65536, Components, Code, Error))
        { R->SetObjectField(TEXT("local_component_observation"), Components.Data); Fail(R, TEXT("graph_member_not_declared"), TEXT("No unambiguous reachable local SCS component matched the member; inherited and external members remain unsupported.")); return R; }
        const TWeakObjectPtr<USCS_Node>* Found = Components.LocalNodes.Find(VariableName);
        ComponentNode = Found ? Found->Get() : nullptr;
        if (!IsValid(ComponentNode) || !ComponentNode->VariableGuid.IsValid() || Components.bLoadedDuringRead
            || !Components.bDirtyObserved || !Components.bDirtyAfterObserved
            || Components.bDirtyBefore != Components.bDirtyAfter || Components.StatusBefore != Components.StatusAfter)
        { Fail(R, TEXT("graph_component_observation_invalid"), TEXT("Local component identity/read-only baseline was not preserved; no getter was spawned.")); return R; }
        const auto Inspection = GeneratedComponent::InspectSavedClass(ComponentNode->ComponentClass.Get());
        R->SetObjectField(TEXT("component_class_validation"), Inspection);
        if (!Inspection->GetBoolField(TEXT("success")))
        { Fail(R, TEXT("graph_component_class_unsupported"), TEXT("Local component class failed the saved runtime ActorComponent inspection.")); return R; }
        MemberName = ComponentNode->GetVariableName();
        R->SetStringField(TEXT("member_scope"), TEXT("local_scs_generated_component"));
        R->SetStringField(TEXT("scs_node_path"), ComponentNode->GetPathName());
        R->SetStringField(TEXT("scs_variable_guid"), ComponentNode->VariableGuid.ToString(EGuidFormats::Digits));
        R->SetBoolField(TEXT("scs_guid_equals_member_reference_guid_verified"), false);
        R->SetStringField(TEXT("component_class_path"), ComponentNode->ComponentClass->GetPathName());
    }
    else { MemberName = Description->VarName; }
    const FProperty* Property = BP->SkeletonGeneratedClass->FindPropertyByName(MemberName);
    FEdGraphPinType ActualType;
    if (!Property || (!bInheritedGetter && Property->GetOwnerClass() != BP->SkeletonGeneratedClass) ||
        !GetDefault<UEdGraphSchema_K2>()->ConvertPropertyToPinType(Property, ActualType)
        || (!bComponentGetter && ActualType != Description->VarType))
    { Fail(R, TEXT("graph_member_skeleton_mismatch"), TEXT("Declared member and skeleton property do not agree; no node was spawned or implicit compile requested.")); return R; }
    if (!Property->HasAnyPropertyFlags(CPF_BlueprintVisible) || (bSetter && Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly)))
    { Fail(R, TEXT("graph_member_access_rejected"), TEXT("The member is not Blueprint-visible or is read-only for a setter.")); return R; }
    // SCS generated components and explicitly audited native object declarations
    // are separate from arbitrary external generated/object member types.
    const FName Category = ActualType.PinCategory;
    const bool Primitive = Category == UEdGraphSchema_K2::PC_Boolean || Category == UEdGraphSchema_K2::PC_Int ||
        Category == UEdGraphSchema_K2::PC_Int64 || Category == UEdGraphSchema_K2::PC_Real ||
        Category == UEdGraphSchema_K2::PC_String || Category == UEdGraphSchema_K2::PC_Name || Category == UEdGraphSchema_K2::PC_Text;
    const UObject* TypeObject = ActualType.PinSubCategoryObject.Get();
    const bool BuiltinStruct = Category == UEdGraphSchema_K2::PC_Struct &&
        (TypeObject == TBaseStructure<FVector>::Get() || TypeObject == TBaseStructure<FRotator>::Get() || TypeObject == TBaseStructure<FTransform>::Get());
    const FObjectProperty* ComponentProperty = bComponentGetter ? CastField<FObjectProperty>(Property) : nullptr;
    const bool AuditedComponent = bComponentGetter && ComponentProperty && ComponentProperty->PropertyClass == ComponentNode->ComponentClass.Get()
        && Category == UEdGraphSchema_K2::PC_Object && TypeObject == ComponentNode->ComponentClass.Get()
        && ActualType.ContainerType == EPinContainerType::None && !ActualType.bIsReference;
    const bool AuditedObject = RuntimeObject::IsSupportedPin(ActualType);
    if (AuditedObject && !bComponentGetter && !bInheritedGetter) { R->SetStringField(TEXT("member_scope"), TEXT("own_runtime_object")); }
    if (bComponentGetter ? !AuditedComponent : ((!Primitive && !BuiltinStruct && !AuditedObject) || ActualType.IsMap() || ActualType.IsSet() || (Primitive && TypeObject)))
    { Fail(R, TEXT("graph_member_type_pending"), TEXT("Supports own scalar/array primitive and Vector/Rotator/Transform members plus scalar audited native UObject/Actor/Controller/PlayerController/ActorComponent references; other objects, class, enum, user-struct, set/map and delegate members remain pending.")); return R; }
    auto Parent = InspectBlueprintParent(BP->ParentClass ? BP->ParentClass->GetPathName() : FString());
    bool ParentOK = false;
    if (!Parent->TryGetBoolField(TEXT("success"), ParentOK) || !ParentOK)
    { R->SetObjectField(TEXT("parent_inspection"), Parent); Fail(R, TEXT("graph_member_parent_unsupported"), TEXT("Blueprint parent chain failed the existing runtime-class guard.")); return R; }
    R->SetStringField(TEXT("variable_name"), MemberName.ToString());
    if (!bComponentGetter) { R->SetStringField(TEXT("variable_guid"), Description->VarGuid.ToString(EGuidFormats::Digits)); }
    R->SetStringField(TEXT("property_path"), Property->GetPathName());
    R->SetStringField(TEXT("pin_category"), Category.ToString());
    R->SetStringField(TEXT("pin_subcategory"), ActualType.PinSubCategory.ToString());
    R->SetBoolField(TEXT("setter"), bSetter);
    FString TypeText; FEdGraphPinType::StaticStruct()->ExportText(TypeText, &ActualType, &ActualType, nullptr, PPF_None, nullptr);
    R->SetStringField(TEXT("expected_pin_type_text"), TypeText);
    R->SetStringField(TEXT("scope"), bInheritedGetter
        ? TEXT("saved generated Actor ancestor's readable scalar Boolean getter only; no inherited setters, native/SCS/object members or load/compile/save") : bComponentGetter
        ? TEXT("reachable local SCS generated non-scene ActorComponent getter; exact skeleton property/type/name, SCS GUID observed separately; no load/compile/save")
        : TEXT("own declared runtime scalar/array member only; no compile/save or external member resolution"));
    Complete(R, Graph); return R;
}

TSharedRef<FJsonObject> ReadExternalMemberVariablePlan(const FString& GraphPath, const FString& VariableName, bool bSetter, const FString& TargetClassPath)
{
    auto R = Result(); UEdGraph* G = ResolveLoadedGraph(GraphPath, R); if (!G) { return R; }
    if (!SafeSelector(VariableName) || !SafeSelector(TargetClassPath) || !TargetClassPath.StartsWith(TEXT("/Game/")))
    { Fail(R, TEXT("graph_external_member_selector_invalid"), TEXT("Requires an exact bounded member and loaded /Game generated class path.")); return R; }
    UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForGraph(G);
    UClass* Class = FindObject<UClass>(nullptr, *TargetClassPath);
    UBlueprint* Source = IsValid(Class) ? Cast<UBlueprint>(Class->ClassGeneratedBy) : nullptr;
    if (!IsValid(Source) || Source == BP || !BP->SkeletonGeneratedClass || BP->SkeletonGeneratedClass->IsChildOf(Class)
        || BP->BlueprintType != BPTYPE_Normal || BP->bBeingCompiled || BP->bQueuedForCompilation || BP->bIsRegeneratingOnLoad
        || BP->bSuppressStructurallyModified || G->GetSchema()->GetClass() != UEdGraphSchema_K2::StaticClass())
    { Fail(R, TEXT("graph_external_member_scope_unsupported"), TEXT("Requires a distinct already-loaded component class and idle standard K2 target; inherited/self contexts use the own member route.")); return R; }
    const bool SourceDirty = Source->GetOutermost()->IsDirty(); const EBlueprintStatus SourceStatus = Source->Status.GetValue();
    auto SourceState = [&]()
    {
        auto State = MakeShared<FJsonObject>(); State->SetStringField(TEXT("blueprint_path"), Source->GetPathName());
        State->SetBoolField(TEXT("dirty"), Source->GetOutermost()->IsDirty()); State->SetNumberField(TEXT("status"), Source->Status.GetValue());
        State->SetStringField(TEXT("generated_class"), Source->GeneratedClass ? Source->GeneratedClass->GetPathName() : FString());
        State->SetStringField(TEXT("skeleton_class"), Source->SkeletonGeneratedClass ? Source->SkeletonGeneratedClass->GetPathName() : FString());
        State->SetNumberField(TEXT("declared_variable_count"), Source->NewVariables.Num()); return State;
    };
    R->SetObjectField(TEXT("external_source_before"), SourceState());
    const auto Inspection = GeneratedComponent::InspectSavedClass(Class); R->SetObjectField(TEXT("external_source_validation"), Inspection);
    R->SetObjectField(TEXT("external_source_after"), SourceState());
    const bool Unchanged = SourceDirty == Source->GetOutermost()->IsDirty() && SourceStatus == Source->Status && Source->GeneratedClass == Class;
    R->SetBoolField(TEXT("external_source_observed_unchanged"), Unchanged);
    if (!Inspection->GetBoolField(TEXT("success")) || !Unchanged || Class->GetPathName() != TargetClassPath || !Source->SkeletonGeneratedClass)
    { Fail(R, TEXT("graph_external_member_class_unsupported"), TEXT("External class must remain saved, clean, compiled and current under the shared non-scene runtime ActorComponent inspection.")); return R; }
    const FName Name(*VariableName); const int32 Index = FBlueprintEditorUtils::FindNewVariableIndex(Source, Name);
    const FProperty* Compiled = Class->FindPropertyByName(Name);
    const FProperty* Property = Source->SkeletonGeneratedClass->FindPropertyByName(Name);
    FEdGraphPinType Type, CompiledType;
    if (Index == INDEX_NONE || !Compiled || !Property || Compiled->GetOwnerClass() != Class
        || Property->GetOwnerClass() != Source->SkeletonGeneratedClass || !Source->NewVariables[Index].VarGuid.IsValid()
        || Source->NewVariables[Index].VarName.ToString() != VariableName
        || !GetDefault<UEdGraphSchema_K2>()->ConvertPropertyToPinType(Property, Type)
        || !GetDefault<UEdGraphSchema_K2>()->ConvertPropertyToPinType(Compiled, CompiledType)
        || Type != CompiledType || Type != Source->NewVariables[Index].VarType)
    { Fail(R, TEXT("graph_external_member_declaration_mismatch"), TEXT("External member must be own-declared with an exact matching GUID/name/current skeleton and compiled type.")); return R; }
    const bool AllowedType = !Type.IsContainer() && !Type.bIsReference && !Type.bIsWeakPointer
        && ((Type.PinCategory == UEdGraphSchema_K2::PC_Boolean && !Type.PinSubCategoryObject.IsValid())
            || (Type.PinCategory == UEdGraphSchema_K2::PC_Real && !Type.PinSubCategoryObject.IsValid()
                && (Type.PinSubCategory == UEdGraphSchema_K2::PC_Float || Type.PinSubCategory == UEdGraphSchema_K2::PC_Double))
            || (Type.PinCategory == UEdGraphSchema_K2::PC_Struct && Type.PinSubCategoryObject.Get() == TBaseStructure<FVector>::Get())
            || RuntimeObject::IsSupportedPin(Type));
    if (!AllowedType)
    { Fail(R, TEXT("graph_external_member_type_pending"), TEXT("External own members currently support scalar Boolean/float/double/Vector and audited native runtime object references; other types remain pending.")); return R; }
    auto Accessible = [&](const FProperty* P)
    {
        return !FBlueprintEditorUtils::IsPropertyPrivate(P)
            && FBlueprintEditorUtils::IsPropertyReadableInBlueprint(BP, P) == FBlueprintEditorUtils::EPropertyReadableState::Readable
            && (!bSetter || FBlueprintEditorUtils::IsPropertyWritableInBlueprint(BP, P) == FBlueprintEditorUtils::EPropertyWritableState::Writable);
    };
    if (!Accessible(Property) || !Accessible(Compiled))
    { Fail(R, TEXT("graph_member_access_rejected"), TEXT("The external property is private, not Blueprint-readable or not writable for this setter.")); return R; }
    const auto Parent = InspectBlueprintParent(BP->ParentClass ? BP->ParentClass->GetPathName() : FString());
    if (!Parent->GetBoolField(TEXT("success")))
    { R->SetObjectField(TEXT("parent_inspection"), Parent); Fail(R, TEXT("graph_member_parent_unsupported"), TEXT("Target parent failed the runtime-class inspection.")); return R; }
    FEdGraphPinType SelfType; SelfType.PinCategory = UEdGraphSchema_K2::PC_Object; SelfType.PinSubCategoryObject = Class;
    FString TypeText, SelfText;
    FEdGraphPinType::StaticStruct()->ExportText(TypeText, &Type, &Type, nullptr, PPF_None, nullptr);
    FEdGraphPinType::StaticStruct()->ExportText(SelfText, &SelfType, &SelfType, nullptr, PPF_None, nullptr);
    R->SetStringField(TEXT("member_scope"), TEXT("external_generated_component")); R->SetStringField(TEXT("variable_name"), VariableName);
    R->SetStringField(TEXT("variable_guid"), Source->NewVariables[Index].VarGuid.ToString(EGuidFormats::Digits)); R->SetBoolField(TEXT("setter"), bSetter);
    R->SetStringField(TEXT("target_class_path"), Class->GetPathName()); R->SetStringField(TEXT("declaring_class_path"), Class->GetPathName());
    R->SetStringField(TEXT("declaring_blueprint_path"), Source->GetPathName()); R->SetStringField(TEXT("property_path"), Property->GetPathName());
    R->SetStringField(TEXT("compiled_property_path"), Compiled->GetPathName()); R->SetStringField(TEXT("pin_category"), Type.PinCategory.ToString());
    R->SetStringField(TEXT("pin_subcategory"), Type.PinSubCategory.ToString()); R->SetStringField(TEXT("expected_pin_type_text"), TypeText);
    R->SetStringField(TEXT("expected_self_pin_type_text"), SelfText); R->SetObjectField(TEXT("external_source_after"), SourceState());
    R->SetStringField(TEXT("scope"), TEXT("loaded saved non-scene ActorComponent own declared readable/writable member; no load, spawn, compile or save"));
    Complete(R, G); return R;
}

TSharedRef<FJsonObject> ReadConnectionPlan(const FString& GraphPath, const FString& SourceNodePath,
    const FString& SourcePinName, const FString& TargetNodePath, const FString& TargetPinName)
{
    auto R = Result(); UEdGraph* Graph = ResolveLoadedGraph(GraphPath, R); if (!Graph) { return R; }
    if (!SafeSelector(SourceNodePath) || !SafeSelector(TargetNodePath) || !SafeSelector(SourcePinName) || !SafeSelector(TargetPinName))
    { Fail(R, TEXT("graph_pin_selector_invalid"), TEXT("Node paths and pin names must be nonempty bounded selectors without control characters.")); return R; }
    UK2Node* Source = FindObject<UK2Node>(nullptr, *SourceNodePath);
    UK2Node* Target = FindObject<UK2Node>(nullptr, *TargetNodePath);
    if (!Source || !Target || Source->GetGraph() != Graph || Target->GetGraph() != Graph || !Graph->Nodes.Contains(Source) || !Graph->Nodes.Contains(Target))
    { Fail(R, TEXT("graph_node_not_found"), TEXT("Both exact loaded K2 nodes must belong to the requested graph.")); return R; }
    UEdGraphPin* Output = Source->FindPin(FName(*SourcePinName), EGPD_Output);
    UEdGraphPin* Input = Target->FindPin(FName(*TargetPinName), EGPD_Input);
    if (!Output || !Input || Output->bOrphanedPin || Input->bOrphanedPin)
    { Fail(R, TEXT("graph_pin_not_found"), TEXT("A non-orphaned output/input pin with the requested name was not found.")); return R; }
    const FPinConnectionResponse Plan = Graph->GetSchema()->CanCreateConnection(Output, Input);
    // The reflected bool CanCreateConnection also admits conversions and link
    // replacement. Expose the exact public schema decision without executing it.
    R->SetBoolField(TEXT("direct_connection_allowed"), Plan.Response == CONNECT_RESPONSE_MAKE);
    R->SetNumberField(TEXT("schema_response_code"), static_cast<int32>(Plan.Response));
    R->SetStringField(TEXT("schema_response_name"), Connection::SchemaResponseName(Plan.Response));
    R->SetBoolField(TEXT("exact_standard_k2_schema"), Graph->GetSchema()->GetClass() == UEdGraphSchema_K2::StaticClass());
    R->SetStringField(TEXT("schema_message"), Plan.Message.ToString());
    R->SetBoolField(TEXT("already_linked"), Output->LinkedTo.Contains(Input) && Input->LinkedTo.Contains(Output));
    R->SetNumberField(TEXT("source_link_count"), Output->LinkedTo.Num()); R->SetNumberField(TEXT("target_link_count"), Input->LinkedTo.Num());
    R->SetBoolField(TEXT("type_exactly_equal"), Output->PinType == Input->PinType);
    R->SetObjectField(TEXT("source_pin_type"), PinTypeObservation(Output->PinType));
    R->SetObjectField(TEXT("target_pin_type"), PinTypeObservation(Input->PinType));
    R->SetStringField(TEXT("scope"), TEXT("current schema decision only; conversion, promotion and link replacement are not dispatched"));
    Complete(R, Graph); return R;
}
}
