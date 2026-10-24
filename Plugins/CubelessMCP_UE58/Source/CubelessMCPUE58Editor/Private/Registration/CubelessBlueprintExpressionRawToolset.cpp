// DB: original Math7/Boolean7 on two fixed named official raw routes.
#include "Registration/CubelessBlueprintExpressionRawToolset.h"
#include "Registration/CubelessOwnedToolsetIdentity.h"
#include "Async/Async.h"
#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "IPythonScriptPlugin.h"
#include "Misc/Base64.h"
#include "Misc/Guid.h"
#include "Misc/CoreMisc.h"
#include "PythonScriptTypes.h"
#include "Serialization/JsonSerializer.h"
#include "Templates/Atomic.h"
#include "ToolsetRegistry/Toolset.h"
#include "ToolsetRegistry/ToolsetRegistrySubsystem.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogCubelessBlueprintExpressionRaw, Log, All);

namespace Cubeless::Registration
{
struct FBlueprintExpressionRawCallState
{
    TAtomic<bool> Active { false };
    TWeakObjectPtr<UToolsetRegistrySubsystem> Subsystem;
    const UE::ToolsetRegistry::FToolset* Identity = nullptr;
};
namespace
{
using FOutput = TValueOrError<FString, FString>;
constexpr const TCHAR* ToolsetName = TEXT("CubelessMCPUE58Editor.CubelessBlueprintExpressionTools");
constexpr const TCHAR* MathMethod = TEXT("add_blueprint_math_node");
constexpr const TCHAR* BooleanMethod = TEXT("add_blueprint_boolean_node");
constexpr const TCHAR* CompareMethod = TEXT("add_blueprint_compare_node");
constexpr const TCHAR* LiteralMethod = TEXT("add_blueprint_literal_node");
constexpr const TCHAR* SubsystemMethod = TEXT("add_blueprint_get_subsystem_from_pc_node");
constexpr const TCHAR* ConstructMethod = TEXT("add_blueprint_construct_object_node");
constexpr const TCHAR* InputActionMethod = TEXT("add_blueprint_input_action_node");
constexpr const TCHAR* SelfMethod = TEXT("add_blueprint_self_reference");
constexpr const TCHAR* EditorSubsystemMethod = TEXT("add_blueprint_get_editor_subsystem_node");
constexpr const TCHAR* InputAxisMethod = TEXT("add_blueprint_input_axis_event_node");
constexpr const TCHAR* EnhancedInputActionMethod = TEXT("add_blueprint_enhanced_input_action_node");
bool AllowedMethod(const FString& Name) { return Name == MathMethod || Name == BooleanMethod || Name == CompareMethod || Name == LiteralMethod || Name == SubsystemMethod || Name == ConstructMethod || Name == InputActionMethod || Name == SelfMethod || Name == EditorSubsystemMethod || Name == InputAxisMethod || Name == EnhancedInputActionMethod; }
constexpr const TCHAR* ResultPrefix = TEXT("__CUBELESS_BLUEPRINT_EXPRESSION_RESULT__");
constexpr int32 MaxInputBytes = 131072, MaxResultBytes = 16 * 1024 * 1024;

FString Schema()
{
    // Whole input/output schema: unrestricted original position with defaultNull.
    return FString(TEXT("{\"name\":\"CubelessMCPUE58Editor.CubelessBlueprintExpressionTools\",\"version\":\"1.0\",\"tools\":[{\"name\":\"CubelessMCPUE58Editor.CubelessBlueprintExpressionTools.add_blueprint_math_node\",\"description\":\"Original typed expression helper; finite public Engine node creation, no explicit compile/save or replay.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"blueprint_name\":{\"type\":\"string\"},\"operation\":{\"type\":\"string\"},\"node_position\":{\"default\":null},\"graph_name\":{\"type\":\"string\",\"default\":\"\"},\"graph_id\":{\"type\":\"string\",\"default\":\"\"},\"graph_type\":{\"type\":\"string\",\"default\":\"\"},\"create_graph_if_missing\":{\"type\":\"boolean\",\"default\":false}},\"required\":[\"blueprint_name\",\"operation\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}},{\"name\":\"CubelessMCPUE58Editor.CubelessBlueprintExpressionTools.add_blueprint_boolean_node\",\"description\":\"Original typed expression helper; finite public Engine node creation, no explicit compile/save or replay.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"blueprint_name\":{\"type\":\"string\"},\"operation\":{\"type\":\"string\"},\"node_position\":{\"default\":null},\"graph_name\":{\"type\":\"string\",\"default\":\"\"},\"graph_id\":{\"type\":\"")) + TEXT("string\",\"default\":\"\"},\"graph_type\":{\"type\":\"string\",\"default\":\"\"},\"create_graph_if_missing\":{\"type\":\"boolean\",\"default\":false}},\"required\":[\"blueprint_name\",\"operation\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}},{\"name\":\"CubelessMCPUE58Editor.CubelessBlueprintExpressionTools.add_blueprint_compare_node\",\"description\":\"Original Compare/Literal helper; actual typed Engine CallFunction/default/retirement prefix, no explicit compile/save or replay. Missing MakeLiteralFloat uses Engine MathLibrary WeightedMovingAverage_Float with identical CurrentSample/PreviousSample and Weight=1; actual float pins differ, no double substitution or full parity.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"blueprint_name\":{\"type\":\"string\"},\"operation\":{\"type\":\"string\"},\"value_type\":{\"type\":\"string\",\"default\":\"double\"},\"node_position\":{\"default\":null},\"graph_name\":{\"type\":\"string\",\"default\":\"\"},\"graph_id\":{\"type\":\"string\",\"default\":\"\"},\"graph_type\":{\"type\":\"string\",\"default\":\"\"},\"create_graph_if_missing\":{\"type\":\"boolean\",\"default\":false}},\"required\":[\"blueprint_name\",\"operation\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"pr") + TEXT("operties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}},{\"name\":\"CubelessMCPUE58Editor.CubelessBlueprintExpressionTools.add_blueprint_literal_node\",\"description\":\"Original Compare/Literal helper; actual typed Engine CallFunction/default/retirement prefix, no explicit compile/save or replay. Missing MakeLiteralFloat uses Engine MathLibrary WeightedMovingAverage_Float with identical CurrentSample/PreviousSample and Weight=1; actual float pins differ, no double substitution or full parity.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"blueprint_name\":{\"type\":\"string\"},\"literal_type\":{\"type\":\"string\"},\"value\":{\"default\":null},\"node_position\":{\"default\":null},\"graph_name\":{\"type\":\"string\",\"default\":\"\"},\"graph_id\":{\"type\":\"string\",\"default\":\"\"},\"graph_type\":{\"type\":\"string\",\"default\":\"\"},\"create_graph_if_missing\":{\"type\":\"boolean\",\"default\":false}},\"required\":[\"blueprint_name\",\"literal_type\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"typ") + TEXT("e\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}},{\"name\":\"CubelessMCPUE58Editor.CubelessBlueprintExpressionTools.add_blueprint_get_subsystem_from_pc_node\",\"description\":\"Original subsystem-from-controller/object-construction helper; specialized Engine nodes, typed class/default/restoration/retirement prefix, no runtime getter/spawn or explicit compile/save/replay. EditorSubsystem/EditorUtility remains a separate unfinished API.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"blueprint_name\":{\"type\":\"string\"},\"subsystem_class\":{\"type\":\"string\"},\"node_position\":{\"default\":null},\"graph_name\":{\"type\":\"string\",\"default\":\"\"},\"graph_id\":{\"type\":\"string\",\"default\":\"\"},\"graph_type\":{\"type\":\"string\",\"default\":\"\"},\"create_graph_if_missing\":{\"type\":\"boolean\",\"default\":false}},\"required\":[\"blueprint_name\",\"subsystem_class\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"r") + TEXT("equired\":[\"returnValue\"],\"additionalProperties\":false}},{\"name\":\"CubelessMCPUE58Editor.CubelessBlueprintExpressionTools.add_blueprint_construct_object_node\",\"description\":\"Bounded saved-clean direct UObject Blueprint sources additionally admit one-level primitive Array fields and exact native Vector/Rotator/Transform scalar fields. Existing generated/skeleton/CDO fields, borrowed Inner role/layout, held native value sources, authored pin types and SDK callback/retirement/partial checks remain. Exposed value/runtime initialization, inheritance and wider complex/object/struct arrays remain incomplete or usage pending. For the detailed already-loaded saved-clean generated UObject ConstructObject path, the target Blueprint additionally admits held loaded Engine-native Actor parent chains including Pawn/Character, with existing CDO and current class/super/package/graph roles. This does not extend native requested-class or SubsystemFromPC owner support. Actual new-node/pin reconstruction/retirement and partial receipts remain unchanged; source or parent loading, CDO creation, explicit compile/save, runtime construction, custom/generated owners and full original parity remain incomplete or usage pending. Original construct-object helper7 with whole JSON Any/defaultNull position and literal class path. Alongside existing native classes, Native admits the bounded already-loaded saved-clean generated Blueprint class directly derived from UObject and its supported primitive ExposeOnSpawn pins. Actual class/skeleton/CDO/source, typed Return, checked Class default/reconstruction and only-new-node retirement receipts remain authoritative. No runtime construction, explicit compile/save, source loading or automatic replay; broader generated inheritance, properties, owners and cold/runt") + TEXT("ime pipeline parity remain incomplete.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"blueprint_name\":{\"type\":\"string\"},\"object_class\":{\"type\":\"string\"},\"node_position\":{\"default\":null},\"graph_name\":{\"type\":\"string\",\"default\":\"\"},\"graph_id\":{\"type\":\"string\",\"default\":\"\"},\"graph_type\":{\"type\":\"string\",\"default\":\"\"},\"create_graph_if_missing\":{\"type\":\"boolean\",\"default\":false}},\"required\":[\"blueprint_name\",\"object_class\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}},{\"name\":\"CubelessMCPUE58Editor.CubelessBlueprintExpressionTools.add_blueprint_input_action_node\",\"description\":\"Original legacy mapped InputAction event constructor; literal empty or unknown action names remain admitted and may produce SDK compile warnings. Actual Engine event pins and partial authoring receipt only; no EnhancedInput replacement, input mapping mutation, explicit compile/save, runtime/hardware or full pipeline claim.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"blueprint_name\":{\"type\":\"string\"},\"action_name\":{\"type\":\"string\"},\"node_position\":{\"default\":null},\"graph_name\":{\"type\":\"string\",\"default\":\"\"},\"graph_id\":{\"type\":\"string\",\"default\":\"\"},\"graph_") + TEXT("type\":{\"type\":\"string\",\"default\":\"\"},\"create_graph_if_missing\":{\"type\":\"boolean\",\"default\":false}},\"required\":[\"blueprint_name\",\"action_name\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}},{\"name\":\"CubelessMCPUE58Editor.CubelessBlueprintExpressionTools.add_blueprint_self_reference\",\"description\":\"Original pure Engine Self node constructor; current-owner self pin and partial authoring receipt only. No runtime object evaluation, explicit compile/save, cleanup/replay or full owner/runtime/pipeline claim. Also admits the bounded already-loaded exact direct-UserWidget WidgetBlueprint profile with an empty Canvas root and existing EventGraph, including dirty hosts. The stock output remains PC_Object/PSC_Self with a null subtype object and actual pin name self; this is an authored self role, not a returned live Widget instance. Missing Widget graphs and broader Widget/runtime state remain incomplete.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"blueprint_name\":{\"type\":\"string\"},\"node_position\":{\"default\":null},\"graph_name\":{\"type\":\"string\",\"default\":\"\"},\"graph_id\":{\"type\":\"string\",\"default\":\"\"},\"graph_type\":{\"type\":\"string\",\"default") + TEXT("\":\"\"},\"create_graph_if_missing\":{\"type\":\"boolean\",\"default\":false}},\"required\":[\"blueprint_name\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}},{\"name\":\"CubelessMCPUE58Editor.CubelessBlueprintExpressionTools.add_blueprint_get_editor_subsystem_node\",\"description\":\"Original EditorSubsystem getter constructor for a legitimate EditorUtility Blueprint; specialized pure Engine node with typed EditorSubsystem result and actual partial authoring receipt. Intentional Blutility/UnrealEd editor-only references are not game asset equivalence. No utility Run, subsystem method invocation, explicit compile/save, retry or full editor workflow claim. Additionally admits the exact native EditorUtilityWidgetBlueprint/EditorUtilityWidget owner with an already-existing empty authored WidgetTree, no root/owned widget exports and no existing preview. Current tree/class/graph and native preview slot roles are protected at SDK boundaries without creating a CDO, widget or preview. Existing UtilityObject behavior and original writer/compile-save0 are retained. Populated trees and wider generated/custom owners remain incomplete; actual new Widget usage is implemented / usage validation pending.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"blueprint_name\":{\"type\":\"string\"},\"subsystem_class\":{\"type\":\"string\"},\"node_position\":{\"default\":null},\"graph_name\":{\"type\":\"string\",\"default\":\"\"},\"graph_id\":{\"type\":\"string\",\"default\":\"\"},\"graph_type\":{\"type\":\"string\",\"default\":\"\"},\"create_graph_if_missing\":{\"type\":\"boolean\",\"default\":false}},\"required\":[\"blueprint_name\",\"subsystem_class\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"prope") + TEXT("rties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}},{\"name\":\"CubelessMCPUE58Editor.CubelessBlueprintExpressionTools.add_blueprint_input_axis_event_node\",\"description\":\"Original legacy InputAxisEvent constructor with explicit consume/paused/override flags. Empty or unmapped axis names remain admitted and may yield SDK compile warnings. Actual event delegate/value pins and partial source/graph/lifecycle receipt only; no mapping settings mutation, explicit compile/save, runtime input or full pipeline claim.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"blueprint_name\":{\"type\":\"string\"},\"axis_name\":{\"type\":\"string\"},\"node_position\":{\"default\":null},\"consume_input\":{\"type\":\"boolean\",\"default\":true},\"execute_when_paused\":{\"type\":\"boolean\",\"default\":false},\"override_parent_binding\":{\"type\":\"boolean\",\"default\":false},\"graph_name\":{\"type\":\"string\",\"default\":\"\"},\"graph_id\":{\"type\":\"string\",\"default\":\"\"},\"graph_type\":{\"type\":\"string\",\"default\":\"\"},\"create_graph_if_missing\":{\"type\":\"boolean\",\"default\":false}},\"required\":[\"blueprint_name\",\"axis_name\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"}") + TEXT(",\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}},{\"name\":\"CubelessMCPUE58Editor.CubelessBlueprintExpressionTools.add_blueprint_enhanced_input_action_node\",\"description\":\"Original direct EnhancedInputAction event constructor with a literal InputAction reference and original dependency guard. Native resolves the graph before action load and preserves actual late partial prefixes, loaded action class/value-type/property/current-pin proof and duplicate creation. No UI spawner reuse, mapping context mutation, explicit compile/save, runtime input or full pipeline claim.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"blueprint_name\":{\"type\":\"string\"},\"input_action\":{\"type\":\"string\"},\"node_position\":{\"default\":null},\"graph_name\":{\"type\":\"string\",\"default\":\"\"},\"graph_id\":{\"type\":\"string\",\"default\":\"\"},\"graph_type\":{\"type\":\"string\",\"default\":\"\"},\"create_graph_if_missing\":{\"type\":\"boolean\",\"default\":false}},\"required\":[\"blueprint_name\",\"input_action\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"re") + TEXT("try_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}}],\"description\":\"Original Math7, Boolean7, Compare8 and Literal8 with actual Engine mapped nodes, value validation and partial-failure reports; no full runtime/pipeline parity claim.\"}");
}

