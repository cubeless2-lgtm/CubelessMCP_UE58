#include "Compatibility/UE58/CubelessNiagaraModuleInputApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "EditorAssetLibrary.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceExport.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraParameterMapHistory.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraScriptVariable.h"
#include "NiagaraSystem.h"
#include "NiagaraSettings.h"
#include "NiagaraCommon.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "Misc/EngineVersion.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::NiagaraModuleInput
{
namespace
{
using Function::FResult;
constexpr int32 MaxNodes = 1024, MaxPins = 8192, MaxLinks = 32768, MaxEmitters = 128, MaxParameters = 2048;
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
TSharedRef<FJsonObject> CompileState(UNiagaraSystem* System)
{
    auto J = MakeShared<FJsonObject>();
    const bool Outstanding = System->HasOutstandingCompilationRequests(true), Needs = System->NeedsRequestCompile();
    J->SetBoolField(TEXT("outstanding_including_gpu"), Outstanding); J->SetBoolField(TEXT("needs_request_compile"), Needs);
    J->SetBoolField(TEXT("ready_to_run"), System->IsReadyToRun()); J->SetBoolField(TEXT("ready_to_save"), !Outstanding && !Needs);
    J->SetBoolField(TEXT("compile_success_verified"), false); J->SetBoolField(TEXT("poll_or_flush_invoked"), false); return J;
}
FString PinId(const UEdGraphPin* Pin)
{ return Pin ? Path(Pin->GetOwningNode()) + TEXT(".") + Pin->PinName.ToString() + TEXT("/") + Guid(Pin->PinId) : FString(); }
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
    J->SetStringField(TEXT("default_object_path"), Path(Pin->DefaultObject)); J->SetStringField(TEXT("default_text"), Pin->DefaultTextValue.ToString());
    J->SetBoolField(TEXT("default_text_empty"), Pin->DefaultTextValue.IsEmpty()); J->SetBoolField(TEXT("hidden"), Pin->bHidden);
    J->SetBoolField(TEXT("orphaned"), Pin->bOrphanedPin); J->SetBoolField(TEXT("not_connectable"), Pin->bNotConnectable);
    J->SetBoolField(TEXT("default_value_is_ignored"), Pin->bDefaultValueIsIgnored); J->SetBoolField(TEXT("default_value_is_readonly"), Pin->bDefaultValueIsReadOnly);
    return J;
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
        auto Variable = [](const FNiagaraVariableBase& V)
        {
            auto Row = MakeShared<FJsonObject>(); const auto& T = V.GetType(); Row->SetStringField(TEXT("name"), V.GetName().ToString());
            Row->SetStringField(TEXT("type"), T.GetNameText().ToString()); Row->SetBoolField(TEXT("type_is_valid"), T.IsValid());
            Row->SetBoolField(TEXT("type_is_data_interface"), T.IsDataInterface()); Row->SetBoolField(TEXT("type_is_uobject"), T.IsUObject());
            if (UClass* Class = T.GetClass()) { Row->SetStringField(TEXT("type_class"), Class->GetName()); Row->SetStringField(TEXT("type_class_path"), Path(Class)); }
            return MakeShared<FJsonValueObject>(Row);
        };
        TArray<TSharedPtr<FJsonValue>> Inputs, Outputs, Specifiers; FString Search = Call->GetFunctionName() + TEXT(" ") + Path(Call->FunctionScript);
        for (const auto& V : Call->Signature.Inputs) { Inputs.Add(Variable(V)); Search += TEXT(" ") + V.GetName().ToString(); }
        for (const auto& V : Call->Signature.Outputs) { Outputs.Add(Variable(V)); Search += TEXT(" ") + V.GetName().ToString(); }
        J->SetArrayField(TEXT("signature_inputs"), Inputs); J->SetArrayField(TEXT("signature_outputs"), Outputs);
        TArray<FString> SpecifierText; for (const auto& V : Call->FunctionSpecifiers) { SpecifierText.AddUnique(V.Key.ToString() + TEXT("=") + V.Value.ToString()); }
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
    return J;
}
struct FSnapshot
{
    bool bComplete = false; TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    TMap<FString, FString> Nodes, Pins; TMap<FString, TArray<FString>> Links;
};
FSnapshot Capture(UNiagaraGraph* Graph)
{
    FSnapshot S; S.Json->SetStringField(TEXT("graph_path"), Path(Graph));
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
        auto Row = NodeMetadata(Node); const FString N = Path(Node); S.Nodes.Add(N, Text(Row));
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
            auto PinRow = PinMetadata(Pin); S.Pins.Add(P, Text(PinRow)); S.Links.Add(P, Links);
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
struct FProtectedGraph { UNiagaraScriptSource* Source = nullptr; UNiagaraGraph* Graph = nullptr; FSnapshot Before; TSharedRef<FJsonObject> Parameters = MakeShared<FJsonObject>(); bool Dirty = false, InheritedRole = false; };
struct FInheritedEmitterRole
{
    TStrongObjectPtr<UNiagaraEmitter> Emitter;TStrongObjectPtr<UClass> Class;TStrongObjectPtr<UObject> Outer;TStrongObjectPtr<UPackage> Package;
    TStrongObjectPtr<UNiagaraScriptSource> Source;TStrongObjectPtr<UNiagaraGraph> Graph;
    TStrongObjectPtr<UClass> SourceClass;TStrongObjectPtr<UObject> SourceOuter;TStrongObjectPtr<UPackage> SourcePackage;FString SourcePath;
    FGuid Version;FVersionedNiagaraEmitterData* Data=nullptr;FVersionedNiagaraEmitter Parent,LastMerge;FString PathName;bool Ready=false;
};
struct FContext
{
    UNiagaraSystem* System = nullptr; FVersionedNiagaraEmitter Emitter; FVersionedNiagaraEmitterData* Data = nullptr;
    TStrongObjectPtr<UNiagaraSystem> KeepSystem; TStrongObjectPtr<UNiagaraScript> KeepModuleScript;
    UNiagaraScriptSource* Source = nullptr; UNiagaraGraph* Graph = nullptr; UNiagaraNodeFunctionCall* Module = nullptr;
    UNiagaraScript* ModuleScript = nullptr; UNiagaraScriptSource* ModuleSource = nullptr; UNiagaraGraph* ModuleGraph = nullptr;
    const UEdGraphSchema_Niagara* Schema = nullptr; FNiagaraVariable Input;
    FNiagaraParameterHandle Alias; UEdGraphPin* ModuleMapIn = nullptr; UEdGraphPin* Existing = nullptr;
    UEdGraphPin* PreviousMapOut = nullptr;
    UEdGraphNode* OverrideNode = nullptr; int32 EmitterIndex = INDEX_NONE, ModuleIndex = INDEX_NONE;
    FString EmitterName, ModuleName; ENiagaraScriptUsage Usage = ENiagaraScriptUsage::Function;
    FSnapshot Before; TSharedRef<FJsonObject> UsersBefore = MakeShared<FJsonObject>(), HandlesBefore = MakeShared<FJsonObject>(), ParametersBefore = MakeShared<FJsonObject>();
    TArray<FProtectedGraph> Protected; bool Dirty = false;
    TArray<TUniquePtr<FInheritedEmitterRole>> InheritedRoles;TStrongObjectPtr<UNiagaraEmitter> InheritedTarget;
    TStrongObjectPtr<UNiagaraScriptSource> InheritedLocalSource;TStrongObjectPtr<UNiagaraGraph> InheritedLocalGraph;TStrongObjectPtr<UNiagaraNodeFunctionCall> InheritedLocalModule;
    TArray<TStrongObjectPtr<UNiagaraEmitter>> InheritedSlotHolds;
    FVersionedNiagaraEmitter InitialParent,InitialLastMerge;bool InheritedProfile=false,InheritedLost=false;
};
void Initialize(FResult& R, const FString& SystemPath, const FString& InputName, bool bSave)
{
    R.Data->SetStringField(TEXT("requested_system_path"), SystemPath); R.Data->SetStringField(TEXT("requested_input_name"), InputName);
    for (const TCHAR* K : {TEXT("compile_requested"), TEXT("compile_attempted"), TEXT("save_attempted"), TEXT("saved"), TEXT("save_deferred"),
        TEXT("blocking_wait_invoked"), TEXT("asset_load_attempted"), TEXT("explicit_user_parameter_writes"), TEXT("graph_parameter_metadata_changed"),
        TEXT("cold_private_di_reference_verified"), TEXT("atomic_rollback_available"), TEXT("full_legacy_module_input_parity")}) { R.Data->SetBoolField(K, false); }
    R.Data->SetBoolField(TEXT("save_requested"), bSave); R.Data->SetBoolField(TEXT("auto_retry_safe"), false);
    R.Data->SetBoolField(TEXT("implicit_graph_notification_callbacks_possible"), true);
    R.Data->SetBoolField(TEXT("input_enumeration_cache_updates_possible"), true);
    R.Data->SetBoolField(TEXT("recursive_dependency_audit_performed"), false);
    R.Data->SetStringField(TEXT("dependency_layer"), TEXT("EngineAPI: exported NiagaraEditor stack utilities and direct public fields; no ServerRPC or ToolsetAPI"));
    R.Data->SetStringField(TEXT("selector_policy"), TEXT("legacy first matching emitter, then all FunctionCall nodes sorted Y/X; index is not stack module order"));
    R.Data->SetNumberField(TEXT("explicit_compile_requests"), 0); R.Data->SetNumberField(TEXT("exact_save_requests"), 0);
    R.Data->SetNumberField(TEXT("override_pin_requests"), 0); R.Data->SetNumberField(TEXT("value_write_requests"), 0);
    R.Data->SetBoolField(TEXT("outcome_unknown"), false);
}
bool OrdinaryGraph(UNiagaraScriptSource* Source, UNiagaraGraph*& Graph)
{
    Graph = Source ? Source->NodeGraph.Get() : nullptr;
    return Loaded(Source) && !Source->bIsCompilationCopy && !Source->bIsReleased && Loaded(Graph) &&
        Graph->GetClass() == UNiagaraGraph::StaticClass() && !Graph->IsCompilationCopy() && Graph->GetOuter() == Source &&
        Graph->Schema.Get() == UEdGraphSchema_Niagara::StaticClass();
}
bool Protect(FResult& R, FContext& C, UNiagaraScriptSource* Source)
{
    UNiagaraGraph* G = nullptr;
    if (!OrdinaryGraph(Source, G)) { return Fail(R, TEXT("niagara_source_graph_invalid"), TEXT("Every protected ordinary native source graph must already be loaded.")); }
    if (G == C.Graph) { return true; }
    for (const auto& P : C.Protected) { if (P.Graph == G) { return true; } }
    if (C.Protected.Num() >= 128) { return Fail(R, TEXT("niagara_source_bound_exceeded"), TEXT("Protected native graph inventory exceeds 128 unique graphs.")); }
    FProtectedGraph P; P.Source = Source; P.Graph = G; P.Before = Capture(G); P.Dirty = G->GetOutermost()->IsDirty();
    bool Complete = false; P.Parameters = GraphParameters(G, Complete);
    if (!P.Before.bComplete || !Complete) { return Fail(R, TEXT("niagara_source_observation_incomplete"), TEXT("Protected source pin/link/parameter observation is incomplete before dispatch.")); }
    C.Protected.Add(MoveTemp(P)); return true;
}
// WH: standard local emitter edits retain readonly Parent and LastMerge roles.
// LastMerge is often a non-versioned duplicate inside the child emitter/package;
// no asset flags, common Outer, or stored GUID == version-data GUID is assumed.
bool InheritedFailure(FResult& R,FContext& C,const TCHAR* Stage,const FString& Object,const TCHAR* Property,const FString& Expected,const FString& Actual)
{
    if(!C.InheritedLost){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),Stage);J->SetStringField(TEXT("object"),Object);J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);R.Data->SetObjectField(TEXT("inherited_source_first_mismatch"),J);}
    C.InheritedLost=true;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("outcome_unknown"),true);R.Data->SetBoolField(TEXT("do_not_retry"),true);
    return Fail(R,TEXT("niagara_inherited_source_changed"),TEXT("Captured inherited emitter/source role changed; actual prefix retained, no next writer/compile/save/retry."));
}
bool ParentSlotSame(const FVersionedNiagaraEmitter& A,const FVersionedNiagaraEmitter& B)
{return A.Emitter.IsResolved()&&B.Emitter.IsResolved()&&A.Emitter.Get()==B.Emitter.Get()&&A.Version==B.Version;}
bool InheritedCurrent(FResult& R,FContext& C,const TCHAR* Stage)
{
    if(!C.InheritedProfile)return true;if(C.InheritedLost)return false;
    if(!IsEditorGameThread()||IsPlaySessionActive()||IsGarbageCollecting()||UE::IsSavingPackage()||GExitPurge||IsEngineExitRequested()||!Loaded(C.System)||!Loaded(C.InheritedTarget.Get()))return InheritedFailure(R,C,Stage,Path(C.System),TEXT("execution_context"),TEXT("same live idle Editor target"),TEXT("unavailable"));
    const auto& HandlesNow=C.System->GetEmitterHandles();
    if(!HandlesNow.IsValidIndex(C.EmitterIndex)||!ParentSlotSame(HandlesNow[C.EmitterIndex].GetInstance(),C.Emitter))return InheritedFailure(R,C,Stage,Path(C.System),TEXT("selected_emitter_handle"),Path(C.InheritedTarget.Get())+TEXT("|")+Guid(C.Emitter.Version),TEXT("changed"));
    auto* CurrentData=C.InheritedTarget->GetEmitterData(C.Emitter.Version);
    if(CurrentData!=C.Data||!CurrentData||!CurrentData->GraphSource.IsResolved()||CurrentData->GraphSource.Get()!=C.Source||!ParentSlotSame(CurrentData->VersionedParent,C.InitialParent)||!ParentSlotSame(CurrentData->VersionedParentAtLastMerge,C.InitialLastMerge))return InheritedFailure(R,C,Stage,Path(C.InheritedTarget.Get()),TEXT("selected_version_source_parent_slots"),TEXT("captured public version/source/Parent/LastMerge"),TEXT("changed"));
    // The selected local writer is held independently of emitter/source slots.
    // A callback may replace those slots or remove the module from its graph.
    if(C.InheritedLocalSource.Get()!=C.Source||C.InheritedLocalGraph.Get()!=C.Graph||C.InheritedLocalModule.Get()!=C.Module||!Loaded(C.InheritedLocalSource.Get())||!Loaded(C.InheritedLocalGraph.Get())||!Loaded(C.InheritedLocalModule.Get())||!C.Source->NodeGraph.IsResolved()||C.Source->NodeGraph.Get()!=C.Graph||C.Graph->GetOuter()!=C.Source||C.Module->GetOuter()!=C.Graph||C.Module->GetGraph()!=C.Graph||!C.Graph->Nodes.Contains(C.Module))return InheritedFailure(R,C,Stage,Path(C.InheritedTarget.Get()),TEXT("selected_local_source_graph_module"),TEXT("held Source.NodeGraph and live Module owner/Graph/Nodes membership"),TEXT("changed"));
    for(const auto& Role:C.InheritedRoles)
    {
        const auto& E=*Role;
        if(!Loaded(E.Emitter.Get())||!Loaded(E.Class.Get())||!Loaded(E.Outer.Get())||!Loaded(E.Package.Get())||E.Emitter->GetClass()!=E.Class.Get()||E.Emitter->GetOuter()!=E.Outer.Get()||E.Emitter->GetOutermost()!=E.Package.Get()||!E.Emitter->GetPathName().Equals(E.PathName,ESearchCase::CaseSensitive))return InheritedFailure(R,C,Stage,E.PathName,TEXT("parent_object_role"),TEXT("held emitter/class/outer/package/path"),TEXT("changed"));
        // Reacquire via each original stored GUID before dereferencing data.
        // GetEmitterData intentionally ignores GUID for non-versioned snapshots.
        auto* D=E.Emitter->GetEmitterData(E.Version);
        if(!D||D!=E.Data||!D->GraphSource.IsResolved()||D->GraphSource.Get()!=E.Source.Get()||!ParentSlotSame(D->VersionedParent,E.Parent)||!ParentSlotSame(D->VersionedParentAtLastMerge,E.LastMerge)||!Loaded(E.Source.Get())||!E.Source->NodeGraph.IsResolved()||E.Source->NodeGraph.Get()!=E.Graph.Get()||!Loaded(E.SourceClass.Get())||!Loaded(E.SourceOuter.Get())||!Loaded(E.SourcePackage.Get())||E.Source->GetClass()!=E.SourceClass.Get()||E.Source->GetOuter()!=E.SourceOuter.Get()||E.Source->GetOutermost()!=E.SourcePackage.Get()||!E.Source->GetPathName().Equals(E.SourcePath,ESearchCase::CaseSensitive))return InheritedFailure(R,C,Stage,E.PathName,TEXT("parent_version_source_slots"),TEXT("captured getter return/source/Parent/LastMerge"),TEXT("changed"));
    }
    for(const auto& P:C.Protected)if(P.InheritedRole)
    {
        UNiagaraGraph* G=nullptr;if(!OrdinaryGraph(P.Source,G)||G!=P.Graph)return InheritedFailure(R,C,Stage,Path(P.Source),TEXT("parent_graph_role"),Path(P.Graph),TEXT("changed"));
        const auto Now=Capture(G);bool Complete=false;const auto Params=GraphParameters(G,Complete);
        const bool Shared=G->GetOutermost()==C.System->GetOutermost();
        if(!Now.bComplete||!Complete||!SameGraph(P.Before,Now)||Text(P.Before.Json)!=Text(Now.Json)||Text(P.Parameters)!=Text(Params)||(!Shared&&P.Dirty!=G->GetOutermost()->IsDirty()))return InheritedFailure(R,C,Stage,Path(G),TEXT("readonly_parent_graph"),TEXT("captured authored nodes/pins/links/parameters and external package dirty"),TEXT("changed or incomplete"));
    }
    return true;
}
bool CaptureInherited(FResult& R,FContext& C)
{
    if(!C.Data->VersionedParent.Emitter.IsResolved()||!C.Data->VersionedParentAtLastMerge.Emitter.IsResolved())return Fail(R,TEXT("niagara_inherited_parent_unresolved"),TEXT("Already-resolved nullable Parent/LastMerge slots required; no loader or detach."));
    C.InitialParent=C.Data->VersionedParent;C.InitialLastMerge=C.Data->VersionedParentAtLastMerge;
    if(!C.InitialParent.Emitter.Get()&&!C.InitialLastMerge.Emitter.Get())return true;
    C.InheritedTarget.Reset(C.Emitter.Emitter.Get());
    if(!Loaded(C.Source)||!Loaded(C.Graph)||!Loaded(C.Module)||!C.Source->NodeGraph.IsResolved()||C.Source->NodeGraph.Get()!=C.Graph||C.Graph->GetOuter()!=C.Source||C.Module->GetOuter()!=C.Graph||C.Module->GetGraph()!=C.Graph||!C.Graph->Nodes.Contains(C.Module))return Fail(R,TEXT("niagara_inherited_local_writer_unavailable"),TEXT("Live selected local Source/Graph/Module ownership and membership required before parent observers."));
    C.InheritedLocalSource.Reset(C.Source);C.InheritedLocalGraph.Reset(C.Graph);C.InheritedLocalModule.Reset(C.Module);
    auto HoldSlot=[&](const FVersionedNiagaraEmitter& Slot)->bool
    {if(!Slot.Emitter.IsResolved())return false;auto* E=Slot.Emitter.Get();if(!E)return true;if(!Loaded(E))return false;C.InheritedSlotHolds.Emplace(E);return true;};
    if(!HoldSlot(C.InitialParent)||!HoldSlot(C.InitialLastMerge))return Fail(R,TEXT("niagara_inherited_parent_unavailable"),TEXT("Existing nullable parent objects must be live before source observers; no loader."));
    TFunction<bool(const FVersionedNiagaraEmitter&,int32)> Visit;
    Visit=[&](const FVersionedNiagaraEmitter& Version,int32 Depth)->bool
    {
        if(!Version.Emitter.IsResolved())return Fail(R,TEXT("niagara_inherited_parent_unresolved"),TEXT("No implicit parent loader is dispatched."));auto* E=Version.Emitter.Get();if(!E)return true;
        if(Depth>32||E==C.InheritedTarget.Get())return Fail(R,TEXT("niagara_inherited_parent_scope_pending"),TEXT("Finite acyclic readonly parent/snapshot closure required; self/cyclic/wider closures remain incomplete."));
        for(const auto& Existing:C.InheritedRoles)if(Existing->Emitter.Get()==E&&Existing->Version==Version.Version)return Existing->Ready||Fail(R,TEXT("niagara_inherited_parent_cycle_pending"),TEXT("Parent/snapshot closure contains a visiting version; no merge/repair/writer."));
        if(C.InheritedRoles.Num()>=64||!Loaded(E)||E->GetClass()!=UNiagaraEmitter::StaticClass()||!Loaded(E->GetOuter())||!Loaded(E->GetOutermost()))return Fail(R,TEXT("niagara_inherited_parent_scope_pending"),TEXT("At most64 loaded native emitter/version source roles required."));
        auto Role=MakeUnique<FInheritedEmitterRole>();auto* V=Role.Get();V->Emitter.Reset(E);V->Class.Reset(E->GetClass());V->Outer.Reset(E->GetOuter());V->Package.Reset(E->GetOutermost());V->Version=Version.Version;V->PathName=E->GetPathName();
        auto* D=E->GetEmitterData(Version.Version);if(!D||!D->GraphSource.IsResolved()||!D->VersionedParent.Emitter.IsResolved()||!D->VersionedParentAtLastMerge.Emitter.IsResolved())return Fail(R,TEXT("niagara_inherited_parent_data_pending"),TEXT("Stored parent GUID must return loaded data with resolved source/parent slots; nullable LastMerge is allowed."));V->Data=D;V->Parent=D->VersionedParent;V->LastMerge=D->VersionedParentAtLastMerge;
        if(!HoldSlot(V->Parent)||!HoldSlot(V->LastMerge))return Fail(R,TEXT("niagara_inherited_parent_unavailable"),TEXT("Nested parent/snapshot slots must be live before graph observers."));
        auto* Source=Cast<UNiagaraScriptSource>(D->GraphSource.Get());UNiagaraGraph* G=nullptr;if(!Loaded(Source)||!Source->NodeGraph.IsResolved()||!OrdinaryGraph(Source,G)||G==C.Graph)return Fail(R,TEXT("niagara_inherited_parent_graph_pending"),TEXT("Loaded ordinary readonly graph distinct from the local writer required; no parent Outer/package/asset-flags restriction is imposed."));
        if(!Loaded(Source->GetClass())||!Loaded(Source->GetOuter())||!Loaded(Source->GetOutermost()))return Fail(R,TEXT("niagara_inherited_source_unavailable"),TEXT("Live captured source class/outer/package roles required without imposing a particular owner."));
        V->Source.Reset(Source);V->Graph.Reset(G);V->SourceClass.Reset(Source->GetClass());V->SourceOuter.Reset(Source->GetOuter());V->SourcePackage.Reset(Source->GetOutermost());V->SourcePath=Source->GetPathName();
        for(const auto& Slot:G->Nodes){if(!Slot.IsResolved())return Fail(R,TEXT("niagara_inherited_source_unavailable"),TEXT("Parent source node slots must already be resolved; no implicit node loader."));UEdGraphNode* N=Slot.Get();if(!Loaded(N))return Fail(R,TEXT("niagara_inherited_source_unavailable"),TEXT("Parent source nodes must be loaded before snapshot."));if(auto* Call=Cast<UNiagaraNodeFunctionCall>(N)){if(!Call->FunctionScript.IsResolved())return Fail(R,TEXT("niagara_inherited_function_source_unresolved"),TEXT("Parent function script slots must already be resolved; no implicit slot loader is admitted."));}}
        C.InheritedRoles.Add(MoveTemp(Role));
        if(!Protect(R,C,Source))return false;for(auto& P:C.Protected)if(P.Graph==G)P.InheritedRole=true;
        if(!Visit(V->Parent,Depth+1)||!Visit(V->LastMerge,Depth+1))return false;V->Ready=true;return true;
    };
    if(!Visit(C.InitialParent,0)||!Visit(C.InitialLastMerge,0))return false;
    C.InheritedProfile=true;R.Data->SetBoolField(TEXT("inherited_local_module_input_supported"),true);R.Data->SetNumberField(TEXT("readonly_parent_version_roles"),C.InheritedRoles.Num());
    R.Data->SetStringField(TEXT("inherited_source_scope"),TEXT("Local standard emitter graph writer only. Public Parent/LastMerge slots and getter-returned version/source/graph roles are held; readonly authored graph state is compared at Engine boundaries. LastMerge may be nested/non-versioned and share the target package: its package dirty follows the target save/dirty lifecycle, not an immutable source boolean. No detach/merge/load/parent writer, compiled payload/cache equivalence or whole inheritance parity is claimed."));
    return InheritedCurrent(R,C,TEXT("parent_capture_complete"));
}
// Follow only public reciprocal parameter-map outputs; reject cycles and multiple
// output identities before calling the resolver-bearing exported input utility.
bool OutputUsage(FResult& R, FContext& C)
{
    TArray<UEdGraphNode*> Pending{C.Module}; TSet<UEdGraphNode*> Seen; UNiagaraNodeOutput* Output = nullptr;
    while (!Pending.IsEmpty())
    {
        UEdGraphNode* N = Pending.Pop(EAllowShrinking::No);
        if (Seen.Contains(N)) { continue; } Seen.Add(N);
        if (Seen.Num() > MaxNodes) { return Fail(R, TEXT("niagara_map_traversal_bound"), TEXT("Parameter-map traversal exceeds the bounded graph.")); }
        if (auto* O = Cast<UNiagaraNodeOutput>(N))
        { if (Output && Output != O) { return Fail(R, TEXT("niagara_module_output_ambiguous"), TEXT("Selected module reaches more than one output usage.")); } Output = O; continue; }
        UEdGraphPin* Out = nullptr;
        if (!MapPin(N, EGPD_Output, Out) || !Out || Out->LinkedTo.IsEmpty()) { return Fail(R, TEXT("niagara_module_output_missing"), TEXT("Selected module has no complete output map chain.")); }
        for (UEdGraphPin* Linked : Out->LinkedTo)
        {
            UEdGraphNode* Next = Linked->GetOwningNode(); UEdGraphPin* In = nullptr;
            if (!MapPin(Next, EGPD_Input, In) || In != Linked || In->LinkedTo.Num() != 1 || In->LinkedTo[0] != Out ||
                Seen.Contains(Next)) { return Fail(R, TEXT("niagara_map_chain_invalid"), TEXT("Ambiguous input or cyclic parameter-map chain is unsupported.")); }
            // A native MapGet consumes the current map as Source but is not a
            // stack-flow successor. The Engine creates this read side branch
            // when it links a User/DI value into another module's override.
            if (ExactNodeClass(Next, TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet")))
            {
                UEdGraphPin* ReadMapOutput = nullptr;
                if (!C.Graph || C.Graph->GetClass() != UNiagaraGraph::StaticClass() ||
                    Next->GetOuter() != C.Graph || Next->GetGraph() != C.Graph || !C.Graph->Nodes.Contains(Next) ||
                    In->PinName != TEXT("Source") || !MapPin(Next, EGPD_Output, ReadMapOutput) || ReadMapOutput)
                { return Fail(R, TEXT("niagara_map_read_branch_invalid"), TEXT("Only an exact own-graph native MapGet Source read leaf without a map output is supported.")); }
                Seen.Add(Next);
                if (Seen.Num() > MaxNodes) { return Fail(R, TEXT("niagara_map_traversal_bound"), TEXT("Parameter-map traversal exceeds the bounded graph.")); }
                double Reads = 0; R.Data->TryGetNumberField(TEXT("parameter_map_read_branches_observed"), Reads);
                R.Data->SetNumberField(TEXT("parameter_map_read_branches_observed"), Reads + 1);
                continue;
            }
            Pending.Add(Next);
        }
    }
    if (!Output) { return Fail(R, TEXT("niagara_module_output_missing"), TEXT("No existing output is reachable from the selected module.")); }
    C.Usage = Output->GetUsage(); R.Data->SetStringField(TEXT("output_usage"), UsageName(C.Usage));
    R.Data->SetStringField(TEXT("output_usage_id"), Guid(Output->GetUsageId())); return true;
}
bool Prepare(FResult& R, FContext& C, const FString& SystemPath, const FString& EI, const FString& EN,
    const FString& MI, const FString& MN, const FString& MG, const FString& InputName, bool bWrite, bool bAllowSourceEdit, bool bLinked)
{
    const auto& Version = FEngineVersion::Current(); FString Object; int32 EmitterIndex, ModuleIndex;
    if (!IsEditorGameThread() || IsPlaySessionActive()) { return Fail(R, TEXT("niagara_editor_state_invalid"), TEXT("Requires Editor game thread outside PIE/SIE.")); }
    if (Version.GetMajor() != 5 || Version.GetMinor() != 8 || Version.GetPatch() != 3) { return Fail(R, TEXT("niagara_version_unsupported"), TEXT("Only UE 5.8.3 is implemented.")); }
    if (!Index(EI, EmitterIndex) || !Index(MI, ModuleIndex) || !Bounded(EN, 256) || !Bounded(MN, 256) || !Bounded(MG, 64) ||
        !Bounded(InputName, 256) || InputName.IsEmpty() || !ObjectPath(SystemPath, Object))
    { return Fail(R, TEXT("niagara_module_input_selector_invalid"), TEXT("Exact bounded names, GUID filter and int32/null indices are required.")); }
    FGuid ParsedGuid; if (!MG.IsEmpty() && !FGuid::Parse(MG, ParsedGuid)) { return Fail(R, TEXT("niagara_module_guid_invalid"), TEXT("module_node_guid must be empty or a valid GUID.")); }
    C.System = FindObject<UNiagaraSystem>(nullptr, *Object);
    if (!Loaded(C.System) || C.System->GetClass() != UNiagaraSystem::StaticClass() || !C.System->IsAsset() ||
        C.System->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly | PKG_PlayInEditor | PKG_CompiledIn) || C.System->GetEmitterHandles().Num() > MaxEmitters)
    { return Fail(R, TEXT("niagara_loaded_system_required"), TEXT("Load the exact ordinary native NiagaraSystem with official tools before this find-only operation.")); }
    C.KeepSystem.Reset(C.System);
    const FString Package = C.System->GetOutermost()->GetName();
    const bool Temp = Package.StartsWith(TEXT("/Game/_MCP_Temp/NiagaraGenerated/")) || Package.StartsWith(TEXT("/Game/CubelessMCPTests/"));
    if (bWrite && (!Package.StartsWith(TEXT("/Game/")) || (!Temp && !bAllowSourceEdit)))
    { return Fail(R, TEXT("niagara_write_scope_forbidden"), TEXT("Writes require isolated generated/test roots or explicit source edit on /Game.")); }
    if (bWrite)
    {
        for (TObjectIterator<UNiagaraComponent> It; It; ++It)
        { if (IsValid(*It) && !(*It)->IsTemplate() && (*It)->GetAsset() == C.System && (*It)->GetWorld()) { return Fail(R, TEXT("niagara_system_has_world_instances"), TEXT("Refuses authoring a system used by live world components.")); } }
        if (C.System->HasOutstandingCompilationRequests(true)) { return Fail(R, TEXT("niagara_compile_outstanding"), TEXT("Inspect readiness before editing; this API never waits or flushes compilation.")); }
    }
    UClass* SchemaClass = FindObject<UClass>(nullptr, TEXT("/Script/NiagaraEditor.EdGraphSchema_Niagara"));
    if (!IsValid(SchemaClass) || SchemaClass != UEdGraphSchema_Niagara::StaticClass() || !IsValid(SchemaClass->GetDefaultObject(false)))
    { return Fail(R, TEXT("niagara_schema_not_loaded"), TEXT("Exact existing Niagara schema/default is required.")); }
    C.Schema = Cast<UEdGraphSchema_Niagara>(SchemaClass->GetDefaultObject(false));
    for (int32 I = 0; I < C.System->GetEmitterHandles().Num() && !C.Module; ++I)
    {
        const auto& H = C.System->GetEmitterHandles()[I];
        if ((EmitterIndex != INDEX_NONE && EmitterIndex != I) || (!EN.IsEmpty() && !EN.Equals(H.GetName().ToString(), ESearchCase::IgnoreCase))) { continue; }
        auto* D = H.GetEmitterData(); auto Instance = H.GetInstance(); UNiagaraGraph* G = nullptr;
        if (H.GetEmitterMode() != ENiagaraEmitterMode::Standard || !D || !Loaded(Instance.Emitter) || !Instance.Version.IsValid() ||
            Instance.Emitter->GetEmitterData(Instance.Version) != D || !Instance.Emitter->IsIn(C.System) ||
            !OrdinaryGraph(Cast<UNiagaraScriptSource>(D->GraphSource), G))
        { return Fail(R, TEXT("niagara_emitter_source_invalid"), TEXT("Selected emitter must be an already-loaded standard version owned by the exact system.")); }
        FSnapshot Candidate = Capture(G); if (!Candidate.bComplete) { return Fail(R, TEXT("niagara_graph_observation_incomplete"), TEXT("Emitter graph pin/link observation is incomplete before selection.")); }
        TArray<UNiagaraNodeFunctionCall*> Calls;
        for (UEdGraphNode* N : G->Nodes) { if (auto* F = Cast<UNiagaraNodeFunctionCall>(N)) { Calls.Add(F); } }
        Calls.Sort([](const UNiagaraNodeFunctionCall& A, const UNiagaraNodeFunctionCall& B) { return A.NodePosY != B.NodePosY ? A.NodePosY < B.NodePosY : A.NodePosX < B.NodePosX; });
        for (int32 J = 0; J < Calls.Num(); ++J)
        {
            auto* F = Calls[J]; FString Name = F->GetFunctionName(); if (Name.IsEmpty()) { Name = F->Signature.GetNameString(); }
            if ((ModuleIndex != INDEX_NONE && ModuleIndex != J) || (!MN.IsEmpty() && !MN.Equals(Name, ESearchCase::IgnoreCase)) ||
                (!MG.IsEmpty() && F->NodeGuid != ParsedGuid)) { continue; }
            C.Module = F; C.Emitter = Instance; C.Data = D; C.Graph = G; C.Source = Cast<UNiagaraScriptSource>(D->GraphSource);
            C.Before = MoveTemp(Candidate); C.EmitterIndex = I; C.ModuleIndex = J; C.EmitterName = H.GetName().ToString(); C.ModuleName = Name; break;
        }
    }
    if (!C.Module) { return Fail(R, TEXT("niagara_module_not_found"), TEXT("No matching legacy first-match emitter/function-call selector exists.")); }
    C.ModuleScript = C.Module->FunctionScript;
    if (!Loaded(C.ModuleScript) || C.ModuleScript->GetClass() != UNiagaraScript::StaticClass() ||
        !C.ModuleScript->GetScriptData(C.Module->SelectedScriptVersion))
    { return Fail(R, TEXT("niagara_loaded_function_source_required"), TEXT("Selected native function/script version must already be loaded before source resolution.")); }
    const FString ScriptPath = Path(C.ModuleScript);
    if (!ScriptPath.StartsWith(TEXT("/Game/")) && !ScriptPath.StartsWith(TEXT("/Niagara/")) && !ScriptPath.StartsWith(TEXT("/Engine/")))
    { return Fail(R, TEXT("niagara_function_runtime_dependency_forbidden"), TEXT("Selected native function source must reside in /Game, /Niagara or /Engine; Cubeless/MCP plugin content cannot become a runtime dependency.")); }
    C.KeepModuleScript.Reset(C.ModuleScript);
    C.ModuleSource = Cast<UNiagaraScriptSource>(C.ModuleScript->GetSource(C.Module->SelectedScriptVersion));
    if (
        !OrdinaryGraph(C.ModuleSource, C.ModuleGraph) || C.ModuleGraph == C.Graph ||
        C.ModuleSource->GetOutermost() != C.ModuleScript->GetOutermost())
    { return Fail(R, TEXT("niagara_loaded_function_source_required"), TEXT("Selected version must have an already-loaded ordinary native function source; no load or signature-only fallback.")); }
    if (!MapPin(C.Module, EGPD_Input, C.ModuleMapIn) || !C.ModuleMapIn || C.ModuleMapIn->LinkedTo.Num() != 1 ||
        !OutputUsage(R, C)) { if (R.ErrorCode.IsEmpty()) { Fail(R, TEXT("niagara_override_chain_invalid"), TEXT("A unique reciprocal parameter-map predecessor is required before exported helpers.")); } return false; }
    C.OverrideNode = ExactNodeClass(C.ModuleMapIn->LinkedTo[0]->GetOwningNode(), TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapSet")) ? C.ModuleMapIn->LinkedTo[0]->GetOwningNode() : nullptr;
    C.PreviousMapOut = C.ModuleMapIn->LinkedTo[0];
    if (C.OverrideNode)
    {
        UEdGraphPin* In = nullptr; UEdGraphPin* Out = nullptr;
        if (!MapPin(C.OverrideNode, EGPD_Input, In) || !In || In->LinkedTo.Num() != 1 || !MapPin(C.OverrideNode, EGPD_Output, Out) || Out != C.ModuleMapIn->LinkedTo[0])
        { return Fail(R, TEXT("niagara_override_chain_invalid"), TEXT("Existing override MapSet needs one valid predecessor and exact module map output.")); }
    }
    C.Dirty = C.System->GetOutermost()->IsDirty(); C.HandlesBefore = Handles(C.System);
    bool Complete = false; C.UsersBefore = UserState(C.System, Complete);
    if (!Complete) { return Fail(R, TEXT("niagara_user_state_unobservable"), TEXT("User store preservation cannot be established before mutation.")); }
    C.ParametersBefore = GraphParameters(C.Graph, Complete);
    if (!Complete) { return Fail(R, TEXT("niagara_graph_parameters_unobservable"), TEXT("Target graph parameter metadata preservation cannot be established before mutation.")); }
    if (!Protect(R, C, C.ModuleSource)) { return false; }
    auto AddOther = [&](UNiagaraScriptSourceBase* S) { return Protect(R, C, Cast<UNiagaraScriptSource>(S)); };
    for (const auto& H : C.System->GetEmitterHandles())
    { const auto* D = H.GetEmitterData(); if (H.GetEmitterMode() != ENiagaraEmitterMode::Standard || !D || !AddOther(D->GraphSource)) { return Fail(R, TEXT("niagara_other_emitter_unobservable"), TEXT("All standard emitter source graphs must be observable for preservation.")); } }
    for (UNiagaraScript* S : {C.System->GetSystemSpawnScript(), C.System->GetSystemUpdateScript()})
    { if (!Loaded(S) || !AddOther(S->GetLatestSource())) { return Fail(R, TEXT("niagara_system_source_unobservable"), TEXT("System script source preservation is incomplete.")); } }
    if(bWrite&&!CaptureInherited(R,C))return false;
    FCompileConstantResolver Resolver(C.Emitter, C.Usage, C.Module->DebugState);
    TArray<FNiagaraVariable> Inputs; TSet<FNiagaraVariable> Hidden;
    FNiagaraStackGraphUtilities::GetStackFunctionInputs(*C.Module, Inputs, Hidden, Resolver,
        FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions::ModuleInputsOnly, true);
    if(!InheritedCurrent(R,C,TEXT("after_input_enumeration")))return false;
    if (Inputs.Num() > MaxParameters) { return Fail(R, TEXT("niagara_input_bound_exceeded"), TEXT("Resolver input enumeration exceeds the finite bound.")); }
    auto Match = [&](const FNiagaraVariable& V) { FString Short = V.GetName().ToString(); Short.RemoveFromStart(TEXT("Module.")); return V.GetName().ToString().Equals(InputName, ESearchCase::IgnoreCase) || Short.Equals(InputName, ESearchCase::IgnoreCase); };
    int32 Count = 0; for (const auto& V : Inputs) { if (Match(V)) { C.Input = V; ++Count; } }
    if (Count == 0 && bLinked)
    { for (const auto& P : C.ModuleGraph->GetAllMetaData()) { if (Match(P.Key)) { C.Input = P.Key; ++Count; } } R.Data->SetBoolField(TEXT("scratch_metadata_fallback_used"), Count > 0); }
    if (Count != 1 || !C.Input.GetType().IsValid()) { return Fail(R, TEXT("niagara_module_input_not_unique"), TEXT("No unique typed module input matches full or Module-stripped name.")); }
    const FNiagaraParameterHandle Handle(C.Input.GetName());
    C.Alias = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(Handle.IsModuleHandle() ? Handle : FNiagaraParameterHandle::CreateModuleParameterHandle(C.Input.GetName()), C.Module);
    for (UEdGraphPin* P : C.Module->Pins)
    { if (P && P->Direction == EGPD_Input && P != C.ModuleMapIn && P->PinName == C.Alias.GetName()) { return Fail(R, TEXT("niagara_static_switch_override_unsupported"), TEXT("Static switch input defaults are not ordinary value overrides.")); } }
    if (C.OverrideNode)
    { for (UEdGraphPin* P : C.OverrideNode->Pins) { if (P->Direction == EGPD_Input && P->PinName == C.Alias.GetParameterHandleString()) { if (C.Existing) { return Fail(R, TEXT("niagara_override_pin_ambiguous"), TEXT("Duplicate alias override pins are unsupported.")); } C.Existing = P; } } }
    if (C.Existing && (C.Existing->bOrphanedPin || C.Existing->PinType.ContainerType != EPinContainerType::None ||
        UEdGraphSchema_Niagara::PinToTypeDefinition(C.Existing) != C.Input.GetType()))
    { return Fail(R, TEXT("niagara_override_pin_type_mismatch"), TEXT("Existing exact alias override must retain the resolved input type.")); }
    R.Data->SetStringField(TEXT("system_path"), Path(C.System)); R.Data->SetStringField(TEXT("emitter_name"), C.EmitterName);
    R.Data->SetNumberField(TEXT("emitter_index"), C.EmitterIndex); R.Data->SetStringField(TEXT("emitter_version"), Guid(C.Emitter.Version));
    R.Data->SetStringField(TEXT("module_name"), C.ModuleName); R.Data->SetNumberField(TEXT("module_index"), C.ModuleIndex);
    R.Data->SetStringField(TEXT("module_node_guid"), Guid(C.Module->NodeGuid)); R.Data->SetStringField(TEXT("module_node_path"), Path(C.Module));
    R.Data->SetStringField(TEXT("module_script_path"), Path(C.ModuleScript)); R.Data->SetStringField(TEXT("module_source_path"), Path(C.ModuleSource));
    R.Data->SetStringField(TEXT("input_name"), C.Input.GetName().ToString()); R.Data->SetStringField(TEXT("input_type"), C.Input.GetType().GetName());
    R.Data->SetObjectField(TEXT("input_type_metadata"), TypeRow(C.Input.GetType()));
    R.Data->SetStringField(TEXT("aliased_input_handle"), C.Alias.GetParameterHandleString().ToString());
    R.Data->SetNumberField(TEXT("previous_override_link_count"), C.Existing ? C.Existing->LinkedTo.Num() : 0);
    R.Data->SetStringField(TEXT("previous_default_value"), C.Existing ? C.Existing->DefaultValue : FString());
    R.Data->SetBoolField(TEXT("override_pin_created"), false); R.Data->SetBoolField(TEXT("overwrote_existing"), false);
    R.Data->SetObjectField(TEXT("before"), C.Before.Json); R.Data->SetObjectField(TEXT("graph_before"), C.Before.Json);
    R.Data->SetObjectField(TEXT("user_parameters_before"), C.UsersBefore); R.Data->SetObjectField(TEXT("emitter_handles_before"), C.HandlesBefore);
    R.Data->SetObjectField(TEXT("graph_parameters_before"), C.ParametersBefore); R.Data->SetBoolField(TEXT("package_dirty_before"), C.Dirty);
    R.Data->SetStringField(TEXT("asset_write_scope"), Temp ? TEXT("target_temp_module_input_override") : TEXT("source_module_input_override"));
    R.Data->SetObjectField(TEXT("compile_state"), CompileState(C.System)); return true;
}

bool JsonValue(const FString& Json, TSharedPtr<FJsonValue>& Out)
{
    if (Json.Len() > 16384) { return false; } TArray<TSharedPtr<FJsonValue>> A;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[") + Json + TEXT("]")), A) || A.Num() != 1 || !A[0].IsValid()) { return false; }
    Out = A[0]; return true;
}
bool FloatNumber(const TSharedPtr<FJsonValue>& V, float& Out)
{
    double N; if (!V.IsValid() || V->Type != EJson::Number || !V->TryGetNumber(N) || !FMath::IsFinite(N) ||
        N < -static_cast<double>(MAX_flt) || N > static_cast<double>(MAX_flt)) { return false; }
    Out = static_cast<float>(N); return FMath::IsFinite(Out);
}
bool IntNumber(const TSharedPtr<FJsonValue>& V, int32& Out)
{
    double N; if (!V.IsValid() || V->Type != EJson::Number || !V->TryGetNumber(N) || !FMath::IsFinite(N) ||
        N != FMath::TruncToDouble(N) || N < static_cast<double>(MIN_int32) || N > static_cast<double>(MAX_int32)) { return false; }
    Out = static_cast<int32>(N); return true;
}
TSharedPtr<FJsonValue> Numbers(std::initializer_list<double> Values)
{
    TArray<TSharedPtr<FJsonValue>> A; for (double V : Values) { A.Add(MakeShared<FJsonValueNumber>(V)); }
    return MakeShared<FJsonValueArray>(A);
}
bool Literal(FResult& R, const FNiagaraTypeDefinition& T, const TSharedPtr<FJsonValue>& V,
    FNiagaraVariable& Out, TSharedPtr<FJsonValue>& Actual)
{
    if (T.IsDataInterface() || T.IsUObject()) { return Fail(R, TEXT("niagara_literal_type_unsupported"), TEXT("Data Interface and UObject inputs require their dedicated route.")); }
    Out = FNiagaraVariable(T, NAME_None); float F; int32 I; bool B;
    if (T == FNiagaraTypeDefinition::GetFloatDef() && T.GetSize() == sizeof(float))
    { if (!FloatNumber(V, F)) { return Fail(R, TEXT("niagara_literal_value_invalid"), TEXT("Float input requires a finite float-representable number.")); } Out.SetValue<float>(F); Actual = MakeShared<FJsonValueNumber>(F); }
    else if (T == FNiagaraTypeDefinition::GetIntDef() && T.GetSize() == sizeof(int32))
    { if (!IntNumber(V, I)) { return Fail(R, TEXT("niagara_literal_value_invalid"), TEXT("Integer input requires exact int32; legacy fractional truncation is deliberately rejected before mutation.")); } Out.SetValue<int32>(I); Actual = MakeShared<FJsonValueNumber>(I); }
    else if (T == FNiagaraTypeDefinition::GetBoolDef() && T.GetSize() == sizeof(FNiagaraBool))
    { if (!V.IsValid() || V->Type != EJson::Boolean || !V->TryGetBool(B)) { return Fail(R, TEXT("niagara_literal_value_invalid"), TEXT("Bool input requires a JSON boolean.")); } Out.SetValue<FNiagaraBool>(FNiagaraBool(B)); Actual = MakeShared<FJsonValueBoolean>(B); }
    else if (T.IsEnum() && Loaded(T.GetEnum()) && T.GetSize() == sizeof(FNiagaraInt32) &&
        (Path(T.GetEnum()).StartsWith(TEXT("/Game/")) || Path(T.GetEnum()).StartsWith(TEXT("/Script/"))) &&
        !Path(T.GetEnum()).Contains(TEXT("Cubeless"), ESearchCase::IgnoreCase) &&
        !Path(T.GetEnum()).Contains(TEXT("ModelContextProtocol"), ESearchCase::IgnoreCase))
    {
        const UEnum* Enum = T.GetEnum(); int32 Found = INDEX_NONE;
        if (Enum->NumEnums() > 4096) { return Fail(R, TEXT("niagara_enum_bound_exceeded"), TEXT("Enum has more than 4096 declared entries.")); }
        if (IntNumber(V, I)) { Found = Enum->GetIndexByValue(I); }
        else if (V.IsValid() && V->Type == EJson::String)
        {
            const FString Name = V->AsString().TrimStartAndEnd(); int32 Matches = 0;
            if (!Bounded(Name, 256) || Name.IsEmpty()) { return Fail(R, TEXT("niagara_literal_value_invalid"), TEXT("Enum name must be bounded and nonempty.")); }
            for (int32 E = 0; E < Enum->NumEnums(); ++E)
            { if (Enum->GetNameStringByIndex(E).Equals(Name, ESearchCase::IgnoreCase) || Enum->GetNameByIndex(E).ToString().Equals(Name, ESearchCase::IgnoreCase) ||
                Enum->GetAuthoredNameStringByIndex(E).Equals(Name, ESearchCase::IgnoreCase) || Enum->GetDisplayNameTextByIndex(E).ToString().Equals(Name, ESearchCase::IgnoreCase)) { Found = E; ++Matches; } }
            if (Matches != 1) { Found = INDEX_NONE; }
        }
        if (Found == INDEX_NONE || Enum->HasMetaData(TEXT("Hidden"), Found) || Enum->GetNameStringByIndex(Found).EndsWith(TEXT("_MAX"), ESearchCase::IgnoreCase) ||
            Enum->GetNameStringByIndex(Found).Equals(TEXT("MAX"), ESearchCase::IgnoreCase) || Enum->GetValueByIndex(Found) < MIN_int32 || Enum->GetValueByIndex(Found) > MAX_int32)
        { return Fail(R, TEXT("niagara_literal_value_invalid"), TEXT("Enum requires one visible non-terminal exact name/value within int32.")); }
        FNiagaraInt32 E; E.Value = static_cast<int32>(Enum->GetValueByIndex(Found)); Out.SetValue<FNiagaraInt32>(E); Actual = MakeShared<FJsonValueString>(Enum->GetNameStringByIndex(Found));
    }
    else
    {
        const TArray<TSharedPtr<FJsonValue>>* A = nullptr; TArray<float> N;
        if (!V.IsValid() || !V->TryGetArray(A) || A->Num() > 16) { return Fail(R, TEXT("niagara_literal_value_invalid"), TEXT("Vector/color input requires a bounded numeric array.")); }
        for (const auto& E : *A) { if (!FloatNumber(E, F)) { return Fail(R, TEXT("niagara_literal_value_invalid"), TEXT("Every vector/color element must be finite and float-representable.")); } N.Add(F); }
        if (T == FNiagaraTypeDefinition::GetColorDef() && T.GetSize() == sizeof(FLinearColor) && N.Num() >= 3)
        { const float Alpha = N.Num() > 3 ? N[3] : 1.0f; Out.SetValue<FLinearColor>(FLinearColor(N[0], N[1], N[2], Alpha)); Actual = Numbers({N[0], N[1], N[2], Alpha}); }
        else if (T == FNiagaraTypeDefinition::GetVec2Def() && T.GetSize() == sizeof(FVector2f) && N.Num() >= 2)
        { Out.SetValue<FVector2f>(FVector2f(N[0], N[1])); Actual = Numbers({N[0], N[1]}); }
        else if (T == FNiagaraTypeDefinition::GetVec3Def() && T.GetSize() == sizeof(FVector3f) && N.Num() >= 3)
        { Out.SetValue<FVector3f>(FVector3f(N[0], N[1], N[2])); Actual = Numbers({N[0], N[1], N[2]}); }
        else if (T == FNiagaraTypeDefinition::GetVec4Def() && T.GetSize() == sizeof(FVector4f) && N.Num() >= 4)
        { Out.SetValue<FVector4f>(FVector4f(N[0], N[1], N[2], N[3])); Actual = Numbers({N[0], N[1], N[2], N[3]}); }
        else if (T == FNiagaraTypeDefinition::GetPositionDef() && T.GetSize() == sizeof(FNiagaraPosition) && N.Num() >= 3)
        { Out.SetValue<FNiagaraPosition>(FNiagaraPosition(N[0], N[1], N[2])); Actual = Numbers({N[0], N[1], N[2]}); }
        else { return Fail(R, TEXT("niagara_literal_type_or_arity_unsupported"), TEXT("Only legacy scalar, enum, Color and Vec2/3/4/Position types with their installed size and minimum arity are implemented.")); }
    }
    return Out.IsDataAllocated();
}
struct FExportFields
{
    bool CallbackSet = false, ModeSet = false, FixedSet = false, PerParticleSet = false;
    FNiagaraUserParameterBinding Callback; ENDIExport_GPUAllocationMode Mode = ENDIExport_GPUAllocationMode::FixedSize;
    int32 Fixed = 64; float PerParticle = 1.0f; TSharedRef<FJsonObject> Requested = MakeShared<FJsonObject>();
};
TSharedRef<FJsonObject> ExportFields(const UNiagaraDataInterfaceExport* DI)
{
    auto J = MakeShared<FJsonObject>();
    J->SetStringField(TEXT("CallbackHandlerParameter"), DI->CallbackHandlerParameter.Parameter.GetName().ToString());
    J->SetObjectField(TEXT("CallbackHandlerParameterType"), TypeRow(DI->CallbackHandlerParameter.Parameter.GetType()));
    J->SetStringField(TEXT("GPUAllocationMode"), DI->GPUAllocationMode == ENDIExport_GPUAllocationMode::FixedSize ? TEXT("FixedSize") :
        (DI->GPUAllocationMode == ENDIExport_GPUAllocationMode::PerParticle ? TEXT("PerParticle") : TEXT("invalid")));
    J->SetNumberField(TEXT("GPUAllocationFixedSize"), DI->GPUAllocationFixedSize);
    J->SetNumberField(TEXT("GPUAllocationPerParticleSize"), DI->GPUAllocationPerParticleSize);
    return J;
}
bool ScalarPropertyValue(const TSharedPtr<FJsonValue>& V, TSharedPtr<FJsonValue>& Out)
{
    if (!V.IsValid()) { return false; }
    if (V->Type == EJson::String)
    {
        const FString S = V->AsString().TrimStartAndEnd();
        if (!Bounded(S, 256) || !JsonValue(S, Out) || Out->Type != EJson::Number) { return false; }
        return true;
    }
    Out = V; return V->Type == EJson::Number;
}
bool PrepareExport(FResult& R, const FNiagaraTypeDefinition& Type, const FString& Json, FExportFields& F)
{
    UClass* Class = FindObject<UClass>(nullptr, TEXT("/Script/Niagara.NiagaraDataInterfaceExport"));
    if (!IsValid(Class) || Class != UNiagaraDataInterfaceExport::StaticClass() || !Type.IsDataInterface() || Type.GetClass() != Class ||
        Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
    { return Fail(R, TEXT("niagara_data_interface_class_unsupported"), TEXT("This finite route implements only the exact loaded native NiagaraDataInterfaceExport class.")); }
    const FString Input = Json.IsEmpty() ? TEXT("{}") : Json;
    TSharedPtr<FJsonObject> Parsed;
    if (Input.Len() > 16384 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input), Parsed) || !Parsed.IsValid() || Parsed->Values.Num() > 4)
    { return Fail(R, TEXT("niagara_di_properties_invalid"), TEXT("properties must be an object containing only the four public Export DI fields.")); }
    F.Requested = Parsed.ToSharedRef();
    for (const auto& Pair : F.Requested->Values)
    {
        const FString K(Pair.Key); const auto& V = Pair.Value; TSharedPtr<FJsonValue> Number;
        if (K == TEXT("CallbackHandlerParameter"))
        {
            const FString S = V.IsValid() && V->Type == EJson::String ? V->AsString() : FString();
            FStructProperty* P = FindFProperty<FStructProperty>(Class, TEXT("CallbackHandlerParameter"));
            if (!P || P->GetOwnerClass() != Class || !P->HasAllPropertyFlags(CPF_NativeAccessSpecifierPublic) ||
                P->Struct != FNiagaraUserParameterBinding::StaticStruct() || !Bounded(S, 2048) || S.IsEmpty())
            { return Fail(R, TEXT("niagara_export_callback_invalid"), TEXT("CallbackHandlerParameter requires the exact public User-binding struct text.")); }
            // Finite producer form only: no arbitrary nested property/type/data
            // fields may be imported even if the generic importer ignores them.
            const FString Clean = S.TrimStartAndEnd(); const FString Prefix = TEXT("(Parameter=(Name=");
            if (!Clean.StartsWith(Prefix) || !Clean.EndsWith(TEXT("))")))
            { return Fail(R, TEXT("niagara_export_callback_invalid"), TEXT("Supported callback text is (Parameter=(Name=\"User.Name\")) only.")); }
            FString NameText = Clean.Mid(Prefix.Len(), Clean.Len() - Prefix.Len() - 2).TrimStartAndEnd();
            if (NameText.StartsWith(TEXT("\"")) && NameText.EndsWith(TEXT("\"")) && NameText.Len() >= 2) { NameText = NameText.Mid(1, NameText.Len() - 2); }
            if (!NameText.StartsWith(TEXT("User.")) || NameText.Len() <= 5 || !Bounded(NameText, 256))
            { return Fail(R, TEXT("niagara_export_callback_invalid"), TEXT("Callback requires a qualified nonempty User name.")); }
            for (TCHAR Character : NameText)
            { if (FChar::IsWhitespace(Character) || Character == TCHAR('(') || Character == TCHAR(')') || Character == TCHAR(',') ||
                Character == TCHAR('=') || Character == TCHAR('"') || Character == TCHAR('\\'))
                { return Fail(R, TEXT("niagara_export_callback_invalid"), TEXT("Callback text contains an unsupported nested field, escape or name character.")); } }
            // Parse into an independent stack value before any graph allocation.
            const TCHAR* End = P->ImportText_Direct(*S, &F.Callback, nullptr, PPF_None);
            if (!End || !FString(End).TrimStartAndEnd().IsEmpty() || F.Callback.Parameter.GetType() != FNiagaraTypeDefinition::GetUObjectDef() ||
                !F.Callback.Parameter.GetName().ToString().StartsWith(TEXT("User.")) || F.Callback.Parameter.GetName().ToString().Len() <= 5 ||
                !Bounded(F.Callback.Parameter.GetName().ToString(), 256))
            { return Fail(R, TEXT("niagara_export_callback_invalid"), TEXT("Callback must retain exact UObject type and a qualified nonempty User parameter name; no arbitrary struct type is accepted.")); }
            F.CallbackSet = true;
        }
        else if (K == TEXT("GPUAllocationMode"))
        {
            FString S = V.IsValid() && V->Type == EJson::String ? V->AsString().TrimStartAndEnd() : FString(); int32 N;
            S.RemoveFromStart(TEXT("ENDIExport_GPUAllocationMode::"));
            if (S.Equals(TEXT("FixedSize"), ESearchCase::IgnoreCase) || (IntNumber(V, N) && N == static_cast<int32>(ENDIExport_GPUAllocationMode::FixedSize))) { F.Mode = ENDIExport_GPUAllocationMode::FixedSize; }
            else if (S.Equals(TEXT("PerParticle"), ESearchCase::IgnoreCase) || (IntNumber(V, N) && N == static_cast<int32>(ENDIExport_GPUAllocationMode::PerParticle))) { F.Mode = ENDIExport_GPUAllocationMode::PerParticle; }
            else { return Fail(R, TEXT("niagara_export_allocation_invalid"), TEXT("GPUAllocationMode requires FixedSize or PerParticle (or their exact public enum values).")); }
            F.ModeSet = true;
        }
        else if (K == TEXT("GPUAllocationFixedSize"))
        { if (!ScalarPropertyValue(V, Number) || !IntNumber(Number, F.Fixed) || F.Fixed < 0) { return Fail(R, TEXT("niagara_export_allocation_invalid"), TEXT("GPUAllocationFixedSize requires a nonnegative exact int32.")); } F.FixedSet = true; }
        else if (K == TEXT("GPUAllocationPerParticleSize"))
        { if (!ScalarPropertyValue(V, Number) || !FloatNumber(Number, F.PerParticle) || F.PerParticle < 0.0f) { return Fail(R, TEXT("niagara_export_allocation_invalid"), TEXT("GPUAllocationPerParticleSize requires a finite nonnegative float.")); } F.PerParticleSet = true; }
        else { return Fail(R, TEXT("niagara_di_property_unsupported"), TEXT("Only CallbackHandlerParameter, GPUAllocationMode, GPUAllocationFixedSize and GPUAllocationPerParticleSize are implemented; arbitrary DI reflection is excluded.")); }
    }
    return true;
}
TArray<TSharedPtr<FJsonValue>> DIInventory(const FContext& C, UEdGraphPin* Override)
{
    TArray<TSharedPtr<FJsonValue>> Rows;
    if (!Override || Override->LinkedTo.Num() != 1) { return Rows; }
    auto* Input = Cast<UNiagaraNodeInput>(Override->LinkedTo[0]->GetOwningNode());
    if (!Input) { return Rows; }
    for (TObjectIterator<UNiagaraDataInterfaceExport> It; It; ++It)
    {
        auto* DI = *It;
        if (Loaded(DI) && DI->GetClass() == UNiagaraDataInterfaceExport::StaticClass() && DI->GetOuter() == Input && DI->GetOutermost() == C.System->GetOutermost())
        {
            auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("object_path"), Path(DI)); J->SetStringField(TEXT("object_class"), Path(DI->GetClass()));
            J->SetStringField(TEXT("owner_node_path"), Path(Input)); J->SetObjectField(TEXT("public_fields"), ExportFields(DI));
            J->SetBoolField(TEXT("private_node_input_reference_verified"), false); Rows.Add(MakeShared<FJsonValueObject>(J));
            if (Rows.Num() > 32) { break; }
        }
    }
    return Rows;
}

