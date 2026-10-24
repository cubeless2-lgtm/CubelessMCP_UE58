#include "Compatibility/UE58/CubelessParticleReceiverConstructionApi.h"
#include "Compatibility/UE58/CubelessParticleReceiverApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "BlueprintEditorLibrary.h"
#include "BlueprintEditorSettings.h"
#include "Components/ActorComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "K2Node_Event.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_CallFunction.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_Variable.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/CanvasPanel.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManager.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_EditablePinBase.h"
#include "GameFramework/Actor.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/SceneComponent.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/TextBlock.h"
#include "Fonts/SlateFontInfo.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/SparseDelegate.h"
#include "UObject/Interface.h"

namespace Cubeless::UE58::ParticleReceiver
{
namespace
{
using Function::FResult;
constexpr const TCHAR* InterfacePath = TEXT("/Script/Niagara.NiagaraParticleCallbackHandler");
constexpr const TCHAR* BasicPath = TEXT("/Script/Niagara.BasicParticleData");
constexpr const TCHAR* VectorPath = TEXT("/Script/CoreUObject.Vector");
constexpr const TCHAR* CallbackName = TEXT("ReceiveParticleData");
bool Fail(FResult& R, const TCHAR* Code, const FString& Message)
{ R.ErrorCode = Code; R.Error = Message; return false; }
FString Text(const TSharedRef<FJsonObject>& J)
{ FString Out; FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&Out)); return Out; }
FString TypeText(const FEdGraphPinType& Type)
{ FString Out; FEdGraphPinType::StaticStruct()->ExportText(Out, &Type, &Type, nullptr, PPF_None, nullptr); return Out; }
bool SameType(const FEdGraphPinType& A, const FEdGraphPinType& B)
{ return A == B && A.bIsUObjectWrapper == B.bIsUObjectWrapper && A.bSerializeAsSinglePrecisionFloat == B.bSerializeAsSinglePrecisionFloat
    && A.PinValueType.bTerminalIsUObjectWrapper == B.PinValueType.bTerminalIsUObjectWrapper; }
bool SafePath(const FString& Path)
{
    if (!Path.StartsWith(TEXT("/Game/")) || Path.Len() >= NAME_SIZE) { return false; }
    for (TCHAR C : Path) { if (C < 32 || C == 127) { return false; } } return true;
}
FEdGraphPinType StructType(UScriptStruct* Struct, bool bReference = false, bool bConst = false, bool bArray = false)
{
    FEdGraphPinType Type; Type.PinCategory = UEdGraphSchema_K2::PC_Struct; Type.PinSubCategoryObject = Struct;
    Type.bIsReference = bReference; Type.bIsConst = bConst; Type.ContainerType = bArray ? EPinContainerType::Array : EPinContainerType::None; return Type;
}
FEdGraphPinType IntType()
{ FEdGraphPinType Type; Type.PinCategory = UEdGraphSchema_K2::PC_Int; return Type; }

struct FContext
{
    UBlueprint* BP = nullptr; UEdGraph* Graph = nullptr;
    UClass* Interface = nullptr; UScriptStruct* Basic = nullptr; UScriptStruct* Vector = nullptr;
    UFunction* Callback = nullptr; UFunction* Length = nullptr; UFunction* GetCopy = nullptr;
    const UObject* ArrayLibraryCDO = nullptr;
    const UBlueprintEditorSettings* SavePolicy = nullptr;
    const UEdGraphSchema_K2* Schema = nullptr;
    TSharedRef<FJsonObject> Before = MakeShared<FJsonObject>();
    TSharedRef<FJsonObject> NativeBefore = MakeShared<FJsonObject>();
};

TSharedRef<FJsonObject> NodeState(const UEdGraphNode* Node)
{
    auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("object_path"), Node->GetPathName());
    Row->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString(EGuidFormats::Digits));
    Row->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString(EGuidFormats::Digits));
    Row->SetStringField(TEXT("node_class"), Node->GetClass()->GetPathName());
    Row->SetNumberField(TEXT("x"), Node->NodePosX); Row->SetNumberField(TEXT("y"), Node->NodePosY);
    Row->SetNumberField(TEXT("enabled_state"), static_cast<int32>(Node->GetDesiredEnabledState()));
    Row->SetBoolField(TEXT("ghost"), Node->IsAutomaticallyPlacedGhostNode());
    TArray<TSharedPtr<FJsonValue>> Pins;
    for (const UEdGraphPin* Pin : Node->Pins)
    {
        if (!Pin || Pin->WasTrashed()) { Row->SetBoolField(TEXT("pin_observation_incomplete"), true); continue; }
        auto P = MakeShared<FJsonObject>(); P->SetStringField(TEXT("name"), Pin->PinName.ToString());
        P->SetStringField(TEXT("pin_id"), Pin->PinId.ToString(EGuidFormats::Digits));
        P->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
        P->SetStringField(TEXT("pin_type_text"), TypeText(Pin->PinType));
        P->SetStringField(TEXT("default_value"), Pin->DefaultValue); P->SetStringField(TEXT("autogenerated_default_value"), Pin->AutogeneratedDefaultValue);
        P->SetStringField(TEXT("default_object"), Pin->DefaultObject ? Pin->DefaultObject->GetPathName() : FString());
        P->SetStringField(TEXT("default_text"), Pin->DefaultTextValue.ToString()); P->SetBoolField(TEXT("hidden"), Pin->bHidden);
        P->SetBoolField(TEXT("orphaned"), Pin->bOrphanedPin); P->SetBoolField(TEXT("default_value_is_ignored"), Pin->bDefaultValueIsIgnored);
        P->SetBoolField(TEXT("default_value_is_read_only"), Pin->bDefaultValueIsReadOnly); P->SetBoolField(TEXT("not_connectable"), Pin->bNotConnectable);
        P->SetBoolField(TEXT("advanced_view"), Pin->bAdvancedView); P->SetBoolField(TEXT("display_as_mutable_ref"), Pin->bDisplayAsMutableRef);
        P->SetStringField(TEXT("parent_pin_id"), Pin->ParentPin ? Pin->ParentPin->PinId.ToString(EGuidFormats::Digits) : FString());
        TArray<TSharedPtr<FJsonValue>> SubPins;
        for (const UEdGraphPin* Sub : Pin->SubPins) { SubPins.Add(MakeShared<FJsonValueString>(Sub ? Sub->PinId.ToString(EGuidFormats::Digits) : TEXT("invalid"))); }
        P->SetArrayField(TEXT("sub_pin_ids"), SubPins);
        TArray<FString> Edges;
        for (const UEdGraphPin* Other : Pin->LinkedTo)
        {
            if (!Other || Other->WasTrashed() || !IsValid(Other->GetOwningNodeUnchecked())) { Row->SetBoolField(TEXT("link_observation_incomplete"), true); continue; }
            Edges.Add(Other->GetOwningNodeUnchecked()->GetPathName() + TEXT(":") + Other->PinName.ToString());
        }
        Edges.Sort(); TArray<TSharedPtr<FJsonValue>> Links;
        for (const FString& Edge : Edges) { Links.Add(MakeShared<FJsonValueString>(Edge)); }
        P->SetArrayField(TEXT("links"), Links); Pins.Add(MakeShared<FJsonValueObject>(P));
    }
    Row->SetArrayField(TEXT("pins"), Pins);
    if (const UK2Node_Event* Event = Cast<UK2Node_Event>(Node))
    {
        Row->SetStringField(TEXT("event_name"), Event->EventReference.GetMemberName().ToString());
        const UClass* Parent = Event->EventReference.GetMemberParentClass();
        Row->SetStringField(TEXT("event_member_parent_path"), Parent ? Parent->GetPathName() : FString());
        Row->SetBoolField(TEXT("override_function"), Event->bOverrideFunction);
        Row->SetStringField(TEXT("event_member_guid"), Event->EventReference.GetMemberGuid().ToString(EGuidFormats::Digits));
        Row->SetBoolField(TEXT("event_self_context"), Event->EventReference.IsSelfContext());
        Row->SetStringField(TEXT("custom_function_name"), Event->CustomFunctionName.ToString());
        Row->SetStringField(TEXT("event_function_flags_string"), LexToString(Event->FunctionFlags));
        Row->SetBoolField(TEXT("internal_event"), Event->bInternalEvent);
    }
    if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
    {
        Row->SetStringField(TEXT("function_name"), Call->FunctionReference.GetMemberName().ToString());
        const UClass* Parent = Call->FunctionReference.GetMemberParentClass();
        Row->SetStringField(TEXT("function_member_parent_path"), Parent ? Parent->GetPathName() : FString());
        Row->SetStringField(TEXT("function_member_guid"), Call->FunctionReference.GetMemberGuid().ToString(EGuidFormats::Digits));
        Row->SetBoolField(TEXT("function_self_context"), Call->FunctionReference.IsSelfContext());
    }
    if (const UK2Node_Variable* Variable = Cast<UK2Node_Variable>(Node))
    {
        Row->SetStringField(TEXT("variable_name"), Variable->VariableReference.GetMemberName().ToString());
        const UClass* Parent = Variable->VariableReference.GetMemberParentClass();
        Row->SetStringField(TEXT("variable_member_parent_path"), Parent ? Parent->GetPathName() : FString());
        Row->SetStringField(TEXT("variable_member_guid"), Variable->VariableReference.GetMemberGuid().ToString(EGuidFormats::Digits));
        Row->SetBoolField(TEXT("variable_self_context"), Variable->VariableReference.IsSelfContext());
    }
    if (const UK2Node_BreakStruct* Break = Cast<UK2Node_BreakStruct>(Node))
    { Row->SetStringField(TEXT("struct_type"), Break->StructType ? Break->StructType->GetPathName() : FString()); }
    return Row;
}

bool Capture(FContext& C, const TSharedRef<FJsonObject>& Out)
{
    if (!IsValid(C.BP) || !IsValid(C.Graph) || C.Graph->GetOuter() != C.BP) { return false; }
    Out->SetStringField(TEXT("blueprint_path"), C.BP->GetPathName()); Out->SetStringField(TEXT("graph_path"), C.Graph->GetPathName());
    Out->SetStringField(TEXT("graph_id"), C.Graph->GraphGuid.ToString(EGuidFormats::Digits));
    Out->SetBoolField(TEXT("dirty"), C.BP->GetOutermost()->IsDirty()); Out->SetNumberField(TEXT("blueprint_status"), static_cast<int32>(C.BP->Status.GetValue()));
    TArray<TSharedPtr<FJsonValue>> Variables;
    for (const FBPVariableDescription& Variable : C.BP->NewVariables)
    {
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("name"), Variable.VarName.ToString());
        Row->SetStringField(TEXT("guid"), Variable.VarGuid.ToString(EGuidFormats::Digits)); Row->SetStringField(TEXT("type"), TypeText(Variable.VarType));
        Row->SetStringField(TEXT("flags_string"), LexToString(Variable.PropertyFlags)); Row->SetStringField(TEXT("stored_default"), Variable.DefaultValue);
        Row->SetStringField(TEXT("category"), Variable.Category.ToString()); TArray<TSharedPtr<FJsonValue>> Metadata;
        for (const FBPVariableMetaDataEntry& Entry : Variable.MetaDataArray)
        { auto M = MakeShared<FJsonObject>(); M->SetStringField(TEXT("key"), Entry.DataKey.ToString()); M->SetStringField(TEXT("value"), Entry.DataValue); Metadata.Add(MakeShared<FJsonValueObject>(M)); }
        Row->SetArrayField(TEXT("metadata"), Metadata); Variables.Add(MakeShared<FJsonValueObject>(Row));
    }
    Out->SetArrayField(TEXT("variables"), Variables);
    TArray<TSharedPtr<FJsonValue>> Interfaces;
    for (const FBPInterfaceDescription& Interface : C.BP->ImplementedInterfaces)
    {
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("class_path"), Interface.Interface ? Interface.Interface->GetPathName() : FString());
        TArray<TSharedPtr<FJsonValue>> Graphs; for (const UEdGraph* G : Interface.Graphs) { if (!IsValid(G)) { return false; } Graphs.Add(MakeShared<FJsonValueString>(G->GetPathName())); }
        Row->SetArrayField(TEXT("graphs"), Graphs); Interfaces.Add(MakeShared<FJsonValueObject>(Row));
    }
    Out->SetArrayField(TEXT("interfaces"), Interfaces);
    TArray<UEdGraph*> Graphs; C.BP->GetAllGraphs(Graphs); if (Graphs.Num() > 256) { return false; }
    TArray<TSharedPtr<FJsonValue>> GraphRows; TSet<FGuid> GraphIds; int32 TotalPins = 0;
    for (const UEdGraph* G : Graphs)
    {
        if (!IsValid(G) || !G->GraphGuid.IsValid() || GraphIds.Contains(G->GraphGuid) || G->Nodes.Num() > 2048) { return false; }
        GraphIds.Add(G->GraphGuid); auto GraphRow = MakeShared<FJsonObject>(); GraphRow->SetStringField(TEXT("path"), G->GetPathName());
        GraphRow->SetStringField(TEXT("guid"), G->GraphGuid.ToString(EGuidFormats::Digits)); TArray<TSharedPtr<FJsonValue>> Nodes; TSet<FGuid> NodeIds;
        for (const UEdGraphNode* Node : G->Nodes)
        {
            if (!IsValid(Node) || !Node->NodeGuid.IsValid() || NodeIds.Contains(Node->NodeGuid)) { return false; }
            NodeIds.Add(Node->NodeGuid); TSet<FGuid> PinIds;
            for (const UEdGraphPin* P : Node->Pins)
            {
                if (++TotalPins > 16384 || !P || P->WasTrashed() || !P->PinId.IsValid() || PinIds.Contains(P->PinId)) { return false; }
                PinIds.Add(P->PinId);
                for (const UEdGraphPin* Other : P->LinkedTo)
                { if (!Other || Other->WasTrashed() || !IsValid(Other->GetOwningNodeUnchecked()) || !Other->LinkedTo.Contains(const_cast<UEdGraphPin*>(P))) { return false; } }
            }
            Nodes.Add(MakeShared<FJsonValueObject>(NodeState(Node)));
        }
        GraphRow->SetArrayField(TEXT("nodes"), Nodes); GraphRows.Add(MakeShared<FJsonValueObject>(GraphRow));
    }
    Out->SetArrayField(TEXT("graphs"), GraphRows); return true;
}

struct FParameter
{
    const TCHAR* Name; const TCHAR* Class; uint64 Flags; FEdGraphPinType Type;
    const TCHAR* InnerClass = nullptr; uint64 InnerFlags = 0;
};
bool ExactFunction(UFunction* Function, UClass* Owner, uint32 Flags, const TArray<FParameter>& Expected, const UEdGraphSchema_K2* Schema)
{
    if (!IsValid(Function) || Function->GetOuterUClass() != Owner || static_cast<uint32>(Function->FunctionFlags) != Flags || Function->NumParms != Expected.Num()) { return false; }
    int32 Index = 0;
    for (TFieldIterator<FProperty> It(Function, EFieldIteratorFlags::ExcludeSuper); It; ++It)
    {
        if (!It->HasAnyPropertyFlags(CPF_Parm)) { continue; }
        if (Index >= Expected.Num()) { return false; } const FParameter& E = Expected[Index++]; FEdGraphPinType Type;
        if (It->GetName() != E.Name || It->GetClass()->GetName() != E.Class || It->GetOwnerStruct() != Function || It->ArrayDim != 1
            || static_cast<uint64>(It->GetPropertyFlags()) != E.Flags || !Schema->ConvertPropertyToPinType(*It, Type) || !SameType(Type, E.Type)) { return false; }
        if (E.InnerClass)
        {
            const FArrayProperty* Array = CastField<FArrayProperty>(*It);
            if (!Array || !Array->Inner || Array->Inner->GetClass()->GetName() != E.InnerClass || Array->Inner->ArrayDim != 1
                || static_cast<uint64>(Array->Inner->GetPropertyFlags()) != E.InnerFlags) { return false; }
            if (const FStructProperty* Inner = CastField<FStructProperty>(Array->Inner))
            { if (Inner->Struct != E.Type.PinSubCategoryObject.Get()) { return false; } }
        }
    }
    return Index == Expected.Num();
}

bool Native(FContext& C, FResult& R)
{
    const FResult Inspection = InspectNativeSignatures(); R.Data->SetObjectField(TEXT("native_signature_inspection"), Inspection.Data);
    if (!Inspection.bSuccess || !Inspection.bPostconditionPassed || Inspection.bChanged)
    { return Fail(R, TEXT("particle_native_signature_unavailable"), TEXT("Exact already-loaded receiver native metadata could not be observed unchanged; no target modification dispatched.")); }
    C.NativeBefore = Inspection.Data->GetObjectField(TEXT("before")).ToSharedRef();
    C.Interface = FindObject<UClass>(nullptr, InterfacePath); C.Basic = FindObject<UScriptStruct>(nullptr, BasicPath); C.Vector = FindObject<UScriptStruct>(nullptr, VectorPath);
    UClass* System = FindObject<UClass>(nullptr, TEXT("/Script/Niagara.NiagaraSystem"));
    UClass* ArrayClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.KismetArrayLibrary"));
    C.Callback = C.Interface ? C.Interface->FindFunctionByName(CallbackName) : nullptr;
    C.Length = ArrayClass ? ArrayClass->FindFunctionByName(TEXT("Array_Length")) : nullptr;
    C.GetCopy = ArrayClass ? ArrayClass->FindFunctionByName(TEXT("Array_Get")) : nullptr;
    C.ArrayLibraryCDO = ArrayClass ? ArrayClass->GetDefaultObject(false) : nullptr;
    FEdGraphPinType Object; Object.PinCategory = UEdGraphSchema_K2::PC_Object; Object.PinSubCategoryObject = System;
    FEdGraphPinType WildArray; WildArray.PinCategory = UEdGraphSchema_K2::PC_Wildcard; WildArray.ContainerType = EPinContainerType::Array; WildArray.bIsReference = true; WildArray.bIsConst = true;
    FEdGraphPinType WildItem; WildItem.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
    bool Callback = C.Interface && System && C.Basic && C.Vector && C.Interface->ClassFlags == 809517217U && System->ClassFlags == 818413728U
        && ExactFunction(C.Callback, C.Interface, 214043648U, {
            {TEXT("Data"), TEXT("ArrayProperty"), 4503599761589122ULL, StructType(C.Basic, true, true, true), TEXT("StructProperty"), 68719476736ULL},
            {TEXT("NiagaraSystem"), TEXT("ObjectProperty"), 6755468160533120ULL, Object},
            {TEXT("SimulationPositionOffset"), TEXT("StructProperty"), 6755469368492930ULL, StructType(C.Vector, true, true)}}, C.Schema)
        && UEdGraphSchema_K2::CanKismetOverrideFunction(C.Callback) && UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(C.Callback);
    int32 InterfaceFunctionCount = 0;
    if (C.Interface)
    {
        for (TFieldIterator<UFunction> It(C.Interface, EFieldIteratorFlags::ExcludeSuper); It; ++It)
        { ++InterfaceFunctionCount; Callback &= *It == C.Callback && It->GetOuterUClass() == C.Interface; }
    }
    Callback &= InterfaceFunctionCount == 1;
    // UObject's source-declared ExecuteUbergraph is inherited by UInterface.
    // It is not another directly declared Niagara callback. Observe the actual
    // loaded identity separately; permit no other inherited function here.
    UClass* BaseInterface = FindObject<UClass>(nullptr, TEXT("/Script/CoreUObject.Interface"));
    UClass* BaseObject = FindObject<UClass>(nullptr, TEXT("/Script/CoreUObject.Object"));
    UFunction* ExpectedInherited = BaseObject ? BaseObject->FindFunctionByName(TEXT("ExecuteUbergraph")) : nullptr;
    bool InheritedValid = IsValid(BaseInterface) && IsValid(BaseObject) && IsValid(ExpectedInherited)
        && C.Interface && C.Interface->GetSuperClass() == BaseInterface && BaseInterface->GetSuperClass() == BaseObject
        && BaseInterface->HasAnyClassFlags(CLASS_Native) && BaseObject->HasAnyClassFlags(CLASS_Native)
        && ExpectedInherited->GetOuterUClass() == BaseObject && ExpectedInherited->GetName() == TEXT("ExecuteUbergraph")
        && !ExpectedInherited->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects);
    TArray<TSharedPtr<FJsonValue>> InheritedRows; int32 InheritedCount = 0;
    if (C.Interface)
    {
        for (TFieldIterator<UFunction> It(C.Interface, EFieldIteratorFlags::IncludeSuper); It; ++It)
        {
            if (It->GetOuterUClass() == C.Interface) { continue; }
            if (++InheritedCount > 8) { InheritedValid = false; break; }
            auto Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("name"), It->GetName()); Row->SetStringField(TEXT("function_path"), It->GetPathName());
            Row->SetStringField(TEXT("declaring_class_path"), It->GetOuterUClass() ? It->GetOuterUClass()->GetPathName() : FString());
            Row->SetStringField(TEXT("function_flags_string"), LexToString(static_cast<uint32>(It->FunctionFlags)));
            Row->SetNumberField(TEXT("declared_num_parms"), It->NumParms);
            const bool Exact = InheritedValid && *It == ExpectedInherited;
            Row->SetBoolField(TEXT("exact_source_declared_uobject_identity"), Exact); InheritedValid &= Exact;
            InheritedRows.Add(MakeShared<FJsonValueObject>(Row));
        }
    }
    InheritedValid &= InheritedCount == 1; Callback &= InheritedValid;
    R.Data->SetArrayField(TEXT("inherited_interface_functions"), InheritedRows);
    R.Data->SetNumberField(TEXT("inherited_interface_function_count_observed"), InheritedCount);
    R.Data->SetNumberField(TEXT("interface_function_count_including_super_observed"), InterfaceFunctionCount + InheritedCount);
    R.Data->SetBoolField(TEXT("inherited_interface_function_identity_verified"), InheritedValid);
    R.Data->SetStringField(TEXT("interface_function_count_scope"), TEXT("directly declared exact callback only; inherited UObject.ExecuteUbergraph observed separately"));
    R.Data->SetStringField(TEXT("inherited_interface_function_source_basis"), TEXT("installed Object.h:1778-1779 UObject.ExecuteUbergraph; Interface.h:18 UInterface derives UObject; NiagaraDataInterfaceExport.h:26 interface derives UInterface; actual loaded identity must match"));
    if (C.Callback)
    {
        for (const TCHAR* Key : {TEXT("ArrayParm"), TEXT("ArrayTypeDependentParams"), TEXT("AutoCreateRefTerm"), TEXT("BlueprintInternalUseOnly"),
            TEXT("CustomThunk"), TEXT("CommutativeAssociativeBinaryOperator"), TEXT("Latent"), TEXT("DeprecatedFunction"), TEXT("WorldContext"), TEXT("ScriptName"), TEXT("AnimBlueprintFunction")})
        { Callback &= C.Callback->GetMetaData(Key).IsEmpty(); }
    }
    bool Arrays = ArrayClass && ArrayClass->ClassFlags == 810025120U && IsValid(C.ArrayLibraryCDO) && C.ArrayLibraryCDO->GetClass() == ArrayClass
        && ExactFunction(C.Length, ArrayClass, 339878915U, {
            {TEXT("TargetArray"), TEXT("ArrayProperty"), 4503599761589122ULL, WildArray, TEXT("IntProperty"), 2251869606904320ULL},
            {TEXT("ReturnValue"), TEXT("IntProperty"), 6755469234276224ULL, IntType()}}, C.Schema)
        && ExactFunction(C.GetCopy, ArrayClass, 339878915U, {
            {TEXT("TargetArray"), TEXT("ArrayProperty"), 4503599761589122ULL, WildArray, TEXT("IntProperty"), 2251869606904320ULL},
            {TEXT("Index"), TEXT("IntProperty"), 6755469234274944ULL, IntType()},
            {TEXT("Item"), TEXT("IntProperty"), 6755469234275200ULL, WildItem}}, C.Schema)
        && C.Length->GetMetaData(TEXT("ArrayParm")) == TEXT("TargetArray") && C.Length->GetMetaData(TEXT("ArrayTypeDependentParams")).IsEmpty()
        && C.Length->GetMetaData(TEXT("CustomThunk")) == TEXT("true") && C.Length->GetMetaData(TEXT("BlueprintInternalUseOnly")).IsEmpty()
        && C.GetCopy->GetMetaData(TEXT("ArrayParm")) == TEXT("TargetArray") && C.GetCopy->GetMetaData(TEXT("ArrayTypeDependentParams")) == TEXT("Item")
        && C.GetCopy->GetMetaData(TEXT("CustomThunk")) == TEXT("true") && C.GetCopy->GetMetaData(TEXT("BlueprintInternalUseOnly")) == TEXT("true")
        && !C.Length->HasMetaData(TEXT("AutoCreateRefTerm")) && !C.GetCopy->HasMetaData(TEXT("AutoCreateRefTerm"));
    if (C.Length && C.GetCopy)
    {
        for (const TCHAR* Key : {TEXT("CommutativeAssociativeBinaryOperator"), TEXT("Latent"), TEXT("DeprecatedFunction"), TEXT("WorldContext"), TEXT("ScriptName")})
        { Arrays &= C.Length->GetMetaData(Key).IsEmpty() && C.GetCopy->GetMetaData(Key).IsEmpty(); }
    }
    bool Fields = C.Basic && static_cast<uint32>(C.Basic->StructFlags) == 4097U && C.Basic->GetStructureSize() == 56
        && C.Basic->GetMetaData(TEXT("BlueprintType")) == TEXT("true") && C.Basic->GetMetaData(TEXT("Blueprintable")).IsEmpty()
        && !C.Basic->HasMetaData(TEXT("NativeBreakFunction")) && !C.Basic->HasMetaData(TEXT("NativeMakeFunction"));
    int32 Index = 0; const TCHAR* Names[] = {TEXT("Position"), TEXT("Size"), TEXT("Velocity")};
    if (C.Basic)
    {
        for (TFieldIterator<FProperty> It(C.Basic, EFieldIteratorFlags::ExcludeSuper); It; ++It)
        {
            FEdGraphPinType Type; FEdGraphPinType Expected = StructType(C.Vector);
            if (Index == 1) { Expected = FEdGraphPinType(); Expected.PinCategory = UEdGraphSchema_K2::PC_Real; Expected.PinSubCategory = UEdGraphSchema_K2::PC_Float; }
            Fields &= Index < 3 && It->GetOwnerStruct() == C.Basic && It->GetName() == Names[FMath::Min(Index, 2)] && It->ArrayDim == 1
                && It->GetClass()->GetName() == (Index == 1 ? TEXT("FloatProperty") : TEXT("StructProperty"))
                && static_cast<uint64>(It->GetPropertyFlags()) == 6755469234274836ULL && C.Schema->ConvertPropertyToPinType(*It, Type) && SameType(Type, Expected);
            ++Index;
        }
    }
    Fields &= Index == 3;
    R.Data->SetStringField(TEXT("native_contract_basis"), TEXT("UE5.8.3-58210709 engine-particle-receiver-signatures-AD4 exact reflection"));
    R.Data->SetBoolField(TEXT("native_callback_contract_verified"), Callback); R.Data->SetBoolField(TEXT("native_array_contracts_verified"), Arrays);
    R.Data->SetBoolField(TEXT("native_basic_particle_contract_verified"), Fields);
    R.Data->SetNumberField(TEXT("interface_function_count_observed"), InterfaceFunctionCount);
    R.Data->SetStringField(TEXT("existing_array_library_cdo_path"), C.ArrayLibraryCDO ? C.ArrayLibraryCDO->GetPathName() : FString());
    return (Callback && Arrays && Fields) || Fail(R, TEXT("particle_native_contract_mismatch"), TEXT("Exact native receiver/array/struct flags, metadata, ordered types or owners differ; no modification dispatched."));
}

