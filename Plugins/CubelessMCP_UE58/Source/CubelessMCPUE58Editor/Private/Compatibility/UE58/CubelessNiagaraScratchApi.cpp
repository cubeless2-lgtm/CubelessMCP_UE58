#include "Compatibility/UE58/CubelessNiagaraScratchApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "EditorAssetLibrary.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraComponent.h"
#include "NiagaraEditorSettings.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeCustomHlsl.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraScratchPadContainer.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraScriptVariable.h"
#include "NiagaraSystem.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "Misc/EngineVersion.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/PropertyAccessUtil.h"
#include "ToolsetRegistry/ToolsetLibrary.h"
#include "UObject/UObjectIterator.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Misc/CoreDelegates.h"
#include "UObject/GarbageCollection.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"
#include "HAL/FileManager.h"
#include "Misc/SecureHash.h"
#include "Misc/Paths.h"

namespace Cubeless::UE58::NiagaraScratch
{
namespace
{
using Function::FResult;
constexpr int32 MaxNodes=1024, MaxPins=8192, MaxLinks=32768, MaxEmitters=128, MaxParameters=2048, MaxScratch=64;
FString UsageName(ENiagaraScriptUsage Value)
{ return StaticEnum<ENiagaraScriptUsage>()->GetNameStringByValue(static_cast<int64>(Value)); }
FString Path(const UObject* Value) { return Value ? Value->GetPathName() : FString(); }
FString Guid(const FGuid& Value) { return Value.ToString(EGuidFormats::DigitsWithHyphens); }
FString Text(const TSharedRef<FJsonObject>& Value)
{ FString Out; FJsonSerializer::Serialize(Value, TJsonWriterFactory<>::Create(&Out)); return Out; }
bool Loaded(const UObject* Value)
{
    return IsValid(Value) && !Value->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects |
        RF_BeginDestroyed | RF_FinishDestroyed | RF_ClassDefaultObject | RF_ArchetypeObject);
}
bool Fail(FResult& R, const TCHAR* Code, const TCHAR* Message)
{ if (R.ErrorCode.IsEmpty()) { R.ErrorCode = Code; R.Error = Message; } return false; }
// The NiagaraGraph override is not exported; dispatch through the public base virtual.
FORCENOINLINE void NotifyGraph(UEdGraph& Graph) { Graph.NotifyGraphChanged(); }
bool Bounded(const FString& Value, int32 Limit = 1024)
{ if (Value.Len() > Limit) { return false; } for (TCHAR C : Value) { if (C < 32 || C == 127) { return false; } } return true; }
FString Token(FString Value)
{ Value = Value.TrimStartAndEnd().ToLower(); Value.ReplaceInline(TEXT("_"), TEXT("")); Value.ReplaceInline(TEXT("-"), TEXT("")); Value.ReplaceInline(TEXT(" "), TEXT("")); return Value; }
bool Index(const FString& Json, int32& Out)
{
    if (Json.Len() > 64) { return false; }
    TArray<TSharedPtr<FJsonValue>> Values;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[") + Json + TEXT("]")), Values) || Values.Num() != 1 || !Values[0].IsValid()) { return false; }
    if (Values[0]->Type == EJson::Null) { Out = INDEX_NONE; return true; }
    double Number;
    if (Values[0]->Type != EJson::Number || !Values[0]->TryGetNumber(Number) || !FMath::IsFinite(Number) ||
        Number != FMath::TruncToDouble(Number) || Number < static_cast<double>(MIN_int32) || Number > static_cast<double>(MAX_int32)) { return false; }
    Out = static_cast<int32>(Number); return true;
}
bool ObjectPath(const FString& Input, FString& Out)
{
    if (!Bounded(Input)) { return false; } Out = Input.TrimStartAndEnd();
    if (!Out.Contains(TEXT(".")) && FPackageName::IsValidLongPackageName(Out))
    { Out += TEXT(".") + FPackageName::GetLongPackageAssetName(Out); }
    return FPackageName::IsValidObjectPath(Out);
}
// HN's exact bool-User attachment and legacy stack-save boundary use this
// synchronous scoped admission only; no continuation or public API is added here.
struct FHNWriterGuard;
FHNWriterGuard* HNWriter=nullptr;
bool HNAdmit(const TCHAR* Stage);
bool HNCurrentPin(const UEdGraphPin* Pin);
bool HNDispatch(const TCHAR* Name,bool Writer=false);
struct FUKHlslEdit;
FUKHlslEdit* ExistingHlslEdit=nullptr;
bool UKPlanConnection(UEdGraphPin* Out,UEdGraphPin* In,bool Replace);
bool HNTrackProtectedSource(UNiagaraScriptSource* Source,UNiagaraGraph* Graph);
bool STCloneAdmit();
bool HNKeepType(const FNiagaraTypeDefinition& Type);
TSharedRef<FJsonObject> CompileState(UNiagaraSystem* System)
{
    auto J = MakeShared<FJsonObject>();
    if(!HNAdmit(TEXT("before_compile_state")))return J;
    const bool Outstanding=System->HasOutstandingCompilationRequests(true);if(!HNAdmit(TEXT("after_HasOutstandingCompilationRequests")))return J;
    const bool Needs=System->NeedsRequestCompile();if(!HNAdmit(TEXT("after_NeedsRequestCompile")))return J;
    J->SetBoolField(TEXT("outstanding_including_gpu"), Outstanding); J->SetBoolField(TEXT("needs_request_compile"), Needs);
    const bool Ready=System->IsReadyToRun();if(!HNAdmit(TEXT("after_IsReadyToRun")))return J;J->SetBoolField(TEXT("ready_to_run"), Ready); J->SetBoolField(TEXT("ready_to_save"), !Outstanding && !Needs);
    J->SetBoolField(TEXT("compile_success_verified"), false); J->SetBoolField(TEXT("poll_or_flush_invoked"), false); return J;
}
FString PinId(const UEdGraphPin* Pin)
{ return Pin ? Path(Pin->GetOwningNode()) + TEXT(".") + Pin->PinName.ToString() + TEXT("/") + Guid(Pin->PinId) : FString(); }
// CustomHlsl's C++ accessors are unexported on a MinimalAPI class. Its declared
// EditAnywhere property is accessed only through the permission-checked public
// ToolsetLibrary API. This never bypasses private/protected property permissions.
bool HlslText(const UNiagaraNodeCustomHlsl* Node,FString& Out)
{
    if(!HNAdmit(TEXT("before_GetObjectProperties_CustomHlsl")))return false;
    FProperty* P=FindFProperty<FProperty>(UNiagaraNodeCustomHlsl::StaticClass(),TEXT("CustomHlsl"));
    if(!Loaded(Node)||Node->GetClass()!=UNiagaraNodeCustomHlsl::StaticClass()||!CastField<FStrProperty>(P)||P->ArrayDim!=1||
        EnumHasAnyFlags(PropertyAccessUtil::CanGetPropertyValue(P),EPropertyAccessResultFlags::PermissionDenied)){return false;}
    TSharedPtr<FJsonObject> J;
    if(!HNDispatch(TEXT("GetObjectProperties_CustomHlsl")))return false;
    const FString Raw=UToolsetLibrary::GetObjectProperties(Node,{FName(TEXT("CustomHlsl"))});
    return HNAdmit(TEXT("after_GetObjectProperties_CustomHlsl"))&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),J)&&
        J.IsValid()&&J->TryGetStringField(TEXT("CustomHlsl"),Out);
}
TSharedRef<FJsonObject> PinMetadata(const UEdGraphPin* Pin)
{
    auto J = MakeShared<FJsonObject>(); const auto& T = Pin->PinType;
    J->SetStringField(TEXT("pin_id"), PinId(Pin)); J->SetStringField(TEXT("pin_guid"), Guid(Pin->PinId));
    J->SetStringField(TEXT("name"), Pin->PinName.ToString()); J->SetNumberField(TEXT("direction"), static_cast<int32>(Pin->Direction));
    J->SetStringField(TEXT("category"), T.PinCategory.ToString()); J->SetStringField(TEXT("subcategory"), T.PinSubCategory.ToString());
    J->SetStringField(TEXT("subcategory_object_path"), Path(T.PinSubCategoryObject.Get()));
    J->SetNumberField(TEXT("container_type"), static_cast<int32>(T.ContainerType)); J->SetBoolField(TEXT("is_reference"), T.bIsReference);
    J->SetBoolField(TEXT("is_const"), T.bIsConst); J->SetBoolField(TEXT("is_weak_pointer"), T.bIsWeakPointer);
    J->SetStringField(TEXT("terminal_category"), T.PinValueType.TerminalCategory.ToString());
    J->SetStringField(TEXT("terminal_subcategory"), T.PinValueType.TerminalSubCategory.ToString());
    J->SetStringField(TEXT("terminal_subcategory_object_path"), Path(T.PinValueType.TerminalSubCategoryObject.Get()));
    J->SetBoolField(TEXT("terminal_is_const"), T.PinValueType.bTerminalIsConst); J->SetBoolField(TEXT("terminal_is_weak_pointer"), T.PinValueType.bTerminalIsWeakPointer);
    J->SetStringField(TEXT("raw_default_value"), Pin->DefaultValue); J->SetStringField(TEXT("autogenerated_default_value"), Pin->AutogeneratedDefaultValue);
    J->SetStringField(TEXT("default_object_path"), Path(Pin->DefaultObject));const FText CopiedText=Pin->DefaultTextValue;
    J->SetBoolField(TEXT("hidden"), Pin->bHidden);
    J->SetBoolField(TEXT("orphaned"), Pin->bOrphanedPin); J->SetBoolField(TEXT("not_connectable"), Pin->bNotConnectable);
    J->SetBoolField(TEXT("default_value_is_ignored"), Pin->bDefaultValueIsIgnored); J->SetBoolField(TEXT("default_value_is_readonly"), Pin->bDefaultValueIsReadOnly);
    const FString Display=CopiedText.ToString();if(!HNAdmit(TEXT("after_copied_pin_text")))return J;
    J->SetStringField(TEXT("default_text"),Display);J->SetBoolField(TEXT("default_text_empty"),Display.IsEmpty());return J;
}
TSharedRef<FJsonObject> NodeMetadata(const UEdGraphNode* Node)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("node_path"), Path(Node)); J->SetStringField(TEXT("node_name"), Node->GetName());
    J->SetStringField(TEXT("node_class"), Path(Node->GetClass())); J->SetStringField(TEXT("node_guid"), Guid(Node->NodeGuid));
    J->SetNumberField(TEXT("position_x"), Node->NodePosX); J->SetNumberField(TEXT("position_y"), Node->NodePosY);
    if (const auto* Call = Cast<UNiagaraNodeFunctionCall>(Node))
    {
        J->SetStringField(TEXT("function_script_path"), Path(Call->FunctionScript)); J->SetStringField(TEXT("selected_script_version"), Guid(Call->SelectedScriptVersion));
        J->SetStringField(TEXT("function_name"), Call->GetFunctionName()); J->SetStringField(TEXT("function_script_asset_object_path"), Call->FunctionScriptAssetObjectPath.ToString());
        J->SetNumberField(TEXT("node_index"), INDEX_NONE); J->SetStringField(TEXT("function_script"), Path(Call->FunctionScript));
        J->SetStringField(TEXT("script_usage"), Call->FunctionScript ? UsageName(Call->FunctionScript->GetUsage()) : FString());
        J->SetStringField(TEXT("called_usage"), UsageName(Call->GetCalledUsage()));
        const bool Scratch = Call->FunctionScript && Call->FunctionScript->GetOuter() && Call->FunctionScript->GetOuter()->GetClass()->GetName() == TEXT("NiagaraScratchPadContainer");
        J->SetBoolField(TEXT("is_scratch_pad"), Scratch); J->SetStringField(TEXT("source_kind"), Scratch ? TEXT("scratch_pad") : (Call->FunctionScript ? TEXT("script_asset") : TEXT("signature_only")));
        const auto SignatureCopy=Call->Signature;const auto SpecifierCopy=Call->FunctionSpecifiers;
        auto Variable = [](FNiagaraVariableBase V)
        {
            auto Row = MakeShared<FJsonObject>(); const auto& T = V.GetType(); Row->SetStringField(TEXT("name"), V.GetName().ToString());
            Row->SetBoolField(TEXT("type_is_valid"), T.IsValid());
            Row->SetBoolField(TEXT("type_is_data_interface"), T.IsDataInterface()); Row->SetBoolField(TEXT("type_is_uobject"), T.IsUObject());
            if (UClass* Class = T.GetClass()) { Row->SetStringField(TEXT("type_class"), Class->GetName()); Row->SetStringField(TEXT("type_class_path"), Path(Class)); }
            const FText CopiedName=T.GetNameText();const FString Display=CopiedName.ToString();if(!HNAdmit(TEXT("after_copied_signature_type_text")))return MakeShared<FJsonValueObject>(Row);Row->SetStringField(TEXT("type"),Display);return MakeShared<FJsonValueObject>(Row);
        };
        TArray<TSharedPtr<FJsonValue>> Inputs, Outputs, Specifiers; FString Search = Call->GetFunctionName() + TEXT(" ") + Path(Call->FunctionScript);
        for (const auto& V : SignatureCopy.Inputs) { Inputs.Add(Variable(V));if(!HNAdmit(TEXT("after_signature_input")))return J;Search += TEXT(" ") + V.GetName().ToString(); }
        for (const auto& V : SignatureCopy.Outputs) { Outputs.Add(Variable(V));if(!HNAdmit(TEXT("after_signature_output")))return J;Search += TEXT(" ") + V.GetName().ToString(); }
        J->SetArrayField(TEXT("signature_inputs"), Inputs); J->SetArrayField(TEXT("signature_outputs"), Outputs);
        TArray<FString> SpecifierText; for (const auto& V : SpecifierCopy) { SpecifierText.AddUnique(V.Key.ToString() + TEXT("=") + V.Value.ToString()); }
        SpecifierText.Sort(); for (const auto& V : SpecifierText) { Specifiers.Add(MakeShared<FJsonValueString>(V)); } J->SetArrayField(TEXT("function_specifiers"), Specifiers);
        const TPair<const TCHAR*, const TCHAR*> HintTokens[] = {
            {TEXT("color"),TEXT("color_or_tint_control")},{TEXT("colour"),TEXT("color_or_tint_control")},{TEXT("tint"),TEXT("color_or_tint_control")},
            {TEXT("ribbonwidth"),TEXT("ribbon_width_control")},{TEXT("ribbon width"),TEXT("ribbon_width_control")},{TEXT("width"),TEXT("width_control")},
            {TEXT("scale"),TEXT("scale_control")},{TEXT("size"),TEXT("size_control")},{TEXT("lifetime"),TEXT("lifetime_control")},{TEXT("life time"),TEXT("lifetime_control")},
            {TEXT("duration"),TEXT("duration_control")},{TEXT("velocity"),TEXT("velocity_control")},{TEXT("speed"),TEXT("velocity_control")},
            {TEXT("spawn"),TEXT("spawn_control")},{TEXT("materialparam"),TEXT("dynamic_material_parameter_control")},{TEXT("dynamicmaterial"),TEXT("dynamic_material_parameter_control")},
            {TEXT("user."),TEXT("user_parameter_reference")},{TEXT("scratch"),TEXT("scratch_pad_reference")},{TEXT("trail"),TEXT("trail_behavior")},{TEXT("ribbon"),TEXT("ribbon_behavior")}};
        TArray<FString> HintText; for (const auto& Hint : HintTokens) { if (Search.Contains(Hint.Key, ESearchCase::IgnoreCase)) { HintText.AddUnique(Hint.Value); } }
        TArray<TSharedPtr<FJsonValue>> Hints; for (const auto& Hint : HintText) { Hints.Add(MakeShared<FJsonValueString>(Hint)); } J->SetArrayField(TEXT("control_hints"), Hints);
    }
    J->SetStringField(TEXT("comment"), Node->NodeComment);
    J->SetNumberField(TEXT("enabled_state"), static_cast<int32>(Node->GetDesiredEnabledState()));
    J->SetBoolField(TEXT("user_set_enabled_state"), Node->HasUserSetTheEnabledState());
    if (const auto* Input = Cast<UNiagaraNodeInput>(Node))
    {
        J->SetStringField(TEXT("input_name"), Input->Input.GetName().ToString());
        J->SetStringField(TEXT("input_type"), Input->Input.GetType().GetName());
        J->SetStringField(TEXT("input_type_object_path"), Input->Input.GetType().GetClass() ? Path(Input->Input.GetType().GetClass()) : Path(Input->Input.GetType().GetScriptStruct()));
        J->SetNumberField(TEXT("input_usage"), static_cast<int32>(Input->Usage));
        J->SetBoolField(TEXT("private_data_interface_reference_observed"), false);
    }
    if (const auto* Output = Cast<UNiagaraNodeOutput>(Node))
    { J->SetStringField(TEXT("usage"), UsageName(Output->GetUsage())); J->SetStringField(TEXT("usage_id"), Guid(Output->GetUsageId())); }
    if(const auto* Custom=Cast<UNiagaraNodeCustomHlsl>(Node))
    {FString Value;const bool Observed=HlslText(Custom,Value);if(!HNAdmit(TEXT("after_CustomHlsl_metadata")))return J;J->SetBoolField(TEXT("custom_hlsl_observed"),Observed);
        if(Observed){J->SetStringField(TEXT("custom_hlsl"),Value);}J->SetStringField(TEXT("custom_hlsl_script_usage"),UsageName(Custom->ScriptUsage));
        J->SetBoolField(TEXT("signature_requires_context"),Custom->Signature.bRequiresContext);}
    return J;
}
struct FSnapshot
{
    bool bComplete = false; TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    TMap<FString, FString> Nodes, Pins; TMap<FString, TArray<FString>> Links;
};
FSnapshot Capture(UNiagaraGraph* Graph)
{
    FSnapshot S;if(!HNAdmit(TEXT("before_graph_snapshot")))return S; S.Json->SetStringField(TEXT("graph_path"), Path(Graph));
    if (!Loaded(Graph) || Graph->Nodes.Num() > MaxNodes) { return S; }
    S.Json->SetStringField(TEXT("graph_change_id"), Guid(Graph->GetChangeID()));
    TSet<FGuid> NodeGuids; TSet<const UEdGraphNode*> Nodes; TSet<const UEdGraphPin*> Pins; int32 LinkCount = 0;
    for (const UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Loaded(Node) || Node->GetOuter() != Graph || !Node->GetClass()->HasAllClassFlags(CLASS_Native) || !Node->NodeGuid.IsValid() || NodeGuids.Contains(Node->NodeGuid) || Nodes.Contains(Node) ||
            Node->Pins.Num() > MaxPins || Pins.Num() > MaxPins - Node->Pins.Num()) { return S; }
        Nodes.Add(Node); NodeGuids.Add(Node->NodeGuid);
        for (const UEdGraphPin* Pin : Node->Pins)
        { if (!Pin || Pin->GetOwningNode() != Node || Pins.Contains(Pin) || !Pin->PinId.IsValid()) { return S; } Pins.Add(Pin); }
    }
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (const UEdGraphNode* Node : Graph->Nodes)
    {
        auto Row = NodeMetadata(Node);if(!HNAdmit(TEXT("after_node_metadata")))return S;bool HlslObserved=true;
        if(Row->TryGetBoolField(TEXT("custom_hlsl_observed"),HlslObserved)&&!HlslObserved){return S;}
        const FString N = Path(Node); S.Nodes.Add(N, Text(Row));
        TArray<TSharedPtr<FJsonValue>> PinRows;
        for (const UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin->LinkedTo.Num() > MaxLinks || LinkCount > MaxLinks - Pin->LinkedTo.Num()) { return S; }
            LinkCount += Pin->LinkedTo.Num(); TSet<const UEdGraphPin*> Seen; TArray<FString> Links;
            for (const UEdGraphPin* Linked : Pin->LinkedTo)
            {
                if (!Linked || !Pins.Contains(Linked) || Seen.Contains(Linked) || Linked->Direction == Pin->Direction || !Linked->LinkedTo.Contains(Pin)) { return S; }
                Seen.Add(Linked); Links.Add(PinId(Linked));
            }
            Links.Sort(); const FString P = PinId(Pin); if (S.Pins.Contains(P)) { return S; }
            auto PinRow = PinMetadata(Pin);if(!HNAdmit(TEXT("after_pin_metadata")))return S; S.Pins.Add(P, Text(PinRow)); S.Links.Add(P, Links);
            TArray<TSharedPtr<FJsonValue>> LinkRows; for (const auto& Link : Links) { LinkRows.Add(MakeShared<FJsonValueString>(Link)); }
            PinRow->SetArrayField(TEXT("links"), LinkRows); PinRows.Add(MakeShared<FJsonValueObject>(PinRow));
        }
        Row->SetArrayField(TEXT("pins"), PinRows); Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    S.Json->SetArrayField(TEXT("nodes"), Rows); S.Json->SetNumberField(TEXT("node_count"), S.Nodes.Num());
    S.Json->SetNumberField(TEXT("pin_count"), S.Pins.Num()); S.Json->SetNumberField(TEXT("physical_link_count"), LinkCount / 2);
    S.Json->SetBoolField(TEXT("observation_complete"), true); S.bComplete = true; return S;
}

bool MapPin(UEdGraphNode* Node, EEdGraphPinDirection Direction, UEdGraphPin*& Out)
{
    Out = nullptr;
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->Direction == Direction && !Pin->bOrphanedPin &&
            Pin->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryType &&
            Pin->PinType.PinSubCategoryObject.Get() == FNiagaraTypeDefinition::GetParameterMapDef().GetScriptStruct() &&
            UEdGraphSchema_Niagara::PinToTypeDefinition(Pin) == FNiagaraTypeDefinition::GetParameterMapDef())
        { if (Out) { return false; } Out = Pin; }
    }
    return true;
}

bool ExactNodeClass(const UEdGraphNode* Node, const TCHAR* ClassPath)
{
    UClass* Class = FindObject<UClass>(nullptr, ClassPath);
    return Loaded(Node) && IsValid(Class) && Node->GetClass() == Class && Class->GetPathName() == ClassPath &&
        Class->HasAllClassFlags(CLASS_Native) && Class->IsChildOf(UNiagaraNode::StaticClass()) &&
        !Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists);
}
bool SameGraph(const FSnapshot& A, const FSnapshot& B)
{
    if (!A.bComplete || !B.bComplete || !A.Nodes.OrderIndependentCompareEqual(B.Nodes) ||
        !A.Pins.OrderIndependentCompareEqual(B.Pins) || A.Links.Num() != B.Links.Num()) { return false; }
    for (const auto& P : A.Links) { const auto* V = B.Links.Find(P.Key); if (!V || *V != P.Value) { return false; } }
    return true;
}
TSharedRef<FJsonObject> TypeRow(const FNiagaraTypeDefinition& T)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("name"), T.GetName());
    J->SetStringField(TEXT("object_path"), T.GetClass() ? Path(T.GetClass()) : (T.IsEnum() ? Path(T.GetEnum()) : Path(T.GetScriptStruct())));
    J->SetStringField(TEXT("flags_string"), LexToString(T.GetFlags()));
    J->SetNumberField(TEXT("size_bytes"), T.GetSize()); J->SetBoolField(TEXT("valid"), T.IsValid());
    J->SetBoolField(TEXT("data_interface"), T.IsDataInterface()); J->SetBoolField(TEXT("uobject"), T.IsUObject());
    return J;
}
// Only public store data/accessors; module-input edits never write the User store.
TSharedRef<FJsonObject> UserState(UNiagaraSystem* System, bool& Complete)
{
    auto J = MakeShared<FJsonObject>(); const auto& S = System->GetExposedParameters();
    TArray<FNiagaraVariable> Vars; S.GetParameters(Vars);
    Complete = S.GetOwner() == System && Vars.Num() <= MaxParameters && S.GetParameterDataArray().Num() <= 262144 &&
        S.GetDataInterfaces().Num() <= MaxParameters && S.GetUObjects().Num() <= MaxParameters;
    Vars.Sort([](const FNiagaraVariable& A, const FNiagaraVariable& B) { return A.GetName().ToString() < B.GetName().ToString(); });
    TArray<TSharedPtr<FJsonValue>> Rows; TSet<FString> Names;
    for (const auto& V : Vars)
    {
        const auto& T = V.GetType(); const int32 Offset = S.IndexOf(V), Size = V.GetSizeInBytes();
        if (!T.IsValid() || Names.Contains(V.GetName().ToString().ToLower())) { Complete = false; break; }
        Names.Add(V.GetName().ToString().ToLower()); auto R = MakeShared<FJsonObject>();
        R->SetStringField(TEXT("name"), V.GetName().ToString()); R->SetObjectField(TEXT("type"), TypeRow(T));
        R->SetNumberField(TEXT("offset"), Offset);
        if (T.IsDataInterface())
        {
            if (!S.GetDataInterfaces().IsValidIndex(Offset)) { Complete = false; break; }
            auto* O = S.GetDataInterface(Offset); R->SetStringField(TEXT("reference"), Path(O));
            R->SetStringField(TEXT("reference_class"), O ? Path(O->GetClass()) : FString());
        }
        else if (T.IsUObject())
        {
            if (!S.GetUObjects().IsValidIndex(Offset)) { Complete = false; break; }
            auto* O = S.GetUObject(Offset).Get(); R->SetStringField(TEXT("reference"), Path(O));
            R->SetStringField(TEXT("reference_class"), O ? Path(O->GetClass()) : FString());
        }
        else
        {
            if (Size <= 0 || Size > 4096 || Offset < 0 || Offset > S.GetParameterDataArray().Num() - Size) { Complete = false; break; }
            FString Bytes; for (int32 I = 0; I < Size; ++I) { Bytes += FString::Printf(TEXT("%02x"), S.GetParameterDataArray()[Offset + I]); }
            R->SetStringField(TEXT("bytes_hex"), Bytes);
            if (T == FNiagaraTypeDefinition::GetPositionDef())
            {
                const FVector* P = S.GetPositionParameterValue(V.GetName()); if (!P) { Complete = false; break; }
                R->SetArrayField(TEXT("position"), {MakeShared<FJsonValueNumber>(P->X), MakeShared<FJsonValueNumber>(P->Y), MakeShared<FJsonValueNumber>(P->Z)});
            }
        }
        Rows.Add(MakeShared<FJsonValueObject>(R));
    }
    J->SetArrayField(TEXT("parameters"), Rows); J->SetNumberField(TEXT("layout_version"), S.GetLayoutVersion());
    J->SetStringField(TEXT("owner_path"), Path(S.GetOwner())); J->SetBoolField(TEXT("observation_complete"), Complete);
    return J;
}
TSharedRef<FJsonObject> GraphParameters(UNiagaraGraph* Graph, bool& Complete)
{
    auto J = MakeShared<FJsonObject>(); const auto& M = Graph->GetAllMetaData(); Complete = M.Num() <= MaxParameters;
    TArray<FString> Keys; TMap<FString, TSharedPtr<FJsonValue>> Rows;
    for (const auto& Pair : M)
    {
        UNiagaraScriptVariable* V = Pair.Value.Get(); const FString Key = Pair.Key.GetName().ToString();
        if (!Loaded(V) || !V->IsIn(Graph) || !Pair.Key.GetType().IsValid() || V->Variable.GetName() != Pair.Key.GetName() ||
            V->Variable.GetType() != Pair.Key.GetType() || Rows.Contains(Key)) { Complete = false; break; }
        auto R = MakeShared<FJsonObject>(); R->SetStringField(TEXT("name"), Key); R->SetObjectField(TEXT("type"), TypeRow(Pair.Key.GetType()));
        R->SetStringField(TEXT("object_path"), Path(V)); R->SetStringField(TEXT("guid"), Guid(V->Metadata.GetVariableGuid()));
        R->SetNumberField(TEXT("default_mode"), static_cast<int32>(V->DefaultMode));
        R->SetStringField(TEXT("change_id"), Guid(V->GetChangeId()));
        Keys.Add(Key); Rows.Add(Key, MakeShared<FJsonValueObject>(R));
    }
    Keys.Sort(); for (const FString& Key : Keys) { J->SetField(Key, Rows[Key]); }
    return J;
}
TSharedRef<FJsonObject> Handles(UNiagaraSystem* S)
{
    auto J = MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> Rows;
    for (const auto& H : S->GetEmitterHandles())
    {
        auto R = MakeShared<FJsonObject>(); const auto I = H.GetInstance();
        R->SetStringField(TEXT("id"), Guid(H.GetId())); R->SetStringField(TEXT("name"), H.GetName().ToString());
        R->SetStringField(TEXT("emitter"), Path(I.Emitter)); R->SetStringField(TEXT("version"), Guid(I.Version));
        R->SetBoolField(TEXT("enabled"), H.GetIsEnabled()); Rows.Add(MakeShared<FJsonValueObject>(R));
    }
    J->SetArrayField(TEXT("handles"), Rows); return J;
}

bool Usage(const FString& Value, ENiagaraScriptUsage& Out)
{
    const FString N = Token(Value);
    if (N == TEXT("systemspawn") || N == TEXT("systemspawnscript")) { Out = ENiagaraScriptUsage::SystemSpawnScript; }
    else if (N == TEXT("systemupdate") || N == TEXT("systemupdatescript")) { Out = ENiagaraScriptUsage::SystemUpdateScript; }
    else if (N == TEXT("emitterspawn") || N == TEXT("emitterspawnscript")) { Out = ENiagaraScriptUsage::EmitterSpawnScript; }
    else if (N == TEXT("emitterupdate") || N == TEXT("emitterupdatescript")) { Out = ENiagaraScriptUsage::EmitterUpdateScript; }
    else if (N == TEXT("particlespawn") || N == TEXT("particlespawnscript")) { Out = ENiagaraScriptUsage::ParticleSpawnScript; }
    else if (N == TEXT("particlespawninterpolated") || N == TEXT("particlespawnscriptinterpolated")) { Out = ENiagaraScriptUsage::ParticleSpawnScriptInterpolated; }
    else if (N == TEXT("particleupdate") || N == TEXT("particleupdatescript")) { Out = ENiagaraScriptUsage::ParticleUpdateScript; }
    else if (N == TEXT("particleevent") || N == TEXT("particleeventscript")) { Out = ENiagaraScriptUsage::ParticleEventScript; }
    else if (N == TEXT("particlesimulationstage") || N == TEXT("particlesimulationstagescript") || N == TEXT("simulationstage")) { Out = ENiagaraScriptUsage::ParticleSimulationStageScript; }
    else { return false; }
    return true;
}

