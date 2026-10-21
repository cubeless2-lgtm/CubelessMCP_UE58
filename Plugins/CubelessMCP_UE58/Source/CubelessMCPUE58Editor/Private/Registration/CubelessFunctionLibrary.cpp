#include "Registration/CubelessFunctionLibrary.h"
#include "Compatibility/UE58/CubelessLoopApi.h"
#include "Operations/CubelessFunctionOperations.h"
#include "Compatibility/UE58/CubelessBlueprintMakeStructDefaultsConverter.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "ToolsetRegistry/ToolsetRegistry.h"
#include "ToolsetRegistry/ToolsetRegistrySubsystem.h"
#include "ToolsetRegistry/ToolsetJsonConverter.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"
using namespace Cubeless;
FString UCubelessFunctionLibrary::InspectCompiledFunction(const FString& B, const FString& C, const FString& N)
{ return Operations::EncodeFunctionResult(UE58::Function::InspectCompiledFunction(B, C, N), TEXT("inspect_compiled_function")); }
FString UCubelessFunctionLibrary::ResolveBlueprintGraph(const FString& B, const FString& N, const FString& I, const FString& T, bool C)
{ return Operations::EncodeFunctionResult(UE58::Function::Resolve(B, N, I, T, C), TEXT("resolve_blueprint_graph")); }
FString UCubelessFunctionLibrary::AddBlueprintFunctionParameter(const FString& B, const FString& N, const FString& T,
    const FString& D, const FString& V, const FString& O, bool A, const FString& GN, const FString& GI, const FString& GT, bool C)
{ return Operations::EncodeFunctionResult(UE58::Function::AddParameter(B, N, T, D, V, O, A, GN, GI, GT, C), TEXT("add_blueprint_function_parameter")); }
FString UCubelessFunctionLibrary::AddBlueprintReturnNode(const FString& B, const FString& P,
    const FString& GN, const FString& GI, const FString& GT, bool C)
{ return Operations::EncodeFunctionResult(UE58::Function::AddReturn(B, P, GN, GI, GT, C), TEXT("add_blueprint_return_node")); }
FString UCubelessFunctionLibrary::AddBlueprintDynamicCastNode(const FString& B, const FString& T,
    const FString& P, bool Pure, const FString& GN, const FString& GI, const FString& GT, bool C)
{ return Operations::EncodeFunctionResult(UE58::Function::AddDynamicCast(B, T, P, Pure, GN, GI, GT, C), TEXT("add_blueprint_dynamic_cast_node")); }
FString UCubelessFunctionLibrary::AddBlueprintCallFunctionNode(const FString& B, const FString& C, const FString& N,
    const FString& D, const FString& P, const FString& GN, const FString& GI, const FString& GT, bool CG, bool L, bool E, bool W, bool DP, bool I)
{ return Operations::EncodeFunctionResult(UE58::Function::AddCall(B, C, N, D, P, GN, GI, GT, CG, L, E, W, DP, I), TEXT("add_blueprint_call_function_node")); }
FString UCubelessFunctionLibrary::AddBlueprintMakeStructNode(const FString& B, const FString& T, const FString& P,
    const FString& GN, const FString& GI, const FString& GT, bool C)
{ return Operations::EncodeFunctionResult(UE58::Function::AddMakeStruct(B, T, P, GN, GI, GT, C), TEXT("add_blueprint_make_struct_node")); }

FString UCubelessFunctionLibrary::AddBlueprintBreakAssetDataNode(const FString& B,const FString& T,const FString& P,
    const FString& GN,const FString& GI,const FString& GT,bool C)
{ return Operations::EncodeFunctionResult(UE58::Function::AddBreakAssetData(B,T,P,GN,GI,GT,C),TEXT("add_blueprint_break_struct_node")); }