bool Prepare(const FString& GraphPath, int32 X, int32 Y, FContext& C, FResult& R)
{
    if (!IsEditorGameThread() || IsPlaySessionActive()) { return Fail(R, TEXT("particle_editor_state_invalid"), TEXT("Requires Editor game thread outside PIE/SIE.")); }
    if (!SafePath(GraphPath) || X < -1000000 || X > 1000000 || Y < -1000000 || Y > 1000000)
    { return Fail(R, TEXT("particle_selector_invalid"), TEXT("Requires an exact bounded loaded Game graph path and bounded coordinates.")); }
    C.Graph = FindObject<UEdGraph>(nullptr, *GraphPath); C.BP = C.Graph ? FBlueprintEditorUtils::FindBlueprintForGraph(C.Graph) : nullptr;
    C.Schema = C.Graph ? Cast<UEdGraphSchema_K2>(C.Graph->GetSchema()) : nullptr;
    if (!IsValid(C.BP) || !IsValid(C.Graph) || C.Graph->GetOuter() != C.BP || C.BP->ParentClass != UActorComponent::StaticClass()
        || C.BP->BlueprintType != BPTYPE_Normal || C.BP->bBeingCompiled || C.BP->bQueuedForCompilation || C.BP->bSuppressStructurallyModified
        || C.BP->bIsRegeneratingOnLoad || C.BP->Status == BS_BeingCreated || !C.BP->SkeletonGeneratedClass
        || !C.Schema || C.Schema->GetClass() != UEdGraphSchema_K2::StaticClass() || !C.BP->UbergraphPages.Contains(C.Graph)
        || !C.Graph->GraphGuid.IsValid() || FBlueprintEditorUtils::FindEventGraph(C.BP) != C.Graph || C.Graph->Nodes.Num() >= 2048)
    { return Fail(R, TEXT("particle_receiver_scope_unsupported"), TEXT("Requires idle normal native ActorComponent Blueprint and its exact directly owned standard K2 EventGraph; no graph creation or asset loading.")); }
    for (TObjectIterator<UBlueprint> It; It; ++It)
    { if (*It != C.BP && It->ParentClass && C.BP->GeneratedClass && It->ParentClass->IsChildOf(C.BP->GeneratedClass)) { return Fail(R, TEXT("particle_external_dependants_unsupported"), TEXT("A loaded derived Blueprint could be changed by structural regeneration.")); } }
    if (C.BP->GeneratedClass)
    {
        TArray<UObject*> Instances; GetObjectsOfClass(C.BP->GeneratedClass, Instances, true, RF_NoFlags);
        for (const UObject* Instance : Instances)
        { if (IsValid(Instance) && Instance->GetOutermost() != C.BP->GetOutermost()) { return Fail(R, TEXT("particle_external_instances_unsupported"), TEXT("Loaded external instances are unsupported during receiver authoring.")); } }
    }
    if (!Capture(C, C.Before)) { return Fail(R, TEXT("particle_observation_incomplete"), TEXT("Blueprint graph/pin/declaration observation is invalid or exceeds its bounds.")); }
    R.Data->SetObjectField(TEXT("before"), C.Before); R.Data->SetNumberField(TEXT("requested_x"), X); R.Data->SetNumberField(TEXT("requested_y"), Y);
    if (!Native(C, R)) { return false; }
    for (const FBPInterfaceDescription& Interface : C.BP->ImplementedInterfaces)
    { if (Interface.Interface != C.Interface || !Interface.Graphs.IsEmpty()) { return Fail(R, TEXT("particle_interfaces_unsupported"), TEXT("Only the exact callback interface with no alternate function graphs is supported in this finite receiver slice.")); } }
    if (C.BP->ImplementedInterfaces.Num() > 1) { return Fail(R, TEXT("particle_interface_identity_invalid"), TEXT("Duplicate interface declarations are not repaired.")); }
    auto AfterRead = MakeShared<FJsonObject>();
    if (!Capture(C, AfterRead) || Text(AfterRead) != Text(C.Before))
    { R.bChangeStateKnown = false; R.Data->SetObjectField(TEXT("after_preflight"), AfterRead); return Fail(R, TEXT("particle_preflight_changed_state"), TEXT("Read-only preflight did not preserve the observed target; no modification dispatched.")); }
    return true;
}

bool GuardStructuralSavePolicy(FContext& C, FResult& R)
{
    C.SavePolicy = FindObject<UBlueprintEditorSettings>(nullptr, TEXT("/Script/BlueprintGraph.Default__BlueprintEditorSettings"));
    const bool Present = IsValid(C.SavePolicy) && C.SavePolicy->GetClass() == UBlueprintEditorSettings::StaticClass()
        && C.SavePolicy->HasAnyFlags(RF_ClassDefaultObject) && C.SavePolicy->GetClass()->GetDefaultObject(false) == C.SavePolicy;
    R.Data->SetStringField(TEXT("save_policy_object_path"), Present ? C.SavePolicy->GetPathName() : FString());
    if (Present) { R.Data->SetNumberField(TEXT("save_on_compile_observed"), static_cast<int32>(C.SavePolicy->SaveOnCompile.GetValue())); }
    R.Data->SetBoolField(TEXT("save_on_compile_never_verified"), Present && C.SavePolicy->SaveOnCompile == SoC_Never);
    R.Data->SetBoolField(TEXT("save_policy_modified"), false);
    return (Present && C.SavePolicy->SaveOnCompile == SoC_Never)
        || Fail(R, TEXT("particle_save_policy_unsafe"), TEXT("The already-created BlueprintEditorSettings CDO must have SaveOnCompile=Never before structural interface/event changes; settings were not changed."));
}

bool InterfacePresent(const FContext& C)
{ return C.BP->ImplementedInterfaces.Num() == 1 && C.BP->ImplementedInterfaces[0].Interface == C.Interface; }
bool HasCallbackDeclarationConflict(const FContext& C)
{
    TArray<UEdGraph*> Graphs; C.BP->GetAllGraphs(Graphs);
    for (const UEdGraph* Graph : Graphs)
    {
        if (Graph->GetFName() == FName(CallbackName)) { return true; }
        for (const UEdGraphNode* Node : Graph->Nodes)
        {
            const UK2Node_Event* Event = Cast<UK2Node_Event>(Node);
            if (Event && Event->CustomFunctionName == FName(CallbackName)) { return true; }
        }
    }
    return false;
}
bool EventMatches(const FContext& C, const UK2Node_Event* Event, bool bRequireUnlinked)
{
    if (!Event || Event->GetClass() != UK2Node_Event::StaticClass() || Event->GetGraph() != C.Graph || !Event->bOverrideFunction
        || Event->EventReference.GetMemberParentClass() != C.Interface || Event->EventReference.GetMemberName() != FName(CallbackName)
        || Event->EventReference.IsSelfContext() || Event->EventReference.GetMemberGuid().IsValid()
        || Event->bInternalEvent || Event->FunctionFlags != 0 || Event->CustomFunctionName != NAME_None || Event->Pins.Num() != 5 || !Event->NodeGuid.IsValid()) { return false; }
    const UEdGraphPin* Then = Event->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
    const UEdGraphPin* Delegate = Event->FindPin(TEXT("OutputDelegate"), EGPD_Output);
    if (!Then || Then->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec || !Delegate || Delegate->PinType.PinCategory != UEdGraphSchema_K2::PC_Delegate) { return false; }
    for (TFieldIterator<FProperty> It(C.Callback, EFieldIteratorFlags::ExcludeSuper); It; ++It)
    {
        if (!It->HasAnyPropertyFlags(CPF_Parm)) { continue; }
        const UEdGraphPin* Pin = Event->FindPin(It->GetFName(), EGPD_Output); FEdGraphPinType Type;
        if (!Pin || !C.Schema->ConvertPropertyToPinType(*It, Type) || !SameType(Pin->PinType, Type) || Pin->bOrphanedPin) { return false; }
    }
    for (const UEdGraphPin* Pin : Event->Pins) { if (!Pin || Pin->bOrphanedPin || (bRequireUnlinked && !Pin->LinkedTo.IsEmpty())) { return false; } }
    return true;
}

bool ArrayNodeMatches(const FContext& C, const UK2Node_CallArrayFunction* Node, UFunction* Function, bool bCopy, bool bTyped)
{
    if (!IsValid(Node) || Node->GetClass() != UK2Node_CallArrayFunction::StaticClass() || Node->GetGraph() != C.Graph
        || !Node->NodeGuid.IsValid() || !Node->IsNodePure() || Node->Pins.Num() != (bCopy ? 4 : 3)
        || Node->FunctionReference.GetMemberParentClass() != Function->GetOuterUClass()
        || Node->FunctionReference.GetMemberName() != Function->GetFName() || Node->FunctionReference.GetMemberGuid().IsValid()
        || Node->FunctionReference.IsSelfContext()) { return false; }
    const UEdGraphPin* Self = Node->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input);
    FEdGraphPinType SelfType; SelfType.PinCategory = UEdGraphSchema_K2::PC_Object; SelfType.PinSubCategoryObject = Function->GetOuterUClass();
    const UEdGraphPin* Array = Node->FindPin(TEXT("TargetArray"), EGPD_Input);
    const UEdGraphPin* Output = Node->FindPin(bCopy ? TEXT("Item") : TEXT("ReturnValue"), EGPD_Output);
    FEdGraphPinType ArrayType = StructType(C.Basic, true, true, true);
    FEdGraphPinType OutputType = bCopy ? StructType(C.Basic) : IntType();
    if (!bTyped)
    {
        ArrayType = FEdGraphPinType(); ArrayType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
        ArrayType.ContainerType = EPinContainerType::Array; ArrayType.bIsReference = true; ArrayType.bIsConst = true;
        if (bCopy) { OutputType = FEdGraphPinType(); OutputType.PinCategory = UEdGraphSchema_K2::PC_Wildcard; }
    }
    if (!Self || !SameType(Self->PinType, SelfType) || !Self->bHidden || Self->DefaultObject != C.ArrayLibraryCDO || !Self->LinkedTo.IsEmpty()
        || !Array || !SameType(Array->PinType, ArrayType) || !Array->bDefaultValueIsIgnored || Array->bHidden
        || !Output || !SameType(Output->PinType, OutputType) || !Output->LinkedTo.IsEmpty() || Output->bHidden) { return false; }
    if (bCopy)
    {
        const UEdGraphPin* Index = Node->FindPin(TEXT("Index"), EGPD_Input);
        if (!Index || !SameType(Index->PinType, IntType()) || Index->DefaultValue != TEXT("0") || Index->AutogeneratedDefaultValue != TEXT("0")
            || !Index->LinkedTo.IsEmpty() || Index->DefaultObject || !Index->DefaultTextValue.IsEmpty() || Index->bHidden || Index->bDefaultValueIsIgnored) { return false; }
    }
    for (const UEdGraphPin* P : Node->Pins)
    { if (!P || P->WasTrashed() || P->bOrphanedPin || P->ParentPin || !P->SubPins.IsEmpty()) { return false; } }
    return true;
}

bool PriorNodesPreserved(const FContext& C, UEdGraphNode* Created, UK2Node_Event* DataSource, UEdGraphPin* NewArrayPin)
{
    for (const auto& GraphValue : C.Before->GetArrayField(TEXT("graphs")))
    {
        const auto OldGraph = GraphValue->AsObject(); const UEdGraph* G = FindObject<UEdGraph>(nullptr, *OldGraph->GetStringField(TEXT("path")));
        if (!IsValid(G) || G->GraphGuid.ToString(EGuidFormats::Digits) != OldGraph->GetStringField(TEXT("guid"))
            || G->Nodes.Num() != OldGraph->GetArrayField(TEXT("nodes")).Num() + (G == C.Graph && Created ? 1 : 0)) { return false; }
        for (const auto& NodeValue : OldGraph->GetArrayField(TEXT("nodes")))
        {
            const auto Old = NodeValue->AsObject(); const UEdGraphNode* Node = FindObject<UEdGraphNode>(nullptr, *Old->GetStringField(TEXT("object_path")));
            if (!IsValid(Node) || !G->Nodes.Contains(Node)) { return false; }
            if (Node == DataSource && NewArrayPin)
            {
                const UEdGraphPin* Data = DataSource->FindPin(TEXT("Data"), EGPD_Output);
                if (!Data || !Data->LinkedTo.Contains(NewArrayPin) || !NewArrayPin->LinkedTo.Contains(const_cast<UEdGraphPin*>(Data))) { return false; }
                auto Expected = MakeShared<FJsonObject>(); for (const auto& Pair : Old->Values) { Expected->SetField(FString(Pair.Key.ToView()), Pair.Value); }
                TArray<TSharedPtr<FJsonValue>> Pins;
                for (const auto& PinValue : Old->GetArrayField(TEXT("pins")))
                {
                    const auto OldPin = PinValue->AsObject(); auto P = MakeShared<FJsonObject>();
                    for (const auto& Pair : OldPin->Values) { P->SetField(FString(Pair.Key.ToView()), Pair.Value); }
                    if (OldPin->GetStringField(TEXT("name")) == TEXT("Data") && OldPin->GetStringField(TEXT("direction")) == TEXT("output"))
                    {
                        TArray<FString> Edges; for (const auto& Edge : OldPin->GetArrayField(TEXT("links"))) { Edges.Add(Edge->AsString()); }
                        Edges.Add(NewArrayPin->GetOwningNode()->GetPathName() + TEXT(":") + NewArrayPin->PinName.ToString()); Edges.Sort();
                        TArray<TSharedPtr<FJsonValue>> Links; for (const FString& Edge : Edges) { Links.Add(MakeShared<FJsonValueString>(Edge)); } P->SetArrayField(TEXT("links"), Links);
                    }
                    Pins.Add(MakeShared<FJsonValueObject>(P));
                }
                Expected->SetArrayField(TEXT("pins"), Pins); if (Text(Expected) != Text(NodeState(Node))) { return false; }
            }
            else if (Text(Old.ToSharedRef()) != Text(NodeState(Node))) { return false; }
        }
    }
    return true;
}

void Finish(FContext& C, FResult& R, bool Verified, UEdGraphNode* Created = nullptr, UK2Node_Event* DataSource = nullptr,
    UEdGraphPin* NewArrayPin = nullptr, bool bInterfaceAdded = false)
{
    if (!R.ErrorCode.IsEmpty()) { R.Data->SetStringField(TEXT("failed_mutation_stage"), R.Stage); }
    R.Stage = TEXT("verification"); auto After = MakeShared<FJsonObject>(); const bool Known = Capture(C, After);
    R.Data->SetObjectField(TEXT("after"), After); R.bChangeStateKnown = Known; R.bChanged = Known && Text(C.Before) != Text(After);
    R.Data->SetBoolField(TEXT("created"), Created != nullptr); R.Data->SetBoolField(TEXT("reused"), !R.bMutationAttempted);
    if (Created && IsValid(Created))
    {
        const auto Node = NodeState(Created); R.Data->SetObjectField(TEXT("node"), Node);
        for (const TCHAR* Key : {TEXT("node_id"), TEXT("node_guid"), TEXT("object_path"), TEXT("node_class")}) { R.Data->SetStringField(Key, Node->GetStringField(Key)); }
    }
    const FResult NativeAfter = InspectNativeSignatures(); R.Data->SetObjectField(TEXT("native_after"), NativeAfter.Data);
    const bool NativePreserved = NativeAfter.bSuccess && !NativeAfter.bChanged
        && Text(NativeAfter.Data->GetObjectField(TEXT("after")).ToSharedRef()) == Text(C.NativeBefore);
    R.Data->SetBoolField(TEXT("native_source_observed_unchanged"), NativePreserved);
    const bool Prior = Known && PriorNodesPreserved(C, Created, DataSource, NewArrayPin);
    const bool SavePolicyPreserved = !C.SavePolicy || (IsValid(C.SavePolicy) && C.SavePolicy->SaveOnCompile == SoC_Never
        && FindObject<UBlueprintEditorSettings>(nullptr, TEXT("/Script/BlueprintGraph.Default__BlueprintEditorSettings")) == C.SavePolicy
        && C.SavePolicy->GetClass()->GetDefaultObject(false) == C.SavePolicy);
    R.Data->SetBoolField(TEXT("save_policy_observed_unchanged"), SavePolicyPreserved);
    bool Declarations = Known;
    if (Known)
    {
        auto A = MakeShared<FJsonObject>(); A->SetArrayField(TEXT("variables"), After->GetArrayField(TEXT("variables")));
        auto B = MakeShared<FJsonObject>(); B->SetArrayField(TEXT("variables"), C.Before->GetArrayField(TEXT("variables"))); Declarations = Text(A) == Text(B);
        if (!bInterfaceAdded)
        { A->SetArrayField(TEXT("interfaces"), After->GetArrayField(TEXT("interfaces"))); B->SetArrayField(TEXT("interfaces"), C.Before->GetArrayField(TEXT("interfaces"))); Declarations &= Text(A) == Text(B); }
        else { Declarations &= InterfacePresent(C) && C.Before->GetArrayField(TEXT("interfaces")).IsEmpty(); }
        Declarations &= After->GetArrayField(TEXT("graphs")).Num() == C.Before->GetArrayField(TEXT("graphs")).Num();
    }
    R.Data->SetBoolField(TEXT("existing_nodes_preserved"), Prior); R.Data->SetBoolField(TEXT("declarations_preserved"), Declarations);
    R.Data->SetBoolField(TEXT("atomic_rollback_available"), false); R.Data->SetBoolField(TEXT("safe_to_automatically_retry"), false);
    R.Data->SetStringField(TEXT("change_observation_scope"), TEXT("loaded owned Blueprint graphs/nodes/pins/defaults/links/declarations/interfaces and dirty/status plus exact native metadata; arbitrary Editor callbacks and UI selection are not completely audited"));
    R.bSuccess = R.bPostconditionPassed = Verified && Known && Prior && Declarations && NativePreserved && SavePolicyPreserved;
    if (!R.bSuccess && R.ErrorCode.IsEmpty()) { Fail(R, TEXT("particle_postcondition_failed"), TEXT("Actual receiver graph/declaration/native observations differ; retain partial nodes/links and inspect before any new operation. No rollback or retry was performed.")); }
    if (R.bSuccess) { R.Stage = TEXT("complete"); }
}