struct FOwner
{
    UObject* Outer=nullptr; TArray<TObjectPtr<UNiagaraScript>>* Scripts=nullptr;
    FVersionedNiagaraEmitter Emitter; FVersionedNiagaraEmitterData* Data=nullptr;
    FString Kind, Name, Key; int32 Index=INDEX_NONE;
};
struct FProtected
{
    UNiagaraScriptSource* Source=nullptr; UNiagaraGraph* Graph=nullptr; FSnapshot Before; TSharedRef<FJsonObject> Parameters=MakeShared<FJsonObject>(); bool Dirty=false;
};
struct FContext
{
    UNiagaraSystem* System=nullptr; TStrongObjectPtr<UNiagaraSystem> Keep;
    UNiagaraScript* Script=nullptr; TStrongObjectPtr<UNiagaraScript> KeepScript;
    UNiagaraScriptSource* Source=nullptr; UNiagaraGraph* Graph=nullptr;
    FOwner Owner; int32 ScriptIndex=INDEX_NONE;
    TSharedRef<FJsonObject> Users=MakeShared<FJsonObject>(), HandleState=MakeShared<FJsonObject>(), Owners=MakeShared<FJsonObject>();
    TArray<FProtected> Protected; bool Dirty=false;
};
bool Ordinary(UNiagaraScriptSource* Source, UNiagaraGraph*& Graph)
{
    Graph=nullptr;if(!Loaded(Source)||Source->bIsCompilationCopy||Source->bIsReleased)return false;
    Graph=Source->NodeGraph.Get();TStrongObjectPtr<UNiagaraGraph> Immediate(Graph);if(!HNAdmit(TEXT("after_source_NodeGraph_handle")))return false;
    return Loaded(Graph)&&
        Graph->GetClass()==UNiagaraGraph::StaticClass()&&!Graph->IsCompilationCopy()&&Graph->GetOuter()==Source&&
        Graph->Schema.Get()==UEdGraphSchema_Niagara::StaticClass();
}
TSharedRef<FJsonObject> Catalog(UNiagaraSystem* System, bool& Complete)
{
    auto J=MakeShared<FJsonObject>(); Complete=System->ScratchPadScripts.Num()<=MaxScratch;
    auto Add=[&](const FString& Key,UObject* Outer,const TArray<TObjectPtr<UNiagaraScript>>& Scripts)
    {
        auto Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("owner_path"),Path(Outer));
        TArray<TSharedPtr<FJsonValue>> Paths;
        if(Scripts.Num()>MaxScratch){Complete=false;return;}
        TSet<UNiagaraScript*> Seen;
        for(UNiagaraScript* Script:Scripts)
        {
            if(!Loaded(Script)||Script->GetClass()!=UNiagaraScript::StaticClass()||Script->GetOuter()!=Outer||Seen.Contains(Script)) {Complete=false;return;}
            Seen.Add(Script); Paths.Add(MakeShared<FJsonValueString>(Path(Script)));
        }
        Row->SetArrayField(TEXT("script_paths"),Paths); J->SetObjectField(Key,Row);
    };
    Add(TEXT("system"),System,System->ScratchPadScripts);
    for(const auto& H:System->GetEmitterHandles())
    {
        const auto* D=H.GetEmitterData(); const auto E=H.GetInstance();
        if(H.GetEmitterMode()!=ENiagaraEmitterMode::Standard||!Loaded(E.Emitter)||!D){Complete=false;break;}
        for(bool Parent:{false,true})
        {
            auto* Container=Parent?D->ParentScratchPads.Get():D->ScratchPads.Get();
            const FString Key=(Parent?TEXT("parent_"):TEXT("emitter_"))+Guid(H.GetId());
            if(Container)
            {
                if(!Loaded(Container)||Container->GetClass()!=UNiagaraScratchPadContainer::StaticClass()||!Container->IsIn(System)) {Complete=false;break;}
                Add(Key,Container,Container->Scripts);
            }
            else {const TArray<TObjectPtr<UNiagaraScript>> Empty; Add(Key,nullptr,Empty);}
        }
    }
    return J;
}
bool Protect(FResult& R,FContext& C,UNiagaraScriptSource* Source)
{
    UNiagaraGraph* Graph=nullptr;
    if(!Ordinary(Source,Graph)){return Fail(R,TEXT("niagara_scratch_source_unobservable"),TEXT("All retained source graphs must already be loaded ordinary native graphs."));}
    for(const auto& P:C.Protected){if(P.Graph==Graph){return true;}}
    if(C.Protected.Num()>=256){return Fail(R,TEXT("niagara_scratch_source_bound"),TEXT("Retained native source graph inventory exceeds256."));}
    if(!HNTrackProtectedSource(Source,Graph))return false;
    FProtected P; P.Source=Source;P.Graph=Graph;P.Dirty=Graph->GetOutermost()->IsDirty();P.Before=Capture(Graph);if(!HNAdmit(TEXT("after_protected_source_capture")))return false;bool Complete=false;P.Parameters=GraphParameters(Graph,Complete);
    if(!P.Before.bComplete||!Complete){return Fail(R,TEXT("niagara_scratch_source_unobservable"),TEXT("Retained graph/pin/parameter inventory is incomplete before mutation."));}
    C.Protected.Add(MoveTemp(P));return true;
}
void Initialize(FResult& R,const FString& PathValue,bool bSave)
{
    R.Data->SetStringField(TEXT("requested_system_path"),PathValue);
    for(const TCHAR* K:{TEXT("compile_requested"),TEXT("compile_attempted"),TEXT("save_attempted"),TEXT("saved"),TEXT("save_deferred"),
        TEXT("asset_load_attempted"),TEXT("blocking_wait_invoked"),TEXT("atomic_rollback_available"),TEXT("auto_retry_safe"),
        TEXT("explicit_user_parameter_writes"),TEXT("full_legacy_scratch_parity"),TEXT("hlsl_producer_verified"),TEXT("runtime_execution_verified")}){R.Data->SetBoolField(K,false);}
    R.Data->SetBoolField(TEXT("save_requested"),bSave);R.Data->SetBoolField(TEXT("implicit_notification_callbacks_possible"),true);
    R.Data->SetNumberField(TEXT("explicit_compile_requests"),0);R.Data->SetNumberField(TEXT("exact_save_requests"),0);
    R.Data->SetNumberField(TEXT("script_duplicate_requests"),0);R.Data->SetNumberField(TEXT("parameter_write_requests"),0);R.Data->SetNumberField(TEXT("stack_insert_requests"),0);
    R.Data->SetStringField(TEXT("dependency_layer"),TEXT("EngineAPI: public native duplication/graph/stack work; finite public ToolsetLibrary property text assignment/readback when Custom HLSL is involved; no ServerRPC"));
}
bool Base(FResult& R,FContext& C,const FString& SystemPath,bool Write,bool Allow)
{
    R.Stage=TEXT("preflight");const auto& V=FEngineVersion::Current();FString Object;
    if(!IsEditorGameThread()||IsPlaySessionActive()||V.GetMajor()!=5||V.GetMinor()!=8||V.GetPatch()!=3)
    {return Fail(R,TEXT("niagara_scratch_editor_state_invalid"),TEXT("Only UE5.8.3 editor game thread outside PIE/SIE is implemented."));}
    if(!ObjectPath(SystemPath,Object)){return Fail(R,TEXT("niagara_scratch_system_path_invalid"),TEXT("Exact native NiagaraSystem package/object path required."));}
    C.System=FindObject<UNiagaraSystem>(nullptr,*Object);C.Keep.Reset(C.System);
    if(!Loaded(C.System)||C.System->GetClass()!=UNiagaraSystem::StaticClass()||!C.System->IsAsset()||
        C.System->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor|PKG_CompiledIn|PKG_EditorOnly)||C.System->GetEmitterHandles().Num()>MaxEmitters)
    {return Fail(R,TEXT("niagara_loaded_system_required"),TEXT("Exact ordinary native system and at most128 standard emitters must already be loaded."));}
    const FString Package=C.System->GetOutermost()->GetName();
    const bool Temp=Package.StartsWith(TEXT("/Game/CubelessMCPTests/"))||Package.StartsWith(TEXT("/Game/_MCP_Temp/NiagaraGenerated/"));
    if(Write&&(!Package.StartsWith(TEXT("/Game/"))||(!Temp&&!Allow)))
    {return Fail(R,TEXT("niagara_write_scope_forbidden"),TEXT("Writes require isolated generated/test roots or explicit allow_source_edit on /Game."));}
    if(Write)
    {
        for(TObjectIterator<UNiagaraComponent> It;It;++It){auto* N=*It;if(IsValid(N)&&!N->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject)&&N->GetAsset()==C.System&&N->GetWorld())
        {return Fail(R,TEXT("niagara_system_has_world_instances"),TEXT("Authoring refuses systems referenced by live world components."));}}
        if(C.System->HasOutstandingCompilationRequests(true)){return Fail(R,TEXT("niagara_compile_outstanding"),TEXT("Observe outstanding compile completion before authoring; this game-thread API never polls/waits."));}
    }
    C.Dirty=C.System->GetOutermost()->IsDirty();bool Complete=false;C.Users=UserState(C.System,Complete);
    if(!Complete){return Fail(R,TEXT("niagara_user_state_unobservable"),TEXT("Existing typed User state must be fully observable before mutation."));}
    C.HandleState=Handles(C.System);C.Owners=Catalog(C.System,Complete);
    if(!Complete){return Fail(R,TEXT("niagara_scratch_catalog_unobservable"),TEXT("Native local/parent scratch owner catalog must be complete before mutation."));}
    for(UNiagaraScript* S:{C.System->GetSystemSpawnScript(),C.System->GetSystemUpdateScript()})
    {if(!Loaded(S)){return Fail(R,TEXT("niagara_scratch_system_source_unobservable"),TEXT("Native system script must already be loaded for source preservation."));}if(!Protect(R,C,Cast<UNiagaraScriptSource>(S->GetLatestSource()))){return false;}}
    for(const auto& H:C.System->GetEmitterHandles())
    {const auto* D=H.GetEmitterData();if(!D){return Fail(R,TEXT("niagara_scratch_emitter_source_unobservable"),TEXT("Every retained emitter requires an observable native version."));}if(!Protect(R,C,Cast<UNiagaraScriptSource>(D->GraphSource))){return false;}}
    for(const auto& Pair:C.Owners->Values)
    {for(const auto& Name:Pair.Value->AsObject()->GetArrayField(TEXT("script_paths")))
        {auto* S=FindObject<UNiagaraScript>(nullptr,*Name->AsString());if(!Loaded(S)){return Fail(R,TEXT("niagara_scratch_source_unobservable"),TEXT("Retained registered private script must already be loaded."));}if(!Protect(R,C,Cast<UNiagaraScriptSource>(S->GetLatestSource()))){return false;}}}
    R.Data->SetStringField(TEXT("system_path"),Path(C.System));R.Data->SetObjectField(TEXT("scratch_catalog_before"),C.Owners);
    R.Data->SetObjectField(TEXT("user_parameters_before"),C.Users);R.Data->SetObjectField(TEXT("emitter_handles_before"),C.HandleState);
    R.Data->SetBoolField(TEXT("package_dirty_before"),C.Dirty);R.Data->SetObjectField(TEXT("compile_state"),CompileState(C.System));return true;
}
bool Owner(FResult& R,FContext& C,const FString& Kind,const FString& IndexJson,const FString& Name,FOwner& O,bool Target)
{
    const FString K=Kind.ToLower();int32 Wanted;
    if(!Index(IndexJson,Wanted)||!Bounded(Name,256)){return Fail(R,TEXT("niagara_scratch_owner_selector_invalid"),TEXT("Owner index requires JSON null or exact int32; name must be bounded."));}
    if(K==TEXT("system")){O.Outer=C.System;O.Scripts=&C.System->ScratchPadScripts;O.Kind=K;O.Name=C.System->GetName();O.Key=K;return true;}
    const bool Parent=K==TEXT("parent_emitter")||K==TEXT("parent");
    if((K!=TEXT("emitter")&&!Parent)||(Target&&Parent)){return Fail(R,TEXT("niagara_scratch_owner_kind_unsupported"),TEXT("Target owners are system/emitter; source may additionally select parent_emitter read-only."));}
    for(int32 I=0;I<C.System->GetEmitterHandles().Num();++I)
    {
        const auto& H=C.System->GetEmitterHandles()[I];if((Wanted!=INDEX_NONE&&Wanted!=I)||(!Name.IsEmpty()&&!Name.Equals(H.GetName().ToString(),ESearchCase::IgnoreCase))){continue;}
        auto* D=H.GetEmitterData();const auto E=H.GetInstance();
        if(!D||!Loaded(E.Emitter)||H.GetEmitterMode()!=ENiagaraEmitterMode::Standard){return Fail(R,TEXT("niagara_scratch_emitter_invalid"),TEXT("Selected emitter must be already-loaded native standard emitter."));}
        if(Target&&D->GetParent().Emitter){return Fail(R,TEXT("niagara_inherited_emitter_unsupported"),TEXT("Detach inherited target emitters before authoring their local private scratch scripts."));}
        auto* Container=Parent?D->ParentScratchPads.Get():D->ScratchPads.Get();
        O.Outer=Container;O.Scripts=Container?&Container->Scripts:nullptr;O.Emitter=E;O.Data=D;O.Index=I;O.Name=H.GetName().ToString();O.Kind=Parent?TEXT("parent_emitter"):TEXT("emitter");
        O.Key=(Parent?TEXT("parent_"):TEXT("emitter_"))+Guid(H.GetId());return true;
    }
    return Fail(R,TEXT("niagara_scratch_owner_not_found"),TEXT("No existing emitter matches the original combined index/name first-match selector."));
}
bool Select(FResult& R,FContext& C,const FString& ScriptPath,const FString& Kind,const FString& ScriptIndexJson,
    const FString& Name,const FString& EmitterIndexJson,const FString& EmitterName,bool Target,bool CloneSource=false)
{
    int32 Wanted;if(!Index(ScriptIndexJson,Wanted)||!Bounded(Name,256)){return Fail(R,TEXT("niagara_scratch_script_selector_invalid"),TEXT("Script index must be JSON null or exact int32; name must be bounded."));}
    auto Match=[&](const FOwner& O,UNiagaraScript* Requested)
    {
        if(!O.Scripts){return false;}
        for(int32 I=0;I<O.Scripts->Num();++I){auto* S=(*O.Scripts)[I].Get();
            if((Requested&&S!=Requested)||(!Requested&&Wanted!=INDEX_NONE&&I!=Wanted)||(!Requested&&!Name.IsEmpty()&&!S->GetName().Equals(Name,ESearchCase::IgnoreCase))){continue;}
            C.Script=S;C.Owner=O;C.ScriptIndex=I;return true;}
        return false;
    };
    if(!ScriptPath.IsEmpty())
    {
        if(!Bounded(ScriptPath)||!ScriptPath.Contains(TEXT(":"))){return Fail(R,TEXT("niagara_scratch_private_path_required"),TEXT("Target-local private script uses an exact subobject path, not an external asset path."));}
        auto* Requested=FindObject<UNiagaraScript>(nullptr,*ScriptPath);
        FOwner O;if(!Loaded(Requested)||!Requested->IsIn(C.System)){return Fail(R,TEXT("niagara_target_local_scratch_required"),TEXT("Direct private script must already be loaded inside this exact system."));}
        if(Owner(R,C,TEXT("system"),TEXT("null"),TEXT(""),O,Target)&&Match(O,Requested)){}
        else
        {
            bool Found=false;
            for(int32 I=0;I<C.System->GetEmitterHandles().Num()&&!Found;++I)
            {FOwner E;if(!Owner(R,C,TEXT("emitter"),LexToString(I),TEXT(""),E,Target)){return false;}Found=Match(E,Requested);}
            if(!Found){return Fail(R,TEXT("niagara_target_local_scratch_required"),TEXT("Direct source is not registered in this system/local-emitter scratch inventory; parent ownership is not a writable shortcut."));}
        }
    }
    else {FOwner O;if(!Owner(R,C,Kind,EmitterIndexJson,EmitterName,O,Target)||!Match(O,nullptr)) {if(R.ErrorCode.IsEmpty()){Fail(R,TEXT("niagara_scratch_script_not_found"),TEXT("No registered scratch script matches the original combined index/name selector."));}return false;}}
    C.KeepScript.Reset(C.Script);C.Source=Cast<UNiagaraScriptSource>(C.Script->GetLatestSource());
    if(!Loaded(C.Script)||C.Script->GetClass()!=UNiagaraScript::StaticClass()||(C.Script->GetUsage()!=ENiagaraScriptUsage::Module&&(!CloneSource||C.Script->GetUsage()!=ENiagaraScriptUsage::DynamicInput))||C.Script->IsAsset()||
        C.Script->GetOutermost()!=C.System->GetOutermost()||!Ordinary(C.Source,C.Graph)||!C.Source->IsIn(C.Script))
    {return Fail(R,TEXT("niagara_scratch_module_source_invalid"),TEXT("Selected registered private module must own an ordinary already-loaded native source graph."));}
    R.Data->SetStringField(TEXT("scratch_pad_script_path"),Path(C.Script));R.Data->SetStringField(TEXT("scratch_pad_script_name"),C.Script->GetName());
    R.Data->SetStringField(TEXT("scratch_pad_owner_kind"),C.Owner.Kind);R.Data->SetStringField(TEXT("scratch_pad_owner_name"),C.Owner.Name);
    R.Data->SetNumberField(TEXT("scratch_pad_script_index"),C.ScriptIndex);R.Data->SetNumberField(TEXT("scratch_pad_emitter_index"),C.Owner.Index);return true;
}

// Concrete guard for the two HN changes in this existing synchronous core.
// UObject identity is held strongly during this call. Raw pin values are never
// admitted after a callback until their current physical membership is checked.
struct FHNWriterGuard
{
    struct FObject
    {
        TWeakObjectPtr<UObject> Object,Outer,Package,Outermost;TWeakObjectPtr<UClass> Class;FString Path;EObjectFlags Flags=RF_NoFlags;TStrongObjectPtr<UObject> Hold,ClassHold,OuterHold,PackageHold,OutermostHold;bool TemplateRole=false;
        explicit FObject(UObject* O,bool AllowTemplate=false):Object(O),Outer(O->GetOuter()),Package(O->GetPackage()),Outermost(O->GetOutermost()),Class(O->GetClass()),Path(O->GetPathName()),Flags(O->GetFlags()),Hold(O),ClassHold(O->GetClass()),OuterHold(O->GetOuter()),PackageHold(O->GetPackage()),OutermostHold(O->GetOutermost()),TemplateRole(AllowTemplate){}
        bool Exact(uint32 AllowedAddedFlags,uint32& ObservedFlags,bool& FlagsObserved)const
        {
            auto* O=Object.Get();FlagsObserved=false;if(!O||!(TemplateRole?(IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed)):Loaded(O))||!IsValid(Class.Get())||Class->HasAnyClassFlags(CLASS_NewerVersionExists)||(Outer.Get()&&!IsValid(Outer.Get()))||!IsValid(Package.Get())||!IsValid(Outermost.Get()))return false;
            ObservedFlags=static_cast<uint32>(O->GetFlags());FlagsObserved=true;const uint32 ExpectedFlags=static_cast<uint32>(Flags);
            return O->GetClass()==Class.Get()&&O->GetOuter()==Outer.Get()&&O->GetPackage()==Package.Get()&&O->GetOutermost()==Outermost.Get()&&O->GetPathName()==Path
                &&(ObservedFlags&ExpectedFlags)==ExpectedFlags&&((ObservedFlags^ExpectedFlags)&~AllowedAddedFlags)==0;
        }
    };
    struct FNodePins{TWeakObjectPtr<UEdGraphNode> Node;FGuid Id;TArray<UEdGraphPin*> Pins;TArray<FGuid> PinIds;TArray<FEdGraphPinType> Types;TArray<FString> Values,Autos;TArray<TWeakObjectPtr<UObject>> Defaults;TArray<const void*> TextIds;};
    struct FEmitter{FGuid Id,Version;TWeakObjectPtr<UNiagaraEmitter> Object;const FVersionedNiagaraEmitterData* Data=nullptr;TWeakObjectPtr<UNiagaraScriptSource> Source;};
    FResult& R;FContext& C;UNiagaraGraph* WrittenGraph=nullptr;TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;TWeakObjectPtr<UPackage> CapturedSystemPackage,CapturedSystemOutermost;
    TArray<TUniquePtr<FObject>> Objects;TArray<FNodePins> FixedPins;TArray<FEmitter> Emitters;TArray<TWeakObjectPtr<UNiagaraScript>> SystemScripts;
    TWeakObjectPtr<UNiagaraScript> DefaultTemplateScript;TWeakObjectPtr<UNiagaraScriptSource> DefaultTemplateSource;TWeakObjectPtr<UNiagaraGraph> DefaultTemplateGraph;const FVersionedNiagaraScriptData* DefaultTemplateData=nullptr;
    FOwner* BlankDestination=nullptr;TWeakObjectPtr<UObject> BlankOuter;TArray<TWeakObjectPtr<UNiagaraScript>> BlankScripts;const FVersionedNiagaraScriptData* BoundCreatedData=nullptr;
    TArray<TWeakObjectPtr<UNiagaraGraph>> Graphs;bool Valid=true,ObserveSavedLoadFlags=false;
    // UK declared retirement/reconstruction only; non-opt-in callers are unchanged.
    TSet<const UObject*> HlslRetiredObjects;TSet<const UEdGraphNode*> HlslReconstructing;
    TFunction<bool()> ExistingHlslCurrent;
    // ACQ: only Hlsl enrolls these copied primitive User declarations. No store
    // data pointer is retained; values keep the existing final UserState proof.
    TArray<FNiagaraVariable> HlslUserDeclarations;
    FHNWriterGuard(FResult& Result,FContext& Context,UNiagaraGraph* G):R(Result),C(Context),WrittenGraph(G)
    {
        if(HNWriter||!Globals()){Stop(TEXT("niagara_scratch_context_unavailable"),TEXT("Exact non-reentrant live Editor context required before this writer boundary."));return;}
        Editor=GEditor;World=GEditor->GetEditorWorldContext().World();if(!Loaded(World.Get())||World->bIsTearingDown){Stop(TEXT("niagara_scratch_world_unavailable"),TEXT("Live non-tearing-down Editor world required."));return;}
        for(UObject* O:{static_cast<UObject*>(C.System),static_cast<UObject*>(C.Script),static_cast<UObject*>(C.Source),static_cast<UObject*>(G)})if(O&&!Track(O))return;
        CapturedSystemPackage=C.System->GetPackage();CapturedSystemOutermost=C.System->GetOutermost();
        for(const auto& P:C.Protected){if(!Track(P.Source)||!TrackGraph(P.Graph))return;}
        if(G&&!Graphs.Contains(G)&&!TrackGraph(G))return;
        for(auto* Script:{C.System->GetSystemSpawnScript(),C.System->GetSystemUpdateScript()}){SystemScripts.Add(Script);if(Script&&!Track(Script))return;}
        for(const auto& Handle:C.System->GetEmitterHandles())
        {FEmitter E;const auto Instance=Handle.GetInstance();E.Id=Handle.GetId();E.Version=Instance.Version;E.Object=Instance.Emitter;E.Data=Handle.GetEmitterData();if(!E.Data||!Track(Instance.Emitter)){Stop(TEXT("niagara_scratch_emitter_identity_unavailable"),TEXT("Exact loaded standard emitter version required."));return;}E.Source=Cast<UNiagaraScriptSource>(E.Data->GraphSource);Emitters.Add(E);}
        // A selected module source may not also be another registered script's writable source.
        for(const auto& Pair:C.Owners->Values)for(const auto& Name:Pair.Value->AsObject()->GetArrayField(TEXT("script_paths")))
        {auto* S=FindObject<UNiagaraScript>(nullptr,*Name->AsString());if(!Loaded(S)||!Track(S)){Stop(TEXT("niagara_scratch_script_identity_unavailable"),TEXT("Registered source script must remain loaded."));return;}if(S!=C.Script&&S->GetLatestSource()==C.Source){Stop(TEXT("niagara_scratch_shared_source_unsupported"),TEXT("Selected scratch source is shared with a different registered script."));return;}}
    }
    bool BindDefaultTemplate(UNiagaraScript* Script,UNiagaraScriptSource* Source,UNiagaraGraph* Graph)
    {DefaultTemplateScript=Script;DefaultTemplateSource=Source;DefaultTemplateGraph=Graph;DefaultTemplateData=Script->GetLatestScriptData();if(!DefaultTemplateData)return Stop(TEXT("niagara_scratch_default_template_data_unavailable"),TEXT("Configured template has no current latest data before duplicate."));return Track(Source)&&TrackGraph(Graph)&&Admit(TEXT("default_template_source_bound"));}
    bool BindBlankDestination(FOwner& O)
    {BlankDestination=&O;BlankOuter=O.Outer;BlankScripts.Empty();if(O.Scripts)for(const auto& S:*O.Scripts)BlankScripts.Add(S.Get());return true;}
    bool AdmitBlankDestination()
    {
        if(!BlankDestination)return true;const auto& O=*BlankDestination;UObject* ActualOuter=nullptr;const TArray<TObjectPtr<UNiagaraScript>>* ActualScripts=nullptr;
        if(O.Kind==TEXT("system")){ActualOuter=C.System;ActualScripts=&C.System->ScratchPadScripts;}
        else{if(!O.Data)return Stop(TEXT("niagara_scratch_blank_owner_changed"),TEXT("Expected local emitter data unavailable."));ActualOuter=O.Data->ScratchPads.Get();if(ActualOuter)ActualScripts=&O.Data->ScratchPads->Scripts;}
        if(ActualOuter!=BlankOuter.Get()||(!ActualScripts&&!BlankScripts.IsEmpty())||(ActualScripts&&ActualScripts->Num()!=BlankScripts.Num()))return Stop(TEXT("niagara_scratch_blank_owner_changed"),TEXT("Current scratch owner/ordered array changed outside the declared append."));
        if(ActualScripts)for(int32 I=0;I<BlankScripts.Num();++I)if((*ActualScripts)[I]!=BlankScripts[I].Get())return Stop(TEXT("niagara_scratch_blank_owner_changed"),TEXT("Existing scratch script identity/order changed during creation callback."));
        return true;
    }
    bool AdoptBlankContainer(UObject* O){BlankOuter=O;return AdmitBlankDestination();}
    bool ExpectBlankAppend(UNiagaraScript* O){BlankScripts.Add(O);return AdmitBlankDestination();}
    bool BindCreatedBlank(UNiagaraScript* Script,UNiagaraScriptSource* Source,UNiagaraGraph* Graph)
    {
        if(!Track(Script)||!Track(Source)||!TrackGraph(Graph))return false;
        C.Script=Script;C.Source=Source;C.Graph=Graph;BoundCreatedData=Script->GetLatestScriptData();
        if(!BoundCreatedData)return Stop(TEXT("niagara_scratch_blank_data_unavailable"),TEXT("Created blank module latest data is unavailable before registration."));return Admit(TEXT("created_blank_module_bound"));
    }
    bool Globals()const{return IsInGameThread()&&GEditor&&!IsEngineExitRequested()&&!GExitPurge&&!IsGarbageCollecting()&&!UE::IsSavingPackage();}
    bool Stop(const TCHAR* Code,const TCHAR* Message){Valid=false;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("projection_skipped_unsafe"),true);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);return Fail(R,Code,Message);}
    bool Track(UObject* O,bool AllowTemplate=false)
    {if(!Valid||!Globals()||!(AllowTemplate?(IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed)):Loaded(O)))return Stop(TEXT("niagara_scratch_object_unavailable"),TEXT("Writer identity is unavailable; no further UObject reads or retry."));for(const auto& E:Objects)if(E->Object.Get()==O)return true;Objects.Add(MakeUnique<FObject>(O,AllowTemplate));return true;}
    bool TrackPins(UEdGraphNode* N)
    {if(!Track(N)||N->Pins.Num()>MaxPins)return Stop(TEXT("niagara_scratch_pin_inventory_invalid"),TEXT("Bounded native node pin inventory required."));FNodePins E;E.Node=N;E.Id=N->NodeGuid;E.Pins=N->Pins;TSet<UEdGraphPin*> Seen;for(auto* P:E.Pins){if(!P||Seen.Contains(P)||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||P->IsPendingKill())return Stop(TEXT("niagara_scratch_pin_inventory_invalid"),TEXT("Current unique live pins must belong to the exact node."));Seen.Add(P);E.PinIds.Add(P->PinId);E.Types.Add(P->PinType);E.Values.Add(P->DefaultValue);E.Autos.Add(P->AutogeneratedDefaultValue);E.Defaults.Add(P->DefaultObject);E.TextIds.Add(FTextInspector::GetSharedDataId(P->DefaultTextValue));if(P->PinType.PinSubCategoryObject.IsStale()||P->PinType.PinValueType.TerminalSubCategoryObject.IsStale())return Stop(TEXT("niagara_scratch_pin_type_retired"),TEXT("Actual pin subtype was retired."));for(UObject* Type:TArray<UObject*>{P->PinType.PinSubCategoryObject.Get(),P->PinType.PinValueType.TerminalSubCategoryObject.Get(),P->DefaultObject})if(Type&&!Track(Type,true))return false;}FixedPins.Add(MoveTemp(E));if(const auto* Call=Cast<UNiagaraNodeFunctionCall>(N)){if(Call->FunctionScript&&!Track(Call->FunctionScript))return false;for(const auto& V:Call->Signature.Inputs)if(!HNKeepType(V.GetType()))return false;for(const auto& V:Call->Signature.Outputs)if(!HNKeepType(V.GetType()))return false;}return true;}
    bool TrackGraph(UNiagaraGraph* G)
    {if(Graphs.Contains(G))return Admit(TEXT("existing_tracked_graph"));if(!Track(G)||G->Nodes.Num()>MaxNodes)return Stop(TEXT("niagara_scratch_graph_inventory_invalid"),TEXT("Bounded live graph required."));Graphs.AddUnique(G);for(const auto& Entry:G->Nodes){auto* N=Entry.Get();if(!Loaded(N)||N->GetGraph()!=G||N->GetOuter()!=G||!TrackPins(N))return Stop(TEXT("niagara_scratch_graph_node_invalid"),TEXT("Every initial graph node must be an exact owned current member."));}return true;}
    bool Admit(const TCHAR* Stage)
    {
        if(!Valid)return false;R.Stage=Stage;
        if(!Globals())return Stop(TEXT("niagara_scratch_context_unavailable"),TEXT("GC/exit/save or Editor game-thread admission failed after SDK; actual work retained."));
        if(GEditor!=Editor.Get()||GEditor->GetEditorWorldContext().World()!=World.Get()||!Loaded(World.Get())||World->bIsTearingDown||GEditor->PlayWorld||GEditor->GetPIEWorldContext())return Stop(TEXT("niagara_scratch_context_identity_changed"),TEXT("Exact Editor/world or stopped-play context changed; no new SDK dispatch."));
        for(const auto& O:Objects)
        {
            if(HlslRetiredObjects.Contains(O->Hold.Get()))continue; // no retired getter
            const bool SavedPackageObject=ObserveSavedLoadFlags&&O->Package.Get()==CapturedSystemPackage.Get()&&O->Outermost.Get()==CapturedSystemOutermost.Get()&&!EnumHasAnyFlags(O->Flags,RF_Transient);
            const uint32 Allowed=SavedPackageObject?static_cast<uint32>(RF_WasLoaded|RF_LoadCompleted):0;
            uint32 ActualFlags=0;bool FlagsObserved=false;
            if(!O->Exact(Allowed,ActualFlags,FlagsObserved))
            {
                R.Data->SetStringField(TEXT("guard_failed_object_expected_path"),O->Path);
                R.Data->SetNumberField(TEXT("guard_failed_flags_before"),static_cast<uint32>(O->Flags));
                R.Data->SetBoolField(TEXT("guard_failed_flags_after_observed"),FlagsObserved);
                if(FlagsObserved)R.Data->SetNumberField(TEXT("guard_failed_flags_after"),ActualFlags);
                R.Data->SetNumberField(TEXT("guard_failed_allowed_added_flags"),Allowed);
                return Stop(TEXT("niagara_scratch_writer_identity_changed"),TEXT("Captured class/outer/effective package/path or non-SDK flag transition changed; no replacement adopted."));
            }
        }
        if((C.Script&&(!C.Source||C.Script->GetLatestSource()!=C.Source||C.Source->NodeGraph!=C.Graph))||C.System->GetSystemSpawnScript()!=SystemScripts[0].Get()||C.System->GetSystemUpdateScript()!=SystemScripts[1].Get()||C.System->GetEmitterHandles().Num()!=Emitters.Num())return Stop(TEXT("niagara_scratch_writer_routing_changed"),TEXT("Selected script/source or System routing changed across SDK."));
        for(int32 I=0;I<Emitters.Num();++I){const auto& Handle=C.System->GetEmitterHandles()[I];const auto Instance=Handle.GetInstance();const auto& E=Emitters[I];if(Handle.GetId()!=E.Id||Instance.Emitter!=E.Object.Get()||Instance.Version!=E.Version||Handle.GetEmitterData()!=E.Data)return Stop(TEXT("niagara_scratch_writer_emitter_changed"),TEXT("Captured emitter version changed before version-data access."));if(Cast<UNiagaraScriptSource>(E.Data->GraphSource)!=E.Source.Get())return Stop(TEXT("niagara_scratch_writer_source_changed"),TEXT("Emitter graph source changed across SDK."));}
        if(!AdmitBlankDestination())return false;
        if(DefaultTemplateData&&(!DefaultTemplateScript.IsValid()||DefaultTemplateScript->GetLatestSource()!=DefaultTemplateSource.Get()||DefaultTemplateScript->GetLatestScriptData()!=DefaultTemplateData||!DefaultTemplateSource.IsValid()||DefaultTemplateSource->NodeGraph!=DefaultTemplateGraph.Get()))return Stop(TEXT("niagara_scratch_default_template_routing_changed"),TEXT("Captured template Script/latest data/Source/Graph routing changed during creation callback."));
        if(BoundCreatedData&&C.Script->GetLatestScriptData()!=BoundCreatedData)return Stop(TEXT("niagara_scratch_blank_version_changed"),TEXT("Created module latest version data changed across callback before readback."));
        if(BlankDestination)for(const auto& P:C.Protected)if(!Loaded(P.Source)||P.Source->NodeGraph!=P.Graph)return Stop(TEXT("niagara_scratch_writer_source_changed"),TEXT("Retained source-to-graph binding changed before blank creation."));
        for(const auto& E:FixedPins){if(HlslRetiredObjects.Contains(E.Node.Get())||HlslReconstructing.Contains(E.Node.Get()))continue;auto* N=E.Node.Get();if(!N||N->NodeGuid!=E.Id||!N->GetGraph()->Nodes.Contains(N)||N->Pins.Num()!=E.Pins.Num())return Stop(TEXT("niagara_scratch_retained_pin_changed"),TEXT("An existing node or pin inventory changed across callback."));for(int32 I=0;I<E.Pins.Num();++I){auto* P=N->Pins[I];if(P!=E.Pins[I]||!P||P->GetOwningNodeUnchecked()!=N||P->IsPendingKill()||P->PinId!=E.PinIds[I])return Stop(TEXT("niagara_scratch_retained_pin_changed"),TEXT("An original physical pin was replaced or retired; no old raw pin is read."));if(N->GetGraph()!=WrittenGraph&&(!(P->PinType==E.Types[I])||P->DefaultValue!=E.Values[I]||P->AutogeneratedDefaultValue!=E.Autos[I]||P->DefaultObject!=E.Defaults[I].Get()||FTextInspector::GetSharedDataId(P->DefaultTextValue)!=E.TextIds[I]))return Stop(TEXT("niagara_scratch_retained_pin_value_changed"),TEXT("Immutable source pin type/default binding changed across callback."));}}
        if(!HlslUserDeclarations.IsEmpty())
        {
            const auto& Store=C.System->GetExposedParameters();TArray<FNiagaraVariable> CurrentUsers;Store.GetParameters(CurrentUsers);
            if(Store.GetOwner()!=C.System||CurrentUsers.Num()>MaxParameters)return Stop(TEXT("niagara_hlsl_user_declaration_current_lost"),TEXT("Current User store owner/declaration budget changed across callback; no following writer or retry."));
            for(const auto& Expected:HlslUserDeclarations)
            {
                int32 Matches=0;for(const auto& Actual:CurrentUsers)if(Actual.GetName()==Expected.GetName()){if(Actual.GetType()!=Expected.GetType())return Stop(TEXT("niagara_hlsl_user_declaration_current_lost"),TEXT("Planned User input type changed across callback; actual graph prefix retained."));++Matches;}
                FNiagaraVariable Redirected=Expected;if(Matches!=1||!Store.RedirectUserVariable(Redirected)||Redirected!=Expected)return Stop(TEXT("niagara_hlsl_user_declaration_current_lost"),TEXT("Planned User declaration disappeared, became ambiguous or redirected across callback; no subsequent source read/write."));
            }
        }
        return !ExistingHlslCurrent||ExistingHlslCurrent();
    }
    bool AdmitAfterSave(bool SaveReturnedTrue)
    {
        // SavePackage2 marks serialized exports RF_WasLoaded|RF_LoadCompleted.
        // Only this successful Save boundary permits additions of these two bits
        // on non-transient objects captured in the exact saved package. No flag removal, other
        // flag, class, owner, package, path, source, version or pin change is allowed.
        TGuardValue<bool> SaveFlagsScope(ObserveSavedLoadFlags,SaveReturnedTrue);
        if(!Admit(TEXT("after_SaveLoadedAsset")))return false;
        TArray<TSharedPtr<FJsonValue>> Changes;int32 ChangedCount=0;
        if(SaveReturnedTrue)for(auto& O:Objects)
        {
            if(HlslRetiredObjects.Contains(O->Hold.Get()))continue;
            if(O->Package.Get()!=CapturedSystemPackage.Get()||O->Outermost.Get()!=CapturedSystemOutermost.Get()||EnumHasAnyFlags(O->Flags,RF_Transient))continue;
            const EObjectFlags Actual=O->Object->GetFlags();if(Actual==O->Flags)continue;
            ++ChangedCount;
            if(Changes.Num()<32){auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("object_path"),O->Path);Row->SetNumberField(TEXT("flags_before"),static_cast<uint32>(O->Flags));Row->SetNumberField(TEXT("flags_after"),static_cast<uint32>(Actual));Row->SetNumberField(TEXT("added_flags"),static_cast<uint32>(Actual)&~static_cast<uint32>(O->Flags));Changes.Add(MakeShared<FJsonValueObject>(Row));}
            O->Flags=Actual;
        }
        R.Data->SetBoolField(TEXT("save_loaded_flag_transition_checked"),true);
        R.Data->SetNumberField(TEXT("save_loaded_flag_transition_count"),ChangedCount);
        R.Data->SetNumberField(TEXT("save_loaded_flag_diagnostics_omitted"),ChangedCount-Changes.Num());
        R.Data->SetArrayField(TEXT("save_loaded_flag_transitions"),Changes);
        return true;
    }
    bool CurrentPin(const UEdGraphPin* Pin)
    {if(!Admit(TEXT("current_pin_admission"))||!Pin)return false;for(const auto& Weak:Graphs){auto* G=Weak.Get();if(!Loaded(G)||G->Nodes.Num()>MaxNodes)return Stop(TEXT("niagara_scratch_graph_inventory_invalid"),TEXT("Current graph membership unavailable."));for(const auto& Entry:G->Nodes){auto* N=Entry.Get();if(!Loaded(N)||N->GetGraph()!=G||N->GetOuter()!=G||N->Pins.Num()>MaxPins)return Stop(TEXT("niagara_scratch_graph_node_invalid"),TEXT("Current node membership unavailable."));for(auto* P:N->Pins)if(P==Pin)return P&&P->GetOwningNodeUnchecked()==N&&!P->IsPendingKill()||Stop(TEXT("niagara_scratch_pin_retired"),TEXT("Current pin owner or lifetime changed."));}}return Stop(TEXT("niagara_scratch_pin_retired"),TEXT("Raw pin is outside current exact physical graph membership."));}
    bool PackageOwned()
    {if(!Admit(TEXT("save_package_admission")))return false;TArray<UObject*> Exports;GetObjectsWithPackage(C.System->GetOutermost(),Exports,EGetObjectsFlags::IncludeNestedObjects);for(auto* O:Exports)if(!HlslRetiredObjects.Contains(O)&&(!Loaded(O)||O->GetPackage()!=C.System->GetPackage()||O->GetOutermost()!=C.System->GetOutermost()||(O!=C.System&&!O->IsIn(C.System))))return Stop(TEXT("niagara_scratch_save_foreign_export"),TEXT("Exact System root/subtree package ownership required before and after save."));return true;}
};
bool HNAdmit(const TCHAR* Stage){return (!HNWriter||HNWriter->Admit(Stage))&&STCloneAdmit();}
bool HNTrackProtectedSource(UNiagaraScriptSource* Source,UNiagaraGraph* Graph){return !HNWriter||(HNWriter->Track(Source)&&HNWriter->TrackGraph(Graph));}
bool HNCurrentPin(const UEdGraphPin* Pin){return !HNWriter||HNWriter->CurrentPin(Pin);}
bool HNKeepType(const FNiagaraTypeDefinition& Type){if(!HNWriter)return true;UObject* O=Type.IsEnum()?static_cast<UObject*>(Type.GetEnum()):static_cast<UObject*>(Type.GetStruct());return !O||HNWriter->Track(O,true);}
bool HNDispatch(const TCHAR* Name,bool Writer)
{if(!HNAdmit(Name))return false;if(!HNWriter)return true;auto& R=HNWriter->R;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetStringField(TEXT("last_SDK_entered"),Name);if(Writer){R.bMutationAttempted=true;R.bChangeStateKnown=false;}return true;}

