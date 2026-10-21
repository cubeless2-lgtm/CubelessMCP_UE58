#include "Registration/CubelessStateTreeMutationLibrary.h"
#include "Compatibility/UE58/CubelessStateTreeMutationApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "Registration/CubelessJsonConverters.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
FString UCubelessStateTreeMutationLibrary::AddStateTreeState(const FString& AssetPath, const FString& StateName,
    const FString& ParentPath, const FString& StateType, const FString& SelectionBehavior, bool bDryRun)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::StateTreeMutation::Add(
        AssetPath, StateName, ParentPath, StateType, SelectionBehavior, bDryRun), TEXT("add_state_tree_state"));
}
FString UCubelessStateTreeMutationLibrary::CompileStateTree(const FString& AssetPath, bool bSave)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::StateTreeMutation::Compile(AssetPath, bSave), TEXT("compile_state_tree"));
}
namespace
{
using FStateTreeAttachmentOperation = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString StateTreeAttachmentReport(UClass* ToolsetClass, const TCHAR* Operation, FStateTreeAttachmentOperation Callback)
{
    auto Report = MakeShared<FJsonObject>(); FString Error;
    Report->SetStringField(TEXT("operation"), Operation);
    Report->SetBoolField(TEXT("ready"), false); Report->SetBoolField(TEXT("verified"), false);
    Report->SetBoolField(TEXT("asset_mutation"), false); Report->SetBoolField(TEXT("preparation_required"), false);
    Report->SetBoolField(TEXT("actual_named_wire_verified"), false);
    bool bVerified = false;
    if (!Cubeless::UE58::IsEditorGameThread()) { Error = TEXT("StateTree strict input verification requires the editor game thread"); }
    else { bVerified = Callback(ToolsetClass, Report, Error); }
    Report->SetBoolField(TEXT("ready"), bVerified); Report->SetBoolField(TEXT("verified"), bVerified); Report->SetStringField(TEXT("error"), Error);
    FString Text; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Text)); return Text;
}
}
FString UCubelessStateTreeMutationLibrary::InstallStateTreeStrictDryRunToolset(UClass* ToolsetClass)
{
    return StateTreeAttachmentReport(ToolsetClass, TEXT("install_state_tree_strict_dry_run_toolset"),
        &Cubeless::Registration::InstallStateTreeStrictDryRunToolset);
}
FString UCubelessStateTreeMutationLibrary::VerifyStateTreeStrictDryRunToolset(UClass* ToolsetClass)
{
    return StateTreeAttachmentReport(ToolsetClass, TEXT("verify_state_tree_strict_dry_run_toolset"),
        &Cubeless::Registration::VerifyStateTreeStrictDryRunToolset);
}
FString UCubelessStateTreeMutationLibrary::RemoveStateTreeStrictDryRunToolset(UClass* ToolsetClass)
{
    return StateTreeAttachmentReport(ToolsetClass, TEXT("remove_state_tree_strict_dry_run_toolset"),
        &Cubeless::Registration::RemoveStateTreeStrictDryRunToolset);
}