FResult AddArray(const FString& GraphPath, const FString& EventNodePath, int32 X, int32 Y, bool bCopy)
{
    FResult R; FContext C; if (!Prepare(GraphPath, X, Y, C, R)) { return R; }
    if (!InterfacePresent(C) || !SafePath(EventNodePath)) { Fail(R, TEXT("particle_array_source_invalid"), TEXT("Requires the implemented exact callback interface and loaded exact owned event node path.")); return R; }
    UK2Node_Event* Event = FindObject<UK2Node_Event>(nullptr, *EventNodePath);
    if (!EventMatches(C, Event, false)) { Fail(R, TEXT("particle_array_source_invalid"), TEXT("Source is not the exact native ReceiveParticleData override with its current typed parameters.")); return R; }
    UEdGraphPin* Data = Event->FindPin(TEXT("Data"), EGPD_Output);
    const auto SourceBefore = NodeState(Event); R.Data->SetObjectField(TEXT("source_event_before"), SourceBefore);
    UFunction* Function = bCopy ? C.GetCopy : C.Length; TStrongObjectPtr<UBlueprint> KeepAlive(C.BP);
    R.Stage = TEXT("create_array_node"); R.bMutationAttempted = true; C.BP->Modify(); C.Graph->Modify();
    FGraphNodeCreator<UK2Node_CallArrayFunction> Creator(*C.Graph); UK2Node_CallArrayFunction* Node = Creator.CreateNode(false);
    Node->SetFromFunction(Function); Node->NodePosX = X; Node->NodePosY = Y; Creator.Finalize();
    R.Data->SetStringField(TEXT("function_name"), Function->GetName()); R.Data->SetStringField(TEXT("function_path"), Function->GetPathName());
    R.Data->SetStringField(TEXT("source_event_node_path"), Event->GetPathName()); R.Data->SetStringField(TEXT("source_pin"), TEXT("Data"));
    UEdGraphPin* Array = Node->FindPin(TEXT("TargetArray"), EGPD_Input); UEdGraphPin* Output = Node->FindPin(bCopy ? TEXT("Item") : TEXT("ReturnValue"), EGPD_Output);
    R.Data->SetObjectField(TEXT("created_array_node_before_connection"), NodeState(Node));
    bool Shape = ArrayNodeMatches(C, Node, Function, bCopy, false) && Array->LinkedTo.IsEmpty();
    bool Connected = false;
    if (Shape)
    {
        const FPinConnectionResponse Response = C.Schema->CanCreateConnection(Data, Array);
        R.Data->SetNumberField(TEXT("schema_response"), static_cast<int32>(Response.Response)); R.Data->SetStringField(TEXT("schema_message"), Response.Message.ToString());
        Shape &= Response.Response == CONNECT_RESPONSE_MAKE;
        if (Shape) { R.Stage = TEXT("connect_array_data"); Connected = C.Schema->TryCreateConnection(Data, Array); }
    }
    R.Data->SetBoolField(TEXT("direct_connection_allowed"), Shape); R.Data->SetBoolField(TEXT("engine_connection_return"), Connected);
    FBlueprintEditorUtils::MarkBlueprintAsModified(C.BP); C.Graph->NotifyGraphChanged();
    const bool ActualLink = Array && Array->LinkedTo.Num() == 1 && Array->LinkedTo[0] == Data && Data->LinkedTo.Contains(Array);
    const bool Typed = Shape && Connected && ActualLink && ArrayNodeMatches(C, Node, Function, bCopy, true);
    R.Data->SetBoolField(TEXT("source_data_connection_observed"), ActualLink);
    R.Data->SetBoolField(TEXT("typed_array_specialization_verified"), Typed);
    R.Data->SetBoolField(TEXT("array_source_read_only"), Typed && Data->PinType.bIsConst && Array->PinType.bIsConst);
    R.Data->SetBoolField(TEXT("copy_result_not_reference"), bCopy && Typed && !Output->PinType.bIsReference);
    R.Data->SetNumberField(TEXT("index"), bCopy ? 0 : -1); R.Data->SetBoolField(TEXT("implicit_skeleton_compile_requested"), false);
    if (!Typed) { Fail(R, TEXT("particle_array_shape_or_connection_failed"), TEXT("The owned array node/link failed its exact wildcard→BasicParticleData copy/length postcondition; partial node/link is retained, not rolled back.")); }
    Finish(C, R, Typed, Node, ActualLink ? Event : nullptr, ActualLink ? Array : nullptr);
    R.Data->SetObjectField(TEXT("source_event_after"), NodeState(Event)); return R;
}
// QB concrete first generic interface leaf; existing Niagara receiver is separate.
struct FWidgetInterfaceWork
{
    FResult& R;TStrongObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;
    TStrongObjectPtr<UWidgetBlueprint> BP;TStrongObjectPtr<UPackage> Package;TStrongObjectPtr<UWidgetTree> Tree;TStrongObjectPtr<UCanvasPanel> Root;
    TStrongObjectPtr<UClass> Interface,Parent,Generated,Skeleton;TStrongObjectPtr<UObject> GeneratedCDO,SkeletonCDO;
    TStrongObjectPtr<UBlueprintEditorSettings> SavePolicy;TArray<TStrongObjectPtr<UObject>> Holds;
    FString BlueprintPath,GeneratedPath,SkeletonPath,TreePath,RootPath,Filename,FileHash;UClass* GeneratedMeta=nullptr;UClass* SkeletonMeta=nullptr;
    bool Stopped=false,Ready=false,Rebinding=false,Writer=false,WasImplemented=false;int32 ExpectedInterfaces=0;
    TMap<FName,FGuid> WidgetGuids;TSharedRef<FJsonObject> SourceBefore=MakeShared<FJsonObject>();
    struct FClassRole{TStrongObjectPtr<UClass> C;UClass* Super;UObject* Outer;UPackage* Package;FString Path;EClassFlags Flags;TStrongObjectPtr<UObject> CDO;};TArray<FClassRole> Sources;
    struct FField{FProperty* P;FFieldClass* Class;FName Name;UStruct* Owner;uint64 Flags;int32 Size,Offset,Dim;UObject* Binding;FProperty* Underlying;};
    struct FFunctionRole{TStrongObjectPtr<UFunction> F;UClass* Owner;FName Name;EFunctionFlags Flags;TArray<FField> Fields;};TArray<FFunctionRole> Functions;
    struct FPinRole{UEdGraphPin* P;FGuid Id;FName Name;EEdGraphPinDirection Direction;FEdGraphPinType Type;FString Raw,Auto,Display;FText Text;uint32 TextFlags;UObject* Default;};
    struct FNodeRole{TStrongObjectPtr<UK2Node_Event> N;FGuid Id;FString Name,Comment;int32 X,Y;FMemberReference Reference;TArray<FPinRole> Pins;};TArray<FNodeRole> Nodes,CandidateNodes;
    TStrongObjectPtr<UEdGraph> Graph;FGuid GraphGuid;FString GraphPath;TArray<TObjectPtr<UEdGraph>> BeforeUber;
    explicit FWidgetInterfaceWork(FResult& In):R(In){}
    static void Count(FResult& Result,const TCHAR* Key){double Value=0;Result.Data->TryGetNumberField(Key,Value);Result.Data->SetNumberField(Key,Value+1);}
    bool Stop(const TCHAR* Stage,const TCHAR* Code)
    {
        if(!Stopped){R.Stage=Stage;R.ErrorCode=Code;R.Error=TEXT("Current Widget/interface role or returned observation failed; actual prefix retained; no retry, rollback or further SDK.");auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),Stage);J->SetStringField(TEXT("code"),Code);J->SetBoolField(TEXT("individual_operands_observed"),false);R.Data->SetObjectField(TEXT("first_current_failure"),J);}Stopped=true;R.bChangeStateKnown=false;return false;
    }
    bool Reject(const TCHAR* Code,const FString& Why){return Fail(R,Code,Why);}
    static bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
    static bool Reference(const FString& S)
    {
        if(S.IsEmpty()||S.Len()>=NAME_SIZE)return false;for(TCHAR C:S)if(C<32||C==127)return false;
        for(const TCHAR* Bad:{TEXT("UnrealMCP"),TEXT("MCPUnreal"),TEXT("MCP_Unreal"),TEXT("mcp_unreal"),TEXT("CubelessMCP"),TEXT("CubelessControlRigTools")})if(S.Contains(Bad,ESearchCase::IgnoreCase))return false;return true;
    }
    bool Context()
    {
        return !Stopped&&IsInGameThread()&&Live(Editor.Get())&&GEditor==Editor.Get()&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&!GEditor->PlayWorld&&!GEditor->GetPIEWorldContext()&&Live(World.Get())&&GEditor->GetEditorWorldContext().World()==World.Get()&&World->WorldType==EWorldType::Editor&&!World->bIsTearingDown;
    }
    bool FieldRows(UFunction* F,TArray<FField>& Rows)
    {
        for(TFieldIterator<FProperty> It(F,EFieldIterationFlags::None);It;++It)
        {
            FProperty* P=*It;if(Rows.Num()>=64||P->GetOwnerStruct()!=F||P->GetOwnerProperty()!=P||P->HasSetterOrGetter())return false;UObject* Binding=nullptr;FProperty* Underlying=nullptr;
            if(auto* O=CastField<FObjectPropertyBase>(P)){if(!O->PropertyClass.IsResolved())return false;Binding=O->PropertyClass.Get();}
            else if(auto* S=CastField<FStructProperty>(P)){if(!S->Struct.IsResolved())return false;Binding=S->Struct.Get();}
            else if(auto* E=CastField<FEnumProperty>(P)){Binding=E->GetEnum();Underlying=E->GetUnderlyingProperty();}
            else if(auto* B=CastField<FByteProperty>(P)){if(!B->Enum.IsResolved())return false;Binding=B->Enum.Get();}
            if(Binding){if(!Live(Binding))return false;Holds.Emplace(Binding);}Rows.Add({P,P->GetClass(),P->GetFName(),F,uint64(P->GetPropertyFlags()),P->GetSize(),P->GetOffset_ForInternal(),P->ArrayDim,Binding,Underlying});
        }return true;
    }
    bool FieldsCurrent(const FFunctionRole& Role)
    {
        TArray<FProperty*> Actual;for(TFieldIterator<FProperty> It(Role.F.Get(),EFieldIterationFlags::None);It;++It){if(Actual.Num()>=64)return false;Actual.Add(*It);}if(Actual.Num()!=Role.Fields.Num())return false;
        for(int32 I=0;I<Actual.Num();++I)
        {
            const auto& E=Role.Fields[I];if(Actual[I]!=E.P)return false;auto* P=E.P;if(P->GetClass()!=E.Class||P->GetFName()!=E.Name||P->GetOwnerStruct()!=E.Owner||P->GetOwnerProperty()!=P||uint64(P->GetPropertyFlags())!=E.Flags||P->GetSize()!=E.Size||P->GetOffset_ForInternal()!=E.Offset||P->ArrayDim!=E.Dim||P->HasSetterOrGetter())return false;
            if(auto* O=CastField<FObjectPropertyBase>(P)){if(!O->PropertyClass.IsResolved()||O->PropertyClass.Get()!=E.Binding)return false;}else if(auto* S=CastField<FStructProperty>(P)){if(!S->Struct.IsResolved()||S->Struct.Get()!=E.Binding)return false;}else if(auto* Enum=CastField<FEnumProperty>(P)){if(Enum->GetEnum()!=E.Binding||Enum->GetUnderlyingProperty()!=E.Underlying)return false;}else if(auto* B=CastField<FByteProperty>(P)){if(!B->Enum.IsResolved()||B->Enum.Get()!=E.Binding)return false;}
        }return true;
    }
    bool ClassRole(UClass* C,UClass* Meta,const FString& Path)
    {
        return Live(C)&&C->GetClass()==Meta&&C->GetOuter()==Package.Get()&&C->GetSuperClass()==Parent.Get()&&C->ClassGeneratedBy.IsResolved()&&C->ClassGeneratedBy.Get()==BP.Get()&&C->GetPathName().Equals(Path,ESearchCase::CaseSensitive)&&!C->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists)&&C->ClassDefaultObject.IsResolved();
    }
    bool CDORole(UClass* C,UObject* Object)
    {
        return !Object||(Live(Object)&&Object->GetClass()==C&&Object->GetOuter()==C->GetOuter()&&Object->GetPackage()==Package.Get()&&Object->HasAnyFlags(RF_ClassDefaultObject)&&Object->GetFName()==FName(*(TEXT("Default__")+C->GetName())));
    }
    bool Current(const TCHAR* Stage)
    {
        if(!Context())return Stop(Stage,TEXT("interface_context_changed"));
        for(const auto& S:Sources){auto* C=S.C.Get();if(!Live(C)||C->GetSuperClass()!=S.Super||C->GetOuter()!=S.Outer||C->GetPackage()!=S.Package||C->GetClassFlags()!=S.Flags||!C->GetPathName().Equals(S.Path,ESearchCase::CaseSensitive)||!C->ClassDefaultObject.IsResolved()||C->ClassDefaultObject.Get()!=S.CDO.Get()||(S.CDO.IsValid()&&(!Live(S.CDO.Get())||S.CDO->GetClass()!=C)))return Stop(Stage,TEXT("interface_native_source_changed"));}
        TArray<UFunction*> ActualFunctions;for(TFieldIterator<UFunction> It(Interface.Get(),EFieldIterationFlags::IncludeSuper);It;++It){if(ActualFunctions.Num()>=32)return Stop(Stage,TEXT("interface_signature_changed"));ActualFunctions.Add(*It);}if(ActualFunctions.Num()!=Functions.Num())return Stop(Stage,TEXT("interface_signature_changed"));
        for(int32 I=0;I<Functions.Num();++I){const auto& F=Functions[I];if(ActualFunctions[I]!=F.F.Get()||!Live(F.F.Get())||F.F->GetOuter()!=F.Owner||F.F->GetFName()!=F.Name||F.F->FunctionFlags!=F.Flags||!FieldsCurrent(F))return Stop(Stage,TEXT("interface_signature_changed"));}
        auto* B=BP.Get();if(!Live(B)||B->GetClass()!=UWidgetBlueprint::StaticClass()||B->GetOuter()!=Package.Get()||!B->GetPathName().Equals(BlueprintPath,ESearchCase::CaseSensitive)||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||B->bSuppressStructurallyModified||B->BlueprintType!=BPTYPE_Normal||!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.GetGCPtr().Get()!=Parent.Get()||!B->NewVariables.IsEmpty()||!B->FunctionGraphs.IsEmpty()||!B->MacroGraphs.IsEmpty()||!B->DelegateSignatureGraphs.IsEmpty()||B->UbergraphPages!=BeforeUber||!B->Bindings.IsEmpty()||!B->Animations.IsEmpty()||!B->WidgetVariableNameToGuidMap.OrderIndependentCompareEqual(WidgetGuids))return Stop(Stage,TEXT("interface_target_authored_changed"));
        if(!Writer&&(Package->IsDirty()||B->Status!=BS_UpToDate))return Stop(Stage,TEXT("interface_preexisting_target_changed"));
        if(B->ImplementedInterfaces.Num()!=ExpectedInterfaces)return Stop(Stage,TEXT("interface_description_changed"));
        if(ExpectedInterfaces){auto& D=B->ImplementedInterfaces[0];if(!D.Interface.GetGCPtr().IsResolved()||D.Interface.GetGCPtr().Get()!=Interface.Get()||!D.Graphs.IsEmpty())return Stop(Stage,TEXT("interface_description_changed"));}
        if(!Live(Tree.Get())||!B->WidgetTree.IsResolved()||B->WidgetTree.Get()!=Tree.Get()||Tree->GetOuter()!=B||!Tree->GetPathName().Equals(TreePath,ESearchCase::CaseSensitive)||!Tree->RootWidget.IsResolved()||Tree->RootWidget.Get()!=Root.Get()||!Live(Root.Get())||Root->GetClass()!=UCanvasPanel::StaticClass()||Root->GetOuter()!=Tree.Get()||!Root->GetPathName().Equals(RootPath,ESearchCase::CaseSensitive)||Root->GetChildrenCount()!=0||!Root->Slot.IsResolved()||Root->Slot.Get()!=nullptr)return Stop(Stage,TEXT("interface_widget_tree_changed"));
        if(Graph.IsValid())
        {
            if(!Live(Graph.Get())||Graph->GetOuter()!=B||Graph->GetClass()!=UEdGraph::StaticClass()||Graph->GraphGuid!=GraphGuid||!Graph->GetPathName().Equals(GraphPath,ESearchCase::CaseSensitive)||!Graph->Schema.GetGCPtr().IsResolved()||Graph->Schema.GetGCPtr().Get()!=UEdGraphSchema_K2::StaticClass()||!Graph->SubGraphs.IsEmpty()||Graph->Nodes.Num()!=Nodes.Num())return Stop(Stage,TEXT("interface_author_graph_changed"));
            for(int32 I=0;I<Nodes.Num();++I)if(!Graph->Nodes[I].IsResolved()||Graph->Nodes[I].Get()!=Nodes[I].N.Get())return Stop(Stage,TEXT("interface_author_nodes_changed"));
        }
        const auto& Roles=Rebinding?CandidateNodes:Nodes;
        for(const auto& E:Roles)
        {
            auto* N=E.N.Get();if(!Live(N)||N->GetClass()!=UK2Node_Event::StaticClass()||N->GetOuter()!=Graph.Get()||N->NodeGuid!=E.Id||!N->GetName().Equals(E.Name,ESearchCase::CaseSensitive)||!N->NodeComment.Equals(E.Comment,ESearchCase::CaseSensitive)||N->NodePosX!=E.X||N->NodePosY!=E.Y||!N->EventReference.IsSameReference(E.Reference)||N->Pins.Num()!=E.Pins.Num())return Stop(Stage,TEXT("interface_author_node_changed"));
            for(int32 I=0;I<E.Pins.Num();++I){const auto& P=E.Pins[I];const auto* A=N->Pins[I];if(A!=P.P||!A||A->WasTrashed()||A->GetOwningNodeUnchecked()!=N||A->PinId!=P.Id||A->PinName!=P.Name||A->Direction!=P.Direction||A->PinType.PinSubCategoryObject.IsStale()||A->PinType.PinValueType.TerminalSubCategoryObject.IsStale()||!SameType(A->PinType,P.Type)||!A->DefaultValue.Equals(P.Raw,ESearchCase::CaseSensitive)||!A->AutogeneratedDefaultValue.Equals(P.Auto,ESearchCase::CaseSensitive)||!A->DefaultObject.IsResolved()||A->DefaultObject.Get()!=P.Default||!A->LinkedTo.IsEmpty()||FTextInspector::GetFlags(A->DefaultTextValue)!=P.TextFlags||FTextInspector::GetSharedDataId(A->DefaultTextValue)!=FTextInspector::GetSharedDataId(P.Text))return Stop(Stage,TEXT("interface_author_pin_changed"));}
        }
        if(!Rebinding)
        {
            if(!B->GeneratedClass.GetGCPtr().IsResolved()||B->GeneratedClass.GetGCPtr().Get()!=Generated.Get()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||B->SkeletonGeneratedClass.GetGCPtr().Get()!=Skeleton.Get()||!ClassRole(Generated.Get(),GeneratedMeta,GeneratedPath)||!ClassRole(Skeleton.Get(),SkeletonMeta,SkeletonPath)||Generated->ClassDefaultObject.Get()!=GeneratedCDO.Get()||Skeleton->ClassDefaultObject.Get()!=SkeletonCDO.Get()||!CDORole(Generated.Get(),GeneratedCDO.Get())||!CDORole(Skeleton.Get(),SkeletonCDO.Get()))return Stop(Stage,TEXT("interface_compiled_role_changed"));
        }
        if(SavePolicy.IsValid()&&(!Live(SavePolicy.Get())||SavePolicy->GetClass()!=UBlueprintEditorSettings::StaticClass()||!UBlueprintEditorSettings::StaticClass()->ClassDefaultObject.IsResolved()||UBlueprintEditorSettings::StaticClass()->ClassDefaultObject.Get()!=SavePolicy.Get()||SavePolicy->SaveOnCompile!=SoC_Never))return Stop(Stage,TEXT("interface_save_policy_changed"));return true;
    }
    bool Pins(UK2Node_Event* N,TArray<FPinRole>& Out)
    {
        if(N->Pins.Num()>32)return false;for(UEdGraphPin* P:N->Pins){if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->DefaultObject.IsResolved()||P->PinType.PinSubCategoryObject.IsStale()||P->PinType.PinValueType.TerminalSubCategoryObject.IsStale()||!P->LinkedTo.IsEmpty())return false;if(P->PinType.PinSubCategoryObject.Get())Holds.Emplace(P->PinType.PinSubCategoryObject.Get());if(P->PinType.PinValueType.TerminalSubCategoryObject.Get())Holds.Emplace(P->PinType.PinValueType.TerminalSubCategoryObject.Get());if(P->DefaultObject.Get())Holds.Emplace(P->DefaultObject.Get());Out.Add({P,P->PinId,P->PinName,P->Direction,P->PinType,P->DefaultValue,P->AutogeneratedDefaultValue,FString(),P->DefaultTextValue,FTextInspector::GetFlags(P->DefaultTextValue),P->DefaultObject.Get()});}return true;
    }
    bool Reacquire(const TCHAR* Stage)
    {
        // No old compiled class/CDO/pin field is read after the structural call.
        auto Gen=BP->GeneratedClass,Skel=BP->SkeletonGeneratedClass;if(!Gen.GetGCPtr().IsResolved()||!Skel.GetGCPtr().IsResolved())return Stop(Stage,TEXT("interface_compiled_slot_unresolved"));TStrongObjectPtr<UClass> NewGen(Gen.GetGCPtr().Get()),NewSkel(Skel.GetGCPtr().Get());
        if(!Current(Stage)||!ClassRole(NewGen.Get(),GeneratedMeta,GeneratedPath)||!ClassRole(NewSkel.Get(),SkeletonMeta,SkeletonPath))return Stop(Stage,TEXT("interface_compiled_reacquire_failed"));
        TStrongObjectPtr<UObject> NewGenCDO(NewGen->ClassDefaultObject.Get()),NewSkelCDO(NewSkel->ClassDefaultObject.Get());if(!CDORole(NewGen.Get(),NewGenCDO.Get())||!CDORole(NewSkel.Get(),NewSkelCDO.Get()))return Stop(Stage,TEXT("interface_reacquired_cdo_role_invalid"));
        CandidateNodes.Reset();for(const auto& E:Nodes){FNodeRole New;New.N.Reset(E.N.Get());New.Id=E.Id;New.Name=E.Name;New.Comment=E.Comment;New.X=E.X;New.Y=E.Y;New.Reference=E.Reference;if(!Pins(New.N.Get(),New.Pins))return Stop(Stage,TEXT("interface_pin_reacquire_failed"));CandidateNodes.Add(MoveTemp(New));}
        if(!Current(Stage))return false;
        for(int32 I=0;I<Nodes.Num();++I)
        {
            const auto& Old=Nodes[I];auto& New=CandidateNodes[I];if(New.Pins.Num()!=Old.Pins.Num())return Stop(Stage,TEXT("interface_logical_pin_changed"));
            for(int32 K=0;K<Old.Pins.Num();++K){const auto& A=Old.Pins[K];auto& B=New.Pins[K];if(A.Id!=B.Id||A.Name!=B.Name||A.Direction!=B.Direction||!SameType(A.Type,B.Type)||!A.Raw.Equals(B.Raw,ESearchCase::CaseSensitive)||!A.Auto.Equals(B.Auto,ESearchCase::CaseSensitive)||A.Default!=B.Default||A.TextFlags!=B.TextFlags)return Stop(Stage,TEXT("interface_logical_pin_changed"));const FText Copy=B.Text;const FString Display=Copy.ToString();if(!Current(Stage))return false;if(!Display.Equals(A.Display,ESearchCase::CaseSensitive))return Stop(Stage,TEXT("interface_logical_text_changed"));B.Display=A.Display;}
        }
        Generated.Reset(NewGen.Get());Skeleton.Reset(NewSkel.Get());GeneratedCDO.Reset(NewGenCDO.Get());SkeletonCDO.Reset(NewSkelCDO.Get());Nodes=MoveTemp(CandidateNodes);Rebinding=false;return Current(Stage);
    }
    bool File(const TCHAR* Stage,FString& Out)
    {
        if(!Current(Stage))return false;const int64 Size=IFileManager::Get().FileSize(*Filename);if(Size<=0||Size>64*1024*1024)return Stop(Stage,TEXT("interface_target_file_unavailable"));TArray<uint8> Bytes;const bool Read=FFileHelper::LoadFileToArray(Bytes,*Filename);if(!Current(Stage)||!Read||Bytes.Num()!=Size)return Stop(Stage,TEXT("interface_target_file_changed"));FSHAHash H;FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),H.Hash);Out=H.ToString();return true;
    }
    bool SourceSnapshot(TSharedRef<FJsonObject> Out,const TCHAR* Stage)
    {
        if(!Current(Stage))return false;const bool Implementable=FKismetEditorUtilities::IsClassABlueprintImplementableInterface(Interface.Get());if(!Current(Stage))return false;const bool Can=FKismetEditorUtilities::CanBlueprintImplementInterface(BP.Get(),Interface.Get());if(!Current(Stage))return false;
        Out->SetBoolField(TEXT("blueprint_implementable"),Implementable);Out->SetBoolField(TEXT("can_blueprint_implement"),Can);TArray<TSharedPtr<FJsonValue>> Rows;
        for(const auto& E:Functions)
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("function"),E.F->GetPathName());J->SetStringField(TEXT("owner"),E.Owner->GetPathName());J->SetNumberField(TEXT("flags"),double(uint32(E.Flags)));
            const bool Override=UEdGraphSchema_K2::CanKismetOverrideFunction(E.F.Get());if(!Current(Stage))return false;const bool Event=UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(E.F.Get());if(!Current(Stage))return false;
            const bool Anim=E.F->HasMetaData(FBlueprintMetadata::MD_AnimBlueprintFunction);if(!Current(Stage))return false;J->SetBoolField(TEXT("can_override"),Override);J->SetBoolField(TEXT("can_place_as_event"),Event);J->SetBoolField(TEXT("anim_function"),Anim);J->SetBoolField(TEXT("requires_interface_graph"),(Override&&!Event)||Anim);Rows.Add(MakeShared<FJsonValueObject>(J));
        }Out->SetArrayField(TEXT("functions"),Rows);return true;
    }
    bool Bind(const FString& Path,const FString& RequestedInterface)
    {
        R.Data->SetBoolField(TEXT("native_widget_interface"),true);R.Data->SetStringField(TEXT("requested_interface"),RequestedInterface);R.Data->SetNumberField(TEXT("explicit_compile_calls"),0);R.Data->SetNumberField(TEXT("explicit_save_calls"),0);R.Data->SetBoolField(TEXT("automatic_retry_performed"),false);
        if(!Reference(Path)||!Reference(RequestedInterface))return Reject(TEXT("interface_dependency_reference_rejected"),TEXT("Bounded non-MCP source/target references required before lookup or writer."));
        if(!RequestedInterface.Equals(TEXT("/Script/UMG.UserObjectListEntry"),ESearchCase::IgnoreCase))return Reject(TEXT("interface_source_scope_pending"),TEXT("First generic slice admits loaded native UMG.UserObjectListEntry. Broader native/Game BPI interfaces and aliases remain incomplete; Niagara exact route is separate."));
        if(!Path.StartsWith(TEXT("/Game/")))return Reject(TEXT("interface_target_path_pending"),TEXT("Exact loaded Game Widget Blueprint package/object path required; no loader or simple-name search in this slice."));
        const FString ObjectPath=Path.Contains(TEXT("."))?Path:Path+TEXT(".")+FPackageName::GetLongPackageAssetName(Path);
        Editor.Reset(GEditor);if(!IsInGameThread()||!Live(Editor.Get())||IsPlaySessionActive()||IsGarbageCollecting()||UE::IsSavingPackage())return Reject(TEXT("interface_editor_invalid"),TEXT("Idle Editor game thread required."));World.Reset(GEditor->GetEditorWorldContext().World());if(!Context())return Reject(TEXT("interface_editor_invalid"),TEXT("Stable idle Editor world required."));
        BP.Reset(FindObject<UWidgetBlueprint>(nullptr,*ObjectPath));Interface.Reset(FindObject<UClass>(nullptr,*RequestedInterface));
        if(!Live(BP.Get())||BP->GetClass()!=UWidgetBlueprint::StaticClass()||!Live(Interface.Get())||!Interface->HasAllClassFlags(CLASS_Native|CLASS_Interface)||Interface->GetPathName()!=TEXT("/Script/UMG.UserObjectListEntry"))return Reject(TEXT("interface_loaded_source_unavailable"),TEXT("Already-loaded exact native interface and ordinary Widget Blueprint required; no forced UMG module/asset load."));
        auto* B=BP.Get();Package.Reset(B->GetPackage());BlueprintPath=B->GetPathName();
        if(!Live(Package.Get())||Package->IsDirty()||B->Status!=BS_UpToDate||B->GetOuter()!=Package.Get()||B->BlueprintType!=BPTYPE_Normal||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||B->bSuppressStructurallyModified||!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.GetGCPtr().Get()!=UUserWidget::StaticClass()||!B->GeneratedClass.GetGCPtr().IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||!B->WidgetTree.IsResolved()||!B->NewVariables.IsEmpty()||!B->FunctionGraphs.IsEmpty()||!B->MacroGraphs.IsEmpty()||!B->DelegateSignatureGraphs.IsEmpty()||B->UbergraphPages.Num()>1||!B->Bindings.IsEmpty()||!B->Animations.IsEmpty())return Reject(TEXT("interface_fresh_widget_scope_pending"),TEXT("Saved-clean UpToDate direct UserWidget Blueprint with no own variables/functions/macros/delegates/animations/bindings required. Wider original owners remain incomplete."));
        Parent.Reset(B->ParentClass.GetGCPtr().Get());Generated.Reset(B->GeneratedClass.GetGCPtr().Get());Skeleton.Reset(B->SkeletonGeneratedClass.GetGCPtr().Get());Tree.Reset(B->WidgetTree.Get());
        if(!Live(Generated.Get())||!Live(Skeleton.Get())||!Generated->GetClass()->IsChildOf(UBlueprintGeneratedClass::StaticClass())||!Skeleton->GetClass()->IsChildOf(UBlueprintGeneratedClass::StaticClass())||!Live(Tree.Get())||Tree->GetOuter()!=B||!Tree->RootWidget.IsResolved())return Reject(TEXT("interface_widget_roles_unavailable"),TEXT("Current generated/skeleton/source WidgetTree roles required."));
        Root.Reset(Cast<UCanvasPanel>(Tree->RootWidget.Get()));if(!Live(Root.Get())||Root->GetClass()!=UCanvasPanel::StaticClass()||Root->GetChildrenCount()!=0||!Root->Slot.IsResolved()||Root->Slot.Get()!=nullptr)return Reject(TEXT("interface_fresh_widget_tree_pending"),TEXT("First fresh source WidgetTree has one exact native empty Canvas root. Broader authored trees remain incomplete."));
        TreePath=Tree->GetPathName();RootPath=Root->GetPathName();GeneratedPath=Generated->GetPathName();SkeletonPath=Skeleton->GetPathName();GeneratedMeta=Generated->GetClass();SkeletonMeta=Skeleton->GetClass();Holds.Emplace(GeneratedMeta);Holds.Emplace(SkeletonMeta);WidgetGuids=B->WidgetVariableNameToGuidMap;BeforeUber=B->UbergraphPages;
        if(!ClassRole(Generated.Get(),GeneratedMeta,GeneratedPath)||!ClassRole(Skeleton.Get(),SkeletonMeta,SkeletonPath))return Reject(TEXT("interface_compiled_roles_unavailable"),TEXT("Generated/skeleton actual class/owner/path/parent/source binding required."));GeneratedCDO.Reset(Generated->ClassDefaultObject.Get());SkeletonCDO.Reset(Skeleton->ClassDefaultObject.Get());if(!CDORole(Generated.Get(),GeneratedCDO.Get())||!CDORole(Skeleton.Get(),SkeletonCDO.Get()))return Reject(TEXT("interface_initial_cdo_role_invalid"),TEXT("Existing nullable CDO must have its actual class/outer/package/default-object role; no forced CDO construction."));
        if(B->ImplementedInterfaces.Num()>1)return Reject(TEXT("interface_existing_descriptions_pending"),TEXT("First profile has no prior interface or the selected same event interface only."));
        ExpectedInterfaces=B->ImplementedInterfaces.Num();WasImplemented=ExpectedInterfaces==1;if(WasImplemented){auto& D=B->ImplementedInterfaces[0];if(!D.Interface.GetGCPtr().IsResolved()||D.Interface.GetGCPtr().Get()!=Interface.Get()||!D.Graphs.IsEmpty())return Reject(TEXT("interface_existing_description_pending"),TEXT("Existing selected native event interface description must be exact and have no interface-function graphs."));}
        for(UClass* C=Interface.Get();C;C=C->GetSuperClass())
        {
            if(Sources.Num()>=4||!Live(C)||!C->HasAnyClassFlags(CLASS_Native)||!C->ClassDefaultObject.IsResolved())return Reject(TEXT("interface_native_hierarchy_pending"),TEXT("Finite already-resolved native interface hierarchy required."));
            FClassRole E;E.C.Reset(C);E.Super=C->GetSuperClass();E.Outer=C->GetOuter();E.Package=C->GetPackage();E.Path=C->GetPathName();E.Flags=C->GetClassFlags();E.CDO.Reset(C->ClassDefaultObject.Get());if(E.CDO.IsValid()&&(!Live(E.CDO.Get())||E.CDO->GetClass()!=C))return Reject(TEXT("interface_native_cdo_unavailable"),TEXT("Existing native CDO role unavailable; no CDO creation."));Sources.Add(MoveTemp(E));
        }
        for(TFieldIterator<UFunction> It(Interface.Get(),EFieldIterationFlags::IncludeSuper);It;++It)
        {
            UFunction* F=*It;if(Functions.Num()>=32||!Live(F))return Reject(TEXT("interface_function_inventory_pending"),TEXT("Finite live interface function inventory required."));FFunctionRole E;E.F.Reset(F);E.Owner=F->GetOuterUClass();E.Name=F->GetFName();E.Flags=F->FunctionFlags;if(!Sources.ContainsByPredicate([&](const FClassRole& C){return C.C.Get()==E.Owner;})||!FieldRows(F,E.Fields))return Reject(TEXT("interface_function_layout_pending"),TEXT("Direct native signature inventory/layout required before SDK metadata readers."));Functions.Add(MoveTemp(E));
        }
        if(Functions.IsEmpty())return Reject(TEXT("interface_no_observed_events"),TEXT("Expected public native event functions were not observed; no guessed signature constants."));
        if(!BeforeUber.IsEmpty())
        {
            if(!BeforeUber[0].IsResolved())return Reject(TEXT("interface_graph_unresolved"),TEXT("Current source graph slot required."));Graph.Reset(BeforeUber[0].Get());if(!Live(Graph.Get())||Graph->GetClass()!=UEdGraph::StaticClass()||Graph->GetOuter()!=B||!Graph->SubGraphs.IsEmpty()||Graph->Nodes.Num()>16)return Reject(TEXT("interface_source_graph_pending"),TEXT("Bounded direct source EventGraph without nested graphs required."));GraphGuid=Graph->GraphGuid;GraphPath=Graph->GetPathName();
            for(const auto& H:Graph->Nodes)
            {
                if(!H.IsResolved()||!Live(H.Get())||H->GetClass()!=UK2Node_Event::StaticClass()||H->GetOuter()!=Graph.Get())return Reject(TEXT("interface_source_nodes_pending"),TEXT("First fresh Widget graph admits existing unlinked stock default Event nodes only."));auto* N=static_cast<UK2Node_Event*>(H.Get());FNodeRole E;E.N.Reset(N);E.Id=N->NodeGuid;E.Name=N->GetName();E.Comment=N->NodeComment;E.X=N->NodePosX;E.Y=N->NodePosY;E.Reference=N->EventReference;if(!Pins(N,E.Pins))return Reject(TEXT("interface_source_pins_pending"),TEXT("Bounded resolved unlinked source pins required."));Nodes.Add(MoveTemp(E));
            }
        }
        SavePolicy.Reset(FindObject<UBlueprintEditorSettings>(nullptr,TEXT("/Script/BlueprintGraph.Default__BlueprintEditorSettings")));if(!Live(SavePolicy.Get())||SavePolicy->SaveOnCompile!=SoC_Never)return Reject(TEXT("interface_save_policy_unsafe"),TEXT("Existing BlueprintEditorSettings SaveOnCompile=Never required; settings are not changed."));
        if(!Current(TEXT("bind_roles")))return false;
        for(auto& E:Nodes)for(auto& P:E.Pins){const FText Copy=P.Text;const FString Display=Copy.ToString();if(!Current(TEXT("bind_pin_text")))return false;P.Display=Display;}
        for(TObjectIterator<UUserWidget> It;It;++It)if(Live(*It)&&!It->IsTemplate()&&It->IsA(Generated.Get()))
        {
            TStrongObjectPtr<UUserWidget> Observed(*It);TStrongObjectPtr<UObject> Outer(Observed->GetOuter());TStrongObjectPtr<UPackage> ObjectPackage(Observed->GetPackage());
            auto First=MakeShared<FJsonObject>();First->SetStringField(TEXT("object"),Observed->GetPathName());First->SetStringField(TEXT("class"),Observed->GetClass()->GetPathName());First->SetNumberField(TEXT("flags"),double(uint32(Observed->GetFlags())));First->SetBoolField(TEXT("is_template"),Observed->IsTemplate());First->SetStringField(TEXT("outer"),Live(Outer.Get())?Outer->GetPathName():TEXT("<unavailable>"));First->SetStringField(TEXT("package"),Live(ObjectPackage.Get())?ObjectPackage->GetPathName():TEXT("<unavailable>"));First->SetStringField(TEXT("property"),TEXT("loaded_non_template_widget_instance"));First->SetStringField(TEXT("expected"),TEXT("no external instance in first asset-only profile"));First->SetStringField(TEXT("stage"),TEXT("bind_external_instances"));
            R.Data->SetObjectField(TEXT("first_external_widget_instance"),First);
            return Reject(TEXT("interface_external_widget_instance_pending"),TEXT("Loaded widget/preview instances are outside first asset-only callback scope."));
        }
        for(TObjectIterator<UBlueprintGeneratedClass> It;It;++It)if(Live(*It)&&*It!=Generated.Get()&&*It!=Skeleton.Get()&&It->IsChildOf(Generated.Get()))return Reject(TEXT("interface_derived_class_pending"),TEXT("Loaded derived class propagation remains incomplete."));
        Filename=FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension());if(!File(TEXT("bind_file"),FileHash)||!SourceSnapshot(SourceBefore,TEXT("source_preflight")))return false;R.Data->SetObjectField(TEXT("interface_source_before"),SourceBefore);
        if(!SourceBefore->GetBoolField(TEXT("blueprint_implementable"))||!SourceBefore->GetBoolField(TEXT("can_blueprint_implement")))return Reject(TEXT("interface_public_eligibility_rejected"),TEXT("Actual public interface/target eligibility rejected before writer; metadata return retained."));
        for(const auto& V:SourceBefore->GetArrayField(TEXT("functions")))if(V->AsObject()->GetBoolField(TEXT("requires_interface_graph")))return Reject(TEXT("interface_function_graph_profile_pending"),TEXT("First native UMG event profile requires actual event-only functions; non-event/Anim interface graphs are not silently fabricated."));
        R.Data->SetStringField(TEXT("blueprint"),BlueprintPath);R.Data->SetStringField(TEXT("interface_class"),Interface->GetPathName());R.Data->SetBoolField(TEXT("already_implemented_before"),WasImplemented);R.Data->SetNumberField(TEXT("planned_interface_graphs"),0);R.Data->SetStringField(TEXT("target_file_sha1_before"),FileHash);Ready=true;return Current(TEXT("prewriter"));
    }
    FResult Run(const FString& RequestedPath)
    {
        if(!Ready||!Current(TEXT("before_modify")))return R;Writer=true;R.bMutationAttempted=true;R.bChangeStateKnown=false;R.Stage=TEXT("modify_blueprint");Count(R,TEXT("blueprint_modify_calls"));BP->Modify();if(!Current(TEXT("after_modify")))return R;
        R.Stage=TEXT("implement_interface");Rebinding=true;CandidateNodes.Reset();Count(R,TEXT("implement_interface_calls"));const bool EngineReturn=FBlueprintEditorUtils::ImplementNewInterface(BP.Get(),Interface->GetClassPathName());R.Data->SetBoolField(TEXT("engine_return"),EngineReturn);R.Data->SetBoolField(TEXT("implement_returned"),true);R.Data->SetNumberField(TEXT("sdk_requested_structural_compiles"),EngineReturn?1:0);
        ExpectedInterfaces=(WasImplemented||EngineReturn)?1:0;if(!Reacquire(TEXT("after_implement_interface")))return R;
        // Original handler calls this even when an interface was already present.
        // Preserve returned false/actual dirty state instead of inventing a no-op.
        Rebinding=true;CandidateNodes.Reset();Count(R,TEXT("mark_structural_calls"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP.Get());Count(R,TEXT("mark_structural_returns"));R.Data->SetBoolField(TEXT("implicit_skeleton_compile_requested"),true);
        if(!Reacquire(TEXT("after_explicit_structural")))return R;
        auto After=MakeShared<FJsonObject>();if(!SourceSnapshot(After,TEXT("source_after")))return R;R.Data->SetObjectField(TEXT("interface_source_after"),After);
        if(!Text(SourceBefore).Equals(Text(After),ESearchCase::CaseSensitive))return Stop(TEXT("source_after"),TEXT("interface_source_metadata_changed")),R;
        FString FinalHash;if(!File(TEXT("final_file"),FinalHash))return R;R.Data->SetStringField(TEXT("target_file_sha1_after"),FinalHash);if(!FinalHash.Equals(FileHash,ESearchCase::CaseSensitive))return Stop(TEXT("final_file"),TEXT("interface_target_file_changed")),R;
        const bool Present=ExpectedInterfaces==1;R.Data->SetBoolField(TEXT("interface_present_after"),Present);R.Data->SetBoolField(TEXT("source_observed_unchanged"),true);R.Data->SetBoolField(TEXT("authored_widget_graphs_preserved"),true);R.Data->SetBoolField(TEXT("dirty_after"),Package->IsDirty());R.Data->SetNumberField(TEXT("blueprint_status_after"),BP->Status.GetValue());R.Data->SetNumberField(TEXT("interface_graph_count_after"),Present?BP->ImplementedInterfaces[0].Graphs.Num():0);
        auto Legacy=MakeShared<FJsonObject>();Legacy->SetBoolField(TEXT("success"),true);Legacy->SetStringField(TEXT("blueprint_name"),RequestedPath);Legacy->SetStringField(TEXT("interface_class"),Interface->GetPathName());Legacy->SetStringField(TEXT("interface_class_path"),Interface->GetClassPathName().ToString());Legacy->SetBoolField(TEXT("added_or_already_present"),EngineReturn);Legacy->SetBoolField(TEXT("dirty_after"),Package->IsDirty());R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
        R.bChangeStateKnown=true;R.bChanged=Package->IsDirty()||(!WasImplemented&&Present);R.bSuccess=R.bPostconditionPassed=Present&&(WasImplemented?!EngineReturn:EngineReturn);R.Stage=R.bSuccess?TEXT("complete"):TEXT("postcondition_failed");if(!R.bSuccess)Fail(R,TEXT("interface_implementation_not_observed"),TEXT("Returned false/actual structural prefix retained; no success or retry inferred."));return R;
    }
};