bool Preserve(FResult& R,FContext& C,UNiagaraGraph* ChangedGraph,const FString& AddedOwner=FString(),UNiagaraScript* Added=nullptr)
{
    if(!HNAdmit(TEXT("before_preservation_snapshot")))return false;
    bool Complete=false;auto Users=UserState(C.System,Complete);const bool UserSame=Complete&&Text(Users)==Text(C.Users);
    auto H=Handles(C.System);const bool HandleSame=Text(H)==Text(C.HandleState);
    bool OwnersComplete=false;auto Owners=Catalog(C.System,OwnersComplete);TSharedPtr<FJsonObject> Expected;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text(C.Owners)),Expected)||!Expected.IsValid())
    {R.bChangeStateKnown=false;return Fail(R,TEXT("niagara_scratch_catalog_unobservable"),TEXT("Retained catalog comparison could not be decoded; do not retry mutation."));}
    if(Added)
    {auto Row=Expected->GetObjectField(AddedOwner);auto Paths=Row->GetArrayField(TEXT("script_paths"));Paths.Add(MakeShared<FJsonValueString>(Path(Added)));
        Row->SetArrayField(TEXT("script_paths"),Paths);Row->SetStringField(TEXT("owner_path"),Path(Added->GetOuter()));}
    const bool OwnerSame=OwnersComplete&&Text(Owners)==Text(Expected.ToSharedRef());
    bool Sources=true;TArray<TSharedPtr<FJsonValue>> Rows;
    for(const auto& P:C.Protected)
    {
        if(P.Graph==ChangedGraph){continue;}const auto After=Capture(P.Graph);if(!HNAdmit(TEXT("after_protected_graph_snapshot")))return false;bool ParamsComplete=false;auto Parameters=GraphParameters(P.Graph,ParamsComplete);
        const bool Same=After.bComplete&&ParamsComplete&&SameGraph(P.Before,After)&&Text(P.Parameters)==Text(Parameters)&&
            Loaded(P.Source)&&P.Source->NodeGraph.Get()==P.Graph&&(P.Graph->GetOutermost()==C.System->GetOutermost()||P.Dirty==P.Graph->GetOutermost()->IsDirty());
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("graph_path"),Path(P.Graph));Row->SetObjectField(TEXT("before"),P.Before.Json);Row->SetObjectField(TEXT("after"),After.Json);
        Row->SetObjectField(TEXT("parameters_before"),P.Parameters);Row->SetObjectField(TEXT("parameters_after"),Parameters);Row->SetBoolField(TEXT("preserved"),Same);Rows.Add(MakeShared<FJsonValueObject>(Row));Sources&=Same;
        if(!After.bComplete||!ParamsComplete){R.bChangeStateKnown=false;}
    }
    R.Data->SetObjectField(TEXT("user_parameters_after"),Users);R.Data->SetBoolField(TEXT("user_parameters_preserved"),UserSame);
    R.Data->SetObjectField(TEXT("emitter_handles_after"),H);R.Data->SetBoolField(TEXT("emitter_handles_preserved"),HandleSame);
    R.Data->SetObjectField(TEXT("scratch_catalog_after"),Owners);R.Data->SetBoolField(TEXT("scratch_catalog_changes_bounded"),OwnerSame);
    R.Data->SetArrayField(TEXT("protected_sources"),Rows);R.Data->SetBoolField(TEXT("source_graphs_preserved"),Sources);
    if(!Complete||!OwnersComplete){R.bChangeStateKnown=false;}
    if(!UserSame||!HandleSame||!OwnerSame||!Sources){return Fail(R,TEXT("niagara_scratch_preservation_failed"),TEXT("User/handles/registered scratch catalog or retained native source graph changed beyond the selected operation; actual changes retained."));}return true;
}
void Complete(FResult& R)
{
    R.Data->SetBoolField(TEXT("outcome_unknown"),!R.bChangeStateKnown);R.Data->SetBoolField(TEXT("actual_changes_retained"),R.bChanged||!R.bChangeStateKnown);
    R.bSuccess=R.bChangeStateKnown&&R.ErrorCode.IsEmpty();R.bPostconditionPassed=R.bSuccess;if(R.bSuccess){R.Stage=TEXT("complete");}
}
bool STClonePreserved();
void Finish(FResult& R,FContext& C,UNiagaraGraph* Graph,const FSnapshot& Written,bool Save,bool Compile,
    const FString& AddedOwner=FString(),UNiagaraScript* Added=nullptr,bool LegacyStackSaveOrder=false)
{
    TUniquePtr<FHNWriterGuard> StackGuard;if(LegacyStackSaveOrder&&!HNWriter){StackGuard=MakeUnique<FHNWriterGuard>(R,C,Graph);if(!StackGuard->Valid){Complete(R);return;}}
    TGuardValue<FHNWriterGuard*> HNStackScope(HNWriter,StackGuard?StackGuard.Get():HNWriter);
    if(!HNAdmit(TEXT("before_compile_save"))){Complete(R);return;}
    bool DiskUnknown=false,ParametersComplete=false;const auto WrittenParameters=GraphParameters(Graph,ParametersComplete);
    if(!ParametersComplete){R.bChangeStateKnown=false;Fail(R,TEXT("niagara_scratch_parameters_unobservable"),TEXT("Selected metadata after write is incomplete; compile/save suppressed."));}
    if(R.ErrorCode.IsEmpty()&&R.bChangeStateKnown)
    {
        if(Compile){R.Stage=TEXT("request_compile");R.Data->SetBoolField(TEXT("compile_requested"),true);R.Data->SetBoolField(TEXT("compile_attempted"),true);
            R.Data->SetNumberField(TEXT("explicit_compile_requests"),1);if(!HNDispatch(TEXT("RequestCompile"))){Complete(R);return;}const bool Returned=C.System->RequestCompile(false);R.Data->SetBoolField(TEXT("compile_request_returned"),Returned);
            if(!HNAdmit(TEXT("after_RequestCompile"))||!STClonePreserved()){Complete(R);return;}}
        if(Save){auto State=CompileState(C.System);if(!HNAdmit(TEXT("after_save_compile_state"))||!STClonePreserved()){Complete(R);return;}R.Data->SetObjectField(TEXT("save_compile_state"),State);
            R.Data->SetBoolField(TEXT("legacy_stack_compile_then_save"),LegacyStackSaveOrder);
            if(!LegacyStackSaveOrder&&!State->GetBoolField(TEXT("ready_to_save"))){R.Stage=TEXT("save_deferred");R.Data->SetBoolField(TEXT("save_deferred"),true);Fail(R,TEXT("niagara_save_deferred_compile_pending"),TEXT("Actual scratch work remains; inspect async readiness and save the exact system instead of replaying creation/write/insertion."));}
            else{if(HNWriter&&!HNWriter->PackageOwned()){Complete(R);return;}R.Stage=TEXT("save");R.Data->SetBoolField(TEXT("save_attempted"),true);R.Data->SetNumberField(TEXT("exact_save_requests"),1);
                if(!HNDispatch(TEXT("SaveLoadedAsset"))){Complete(R);return;}const bool Saved=UEditorAssetLibrary::SaveLoadedAsset(C.System,false);R.Data->SetBoolField(TEXT("saved"),Saved||R.Data->GetBoolField(TEXT("saved")));
                R.Data->SetBoolField(TEXT("save_sdk_returned"),true);R.Data->SetBoolField(TEXT("save_sdk_may_wait_for_compilation"),true);
                if(!(HNWriter?HNWriter->AdmitAfterSave(Saved):HNAdmit(TEXT("after_SaveLoadedAsset")))||(HNWriter&&!HNWriter->PackageOwned())){Complete(R);return;}
                if(!Saved){DiskUnknown=true;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("disk_outcome_unknown"),true);Fail(R,TEXT("niagara_save_failed"),TEXT("Save returned false; actual disk outcome is uncertain, no rollback/retry."));Complete(R);return;}
                else if(C.System->GetOutermost()->IsDirty()){DiskUnknown=true;Fail(R,TEXT("niagara_save_postcondition_failed"),TEXT("Exact saved system remains dirty."));}}
        }
    }
    const auto After=Capture(Graph);if(!HNAdmit(TEXT("after_compile_save_snapshot"))){Complete(R);return;}R.Data->SetObjectField(TEXT("after"),After.Json);R.Data->SetObjectField(TEXT("graph_after"),After.Json);
    const bool Same=SameGraph(Written,After);R.Data->SetBoolField(TEXT("graph_preserved_across_compile_save"),Same);
    if(!After.bComplete){R.bChangeStateKnown=false;}if(!Same){Fail(R,TEXT("niagara_scratch_compile_save_graph_changed"),TEXT("Selected graph node/pin/default/link state changed across compile/save; observed work remains."));}
    bool FinalParametersComplete=false;const auto FinalParameters=GraphParameters(Graph,FinalParametersComplete);
    const bool ParameterSame=ParametersComplete&&FinalParametersComplete&&Text(WrittenParameters)==Text(FinalParameters);
    R.Data->SetObjectField(TEXT("graph_parameters_after"),FinalParameters);R.Data->SetBoolField(TEXT("graph_parameters_preserved_across_compile_save"),ParameterSame);
    if(!FinalParametersComplete){R.bChangeStateKnown=false;}if(!ParameterSame){Fail(R,TEXT("niagara_scratch_compile_save_parameters_changed"),TEXT("Selected typed parameter metadata changed across compile/save; actual work retained."));}
    Preserve(R,C,Added?nullptr:Graph,AddedOwner,Added);if(!R.bChangeStateKnown){Complete(R);return;}if(!HNAdmit(TEXT("after_compile_save_preservation"))){Complete(R);return;}R.Data->SetBoolField(TEXT("package_dirty_after"),C.System->GetOutermost()->IsDirty());R.Data->SetObjectField(TEXT("compile_state"),CompileState(C.System));
    if(DiskUnknown){R.bChangeStateKnown=false;}R.Data->SetBoolField(TEXT("disk_outcome_unknown"),DiskUnknown);Complete(R);
}
struct FStack
{
    UNiagaraGraph* Graph=nullptr; UNiagaraNodeOutput* Output=nullptr; UNiagaraNodeInput* Input=nullptr;
    UNiagaraScript* Module=nullptr; FGuid ModuleVersion; FSnapshot Before;
    TArray<UNiagaraNodeFunctionCall*> Modules;
};
void AddEdge(TMap<FString,TArray<FString>>& Links,const FString& A,const FString& B)
{Links.FindOrAdd(A).AddUnique(B);Links.FindOrAdd(B).AddUnique(A);}
void RemoveEdge(TMap<FString,TArray<FString>>& Links,const FString& A,const FString& B)
{if(auto* V=Links.Find(A)){V->Remove(B);}if(auto* V=Links.Find(B)){V->Remove(A);}}
bool Stack(FResult& R, FStack& C)
{
    C.Modules.Empty(); C.Input = nullptr; UEdGraphNode* Current = C.Output; TSet<UEdGraphNode*> Visited;
    while (Current && Visited.Num() < MaxNodes)
    {
        if (!Loaded(Current) || Current->GetOuter() != C.Graph || Visited.Contains(Current) || !Cast<UNiagaraNode>(Current))
        { return Fail(R, TEXT("niagara_stack_chain_invalid"), TEXT("Selected map chain is cyclic, foreign or incomplete; no insertion dispatched.")); }
        Visited.Add(Current);
        if (auto* Call = Cast<UNiagaraNodeFunctionCall>(Current)) { C.Modules.Insert(Call, 0); }
        UEdGraphPin* In = nullptr; UEdGraphPin* Out = nullptr;
        if (!MapPin(Current, EGPD_Input, In) || !MapPin(Current, EGPD_Output, Out))
        { return Fail(R, TEXT("niagara_stack_map_pin_ambiguous"), TEXT("Map-chain nodes must have at most one non-orphan map pin per direction.")); }
        if (auto* Input = Cast<UNiagaraNodeInput>(Current))
        {
            if (In || !Out || Input->Input.GetType() != FNiagaraTypeDefinition::GetParameterMapDef())
            { return Fail(R, TEXT("niagara_stack_input_invalid"), TEXT("Selected chain does not end at an exact parameter-map input.")); }
            C.Input = Input; break;
        }
        if (!In || In->LinkedTo.Num() != 1 || !In->LinkedTo[0] || In->LinkedTo[0]->Direction != EGPD_Output)
        { return Fail(R, TEXT("niagara_stack_input_link_invalid"), TEXT("Selected map chain requires one reciprocal upstream map link per node.")); }
        UEdGraphPin* Source = In->LinkedTo[0]; UEdGraphNode* SourceNode = Source->GetOwningNode(); UEdGraphPin* SourceMap = nullptr;
        if (!Loaded(SourceNode) || SourceNode->GetOuter() != C.Graph || !MapPin(SourceNode, EGPD_Output, SourceMap) || SourceMap != Source || !Source->LinkedTo.Contains(In))
        { return Fail(R, TEXT("niagara_stack_upstream_invalid"), TEXT("Upstream pin is not the exact owned reciprocal parameter-map output.")); }
        Current = SourceNode;
    }
    if (!C.Input || C.Modules.Num() > MaxNodes - 2)
    { return Fail(R, TEXT("niagara_stack_input_missing"), TEXT("Bounded selected stack lacks a valid parameter-map input/output pair.")); }
    // The native grouping routine dereferences every start-node map input.
    // Validate all linked starts of each group boundary, including overrides.
    TArray<UEdGraphNode*> Boundaries; Boundaries.Add(C.Input); for (auto* M : C.Modules) { Boundaries.Add(M); }
    for (UEdGraphNode* Boundary : Boundaries)
    {
        UEdGraphPin* Out = nullptr;
        if (!MapPin(Boundary, EGPD_Output, Out) || !Out || Out->LinkedTo.IsEmpty())
        { return Fail(R, TEXT("niagara_stack_boundary_invalid"), TEXT("A selected stack boundary has no parameter-map output/start group.")); }
        for (UEdGraphPin* Target : Out->LinkedTo)
        {
            UEdGraphNode* Node = Target ? Target->GetOwningNode() : nullptr; UEdGraphPin* In = nullptr;
            if (!Loaded(Node) || !Cast<UNiagaraNode>(Node) || Node->GetOuter() != C.Graph ||
                !MapPin(Node, EGPD_Input, In) || !In || In != Target || !In->LinkedTo.Contains(Out))
            { return Fail(R, TEXT("niagara_stack_group_invalid"), TEXT("A native stack start group contains a missing/foreign/non-map node; no unsafe grouping call dispatched.")); }
        }
    }
    return true;
}
TArray<TSharedPtr<FJsonValue>> ModuleRows(const FStack& C)
{
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (int32 I = 0; I < C.Modules.Num(); ++I)
    { auto Row = NodeMetadata(C.Modules[I]); Row->SetNumberField(TEXT("module_index"), I); Rows.Add(MakeShared<FJsonValueObject>(Row)); }
    return Rows;
}

bool VerifyInsertion(FResult& R, FStack& C, UNiagaraNodeFunctionCall* NewModule, const FSnapshot& After,
    int32 InsertIndex, const FString& PreviousId, const TArray<FString>& NextIds, const TArray<UNiagaraNodeFunctionCall*>& BeforeModules)
{
    if (!After.bComplete) { R.bChangeStateKnown = false; return Fail(R, TEXT("niagara_insert_state_unobservable"), TEXT("Native insertion was dispatched but actual bounded graph state is incomplete; stop without retry.")); }
    const bool PriorNodes = [&]() { for (const auto& Pair : C.Before.Nodes) { const auto* V = After.Nodes.Find(Pair.Key); if (!V || *V != Pair.Value) { return false; } } return true; }();
    const bool PriorPins = [&]() { for (const auto& Pair : C.Before.Pins) { const auto* V = After.Pins.Find(Pair.Key); if (!V || *V != Pair.Value) { return false; } } return true; }();
    R.Data->SetBoolField(TEXT("prior_node_metadata_preserved"), PriorNodes); R.Data->SetBoolField(TEXT("prior_pin_metadata_preserved"), PriorPins);
    const bool Identity = Loaded(NewModule) && NewModule->GetClass() == UNiagaraNodeFunctionCall::StaticClass() && NewModule->GetOuter() == C.Graph &&
        C.Graph->Nodes.Contains(NewModule) && NewModule->FunctionScript == C.Module && NewModule->SelectedScriptVersion == C.ModuleVersion &&
        NewModule->NodeGuid.IsValid() && !C.Before.Nodes.Contains(Path(NewModule)) && After.Nodes.Num() == C.Before.Nodes.Num() + 1 && !NewModule->GetFunctionName().IsEmpty();
    R.Data->SetBoolField(TEXT("new_module_identity_verified"), Identity);
    if (!PriorNodes || !PriorPins || !Identity) { return Fail(R, TEXT("niagara_insert_node_postcondition_failed"), TEXT("New module identity/version/node count or existing node/pin metadata differs; observed partial graph retained.")); }
    for (UEdGraphNode* Existing : C.Graph->Nodes)
    { if (Existing != NewModule && Existing->NodeGuid == NewModule->NodeGuid) { return Fail(R, TEXT("niagara_insert_guid_duplicate"), TEXT("Inserted module GUID duplicates an existing node; observed graph retained.")); } }
    UEdGraphPin* NewIn = nullptr; UEdGraphPin* NewOut = nullptr;
    if (!MapPin(NewModule, EGPD_Input, NewIn) || !MapPin(NewModule, EGPD_Output, NewOut) || !NewIn || !NewOut)
    { return Fail(R, TEXT("niagara_insert_map_pins_invalid"), TEXT("Inserted module lacks unique actual parameter-map input/output pins; graph retained.")); }
    auto ExpectedLinks = C.Before.Links; const FString InputId = PinId(NewIn), OutputId = PinId(NewOut);
    for (const FString& Next : NextIds) { RemoveEdge(ExpectedLinks, PreviousId, Next); AddEdge(ExpectedLinks, OutputId, Next); }
    AddEdge(ExpectedLinks, PreviousId, InputId);
    for (const UEdGraphPin* Pin : NewModule->Pins) { ExpectedLinks.FindOrAdd(PinId(Pin)); }
    for (auto& Pair : ExpectedLinks) { Pair.Value.Sort(); }
    bool ExactLinks = ExpectedLinks.Num() == After.Links.Num();
    for (const auto& Pair : ExpectedLinks) { const auto* V = After.Links.Find(Pair.Key); if (!V || *V != Pair.Value) { ExactLinks = false; break; } }
    R.Data->SetBoolField(TEXT("exact_native_splice_verified"), ExactLinks);
    if (!ExactLinks) { return Fail(R, TEXT("niagara_insert_splice_postcondition_failed"), TEXT("Actual link union differs from the one native boundary splice; all observed changes retained.")); }
    if (!Stack(R, C)) { return false; }
    auto ExpectedModules = BeforeModules; ExpectedModules.Insert(NewModule, InsertIndex);
    const bool Order = ExpectedModules == C.Modules;
    R.Data->SetBoolField(TEXT("module_order_verified"), Order); R.Data->SetArrayField(TEXT("ordered_modules"), ModuleRows(C));
    R.Data->SetNumberField(TEXT("module_count"), C.Modules.Num());
    if (!Order) { return Fail(R, TEXT("niagara_insert_order_postcondition_failed"), TEXT("Observed module order differs from the clamped legacy absolute insertion index.")); }
    return true;
}

bool LiteralType(const FString& Name,FNiagaraTypeDefinition& T)
{
    const FString N=Token(Name);
    if(N==TEXT("float")){T=FNiagaraTypeDefinition::GetFloatDef();}
    else if(N==TEXT("int")||N==TEXT("integer")){T=FNiagaraTypeDefinition::GetIntDef();}
    else if(N==TEXT("bool")||N==TEXT("boolean")){T=FNiagaraTypeDefinition::GetBoolDef();}
    else if(N==TEXT("vec2")||N==TEXT("vector2")){T=FNiagaraTypeDefinition::GetVec2Def();}
    else if(N==TEXT("vec3")||N==TEXT("vector")||N==TEXT("vector3")){T=FNiagaraTypeDefinition::GetVec3Def();}
    else if(N==TEXT("vec4")||N==TEXT("vector4")){T=FNiagaraTypeDefinition::GetVec4Def();}
    else if(N==TEXT("color")||N==TEXT("linearcolor")){T=FNiagaraTypeDefinition::GetColorDef();}
    else if(N==TEXT("position")){T=FNiagaraTypeDefinition::GetPositionDef();}
    else{return false;}return T.IsValid();
}
bool Default(FResult& R,const FNiagaraTypeDefinition& T,const FString& Raw,const FString& Json,
    const UEdGraphSchema_Niagara* Schema,FString& Out)
{
    if(!Raw.IsEmpty()){if(!Bounded(Raw,4096)){return Fail(R,TEXT("niagara_scratch_default_invalid"),TEXT("Raw default string exceeds finite bounds or includes control characters."));}Out=Raw;return true;}
    TArray<TSharedPtr<FJsonValue>> Values;
    if(Json.Len()>16384||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+Json+TEXT("]")),Values)||Values.Num()!=1||!Values[0].IsValid())
    {return Fail(R,TEXT("niagara_scratch_value_invalid"),TEXT("One bounded JSON value is required when raw default_value is empty."));}
    const auto& V=Values[0];if(V->Type==EJson::Null){Out.Empty();return true;}
    if(V->Type==EJson::String){Out=V->AsString();if(!Bounded(Out,4096)){return Fail(R,TEXT("niagara_scratch_default_invalid"),TEXT("String default exceeds finite bounds."));}return true;}
    FNiagaraVariable Variable(T,NAME_None);double N=0;bool B=false;
    if(T==FNiagaraTypeDefinition::GetFloatDef())
    {if(!V->TryGetNumber(N)||!FMath::IsFinite(N)||!FMath::IsFinite(static_cast<float>(N))){return Fail(R,TEXT("niagara_scratch_value_invalid"),TEXT("Float requires finite float-representable JSON number."));}Variable.SetValue<float>(static_cast<float>(N));}
    else if(T==FNiagaraTypeDefinition::GetIntDef())
    {if(!V->TryGetNumber(N)||!FMath::IsFinite(N)||N!=FMath::TruncToDouble(N)||N<MIN_int32||N>MAX_int32){return Fail(R,TEXT("niagara_scratch_value_invalid"),TEXT("Integer requires exact int32; legacy fractional truncation is not reproduced."));}Variable.SetValue<int32>(static_cast<int32>(N));}
    else if(T==FNiagaraTypeDefinition::GetBoolDef())
    {if(V->Type!=EJson::Boolean||!V->TryGetBool(B)){return Fail(R,TEXT("niagara_scratch_value_invalid"),TEXT("Bool requires a JSON boolean."));}Variable.SetValue<FNiagaraBool>(FNiagaraBool(B));}
    else
    {
        const TArray<TSharedPtr<FJsonValue>>* A=nullptr;const int32 Count=T==FNiagaraTypeDefinition::GetVec2Def()?2:((T==FNiagaraTypeDefinition::GetVec4Def()||T==FNiagaraTypeDefinition::GetColorDef())?4:3);
        if(!V->TryGetArray(A)||A->Num()!=Count){return Fail(R,TEXT("niagara_scratch_value_invalid"),TEXT("Vector/Position/Color JSON array must match its exact finite native arity."));}
        float F[4]={0,0,0,1};for(int32 I=0;I<Count;++I){if(!(*A)[I]->TryGetNumber(N)||!FMath::IsFinite(N)||!FMath::IsFinite(static_cast<float>(N))){return Fail(R,TEXT("niagara_scratch_value_invalid"),TEXT("Vector components require finite float-representable numbers."));}F[I]=static_cast<float>(N);}
        if(T==FNiagaraTypeDefinition::GetVec2Def()){Variable.SetValue<FVector2f>(FVector2f(F[0],F[1]));}
        else if(T==FNiagaraTypeDefinition::GetVec3Def()){Variable.SetValue<FVector3f>(FVector3f(F[0],F[1],F[2]));}
        else if(T==FNiagaraTypeDefinition::GetPositionDef()){Variable.SetValue<FNiagaraPosition>(FNiagaraPosition(F[0],F[1],F[2]));}
        else if(T==FNiagaraTypeDefinition::GetColorDef()){Variable.SetValue<FLinearColor>(FLinearColor(F[0],F[1],F[2],F[3]));}
        else{Variable.SetValue<FVector4f>(FVector4f(F[0],F[1],F[2],F[3]));}
    }
    if(!Schema->TryGetPinDefaultValueFromNiagaraVariable(Variable,Out)){return Fail(R,TEXT("niagara_scratch_default_conversion_failed"),TEXT("Public Niagara schema could not export the typed native default."));}return true;
}
void BeforeGraph(FResult& R,UNiagaraGraph* Graph,const FSnapshot& Before)
{
    R.Data->SetObjectField(TEXT("before"),Before.Json);R.Data->SetObjectField(TEXT("graph_before"),Before.Json);
    bool Complete=false;R.Data->SetObjectField(TEXT("graph_parameters_before"),GraphParameters(Graph,Complete));
}
void RejectAfterBaseline(FResult& R,FContext& C)
{
    if(C.Graph){const auto After=Capture(C.Graph);R.Data->SetObjectField(TEXT("after"),After.Json);R.Data->SetObjectField(TEXT("graph_after"),After.Json);if(!After.bComplete){R.bChangeStateKnown=false;}
        bool ParamsComplete=false;R.Data->SetObjectField(TEXT("graph_parameters_after"),GraphParameters(C.Graph,ParamsComplete));
        if(!ParamsComplete){R.bChangeStateKnown=false;Fail(R,TEXT("niagara_scratch_parameters_unobservable"),TEXT("Actual read-after metadata is incomplete; no mutation retry."));}}
    Preserve(R,C,nullptr);R.Data->SetBoolField(TEXT("package_dirty_after"),C.System->GetOutermost()->IsDirty());Complete(R);
}
bool SafeCloneSource(FResult& R,UNiagaraScript* Source,UNiagaraScriptSource*& ScriptSource,UNiagaraGraph*& Graph)
{
    if(!Loaded(Source)||Source->GetClass()!=UNiagaraScript::StaticClass()||(Source->GetUsage()!=ENiagaraScriptUsage::Module&&Source->GetUsage()!=ENiagaraScriptUsage::DynamicInput))
    {return Fail(R,TEXT("niagara_scratch_clone_source_unsupported"),TEXT("Source duplication requires an ordinary native Module or DynamicInput."));}
    UNiagaraScriptSourceBase* Returned=Source->GetLatestSource();TStrongObjectPtr<UNiagaraScriptSourceBase> Immediate(Returned);
    if(!HNAdmit(TEXT("after_GetLatestSource_clone")))return false;ScriptSource=Cast<UNiagaraScriptSource>(Returned);
    if(!Loaded(ScriptSource)||(HNWriter&&!HNWriter->Track(ScriptSource))||!Ordinary(ScriptSource,Graph))return Fail(R,TEXT("niagara_scratch_clone_source_unsupported"),TEXT("Ordinary actual exposed source graph required; actual return retained."));
    const FString Package=Source->GetOutermost()->GetName();
    if(!Package.StartsWith(TEXT("/Game/"))&&!Package.StartsWith(TEXT("/Niagara/"))&&!Package.StartsWith(TEXT("/Engine/")))
    {return Fail(R,TEXT("niagara_scratch_source_dependency_forbidden"),TEXT("Source packages must be Engine/Niagara or /Game, never Cubeless/MCP plugin content."));}
    // Original duplication preserves actual external function references; never localize them implicitly.
    for(UEdGraphNode* N:Graph->Nodes)if(const auto* Call=Cast<UNiagaraNodeFunctionCall>(N);Call&&Call->FunctionScript){if(!Loaded(Call->FunctionScript)||!HNWriter||!HNWriter->Track(Call->FunctionScript))return Fail(R,TEXT("niagara_scratch_function_reference_unavailable"),TEXT("Actual function reference must remain live; no dependency replacement or retry."));}
    return true;
}
// Finite Custom HLSL attachment. All map-get pairing/metadata work is performed
// by the installed Niagara schema's normal DynamicAddPin callbacks, not by
// writing the protected PinOutputToPinDefaultPersistentId storage.
bool HlslIdentifier(const FString& S)
{
    if(S.IsEmpty()||S.Len()>128||!(FChar::IsAlpha(S[0])||S[0]=='_')){return false;}
    for(TCHAR C:S){if(C>127||!(FChar::IsAlnum(C)||C=='_')){return false;}}return true;
}
bool HlslExpression(const FString& S)
{
    if(S.IsEmpty()||S.Len()>16384){return false;}for(TCHAR C:S)
    {if((C<32&&C!='\n'&&C!='\r'&&C!='\t')||C==127){return false;}}return true;
}
bool HlslValueType(const FNiagaraTypeDefinition& T)
{
    return T==FNiagaraTypeDefinition::GetFloatDef()||T==FNiagaraTypeDefinition::GetIntDef()||
        T==FNiagaraTypeDefinition::GetBoolDef()||T==FNiagaraTypeDefinition::GetVec2Def()||
        T==FNiagaraTypeDefinition::GetVec3Def()||T==FNiagaraTypeDefinition::GetVec4Def()||
        T==FNiagaraTypeDefinition::GetColorDef()||T==FNiagaraTypeDefinition::GetPositionDef();
}
bool HlslRows(FResult& R,const FString& Json,TArray<TSharedPtr<FJsonValue>>& Out)
{
    TArray<TSharedPtr<FJsonValue>> Envelope;
    if(Json.Len()>16384||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+Json+TEXT("]")),Envelope)||
        Envelope.Num()!=1||!Envelope[0].IsValid()){return Fail(R,TEXT("niagara_hlsl_inputs_invalid"),TEXT("Input lists must be one bounded JSON array or null."));}
    if(Envelope[0]->Type==EJson::Null){return true;}
    const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;
    if(!Envelope[0]->TryGetArray(Rows)||Rows->Num()>32){return Fail(R,TEXT("niagara_hlsl_inputs_invalid"),TEXT("At most32 typed inputs are supported by this attachment."));}
    Out=*Rows;for(const auto& V:Out){if(!V.IsValid()||V->Type!=EJson::Object){return Fail(R,TEXT("niagara_hlsl_inputs_invalid"),TEXT("Each typed input must be an object."));}}return true;
}
UEdGraphPin* NamedPin(UEdGraphNode* N,EEdGraphPinDirection D,const FString& Name)
{
    UEdGraphPin* Out=nullptr;for(UEdGraphPin* P:N->Pins)
    {if(P&&P->Direction==D&&!P->bOrphanedPin&&P->PinName.ToString().Equals(Name,ESearchCase::IgnoreCase))
        {if(Out){return nullptr;}Out=P;}}return Out;
}
UEdGraphNode* HlslNode(FResult& R,UNiagaraGraph* G,const FString& Id,const FString& Name,int32 IndexValue)
{
    FGuid Wanted;if(!Id.IsEmpty()&&!FGuid::Parse(Id,Wanted)){Fail(R,TEXT("niagara_hlsl_node_selector_invalid"),TEXT("Node GUID is invalid."));return nullptr;}
    UEdGraphNode* Found=nullptr;
    for(int32 I=0;I<G->Nodes.Num();++I)
    {UEdGraphNode* N=G->Nodes[I].Get();if((IndexValue==INDEX_NONE||I==IndexValue)&&(Id.IsEmpty()||N->NodeGuid==Wanted)&&
        (Name.IsEmpty()||N->GetName().Equals(Name,ESearchCase::IgnoreCase)))
        {if(Found){Fail(R,TEXT("niagara_hlsl_node_selector_ambiguous"),TEXT("Combined node selector must identify exactly one node."));return nullptr;}Found=N;}}
    if(!Found){Fail(R,TEXT("niagara_hlsl_node_not_found"),TEXT("No node matches the supplied combined selector."));}return Found;
}
UEdGraphPin* DynamicOutput(UEdGraphNode* N)
{
    // Literal is pinned to NiagaraNodeWithDynamicPins.cpp:23; the static symbol
    // itself is not exported. Actual category/direction/empty links are checked.
    UEdGraphPin* Out=nullptr;for(UEdGraphPin* P:N->Pins)
    {if(P&&P->Direction==EGPD_Output&&P->PinType.PinCategory==UEdGraphSchema_Niagara::PinCategoryMisc&&
        P->PinType.PinSubCategory==FName(TEXT("DynamicAddPin"))&&!P->bOrphanedPin&&P->LinkedTo.IsEmpty())
        {if(Out){return nullptr;}Out=P;}}return Out;
}
bool HlslConnect(FResult& R,const UEdGraphSchema_Niagara* Schema,UEdGraphPin* Out,UEdGraphPin* In,bool Replace=false)
{
    if(!HNCurrentPin(Out)||!HNCurrentPin(In))return false;
    if(!Out||!In||Out->Direction!=EGPD_Output||In->Direction!=EGPD_Input||Out->GetOwningNode()->GetGraph()!=In->GetOwningNode()->GetGraph())
    {return Fail(R,TEXT("niagara_hlsl_connection_invalid"),TEXT("Connection must have exact same-graph output/input owners."));}
    if(!HNDispatch(TEXT("CanCreateConnection")))return false;const auto Response=Schema->CanCreateConnection(Out,In);
    if(!HNAdmit(TEXT("after_CanCreateConnection"))||!HNCurrentPin(Out)||!HNCurrentPin(In))return false;
    if(Response.Response!=CONNECT_RESPONSE_MAKE&&!(Replace&&Response.Response==CONNECT_RESPONSE_BREAK_OTHERS_B))
    {return Fail(R,TEXT("niagara_hlsl_connection_unsupported"),TEXT("Only direct MAKE or replacement of the selected input is supported; no conversion nodes or extra breaks."));}
    if(!HNDispatch(TEXT("TryCreateConnection"),true)||!UKPlanConnection(Out,In,Replace))return false;const bool Connected=Schema->TryCreateConnection(Out,In);
    if(!HNAdmit(TEXT("after_TryCreateConnection"))||!HNCurrentPin(Out)||!HNCurrentPin(In))return false;
    return Connected&&Out->LinkedTo.Contains(In)&&In->LinkedTo.Contains(Out)
        ?true:Fail(R,TEXT("niagara_hlsl_connection_failed"),TEXT("Native schema did not retain the requested reciprocal edge; actual work remains."));
}
bool HlslMetadataAdmit(FResult& R,UNiagaraGraph* G,const FNiagaraVariable& V)
{
    for(const auto& P:G->GetAllMetaData())
    {if(P.Key.GetName().ToString().Equals(V.GetName().ToString(),ESearchCase::IgnoreCase)&&
        (P.Key!=V||!Loaded(P.Value.Get())||P.Value->Variable!=V||P.Value->DefaultMode==ENiagaraDefaultMode::Custom))
        {return Fail(R,TEXT("niagara_hlsl_parameter_metadata_conflict"),TEXT("Existing module input has another case/type or incomplete native metadata."));}}return true;
}
void HlslFailure(FResult& R,FContext& C,const FSnapshot& Before,const TSharedRef<FJsonObject>& Parameters)
{
    if(!HNAdmit(TEXT("before_HLSL_failure_projection"))){Complete(R);return;}
    const auto Actual=Capture(C.Graph);if(!HNAdmit(TEXT("after_HLSL_failure_projection"))){Complete(R);return;}bool PComplete=false;const auto P=GraphParameters(C.Graph,PComplete);
    R.Data->SetObjectField(TEXT("after"),Actual.Json);R.Data->SetObjectField(TEXT("graph_after"),Actual.Json);R.Data->SetObjectField(TEXT("graph_parameters_after"),P);
    R.bChangeStateKnown=Actual.bComplete&&PComplete;R.bChanged=R.bChangeStateKnown&&(!SameGraph(Before,Actual)||Text(P)!=Text(Parameters));
    Preserve(R,C,R.bMutationAttempted?C.Graph:nullptr);if(!HNAdmit(TEXT("after_HLSL_failure_preservation"))){Complete(R);return;}R.Data->SetBoolField(TEXT("compile_and_save_suppressed_after_failure"),!R.ErrorCode.IsEmpty());
    R.Data->SetBoolField(TEXT("package_dirty_after"),C.System->GetOutermost()->IsDirty());Complete(R);
}
bool HlslPriorGraph(FResult& R,const FSnapshot& Before,const FSnapshot& After,
    TMap<FString,TArray<FString>> ExpectedLinks,const TSet<FString>& ConsumedAddPins)
{
    bool Nodes=After.bComplete,Pins=After.bComplete,Edges=After.bComplete;
    for(const auto& P:Before.Nodes){const auto* V=After.Nodes.Find(P.Key);Nodes&=V&&*V==P.Value;}
    for(const auto& P:Before.Pins)
    {if(ConsumedAddPins.Contains(P.Key)){ExpectedLinks.Remove(P.Key);continue;}const auto* V=After.Pins.Find(P.Key);Pins&=V&&*V==P.Value;}
    for(const auto& P:After.Pins){ExpectedLinks.FindOrAdd(P.Key);}Edges&=ExpectedLinks.Num()==After.Links.Num();
    for(auto& P:ExpectedLinks){P.Value.Sort();const auto* V=After.Links.Find(P.Key);Edges&=V&&*V==P.Value;}
    R.Data->SetBoolField(TEXT("prior_node_metadata_preserved"),Nodes);R.Data->SetBoolField(TEXT("prior_pin_metadata_preserved"),Pins);
    R.Data->SetBoolField(TEXT("exact_native_splice_verified"),Edges);
    return Nodes&&Pins&&Edges?true:Fail(R,TEXT("niagara_hlsl_graph_postcondition_failed"),TEXT("Actual graph differs beyond the selected native attachment; no retry or rollback."));
}
bool HlslMetadataBounded(FResult& R,UNiagaraGraph* G,const TSharedRef<FJsonObject>& Before,const TSet<FString>& NewNames)
{
    bool Complete=false;const auto After=GraphParameters(G,Complete);bool Same=Complete;
    for(const auto& P:Before->Values){const auto* V=After->Values.Find(P.Key);Same&=V&&Text(P.Value->AsObject().ToSharedRef())==Text((*V)->AsObject().ToSharedRef());}
    for(const auto& P:After->Values){if(!Before->HasField(FString(P.Key))){Same&=NewNames.Contains(FString(P.Key));}}
    R.Data->SetObjectField(TEXT("graph_parameters_after"),After);R.Data->SetBoolField(TEXT("parameter_metadata_changes_bounded"),Same);
    if(!Complete){R.bChangeStateKnown=false;}return Same?true:Fail(R,TEXT("niagara_hlsl_metadata_postcondition_failed"),TEXT("Existing typed metadata changed or an unexpected declaration remained; actual changes retained."));
}