bool TypedResult(const FString& Text)
{
    TSharedPtr<FJsonObject> O;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), O) || !O.IsValid() || O->Values.Num() != 1) { return false; }
    const TSharedPtr<FJsonObject>* Value = nullptr;
    if (!O->TryGetObjectField(TEXT("returnValue"), Value) || !Value || !Value->IsValid() || (*Value)->Values.Num() != 11) { return false; }
    const auto Success = (*Value)->TryGetField(TEXT("success"));
    if (!Success.IsValid() || Success->Type != EJson::Boolean) { return false; }
    for (const TCHAR* Key : { TEXT("operation_id"), TEXT("stage"), TEXT("error_code"), TEXT("error"), TEXT("changes_json"),
        TEXT("compile_json"), TEXT("save_json"), TEXT("validation_json"), TEXT("retry_json"), TEXT("data_json") })
    { const auto Field = (*Value)->TryGetField(Key); if (!Field.IsValid() || Field->Type != EJson::String) { return false; } }
    return true;
}

FString FailureReceipt(const FPythonCommandEx& Command, bool Executed, const TCHAR* Reason)
{
    auto O = MakeShared<FJsonObject>(); O->SetStringField(TEXT("error"), Reason);
    O->SetStringField(TEXT("scope"), TEXT("Python statement was dispatched; mutation outcome is unverified unless the exact emitted operation receipt is independently recovered. No retry/rollback/secondary save."));
    O->SetBoolField(TEXT("python_statement_completed"), Executed); O->SetStringField(TEXT("command_result"), Command.CommandResult);
    TArray<TSharedPtr<FJsonValue>> Logs;
    for (const auto& Entry : Command.LogOutput)
    {
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("type"), LexToString(Entry.Type)); Row->SetStringField(TEXT("output"), Entry.Output);
        Logs.Add(MakeShared<FJsonValueObject>(Row));
    }
    O->SetArrayField(TEXT("captured_log_output"), Logs);
    FString Out; FJsonSerializer::Serialize(O, TJsonWriterFactory<>::Create(&Out)); return Out;
}