// QG concrete saved Game-BPI -> direct Actor leaf. Existing Widget/Niagara implementations remain opt-out.
struct FGameInterfaceWork
{
    using FField=FWidgetInterfaceWork::FField;using FFunctionRole=FWidgetInterfaceWork::FFunctionRole;
    FResult& R;FWidgetInterfaceWork FieldReader;
    // WU shares QG's source/signature/graph plan; only the concrete target owner differs.
    bool WidgetTarget=false;UClass* TargetGeneratedMeta=nullptr;UClass* TargetSkeletonMeta=nullptr;
    TStrongObjectPtr<UWidgetTree> WidgetTree;TStrongObjectPtr<UCanvasPanel> WidgetRoot;
    FString WidgetTreePath,WidgetRootPath;TMap<FName,FGuid> WidgetGuids;
    // ACX reuses ACH authored/runtime WidgetTree observation only for populated Game-BPI owners.
    struct FTreeObject
    {
        TStrongObjectPtr<UObject> Object,Outer;TStrongObjectPtr<UClass> Class;
        FString Relative,Path,Values,RuntimeValues;
    };
    struct FTreeState{TArray<FTreeObject> Objects;FString Shape,Runtime;};
    FTreeState AuthoredTree;
    struct FTreeField{FProperty* Property;FFieldClass* Class;UStruct* Owner;FName Name;uint64 Flags;int32 Offset,Size,Dim;bool Accessor;UStruct* Type;uint32 BoolLayout;FProperty* Inner;FProperty* Value;FProperty* RootProperty;};
    TArray<FTreeField> TreeFields;
    bool TreeReady=false,RichTree=false;int32 RootChildren=0;
    static void TreeToken(FString& Out,const FString& Value){Out+=FString::FromInt(Value.Len())+TEXT(":")+Value;}
    bool TreeNative(UClass* C)
    {
        return Live(C)&&C->HasAnyClassFlags(CLASS_Native)&&C->ClassGeneratedBy.IsResolved()&&!C->ClassGeneratedBy.Get()
            &&Live(C->GetOuter())&&C->GetOuter()->GetPathName()==TEXT("/Script/UMG")&&!C->HasAnyClassFlags(CLASS_NewerVersionExists|CLASS_Deprecated);
    }
    bool TreeField(FProperty* P)
    {
        if(!P)return false;auto* K=P->GetClass();
        const bool Plain=K==FBoolProperty::StaticClass()||K==FByteProperty::StaticClass()||K==FInt8Property::StaticClass()||K==FInt16Property::StaticClass()||K==FUInt16Property::StaticClass()||K==FIntProperty::StaticClass()||K==FUInt32Property::StaticClass()||K==FInt64Property::StaticClass()||K==FUInt64Property::StaticClass()||K==FFloatProperty::StaticClass()||K==FDoubleProperty::StaticClass()||K==FNameProperty::StaticClass()||K==FStrProperty::StaticClass()||K==FTextProperty::StaticClass()||K==FObjectProperty::StaticClass()||K==FSoftObjectProperty::StaticClass()||K==FStructProperty::StaticClass()||K==FEnumProperty::StaticClass()||K==FArrayProperty::StaticClass()||K==FSetProperty::StaticClass()||K==FMapProperty::StaticClass()||K==FDelegateProperty::StaticClass()||K==FMulticastInlineDelegateProperty::StaticClass()||K==FMulticastSparseDelegateProperty::StaticClass();
        if(!Plain||P->ArrayDim<1||P->ArrayDim>256||P->GetSize()<1)return false;
        UStruct* Type=nullptr;uint32 BoolLayout=0;
        if(K==FStructProperty::StaticClass()){auto* SP=static_cast<FStructProperty*>(P);if(!SP->Struct.IsResolved()||!Live(SP->Struct.Get()))return false;Type=SP->Struct.Get();}
        if(K==FObjectProperty::StaticClass()){auto* OP=static_cast<FObjectProperty*>(P);if(!OP->PropertyClass.IsResolved()||!Live(OP->PropertyClass.Get()))return false;Type=OP->PropertyClass.Get();}
        if(K==FBoolProperty::StaticClass()){const auto* B=static_cast<FBoolProperty*>(P);BoolLayout=uint32(B->GetBoolFieldSize())|(uint32(B->GetByteOffset())<<8)|(uint32(B->GetByteMask())<<16)|(uint32(B->GetFieldMask())<<24);}
        FProperty* Inner=nullptr;FProperty* Value=nullptr;
        if(K==FArrayProperty::StaticClass())Inner=static_cast<FArrayProperty*>(P)->Inner;
        else if(K==FSetProperty::StaticClass())Inner=static_cast<FSetProperty*>(P)->ElementProp;
        else if(K==FMapProperty::StaticClass()){Inner=static_cast<FMapProperty*>(P)->KeyProp;Value=static_cast<FMapProperty*>(P)->ValueProp;}
        else if(K==FEnumProperty::StaticClass())Inner=static_cast<FEnumProperty*>(P)->GetUnderlyingProperty();
        if(const auto* E=TreeFields.FindByPredicate([&](const FTreeField& V){return V.Property==P;}))return E->Class==K&&E->Owner==P->GetOwnerStruct()&&E->Name==P->GetFName()&&E->Flags==uint64(P->GetPropertyFlags())&&E->Offset==P->GetOffset_ForInternal()&&E->Size==P->GetSize()&&E->Dim==P->ArrayDim&&E->Accessor==P->HasSetterOrGetter()&&E->Type==Type&&E->BoolLayout==BoolLayout&&E->Inner==Inner&&E->Value==Value&&E->RootProperty==P->GetOwnerProperty();
        if(TreeReady||TreeFields.Num()>=4096)return false;
        auto* Owner=P->GetOwnerStruct();if(!Live(Owner))return false;Holds.Emplace(Owner);if(Type)Holds.Emplace(Type);
        TreeFields.Add({P,K,Owner,P->GetFName(),uint64(P->GetPropertyFlags()),P->GetOffset_ForInternal(),P->GetSize(),P->ArrayDim,P->HasSetterOrGetter(),Type,BoolLayout,Inner,Value,P->GetOwnerProperty()});return true;
    }
    bool TreeReference(UObject* Value,UWidgetTree* InTree,FString& Out)
    {
        if(!Value){TreeToken(Out,TEXT("null"));return true;}if(!Live(Value))return false;
        if(Value==InTree||Value->IsIn(InTree)){TreeToken(Out,TEXT("tree:")+Value->GetPathName(InTree));return true;}
        const FString Ref=Value->GetPathName();if(!FWidgetInterfaceWork::Reference(Ref))return false;
        if(!TreeReady)Holds.Emplace(Value);else if(!Holds.ContainsByPredicate([&](const auto& H){return H.Get()==Value;}))return false;
        TreeToken(Out,TEXT("external:")+Ref);TreeToken(Out,Value->GetClass()->GetPathName());return true;
    }
    bool CompleteNumericPOD(UScriptStruct* S,int32 Depth)
    {
        if(Depth>16||!Live(S)||S->GetClass()!=UScriptStruct::StaticClass()||!S->GetOutermost()->GetName().StartsWith(TEXT("/Script/"))||!FWidgetInterfaceWork::Reference(S->GetPathName()))return false;
        const uint32 Required=uint32(STRUCT_IsPlainOldData)|uint32(STRUCT_NoDestructor);
        const int32 Size=S->GetStructureSize();if((uint32(S->StructFlags)&Required)!=Required||Size<=0||Size>65536)return false;
        TArray<uint8> Covered;Covered.Init(0,Size);int32 Count=0;
        for(TFieldIterator<FProperty> It(S,EFieldIterationFlags::IncludeSuper|EFieldIterationFlags::IncludeDeprecated);It;++It)
        {
            FProperty* Q=*It;if(++Count>256||!TreeField(Q)||Q->HasAnyPropertyFlags(CPF_Transient|CPF_DuplicateTransient|CPF_NonPIEDuplicateTransient|CPF_SkipSerialization|CPF_EditorOnly))return false;
            const int32 Offset=Q->GetOffset_ForInternal(),Span=Q->GetSize();if(Offset<0||Span<=0||int64(Offset)+Span>Size||int64(Q->ElementSize)*Q->ArrayDim!=Span)return false;
            auto* K=Q->GetClass();
            const bool Numeric=K==FByteProperty::StaticClass()||K==FInt8Property::StaticClass()||K==FInt16Property::StaticClass()||K==FUInt16Property::StaticClass()||K==FIntProperty::StaticClass()||K==FUInt32Property::StaticClass()||K==FInt64Property::StaticClass()||K==FUInt64Property::StaticClass()||K==FFloatProperty::StaticClass()||K==FDoubleProperty::StaticClass();
            if(!Numeric)
            {
                if(K!=FStructProperty::StaticClass())return false;auto* Nested=static_cast<FStructProperty*>(Q);if(!Nested->Struct.IsResolved()||!Live(Nested->Struct.Get())||Nested->Struct->GetStructureSize()!=Q->ElementSize||!CompleteNumericPOD(Nested->Struct.Get(),Depth+1))return false;
            }
            for(int32 I=Offset;I<Offset+Span;++I){if(Covered[I])return false;Covered[I]=1;}
        }
        if(Count==0||Covered.Contains(0))return false;
        return true; // No padding, unreflected payload or object/container/bool storage is admitted.
    }
    bool TreeScalar(FProperty* P,const void* Data,UWidgetTree* InTree,FString& Out,int32 Depth,bool Runtime)
    {
        if(Depth>16||!TreeField(P)||!Data)return false;auto* K=P->GetClass();
        if(K==FBoolProperty::StaticClass()){TreeToken(Out,static_cast<FBoolProperty*>(P)->GetPropertyValue(Data)?TEXT("1"):TEXT("0"));return true;}
        if(K==FNameProperty::StaticClass()){TreeToken(Out,static_cast<const FName*>(Data)->ToString());return true;}
        if(K==FStrProperty::StaticClass()){const auto& S=*static_cast<const FString*>(Data);if(S.Len()>65536)return false;TreeToken(Out,S);return true;}
        if(K==FTextProperty::StaticClass())
        {
            const FText& T=*static_cast<const FText*>(Data);const FString* S=FTextInspector::GetSourceString(T);
            const auto NS=FTextInspector::GetNamespace(T),Key=FTextInspector::GetKey(T);
            TreeToken(Out,LexToString(FTextInspector::GetFlags(T)));TreeToken(Out,S?*S:TEXT(""));TreeToken(Out,NS.IsSet()?TEXT("set:")+NS.GetValue():TEXT("unset"));TreeToken(Out,Key.IsSet()?TEXT("set:")+Key.GetValue():TEXT("unset"));
            // No display evaluation, namespace repair or native exporter is invoked.
            if(!Runtime)TreeToken(Out,FString::Printf(TEXT("%p"),FTextInspector::GetSharedDataId(T)));
            return S!=nullptr||FTextInspector::GetSharedDataId(T)==FTextInspector::GetSharedDataId(FText::GetEmpty());
        }
        if(K==FObjectProperty::StaticClass())
        {
            const auto* V=static_cast<const TObjectPtr<UObject>*>(Data);if(!V->IsResolved())return false;return TreeReference(V->Get(),InTree,Out);
        }
        if(K==FSoftObjectProperty::StaticClass()){const auto& V=*static_cast<const FSoftObjectPtr*>(Data);const FString Ref=V.ToSoftObjectPath().ToString();if(!Ref.IsEmpty()&&!FWidgetInterfaceWork::Reference(Ref))return false;TreeToken(Out,Ref);return true;}
        if(K==FStructProperty::StaticClass())
        {
            auto* SP=static_cast<FStructProperty*>(P);if(!SP->Struct.IsResolved())return false;auto* S=SP->Struct.Get();
            if(!Live(S)||S->GetClass()!=UScriptStruct::StaticClass()||!S->GetOutermost()->GetName().StartsWith(TEXT("/Script/"))||!FWidgetInterfaceWork::Reference(S->GetPathName())||S->GetStructureSize()!=P->ElementSize)return false;
            // Private native serialization/export state has no general reflected proof.
            if((S->StructFlags&STRUCT_ExportTextItemNative)||((S->StructFlags&STRUCT_SerializeNative)&&S!=FFontOutlineSettings::StaticStruct()))
            {
                if(!CompleteNumericPOD(S,Depth))
                {
                    if(!R.Data->HasField(TEXT("widget_tree_pending_native_struct"))){R.Data->SetStringField(TEXT("widget_tree_pending_native_struct"),S->GetPathName());R.Data->SetNumberField(TEXT("widget_tree_pending_native_struct_flags"),uint32(S->StructFlags));R.Data->SetNumberField(TEXT("widget_tree_pending_native_struct_size"),S->GetStructureSize());}
                    return false;
                }
                if(!TreeReady)R.Data->SetBoolField(TEXT("widget_tree_complete_numeric_pod_observed"),true);
            }
            if(!TreeReady)Holds.Emplace(S);TreeToken(Out,S->GetPathName());int32 Count=0;
            for(TFieldIterator<FProperty> It(S,EFieldIterationFlags::IncludeSuper|EFieldIterationFlags::IncludeDeprecated);It;++It)
            {
                auto* Q=*It;if(Q->HasAnyPropertyFlags(CPF_Transient|CPF_DuplicateTransient|CPF_NonPIEDuplicateTransient|CPF_SkipSerialization)||(Runtime&&Q->HasAnyPropertyFlags(CPF_EditorOnly)))continue;
                if(++Count>256||Q->GetOffset_ForInternal()<0||Q->GetOffset_ForInternal()+Q->GetSize()>S->GetStructureSize()||!TreeValue(Q,Q->ContainerPtrToValuePtr<void>(Data),InTree,Out,Depth+1,Runtime))return false;
            }return Count>0;
        }
        if(K==FArrayProperty::StaticClass())
        {
            auto* A=static_cast<FArrayProperty*>(P);if(!TreeField(A->Inner))return false;FScriptArrayHelper H(A,Data);if(H.Num()>256)return false;TreeToken(Out,FString::FromInt(H.Num()));for(int32 I=0;I<H.Num();++I)if(!TreeValue(A->Inner,H.GetRawPtr(I),InTree,Out,Depth+1,Runtime))return false;return true;
        }
        if(K==FSetProperty::StaticClass())
        {
            auto* A=static_cast<FSetProperty*>(P);if(!TreeField(A->ElementProp))return false;FScriptSetHelper H(A,Data);if(H.Num()>256)return false;TArray<FString> Values;for(int32 I=0;I<H.GetMaxIndex();++I)if(H.IsValidIndex(I)){FString V;if(!TreeValue(A->ElementProp,H.GetElementPtr(I),InTree,V,Depth+1,Runtime))return false;Values.Add(MoveTemp(V));}Values.Sort();for(const auto& V:Values)TreeToken(Out,V);return true;
        }
        if(K==FMapProperty::StaticClass())
        {
            auto* A=static_cast<FMapProperty*>(P);if(!TreeField(A->KeyProp)||!TreeField(A->ValueProp))return false;FScriptMapHelper H(A,Data);if(H.Num()>256)return false;TArray<FString> Values;for(int32 I=0;I<H.GetMaxIndex();++I)if(H.IsValidIndex(I)){FString V;if(!TreeValue(A->KeyProp,H.GetKeyPtr(I),InTree,V,Depth+1,Runtime)||!TreeValue(A->ValueProp,H.GetValuePtr(I),InTree,V,Depth+1,Runtime))return false;Values.Add(MoveTemp(V));}Values.Sort();for(const auto& V:Values)TreeToken(Out,V);return true;
        }
        if(K==FDelegateProperty::StaticClass()){if(!static_cast<const FScriptDelegate*>(Data)->GetFunctionName().IsNone())return false;TreeToken(Out,TEXT("unbound"));return true;}
        if(K==FMulticastSparseDelegateProperty::StaticClass()){if(static_cast<const FSparseDelegate*>(Data)->IsBound())return false;TreeToken(Out,TEXT("unbound"));return true;}
        if(K==FMulticastInlineDelegateProperty::StaticClass())
        {
            const auto* D=static_cast<FMulticastDelegateProperty*>(P)->GetMulticastDelegate(Data);if(D&&D->IsBound())return false;TreeToken(Out,TEXT("unbound"));return true;
        }
        if(K==FEnumProperty::StaticClass())
        {
            auto* Inner=static_cast<FEnumProperty*>(P)->GetUnderlyingProperty();if(!TreeField(Inner)||Inner->GetSize()!=P->ElementSize)return false;
        }
        TreeToken(Out,BytesToHex(static_cast<const uint8*>(Data),P->ElementSize));return true;
    }
    bool TreeValue(FProperty* P,const void* Data,UWidgetTree* InTree,FString& Out,int32 Depth,bool Runtime)
    {
        if(!TreeField(P))return false;TreeToken(Out,P->GetName());TreeToken(Out,P->GetClass()->GetName());
        for(int32 I=0;I<P->ArrayDim;++I)if(!TreeScalar(P,static_cast<const uint8*>(Data)+I*P->ElementSize,InTree,Out,Depth,Runtime)){if(!R.Data->HasField(TEXT("widget_tree_pending_property"))){R.Data->SetStringField(TEXT("widget_tree_pending_property"),P->GetPathName());R.Data->SetStringField(TEXT("widget_tree_pending_field_class"),P->GetClass()->GetName());}return false;}return Out.Len()<=1048576;
    }
    bool ReadWidgetTree(UWidgetTree* InTree,FTreeState& Out)
    {
        if(!Live(InTree)||InTree->GetClass()!=UWidgetTree::StaticClass()||!InTree->RootWidget.IsResolved()||!InTree->NamedSlotBindings.IsEmpty())return false;
        auto* TreeRoot=InTree->RootWidget.Get();if(!Live(TreeRoot)||TreeRoot->GetClass()!=UCanvasPanel::StaticClass())return false;
        TArray<UObject*> Objects;GetObjectsWithOuter(InTree,Objects,true);if(Objects.Num()>256)return false;
        for(auto* O:Objects)
        {
            if(!Live(O)||(O->GetClass()!=UCanvasPanel::StaticClass()&&O->GetClass()!=UButton::StaticClass()&&O->GetClass()!=UTextBlock::StaticClass()&&O->GetClass()!=UCanvasPanelSlot::StaticClass()&&O->GetClass()!=UButtonSlot::StaticClass())||!TreeNative(O->GetClass()))return false;
            FTreeObject Row;Row.Object.Reset(O);Row.Class.Reset(O->GetClass());Row.Outer.Reset(O->GetOuter());Row.Path=O->GetPathName();Row.Relative=O->GetPathName(InTree);Out.Objects.Add(MoveTemp(Row));
        }
        if(!Objects.Contains(TreeRoot))return false;Out.Objects.Sort([](const auto& A,const auto& B){return A.Relative<B.Relative;});
        TSet<UWidget*> Seen;TArray<UWidget*> Pending;Pending.Add(TreeRoot);
        for(int32 I=0;I<Pending.Num();++I)
        {
            auto* W=Pending[I];if(!Objects.Contains(W)||Seen.Contains(W)||W->GetOuter()!=InTree||!W->Slot.IsResolved())return false;Seen.Add(W);
            if(W==TreeRoot){if(W->Slot.Get())return false;}
            else{auto* Slot=W->Slot.Get();if(!Objects.Contains(Slot)||!Slot->Parent.IsResolved()||!Slot->Content.IsResolved()||Slot->Content.Get()!=W||!Live(Slot->Parent.Get())||!Objects.Contains(Slot->Parent.Get()))return false;}
            TreeToken(Out.Shape,W->GetPathName(InTree));
            if(auto* Panel=Cast<UPanelWidget>(W))
            {
                auto* SlotsField=FindFProperty<FArrayProperty>(UPanelWidget::StaticClass(),TEXT("Slots"));
                if(!SlotsField||SlotsField->GetOwnerClass()!=UPanelWidget::StaticClass()||!TreeField(SlotsField)||SlotsField->Inner->GetClass()!=FObjectProperty::StaticClass()||!TreeField(SlotsField->Inner))return false;
                FScriptArrayHelper Slots(SlotsField,SlotsField->ContainerPtrToValuePtr<void>(Panel));if(Slots.Num()>128)return false;
                for(int32 K=0;K<Slots.Num();++K){const auto& H=*reinterpret_cast<const TObjectPtr<UPanelSlot>*>(Slots.GetRawPtr(K));if(!H.IsResolved()||!Objects.Contains(H.Get()))return false;auto* Slot=H.Get();if(!Slot->Parent.IsResolved()||Slot->Parent.Get()!=Panel||!Slot->Content.IsResolved()||!Objects.Contains(Slot->Content.Get())||!Slot->Content->Slot.IsResolved()||Slot->Content->Slot.Get()!=Slot)return false;Pending.Add(Slot->Content.Get());TreeToken(Out.Shape,Slot->GetPathName(InTree));}
            }
        }
        int32 Widgets=0,Slots=0;for(auto* O:Objects){Widgets+=O->IsA<UWidget>()?1:0;Slots+=O->IsA<UPanelSlot>()?1:0;}if(Widgets!=Seen.Num()||Slots!=Widgets-1)return false;
        for(auto& Row:Out.Objects)
        {
            auto* O=Row.Object.Get();TreeToken(Out.Runtime,Row.Relative);TreeToken(Out.Runtime,Row.Class->GetPathName());
            for(TFieldIterator<FProperty> It(Row.Class.Get(),EFieldIterationFlags::IncludeSuper|EFieldIterationFlags::IncludeDeprecated);It;++It)
            {
                auto* Q=*It;if(Q->HasAnyPropertyFlags(CPF_Transient|CPF_DuplicateTransient|CPF_NonPIEDuplicateTransient|CPF_SkipSerialization))continue;
                if(Q->GetOffset_ForInternal()<0||Q->GetOffset_ForInternal()+Q->GetSize()>Row.Class->GetPropertiesSize())return false;
                if(!TreeValue(Q,Q->ContainerPtrToValuePtr<void>(O),InTree,Row.Values,0,false))return false;
                if(!Q->HasAnyPropertyFlags(CPF_EditorOnly)&&!TreeValue(Q,Q->ContainerPtrToValuePtr<void>(O),InTree,Row.RuntimeValues,0,true))return false;
            }TreeToken(Out.Runtime,Row.RuntimeValues);
        }TreeToken(Out.Runtime,Out.Shape);return true;
    }
    bool WidgetTreeCurrent()
    {
        if(!TreeReady)return true;FTreeState Actual;if(!ReadWidgetTree(WidgetTree.Get(),Actual)||Actual.Shape!=AuthoredTree.Shape||Actual.Objects.Num()!=AuthoredTree.Objects.Num())return false;
        for(int32 I=0;I<Actual.Objects.Num();++I){const auto& A=Actual.Objects[I];const auto& E=AuthoredTree.Objects[I];if(A.Object.Get()!=E.Object.Get()||A.Class.Get()!=E.Class.Get()||A.Outer.Get()!=E.Outer.Get()||A.Path!=E.Path||A.Values!=E.Values)return false;}return true;
    }