void AddEdge(TMap<FString, TArray<FString>>& Links, const FString& A, const FString& B)
{ Links.FindOrAdd(A).AddUnique(B); Links.FindOrAdd(B).AddUnique(A); }
void RemoveEdge(TMap<FString, TArray<FString>>& Links, const FString& A, const FString& B)
{ if (auto* L = Links.Find(A)) { L->Remove(B); } if (auto* L = Links.Find(B)) { L->Remove(A); } }
bool ObserveProtected(FResult& R, const FContext& C)
{
    bool All = true; TArray<TSharedPtr<FJsonValue>> Rows;
    for (const auto& P : C.Protected)
    {
        auto Row = MakeShared<FJsonObject>(); const FSnapshot After = Capture(P.Graph);
        bool Complete = false; auto Params = GraphParameters(P.Graph, Complete);
        // Dirty is one UPackage flag shared by every graph in the target package,
        // including readonly system-script graphs, not just inherited snapshots.
        const bool SharedTargetPackage = P.Graph->GetOutermost() == C.System->GetOutermost();
        const bool DirtyAfter = P.Graph->GetOutermost()->IsDirty();
        const bool Preserved = After.bComplete && Complete && SameGraph(P.Before, After) &&
            Text(P.Before.Json) == Text(After.Json) && Text(P.Parameters) == Text(Params) && (P.Dirty == DirtyAfter || SharedTargetPackage) &&
            Loaded(P.Source) && P.Source->NodeGraph.Get() == P.Graph;
        Row->SetStringField(TEXT("graph_path"), Path(P.Graph)); Row->SetObjectField(TEXT("before"), P.Before.Json);
        Row->SetObjectField(TEXT("after"), After.Json); Row->SetObjectField(TEXT("parameters_before"), P.Parameters); Row->SetObjectField(TEXT("parameters_after"), Params);
        Row->SetBoolField(TEXT("dirty_before"), P.Dirty); Row->SetBoolField(TEXT("dirty_after"), DirtyAfter);
        Row->SetBoolField(TEXT("inherited_readonly_role"),P.InheritedRole);Row->SetBoolField(TEXT("dirty_shared_with_target_lifecycle"),SharedTargetPackage);
        Row->SetBoolField(TEXT("preserved"), Preserved); Rows.Add(MakeShared<FJsonValueObject>(Row)); All &= Preserved;
        if (!Preserved && !R.Data->HasField(TEXT("first_protected_source_mismatch")))
        {
            auto Mismatch = MakeShared<FJsonObject>();
            Mismatch->SetStringField(TEXT("object_path"), Path(P.Graph));
            Mismatch->SetBoolField(TEXT("dirty_shared_with_target_lifecycle"), SharedTargetPackage);
            if (!After.bComplete || !Complete)
            { Mismatch->SetStringField(TEXT("property"),TEXT("graph_and_parameter_observation_complete"));Mismatch->SetBoolField(TEXT("expected"),true);Mismatch->SetBoolField(TEXT("actual"),false); }
            else if (!SameGraph(P.Before, After) || Text(P.Before.Json) != Text(After.Json))
            { Mismatch->SetStringField(TEXT("property"),TEXT("authored_graph_snapshot"));Mismatch->SetObjectField(TEXT("expected"),P.Before.Json);Mismatch->SetObjectField(TEXT("actual"),After.Json); }
            else if (Text(P.Parameters) != Text(Params))
            { Mismatch->SetStringField(TEXT("property"),TEXT("authored_graph_parameters"));Mismatch->SetObjectField(TEXT("expected"),P.Parameters);Mismatch->SetObjectField(TEXT("actual"),Params); }
            else if (P.Dirty != DirtyAfter && !SharedTargetPackage)
            { Mismatch->SetStringField(TEXT("object_path"),Path(P.Graph->GetOutermost()));Mismatch->SetStringField(TEXT("property"),TEXT("UPackage.bDirty"));Mismatch->SetBoolField(TEXT("expected"),P.Dirty);Mismatch->SetBoolField(TEXT("actual"),DirtyAfter); }
            else
            { Mismatch->SetStringField(TEXT("object_path"),Path(P.Source));Mismatch->SetStringField(TEXT("property"),TEXT("NodeGraph"));Mismatch->SetStringField(TEXT("expected"),Path(P.Graph));Mismatch->SetStringField(TEXT("actual"),Loaded(P.Source)?Path(P.Source->NodeGraph.Get()):TEXT("source_unavailable")); }
            R.Data->SetObjectField(TEXT("first_protected_source_mismatch"),Mismatch);
        }
        if (!After.bComplete || !Complete) { R.bChangeStateKnown = false; Fail(R, TEXT("niagara_source_state_unobservable"), TEXT("Actual protected graph/parameter state is incomplete after dispatch; stop without retry.")); }
        else if (!Preserved) { R.bChanged = true; Fail(R, TEXT("niagara_source_changed"), TEXT("A protected module/system/unselected emitter source changed; observed state retained.")); }
    }
    R.Data->SetArrayField(TEXT("protected_sources"), Rows); R.Data->SetBoolField(TEXT("source_graphs_preserved"), All);
    R.Data->SetBoolField(TEXT("module_source_preserved"), All); return All;
}
bool ObserveCommon(FResult& R, FContext& C, FSnapshot& After)
{
    if(!InheritedCurrent(R,C,TEXT("observe_common")))return false;
    After = Capture(C.Graph); bool UserComplete = false, ParamsComplete = false;
    auto Users = UserState(C.System, UserComplete); auto Params = GraphParameters(C.Graph, ParamsComplete); auto CurrentHandles = Handles(C.System);
    R.Data->SetObjectField(TEXT("after"), After.Json); R.Data->SetObjectField(TEXT("graph_after"), After.Json);
    R.Data->SetObjectField(TEXT("user_parameters_after"), Users); R.Data->SetObjectField(TEXT("emitter_handles_after"), CurrentHandles);
    R.Data->SetObjectField(TEXT("graph_parameters_after"), Params);
    const bool UserPreserved = UserComplete && Text(C.UsersBefore) == Text(Users), HandlesPreserved = Text(C.HandlesBefore) == Text(CurrentHandles);
    const bool TargetSource = Loaded(C.Source) && C.Source->NodeGraph.Get() == C.Graph && C.Data->GraphSource == C.Source &&
        Loaded(C.Emitter.Emitter) && C.Emitter.Emitter->GetEmitterData(C.Emitter.Version) == C.Data;
    R.Data->SetBoolField(TEXT("user_parameters_preserved"), UserPreserved); R.Data->SetBoolField(TEXT("emitter_handles_preserved"), HandlesPreserved);
    R.Data->SetBoolField(TEXT("target_source_identity_preserved"), TargetSource);
    R.Data->SetBoolField(TEXT("graph_parameter_metadata_changed"), Text(C.ParametersBefore) != Text(Params));
    R.Data->SetNumberField(TEXT("graph_node_count_before"), C.Before.Nodes.Num()); R.Data->SetNumberField(TEXT("graph_node_count_after"), After.Nodes.Num());
    R.Data->SetNumberField(TEXT("new_node_count"), After.Nodes.Num() - C.Before.Nodes.Num());
    R.Data->SetBoolField(TEXT("package_dirty_after"), C.System->GetOutermost()->IsDirty());
    R.Data->SetObjectField(TEXT("compile_state"), CompileState(C.System));
    if (!After.bComplete || !UserComplete || !ParamsComplete)
    { R.bChangeStateKnown = false; Fail(R, TEXT("niagara_input_after_unobservable"), TEXT("Graph/User/parameter state cannot be completely observed; mutation is retained and no retry is safe.")); }
    else
    {
        R.bChanged |= !SameGraph(C.Before, After) || Text(C.ParametersBefore) != Text(Params) || C.Dirty != C.System->GetOutermost()->IsDirty();
        if (!UserPreserved || !HandlesPreserved || !TargetSource) { R.bChanged = true; Fail(R, TEXT("niagara_input_unrelated_state_changed"), TEXT("User declarations/values, selected source or emitter handle identities changed across a module input operation.")); }
    }
    return ObserveProtected(R, C) && After.bComplete && UserComplete && ParamsComplete && UserPreserved && HandlesPreserved && TargetSource;
}
bool GetCurrentOverride(FResult& R, FContext& C, UEdGraphPin*& Pin, UEdGraphNode*& Map)
{
    UEdGraphPin* ModuleIn = nullptr;
    if (!MapPin(C.Module, EGPD_Input, ModuleIn) || ModuleIn != C.ModuleMapIn || ModuleIn->LinkedTo.Num() != 1)
    { return Fail(R, TEXT("niagara_override_postcondition_failed"), TEXT("Actual selected module map input changed or has no unique override predecessor.")); }
    Map = ModuleIn->LinkedTo[0]->GetOwningNode(); Pin = nullptr;
    if (!Loaded(Map) || !ExactNodeClass(Map, TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapSet")) || Map->GetOuter() != C.Graph)
    { return Fail(R, TEXT("niagara_override_postcondition_failed"), TEXT("Exported helper did not create/retain the exact native owned MapSet.")); }
    UEdGraphPin* In = nullptr; UEdGraphPin* Out = nullptr;
    if (!MapPin(Map, EGPD_Input, In) || !In || In->LinkedTo.Num() != 1 || !MapPin(Map, EGPD_Output, Out) || Out != ModuleIn->LinkedTo[0])
    { return Fail(R, TEXT("niagara_override_postcondition_failed"), TEXT("Actual MapSet predecessor must remain unique and reciprocal before a value setter.")); }
    for (UEdGraphPin* P : Map->Pins) { if (P && P->Direction == EGPD_Input && P->PinName == C.Alias.GetParameterHandleString()) { if (Pin) { return Fail(R, TEXT("niagara_override_postcondition_failed"), TEXT("Actual MapSet contains multiple exact alias input pins.")); } Pin = P; } }
    if (!Pin || Pin->bOrphanedPin || Pin->DefaultObject || !Pin->DefaultTextValue.IsEmpty() || Pin->PinType.ContainerType != EPinContainerType::None ||
        UEdGraphSchema_Niagara::PinToTypeDefinition(Pin) != C.Input.GetType() || (C.Existing && Pin != C.Existing))
    { return Fail(R, TEXT("niagara_override_postcondition_failed"), TEXT("Actual exact alias override has unexpected object/text/type/identity.")); }
    return true;
}
bool VerifyGraph(FResult& R, FContext& C, const FSnapshot& After, UEdGraphPin* Override, UEdGraphNode* Map,
    UEdGraphNode* ValueNode, bool bLiteral, const FString& DefaultValue)
{
    if (!After.bComplete) { R.bChangeStateKnown = false; return false; }
    bool PriorNodes = true, PriorPins = true;
    for (const auto& P : C.Before.Nodes) { const auto* V = After.Nodes.Find(P.Key); PriorNodes &= V && *V == P.Value; }
    for (const auto& P : C.Before.Pins)
    {
        FString Expected = P.Value;
        if (bLiteral && C.Existing && P.Key == PinId(C.Existing))
        {
            TSharedPtr<FJsonObject> J;
            if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Expected), J) || !J.IsValid()) { PriorPins = false; break; }
            J->SetStringField(TEXT("raw_default_value"), DefaultValue); Expected = Text(J.ToSharedRef());
        }
        const auto* V = After.Pins.Find(P.Key); PriorPins &= V && *V == Expected;
    }
    const int32 ExpectedNodes = C.Before.Nodes.Num() + (C.OverrideNode ? 0 : 1) + (ValueNode ? 1 : 0);
    bool Nodes = Map && (C.OverrideNode ? Map == C.OverrideNode : !C.Before.Nodes.Contains(Path(Map))) && After.Nodes.Num() == ExpectedNodes &&
        (!ValueNode || (Loaded(ValueNode) && ValueNode->GetOuter() == C.Graph && !C.Before.Nodes.Contains(Path(ValueNode))));
    auto Links = C.Before.Links;
    if (!C.OverrideNode)
    {
        UEdGraphPin* In = nullptr; UEdGraphPin* Out = nullptr;
        if (!MapPin(Map, EGPD_Input, In) || !In || !MapPin(Map, EGPD_Output, Out) || !Out) { Nodes = false; }
        else
        {
            const FString PreviousId = PinId(C.PreviousMapOut);
            const TArray<FString>* PreviousTargets = C.Before.Links.Find(PreviousId);
            if (!PreviousTargets) { Nodes = false; }
            else { for (const auto& Next : *PreviousTargets) { RemoveEdge(Links, PreviousId, Next); AddEdge(Links, PinId(Out), Next); } AddEdge(Links, PreviousId, PinId(In)); }
        }
    }
    if (ValueNode)
    {
        if (!Override || Override->LinkedTo.Num() != 1 || Override->LinkedTo[0]->GetOwningNode() != ValueNode) { Nodes = false; }
        else { AddEdge(Links, PinId(Override), PinId(Override->LinkedTo[0])); }
        if (ExactNodeClass(ValueNode, TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet")))
        {
            UEdGraphPin* In = nullptr; UEdGraphPin* OverrideIn = nullptr;
            if (!MapPin(ValueNode, EGPD_Input, In) || !In || !MapPin(Map, EGPD_Input, OverrideIn) || !OverrideIn || OverrideIn->LinkedTo.Num() != 1 ||
                In->LinkedTo.Num() != 1 || In->LinkedTo[0] != OverrideIn->LinkedTo[0]) { Nodes = false; }
            else { AddEdge(Links, PinId(In), PinId(OverrideIn->LinkedTo[0])); }
        }
    }
    for (const auto& P : After.Pins) { Links.FindOrAdd(P.Key); }
    bool EdgeUnion = Links.Num() == After.Links.Num();
    for (auto& P : Links) { P.Value.Sort(); const auto* V = After.Links.Find(P.Key); EdgeUnion &= V && *V == P.Value; }
    R.Data->SetBoolField(TEXT("prior_node_metadata_preserved"), PriorNodes); R.Data->SetBoolField(TEXT("prior_pin_metadata_preserved"), PriorPins);
    R.Data->SetBoolField(TEXT("new_node_identity_verified"), Nodes); R.Data->SetBoolField(TEXT("exact_override_link_union_verified"), EdgeUnion);
    if (!PriorNodes || !PriorPins || !Nodes || !EdgeUnion)
    { return Fail(R, TEXT("niagara_input_graph_postcondition_failed"), TEXT("Actual graph differs from exact native override/value splice, allowed literal default and preserved prior node/pin metadata.")); }
    return true;
}
void CompleteResult(FResult& R)
{
    auto Wait = MakeShared<FJsonObject>(); Wait->SetBoolField(TEXT("wait_for_completion_requested"), false);
    Wait->SetBoolField(TEXT("wait_performed"), false); Wait->SetBoolField(TEXT("poll_or_flush_invoked"), false);
    Wait->SetBoolField(TEXT("compile_success_verified"), false); Wait->SetField(TEXT("completed"), MakeShared<FJsonValueNull>());
    Wait->SetStringField(TEXT("scope"), TEXT("nonblocking request only; separate readiness/exact save required"));
    R.Data->SetObjectField(TEXT("compile_wait"), Wait);
    R.Data->SetBoolField(TEXT("outcome_unknown"), !R.bChangeStateKnown);
    R.Data->SetBoolField(TEXT("actual_changes_retained"), R.bChanged || !R.bChangeStateKnown);
    R.bSuccess = R.bChangeStateKnown && R.ErrorCode.IsEmpty(); R.bPostconditionPassed = R.bSuccess;
    if (R.bSuccess) { R.Stage = TEXT("complete"); }
}
void Finish(FResult& R, FContext& C, const FSnapshot& AfterWrite, bool bSave, bool bCompile, UNiagaraDataInterfaceExport* ExpectedDI = nullptr)
{
    bool DiskUnknown = false;
    if(!InheritedCurrent(R,C,TEXT("before_compile_save"))){CompleteResult(R);return;}
    const TSharedPtr<FJsonObject> ParametersAfterWrite = R.Data->HasField(TEXT("graph_parameters_after")) ? R.Data->GetObjectField(TEXT("graph_parameters_after")) : nullptr;
    const TSharedPtr<FJsonObject> DIFieldsAfterWrite = ExpectedDI && R.Data->HasField(TEXT("export_fields_after")) ? R.Data->GetObjectField(TEXT("export_fields_after")) : nullptr;
    if (R.ErrorCode.IsEmpty() && R.bChangeStateKnown)
    {
        if (bCompile)
        {
            R.Stage = TEXT("request_compile"); R.Data->SetBoolField(TEXT("compile_requested"), true); R.Data->SetBoolField(TEXT("compile_attempted"), true);
            R.Data->SetNumberField(TEXT("explicit_compile_requests"), 1); R.Data->SetBoolField(TEXT("compile_request_returned"), C.System->RequestCompile(false));
            if(!InheritedCurrent(R,C,TEXT("after_request_compile"))){CompleteResult(R);return;}
        }
        if (bSave)
        {
            auto State = CompileState(C.System); R.Data->SetObjectField(TEXT("save_compile_state"), State);
            if(!InheritedCurrent(R,C,TEXT("before_save"))){CompleteResult(R);return;}
            if (!State->GetBoolField(TEXT("ready_to_save")))
            {
                R.Stage = TEXT("save_deferred"); R.Data->SetBoolField(TEXT("save_deferred"), true);
                Fail(R, TEXT("niagara_save_deferred_compile_pending"), TEXT("Input write remains; asynchronous or required compilation prevents a nonblocking save. Inspect readiness then explicitly save the exact system, never replay this input write."));
            }
            else
            {
                R.Stage = TEXT("save"); R.Data->SetBoolField(TEXT("save_attempted"), true); R.Data->SetNumberField(TEXT("exact_save_requests"), 1);
                const bool Saved = UEditorAssetLibrary::SaveLoadedAsset(C.System, false); R.Data->SetBoolField(TEXT("saved"), Saved);
                if(!InheritedCurrent(R,C,TEXT("after_save"))){CompleteResult(R);return;}
                if (!Saved) { DiskUnknown = true; Fail(R, TEXT("niagara_save_failed"), TEXT("Save returned false; in-memory graph is retained but actual disk outcome is uncertain. No rollback/retry.")); }
                else if (C.System->GetOutermost()->IsDirty()) { Fail(R, TEXT("niagara_save_postcondition_failed"), TEXT("Save returned true but the exact system remains dirty.")); }
            }
        }
    }
    FSnapshot After; ObserveCommon(R, C, After);if(C.InheritedLost){CompleteResult(R);return;}
    const bool Preserved = SameGraph(AfterWrite, After);
    R.Data->SetBoolField(TEXT("graph_preserved_across_compile_save"), Preserved);
    if (!Preserved && R.ErrorCode.IsEmpty()) { R.Stage = TEXT("observe_after"); Fail(R, TEXT("niagara_compile_save_graph_changed"), TEXT("Graph node/pin/link state changed across compilation/save; actual state retained.")); }
    const bool ParametersPreserved = ParametersAfterWrite.IsValid() && Text(ParametersAfterWrite.ToSharedRef()) == Text(R.Data->GetObjectField(TEXT("graph_parameters_after")).ToSharedRef());
    R.Data->SetBoolField(TEXT("graph_parameters_preserved_across_compile_save"), ParametersPreserved);
    if (!ParametersPreserved) { Fail(R, TEXT("niagara_compile_save_parameters_changed"), TEXT("Target graph parameter metadata changed across compile/save; actual state retained.")); }
    if (ExpectedDI)
    {
        if (!Loaded(ExpectedDI) || ExpectedDI->GetClass() != UNiagaraDataInterfaceExport::StaticClass() ||
            ExpectedDI->GetOutermost() != C.System->GetOutermost() || !DIFieldsAfterWrite.IsValid())
        { R.bChangeStateKnown = false; Fail(R, TEXT("niagara_export_final_unobservable"), TEXT("Previously returned exact Export DI is not observable after compile/save.")); }
        else
        {
            auto Fields = ExportFields(ExpectedDI); R.Data->SetObjectField(TEXT("export_fields_final"), Fields);
            const bool FieldsPreserved = Text(DIFieldsAfterWrite.ToSharedRef()) == Text(Fields);
            R.Data->SetBoolField(TEXT("export_fields_preserved_across_compile_save"), FieldsPreserved);
            if (!FieldsPreserved) { R.bChanged = true; Fail(R, TEXT("niagara_export_compile_save_fields_changed"), TEXT("Public Export DI values changed across compile/save; actual saved/dirty state retained.")); }
        }
    }
    if (DiskUnknown) { R.bChangeStateKnown = false; } R.Data->SetBoolField(TEXT("disk_outcome_unknown"), DiskUnknown);
    CompleteResult(R);
}
bool EmptyValueGuard(FResult& R, const FContext& C, bool bLiteral, bool bOverwrite)
{
    if (!C.Existing) { return true; }
    const int32 Links = C.Existing->LinkedTo.Num();
    if (Links)
    { return Fail(R, bLiteral ? TEXT("niagara_literal_linked_override_forbidden") : TEXT("niagara_occupied_override_unsupported"),
        bLiteral ? TEXT("Literal default overrides never remove linked value graphs, including overwrite_existing=true.") :
        TEXT("Occupied DI/linked override replacement is explicitly incomplete before mutation; public cleanup/cold private reference ownership is not sufficient for faithful legacy overwrite.")); }
    if (bLiteral && !C.Existing->DefaultValue.IsEmpty() && !bOverwrite)
    { return Fail(R, TEXT("niagara_literal_override_exists"), TEXT("Existing nonempty literal default requires overwrite_existing=true.")); }
    return true;
}
UEdGraphPin* CreateOverride(FResult& R, FContext& C, UEdGraphNode*& Map)
{
    if(!InheritedCurrent(R,C,TEXT("before_override_writer")))return nullptr;
    R.Stage = TEXT("create_override_pin"); R.bMutationAttempted = true; R.bChangeStateKnown = false;
    R.Data->SetBoolField(TEXT("outcome_unknown"), true); R.Data->SetNumberField(TEXT("override_pin_requests"), 1);
    C.System->Modify();if(!InheritedCurrent(R,C,TEXT("after_system_modify")))return nullptr;
    C.Graph->Modify();if(!InheritedCurrent(R,C,TEXT("after_graph_modify")))return nullptr;
    C.Module->Modify();if(!InheritedCurrent(R,C,TEXT("after_module_modify")))return nullptr;
    UEdGraphPin& Returned = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(*C.Module, C.Alias, C.Input.GetType(), FGuid(), FGuid());
    if(!InheritedCurrent(R,C,TEXT("after_override_sdk")))return nullptr;
    UEdGraphPin* Actual = nullptr;
    if (!GetCurrentOverride(R, C, Actual, Map) || Actual != &Returned || !Actual->LinkedTo.IsEmpty()) { return nullptr; }
    R.Data->SetStringField(TEXT("override_pin_id"), PinId(Actual)); R.Data->SetStringField(TEXT("override_node_path"), Path(Map));
    R.Data->SetBoolField(TEXT("override_pin_created"), C.Existing == nullptr); return Actual;
}
void ObserveFailedMutation(FResult& R, FContext& C)
{
    if(C.InheritedLost){R.Data->SetBoolField(TEXT("compile_and_save_suppressed_after_failure"),true);CompleteResult(R);return;}
    FSnapshot After; R.bChangeStateKnown = true; ObserveCommon(R, C, After);
    R.Data->SetBoolField(TEXT("compile_and_save_suppressed_after_failure"), true); CompleteResult(R);
}
bool ParameterMutationAllowed(FResult& R, const FContext& C, const FNiagaraVariableBase* Linked)
{
    bool Complete = false; const auto Now = GraphParameters(C.Graph, Complete);
    bool Allowed = Complete;
    for (const auto& P : C.ParametersBefore->Values)
    {
        const FString Key(P.Key); const TSharedPtr<FJsonValue>* Found = Now->Values.Find(P.Key);
        if (!Found) { Allowed = false; break; }
        auto Before = P.Value->AsObject(); auto After = (*Found)->AsObject();
        if (Linked && Key == Linked->GetName().ToString())
        {
            auto Expected = MakeShared<FJsonObject>(); Expected->Values = Before->Values;
            Expected->SetNumberField(TEXT("default_mode"), static_cast<int32>(ENiagaraDefaultMode::FailIfPreviouslyNotSet));
            if (Text(Expected) != Text(After.ToSharedRef())) { Allowed = false; }
        }
        else if (Text(Before.ToSharedRef()) != Text(After.ToSharedRef())) { Allowed = false; }
    }
    const bool Existed = Linked && C.ParametersBefore->HasField(Linked->GetName().ToString());
    const int32 ExpectedCount = C.ParametersBefore->Values.Num() + (Linked && !Existed ? 1 : 0);
    Allowed &= Now->Values.Num() == ExpectedCount;
    if (Linked)
    {
        const auto V = Now->TryGetField(Linked->GetName().ToString());
        Allowed &= V.IsValid() && V->Type == EJson::Object &&
            V->AsObject()->GetNumberField(TEXT("default_mode")) == static_cast<int32>(ENiagaraDefaultMode::FailIfPreviouslyNotSet) &&
            Text(V->AsObject()->GetObjectField(TEXT("type")).ToSharedRef()) == Text(TypeRow(Linked->GetType()));
    }
    R.Data->SetBoolField(TEXT("graph_parameter_changes_bounded"), Allowed);
    if (!Complete) { R.bChangeStateKnown = false; }
    if (!Allowed) { return Fail(R, TEXT("niagara_input_parameter_metadata_changed"), TEXT("Graph parameter metadata differs beyond the exact linked parameter's public helper default mode/declaration.")); }
    return true;
}
// Resolve only an actual preceding target-local writer. A private declaration
// elsewhere in the system is not evidence that this selected stack supplies it.
bool CollectPrivateUpstreamWrite(FResult& R, FContext& C, const FString& Name, TSet<FNiagaraVariableBase>& Known)
{
    UEdGraphPin* NextInput = C.ModuleMapIn;
    TSet<UEdGraphNode*> Seen;
    while (NextInput && NextInput->LinkedTo.Num() == 1)
    {
        UEdGraphPin* Out = NextInput->LinkedTo[0]; UEdGraphNode* Node = Out ? Out->GetOwningNode() : nullptr;
        UEdGraphPin* ActualOut = nullptr;
        if (!Node || Node->GetGraph() != C.Graph || Seen.Contains(Node) || Seen.Num() >= MaxNodes ||
            !MapPin(Node, EGPD_Output, ActualOut) || ActualOut != Out || Out->LinkedTo.Num() != 1 || Out->LinkedTo[0] != NextInput)
        { return Fail(R, TEXT("niagara_upstream_map_chain_invalid"), TEXT("Existing producer admission requires one bounded reciprocal upstream parameter-map chain; no load or fabricated parameter.")); }
        Seen.Add(Node);
        if (auto* Call = Cast<UNiagaraNodeFunctionCall>(Node))
        {
            UNiagaraScript* Script = Call->FunctionScript;
            // Initial producer cohort is system-owned. Emitter containers and
            // external assets retain their separate admission/parity boundary.
            if (Loaded(Script) && Script->GetClass() == UNiagaraScript::StaticClass() &&
                Script->GetOuter() == C.System && C.System->ScratchPadScripts.Contains(Script) &&
                Script->GetOutermost() == C.System->GetOutermost() && Script->GetScriptData(Call->SelectedScriptVersion))
            {
                auto* Source = Cast<UNiagaraScriptSource>(Script->GetSource(Call->SelectedScriptVersion));
                UNiagaraGraph* Graph = nullptr;
                if (!OrdinaryGraph(Source, Graph) || Source->GetOuter() != Script || !Protect(R, C, Source))
                { if (R.ErrorCode.IsEmpty()) { Fail(R, TEXT("niagara_upstream_source_invalid"), TEXT("The actual private producer version needs a complete already-loaded native source graph.")); } return false; }
                UEdGraphPin* Writer = nullptr; UEdGraphNode* WriterNode = nullptr;
                for (UEdGraphNode* Candidate : Graph->Nodes)
                {
                    if (!ExactNodeClass(Candidate, TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapSet"))) { continue; }
                    for (UEdGraphPin* Pin : Candidate->Pins)
                    {
                        if (!Pin || Pin->Direction != EGPD_Input || Pin->PinName != FName(*Name) ||
                            UEdGraphSchema_Niagara::PinToTypeDefinition(Pin) == FNiagaraTypeDefinition::GetParameterMapDef()) { continue; }
                        if (Writer) { return Fail(R, TEXT("niagara_upstream_write_ambiguous"), TEXT("Multiple private write pins for the same name are outside this finite producer admission.")); }
                        Writer = Pin; WriterNode = Candidate;
                    }
                }
                if (Writer)
                {
                    const auto Type = UEdGraphSchema_Niagara::PinToTypeDefinition(Writer);
                    const FNiagaraVariable Variable(Type, FName(*Name));
                    const auto* Metadata = Graph->GetAllMetaData().Find(Variable);
                    UEdGraphPin* MapOut = nullptr;
                    auto* Output = MapPin(WriterNode, EGPD_Output, MapOut) && MapOut && MapOut->LinkedTo.Num() == 1
                        ? Cast<UNiagaraNodeOutput>(MapOut->LinkedTo[0]->GetOwningNode()) : nullptr;
                    UEdGraphPin* OutputIn = nullptr;
                    if (!Type.IsValid() || !Metadata || !Loaded(Metadata->Get()) || !(*Metadata)->IsIn(Graph) ||
                        (*Metadata)->Variable.GetName() != Variable.GetName() || (*Metadata)->Variable.GetType() != Type ||
                        !Output || Output->GetGraph() != Graph || Output->GetUsage() != ENiagaraScriptUsage::Module ||
                        !MapPin(Output, EGPD_Input, OutputIn) || OutputIn != MapOut->LinkedTo[0] ||
                        OutputIn->LinkedTo.Num() != 1 || OutputIn->LinkedTo[0] != MapOut)
                    { return Fail(R, TEXT("niagara_upstream_write_unverified"), TEXT("The actual typed private write needs matching native metadata and a direct reciprocal map path to its Module output.")); }
                    Known.Add(FNiagaraVariableBase(Variable));
                    auto Row = MakeShared<FJsonObject>();
                    Row->SetStringField(TEXT("module_node_guid"), Guid(Call->NodeGuid)); Row->SetStringField(TEXT("module_node_path"), Path(Call));
                    Row->SetStringField(TEXT("script_path"), Path(Script)); Row->SetStringField(TEXT("script_version"), Guid(Call->SelectedScriptVersion));
                    Row->SetStringField(TEXT("graph_path"), Path(Graph)); Row->SetStringField(TEXT("write_node_guid"), Guid(WriterNode->NodeGuid));
                    Row->SetStringField(TEXT("write_pin_id"), PinId(Writer)); Row->SetStringField(TEXT("parameter_name"), Name);
                    Row->SetObjectField(TEXT("parameter_type"), TypeRow(Type)); Row->SetBoolField(TEXT("runtime_value_or_execution_verified"), false);
                    R.Data->SetArrayField(TEXT("upstream_private_parameter_sources"), {MakeShared<FJsonValueObject>(Row)});
                    R.Data->SetStringField(TEXT("linked_parameter_source_scope"), TEXT("nearest actual preceding system-owned private module write; typed definition only, not simulation execution"));
                    return true;
                }
            }
        }
        if (ExactNodeClass(Node, TEXT("/Script/NiagaraEditor.NiagaraNodeInput"))) { return true; }
        if (!Cast<UNiagaraNodeFunctionCall>(Node) && !ExactNodeClass(Node, TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapSet")))
        { return Fail(R, TEXT("niagara_upstream_node_unsupported"), TEXT("Finite producer lookup follows existing native FunctionCall/MapSet nodes only.")); }
        UEdGraphPin* In = nullptr;
        if (!MapPin(Node, EGPD_Input, In) || !In || In->LinkedTo.Num() != 1)
        { return Fail(R, TEXT("niagara_upstream_map_chain_invalid"), TEXT("Upstream native node requires one actual parameter-map predecessor.")); }
        NextInput = In;
    }
    return Fail(R, TEXT("niagara_upstream_map_chain_invalid"), TEXT("Producer map path is incomplete; no parameter or graph mutation was dispatched."));
}
bool PrepareLink(FResult& R, FContext& C, const FString& Name, FNiagaraVariableBase& Linked, TSet<FNiagaraVariableBase>& Known)
{
    if (!Bounded(Name, 256) || Name.IsEmpty() || !Name.Contains(TEXT(".")) || Name.Contains(TEXT(" ")) || Name.StartsWith(TEXT(".")) || Name.EndsWith(TEXT(".")))
    { return Fail(R, TEXT("niagara_linked_parameter_invalid"), TEXT("Linked parameter requires a bounded qualified name with no spaces/empty namespace.")); }
    const auto& InputType = C.Input.GetType();
    const bool Primitive = InputType == FNiagaraTypeDefinition::GetFloatDef() || InputType == FNiagaraTypeDefinition::GetIntDef() ||
        InputType == FNiagaraTypeDefinition::GetBoolDef() || InputType == FNiagaraTypeDefinition::GetColorDef() ||
        InputType == FNiagaraTypeDefinition::GetVec2Def() || InputType == FNiagaraTypeDefinition::GetVec3Def() ||
        InputType == FNiagaraTypeDefinition::GetVec4Def() || InputType == FNiagaraTypeDefinition::GetPositionDef();
    const FString EnumPath = InputType.IsEnum() ? Path(InputType.GetEnum()) : FString();
    const bool Enum = InputType.IsEnum() && Loaded(InputType.GetEnum()) &&
        (EnumPath.StartsWith(TEXT("/Game/")) || EnumPath.StartsWith(TEXT("/Script/"))) &&
        !EnumPath.Contains(TEXT("Cubeless"), ESearchCase::IgnoreCase) && !EnumPath.Contains(TEXT("ModelContextProtocol"), ESearchCase::IgnoreCase);
    if (!Primitive && !Enum)
    { return Fail(R, TEXT("niagara_linked_input_type_unsupported"), TEXT("This finite linked route supports native scalar/vector/color or existing non-Cubeless enum values; DI/UObject/parameter-map/custom struct bindings are explicitly incomplete.")); }
    TArray<FNiagaraVariable> Users; C.System->GetExposedParameters().GetParameters(Users);
    for (const auto& V : Users) { Known.Add(FNiagaraVariableBase(V)); }
    for (const auto& P : C.Graph->GetAllMetaData()) { Known.Add(FNiagaraVariableBase(P.Key)); }
    for (const auto& P : C.ModuleGraph->GetAllMetaData()) { Known.Add(FNiagaraVariableBase(P.Key)); }
    // Existing User/graph admission is unchanged. Only a previously absent name
    // needs the finite actual upstream-source lookup used by the new producer.
    bool NameObserved = false;
    for (const auto& V : Known) { NameObserved |= V.GetName() == FName(*Name); }
    if (!NameObserved && !CollectPrivateUpstreamWrite(R, C, Name, Known)) { return false; }
    if (Known.Num() > MaxParameters * 3) { return Fail(R, TEXT("niagara_linked_parameter_bound"), TEXT("Known typed parameter inventory exceeds the finite bound.")); }
    Linked = FNiagaraVariableBase(C.Input.GetType(), FName(*Name));
    if (!Known.Contains(Linked))
    {
        // Preserve the exported utility's Vec3/Position loose-type choice only
        // when its exact already-loaded public settings default enables it.
        UClass* Class = FindObject<UClass>(nullptr, TEXT("/Script/Niagara.NiagaraSettings"));
        const auto* Settings = Class && Class == UNiagaraSettings::StaticClass() ? Cast<UNiagaraSettings>(Class->GetDefaultObject(false)) : nullptr;
        const auto& T = C.Input.GetType(); const bool Vector = T == FNiagaraTypeDefinition::GetVec3Def() || T == FNiagaraTypeDefinition::GetPositionDef();
        const FNiagaraTypeDefinition Alt = T == FNiagaraTypeDefinition::GetVec3Def() ? FNiagaraTypeDefinition::GetPositionDef() : FNiagaraTypeDefinition::GetVec3Def();
        FNiagaraVariableBase Alternate(Alt, FName(*Name));
        FNiagaraVariableBase Static(T.ToStaticDef(), FName(*Name));
        if (Vector && Settings && !Settings->bEnforceStrictStackTypes && Known.Contains(Alternate)) { Linked = Alternate; }
        else if (!Vector && Known.Contains(Static)) { Linked = Static; }
        else { return Fail(R, TEXT("niagara_linked_parameter_not_found_or_type_mismatch"), TEXT("A matching typed User/target/source graph parameter must already exist. Legacy fabricated KnownParameters does not establish an existing value source.")); }
    }
    R.Data->SetStringField(TEXT("linked_parameter_name"), Name); R.Data->SetObjectField(TEXT("linked_parameter_actual_type"), TypeRow(Linked.GetType()));
    R.Data->SetBoolField(TEXT("linked_parameter_existing_type_verified"), true); return true;
}
void PreflightClosure(FResult& R, FContext& C)
{
    // Once complete target/source baselines exist, even a known input/value
    // rejection receives an actual preservation observation. No mutation retry.
    if (C.Before.bComplete && !C.Protected.IsEmpty() && C.UsersBefore->HasField(TEXT("observation_complete")))
    {
        FSnapshot After; ObserveCommon(R, C, After);if(C.InheritedLost){CompleteResult(R);return;}
        const bool Same = SameGraph(C.Before, After) && C.Dirty == C.System->GetOutermost()->IsDirty();
        R.Data->SetBoolField(TEXT("preflight_state_preserved"), Same);
        if (!Same) { R.bChanged = true; Fail(R, TEXT("niagara_preflight_state_changed"), TEXT("Input enumeration/rejection changed observed graph state; no value mutation was dispatched.")); }
    }
    CompleteResult(R);
}
} // anonymous namespace

Function::FResult Inspect(const FString& SystemPath, const FString& EmitterIndexJson, const FString& EmitterName,
    const FString& ModuleIndexJson, const FString& ModuleName, const FString& ModuleNodeGuid, const FString& InputName)
{
    FResult R; Initialize(R, SystemPath, InputName, false); R.Data->SetBoolField(TEXT("read_only"), true); FContext C;
    if (!Prepare(R, C, SystemPath, EmitterIndexJson, EmitterName, ModuleIndexJson, ModuleName, ModuleNodeGuid, InputName, false, false, true))
    { PreflightClosure(R, C); return R; }
    if (C.Existing) { auto J = PinMetadata(C.Existing); TArray<TSharedPtr<FJsonValue>> Links; for (const auto* P : C.Existing->LinkedTo) { Links.Add(MakeShared<FJsonValueString>(PinId(P))); } J->SetArrayField(TEXT("links"), Links); R.Data->SetObjectField(TEXT("override_pin"), J); }
    else { R.Data->SetField(TEXT("override_pin"), MakeShared<FJsonValueNull>()); }
    auto Inventory = DIInventory(C, C.Existing); const bool InventoryComplete = Inventory.Num() <= 32;
    if (!InventoryComplete) { Inventory.SetNum(32); }
    R.Data->SetArrayField(TEXT("owned_export_di_inventory"), Inventory);
    R.Data->SetBoolField(TEXT("owned_export_di_inventory_complete"), InventoryComplete);
    R.Data->SetBoolField(TEXT("owned_export_di_inventory_truncated"), !InventoryComplete);
    R.Data->SetNumberField(TEXT("owned_export_di_inventory_bound"), 32);
    if (!InventoryComplete) { Fail(R, TEXT("niagara_export_di_inventory_bound_exceeded"), TEXT("Owned-node Export DI inventory was truncated at32; graph readback remains separate, no complete DI inventory is claimed.")); }
    FSnapshot After; ObserveCommon(R, C, After);
    const bool Preserved = SameGraph(C.Before, After) && Text(C.Before.Json) == Text(After.Json) && C.Dirty == C.System->GetOutermost()->IsDirty();
    R.Data->SetBoolField(TEXT("state_preserved"), Preserved);
    if (!Preserved) { R.bChanged = true; Fail(R, TEXT("niagara_inspect_state_changed"), TEXT("Native module-input enumeration/inspection changed observed state.")); }
    CompleteResult(R); return R;
}
Function::FResult SetLiteral(const FString& SystemPath, const FString& InputName, const FString& ValueJson,
    const FString& EmitterIndexJson, const FString& EmitterName, const FString& ModuleIndexJson, const FString& ModuleName,
    const FString& ModuleNodeGuid, bool bOverwriteExisting, bool bAllowSourceEdit, bool bSave, bool bRequestCompile)
{
    FResult R; Initialize(R, SystemPath, InputName, bSave); FContext C;
    R.Data->SetStringField(TEXT("write_scope"), TEXT("module_input_literal_override_pin_default"));
    if (!Prepare(R, C, SystemPath, EmitterIndexJson, EmitterName, ModuleIndexJson, ModuleName, ModuleNodeGuid, InputName, true, bAllowSourceEdit, false))
    { PreflightClosure(R, C); return R; }
    TSharedPtr<FJsonValue> Value, Actual; FNiagaraVariable LiteralValue; FString Default;
    if (!JsonValue(ValueJson, Value) || !Literal(R, C.Input.GetType(), Value, LiteralValue, Actual) ||
        !C.Schema->TryGetPinDefaultValueFromNiagaraVariable(LiteralValue, Default) || !EmptyValueGuard(R, C, true, bOverwriteExisting))
    { if (R.ErrorCode.IsEmpty()) { Fail(R, TEXT("niagara_literal_value_invalid"), TEXT("Required literal JSON cannot be converted to an exact typed native pin default.")); } PreflightClosure(R, C); return R; }
    R.Data->SetField(TEXT("new_value"), Actual); R.Data->SetStringField(TEXT("new_default_value"), Default);
    bool RapidExists = false; TArray<UNiagaraScript*> Scripts; C.Data->GetScripts(Scripts, false, false);
    for (UNiagaraScript* Script : Scripts)
    {
        if (Loaded(Script) && Script->IsEquivalentUsage(C.Usage))
        {
            const FNiagaraVariable Variable(C.Input.GetType(), C.Alias.GetParameterHandleString());
            const FNiagaraVariable Rapid = FNiagaraUtilities::ConvertVariableToRapidIterationConstantName(Variable, *C.EmitterName, Script->GetUsage());
            RapidExists = Rapid.IsValid() && Script->RapidIterationParameters.GetParameterData(Rapid) != nullptr; break;
        }
    }
    R.Data->SetBoolField(TEXT("rapid_iteration_parameter_exists"), RapidExists);
    UEdGraphNode* Map = nullptr; UEdGraphPin* Override = CreateOverride(R, C, Map);
    if (!Override) { ObserveFailedMutation(R, C); return R; }
    R.Stage = TEXT("set_literal_default"); R.Data->SetNumberField(TEXT("value_write_requests"), 1);
    Override->Modify();if(!InheritedCurrent(R,C,TEXT("after_literal_pin_modify"))){CompleteResult(R);return R;}
    Map->Modify();if(!InheritedCurrent(R,C,TEXT("after_literal_node_modify"))){CompleteResult(R);return R;}
    Override->DefaultValue = Default;
    static_cast<UNiagaraNode*>(Map)->MarkNodeRequiresSynchronization(TEXT("Cubeless: literal input default changed"), true);
    if(!InheritedCurrent(R,C,TEXT("after_literal_notification"))){CompleteResult(R);return R;}
    C.System->MarkPackageDirty();if(!InheritedCurrent(R,C,TEXT("after_literal_dirty"))){CompleteResult(R);return R;}
    R.Data->SetBoolField(TEXT("overwrote_existing"), C.Existing && !R.Data->GetStringField(TEXT("previous_default_value")).IsEmpty());
    R.bChangeStateKnown = true; FSnapshot After; ObserveCommon(R, C, After);if(C.InheritedLost){CompleteResult(R);return R;}
    const bool ValueOk = Override->DefaultValue == Default && Override->LinkedTo.IsEmpty();
    R.Data->SetBoolField(TEXT("literal_default_verified"), ValueOk); R.Data->SetNumberField(TEXT("values_written_count"), ValueOk ? 1 : 0);
    if (!ValueOk) { Fail(R, TEXT("niagara_literal_postcondition_failed"), TEXT("Typed native pin default readback differs; observed graph retained.")); }
    VerifyGraph(R, C, After, Override, Map, nullptr, true, Default); ParameterMutationAllowed(R, C, nullptr);
    Finish(R, C, After, bSave, bRequestCompile); return R;
}
Function::FResult SetDataInterface(const FString& SystemPath, const FString& InputName, const FString& PropertiesJson,
    const FString& EmitterIndexJson, const FString& EmitterName, const FString& ModuleIndexJson, const FString& ModuleName,
    const FString& ModuleNodeGuid, bool bOverwriteExisting, bool bAllowSourceEdit, bool bSave, bool bRequestCompile)
{
    FResult R; Initialize(R, SystemPath, InputName, bSave); FContext C;
    R.Data->SetStringField(TEXT("write_scope"), TEXT("data_interface_module_input"));
    if (!Prepare(R, C, SystemPath, EmitterIndexJson, EmitterName, ModuleIndexJson, ModuleName, ModuleNodeGuid, InputName, true, bAllowSourceEdit, false))
    { PreflightClosure(R, C); return R; }
    FExportFields Fields;
    if (!PrepareExport(R, C.Input.GetType(), PropertiesJson, Fields) || !EmptyValueGuard(R, C, false, bOverwriteExisting))
    { PreflightClosure(R, C); return R; }
    bool CallbackExists = false, CallbackTypeMatches = false; TArray<FNiagaraVariable> Users;
    C.System->GetExposedParameters().GetParameters(Users);
    if (Fields.CallbackSet)
    {
        for (const auto& V : Users)
        { if (V.GetName() == Fields.Callback.Parameter.GetName()) { CallbackExists = true; CallbackTypeMatches = V.GetType() == FNiagaraTypeDefinition::GetUObjectDef(); } }
        if (CallbackExists && !CallbackTypeMatches)
        { Fail(R, TEXT("niagara_export_callback_user_type_mismatch"), TEXT("An existing callback User declaration has a different type; no DI allocated.")); PreflightClosure(R, C); return R; }
    }
    R.Data->SetBoolField(TEXT("callback_user_parameter_exists"), CallbackExists); R.Data->SetBoolField(TEXT("callback_user_parameter_type_matches"), CallbackTypeMatches);
    R.Data->SetBoolField(TEXT("callback_declaration_created"), false);
    UEdGraphNode* Map = nullptr; UEdGraphPin* Override = CreateOverride(R, C, Map);
    if (!Override) { ObserveFailedMutation(R, C); return R; }
    R.Stage = TEXT("set_export_data_interface"); R.Data->SetNumberField(TEXT("value_write_requests"), 1);
    UNiagaraDataInterface* Returned = nullptr;
    FNiagaraStackGraphUtilities::SetDataInterfaceValueForFunctionInput(*Override, C.Input.GetType().GetClass(),
        C.Input.GetName().ToString(), Returned);
    if(!InheritedCurrent(R,C,TEXT("after_di_input_sdk"))){CompleteResult(R);return R;}
    auto* DI = Cast<UNiagaraDataInterfaceExport>(Returned);
    UEdGraphNode* ValueNode = Override->LinkedTo.Num() == 1 ? Override->LinkedTo[0]->GetOwningNode() : nullptr;
    const bool Identity = Loaded(DI) && DI->GetClass() == UNiagaraDataInterfaceExport::StaticClass() && Loaded(ValueNode) &&
        ValueNode->GetClass() == UNiagaraNodeInput::StaticClass() && ValueNode->GetOuter() == C.Graph &&
        DI->GetOuter() == ValueNode && DI->GetOutermost() == C.System->GetOutermost() &&
        UEdGraphSchema_Niagara::PinToTypeDefinition(Override->LinkedTo[0]) == C.Input.GetType();
    R.Data->SetBoolField(TEXT("live_returned_di_identity_verified"), Identity);
    if (!Identity) { Fail(R, TEXT("niagara_di_returned_identity_invalid"), TEXT("Exported DI utility returned no exact owned native Export DI/input link; allocated state retained.")); ObserveFailedMutation(R, C); return R; }
    R.Data->SetStringField(TEXT("data_interface_class"), DI->GetClass()->GetName());
    R.Data->SetStringField(TEXT("data_interface_class_path"), Path(DI->GetClass())); R.Data->SetStringField(TEXT("data_interface_path"), Path(DI));
    R.Data->SetStringField(TEXT("data_interface_owner_node_path"), Path(ValueNode)); R.Data->SetObjectField(TEXT("export_fields_before"), ExportFields(DI));
    DI->Modify();if(!InheritedCurrent(R,C,TEXT("after_di_modify"))){CompleteResult(R);return R;} if (Fields.CallbackSet) { DI->CallbackHandlerParameter = Fields.Callback; }
    if (Fields.ModeSet) { DI->GPUAllocationMode = Fields.Mode; } if (Fields.FixedSet) { DI->GPUAllocationFixedSize = Fields.Fixed; }
    if (Fields.PerParticleSet) { DI->GPUAllocationPerParticleSize = Fields.PerParticle; }
    TArray<TSharedPtr<FJsonValue>> Applied; const auto ActualFields = ExportFields(DI);
    for (const auto& P : Fields.Requested->Values)
    {
        auto J = MakeShared<FJsonObject>(); const FString Name(P.Key); J->SetStringField(TEXT("name"), Name);
        FString OriginalText; if (P.Value->Type == EJson::String) { OriginalText = P.Value->AsString(); }
        else if (P.Value->Type == EJson::Number) { OriginalText = FString::Printf(TEXT("%.17g"), P.Value->AsNumber()); }
        J->SetStringField(TEXT("value"), OriginalText); J->SetField(TEXT("actual_public_value"), ActualFields->TryGetField(Name));
        Applied.Add(MakeShared<FJsonValueObject>(J));
    }
    const bool Values = (!Fields.CallbackSet || DI->CallbackHandlerParameter.Parameter.GetName() == Fields.Callback.Parameter.GetName()) &&
        DI->CallbackHandlerParameter.Parameter.GetType() == FNiagaraTypeDefinition::GetUObjectDef() &&
        (!Fields.ModeSet || DI->GPUAllocationMode == Fields.Mode) && (!Fields.FixedSet || DI->GPUAllocationFixedSize == Fields.Fixed) &&
        (!Fields.PerParticleSet || DI->GPUAllocationPerParticleSize == Fields.PerParticle);
    R.Data->SetArrayField(TEXT("applied_properties"), Applied); R.Data->SetObjectField(TEXT("export_fields_after"), ActualFields);
    R.Data->SetBoolField(TEXT("export_properties_verified"), Values); R.Data->SetNumberField(TEXT("values_written_count"), Values ? 1 : 0);
    R.Data->SetBoolField(TEXT("export_runtime_callback_or_gpu_verified"), false);
    static_cast<UNiagaraNode*>(Map)->MarkNodeRequiresSynchronization(TEXT("Cubeless: Export DI input changed"), true);if(!InheritedCurrent(R,C,TEXT("after_di_notification"))){CompleteResult(R);return R;}
    C.System->MarkPackageDirty();if(!InheritedCurrent(R,C,TEXT("after_di_dirty"))){CompleteResult(R);return R;}
    R.bChangeStateKnown = true; FSnapshot After; ObserveCommon(R, C, After);if(C.InheritedLost){CompleteResult(R);return R;}
    if (!Values) { Fail(R, TEXT("niagara_export_properties_postcondition_failed"), TEXT("Actual public Export fields differ after the retained native write.")); }
    VerifyGraph(R, C, After, Override, Map, ValueNode, false, FString()); ParameterMutationAllowed(R, C, nullptr);
    Finish(R, C, After, bSave, bRequestCompile, DI); return R;
}
Function::FResult SetLinked(const FString& SystemPath, const FString& InputName, const FString& LinkedParameterName,
    const FString& EmitterIndexJson, const FString& EmitterName, const FString& ModuleIndexJson, const FString& ModuleName,
    const FString& ModuleNodeGuid, bool bOverwriteExisting, bool bAllowSourceEdit, bool bSave, bool bRequestCompile)
{
    FResult R; Initialize(R, SystemPath, InputName, bSave); FContext C;
    R.Data->SetStringField(TEXT("write_scope"), TEXT("module_input_linked_parameter"));
    if (!Prepare(R, C, SystemPath, EmitterIndexJson, EmitterName, ModuleIndexJson, ModuleName, ModuleNodeGuid, InputName, true, bAllowSourceEdit, true))
    { PreflightClosure(R, C); return R; }
    FNiagaraVariableBase Linked; TSet<FNiagaraVariableBase> Known;
    if (!EmptyValueGuard(R, C, false, bOverwriteExisting) || !PrepareLink(R, C, LinkedParameterName, Linked, Known))
    { PreflightClosure(R, C); return R; }
    UEdGraphNode* Map = nullptr; UEdGraphPin* Override = CreateOverride(R, C, Map);
    if (!Override) { ObserveFailedMutation(R, C); return R; }
    R.Stage = TEXT("set_linked_parameter"); R.Data->SetNumberField(TEXT("value_write_requests"), 1);
    // Public utility allocates a MapGet and adds/updates this exact graph parameter
    // metadata. User store declarations are never fabricated by this operation.
    FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(*Override, Linked, Known);
    if(!InheritedCurrent(R,C,TEXT("after_linked_input_sdk"))){CompleteResult(R);return R;}
    UEdGraphPin* Source = Override->LinkedTo.Num() == 1 ? Override->LinkedTo[0] : nullptr;
    UEdGraphNode* ValueNode = Source ? Source->GetOwningNode() : nullptr;
    const bool Value = Source && ExactNodeClass(ValueNode, TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet")) &&
        Source->Direction == EGPD_Output && Source->PinName == Linked.GetName() && UEdGraphSchema_Niagara::PinToTypeDefinition(Source) == Linked.GetType();
    R.Data->SetStringField(TEXT("linked_value_node_path"), Path(ValueNode)); R.Data->SetBoolField(TEXT("linked_parameter_readback_verified"), Value);
    R.Data->SetNumberField(TEXT("values_written_count"), Value ? 1 : 0);
    static_cast<UNiagaraNode*>(Map)->MarkNodeRequiresSynchronization(TEXT("Cubeless: linked input changed"), true);if(!InheritedCurrent(R,C,TEXT("after_linked_notification"))){CompleteResult(R);return R;}
    C.System->MarkPackageDirty();if(!InheritedCurrent(R,C,TEXT("after_linked_dirty"))){CompleteResult(R);return R;}
    R.bChangeStateKnown = true; FSnapshot After; ObserveCommon(R, C, After);if(C.InheritedLost){CompleteResult(R);return R;}
    if (!Value) { Fail(R, TEXT("niagara_linked_parameter_postcondition_failed"), TEXT("Actual native MapGet source/name/type differs from the admitted existing linked parameter.")); }
    VerifyGraph(R, C, After, Override, Map, ValueNode, false, FString()); ParameterMutationAllowed(R, C, &Linked);
    Finish(R, C, After, bSave, bRequestCompile); return R;
}
}