FOutput Run(const FString& Name, const FString& Input, const TSharedRef<FBlueprintExpressionRawCallState, ESPMode::ThreadSafe>& State)
{
    if (!AllowedMethod(Name) || !IsInGameThread() || IsEngineExitRequested() || !State->Active.Load())
    { return MakeError(FString(TEXT("blueprint_expression_raw_not_dispatched: game-thread/live owned handler required."))); }
    auto Subsystem = State->Subsystem.Get();
    if (!Subsystem || Subsystem->ToolsetRegistry.Find(ToolsetName).Get() != State->Identity)
    { return MakeError(FString(TEXT("blueprint_expression_raw_not_dispatched: exact retained Registry handler is unavailable or filtered."))); }
    IPythonScriptPlugin* Python = IPythonScriptPlugin::Get();
    if (!Python || !Python->IsPythonAvailable() || !Python->IsPythonInitialized())
    { return MakeError(FString(TEXT("blueprint_expression_raw_not_dispatched: existing initialized Python required; no load/force-enable/fallback."))); }
    const FTCHARToUTF8 UTF8(*Input, Input.Len());
    if (UTF8.Length() > MaxInputBytes)
    { return MakeError(FString(TEXT("blueprint_expression_raw_not_dispatched: raw request exceeds128KiB UTF8 limit."))); }
    const FUTF8ToTCHAR InputRoundTrip(reinterpret_cast<const UTF8CHAR*>(UTF8.Get()), UTF8.Length());
    if (FString(InputRoundTrip.Length(), InputRoundTrip.Get()) != Input)
    { return MakeError(FString(TEXT("blueprint_expression_raw_not_dispatched: input is not lossless valid Unicode."))); }
    const FString Encoded = FBase64::Encode(reinterpret_cast<const uint8*>(UTF8.Get()), UTF8.Length());
    const FString Nonce = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
    FPythonCommandEx Command; Command.Flags = EPythonCommandFlags::Unattended;
    Command.ExecutionMode = EPythonCommandExecutionMode::ExecuteStatement;
    Command.Command = FString::Printf(TEXT("from cubeless_mcp_ue58.registration.raw_blueprint_expression import emit_result as _cubeless_blueprint_expression_emit; _cubeless_blueprint_expression_emit('%s','%s','%s')"), *Name, *Encoded, *Nonce);
    // Interpolated method is one of two exact literals; remaining values are local Base64/GUID.
    const bool Executed = Python->ExecPythonCommandEx(Command);
    const FString Prefix = FString(ResultPrefix) + Nonce + TEXT(":"); FString Payload; int32 Markers = 0;
    for (const auto& Entry : Command.LogOutput)
    {
        TArray<FString> Lines; Entry.Output.ParseIntoArrayLines(Lines, false);
        for (const auto& Line : Lines) { if (Line.StartsWith(Prefix, ESearchCase::CaseSensitive)) { ++Markers; Payload = Line.Mid(Prefix.Len()); } }
    }
    TArray<uint8> Bytes;
    if (Markers != 1 || Payload.Len() > ((MaxResultBytes + 2) / 3) * 4 || !FBase64::Decode(Payload, Bytes)
        || Bytes.Num() == 0 || Bytes.Num() > MaxResultBytes || FBase64::Encode(Bytes) != Payload)
    { return MakeError(FailureReceipt(Command, Executed, TEXT("blueprint_expression_raw_return_unverified: missing/multiple/invalid/oversized exact nonce marker."))); }
    const FUTF8ToTCHAR Decoded(reinterpret_cast<const UTF8CHAR*>(Bytes.GetData()), Bytes.Num());
    const FString Result(Decoded.Length(), Decoded.Get()); const FTCHARToUTF8 RoundTrip(*Result, Result.Len());
    if (RoundTrip.Length() != Bytes.Num() || FMemory::Memcmp(RoundTrip.Get(), Bytes.GetData(), Bytes.Num()) != 0 || !TypedResult(Result))
    { return MakeError(FailureReceipt(Command, Executed, TEXT("blueprint_expression_raw_return_unverified: emitted bytes are not exact UTF8/standard typed returnValue JSON."))); }
    // A valid emitted receipt remains authoritative even if later Python logging
    // reported an exception. Never synthesize known0 after Python dispatch.
    if (!Executed) { UE_LOG(LogCubelessBlueprintExpressionRaw, Warning, TEXT("Expression Python statement reported failure after a valid operation receipt; returned receipt preserved.")); }
    return MakeValue(Result);
}