    struct FCompiledTree{UClass* Owner=nullptr;TStrongObjectPtr<UWidgetTree> Tree;TStrongObjectPtr<UWidget> Root;FString Path,RootPath;FTreeState State;};TArray<FCompiledTree> CompiledTrees;
    FObjectProperty* CompiledTreeField=nullptr;int32 CompiledTreeOffset=0;
    TArray<FBPVariableDescription> WidgetVariables;
    bool WidgetVariablesCurrent()
    {
        if(Target->NewVariables.Num()!=WidgetVariables.Num())return false;
        for(int32 I=0;I<WidgetVariables.Num();++I)
        {
            const auto& A=WidgetVariables[I];const auto& B=Target->NewVariables[I];
            if(A.VarName!=B.VarName||A.VarGuid!=B.VarGuid||!SameType(A.VarType,B.VarType)||A.PropertyFlags!=B.PropertyFlags||!A.FriendlyName.Equals(B.FriendlyName,ESearchCase::CaseSensitive)||!A.DefaultValue.Equals(B.DefaultValue,ESearchCase::CaseSensitive)||A.RepNotifyFunc!=B.RepNotifyFunc||A.ReplicationCondition!=B.ReplicationCondition||A.MetaDataArray.Num()!=B.MetaDataArray.Num()||FTextInspector::GetFlags(A.Category)!=FTextInspector::GetFlags(B.Category)||FTextInspector::GetSharedDataId(A.Category)!=FTextInspector::GetSharedDataId(B.Category))return false;
            for(int32 K=0;K<A.MetaDataArray.Num();++K)if(A.MetaDataArray[K].DataKey!=B.MetaDataArray[K].DataKey||!A.MetaDataArray[K].DataValue.Equals(B.MetaDataArray[K].DataValue,ESearchCase::CaseSensitive))return false;
        }return true;
    }
    bool TreeFieldCurrent()
    {
        if(!CompiledTreeField||FindFProperty<FObjectProperty>(UWidgetBlueprintGeneratedClass::StaticClass(),TEXT("WidgetTree"))!=CompiledTreeField)return false;
        return CompiledTreeField->GetOwnerClass()==UWidgetBlueprintGeneratedClass::StaticClass()&&CompiledTreeField->GetOffset_ForInternal()==CompiledTreeOffset&&CompiledTreeField->ArrayDim==1&&CompiledTreeField->GetSize()==sizeof(TObjectPtr<UWidgetTree>)&&CompiledTreeField->PropertyClass.IsResolved()&&CompiledTreeField->PropertyClass.Get()==UWidgetTree::StaticClass();
    }
    bool CaptureCompiledTrees(TArray<FCompiledTree>& Out)
    {
        if(!TreeFieldCurrent())return false;
        for(UClass* C:{Generated.Get(),Skeleton.Get()})
        {
            if(!C->GetClass()->IsChildOf(UWidgetBlueprintGeneratedClass::StaticClass()))continue;
            const auto* Slot=CompiledTreeField->ContainerPtrToValuePtr<TObjectPtr<UWidgetTree>>(C);if(!Slot->IsResolved())return false;FCompiledTree Row;Row.Owner=C;Row.Tree.Reset(Slot->Get());
            if(Row.Tree.IsValid())
            {
                if(!Live(Row.Tree.Get())||Row.Tree->GetClass()!=UWidgetTree::StaticClass()||Row.Tree->GetOuter()!=C||!Row.Tree->RootWidget.IsResolved())return false;
                Row.Path=Row.Tree->GetPathName();Row.Root.Reset(Row.Tree->RootWidget.Get());
                if(Row.Root.IsValid()){if(!Live(Row.Root.Get())||Row.Root->GetClass()!=UCanvasPanel::StaticClass()||Row.Root->GetOuter()!=Row.Tree.Get())return false;Row.RootPath=Row.Root->GetPathName();if(RichTree&&!ReadWidgetTree(Row.Tree.Get(),Row.State))return false;}
            }Out.Add(MoveTemp(Row));
        }return true;
    }
    bool CompiledTreesCurrent()
    {
        if(!TreeFieldCurrent())return false;
        for(const auto& Row:CompiledTrees)
        {
            if(Row.Owner!=Generated.Get()&&Row.Owner!=Skeleton.Get())return false;
            const auto* Slot=CompiledTreeField->ContainerPtrToValuePtr<TObjectPtr<UWidgetTree>>(Row.Owner);if(!Slot->IsResolved()||Slot->Get()!=Row.Tree.Get())return false;
            if(Row.Tree.IsValid()&&(!Live(Row.Tree.Get())||Row.Tree->GetClass()!=UWidgetTree::StaticClass()||Row.Tree->GetOuter()!=Row.Owner||!Row.Tree->GetPathName().Equals(Row.Path,ESearchCase::CaseSensitive)||!Row.Tree->RootWidget.IsResolved()||Row.Tree->RootWidget.Get()!=Row.Root.Get()))return false;
            if(Row.Root.IsValid()&&(!Live(Row.Root.Get())||Row.Root->GetClass()!=UCanvasPanel::StaticClass()||Row.Root->GetOuter()!=Row.Tree.Get()||!Row.Root->GetPathName().Equals(Row.RootPath,ESearchCase::CaseSensitive)))return false;
            if(RichTree&&Row.Root.IsValid()){FTreeState Actual;if(!ReadWidgetTree(Row.Tree.Get(),Actual)||Actual.Shape!=Row.State.Shape||Actual.Objects.Num()!=Row.State.Objects.Num())return false;for(int32 I=0;I<Actual.Objects.Num();++I){const auto& A=Actual.Objects[I];const auto& E=Row.State.Objects[I];if(A.Object.Get()!=E.Object.Get()||A.Class.Get()!=E.Class.Get()||A.Outer.Get()!=E.Outer.Get()||A.Path!=E.Path||A.Values!=E.Values)return false;}}
        }return true;
    }
    UClass* TargetParent()const{return WidgetTarget?UUserWidget::StaticClass():AActor::StaticClass();}
    UClass* TargetAssetClass()const{return WidgetTarget?UWidgetBlueprint::StaticClass():UBlueprint::StaticClass();}
    TStrongObjectPtr<UBlueprint> Source,Target;TStrongObjectPtr<UClass> Interface,SourceSkeleton,Generated,Skeleton;TStrongObjectPtr<UObject> SourceCDO,SourceSkelCDO,CDO,SkelCDO;
    TStrongObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;TStrongObjectPtr<UBlueprintEditorSettings> SavePolicy;TArray<TStrongObjectPtr<UObject>> Holds,Retired;
    FString SourcePath,TargetPath,InterfacePathValue,SourceSkeletonPath,GeneratedPath,SkeletonPath,SourceFile,TargetFile,SourceHash,TargetHash,ClassPolicy;
    struct FParam{FName Name;FEdGraphPinType Type;bool Out=false;};
    struct FUser {TSharedPtr<FUserPinInfo> Hold;FName Name;FEdGraphPinType Type;EEdGraphPinDirection Direction;FString Default;};
    struct FPin{UEdGraphPin* Address=nullptr;FGuid Id,Persistent;FName Name;EEdGraphPinDirection Direction;FEdGraphPinType Type;FString Raw,Auto,Display,FriendlyDisplay;FText Text,Friendly;UObject* Object=nullptr;TArray<UEdGraphPin*> Links,Children;UEdGraphPin* Parent=nullptr;uint32 Flags=0;};
    struct FNode{TStrongObjectPtr<UK2Node> Hold;UClass* Class=nullptr;FGuid Id;FString Path,Comment;int32 X=0,Y=0;FMemberReference Ref;int32 Extra=0;TArray<FUser> Users;TArray<FPin> Pins;};
    struct FGraph{TStrongObjectPtr<UEdGraph> Hold;FGuid Guid,InterfaceId;FString Path;bool AllowDelete=true;TArray<FNode> Nodes;};
    // TY concrete per-function plan; no active-method switch or observer over a partially enrolled array.
    struct FMethod
    {
        FFunctionRole Decl,SkelDecl;FName Name;FGuid Guid;FMemberReference PlannedReference;EFunctionFlags Flags=FUNC_None;
        TArray<FParam> Params;FGraph SourceGraph,NewGraph,CandidateNew;FString DeclDefaults,SkelDefaults;
        bool HasNew=false,GraphRequired=false,HasOutput=false,CanOverride=false,AsEvent=false,Anim=false;
    };
    TArray<FMethod> Methods;TArray<TStrongObjectPtr<UFunction>> DeclOrder,SkelOrder,SDKOrder;TArray<FFunctionRole> InheritedFunctions;TArray<FString> InheritedDefaults;TArray<EClassFlags> InheritedOwnerFlags;
    FGraph BeforeGraph,CandidateBefore,Construction,CandidateConstruction;TArray<TObjectPtr<UEdGraph>> SourceFunctions,BeforeFunctions,BeforeUber;
    TStrongObjectPtr<USimpleConstructionScript> SCS;TStrongObjectPtr<USCS_Node> RootNode;TStrongObjectPtr<USceneComponent> RootTemplate;FGuid RootGuid;FName RootName;UObject* SCSOuter=nullptr;FString RootPath,RootValues;
    bool Ready=false,Stopped=false,Writer=false,Rebinding=false,ClassesReady=false,CandidatesReady=false,WasImplemented=false,PlanReady=false,ExposeDeprecated=false;int32 ExpectedInterfaces=0,RequiredGraphs=0;
    explicit FGameInterfaceWork(FResult& In):R(In),FieldReader(In){}
    static bool Live(const UObject* O){return FWidgetInterfaceWork::Live(O);}
    bool Stop(const TCHAR* Property,const FString& Expected,const FString& Actual,const FString& Object=FString())
    {if(!Stopped){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("object_captured_path"),Object);J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);J->SetBoolField(TEXT("later_conditions_unobserved"),true);R.Data->SetObjectField(TEXT("first_current_failure"),J);R.ErrorCode=TEXT("game_interface_current_unknown");R.Error=TEXT("Game interface source/target/current observation failed; actual prefix retained without another SDK, cleanup or retry.");}Stopped=true;R.bChangeStateKnown=false;R.bSuccess=R.bPostconditionPassed=false;return false;}
    bool Reject(const TCHAR* Code,const FString& Why){return Fail(R,Code,Why);}
    bool Context()
    {return !Stopped&&IsInGameThread()&&Live(Editor.Get())&&GEditor==Editor.Get()&&Live(World.Get())&&GEditor->GetEditorWorldContext().World()==World.Get()&&!GEditor->PlayWorld&&!GEditor->GetPIEWorldContext()&&!World->bIsTearingDown&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage();}
    static uint32 PinFlags(const UEdGraphPin* P){return uint32(P->bHidden)|(uint32(P->bOrphanedPin)<<1)|(uint32(P->bNotConnectable)<<2)|(uint32(P->bDefaultValueIsReadOnly)<<3)|(uint32(P->bDefaultValueIsIgnored)<<4)|(uint32(P->bAdvancedView)<<5)|(uint32(P->bDeprecated)<<6)|(uint32(P->bDisplayAsMutableRef)<<7)|(uint32(P->bAllowFriendlyName)<<8);}
    bool ClassRole(UClass* C,UBlueprint* B,UClass* Parent,const FString& Path)
    {return Live(C)&&C->GetClass()==(WidgetTarget&&B==Target.Get()?(C->GetPathName()==GeneratedPath?TargetGeneratedMeta:TargetSkeletonMeta):UBlueprintGeneratedClass::StaticClass())&&C->GetOuter()==B->GetPackage()&&C->GetPackage()==B->GetPackage()&&C->ClassGeneratedBy.IsResolved()&&C->ClassGeneratedBy.Get()==B&&C->GetSuperClass()==Parent&&C->GetPathName().Equals(Path,ESearchCase::CaseSensitive)&&!C->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists)&&C->ClassDefaultObject.IsResolved();}
    bool CDORole(UObject* O,UClass* C)
    {const EObjectFlags Required=WidgetTarget&&C->ClassGeneratedBy.Get()==Target.Get()?RF_ClassDefaultObject:RF_ClassDefaultObject|RF_ArchetypeObject;return !O||(Live(O)&&O->GetClass()==C&&O->GetOuter()==C->GetOuter()&&O->GetPackage()==C->GetPackage()&&O->HasAllFlags(Required)&&O->GetFName()==FName(*(TEXT("Default__")+C->GetName())));}
    static FMemberReference Reference(UK2Node* N)
    {if(auto* E=Cast<UK2Node_Event>(N))return E->EventReference;if(auto* E=Cast<UK2Node_FunctionEntry>(N))return E->FunctionReference;return static_cast<UK2Node_FunctionResult*>(N)->FunctionReference;}
    bool Capture(UEdGraph* G,FGraph& Out)
    {
        if(!Live(G)||G->GetClass()!=UEdGraph::StaticClass()||!G->Schema.GetGCPtr().IsResolved()||G->Schema.Get()!=UEdGraphSchema_K2::StaticClass()||!G->SubGraphs.IsEmpty()||G->Nodes.Num()>16)return Stop(TEXT("graph_role"),TEXT("exact bounded flat K2 graph"),TEXT("different"));Out=FGraph();Out.Hold.Reset(G);Out.Guid=G->GraphGuid;Out.InterfaceId=G->InterfaceGuid;Out.Path=G->GetPathName();Out.AllowDelete=G->bAllowDeletion;TSet<UEdGraphPin*> Pins;TSet<FGuid> Ids;
        for(const auto& Slot:G->Nodes)
        {
            if(!Slot.IsResolved()||!Live(Slot.Get()))return Stop(TEXT("node_slot"),TEXT("current resolved"),TEXT("unavailable"));auto* N=Cast<UK2Node>(Slot.Get());if(!N||N->GetOuter()!=G||(N->GetClass()!=UK2Node_Event::StaticClass()&&N->GetClass()!=UK2Node_FunctionEntry::StaticClass()&&N->GetClass()!=UK2Node_FunctionResult::StaticClass())||!N->NodeGuid.IsValid())return Stop(TEXT("node_role"),TEXT("stock Event/Entry/Result"),TEXT("different"));FNode V;V.Hold.Reset(N);V.Class=N->GetClass();V.Id=N->NodeGuid;V.Path=N->GetPathName();V.Comment=N->NodeComment;V.X=N->NodePosX;V.Y=N->NodePosY;V.Ref=Reference(N);if(auto* E=Cast<UK2Node_FunctionEntry>(N))V.Extra=E->GetExtraFlags();
            if(auto* Editable=Cast<UK2Node_EditablePinBase>(N))for(const auto& U:Editable->UserDefinedPins){if(!U.IsValid()||V.Users.Num()>32)return Stop(TEXT("user_pin_role"),TEXT("bounded current UserPinInfo"),TEXT("different"));V.Users.Add({U,U->PinName,U->PinType,U->DesiredPinDirection,U->PinDefaultValue});}
            for(auto* P:N->Pins){if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||Ids.Contains(P->PinId)||Pins.Contains(P)||N->Pins.Num()>64||!P->DefaultObject.IsResolved()||P->PinType.PinSubCategoryObject.IsStale()||P->PinType.PinValueType.TerminalSubCategoryObject.IsStale())return Stop(TEXT("pin_role"),TEXT("current unique resolved own pins"),TEXT("different"));Ids.Add(P->PinId);Pins.Add(P);FPin Q;Q.Address=P;Q.Id=P->PinId;Q.Persistent=P->PersistentGuid;Q.Name=P->PinName;Q.Direction=P->Direction;Q.Type=P->PinType;Q.Raw=P->DefaultValue;Q.Auto=P->AutogeneratedDefaultValue;Q.Text=P->DefaultTextValue;Q.Friendly=P->PinFriendlyName;Q.Object=P->DefaultObject.Get();Q.Links=P->LinkedTo;Q.Parent=P->ParentPin;Q.Children=P->SubPins;Q.Flags=PinFlags(P);for(UObject* O:{Q.Object,Q.Type.PinSubCategoryObject.Get(),Q.Type.PinValueType.TerminalSubCategoryObject.Get()})if(O){if(!Live(O))return Stop(TEXT("pin_object"),TEXT("live"),TEXT("unavailable"));Holds.Emplace(O);}V.Pins.Add(MoveTemp(Q));}Out.Nodes.Add(MoveTemp(V));
        }
        for(const auto& N:Out.Nodes)for(const auto& P:N.Pins){for(auto* L:P.Links)if(!Pins.Contains(L)||!L->LinkedTo.Contains(P.Address))return Stop(TEXT("pin_link_membership"),TEXT("closed reciprocal"),TEXT("different"));if(P.Parent&&(!Pins.Contains(P.Parent)||!P.Parent->SubPins.Contains(P.Address)))return Stop(TEXT("pin_parent"),TEXT("closed reciprocal split"),TEXT("different"));for(auto* L:P.Children)if(!Pins.Contains(L)||L->ParentPin!=P.Address)return Stop(TEXT("pin_child"),TEXT("closed reciprocal split"),TEXT("different"));}return true;
    }
    static FString TypeEvidence(const FEdGraphPinType& T)
    {return T.PinCategory.ToString()+TEXT("/")+T.PinSubCategory.ToString()+TEXT("/container=")+LexToString(uint8(T.ContainerType))+TEXT("/reference=")+LexToString(T.bIsReference)+TEXT("/const=")+LexToString(T.bIsConst)+TEXT("/weak=")+LexToString(T.bIsWeakPointer)+TEXT("/wrapper=")+LexToString(T.bIsUObjectWrapper)+TEXT("/single=")+LexToString(T.bSerializeAsSinglePrecisionFloat)+TEXT("/object=")+LexToString(reinterpret_cast<UPTRINT>(T.PinSubCategoryObject.Get()))+TEXT("/terminal=")+T.PinValueType.TerminalCategory.ToString()+TEXT("/")+T.PinValueType.TerminalSubCategory.ToString()+TEXT("/")+LexToString(reinterpret_cast<UPTRINT>(T.PinValueType.TerminalSubCategoryObject.Get()))+TEXT("/const=")+LexToString(T.PinValueType.bTerminalIsConst)+TEXT("/weak=")+LexToString(T.PinValueType.bTerminalIsWeakPointer)+TEXT("/wrapper=")+LexToString(T.PinValueType.bTerminalIsUObjectWrapper);}
    bool PinCells(const FPin& A,const FPin& B,const FString& Object)
    {
        const FString Where=Object+TEXT(":")+A.Name.ToString()+TEXT("[")+A.Id.ToString()+TEXT("]");
        if(A.Name!=B.Name)return Stop(TEXT("pin.name"),A.Name.ToString(),B.Name.ToString(),Where);
        if(A.Direction!=B.Direction)return Stop(TEXT("pin.direction"),LexToString(uint8(A.Direction)),LexToString(uint8(B.Direction)),Where);
        if(!SameType(A.Type,B.Type))return Stop(TEXT("pin.type"),TypeEvidence(A.Type),TypeEvidence(B.Type),Where);
        if(!A.Raw.Equals(B.Raw,ESearchCase::CaseSensitive))return Stop(TEXT("pin.raw"),A.Raw,B.Raw,Where);
        if(!A.Auto.Equals(B.Auto,ESearchCase::CaseSensitive))return Stop(TEXT("pin.auto"),A.Auto,B.Auto,Where);
        if(A.Flags!=B.Flags)return Stop(TEXT("pin.flags"),LexToString(A.Flags),LexToString(B.Flags),Where);
        if(A.Object!=B.Object)return Stop(TEXT("pin.default_object_address"),LexToString(reinterpret_cast<UPTRINT>(A.Object)),LexToString(reinterpret_cast<UPTRINT>(B.Object)),Where);
        if(FTextInspector::GetFlags(A.Text)!=FTextInspector::GetFlags(B.Text))return Stop(TEXT("pin.default_text_flags"),LexToString(FTextInspector::GetFlags(A.Text)),LexToString(FTextInspector::GetFlags(B.Text)),Where);
        if(FTextInspector::GetFlags(A.Friendly)!=FTextInspector::GetFlags(B.Friendly))return Stop(TEXT("pin.friendly_flags"),LexToString(FTextInspector::GetFlags(A.Friendly)),LexToString(FTextInspector::GetFlags(B.Friendly)),Where);return true;
    }
    bool Physical(const FGraph& G)
    {
        if(!G.Hold.IsValid())return true;if(!Live(G.Hold.Get())||G.Hold->GetClass()!=UEdGraph::StaticClass()||G.Hold->GraphGuid!=G.Guid||G.Hold->InterfaceGuid!=G.InterfaceId||G.Hold->bAllowDeletion!=G.AllowDelete||!G.Hold->GetPathName().Equals(G.Path,ESearchCase::CaseSensitive)||!G.Hold->Schema.GetGCPtr().IsResolved()||G.Hold->Schema.Get()!=UEdGraphSchema_K2::StaticClass()||!G.Hold->SubGraphs.IsEmpty()||G.Hold->Nodes.Num()!=G.Nodes.Num())return Stop(TEXT("graph_current"),G.Path,TEXT("changed"));
        for(const auto& N:G.Nodes){if(!Live(N.Hold.Get())||!G.Hold->Nodes.Contains(N.Hold.Get())||N.Hold->GetClass()!=N.Class||N.Hold->GetOuter()!=G.Hold.Get()||N.Hold->NodeGuid!=N.Id||N.Hold->NodePosX!=N.X||N.Hold->NodePosY!=N.Y||!N.Hold->GetPathName().Equals(N.Path,ESearchCase::CaseSensitive)||!N.Hold->NodeComment.Equals(N.Comment,ESearchCase::CaseSensitive)||!Reference(N.Hold.Get()).IsSameReference(N.Ref)||N.Hold->Pins.Num()!=N.Pins.Num())return Stop(TEXT("node_current"),N.Path,TEXT("changed"));for(int32 I=0;I<N.Pins.Num();++I)if(N.Hold->Pins[I]!=N.Pins[I].Address)return Stop(TEXT("pin_membership"),N.Path,TEXT("changed"));if(auto* Entry=Cast<UK2Node_FunctionEntry>(N.Hold.Get()))if(Entry->GetExtraFlags()!=N.Extra)return Stop(TEXT("entry_flags"),LexToString(N.Extra),LexToString(Entry->GetExtraFlags()));if(auto* Editable=Cast<UK2Node_EditablePinBase>(N.Hold.Get())){if(Editable->UserDefinedPins.Num()!=N.Users.Num())return Stop(TEXT("user_pin_count"),LexToString(N.Users.Num()),TEXT("different"));for(int32 I=0;I<N.Users.Num();++I){const auto& U=N.Users[I];const auto& A=Editable->UserDefinedPins[I];if(A!=U.Hold||!A.IsValid()||A->PinName!=U.Name||!SameType(A->PinType,U.Type)||A->DesiredPinDirection!=U.Direction||!A->PinDefaultValue.Equals(U.Default,ESearchCase::CaseSensitive))return Stop(TEXT("user_pin_current"),U.Name.ToString(),TEXT("changed"));}}}
        for(const auto& N:G.Nodes)for(const auto& Q:N.Pins){const auto* P=Q.Address;if(P->WasTrashed())return Stop(TEXT("pin.trashed"),TEXT("false"),TEXT("true"),N.Path);if(!P->DefaultObject.IsResolved())return Stop(TEXT("pin.default_object_resolved"),TEXT("true"),TEXT("false"),N.Path);FPin Actual;Actual.Id=P->PinId;Actual.Name=P->PinName;Actual.Direction=P->Direction;Actual.Type=P->PinType;Actual.Raw=P->DefaultValue;Actual.Auto=P->AutogeneratedDefaultValue;Actual.Flags=PinFlags(P);Actual.Object=P->DefaultObject.Get();Actual.Text=P->DefaultTextValue;Actual.Friendly=P->PinFriendlyName;if(!PinCells(Q,Actual,N.Path))return false;if(P->WasTrashed()||P->PinId!=Q.Id||P->PersistentGuid!=Q.Persistent||P->PinName!=Q.Name||P->Direction!=Q.Direction||!SameType(P->PinType,Q.Type)||!P->DefaultObject.IsResolved()||P->DefaultObject.Get()!=Q.Object||!P->DefaultValue.Equals(Q.Raw,ESearchCase::CaseSensitive)||!P->AutogeneratedDefaultValue.Equals(Q.Auto,ESearchCase::CaseSensitive)||FTextInspector::GetSharedDataId(P->DefaultTextValue)!=FTextInspector::GetSharedDataId(Q.Text)||FTextInspector::GetFlags(P->DefaultTextValue)!=FTextInspector::GetFlags(Q.Text)||FTextInspector::GetSharedDataId(P->PinFriendlyName)!=FTextInspector::GetSharedDataId(Q.Friendly)||FTextInspector::GetFlags(P->PinFriendlyName)!=FTextInspector::GetFlags(Q.Friendly)||PinFlags(P)!=Q.Flags||P->LinkedTo!=Q.Links||P->ParentPin!=Q.Parent||P->SubPins!=Q.Children)return Stop(TEXT("pin_current"),Q.Id.ToString(),TEXT("changed"));}return true;
    }
    static FString Defaults(const FFunctionRole& Role)
    {
        FString Value;for(const auto& Field:Role.Fields){const FString Key=TEXT("CPP_Default_")+Field.Name.ToString();const FString Cell=Role.F->GetMetaData(FName(*Key));Value+=Key+TEXT(":")+LexToString(Cell.Len())+TEXT(":")+Cell;}
        for(const TCHAR* Key:{TEXT("BlueprintInternalUseOnly"),TEXT("DeprecatedFunction"),TEXT("ForceAsFunction"),TEXT("AnimBlueprintFunction"),TEXT("BlueprintThreadSafe"),TEXT("NotBlueprintThreadSafe")}){const FString Cell=Role.F->GetMetaData(Key);Value+=FString(Key)+TEXT(":")+(Role.F->HasMetaData(Key)?TEXT("1:"):TEXT("0:"))+LexToString(Cell.Len())+TEXT(":")+Cell;}return Value;
    }
    FString ReadClassPolicy()const
    {
        FString Value;for(const UClass* Class:{Interface.Get(),SourceSkeleton.Get(),UInterface::StaticClass(),UObject::StaticClass(),TargetParent()}){if(!Live(Class))return TEXT("unavailable");Value+=LexToString(reinterpret_cast<UPTRINT>(Class))+TEXT(":")+LexToString(uint32(Class->ClassFlags));for(const TCHAR* Key:{TEXT("CannotImplementInterfaceInBlueprint"),TEXT("IsBlueprintBase"),TEXT("ProhibitedInterfaces")}){const FString Cell=Class->GetMetaData(Key);Value+=FString(Key)+TEXT(":")+(Class->HasMetaData(Key)?TEXT("1:"):TEXT("0:"))+LexToString(Cell.Len())+TEXT(":")+Cell;}}return Value;
    }
    bool SourceMethodsCurrent()
    {
        if(Source->FunctionGraphs.Num()!=SourceFunctions.Num())return Stop(TEXT("source_graph_inventory"),LexToString(SourceFunctions.Num()),LexToString(Source->FunctionGraphs.Num()),SourcePath);
        for(int32 Slot=0;Slot<SourceFunctions.Num();++Slot)if(!Source->FunctionGraphs[Slot].IsResolved()||Source->FunctionGraphs[Slot].Get()!=SourceFunctions[Slot].Get())return Stop(TEXT("source_graph_slot"),LexToString(Slot),TEXT("different/unresolved"),SourcePath);
        for(int32 Kind=0;Kind<2;++Kind)
        {
            UClass* Owner=Kind==0?Interface.Get():SourceSkeleton.Get();const auto& BeforeOrder=Kind==0?DeclOrder:SkelOrder;TArray<UFunction*> Actual;
            for(TFieldIterator<UFunction> It(Owner,EFieldIterationFlags::None);It;++It){if(Actual.Num()>=32)return Stop(TEXT("source_function_bound"),TEXT("<=32 own functions"),TEXT("larger"));Actual.Add(*It);}
            if(Actual.Num()!=BeforeOrder.Num())return Stop(TEXT("source_function_inventory"),LexToString(BeforeOrder.Num()),LexToString(Actual.Num()),Owner->GetPathName());
            for(int32 Slot=0;Slot<Actual.Num();++Slot)if(Actual[Slot]!=BeforeOrder[Slot].Get())return Stop(TEXT("source_function_slot"),LexToString(Slot),TEXT("different"),Owner->GetPathName());
        }
        for(const auto& Method:Methods)for(int32 Kind=0;Kind<2;++Kind)
        {
            const auto& Role=Kind==0?Method.Decl:Method.SkelDecl;const FString& BeforeDefaults=Kind==0?Method.DeclDefaults:Method.SkelDefaults;
            if(!Live(Role.F.Get())||Role.F->GetClass()!=UFunction::StaticClass()||Role.F->GetOuter()!=Role.Owner||Role.F->GetFName()!=Role.Name||Role.F->FunctionFlags!=Role.Flags||!FieldReader.FieldsCurrent(Role))return Stop(TEXT("source_signature"),Method.Name.ToString(),TEXT("different"));
            const FString NowDefaults=Defaults(Role);if(!NowDefaults.Equals(BeforeDefaults,ESearchCase::CaseSensitive))return Stop(TEXT("source_parameter_defaults"),BeforeDefaults,NowDefaults,Method.Name.ToString());
        }
        TArray<UFunction*> SDKNow;for(TFieldIterator<UFunction> It(Interface.Get(),EFieldIteratorFlags::IncludeSuper);It;++It){if(SDKNow.Num()>=64)return Stop(TEXT("source_sdk_function_bound"),TEXT("<=64 declared/inherited functions"),TEXT("larger"));SDKNow.Add(*It);}if(SDKNow.Num()!=SDKOrder.Num())return Stop(TEXT("source_sdk_function_count"),LexToString(SDKOrder.Num()),LexToString(SDKNow.Num()));for(int32 Slot=0;Slot<SDKNow.Num();++Slot)if(SDKNow[Slot]!=SDKOrder[Slot].Get())return Stop(TEXT("source_sdk_function_order"),LexToString(Slot),TEXT("different"));
        for(int32 Slot=0;Slot<InheritedFunctions.Num();++Slot){const auto& Role=InheritedFunctions[Slot];UClass* Owner=Role.Owner;if(!Live(Owner)||(Owner!=UInterface::StaticClass()&&Owner!=UObject::StaticClass())||Owner->ClassFlags!=InheritedOwnerFlags[Slot]||!Live(Role.F.Get())||Role.F->GetClass()!=UFunction::StaticClass()||Role.F->GetOuter()!=Owner||Role.F->GetFName()!=Role.Name||Role.F->FunctionFlags!=Role.Flags||!FieldReader.FieldsCurrent(Role)||!Defaults(Role).Equals(InheritedDefaults[Slot],ESearchCase::CaseSensitive))return Stop(TEXT("inherited_native_source"),Role.Name.ToString(),TEXT("different"));}
        return true;
    }
    bool Current()
    {
        if(!Context())return Stop(TEXT("context"),TEXT("same idle Editor/world"),TEXT("changed"));auto* S=Source.Get();auto* B=Target.Get();
        if(!Live(S)||S->GetClass()!=UBlueprint::StaticClass()||S->BlueprintType!=BPTYPE_Interface||S->GetPackage()->IsDirty()||S->Status!=BS_UpToDate||S->bBeingCompiled||S->bQueuedForCompilation||S->bIsRegeneratingOnLoad||S->bSuppressStructurallyModified||!S->ParentClass.GetGCPtr().IsResolved()||S->ParentClass.Get()!=UInterface::StaticClass()||!S->GeneratedClass.GetGCPtr().IsResolved()||S->GeneratedClass.Get()!=Interface.Get()||!S->SkeletonGeneratedClass.GetGCPtr().IsResolved()||S->SkeletonGeneratedClass.Get()!=SourceSkeleton.Get()||!S->UbergraphPages.IsEmpty()||!S->MacroGraphs.IsEmpty()||!S->DelegateSignatureGraphs.IsEmpty()||!S->ImplementedInterfaces.IsEmpty()||!S->NewVariables.IsEmpty())return Stop(TEXT("source_blueprint"),SourcePath,TEXT("changed"));
        if(!ClassRole(Interface.Get(),S,UInterface::StaticClass(),InterfacePathValue)||!ClassRole(SourceSkeleton.Get(),S,UInterface::StaticClass(),SourceSkeletonPath)||!Interface->HasAnyClassFlags(CLASS_Interface)||!SourceSkeleton->HasAnyClassFlags(CLASS_Interface)||Interface->ClassDefaultObject.Get()!=SourceCDO.Get()||SourceSkeleton->ClassDefaultObject.Get()!=SourceSkelCDO.Get()||!CDORole(SourceCDO.Get(),Interface.Get())||!CDORole(SourceSkelCDO.Get(),SourceSkeleton.Get())||!SourceMethodsCurrent())return Stop(TEXT("source_class_role"),TEXT("held interface classes/CDOs/signatures"),TEXT("changed"));
        const FString PolicyNow=ReadClassPolicy();if(!PolicyNow.Equals(ClassPolicy,ESearchCase::CaseSensitive))return Stop(TEXT("source_class_eligibility_policy"),ClassPolicy,PolicyNow);
        if(!Live(B)||B->GetClass()!=TargetAssetClass()||!B->GetPathName().Equals(TargetPath,ESearchCase::CaseSensitive)||B->BlueprintType!=BPTYPE_Normal||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||B->bSuppressStructurallyModified||!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.Get()!=TargetParent()||(WidgetTarget?!WidgetVariablesCurrent():!B->NewVariables.IsEmpty())||B->FunctionGraphs!=BeforeFunctions||!B->MacroGraphs.IsEmpty()||!B->DelegateSignatureGraphs.IsEmpty()||B->UbergraphPages!=BeforeUber||!B->Timelines.IsEmpty()||B->ImplementedInterfaces.Num()!=ExpectedInterfaces)return Stop(TEXT("target_authored"),TargetPath,TEXT("changed"));
        if(!Writer&&(B->GetPackage()->IsDirty()||B->Status!=BS_UpToDate))return Stop(TEXT("target_preflight"),TEXT("saved-clean UpToDate"),TEXT("changed"));
        if(ExpectedInterfaces)
        {
            auto& Description=B->ImplementedInterfaces[0];if(!Description.Interface.GetGCPtr().IsResolved()||Description.Interface.Get()!=Interface.Get())return Stop(TEXT("interface_description"),TEXT("selected same interface"),TEXT("changed"));
            if(PlanReady){if(Description.Graphs.Num()!=RequiredGraphs)return Stop(TEXT("interface_graph_count"),LexToString(RequiredGraphs),LexToString(Description.Graphs.Num()));int32 GraphSlot=0;for(const auto& Method:Methods)if(Method.GraphRequired){if(!Description.Graphs[GraphSlot].IsResolved()||(Method.HasNew&&Description.Graphs[GraphSlot].Get()!=Method.NewGraph.Hold.Get()))return Stop(TEXT("interface_graph_slot"),Method.Name.ToString(),TEXT("different/unresolved"));++GraphSlot;}}
        }
        if(WidgetTarget)
        {
            auto* W=static_cast<UWidgetBlueprint*>(B);
            if(!W->WidgetTree.IsResolved()||W->WidgetTree.Get()!=WidgetTree.Get()||!Live(WidgetTree.Get())||WidgetTree->GetOuter()!=B||WidgetTree->GetPathName()!=WidgetTreePath
                ||!WidgetTree->RootWidget.IsResolved()||WidgetTree->RootWidget.Get()!=WidgetRoot.Get()||!Live(WidgetRoot.Get())||WidgetRoot->GetClass()!=UCanvasPanel::StaticClass()
                ||WidgetRoot->GetOuter()!=WidgetTree.Get()||WidgetRoot->GetPathName()!=WidgetRootPath||WidgetRoot->GetChildrenCount()!=RootChildren||!WidgetTreeCurrent()||!WidgetRoot->Slot.IsResolved()||WidgetRoot->Slot.Get()
                ||!W->Bindings.IsEmpty()||!W->Animations.IsEmpty()||!W->WidgetVariableNameToGuidMap.OrderIndependentCompareEqual(WidgetGuids))
                return Stop(TEXT("widget_authored_tree"),TEXT("same owned native Canvas tree, captured child topology/authored fields/variable GUIDs and no bindings or animations"),TEXT("changed"));
        }
        else
        {
        if(!B->SimpleConstructionScript.IsResolved()||B->SimpleConstructionScript.Get()!=SCS.Get()||!Live(SCS.Get())||SCS->GetOuter()!=SCSOuter||SCS->GetAllNodes().Num()!=1||SCS->GetAllNodes()[0]!=RootNode.Get()||SCS->GetRootNodes().Num()!=1||SCS->GetRootNodes()[0]!=RootNode.Get()||!Live(RootNode.Get())||RootNode->VariableGuid!=RootGuid||RootNode->GetVariableName()!=RootName||!RootNode->GetChildNodes().IsEmpty()||!RootNode->ComponentClass.IsResolved()||RootNode->ComponentClass.Get()!=USceneComponent::StaticClass()||!RootNode->ComponentTemplate.IsResolved()||RootNode->ComponentTemplate.Get()!=RootTemplate.Get()||!Live(RootTemplate.Get())||RootTemplate->GetClass()!=USceneComponent::StaticClass()||RootTemplate->IsRegistered()||RootTemplate->GetOwner()||RootTemplate->GetWorld()||!RootTemplate->GetPathName().Equals(RootPath,ESearchCase::CaseSensitive))return Stop(TEXT("SCS_root_role"),TEXT("captured lone native Scene root"),TEXT("changed"));
        }
        if((!Rebinding||ClassesReady)&&(!B->GeneratedClass.GetGCPtr().IsResolved()||B->GeneratedClass.Get()!=Generated.Get()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||B->SkeletonGeneratedClass.Get()!=Skeleton.Get()||!ClassRole(Generated.Get(),B,TargetParent(),GeneratedPath)||!ClassRole(Skeleton.Get(),B,TargetParent(),SkeletonPath)||Generated->ClassDefaultObject.Get()!=CDO.Get()||Skeleton->ClassDefaultObject.Get()!=SkelCDO.Get()||!CDORole(CDO.Get(),Generated.Get())||!CDORole(SkelCDO.Get(),Skeleton.Get())||(WidgetTarget&&RichTree&&!CompiledTreesCurrent())))return Stop(TEXT("target_class_current"),TEXT("held generated/skeleton/CDO roles"),TEXT("changed"));
        if(!Live(SavePolicy.Get())||!UBlueprintEditorSettings::StaticClass()->ClassDefaultObject.IsResolved()||UBlueprintEditorSettings::StaticClass()->ClassDefaultObject.Get()!=SavePolicy.Get()||SavePolicy->SaveOnCompile!=SoC_Never||SavePolicy->bExposeDeprecatedFunctions!=ExposeDeprecated)return Stop(TEXT("save_policy"),TEXT("captured Never/eligibility policy"),TEXT("changed"));
        for(const auto& Method:Methods)if(!Physical(Method.SourceGraph))return false;
        if(Rebinding&&!CandidatesReady)return true;if(!Physical(CandidatesReady?CandidateBefore:BeforeGraph)||!Physical(CandidatesReady?CandidateConstruction:Construction))return false;
        for(const auto& Method:Methods)if(Method.HasNew&&!Physical(CandidatesReady?Method.CandidateNew:Method.NewGraph))return false;return true;
    }

    bool Texts(FGraph& G)
    {for(auto& N:G.Nodes)for(auto& P:N.Pins){const FText A=P.Text,B=P.Friendly;P.Display=A.ToString();if(!Current())return false;P.FriendlyDisplay=B.ToString();if(!Current())return false;}return true;}
    static FGuid PinId(UEdGraphPin* P,const FGraph& G){for(const auto& N:G.Nodes)for(const auto& Q:N.Pins)if(Q.Address==P)return Q.Id;return FGuid();}
    static TArray<FGuid> Ids(const TArray<UEdGraphPin*>& Pins,const FGraph& G){TArray<FGuid> A;for(auto* P:Pins)A.Add(PinId(P,G));return A;}
    bool Logical(const FGraph& Old,const FGraph& Now)
    {
        if(Old.Hold.Get()!=Now.Hold.Get()||Old.Guid!=Now.Guid||Old.InterfaceId!=Now.InterfaceId||Old.AllowDelete!=Now.AllowDelete||!Old.Path.Equals(Now.Path,ESearchCase::CaseSensitive)||Old.Nodes.Num()!=Now.Nodes.Num())return Stop(TEXT("graph_semantics"),Old.Path,TEXT("changed"));
        for(const auto& N:Old.Nodes){const FNode* Found=Now.Nodes.FindByPredicate([&](const FNode& V){return V.Id==N.Id;});if(!Found)return Stop(TEXT("node_guid"),N.Id.ToString(),TEXT("missing"));const auto& M=*Found;if(N.Hold.Get()!=M.Hold.Get()||N.Class!=M.Class||!N.Path.Equals(M.Path,ESearchCase::CaseSensitive)||N.X!=M.X||N.Y!=M.Y||!N.Comment.Equals(M.Comment,ESearchCase::CaseSensitive)||!N.Ref.IsSameReference(M.Ref)||N.Extra!=M.Extra||N.Pins.Num()!=M.Pins.Num()||N.Users.Num()!=M.Users.Num())return Stop(TEXT("node_semantics"),N.Path,TEXT("changed"));for(int32 U=0;U<N.Users.Num();++U){const auto& E=N.Users[U];const auto& V=M.Users[U];if(E.Name!=V.Name)return Stop(TEXT("user_pin.name"),E.Name.ToString(),V.Name.ToString(),N.Path);if(!SameType(E.Type,V.Type))return Stop(TEXT("user_pin.type"),TypeEvidence(E.Type),TypeEvidence(V.Type),N.Path);if(E.Direction!=V.Direction)return Stop(TEXT("user_pin.direction"),LexToString(uint8(E.Direction)),LexToString(uint8(V.Direction)),N.Path);if(!E.Default.Equals(V.Default,ESearchCase::CaseSensitive))return Stop(TEXT("user_pin.default"),E.Default,V.Default,N.Path);}for(const auto& P:N.Pins){const FPin* Q=M.Pins.FindByPredicate([&](const FPin& V){return V.Id==P.Id;});if(!Q)return Stop(TEXT("pin.guid"),P.Id.ToString(),TEXT("missing"),N.Path);if(!PinCells(P,*Q,N.Path))return false;if(!Q||P.Name!=Q->Name||P.Direction!=Q->Direction||P.Persistent!=Q->Persistent||!SameType(P.Type,Q->Type)||P.Object!=Q->Object||P.Flags!=Q->Flags||!P.Raw.Equals(Q->Raw,ESearchCase::CaseSensitive)||!P.Auto.Equals(Q->Auto,ESearchCase::CaseSensitive)||!P.Display.Equals(Q->Display,ESearchCase::CaseSensitive)||!P.FriendlyDisplay.Equals(Q->FriendlyDisplay,ESearchCase::CaseSensitive)||FTextInspector::GetFlags(P.Text)!=FTextInspector::GetFlags(Q->Text)||FTextInspector::GetFlags(P.Friendly)!=FTextInspector::GetFlags(Q->Friendly)||Ids(P.Links,Old)!=Ids(Q->Links,Now)||PinId(P.Parent,Old)!=PinId(Q->Parent,Now)||Ids(P.Children,Old)!=Ids(Q->Children,Now))return Stop(TEXT("pin_semantics_remaining"),P.Id.ToString(),TEXT("copied display/persistent GUID/link/split mismatch; later cells unobserved"),N.Path);}}return true;
    }
    bool File(const FString& Filename,FString& Hash)
    {if(!Current())return false;const int64 Size=IFileManager::Get().FileSize(*Filename);TArray<uint8> Bytes;if(Size<1||Size>64*1024*1024||!FFileHelper::LoadFileToArray(Bytes,*Filename)||!Current()||Bytes.Num()!=Size)return Stop(TEXT("source_or_target_file"),TEXT("current readable bounded saved file"),Filename);FSHAHash H;FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),H.Hash);Hash=H.ToString();return true;}
    bool RootSnapshot(FString& Out)
    {
        if(WidgetTarget)
        {
            if(RichTree)
            {
                if(!Current())return false;Out=AuthoredTree.Runtime;
                for(const auto& Row:AuthoredTree.Objects){TreeToken(Out,Row.Relative);TreeToken(Out,Row.Values);}return true;
            }
            if(!Current())return false;Out.Reset();TArray<FProperty*> Fields;
            for(TFieldIterator<FProperty> It(UCanvasPanel::StaticClass(),EFieldIterationFlags::IncludeSuper);It;++It)
            {if(Fields.Num()>=1024)return Stop(TEXT("widget_root_fields"),TEXT("<=1024 native fields"),TEXT("larger"));if(!It->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated|CPF_SkipSerialization))Fields.Add(*It);}
            for(auto* P:Fields)
            {
                if(!Current())return false;TArray<FProperty*> Now;for(TFieldIterator<FProperty> It(UCanvasPanel::StaticClass(),EFieldIterationFlags::IncludeSuper);It;++It)Now.Add(*It);
                if(!Now.Contains(P)||P->HasSetterOrGetter()||P->ArrayDim!=1)return Stop(TEXT("widget_root_descriptor"),TEXT("same native scalar descriptor without accessor"),TEXT("different"));
                const FString Name=P->GetName();FString Value;const bool Done=P->ExportText_InContainer(0,Value,WidgetRoot.Get(),WidgetRoot.Get(),WidgetRoot.Get(),PPF_None);
                if(!Current()||!Done)return Stop(TEXT("widget_root_export"),TEXT("returned current export"),TEXT("failed"));Out+=Name+TEXT(":")+LexToString(Value.Len())+TEXT(":")+Value;
                if(Out.Len()>1024*1024)return Stop(TEXT("widget_root_export_bound"),TEXT("<=1MiB"),TEXT("larger"));
            }return Current();
        }
        if(!Current())return false;Out.Reset();TArray<FProperty*> Fields;for(TFieldIterator<FProperty> It(USceneComponent::StaticClass(),EFieldIterationFlags::IncludeSuper);It;++It){if(Fields.Num()>=1024)return Stop(TEXT("root_fields"),TEXT("bounded reflected root"),TEXT("too many"));Fields.Add(*It);}for(auto* P:Fields){TArray<FProperty*> Actual;for(TFieldIterator<FProperty> It(USceneComponent::StaticClass(),EFieldIterationFlags::IncludeSuper);It;++It)Actual.Add(*It);if(!Actual.Contains(P))return Stop(TEXT("root_field_membership"),TEXT("same own native field"),TEXT("changed"));const FString FieldName=P->GetName();FString Value;const bool Done=P->ExportText_InContainer(0,Value,RootTemplate.Get(),RootTemplate.Get(),RootTemplate.Get(),PPF_None);if(!Current()||!Done)return Stop(TEXT("root_export"),TEXT("returned complete current native field export"),TEXT("failed"));Out+=FieldName+TEXT(":")+LexToString(Value.Len())+TEXT(":")+Value;if(Out.Len()>1024*1024)return Stop(TEXT("root_export_bound"),TEXT("<=1MiB"),TEXT("too large"));}return true;}
    bool SourceSnapshot(TSharedRef<FJsonObject> Out)
    {
        if(!Current())return false;const bool Implementable=FKismetEditorUtilities::IsClassABlueprintImplementableInterface(Interface.Get());if(!Current())return false;const bool Can=FKismetEditorUtilities::CanBlueprintImplementInterface(Target.Get(),Interface.Get());if(!Current())return false;
        Out->SetBoolField(TEXT("blueprint_implementable"),Implementable);Out->SetBoolField(TEXT("can_implement"),Can);Out->SetStringField(TEXT("source_blueprint"),SourcePath);Out->SetStringField(TEXT("interface_class"),InterfacePathValue);Out->SetBoolField(TEXT("generated_cdo_observed_null"),!SourceCDO.IsValid());Out->SetBoolField(TEXT("skeleton_cdo_observed_null"),!SourceSkelCDO.IsValid());TArray<TSharedPtr<FJsonValue>> FunctionRows;
        for(const auto& Method:Methods)
        {
            const bool Override=UEdGraphSchema_K2::CanKismetOverrideFunction(Method.Decl.F.Get());if(!Current())return false;const bool Event=UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(Method.Decl.F.Get());if(!Current())return false;const bool Anim=Method.Decl.F->HasMetaData(FBlueprintMetadata::MD_AnimBlueprintFunction);if(!Current())return false;
            if(PlanReady&&(Override!=Method.CanOverride||Event!=Method.AsEvent||Anim!=Method.Anim))return Stop(TEXT("function_eligibility"),TEXT("copied SDK override/event/Anim predicates"),TEXT("different"),Method.Name.ToString());
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("function"),Method.Decl.F->GetPathName());Row->SetStringField(TEXT("function_name"),Method.Name.ToString());Row->SetStringField(TEXT("function_guid"),Method.Guid.ToString());Row->SetStringField(TEXT("function_flags"),LexToString(uint32(Method.Flags)));Row->SetBoolField(TEXT("can_override"),Override);Row->SetBoolField(TEXT("can_place_as_event"),Event);Row->SetBoolField(TEXT("anim_function"),Anim);Row->SetBoolField(TEXT("requires_interface_graph"),(Override&&!Event)||Anim);Row->SetBoolField(TEXT("has_output_parameter"),Method.HasOutput);Row->SetStringField(TEXT("compiled_parameter_defaults"),Method.DeclDefaults);Row->SetStringField(TEXT("skeleton_parameter_defaults"),Method.SkelDefaults);
            TArray<TSharedPtr<FJsonValue>> Parameters;for(const auto& Param:Method.Params){auto Cell=MakeShared<FJsonObject>();Cell->SetStringField(TEXT("name"),Param.Name.ToString());Cell->SetBoolField(TEXT("is_output"),Param.Out);Cell->SetStringField(TEXT("category"),Param.Type.PinCategory.ToString());Cell->SetStringField(TEXT("subcategory"),Param.Type.PinSubCategory.ToString());Parameters.Add(MakeShared<FJsonValueObject>(Cell));}Row->SetArrayField(TEXT("parameters"),Parameters);FunctionRows.Add(MakeShared<FJsonValueObject>(Row));
            if(Methods.Num()==1){for(const auto& Pair:Row->Values)Out->SetField(Pair.Key,Pair.Value);}
        }
        Out->SetArrayField(TEXT("functions"),FunctionRows);return true;
    }
    bool Projection(const FMethod& Method,const FGraph& G)
    {
        const auto& Params=Method.Params;const auto& PlannedReference=Method.PlannedReference;const auto FunctionFlags=Method.Flags;const FName FunctionName=Method.Name;const FGuid InterfaceGuid=Method.Guid;
        const int32 ExpectedNodes=Method.HasOutput?2:1;if(!Method.GraphRequired||G.Hold->GetOuter()!=Target.Get()||G.Hold->GetFName()!=FunctionName||G.InterfaceId!=InterfaceGuid||G.AllowDelete||!G.Guid.IsValid()||G.Nodes.Num()!=ExpectedNodes)return Stop(TEXT("interface_graph_projection"),FString::Printf(TEXT("own named graph; source GUID; nondeletable Entry1/Result%d"),Method.HasOutput?1:0),G.Path);
        const FNode* Entry=nullptr;const FNode* Result=nullptr;for(const auto& Node:G.Nodes){if(Node.Class==UK2Node_FunctionEntry::StaticClass()&&!Entry)Entry=&Node;else if(Node.Class==UK2Node_FunctionResult::StaticClass()&&!Result)Result=&Node;else return Stop(TEXT("interface_terminators"),TEXT("derived Entry/optional Result roles"),Node.Path);}
        if(!Entry||(Method.HasOutput?!Result:Result!=nullptr)||!Entry->Ref.IsSameReference(PlannedReference)||(Result&&!Result->Ref.IsSameReference(PlannedReference))||!Entry->Users.IsEmpty()||(Result&&!Result->Users.IsEmpty()))return Stop(TEXT("interface_function_binding"),TEXT("source external member/GUID and inherited signature pins"),TEXT("different"),FunctionName.ToString());
        const uint32 Mask=FUNC_Const|FUNC_Static|FUNC_BlueprintPure;if((uint32(Entry->Extra)&Mask)!=(uint32(FunctionFlags)&Mask))return Stop(TEXT("entry_flags_projection"),LexToString(uint32(FunctionFlags)&Mask),LexToString(uint32(Entry->Extra)&Mask));
        int32 Data=0;const FPin* Then=nullptr;const FPin* Execute=nullptr;TSet<FName> EntryNames,ResultNames;
        for(const FNode* Node:{Entry,Result})if(Node)for(const auto& Pin:Node->Pins)
        {
            if(Pin.Type.PinCategory==UEdGraphSchema_K2::PC_Exec){if(Node==Entry&&Pin.Direction==EGPD_Output&&Pin.Name==UEdGraphSchema_K2::PN_Then&&!Then)Then=&Pin;else if(Node==Result&&Pin.Direction==EGPD_Input&&Pin.Name==UEdGraphSchema_K2::PN_Execute&&!Execute)Execute=&Pin;else return Stop(TEXT("interface_exec_role"),TEXT("canonical Then/Execute roles"),Pin.Name.ToString());continue;}
            const FParam* Expected=Params.FindByPredicate([&](const FParam& Value){return Value.Name==Pin.Name&&Value.Out==(Node==Result);});auto& Names=Node==Entry?EntryNames:ResultNames;
            if(!Expected||Names.Contains(Pin.Name)||Pin.Direction!=(Node==Entry?EGPD_Output:EGPD_Input)||!SameType(Pin.Type,Expected->Type)||!Pin.Links.IsEmpty()||Pin.Parent||!Pin.Children.IsEmpty())return Stop(TEXT("interface_data_projection"),TEXT("unique source parameter name/type/direction, unlinked whole data"),Pin.Name.ToString());Names.Add(Pin.Name);
            const FString Default=Pin.Type.PinCategory==UEdGraphSchema_K2::PC_Int||Pin.Type.PinCategory==UEdGraphSchema_K2::PC_Int64?TEXT("0"):Pin.Type.PinCategory==UEdGraphSchema_K2::PC_Real?TEXT("0.0"):Pin.Type.PinCategory==UEdGraphSchema_K2::PC_Boolean?TEXT("false"):Pin.Type.PinCategory==UEdGraphSchema_K2::PC_Name?TEXT("None"):FString();
            if(!Pin.Raw.Equals(Default,ESearchCase::CaseSensitive)||!Pin.Auto.Equals(Default,ESearchCase::CaseSensitive)||Pin.Object||!Pin.Text.IsEmpty())return Stop(TEXT("interface_stock_terminator_default"),Default,Pin.Raw+TEXT(" / ")+Pin.Auto,Pin.Name.ToString());++Data;
        }
        if(Data!=Params.Num())return Stop(TEXT("interface_parameter_count"),LexToString(Params.Num()),LexToString(Data));
        // These native terminators allocate exec pins even for a pure signature; the function flags stay separate.
        if(!Then||(Method.HasOutput?(!Execute||Then->Links.Num()!=1||Execute->Links.Num()!=1||Then->Links[0]!=Execute->Address||Execute->Links[0]!=Then->Address):(!Then->Links.IsEmpty()||Execute)))return Stop(TEXT("interface_exec_projection"),Method.HasOutput?TEXT("Entry.Then <-> Result.Execute only"):TEXT("Entry.Then unlinked without Result"),TEXT("different"));return true;
    }

    bool Reacquire(bool RequireProjection)
    {
        // Class and graph return roles are held before observation. Old compiler pins/classes are not re-read.
        if(!Current())return false;if(!Target->GeneratedClass.GetGCPtr().IsResolved()||!Target->SkeletonGeneratedClass.GetGCPtr().IsResolved())return Stop(TEXT("returned_class_slots"),TEXT("resolved current"),TEXT("unavailable"));TStrongObjectPtr<UClass> G(Target->GeneratedClass.Get()),K(Target->SkeletonGeneratedClass.Get());if(!ClassRole(G.Get(),Target.Get(),TargetParent(),GeneratedPath)||!ClassRole(K.Get(),Target.Get(),TargetParent(),SkeletonPath))return Stop(TEXT("returned_classes"),TEXT("own generated/skeleton logical roles"),TEXT("different"));TStrongObjectPtr<UObject> D(G->ClassDefaultObject.Get()),SD(K->ClassDefaultObject.Get());if(!CDORole(D.Get(),G.Get())||!CDORole(SD.Get(),K.Get()))return Stop(TEXT("returned_CDO"),TEXT("nullable actual class/default role"),TEXT("different"));
        Retired.Emplace(Generated.Get());Retired.Emplace(Skeleton.Get());if(CDO.IsValid())Retired.Emplace(CDO.Get());if(SkelCDO.IsValid())Retired.Emplace(SkelCDO.Get());Generated=MoveTemp(G);Skeleton=MoveTemp(K);CDO=MoveTemp(D);SkelCDO=MoveTemp(SD);
        if(WidgetTarget&&RichTree)
        {
            TArray<FCompiledTree> NewTrees;if(!CaptureCompiledTrees(NewTrees))return Stop(TEXT("returned_compiled_widget_tree"),TEXT("actual current owned nullable compiled trees"),TEXT("unavailable"));
            if(NewTrees.Num()!=CompiledTrees.Num())return Stop(TEXT("returned_compiled_tree_inventory"),LexToString(CompiledTrees.Num()),LexToString(NewTrees.Num()));
            for(int32 I=0;I<NewTrees.Num();++I)
            {
                const auto& N=NewTrees[I];const auto& E=CompiledTrees[I];
                if(N.Root.IsValid()&&N.State.Runtime!=AuthoredTree.Runtime&&(!E.Root.IsValid()||N.State.Runtime!=E.State.Runtime))return Stop(TEXT("returned_compiled_tree_projection"),TEXT("prior runtime cache or protected authored source projection"),TEXT("different"));
                if(!N.Root.IsValid()&&E.Root.IsValid())return Stop(TEXT("returned_compiled_tree_root"),TEXT("existing root remains present"),TEXT("lost"));
            }CompiledTrees=MoveTemp(NewTrees);
        }
        ClassesReady=true;
        TSet<FName> FirstNew;int32 GraphSlot=0;
        for(auto& Method:Methods)
        {
            if(!Method.GraphRequired)continue;TStrongObjectPtr<UEdGraph> Returned;
            if(ExpectedInterfaces){const auto& Description=Target->ImplementedInterfaces[0];if(!Description.Graphs.IsValidIndex(GraphSlot)||!Description.Graphs[GraphSlot].IsResolved())return Stop(TEXT("returned_interface_graph"),Method.Name.ToString(),TEXT("missing/unresolved"));Returned.Reset(Description.Graphs[GraphSlot].Get());}
            else Returned.Reset(FindObject<UEdGraph>(Target.Get(),*Method.Name.ToString()));++GraphSlot;
            auto Prefix=R.Data->HasField(TEXT("interface_graph_return_prefix"))?R.Data->GetArrayField(TEXT("interface_graph_return_prefix")):TArray<TSharedPtr<FJsonValue>>();auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("function_name"),Method.Name.ToString());Row->SetStringField(TEXT("returned_identity"),LexToString(reinterpret_cast<UPTRINT>(Returned.Get())));Row->SetBoolField(TEXT("present"),Returned.IsValid());Prefix.Add(MakeShared<FJsonValueObject>(Row));R.Data->SetArrayField(TEXT("interface_graph_return_prefix"),Prefix);
            if(Method.HasNew&&Returned.Get()!=Method.NewGraph.Hold.Get())return Stop(TEXT("returned_graph_slot"),Method.NewGraph.Path,TEXT("different"));if(Returned.IsValid()&&!Live(Returned.Get()))return Stop(TEXT("returned_graph_lifetime"),Method.Name.ToString(),TEXT("unavailable"));
            if(Returned.IsValid()){if(!Capture(Returned.Get(),Method.CandidateNew))return false;if(!Method.HasNew){FirstNew.Add(Method.Name);Method.HasNew=true;Method.NewGraph.Hold.Reset(Returned.Get());}}else if(RequireProjection)return Stop(TEXT("returned_graph_missing"),Method.Name.ToString(),TEXT("null"));
        }
        if(!Current())return false;if(BeforeGraph.Hold.IsValid()&&!Capture(BeforeGraph.Hold.Get(),CandidateBefore))return false;if(Construction.Hold.IsValid()&&!Capture(Construction.Hold.Get(),CandidateConstruction))return false;CandidatesReady=true;
        if(!Current()||!Texts(CandidateBefore)||(Construction.Hold.IsValid()&&!Texts(CandidateConstruction)))return false;for(auto& Method:Methods)if(Method.HasNew&&!Texts(Method.CandidateNew))return false;
        if((BeforeGraph.Hold.IsValid()&&!Logical(BeforeGraph,CandidateBefore))||(Construction.Hold.IsValid()&&!Logical(Construction,CandidateConstruction)))return false;
        for(auto& Method:Methods)if(Method.HasNew){if(!FirstNew.Contains(Method.Name)&&!Logical(Method.NewGraph,Method.CandidateNew))return false;if(RequireProjection&&!Projection(Method,Method.CandidateNew))return false;}
        if(BeforeGraph.Hold.IsValid())BeforeGraph=MoveTemp(CandidateBefore);if(Construction.Hold.IsValid())Construction=MoveTemp(CandidateConstruction);for(auto& Method:Methods)if(Method.HasNew)Method.NewGraph=MoveTemp(Method.CandidateNew);CandidatesReady=false;Rebinding=false;ClassesReady=false;return Current();
    }

    static bool ScalarField(const FProperty* Field)
    {const auto* Kind=Field->GetClass();return Kind==FIntProperty::StaticClass()||Kind==FInt64Property::StaticClass()||Kind==FFloatProperty::StaticClass()||Kind==FDoubleProperty::StaticClass()||Kind==FBoolProperty::StaticClass()||Kind==FStrProperty::StaticClass()||Kind==FNameProperty::StaticClass()||Kind==FTextProperty::StaticClass();}
    bool BindFunction(UFunction* Function,UClass* Owner,FFunctionRole& Role)
    {
        if(!Live(Function)||Function->GetClass()!=UFunction::StaticClass()||Function->GetOuter()!=Owner)return Reject(TEXT("game_interface_function_role_pending"),TEXT("Live own exact compiled UFunction required."));
        for(TFieldIterator<FProperty> It(Function,EFieldIterationFlags::None);It;++It)if(!ScalarField(*It))return Reject(TEXT("game_interface_parameter_type_pending"),TEXT("Existing primitive scalar field classes only, before layout/accessor observation; containers/object/reference/Anim remain incomplete."));
        Role.F.Reset(Function);Role.Owner=Owner;Role.Name=Function->GetFName();Role.Flags=Function->FunctionFlags;if(!FieldReader.FieldRows(Function,Role.Fields))return Reject(TEXT("game_interface_fields_pending"),TEXT("Current bounded direct reflected parameter fields required."));return true;
    }
    bool Bind(const FString& RequestedTarget,const FString& RequestedInterface)
    {
        R.Data->SetBoolField(TEXT("generated_game_interface"),true);R.Data->SetStringField(TEXT("requested_interface"),RequestedInterface);R.Data->SetNumberField(TEXT("explicit_compile_calls"),0);R.Data->SetNumberField(TEXT("explicit_save_calls"),0);
        if(!FWidgetInterfaceWork::Reference(RequestedTarget)||!FWidgetInterfaceWork::Reference(RequestedInterface)||!RequestedTarget.StartsWith(TEXT("/Game/"))||!RequestedInterface.StartsWith(TEXT("/Game/")))return Reject(TEXT("game_interface_reference_pending"),TEXT("Bounded explicit non-MCP loaded Game source/target references required."));
        Editor.Reset(GEditor);if(!IsInGameThread()||!Live(Editor.Get())||IsPlaySessionActive()||IsGarbageCollecting()||UE::IsSavingPackage())return Reject(TEXT("game_interface_editor_invalid"),TEXT("Idle Editor GT required."));World.Reset(GEditor->GetEditorWorldContext().World());if(!Context())return Reject(TEXT("game_interface_editor_invalid"),TEXT("Stable idle Editor required."));
        const FString ObjectPath=RequestedTarget.Contains(TEXT("."))?RequestedTarget:RequestedTarget+TEXT(".")+FPackageName::GetLongPackageAssetName(RequestedTarget);Target.Reset(FindObject<UBlueprint>(nullptr,*ObjectPath));Interface.Reset(FindObject<UClass>(nullptr,*RequestedInterface));
        if(!Interface.IsValid()){const FString Asset=RequestedInterface.Contains(TEXT("."))?RequestedInterface:RequestedInterface+TEXT(".")+FPackageName::GetLongPackageAssetName(RequestedInterface);TStrongObjectPtr<UBlueprint> Found(FindObject<UBlueprint>(nullptr,*Asset));if(Live(Found.Get())&&Found->GeneratedClass.GetGCPtr().IsResolved())Interface.Reset(Found->GeneratedClass.Get());}
        if(!Live(Interface.Get())||Interface->GetClass()!=UBlueprintGeneratedClass::StaticClass()||!Interface->ClassGeneratedBy.IsResolved())return Reject(TEXT("game_interface_loaded_source_required"),TEXT("Already loaded compiled Game Blueprint interface source required; no loader or CDO warming."));Source.Reset(Cast<UBlueprint>(Interface->ClassGeneratedBy.Get()));if(!Live(Source.Get())||!Live(Target.Get())||Source.Get()==Target.Get()||Source->GetPackage()==Target->GetPackage())return Reject(TEXT("game_interface_source_target_pending"),TEXT("Distinct loaded direct Game BPI and normal Actor/UserWidget assets required."));
        auto* S=Source.Get();auto* B=Target.Get();WidgetTarget=B->GetClass()==UWidgetBlueprint::StaticClass();SourcePath=S->GetPathName();TargetPath=B->GetPathName();InterfacePathValue=Interface->GetPathName();
        if(S->GetClass()!=UBlueprint::StaticClass()||S->BlueprintType!=BPTYPE_Interface||S->Status!=BS_UpToDate||S->GetPackage()->IsDirty()||!S->ParentClass.GetGCPtr().IsResolved()||S->ParentClass.Get()!=UInterface::StaticClass()||!S->SkeletonGeneratedClass.GetGCPtr().IsResolved()||S->FunctionGraphs.Num()>32||!S->UbergraphPages.IsEmpty()||!S->MacroGraphs.IsEmpty()||!S->DelegateSignatureGraphs.IsEmpty()||!S->ImplementedInterfaces.IsEmpty()||!S->NewVariables.IsEmpty())return Reject(TEXT("game_interface_source_profile_pending"),TEXT("Saved-clean UpToDate direct-UInterface BPI with bounded own signature graphs; inherited/custom/Anim sources remain incomplete."));SourceSkeleton.Reset(S->SkeletonGeneratedClass.Get());SourceSkeletonPath=SourceSkeleton.IsValid()?SourceSkeleton->GetPathName():FString();
        if(!ClassRole(Interface.Get(),S,UInterface::StaticClass(),InterfacePathValue)||!ClassRole(SourceSkeleton.Get(),S,UInterface::StaticClass(),SourceSkeletonPath)||!Interface->HasAnyClassFlags(CLASS_Interface)||!SourceSkeleton->HasAnyClassFlags(CLASS_Interface))return Reject(TEXT("game_interface_source_class_pending"),TEXT("Exact own generated/skeleton interface roles required."));SourceCDO.Reset(Interface->ClassDefaultObject.Get());SourceSkelCDO.Reset(SourceSkeleton->ClassDefaultObject.Get());if(!CDORole(SourceCDO.Get(),Interface.Get())||!CDORole(SourceSkelCDO.Get(),SourceSkeleton.Get()))return Reject(TEXT("game_interface_source_CDO_role"),TEXT("Observed nullable interface CDO slots must retain actual role; no forced CDO allocation."));
        for(UClass* Class:{Interface.Get(),SourceSkeleton.Get()})for(TFieldIterator<UFunction> It(Class,EFieldIterationFlags::None);It;++It){auto& Order=Class==Interface.Get()?DeclOrder:SkelOrder;if(Order.Num()>=32)return Reject(TEXT("game_interface_function_bound"),TEXT("At most32 own functions; no literal sample count."));Order.Emplace(*It);}
        if(DeclOrder.Num()!=SkelOrder.Num()||DeclOrder.Num()!=S->FunctionGraphs.Num())return Reject(TEXT("game_interface_function_graph_inventory_pending"),TEXT("Each own compiled/skeleton declaration must have one source signature graph."));SourceFunctions=S->FunctionGraphs;TSet<FName> Names;TSet<UEdGraph*> UsedGraphs;
        for(const auto& Function:DeclOrder)
        {
            FMethod Method;if(!BindFunction(Function.Get(),Interface.Get(),Method.Decl))return false;Method.Name=Method.Decl.Name;Method.Flags=Method.Decl.Flags;
            const auto* Other=SkelOrder.FindByPredicate([&](const TStrongObjectPtr<UFunction>& Value){return Live(Value.Get())&&Value->GetFName()==Method.Name;});if(!Other||!BindFunction(Other->Get(),SourceSkeleton.Get(),Method.SkelDecl)||Names.Contains(Method.Name)||Method.Decl.Fields.Num()>16||Method.Decl.Fields.Num()!=Method.SkelDecl.Fields.Num())return Reject(TEXT("game_interface_signature_pending"),TEXT("Unique matching bounded own primitive parameter signatures required."));Names.Add(Method.Name);
            UEdGraph* Match=nullptr;for(const auto& Slot:SourceFunctions){if(!Slot.IsResolved()||!Live(Slot.Get()))return Reject(TEXT("game_interface_source_graph_pending"),TEXT("Resolved live signature graph slots required."));if(Slot->GetFName()==Method.Name){if(Match)return Reject(TEXT("game_interface_source_graph_pending"),TEXT("Unique named signature graph required."));Match=Slot.Get();}}
            if(!Match||UsedGraphs.Contains(Match)||Match->GetOuter()!=S)return Reject(TEXT("game_interface_source_graph_pending"),TEXT("One own signature graph per compiled function required."));if(!Capture(Match,Method.SourceGraph))return false;UsedGraphs.Add(Match);Method.DeclDefaults=Defaults(Method.Decl);Method.SkelDefaults=Defaults(Method.SkelDecl);Methods.Add(MoveTemp(Method));
        }
        for(TFieldIterator<UFunction> It(Interface.Get(),EFieldIteratorFlags::IncludeSuper);It;++It){if(SDKOrder.Num()>=64)return Reject(TEXT("game_interface_sdk_function_bound"),TEXT("Bounded declared/inherited SDK iteration required."));SDKOrder.Emplace(*It);UClass* Owner=It->GetOuterUClass();if(Owner!=Interface.Get()){if(!Live(Owner)||(Owner!=UInterface::StaticClass()&&Owner!=UObject::StaticClass()))return Reject(TEXT("game_interface_inherited_source_pending"),TEXT("Only existing native UInterface/UObject inherited source roles are admitted."));Holds.Emplace(Owner);FFunctionRole Role;if(!BindFunction(*It,Owner,Role))return false;InheritedOwnerFlags.Add(Owner->ClassFlags);InheritedDefaults.Add(Defaults(Role));InheritedFunctions.Add(MoveTemp(Role));}}
        if(B->GetClass()!=TargetAssetClass()||B->BlueprintType!=BPTYPE_Normal||B->Status!=BS_UpToDate||B->GetPackage()->IsDirty()||!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.Get()!=TargetParent()||!B->GeneratedClass.GetGCPtr().IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||(WidgetTarget?B->NewVariables.Num()>64:!B->NewVariables.IsEmpty())||B->FunctionGraphs.Num()>(WidgetTarget?0:1)||!B->MacroGraphs.IsEmpty()||!B->DelegateSignatureGraphs.IsEmpty()||B->UbergraphPages.Num()>1||(!WidgetTarget&&B->UbergraphPages.Num()!=1)||!B->Timelines.IsEmpty()||B->ImplementedInterfaces.Num()>1)
            return Reject(TEXT("game_interface_target_profile_pending"),TEXT("Saved-clean direct Actor/default SCS or direct UserWidget/known native Canvas-Button-Text tree with at most one default EventGraph; only captured widget descriptors, no user functions/macros/delegates/timelines. Wider valid owners remain incomplete."));
        Generated.Reset(B->GeneratedClass.Get());Skeleton.Reset(B->SkeletonGeneratedClass.Get());if(!Live(Generated.Get())||!Live(Skeleton.Get()))return Reject(TEXT("game_interface_target_classes_pending"),TEXT("Existing current generated/skeleton roles required."));GeneratedPath=Generated->GetPathName();SkeletonPath=Skeleton->GetPathName();TargetGeneratedMeta=Generated->GetClass();TargetSkeletonMeta=Skeleton->GetClass();if(WidgetTarget&&(TargetGeneratedMeta!=UWidgetBlueprintGeneratedClass::StaticClass()||TargetSkeletonMeta!=UWidgetBlueprintGeneratedClass::StaticClass()))return Reject(TEXT("game_interface_widget_compiler_class_pending"),TEXT("Installed standard Widget generated/skeleton metaclasses required."));if(!ClassRole(Generated.Get(),B,TargetParent(),GeneratedPath)||!ClassRole(Skeleton.Get(),B,TargetParent(),SkeletonPath))return Reject(TEXT("game_interface_target_classes_pending"),TEXT("Current own generated/skeleton/CDO roles required."));CDO.Reset(Generated->ClassDefaultObject.Get());SkelCDO.Reset(Skeleton->ClassDefaultObject.Get());if(!CDORole(CDO.Get(),Generated.Get())||!CDORole(SkelCDO.Get(),Skeleton.Get()))return Reject(TEXT("game_interface_target_CDO_role"),TEXT("Nullable existing target CDO roles required."));
        if(WidgetTarget)
        {
            auto* W=static_cast<UWidgetBlueprint*>(B);
            if(!W->WidgetTree.IsResolved()||!Live(W->WidgetTree.Get())||W->WidgetTree->GetOuter()!=W||!W->Bindings.IsEmpty()||!W->Animations.IsEmpty())return Reject(TEXT("game_interface_widget_source_pending"),TEXT("Own resolved WidgetTree without bindings/animations required."));
            WidgetTree.Reset(W->WidgetTree.Get());WidgetTreePath=WidgetTree->GetPathName();
            if(!WidgetTree->RootWidget.IsResolved()||!Live(WidgetTree->RootWidget.Get())||WidgetTree->RootWidget->GetClass()!=UCanvasPanel::StaticClass())return Reject(TEXT("game_interface_widget_root_pending"),TEXT("Existing native Canvas root required; custom/non-admitted widget trees remain incomplete."));
            WidgetRoot.Reset(static_cast<UCanvasPanel*>(WidgetTree->RootWidget.Get()));WidgetRootPath=WidgetRoot->GetPathName();WidgetGuids=W->WidgetVariableNameToGuidMap;
            if(WidgetRoot->GetOuter()!=WidgetTree.Get()||!WidgetRoot->Slot.IsResolved()||WidgetRoot->Slot.Get())return Reject(TEXT("game_interface_widget_root_pending"),TEXT("Own native Canvas with null root Slot required."));
            RootChildren=WidgetRoot->GetChildrenCount();RichTree=RootChildren>0;
            if(RootChildren<0||RootChildren>64||(!RichTree&&!B->NewVariables.IsEmpty()))return Reject(TEXT("game_interface_widget_tree_pending"),TEXT("Existing empty Canvas profile or bounded Canvas/Button/Text source tree; wider valid owners remain incomplete."));
            if(RichTree)
            {
                if(!ReadWidgetTree(WidgetTree.Get(),AuthoredTree))return Reject(TEXT("game_interface_widget_tree_pending"),TEXT("Existing ACH reader requires bounded exact Canvas/Button/Text and native slots with closed ownership and observable authored values; custom/opaque/bound variants remain incomplete."));
                for(const auto& D:B->NewVariables)
                {
                    const auto* Widget=AuthoredTree.Objects.FindByPredicate([&](const FTreeObject& O){return O.Object->IsA<UWidget>()&&O.Object->GetFName()==D.VarName;});
                    if(!Widget||D.VarType.PinCategory!=UEdGraphSchema_K2::PC_Object||D.VarType.ContainerType!=EPinContainerType::None||D.VarType.bIsReference||!D.VarType.PinSubCategoryObject.IsValid()||D.VarType.PinSubCategoryObject.Get()!=Widget->Class.Get())return Reject(TEXT("game_interface_widget_descriptor_pending"),TEXT("Only existing known native widget descriptors are admitted; arbitrary user variables remain incomplete."));
                }
                CompiledTreeField=FindFProperty<FObjectProperty>(UWidgetBlueprintGeneratedClass::StaticClass(),TEXT("WidgetTree"));
                if(!CompiledTreeField)return Reject(TEXT("game_interface_widget_compiled_tree_pending"),TEXT("Existing native compiled WidgetTree descriptor required; no private member access."));
                CompiledTreeOffset=CompiledTreeField->GetOffset_ForInternal();
                if(!CaptureCompiledTrees(CompiledTrees))return Reject(TEXT("game_interface_widget_compiled_tree_pending"),TEXT("Actual separate nullable compiled WidgetTree/root roles required."));
                TreeReady=true;
            }
            WidgetVariables=B->NewVariables;
            Holds.Emplace(WidgetTree.Get());Holds.Emplace(WidgetRoot.Get());Holds.Emplace(UUserWidget::StaticClass());Holds.Emplace(UWidgetBlueprintGeneratedClass::StaticClass());
        }
        else
        {
        if(!B->SimpleConstructionScript.IsResolved())return Reject(TEXT("game_interface_target_SCS_pending"),TEXT("Already resolved Actor SCS required; no loader."));
        SCS.Reset(B->SimpleConstructionScript.Get());if(!Live(SCS.Get())||SCS->GetAllNodes().Num()!=1||SCS->GetRootNodes().Num()!=1||SCS->GetAllNodes()[0]!=SCS->GetRootNodes()[0])return Reject(TEXT("game_interface_target_SCS_pending"),TEXT("One own native Scene root required."));SCSOuter=SCS->GetOuter();RootNode.Reset(SCS->GetAllNodes()[0]);if(!Live(RootNode.Get())||!RootNode->ComponentClass.IsResolved()||RootNode->ComponentClass.Get()!=USceneComponent::StaticClass()||!RootNode->ComponentTemplate.IsResolved()||!RootNode->GetChildNodes().IsEmpty())return Reject(TEXT("game_interface_root_pending"),TEXT("Lone inactive native Scene root required."));RootTemplate.Reset(Cast<USceneComponent>(RootNode->ComponentTemplate.Get()));if(!Live(RootTemplate.Get())||RootTemplate->IsRegistered()||RootTemplate->GetOwner()||RootTemplate->GetWorld())return Reject(TEXT("game_interface_root_pending"),TEXT("Own inactive native Scene template required."));RootGuid=RootNode->VariableGuid;RootName=RootNode->GetVariableName();RootPath=RootTemplate->GetPathName();
        }
        if(!B->UbergraphPages.IsEmpty())
        {
            if(!B->UbergraphPages[0].IsResolved()||!Capture(B->UbergraphPages[0].Get(),BeforeGraph))return false;
            for(const auto& Node:BeforeGraph.Nodes)if(Node.Class!=UK2Node_Event::StaticClass())return Reject(TEXT("game_interface_target_graph_pending"),TEXT("Existing unlinked stock default Event nodes only; richer graphs remain incomplete."));
            for(const auto& Node:BeforeGraph.Nodes)for(const auto& Pin:Node.Pins)if(!Pin.Links.IsEmpty())return Reject(TEXT("game_interface_target_graph_pending"),TEXT("Unlinked original default events required."));
        }
        BeforeFunctions=B->FunctionGraphs;if(!BeforeFunctions.IsEmpty()){if(!BeforeFunctions[0].IsResolved()||BeforeFunctions[0]->GetFName()!=UEdGraphSchema_K2::FN_UserConstructionScript||!Capture(BeforeFunctions[0].Get(),Construction)||Construction.Nodes.Num()!=1||Construction.Nodes[0].Class!=UK2Node_FunctionEntry::StaticClass())return Reject(TEXT("game_interface_UCS_pending"),TEXT("Only actual empty stock UCS graph is admitted alongside the default EventGraph."));for(const auto& Pin:Construction.Nodes[0].Pins)if(!Pin.Links.IsEmpty())return Reject(TEXT("game_interface_UCS_pending"),TEXT("Stock UCS must have no authored links or body nodes."));}
        BeforeUber=B->UbergraphPages;ExpectedInterfaces=B->ImplementedInterfaces.Num();WasImplemented=ExpectedInterfaces==1;SavePolicy.Reset(FindObject<UBlueprintEditorSettings>(nullptr,TEXT("/Script/BlueprintGraph.Default__BlueprintEditorSettings")));if(!Live(SavePolicy.Get())||SavePolicy->SaveOnCompile!=SoC_Never)return Reject(TEXT("game_interface_save_policy_unsafe"),TEXT("Existing SaveOnCompile=Never required; settings are not changed."));ExposeDeprecated=SavePolicy->bExposeDeprecatedFunctions;for(UClass* Class:{UInterface::StaticClass(),UObject::StaticClass(),TargetParent()})Holds.Emplace(Class);ClassPolicy=ReadClassPolicy();
        if(!Current()||!Texts(BeforeGraph)||(Construction.Hold.IsValid()&&!Texts(Construction)))return false;for(auto& Method:Methods)if(!Texts(Method.SourceGraph))return false;
        auto* SchemaClass=UEdGraphSchema_K2::StaticClass();if(!SchemaClass->ClassDefaultObject.IsResolved())return Reject(TEXT("game_interface_schema_unavailable"),TEXT("Existing resolved K2 schema CDO required."));TStrongObjectPtr<UObject> KeepSchema(SchemaClass->ClassDefaultObject.Get());if(!Live(KeepSchema.Get())||KeepSchema->GetClass()!=SchemaClass)return Reject(TEXT("game_interface_schema_unavailable"),TEXT("Exact native schema CDO required; no warming."));Holds.Emplace(KeepSchema.Get());const auto* Schema=static_cast<UEdGraphSchema_K2*>(KeepSchema.Get());
        for(auto& Method:Methods)
        {
            for(const auto& Field:Method.Decl.Fields)
            {
                auto* Property=Field.P;if(!ScalarField(Property)||Property->ArrayDim!=1||!Property->HasAnyPropertyFlags(CPF_Parm)||Property->HasAnyPropertyFlags(CPF_ReferenceParm))return Reject(TEXT("game_interface_parameter_type_pending"),TEXT("Primitive scalar by-value inputs/outputs only."));FEdGraphPinType Type;const bool Converted=Schema->ConvertPropertyToPinType(Property,Type);if(!Current())return false;if(!Converted||Type.ContainerType!=EPinContainerType::None||Type.bIsReference)return Reject(TEXT("game_interface_parameter_conversion_pending"),TEXT("Public scalar schema conversion required."));
                const auto* Other=Method.SkelDecl.Fields.FindByPredicate([&](const FField& Value){return Value.Name==Field.Name;});if(!Other||!ScalarField(Other->P)||Other->P->HasAnyPropertyFlags(CPF_ReferenceParm)||Other->P->HasAnyPropertyFlags(CPF_OutParm)!=Property->HasAnyPropertyFlags(CPF_OutParm))return Reject(TEXT("game_interface_skeleton_signature_pending"),TEXT("Generated/skeleton matching scalar parameter roles required."));FEdGraphPinType OtherType;const bool OtherConverted=Schema->ConvertPropertyToPinType(Other->P,OtherType);if(!Current())return false;if(!OtherConverted||!SameType(Type,OtherType))return Reject(TEXT("game_interface_skeleton_signature_pending"),TEXT("Actual generated/skeleton schema types must match."));const bool Output=Property->HasAnyPropertyFlags(CPF_OutParm);Method.Params.Add({Field.Name,Type,Output});Method.HasOutput|=Output;
            }
            Method.Guid=FBlueprintEditorUtils::FindInterfaceFunctionGuid(Method.Decl.F.Get(),Interface.Get());if(!Current())return false;if(!Method.Guid.IsValid()||Method.Guid!=Method.SourceGraph.Guid)return Reject(TEXT("game_interface_guid_pending"),TEXT("Public function GUID must identify its captured source signature graph."));Method.PlannedReference.SetExternalMember(Method.Name,Interface.Get(),Method.Guid);
            Method.CanOverride=UEdGraphSchema_K2::CanKismetOverrideFunction(Method.Decl.F.Get());if(!Current())return false;Method.AsEvent=UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(Method.Decl.F.Get());if(!Current())return false;Method.Anim=Method.Decl.F->HasMetaData(FBlueprintMetadata::MD_AnimBlueprintFunction);if(!Current())return false;if(Method.Anim)return Reject(TEXT("game_interface_anim_pending"),TEXT("Anim interface graphs require a different owner/schema; valid original scope remains incomplete."));Method.GraphRequired=Method.CanOverride&&!Method.AsEvent;if(Method.GraphRequired)++RequiredGraphs;
        }
        for(const auto& Role:InheritedFunctions){const bool Override=UEdGraphSchema_K2::CanKismetOverrideFunction(Role.F.Get());if(!Current())return false;const bool Event=UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(Role.F.Get());if(!Current())return false;const bool Anim=Role.F->HasMetaData(FBlueprintMetadata::MD_AnimBlueprintFunction);if(!Current())return false;if((Override&&!Event)||Anim)return Reject(TEXT("game_interface_inherited_graph_pending"),TEXT("Inherited generated/Anim interface graphs remain valid original scope incomplete."));}
        for(const auto& Method:Methods)if(!SDKOrder.ContainsByPredicate([&](const TStrongObjectPtr<UFunction>& Value){return Value.Get()==Method.Decl.F.Get();}))return Reject(TEXT("game_interface_sdk_iteration_pending"),TEXT("Each admitted own declaration must occur in the actual SDK IncludeSuper iteration."));
        Methods.Sort([&](const FMethod& A,const FMethod& B){const int32 First=SDKOrder.IndexOfByPredicate([&](const TStrongObjectPtr<UFunction>& Value){return Value.Get()==A.Decl.F.Get();});const int32 Second=SDKOrder.IndexOfByPredicate([&](const TStrongObjectPtr<UFunction>& Value){return Value.Get()==B.Decl.F.Get();});return First<Second;});
        PlanReady=true;if(!Current())return false;
        if(WasImplemented){const auto& Description=B->ImplementedInterfaces[0];int32 Slot=0;for(auto& Method:Methods)if(Method.GraphRequired){if(!Description.Graphs[Slot].IsResolved()||!Capture(Description.Graphs[Slot].Get(),Method.NewGraph))return false;Method.HasNew=true;++Slot;}}
        for(auto& Method:Methods){if(Method.HasNew){if(!Texts(Method.NewGraph)||!Projection(Method,Method.NewGraph))return false;}else if(Method.GraphRequired&&FindObject<UObject>(B,*Method.Name.ToString()))return Reject(TEXT("game_interface_function_name_conflict"),TEXT("Existing same-name target object conflicts; no writer dispatched."));}
        auto Snapshot=MakeShared<FJsonObject>();if(!SourceSnapshot(Snapshot))return false;if(!Snapshot->GetBoolField(TEXT("blueprint_implementable"))||!Snapshot->GetBoolField(TEXT("can_implement")))return Reject(TEXT("game_interface_public_eligibility_rejected"),TEXT("Actual public interface eligibility predicates rejected the source/owner."));R.Data->SetObjectField(TEXT("interface_source_before"),Snapshot);
        if(WidgetTarget)for(TObjectIterator<UUserWidget> It;It;++It)if(Live(*It)&&!It->IsTemplate()&&It->IsA(Generated.Get()))return Reject(TEXT("game_interface_live_widget_pending"),TEXT("Loaded target UserWidget instances remain outside the asset-only structural profile."));
        for(TObjectIterator<AActor> It;It;++It)if(Live(*It)&&!It->IsTemplate()&&It->IsA(Generated.Get()))return Reject(TEXT("game_interface_live_actor_pending"),TEXT("Loaded target Actor instances remain outside first asset-only profile."));for(TObjectIterator<UBlueprintGeneratedClass> It;It;++It)if(Live(*It)&&*It!=Generated.Get()&&*It!=Skeleton.Get()&&It->IsChildOf(Generated.Get()))return Reject(TEXT("game_interface_derived_target_pending"),TEXT("Loaded derived target classes remain incomplete."));
        SourceFile=FPackageName::LongPackageNameToFilename(S->GetPackage()->GetName(),FPackageName::GetAssetPackageExtension());TargetFile=FPackageName::LongPackageNameToFilename(B->GetPackage()->GetName(),FPackageName::GetAssetPackageExtension());if(!File(SourceFile,SourceHash)||!File(TargetFile,TargetHash)||!RootSnapshot(RootValues))return false;Ready=true;R.Data->SetBoolField(TEXT("game_interface_widget_target"),WidgetTarget);R.Data->SetStringField(TEXT("source_blueprint"),SourcePath);R.Data->SetStringField(TEXT("target_blueprint"),TargetPath);if(Methods.Num()==1)R.Data->SetStringField(TEXT("interface_function"),Methods[0].Name.ToString());R.Data->SetNumberField(TEXT("interface_function_count"),Methods.Num());R.Data->SetNumberField(TEXT("planned_interface_graph_count"),RequiredGraphs);R.Data->SetBoolField(TEXT("already_implemented_before"),WasImplemented);return Current();
    }

    FResult Run(const FString& RequestedTarget)
    {
        if(!Ready||!Current())return R;Writer=true;R.bMutationAttempted=true;R.bChangeStateKnown=false;R.Stage=TEXT("implement_game_interface");Rebinding=true;ClassesReady=false;const bool EngineReturn=FBlueprintEditorUtils::ImplementNewInterface(Target.Get(),Interface->GetClassPathName());R.Data->SetBoolField(TEXT("engine_return"),EngineReturn);R.Data->SetBoolField(TEXT("implement_returned"),true);R.Data->SetNumberField(TEXT("implement_interface_calls"),1);ExpectedInterfaces=(WasImplemented||EngineReturn)?1:0;if(!Reacquire(WasImplemented||EngineReturn))return R;
        // Preserve the original explicit structural mark even for false/already-present SDK returns.
        R.Stage=TEXT("explicit_structural");Rebinding=true;ClassesReady=false;R.Data->SetNumberField(TEXT("mark_structural_calls"),1);FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Target.Get());R.Data->SetBoolField(TEXT("mark_structural_returned"),true);R.Data->SetBoolField(TEXT("implicit_skeleton_compile_requested"),true);if(!Reacquire(WasImplemented||EngineReturn))return R;
        auto After=MakeShared<FJsonObject>();if(!SourceSnapshot(After))return R;R.Data->SetObjectField(TEXT("interface_source_after"),After);const auto& BeforeSource=R.Data->GetObjectField(TEXT("interface_source_before"));for(const auto& Pair:BeforeSource->Values){auto ExpectedRow=MakeShared<FJsonObject>();auto ActualRow=MakeShared<FJsonObject>();ExpectedRow->SetField(Pair.Key,Pair.Value);const auto* Found=After->Values.Find(Pair.Key);if(Found)ActualRow->SetField(Pair.Key,*Found);const FString Expected=Text(ExpectedRow),Actual=Text(ActualRow);if(!Expected.Equals(Actual,ESearchCase::CaseSensitive))return Stop(*Pair.Key,Expected,Actual,SourcePath),R;}if(BeforeSource->Values.Num()!=After->Values.Num())return Stop(TEXT("source_snapshot_key_count"),LexToString(BeforeSource->Values.Num()),LexToString(After->Values.Num()),SourcePath),R;
        FString SourceNow,TargetNow,RootNow;if(!File(SourceFile,SourceNow)||!File(TargetFile,TargetNow)||!RootSnapshot(RootNow))return R;if(!SourceNow.Equals(SourceHash,ESearchCase::CaseSensitive)||!TargetNow.Equals(TargetHash,ESearchCase::CaseSensitive)||!RootNow.Equals(RootValues,ESearchCase::CaseSensitive))return Stop(TEXT("source_target_preservation"),TEXT("same files/native root captured values"),TEXT("different")),R;
        bool Present=ExpectedInterfaces==1;int32 ObservedGraphs=0;TArray<TSharedPtr<FJsonValue>> GraphRows;
        for(const auto& Method:Methods){if(Method.GraphRequired)Present&=Method.HasNew;if(Method.HasNew){++ObservedGraphs;auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("function_name"),Method.Name.ToString());Row->SetStringField(TEXT("path"),Method.NewGraph.Path);Row->SetStringField(TEXT("graph_guid"),Method.NewGraph.Guid.ToString());Row->SetStringField(TEXT("interface_guid"),Method.NewGraph.InterfaceId.ToString());Row->SetNumberField(TEXT("node_count"),Method.NewGraph.Nodes.Num());GraphRows.Add(MakeShared<FJsonValueObject>(Row));}}
        auto Legacy=MakeShared<FJsonObject>();Legacy->SetBoolField(TEXT("success"),true);Legacy->SetStringField(TEXT("blueprint_name"),RequestedTarget);Legacy->SetStringField(TEXT("interface_class"),InterfacePathValue);Legacy->SetStringField(TEXT("interface_class_path"),Interface->GetClassPathName().ToString());Legacy->SetBoolField(TEXT("added_or_already_present"),EngineReturn);Legacy->SetBoolField(TEXT("dirty_after"),Target->GetPackage()->IsDirty());R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
        R.Data->SetBoolField(TEXT("interface_present_after"),Present);R.Data->SetNumberField(TEXT("interface_graph_count_after"),ObservedGraphs);R.Data->SetArrayField(TEXT("interface_graphs_after"),GraphRows);if(Methods.Num()==1&&Methods[0].HasNew){R.Data->SetStringField(TEXT("interface_graph_path"),Methods[0].NewGraph.Path);R.Data->SetStringField(TEXT("interface_graph_guid"),Methods[0].NewGraph.Guid.ToString());R.Data->SetStringField(TEXT("interface_guid"),Methods[0].NewGraph.InterfaceId.ToString());}R.Data->SetBoolField(TEXT("source_and_existing_graph_observed_unchanged"),true);R.Data->SetBoolField(TEXT("runtime_execution_verified"),false);R.Data->SetBoolField(TEXT("source_file_preserved"),true);R.Data->SetBoolField(TEXT("target_file_preserved"),true);
        R.bChanged=Target->GetPackage()->IsDirty()||(!WasImplemented&&(Present||ObservedGraphs>0));R.bChangeStateKnown=true;R.bSuccess=R.bPostconditionPassed=Present&&(WasImplemented?!EngineReturn:EngineReturn);R.Stage=R.bSuccess?TEXT("complete"):TEXT("interface_sdk_failed_with_actual_prefix");if(!R.bSuccess)Reject(TEXT("game_interface_implementation_failed"),TEXT("Actual SDK false/partial and original explicit structural prefix retained; no retry or cleanup."));R.Data->SetStringField(TEXT("game_interface_scope"),TEXT("Saved-clean loaded direct-UInterface Game BPI with finite own primitive by-value functions -> saved-clean direct Actor/default Scene root or direct UserWidget/empty Canvas or bounded native Canvas/Button/Text tree, with no/default unlinked events. Each actual override/event predicate determines interface graph0..N; each graph has Entry1 and Result only when the signature has output. Event signatures add no event/graph node. Source declarations/defaults/GUIDs and existing target graph/pin/root values remain guarded through SDK class/pin regeneration. No source load/CDO warming, full compile/save/runtime, inherited/container/reference/Anim/general owner parity. Populated Widget targets reuse ACH physical authored-tree/value/FText and variable-GUID protection with separate actual compiled trees; compiler returns allow only prior-cache or protected source runtime projection. Existing empty Canvas remains separate. No bindings/animations/arbitrary variables/user functions/live UI; custom/opaque/bound/wider trees remain incomplete and populated Game-BPI use awaits validation."));return R;
    }
};
static bool GameInterfaceRunning=false;

}