// SetObjectProperties may reconstruct the fresh Custom pins. Compare plain
// values by PinId after the callback, without dereferencing any former pin.
bool HNSetFreshHlslText(FResult& R,UNiagaraNodeCustomHlsl* Custom,const FString& Code,bool& SetReturned,TArray<FName>& WrittenNames)
{
    if(!HNWriter)return false;
    if(!HNAdmit(TEXT("before_fresh_HLSL_text"))||!HNWriter->Track(Custom))return false;
    TMap<FGuid,FString> BeforePins;const int32 NodeCount=Custom->GetGraph()->Nodes.Num();
    for(auto* P:Custom->Pins){if(!HNCurrentPin(P)||!P->PinId.IsValid()||BeforePins.Contains(P->PinId)||!P->LinkedTo.IsEmpty())return HNWriter->Stop(TEXT("niagara_hlsl_fresh_reconstruction_invalid"),TEXT("Fresh Custom pins must be unique and completely unlinked."));auto V=PinMetadata(P);V->SetStringField(TEXT("persistent_guid"),Guid(P->PersistentGuid));BeforePins.Add(P->PinId,Text(V));}
    if(!HNDispatch(TEXT("SetObjectProperties_CustomHlsl"),true))return false;
    auto Values=MakeShared<FJsonObject>();Values->SetStringField(TEXT("CustomHlsl"),Code);
    SetReturned=UToolsetLibrary::SetObjectProperties(Custom,Text(Values),WrittenNames,EBypassContainerCheck::No);
    R.Data->SetBoolField(TEXT("custom_hlsl_text_set_returned"),SetReturned);
    if(!HNAdmit(TEXT("after_fresh_HLSL_text"))||Custom->GetGraph()->Nodes.Num()!=NodeCount||Custom->Pins.Num()!=BeforePins.Num())return HNWriter->Stop(TEXT("niagara_hlsl_fresh_reconstruction_changed"),TEXT("Fresh HLSL pin inventory or unrelated node count changed during setter."));
    TSet<FGuid> Seen;
    for(auto* P:Custom->Pins){if(!HNCurrentPin(P)||!P->PinId.IsValid()||Seen.Contains(P->PinId)||!P->LinkedTo.IsEmpty())return HNWriter->Stop(TEXT("niagara_hlsl_fresh_reconstruction_changed"),TEXT("Reconstructed current pins must be unique, live and unlinked."));const auto* Prior=BeforePins.Find(P->PinId);auto V=PinMetadata(P);V->SetStringField(TEXT("persistent_guid"),Guid(P->PersistentGuid));if(!Prior||*Prior!=Text(V))return HNWriter->Stop(TEXT("niagara_hlsl_fresh_reconstruction_changed"),TEXT("Reconstructed pin GUID/name/type/default/flags differs from exact captured value."));Seen.Add(P->PinId);}
    if(!HNWriter->TrackPins(Custom))return false;R.Data->SetBoolField(TEXT("fresh_unlinked_HLSL_pin_reconstruction_verified"),true);return true;
}

// Existing-wrapper edit plan. Plain checks use copied values and actual current
// membership; no exporters, FText observers or disk snapshots in scratch loops.
struct FUKHlslEdit
{
    struct FPin
    {
        UEdGraphPin* Address=nullptr;FGuid Id,Persistent;FName Name;EEdGraphPinDirection Direction=EGPD_Input;
        FEdGraphPinType Type;FString Value,Auto,Key;UObject* Default=nullptr;const void* TextId=nullptr;
        bool Hidden=false,Orphan=false,NoConnect=false,IgnoreDefault=false,ReadOnly=false;TArray<UEdGraphPin*> Links;
    };
    struct FNode
    {
        UEdGraphNode* Object=nullptr;FString Key,Comment;int32 X=0,Y=0;FGuid Id;TArray<FPin> Pins;
        UNiagaraScript* Function=nullptr;FNiagaraFunctionSignature Signature;FGuid FunctionVersion;FName FunctionAsset;FString FunctionName;TMap<FName,FName> Specifiers;bool Custom=false,InputRole=false,OutputRole=false;
        FNiagaraVariableBase InputVariable;int32 InputUsage=0,DesiredEnabled=0;bool UserEnabled=false;ENiagaraScriptUsage OutputUsage=ENiagaraScriptUsage::Function;FGuid OutputUsageId;
        ENiagaraScriptUsage Usage=ENiagaraScriptUsage::Function;FString Hlsl;
    };
    FHNWriterGuard& Guard;UNiagaraGraph* Graph;FString GraphPath;TArray<UEdGraphNode*> ExpectedNodes;
    TMap<UEdGraphNode*,FNode> Rows;TSet<UEdGraphNode*> Retired,Added;TSet<FString> ChangedNodeKeys,RemovedPinKeys;
    TSet<UEdGraphPin*> ReconstructingPins;TMap<FString,FString> PinKeyMoves;FStrProperty* TextField=nullptr;uint64 TextFlags=0;int32 TextOffset=0;bool Ready=false;
    FUKHlslEdit(FHNWriterGuard& InGuard,UNiagaraGraph* InGraph):Guard(InGuard),Graph(InGraph){}
    ~FUKHlslEdit(){Guard.ExistingHlslCurrent=nullptr;}
    bool Bad(const TCHAR* Property)
    {
        if(!Guard.R.Data->HasField(TEXT("existing_hlsl_first_mismatch"))){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("stage"),Guard.R.Stage);J->SetStringField(TEXT("graph"),GraphPath);Guard.R.Data->SetObjectField(TEXT("existing_hlsl_first_mismatch"),J);}
        return Guard.Stop(TEXT("niagara_hlsl_existing_current_changed"),TEXT("Existing wrapper/source/peer state differs from the declared edit; actual prefix retained, no former pin access or retry."));
    }
    bool TextCurrent()const
    {return TextField&&FindFProperty<FProperty>(UNiagaraNodeCustomHlsl::StaticClass(),TEXT("CustomHlsl"))==TextField&&TextField->GetClass()==FStrProperty::StaticClass()&&TextField->GetOwnerStruct()==UNiagaraNodeCustomHlsl::StaticClass()&&TextField->ArrayDim==1&&TextField->GetSize()==sizeof(FString)&&!TextField->HasSetterOrGetter()&&uint64(TextField->GetPropertyFlags())==TextFlags&&TextField->GetOffset_ForInternal()==TextOffset;}
    bool ReadPin(UEdGraphNode* Owner,UEdGraphPin* CurrentPin,FPin& OutPin)
    {
        if(!CurrentPin||CurrentPin->IsPendingKill()||CurrentPin->GetOwningNodeUnchecked()!=Owner||!CurrentPin->PinId.IsValid()||!CurrentPin->DefaultObject.IsResolved()||!(CurrentPin->PinType.PinSubCategoryObject.IsExplicitlyNull()||CurrentPin->PinType.PinSubCategoryObject.IsValid())||!(CurrentPin->PinType.PinValueType.TerminalSubCategoryObject.IsExplicitlyNull()||CurrentPin->PinType.PinValueType.TerminalSubCategoryObject.IsValid()))return Bad(TEXT("pin_role"));
        OutPin.Address=CurrentPin;OutPin.Id=CurrentPin->PinId;OutPin.Persistent=CurrentPin->PersistentGuid;OutPin.Name=CurrentPin->PinName;OutPin.Direction=CurrentPin->Direction;OutPin.Type=CurrentPin->PinType;
        OutPin.Value=CurrentPin->DefaultValue;OutPin.Auto=CurrentPin->AutogeneratedDefaultValue;OutPin.Default=CurrentPin->DefaultObject.Get();OutPin.TextId=FTextInspector::GetSharedDataId(CurrentPin->DefaultTextValue);
        OutPin.Hidden=CurrentPin->bHidden;OutPin.Orphan=CurrentPin->bOrphanedPin;OutPin.NoConnect=CurrentPin->bNotConnectable;OutPin.IgnoreDefault=CurrentPin->bDefaultValueIsIgnored;OutPin.ReadOnly=CurrentPin->bDefaultValueIsReadOnly;OutPin.Links=CurrentPin->LinkedTo;OutPin.Key=PinId(CurrentPin);
        for(UObject* Ref:TArray<UObject*>{OutPin.Default,OutPin.Type.PinSubCategoryObject.Get(),OutPin.Type.PinValueType.TerminalSubCategoryObject.Get()})if(Ref&&!Guard.Track(Ref,true))return false;
        return true;
    }
    static bool SameLinks(const TArray<UEdGraphPin*>& A,const TArray<UEdGraphPin*>& B)
    {if(A.Num()!=B.Num())return false;TSet<UEdGraphPin*> Seen;for(auto* Item:A){if(!Item||Seen.Contains(Item)||!B.Contains(Item))return false;Seen.Add(Item);}return true;}
    static bool SameAuthored(const FPin& A,const FPin& B)
    {return A.Name==B.Name&&A.Direction==B.Direction&&A.Type==B.Type&&A.Value==B.Value&&A.Auto==B.Auto&&A.Default==B.Default&&A.TextId==B.TextId&&A.Hidden==B.Hidden&&A.Orphan==B.Orphan&&A.NoConnect==B.NoConnect&&A.IgnoreDefault==B.IgnoreDefault&&A.ReadOnly==B.ReadOnly;}
    bool Bind()
    {
        if(!Guard.Admit(TEXT("existing_HLSL_plan_admission")))return false;
        auto* Found=FindFProperty<FProperty>(UNiagaraNodeCustomHlsl::StaticClass(),TEXT("CustomHlsl"));
        if(!Found||Found->GetClass()!=FStrProperty::StaticClass()||Found->GetOwnerStruct()!=UNiagaraNodeCustomHlsl::StaticClass())return Bad(TEXT("text_descriptor"));
        TextField=static_cast<FStrProperty*>(Found);TextFlags=uint64(Found->GetPropertyFlags());TextOffset=Found->GetOffset_ForInternal();
        // Same declared editor-readable property as HlslText; permission is checked
        // before its exact native value is copied. No private C++ member access.
        if(!TextCurrent()||EnumHasAnyFlags(PropertyAccessUtil::CanGetPropertyValue(Found),EPropertyAccessResultFlags::PermissionDenied))return Bad(TEXT("text_access"));
        GraphPath=Graph->GetPathName();for(const auto& Entry:Graph->Nodes)ExpectedNodes.Add(Entry.Get());
        for(auto* InitialNode:ExpectedNodes)
        {
            if(InitialNode->GetClass()!=UNiagaraNodeInput::StaticClass()&&InitialNode->GetClass()!=UNiagaraNodeOutput::StaticClass()&&InitialNode->GetClass()!=UNiagaraNodeCustomHlsl::StaticClass()&&InitialNode->GetClass()!=UNiagaraNodeFunctionCall::StaticClass()&&!ExactNodeClass(InitialNode,TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"))&&!ExactNodeClass(InitialNode,TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapSet")))return Bad(TEXT("existing_native_node_profile_pending"));
            FNode Row;Row.Object=InitialNode;Row.Key=Path(InitialNode);Row.Id=InitialNode->NodeGuid;Row.X=InitialNode->NodePosX;Row.Y=InitialNode->NodePosY;Row.Comment=InitialNode->NodeComment;Row.DesiredEnabled=static_cast<int32>(InitialNode->GetDesiredEnabledState());Row.UserEnabled=InitialNode->HasUserSetTheEnabledState();
            if(auto* SourceInput=Cast<UNiagaraNodeInput>(InitialNode)){Row.InputRole=true;Row.InputVariable=SourceInput->Input;Row.InputUsage=static_cast<int32>(SourceInput->Usage);if(!HNKeepType(SourceInput->Input.GetType()))return false;}
            if(auto* SourceOutput=Cast<UNiagaraNodeOutput>(InitialNode)){Row.OutputRole=true;Row.OutputUsage=SourceOutput->GetUsage();Row.OutputUsageId=SourceOutput->GetUsageId();}
            if(auto* Call=Cast<UNiagaraNodeFunctionCall>(InitialNode)){if(!Call->FunctionScript.IsResolved())return Bad(TEXT("function_slot"));Row.Function=Call->FunctionScript.Get();Row.Signature=Call->Signature;Row.FunctionVersion=Call->SelectedScriptVersion;Row.FunctionAsset=Call->FunctionScriptAssetObjectPath;Row.FunctionName=Call->GetFunctionName();Row.Specifiers=Call->FunctionSpecifiers;}
            if(InitialNode->GetClass()==UNiagaraNodeCustomHlsl::StaticClass()){auto* Custom=static_cast<UNiagaraNodeCustomHlsl*>(InitialNode);Row.Custom=true;Row.Usage=Custom->ScriptUsage;Row.Hlsl=TextField->GetPropertyValue_InContainer(Custom);}
            for(auto* InitialPin:InitialNode->Pins){FPin PinRow;if(!ReadPin(InitialNode,InitialPin,PinRow))return false;Row.Pins.Add(MoveTemp(PinRow));}
            Rows.Add(InitialNode,MoveTemp(Row));
        }
        Ready=true;Guard.ExistingHlslCurrent=[this](){return Current();};return Guard.Admit(TEXT("existing_HLSL_plan_bound"));
    }
    bool Current()
    {
        if(!Ready)return true;if(!TextCurrent()||Graph->Nodes.Num()!=ExpectedNodes.Num())return Bad(TEXT("graph_or_descriptor_inventory"));
        for(int32 IndexValue=0;IndexValue<ExpectedNodes.Num();++IndexValue)if(Graph->Nodes[IndexValue].Get()!=ExpectedNodes[IndexValue])return Bad(TEXT("graph_node_order"));
        for(const auto& Pair:Rows)
        {
            if(Retired.Contains(Pair.Key))continue;const auto& Row=Pair.Value;auto* LiveNode=Row.Object;
            if(LiveNode->NodeGuid!=Row.Id||LiveNode->NodePosX!=Row.X||LiveNode->NodePosY!=Row.Y||LiveNode->NodeComment!=Row.Comment||static_cast<int32>(LiveNode->GetDesiredEnabledState())!=Row.DesiredEnabled||LiveNode->HasUserSetTheEnabledState()!=Row.UserEnabled)return Bad(TEXT("authored_node"));
            if(Row.InputRole){auto* SourceInput=static_cast<UNiagaraNodeInput*>(LiveNode);if(SourceInput->Input!=Row.InputVariable||static_cast<int32>(SourceInput->Usage)!=Row.InputUsage)return Bad(TEXT("input_source_identity"));}
            if(Row.OutputRole){auto* SourceOutput=static_cast<UNiagaraNodeOutput*>(LiveNode);if(SourceOutput->GetUsage()!=Row.OutputUsage||SourceOutput->GetUsageId()!=Row.OutputUsageId)return Bad(TEXT("output_usage_identity"));}
            if(auto* Call=Cast<UNiagaraNodeFunctionCall>(LiveNode)){if(!Call->FunctionScript.IsResolved()||Call->FunctionScript.Get()!=Row.Function||!(Call->Signature==Row.Signature)||Call->SelectedScriptVersion!=Row.FunctionVersion||Call->FunctionScriptAssetObjectPath!=Row.FunctionAsset||Call->GetFunctionName()!=Row.FunctionName||!Call->FunctionSpecifiers.OrderIndependentCompareEqual(Row.Specifiers))return Bad(TEXT("function_signature"));}
            if(Row.Custom){auto* Custom=static_cast<UNiagaraNodeCustomHlsl*>(LiveNode);if(Custom->ScriptUsage!=Row.Usage||TextField->GetPropertyValue_InContainer(Custom)!=Row.Hlsl)return Bad(TEXT("custom_usage_or_text"));}
            if(Guard.HlslReconstructing.Contains(LiveNode))continue;
            if(LiveNode->Pins.Num()!=Row.Pins.Num())return Bad(TEXT("pin_inventory"));
            for(int32 PinIndex=0;PinIndex<Row.Pins.Num();++PinIndex)
            {
                const auto& Prior=Row.Pins[PinIndex];auto* Fresh=LiveNode->Pins[PinIndex];
                if(Fresh!=Prior.Address)return Bad(TEXT("physical_pin_changed")); // no former pin dereference
                FPin Actual;if(!ReadPin(LiveNode,Fresh,Actual)||Actual.Id!=Prior.Id||Actual.Persistent!=Prior.Persistent||!SameAuthored(Actual,Prior))return Bad(TEXT("authored_pin"));
                bool PendingPeer=false;for(auto* Linked:Prior.Links)PendingPeer|=ReconstructingPins.Contains(Linked);
                if(!PendingPeer&&!SameLinks(Actual.Links,Prior.Links))return Bad(TEXT("pin_links"));
            }
        }
        return true;
    }
    bool EnrollPrepared(UEdGraphNode* Prepared)
    {
        if(!Guard.Admit(TEXT("before_prepared_HLSL_enrollment"))||!Added.Contains(Prepared)||Rows.Contains(Prepared))return Bad(TEXT("prepared_node_role"));
        FNode Row;Row.Object=Prepared;Row.Key=Path(Prepared);Row.Id=Prepared->NodeGuid;Row.X=Prepared->NodePosX;Row.Y=Prepared->NodePosY;Row.Comment=Prepared->NodeComment;Row.DesiredEnabled=static_cast<int32>(Prepared->GetDesiredEnabledState());Row.UserEnabled=Prepared->HasUserSetTheEnabledState();
            if(auto* SourceInput=Cast<UNiagaraNodeInput>(Prepared)){Row.InputRole=true;Row.InputVariable=SourceInput->Input;Row.InputUsage=static_cast<int32>(SourceInput->Usage);if(!HNKeepType(SourceInput->Input.GetType()))return false;}
            if(auto* SourceOutput=Cast<UNiagaraNodeOutput>(Prepared)){Row.OutputRole=true;Row.OutputUsage=SourceOutput->GetUsage();Row.OutputUsageId=SourceOutput->GetUsageId();}
        if(auto* Call=Cast<UNiagaraNodeFunctionCall>(Prepared)){if(!Call->FunctionScript.IsResolved())return Bad(TEXT("prepared_function_slot"));Row.Function=Call->FunctionScript.Get();Row.Signature=Call->Signature;Row.FunctionVersion=Call->SelectedScriptVersion;Row.FunctionAsset=Call->FunctionScriptAssetObjectPath;Row.FunctionName=Call->GetFunctionName();Row.Specifiers=Call->FunctionSpecifiers;}
        if(Prepared->GetClass()==UNiagaraNodeCustomHlsl::StaticClass()){auto* Custom=static_cast<UNiagaraNodeCustomHlsl*>(Prepared);Row.Custom=true;Row.Usage=Custom->ScriptUsage;Row.Hlsl=TextField->GetPropertyValue_InContainer(Custom);}
        for(auto* PreparedPin:Prepared->Pins){FPin PinRow;if(!ReadPin(Prepared,PreparedPin,PinRow))return false;Row.Pins.Add(MoveTemp(PinRow));}
        Rows.Add(Prepared,MoveTemp(Row));return Guard.TrackPins(Prepared)&&Guard.Admit(TEXT("after_prepared_HLSL_enrollment"));
    }
    void ExpectAdd(UEdGraphNode* Node){ExpectedNodes.Add(Node);Added.Add(Node);}
    void PlanConnection(UEdGraphPin* Output,UEdGraphPin* Input,bool Replace)
    {
        if(Replace)for(auto& Pair:Rows)if(!Retired.Contains(Pair.Key))for(auto& PinRow:Pair.Value.Pins){if(PinRow.Address==Input)PinRow.Links.Empty();else PinRow.Links.Remove(Input);}
        for(auto& Pair:Rows)if(!Retired.Contains(Pair.Key))for(auto& PinRow:Pair.Value.Pins){if(PinRow.Address==Output)PinRow.Links.AddUnique(Input);if(PinRow.Address==Input)PinRow.Links.AddUnique(Output);}
    }
    bool Retire(UEdGraphNode* Node,const TCHAR* Counter)
    {
        if(!HNDispatch(TEXT("Remove_old_HLSL_node"),true))return false;auto* Row=Rows.Find(Node);if(!Row||Retired.Contains(Node))return Bad(TEXT("retirement_role"));
        auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("node_path"),Row->Key);Receipt->SetStringField(TEXT("node_guid"),Guid(Row->Id));Receipt->SetStringField(TEXT("stage"),TEXT("before_RemoveNode"));
        Guard.R.Data->SetObjectField(TEXT("HLSL_removal_prefix"),Receipt);Receipt->SetBoolField(TEXT("dispatch_attempted"),true);
        for(const auto& OldPin:Row->Pins){RemovedPinKeys.Add(OldPin.Key);for(auto& Pair:Rows)if(!Retired.Contains(Pair.Key))for(auto& PinRow:Pair.Value.Pins)PinRow.Links.Remove(OldPin.Address);}
        ChangedNodeKeys.Add(Row->Key);Retired.Add(Node);ExpectedNodes.Remove(Node);Guard.HlslRetiredObjects.Add(Node);
        const bool Removed=Graph->RemoveNode(Node,true,false);Receipt->SetBoolField(TEXT("RemoveNode_returned"),Removed);if(Removed)Guard.R.Data->SetNumberField(Counter,Guard.R.Data->GetNumberField(Counter)+1);Receipt->SetStringField(TEXT("stage"),TEXT("returned_RemoveNode"));
        if(!Guard.Admit(TEXT("after_Remove_old_HLSL_node")))return false;return Removed||Bad(TEXT("RemoveNode_returned_false"));
    }
    void OpenReconstruction(UNiagaraNodeCustomHlsl* Custom)
    {Guard.HlslReconstructing.Add(Custom);for(const auto& PinRow:Rows.FindChecked(Custom).Pins)ReconstructingPins.Add(PinRow.Address);}
    bool Rebind(UNiagaraNodeCustomHlsl* Custom)
    {
        if(!Guard.Admit(TEXT("before_existing_pin_rebind")))return false;auto* Row=Rows.Find(Custom);if(!Row||Custom->Pins.Num()!=Row->Pins.Num())return Bad(TEXT("reconstruction_inventory"));
        TArray<FPin> FreshPins;TMap<UEdGraphPin*,UEdGraphPin*> Moves;TSet<FName> SeenInput,SeenOutput;
        for(auto* Fresh:Custom->Pins)
        {
            FPin Actual;if(!ReadPin(Custom,Fresh,Actual))return false;auto& Seen=Actual.Direction==EGPD_Input?SeenInput:SeenOutput;if(Seen.Contains(Actual.Name))return Bad(TEXT("reconstruction_duplicate_name"));Seen.Add(Actual.Name);
            const auto* Prior=Row->Pins.FindByPredicate([&](const FPin& Entry){return Entry.Name==Actual.Name&&Entry.Direction==Actual.Direction;});
            if(!Prior||!SameAuthored(Actual,*Prior))return Bad(TEXT("reconstruction_authored_value"));
            Moves.Add(Prior->Address,Actual.Address);for(auto& KeyMove:PinKeyMoves)if(KeyMove.Value==Prior->Key)KeyMove.Value=Actual.Key;PinKeyMoves.Add(Prior->Key,Actual.Key);FreshPins.Add(MoveTemp(Actual));
        }
        for(auto& Pair:Rows)if(!Retired.Contains(Pair.Key))for(auto& PinRow:Pair.Value.Pins)for(auto*& Linked:PinRow.Links)if(auto* const* Replacement=Moves.Find(Linked))Linked=*Replacement;
        for(auto& Fresh:FreshPins){const auto* Prior=Row->Pins.FindByPredicate([&](const FPin& Entry){return Entry.Name==Fresh.Name&&Entry.Direction==Fresh.Direction;});if(!Prior||!SameLinks(Fresh.Links,Prior->Links))return Bad(TEXT("reconstruction_peer_links"));}
        Row->Pins=MoveTemp(FreshPins);ReconstructingPins.Empty();Guard.FixedPins.RemoveAll([&](const FHNWriterGuard::FNodePins& Entry){return Entry.Node.Get()==Custom;});Guard.HlslReconstructing.Remove(Custom);
        return Guard.TrackPins(Custom)&&Guard.Admit(TEXT("after_existing_pin_rebind"));
    }
    FSnapshot BeforeForProof(const FSnapshot& Before)const
    {
        FSnapshot Filtered=Before;for(const auto& Key:ChangedNodeKeys)Filtered.Nodes.Remove(Key);
        for(const auto& Pair:Rows)if(ChangedNodeKeys.Contains(Pair.Value.Key))for(const auto& PinRow:Pair.Value.Pins)Filtered.Pins.Remove(PinRow.Key);
        for(const auto& Pair:PinKeyMoves)Filtered.Pins.Remove(Pair.Key);for(const auto& Key:RemovedPinKeys)Filtered.Pins.Remove(Key);return Filtered;
    }
    void AdjustLinks(TMap<FString,TArray<FString>>& Planned)const
    {
        TMap<FString,TArray<FString>> Rebased;for(const auto& Pair:Planned){if(RemovedPinKeys.Contains(Pair.Key))continue;const FString* MoveKey=PinKeyMoves.Find(Pair.Key);auto& Dest=Rebased.FindOrAdd(MoveKey?*MoveKey:Pair.Key);for(const auto& Linked:Pair.Value){if(RemovedPinKeys.Contains(Linked))continue;const FString* Moved=PinKeyMoves.Find(Linked);Dest.AddUnique(Moved?*Moved:Linked);}}Planned=MoveTemp(Rebased);
    }
};
bool UKPlanConnection(UEdGraphPin* Out,UEdGraphPin* In,bool Replace)
{if(ExistingHlslEdit)ExistingHlslEdit->PlanConnection(Out,In,Replace);return true;}

struct FHlslInput
{
    FString Name,ParameterName;FNiagaraTypeDefinition Type;UEdGraphPin* Source=nullptr;
};
FORCENOINLINE bool CommitModulePin(UNiagaraNode& N,UEdGraphPin* Pin,const FString& Name)
{return N.CommitEditablePinName(FText::AsCultureInvariant(Name),Pin,false);}
// Connect the real DynamicAdd output to the typed input, allowing Niagara to
// create its replacement Add pin, typed metadata and protected default pair.
bool AttachModuleRead(FResult& R,FContext& C,const UEdGraphSchema_Niagara* Schema,UNiagaraNode* Get,
    UEdGraphPin* Input,const FNiagaraVariable& Variable,TMap<FString,TArray<FString>>& Links,TSet<FString>& Consumed,
    TArray<TSharedPtr<FJsonValue>>& PairRows)
{
    UEdGraphPin* Add=DynamicOutput(Get);if(!Add){return Fail(R,TEXT("niagara_hlsl_mapget_add_pin_missing"),TEXT("Exact native map-get has no unique unused DynamicAdd output."));}
    const FString OldAdd=PinId(Add);const FGuid OldGuid=Add->PinId;TSet<UEdGraphPin*> OldPins;for(auto* P:Get->Pins){OldPins.Add(P);}
    if(!HlslConnect(R,Schema,Add,Input,!Input->LinkedTo.IsEmpty())){return false;}
    if(!HNCurrentPin(Add)||!HNCurrentPin(Input))return false;
    if(Add->PinId!=OldGuid||UEdGraphSchema_Niagara::PinToTypeDefinition(Add)!=Variable.GetType()||!Add->PersistentGuid.IsValid())
    {return Fail(R,TEXT("niagara_hlsl_mapget_specialization_failed"),TEXT("Native callback did not preserve output identity and exact typed specialization."));}
    if(Add->PinName!=Variable.GetName())
    {if(!HNDispatch(TEXT("CommitEditablePinName"),true))return false;const bool Renamed=CommitModulePin(*Get,Add,Variable.GetName().ToString());if(!HNAdmit(TEXT("after_CommitEditablePinName"))||!HNCurrentPin(Add))return false;
        if(!Renamed||Add->PinName!=Variable.GetName())
        {return Fail(R,TEXT("niagara_hlsl_module_pin_rename_failed"),TEXT("Public name-commit callback did not retain exact Module input name."));}}
    UEdGraphPin* DefaultPin=nullptr;for(auto* P:Get->Pins)
    {if(!OldPins.Contains(P)&&P->Direction==EGPD_Input&&UEdGraphSchema_Niagara::PinToTypeDefinition(P)==Variable.GetType())
        {if(DefaultPin){return Fail(R,TEXT("niagara_hlsl_default_pair_unobservable"),TEXT("Native callback added multiple candidate default inputs."));}DefaultPin=P;}}
    const auto* Meta=C.Graph->GetAllMetaData().Find(Variable);
    if(!DefaultPin||!DefaultPin->PersistentGuid.IsValid()||!DefaultPin->bNotConnectable||!DefaultPin->bDefaultValueIsReadOnly||
        !DefaultPin->LinkedTo.IsEmpty()||!Meta||!Loaded(Meta->Get())||(*Meta)->Variable!=Variable||!(*Meta)->Metadata.GetVariableGuid().IsValid())
    {return Fail(R,TEXT("niagara_hlsl_default_pair_unobservable"),TEXT("New public default-input/typed metadata identity is incomplete after native callback."));}
    Consumed.Add(OldAdd);Links.Remove(OldAdd);AddEdge(Links,PinId(Add),PinId(Input));
    auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("module_parameter_name"),Variable.GetName().ToString());
    Row->SetObjectField(TEXT("parameter_type"),TypeRow(Variable.GetType()));Row->SetStringField(TEXT("map_get_node_path"),Path(Get));
    Row->SetStringField(TEXT("output_pin_id"),PinId(Add));Row->SetStringField(TEXT("default_pin_id"),PinId(DefaultPin));
    Row->SetStringField(TEXT("output_persistent_guid"),Guid(Add->PersistentGuid));Row->SetStringField(TEXT("default_persistent_guid"),Guid(DefaultPin->PersistentGuid));
    Row->SetStringField(TEXT("parameter_metadata_path"),Path(Meta->Get()));Row->SetStringField(TEXT("parameter_metadata_guid"),Guid((*Meta)->Metadata.GetVariableGuid()));
    Row->SetBoolField(TEXT("native_dynamic_pair_callback_invoked"),true);Row->SetBoolField(TEXT("public_default_input_shape_verified"),true);
    Row->SetBoolField(TEXT("protected_pair_storage_independently_read"),false);PairRows.Add(MakeShared<FJsonValueObject>(Row));return true;
}
} // anonymous namespace

Function::FResult Inspect(const FString& SystemPath,const FString& ScriptPath,const FString& OwnerKind,
    const FString& ScriptIndexJson,const FString& ScriptName,const FString& EmitterIndexJson,const FString& EmitterName)
{
    FResult R;Initialize(R,SystemPath,false);R.Data->SetBoolField(TEXT("read_only"),true);FContext C;
    if(!Base(R,C,SystemPath,false,false)||!Select(R,C,ScriptPath,OwnerKind,ScriptIndexJson,ScriptName,EmitterIndexJson,EmitterName,false)){return R;}
    const auto Before=Capture(C.Graph);BeforeGraph(R,C.Graph,Before);const auto After=Capture(C.Graph);
    R.Data->SetObjectField(TEXT("after"),After.Json);R.Data->SetObjectField(TEXT("graph_after"),After.Json);
    const bool Same=SameGraph(Before,After)&&C.Dirty==C.System->GetOutermost()->IsDirty();R.Data->SetBoolField(TEXT("state_preserved"),Same);
    bool ParamsComplete=false;R.Data->SetObjectField(TEXT("graph_parameters_after"),GraphParameters(C.Graph,ParamsComplete));
    if(!Before.bComplete||!After.bComplete||!ParamsComplete){R.bChangeStateKnown=false;}if(!Same){Fail(R,TEXT("niagara_scratch_inspect_changed"),TEXT("Read-only scratch observation changed observed state."));}
    Preserve(R,C,nullptr);R.Data->SetBoolField(TEXT("package_dirty_after"),C.System->GetOutermost()->IsDirty());R.Data->SetObjectField(TEXT("compile_state"),CompileState(C.System));Complete(R);return R;
}

namespace
{
// Rebase only observed UObject identity cells; authored code/text/default strings
// are not paths and must never be rewritten to make a duplicate appear equal.
FString STRebaseIdentityJson(const FString& Input,const TArray<TPair<FString,FString>>& Paths)
{
    TSharedPtr<FJsonObject> Root;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input),Root)||!Root.IsValid())return FString();
    const TSet<FString> Keys={TEXT("graph_path"),TEXT("node_path"),TEXT("pin_id"),TEXT("default_object_path"),TEXT("subcategory_object_path"),TEXT("terminal_subcategory_object_path"),TEXT("object_path"),TEXT("function_script_path"),TEXT("function_script"),TEXT("function_script_asset_object_path"),TEXT("input_type_object_path"),TEXT("type_class_path")};
    TFunction<void(TSharedPtr<FJsonValue>&,const FString&)> Visit;
    Visit=[&](TSharedPtr<FJsonValue>& Value,const FString& Key)
    {
        if(!Value.IsValid())return;
        if(Value->Type==EJson::Object){auto Object=Value->AsObject();for(auto& Pair:Object->Values)Visit(Pair.Value,FString(Pair.Key));}
        else if(Value->Type==EJson::Array){auto Array=Value->AsArray();for(auto& Item:Array)Visit(Item,Key);Value=MakeShared<FJsonValueArray>(MoveTemp(Array));}
        else if(Value->Type==EJson::String&&(Keys.Contains(Key)||Key==TEXT("links")))
        {
            FString S=Value->AsString();for(const auto& Pair:Paths)if(S.StartsWith(Pair.Key,ESearchCase::CaseSensitive)&&(S.Len()==Pair.Key.Len()||S[Pair.Key.Len()]==TEXT('.')||S[Pair.Key.Len()]==TEXT(':'))){S=Pair.Value+S.Mid(Pair.Key.Len());break;}Value=MakeShared<FJsonValueString>(S);
        }
    };
    for(auto& Pair:Root->Values)Visit(Pair.Value,FString(Pair.Key));return Text(Root.ToSharedRef());
}