FString UCubelessFunctionLibrary::PrepareBlueprintMakeStructToolset(UClass* ToolsetClass)
{
    auto Report = MakeShared<FJsonObject>(); Report->SetBoolField(TEXT("success"), false); Report->SetBoolField(TEXT("postcondition_passed"), false); Report->SetStringField(TEXT("class_path"), ToolsetClass ? ToolsetClass->GetPathName() : FString());
    for (const TCHAR* Field : { TEXT("exact_converter_active"), TEXT("default_verified"), TEXT("mutation_attempted"), TEXT("changed"), TEXT("asset_mutation_attempted"), TEXT("compile_requested"), TEXT("compile_attempted"), TEXT("save_requested"), TEXT("save_attempted"), TEXT("saved") }) { Report->SetBoolField(Field, false); }
    Report->SetBoolField(TEXT("change_state_known"), true); Report->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>()); Report->SetNumberField(TEXT("metadata_write_attempt_count"), 0); Report->SetNumberField(TEXT("metadata_changed_count"), 0);
    Report->SetStringField(TEXT("scope"), TEXT("Only appended MakeStruct method's exact position empty-array metadata and graph_name/graph_id/graph_type empty-string metadata (four targets); public nullable position/default null and selectors remain; old Graph defaults, Python bodies/assets/compile/save and global conversion remain unchanged.")); FString Error;
    if (!IsInGameThread()) { Error = TEXT("Blueprint MakeStruct default preparation requires the game thread."); }
    else if (!ToolsetClass || ToolsetClass->GetPathName() != TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/graph_toolset_PY.CubelessBlueprintGraphTools")) { Error = TEXT("Exact mounted current CubelessBlueprintGraphTools class required."); }
    else if (UToolsetRegistry::IsToolsetClassRegistered(ToolsetClass)) { Error = TEXT("Invalidate the exact cached Graph class before preparation/registration."); }
    else
    {
        auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Cubeless Blueprint MakeStruct empty default preparation"));
        if (!Subsystem.HasValue()) { Error = TEXT("Live official registry subsystem required."); }
        else
        {
            UFunction* Function = ToolsetClass->FindFunctionByName(TEXT("add_blueprint_make_struct_node")); bool Active = true;
            for (const FName Name : { FName(TEXT("graph_name")), FName(TEXT("graph_id")), FName(TEXT("graph_type")), FName(TEXT("node_position")) })
            {
                FProperty* Property = Function ? FindFProperty<FProperty>(Function, Name) : nullptr; auto Converter = Property ? Subsystem.GetValue()->ToolsetRegistry.GetConverterForProperty(Property) : nullptr;
                Active &= Converter.IsValid() && Converter->GetName() == TEXT("CubelessMCP_UE58.BlueprintMakeStructEmptyStringDefaults.v1");
            }
            Report->SetBoolField(TEXT("exact_converter_active"), Active);
            if (!Active) { Error = TEXT("The three exact FString selectors and direct numeric-array position must use the registered finite converter."); }
            else if (Cubeless::UE58::PrepareBlueprintMakeStructDefaults(ToolsetClass, Report, Error)) { Report->SetBoolField(TEXT("success"), true); Report->SetBoolField(TEXT("postcondition_passed"), true); }
        }
    }
    Report->SetStringField(TEXT("stage"), Report->GetBoolField(TEXT("mutation_attempted")) ? TEXT("metadata_preparation") : TEXT("schema_preflight")); Report->SetStringField(TEXT("error_code"), Error.IsEmpty() ? FString() : TEXT("make_struct_default_preparation_failed")); Report->SetStringField(TEXT("error"), Error); FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json)); return Json;
}

FString UCubelessFunctionLibrary::AddBlueprintDataTableBoolSelectNode(const FString& B,const FString& P,
    const FString& GN,const FString& GI,const FString& GT,bool C)
{return Operations::EncodeFunctionResult(UE58::Function::AddDataTableBoolSelect(B,P,GN,GI,GT,C),TEXT("add_blueprint_data_table_bool_select_node"));}

FString UCubelessFunctionLibrary::ConnectDataTableBoolSelectPins(const FString& G,const FString& S,const FString& SP,const FString& T,const FString& TP)
{auto J=Cubeless::UE58::Function::ConnectDataTableBoolSelect(G,S,SP,T,TP);FString Json;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Json));return Json;}

FString UCubelessFunctionLibrary::ConnectNameForEachPins(const FString& G,const FString& S,const FString& SP,const FString& T,const FString& TP)
{auto J=Cubeless::UE58::Loop::ConnectNameForEachPins(G,S,SP,T,TP);FString Json;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Json));return Json;}

FString UCubelessFunctionLibrary::AddBlueprintIsValidExecMacroNode(const FString& B,const FString& P,
    const FString& GN,const FString& GI,const FString& GT,bool C)
{return Operations::EncodeFunctionResult(UE58::Function::AddIsValidExecMacro(B,P,GN,GI,GT,C),TEXT("add_blueprint_is_valid_exec_macro_node"));}