FResult ImplementBlueprintInterface(const FString& BlueprintPath,const FString& InterfaceClass)
{
    FResult R;
    if(GameInterfaceRunning){Fail(R,TEXT("game_interface_reentrant"),TEXT("Active Game BPI interface compound operation; no nested writer."));return R;}
    if(InterfaceClass.StartsWith(TEXT("/Game/")))
    {
        TGuardValue<bool> Active(GameInterfaceRunning,true);FGameInterfaceWork Work(R);
        if(!Work.Bind(BlueprintPath,InterfaceClass))return R;return Work.Run(BlueprintPath);
    }
    FWidgetInterfaceWork Work(R);if(!Work.Bind(BlueprintPath,InterfaceClass))return R;return Work.Run(BlueprintPath);
}

FResult ReadPlan(const FString& GraphPath)
{
    FResult R; FContext C; if (!Prepare(GraphPath, 0, 0, C, R)) { return R; }
    R.Data->SetBoolField(TEXT("callback_interface_present"), InterfacePresent(C));
    R.Data->SetStringField(TEXT("interface_path"), InterfacePath); R.Data->SetStringField(TEXT("callback_function_path"), C.Callback->GetPathName());
    R.Data->SetStringField(TEXT("basic_particle_struct_path"), BasicPath); R.Data->SetStringField(TEXT("graph_path"), C.Graph->GetPathName());
    Finish(C, R, true); return R;
}