// ST primary acquisition preserves target -> selected source order. No internal read reload.
bool STPrimaryLoad(FResult& R,const FString& Requested,bool System,TStrongObjectPtr<UObject>& Held)
{
    FString P=FPackageName::ExportTextPathToObjectPath(Requested).TrimStartAndEnd();P.TrimQuotesInline();
    if(!Bounded(P)||(!P.Contains(TEXT(":"))&&!ObjectPath(P,P)))return Fail(R,TEXT("niagara_scratch_primary_path_invalid"),TEXT("Exact bounded asset/subobject path required before primary acquisition."));
    if(!IsEditorGameThread()||IsPlaySessionActive()||!GEditor||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage())return Fail(R,TEXT("niagara_scratch_primary_context_invalid"),TEXT("Idle live Editor context required before primary acquisition."));
    TStrongObjectPtr<UEditorEngine> Editor(GEditor);TStrongObjectPtr<UWorld> World(GEditor->GetEditorWorldContext().World());
    if(!Loaded(World.Get())||World->bIsTearingDown)return Fail(R,TEXT("niagara_scratch_primary_world_unavailable"),TEXT("Live Editor world required before primary acquisition."));
    if(!HNAdmit(TEXT("before_primary_find")))return false;
    UObject* Found=System?static_cast<UObject*>(FindObject<UNiagaraSystem>(nullptr,*P)):static_cast<UObject*>(FindObject<UNiagaraScript>(nullptr,*P));Held.Reset(Found);
    auto Calls=R.Data->TryGetField(TEXT("primary_load_calls"));TArray<TSharedPtr<FJsonValue>> Rows;if(Calls.IsValid()&&Calls->Type==EJson::Array)Rows=Calls->AsArray();
    auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("role"),System?TEXT("system"):TEXT("script"));Row->SetStringField(TEXT("requested_path"),P);Row->SetBoolField(TEXT("already_loaded"),Found!=nullptr);Row->SetBoolField(TEXT("entered"),false);Row->SetBoolField(TEXT("returned"),false);Rows.Add(MakeShared<FJsonValueObject>(Row));R.Data->SetArrayField(TEXT("primary_load_calls"),Rows);
    if(!Found)
    {
        if(!HNDispatch(TEXT("LoadObject_primary_scratch_source")))return false;
        R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("asset_load_attempted"),true);R.Data->SetBoolField(TEXT("loader_full_effect_state_known"),false);Row->SetBoolField(TEXT("entered"),true);
        UObject* Returned=System?static_cast<UObject*>(LoadObject<UNiagaraSystem>(nullptr,*P)):static_cast<UObject*>(LoadObject<UNiagaraScript>(nullptr,*P));Held.Reset(Returned);Row->SetBoolField(TEXT("returned"),true);Row->SetBoolField(TEXT("returned_nonnull"),Returned!=nullptr);
    }
    // The returned object is strongly held before any context or UObject observer.
    if(!IsEditorGameThread()||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||GEditor!=Editor.Get()||!IsValid(Editor.Get())||!Loaded(World.Get())||World->bIsTearingDown||GEditor->PlayWorld||GEditor->GetPIEWorldContext()||GEditor->GetEditorWorldContext().World()!=World.Get()||!HNAdmit(TEXT("after_primary_load")))
    {R.bChangeStateKnown=false;return Fail(R,TEXT("niagara_scratch_primary_context_changed"),TEXT("Primary load returned but captured Editor/target state changed; no retry."));}
    if(!Held.IsValid()){R.bChangeStateKnown=!Row->GetBoolField(TEXT("entered"));return Fail(R,TEXT("niagara_scratch_primary_not_found"),TEXT("Primary acquisition returned no requested native asset; actual loader prefix retained."));}
    UClass* Expected=System?UNiagaraSystem::StaticClass():UNiagaraScript::StaticClass();
    if(!Loaded(Held.Get())||Held->GetClass()!=Expected||Held->GetPathName()!=P){R.bChangeStateKnown=false;return Fail(R,TEXT("niagara_scratch_primary_identity_invalid"),TEXT("Primary returned an incomplete, redirected or nonexact native object; no replacement/reload."));}
    Row->SetStringField(TEXT("actual_path"),Held->GetPathName());Row->SetBoolField(TEXT("continuation_current"),true);R.bChangeStateKnown=true;return true;
}

// One concrete source-clone lease. Native version data is copied before metadata
// comparisons so an FText observer never reads a stale live VersionData element.
struct FSTCloneSource
{
    struct FVersion
    {
        FNiagaraAssetVersion Version;const FVersionedNiagaraScriptData* Data=nullptr;
        TWeakObjectPtr<UNiagaraScriptSource> Source;TWeakObjectPtr<UNiagaraGraph> Graph;
        TUniquePtr<FVersionedNiagaraScriptData> Authored;
        FSnapshot Before;TSharedRef<FJsonObject> Parameters=MakeShared<FJsonObject>();
    };
    FResult& R;FHNWriterGuard& Guard;TStrongObjectPtr<UNiagaraScript> Script;TStrongObjectPtr<UScriptStruct> DataType;
    TArray<FVersion> Versions;ENiagaraScriptUsage ScriptUsage=ENiagaraScriptUsage::Module;FGuid Exposed;bool Versioned=false,Ready=false;
    TArray<FProperty*> MetadataFields;TArray<FString> MetadataNames;FObjectProperty* SourceCell=nullptr;int32 SourceOffset=0;
    TArray<FFieldClass*> MetadataClasses;TArray<int32> MetadataOffsets,MetadataSizes,MetadataDims;TArray<uint64> MetadataFlags;TArray<TWeakObjectPtr<UClass>> MetadataObjectClasses;
    struct FCopyBinding{FGuid Id;const FVersionedNiagaraScriptData* Data=nullptr;TUniquePtr<FVersionedNiagaraScriptData> Snapshot;};
    TStrongObjectPtr<UNiagaraScript> CopiedScript;TArray<FCopyBinding> CopiedBindings;bool CopyReady=false;
    TStrongObjectPtr<UNiagaraSystem> OriginSystem;FOwner Origin;int32 OriginScriptIndex=INDEX_NONE;
    TArray<TWeakObjectPtr<UNiagaraScript>> OriginScripts;bool OriginReady=false;
    struct FSourceFile{TStrongObjectPtr<UPackage> Package;FString Filename;int64 Bytes=-1;FMD5Hash Hash;bool Dirty=false;};
    TArray<FSourceFile> SourceFiles;
    FSTCloneSource(FResult& In,FHNWriterGuard& G):R(In),Guard(G){}
    bool Stop(const TCHAR* C,const TCHAR* M){return Guard.Stop(C,M);}
    static bool VersionSame(const FNiagaraAssetVersion& A,const FNiagaraAssetVersion& B)
    {return A.VersionGuid==B.VersionGuid&&A.MajorVersion==B.MajorVersion&&A.MinorVersion==B.MinorVersion&&A.bIsVisibleInVersionSelector==B.bIsVisibleInVersionSelector;}
    bool Current()
    {
        if(OriginReady)
        {
            if(!Loaded(OriginSystem.Get()))return Stop(TEXT("niagara_clone_origin_lost"),TEXT("Captured source System is unavailable."));
            const TArray<TObjectPtr<UNiagaraScript>>* Scripts=nullptr;
            if(Origin.Kind==TEXT("system"))Scripts=&OriginSystem->ScratchPadScripts;
            else
            {
                if(!OriginSystem->GetEmitterHandles().IsValidIndex(Origin.Index))return Stop(TEXT("niagara_clone_origin_changed"),TEXT("Source emitter membership changed."));
                const auto& H=OriginSystem->GetEmitterHandles()[Origin.Index];const auto E=H.GetInstance();
                if(E.Emitter!=Origin.Emitter.Emitter||E.Version!=Origin.Emitter.Version||H.GetEmitterData()!=Origin.Data||H.GetName().ToString()!=Origin.Name)return Stop(TEXT("niagara_clone_origin_changed"),TEXT("Source emitter/version changed before container access."));
                auto* Container=Origin.Kind==TEXT("parent_emitter")?Origin.Data->ParentScratchPads.Get():Origin.Data->ScratchPads.Get();
                if(Container!=Origin.Outer)return Stop(TEXT("niagara_clone_origin_changed"),TEXT("Source container identity changed."));
                if(Container)Scripts=&Container->Scripts;
            }
            if(!Scripts||Scripts->Num()!=OriginScripts.Num()||!Scripts->IsValidIndex(OriginScriptIndex))return Stop(TEXT("niagara_clone_origin_changed"),TEXT("Source script inventory changed."));
            for(int32 I=0;I<OriginScripts.Num();++I)if((*Scripts)[I]!=OriginScripts[I].Get())return Stop(TEXT("niagara_clone_origin_changed"),TEXT("Source ordered scratch membership changed."));
        }
        if(!Ready)return true;
        if(!Guard.Valid||!Loaded(Script.Get())||FVersionedNiagaraScriptData::StaticStruct()!=DataType.Get()||!IsValid(DataType.Get())||Script->GetUsage()!=ScriptUsage||Script->IsVersioningEnabled()!=Versioned||Script->GetExposedVersion().VersionGuid!=Exposed)return Stop(TEXT("niagara_clone_source_identity_changed"),TEXT("Captured native source/version metadata root changed; no further observer."));
        auto A=Script->GetAllAvailableVersions();if(A.Num()!=Versions.Num())return Stop(TEXT("niagara_clone_versions_changed"),TEXT("Source version inventory changed across SDK."));
        if(FindFProperty<FObjectProperty>(DataType.Get(),TEXT("Source"))!=SourceCell||!SourceCell||SourceCell->GetOwnerStruct()!=DataType.Get()||SourceCell->ArrayDim!=1||SourceCell->GetElementSize()!=static_cast<int32>(sizeof(TObjectPtr<UNiagaraScriptSourceBase>))||SourceCell->GetOffset_ForInternal()!=SourceOffset||!SourceCell->PropertyClass.IsResolved()||SourceCell->PropertyClass.Get()!=UNiagaraScriptSourceBase::StaticClass())return Stop(TEXT("niagara_clone_source_cell_changed"),TEXT("Native source-cell descriptor binding/layout changed."));
        TArray<FProperty*> Fields;for(TFieldIterator<FProperty> It(DataType.Get(),EFieldIteratorFlags::ExcludeSuper);It;++It)if(!It->HasAnyPropertyFlags(CPF_Transient)&&It->GetFName()!=TEXT("Source")&&It->GetFName()!=TEXT("LastGeneratedVMId"))Fields.Add(*It);
        if(Fields!=MetadataFields)return Stop(TEXT("niagara_clone_metadata_fields_changed"),TEXT("Native version metadata field membership changed."));
        for(int32 I=0;I<Fields.Num();++I){const auto* P=Fields[I];if(P->GetOwnerStruct()!=DataType.Get()||P->GetClass()!=MetadataClasses[I]||P->GetOffset_ForInternal()!=MetadataOffsets[I]||P->GetElementSize()!=MetadataSizes[I]||P->ArrayDim!=MetadataDims[I]||static_cast<uint64>(P->GetPropertyFlags())!=MetadataFlags[I])return Stop(TEXT("niagara_clone_metadata_layout_changed"),TEXT("Version metadata root descriptor class/layout/flags changed."));if(auto* ObjectField=CastField<FObjectProperty>(Fields[I]))if(!ObjectField->PropertyClass.IsResolved()||ObjectField->PropertyClass.Get()!=MetadataObjectClasses[I].Get())return Stop(TEXT("niagara_clone_metadata_object_type_changed"),TEXT("Metadata object-field class slot changed."));}
        for(int32 I=0;I<Versions.Num();++I)
        {
            const auto& V=Versions[I];if(!VersionSame(A[I],V.Version)||Script->GetScriptData(V.Version.VersionGuid)!=V.Data||!Loaded(V.Source.Get())||!Loaded(V.Graph.Get())||!V.Source->NodeGraph.IsResolved()||V.Source->NodeGraph.Get()!=V.Graph.Get()||!SourceCell->Identical_InContainer(V.Data,V.Authored.Get(),PPF_None))return Stop(TEXT("niagara_clone_source_routing_changed"),TEXT("Source version/data/graph binding changed before value access."));
        }
        if(CopyReady)
        {
            if(!Loaded(CopiedScript.Get())||CopiedScript->GetUsage()!=ScriptUsage||CopiedScript->IsVersioningEnabled()!=Versioned||CopiedScript->GetExposedVersion().VersionGuid!=Exposed)return Stop(TEXT("niagara_clone_copied_routing_changed"),TEXT("Copied usage/exposed version changed."));
            auto V=CopiedScript->GetAllAvailableVersions();if(V.Num()!=CopiedBindings.Num())return Stop(TEXT("niagara_clone_copied_routing_changed"),TEXT("Copied version inventory changed."));
            for(int32 I=0;I<V.Num();++I)if(!VersionSame(V[I],Versions[I].Version)||V[I].VersionGuid!=CopiedBindings[I].Id||CopiedScript->GetScriptData(V[I].VersionGuid)!=CopiedBindings[I].Data||!SourceCell->Identical_InContainer(CopiedBindings[I].Data,CopiedBindings[I].Snapshot.Get(),PPF_None))return Stop(TEXT("niagara_clone_copied_routing_changed"),TEXT("Copied version data/source cell changed before access."));
        }
        return true;
    }
    bool BindOrigin(FContext& C)
    {
        OriginSystem.Reset(C.System);Origin=C.Owner;OriginScriptIndex=C.ScriptIndex;
        if(!Guard.Track(C.System)||(Origin.Outer&&!Guard.Track(Origin.Outer))||!Guard.Track(C.Script))return false;
        if(!Origin.Scripts)return Stop(TEXT("niagara_clone_origin_unavailable"),TEXT("Selected source owner inventory unavailable."));
        for(const auto& S:*Origin.Scripts){if(!Guard.Track(S.Get()))return false;OriginScripts.Add(S.Get());}
        OriginReady=true;return Current();
    }
    void ExpectSourceOwnerAppend(const FContext& Target,const FOwner& Destination,UNiagaraScript* Created)
    {
        // Only the declared one-item target append may also extend the captured
        // source inventory; every pre-existing ordered source remains unchanged.
        if(OriginReady&&OriginSystem.Get()==Target.System&&Origin.Kind==Destination.Kind&&Origin.Index==Destination.Index&&Origin.Outer==Destination.Outer)OriginScripts.Add(Created);
    }
    bool CaptureSourceFiles()
    {
        for(UPackage* Package:{Script->GetPackage(),Script->GetOutermost()})
        {
            bool Seen=false;for(const auto& File:SourceFiles)Seen|=File.Package.Get()==Package;if(Seen)continue;
            if(!Guard.Track(Package))return false;FSourceFile File;File.Package.Reset(Package);File.Dirty=Package->IsDirty();
            if(!FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(),File.Filename,FPackageName::GetAssetPackageExtension()))return Stop(TEXT("niagara_clone_source_filename_unavailable"),TEXT("Selected source package filename unavailable."));
            File.Filename=FPaths::ConvertRelativePathToFull(File.Filename);File.Bytes=IFileManager::Get().FileSize(*File.Filename);
            if(File.Bytes>=0){File.Hash=FMD5Hash::HashFile(*File.Filename);if(!File.Hash.IsValid())return Stop(TEXT("niagara_clone_source_file_unavailable"),TEXT("Selected source file read failed before writer."));}
            SourceFiles.Add(MoveTemp(File));
        }
        TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& File:SourceFiles){auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("package"),File.Package->GetName());Row->SetStringField(TEXT("filename"),File.Filename);Row->SetNumberField(TEXT("bytes_before"),File.Bytes);Row->SetStringField(TEXT("md5_before"),LexToString(File.Hash));Row->SetBoolField(TEXT("dirty_before"),File.Dirty);Rows.Add(MakeShared<FJsonValueObject>(Row));}R.Data->SetArrayField(TEXT("source_files_before"),Rows);return Current();
    }
    bool CheckSourceFiles()
    {
        if(!Current()||!HNAdmit(TEXT("source_file_final_identity")))return false;TArray<TSharedPtr<FJsonValue>> Rows;
        bool Saved=false;R.Data->TryGetBoolField(TEXT("saved"),Saved);
        for(const auto& File:SourceFiles)
        {
            const int64 Bytes=IFileManager::Get().FileSize(*File.Filename);const FMD5Hash Hash=Bytes>=0?FMD5Hash::HashFile(*File.Filename):FMD5Hash();
            const bool TargetPackage=File.Package.Get()==Guard.C.System->GetPackage()||File.Package.Get()==Guard.C.System->GetOutermost();
            const bool Same=Bytes==File.Bytes&&Hash==File.Hash;const bool DirtySame=File.Package->IsDirty()==File.Dirty;
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("filename"),File.Filename);Row->SetNumberField(TEXT("bytes_after"),Bytes);Row->SetStringField(TEXT("md5_after"),LexToString(Hash));Row->SetBoolField(TEXT("file_preserved"),Same);Row->SetBoolField(TEXT("dirty_preserved"),DirtySame);Row->SetBoolField(TEXT("source_is_written_target_package"),TargetPackage);Rows.Add(MakeShared<FJsonValueObject>(Row));R.Data->SetArrayField(TEXT("source_files_after"),Rows);
            if((Bytes>=0&&!Hash.IsValid())||(!TargetPackage&&(!Same||!DirtySame))||(TargetPackage&&!Saved&&!Same))return Stop(TEXT("niagara_clone_source_file_changed"),TEXT("Readonly source file/dirty changed outside actual requested target save."));
        }
        R.Data->SetBoolField(TEXT("source_files_verified"),true);return Current();
    }
    bool MetadataSame(const FVersionedNiagaraScriptData& Before,const FVersionedNiagaraScriptData& Actual,const TCHAR* Stage,UNiagaraScript* CopyOwner=nullptr)
    {
        FVersionedNiagaraScriptData Expected=Before;
        // Standard duplication remaps source-owned object references. Plan those
        // exact already-resolved identity cells in a copy, never in either asset.
        if(CopyOwner)for(auto* Field:MetadataFields)if(auto* ObjectField=CastField<FObjectProperty>(Field))
        {
            if(!HNAdmit(TEXT("before_copied_metadata_cell"))||!Current())return false;
            const auto Ref=*ObjectField->ContainerPtrToValuePtr<TObjectPtr<UObject>>(&Before);
            if(!Ref.IsResolved())continue;UObject* Original=Ref.Get();if(!Original)continue;
            if(!HNAdmit(TEXT("before_owned_metadata_remap"))||!Current())return false;
            if(Original!=Script.Get()&&!Original->IsIn(Script.Get()))continue;
            const FString OldRoot=Path(Script.Get()),OldPath=Path(Original);if(!OldPath.StartsWith(OldRoot,ESearchCase::CaseSensitive))return Stop(TEXT("niagara_clone_metadata_reference_invalid"),TEXT("Source-owned metadata path is inconsistent."));
            const FString NewPath=Path(CopyOwner)+OldPath.Mid(OldRoot.Len());UObject* Mapped=FindObject<UObject>(nullptr,*NewPath);TStrongObjectPtr<UObject> Immediate(Mapped);
            if(!HNAdmit(TEXT("after_owned_metadata_remap"))||!Current())return false;
            if(!Mapped||!Guard.Track(Mapped,true)||Mapped->GetClass()!=Original->GetClass()||(Mapped!=CopyOwner&&!Mapped->IsIn(CopyOwner)))return Stop(TEXT("niagara_clone_metadata_reference_invalid"),TEXT("Actual duplicate-owned metadata reference is missing or differs; no loading/localization substitute."));
            *ObjectField->ContainerPtrToValuePtr<TObjectPtr<UObject>>(&Expected)=Mapped;
        }
        // Both operands own their arrays/text. Object-property PPF_None equality
        // compares handles without deep object resolution (PropertyObject.cpp153+).
        for(int32 I=0;I<MetadataFields.Num();++I)
        {
            if(!Current()||!HNAdmit(Stage))return false;auto* P=MetadataFields[I];
            if(P->GetOwnerStruct()!=DataType.Get()||P->GetName()!=MetadataNames[I]||P->GetOffset_ForInternal()<0||P->GetOffset_ForInternal()+P->GetSize()>static_cast<int32>(sizeof(FVersionedNiagaraScriptData)))return Stop(TEXT("niagara_clone_metadata_layout_changed"),TEXT("Native metadata descriptor layout changed before comparison."));
            const bool Same=P->Identical_InContainer(&Expected,&Actual,PPF_None);
            if(!HNAdmit(TEXT("after_copied_version_metadata_compare"))||!Current())return false;
            if(!Same){R.Data->SetStringField(TEXT("clone_metadata_mismatch"),MetadataNames[I]);return Stop(TEXT("niagara_clone_metadata_changed"),TEXT("Authored version metadata differs; derived LastGeneratedVMId is observed separately, not fixed."));}
        }
        return true;
    }
    bool Bind(UNiagaraScript* S,FContext& C)
    {
        Script.Reset(S);if(!Guard.Track(S))return false;DataType.Reset(FVersionedNiagaraScriptData::StaticStruct());
        if(!IsValid(DataType.Get())||DataType->GetStructureSize()!=static_cast<int32>(sizeof(FVersionedNiagaraScriptData)))return Stop(TEXT("niagara_clone_metadata_type_unavailable"),TEXT("Exact native compiled version-data layout required."));
        ScriptUsage=S->GetUsage();Versioned=S->IsVersioningEnabled();
        auto A=S->GetAllAvailableVersions();if(A.IsEmpty())return Stop(TEXT("niagara_clone_version_inventory_unavailable"),TEXT("Nonempty native script versions required before duplicate."));
        Exposed=S->GetExposedVersion().VersionGuid;SourceCell=FindFProperty<FObjectProperty>(DataType.Get(),TEXT("Source"));if(!SourceCell)return Stop(TEXT("niagara_clone_source_cell_unavailable"),TEXT("Native reflected source cell is unavailable."));SourceOffset=SourceCell->GetOffset_ForInternal();
        for(TFieldIterator<FProperty> It(DataType.Get(),EFieldIteratorFlags::ExcludeSuper);It;++It)if(!It->HasAnyPropertyFlags(CPF_Transient)&&It->GetFName()!=TEXT("Source")&&It->GetFName()!=TEXT("LastGeneratedVMId")){MetadataFields.Add(*It);MetadataNames.Add(It->GetName());MetadataClasses.Add(It->GetClass());MetadataOffsets.Add(It->GetOffset_ForInternal());MetadataSizes.Add(It->GetElementSize());MetadataDims.Add(It->ArrayDim);MetadataFlags.Add(static_cast<uint64>(It->GetPropertyFlags()));UClass* ObjectType=nullptr;if(auto* ObjectField=CastField<FObjectProperty>(*It)){if(!ObjectField->PropertyClass.IsResolved())return Stop(TEXT("niagara_clone_metadata_object_type_unresolved"),TEXT("Metadata class must already be resolved before object-cell observation."));ObjectType=ObjectField->PropertyClass.Get();if(!ObjectType||!Guard.Track(ObjectType,true))return false;}MetadataObjectClasses.Add(ObjectType);}
        for(const auto& V:A)
        {
            const auto* D=S->GetScriptData(V.VersionGuid);if(!V.VersionGuid.IsValid()||!D)return Stop(TEXT("niagara_clone_version_data_unavailable"),TEXT("Every copied native version requires actual data."));
            FVersion Row;Row.Version=V;Row.Data=D;Row.Authored=MakeUnique<FVersionedNiagaraScriptData>(*D);
            UNiagaraScriptSourceBase* Returned=S->GetSource(V.VersionGuid);TStrongObjectPtr<UNiagaraScriptSourceBase> Immediate(Returned);if(!HNAdmit(TEXT("after_GetSource_clone_version")))return false;if(S->GetScriptData(V.VersionGuid)!=D)return Stop(TEXT("niagara_clone_version_changed_during_getter"),TEXT("Source version data changed during its public source getter."));Row.Authored=MakeUnique<FVersionedNiagaraScriptData>(*D);
            auto* Source=Cast<UNiagaraScriptSource>(Returned);UNiagaraGraph* Graph=nullptr;
            if(!Loaded(Source)||!Guard.Track(Source)||!Ordinary(Source,Graph)||!Source->IsIn(S)||!Guard.TrackGraph(Graph))return Stop(TEXT("niagara_clone_version_source_unavailable"),TEXT("Each copied version must own an ordinary loaded native source graph."));
            for(auto* Field:MetadataFields)if(auto* ObjectField=CastField<FObjectProperty>(Field))
            {
                const auto Ref=*ObjectField->ContainerPtrToValuePtr<TObjectPtr<UObject>>(Row.Authored.Get());
                if(Ref.IsResolved()&&Ref.Get()&&!Guard.Track(Ref.Get(),true))return false;
            }
            Row.Source=Source;Row.Graph=Graph;Versions.Add(MoveTemp(Row));
        }
        Ready=true;if(!Current())return false;
        for(auto& V:Versions){if(!Protect(R,C,V.Source.Get()))return false;V.Before=Capture(V.Graph.Get());if(!HNAdmit(TEXT("after_clone_version_snapshot")))return false;bool Complete=false;V.Parameters=GraphParameters(V.Graph.Get(),Complete);if(!V.Before.bComplete||!Complete)return Stop(TEXT("niagara_clone_source_snapshot_incomplete"),TEXT("All version graph/metadata projections must be complete."));}
        R.Data->SetNumberField(TEXT("source_version_count"),Versions.Num());R.Data->SetBoolField(TEXT("source_versioning_enabled"),Versioned);R.Data->SetStringField(TEXT("source_exposed_version"),Guid(Exposed));return CaptureSourceFiles();
    }
    bool Unchanged()
    {
        if(!Current())return false;
        for(const auto& V:Versions)
        {
            const FVersionedNiagaraScriptData Actual=*V.Data;
            if(!MetadataSame(*V.Authored,Actual,TEXT("source_metadata_current")))return false;
            const auto A=Capture(V.Graph.Get());if(!HNAdmit(TEXT("after_source_clone_snapshot")))return false;bool Good=false;auto P=GraphParameters(V.Graph.Get(),Good);
            if(!Good||!SameGraph(V.Before,A)||Text(P)!=Text(V.Parameters))return Stop(TEXT("niagara_clone_source_authored_changed"),TEXT("Readonly source graph/pins/metadata changed across SDK; no continuation/retry."));
        }
        R.Data->SetBoolField(TEXT("all_source_versions_preserved"),true);return Current();
    }
    bool VerifyCopy(UNiagaraScript* Created)
    {
        if(!Current()||!HNAdmit(TEXT("before_clone_copy_verification")))return false;
        auto A=Created->GetAllAvailableVersions();if(A.Num()!=Versions.Num()||Created->GetUsage()!=ScriptUsage||Created->IsVersioningEnabled()!=Versioned||Created->GetExposedVersion().VersionGuid!=Exposed)return Stop(TEXT("niagara_clone_copy_versions_mismatch"),TEXT("Duplicate did not preserve actual usage/version inventory."));
        if(!Guard.Track(Created))return false;CopiedScript.Reset(Created);CopyReady=false;CopiedBindings.Empty();
        for(const auto& V:A){const auto* D=Created->GetScriptData(V.VersionGuid);if(!D)return Stop(TEXT("niagara_clone_copy_data_missing"),TEXT("Copied version is unavailable before registration."));FCopyBinding Row;Row.Id=V.VersionGuid;Row.Data=D;Row.Snapshot=MakeUnique<FVersionedNiagaraScriptData>(*D);CopiedBindings.Add(MoveTemp(Row));}
        CopyReady=true;if(!Current())return false;TArray<TSharedPtr<FJsonValue>> Rows;
        for(int32 I=0;I<Versions.Num();++I)
        {
            const auto& V=Versions[I];if(!VersionSame(A[I],V.Version))return Stop(TEXT("niagara_clone_copy_version_mismatch"),TEXT("Duplicate version GUID/order/major/minor/visibility differs."));
            const auto* D=Created->GetScriptData(V.Version.VersionGuid);if(!D)return Stop(TEXT("niagara_clone_copy_data_missing"),TEXT("Duplicate version data unavailable."));const FVersionedNiagaraScriptData Copy=*D;
            if(!MetadataSame(*V.Authored,Copy,TEXT("copied_metadata_verification"),Created))return false;
            UNiagaraScriptSourceBase* Returned=Created->GetSource(V.Version.VersionGuid);TStrongObjectPtr<UNiagaraScriptSourceBase> Immediate(Returned);if(!HNAdmit(TEXT("after_created_version_source")))return false;
            auto* Source=Cast<UNiagaraScriptSource>(Returned);UNiagaraGraph* G=nullptr;if(!Loaded(Source)||!Guard.Track(Source)||!Ordinary(Source,G)||!Source->IsIn(Created)||!Guard.TrackGraph(G))return Stop(TEXT("niagara_clone_copy_source_invalid"),TEXT("Duplicate version must own an ordinary private source graph."));
            const auto B=Capture(G);if(!HNAdmit(TEXT("after_copied_version_graph")))return false;bool Good=false;auto P=GraphParameters(G,Good);
            auto Rebase=[&](const FString& T){return STRebaseIdentityJson(T,{{Path(G),Path(V.Graph.Get())},{Path(Source),Path(V.Source.Get())},{Path(Created),Path(Script.Get())}});};
            if(!Good||!B.bComplete||Rebase(Text(B.Json))!=Text(V.Before.Json)||Rebase(Text(P))!=Text(V.Parameters))return Stop(TEXT("niagara_clone_copy_graph_mismatch"),TEXT("Duplicate authored graph/pins/parameter metadata differ from corresponding source version."));
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("version"),Guid(V.Version.VersionGuid));Row->SetStringField(TEXT("source_graph"),Path(V.Graph.Get()));Row->SetStringField(TEXT("copied_graph"),Path(G));Row->SetBoolField(TEXT("authored_copy_verified"),true);Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
        R.Data->SetArrayField(TEXT("cloned_versions"),Rows);return Unchanged();
    }
};
FSTCloneSource* STClone=nullptr;
bool STCloneAdmit(){return !STClone||STClone->Current();}

bool STClonePreserved(){return !STClone||STClone->Unchanged();}

} // anonymous ST clone helpers