class FBlueprintExpressionRawToolset final : public UE::ToolsetRegistry::FToolset
{
public:
    explicit FBlueprintExpressionRawToolset(const TSharedRef<FBlueprintExpressionRawCallState, ESPMode::ThreadSafe>& InState) : State(InState) {}
    virtual FString GetToolsetName() const override { return ToolsetName; }
    virtual FString GetToolsetVersion() const override { return TEXT("1.0"); }
    virtual FString GetToolsetDescription() const override { return TEXT("Original Math7, Boolean7, Compare8, Literal8, SubsystemFromPC7, ConstructObject7, InputAction7, Self6, EditorSubsystem7, InputAxisEvent10 and EnhancedInputAction7, fixed journaled Engine authoring with actual defaults and partial-failure reports; EditorSubsystem requires a legitimate Editor Utility owner and input events require an eligible input graph."); }
protected:
    virtual FString GetJsonSchemaInternal() const override { return Schema(); }
    virtual TFuture<FOutput> ExecuteToolInternal(const FString& Name, const FString& Input) override
    {
        if (!AllowedMethod(Name)) { return MakeFulfilledPromise<FOutput>(MakeError(FString(TEXT("blueprint_expression_raw_not_dispatched: exact named method required.")))).GetFuture(); }
        const FTCHARToUTF8 InputBytes(*Input, Input.Len());
        if (InputBytes.Length() > MaxInputBytes)
        { return MakeFulfilledPromise<FOutput>(MakeError(FString(TEXT("blueprint_expression_raw_not_dispatched: raw request exceeds128KiB UTF8 limit.")))).GetFuture(); }
        // Capture only strings and shared lifetime state; no UObject is accessed off the game thread.
        if (IsInGameThread()) { return MakeFulfilledPromise<FOutput>(Run(Name, Input, State)).GetFuture(); }
        auto Promise = MakeShared<TPromise<FOutput>, ESPMode::ThreadSafe>(); auto Future = Promise->GetFuture();
        const auto LiveState = State;
        AsyncTask(ENamedThreads::GameThread, [Promise, LiveState, Name, Input]() { Promise->SetValue(Run(Name, Input, LiveState)); });
        return Future;
    }
private:
    TSharedRef<FBlueprintExpressionRawCallState, ESPMode::ThreadSafe> State;
};
}