FResult ImplementInterface(const FString& GraphPath)
{
    FResult R; FContext C; if (!Prepare(GraphPath, 0, 0, C, R)) { return R; }
    R.Data->SetStringField(TEXT("interface_path"), InterfacePath);
    if (InterfacePresent(C)) { R.Data->SetBoolField(TEXT("implicit_skeleton_compile_requested"), false); Finish(C, R, true); return R; }
    if (HasCallbackDeclarationConflict(C) || C.BP->SkeletonGeneratedClass->FindFunctionByName(FName(CallbackName)))
    { Fail(R, TEXT("particle_callback_name_conflict"), TEXT("A current own/inherited callback name already exists before interface attachment; no declaration was changed.")); return R; }
    if (!GuardStructuralSavePolicy(C, R)) { return R; }
    TStrongObjectPtr<UBlueprint> KeepAlive(C.BP); R.Stage = TEXT("implement_interface"); R.bMutationAttempted = true; C.BP->Modify();
    const bool EngineReturn = FBlueprintEditorUtils::ImplementNewInterface(C.BP, C.Interface->GetClassPathName());
    R.Data->SetBoolField(TEXT("engine_return"), EngineReturn); R.Data->SetBoolField(TEXT("implicit_skeleton_compile_requested"), EngineReturn);
    R.Data->SetStringField(TEXT("implicit_skeleton_compile_status"), TEXT("not_independently_verified; explicit full compile/save is a separate pipeline step"));
    R.Data->SetStringField(TEXT("structural_api"), TEXT("FBlueprintEditorUtils::ImplementNewInterface(FTopLevelAssetPath); native API requests structural skeleton compilation; no explicit full compile/save"));
    Finish(C, R, EngineReturn && InterfacePresent(C), nullptr, nullptr, nullptr, true); return R;
}