Function::FResult Create(const FString& TargetSystemPath,const FString& SourceScriptPath,const FString& SourceSystemPath,
    const FString& SourceOwnerKind,const FString& SourceScriptIndexJson,const FString& SourceScratchPadName,
    const FString& SourceEmitterIndexJson,const FString& SourceEmitterName,const FString& TargetOwnerKind,
    const FString& TargetEmitterIndexJson,const FString& TargetEmitterName,const FString& NewScriptName,
    const FString& TargetSupportedUsage,bool bAllowSourceEdit,bool bSave,bool bRequestCompile)
{
    FResult R;Initialize(R,TargetSystemPath,bSave);FContext C;ENiagaraScriptUsage UsageValue;FOwner Destination;
    if(!Usage(TargetSupportedUsage,UsageValue)||!Bounded(NewScriptName,128)){Fail(R,TEXT("niagara_scratch_usage_or_name_invalid"),TEXT("Use a supported legacy stack usage and bounded new script name."));return R;}
    for(TCHAR Ch:NewScriptName){if(!FChar::IsAlnum(Ch)&&Ch!=TEXT('_')){Fail(R,TEXT("niagara_scratch_name_unsupported"),TEXT("First cohort admits literal alphanumeric/underscore new object names only."));return R;}}
    FString TargetCanonical;if(!ObjectPath(FPackageName::ExportTextPathToObjectPath(TargetSystemPath),TargetCanonical)){Fail(R,TEXT("niagara_scratch_system_path_invalid"),TEXT("Exact primary target path required."));return R;}
    const FString TargetPackage=FPackageName::ObjectPathToPackageName(TargetCanonical);const bool TempTarget=TargetPackage.StartsWith(TEXT("/Game/CubelessMCPTests/"))||TargetPackage.StartsWith(TEXT("/Game/_MCP_Temp/NiagaraGenerated/"));
    if(!TargetPackage.StartsWith(TEXT("/Game/"))||(!TempTarget&&!bAllowSourceEdit)){Fail(R,TEXT("niagara_write_scope_forbidden"),TEXT("Target scope is refused before primary loading."));return R;}
    TStrongObjectPtr<UObject> TargetLoaded;if(!STPrimaryLoad(R,TargetSystemPath,true,TargetLoaded)){Complete(R);return R;}C.System=Cast<UNiagaraSystem>(TargetLoaded.Get());C.Keep.Reset(C.System);
    const bool DefaultTemplateMode=SourceScriptPath.IsEmpty()&&SourceSystemPath.IsEmpty();
    auto TemplateGuard=MakeUnique<FHNWriterGuard>(R,C,nullptr);if(!TemplateGuard->Valid){Complete(R);return R;}
    TGuardValue<FHNWriterGuard*> HNTemplateScope(HNWriter,TemplateGuard.Get());
    if(!Base(R,C,TargetLoaded->GetPathName(),true,bAllowSourceEdit)){Complete(R);return R;}
    if(DefaultTemplateMode){if(!Owner(R,C,TargetOwnerKind,TargetEmitterIndexJson,TargetEmitterName,Destination,true)){Complete(R);return R;}if(Destination.Outer&&!TemplateGuard->Track(Destination.Outer)){Complete(R);return R;}TemplateGuard->BindBlankDestination(Destination);}
    FSTCloneSource CloneSource(R,*TemplateGuard);TGuardValue<FSTCloneSource*> CloneScope(STClone,DefaultTemplateMode?nullptr:&CloneSource);
    auto STAfter=[&](const TCHAR* Stage){return HNAdmit(Stage)&&(DefaultTemplateMode||!CloneSource.Ready||CloneSource.Unchanged());};
    UNiagaraScript* Source=nullptr;TStrongObjectPtr<UNiagaraScript> KeepSource;TStrongObjectPtr<UNiagaraSystem> KeepSourceSystem;
    FString CreationMode=TEXT("duplicate"),SourceKind=TEXT("script_path");int32 SourceIndex=INDEX_NONE,SourceEmitter=INDEX_NONE;FString SourceOwnerName;
    if(!SourceScriptPath.IsEmpty())
    {TStrongObjectPtr<UObject> SourceLoaded;if(!STPrimaryLoad(R,SourceScriptPath,false,SourceLoaded)){Complete(R);return R;}Source=Cast<UNiagaraScript>(SourceLoaded.Get());KeepSource.Reset(Source);if(!TemplateGuard->Track(Source)){Complete(R);return R;}SourceOwnerName=Source->GetOuter()->GetName();}
    else if(!SourceSystemPath.IsEmpty())
    {
        TStrongObjectPtr<UObject> SourceLoaded;if(!STPrimaryLoad(R,SourceSystemPath,true,SourceLoaded)){Complete(R);return R;}if(!TemplateGuard->Track(SourceLoaded.Get())){Complete(R);return R;}
        FContext From;if(!Base(R,From,SourceLoaded->GetPathName(),false,false)||!Select(R,From,TEXT(""),SourceOwnerKind,SourceScriptIndexJson,SourceScratchPadName,SourceEmitterIndexJson,SourceEmitterName,false,true)){Complete(R);return R;}
        Source=From.Script;if(!CloneSource.BindOrigin(From)){Complete(R);return R;}KeepSourceSystem.Reset(From.System);SourceKind=From.Owner.Kind;SourceOwnerName=From.Owner.Name;SourceIndex=From.ScriptIndex;SourceEmitter=From.Owner.Index;
        R.Data->SetStringField(TEXT("system_path"),Path(C.System));R.Data->SetObjectField(TEXT("scratch_catalog_before"),C.Owners);R.Data->SetObjectField(TEXT("user_parameters_before"),C.Users);R.Data->SetObjectField(TEXT("emitter_handles_before"),C.HandleState);
        R.Data->SetBoolField(TEXT("package_dirty_before"),C.Dirty);
    }
    else
    {
        UClass* Class=FindObject<UClass>(nullptr,TEXT("/Script/NiagaraEditor.NiagaraEditorSettings"));
        const auto* Settings=Class&&Class==UNiagaraEditorSettings::StaticClass()?Cast<UNiagaraEditorSettings>(Class->GetDefaultObject(false)):nullptr;
        if(!Settings||!Settings->HasAnyFlags(RF_ClassDefaultObject)||Settings->GetClass()!=Class)
        {Fail(R,TEXT("niagara_scratch_default_template_unavailable"),TEXT("Exact native EditorSettings/default is unavailable; no arbitrary blank graph substitute."));return R;}
        const TWeakObjectPtr<const UNiagaraEditorSettings> SettingsIdentity(Settings);const FSoftObjectPath TemplatePath=Settings->DefaultModuleScript;
        const FString TemplateText=TemplatePath.ToString();FString NormalizedTemplate;
        if(!ObjectPath(TemplateText,NormalizedTemplate)||TemplateText.Len()>=NAME_SIZE||NormalizedTemplate.Len()>=NAME_SIZE)
        {Fail(R,TEXT("niagara_scratch_default_template_path_invalid"),TEXT("Configured template must be a bounded raw/expanded exact object path before TryLoad."));return R;}
        R.Data->SetStringField(TEXT("configured_default_module_template"),TemplateText);R.Data->SetBoolField(TEXT("asset_load_attempted"),true);
        R.bChangeStateKnown=false;
        if(!HNDispatch(TEXT("TryLoad_DefaultModuleScript"))){Complete(R);return R;}
        UObject* LoadedTemplate=TemplatePath.TryLoad();R.Data->SetBoolField(TEXT("default_template_load_returned_nonnull"),LoadedTemplate!=nullptr);
        if(!HNAdmit(TEXT("after_TryLoad_DefaultModuleScript"))){Complete(R);return R;}
        if(SettingsIdentity.Get()!=Settings||Class->GetDefaultObject(false)!=Settings||Settings->DefaultModuleScript!=TemplatePath)
        {HNWriter->Stop(TEXT("niagara_scratch_default_settings_changed"),TEXT("Configured EditorSettings/template changed during load; no replacement default adopted."));Complete(R);return R;}
        Source=Cast<UNiagaraScript>(LoadedTemplate);CreationMode=TEXT("default_module_template");SourceKind=TEXT("none");
        if(!Loaded(Source)||Source->GetClass()!=UNiagaraScript::StaticClass()||Source->GetPathName()!=NormalizedTemplate)
        {Fail(R,TEXT("niagara_scratch_default_template_unavailable"),TEXT("Configured DefaultModuleScript load returned no exact native Script; no synthesized graph or retry."));Complete(R);return R;}
        R.Data->SetStringField(TEXT("default_template_loaded_path"),Source->GetPathName());R.Data->SetStringField(TEXT("default_template_loaded_usage"),UsageName(Source->GetUsage()));
        R.Data->SetBoolField(TEXT("default_template_versioning_enabled"),Source->IsVersioningEnabled());
        if(!HNWriter->Track(Source)){Complete(R);return R;}
    }
    KeepSource.Reset(Source);if(!DefaultTemplateMode){if(!Owner(R,C,TargetOwnerKind,TargetEmitterIndexJson,TargetEmitterName,Destination,true)){Complete(R);return R;}if(Destination.Outer&&!TemplateGuard->Track(Destination.Outer)){Complete(R);return R;}TemplateGuard->BindBlankDestination(Destination);}
    if(!TemplateGuard->Track(Source)){Complete(R);return R;}
    UNiagaraScriptSource* SourceNative=nullptr;UNiagaraGraph* SourceGraph=nullptr;
    if(!DefaultTemplateMode&&!CloneSource.Bind(Source,C)){Complete(R);return R;}
    if(!SafeCloneSource(R,Source,SourceNative,SourceGraph)){Complete(R);return R;}
    if(HNWriter&&!HNWriter->BindDefaultTemplate(Source,SourceNative,SourceGraph)){Complete(R);return R;}
    if(!Protect(R,C,SourceNative)||!HNAdmit(TEXT("after_default_template_capture"))){Complete(R);return R;}
    const auto SourceBefore=Capture(SourceGraph);if(!HNAdmit(TEXT("after_clone_source_capture"))){Complete(R);return R;}bool ParamsComplete=false;const auto SourceParameters=GraphParameters(SourceGraph,ParamsComplete);
    if(!SourceBefore.bComplete||!ParamsComplete||!Source->GetLatestScriptData()){Fail(R,TEXT("niagara_scratch_clone_source_unobservable"),TEXT("Source graph/parameter/latest version must be complete before duplication."));return R;}
    if(!HNAdmit(TEXT("after_default_template_snapshot"))){Complete(R);return R;}
    if(Destination.Scripts&&Destination.Scripts->Num()>=MaxScratch){Fail(R,TEXT("niagara_scratch_owner_bound"),TEXT("Target scratch owner already contains64 scripts."));return R;}
    R.Data->SetStringField(TEXT("source_script_path"),DefaultTemplateMode?FString():Path(Source));R.Data->SetStringField(TEXT("source_script_name"),DefaultTemplateMode?FString():Source->GetName());R.Data->SetStringField(TEXT("source_owner_kind"),SourceKind);
    R.Data->SetStringField(TEXT("source_owner_name"),SourceOwnerName);R.Data->SetNumberField(TEXT("source_script_index"),SourceIndex);R.Data->SetNumberField(TEXT("source_emitter_index"),SourceEmitter);
    R.Data->SetObjectField(TEXT("source_graph_before"),SourceBefore.Json);R.Data->SetStringField(TEXT("creation_mode"),CreationMode);
    R.Data->SetObjectField(TEXT("before"),C.Owners);R.Stage=TEXT("duplicate_private_script");R.bMutationAttempted=true;R.bChangeStateKnown=false;R.Data->SetNumberField(TEXT("script_duplicate_requests"),1);
    if(!HNDispatch(TEXT("System_Modify_blank"),true)){Complete(R);return R;}C.System->Modify();if(!STAfter(TEXT("after_System_Modify_blank"))){Complete(R);return R;}
    if(!Destination.Outer)
    {
        if(!HNDispatch(TEXT("NewScratchPadContainer_blank"),true)){Complete(R);return R;}Destination.Outer=NewObject<UNiagaraScratchPadContainer>(Destination.Emitter.Emitter,NAME_None,RF_Transactional);
        R.Data->SetBoolField(TEXT("new_scratch_container_returned_nonnull"),Destination.Outer!=nullptr);if(!STAfter(TEXT("after_NewScratchPadContainer_blank"))){Complete(R);return R;}
        if(!Loaded(Destination.Outer)){Fail(R,TEXT("niagara_scratch_container_creation_failed"),TEXT("Owned local scratch container allocation did not return an observable object; actual partial owner state retained."));Complete(R);return R;}
        if(HNWriter&&(!HNWriter->Track(Destination.Outer)||Destination.Outer->GetOuter()!=Destination.Emitter.Emitter||!Destination.Outer->IsIn(C.System)||Destination.Outer->GetPackage()!=C.System->GetPackage()||Destination.Outer->GetOutermost()!=C.System->GetOutermost())){Fail(R,TEXT("niagara_scratch_container_owner_changed"),TEXT("New container must be owned by the exact local emitter/System package."));Complete(R);return R;}
        Destination.Data->ScratchPads=Cast<UNiagaraScratchPadContainer>(Destination.Outer);Destination.Scripts=&Destination.Data->ScratchPads->Scripts;
        if(HNWriter&&!HNWriter->AdoptBlankContainer(Destination.Outer)){Complete(R);return R;}
    }
    if(!HNDispatch(TEXT("ScratchOwner_Modify_blank"),true)){Complete(R);return R;}Destination.Outer->Modify();if(!STAfter(TEXT("after_ScratchOwner_Modify_blank"))){Complete(R);return R;}
    const FName Desired(NewScriptName.IsEmpty()?(DefaultTemplateMode?TEXT("ScratchModule"):*Source->GetName()):*NewScriptName);
    const FName Unique=MakeUniqueObjectName(Destination.Outer,UNiagaraScript::StaticClass(),Desired);
    R.Data->SetStringField(TEXT("expected_new_script_name"),Unique.ToString());R.Data->SetStringField(TEXT("expected_new_script_outer"),Path(Destination.Outer));
    if(!HNDispatch(TEXT("StaticDuplicateObject_DefaultModule"),true)){Complete(R);return R;}
    UObject* Duplicated=StaticDuplicateObject(Source,Destination.Outer,Unique,RF_Transactional);TStrongObjectPtr<UObject> ImmediateDuplicate(Duplicated);R.Data->SetBoolField(TEXT("script_duplicate_returned_nonnull"),Duplicated!=nullptr);
    if(!STAfter(TEXT("after_StaticDuplicateObject_DefaultModule"))){Complete(R);return R;}
    auto* Created=Cast<UNiagaraScript>(Duplicated);C.KeepScript.Reset(Created);
    if(!Loaded(Created)||Created->GetClass()!=UNiagaraScript::StaticClass()||Created->GetOuter()!=Destination.Outer)
    {Fail(R,TEXT("niagara_scratch_duplicate_failed"),TEXT("Native duplicate identity is unavailable; partial owner/script allocation is retained, no retry."));Preserve(R,C,nullptr);Complete(R);return R;}
    Created->ClearFlags(RF_Public|RF_Standalone);Created->SetFlags(RF_Transactional);
    if(HNWriter&&(Created->GetFName()!=Unique||!Created->IsIn(C.System)||Created->GetPackage()!=C.System->GetPackage()||Created->GetOutermost()!=C.System->GetOutermost()||!HNWriter->Track(Created))){Fail(R,TEXT("niagara_scratch_new_module_owner_changed"),TEXT("Created module must retain exact unique name/owner/effective package."));Complete(R);return R;}
    if(DefaultTemplateMode){R.Data->SetStringField(TEXT("new_script_name"),Created->GetName());R.Data->SetStringField(TEXT("new_script_path"),Path(Created));}
    auto* CreatedData=Created->GetLatestScriptData();
    if(!CreatedData){Fail(R,TEXT("niagara_scratch_duplicate_data_invalid"),TEXT("Duplicated script lacks an observable latest version; partial script allocation is retained."));Preserve(R,C,nullptr);Complete(R);return R;}
    if(CreationMode==TEXT("default_module_template")){CreatedData->ModuleUsageBitmask|=1<<static_cast<int32>(UsageValue);}
    {
        UNiagaraGraph* NewBlankGraph=nullptr;auto* NewBlankSource=Cast<UNiagaraScriptSource>(Created->GetLatestSource());
        if(!Ordinary(NewBlankSource,NewBlankGraph)||!NewBlankSource->IsIn(Created)||Created->GetUsage()!=Source->GetUsage()||!HNWriter->BindCreatedBlank(Created,NewBlankSource,NewBlankGraph))
        {Fail(R,TEXT("niagara_scratch_blank_source_invalid"),TEXT("Duplicated configured template must own the exact ordinary native Module source/graph before registration."));Complete(R);return R;}
    }
    if(!DefaultTemplateMode&&!CloneSource.VerifyCopy(Created)){Complete(R);return R;}
    if(!HNDispatch(TEXT("ScratchScripts_Add_blank"),true)){Complete(R);return R;}Destination.Scripts->Add(Created);R.Data->SetBoolField(TEXT("scratch_script_registered"),true);R.bChanged=true;
    if(HNWriter&&!HNWriter->ExpectBlankAppend(Created)){Complete(R);return R;}
    if(!DefaultTemplateMode)CloneSource.ExpectSourceOwnerAppend(C,Destination,Created);
    if(!STAfter(TEXT("after_ScratchScripts_Add_blank"))){Complete(R);return R;}
    if(Destination.Emitter.Emitter){if(!HNDispatch(TEXT("NotifyScratchPadScriptsChanged_blank"),true)){Complete(R);return R;}Destination.Emitter.Emitter->NotifyScratchPadScriptsChanged();if(!STAfter(TEXT("after_NotifyScratchPadScriptsChanged_blank"))){Complete(R);return R;}}
    if(!HNDispatch(TEXT("MarkPackageDirty_blank"),true)){Complete(R);return R;}C.System->MarkPackageDirty();if(!STAfter(TEXT("after_MarkPackageDirty_blank"))){Complete(R);return R;}
    UNiagaraGraph* CreatedGraph=nullptr;auto* CreatedSource=Cast<UNiagaraScriptSource>(Created->GetLatestSource());
    if(!Ordinary(CreatedSource,CreatedGraph)||!CreatedSource->IsIn(Created)){Fail(R,TEXT("niagara_scratch_duplicate_source_invalid"),TEXT("Created private module lacks its own ordinary native graph; retained partial work."));Preserve(R,C,nullptr,Destination.Key,Created);Complete(R);return R;}
    if(HNWriter&&!HNAdmit(TEXT("created_blank_source_recheck"))){Complete(R);return R;}
    const auto After=Capture(CreatedGraph);if(!HNAdmit(TEXT("after_created_module_snapshot"))){Complete(R);return R;}bool NewParamsComplete=false;const auto NewParameters=GraphParameters(CreatedGraph,NewParamsComplete);
    // Asset and private subobjects use different first-subobject separators. Rebase
    // exact observed owners deepest first without changing any other copied field.
    const auto RebaseCreatedPaths=[&](const FString& Value)
    {
        return STRebaseIdentityJson(Value,{{Path(CreatedGraph),Path(SourceGraph)},{Path(CreatedSource),Path(SourceNative)},{Path(Created),Path(Source)}});
    };
    const FString RebasedGraph=RebaseCreatedPaths(Text(After.Json));
    const FString RebasedParameters=RebaseCreatedPaths(Text(NewParameters));
    const bool GraphCopy=After.bComplete&&Text(SourceBefore.Json)==RebasedGraph&&NewParamsComplete&&Text(SourceParameters)==RebasedParameters;
    const bool Identity=Created->GetUsage()==Source->GetUsage()&&!Created->IsAsset()&&Created->IsIn(C.System)&&Created->GetOutermost()==C.System->GetOutermost();
    R.Data->SetBoolField(TEXT("source_graph_copy_verified"),GraphCopy);R.Data->SetBoolField(TEXT("private_script_identity_verified"),Identity);
    R.Data->SetStringField(TEXT("new_script_name"),Created->GetName());R.Data->SetStringField(TEXT("new_script_path"),Path(Created));R.Data->SetStringField(TEXT("target_owner_kind"),Destination.Kind);R.Data->SetStringField(TEXT("target_owner_name"),Destination.Name);
    R.Data->SetNumberField(TEXT("target_emitter_index"),Destination.Index);R.Data->SetNumberField(TEXT("target_scratch_pad_count"),Destination.Scripts->Num());R.Data->SetStringField(TEXT("target_supported_usage"),UsageName(UsageValue));R.Data->SetStringField(TEXT("usage"),UsageName(Created->GetUsage()));
    R.Data->SetNumberField(TEXT("module_usage_bitmask"),CreatedData->ModuleUsageBitmask);R.Data->SetStringField(TEXT("write_scope"),CreationMode==TEXT("duplicate")?TEXT("target_temp_scratch_pad_duplicate_only"):TEXT("target_temp_scratch_pad_create"));
    R.Data->SetObjectField(TEXT("graph_before"),SourceBefore.Json);R.Data->SetObjectField(TEXT("graph_after"),After.Json);R.bChangeStateKnown=After.bComplete&&NewParamsComplete;
    if(!GraphCopy||!Identity){Fail(R,TEXT("niagara_scratch_duplicate_postcondition_failed"),TEXT("Exact source graph/type/default/link copy or native private identity differs; created work remains."));}
    Finish(R,C,CreatedGraph,After,bSave,bRequestCompile,Destination.Key,Created,true);
    if(!R.bChangeStateKnown||!HNWriter->Valid){Complete(R);return R;}
    if(!DefaultTemplateMode&&(!CloneSource.VerifyCopy(Created)||!CloneSource.CheckSourceFiles())){Complete(R);return R;}
    if(R.Data->HasField(TEXT("scratch_catalog_after"))){R.Data->SetObjectField(TEXT("after"),R.Data->GetObjectField(TEXT("scratch_catalog_after")).ToSharedRef());}
    return R;
}
Function::FResult Write(const FString& SystemPath,const FString& ParameterName,const FString& ValueJson,
    const FString& ParameterType,const FString& DefaultValue,const FString& ScriptPath,const FString& OwnerKind,
    const FString& ScriptIndexJson,const FString& ScriptName,const FString& EmitterIndexJson,
    const FString& EmitterName,bool bSkipIfExists,bool bAllowSourceEdit,bool bSave,bool bRequestCompile)
{
    FResult R;Initialize(R,SystemPath,bSave);FNiagaraTypeDefinition Type;FContext C;
    FString Name=ParameterName.TrimStartAndEnd();
    if(!Bounded(Name,256)||Name.IsEmpty()||!Name.Contains(TEXT("."))||Name.Contains(TEXT(" "))||Name.StartsWith(TEXT("."))||Name.EndsWith(TEXT("."))||!LiteralType(ParameterType,Type))
    {Fail(R,TEXT("niagara_scratch_parameter_invalid"),TEXT("Namespaced bounded parameter and finite native float/int/bool/vector/color/Position type required."));return R;}
    if(ScriptPath.IsEmpty()&&ScriptName.IsEmpty()&&ScriptIndexJson==TEXT("null"))
    {Fail(R,TEXT("niagara_scratch_script_selector_required"),TEXT("Parameter write requires exact target-local script path/name/index."));return R;}
    if(!Base(R,C,SystemPath,true,bAllowSourceEdit)||!Select(R,C,ScriptPath,OwnerKind,ScriptIndexJson,ScriptName,EmitterIndexJson,EmitterName,true)){return R;}
    const auto Before=Capture(C.Graph);BeforeGraph(R,C.Graph,Before);
    const auto* Schema=Cast<UEdGraphSchema_Niagara>(UEdGraphSchema_Niagara::StaticClass()->GetDefaultObject(false));FString DefaultText;
    if(!Before.bComplete||!Schema||!Default(R,Type,DefaultValue,ValueJson,Schema,DefaultText))
    {if(R.ErrorCode.IsEmpty()){Fail(R,TEXT("niagara_scratch_graph_unobservable"),TEXT("Selected graph and exact already-loaded Niagara schema default must be available."));}RejectAfterBaseline(R,C);return R;}
    R.Data->SetStringField(TEXT("requested_parameter_name"),Name);R.Data->SetStringField(TEXT("parameter_type"),Type.GetName());
    R.Data->SetBoolField(TEXT("raw_default_precedence"),!DefaultValue.IsEmpty());R.Data->SetBoolField(TEXT("skipped_existing"),false);
    UEdGraphNode* ExistingNode=nullptr;UEdGraphPin* ExistingPin=nullptr;int32 ExistingIndex=INDEX_NONE;
    for(int32 I=0;I<C.Graph->Nodes.Num();++I)
    {auto* Node=C.Graph->Nodes[I].Get();if(!ExactNodeClass(Node,TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapSet"))){continue;}
        for(auto* Pin:Node->Pins){if(Pin&&Pin->Direction==EGPD_Input&&Pin->PinName.ToString().Equals(Name,ESearchCase::IgnoreCase))
        {ExistingNode=Node;ExistingPin=Pin;ExistingIndex=I;break;}}if(ExistingPin){break;}}
    if(ExistingPin)
    {
        if(!bSkipIfExists){Fail(R,TEXT("niagara_scratch_parameter_write_exists"),TEXT("Scratch already writes the selected name; skip_if_exists=false never replaces a value graph."));RejectAfterBaseline(R,C);return R;}
        R.Data->SetBoolField(TEXT("skipped_existing"),true);R.Data->SetBoolField(TEXT("save_requested"),false);
        R.Data->SetStringField(TEXT("parameter_name"),ExistingPin->PinName.ToString());R.Data->SetStringField(TEXT("parameter_type"),UEdGraphSchema_Niagara::PinToTypeDefinition(ExistingPin).GetName());
        R.Data->SetStringField(TEXT("default_value"),ExistingPin->DefaultValue);R.Data->SetStringField(TEXT("parameter_map_set_node_name"),ExistingNode->GetName());
        R.Data->SetStringField(TEXT("parameter_map_set_node_guid"),Guid(ExistingNode->NodeGuid));R.Data->SetNumberField(TEXT("parameter_map_set_node_index"),ExistingIndex);
        R.Data->SetStringField(TEXT("write_scope"),TEXT("scratch_pad_parameter_write_existing_skip"));RejectAfterBaseline(R,C);return R;
    }
    FNiagaraVariable Variable(Type,FName(*Name));auto& Metadata=C.Graph->GetAllMetaData();UNiagaraScriptVariable* ScriptVariable=nullptr;
    for(const auto& P:Metadata)
    {if(P.Key.GetName().ToString().Equals(Name,ESearchCase::IgnoreCase))
        {if(P.Key.GetName()!=Variable.GetName()||P.Key.GetType()!=Type||!Loaded(P.Value.Get())){Fail(R,TEXT("niagara_scratch_parameter_metadata_conflict"),TEXT("Existing same-name parameter metadata has another case/type or unobservable identity."));RejectAfterBaseline(R,C);return R;}ScriptVariable=P.Value.Get();}}
    if(!ScriptVariable&&Metadata.Num()>=MaxParameters){Fail(R,TEXT("niagara_scratch_parameter_bound"),TEXT("Selected graph parameter inventory is full before declaration."));RejectAfterBaseline(R,C);return R;}
    FStack Chain;Chain.Graph=C.Graph;int32 Outputs=0;
    for(UEdGraphNode* N:C.Graph->Nodes){if(auto* O=Cast<UNiagaraNodeOutput>(N);O&&O->GetUsage()==ENiagaraScriptUsage::Module){Chain.Output=O;++Outputs;}}
    if(Outputs!=1||!Stack(R,Chain)){if(R.ErrorCode.IsEmpty()){Fail(R,TEXT("niagara_scratch_module_output_not_unique"),TEXT("Scratch needs one native Module output and its complete reciprocal map chain."));}RejectAfterBaseline(R,C);return R;}
    UEdGraphPin* OutputIn=nullptr;if(!MapPin(Chain.Output,EGPD_Input,OutputIn)||!OutputIn||OutputIn->LinkedTo.Num()!=1)
    {Fail(R,TEXT("niagara_scratch_map_chain_invalid"),TEXT("Module OutputMap requires exactly one existing reciprocal predecessor before append."));RejectAfterBaseline(R,C);return R;}
    UEdGraphPin* Previous=OutputIn->LinkedTo[0];UClass* SetClass=FindObject<UClass>(nullptr,TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapSet"));
    if(!IsValid(SetClass)||!SetClass->HasAllClassFlags(CLASS_Native)||SetClass->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)||!SetClass->IsChildOf(UNiagaraNode::StaticClass()))
    {Fail(R,TEXT("niagara_scratch_mapset_class_unavailable"),TEXT("Exact concrete native MapSet class must already be loaded; no private header or class load fallback."));RejectAfterBaseline(R,C);return R;}
    const bool NewMetadata=!ScriptVariable;bool BeforeParamsComplete=false;const auto BeforeParameters=GraphParameters(C.Graph,BeforeParamsComplete);
    if(!BeforeParamsComplete){Fail(R,TEXT("niagara_scratch_parameters_unobservable"),TEXT("Selected typed metadata inventory is incomplete before allocation."));RejectAfterBaseline(R,C);return R;}
    R.Stage=TEXT("append_parameter_map_write");R.bMutationAttempted=true;R.bChangeStateKnown=false;R.Data->SetNumberField(TEXT("parameter_write_requests"),1);
    C.System->Modify();C.Script->Modify();C.Source->Modify();C.Graph->Modify();
    auto* Set=NewObject<UNiagaraNode>(C.Graph,SetClass,NAME_None,RF_Transactional);
    auto ObserveFailure=[&]()
    {const auto Actual=Capture(C.Graph);R.Data->SetObjectField(TEXT("after"),Actual.Json);R.Data->SetObjectField(TEXT("graph_after"),Actual.Json);
        bool PComplete=false;R.Data->SetObjectField(TEXT("graph_parameters_after"),GraphParameters(C.Graph,PComplete));R.bChangeStateKnown=Actual.bComplete&&PComplete;
        R.bChanged=Actual.bComplete&&!SameGraph(Before,Actual);Preserve(R,C,C.Graph);R.Data->SetBoolField(TEXT("compile_and_save_suppressed_after_failure"),true);Complete(R);};
    if(!Loaded(Set)||Set->GetClass()!=SetClass){Fail(R,TEXT("niagara_scratch_mapset_allocation_failed"),TEXT("MapSet allocation did not return exact observable native identity; retained partial state."));ObserveFailure();return R;}
    Set->NodePosX=Chain.Output->NodePosX-300;Set->NodePosY=Chain.Output->NodePosY+120;C.Graph->AddNode(Set,false,false);Set->CreateNewGuid();Set->PostPlacedNewNode();Set->AllocateDefaultPins();
    UEdGraphPin* In=nullptr;UEdGraphPin* Out=nullptr;
    if(!MapPin(Set,EGPD_Input,In)||!MapPin(Set,EGPD_Output,Out)||!In||!Out)
    {Fail(R,TEXT("niagara_scratch_mapset_shape_failed"),TEXT("Allocated MapSet lacks exact native map pins; node is retained, no rollback."));ObserveFailure();return R;}
    if(NewMetadata)
    {
        ScriptVariable=NewObject<UNiagaraScriptVariable>(C.Graph,NAME_None,RF_Transactional);
        if(!Loaded(ScriptVariable)){Fail(R,TEXT("niagara_scratch_metadata_allocation_failed"),TEXT("Graph-owned metadata allocation is unavailable; actual map node is retained."));ObserveFailure();return R;}
        ScriptVariable->Init(Variable,FNiagaraVariableMetaData());Metadata.Add(Variable,ScriptVariable);
    }
    UEdGraphPin* Pin=Set->CreatePin(EGPD_Input,UEdGraphSchema_Niagara::TypeDefinitionToPinType(Type),FName(*Name));
    if(!Pin||!Loaded(ScriptVariable)||ScriptVariable->GetOuter()!=C.Graph)
    {Fail(R,TEXT("niagara_scratch_pin_metadata_creation_failed"),TEXT("Actual typed pin/graph-owned metadata allocation is incomplete; retained node/metadata."));ObserveFailure();return R;}
    Pin->PersistentGuid=FGuid::NewGuid();Pin->PinType.PinSubCategory=TEXT("ParameterPin");Pin->PinFriendlyName=FText::AsCultureInvariant(Name);
    Schema->TrySetDefaultValue(*Pin,DefaultText);OutputIn->BreakLinkTo(Previous);Previous->MakeLinkTo(In);Out->MakeLinkTo(OutputIn);
    Set->MarkNodeRequiresSynchronization(TEXT("CubelessScratchParameterWrite"),true);C.Graph->ConditionalRefreshParameterReferences();NotifyGraph(*C.Graph);C.System->MarkPackageDirty();
    const auto After=Capture(C.Graph);bool ParamsComplete=false;const auto Params=GraphParameters(C.Graph,ParamsComplete);
    bool PriorNodes=true,PriorPins=true;for(const auto& P:Before.Nodes){const auto* V=After.Nodes.Find(P.Key);PriorNodes&=V&&*V==P.Value;}
    for(const auto& P:Before.Pins){const auto* V=After.Pins.Find(P.Key);PriorPins&=V&&*V==P.Value;}
    auto Links=Before.Links;RemoveEdge(Links,PinId(Previous),PinId(OutputIn));AddEdge(Links,PinId(Previous),PinId(In));AddEdge(Links,PinId(Out),PinId(OutputIn));
    for(const auto& P:After.Pins){Links.FindOrAdd(P.Key);}bool EdgeUnion=Links.Num()==After.Links.Num();for(auto& P:Links){P.Value.Sort();const auto* V=After.Links.Find(P.Key);EdgeUnion&=V&&*V==P.Value;}
    bool MetadataBounded=ParamsComplete&&Params->Values.Num()==BeforeParameters->Values.Num()+static_cast<int32>(NewMetadata);
    for(const auto& P:BeforeParameters->Values){const auto* V=Params->Values.Find(P.Key);MetadataBounded&=V&&Text(P.Value->AsObject().ToSharedRef())==Text((*V)->AsObject().ToSharedRef());}
    const bool Identity=After.bComplete&&After.Nodes.Num()==Before.Nodes.Num()+1&&!Before.Nodes.Contains(Path(Set))&&Pin->DefaultValue==DefaultText&&Pin->LinkedTo.IsEmpty()&&
        UEdGraphSchema_Niagara::PinToTypeDefinition(Pin)==Type&&Pin->PersistentGuid.IsValid()&&ScriptVariable->Variable==Variable&&ScriptVariable->Metadata.GetVariableGuid().IsValid();
    R.bChanged=true;R.bChangeStateKnown=After.bComplete&&ParamsComplete;
    R.Data->SetBoolField(TEXT("prior_node_metadata_preserved"),PriorNodes);R.Data->SetBoolField(TEXT("prior_pin_metadata_preserved"),PriorPins);R.Data->SetBoolField(TEXT("exact_native_splice_verified"),EdgeUnion);
    R.Data->SetBoolField(TEXT("parameter_metadata_changes_bounded"),MetadataBounded);R.Data->SetBoolField(TEXT("new_write_identity_verified"),Identity);R.Data->SetBoolField(TEXT("metadata_created"),NewMetadata);
    R.Data->SetStringField(TEXT("parameter_name"),Pin->PinName.ToString());R.Data->SetStringField(TEXT("default_value"),Pin->DefaultValue);R.Data->SetObjectField(TEXT("parameter_type_metadata"),TypeRow(Type));
    R.Data->SetStringField(TEXT("parameter_map_set_node_name"),Set->GetName());R.Data->SetStringField(TEXT("parameter_map_set_node_guid"),Guid(Set->NodeGuid));R.Data->SetStringField(TEXT("parameter_map_set_node_path"),Path(Set));
    R.Data->SetStringField(TEXT("parameter_pin_id"),PinId(Pin));R.Data->SetStringField(TEXT("parameter_metadata_path"),Path(ScriptVariable));R.Data->SetStringField(TEXT("parameter_metadata_guid"),Guid(ScriptVariable->Metadata.GetVariableGuid()));
    R.Data->SetStringField(TEXT("previous_output_source_pin_id"),PinId(Previous));R.Data->SetNumberField(TEXT("previous_output_link_count"),1);R.Data->SetNumberField(TEXT("new_output_link_count"),OutputIn->LinkedTo.Num());
    R.Data->SetStringField(TEXT("write_scope"),TEXT("scratch_pad_parameter_map_set_write"));R.Data->SetObjectField(TEXT("graph_parameters_after"),Params);
    if(!PriorNodes||!PriorPins||!EdgeUnion||!MetadataBounded||!Identity){Fail(R,TEXT("niagara_scratch_write_postcondition_failed"),TEXT("Actual append differs from one typed map write and preserved prior graph/metadata; retained changes, no retry."));}
    Finish(R,C,C.Graph,After,bSave,bRequestCompile);return R;
}
Function::FResult Insert(const FString& TargetSystemPath,const FString& ScriptPath,const FString& OwnerKind,
    const FString& ScriptIndexJson,const FString& ScriptName,const FString& ScriptEmitterIndexJson,
    const FString& ScriptEmitterName,const FString& TargetUsage,const FString& TargetUsageId,
    const FString& TargetEmitterIndexJson,const FString& TargetEmitterName,const FString& TargetIndexJson,
    const FString& SuggestedName,bool bSkipIfDuplicate,bool bAllowSourceEdit,bool bSave,bool bRequestCompile)
{
    FResult R;Initialize(R,TargetSystemPath,bSave);FContext C;ENiagaraScriptUsage UsageValue;FGuid UsageId;int32 RequestedIndex;
    if(!Usage(TargetUsage,UsageValue)||(!TargetUsageId.IsEmpty()&&!FGuid::Parse(TargetUsageId,UsageId))||
        !Index(TargetIndexJson,RequestedIndex)||!Bounded(SuggestedName,256))
    {Fail(R,TEXT("niagara_scratch_stack_selector_invalid"),TEXT("Supported legacy usage, optional exact GUID, exact int32/null insertion index and bounded suggested name required."));return R;}
    if(!Base(R,C,TargetSystemPath,true,bAllowSourceEdit)||!Select(R,C,ScriptPath,OwnerKind,ScriptIndexJson,ScriptName,ScriptEmitterIndexJson,ScriptEmitterName,true)){return R;}
    FStack S;S.Module=C.Script;S.ModuleVersion=C.Script->IsVersioningEnabled()?C.Script->GetExposedVersion().VersionGuid:FGuid();
    const auto* Data=C.Script->GetScriptData(S.ModuleVersion);
    if(!Data||!UNiagaraScript::IsSupportedUsageContextForBitmask(Data->ModuleUsageBitmask,UsageValue))
    {Fail(R,TEXT("niagara_scratch_usage_incompatible"),TEXT("Selected private module version does not advertise the selected stack usage."));RejectAfterBaseline(R,C);return R;}
    UNiagaraScriptSource* StackSource=nullptr;int32 TargetEmitter=INDEX_NONE;FString StackOwnerName;FOwner TargetOwner;
    if(UsageValue==ENiagaraScriptUsage::SystemSpawnScript||UsageValue==ENiagaraScriptUsage::SystemUpdateScript)
    {auto* Script=UsageValue==ENiagaraScriptUsage::SystemSpawnScript?C.System->GetSystemSpawnScript():C.System->GetSystemUpdateScript();StackSource=Cast<UNiagaraScriptSource>(Script->GetLatestSource());StackOwnerName=C.System->GetName();}
    else
    {if(!Owner(R,C,TEXT("emitter"),TargetEmitterIndexJson,TargetEmitterName,TargetOwner,true)){RejectAfterBaseline(R,C);return R;}
        StackSource=Cast<UNiagaraScriptSource>(TargetOwner.Data->GraphSource);TargetEmitter=TargetOwner.Index;StackOwnerName=TargetOwner.Name;}
    if(!Ordinary(StackSource,S.Graph)||!StackSource->IsIn(C.System)||S.Graph==C.Graph)
    {Fail(R,TEXT("niagara_scratch_stack_source_invalid"),TEXT("Target must be an existing ordinary native system/emitter stack graph distinct from private module source."));RejectAfterBaseline(R,C);return R;}
    int32 Outputs=0;for(UEdGraphNode* N:S.Graph->Nodes)
    {if(auto* O=Cast<UNiagaraNodeOutput>(N);O&&UNiagaraScript::IsEquivalentUsage(O->GetUsage(),UsageValue)&&O->GetUsageId()==UsageId){S.Output=O;++Outputs;}}
    S.Before=Capture(S.Graph);if(Outputs!=1||!S.Before.bComplete||!Stack(R,S))
    {if(R.ErrorCode.IsEmpty()){Fail(R,TEXT("niagara_scratch_stack_output_invalid"),TEXT("Exactly one existing usage/GUID output and complete native reciprocal stack chain required."));}RejectAfterBaseline(R,C);return R;}
    BeforeGraph(R,S.Graph,S.Before);const auto ModuleBefore=Capture(C.Graph);R.Data->SetObjectField(TEXT("module_source_before"),ModuleBefore.Json);
    R.Data->SetStringField(TEXT("target_usage"),UsageName(UsageValue));R.Data->SetStringField(TEXT("target_usage_id"),Guid(UsageId));
    R.Data->SetNumberField(TEXT("target_emitter_index"),TargetEmitter);R.Data->SetStringField(TEXT("target_emitter_name"),StackOwnerName);
    R.Data->SetStringField(TEXT("module_script_path"),Path(C.Script));R.Data->SetStringField(TEXT("module_script_version"),Guid(S.ModuleVersion));
    R.Data->SetArrayField(TEXT("ordered_modules"),ModuleRows(S));R.Data->SetNumberField(TEXT("module_count"),S.Modules.Num());R.Data->SetBoolField(TEXT("skipped_duplicate"),false);
    if(bSkipIfDuplicate)
    {for(auto* M:S.Modules){if(M->FunctionScript==C.Script)
        {R.Data->SetBoolField(TEXT("skipped_duplicate"),true);R.Data->SetBoolField(TEXT("save_requested"),false);R.Data->SetStringField(TEXT("existing_module_node_guid"),Guid(M->NodeGuid));
            R.Data->SetStringField(TEXT("existing_module_node_path"),Path(M));R.Data->SetObjectField(TEXT("module"),NodeMetadata(M));const auto Current=Capture(S.Graph);
            R.Data->SetObjectField(TEXT("after"),Current.Json);R.Data->SetObjectField(TEXT("graph_after"),Current.Json);const bool Same=SameGraph(S.Before,Current)&&C.Dirty==C.System->GetOutermost()->IsDirty();R.Data->SetBoolField(TEXT("state_preserved"),Same);
            if(!Current.bComplete){R.bChangeStateKnown=false;}if(!Same){Fail(R,TEXT("niagara_scratch_duplicate_skip_changed"),TEXT("Private duplicate skip changed observed target state."));}
            bool ParamsComplete=false;const auto ParamsAfter=GraphParameters(S.Graph,ParamsComplete);R.Data->SetObjectField(TEXT("graph_parameters_after"),ParamsAfter);
            const bool ParamsSame=ParamsComplete&&Text(ParamsAfter)==Text(R.Data->GetObjectField(TEXT("graph_parameters_before")).ToSharedRef());
            R.Data->SetBoolField(TEXT("graph_parameters_preserved"),ParamsSame);if(!ParamsComplete){R.bChangeStateKnown=false;}
            if(!ParamsSame){Fail(R,TEXT("niagara_scratch_duplicate_skip_parameters_changed"),TEXT("Actual read-after target metadata differs during known-zero duplicate observation."));}
            Preserve(R,C,nullptr);R.Data->SetObjectField(TEXT("module_source_after"),Capture(C.Graph).Json);R.Data->SetBoolField(TEXT("module_source_preserved"),R.Data->GetBoolField(TEXT("source_graphs_preserved")));Complete(R);return R;}}}
    const int32 Resolved=RequestedIndex==INDEX_NONE?S.Modules.Num():FMath::Clamp(RequestedIndex,0,S.Modules.Num());
    UEdGraphNode* Previous=Resolved==0?static_cast<UEdGraphNode*>(S.Input):static_cast<UEdGraphNode*>(S.Modules[Resolved-1]);UEdGraphPin* PreviousOut=nullptr;
    if(!MapPin(Previous,EGPD_Output,PreviousOut)||!PreviousOut||PreviousOut->LinkedTo.IsEmpty())
    {Fail(R,TEXT("niagara_scratch_splice_boundary_invalid"),TEXT("Private insertion boundary must be observable before exported native helper dispatch."));RejectAfterBaseline(R,C);return R;}
    TArray<FString> Targets;for(const auto* P:PreviousOut->LinkedTo){Targets.Add(PinId(P));}Targets.Sort();const auto PriorModules=S.Modules;
    R.Data->SetNumberField(TEXT("target_index"),RequestedIndex);R.Data->SetNumberField(TEXT("resolved_target_index"),Resolved);R.Stage=TEXT("insert_private_module");R.bMutationAttempted=true;R.bChangeStateKnown=false;
    R.Data->SetNumberField(TEXT("stack_insert_requests"),1);C.System->Modify();S.Graph->Modify();S.Output->Modify();
    auto* Module=FNiagaraStackGraphUtilities::AddScriptModuleToStack(C.Script,*S.Output,Resolved,SuggestedName,S.ModuleVersion);
    const auto After=Capture(S.Graph);R.Data->SetBoolField(TEXT("native_module_returned"),Module!=nullptr);R.bChangeStateKnown=After.bComplete;R.bChanged=After.bComplete&&!SameGraph(S.Before,After);
    if(!Loaded(Module)){Fail(R,TEXT("niagara_scratch_private_insert_failed"),TEXT("Exported insertion did not return an observable owned function call; actual partial graph retained."));}
    else
    {
        R.Data->SetStringField(TEXT("new_module_node_guid"),Guid(Module->NodeGuid));R.Data->SetStringField(TEXT("new_module_node_name"),Module->GetName());R.Data->SetStringField(TEXT("new_module_function_name"),Module->GetFunctionName());
        R.Data->SetStringField(TEXT("new_module_node_path"),Path(Module));R.Data->SetObjectField(TEXT("module"),NodeMetadata(Module));VerifyInsertion(R,S,Module,After,Resolved,PinId(PreviousOut),Targets,PriorModules);
    }
    R.Data->SetStringField(TEXT("write_scope"),TEXT("target_local_scratch_pad_stack_insert"));Finish(R,C,S.Graph,After,bSave,bRequestCompile,FString(),nullptr,true);
    if(R.Data->HasField(TEXT("projection_skipped_unsafe"))){return R;}
    const auto ModuleAfter=Capture(C.Graph);R.Data->SetObjectField(TEXT("module_source_after"),ModuleAfter.Json);
    R.Data->SetBoolField(TEXT("module_source_preserved"),SameGraph(ModuleBefore,ModuleAfter)&&R.Data->GetBoolField(TEXT("source_graphs_preserved")));
    R.Data->SetBoolField(TEXT("target_metadata_producer_copy_performed"),false);
    return R;
}
Function::FResult Hlsl(const FString& SystemPath,const FString& TargetPinName,const FString& Expression,
    const FString& ScriptPath,const FString& OwnerKind,const FString& ScriptIndexJson,const FString& ScriptName,
    const FString& EmitterIndexJson,const FString& EmitterName,const FString& TargetNodeGuid,const FString& TargetNodeName,
    const FString& TargetNodeIndexJson,const FString& PreserveExistingLinkAs,const FString& OutputPinName,const FString& SignatureName,
    const FString& CustomHlslScriptUsage,const FString& InputsJson,const FString& UserParameterInputsJson,
    bool bSkipIfAlreadyInserted,bool bReplaceExistingCustom,bool bRebuildExistingCustom,bool bRequiresContext,
    bool bDeleteUnlinkedCustomNodes,bool bDeleteUnlinkedCustomInputSourceNodes,bool bAllowSourceEdit,bool bSave,
    bool bRequestCompile,const FString& ParameterInputsJson)
{
    FResult R;Initialize(R,SystemPath,bSave);R.Data->SetStringField(TEXT("write_scope"),TEXT("scratch_pad_custom_hlsl_pin_insert"));
    R.Data->SetStringField(TEXT("dependency_layer"),TEXT("EngineAPI + finite public ToolsetLibrary property access; no ServerRPC"));
    R.Data->SetBoolField(TEXT("custom_hlsl_attachment_verified"),false);R.Data->SetBoolField(TEXT("full_position_producer_verified"),false);
    R.Data->SetBoolField(TEXT("protected_pair_storage_independently_read"),false);R.Data->SetNumberField(TEXT("custom_hlsl_insert_requests"),0);
    R.Data->SetBoolField(TEXT("parameter_inputs_additive_public_attachment"),true);R.Data->SetBoolField(TEXT("already_inserted"),false);
    int32 NodeIndex=INDEX_NONE;const FString Code=Expression.TrimStartAndEnd();const FString UsageToken=Token(CustomHlslScriptUsage);
    ENiagaraScriptUsage HlslUsage=ENiagaraScriptUsage::Function;
    TArray<TSharedPtr<FJsonValue>> Extra,Users,Parameters;
    if(!HlslExpression(Code)||!HlslIdentifier(OutputPinName)||!HlslIdentifier(SignatureName)||!Bounded(TargetPinName,256)||TargetPinName.IsEmpty()||
        !Bounded(TargetNodeGuid,64)||!Bounded(TargetNodeName,256)||!Index(TargetNodeIndexJson,NodeIndex))
    {Fail(R,TEXT("niagara_hlsl_request_invalid"),TEXT("Bounded nonempty expression, identifiers and exact int32/null selectors are required."));return R;}
    if(UsageToken==TEXT("dynamicinput")||UsageToken==TEXT("dynamicinputscript")){HlslUsage=ENiagaraScriptUsage::DynamicInput;}
    else if(UsageToken!=TEXT("function")&&UsageToken!=TEXT("functionscript"))
    {Fail(R,TEXT("niagara_hlsl_usage_unsupported"),TEXT("Only actual Function or DynamicInput usage is implemented."));return R;}
    const bool ExistingEditRequested=bReplaceExistingCustom||bRebuildExistingCustom||bDeleteUnlinkedCustomNodes||bDeleteUnlinkedCustomInputSourceNodes;
    if(ExistingHlslEdit){R.bChangeStateKnown=false;Fail(R,TEXT("niagara_hlsl_reentrant"),TEXT("An existing-wrapper edit is active; no target observation or writer."));return R;}
    if(!HlslRows(R,InputsJson,Extra)||!HlslRows(R,UserParameterInputsJson,Users)||!HlslRows(R,ParameterInputsJson,Parameters)){return R;}
    FContext C;if(!Base(R,C,SystemPath,true,bAllowSourceEdit)||!Select(R,C,ScriptPath,OwnerKind,ScriptIndexJson,ScriptName,EmitterIndexJson,EmitterName,true)){return R;}
    TUniquePtr<FHNWriterGuard> UserGuard;if(!Users.IsEmpty()||ExistingEditRequested){UserGuard=MakeUnique<FHNWriterGuard>(R,C,C.Graph);if(!UserGuard->Valid){Complete(R);return R;}}
    TGuardValue<FHNWriterGuard*> HNUserScope(HNWriter,UserGuard?UserGuard.Get():HNWriter);
    TUniquePtr<FUKHlslEdit> EditPlan;TGuardValue<FUKHlslEdit*> HlslEditScope(ExistingHlslEdit,nullptr);
    if(!HNAdmit(TEXT("before_primitive_User_HLSL"))){Complete(R);return R;}
    const auto Before=Capture(C.Graph);if(!HNAdmit(TEXT("after_HLSL_baseline"))){Complete(R);return R;}BeforeGraph(R,C.Graph,Before);bool PComplete=false;const auto PriorParameters=GraphParameters(C.Graph,PComplete);
    const auto* Schema=Cast<UEdGraphSchema_Niagara>(C.Graph->GetSchema());
    auto Reject=[&](){HlslFailure(R,C,Before,PriorParameters);};
    if(!Before.bComplete||!PComplete||!Schema){Fail(R,TEXT("niagara_hlsl_graph_unobservable"),TEXT("Selected complete native source/schema and typed metadata must be observable."));Reject();return R;}
    // Original target selection filters nodes first, then matches input pins across
    // those nodes. Optional node selectors are not required for one matching pin.
    FGuid ParsedTargetGuid;if(!TargetNodeGuid.IsEmpty()&&!FGuid::Parse(TargetNodeGuid,ParsedTargetGuid))
    {Fail(R,TEXT("niagara_hlsl_node_selector_invalid"),TEXT("Node GUID is invalid."));Reject();return R;}
    UEdGraphNode* Target=nullptr;UEdGraphPin* TargetPin=nullptr;int32 TargetMatchCount=0;
    for(int32 CandidateIndex=0;CandidateIndex<C.Graph->Nodes.Num();++CandidateIndex)
    {
        UEdGraphNode* CandidateNode=C.Graph->Nodes[CandidateIndex].Get();if(!CandidateNode)continue;
        if((NodeIndex!=INDEX_NONE&&NodeIndex!=CandidateIndex)||
            (!TargetNodeGuid.IsEmpty()&&CandidateNode->NodeGuid!=ParsedTargetGuid)||
            (!TargetNodeName.IsEmpty()&&!TargetNodeName.Equals(CandidateNode->GetName(),ESearchCase::IgnoreCase)))continue;
        for(UEdGraphPin* CandidatePin:CandidateNode->Pins)
        {
            if(!CandidatePin||CandidatePin->Direction!=EGPD_Input)continue;
            const FString CandidatePinName=CandidatePin->PinName.ToString();
            const bool PinMatches=CandidatePinName.Equals(TargetPinName,ESearchCase::IgnoreCase)||
                TargetPinName.EndsWith(FString::Printf(TEXT(".%s"),*CandidatePinName),ESearchCase::IgnoreCase)||
                CandidatePinName.EndsWith(FString::Printf(TEXT(".%s"),*TargetPinName),ESearchCase::IgnoreCase);
            if(PinMatches){++TargetMatchCount;if(!Target){Target=CandidateNode;TargetPin=CandidatePin;}}
        }
    }
    if(!Target||!TargetPin)
    {Fail(R,TEXT("niagara_hlsl_target_not_found"),*FString::Printf(TEXT("No matching Scratch Pad input pin was found: %s"),*TargetPinName));Reject();return R;}
    if(TargetMatchCount>1&&NodeIndex==INDEX_NONE&&TargetNodeGuid.IsEmpty()&&TargetNodeName.IsEmpty())
    {Fail(R,TEXT("niagara_hlsl_target_selector_ambiguous"),*FString::Printf(TEXT("Target selector matched %d pins. Provide target_node_index, target_node_guid, or target_node_name."),TargetMatchCount));Reject();return R;}
    if(!ExactNodeClass(Target,TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapSet"))||!TargetPin||TargetPin->bOrphanedPin||TargetPin->bNotConnectable||
        !HlslValueType(UEdGraphSchema_Niagara::PinToTypeDefinition(TargetPin))||TargetPin->LinkedTo.Num()>1)
    {Fail(R,TEXT("niagara_hlsl_target_unsupported"),TEXT("First cohort requires one concrete typed native MapSet value input, with at most one prior source."));Reject();return R;}
    const auto TargetType=UEdGraphSchema_Niagara::PinToTypeDefinition(TargetPin);
    UEdGraphPin* MapIn=nullptr;UEdGraphPin* MapSource=nullptr;
    if(!MapPin(Target,EGPD_Input,MapIn)||!MapIn||MapIn->LinkedTo.Num()!=1)
    {Fail(R,TEXT("niagara_hlsl_map_source_invalid"),TEXT("Selected MapSet needs one existing reciprocal ParameterMap predecessor."));Reject();return R;}
    MapSource=MapIn->LinkedTo[0];auto* Existing=TargetPin->LinkedTo.IsEmpty()?nullptr:Cast<UNiagaraNodeCustomHlsl>(TargetPin->LinkedTo[0]->GetOwningNode());
    if(Existing||ExistingEditRequested)
    {
        if(!HNWriter){UserGuard=MakeUnique<FHNWriterGuard>(R,C,C.Graph);HNWriter=UserGuard.Get();if(!UserGuard->Valid){Complete(R);return R;}}
        EditPlan=MakeUnique<FUKHlslEdit>(*HNWriter,C.Graph);if(!EditPlan->Bind()){Complete(R);return R;}ExistingHlslEdit=EditPlan.Get();
        R.Data->SetNumberField(TEXT("removed_unlinked_custom_node_count"),0);R.Data->SetNumberField(TEXT("removed_unlinked_custom_input_source_node_count"),0);R.Data->SetNumberField(TEXT("replaced_custom_node_count"),0);
    }
    const FString PreservedInputName=EditPlan?PreserveExistingLinkAs.TrimStartAndEnd():PreserveExistingLinkAs;
    if(EditPlan&&PreservedInputName.IsEmpty()){Fail(R,TEXT("niagara_hlsl_preserve_input_required"),TEXT("Original preserve_existing_link_as must be nonempty before any edit."));Reject();return R;}
    if(EditPlan&&!HlslIdentifier(PreservedInputName)){Fail(R,TEXT("niagara_hlsl_preserve_input_pending"),TEXT("Nonempty non-identifier preserved names remain valid original scope incomplete in this finite edit profile; no writer."));Reject();return R;}
    TSet<UEdGraphNode*> ExistingInputSources;UEdGraphPin* PreservedBase=nullptr;
    if(Existing)
    {
        if(Existing->GetClass()!=UNiagaraNodeCustomHlsl::StaticClass()){Fail(R,TEXT("niagara_hlsl_existing_class_pending"),TEXT("Existing custom subclasses remain valid incomplete."));Reject();return R;}
        auto* PreservedInput=NamedPin(Existing,EGPD_Input,PreservedInputName);
        if(PreservedInput&&PreservedInput->LinkedTo.Num()==1)PreservedBase=PreservedInput->LinkedTo[0];
        const auto ExistingPins=Existing->Pins;for(auto* Pin:ExistingPins)if(Pin->Direction==EGPD_Input)for(auto* Linked:Pin->LinkedTo)
        {auto* SourceNode=Linked->GetOwningNode();if(SourceNode->GetClass()==UNiagaraNodeInput::StaticClass()||ExactNodeClass(SourceNode,TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet")))ExistingInputSources.Add(SourceNode);}
    }
    if(bDeleteUnlinkedCustomInputSourceNodes)for(auto* InputSource:ExistingInputSources)
    {if(InputSource->GetClass()==UNiagaraNodeInput::StaticClass()&&!HlslValueType(static_cast<UNiagaraNodeInput*>(InputSource)->Input.GetType())){Fail(R,TEXT("niagara_hlsl_cleanup_input_type_pending"),TEXT("Input-source cleanup currently admits primitive native sources, not DI/object ownership variants."));Reject();return R;}}
    FProperty* TextProperty=FindFProperty<FProperty>(UNiagaraNodeCustomHlsl::StaticClass(),TEXT("CustomHlsl"));
    if(!CastField<FStrProperty>(TextProperty)||TextProperty->ArrayDim!=1||
        EnumHasAnyFlags(PropertyAccessUtil::CanGetPropertyValue(TextProperty),EPropertyAccessResultFlags::PermissionDenied)||
        EnumHasAnyFlags(PropertyAccessUtil::CanSetPropertyValue(TextProperty,PropertyAccessUtil::EditorReadOnlyFlags,false),EPropertyAccessResultFlags::PermissionDenied))
    {Fail(R,TEXT("niagara_hlsl_text_property_inaccessible"),TEXT("Exact editor-accessible CustomHlsl text property must be readable/writable before allocation."));Reject();return R;}
    TArray<FHlslInput> Specs;TSet<FString> Names;TSet<FString> NewMetadataNames;
    auto AddSpec=[&](const FString& Name,const FNiagaraTypeDefinition& Type,UEdGraphPin* Source,const FString& Parameter)->bool
    {if(!HlslIdentifier(Name)||Names.Contains(Name.ToLower())||Specs.Num()>=32||(!HlslValueType(Type)&&Type!=FNiagaraTypeDefinition::GetParameterMapDef()))
        {return Fail(R,TEXT("niagara_hlsl_inputs_invalid"),TEXT("Input names must be unique identifiers with concrete finite native types."));}
        Names.Add(Name.ToLower());FHlslInput S;S.Name=Name;S.Type=Type;S.Source=Source;S.ParameterName=Parameter;Specs.Add(S);return true;};
    if(bRequiresContext&&!AddSpec(TEXT("Map"),FNiagaraTypeDefinition::GetParameterMapDef(),MapSource,TEXT(""))){Reject();return R;}
    if(!PreservedInputName.IsEmpty()&&((Existing&&PreservedBase)||(!Existing&&!TargetPin->LinkedTo.IsEmpty()))&&
        !AddSpec(PreservedInputName,TargetType,Existing?PreservedBase:TargetPin->LinkedTo[0],TEXT(""))){Reject();return R;}
    for(const auto& V:Extra)
    {
        const auto O=V->AsObject();FString Name,PinName,Direction=TEXT("output"),Id,NodeName;int32 SourceIndex=INDEX_NONE;
        O->TryGetStringField(TEXT("input_name"),Name);O->TryGetStringField(TEXT("source_pin_name"),PinName);O->TryGetStringField(TEXT("source_pin_direction"),Direction);
        O->TryGetStringField(TEXT("source_node_guid"),Id);O->TryGetStringField(TEXT("source_node_name"),NodeName);
        if(O->HasField(TEXT("source_node_index")))
        {FString Encoded;FJsonSerializer::Serialize(O->TryGetField(TEXT("source_node_index")).ToSharedRef(),TEXT(""),TJsonWriterFactory<>::Create(&Encoded));
            if(!Index(Encoded,SourceIndex)){Fail(R,TEXT("niagara_hlsl_inputs_invalid"),TEXT("Source node index must be exact int32/null."));Reject();return R;}}
        if((Id.IsEmpty()&&NodeName.IsEmpty()&&SourceIndex==INDEX_NONE)||Token(Direction)!=TEXT("output"))
        {Fail(R,TEXT("niagara_hlsl_source_unsupported"),TEXT("Extra input needs an exact existing output-node selector; input-pin-as-source variants are incomplete."));Reject();return R;}
        auto* N=HlslNode(R,C.Graph,Id,NodeName,SourceIndex);auto* Pin=N?NamedPin(N,EGPD_Output,PinName):nullptr;
        if(!Pin||Pin->bNotConnectable||!AddSpec(Name,UEdGraphSchema_Niagara::PinToTypeDefinition(Pin),Pin,TEXT("")))
        {if(R.ErrorCode.IsEmpty()){Fail(R,TEXT("niagara_hlsl_source_unsupported"),TEXT("Existing source must have one concrete typed connectable output."));}Reject();return R;}
    }
    for(const auto& V:Parameters)
    {
        const auto O=V->AsObject();FString Name,Parameter,TypeText;FNiagaraTypeDefinition Type;
        O->TryGetStringField(TEXT("input_name"),Name);if(!O->TryGetStringField(TEXT("parameter_name"),Parameter)){O->TryGetStringField(TEXT("linked_parameter_name"),Parameter);}
        if(!O->TryGetStringField(TEXT("type"),TypeText)){O->TryGetStringField(TEXT("parameter_type"),TypeText);}
        if(!LiteralType(TypeText,Type)||!HlslValueType(Type)||Parameter!=TEXT("Module.")+Name||
            !HlslMetadataAdmit(R,C.Graph,FNiagaraVariable(Type,FName(*Parameter)))||!AddSpec(Name,Type,nullptr,Parameter))
        {if(R.ErrorCode.IsEmpty()){Fail(R,TEXT("niagara_hlsl_parameter_source_incomplete"),TEXT("First cohort attaches exact Module.<input_name> primitive/Position inputs. System/Engine/User/DI source binding and cross-usage dominance remain incomplete."));}Reject();return R;}
        NewMetadataNames.Add(Parameter);
    }
    // Original Part4:879..934 plans the ordered User rows before graph writes;
    // Shared:3263 admits these eight value types plus separate DI/UObject cases.
    // Use the existing store's full GetParameters keys, not GetUserParameters:
    // UE58 returns short redirect keys there (.h32/.cpp46..68). Both planning
    // and callback checks compare copied canonical User.* values, as before.
    // The existing map-read/pair writer below already iterates every typed spec.
    TArray<TSharedPtr<FJsonValue>> UserPlans;bool SingleBoolUser=false;
    if(!Users.IsEmpty()&&!HNAdmit(TEXT("before_HLSL_user_declaration_plan"))){Reject();return R;}
    TArray<FNiagaraVariable> UserDeclarations;if(!Users.IsEmpty())C.System->GetExposedParameters().GetParameters(UserDeclarations);
    if(UserDeclarations.Num()>MaxParameters){Fail(R,TEXT("niagara_hlsl_user_declaration_bound"),TEXT("User declarations exceed the existing finite store budget; no writer."));Reject();return R;}
    for(const auto& UserValue:Users)
    {
        const auto UserRow=UserValue->AsObject();FString UserInputName,UserName;UserRow->TryGetStringField(TEXT("input_name"),UserInputName);
        if(!UserRow->TryGetStringField(TEXT("user_parameter_name"),UserName))UserRow->TryGetStringField(TEXT("parameter_name"),UserName);
        UserInputName.TrimStartAndEndInline();UserName.TrimStartAndEndInline();if(!UserName.StartsWith(TEXT("User.")))UserName=TEXT("User.")+UserName;
        if(!HlslIdentifier(UserInputName)||!Bounded(UserName,256)||UserName.Len()<=5){Fail(R,TEXT("niagara_hlsl_user_input_invalid"),TEXT("Existing normalized full User name and finite input identifier are required."));Reject();return R;}
        const FName RequestedName(*UserName);const FNiagaraVariable* Matched=nullptr;int32 Matches=0;
        for(const auto& Declared:UserDeclarations)if(Declared.GetName()==RequestedName){Matched=&Declared;++Matches;}
        if(Matches!=1){Fail(R,TEXT("niagara_hlsl_user_canonical_mismatch"),TEXT("Full typed User declaration is absent/ambiguous; no writer dispatched."));Reject();return R;}
        if(!HlslValueType(Matched->GetType())){Fail(R,TEXT("niagara_hlsl_user_type_incomplete"),TEXT("Ordered existing Float/Int/Bool/Color/Vec2/Vec3/Vec4/Position User inputs are supported; original ArrayDI/UObject inputs still require separate source roles and remain incomplete before writer."));Reject();return R;}
        const FNiagaraVariable RequestedUser(Matched->GetType(),RequestedName);const auto& Store=C.System->GetExposedParameters();FNiagaraVariable Redirected=RequestedUser;const bool RedirectedOk=Store.RedirectUserVariable(Redirected);
        if(!RedirectedOk||Redirected!=RequestedUser||!HNKeepType(RequestedUser.GetType())||!HlslMetadataAdmit(R,C.Graph,RequestedUser)||!AddSpec(UserInputName,RequestedUser.GetType(),nullptr,UserName))
        {if(R.ErrorCode.IsEmpty())Fail(R,TEXT("niagara_hlsl_user_canonical_mismatch"),TEXT("Full typed User declaration redirects to another variable or cannot be admitted; no writer dispatched."));Reject();return R;}
        // Repeated reads of one User under distinct input names are valid. Guard
        // each actual source once while keeping request/spec/pair order unchanged.
        if(HNWriter)HNWriter->HlslUserDeclarations.AddUnique(RequestedUser);
        NewMetadataNames.Add(UserName);if(RequestedUser.GetType()==FNiagaraTypeDefinition::GetBoolDef()){R.Data->SetStringField(TEXT("selected_bool_user_parameter"),UserName);SingleBoolUser=Users.Num()==1;}
        auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("request_index"),UserPlans.Num());Row->SetStringField(TEXT("input_name"),UserInputName);Row->SetStringField(TEXT("user_parameter_name"),UserName);Row->SetObjectField(TEXT("type"),TypeRow(RequestedUser.GetType()));UserPlans.Add(MakeShared<FJsonValueObject>(Row));R.Data->SetArrayField(TEXT("selected_user_parameter_inputs"),UserPlans);
    }
    if(!Users.IsEmpty()&&!HNAdmit(TEXT("after_HLSL_user_declaration_plan"))){Reject();return R;}
    R.Data->SetStringField(TEXT("user_input_scope"),TEXT("Ordered existing primitive8 User inputs share the original concrete HLSL signature/MapGet pair/reuse/rebuild/cleanup and actual compile/save/partial paths; total signature budget32 remains. User declarations are current-checked at SDK boundaries without retaining store data pointers; final UserState value preservation is unchanged. ArrayDI/UObject, broader name aliases/source owners, callback variants and full original/runtime behavior remain incomplete or usage validation pending."));
    if(Existing)
    {
        FString ExistingText;bool Match=bSkipIfAlreadyInserted&&Existing->GetClass()==UNiagaraNodeCustomHlsl::StaticClass()&&
            HlslText(Existing,ExistingText)&&ExistingText.TrimStartAndEnd()==Code&&Existing->ScriptUsage==HlslUsage&&
            Existing->Signature.Name==FName(*SignatureName)&&Existing->Signature.bRequiresContext==bRequiresContext&&
            Existing->Signature.Inputs.Num()==Specs.Num()&&Existing->Signature.Outputs.Num()==1;
        if(!HNAdmit(TEXT("after_existing_HLSL_read"))){Reject();return R;}
        auto* Out=NamedPin(Existing,EGPD_Output,OutputPinName);Match&=Out&&TargetPin->LinkedTo[0]==Out&&UEdGraphSchema_Niagara::PinToTypeDefinition(Out)==TargetType;
        if(Match){Match&=Existing->Signature.Outputs[0].GetName()==FName(*OutputPinName)&&Existing->Signature.Outputs[0].GetType()==TargetType;}
        for(int32 I=0;I<Specs.Num()&&Match;++I)
        {const auto& S=Specs[I];auto* Pin=NamedPin(Existing,EGPD_Input,S.Name);Match&=Existing->Signature.Inputs[I].GetName()==FName(*S.Name)&&
            Existing->Signature.Inputs[I].GetType()==S.Type&&Pin&&Pin->LinkedTo.Num()==1;
            if(Match){auto* Source=Pin->LinkedTo[0];if(S.ParameterName.IsEmpty()){Match&=Source==S.Source;}
                else{UEdGraphPin* ReadMap=nullptr;Match&=ExactNodeClass(Source->GetOwningNode(),TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"))&&
                    Source->PinName==FName(*S.ParameterName)&&UEdGraphSchema_Niagara::PinToTypeDefinition(Source)==S.Type&&
                    MapPin(Source->GetOwningNode(),EGPD_Input,ReadMap)&&ReadMap&&ReadMap->LinkedTo.Num()==1&&ReadMap->LinkedTo[0]==MapSource;}}}
        const bool SameExpression=ExistingText.TrimStartAndEnd()==Code;
        const bool ExplicitInputs=InputsJson.TrimStartAndEnd()!=TEXT("null"),ExplicitParameters=ParameterInputsJson.TrimStartAndEnd()!=TEXT("null");
        bool OriginalTopology=Existing->ScriptUsage==HlslUsage&&!ExplicitInputs&&!ExplicitParameters&&PreservedBase;
        for(const auto& UserValue:Users)
        {
            const auto UserRow=UserValue->AsObject();FString RequestedInput,RequestedUser;UserRow->TryGetStringField(TEXT("input_name"),RequestedInput);if(!UserRow->TryGetStringField(TEXT("user_parameter_name"),RequestedUser))UserRow->TryGetStringField(TEXT("parameter_name"),RequestedUser);
            RequestedInput.TrimStartAndEndInline();RequestedUser.TrimStartAndEndInline();if(!RequestedUser.StartsWith(TEXT("User.")))RequestedUser=TEXT("User.")+RequestedUser;
            auto* ExistingUserPin=NamedPin(Existing,EGPD_Input,RequestedInput);UNiagaraNodeInput* ExistingUserSource=nullptr;
            if(ExistingUserPin&&ExistingUserPin->LinkedTo.Num()==1&&ExistingUserPin->LinkedTo[0]->GetOwningNode()->GetClass()==UNiagaraNodeInput::StaticClass())ExistingUserSource=static_cast<UNiagaraNodeInput*>(ExistingUserPin->LinkedTo[0]->GetOwningNode());
            FString ActualUser=ExistingUserSource?ExistingUserSource->Input.GetName().ToString():FString();if(!ActualUser.IsEmpty()&&!ActualUser.StartsWith(TEXT("User.")))ActualUser=TEXT("User.")+ActualUser;
            OriginalTopology&=ExistingUserSource&&ActualUser.Equals(RequestedUser,ESearchCase::IgnoreCase);
        }
        // Additional strict proof conditions must never authorize a removal that
        // the original same-code no-op branch would not have selected.
        const bool CurrentEquivalentSkip=Match&&!bRebuildExistingCustom&&!bDeleteUnlinkedCustomInputSourceNodes&&!ExplicitInputs&&!ExplicitParameters;
        R.Data->SetBoolField(TEXT("current_equivalent_user_mapget_skip_preserved"),CurrentEquivalentSkip&&!OriginalTopology&&!Users.IsEmpty());R.Data->SetBoolField(TEXT("current_equivalent_bool_mapget_skip_preserved"),SingleBoolUser&&CurrentEquivalentSkip&&!OriginalTopology);
        if(bSkipIfAlreadyInserted&&SameExpression&&!CurrentEquivalentSkip&&(!OriginalTopology||bRebuildExistingCustom||bDeleteUnlinkedCustomInputSourceNodes))
        {bReplaceExistingCustom=true;bRebuildExistingCustom=true;R.Data->SetBoolField(TEXT("automatic_skip_to_rebuild"),true);}
        else if(bSkipIfAlreadyInserted&&SameExpression)
        {
            if(!Match){Fail(R,TEXT("niagara_hlsl_original_skip_proof_pending"),TEXT("Original same-code skip selected, but additional finite signature/context proof is incomplete; no rebuild or writer."));Reject();return R;}
            R.Data->SetBoolField(TEXT("already_inserted"),true);R.Data->SetBoolField(TEXT("custom_hlsl_attachment_verified"),true);
            R.Data->SetStringField(TEXT("custom_node_guid"),Guid(Existing->NodeGuid));R.Data->SetStringField(TEXT("custom_hlsl_node_guid"),Guid(Existing->NodeGuid));R.Data->SetNumberField(TEXT("created_user_parameter_input_node_count"),0);R.Data->SetNumberField(TEXT("user_parameter_input_count"),Users.Num());R.Data->SetStringField(TEXT("custom_node_path"),Path(Existing));R.Data->SetBoolField(TEXT("save_requested"),false);
            Reject();return R;
        }
        if(!bReplaceExistingCustom){Fail(R,TEXT("niagara_hlsl_existing_replace_required"),TEXT("Target already has Custom HLSL; set replace_existing_custom=true to replace it."));Reject();return R;}
        if(!bRebuildExistingCustom)
        {
            // Original in-place mode retains signature names/inputs. Type-changing
            // defaults and orphaned-pin variants remain explicit valid pending.
            auto UpdatedSignature=Existing->Signature;UpdatedSignature.bRequiresContext=bRequiresContext;
            for(const auto& Input:UpdatedSignature.Inputs)
            {
                FNiagaraTypeDefinition OriginalResolvedType=Input.GetType();const FString InputLabel=Input.GetName().ToString();
                if(InputLabel.Equals(TEXT("Map"),ESearchCase::IgnoreCase))OriginalResolvedType=FNiagaraTypeDefinition::GetParameterMapDef();
                else if(InputLabel.Equals(PreservedInputName,ESearchCase::IgnoreCase)||InputLabel.Equals(TEXT("BaseValue"),ESearchCase::IgnoreCase))OriginalResolvedType=TargetType;
                else if(InputLabel.Equals(TEXT("StampScale"),ESearchCase::IgnoreCase))OriginalResolvedType=FNiagaraTypeDefinition::GetFloatDef();
                if(OriginalResolvedType!=Input.GetType()||(Input.GetType()!=FNiagaraTypeDefinition::GetParameterMapDef()&&!HlslValueType(Input.GetType())))
                {Fail(R,TEXT("niagara_hlsl_existing_type_pending"),TEXT("Type-changing/default reconstruction remains incomplete; native same-type in-place signature required before Modify."));Reject();return R;}
            }
            for(const auto& Output:UpdatedSignature.Outputs)if(Output.GetType()!=TargetType)
            {Fail(R,TEXT("niagara_hlsl_existing_type_change_pending"),TEXT("Original type-changing in-place reconstruction remains incomplete; no Modify."));Reject();return R;}
            R.Stage=TEXT("update_existing_custom_hlsl");R.Data->SetBoolField(TEXT("updated_existing_custom"),false);R.Data->SetStringField(TEXT("write_scope"),TEXT("scratch_pad_custom_hlsl_pin_update"));
            for(UObject* Object:TArray<UObject*>{C.System,C.Script,C.Source,C.Graph,Existing})
            {if(!HNDispatch(TEXT("Modify_existing_HLSL_owner"),true)){Reject();return R;}Object->Modify();if(!HNAdmit(TEXT("after_Modify_existing_HLSL_owner"))){Reject();return R;}}
            auto& Planned=EditPlan->Rows.FindChecked(Existing);Planned.Signature=UpdatedSignature;Planned.Usage=HlslUsage;EditPlan->ChangedNodeKeys.Add(Planned.Key);
            Existing->Signature=UpdatedSignature;Existing->ScriptUsage=HlslUsage;
            if(!HNDispatch(TEXT("Reconstruct_existing_HLSL"),true)){Reject();return R;}EditPlan->OpenReconstruction(Existing);
            static_cast<UEdGraphNode*>(Existing)->ReconstructNode();R.Data->SetBoolField(TEXT("existing_reconstruct_returned"),true);
            if(!HNAdmit(TEXT("after_Reconstruct_existing_HLSL"))||!EditPlan->Rebind(Existing)){Reject();return R;}
            if(!HNDispatch(TEXT("Set_existing_HLSL_text"),true)){Reject();return R;}EditPlan->OpenReconstruction(Existing);Planned.Hlsl=Code;
            auto Values=MakeShared<FJsonObject>();Values->SetStringField(TEXT("CustomHlsl"),Code);TArray<FName> Written;
            const bool SetReturned=UToolsetLibrary::SetObjectProperties(Existing,Text(Values),Written,EBypassContainerCheck::No);R.Data->SetBoolField(TEXT("custom_hlsl_text_set_returned"),SetReturned);
            if(!HNAdmit(TEXT("after_Set_existing_HLSL_text"))||!EditPlan->Rebind(Existing)){Reject();return R;}
            if(!SetReturned||!Written.Contains(FName(TEXT("CustomHlsl")))){Fail(R,TEXT("niagara_hlsl_existing_text_write_failed"),TEXT("Public editor property setter returned failure; actual in-place prefix retained."));Reject();return R;}
            if(!HNDispatch(TEXT("Synchronize_existing_HLSL"),true)){Reject();return R;}Existing->MarkNodeRequiresSynchronization(TEXT("CubelessExistingCustomHlsl"),true);if(!HNAdmit(TEXT("after_Synchronize_existing_HLSL"))){Reject();return R;}
            if(!HNDispatch(TEXT("Notify_existing_HLSL"),true)){Reject();return R;}NotifyGraph(*C.Graph);if(!HNAdmit(TEXT("after_Notify_existing_HLSL"))){Reject();return R;}
            if(!HNDispatch(TEXT("Dirty_existing_HLSL"),true)){Reject();return R;}C.System->MarkPackageDirty();if(!HNAdmit(TEXT("after_Dirty_existing_HLSL"))){Reject();return R;}
            auto PlannedLinks=Before.Links;EditPlan->AdjustLinks(PlannedLinks);const auto Actual=Capture(C.Graph);if(!HNAdmit(TEXT("after_existing_HLSL_snapshot"))){Reject();return R;}
            R.bChanged=true;R.bChangeStateKnown=Actual.bComplete;const bool Preserved=HlslPriorGraph(R,EditPlan->BeforeForProof(Before),Actual,PlannedLinks,TSet<FString>())&&HlslMetadataBounded(R,C.Graph,PriorParameters,TSet<FString>());
            R.Data->SetBoolField(TEXT("updated_existing_custom"),Preserved&&Actual.Nodes.Num()==Before.Nodes.Num());R.Data->SetBoolField(TEXT("custom_hlsl_attachment_verified"),Preserved);R.Data->SetStringField(TEXT("expression"),Code);
            R.Data->SetStringField(TEXT("custom_hlsl_node_guid"),Guid(Existing->NodeGuid));R.Data->SetStringField(TEXT("custom_node_guid"),Guid(Existing->NodeGuid));
            Finish(R,C,C.Graph,Actual,bSave,bRequestCompile);return R;
        }
        if(!PreservedInputName.IsEmpty()&&!PreservedBase)
        {Fail(R,TEXT("niagara_hlsl_rebuild_preserved_source_missing"),TEXT("Existing wrapper must expose exactly one preserved base input link; no writer."));Reject();return R;}
    }
    UClass* GetClass=FindObject<UClass>(nullptr,TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"));
    if((!Parameters.IsEmpty()||!Users.IsEmpty())&&(!IsValid(GetClass)||!GetClass->HasAllClassFlags(CLASS_Native)||!GetClass->IsChildOf(UNiagaraNode::StaticClass())||
        GetClass->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)))
    {Fail(R,TEXT("niagara_hlsl_mapget_class_unavailable"),TEXT("Exact concrete native MapGet class must already be loaded; no class-load fallback."));Reject();return R;}
    R.Data->SetStringField(TEXT("target_node_guid"),Guid(Target->NodeGuid));R.Data->SetStringField(TEXT("target_node_name"),Target->GetName());
    R.Data->SetStringField(TEXT("target_pin_name"),TargetPin->PinName.ToString());R.Data->SetStringField(TEXT("previous_default_value"),TargetPin->DefaultValue);
    R.Data->SetStringField(TEXT("previous_default_object"),Path(TargetPin->DefaultObject));R.Data->SetNumberField(TEXT("previous_link_count"),TargetPin->LinkedTo.Num());
    auto Links=Before.Links;const auto PriorLinks=TargetPin->LinkedTo;TSet<FString> Consumed;TArray<TSharedPtr<FJsonValue>> PairRows,InputRows,InputPrefix;
    R.Stage=TEXT("insert_custom_hlsl");R.bMutationAttempted=true;R.bChangeStateKnown=false;R.Data->SetNumberField(TEXT("custom_hlsl_insert_requests"),1);
    if(!HNDispatch(TEXT("System_Modify"),true)){Reject();return R;}C.System->Modify();if(!HNAdmit(TEXT("after_System_Modify"))){Reject();return R;}
    if(!HNDispatch(TEXT("Script_Modify"),true)){Reject();return R;}C.Script->Modify();if(!HNAdmit(TEXT("after_Script_Modify"))){Reject();return R;}
    if(!HNDispatch(TEXT("Source_Modify"),true)){Reject();return R;}C.Source->Modify();if(!HNAdmit(TEXT("after_Source_Modify"))){Reject();return R;}
    if(!HNDispatch(TEXT("Graph_Modify"),true)){Reject();return R;}C.Graph->Modify();if(!HNAdmit(TEXT("after_Graph_Modify"))){Reject();return R;}
    if(!HNDispatch(TEXT("CreateCustomHlslNode"),true)){Reject();return R;}TUniquePtr<FGraphNodeCreator<UNiagaraNodeCustomHlsl>> Creator;UNiagaraNodeCustomHlsl* Custom=nullptr;TStrongObjectPtr<UNiagaraNodeCustomHlsl> CustomReturnHold;FGuid PlannedCustomGuid;
    if(HNWriter)
    {
        // UEdGraph::CreateNode is protected. Its installed public-factory sequence
        // is NewObject(Transactional), inherited Transient flag, AddNode(false,false).
        // Spell out that same sequence so AddNode's callback can be admitted before
        // subsequent node SDKs, without FGraphNodeCreator's unfinalized checkf unwind.
        Custom=NewObject<UNiagaraNodeCustomHlsl>(C.Graph,UNiagaraNodeCustomHlsl::StaticClass(),NAME_None,RF_Transactional);CustomReturnHold.Reset(Custom);
        if(!HNAdmit(TEXT("after_NewCustomHlslObject"))){Reject();return R;}
        if(!Loaded(Custom)||Custom->GetClass()!=UNiagaraNodeCustomHlsl::StaticClass()||Custom->GetOuter()!=C.Graph)
        {HNWriter->Stop(TEXT("niagara_hlsl_allocation_failed"),TEXT("Public Custom HLSL factory returned no exact fresh graph-owned object."));Reject();return R;}
        if(C.Graph->HasAnyFlags(RF_Transient))Custom->SetFlags(RF_Transient);
        if(!HNWriter->Track(Custom)){Reject();return R;}
        if(!HNDispatch(TEXT("Graph_AddCustomHlslNode"),true)){Reject();return R;}
        if(EditPlan)EditPlan->ExpectAdd(Custom);C.Graph->AddNode(Custom,false,false);
        if(!HNAdmit(TEXT("after_Graph_AddCustomHlslNode"))){Reject();return R;}
        if(!C.Graph->Nodes.Contains(Custom)||Custom->GetGraph()!=C.Graph||Custom->GetOuter()!=C.Graph||!Custom->Pins.IsEmpty())
        {HNWriter->Stop(TEXT("niagara_hlsl_fresh_node_changed"),TEXT("Fresh Custom node membership/empty pin inventory changed during AddNode callback."));Reject();return R;}
    }
    else{Creator=MakeUnique<FGraphNodeCreator<UNiagaraNodeCustomHlsl>>(*C.Graph);Custom=Creator->CreateNode(false);}
    if(!HNAdmit(TEXT("after_CreateCustomHlslNode"))){Reject();return R;}if(HNWriter&&!HNWriter->Track(Custom)){Reject();return R;}
    Custom->Signature.Name=FName(*SignatureName);Custom->Signature.bRequiresContext=bRequiresContext;Custom->ScriptUsage=HlslUsage;
    for(const auto& S:Specs){Custom->Signature.Inputs.Add(FNiagaraVariable(S.Type,FName(*S.Name)));}
    Custom->Signature.Outputs.Add(FNiagaraVariable(TargetType,FName(*OutputPinName)));Custom->NodePosX=Target->NodePosX-320;Custom->NodePosY=Target->NodePosY+160;const auto ExpectedCustomSignature=Custom->Signature;const int32 ExpectedCustomX=Custom->NodePosX,ExpectedCustomY=Custom->NodePosY;if(HNWriter){Custom->CreateNewGuid();PlannedCustomGuid=Custom->NodeGuid;if(!HNDispatch(TEXT("PostPlacedCustomHlslNode"),true)){Reject();return R;}Custom->PostPlacedNewNode();if(!HNAdmit(TEXT("after_PostPlacedCustomHlslNode"))){Reject();return R;}
        if(Custom->Pins.IsEmpty()){if(!HNDispatch(TEXT("AllocateCustomHlslPins"),true)){Reject();return R;}Custom->AllocateDefaultPins();}}else{Creator->Finalize();}
    if(!HNAdmit(TEXT("after_FinalizeCustomHlslNode"))){Reject();return R;}
    auto FailEdited=[&](){HlslFailure(R,C,Before,PriorParameters);};
    if(!Loaded(Custom)||Custom->GetClass()!=UNiagaraNodeCustomHlsl::StaticClass()||Custom->FunctionScript||!Custom->FunctionScriptAssetObjectPath.IsNone())
    {Fail(R,TEXT("niagara_hlsl_allocation_failed"),TEXT("Custom allocation did not retain exact signature-only native identity."));FailEdited();return R;}
    auto TextJson=MakeShared<FJsonObject>();TextJson->SetStringField(TEXT("CustomHlsl"),Code);TArray<FName> WrittenNames;
    bool TextSet=false;if(HNWriter){if(!HNSetFreshHlslText(R,Custom,Code,TextSet,WrittenNames)){FailEdited();return R;}}
    else{TextSet=UToolsetLibrary::SetObjectProperties(Custom,Text(TextJson),WrittenNames,EBypassContainerCheck::No);}FString ReadText;
    R.Data->SetBoolField(TEXT("custom_hlsl_text_set_returned"),TextSet);const bool TextRead=HlslText(Custom,ReadText);if(!HNAdmit(TEXT("after_HLSL_text_readback"))){FailEdited();return R;}R.Data->SetBoolField(TEXT("custom_hlsl_text_readback_observed"),TextRead);
    if(!TextSet||!WrittenNames.Contains(FName(TEXT("CustomHlsl")))||!TextRead||ReadText!=Code)
    {Fail(R,TEXT("niagara_hlsl_text_write_failed"),TEXT("Public text setter/readback did not verify exact expression; retained actual partial node."));FailEdited();return R;}
    if(EditPlan&&(Custom->NodeGuid!=PlannedCustomGuid||!(Custom->Signature==ExpectedCustomSignature)||Custom->ScriptUsage!=HlslUsage||Custom->NodePosX!=ExpectedCustomX||Custom->NodePosY!=ExpectedCustomY||EditPlan->TextField->GetPropertyValue_InContainer(Custom)!=Code||!EditPlan->EnrollPrepared(Custom)))
    {if(R.ErrorCode.IsEmpty())EditPlan->Bad(TEXT("prepared_custom_plan"));FailEdited();return R;}
    for(const auto& S:Specs)
    {
        // ACQ: publish only actual ordered progress. Later row/callback/cleanup
        // failures retain earlier verified pairs and this row's attempted prefix.
        auto Step=MakeShared<FJsonObject>();Step->SetNumberField(TEXT("input_index"),InputPrefix.Num());Step->SetStringField(TEXT("input_name"),S.Name);Step->SetStringField(TEXT("parameter_name"),S.ParameterName);Step->SetObjectField(TEXT("type"),TypeRow(S.Type));Step->SetStringField(TEXT("stage"),TEXT("resolve_current_input"));Step->SetBoolField(TEXT("link_verified"),false);InputPrefix.Add(MakeShared<FJsonValueObject>(Step));R.Data->SetArrayField(TEXT("hlsl_input_prefix"),InputPrefix);
        auto* In=NamedPin(Custom,EGPD_Input,S.Name);if(!In||UEdGraphSchema_Niagara::PinToTypeDefinition(In)!=S.Type)
        {Fail(R,TEXT("niagara_hlsl_signature_shape_failed"),TEXT("Allocated ordered signature input has another actual native type/name."));FailEdited();return R;}
        if(S.ParameterName.IsEmpty())
        {Step->SetStringField(TEXT("stage"),TEXT("link_existing_source"));if(!HlslConnect(R,Schema,S.Source,In)){FailEdited();return R;}AddEdge(Links,PinId(S.Source),PinId(In));}
        else
        {
            Step->SetStringField(TEXT("stage"),TEXT("allocate_map_get"));if(!HNDispatch(TEXT("NewObject_UserMapGet"),true)){FailEdited();return R;}Step->SetBoolField(TEXT("map_get_allocation_attempted"),true);auto* Get=NewObject<UNiagaraNode>(C.Graph,GetClass,NAME_None,RF_Transactional);TStrongObjectPtr<UNiagaraNode> GetReturnHold(Get);Step->SetBoolField(TEXT("map_get_allocation_returned"),true);Step->SetBoolField(TEXT("map_get_returned_nonnull"),Get!=nullptr);
            if(!HNAdmit(TEXT("after_NewObject_UserMapGet"))){FailEdited();return R;}if(HNWriter&&!HNWriter->Track(Get)){FailEdited();return R;}
            if(!Loaded(Get)||Get->GetClass()!=GetClass){Fail(R,TEXT("niagara_hlsl_mapget_allocation_failed"),TEXT("Native MapGet allocation returned no exact owned node; actual state retained."));FailEdited();return R;}
            Step->SetStringField(TEXT("map_get_node_path"),Path(Get));Step->SetStringField(TEXT("stage"),TEXT("add_map_get"));if(!HNDispatch(TEXT("Graph_AddUserMapGet"),true)){FailEdited();return R;}Step->SetBoolField(TEXT("map_get_add_attempted"),true);if(EditPlan)EditPlan->ExpectAdd(Get);C.Graph->AddNode(Get,false,false);Step->SetBoolField(TEXT("map_get_add_returned"),true);if(!HNAdmit(TEXT("after_Graph_AddUserMapGet"))){FailEdited();return R;}Get->CreateNewGuid();const FGuid PlannedGetGuid=Get->NodeGuid;
            if(!HNDispatch(TEXT("PostPlacedUserMapGet"),true)){FailEdited();return R;}Get->PostPlacedNewNode();if(!HNAdmit(TEXT("after_PostPlacedUserMapGet"))){FailEdited();return R;}
            if(!HNDispatch(TEXT("AllocateUserMapGetPins"),true)){FailEdited();return R;}Get->AllocateDefaultPins();if(!HNAdmit(TEXT("after_AllocateUserMapGetPins"))){FailEdited();return R;}
            Get->NodePosX=Custom->NodePosX-320;Get->NodePosY=Custom->NodePosY+PairRows.Num()*140;const int32 PlannedGetX=Get->NodePosX,PlannedGetY=Get->NodePosY;UEdGraphPin* GetMap=nullptr;
            if(!ExactNodeClass(Get,TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"))||!MapPin(Get,EGPD_Input,GetMap)||!GetMap||
                !HlslConnect(R,Schema,MapSource,GetMap))
            {if(R.ErrorCode.IsEmpty()){Fail(R,TEXT("niagara_hlsl_mapget_shape_failed"),TEXT("New native Module map-read shape is incomplete."));}FailEdited();return R;}
            AddEdge(Links,PinId(MapSource),PinId(GetMap));
            Step->SetStringField(TEXT("stage"),TEXT("specialize_map_read_pair"));if(!AttachModuleRead(R,C,Schema,Get,In,FNiagaraVariable(S.Type,FName(*S.ParameterName)),Links,Consumed,PairRows)){FailEdited();return R;}Step->SetBoolField(TEXT("map_read_pair_verified"),true);R.Data->SetArrayField(TEXT("native_module_read_pairs"),PairRows);
            if(EditPlan&&(Get->NodeGuid!=PlannedGetGuid||Get->NodePosX!=PlannedGetX||Get->NodePosY!=PlannedGetY||!EditPlan->EnrollPrepared(Get))){if(R.ErrorCode.IsEmpty())EditPlan->Bad(TEXT("prepared_MapGet_plan"));FailEdited();return R;}
        }
        Step->SetStringField(TEXT("stage"),TEXT("input_linked"));Step->SetBoolField(TEXT("link_verified"),true);
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("input_name"),S.Name);Row->SetObjectField(TEXT("type"),TypeRow(S.Type));Row->SetStringField(TEXT("pin_id"),PinId(In));
        Row->SetStringField(TEXT("source_pin_id"),In->LinkedTo.Num()==1?PinId(In->LinkedTo[0]):FString());Row->SetStringField(TEXT("module_parameter_name"),S.ParameterName);InputRows.Add(MakeShared<FJsonValueObject>(Row));
    }
    auto* Out=NamedPin(Custom,EGPD_Output,OutputPinName);
    for(auto* P:PriorLinks){RemoveEdge(Links,PinId(P),PinId(TargetPin));}
    if(Existing&&bRebuildExistingCustom)
    {
        if(!EditPlan->Retire(Existing,TEXT("replaced_custom_node_count"))){FailEdited();return R;}
        EditPlan->AdjustLinks(Links);
    }
    if(!HlslConnect(R,Schema,Out,TargetPin,!PriorLinks.IsEmpty())){FailEdited();return R;}AddEdge(Links,PinId(Out),PinId(TargetPin));
    if(EditPlan&&(bDeleteUnlinkedCustomNodes||bDeleteUnlinkedCustomInputSourceNodes))
    {
        // Copy candidates before any RemoveNode callback; input cleanup is limited
        // to the replaced wrapper's original input-source inventory.
        TArray<UEdGraphNode*> Orphans;const auto Candidates=EditPlan->ExpectedNodes;
        for(auto* Candidate:Candidates)
        {
            if(Candidate==Custom||EditPlan->Added.Contains(Candidate)||EditPlan->Retired.Contains(Candidate))continue;
            const bool CustomCandidate=Candidate->GetClass()==UNiagaraNodeCustomHlsl::StaticClass();
            if(!(bDeleteUnlinkedCustomNodes&&CustomCandidate)&&!(bDeleteUnlinkedCustomInputSourceNodes&&ExistingInputSources.Contains(Candidate)))continue;
            bool HasOutputLinks=false;for(auto* Pin:Candidate->Pins)if(Pin->Direction==EGPD_Output&&!(Pin->PinType.PinCategory==UEdGraphSchema_Niagara::PinCategoryMisc&&Pin->PinType.PinSubCategory==FName(TEXT("DynamicAddPin")))&&!Pin->LinkedTo.IsEmpty()){HasOutputLinks=true;break;}
            if(!HasOutputLinks)Orphans.Add(Candidate);
        }
        for(auto* Orphan:Orphans)
        {
            if(!HNAdmit(TEXT("before_HLSL_orphan_role"))){FailEdited();return R;}
            const bool IsCustom=Orphan->GetClass()==UNiagaraNodeCustomHlsl::StaticClass();
            if(!EditPlan->Retire(Orphan,IsCustom?TEXT("removed_unlinked_custom_node_count"):TEXT("removed_unlinked_custom_input_source_node_count"))){FailEdited();return R;}
        }
        EditPlan->AdjustLinks(Links);
    }
    if(!HNDispatch(TEXT("MarkNodeRequiresSynchronization"),true)){FailEdited();return R;}Custom->MarkNodeRequiresSynchronization(TEXT("CubelessCustomHlslAttachment"),true);if(!HNAdmit(TEXT("after_MarkNodeRequiresSynchronization"))){FailEdited();return R;}
    if(!HNDispatch(TEXT("ConditionalRefreshParameterReferences"),true)){FailEdited();return R;}C.Graph->ConditionalRefreshParameterReferences();if(!HNAdmit(TEXT("after_ConditionalRefreshParameterReferences"))){FailEdited();return R;}
    if(!HNDispatch(TEXT("NotifyGraphChanged"),true)){FailEdited();return R;}NotifyGraph(*C.Graph);if(!HNAdmit(TEXT("after_NotifyGraphChanged"))){FailEdited();return R;}
    if(!HNDispatch(TEXT("MarkPackageDirty"),true)){FailEdited();return R;}C.System->MarkPackageDirty();if(!HNAdmit(TEXT("after_MarkPackageDirty"))){FailEdited();return R;}
    const auto After=Capture(C.Graph);if(!HNAdmit(TEXT("after_HLSL_written_snapshot"))){FailEdited();return R;}R.bChanged=true;R.bChangeStateKnown=After.bComplete;
    const bool GraphProof=HlslPriorGraph(R,EditPlan?EditPlan->BeforeForProof(Before):Before,After,Links,Consumed);const bool MetadataProof=HlslMetadataBounded(R,C.Graph,PriorParameters,NewMetadataNames);
    const bool Shape=After.bComplete&&After.Nodes.Num()==Before.Nodes.Num()+1+Parameters.Num()+Users.Num()-(EditPlan?EditPlan->Retired.Num():0)&&Custom->Signature.Inputs.Num()==Specs.Num()&&
        Custom->Signature.Outputs.Num()==1&&Custom->ScriptUsage==HlslUsage&&TargetPin->LinkedTo.Num()==1&&TargetPin->LinkedTo[0]==Out;
    R.Data->SetBoolField(TEXT("custom_hlsl_attachment_verified"),Shape&&GraphProof&&MetadataProof);R.Data->SetArrayField(TEXT("native_module_read_pairs"),PairRows);
    R.Data->SetArrayField(TEXT("custom_inputs"),InputRows);R.Data->SetStringField(TEXT("expression"),ReadText);R.Data->SetStringField(TEXT("custom_node_guid"),Guid(Custom->NodeGuid));R.Data->SetStringField(TEXT("custom_hlsl_node_guid"),Guid(Custom->NodeGuid));
    R.Data->SetStringField(TEXT("custom_node_path"),Path(Custom));R.Data->SetStringField(TEXT("custom_hlsl_script_usage"),UsageName(Custom->ScriptUsage));
    R.Data->SetStringField(TEXT("output_pin_name"),OutputPinName);R.Data->SetObjectField(TEXT("output_type"),TypeRow(TargetType));R.Data->SetStringField(TEXT("output_pin_id"),PinId(Out));
    R.Data->SetBoolField(TEXT("requires_context"),bRequiresContext);R.Data->SetBoolField(TEXT("auto_added_context_map_input"),bRequiresContext);
    R.Data->SetNumberField(TEXT("custom_input_count"),Specs.Num());R.Data->SetNumberField(TEXT("user_parameter_input_count"),Users.Num());R.Data->SetNumberField(TEXT("created_user_parameter_input_node_count"),PairRows.Num());R.Data->SetNumberField(TEXT("parameter_input_count"),Parameters.Num());
    R.Data->SetNumberField(TEXT("broken_link_count"),PriorLinks.Num());if(!EditPlan){R.Data->SetNumberField(TEXT("removed_unlinked_custom_node_count"),0);R.Data->SetNumberField(TEXT("removed_unlinked_custom_input_source_node_count"),0);}
    if(!Shape){Fail(R,TEXT("niagara_hlsl_shape_postcondition_failed"),TEXT("Observed attachment node/signature/target counts differ; actual graph is retained."));}
    Finish(R,C,C.Graph,After,bSave,bRequestCompile);return R;
}
Function::FResult Rewire(const FString& SystemPath,const FString& InputName,const FString& ScriptName,
    const FString& SignatureName,const FString& ModuleParameterName,bool bDeleteOrphanedSourceNode,
    bool bAllowSourceEdit,bool bSave,bool bRequestCompile)
{
    FResult R;Initialize(R,SystemPath,bSave);R.Data->SetStringField(TEXT("write_scope"),TEXT("niagara_scratch_custom_hlsl_input_rewire"));
    R.Data->SetBoolField(TEXT("source_rewiring_verified"),false);R.Data->SetBoolField(TEXT("full_position_producer_verified"),false);
    R.Data->SetBoolField(TEXT("protected_pair_storage_independently_read"),false);R.Data->SetNumberField(TEXT("custom_hlsl_rewire_requests"),0);
    FString Parameter=ModuleParameterName.TrimStartAndEnd();if(Parameter.IsEmpty()){Parameter=InputName;}
    if(!Parameter.StartsWith(TEXT("Module."))){Parameter=TEXT("Module.")+Parameter;}
    if(!HlslIdentifier(InputName)||!HlslIdentifier(Parameter.Mid(7))||!Bounded(ScriptName,256)||!Bounded(SignatureName,256))
    {Fail(R,TEXT("niagara_hlsl_rewire_request_invalid"),TEXT("Input and Module parameter must be finite identifiers; source/signature names must be bounded."));return R;}
    FContext C;if(!Base(R,C,SystemPath,true,bAllowSourceEdit)){return R;}
    UNiagaraScript* Selected=nullptr;
    auto Match=[&](const TArray<TObjectPtr<UNiagaraScript>>& Scripts)
    {for(UNiagaraScript* S:Scripts){if(!Selected&&(ScriptName.IsEmpty()||S->GetName().Equals(ScriptName,ESearchCase::IgnoreCase))){Selected=S;break;}}};
    Match(C.System->ScratchPadScripts);
    for(const auto& H:C.System->GetEmitterHandles()){if(Selected){break;}if(const auto* D=H.GetEmitterData();D&&D->ScratchPads){Match(D->ScratchPads->Scripts);}}
    if(!Selected){Fail(R,TEXT("niagara_scratch_script_not_found"),TEXT("No registered scratch module matches the original system-then-emitter first-match name selector."));return R;}
    if(!Select(R,C,Path(Selected),TEXT("system"),TEXT("null"),TEXT(""),TEXT("null"),TEXT(""),true)){return R;}
    const auto Before=Capture(C.Graph);BeforeGraph(R,C.Graph,Before);bool PComplete=false;const auto PriorParameters=GraphParameters(C.Graph,PComplete);
    const auto* Schema=Cast<UEdGraphSchema_Niagara>(C.Graph->GetSchema());auto Reject=[&](){HlslFailure(R,C,Before,PriorParameters);};
    if(!Before.bComplete||!PComplete||!Schema){Fail(R,TEXT("niagara_hlsl_graph_unobservable"),TEXT("Selected full native graph/schema/typed metadata must be observable before rewire."));Reject();return R;}
    UNiagaraNodeCustomHlsl* Custom=nullptr;UEdGraphPin* Input=nullptr;
    for(UEdGraphNode* N:C.Graph->Nodes)
    {auto* Candidate=Cast<UNiagaraNodeCustomHlsl>(N);if(Candidate&&Candidate->GetClass()==UNiagaraNodeCustomHlsl::StaticClass()&&
        (SignatureName.IsEmpty()||Candidate->Signature.Name.ToString().Equals(SignatureName,ESearchCase::IgnoreCase)))
        {if(auto* P=NamedPin(Candidate,EGPD_Input,InputName)){Custom=Candidate;Input=P;break;}}}
    if(!Custom||!Input||Input->bNotConnectable||Input->LinkedTo.Num()>1||!HlslValueType(UEdGraphSchema_Niagara::PinToTypeDefinition(Input)))
    {Fail(R,TEXT("niagara_hlsl_rewire_input_unsupported"),TEXT("First cohort rewires one existing concrete primitive/Position HLSL input; map/DI/UObject/ambiguous-source variants remain incomplete."));Reject();return R;}
    const auto Type=UEdGraphSchema_Niagara::PinToTypeDefinition(Input);const FNiagaraVariable Variable(Type,FName(*Parameter));
    if(!HlslMetadataAdmit(R,C.Graph,Variable)){Reject();return R;}
    const auto PriorLinks=Input->LinkedTo;UEdGraphNode* Previous=PriorLinks.IsEmpty()?nullptr:PriorLinks[0]->GetOwningNode();
    bool WouldDelete=bDeleteOrphanedSourceNode&&Previous&&Cast<UNiagaraNodeInput>(Previous);
    if(WouldDelete){for(auto* P:Previous->Pins){for(auto* Linked:P->LinkedTo){if(Linked!=Input){WouldDelete=false;}}}}
    if(WouldDelete){Fail(R,TEXT("niagara_hlsl_orphan_deletion_incomplete"),TEXT("This request would delete an orphaned NiagaraNodeInput; destructive cleanup remains incomplete and is rejected before rewire."));Reject();return R;}
    UNiagaraNode* Get=nullptr;
    for(UEdGraphNode* N:C.Graph->Nodes){if(ExactNodeClass(N,TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"))){Get=Cast<UNiagaraNode>(N);break;}}
    UEdGraphPin* MapSource=nullptr;UEdGraphPin* GetMap=nullptr;
    if(Get&&MapPin(Get,EGPD_Input,GetMap)&&GetMap&&GetMap->LinkedTo.Num()==1){MapSource=GetMap->LinkedTo[0];}
    if(!MapSource)
    {for(auto* Out:Custom->Pins){if(Out->Direction!=EGPD_Output){continue;}for(auto* Linked:Out->LinkedTo)
        {auto* N=Linked->GetOwningNode();UEdGraphPin* Map=nullptr;if(ExactNodeClass(N,TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapSet"))&&
            MapPin(N,EGPD_Input,Map)&&Map&&Map->LinkedTo.Num()==1){if(MapSource&&MapSource!=Map->LinkedTo[0])
                {Fail(R,TEXT("niagara_hlsl_map_source_ambiguous"),TEXT("HLSL consumers expose different map predecessors."));Reject();return R;}MapSource=Map->LinkedTo[0];}}}}
    if(!MapSource||!MapSource->LinkedTo.Num()||UEdGraphSchema_Niagara::PinToTypeDefinition(MapSource)!=FNiagaraTypeDefinition::GetParameterMapDef()||
        (Get&&(!GetMap||GetMap->LinkedTo.Num()!=1||GetMap->LinkedTo[0]!=MapSource)))
    {Fail(R,TEXT("niagara_hlsl_map_source_invalid"),TEXT("Existing/rebuilt Module read must have the actual unique reciprocal selected map source; unconnected/shared ambiguous reads are not repaired."));Reject();return R;}
    auto* Existing=Get?NamedPin(Get,EGPD_Output,Parameter):nullptr;
    if(Existing&&UEdGraphSchema_Niagara::PinToTypeDefinition(Existing)!=Type)
    {Fail(R,TEXT("niagara_hlsl_parameter_metadata_conflict"),TEXT("Existing Module output has another concrete type; no edit attempted."));Reject();return R;}
    if(Existing&&PriorLinks.Num()==1&&PriorLinks[0]==Existing)
    {R.Data->SetBoolField(TEXT("already_rewired"),true);R.Data->SetBoolField(TEXT("source_rewiring_verified"),true);R.Data->SetBoolField(TEXT("save_requested"),false);
        R.Data->SetStringField(TEXT("module_parameter_name"),Parameter);R.Data->SetStringField(TEXT("custom_node_signature"),Custom->Signature.Name.ToString());
        R.Data->SetStringField(TEXT("input_name"),InputName);R.Data->SetStringField(TEXT("input_pin_id"),PinId(Input));R.Data->SetObjectField(TEXT("input_type"),TypeRow(Type));
        R.Data->SetBoolField(TEXT("created_map_get_pin"),false);R.Data->SetBoolField(TEXT("deleted_orphaned_source_node"),false);Reject();return R;}
    UClass* GetClass=FindObject<UClass>(nullptr,TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"));
    if((!Get&&(!IsValid(GetClass)||!GetClass->HasAllClassFlags(CLASS_Native)||!GetClass->IsChildOf(UNiagaraNode::StaticClass())||
        GetClass->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)))||
        (Get&&!Existing&&!DynamicOutput(Get)))
    {Fail(R,TEXT("niagara_hlsl_mapget_class_unavailable"),TEXT("Exact native MapGet or its unused dynamic output must be available before mutation."));Reject();return R;}
    R.Data->SetStringField(TEXT("custom_node_signature"),Custom->Signature.Name.ToString());R.Data->SetStringField(TEXT("custom_node_guid"),Guid(Custom->NodeGuid));
    R.Data->SetStringField(TEXT("input_name"),InputName);R.Data->SetStringField(TEXT("module_parameter_name"),Parameter);R.Data->SetObjectField(TEXT("input_type"),TypeRow(Type));
    R.Data->SetStringField(TEXT("previous_source_node_class"),Previous?Path(Previous->GetClass()):FString());R.Data->SetBoolField(TEXT("deleted_orphaned_source_node"),false);
    const bool NewGet=Get==nullptr;auto Links=Before.Links;TSet<FString> Consumed,NewMetadata;TArray<TSharedPtr<FJsonValue>> PairRows;
    for(auto* P:PriorLinks){RemoveEdge(Links,PinId(P),PinId(Input));}
    R.Stage=TEXT("rewire_custom_hlsl_input");R.bMutationAttempted=true;R.bChangeStateKnown=false;R.Data->SetNumberField(TEXT("custom_hlsl_rewire_requests"),1);
    C.System->Modify();C.Script->Modify();C.Source->Modify();C.Graph->Modify();Custom->Modify();
    if(NewGet)
    {Get=NewObject<UNiagaraNode>(C.Graph,GetClass,NAME_None,RF_Transactional);
        if(!Loaded(Get)||Get->GetClass()!=GetClass){Fail(R,TEXT("niagara_hlsl_mapget_allocation_failed"),TEXT("Native MapGet allocation returned no exact owned node; actual state retained."));Reject();return R;}
        C.Graph->AddNode(Get,false,false);Get->CreateNewGuid();Get->PostPlacedNewNode();Get->AllocateDefaultPins();
        Get->NodePosX=Custom->NodePosX-320;Get->NodePosY=Custom->NodePosY+160;
        if(!MapPin(Get,EGPD_Input,GetMap)||!GetMap||!HlslConnect(R,Schema,MapSource,GetMap))
        {if(R.ErrorCode.IsEmpty()){Fail(R,TEXT("niagara_hlsl_mapget_shape_failed"),TEXT("New public native Module read shape failed."));}Reject();return R;}
        AddEdge(Links,PinId(MapSource),PinId(GetMap));}
    Get->Modify();
    if(Existing)
    {if(!HlslConnect(R,Schema,Existing,Input,!PriorLinks.IsEmpty())){Reject();return R;}AddEdge(Links,PinId(Existing),PinId(Input));}
    else
    {NewMetadata.Add(Parameter);if(!AttachModuleRead(R,C,Schema,Get,Input,Variable,Links,Consumed,PairRows)){Reject();return R;}}
    Get->MarkNodeRequiresSynchronization(TEXT("CubelessModuleInputRewire"),true);Custom->MarkNodeRequiresSynchronization(TEXT("CubelessModuleInputRewire"),true);
    C.Graph->ConditionalRefreshParameterReferences();NotifyGraph(*C.Graph);C.System->MarkPackageDirty();
    const auto After=Capture(C.Graph);R.bChanged=true;R.bChangeStateKnown=After.bComplete;
    const bool GraphProof=HlslPriorGraph(R,Before,After,Links,Consumed);const bool MetadataProof=HlslMetadataBounded(R,C.Graph,PriorParameters,NewMetadata);
    auto* Final=NamedPin(Get,EGPD_Output,Parameter);const bool Shape=After.bComplete&&After.Nodes.Num()==Before.Nodes.Num()+static_cast<int32>(NewGet)&&
        Final&&Input->LinkedTo.Num()==1&&Input->LinkedTo[0]==Final&&UEdGraphSchema_Niagara::PinToTypeDefinition(Final)==Type;
    R.Data->SetBoolField(TEXT("source_rewiring_verified"),Shape&&GraphProof&&MetadataProof);R.Data->SetBoolField(TEXT("already_rewired"),false);
    R.Data->SetBoolField(TEXT("created_map_get_node"),NewGet);R.Data->SetBoolField(TEXT("created_map_get_pin"),Existing==nullptr);
    R.Data->SetStringField(TEXT("map_get_node_path"),Path(Get));R.Data->SetStringField(TEXT("module_output_pin_id"),PinId(Final));R.Data->SetStringField(TEXT("input_pin_id"),PinId(Input));
    R.Data->SetArrayField(TEXT("native_module_read_pairs"),PairRows);
    if(!Shape){Fail(R,TEXT("niagara_hlsl_rewire_postcondition_failed"),TEXT("Actual typed Module read did not retain the exact selected HLSL input edge; partial state remains."));}
    Finish(R,C,C.Graph,After,bSave,bRequestCompile);return R;
}
} // namespace Cubeless::UE58::NiagaraScratch