bool FBlueprintExpressionRawRegistration::Register()
{
    if (!IsInGameThread() || !UObjectInitialized() || IsEngineExitRequested()) { return false; }
    if (Handler.IsValid())
    { auto Subsystem = RegistrySubsystem.Get(); return Subsystem && Subsystem->ToolsetRegistry.Find(ToolsetName) == Handler; }
    auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Cubeless original typed selection raw registration"));
    if (!Subsystem.HasValue()) { return false; }
    if (Subsystem.GetValue()->ToolsetRegistry.Find(ToolsetName).IsValid()) { return false; } // Never replace any existing handler.
    auto State = MakeShared<FBlueprintExpressionRawCallState, ESPMode::ThreadSafe>(); State->Subsystem = Subsystem.GetValue().Get();
    TSharedPtr<UE::ToolsetRegistry::FToolset> Candidate = MakeShared<FBlueprintExpressionRawToolset>(State);
    State->Identity = Candidate.Get();
    if (!Subsystem.GetValue()->ToolsetRegistry.RegisterToolset(Candidate)) { return false; }
    RegistrySubsystem = Subsystem.GetValue().Get(); Handler = Candidate; CallState = State; State->Active.Store(true);
    return true;
}

void FBlueprintExpressionRawRegistration::Unregister()
{
    if (CallState.IsValid()) { CallState->Active.Store(false); } // Queued requests must not begin Python after shutdown.
    if (IsInGameThread() && UObjectInitialized())
    {
        if (auto Subsystem = RegistrySubsystem.Get())
        {
            if (Handler.IsValid() && HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, Handler))
            { Subsystem->ToolsetRegistry.UnregisterToolset(Handler); }
        }
    }
    Handler.Reset(); CallState.Reset(); RegistrySubsystem.Reset(); // No new subsystem discovery during teardown.
}
}