FResult AddEvent(const FString& GraphPath, int32 X, int32 Y)
{
    FResult R; FContext C; if (!Prepare(GraphPath, X, Y, C, R)) { return R; }
    if (!InterfacePresent(C)) { Fail(R, TEXT("particle_interface_missing"), TEXT("Implement the exact native callback interface before adding its event.")); return R; }
    if (HasCallbackDeclarationConflict(C))
    { Fail(R, TEXT("particle_callback_name_conflict"), TEXT("An owned function graph or custom event declares the callback name; no replacement or repair.")); return R; }
    UFunction* Override = nullptr; UClass* Owner = FBlueprintEditorUtils::GetOverrideFunctionClass(C.BP, CallbackName, &Override);
    if (Owner != C.Interface || Override != C.Callback) { Fail(R, TEXT("particle_override_identity_mismatch"), TEXT("Public override lookup did not return the exact native interface callback.")); return R; }
    UK2Node_Event* Existing = nullptr;
    for (UEdGraphNode* Node : C.Graph->Nodes)
    {
        UK2Node_Event* Event = Cast<UK2Node_Event>(Node);
        if (Event && Event->CustomFunctionName == FName(CallbackName))
        { Fail(R, TEXT("particle_callback_name_conflict"), TEXT("A custom event already declares the reserved callback name; no replacement or repair.")); return R; }
        if (Event && Event->EventReference.GetMemberName() == FName(CallbackName))
        { if (Existing || !EventMatches(C, Event, false)) { Fail(R, TEXT("particle_override_conflict"), TEXT("Existing callback event is duplicated or differs in exact native owner/signature; no repair.")); return R; } Existing = Event; }
    }
    if (Existing)
    {
        R.Data->SetObjectField(TEXT("node"), NodeState(Existing));
        for (const TCHAR* Key : {TEXT("node_id"), TEXT("node_guid"), TEXT("object_path"), TEXT("node_class")}) { R.Data->SetStringField(Key, NodeState(Existing)->GetStringField(Key)); }
        R.Data->SetBoolField(TEXT("implicit_skeleton_compile_requested"), false); Finish(C, R, true); return R;
    }
    if (FBlueprintEditorUtils::FindOverrideForFunction(C.BP, C.Interface, FName(CallbackName)))
    { Fail(R, TEXT("particle_override_conflict"), TEXT("Public lookup found an override outside the exact selected EventGraph; no relocation.")); return R; }
    if (!GuardStructuralSavePolicy(C, R)) { return R; }
    TStrongObjectPtr<UBlueprint> KeepAlive(C.BP); R.Stage = TEXT("add_callback_event"); R.bMutationAttempted = true;
    UK2Node_Event* Event = UBlueprintEditorLibrary::AddEventOverride(C.BP, FName(CallbackName), FIntPoint(X, Y));
    if (Event) { R.Data->SetBoolField(TEXT("implicit_skeleton_compile_requested"), true); }
    else { R.Data->SetField(TEXT("implicit_skeleton_compile_requested"), MakeShared<FJsonValueNull>()); }
    R.Data->SetBoolField(TEXT("structural_api_dispatched"), true);
    R.Data->SetStringField(TEXT("implicit_skeleton_compile_status"), TEXT("not_independently_verified; explicit full compile/save is a separate pipeline step"));
    R.Data->SetBoolField(TEXT("stock_api_selects_new_graph_node"), true);
    if (Event)
    {
        R.Data->SetNumberField(TEXT("stock_grid_x"), Event->NodePosX); R.Data->SetNumberField(TEXT("stock_grid_y"), Event->NodePosY);
        Event->NodePosX = X; Event->NodePosY = Y;
    }
    Finish(C, R, EventMatches(C, Event, true), Event); return R;
}

FResult AddArrayLength(const FString& GraphPath, const FString& EventNodePath, int32 X, int32 Y)
{ return AddArray(GraphPath, EventNodePath, X, Y, false); }
FResult AddArrayGetCopy(const FString& GraphPath, const FString& EventNodePath, int32 X, int32 Y)
{ return AddArray(GraphPath, EventNodePath, X, Y, true); }

FResult AddBreak(const FString& GraphPath, int32 X, int32 Y)
{
    FResult R; FContext C; if (!Prepare(GraphPath, X, Y, C, R)) { return R; }
    if (!InterfacePresent(C)) { Fail(R, TEXT("particle_interface_missing"), TEXT("The exact receiver interface must be implemented before its finite BasicParticleData break node.")); return R; }
    TStrongObjectPtr<UBlueprint> KeepAlive(C.BP); R.Stage = TEXT("create_basic_particle_break"); R.bMutationAttempted = true; C.BP->Modify(); C.Graph->Modify();
    FGraphNodeCreator<UK2Node_BreakStruct> Creator(*C.Graph); UK2Node_BreakStruct* Node = Creator.CreateNode(false);
    Node->StructType = C.Basic; Node->NodePosX = X; Node->NodePosY = Y; Creator.Finalize();
    FBlueprintEditorUtils::MarkBlueprintAsModified(C.BP); C.Graph->NotifyGraphChanged();
    bool Pins = Node->Pins.Num() == 4 && Node->StructType == C.Basic && Node->IsNodePure();
    const UEdGraphPin* Input = Node->FindPin(TEXT("BasicParticleData"), EGPD_Input);
    Pins &= Input && SameType(Input->PinType, StructType(C.Basic, true, true));
    for (TFieldIterator<FProperty> It(C.Basic, EFieldIteratorFlags::ExcludeSuper); It; ++It)
    {
        const UEdGraphPin* Output = Node->FindPin(It->GetFName(), EGPD_Output); FEdGraphPinType Type;
        Pins &= Output && C.Schema->ConvertPropertyToPinType(*It, Type) && SameType(Output->PinType, Type);
    }
    for (const UEdGraphPin* P : Node->Pins) { Pins &= P && !P->bOrphanedPin && P->LinkedTo.IsEmpty(); }
    R.Data->SetStringField(TEXT("struct_type"), BasicPath); R.Data->SetBoolField(TEXT("basic_particle_pins_verified"), Pins);
    R.Data->SetBoolField(TEXT("requires_typed_input_connection"), true); R.Data->SetBoolField(TEXT("implicit_skeleton_compile_requested"), false);
    Finish(C, R, Pins, Node); return R;
}
}
