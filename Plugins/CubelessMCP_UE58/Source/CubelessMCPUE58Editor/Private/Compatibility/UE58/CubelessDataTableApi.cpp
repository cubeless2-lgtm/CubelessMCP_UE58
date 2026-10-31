// Original DataTable six-contract unit, finite native GameplayTagTableRow admission.
// Editor compatibility only: callbacks/loads are scoped, never advertised as zero work.
#include "Compatibility/UE58/CubelessDataTableApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "DataTableEditorUtils.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Animation/Skeleton.h"
#include "ReferenceSkeleton.h"
#include "Engine/DataTable.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/CompositeDataTable.h"
#include "Animation/MirrorDataTable.h"
#include "GameplayTagsManager.h"
#include "StructUtils/UserDefinedStruct.h"
#include "Kismet2/StructureEditorUtils.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "JsonObjectStructInterface.h"
#include "EditorFramework/AssetImportData.h"
#include "UObject/MetaData.h"
#include "Internationalization/Text.h"
#include "Editor/TransBuffer.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "UObject/NoExportTypes.h"

#include "Compatibility/UE58/CubelessConnectionApi.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_GetDataTableRow.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Variable.h"
#include "K2Node_CallFunction.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace Cubeless::UE58::DataTable
{
namespace
{
constexpr const TCHAR* TagRowPath = TEXT("/Script/GameplayTags.GameplayTagTableRow");
constexpr const TCHAR* MirrorRowPath = TEXT("/Script/Engine.MirrorTableRow");
constexpr int32 MaxRowsObserved = 5000, MaxText = 1048576, MaxFields = 2048, MaxPackages = 8192, MaxObjects = 4096;
constexpr int64 MaxDiskBytes = 64 * 1024 * 1024;
struct FResult
{
    FString Operation, Stage = TEXT("preflight"), Code, Error;
    bool Success = false, Attempted = false, Known = true, Changed = false, Post = false, SaveRequested = false, SaveAttempted = false, Saved = false;
    TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
};
void AppendLegacyMutationContract(FResult& R);
void Complete(FResult& R);
void Count(FResult& R, const TCHAR* Field) { R.Data->SetNumberField(Field, R.Data->GetNumberField(Field) + 1); }
bool Fail(FResult& R, const TCHAR* Code, const FString& Error, bool Unknown = false)
{ R.Code = Code; R.Error = Error; R.Success = false; R.Post = false; if (Unknown) { R.Known = false; } return false; }
FString Encode(FResult& R)
{
    AppendLegacyMutationContract(R);
    auto J = MakeShared<FJsonObject>(); J->SetBoolField(TEXT("success"), R.Success); J->SetStringField(TEXT("operation"), R.Operation); J->SetStringField(TEXT("stage"), R.Stage);
    J->SetBoolField(TEXT("mutation_attempted"), R.Attempted); J->SetBoolField(TEXT("change_state_known"), R.Known); J->SetBoolField(TEXT("outcome_unknown"), !R.Known);
    if (R.Known) { J->SetBoolField(TEXT("changed"), R.Changed); } else { J->SetField(TEXT("changed"), MakeShared<FJsonValueNull>()); }
    J->SetBoolField(TEXT("postcondition_passed"), R.Post); J->SetBoolField(TEXT("compile_requested"), false); J->SetBoolField(TEXT("compile_attempted"), false); J->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    J->SetBoolField(TEXT("save_requested"), R.SaveRequested); J->SetBoolField(TEXT("save_attempted"), R.SaveAttempted); J->SetBoolField(TEXT("saved"), R.Saved);
    if (!R.Code.IsEmpty()) { J->SetStringField(TEXT("error_code"), R.Code); J->SetStringField(TEXT("error"), R.Error); } J->SetObjectField(TEXT("data"), R.Data);
    FString Text; FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&Text)); return Text;
}
void Init(FResult& R, const TCHAR* Operation, bool Save = false)
{
    R.Operation = Operation; R.SaveRequested = Save; R.Data->SetBoolField(TEXT("generic_native_row_profile"),false);
    for (const TCHAR* Field : { TEXT("asset_load_count"), TEXT("row_struct_load_count"), TEXT("package_create_attempts"), TEXT("asset_allocation_attempts"), TEXT("asset_registry_publication_calls"), TEXT("explicit_modify_calls"), TEXT("outer_transaction_calls"), TEXT("editor_add_row_calls"), TEXT("editor_remove_row_calls"), TEXT("json_conversion_calls"), TEXT("mark_package_dirty_calls"), TEXT("row_data_broadcast_calls"), TEXT("save_attempt_count"), TEXT("explicit_clear_dirty_calls") }) { R.Data->SetNumberField(Field, 0); }
    for (const TCHAR* Field : { TEXT("observations_complete"), TEXT("source_preserved"), TEXT("unselected_state_preserved"), TEXT("independent_readback_passed"), TEXT("persistence_verified"), TEXT("runtime_verified"), TEXT("implicit_engine_work_measured") }) { R.Data->SetBoolField(Field, false); }
    R.Data->SetArrayField(TEXT("asset_load_calls"), {}); R.Data->SetArrayField(TEXT("save_calls"), {}); R.Data->SetBoolField(TEXT("save_requested"), Save); R.Data->SetBoolField(TEXT("saved"), false);
    R.Data->SetStringField(TEXT("dependency_tier"), TEXT("public EngineAPI/Editor DataTable helpers and direct SavePackage; original ServerRPC is provenance only; no gameplay Cubeless class"));
    R.Data->SetStringField(TEXT("admission_scope"), TEXT("Exact native ordinary UDataTable and MirrorDataTable profiles; readonly CompositeDataTable DAG with native or admitted compiled UDS rows; bounded saved-clean compiled UDS scalar/native Vector/Rotator/Transform row profiles. Existing-table UDS schema/list/get/add-update/remove additionally admit finite acyclic compiled UDS, scalar7-or-UDS arrays, scalar7 Sets and scalar-key Maps to scalar/admitted-array/UDS values. Current owned root/Inner/Element/Key/Value descriptors precede fresh copied ordered values or unordered typed slot bags retaining multiplicity. Bounds are4096 slots/container,16384 copied cells,depth8,16 nested sources/64 fields each; original JSON coercion, converter partial/no-notify/no-save and save-before-subset failure remain. UDS specialized CompositeDataTable Create, custom operators/native-struct arrays/direct nested container elements, source authoring, K2 widening, specialized factory/autofill/protected refresh and runtime mirroring remain incomplete. Existing UDS CRUD additionally admits resolved exact FByteProperty enums in direct/nested/Array/Set/Map positions, backed by saved-clean Game UserDefinedEnum or native Engine/CoreUObject UEnum; FEnumProperty/custom/generated/dirty sources remain incomplete."));
    R.Data->SetStringField(TEXT("change_accounting_scope"), TEXT("legacy selected row/value delta or fresh asset creation; actual Modify/dirty/callback/load/save receipts and raw whole-target states are separate, including matching Add work"));
    R.Data->SetStringField(TEXT("effect_scope"), TEXT("explicit loads/PostLoad, transactions, DataTable row delegates, loaded Blueprint row-list consumers and save callbacks can perform implicit work; source identity/schema, target state/references/disk and other package dirty flags are observed, not every singleton/CDO/consumer cache"));
    R.Data->SetBoolField(TEXT("native_cdo_state_observed"), false); R.Data->SetBoolField(TEXT("atomic_rollback_available"), false); R.Data->SetBoolField(TEXT("auto_retry_safe"), false);
}
bool Loaded(const UObject* O) { return IsValid(O) && !O->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_BeginDestroyed | RF_FinishDestroyed); }
bool CleanPath(const FString& S)
{ for (const TCHAR* Token : { TEXT("/Script/Cubeless"), TEXT("/Script/UnrealMCP"), TEXT("/Engine/PythonTypes"), TEXT("/Game/UnrealMCP"), TEXT("/CubelessMCP_UE58/"), TEXT("/UnrealMCP/"), TEXT("/MCPUnreal/"), TEXT("/MCP_Unreal/") }) { if (S.Contains(Token, ESearchCase::IgnoreCase)) { return false; } } return true; }
FString Normalize(FString S)
{ S = FPackageName::ExportTextPathToObjectPath(S).TrimStartAndEnd(); S.TrimQuotesInline(); S.ReplaceInline(TEXT("\\"), TEXT("/")); if (!S.IsEmpty() && !S.Contains(TEXT("."))) { S += TEXT(".") + FPackageName::GetShortName(S); } return S; }
bool AssetPath(const FString& S, bool Mutable)
{ const FString P = FPackageName::ObjectPathToPackageName(S); return S.Len() < 1024 && FPackageName::IsValidObjectPath(S) && !S.Contains(TEXT(":")) && CleanPath(S) && (P.StartsWith(TEXT("/Game/")) || (!Mutable && P.StartsWith(TEXT("/Engine/")))); }
bool RowName(const FString& S) { FText Why; return !S.IsEmpty() && S.Len() < NAME_SIZE && !FName(*S).IsNone() && FName::IsValidXName(S, INVALID_OBJECTNAME_CHARACTERS, &Why); }
TSharedRef<FJsonObject> Identity(const UObject* O)
{
    auto J = MakeShared<FJsonObject>(); J->SetBoolField(TEXT("valid"), Loaded(O)); J->SetStringField(TEXT("path"), Loaded(O) ? O->GetPathName() : FString()); J->SetStringField(TEXT("class"), Loaded(O) ? O->GetClass()->GetPathName() : FString());
    J->SetStringField(TEXT("outer"), Loaded(O) && O->GetOuter() ? O->GetOuter()->GetPathName() : FString()); J->SetStringField(TEXT("object_flags"), Loaded(O) ? LexToString(static_cast<uint32>(O->GetFlags())) : FString()); return J;
}
bool Equal(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
{
    if (!A || !B || A->Type != B->Type) { return false; }
    if (A->Type == EJson::Object) { const auto X = A->AsObject(), Y = B->AsObject(); if (X->Values.Num() != Y->Values.Num()) { return false; } for (const auto& Field : X->Values) { if (!Equal(Field.Value, Y->TryGetField(Field.Key.ToView()))) { return false; } } return true; }
    if (A->Type == EJson::Array) { const auto& X = A->AsArray(); const auto& Y = B->AsArray(); if (X.Num() != Y.Num()) { return false; } for (int32 I = 0; I < X.Num(); ++I) { if (!Equal(X[I], Y[I])) { return false; } } return true; }
    if (A->Type == EJson::String) { return A->AsString() == B->AsString(); } if (A->Type == EJson::Boolean) { return A->AsBool() == B->AsBool(); } if (A->Type == EJson::Number) { return A->AsNumber() == B->AsNumber(); } return A->Type == EJson::Null;
}
bool Equal(const TSharedRef<FJsonObject>& A, const TSharedRef<FJsonObject>& B) { return Equal(MakeShared<FJsonValueObject>(A), MakeShared<FJsonValueObject>(B)); }
bool ContainsExpected(const TSharedPtr<FJsonValue>& Expected, const TSharedPtr<FJsonValue>& Actual)
{
    if (!Expected || !Actual || Expected->Type != Actual->Type) { return false; }
    if (Expected->Type == EJson::Object)
    {
        for (const auto& Field : Expected->AsObject()->Values)
        {
            auto Found = Actual->AsObject()->TryGetField(Field.Key.ToView());
            if (!Found) { for (const auto& Row : Actual->AsObject()->Values) { if (FString(Row.Key.ToView()).Equals(FString(Field.Key.ToView()), ESearchCase::IgnoreCase)) { Found = Row.Value; break; } } }
            if (!ContainsExpected(Field.Value, Found)) { return false; }
        } return true;
    }
    if (Expected->Type == EJson::Number) { return FMath::IsNearlyEqual(Expected->AsNumber(), Actual->AsNumber()); }
    if (Expected->Type == EJson::Array) { const auto& X = Expected->AsArray(); const auto& Y = Actual->AsArray(); if (X.Num() != Y.Num()) { return false; } for (int32 I = 0; I < X.Num(); ++I) { if (!ContainsExpected(X[I], Y[I])) { return false; } } return true; }
    return Equal(Expected, Actual);
}
TSharedPtr<FJsonObject> ContractObject(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key)
{
    const auto Value = Object ? Object->TryGetField(Key) : nullptr;
    return Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
}
bool ContractBool(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, bool& Out)
{
    const auto Value = Object ? Object->TryGetField(Key) : nullptr;
    if (!Value.IsValid() || Value->Type != EJson::Boolean) { return false; } Out = Value->AsBool(); return true;
}
bool ContractCount(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, int32& Out)
{
    const auto Value = Object ? Object->TryGetField(Key) : nullptr; double Number = 0;
    if (!Value.IsValid() || Value->Type != EJson::Number || !Value->TryGetNumber(Number) || !FMath::IsFinite(Number)
        || Number < 0 || Number > MaxRowsObserved || Number != FMath::FloorToDouble(Number)) { return false; }
    Out = static_cast<int32>(Number); return true;
}
void AppendLegacyMutationContract(FResult& R)
{
    const bool IsAdd = R.Operation == TEXT("add_or_update_datatable_row"), IsRemove = R.Operation == TEXT("remove_datatable_row"), IsCreate = R.Operation == TEXT("create_datatable");
    if (!IsAdd && !IsRemove && !IsCreate) { return; }
    R.Data->SetBoolField(TEXT("legacy_mutation_contract_available"), false);
    R.Data->SetStringField(TEXT("legacy_mutation_contract_scope"), TEXT("Original mutation_result_v1 selected-row/empty-table and save predicates; raw typed outcome/counts/whole-target states and pre-save delta remain authoritative separately. No extra LoadAsset or disk reload."));
    const auto Before = ContractObject(R.Data, TEXT("before"));
    const auto Observation = ContractObject(R.Data, R.SaveRequested ? TEXT("after_save") : TEXT("after_observation"));
    const auto State = ContractObject(Observation, TEXT("state")); bool Complete = false;
    bool StoredPost = false; const bool HasStoredPost = ContractBool(R.Data, TEXT("postcondition_verified"), StoredPost);
    if (!Before || !Observation || !State || !ContractBool(Observation, TEXT("complete"), Complete) || !Complete
        || (!HasStoredPost && !R.SaveAttempted))
    { R.Data->SetStringField(TEXT("legacy_mutation_contract_unavailable_reason"), TEXT("Original terminal relookup stage was not reached or its current observation is incomplete; early/prewrite/allocation/conversion/preservation prefixes are not stamped v1.")); return; }
    bool SaveGate = !R.SaveRequested;
    if (R.SaveRequested)
    {
        const auto Saves = R.Data->TryGetField(TEXT("save_calls")); bool ReceiptSaved = false;
        if (!R.SaveAttempted || !Saves.IsValid() || Saves->Type != EJson::Array || Saves->AsArray().Num() != 1
            || !Saves->AsArray()[0].IsValid() || Saves->AsArray()[0]->Type != EJson::Object
            || !ContractBool(Saves->AsArray()[0]->AsObject(), TEXT("saved"), ReceiptSaved) || ReceiptSaved != R.Saved)
        { R.Data->SetStringField(TEXT("legacy_mutation_contract_unavailable_reason"), TEXT("Requested save has no exact returned save receipt and complete final observation; no successful compatibility claim.")); return; }
        SaveGate = ReceiptSaved;
    }
    int32 AfterCount = 0; if (!ContractCount(State, TEXT("row_count"), AfterCount))
    { R.Data->SetStringField(TEXT("legacy_mutation_contract_unavailable_reason"), TEXT("Actual terminal native row count is unobservable.")); return; }
    auto After = MakeShared<FJsonObject>(), Checks = MakeShared<FJsonObject>(), Delta = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Names; FString Method; bool Execution = false, Specific = false, Changed = false;
    auto Check = [&](const TCHAR* Name, bool Passed) { Checks->SetBoolField(Name, Passed); Names.Add(MakeShared<FJsonValueString>(Name)); };
    int32 Affected = 0, Created = 0, Updated = 0, Removed = 0, Skipped = 0, Failed = 0;
    if (IsAdd || IsRemove)
    {
        int32 BeforeCount = 0; bool Existed = false; FString RowName;
        const auto Rows = ContractObject(State, TEXT("rows"));
        if (!Rows || !ContractCount(Before, TEXT("row_count"), BeforeCount) || !ContractBool(Before, TEXT("exists"), Existed)
            || !Before->TryGetStringField(TEXT("row_name"), RowName) || RowName.IsEmpty())
        { R.Data->SetStringField(TEXT("legacy_mutation_contract_unavailable_reason"), TEXT("Selected row before/current map projection is incomplete.")); return; }
        TSharedPtr<FJsonValue> Values; FString StoredName; int32 MatchCount = 0;
        for (const auto& Entry : Rows->Values)
        {
            if (FString(Entry.Key.ToView()).Equals(RowName, ESearchCase::IgnoreCase))
            { ++MatchCount; Values = Entry.Value; StoredName = FString(Entry.Key.ToView()); }
        }
        auto Selection = MakeShared<FJsonObject>(); Selection->SetStringField(TEXT("caller_display_name"), RowName); Selection->SetStringField(TEXT("stored_display_name"), StoredName); Selection->SetNumberField(TEXT("match_count"), MatchCount);
        Selection->SetStringField(TEXT("comparison_scope"), TEXT("Case-insensitive identity of captured native FName display strings; caller spelling/raw RowMap keys remain unchanged.")); R.Data->SetObjectField(TEXT("legacy_mutation_selected_row"), Selection);
        if (MatchCount > 1)
        { R.Data->SetStringField(TEXT("legacy_mutation_contract_unavailable_reason"), TEXT("Selected row projection has ambiguous case-insensitive matches; no v1 claim or writer replay.")); return; }
        const bool Exists = Values.IsValid() && Values->Type == EJson::Object;
        After->SetNumberField(TEXT("row_count"), AfterCount); After->SetBoolField(TEXT("exists"), Exists); After->SetStringField(TEXT("row_name"), RowName);
        if (Exists) { After->SetBoolField(TEXT("serialized"), true); After->SetField(TEXT("values"), Values); }
        Method = TEXT("datatable_row_map_relookup");
        if (IsAdd)
        {
            bool Converted = false, Matched = false; const auto Requested = R.Data->TryGetField(TEXT("requested_values"));
            if (!ContractBool(R.Data, TEXT("json_conversion_returned"), Converted) || !Converted
                || !ContractBool(R.Data, TEXT("already_matched"), Matched) || !Requested.IsValid() || Requested->Type != EJson::Object)
            { R.Data->SetStringField(TEXT("legacy_mutation_contract_unavailable_reason"), TEXT("Original successful conversion/matching request stage was not reached.")); return; }
            Execution = true; Changed = !Matched;
            const bool ValuesMatch = Exists && ContainsExpected(Requested, Values), CountMatch = Exists && AfterCount == BeforeCount + (Existed ? 0 : 1);
            Check(TEXT("row_exists_after"), Exists); Check(TEXT("requested_values_match"), ValuesMatch); Check(TEXT("row_count_matches"), CountMatch); Check(TEXT("save_gate"), SaveGate);
            Specific = ValuesMatch && CountMatch && SaveGate; Affected = Changed ? 1 : 0; Created = Existed ? 0 : 1; Updated = Existed && Changed ? 1 : 0; Skipped = Changed ? 0 : 1; Failed = Specific ? 0 : 1;
        }
        else
        {
            int32 Calls = 0;
            if (!ContractCount(R.Data, TEXT("editor_remove_row_calls"), Calls) || Calls != 1 || !Existed)
            { R.Data->SetStringField(TEXT("legacy_mutation_contract_unavailable_reason"), TEXT("Original removal stage requires an actual previously existing selected row and one returned helper call.")); return; }
            Execution = true; Changed = true; const bool Absence = !Exists, CountMatch = AfterCount == BeforeCount - 1, Removal = Absence && CountMatch;
            Check(TEXT("row_existed_before"), Existed); Check(TEXT("row_absent_after"), Absence); Check(TEXT("row_count_decremented"), CountMatch); Check(TEXT("save_gate"), SaveGate);
            Specific = Removal && SaveGate; Affected = Removal ? 1 : 0; Removed = Removal ? 1 : 0; const int32 CompletionFailed = SaveGate ? 0 : 1; Failed = (Removal ? 0 : 1) + CompletionFailed; Delta->SetNumberField(TEXT("completion_failure_count"), CompletionFailed);
        }
    }
    else
    {
        const auto AssetIdentity = ContractObject(State, TEXT("identity")); FString Path, Struct; bool Exists = false;
        if (!AssetIdentity || !ContractBool(AssetIdentity, TEXT("valid"), Exists) || !AssetIdentity->TryGetStringField(TEXT("path"), Path)
            || !State->TryGetStringField(TEXT("row_struct"), Struct))
        { R.Data->SetStringField(TEXT("legacy_mutation_contract_unavailable_reason"), TEXT("Current loaded native asset identity/row-struct projection is incomplete.")); return; }
        After->SetBoolField(TEXT("exists"), Exists); After->SetStringField(TEXT("asset_path"), Path); After->SetStringField(TEXT("row_struct"), Struct); After->SetNumberField(TEXT("row_count"), AfterCount);
        bool UdsAdmitted=false,ScalarUdsAdmitted=false;FString UdsPath,ScalarUdsPath;const bool UdsMatch=(ContractBool(R.Data,TEXT("string1_uds_admitted"),UdsAdmitted)&&UdsAdmitted&&R.Data->TryGetStringField(TEXT("string1_uds_path"),UdsPath)&&Struct.Equals(UdsPath,ESearchCase::CaseSensitive))||(ContractBool(R.Data,TEXT("scalar_uds_crud_admitted"),ScalarUdsAdmitted)&&ScalarUdsAdmitted&&R.Data->TryGetStringField(TEXT("scalar_uds_path"),ScalarUdsPath)&&Struct.Equals(ScalarUdsPath,ESearchCase::CaseSensitive));
        Execution = Exists; Changed = true; const bool StructMatch = Struct == TagRowPath || Struct == MirrorRowPath || UdsMatch || R.Data->GetBoolField(TEXT("generic_native_row_profile")), Empty = AfterCount == 0;
        Check(TEXT("asset_exists_after"), Exists); Check(TEXT("row_struct_matches"), StructMatch); Check(TEXT("initial_row_count_zero"), Empty); Check(TEXT("save_gate"), SaveGate);
        Specific = Exists && StructMatch && Empty && SaveGate; Affected = 1; Created = Exists ? 1 : 0; Failed = Specific ? 0 : 1;
        Method = TEXT("current_loaded_native_datatable_relookup");
    }
    R.Data->SetObjectField(TEXT("legacy_mutation_check_results"), Checks);
    if (HasStoredPost && StoredPost != Specific)
    { R.Data->SetStringField(TEXT("legacy_mutation_contract_unavailable_reason"), TEXT("Retained native selected postcondition disagrees with the independently computed original row/count/save predicates; no compatibility claim.")); return; }
    if (Specific && (!R.Success || !R.Post || !R.Known))
    { R.Data->SetStringField(TEXT("legacy_mutation_contract_unavailable_reason"), TEXT("Original selected predicates passed but the stricter finite native preservation/status fence failed; no verified-success v1 claim.")); return; }
    const auto RawAfter = ContractObject(R.Data, TEXT("after"));
    if (Specific && (!RawAfter || !Equal(RawAfter.ToSharedRef(), After)))
    { R.Data->SetStringField(TEXT("legacy_mutation_contract_unavailable_reason"), TEXT("Actual final selected projection differs from the retained raw after scope; no direct compatibility claim.")); return; }
    Delta->SetNumberField(TEXT("affected_count"), Affected); Delta->SetNumberField(TEXT("created_count"), Created); Delta->SetNumberField(TEXT("updated_count"), Updated); Delta->SetNumberField(TEXT("removed_count"), Removed); Delta->SetNumberField(TEXT("skipped_count"), Skipped); Delta->SetNumberField(TEXT("failed_count"), Failed);
    auto Verification = MakeShared<FJsonObject>(); Verification->SetStringField(TEXT("status"), Specific ? TEXT("passed") : TEXT("failed")); Verification->SetStringField(TEXT("method"), Method);
    Verification->SetBoolField(TEXT("before_observed"), true); Verification->SetBoolField(TEXT("after_observed"), true); Verification->SetBoolField(TEXT("strict"), true); Verification->SetArrayField(TEXT("checks"), Names); Verification->SetObjectField(TEXT("check_results"), Checks);
    Verification->SetStringField(TEXT("observation_scope"), R.SaveRequested ? TEXT("Complete after_save current loaded native table/row projection; save receipt observed separately, no disk reload.") : TEXT("Complete after_observation current loaded native table/row projection; no save requested."));
    Verification->SetBoolField(TEXT("extra_asset_library_load_dispatched"), false); if (IsCreate) { Verification->SetStringField(TEXT("original_method"), TEXT("asset_library_datatable_relookup")); }
    const bool Verified = Execution && Specific; const FString Semantics = Verified ? TEXT("verified_postcondition") : Execution ? TEXT("postcondition_failed") : TEXT("failed_execution");
    auto Mutation = MakeShared<FJsonObject>(); Mutation->SetBoolField(TEXT("attempted"), true); Mutation->SetBoolField(TEXT("performed"), Execution); Mutation->SetStringField(TEXT("state"), Execution ? TEXT("performed") : TEXT("not_performed"));
    auto Contract = MakeShared<FJsonObject>(); Contract->SetStringField(TEXT("contract_version"), TEXT("mutation_result_v1")); Contract->SetStringField(TEXT("mode"), TEXT("apply")); Contract->SetBoolField(TEXT("dry_run"), false); Contract->SetBoolField(TEXT("DRY_RUN"), false);
    Contract->SetBoolField(TEXT("success"), Verified); Contract->SetBoolField(TEXT("changed"), Changed); Contract->SetBoolField(TEXT("mutation_performed"), Execution); Contract->SetObjectField(TEXT("mutation"), Mutation); Contract->SetObjectField(TEXT("before"), Before); Contract->SetObjectField(TEXT("after"), After); Contract->SetObjectField(TEXT("delta"), Delta); Contract->SetObjectField(TEXT("verification"), Verification); Contract->SetBoolField(TEXT("postcondition_verified"), Specific); Contract->SetBoolField(TEXT("verified_success"), Verified); Contract->SetStringField(TEXT("success_semantics"), Semantics);
    if (Execution && !Specific) { Contract->SetBoolField(TEXT("legacy_success"), true); Contract->SetStringField(TEXT("error_code"), TEXT("mutation_postcondition_failed")); }
    R.Data->SetObjectField(TEXT("legacy_mutation_contract"), Contract); R.Data->SetBoolField(TEXT("legacy_mutation_contract_available"), true);
    // Direct required metadata is computed from the same supported evidence; raw
    // before/after/delta and typed changed/attempted/known/save fields are not rewritten.
    R.Data->SetStringField(TEXT("contract_version"), TEXT("mutation_result_v1")); R.Data->SetStringField(TEXT("mode"), TEXT("apply")); R.Data->SetBoolField(TEXT("dry_run"), false); R.Data->SetBoolField(TEXT("DRY_RUN"), false);
    R.Data->SetBoolField(TEXT("verified_success"), Verified); R.Data->SetStringField(TEXT("success_semantics"), Semantics); R.Data->SetObjectField(TEXT("verification"), Verification); R.Data->SetObjectField(TEXT("mutation"), Mutation); R.Data->SetBoolField(TEXT("legacy_mutation_performed"), Execution);
    if (!HasStoredPost) { R.Data->SetBoolField(TEXT("postcondition_verified"), Specific); }
}
struct FStringUdsScope;
struct FNativeRowScope;
bool NativeRowAdmit(FNativeRowScope* Scope);
bool NativeRowWriterRunning=false;
bool StringUdsAdmit(FStringUdsScope* Scope);
struct FMirrorReadScope;
bool MirrorReadAdmit(FMirrorReadScope* Scope);
bool Properties(const UObject* O, TSharedRef<FJsonObject>& J, FMirrorReadScope* Scope = nullptr, FStringUdsScope* Uds = nullptr, FNativeRowScope* Native = nullptr)
{
    int32 Fields = 0, Chars = 0; if (!StringUdsAdmit(Uds) || !NativeRowAdmit(Native) || !MirrorReadAdmit(Scope) || !Loaded(O)) { return false; }
    for (TFieldIterator<FProperty> It(O->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
    {
        FProperty* P = *It;
        // Composite OldParentTables is delegate-maintenance cache, not authored parent order.
        if(O->GetClass()==UCompositeDataTable::StaticClass()&&P->GetOwnerStruct()==UCompositeDataTable::StaticClass()&&P->HasAnyPropertyFlags(CPF_Transient))continue;
        if (++Fields > MaxFields || P->ArrayDim < 1 || P->ArrayDim > 64 || !P->GetOwnerStruct()) { return false; }
        auto Row = MakeShared<FJsonObject>(); J->SetObjectField(P->GetName(), Row); Row->SetStringField(TEXT("owner"), P->GetOwnerStruct()->GetPathName()); Row->SetStringField(TEXT("class"), P->GetClass()->GetName()); Row->SetStringField(TEXT("flags"), LexToString(static_cast<uint64>(P->GetPropertyFlags()))); TArray<TSharedPtr<FJsonValue>> Values;
        for (int32 I = 0; I < P->ArrayDim; ++I) { FString S; const void* V = P->ContainerPtrToValuePtr<void>(O, I); const bool Exported=P->ExportText_Direct(S, V, V, const_cast<UObject*>(O), PPF_None); if (!StringUdsAdmit(Uds) || !NativeRowAdmit(Native) || !MirrorReadAdmit(Scope) || !Exported || (Chars += S.Len()) > MaxText) { return false; } Values.Add(MakeShared<FJsonValueString>(S)); } Row->SetArrayField(TEXT("values"), Values);
    } return true;
}
// IP: finite native value-only MirrorTableRow schema. Existing Tag row source body stays below.
bool MirrorSources(UScriptStruct* Type, TSharedRef<FJsonObject>& J)
{
    UClass* TableClass=UDataTable::StaticClass();J->SetObjectField(TEXT("table_class"),Identity(TableClass));J->SetObjectField(TEXT("row_struct"),Identity(Type));
    if(!Loaded(TableClass)||!TableClass->HasAnyClassFlags(CLASS_Native)||TableClass->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists)||TableClass->ClassGeneratedBy
        ||TableClass->GetOuter()!=TableClass->GetPackage()||TableClass->GetPackage()->GetName()!=TEXT("/Script/Engine")||!TableClass->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||TableClass->GetPackage()->IsDirty()
        ||!Loaded(Type)||Type->GetClass()!=UScriptStruct::StaticClass()||Type->GetPathName()!=MirrorRowPath||Type!=FindObject<UScriptStruct>(nullptr,MirrorRowPath)
        ||Type->GetOuter()!=Type->GetPackage()||Type->GetPackage()!=TableClass->GetPackage()||!(Type->StructFlags&STRUCT_Native)
        ||Type->GetStructureSize()!=sizeof(FMirrorTableRow)||!Loaded(Type->GetSuperStruct())||Type->GetSuperStruct()!=FindObject<UScriptStruct>(nullptr,TEXT("/Script/Engine.TableRowBase"))) { return false; }
    J->SetStringField(TEXT("table_class_flags"),LexToString(static_cast<uint32>(TableClass->GetClassFlags())));J->SetStringField(TEXT("row_struct_flags"),LexToString(static_cast<uint32>(Type->StructFlags)));J->SetNumberField(TEXT("row_struct_bytes"),Type->GetStructureSize());
    TArray<TSharedPtr<FJsonValue>> Fields;J->SetArrayField(TEXT("fields"),Fields);UEnum* EntryEnum=nullptr;int32 Count=0;TSet<FName> Names;
    for(TFieldIterator<FProperty> It(Type);It;++It)
    {
        FProperty* P=*It;const FName FieldName=P->GetFName();if(++Count>4||P->ArrayDim!=1||P->GetOwnerStruct()!=Type||!P->HasAnyPropertyFlags(CPF_Edit)||Names.Contains(FieldName)) { return false; }Names.Add(FieldName);
        const bool NameField=(FieldName==TEXT("Name")||FieldName==TEXT("MirroredName"))&&CastField<FNameProperty>(P);
        const bool BoolField=FieldName==TEXT("bEnabled")&&CastField<FBoolProperty>(P);
        const FByteProperty* Byte=FieldName==TEXT("MirrorEntryType")?CastField<FByteProperty>(P):nullptr;
        if(!NameField&&!BoolField&&!Byte) { return false; }if(Byte) { EntryEnum=Byte->Enum.Get(); }
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),P->GetName());Row->SetStringField(TEXT("authored_name"),P->GetAuthoredName());Row->SetStringField(TEXT("cpp_type"),P->GetCPPType(nullptr,0));Row->SetStringField(TEXT("class"),P->GetClass()->GetName());Row->SetStringField(TEXT("flags"),LexToString(static_cast<uint64>(P->GetPropertyFlags())));Fields.Add(MakeShared<FJsonValueObject>(Row));J->SetArrayField(TEXT("fields"),Fields);
    }
    if(Count!=4||!Names.Contains(TEXT("Name"))||!Names.Contains(TEXT("MirroredName"))||!Names.Contains(TEXT("MirrorEntryType"))||!Names.Contains(TEXT("bEnabled"))
        ||!Loaded(EntryEnum)||EntryEnum->GetClass()!=UEnum::StaticClass()||EntryEnum->GetPathName()!=TEXT("/Script/Engine.EMirrorRowType")||EntryEnum->GetOuter()!=Type->GetPackage()||EntryEnum->GetPackage()!=Type->GetPackage()
        ||EntryEnum->NumEnums()<5||EntryEnum->NumEnums()>16) { return false; }
    auto EnumState=MakeShared<FJsonObject>();J->SetObjectField(TEXT("mirror_entry_enum"),EnumState);EnumState->SetObjectField(TEXT("identity"),Identity(EntryEnum));EnumState->SetStringField(TEXT("cpp_type"),EntryEnum->CppType);
    const TCHAR* Required[5]={TEXT("Bone"),TEXT("AnimationNotify"),TEXT("Curve"),TEXT("SyncMarker"),TEXT("Custom")};TArray<TSharedPtr<FJsonValue>> Entries;
    for(int32 Index=0;Index<EntryEnum->NumEnums();++Index)
    {
        if(Index<5&&(EntryEnum->GetNameStringByIndex(Index)!=Required[Index]||EntryEnum->GetValueByIndex(Index)!=Index)) { return false; }
        auto Entry=MakeShared<FJsonObject>();Entry->SetStringField(TEXT("name"),EntryEnum->GetNameStringByIndex(Index));Entry->SetStringField(TEXT("full_name"),EntryEnum->GetNameByIndex(Index).ToString());Entry->SetNumberField(TEXT("value"),static_cast<double>(EntryEnum->GetValueByIndex(Index)));Entry->SetBoolField(TEXT("hidden"),EntryEnum->HasMetaData(TEXT("Hidden"),Index));Entry->SetBoolField(TEXT("spacer"),EntryEnum->HasMetaData(TEXT("Spacer"),Index));Entries.Add(MakeShared<FJsonValueObject>(Entry));
    }
    EnumState->SetArrayField(TEXT("entries"),Entries);FMirrorTableRow RowDefaults;auto Defaults=MakeShared<FJsonObject>();if(!FJsonObjectConverter::UStructToJsonObject(Type,&RowDefaults,Defaults,0,0)) { return false; }J->SetObjectField(TEXT("row_defaults"),Defaults);J->SetBoolField(TEXT("row_fields_have_no_UObject_references"),true);return true;
}
bool Sources(UScriptStruct* Type, TSharedRef<FJsonObject>& J)
{
    if(Loaded(Type)&&Type->GetPathName()==MirrorRowPath) { return MirrorSources(Type,J); }
    UClass* TableClass = UDataTable::StaticClass(); J->SetObjectField(TEXT("table_class"), Identity(TableClass)); J->SetObjectField(TEXT("row_struct"), Identity(Type));
    if (!Loaded(TableClass) || !TableClass->HasAnyClassFlags(CLASS_Native) || TableClass->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists) || TableClass->ClassGeneratedBy || TableClass->GetOuter() != TableClass->GetOutermost() || TableClass->GetOutermost()->GetName() != TEXT("/Script/Engine") || TableClass->GetOutermost()->IsDirty()
        || !Loaded(Type) || Type->GetClass() != UScriptStruct::StaticClass() || Type->GetPathName() != TagRowPath || Type->GetOuter() != Type->GetOutermost() || !(Type->StructFlags & STRUCT_Native) || Type->GetOutermost()->GetName() != TEXT("/Script/GameplayTags") || !Type->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn) || Type->GetOutermost()->IsDirty() || Type->GetStructureSize() != sizeof(FGameplayTagTableRow) || !Type->GetSuperStruct() || Type->GetSuperStruct()->GetPathName() != TEXT("/Script/Engine.TableRowBase")) { return false; }
    J->SetStringField(TEXT("table_class_flags"), LexToString(static_cast<uint32>(TableClass->GetClassFlags()))); J->SetStringField(TEXT("row_struct_flags"), LexToString(static_cast<uint32>(Type->StructFlags))); J->SetNumberField(TEXT("row_struct_bytes"), Type->GetStructureSize());
    TArray<TSharedPtr<FJsonValue>> Fields; J->SetArrayField(TEXT("fields"), Fields); int32 Count = 0;
    for (TFieldIterator<FProperty> It(Type); It; ++It)
    {
        FProperty* P = *It; auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("name"), P->GetName()); Row->SetStringField(TEXT("authored_name"), P->GetAuthoredName()); Row->SetStringField(TEXT("cpp_type"), P->GetCPPType(nullptr, 0)); Row->SetStringField(TEXT("class"), P->GetClass()->GetName()); Row->SetStringField(TEXT("flags"), LexToString(static_cast<uint64>(P->GetPropertyFlags()))); Fields.Add(MakeShared<FJsonValueObject>(Row)); J->SetArrayField(TEXT("fields"), Fields);
        if (++Count > 2 || P->ArrayDim != 1 || P->GetOwnerStruct() != Type || !P->HasAnyPropertyFlags(CPF_Edit) || !((P->GetName() == TEXT("Tag") && CastField<FNameProperty>(P)) || (P->GetName() == TEXT("DevComment") && CastField<FStrProperty>(P)))) { return false; }
    } return Count == 2;
}
bool Disk(const FString& Package, TSharedRef<FJsonObject>& J)
{
    FString F; if (!FPackageName::TryConvertLongPackageNameToFilename(Package, F, FPackageName::GetAssetPackageExtension())) { return false; } F = FPaths::ConvertRelativePathToFull(F); const int64 Size = IFileManager::Get().FileSize(*F);
    J->SetStringField(TEXT("path"), F); J->SetBoolField(TEXT("exists"), Size >= 0); J->SetNumberField(TEXT("bytes"), FMath::Max<int64>(0, Size)); J->SetStringField(TEXT("sha1"), FString()); if (Size < 0) { return true; }
    TArray<uint8> Bytes; if (Size > MaxDiskBytes || !FFileHelper::LoadFileToArray(Bytes, *F) || Bytes.Num() != Size) { return false; } uint8 Hash[FSHA1::DigestSize]; FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Hash); J->SetStringField(TEXT("sha1"), BytesToHex(Hash, FSHA1::DigestSize)); return true;
}
bool Dirty(const FString& Package, TSharedRef<FJsonObject>& J)
{ int32 Count = 0; for (TObjectIterator<UPackage> It; It; ++It) { if (!Loaded(*It) || It->GetName() == Package) { continue; } if (++Count > MaxPackages) { return false; } J->SetBoolField(It->GetName(), It->IsDirty()); } return true; }
// New Mirror read preservation is literal: case can change FindReplace semantics.
// The pre-existing writer/base Equal contract remains untouched.
bool MirrorReadLiteralEqual(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B,int32 Depth=0)
{
    if(!A||!B||A->Type!=B->Type||Depth>32){return false;}
    if(A->Type==EJson::Object)
    {
        const auto X=A->AsObject(),Y=B->AsObject();if(!X.IsValid()||!Y.IsValid()||X->Values.Num()!=Y->Values.Num()){return false;}
        TArray<FString> XKeys,YKeys;for(const auto& P:X->Values){XKeys.Add(FString(P.Key.ToView()));}for(const auto& P:Y->Values){YKeys.Add(FString(P.Key.ToView()));}
        const auto LiteralOrder=[](const FString& L,const FString& Rhs){return L.Compare(Rhs,ESearchCase::CaseSensitive)<0;};XKeys.Sort(LiteralOrder);YKeys.Sort(LiteralOrder);
        for(int32 I=0;I<XKeys.Num();++I){if(!XKeys[I].Equals(YKeys[I],ESearchCase::CaseSensitive)||!MirrorReadLiteralEqual(X->TryGetField(XKeys[I]),Y->TryGetField(YKeys[I]),Depth+1)){return false;}}return true;
    }
    if(A->Type==EJson::Array){const auto& X=A->AsArray();const auto& Y=B->AsArray();if(X.Num()!=Y.Num()){return false;}for(int32 I=0;I<X.Num();++I){if(!MirrorReadLiteralEqual(X[I],Y[I],Depth+1)){return false;}}return true;}
    if(A->Type==EJson::String){return A->AsString().Equals(B->AsString(),ESearchCase::CaseSensitive);}
    if(A->Type==EJson::Number){return A->AsNumber()==B->AsNumber();}if(A->Type==EJson::Boolean){return A->AsBool()==B->AsBool();}return A->Type==EJson::Null;
}
bool MirrorReadLiteralEqual(const TSharedRef<FJsonObject>& A,const TSharedRef<FJsonObject>& B)
{return MirrorReadLiteralEqual(MakeShared<FJsonValueObject>(A),MakeShared<FJsonValueObject>(B));}
// IV: exact native specialized MirrorDataTable is admitted for the original three reads only.
// Its runtime derived caches are observed, never refreshed. No table delegate or Skeleton writer is called.
struct FMirrorReadScope
{
    FResult& R; bool Active=false,Stopped=false,Mutable=false,ValuesBound=false;
    EClassFlags SourceClassFlags=CLASS_None;FString SelectedPackagePath;int32 ExpectedMirrorAxis=0;
    TMap<FName,FMirrorTableRow> ExpectedValues;TArray<FName> SkeletonNames;TArray<FString> SkeletonNameText;
    TSharedRef<FJsonObject> SkeletonDisk=MakeShared<FJsonObject>();bool SkeletonDirty=false;
    bool ModifyFlagAdopted=false;uint32 ModifyFlagsBefore=0,ModifyFlagsAfter=0;
    TMap<FString,TPair<uint32,uint32>> SavedFlagTransitions;
    TWeakObjectPtr<UEditorEngine> Editor; TWeakObjectPtr<UWorld> World;
    TStrongObjectPtr<UMirrorDataTable> Table; TStrongObjectPtr<USkeleton> Skeleton;
    UClass* NativeClass=nullptr; UObject* NativeDefault=nullptr; UScriptStruct* RowType=nullptr;
    struct FIdentity { TStrongObjectPtr<UObject> Object; UClass* Class=nullptr; UObject* Outer=nullptr; UPackage* Package=nullptr; FString Path; EObjectFlags Flags=RF_NoFlags; };
    TArray<FIdentity> Identities; TArray<TPair<FName,const uint8*>> Rows; TArray<UObject*> Owned;
    explicit FMirrorReadScope(FResult& In,bool IsMutable=false):R(In),Mutable(IsMutable){}
    bool Stop(const TCHAR* Code,const TCHAR* Error)
    { if(!Stopped){Fail(R,Code,Error,true);}Stopped=true;if(Mutable){for(const TCHAR* K:{TEXT("observations_complete"),TEXT("source_preserved"),TEXT("unselected_state_preserved"),TEXT("independent_readback_passed"),TEXT("persistence_verified"),TEXT("postcondition_verified")})R.Data->SetBoolField(K,false);}return false; }
    bool Context()
    {
        if(Stopped) { return false; }
        if(!IsEditorGameThread()||!GEditor||GEditor!=Editor.Get()||!Loaded(Editor.Get())||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||IsPlaySessionActive()
            ||!Loaded(World.Get())||World->WorldType!=EWorldType::Editor||World->bIsTearingDown)
        { return Stop(TEXT("datatable_mirror_read_context_changed"),TEXT("Captured idle Editor/world must remain exact; no later observation or retry.")); }
        if(GEditor->GetEditorWorldContext().World()!=World.Get()) { return Stop(TEXT("datatable_mirror_read_world_changed"),TEXT("Current Editor world changed after original table load or a reader callback.")); }
        return true;
    }
    bool Start()
    {
        if(!IsEditorGameThread()||!Loaded(GEditor)||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||IsPlaySessionActive())
        { return Stop(TEXT("datatable_mirror_read_context_required"),TEXT("Idle Editor game thread required before the original table reader load.")); }
        Editor=GEditor;World=GEditor->GetEditorWorldContext().World();return Context();
    }
    bool Pin(UObject* O)
    {
        if(!O) { return true; } if(!Context()||!Loaded(O)||!Loaded(O->GetClass())||(O->GetOuter()&&!Loaded(O->GetOuter()))||!Loaded(O->GetPackage())||!CleanPath(O->GetPathName())||!CleanPath(O->GetClass()->GetPathName()))
        { return Stop(TEXT("datatable_mirror_read_identity_invalid"),TEXT("Loaded exact read identities and safe references required.")); }
        for(const auto& I:Identities) { if(I.Object.Get()==O) { return true; } }
        if(Identities.Num()>=MaxObjects) { return Stop(TEXT("datatable_mirror_read_identity_budget"),TEXT("Mirror reader strong identity budget exceeded.")); }
        FIdentity I;I.Object.Reset(O);I.Class=O->GetClass();I.Outer=O->GetOuter();I.Package=O->GetPackage();I.Path=O->GetPathName();I.Flags=O->GetFlags();Identities.Add(MoveTemp(I));return true;
    }
    bool Bind(UMirrorDataTable* Selected,UScriptStruct* Type)
    {
        if(!Context()||!Loaded(Selected)||Selected->GetClass()!=UMirrorDataTable::StaticClass()||Selected->GetOuter()!=Selected->GetPackage()||Selected->GetRowStruct()!=Type
            ||!Loaded(Type)||!Type->GetPathName().Equals(MirrorRowPath,ESearchCase::CaseSensitive)||Selected->GetRowMap().Num()>MaxRowsObserved)
        { return Stop(TEXT("datatable_mirror_read_scope_invalid"),TEXT("Exact native MirrorDataTable with the existing native MirrorTableRow required for reads only.")); }
        Table.Reset(Selected);NativeClass=Selected->GetClass();RowType=Type;SourceClassFlags=NativeClass->GetClassFlags();SelectedPackagePath=Selected->GetPackage()->GetPathName();ExpectedMirrorAxis=static_cast<int32>(Selected->MirrorAxis.GetValue());
        if(!Loaded(NativeClass)||!NativeClass->GetPathName().Equals(TEXT("/Script/Engine.MirrorDataTable"),ESearchCase::CaseSensitive)||NativeClass->GetSuperClass()!=UDataTable::StaticClass()
            ||!NativeClass->HasAnyClassFlags(CLASS_Native)||NativeClass->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)||NativeClass->ClassGeneratedBy
            ||NativeClass->GetOuter()!=NativeClass->GetPackage()||!NativeClass->GetPackage()->GetName().Equals(TEXT("/Script/Engine"),ESearchCase::CaseSensitive)||!NativeClass->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||NativeClass->GetPackage()->IsDirty())
        { return Stop(TEXT("datatable_mirror_read_native_class_invalid"),TEXT("Current compiled-in Engine MirrorDataTable class provenance required; no custom subclass support.")); }
        NativeDefault=NativeClass->GetDefaultObject(false);Skeleton.Reset(Selected->Skeleton.Get());
        if((NativeDefault&&(!Loaded(NativeDefault)||NativeDefault->GetClass()!=NativeClass||NativeDefault->GetOuter()!=NativeClass->GetPackage()||!NativeDefault->HasAnyFlags(RF_ClassDefaultObject)))
            ||(Skeleton.IsValid()&&(!Loaded(Skeleton.Get())||Skeleton->GetClass()!=USkeleton::StaticClass())))
        { return Stop(TEXT("datatable_mirror_read_source_invalid"),TEXT("Existing native default and optional exact native Skeleton roles must be readable; neither is created or modified.")); }
        for(UObject* O:{static_cast<UObject*>(Selected),static_cast<UObject*>(Selected->GetPackage()),static_cast<UObject*>(NativeClass),static_cast<UObject*>(NativeClass->GetPackage()),NativeDefault,static_cast<UObject*>(UDataTable::StaticClass()),static_cast<UObject*>(Type),static_cast<UObject*>(Type->GetSuperStruct()),static_cast<UObject*>(Skeleton.Get())})
        { if(!Pin(O)) { return false; } }
        for(TFieldIterator<FProperty> It(Type);It;++It) { if(auto* Byte=CastField<FByteProperty>(*It)) { if(!Pin(Byte->GetIntPropertyEnum())) { return false; } } }
        GetObjectsWithOuter(Selected,Owned,EGetObjectsFlags::IncludeNestedObjects);
        for(UObject* O:Owned) { if(!Loaded(O)||!O->IsIn(Selected)||!O->GetClass()->HasAnyClassFlags(CLASS_Native)||O->GetClass()->ClassGeneratedBy||!Pin(O)) { return Stop(TEXT("datatable_mirror_read_owned_scope_invalid"),TEXT("Current native owned table exports must remain exact during the read.")); } }
        for(const auto& Pair:Selected->GetRowMap()) { if(Pair.Key.IsNone()||!Pair.Value) { return Stop(TEXT("datatable_mirror_read_row_invalid"),TEXT("Complete current native row memory required before reading rows.")); } Rows.Emplace(Pair.Key,Pair.Value); }
        Active=true;if(Mutable){R.Data->SetBoolField(TEXT("specialized_mirror_read_only"),false);R.Data->SetBoolField(TEXT("specialized_mirror_row_mutation"),true);return PrepareMutable();}
        R.Data->SetBoolField(TEXT("specialized_mirror_read_only"),true);R.Data->SetStringField(TEXT("mirror_read_scope"),TEXT("Exact native MirrorDataTable original schema/list/get only. Authored fields, current rows, public derived caches, source identity/default, references, disk and dirty flags are observed. No refresh/row writer/Skeleton writer/save. Initial load/PostLoad precedes the preservation baseline; private engine caches and global callback purity are not inferred."));return Admit();
    }
    bool Admit(bool CheckRows=true)
    {
        if(!Context()) { return false; } if(!Active) { return true; }
        for(const auto& I:Identities)
        { UObject* O=I.Object.Get();if(!Loaded(O)||!Loaded(I.Class)||(I.Outer&&!Loaded(I.Outer))||!Loaded(I.Package)||O->GetClass()!=I.Class||O->GetOuter()!=I.Outer||O->GetPackage()!=I.Package||!O->GetPathName().Equals(I.Path,ESearchCase::CaseSensitive)||O->GetFlags()!=I.Flags)
          { return Stop(TEXT("datatable_mirror_read_identity_changed"),TEXT("Captured target/source/owned identity changed; no new reader SDK or stale row dereference.")); } }
        if(Table->GetClass()!=NativeClass||NativeClass->GetDefaultObject(false)!=NativeDefault||NativeClass->GetSuperClass()!=UDataTable::StaticClass()||Table->GetRowStruct()!=RowType||Table->Skeleton.Get()!=Skeleton.Get())
        { return Stop(TEXT("datatable_mirror_read_role_changed"),TEXT("Table/class/default/row type/Skeleton routing changed during the read.")); }
        if(Mutable&&(NativeClass->GetClassFlags()!=SourceClassFlags||(NativeDefault&&!NativeDefault->HasAnyFlags(RF_ClassDefaultObject))||static_cast<int32>(Table->MirrorAxis.GetValue())!=ExpectedMirrorAxis||!Table->OnDataTableChanged().IsBoundToObject(Table.Get())))
        {return Stop(TEXT("datatable_mirror_mutation_source_role_changed"),TEXT("Captured native class flags/default role changed; no next writer."));}
        if(CheckRows)
        {
            const auto& CurrentRows=Table->GetRowMap();if(CurrentRows.Num()!=Rows.Num()) { return Stop(TEXT("datatable_mirror_read_rows_changed"),TEXT("Selected current RowMap routing changed; no stale row memory read.")); }
            int32 Index=0;for(const auto& Pair:CurrentRows)
            {
                if(Rows[Index].Key!=Pair.Key||Rows[Index].Value!=Pair.Value) { return Stop(TEXT("datatable_mirror_read_rows_changed"),TEXT("Current ordered row key/storage routing changed.")); }
                if(Mutable&&ValuesBound)
                {
                    const auto* Expected=ExpectedValues.Find(Pair.Key);const auto* Current=reinterpret_cast<const FMirrorTableRow*>(Pair.Value);
                    if(!Expected||!SameRow(*Expected,*Current))return Stop(TEXT("datatable_mirror_mutation_values_changed"),TEXT("Captured current row values changed outside the explicit selected import; no next writer."));
                }
                ++Index;
            }
        }
        TArray<UObject*> CurrentOwned;GetObjectsWithOuter(Table.Get(),CurrentOwned,EGetObjectsFlags::IncludeNestedObjects);
        if(CurrentOwned.Num()!=Owned.Num()) { return Stop(TEXT("datatable_mirror_read_owned_changed"),TEXT("Owned table export set changed during the read.")); }
        for(UObject* O:Owned) { if(!CurrentOwned.Contains(O)) { return Stop(TEXT("datatable_mirror_read_owned_changed"),TEXT("Captured table export left the current owned set.")); } }return true;
    }
    static bool SameRow(const FMirrorTableRow& A,const FMirrorTableRow& B)
    {return A.Name==B.Name&&A.MirroredName==B.MirroredName&&A.MirrorEntryType==B.MirrorEntryType&&A.bEnabled==B.bEnabled;}
    bool PrepareMutable()
    {
        if(!Admit())return false;if(!Table->OnDataTableChanged().IsBoundToObject(Table.Get()))return Stop(TEXT("datatable_mirror_mutation_delegate_unbound"),TEXT("Current native table must retain its self-bound row-change delegate; no rebinding/refresh is performed."));auto Layout=MakeShared<FJsonObject>();if(!MirrorSources(RowType,Layout))return Stop(TEXT("datatable_mirror_mutation_layout"),TEXT("Exact native four-value row layout required before reading mutable row values."));
        for(const auto& Pair:Table->GetRowMap())ExpectedValues.Add(Pair.Key,*reinterpret_cast<const FMirrorTableRow*>(Pair.Value));
        ValuesBound=true;
        if(Skeleton.IsValid())
        {
            const auto& Ref=Skeleton->GetReferenceSkeleton();if(Ref.GetNum()>16384)return Stop(TEXT("datatable_mirror_mutation_skeleton_bound"),TEXT("Selected Skeleton bone-name/index input exceeds16384 bound."));
            for(int32 I=0;I<Ref.GetNum();++I){const FName Name=Ref.GetBoneName(I);if(Name.IsNone()||Ref.FindBoneIndex(Name)!=I)return Stop(TEXT("datatable_mirror_mutation_skeleton_routing"),TEXT("Current unique Skeleton name/index routing is incomplete."));SkeletonNames.Add(Name);SkeletonNameText.Add(Name.ToString());}
            SkeletonDirty=Skeleton->GetPackage()->IsDirty();if(!Disk(Skeleton->GetPackage()->GetName(),SkeletonDisk))return Stop(TEXT("datatable_mirror_mutation_skeleton_disk"),TEXT("Selected Skeleton input disk observation is incomplete."));
        }
        R.Data->SetStringField(TEXT("mirror_mutation_scope"),TEXT("Exact native MirrorDataTable row add/update/remove only. Planned selected row storage/value transitions, current source/Skeleton names/index/disk, public derived caches, settings, retained rows, references/disk/other dirty are verified. No protected refresh, Skeleton setter, autofill, private cache access or global delegate purity claim."));
        R.Data->SetNumberField(TEXT("mirror_skeleton_bone_input_count"),SkeletonNames.Num());return StableSource();
    }
    bool StableSource()
    {
        if(!Admit())return false;if(!Skeleton.IsValid())return true;
        const auto& Ref=Skeleton->GetReferenceSkeleton();if(Ref.GetNum()!=SkeletonNames.Num())return Stop(TEXT("datatable_mirror_mutation_skeleton_changed"),TEXT("Skeleton bone input count changed."));
        for(int32 I=0;I<SkeletonNames.Num();++I)if(Ref.GetBoneName(I)!=SkeletonNames[I]||!Ref.GetBoneName(I).ToString().Equals(SkeletonNameText[I],ESearchCase::CaseSensitive)||Ref.FindBoneIndex(SkeletonNames[I])!=I)return Stop(TEXT("datatable_mirror_mutation_skeleton_changed"),TEXT("Skeleton bone name/index input changed; no following writer."));
        auto Current=MakeShared<FJsonObject>();if(Skeleton->GetPackage()->IsDirty()!=SkeletonDirty||!Disk(Skeleton->GetPackage()->GetName(),Current)||!MirrorReadLiteralEqual(SkeletonDisk,Current))return Stop(TEXT("datatable_mirror_mutation_skeleton_changed"),TEXT("Selected Skeleton disk/dirty input changed; no following writer."));
        return Admit();
    }
    bool AcceptRowTransition(FName Selected,bool Remove,const uint8* Returned)
    {
        if(!Mutable||!Admit(false))return false;
        const auto& Current=Table->GetRowMap();const bool Existed=ExpectedValues.Contains(Selected);
        if((Remove&&!Existed)||(!Remove&&Existed)||Current.Num()!=Rows.Num()+(Remove?-1:1)||Current.Num()>MaxRowsObserved)return Stop(TEXT("datatable_mirror_mutation_row_transition"),TEXT("Only the planned selected add or stable removal count is admitted."));
        TArray<TPair<FName,const uint8*>> Next;int32 OldIndex=0;
        for(const auto& Pair:Current)
        {
            if(Remove)while(OldIndex<Rows.Num()&&Rows[OldIndex].Key==Selected)++OldIndex;
            if(!Remove&&Pair.Key==Selected)
            {
                if(Next.Num()!=Rows.Num()||!Returned||Pair.Value!=Returned||!SameRow(*reinterpret_cast<const FMirrorTableRow*>(Pair.Value),FMirrorTableRow()))return Stop(TEXT("datatable_mirror_mutation_row_transition"),TEXT("New row must be the exact returned fresh default storage appended once."));
            }
            else
            {
                const auto* V=ExpectedValues.Find(Pair.Key);
                if(OldIndex>=Rows.Num()||Rows[OldIndex].Key!=Pair.Key||Rows[OldIndex].Value!=Pair.Value||!V||!SameRow(*V,*reinterpret_cast<const FMirrorTableRow*>(Pair.Value)))return Stop(TEXT("datatable_mirror_mutation_retained_row_changed"),TEXT("Retained ordered row storage/values changed across helper; no stale dereference or next writer."));
                ++OldIndex;
            }
            Next.Emplace(Pair.Key,Pair.Value);
        }
        if(Remove){while(OldIndex<Rows.Num()&&Rows[OldIndex].Key==Selected)++OldIndex;if(Current.Contains(Selected)||OldIndex!=Rows.Num())return Stop(TEXT("datatable_mirror_mutation_row_transition"),TEXT("Selected row retirement or retained order is incomplete."));ExpectedValues.Remove(Selected);}
        else{auto Now=Current.Find(Selected);if(!Now||*Now!=Returned||OldIndex!=Rows.Num())return Stop(TEXT("datatable_mirror_mutation_row_transition"),TEXT("Fresh selected row routing is incomplete."));ExpectedValues.Add(Selected,*reinterpret_cast<const FMirrorTableRow*>(*Now));}
        Rows=MoveTemp(Next);return StableSource();
    }
    bool AcceptImportedRow(FName Selected,const uint8* Storage)
    {
        if(!Admit(false))return false;const auto& Current=Table->GetRowMap();if(Current.Num()!=Rows.Num())return Stop(TEXT("datatable_mirror_mutation_import_routing"),TEXT("Row map changed during the primitive import."));
        int32 I=0;for(const auto& P:Current)
        {
            if(Rows[I].Key!=P.Key||Rows[I].Value!=P.Value)return Stop(TEXT("datatable_mirror_mutation_import_routing"),TEXT("Import returned after row storage replacement; no stale read."));
            if(P.Key!=Selected){const auto* Expected=ExpectedValues.Find(P.Key);if(!Expected||!SameRow(*Expected,*reinterpret_cast<const FMirrorTableRow*>(P.Value)))return Stop(TEXT("datatable_mirror_mutation_import_retained"),TEXT("Unselected row changed during import."));}
            ++I;
        }
        auto Now=Current.Find(Selected);if(!Now||*Now!=Storage)return Stop(TEXT("datatable_mirror_mutation_import_routing"),TEXT("Selected import storage is no longer current."));
        ExpectedValues.Add(Selected,*reinterpret_cast<const FMirrorTableRow*>(*Now));return StableSource();
    }
    bool AfterModify(bool WasDirty,ITransaction* UndoBefore,bool ModifyReturned)
    {
        if(!Context())return false;FIdentity* TargetIdentity=nullptr;FIdentity* PackageIdentity=nullptr;
        for(auto& I:Identities)if(I.Object.Get()==Table.Get()){TargetIdentity=&I;break;}
        if(!TargetIdentity||!Loaded(Table.Get())||!Loaded(TargetIdentity->Class)||(TargetIdentity->Outer&&!Loaded(TargetIdentity->Outer))||!Loaded(TargetIdentity->Package)||Table->GetClass()!=TargetIdentity->Class||Table->GetOuter()!=TargetIdentity->Outer||Table->GetPackage()!=TargetIdentity->Package||!Table->GetPathName().Equals(TargetIdentity->Path,ESearchCase::CaseSensitive)||Table->GetFlags()!=TargetIdentity->Flags)return Admit();
        for(auto& I:Identities)if(I.Object.Get()==TargetIdentity->Package){PackageIdentity=&I;break;}
        if(!PackageIdentity)return Admit();auto* P=PackageIdentity->Object.Get();
        if(!Loaded(P)||!Loaded(PackageIdentity->Class)||(PackageIdentity->Outer&&!Loaded(PackageIdentity->Outer))||!Loaded(PackageIdentity->Package)||P->GetClass()!=PackageIdentity->Class||P->GetOuter()!=PackageIdentity->Outer||P->GetPackage()!=PackageIdentity->Package||!P->GetPathName().Equals(PackageIdentity->Path,ESearchCase::CaseSensitive))return Admit();
        const uint32 Before=static_cast<uint32>(PackageIdentity->Flags),After=static_cast<uint32>(P->GetFlags()),Bit=static_cast<uint32>(RF_Transactional);
        if(Before==After)return StableSource();
        auto* Package=static_cast<UPackage*>(P);
        if(WasDirty||!UndoBefore||GUndo!=UndoBefore||!ModifyReturned||!Package->IsDirty()||(Before&Bit)||After!=(Before|Bit)||Package->HasAnyFlags(RF_Transient)||Package->HasAnyPackageFlags(PKG_PlayInEditor|PKG_ContainsScript|PKG_CompiledIn)||!Table->HasAnyFlags(RF_Transactional))return Admit();
        const auto Old=PackageIdentity->Flags;PackageIdentity->Flags=P->GetFlags();
        if(!StableSource()){PackageIdentity->Flags=Old;return false;}
        ModifyFlagAdopted=true;ModifyFlagsBefore=Before;ModifyFlagsAfter=After;R.Data->SetBoolField(TEXT("mirror_modify_package_transactional_adopted"),true);return true;
    }
    bool AfterSave(bool SavedReturn)
    {
        if(!Context())return false;if(!SavedReturn)return Admit();TArray<TPair<int32,EObjectFlags>> Changed;
        for(int32 Index=0;Index<Identities.Num();++Index)
        {
            auto& I=Identities[Index];UObject* O=I.Object.Get();
            if(!Loaded(O)||!Loaded(I.Class)||(I.Outer&&!Loaded(I.Outer))||!Loaded(I.Package)||O->GetClass()!=I.Class||O->GetOuter()!=I.Outer||O->GetPackage()!=I.Package||!O->GetPathName().Equals(I.Path,ESearchCase::CaseSensitive))return Admit();
            if(O->GetFlags()==I.Flags)continue;
            const uint32 Before=static_cast<uint32>(I.Flags),After=static_cast<uint32>(O->GetFlags()),Added=After&~Before,Allowed=static_cast<uint32>(RF_WasLoaded|RF_LoadCompleted);
            if(O->GetPackage()!=Table->GetPackage()||(O!=Table.Get()&&!Owned.Contains(O))||O->HasAnyFlags(RF_Transient)||(Before&~After)||(Added&~Allowed))return Admit();
            Changed.Emplace(Index,I.Flags);
        }
        for(const auto& P:Changed)Identities[P.Key].Flags=Identities[P.Key].Object->GetFlags();
        if(!StableSource()){for(const auto& P:Changed)Identities[P.Key].Flags=P.Value;return false;}
        for(const auto& P:Changed){const auto& I=Identities[P.Key];SavedFlagTransitions.Add(I.Path,TPair<uint32,uint32>(static_cast<uint32>(P.Value),static_cast<uint32>(I.Flags)));}
        R.Data->SetNumberField(TEXT("mirror_saved_loaded_flag_adoption_count"),Changed.Num());return true;
    }
    bool PackageOwnedForSave()
    {
        if(!StableSource())return false;TArray<UObject*> Exports;
        GetObjectsWithPackage(Table->GetPackage(),Exports,EGetObjectsFlags::IncludeNestedObjects);if(!Admit())return false;
        if(Exports.Num()>MaxObjects)return Stop(TEXT("datatable_mirror_mutation_save_package_bound"),TEXT("Current package export budget exceeded; no save."));
        for(UObject* O:Exports)if(!Loaded(O)||O->GetPackage()!=Table->GetPackage()||(O!=Table.Get()&&!Owned.Contains(O)))
            return Stop(TEXT("datatable_mirror_mutation_foreign_package_export"),TEXT("Only the exact table/current native owned exports may share the selected saved package; no foreign export save."));
        return StableSource();
    }
    bool PublicCachesMatch()
    {
        if(!StableSource())return false;auto* T=Table.Get();TArray<int32> Bones;TSet<FName> Disabled;TMap<FName,FName> Curve,Notify,Sync,BoneMap;
        if(Skeleton.IsValid())
        {
            Bones.Init(INDEX_NONE,SkeletonNames.Num());
            for(const auto& P:Table->GetRowMap())
            {
                const auto& V=*reinterpret_cast<const FMirrorTableRow*>(P.Value);
                if(!V.bEnabled){Disabled.Add(V.MirroredName);continue;}
                switch(V.MirrorEntryType)
                {
                case EMirrorRowType::Bone:BoneMap.Add(V.Name,V.MirroredName);break;
                case EMirrorRowType::Curve:{const FName* Existing=Curve.Find(V.MirroredName);if(!Existing||*Existing!=V.Name)Curve.Add(V.Name,V.MirroredName);break;}
                case EMirrorRowType::AnimationNotify:Notify.Add(V.Name,V.MirroredName);break;
                case EMirrorRowType::SyncMarker:Sync.Add(V.Name,V.MirroredName);break;
                default:break;
                }
            }
            if(T->MirrorAxis!=EAxis::None)for(int32 I=0;I<Bones.Num();++I)if(Bones[I]==INDEX_NONE)
            {const FName* Name=BoneMap.Find(SkeletonNames[I]);const int32 Mirrored=Name?SkeletonNames.IndexOfByKey(*Name):INDEX_NONE;Bones[I]=Mirrored;if(Mirrored!=INDEX_NONE)Bones[Mirrored]=I;}
        }
        const auto SameMap=[](const TMap<FName,FName>& A,const TMap<FName,FName>& B){if(A.Num()!=B.Num())return false;for(const auto& P:A){const FName* V=B.Find(P.Key);if(!V||*V!=P.Value)return false;}return true;};
        bool OK=T->BoneToMirrorBoneIndex.Num()==Bones.Num()&&T->DisabledEntries.Num()==Disabled.Num()&&SameMap(T->CurveToMirrorCurveMap,Curve)&&SameMap(T->AnimNotifyToMirrorAnimNotifyMap,Notify)&&SameMap(T->SyncToMirrorSyncMap,Sync);
        if(OK){for(int32 I=0;I<Bones.Num();++I)OK&=static_cast<int32>(T->BoneToMirrorBoneIndex[I])==Bones[I];for(FName N:Disabled)OK&=T->DisabledEntries.Contains(N);}
        R.Data->SetBoolField(TEXT("mirror_public_cache_verified"),OK);
        return OK||Stop(TEXT("datatable_mirror_mutation_cache_mismatch"),TEXT("Public derived caches differ from the installed FillMirrorArrays/FillMirrorBoneIndexes current-row/Skeleton result; no protected refresh, repair or next writer."));
    }
    bool Snapshot(TSharedRef<FJsonObject>& State,TSharedRef<FJsonObject>& Source)
    {
        if(!Admit()) { return false; } auto* T=Table.Get();
        if(T->BoneToMirrorBoneIndex.Num()>16384||T->DisabledEntries.Num()>MaxRowsObserved||T->CurveToMirrorCurveMap.Num()>MaxRowsObserved||T->AnimNotifyToMirrorAnimNotifyMap.Num()>MaxRowsObserved||T->SyncToMirrorSyncMap.Num()>MaxRowsObserved)
        { return Stop(TEXT("datatable_mirror_read_cache_budget"),TEXT("Bounded public derived Mirror caches required; none is rebuilt by the reader.")); }
        auto Cache=MakeShared<FJsonObject>();State->SetObjectField(TEXT("mirror_public_derived_cache"),Cache);TArray<TSharedPtr<FJsonValue>> Bones,Disabled;
        for(const auto Bone:T->BoneToMirrorBoneIndex) { Bones.Add(MakeShared<FJsonValueNumber>(static_cast<int32>(Bone))); }for(FName Name:T->DisabledEntries) { Disabled.Add(MakeShared<FJsonValueString>(Name.ToString())); }
        Cache->SetArrayField(TEXT("bone_to_mirror_bone_index"),Bones);Cache->SetArrayField(TEXT("disabled_entries"),Disabled);
        const auto Map=[&](const auto& Values,const TCHAR* Key){TArray<TSharedPtr<FJsonValue>> Items;for(const auto& Pair:Values){auto Item=MakeShared<FJsonObject>();Item->SetStringField(TEXT("name"),Pair.Key.ToString());Item->SetStringField(TEXT("mirror"),Pair.Value.ToString());Items.Add(MakeShared<FJsonValueObject>(Item));}Cache->SetArrayField(Key,Items);};
        Map(T->CurveToMirrorCurveMap,TEXT("curves"));Map(T->AnimNotifyToMirrorAnimNotifyMap,TEXT("notifies"));Map(T->SyncToMirrorSyncMap,TEXT("sync_markers"));
        auto Native=MakeShared<FJsonObject>();Source->SetObjectField(TEXT("specialized_table_class"),Native);Native->SetObjectField(TEXT("identity"),Identity(NativeClass));Native->SetStringField(TEXT("class_flags"),LexToString(static_cast<uint32>(NativeClass->GetClassFlags())));Native->SetStringField(TEXT("super_class"),NativeClass->GetSuperClass()->GetPathName());Native->SetBoolField(TEXT("package_dirty"),NativeClass->GetPackage()->IsDirty());Native->SetObjectField(TEXT("default_identity"),Identity(NativeDefault));
        if(NativeDefault){auto DefaultFields=MakeShared<FJsonObject>();Native->SetObjectField(TEXT("default_fields"),DefaultFields);if(!Properties(NativeDefault,DefaultFields,this)){return false;}}
        return Admit();
    }
};
bool MirrorReadAdmit(FMirrorReadScope* Scope) { return !Scope||Scope->Admit(); }
struct FContext { TStrongObjectPtr<UDataTable> Table; UScriptStruct* Type = nullptr; FString Path, Package; TUniquePtr<FMirrorReadScope> MirrorRead; TUniquePtr<FStringUdsScope> Uds; TUniquePtr<FNativeRowScope> Native; };
struct FObservation
{
    bool Complete = false;
    TSharedRef<FJsonObject> State = MakeShared<FJsonObject>(), Source = MakeShared<FJsonObject>(), References = MakeShared<FJsonObject>(), DiskState = MakeShared<FJsonObject>(), OtherDirty = MakeShared<FJsonObject>();
};
TSharedRef<FJsonObject> Observation(const FObservation& O)
{ auto J = MakeShared<FJsonObject>(); J->SetBoolField(TEXT("complete"), O.Complete); J->SetObjectField(TEXT("state"), O.State); J->SetObjectField(TEXT("native_sources"), O.Source); J->SetObjectField(TEXT("references"), O.References); J->SetObjectField(TEXT("disk"), O.DiskState); J->SetObjectField(TEXT("other_package_dirty"), O.OtherDirty); return J; }
// KP: one authored FString only. These guards belong to DataTable operations;
// they do not create/recompile/source-save a UDS or normalize JSON field names.
bool StringUdsWriterRunning=false;
// Reuse the established exact ModifyFlags comparison for non-source references.
template<typename TScope>
bool StringUdsEqual(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B,const TScope& S,bool Saved=false,const FString& At=FString(),bool ModifyProperties=false);
struct FStringUdsScope
{
    FResult& R; bool Active=false,Stopped=false,Bound=false,TrackUndo=false;
    ITransaction* ExpectedUndo=nullptr;
    TStrongObjectPtr<UEditorEngine> Editor; TStrongObjectPtr<UWorld> World;
    TStrongObjectPtr<UUserDefinedStruct> Type; TStrongObjectPtr<UDataTable> Table;
    UObject* EditorData=nullptr; FStrProperty* Property=nullptr; FGuid StructGuid,FieldGuid;
    FString InternalName,DefaultValue,TypePath,TablePath,SourcePackage,SelectedPackage;
    EStructFlags StructFlags=STRUCT_NoFlags; int32 StructBytes=0,Offset=0; uint64 PropertyFlags=0;
    const uint8* DefaultMemory=nullptr; uint32 SourcePackageFlags=0;
    TSharedRef<FJsonObject> SourceBaseline=MakeShared<FJsonObject>();
    TArray<TPair<FName,const uint8*>> Rows; TMap<FName,FString> Values; TArray<UObject*> Owned,SourceOwned;
    struct FIdentity { TStrongObjectPtr<UObject> Object; UClass* Class=nullptr; UObject* Outer=nullptr; UPackage* Package=nullptr; FString Path; EObjectFlags Flags=RF_NoFlags; EClassFlags ClassFlags=CLASS_None; int32 ClassBytes=0; };
    TArray<FIdentity> Held;
    TMap<FString,TPair<uint32,uint32>> ModifyFlags,SaveFlags;
    TMap<FString,TPair<FString,FString>> ModifyDerivedMetadata;
    bool TrackModifiedMetadata=false;FName ModifiedRowStructName;FTopLevelAssetPath ModifiedRowStructPath;
    struct FScalarField
    {
        FProperty* Property=nullptr;FFieldClass* Kind=nullptr;FName FieldName;FGuid Guid;
        FString Name,Authored,Layout;int32 Offset=0,Size=0;EPropertyFlags Flags=CPF_None;UScriptStruct* NativeStruct=nullptr;FString NativeStructPath;
        bool Payload=false;FProperty* Inner=nullptr;FFieldClass* InnerKind=nullptr;FName InnerName;int32 InnerSize=0,InnerOffset=0;EPropertyFlags InnerFlags=CPF_None;EArrayPropertyFlags ArrayFlags=EArrayPropertyFlags::None;
    };
    bool AllowScalarCRUD=false,AllowScalarGraph=false,ScalarProfile=false,AllowPrimitiveArrays=false,ArrayProfile=false;TArray<FScalarField> ScalarFields;
    bool NativeMathProfile=false,NativeMathReady=false;
    struct FMathField { FProperty* Address=nullptr;FFieldClass* Kind=nullptr;FName Name;int32 Offset=0,Size=0,Dim=0;uint64 Flags=0;UScriptStruct* Binding=nullptr;FString BindingPath; };
    struct FMathRole { TStrongObjectPtr<UScriptStruct> Type;FString Path;UStruct* Super=nullptr;int32 Bytes=0,Alignment=0;EStructFlags Flags=STRUCT_NoFlags;uint32 PackageFlags=0;bool Dirty=false;TArray<FMathField> Fields; };
    TArray<FMathRole> NativeMathRoles;TArray<TStrongObjectPtr<UScriptStruct>> NativeFieldHolds;
    static bool MathKind(const FProperty* P)
    {
        const auto* Struct=CastField<FStructProperty>(P);if(!Struct||!Struct->Struct.IsResolved())return false;
        UScriptStruct* Returned=Struct->Struct.Get();TStrongObjectPtr<UScriptStruct> Hold(Returned);if(!Loaded(Returned))return false;
        const FString Path=Returned->GetPathName();return Path==TEXT("/Script/CoreUObject.Vector")||Path==TEXT("/Script/CoreUObject.Rotator")||Path==TEXT("/Script/CoreUObject.Transform");
    }
    bool NativeMathCurrent()
    {
        if(!NativeMathReady)return true;
        for(const auto& Role:NativeMathRoles)
        {
            UScriptStruct* Native=Role.Type.Get();
            if(!Loaded(Native)||Native->GetClass()!=UScriptStruct::StaticClass()||!Native->GetPathName().Equals(Role.Path,ESearchCase::CaseSensitive)||Native->GetSuperStruct()!=Role.Super||Native->GetStructureSize()!=Role.Bytes||Native->GetMinAlignment()!=Role.Alignment||Native->StructFlags!=Role.Flags||Native->GetPackage()->IsDirty()!=Role.Dirty||static_cast<uint32>(Native->GetPackage()->GetPackageFlags())!=Role.PackageFlags)return Stop(TEXT("datatable_math_uds_native_source_changed"),TEXT("Exact held native math struct source/layout changed."));
            TArray<FProperty*> Current;for(TFieldIterator<FProperty> It(Native,EFieldIteratorFlags::ExcludeSuper);It;++It)Current.Add(*It);
            if(Current.Num()!=Role.Fields.Num())return Stop(TEXT("datatable_math_uds_native_fields_changed"),TEXT("Native direct-field inventory changed."));
            for(int32 I=0;I<Current.Num();++I)
            {
                const auto& Field=Role.Fields[I];FProperty* Actual=Current[I];
                if(Actual!=Field.Address)return Stop(TEXT("datatable_math_uds_native_fields_changed"),TEXT("Native field membership/order changed; no stale field read."));
                if(Actual->GetOwnerStruct()!=Native||Actual->GetClass()!=Field.Kind||Actual->GetFName()!=Field.Name||Actual->GetOffset_ForInternal()!=Field.Offset||Actual->GetSize()!=Field.Size||Actual->ArrayDim!=Field.Dim||static_cast<uint64>(Actual->GetPropertyFlags())!=Field.Flags)return Stop(TEXT("datatable_math_uds_native_fields_changed"),TEXT("Native direct-field ownership/layout changed."));
                if(Field.Binding){const auto* Struct=CastField<FStructProperty>(Actual);if(!Struct||!Struct->Struct.IsResolved()||Struct->Struct.Get()!=Field.Binding)return Stop(TEXT("datatable_math_uds_native_binding_changed"),TEXT("Transform direct field binding changed."));}
            }
        }
        return true;
    }
    bool BindNativeMath()
    {
        if(!NativeMathProfile)return true;
        bool NeedVector=false,NeedRotator=false,NeedTransform=false;for(const auto& Field:ScalarFields){NeedVector|=Field.NativeStructPath==TEXT("/Script/CoreUObject.Vector");NeedRotator|=Field.NativeStructPath==TEXT("/Script/CoreUObject.Rotator");NeedTransform|=Field.NativeStructPath==TEXT("/Script/CoreUObject.Transform");}
        TArray<TPair<FString,UScriptStruct*>> Roots;
        const auto Acquire=[&](UScriptStruct* Returned,const TCHAR* Path)->bool
        {
            TStrongObjectPtr<UScriptStruct> Hold(Returned);if(!Raw(false))return false;
            if(!Loaded(Returned)||Returned->GetClass()!=UScriptStruct::StaticClass()||!Returned->GetPathName().Equals(Path,ESearchCase::CaseSensitive)||!(Returned->StructFlags&STRUCT_Native)||Returned->GetPackage()->GetName()!=TEXT("/Script/CoreUObject")||Returned->GetPackage()->IsDirty()||!Returned->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||!Pin(Returned)||!Pin(Returned->GetClass())||!Pin(Returned->GetPackage()))return Stop(TEXT("datatable_math_uds_native_source_unsupported"),TEXT("Already-loaded exact CoreUObject native math structs required; no loader/CDO/value query."));
            Roots.Emplace(Path,Returned);return true;
        };
        if((NeedVector||NeedTransform)&&!Acquire(TBaseStructure<FVector>::Get(),TEXT("/Script/CoreUObject.Vector")))return false;
        if(NeedRotator&&!Acquire(TBaseStructure<FRotator>::Get(),TEXT("/Script/CoreUObject.Rotator")))return false;
        if(NeedTransform&&(!Acquire(TBaseStructure<FQuat>::Get(),TEXT("/Script/CoreUObject.Quat"))||!Acquire(TBaseStructure<FTransform>::Get(),TEXT("/Script/CoreUObject.Transform"))))return false;
        for(const auto& Root:Roots)
        {
            FMathRole Role;Role.Type.Reset(Root.Value);Role.Path=Root.Key;Role.Super=Root.Value->GetSuperStruct();Role.Bytes=Root.Value->GetStructureSize();Role.Alignment=Root.Value->GetMinAlignment();Role.Flags=Root.Value->StructFlags;Role.PackageFlags=static_cast<uint32>(Root.Value->GetPackage()->GetPackageFlags());Role.Dirty=Root.Value->GetPackage()->IsDirty();
            const bool Transform=Root.Key==TEXT("/Script/CoreUObject.Transform"),Rotator=Root.Key==TEXT("/Script/CoreUObject.Rotator"),Quat=Root.Key==TEXT("/Script/CoreUObject.Quat");TSet<FName> Names;
            for(TFieldIterator<FProperty> It(Root.Value,EFieldIteratorFlags::ExcludeSuper);It;++It)
            {
                FProperty* P=*It;const FName Name=P->GetFName();UScriptStruct* Binding=nullptr;FString BindingPath;
                const bool NameAllowed=Transform?(Name==TEXT("Rotation")||Name==TEXT("Translation")||Name==TEXT("Scale3D")):Rotator?(Name==TEXT("Pitch")||Name==TEXT("Yaw")||Name==TEXT("Roll")):(Name==TEXT("X")||Name==TEXT("Y")||Name==TEXT("Z")||(Quat&&Name==TEXT("W")));
                if(!NameAllowed||Names.Contains(Name)||P->GetOwnerStruct()!=Root.Value||P->ArrayDim!=1||P->HasSetterOrGetter()||P->GetOffset_ForInternal()<0||P->GetSize()<=0||static_cast<int64>(P->GetOffset_ForInternal())+P->GetSize()>Role.Bytes)return Stop(TEXT("datatable_math_uds_native_layout_unsupported"),TEXT("Exact native direct-field names/kinds/owner/current layout required; no fixed private offsets."));
                if(Transform)
                {
                    auto* Struct=CastField<FStructProperty>(P);BindingPath=Name==TEXT("Rotation")?TEXT("/Script/CoreUObject.Quat"):TEXT("/Script/CoreUObject.Vector");
                    for(const auto& Candidate:Roots)if(Candidate.Key.Equals(BindingPath,ESearchCase::CaseSensitive))Binding=Candidate.Value;
                    if(!Struct||!Struct->Struct.IsResolved()||Struct->Struct.Get()!=Binding||!Binding)return Stop(TEXT("datatable_math_uds_native_binding_unsupported"),TEXT("Transform Rotation->Quat and Translation/Scale3D->Vector exact held binding required."));
                }
                else if(P->GetClass()!=FDoubleProperty::StaticClass())return Stop(TEXT("datatable_math_uds_native_layout_unsupported"),TEXT("Native Vector/Rotator/Quat must expose actual double components."));
                FMathField Field;Field.Address=P;Field.Kind=P->GetClass();Field.Name=Name;Field.Offset=P->GetOffset_ForInternal();Field.Size=P->GetSize();Field.Dim=P->ArrayDim;Field.Flags=static_cast<uint64>(P->GetPropertyFlags());Field.Binding=Binding;Field.BindingPath=BindingPath;Role.Fields.Add(MoveTemp(Field));Names.Add(Name);
            }
            if(Role.Fields.Num()!=(Quat?4:3))return Stop(TEXT("datatable_math_uds_native_layout_unsupported"),TEXT("Exact reflected native math field inventory required."));NativeMathRoles.Add(MoveTemp(Role));
        }
        for(const auto& Field:ScalarFields)if(Field.NativeStruct)
        {
            bool Exact=false;for(const auto& Role:NativeMathRoles)if(Role.Path.Equals(Field.NativeStructPath,ESearchCase::CaseSensitive)&&Role.Type.Get()==Field.NativeStruct)Exact=true;
            if(!Exact)return Stop(TEXT("datatable_math_uds_native_binding_unsupported"),TEXT("UDS native field must bind to the same held TBaseStructure identity, not merely matching path."));
        }
        NativeMathReady=true;return Raw(false);
    }
    TSharedRef<FJsonObject> NativeMathState()const
    {
        auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("observations_complete"),true);J->SetBoolField(TEXT("metadata_observed"),false);J->SetBoolField(TEXT("native_default_values_sampled"),false);J->SetBoolField(TEXT("cdo_creation_requested"),false);TArray<TSharedPtr<FJsonValue>> Roles;
        for(const auto& Role:NativeMathRoles)
        {
            auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("path"),Role.Path);V->SetStringField(TEXT("package_path"),TEXT("/Script/CoreUObject"));V->SetStringField(TEXT("flags"),LexToString(static_cast<uint32>(Role.Flags)));V->SetNumberField(TEXT("size"),Role.Bytes);V->SetNumberField(TEXT("alignment"),Role.Alignment);V->SetStringField(TEXT("package_flags"),LexToString(Role.PackageFlags));V->SetBoolField(TEXT("package_dirty"),Role.Dirty);TArray<TSharedPtr<FJsonValue>> Fields;
            for(const auto& Field:Role.Fields){auto F=MakeShared<FJsonObject>();F->SetStringField(TEXT("name"),Field.Name.ToString());F->SetStringField(TEXT("class"),Field.Kind->GetName());F->SetStringField(TEXT("owner_path"),Role.Path);F->SetNumberField(TEXT("offset"),Field.Offset);F->SetNumberField(TEXT("size"),Field.Size);F->SetNumberField(TEXT("array_dim"),Field.Dim);F->SetStringField(TEXT("flags"),LexToString(Field.Flags));F->SetStringField(TEXT("native_struct_path"),Field.BindingPath);Fields.Add(MakeShared<FJsonValueObject>(F));}V->SetArrayField(TEXT("fields"),Fields);Roles.Add(MakeShared<FJsonValueObject>(V));
        }
        J->SetArrayField(TEXT("structs"),Roles);return J;
    }

    explicit FStringUdsScope(FResult& In,bool InScalarCRUD=false,bool InScalarGraph=false,bool InPrimitiveArrays=false):R(In),AllowScalarCRUD(InScalarCRUD),AllowScalarGraph(InScalarGraph),AllowPrimitiveArrays(InPrimitiveArrays){}
    bool Stop(const TCHAR* Code,const TCHAR* Error)
    {
        if(!Stopped)Fail(R,Code,Error,R.Attempted||Active||Bound||NativeMathProfile);Stopped=true;
        for(const TCHAR* K:{TEXT("observations_complete"),TEXT("source_preserved"),TEXT("unselected_state_preserved"),TEXT("independent_readback_passed"),TEXT("persistence_verified"),TEXT("postcondition_verified")})R.Data->SetBoolField(K,false);
        return false;
    }
    bool Context()
    {
        if(Stopped)return false;
        if(!IsEditorGameThread()||!Loaded(Editor.Get())||GEditor!=Editor.Get()||!Loaded(World.Get())||World->WorldType!=EWorldType::Editor||World->bIsTearingDown
            ||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||IsPlaySessionActive()||GIsImportingT3D
            ||GEditor->GetEditorWorldContext().World()!=World.Get()||(TrackUndo&&GUndo!=ExpectedUndo))
            return Stop(TEXT("datatable_string_uds_context_changed"),TEXT("Captured idle Editor/world/transaction changed; no next SDK or stale row observation."));
        return true;
    }
    bool Start()
    {
        if(!IsEditorGameThread()||!Loaded(GEditor)||GIsImportingT3D)return Stop(TEXT("datatable_string_uds_context_required"),TEXT("Idle Editor game thread without T3D import required."));
        Editor.Reset(GEditor);World.Reset(GEditor->GetEditorWorldContext().World());return Context();
    }
    bool Pin(UObject* O)
    {
        if(!O)return true;if(!Context()||!Loaded(O)||!Loaded(O->GetClass())||(O->GetOuter()&&!Loaded(O->GetOuter()))||!Loaded(O->GetPackage())||!CleanPath(O->GetPathName())||!CleanPath(O->GetClass()->GetPathName()))return Stop(TEXT("datatable_string_uds_identity_invalid"),TEXT("Loaded finite source/target identities required."));
        for(const auto& I:Held)if(I.Object.Get()==O)return true;if(Held.Num()>=MaxObjects)return Stop(TEXT("datatable_string_uds_identity_bound"),TEXT("Owned source/target identity bound exceeded."));
        FIdentity I;I.Object.Reset(O);I.Class=O->GetClass();I.Outer=O->GetOuter();I.Package=O->GetPackage();I.Path=O->GetPathName();I.Flags=O->GetFlags();I.ClassFlags=I.Class->GetClassFlags();I.ClassBytes=I.Class->GetPropertiesSize();Held.Add(MoveTemp(I));return true;
    }
    bool IdentityChanged(const FIdentity& I,const TCHAR* PropertyName,const FString& ExpectedValue,const FString& ActualValue)
    {
        // The first mismatch is already observed. Record only copied values;
        // do not query the failed object or any later identity for diagnostics.
        if(!R.Data->HasField(TEXT("string_uds_identity_first_mismatch")))
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("expected_object_path"),I.Path);
            J->SetStringField(TEXT("expected_object_flags"),LexToString(static_cast<uint32>(I.Flags)));J->SetStringField(TEXT("property"),PropertyName);
            J->SetStringField(TEXT("expected"),ExpectedValue);J->SetStringField(TEXT("actual"),ActualValue);
            J->SetStringField(TEXT("scope"),TEXT("First current held-identity mismatch only; no SDK or failed-object dereference is added after this observation."));
            R.Data->SetObjectField(TEXT("string_uds_identity_first_mismatch"),J);
        }
        return Stop(TEXT("datatable_string_uds_identity_changed"),TEXT("Captured source/target identity changed; first expected/actual mismatch retained, no later SDK."));
    }
    // NQ scalar CRUD remains explicit. NR separately opts graph-only source
    // observation into the same bounded scalar guards; String1 fallback stays intact.
    bool ScalarMismatch(const FString& Object,const FString& Field,const FString& Expected,const FString& Actual)
    {
        if(!R.Data->HasField(TEXT("scalar_uds_first_mismatch")))
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("object"),Object);
            J->SetStringField(TEXT("property"),Field);J->SetStringField(TEXT("expected"),Expected.Left(4096));J->SetStringField(TEXT("actual"),Actual.Left(4096));
            J->SetBoolField(TEXT("copied_observed_values_only"),true);R.Data->SetObjectField(TEXT("scalar_uds_first_mismatch"),J);
        }
        return Stop(TEXT("datatable_scalar_uds_current_changed"),TEXT("Current scalar source/layout/value differs; actual prefix is retained and no later SDK is dispatched."));
    }
    static bool ScalarKind(const FProperty* P)
    {
        const auto* K=P?P->GetClass():nullptr;
        return K==FStrProperty::StaticClass()||K==FIntProperty::StaticClass()||K==FInt64Property::StaticClass()
            ||K==FFloatProperty::StaticClass()||K==FDoubleProperty::StaticClass()||K==FBoolProperty::StaticClass()||K==FNameProperty::StaticClass();
    }
    bool ScalarRowMismatch(const FString& Row,const FString& Expected,const FString& Actual)
    {
        TArray<TSharedPtr<FJsonValue>> A,B;
        if(FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Expected),A)&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Actual),B)&&A.Num()==B.Num()&&A.Num()==ScalarFields.Num())
        {
            for(int32 I=0;I<A.Num();++I)if(!FJsonValue::CompareEqual(*A[I],*B[I]))
            {
                FString X,Y;FJsonSerializer::Serialize(TArray<TSharedPtr<FJsonValue>>{A[I]},TJsonWriterFactory<>::Create(&X));FJsonSerializer::Serialize(TArray<TSharedPtr<FJsonValue>>{B[I]},TJsonWriterFactory<>::Create(&Y));
                return ScalarMismatch(TablePath.IsEmpty()?TypePath:TablePath,Row+TEXT(".")+ScalarFields[I].Name,X,Y);
            }
        }
        return ScalarMismatch(TablePath.IsEmpty()?TypePath:TablePath,Row,Expected,Actual);
    }
    // TR: reuse RI/RM concrete reflected payload walk in the existing DataTable source/row scope.
    struct FValue{int64 Integer=0;uint64 Bits=0;bool Bit=false;FString String;FName Name;int32 Shape=0;TArray<FValue> Children;};
    static FValue Read(FProperty* P,const void* Address)
    {
        FValue V;if(auto* S=CastField<FStrProperty>(P))V.String=S->GetPropertyValue(Address);else if(auto* N=CastField<FNameProperty>(P)){V.Name=N->GetPropertyValue(Address);V.String=V.Name.ToString();}else if(auto* B=CastField<FBoolProperty>(P))V.Bit=B->GetPropertyValue(Address);else if(P->GetClass()==FFloatProperty::StaticClass())FMemory::Memcpy(&V.Bits,Address,sizeof(float));else if(P->GetClass()==FDoubleProperty::StaticClass())FMemory::Memcpy(&V.Bits,Address,sizeof(double));else V.Integer=CastFieldChecked<FNumericProperty>(P)->GetSignedIntPropertyValue(Address);return V;
    }
    static bool Same(const FValue& A,const FValue& B)
    {if(A.Shape!=B.Shape||A.Children.Num()!=B.Children.Num()||A.Integer!=B.Integer||A.Bits!=B.Bits||A.Bit!=B.Bit||A.Name!=B.Name||!A.String.Equals(B.String,ESearchCase::CaseSensitive))return false;for(int32 I=0;I<A.Children.Num();++I)if(!Same(A.Children[I],B.Children[I]))return false;return true;}
    static FString ValueText(const FValue& V)
    {FString S=TEXT("shape=")+LexToString(V.Shape)+TEXT(";int=")+LexToString(V.Integer)+TEXT(";bits=")+LexToString(V.Bits)+TEXT(";bool=")+LexToString(V.Bit)+TEXT(";string=")+V.String+TEXT(";name=")+V.Name.ToString();for(int32 I=0;I<V.Children.Num()&&S.Len()<131072;++I)S+=TEXT(";[")+LexToString(I)+TEXT("]{")+ValueText(V.Children[I])+TEXT("}");return S;}
    // RM: canonicalize copied slots only. Never sort, deduplicate or rehash authored storage here.
    static bool ValueLess(const FValue& A,const FValue& B)
    {
        if(A.Shape!=B.Shape)return A.Shape<B.Shape;if(A.Integer!=B.Integer)return A.Integer<B.Integer;if(A.Bits!=B.Bits)return A.Bits<B.Bits;if(A.Bit!=B.Bit)return A.Bit<B.Bit;
        const int32 TextOrder=A.String.Compare(B.String,ESearchCase::CaseSensitive);if(TextOrder)return TextOrder<0;const int32 NameOrder=A.Name.Compare(B.Name);if(NameOrder)return NameOrder<0;
        for(int32 I=0;I<FMath::Min(A.Children.Num(),B.Children.Num());++I){if(ValueLess(A.Children[I],B.Children[I]))return true;if(ValueLess(B.Children[I],A.Children[I]))return false;}return A.Children.Num()<B.Children.Num();
    }
    static bool HasHashValue(const FValue& V)
    {if(V.Shape==3||V.Shape==4)return true;for(const auto& C:V.Children)if(HasHashValue(C))return true;return false;}
    static bool ByteEnumPayloadKind(const FProperty* P)
    {return P&&P->GetClass()==FByteProperty::StaticClass();}
    static bool HashScalarKind(FProperty* P)
    {return (ScalarKind(P)||ByteEnumPayloadKind(P))&&P->HasAllPropertyFlags(CPF_HasGetValueTypeHash);}
    static bool SetLayoutSame(const FScriptSetLayout& A,const FScriptSetLayout& B)
    {
#if UE_USE_COMPACT_SET_AS_DEFAULT
        return A.Size==B.Size&&A.Alignment==B.Alignment;
#else
        return A.Size==B.Size&&A.HashNextIdOffset==B.HashNextIdOffset&&A.HashIndexOffset==B.HashIndexOffset&&A.SparseArrayLayout.Size==B.SparseArrayLayout.Size&&A.SparseArrayLayout.Alignment==B.SparseArrayLayout.Alignment;
#endif
    }
    static bool MapLayoutSame(const FScriptMapLayout& A,const FScriptMapLayout& B)
    {return A.ValueOffset==B.ValueOffset&&SetLayoutSame(A.SetLayout,B.SetLayout);}
    bool PayloadReject(const TCHAR* Code,const FString& Why){return Stop(Code,*Why);}
    bool PayloadFailure(const TCHAR* Field,const FString& Expected,const FString& Actual,bool Unknown=true,const FString& Object=FString())
    {return ScalarMismatch(Object.IsEmpty()?TypePath:Object,Field,Expected,Actual);}
    static bool ReferencePayloadKind(const FProperty* P)
    {return P&&(P->GetClass()==FObjectProperty::StaticClass()||P->GetClass()==FSoftObjectProperty::StaticClass());}
    static bool PayloadKind(FProperty* P)
    {return P&&(P->GetClass()==FArrayProperty::StaticClass()||P->GetClass()==FSetProperty::StaticClass()||P->GetClass()==FMapProperty::StaticClass()||P->GetClass()==FStructProperty::StaticClass()||ByteEnumPayloadKind(P)||ReferencePayloadKind(P));}
    static TSharedRef<FJsonValue> PayloadJson(const FValue& V)
    {
        TArray<TSharedPtr<FJsonValue>> Children;for(const auto& C:V.Children)Children.Add(PayloadJson(C));
        auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("shape"),V.Shape);J->SetStringField(TEXT("integer"),LexToString(V.Integer));J->SetStringField(TEXT("bits"),LexToString(V.Bits));J->SetBoolField(TEXT("bool"),V.Bit);J->SetStringField(TEXT("string"),V.String);J->SetStringField(TEXT("name"),V.Name.ToString());J->SetArrayField(TEXT("children"),Children);return MakeShared<FJsonValueObject>(J);
    }
    // RI extends this concrete generated asset lease; no second converter or writer is introduced.
    struct FPayloadField
    {
        FProperty* P=nullptr;FProperty* Parent=nullptr;UStruct* Owner=nullptr;FFieldClass* Kind=nullptr;
        FName Name;FGuid Guid;FString Authored;uint64 Flags=0;int32 Size=0,Offset=0,Dim=0,Align=0;uint8 BoolSize=0,BoolOffset=0,BoolMask=0,BoolFieldMask=0;
        FProperty* Inner=nullptr;FProperty* Key=nullptr;FProperty* Value=nullptr;FScriptSetLayout SetLayout{};FScriptMapLayout MapLayout{};EMapPropertyFlags MapFlags=EMapPropertyFlags::None;EArrayPropertyFlags ArrayFlags=EArrayPropertyFlags::None;UUserDefinedStruct* Struct=nullptr;UEnum* ByteEnum=nullptr;UClass* ReferenceClass=nullptr;
    };
    struct FPayloadStruct
    {
        TStrongObjectPtr<UUserDefinedStruct> Type;TStrongObjectPtr<UPackage> Package;TStrongObjectPtr<UObject> Data;
        UClass* DataClass=nullptr;EObjectFlags Flags=RF_NoFlags,DataFlags=RF_NoFlags;EStructFlags StructFlags=STRUCT_NoFlags;
        FGuid Guid;FString Path,DataPath;int32 Size=0,Align=0;uint32 PackageFlags=0;const uint8* Memory=nullptr;
        TArray<FProperty*> Members;FValue Defaults;TSharedRef<FJsonObject> FileBefore=MakeShared<FJsonObject>();bool Ready=false;
    };
    struct FPayloadEnum
    {
        TStrongObjectPtr<UEnum> Type;TStrongObjectPtr<UPackage> Package;UClass* Class=nullptr;
        FString Path,CppType;EObjectFlags Flags=RF_NoFlags;uint32 PackageFlags=0;bool Dirty=false,UserDefined=false,FlagsEnum=false;
        UEnum::ECppForm Form=UEnum::ECppForm::Regular;UEnum::EUnderlyingType Underlying=UEnum::EUnderlyingType::uint8;
        TArray<FName> Names;TArray<int64> Values;TArray<FString> AuthoredNames;TMap<FName,TOptional<FString>> DisplayNames;
        TOptional<FString> Description;uint32 UniqueNameIndex=0;TSharedRef<FJsonObject> FileBefore=MakeShared<FJsonObject>();
    };
    TArray<FPayloadField> PayloadFields;TArray<TUniquePtr<FPayloadStruct>> PayloadStructs;TArray<TUniquePtr<FPayloadEnum>> PayloadEnums;
    bool PayloadDefaultsReady=false,CompositePayload=false,HashedPayload=false,StructArrayPayload=false,PayloadReady=false,ByteEnumPayload=false;
    // ACS: original converter/row writer remains unchanged. Resident references
    // are admitted before writes; soft handles are copied without Get/Load.
    struct FPayloadReferenceRole
    {
        TStrongObjectPtr<UObject> Object;UClass* Class=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;
        FString Path;EObjectFlags Flags=RF_NoFlags;uint32 PackageFlags=0;bool Dirty=false;
    };
    TArray<FPayloadReferenceRole> ReferenceRoles;TMap<FString,TSharedRef<FJsonObject>> ReferenceIdentities,ReferenceFiles;
    TSet<UObject*> ReferenceRoots;bool ReferencePayload=false,ReferenceAdmissionClosed=false;
    bool ReferenceCurrent(bool ReadDisk=false)
    {
        if(!Context())return false;
        for(const auto& E:ReferenceRoles)
        {
            auto* O=E.Object.Get();if(!Loaded(O)||!Loaded(E.Class)||!Loaded(E.Package)||O->GetClass()!=E.Class||O->GetOuter()!=E.Outer||O->GetPackage()!=E.Package||O->GetFlags()!=E.Flags||O->GetPathName()!=E.Path||uint32(E.Package->GetPackageFlags())!=E.PackageFlags||E.Package->IsDirty()!=E.Dirty)
                return PayloadFailure(TEXT("reference_source_role"),E.Path,TEXT("held source/class/outer/package/path/flags/dirty changed"));
        }
        if(ReadDisk)for(const auto& Pair:ReferenceFiles){auto Now=MakeShared<FJsonObject>();if(!Disk(Pair.Key,Now)||!Context()||!MirrorReadLiteralEqual(Pair.Value,Now))return PayloadFailure(TEXT("reference_source_disk"),Pair.Key,TEXT("resident source package file changed"));}
        return true;
    }
    bool EnrollReference(UObject* O)
    {
        if(!O)return true;if(!Context()||!Loaded(O))return PayloadReject(TEXT("datatable_payload_reference_source_pending"),TEXT("Current resident reference required; no resolver fallback."));
        if(ReferenceRoots.Contains(O))return ReferenceCurrent();
        if(ReferenceAdmissionClosed)return PayloadFailure(TEXT("reference_unplanned_after_writer"),TEXT("reference admitted before selected-row writer"),O->GetPathName());
        if(!O->IsAsset()||O->GetOuter()!=O->GetPackage()||!AssetPath(O->GetPathName(),false)||O->GetPackage()->IsDirty()||O->GetPackage()->HasAnyPackageFlags(PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly|PKG_PlayInEditor|PKG_CompiledIn)||O->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject)||!Loaded(O->GetClass())||!O->GetClass()->HasAnyClassFlags(CLASS_Native)||!O->GetClass()->ClassGeneratedBy.IsResolved()||O->GetClass()->ClassGeneratedBy.Get()||O->GetClass()->IsEditorOnly()||O->GetClass()->HasAnyClassFlags(CLASS_DefaultToInstanced|CLASS_NewerVersionExists)||O->GetPackage()->GetName()==SourcePackage||(!SelectedPackage.IsEmpty()&&O->GetPackage()->GetName()==SelectedPackage))
            return PayloadReject(TEXT("datatable_payload_reference_source_pending"),TEXT("Loaded saved-clean top-level runtime Game/Engine native asset in a distinct package required; cold/generated/instanced/self references remain incomplete."));
        // FindReferences invokes AddReferencedObjects. Capture the admitted root
        // before that callback; never adopt its returned state as a new baseline.
        if(!Pin(O)||!Pin(O->GetClass())||!Pin(O->GetOuter())||!Pin(O->GetPackage()))return false;
        if(!ReferenceIdentities.Contains(O->GetPathName()))
        {
            FPayloadReferenceRole E;E.Object.Reset(O);E.Class=O->GetClass();E.Outer=O->GetOuter();E.Package=O->GetPackage();E.Path=O->GetPathName();E.Flags=O->GetFlags();E.PackageFlags=uint32(E.Package->GetPackageFlags());E.Dirty=E.Package->IsDirty();ReferenceIdentities.Add(E.Path,Identity(O));ReferenceRoles.Add(MoveTemp(E));
        }
        const FString RootPackageName=O->GetPackage()->GetName();if(!ReferenceFiles.Contains(RootPackageName)){if(ReferenceFiles.Num()>=32)return PayloadReject(TEXT("datatable_payload_reference_file_bound"),TEXT("At most32 distinct resident source package files per UDS request; wider direct reference sets remain incomplete."));auto File=MakeShared<FJsonObject>();if(!Disk(RootPackageName,File)||!Context()||!File->GetBoolField(TEXT("exists")))return PayloadReject(TEXT("datatable_payload_reference_file_pending"),TEXT("Saved source package file required; no source loader/save."));ReferenceFiles.Add(RootPackageName,File);}
        if(!Raw(false)||!ReferenceCurrent(true))return false;
        TStrongObjectPtr<UObject> ReturnedHold(O);TArray<UObject*> Closure;FReferenceFinder Finder(Closure,nullptr,false,true,false,true);Finder.FindReferences(O);
        if(!Raw(false)||!ReferenceCurrent(true))return false;
        if(Closure.Num()>512)return PayloadReject(TEXT("datatable_payload_reference_closure_pending"),TEXT("Finite resident root and direct reference set required before row conversion."));Closure.AddUnique(O);
        for(auto* Ref:Closure)
        {
            if(!Loaded(Ref)||!Loaded(Ref->GetClass())||!Loaded(Ref->GetPackage())||!CleanPath(Ref->GetPathName())||!CleanPath(Ref->GetClass()->GetPathName())||Ref==Type.Get()||Ref->IsIn(Type.Get())||Ref->GetPackage()->GetName()==SelectedPackage||ReferenceRoles.Num()>=2048)
                return PayloadReject(TEXT("datatable_payload_reference_closure_pending"),TEXT("Source direct reference set must be resident, tooling-free and separate from selected table/UDS; wider aliases remain incomplete."));
            const FString PackageName=Ref->GetPackage()->GetName();if((PackageName.StartsWith(TEXT("/Game/"))||PackageName.StartsWith(TEXT("/Engine/")))&&!ReferenceFiles.Contains(PackageName)){if(ReferenceFiles.Num()>=32)return PayloadReject(TEXT("datatable_payload_reference_file_bound"),TEXT("At most32 distinct resident source package files per UDS request; wider direct reference sets remain incomplete."));auto File=MakeShared<FJsonObject>();if(!Disk(PackageName,File)||!Context()||!File->GetBoolField(TEXT("exists")))return PayloadReject(TEXT("datatable_payload_reference_file_pending"),TEXT("Saved resident source direct-reference file required; no source loader/save."));ReferenceFiles.Add(PackageName,File);}
            if(ReferenceIdentities.Contains(Ref->GetPathName()))continue;if(!Pin(Ref)||!Pin(Ref->GetClass())||!Pin(Ref->GetPackage()))return false;
            FPayloadReferenceRole E;E.Object.Reset(Ref);E.Class=Ref->GetClass();E.Outer=Ref->GetOuter();E.Package=Ref->GetPackage();E.Path=Ref->GetPathName();E.Flags=Ref->GetFlags();E.PackageFlags=uint32(E.Package->GetPackageFlags());E.Dirty=E.Package->IsDirty();ReferenceIdentities.Add(E.Path,Identity(Ref));ReferenceRoles.Add(MoveTemp(E));
        }
        ReferenceRoots.Add(O);R.Data->SetNumberField(TEXT("uds_reference_source_identities"),ReferenceRoles.Num());R.Data->SetNumberField(TEXT("uds_reference_source_files"),ReferenceFiles.Num());R.Data->SetBoolField(TEXT("referenced_asset_full_RAM_payload_purity_proven"),false);return ReferenceCurrent();
    }
    bool ReferenceLiteral(FProperty* P,const TSharedPtr<FJsonValue>& Value)
    {
        if(!Value||Value->Type==EJson::Null)return true;auto V=Value;
        // SDK scalar ArrayDim=1 reads only the first JSON array item. Other
        // nonstring types retain stock noninstanced failure/no-op semantics.
        if(V->Type==EJson::Array){if(V->AsArray().IsEmpty())return true;V=V->AsArray()[0];}
        if(!V||V->Type!=EJson::String)return true;const FString Requested=V->AsString();if(Requested.IsEmpty()||Requested==TEXT("None"))return true;
        if(Requested.Len()>2048||!CleanPath(Requested))return PayloadReject(TEXT("datatable_payload_reference_literal_pending"),TEXT("Bounded runtime reference literal required; broader original aliases remain incomplete."));
        FString Path=FPackageName::ExportTextPathToObjectPath(Requested);Path.TrimQuotesInline();if(!AssetPath(Path,false))return PayloadReject(TEXT("datatable_payload_reference_literal_pending"),TEXT("Exact Game/Engine asset path required; short/class/subobject/cold variants remain incomplete."));
        auto* Ref=StaticFindObject(nullptr,nullptr,*Path);TStrongObjectPtr<UObject> Hold(Ref);if(!Raw(false)||!Loaded(Ref))return PayloadReject(TEXT("datatable_payload_reference_load_pending"),TEXT("Original ImportText may load assets; this profile requires explicit prior load, no preloader or value substitution."));
        const auto* Role=PayloadFields.FindByPredicate([&](const FPayloadField& E){return E.P==P;});if(!Role||!Role->ReferenceClass||!Ref->IsA(Role->ReferenceClass))return PayloadReject(TEXT("datatable_payload_reference_class_pending"),TEXT("Resident value must satisfy actual held property class; wider original source parsing remains incomplete."));
        const int32 Quote=Requested.Find(TEXT("'"));if(Quote!=INDEX_NONE){const FString Prefix=Requested.Left(Quote);if(!Requested.EndsWith(TEXT("'"))||(Prefix!=Ref->GetClass()->GetName()&&Prefix!=Ref->GetClass()->GetPathName()))return PayloadReject(TEXT("datatable_payload_reference_literal_pending"),TEXT("Export prefix must match the actual loaded native asset class; no class resolver fallback."));}
        return EnrollReference(Ref);
    }
    bool ReferenceProjectionEqual(const TSharedRef<FJsonObject>& A,const TSharedRef<FJsonObject>& B)
    {
        if(!ReferenceCurrent())return false;
        for(const auto& Pair:A->Values)
        {
            const FString Path(Pair.Key.ToView());const auto* Known=ReferenceIdentities.Find(Path);const auto Actual=B->TryGetField(Pair.Key.ToView());
            if(Known){if(!MirrorReadLiteralEqual(Pair.Value,MakeShared<FJsonValueObject>(*Known))||(Actual&&!MirrorReadLiteralEqual(Actual,MakeShared<FJsonValueObject>(*Known))))return PayloadFailure(TEXT("reference_source_projection"),Path,TEXT("planned readonly identity differs"));}
            else if(!Actual||!StringUdsEqual(Pair.Value,Actual,*this,false,FString(),false))return false;
        }
        for(const auto& Pair:B->Values)if(!A->HasField(Pair.Key.ToView())){const auto* Known=ReferenceIdentities.Find(FString(Pair.Key.ToView()));if(!Known||!MirrorReadLiteralEqual(Pair.Value,MakeShared<FJsonValueObject>(*Known)))return false;}
        return true;
    }
    static TOptional<FString> PayloadEnumTextSource(const FText& Text)
    {const FString* Source=FTextInspector::GetSourceString(Text);return Source?TOptional<FString>(*Source):TOptional<FString>();}
    bool PayloadEnumsCurrent()
    {
        // One source pass per actual current/SDK boundary, never per copied array/set/map slot.
        // No disk, display-name generation/repair, metadata caches or converter runs here.
        for(const auto& Entry:PayloadEnums)
        {
            const auto& E=*Entry;auto* U=E.Type.Get();auto* K=E.Package.Get();
            if(!Context()||!Loaded(U)||!Loaded(K)||U->GetClass()!=E.Class||U->GetOuter()!=K||U->GetPackage()!=K
                ||U->GetFlags()!=E.Flags||!U->GetPathName().Equals(E.Path,ESearchCase::CaseSensitive)
                ||uint32(K->GetPackageFlags())!=E.PackageFlags||K->IsDirty()!=E.Dirty)
                return PayloadFailure(TEXT("payload_enum_identity"),E.Path,TEXT("captured enum class/owner/identity/package changed"),true,E.Path);
            if(U->GetCppForm()!=E.Form||U->GetUnderlyingType()!=E.Underlying||!U->CppType.Equals(E.CppType,ESearchCase::CaseSensitive)
                ||U->HasAnyEnumFlags(EEnumFlags::Flags)!=E.FlagsEnum||U->HasAnyEnumFlags(EEnumFlags::NewerVersionExists)||U->NumEnums()!=E.Names.Num())
                return PayloadFailure(TEXT("payload_enum_form"),TEXT("captured native enumeration form/name inventory"),TEXT("changed"),true,E.Path);
            for(int32 I=0;I<E.Names.Num();++I)
                if(U->GetNameByIndex(I)!=E.Names[I]||U->GetValueByIndex(I)!=E.Values[I])
                    return PayloadFailure(TEXT("payload_enum_entry"),E.Names[I].ToString()+TEXT("=")+LexToString(E.Values[I]),LexToString(I)+TEXT(" changed"),true,E.Path);
            if(E.UserDefined)
            {
                const auto* User=static_cast<UUserDefinedEnum*>(U);
                if(User->DisplayNameMap.Num()!=E.DisplayNames.Num()||User->UniqueNameIndex!=E.UniqueNameIndex||PayloadEnumTextSource(User->EnumDescription)!=E.Description)
                    return PayloadFailure(TEXT("payload_enum_authored_map"),TEXT("captured authored names/description"),TEXT("changed"),true,E.Path);
                for(const auto& Pair:E.DisplayNames)
                {
                    const auto* Now=User->DisplayNameMap.Find(Pair.Key);
                    if(!Now||PayloadEnumTextSource(*Now)!=Pair.Value)return PayloadFailure(TEXT("payload_enum_authored_name"),Pair.Key.ToString(),TEXT("authored FText source changed"),true,E.Path);
                }
            }
        }
        return true;
    }
    bool BindPayloadEnum(UEnum* U)
    {
        if(!Context()||!Loaded(U))return PayloadReject(TEXT("datatable_payload_enum_unavailable"),TEXT("Byte enum descriptor requires an already resolved loaded enum; no extra loader."));
        for(const auto& Existing:PayloadEnums)if(Existing->Type.Get()==U)return PayloadEnumsCurrent();
        if(PayloadEnums.Num()>=16)return PayloadReject(TEXT("datatable_payload_enum_bound"),TEXT("At most16 distinct byte enum sources per existing UDS DataTable scope."));
        const bool User=U->GetClass()==UUserDefinedEnum::StaticClass();auto* K=U->GetPackage();
        const bool Native=U->GetClass()==UEnum::StaticClass()&&Loaded(K)&&K->HasAnyPackageFlags(PKG_CompiledIn)
            &&(K->GetName()==TEXT("/Script/Engine")||K->GetName()==TEXT("/Script/CoreUObject"));
        if((!User&&!Native)||!Loaded(K)||U->GetOuter()!=K||U->NumEnums()<1||U->NumEnums()>1024
            ||U->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject)||U->HasAnyEnumFlags(EEnumFlags::NewerVersionExists)
            ||K->HasAnyPackageFlags(PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly|PKG_PlayInEditor)
            ||(User&&(!AssetPath(U->GetPathName(),true)||K->IsDirty()||K->HasAnyPackageFlags(PKG_CompiledIn))))
            return PayloadReject(TEXT("datatable_payload_enum_source_pending"),TEXT("Exact loaded native Engine/Core enum or saved-clean top-level Game UserDefinedEnum required; wider/custom/dirty sources remain incomplete."));
        if(!Pin(U)||!Pin(U->GetClass())||!Pin(K))return false;
        auto Role=MakeUnique<FPayloadEnum>();auto& E=*Role;E.Type.Reset(U);E.Package.Reset(K);E.Class=U->GetClass();E.Path=U->GetPathName();E.Flags=U->GetFlags();
        E.PackageFlags=uint32(K->GetPackageFlags());E.Dirty=K->IsDirty();E.UserDefined=User;E.Form=U->GetCppForm();E.Underlying=U->GetUnderlyingType();E.CppType=U->CppType;E.FlagsEnum=U->HasAnyEnumFlags(EEnumFlags::Flags);
        int32 Chars=E.CppType.Len();
        for(int32 I=0;I<U->NumEnums();++I)
        {
            E.Names.Add(U->GetNameByIndex(I));E.Values.Add(U->GetValueByIndex(I));E.AuthoredNames.Add(U->GetAuthoredNameStringByIndex(I));
            Chars+=E.Names.Last().GetStringLength()+E.AuthoredNames.Last().Len();
            if(Chars>65536)return PayloadReject(TEXT("datatable_payload_enum_name_bound"),TEXT("Copied raw/authored enum names exceed65536 characters; source is left untouched."));
        }
        if(User)
        {
            auto* Data=static_cast<UUserDefinedEnum*>(U);if(Data->DisplayNameMap.Num()>1024)return PayloadReject(TEXT("datatable_payload_enum_display_bound"),TEXT("At most1024 authored enum display entries are admitted."));
            for(const auto& Pair:Data->DisplayNameMap)E.DisplayNames.Add(Pair.Key,PayloadEnumTextSource(Pair.Value));E.Description=PayloadEnumTextSource(Data->EnumDescription);E.UniqueNameIndex=Data->UniqueNameIndex;
            if(!Disk(K->GetName(),E.FileBefore)||!Context()||!E.FileBefore->GetBoolField(TEXT("exists"))||E.FileBefore->GetNumberField(TEXT("bytes"))<=0)
                return PayloadReject(TEXT("datatable_payload_enum_file_pending"),TEXT("Existing saved UserDefinedEnum source file required; no compile/repair/save."));
        }
        PayloadEnums.Add(MoveTemp(Role));ByteEnumPayload=CompositePayload=true;return PayloadEnumsCurrent();
    }
    FPayloadStruct* PayloadType(UUserDefinedStruct* S)
    {for(const auto& E:PayloadStructs)if(E->Type.Get()==S)return E.Get();return nullptr;}
    bool BindPayload(FProperty* P,FProperty* Parent=nullptr,int32 Depth=0)
    {
        // Admit the exact native descriptor before any overridable accessor/layout query.
        if(!P||(!ScalarKind(P)&&!PayloadKind(P)))return PayloadReject(TEXT("datatable_payload_descriptor_class_pending"),TEXT("Exact native scalar7/Array/Set/Map/Struct property class required; custom descriptors remain valid original scope not yet ported."));
        if(const auto* Old=PayloadFields.FindByPredicate([&](const FPayloadField& E){return E.P==P;}))
        {if(Old->Struct){const auto* Role=PayloadType(Old->Struct);if(!Role||!Role->Ready)return PayloadReject(TEXT("datatable_payload_struct_cycle"),TEXT("Acyclic finite compiled UDS tree required."));}return true;}
        if(!P||Depth>8||PayloadFields.Num()>=256||P->ArrayDim!=1||P->HasSetterOrGetter()||P->GetSize()<=0||P->GetMinAlignment()<=0
            ||P->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated|CPF_EditorOnly|CPF_InstancedReference|CPF_ContainsInstancedReference|CPF_Config|CPF_GlobalConfig))
            return PayloadReject(TEXT("datatable_payload_layout_pending"),TEXT("Bounded scalar/scalar-or-UDS-array/user-struct fields without accessors, instanced or transient/config roles required; wider original fields remain incomplete."));
        UStruct* Owner=P->GetOwnerStruct();if(!Loaded(Owner)||P->GetOwner<FProperty>()!=Parent||P->GetOwnerProperty()!=(Parent?Parent->GetOwnerProperty():P))
            return PayloadReject(TEXT("datatable_payload_owner_pending"),TEXT("Actual direct reflected member or Array.Inner owner required."));
        FPayloadField E;E.P=P;E.Parent=Parent;E.Owner=Owner;E.Kind=P->GetClass();E.Name=P->GetFName();E.Flags=uint64(P->GetPropertyFlags());E.Size=P->GetSize();E.Offset=P->GetOffset_ForInternal();E.Dim=P->ArrayDim;E.Align=P->GetMinAlignment();
        if(!Parent&&(E.Offset<0||int64(E.Offset)+E.Size>Owner->GetPropertiesSize()))return PayloadReject(TEXT("datatable_payload_member_bounds"),TEXT("Member storage must fit its actual owning struct/class."));
        if(!Parent&&Owner->GetClass()==UUserDefinedStruct::StaticClass())
        {
            auto* U=static_cast<UUserDefinedStruct*>(Owner);E.Guid=U->FindPropertyGuidFromName(E.Name);E.Authored=U->GetAuthoredNameForField(P);
            if(!Context()||!E.Guid.IsValid()||E.Authored.IsEmpty())return PayloadReject(TEXT("datatable_payload_authored_mapping"),TEXT("Compiled UDS members require actual authored names and property GUIDs."));
        }
        if(ScalarKind(P))
        {
            const int32 Size=E.Kind==FStrProperty::StaticClass()?sizeof(FString):E.Kind==FNameProperty::StaticClass()?sizeof(FName):E.Kind==FInt64Property::StaticClass()||E.Kind==FDoubleProperty::StaticClass()?8:E.Kind==FBoolProperty::StaticClass()?sizeof(bool):4;
            if(E.Size!=Size)return PayloadReject(TEXT("datatable_payload_scalar_storage_pending"),TEXT("Actual native scalar storage must match the exact property role."));
            if(E.Kind==FBoolProperty::StaticClass()){auto* B=static_cast<FBoolProperty*>(P);E.BoolSize=B->GetBoolFieldSize();E.BoolOffset=B->GetByteOffset();E.BoolMask=B->GetByteMask();E.BoolFieldMask=B->GetFieldMask();if(!B->IsNativeBool()||E.BoolSize!=sizeof(bool)||E.BoolOffset>=E.Size||!E.BoolMask)return PayloadReject(TEXT("datatable_payload_bool_storage_pending"),TEXT("Actual compiled native bool storage required; custom packed storage remains incomplete."));}
            PayloadFields.Add(E);return true;
        }
        if(ReferencePayloadKind(P))
        {
            if(P->HasAnyPropertyFlags(CPF_PersistentInstance))return PayloadReject(TEXT("datatable_payload_reference_instanced_pending"),TEXT("Nonpersistent noninstanced reference property required; inline object construction remains incomplete."));
            auto* Ref=static_cast<FObjectPropertyBase*>(P);if(!Ref->PropertyClass.IsResolved()||!Loaded(Ref->PropertyClass.Get()))return PayloadReject(TEXT("datatable_payload_reference_class_pending"),TEXT("Resolved loaded native runtime property class required; no class lookup/loader."));
            UClass* C=Ref->PropertyClass.Get();if(!C->HasAnyClassFlags(CLASS_Native)||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy.Get()||C->IsEditorOnly()||C->HasAnyClassFlags(CLASS_DefaultToInstanced|CLASS_NewerVersionExists)||!CleanPath(C->GetPathName())||!Pin(C)||!Pin(C->GetPackage()))return PayloadReject(TEXT("datatable_payload_reference_class_pending"),TEXT("Noninstanced native runtime declared class required; generated/class/instanced variants remain incomplete."));
            const bool Soft=E.Kind==FSoftObjectProperty::StaticClass();if(E.Size!=(Soft?sizeof(FSoftObjectPtr):sizeof(TObjectPtr<UObject>))||E.Align!=(Soft?alignof(FSoftObjectPtr):alignof(TObjectPtr<UObject>)))return PayloadReject(TEXT("datatable_payload_reference_storage_pending"),TEXT("Exact current native hard/soft object storage/layout required."));
            E.ReferenceClass=C;PayloadFields.Add(E);ReferencePayload=CompositePayload=true;return true;
        }
        if(ByteEnumPayloadKind(P))
        {
            auto* Byte=static_cast<FByteProperty*>(P);
            if(E.Size!=sizeof(uint8)||!Byte->Enum.IsResolved()||Byte->Enum==nullptr)
                return PayloadReject(TEXT("datatable_payload_byte_enum_pending"),TEXT("Exact native byte storage with an already resolved nonnull Enum slot required; plain bytes/FEnumProperty remain incomplete."));
            // The public resolved slot can be copied without a tracked read. Bind
            // the object found by its existing path before invoking source methods.
            const FString EnumPath=Byte->Enum.GetPathName();
            auto* Returned=EnumPath.IsEmpty()?nullptr:StaticFindObject(UEnum::StaticClass(),nullptr,*EnumPath);
            if(!Context()||!Loaded(Returned)||Byte->Enum!=static_cast<UEnum*>(Returned))
                return PayloadReject(TEXT("datatable_payload_byte_enum_binding"),TEXT("Current resolved byte enum path/object binding required; no resolver fallback."));
            E.ByteEnum=static_cast<UEnum*>(Returned);PayloadFields.Add(E);
            return BindPayloadEnum(E.ByteEnum);
        }
        if(P->GetClass()==FArrayProperty::StaticClass())
        {
            auto* A=static_cast<FArrayProperty*>(P);E.Inner=A->Inner;E.ArrayFlags=A->ArrayFlags;
            if(!E.Inner||!(ScalarKind(E.Inner)||ByteEnumPayloadKind(E.Inner)||ReferencePayloadKind(E.Inner)||E.Inner->GetClass()==FStructProperty::StaticClass())||E.ArrayFlags!=EArrayPropertyFlags::None||P->GetSize()!=sizeof(FScriptArray)||P->GetMinAlignment()!=alignof(FScriptArray))return PayloadReject(TEXT("datatable_payload_array_pending"),TEXT("Actual default-allocator array with scalar7, admitted byte enum or compiled UDS Inner required; native/custom-struct, direct nested-container, object and FEnumProperty elements remain incomplete."));
            // Struct Inner is owned by the array descriptor, not an authored UDS member.
            // Reuse the existing compiled-source/default/current lease and fresh recursive value copies.
            PayloadFields.Add(E);CompositePayload=true;if(E.Inner->GetClass()==FStructProperty::StaticClass())StructArrayPayload=true;return BindPayload(E.Inner,P,Depth+1);
        }
        if(P->GetClass()==FSetProperty::StaticClass())
        {
            auto* S=static_cast<FSetProperty*>(P);E.Inner=S->ElementProp;E.SetLayout=S->SetLayout;
            if(!E.Inner||!HashScalarKind(E.Inner)||P->GetSize()!=sizeof(FScriptSet)||P->GetMinAlignment()!=alignof(FScriptSet)||!SetLayoutSame(E.SetLayout,FScriptSet::GetScriptLayout(E.Inner->GetSize(),E.Inner->GetMinAlignment())))return PayloadReject(TEXT("datatable_payload_set_pending"),TEXT("Actual default-allocator Set with hashable native scalar7/admitted byte enum ElementProp and SDK-linked layout required; struct/object/FEnumProperty elements remain incomplete."));
            PayloadFields.Add(E);CompositePayload=HashedPayload=true;return BindPayload(E.Inner,P,Depth+1);
        }
        if(P->GetClass()==FMapProperty::StaticClass())
        {
            auto* M=static_cast<FMapProperty*>(P);E.Key=M->KeyProp;E.Value=M->ValueProp;E.MapLayout=M->MapLayout;E.MapFlags=M->MapFlags;
            if(!E.Key||!E.Value||!HashScalarKind(E.Key)||E.MapFlags!=EMapPropertyFlags::None||P->GetSize()!=sizeof(FScriptMap)||P->GetMinAlignment()!=alignof(FScriptMap)||!(ScalarKind(E.Value)||ByteEnumPayloadKind(E.Value)||ReferencePayloadKind(E.Value)||E.Value->GetClass()==FArrayProperty::StaticClass()||E.Value->GetClass()==FStructProperty::StaticClass())||!MapLayoutSame(E.MapLayout,FScriptMap::GetScriptLayout(E.Key->GetSize(),E.Key->GetMinAlignment(),E.Value->GetSize(),E.Value->GetMinAlignment())))return PayloadReject(TEXT("datatable_payload_map_pending"),TEXT("Actual default-allocator Map with hashable scalar7/admitted byte enum key and existing finite scalar/byte-enum/array/UDS value payload required; other key/value operators and allocators remain incomplete."));
            PayloadFields.Add(E);CompositePayload=HashedPayload=true;return BindPayload(E.Key,P,Depth+1)&&BindPayload(E.Value,P,Depth+1);
        }
        if(P->GetClass()!=FStructProperty::StaticClass())return PayloadReject(TEXT("datatable_payload_kind_pending"),TEXT("Reflected payload admits scalar7, scalar-or-UDS arrays, scalar Sets, scalar-key Maps and compiled user-defined structs; other original types remain incomplete."));
        auto* F=static_cast<FStructProperty*>(P);if(!F->Struct.IsResolved())return PayloadReject(TEXT("datatable_payload_struct_unresolved"),TEXT("Existing resolved UDS source required; no source loader or compiler."));
        UScriptStruct* S=F->Struct.Get();TStrongObjectPtr<UScriptStruct> Immediate(S);
        if(!Context()||!Loaded(S)||S->GetClass()!=UUserDefinedStruct::StaticClass()||P->ElementSize!=S->GetStructureSize())return PayloadReject(TEXT("datatable_payload_struct_pending"),TEXT("Exact loaded compiled user-defined struct with matching storage required; custom/native struct operators remain incomplete."));
        E.Struct=static_cast<UUserDefinedStruct*>(S);if(E.Struct==Type.Get())return PayloadReject(TEXT("datatable_payload_struct_cycle"),TEXT("Root row UDS may not recursively contain itself."));PayloadFields.Add(E);CompositePayload=true;
        // A completed shared type is a DAG reuse, not a cycle; only an unfinished ancestor is rejected.
        if(auto* Existing=PayloadType(E.Struct))return Existing->Ready||PayloadReject(TEXT("datatable_payload_struct_cycle"),TEXT("Recursive source type is outside the finite payload tree."));
        if(PayloadStructs.Num()>=16)return PayloadReject(TEXT("datatable_payload_source_bound"),TEXT("At most16 distinct nested UDS sources required."));
        auto Entry=MakeUnique<FPayloadStruct>();auto* Role=Entry.Get();Role->Type.Reset(E.Struct);Role->Package.Reset(E.Struct->GetPackage());
        auto* U=E.Struct;auto* K=Role->Package.Get();
        if(!Loaded(K)||U->GetOuter()!=K||!U->GetPathName().StartsWith(TEXT("/Game/"))||!CleanPath(U->GetPathName())||K->IsDirty()||U->Status!=UDSS_UpToDate||!U->Guid.IsValid()||U->PrimaryStruct.IsStale()||U->PrimaryStruct.Get()||U->GetSuperStruct()||!U->EditorData.IsResolved())
            return PayloadReject(TEXT("datatable_payload_source_pending"),TEXT("Saved-clean top-level own compiled Game UDS with resolved existing editor data and no parent/primary duplicate required."));
        UObject* Data=U->EditorData.Get();Role->Data.Reset(Data);if(!Loaded(Data)||Data->GetOuter()!=U||Data->GetClass()!=UUserDefinedStructEditorData::StaticClass())return PayloadReject(TEXT("datatable_payload_editor_data_pending"),TEXT("Held exact native UDS-owned UserDefinedStructEditorData required for the SDK read-only authored-name implementation; custom editor data remains incomplete."));
        if(!Pin(U)||!Pin(K)||!Pin(Data)||!Pin(Data->GetClass())||!Pin(U->GetClass()))return false;Role->DataClass=Data->GetClass();Role->DataFlags=Data->GetFlags();Role->DataPath=Data->GetPathName();Role->Flags=U->GetFlags();Role->StructFlags=U->StructFlags;Role->Guid=U->Guid;Role->Size=U->GetStructureSize();Role->Align=U->GetMinAlignment();Role->Path=U->GetPathName();Role->PackageFlags=uint32(K->GetPackageFlags());
        Role->Memory=U->GetDefaultInstance();if(!Context()||!Role->Memory||Role->Size<=0||Role->Size>65536||Role->Align<=0)return PayloadReject(TEXT("datatable_payload_default_pending"),TEXT("Existing bounded compiled UDS default instance required; no default allocation/recreation."));
        for(TFieldIterator<FProperty> It(U,EFieldIterationFlags::None);It;++It){if(Role->Members.Num()>=64)return PayloadReject(TEXT("datatable_payload_members_bound"),TEXT("At most64 own fields per UDS source."));Role->Members.Add(*It);}
        if(Role->Members.IsEmpty())return PayloadReject(TEXT("datatable_payload_empty_struct_pending"),TEXT("Nonempty compiled reflected UDS source required."));
        PayloadStructs.Add(MoveTemp(Entry));TSet<FGuid> Guids;TSet<FString> AuthoredNames;
        for(FProperty* Member:Role->Members)
        {
            if(Member->GetOwnerStruct()!=U)return PayloadFailure(TEXT("payload_member_owner"),Role->Path,TEXT("current direct member owner changed"));if(!BindPayload(Member,nullptr,Depth+1))return false;
            const auto* Captured=PayloadFields.FindByPredicate([&](const FPayloadField& Z){return Z.P==Member;});
            if(!Captured||Guids.Contains(Captured->Guid)||AuthoredNames.Contains(Captured->Authored))return PayloadReject(TEXT("datatable_payload_duplicate_authored_mapping"),TEXT("Unique actual UDS member names/GUIDs required."));Guids.Add(Captured->Guid);AuthoredNames.Add(Captured->Authored);
        }
        if(!Disk(K->GetName(),Role->FileBefore)||!Context()||!Role->FileBefore->GetBoolField(TEXT("exists"))||Role->FileBefore->GetNumberField(TEXT("bytes"))<=0)return PayloadReject(TEXT("datatable_payload_source_file_pending"),TEXT("Existing saved source UDS file required."));
        TArray<UObject*> OwnedSource;GetObjectsWithOuter(U,OwnedSource,EGetObjectsFlags::IncludeNestedObjects);
        if(OwnedSource.Num()!=1||OwnedSource[0]!=Data)return PayloadReject(TEXT("datatable_payload_source_exports_pending"),TEXT("Exact UDS and native owned editor-data export roles required; wider source exports remain incomplete."));
        SourceOwned.AddUnique(U);SourceOwned.AddUnique(Data);Role->Ready=true;return true;
    }
    bool PayloadCurrent()
    {
        // Validate owning objects and fresh member inventories before any retained field is read.
        for(const auto& Entry:PayloadStructs)
        {
            const auto& E=*Entry;auto* U=E.Type.Get();auto* K=E.Package.Get();auto* D=E.Data.Get();
            if(!Loaded(U)||U->GetClass()!=UUserDefinedStruct::StaticClass()||!Loaded(K)||U->GetOuter()!=K||U->GetFlags()!=E.Flags||!U->GetPathName().Equals(E.Path,ESearchCase::CaseSensitive)||K->IsDirty()||uint32(K->GetPackageFlags())!=E.PackageFlags||U->Status!=UDSS_UpToDate||U->Guid!=E.Guid||U->PrimaryStruct.IsStale()||U->PrimaryStruct.Get()||U->GetSuperStruct()||U->StructFlags!=E.StructFlags||U->GetStructureSize()!=E.Size||U->GetMinAlignment()!=E.Align||!U->EditorData.IsResolved()||U->EditorData.Get()!=D)
                return PayloadFailure(TEXT("payload_source_role"),E.Path,TEXT("source identity/status/layout/slots changed"));
            if(!Loaded(D)||D->GetClass()!=E.DataClass||D->GetOuter()!=U||D->GetFlags()!=E.DataFlags||!D->GetPathName().Equals(E.DataPath,ESearchCase::CaseSensitive))return PayloadFailure(TEXT("payload_editor_data"),E.DataPath,TEXT("held editor-data role changed"));
            TArray<FProperty*> Now;for(TFieldIterator<FProperty> It(U,EFieldIterationFlags::None);It;++It){if(Now.Num()>=64)return PayloadFailure(TEXT("payload_member_count"),LexToString(E.Members.Num()),TEXT(">64"));Now.Add(*It);}if(Now!=E.Members)return PayloadFailure(TEXT("payload_member_inventory"),E.Path,TEXT("direct field membership/order changed; stale fields not read"));
        }
        for(const auto& E:PayloadFields)
        {
            if(E.Parent)
            {
                bool Member=false;if(E.Parent->GetClass()==FArrayProperty::StaticClass())Member=static_cast<FArrayProperty*>(E.Parent)->Inner==E.P;else if(E.Parent->GetClass()==FSetProperty::StaticClass())Member=static_cast<FSetProperty*>(E.Parent)->ElementProp==E.P;else if(E.Parent->GetClass()==FMapProperty::StaticClass()){auto* M=static_cast<FMapProperty*>(E.Parent);Member=M->KeyProp==E.P||M->ValueProp==E.P;}
                if(!Member)return PayloadFailure(TEXT("payload_inner_slot"),E.Name.ToString(),TEXT("fresh parent Array.Inner/Set.Element/Map.Key-or-Value changed; old nested field not read"));
            }
            auto* P=E.P;if(P->GetClass()!=E.Kind||P->GetOwnerStruct()!=E.Owner||P->GetOwner<FProperty>()!=E.Parent||P->GetOwnerProperty()!=(E.Parent?E.Parent->GetOwnerProperty():P)||P->GetFName()!=E.Name||P->ArrayDim!=E.Dim||P->GetSize()!=E.Size||P->GetOffset_ForInternal()!=E.Offset||P->GetMinAlignment()!=E.Align||uint64(P->GetPropertyFlags())!=E.Flags||P->HasSetterOrGetter())return PayloadFailure(TEXT("payload_field_layout"),E.Name.ToString(),TEXT("direct descriptor/owner/layout/flags changed"));
            if(E.Kind==FBoolProperty::StaticClass()){auto* B=static_cast<FBoolProperty*>(P);if(B->GetBoolFieldSize()!=E.BoolSize||B->GetByteOffset()!=E.BoolOffset||B->GetByteMask()!=E.BoolMask||B->GetFieldMask()!=E.BoolFieldMask)return PayloadFailure(TEXT("payload_bool_storage"),E.Name.ToString(),TEXT("native bool size/mask/offset changed"));}
            if(E.Inner&&E.Kind==FArrayProperty::StaticClass()){auto* A=static_cast<FArrayProperty*>(P);if(A->Inner!=E.Inner||A->ArrayFlags!=E.ArrayFlags)return PayloadFailure(TEXT("payload_array_binding"),E.Name.ToString(),TEXT("Inner/allocator flags changed"));}
            if(E.Kind==FSetProperty::StaticClass()){auto* S=static_cast<FSetProperty*>(P);if(S->ElementProp!=E.Inner||!SetLayoutSame(S->SetLayout,E.SetLayout))return PayloadFailure(TEXT("payload_set_binding"),E.Name.ToString(),TEXT("ElementProp or SDK-linked layout changed"));}
            if(E.Kind==FMapProperty::StaticClass()){auto* M=static_cast<FMapProperty*>(P);if(M->KeyProp!=E.Key||M->ValueProp!=E.Value||M->MapFlags!=E.MapFlags||!MapLayoutSame(M->MapLayout,E.MapLayout))return PayloadFailure(TEXT("payload_map_binding"),E.Name.ToString(),TEXT("Key/Value/allocator/layout changed"));}
            if(E.Struct){auto* S=static_cast<FStructProperty*>(P);if(!S->Struct.IsResolved()||S->Struct.Get()!=E.Struct)return PayloadFailure(TEXT("payload_struct_binding"),E.Name.ToString(),TEXT("resolved held struct slot changed"));}
            if(E.ByteEnum){auto* B=static_cast<FByteProperty*>(P);if(!B->Enum.IsResolved()||B->Enum!=E.ByteEnum)return PayloadFailure(TEXT("payload_byte_enum_binding"),E.Name.ToString(),TEXT("resolved held enum slot changed"));}
            if(E.ReferenceClass){auto* O=static_cast<FObjectPropertyBase*>(P);if(!O->PropertyClass.IsResolved()||O->PropertyClass.Get()!=E.ReferenceClass)return PayloadFailure(TEXT("payload_reference_class_binding"),E.Name.ToString(),TEXT("resolved held reference class changed"));}
            if(!E.Parent&&E.Owner->GetClass()==UUserDefinedStruct::StaticClass())
            {auto* U=static_cast<UUserDefinedStruct*>(E.Owner);const FGuid Guid=U->FindPropertyGuidFromName(E.Name);const FString Authored=U->GetAuthoredNameForField(P);if(!Context()||Guid!=E.Guid||!Authored.Equals(E.Authored,ESearchCase::CaseSensitive))return PayloadFailure(TEXT("payload_authored_mapping"),E.Authored+TEXT(":")+E.Guid.ToString(),Authored+TEXT(":")+Guid.ToString());}
        }
        if(!PayloadEnumsCurrent()||!ReferenceCurrent())return false;
        for(const auto& Entry:PayloadStructs)
        {
            const auto& E=*Entry;const uint8* Memory=E.Type->GetDefaultInstance();if(!Context()||Memory!=E.Memory)return PayloadFailure(TEXT("payload_default_instance"),E.Path,TEXT("compiled default storage changed"));
            if(PayloadDefaultsReady){FValue V;int32 Cells=0,Chars=0;if(!ReadStruct(E,Memory,V,Cells,Chars,0)||!Same(V,E.Defaults))return PayloadFailure(TEXT("payload_source_default"),ValueText(E.Defaults),ValueText(V),true,E.Path);}
        }
        return true;
    }
    bool ReadStruct(const FPayloadStruct& S,const void* Address,FValue& Out,int32& Cells,int32& Chars,int32 Depth)
    {
        if(!Address||Depth>8)return PayloadFailure(TEXT("payload_struct_value_bound"),TEXT("current struct memory/depth<=8"),TEXT("not observed"));Out.Shape=1;
        for(FProperty* P:S.Members){FValue V;if(!ReadPayload(P,P->ContainerPtrToValuePtr<void>(Address),V,Cells,Chars,Depth+1))return false;Out.Children.Add(MoveTemp(V));}return true;
    }
    bool ReadPayload(FProperty* P,const void* Address,FValue& Out,int32& Cells,int32& Chars,int32 Depth)
    {
        if(!Address||Depth>8||++Cells>16384)return PayloadFailure(TEXT("payload_value_bound"),TEXT("depth<=8 and <=16384 cells"),TEXT("exceeded"));
        if(ScalarKind(P)){if((P->GetClass()==FFloatProperty::StaticClass()||P->GetClass()==FDoubleProperty::StaticClass())&&!FMath::IsFinite(static_cast<FNumericProperty*>(P)->GetFloatingPointPropertyValue(Address)))return PayloadFailure(TEXT("payload_real_value"),TEXT("finite native real"),TEXT("nonfinite"));Out=Read(P,Address);if((P->GetClass()==FStrProperty::StaticClass()||P->GetClass()==FNameProperty::StaticClass())&&(Chars+=Out.String.Len())>65536)return PayloadFailure(TEXT("payload_text_bound"),TEXT("<=65536 copied characters"),LexToString(Chars));return true;}
        if(ByteEnumPayloadKind(P)){Out.Shape=6;Out.Integer=static_cast<FByteProperty*>(P)->GetPropertyValue(Address);return true;}
        if(ReferencePayloadKind(P))
        {
            const auto* Role=PayloadFields.FindByPredicate([&](const FPayloadField& E){return E.P==P;});if(!Role||!Role->ReferenceClass)return PayloadFailure(TEXT("reference_field_role"),TEXT("current held descriptor"),TEXT("missing"));
            if(P->GetClass()==FSoftObjectProperty::StaticClass())
            {
                const FSoftObjectPtr Value=*static_cast<const FSoftObjectPtr*>(Address);const FSoftObjectPath Path=Value.ToSoftObjectPath();Out.Shape=8;Out.String=Path.ToString();
                if(!Path.IsNull()){if(!AssetPath(Out.String,false))return PayloadFailure(TEXT("payload_soft_reference_path"),TEXT("exact runtime Game/Engine path"),Out.String);auto* Ref=StaticFindObject(nullptr,nullptr,*Out.String);if(!Loaded(Ref)||!Ref->IsA(Role->ReferenceClass))return PayloadReject(TEXT("datatable_payload_reference_load_pending"),TEXT("Resident compatible soft source required; no soft Get/Load."));if(!EnrollReference(Ref))return false;}return true;
            }
            const TObjectPtr<UObject> Slot=static_cast<FObjectProperty*>(P)->GetObjectPtrPropertyValue(Address);if(!Slot.IsResolved())return PayloadFailure(TEXT("payload_hard_reference_handle"),TEXT("resident or explicit null"),TEXT("unresolved"));
            auto* Ref=Slot.Get();Out.Shape=7;if(Ref){if(!Loaded(Ref))return PayloadFailure(TEXT("payload_hard_reference_current"),TEXT("loaded resident native asset"),TEXT("unavailable"));if(!Ref->IsA(Role->ReferenceClass))return PayloadFailure(TEXT("payload_hard_reference_class"),Role->ReferenceClass->GetPathName(),Ref->GetClass()->GetPathName());if(!EnrollReference(Ref))return false;Out.String=Ref->GetPathName();Out.Bits=UPTRINT(Ref);}return true;
        }
        if(P->GetClass()==FArrayProperty::StaticClass())
        {
            auto* A=static_cast<FArrayProperty*>(P);const auto* Storage=static_cast<const FScriptArray*>(Address);const int32 Num=Storage->Num(),Max=Storage->Max();const void* Data=Storage->GetData();
            if(Num<0||Num>4096||Max<Num||Max>65536||(Max>0&&!Data)||(Data&&UPTRINT(Data)%A->Inner->GetMinAlignment()!=0))return PayloadFailure(TEXT("payload_array_storage"),TEXT("bounded current default-allocator storage with aligned data"),LexToString(Num)+TEXT("/")+LexToString(Max));
            FScriptArrayHelper PayloadValues(A,Address);Out.Shape=2;for(int32 I=0;I<Num;++I){FValue V;if(!ReadPayload(A->Inner,PayloadValues.GetRawPtr(I),V,Cells,Chars,Depth+1))return false;Out.Children.Add(MoveTemp(V));}return true;
        }
        if(P->GetClass()==FSetProperty::StaticClass())
        {
            auto* S=static_cast<FSetProperty*>(P);const auto* Storage=static_cast<const FScriptSet*>(Address);const int32 Num=Storage->NumUnchecked(),Max=Storage->GetMaxIndex();if(Num<0||Num>4096||Max<Num||Max>65536)return PayloadFailure(TEXT("payload_set_storage"),TEXT("bounded current Num/max valid-index storage"),LexToString(Num)+TEXT("/")+LexToString(Max));
            FScriptSetHelper PayloadValues(S,Address);Out.Shape=3;int32 Observed=0;for(int32 I=0;I<Max;++I)if(PayloadValues.IsValidIndex(I)){const void* At=PayloadValues.GetElementPtr(I);if(!At||UPTRINT(At)%S->ElementProp->GetMinAlignment()!=0)return PayloadFailure(TEXT("payload_set_alignment"),TEXT("current aligned native element storage"),TEXT("unavailable"));FValue V;if(!ReadPayload(S->ElementProp,At,V,Cells,Chars,Depth+1))return false;Out.Children.Add(MoveTemp(V));++Observed;}if(Observed!=Num)return PayloadFailure(TEXT("payload_set_membership"),LexToString(Num),LexToString(Observed));Out.Children.Sort([](const FValue& A,const FValue& Z){return ValueLess(A,Z);});return true;
        }
        if(P->GetClass()==FMapProperty::StaticClass())
        {
            auto* M=static_cast<FMapProperty*>(P);const auto* Storage=static_cast<const FScriptMap*>(Address);const int32 Num=Storage->NumUnchecked(),Max=Storage->GetMaxIndex();if(Num<0||Num>4096||Max<Num||Max>65536)return PayloadFailure(TEXT("payload_map_storage"),TEXT("bounded current Num/max valid-index storage"),LexToString(Num)+TEXT("/")+LexToString(Max));
            FScriptMapHelper PayloadValues(M,Address);Out.Shape=4;int32 Observed=0;for(int32 I=0;I<Max;++I)if(PayloadValues.IsValidIndex(I)){const void* Key=PayloadValues.GetKeyPtr(I);const void* Value=PayloadValues.GetValuePtr(I);if(!Key||!Value||UPTRINT(Key)%M->KeyProp->GetMinAlignment()!=0||UPTRINT(Value)%M->ValueProp->GetMinAlignment()!=0)return PayloadFailure(TEXT("payload_map_alignment"),TEXT("current aligned key/value storage"),TEXT("unavailable"));FValue Pair,K,V;Pair.Shape=5;if(!ReadPayload(M->KeyProp,Key,K,Cells,Chars,Depth+1)||!ReadPayload(M->ValueProp,Value,V,Cells,Chars,Depth+1))return false;Pair.Children.Add(MoveTemp(K));Pair.Children.Add(MoveTemp(V));Out.Children.Add(MoveTemp(Pair));++Observed;}if(Observed!=Num)return PayloadFailure(TEXT("payload_map_membership"),LexToString(Num),LexToString(Observed));Out.Children.Sort([](const FValue& A,const FValue& Z){return ValueLess(A,Z);});return true;
        }
        const auto* Field=PayloadFields.FindByPredicate([&](const FPayloadField& E){return E.P==P;});auto* Role=Field?PayloadType(Field->Struct):nullptr;return Role?ReadStruct(*Role,Address,Out,Cells,Chars,Depth):PayloadFailure(TEXT("payload_struct_role"),TEXT("captured current UDS role"),TEXT("missing"));
    }
    bool ReadCell(FProperty* P,const void* Address,FValue& Out)
    {int32 Cells=0,Chars=0;return ReadPayload(P,Address,Out,Cells,Chars,0);}
    bool CapturePayloadDefaults()
    {
        if(!PayloadCurrent())return false;for(auto& Entry:PayloadStructs){int32 Cells=0,Chars=0;if(!ReadStruct(*Entry,Entry->Memory,Entry->Defaults,Cells,Chars,0))return false;}PayloadDefaultsReady=true;return PayloadCurrent();
    }
    bool PayloadInputSafe(FProperty* P,const TSharedPtr<FJsonValue>& Value,int32 Depth=0)
    {
        if(!Value.IsValid()||Depth>8)return false;if(Value->Type==EJson::Null)return true;
        if(ReferencePayloadKind(P))return ReferenceLiteral(P,Value);
        if(ScalarKind(P))
        {
            if(P->GetClass()==FNameProperty::StaticClass()&&Value->Type==EJson::String){const FString Name=Value->AsString();for(TCHAR C:Name)if(C==0)return false;if(Name.Len()>=NAME_SIZE)return false;}
            return !((P->GetClass()==FIntProperty::StaticClass()||P->GetClass()==FInt64Property::StaticClass())&&Value->Type==EJson::Number&&(!FMath::IsFinite(Value->AsNumber())||Value->AsNumber()>=9223372036854775808.0||Value->AsNumber()< -9223372036854775808.0));
        }
        if(ByteEnumPayloadKind(P))
        {
            // Stock converter retains named/authored-name lookup, numeric cast,
            // null/type errors and partial-prefix behavior. Only pre-crash bounds
            // are checked here; enum membership and conversion are not rewritten.
            if(Value->Type==EJson::String){const FString Name=Value->AsString();if(Name.Len()>=NAME_SIZE)return false;for(TCHAR C:Name)if(C==0)return false;}
            return Value->Type!=EJson::Number||(FMath::IsFinite(Value->AsNumber())&&Value->AsNumber()<9223372036854775808.0&&Value->AsNumber()>=-9223372036854775808.0);
        }
        if(P->GetClass()==FArrayProperty::StaticClass())
        {if(Value->Type!=EJson::Array)return true;const auto& PayloadValues=Value->AsArray();if(PayloadValues.Num()>4096)return false;for(const auto& V:PayloadValues)if(!PayloadInputSafe(static_cast<FArrayProperty*>(P)->Inner,V,Depth+1))return false;return true;}
        if(P->GetClass()==FSetProperty::StaticClass())
        {if(Value->Type!=EJson::Array)return true;const auto& PayloadValues=Value->AsArray();if(PayloadValues.Num()>4096)return false;for(const auto& V:PayloadValues)if(!PayloadInputSafe(static_cast<FSetProperty*>(P)->ElementProp,V,Depth+1))return false;return true;}
        if(P->GetClass()==FMapProperty::StaticClass())
        {if(Value->Type!=EJson::Object)return true;const auto Object=Value->AsObject();if(!Object.IsValid()||Object->Values.Num()>4096)return false;auto* M=static_cast<FMapProperty*>(P);for(const auto& Pair:Object->Values){const auto Key=MakeShared<FJsonValueString>(FString(Pair.Key.ToView()));if(!PayloadInputSafe(M->KeyProp,Key,Depth+1)||!PayloadInputSafe(M->ValueProp,Pair.Value,Depth+1))return false;}return true;}
        if(Value->Type!=EJson::Object)return true;const auto* Field=PayloadFields.FindByPredicate([&](const FPayloadField& E){return E.P==P;});const auto* Role=Field?PayloadType(Field->Struct):nullptr;if(!Role)return false;
        const auto Object=Value->AsObject();if(!Object.IsValid())return false;
        for(FProperty* Member:Role->Members)
        {
            const auto* MemberField=PayloadFields.FindByPredicate([&](const FPayloadField& E){return E.P==Member;});if(!MemberField)return false;
            // Match the same UDS authored name used by JsonAttributesToUStructWithContainer.
            auto V=Object->TryGetField(MemberField->Authored);if(V.IsValid()&&!PayloadInputSafe(Member,V,Depth+1))return false;
        }return true;
    }
    bool PayloadSourceSnapshot(TSharedRef<FJsonObject> Out,bool ReadDisk)
    {
        TArray<TSharedPtr<FJsonValue>> Sources;for(const auto& Entry:PayloadStructs)
        {
            if(!Raw(false))return false;const auto& E=*Entry;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),E.Path);J->SetStringField(TEXT("guid"),E.Guid.ToString());J->SetNumberField(TEXT("size"),E.Size);J->SetNumberField(TEXT("alignment"),E.Align);J->SetField(TEXT("default_values"),PayloadJson(E.Defaults));J->SetStringField(TEXT("editor_data"),E.DataPath);
            auto File=E.FileBefore;if(ReadDisk){File=MakeShared<FJsonObject>();if(!Disk(E.Package->GetName(),File)||!Raw(false)||!MirrorReadLiteralEqual(E.FileBefore,File))return PayloadFailure(TEXT("payload_source_file"),TEXT("captured saved nested UDS file"),TEXT("current file differs"),true,E.Path);}J->SetObjectField(TEXT("file"),File);Sources.Add(MakeShared<FJsonValueObject>(J));
        }Out->SetArrayField(TEXT("payload_struct_sources"),Sources);
        TArray<TSharedPtr<FJsonValue>> Enums;
        for(const auto& Entry:PayloadEnums)
        {
            const auto& E=*Entry;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),E.Path);J->SetStringField(TEXT("cpp_type"),E.CppType);J->SetBoolField(TEXT("user_defined"),E.UserDefined);J->SetBoolField(TEXT("flags_enum"),E.FlagsEnum);
            TArray<TSharedPtr<FJsonValue>> Entries;for(int32 I=0;I<E.Names.Num();++I){auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("raw_name"),E.Names[I].ToString());V->SetStringField(TEXT("value"),LexToString(E.Values[I]));V->SetStringField(TEXT("authored_name"),E.AuthoredNames[I]);Entries.Add(MakeShared<FJsonValueObject>(V));}J->SetArrayField(TEXT("entries"),Entries);
            if(E.UserDefined){auto File=E.FileBefore;if(ReadDisk){File=MakeShared<FJsonObject>();if(!Disk(E.Package->GetName(),File)||!Raw(false)||!MirrorReadLiteralEqual(E.FileBefore,File))return PayloadFailure(TEXT("payload_enum_source_file"),TEXT("captured saved enum file"),TEXT("current file differs"),true,E.Path);}J->SetObjectField(TEXT("file"),File);}
            Enums.Add(MakeShared<FJsonValueObject>(J));
        }
        if(!Enums.IsEmpty())Out->SetArrayField(TEXT("payload_byte_enum_sources"),Enums);return ReferenceCurrent(ReadDisk)&&Raw(false);
    }

    // RE: collection storage is observed by copied values, never retained element/buffer addresses.
    static bool PrimitiveArrayKind(const FProperty* P)
    {
        const auto* Array=CastField<FArrayProperty>(P);
        if(!Array||P->GetClass()!=FArrayProperty::StaticClass()||!Array->Inner||Array->ArrayFlags!=EArrayPropertyFlags::None||P->GetSize()!=sizeof(FScriptArray))return false;
        const auto* Inner=Array->Inner;
        return (Inner->GetClass()==FIntProperty::StaticClass()||Inner->GetClass()==FStrProperty::StaticClass())
            &&Inner->GetOwner<FProperty>()==P&&Inner->GetOwnerProperty()==P&&Inner->GetOwnerStruct()==P->GetOwnerStruct()
            &&Inner->ArrayDim==1&&!Inner->HasSetterOrGetter()&&!Inner->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated|CPF_EditorOnly|CPF_InstancedReference|CPF_ContainsInstancedReference)
            &&Inner->GetSize()==(Inner->GetClass()==FIntProperty::StaticClass()?sizeof(int32):sizeof(FString));
    }
    bool BindArrayField(FScalarField& F)
    {
        if(!PrimitiveArrayKind(F.Property))return Stop(TEXT("datatable_array_uds_inner_unsupported"),TEXT("Only owned int32/FString Array.Inner values are admitted; no nested/object/enum/container element or accessor."));
        auto* Array=CastFieldChecked<FArrayProperty>(F.Property);F.Inner=Array->Inner;F.InnerKind=F.Inner->GetClass();F.InnerName=F.Inner->GetFName();
        F.InnerSize=F.Inner->GetSize();F.InnerOffset=F.Inner->GetOffset_ForInternal();F.InnerFlags=F.Inner->GetPropertyFlags();F.ArrayFlags=Array->ArrayFlags;ArrayProfile=true;
        return true;
    }
    bool ArrayFieldCurrent(const FScalarField& F)
    {
        // The outer field has already been re-found and validated by ScalarFieldsCurrent.
        const auto* Array=CastField<FArrayProperty>(F.Property);
        if(!Array||Array->Inner!=F.Inner||Array->ArrayFlags!=F.ArrayFlags)return ScalarMismatch(TypePath,F.Name+TEXT(".Inner"),TEXT("captured actual Array.Inner binding/allocator flags"),TEXT("changed; old descriptor not read"));
        const FProperty* Inner=Array->Inner;
        if(!Inner||Inner->GetClass()!=F.InnerKind||Inner->GetOwner<FProperty>()!=F.Property||Inner->GetOwnerProperty()!=F.Property||Inner->GetOwnerStruct()!=Type.Get()
            ||Inner->GetFName()!=F.InnerName||Inner->ArrayDim!=1||Inner->GetSize()!=F.InnerSize||Inner->GetOffset_ForInternal()!=F.InnerOffset||Inner->GetPropertyFlags()!=F.InnerFlags||Inner->HasSetterOrGetter())
            return ScalarMismatch(TypePath,F.Name+TEXT(".Inner_role"),TEXT("captured owner/kind/name/layout/flags"),TEXT("changed"));
        return true;
    }
    bool ArrayValue(const FScalarField& F,const void* Address,TSharedPtr<FJsonValue>& Value,int32& Elements,int64& Characters)
    {
        // Raw(false) on RowValue entry closes source/root/Inner descriptors before this helper is constructed.
        FScriptArrayHelper Array(CastFieldChecked<FArrayProperty>(F.Property),Address);
        const int32 Num=Array.Num();if(Num<0||Num>4096||(Elements+=Num)>16384)return ScalarMismatch(TypePath,F.Name,TEXT("<=4096 items/array and <=16384 items/row"),LexToString(Num));
        TArray<TSharedPtr<FJsonValue>> REArrayValues;REArrayValues.Reserve(Num);
        for(int32 I=0;I<Num;++I)
        {
            // No SDK observer or writer occurs while this one current element address is in use.
            const uint8* Element=Array.GetRawPtr(I);
            if(F.InnerKind==FIntProperty::StaticClass())REArrayValues.Add(MakeShared<FJsonValueString>(LexToString(static_cast<const FIntProperty*>(F.Inner)->GetPropertyValue(Element))));
            else
            {
                const FString& TextValue=static_cast<const FStrProperty*>(F.Inner)->GetPropertyValue(Element);
                if(TextValue.Len()>MaxText||(Characters+=TextValue.Len())>MaxText)return ScalarMismatch(TypePath,F.Name+TEXT("[")+LexToString(I)+TEXT("]"),TEXT("bounded row string values"),LexToString(TextValue.Len()));
                REArrayValues.Add(MakeShared<FJsonValueString>(TextValue));
            }
        }
        Value=MakeShared<FJsonValueArray>(MoveTemp(REArrayValues));return true;
    }
    bool ArrayImportBounded(const TSharedPtr<FJsonObject>& Requested)
    {
        if(!ArrayProfile)return true;
        int32 Elements=0;
        for(const auto& F:ScalarFields)if(F.Inner)
        {
            // Same authored-name lookup as JsonAttributesToUStruct; no input-key normalization or rewrite.
            const FString Authored=Type->GetAuthoredNameForField(F.Property);if(!Raw())return false;
            const auto Value=Requested->TryGetField(Authored);
            if(!Value.IsValid()||Value->Type!=EJson::Array)continue; // null/nonarray/unknown retain the original converter/error prefix.
            const auto& Items=Value->AsArray();if(Items.Num()>4096||(Elements+=Items.Num())>16384)return Fail(R,TEXT("datatable_array_input_bound"),TEXT("First array profile is bounded to4096 items per field/16384 total; no Modify/import/save was dispatched."));
            if(F.InnerKind==FIntProperty::StaticClass())for(const auto& Item:Items)if(Item.IsValid()&&Item->Type==EJson::Number)
            {
                const double N=Item->AsNumber();
                // Installed JsonObjectConverter casts a JSON Number to int64 before the int32 property write.
                if(!FMath::IsFinite(N)||N<-9223372036854775808.0||N>=9223372036854775808.0)return Fail(R,TEXT("datatable_array_numeric_conversion_bound"),TEXT("JSON Number must be finite and in the defined int64 cast range; wider legacy numeric coercions remain incomplete. No input value is rewritten."));
            }
        }
        return StableSource();
    }

    bool ScalarFieldsCurrent()
    {
        auto* U=Type.Get();TArray<FProperty*> Current;
        for(TFieldIterator<FProperty> It(U,EFieldIteratorFlags::ExcludeSuper);It;++It)Current.Add(*It);
        if(Current.Num()!=ScalarFields.Num())return ScalarMismatch(TypePath,TEXT("field_count"),LexToString(ScalarFields.Num()),LexToString(Current.Num()));
        for(int32 I=0;I<ScalarFields.Num();++I)
        {
            const auto& F=ScalarFields[I];FProperty* P=Current[I];
            if(P!=F.Property)return ScalarMismatch(TypePath,F.Name+TEXT(".property_pointer"),FString::Printf(TEXT("%p"),F.Property),FString::Printf(TEXT("%p"),P));
            auto* Kind=P->GetClass();if(Kind!=F.Kind)return ScalarMismatch(TypePath,F.Name+TEXT(".kind"),FString::Printf(TEXT("%p"),F.Kind),FString::Printf(TEXT("%p"),Kind));
            auto* Owner=P->GetOwnerStruct();if(Owner!=U)return ScalarMismatch(TypePath,F.Name+TEXT(".owner"),FString::Printf(TEXT("%p"),U),FString::Printf(TEXT("%p"),Owner));
            const int32 Dim=P->ArrayDim;if(Dim!=1)return ScalarMismatch(TypePath,F.Name+TEXT(".array_dim"),TEXT("1"),LexToString(Dim));
            const bool Accessor=P->HasSetterOrGetter();if(Accessor)return ScalarMismatch(TypePath,F.Name+TEXT(".accessor"),TEXT("false"),TEXT("true"));
            const int32 CurrentOffset=P->GetOffset_ForInternal();if(CurrentOffset!=F.Offset)return ScalarMismatch(TypePath,F.Name+TEXT(".offset"),LexToString(F.Offset),LexToString(CurrentOffset));
            const int32 CurrentSize=P->GetSize();if(CurrentSize!=F.Size)return ScalarMismatch(TypePath,F.Name+TEXT(".size"),LexToString(F.Size),LexToString(CurrentSize));
            const auto CurrentFlags=P->GetPropertyFlags();if(CurrentFlags!=F.Flags)return ScalarMismatch(TypePath,F.Name+TEXT(".flags"),LexToString(static_cast<uint64>(F.Flags)),LexToString(static_cast<uint64>(CurrentFlags)));
            if(F.Inner&&!ArrayFieldCurrent(F))return false;
            if(F.NativeStruct){const auto* Struct=CastField<FStructProperty>(P);if(!Struct||!Struct->Struct.IsResolved()||Struct->Struct.Get()!=F.NativeStruct||P->GetSize()!=F.NativeStruct->GetStructureSize())return ScalarMismatch(TypePath,F.Name+TEXT(".native_struct"),F.NativeStructPath,TEXT("changed native math binding/size"));}
            const FName CurrentName=P->GetFName();if(CurrentName!=F.FieldName)return ScalarMismatch(TypePath,F.Name+TEXT(".name"),F.Name,CurrentName.ToString());
        }
        return true;
    }
    bool RowValue(const uint8* Memory,FString& Out,bool SourceAlreadyObserved=false)
    {
        if(!Memory)return Stop(TEXT("datatable_scalar_uds_row_memory_null"),TEXT("Current row/default memory is unavailable."));
        if(!ScalarProfile){Out=Property->GetPropertyValue_InContainer(Memory);return true;}
        // YA's Composite caller completes observation and reacquires the selected
        // row route before entry. The remaining copy path has no SDK callbacks.
        if(!SourceAlreadyObserved&&(ArrayProfile||CompositePayload)&&!Raw(false))return false;
        int32 ArrayElements=0,PayloadCells=0,PayloadChars=0;int64 ArrayCharacters=0;TArray<TSharedPtr<FJsonValue>> ValuesJson;
        for(const auto& F:ScalarFields)
        {
            const void* Address=F.Property->ContainerPtrToValuePtr<void>(Memory);TSharedPtr<FJsonValue> Value;
            if(F.Payload)
            {FValue Copied;if(!ReadPayload(F.Property,Address,Copied,PayloadCells,PayloadChars,0))return false;Value=PayloadJson(Copied);}
            else if(F.Inner)
            {if(!ArrayValue(F,Address,Value,ArrayElements,ArrayCharacters))return false;}
            else if(const auto* StringProperty=CastField<FStrProperty>(F.Property))
            {const FString& S=StringProperty->GetPropertyValue(Address);if(S.Len()>MaxText)return ScalarMismatch(TypePath,F.Name,TEXT("bounded FString"),LexToString(S.Len()));Value=MakeShared<FJsonValueString>(S);}
            else if(const auto* NameProperty=CastField<FNameProperty>(F.Property))Value=MakeShared<FJsonValueString>(NameProperty->GetPropertyValue(Address).ToString());
            else if(const auto* BoolProperty=CastField<FBoolProperty>(F.Property))Value=MakeShared<FJsonValueBoolean>(BoolProperty->GetPropertyValue(Address));
            else if(const auto* NumericProperty=CastField<FNumericProperty>(F.Property))
            {
                if(NumericProperty->IsInteger())
                {
                    // Preserve all integer bits in the guard, including values
                    // outside JSON's exact binary64 integer range.
                    Value=MakeShared<FJsonValueString>(LexToString(NumericProperty->GetSignedIntPropertyValue(Address)));
                }
                else
                {
                    const double N=NumericProperty->GetFloatingPointPropertyValue(Address);
                    if(!FMath::IsFinite(N))return ScalarMismatch(TypePath,F.Name,TEXT("finite real value"),LexToString(N));
                    Value=MakeShared<FJsonValueNumber>(N);
                }
            }
            else if(F.NativeStruct)
            {
                TArray<double> Components;
                if(F.NativeStructPath==TEXT("/Script/CoreUObject.Vector")){const FVector V=*static_cast<const FVector*>(Address);Components={V.X,V.Y,V.Z};}
                else if(F.NativeStructPath==TEXT("/Script/CoreUObject.Rotator")){const FRotator V=*static_cast<const FRotator*>(Address);Components={V.Pitch,V.Yaw,V.Roll};}
                else if(F.NativeStructPath==TEXT("/Script/CoreUObject.Transform")){const FTransform V=*static_cast<const FTransform*>(Address);if(V.ContainsNaN())return ScalarMismatch(TypePath,F.Name,TEXT("finite native Transform components"),TEXT("nonfinite Transform"));const FQuat Q=V.GetRotation();const FVector T=V.GetTranslation(),Scale=V.GetScale3D();Components={Q.X,Q.Y,Q.Z,Q.W,T.X,T.Y,T.Z,Scale.X,Scale.Y,Scale.Z};}
                else return ScalarMismatch(TypePath,F.Name,TEXT("exact held native math binding"),TEXT("unsupported math type"));
                TArray<TSharedPtr<FJsonValue>> Cells;for(double N:Components){if(!FMath::IsFinite(N))return ScalarMismatch(TypePath,F.Name,TEXT("finite native math components"),LexToString(N));Cells.Add(MakeShared<FJsonValueNumber>(N));}Value=MakeShared<FJsonValueArray>(Cells);
            }
            else return ScalarMismatch(TypePath,F.Name,TEXT("admitted scalar/native math property"),TEXT("unsupported current property kind"));
            if(ArrayElements+PayloadCells>16384)return PayloadFailure(TEXT("payload_row_cell_bound"),TEXT("<=16384 copied array/payload cells"),LexToString(ArrayElements+PayloadCells));
            ValuesJson.Add(MoveTemp(Value));
        }
        Out.Reset();if(!FJsonSerializer::Serialize(ValuesJson,TJsonWriterFactory<>::Create(&Out))||Out.Len()>MaxText)return Stop(TEXT("datatable_scalar_uds_value_bound"),TEXT("Current scalar cell values exceed the finite row observation bound."));return true;
    }
    bool ObserveScalarFields(bool CaptureGuid)
    {
        if(!Raw())return false;auto* U=Type.Get();
        // Installed UE58 GetVarDescPtr/GetGuidForProperty and the exact native
        // editor-data authored-name getter are readonly metadata lookups. Keep
        // held/source/layout fences at each call, with whole-row checks once
        // on entry/exit rather than once per field inside this pure loop.
        const auto* Descriptions=FStructureEditorUtils::GetVarDescPtr(static_cast<const UUserDefinedStruct*>(U));if(!Raw(false))return false;
        const auto& Current=static_cast<const UUserDefinedStructEditorData*>(EditorData)->VariablesDescriptions;
        if(Descriptions!=&Current||Current.Num()!=ScalarFields.Num())return ScalarMismatch(TypePath,TEXT("authored_descriptor_count"),LexToString(ScalarFields.Num()),LexToString(Current.Num()));
        TSet<FGuid> Seen;TSet<FString> AuthoredNames;
        for(auto& F:ScalarFields)
        {
            const FString Authored=F.Property->GetAuthoredName();if(!Raw(false))return false;
            const FGuid Guid=FStructureEditorUtils::GetGuidForProperty(F.Property);if(!Raw(false))return false;
            // GetVarDesc's returned address is never retained across another
            // SDK boundary. Re-read the exact held EditorData after Raw.
            const auto& CurrentDescriptions=static_cast<const UUserDefinedStructEditorData*>(EditorData)->VariablesDescriptions;
            const auto* Description=CurrentDescriptions.FindByPredicate([&](const FStructVariableDescription& V){return V.VarName==F.FieldName;});
            if(!Description||!Guid.IsValid()||Description->VarGuid!=Guid||Seen.Contains(Guid)||Authored.IsEmpty()||AuthoredNames.Contains(Authored))return ScalarMismatch(TypePath,F.Name+TEXT(".authored_guid"),TEXT("one unique current authored descriptor/name/GUID"),Guid.ToString());
            if(CaptureGuid){F.Guid=Guid;F.Authored=Authored;}
            else if(Guid!=F.Guid||!Authored.Equals(F.Authored,ESearchCase::CaseSensitive))return ScalarMismatch(TypePath,F.Name+TEXT(".authored_mapping"),F.Authored+TEXT(":")+F.Guid.ToString(),Authored+TEXT(":")+Guid.ToString());
            Seen.Add(Guid);AuthoredNames.Add(Authored);
        }
        return Raw();
    }
    bool ScalarSource(TSharedRef<FJsonObject>& J,bool ReadDisk)
    {
        if(!Raw()||!ObserveScalarFields(false))return false;auto* U=Type.Get();
        const auto* Super=U->GetSuperStruct();if(!Raw(false)||Super)return Stop(TEXT("datatable_scalar_uds_inherited_unsupported"),TEXT("Only own compiled scalar fields are admitted."));
        const uint8* Memory=U->GetDefaultInstance();if(!Raw(false))return false;
        if(!Memory||Memory!=DefaultMemory)return ScalarMismatch(TypePath,TEXT("default_instance_pointer"),FString::Printf(TEXT("%p"),DefaultMemory),FString::Printf(TEXT("%p"),Memory));
        FString Default;if(!RowValue(Memory,Default))return false;
        if(!Default.Equals(DefaultValue,ESearchCase::CaseSensitive))return ScalarRowMismatch(TEXT("default_scalar_values"),DefaultValue,Default);
        J->SetObjectField(TEXT("row_struct"),Identity(U));J->SetObjectField(TEXT("editor_data"),Identity(EditorData));J->SetStringField(TEXT("struct_guid"),StructGuid.ToString());J->SetNumberField(TEXT("row_struct_bytes"),StructBytes);J->SetStringField(TEXT("row_struct_flags"),LexToString(static_cast<uint32>(StructFlags)));J->SetStringField(TEXT("default_scalar_values_json"),Default);
        TArray<TSharedPtr<FJsonValue>> Fields;
        for(const auto& F:ScalarFields)
        {
            auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("name"),F.Name);V->SetStringField(TEXT("authored_name"),F.Authored);V->SetStringField(TEXT("field_guid"),F.Guid.ToString());
            const FString CppTypeName=F.Property->GetCPPType();if(!Raw(false))return false;V->SetStringField(TEXT("cpp_type"),CppTypeName);if(NativeMathProfile)V->SetStringField(TEXT("native_struct_path"),F.NativeStructPath);V->SetStringField(TEXT("class"),F.Kind->GetName());V->SetStringField(TEXT("flags"),LexToString(static_cast<uint64>(F.Flags)));V->SetNumberField(TEXT("offset"),F.Offset);V->SetNumberField(TEXT("size"),F.Size);if(F.Inner){auto A=MakeShared<FJsonObject>();A->SetStringField(TEXT("kind"),F.InnerKind->GetName());A->SetStringField(TEXT("name"),F.InnerName.ToString());A->SetStringField(TEXT("owner_property"),F.Name);A->SetStringField(TEXT("owner_struct"),TypePath);A->SetNumberField(TEXT("size"),F.InnerSize);A->SetNumberField(TEXT("offset"),F.InnerOffset);A->SetStringField(TEXT("flags"),LexToString(static_cast<uint64>(F.InnerFlags)));V->SetObjectField(TEXT("array_inner"),A);}Fields.Add(MakeShared<FJsonValueObject>(V));
        }
        J->SetArrayField(TEXT("fields"),Fields);if(ArrayProfile){J->SetBoolField(TEXT("primitive_array_profile"),true);J->SetNumberField(TEXT("max_array_items"),4096);}
        if(NativeMathProfile)J->SetObjectField(TEXT("native_math_structs"),NativeMathState());auto ObjectFields=MakeShared<FJsonObject>();J->SetObjectField(TEXT("source_fields"),ObjectFields);
        for(auto* O:SourceOwned){auto Item=MakeShared<FJsonObject>();ObjectFields->SetObjectField(O->GetPathName(),Item);Item->SetObjectField(TEXT("identity"),Identity(O));auto Props=MakeShared<FJsonObject>();Item->SetObjectField(TEXT("properties"),Props);if(!Properties(O,Props,nullptr,this))return Stopped?false:Stop(TEXT("datatable_scalar_uds_source_fields_unobservable"),TEXT("Source/editor-data fields exceed the finite observable scope."));}
        auto File=MakeShared<FJsonObject>();if(ReadDisk){if(!Disk(SourcePackage,File)||!File->GetBoolField(TEXT("exists")))return Stop(TEXT("datatable_scalar_uds_source_disk_unobservable"),TEXT("Saved source file snapshot required."));}else{const auto Stored=SourceBaseline->TryGetField(TEXT("disk"));if(!Stored.IsValid()||Stored->Type!=EJson::Object)return Stop(TEXT("datatable_scalar_uds_source_disk_baseline_missing"),TEXT("Saved source baseline unavailable."));File=Stored->AsObject().ToSharedRef();}J->SetObjectField(TEXT("disk"),File);
        if(CompositePayload&&!PayloadSourceSnapshot(J,ReadDisk))return false;
        return ObserveScalarFields(false)&&Raw();
    }
    bool BindScalarType(UUserDefinedStruct* U)
    {
        if(!Context()||!Loaded(U)||U->GetClass()!=UUserDefinedStruct::StaticClass()||U->GetOuter()!=U->GetPackage()||!AssetPath(U->GetPathName(),true)||U->GetPackage()->IsDirty()||U->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject)||U->Status!=UDSS_UpToDate||(U->StructFlags&STRUCT_Native)||!U->Guid.IsValid()||!U->PrimaryStruct.IsExplicitlyNull()||U->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn|PKG_EditorOnly|PKG_PlayInEditor))return Stop(TEXT("datatable_scalar_uds_source_unsupported"),TEXT("Loaded saved-clean compiled exact native UserDefinedStruct required; source creation/repair is not performed."));
        Type.Reset(U);TypePath=U->GetPathName();SourcePackage=U->GetPackage()->GetName();StructGuid=U->Guid;StructFlags=U->StructFlags;StructBytes=U->GetStructureSize();SourcePackageFlags=static_cast<uint32>(U->GetPackage()->GetPackageFlags());
        if(StructBytes<1||StructBytes>65536)return Stop(TEXT("datatable_scalar_uds_layout_bound"),TEXT("Compiled scalar row layout must be within 1..65536 bytes."));
        for(TFieldIterator<FProperty> It(U,EFieldIteratorFlags::ExcludeSuper);It;++It)
        {
            FProperty* P=*It;if(ScalarFields.Num()>=32||(!ScalarKind(P)&&!(AllowScalarCRUD&&MathKind(P))&&!(AllowScalarCRUD&&AllowPrimitiveArrays&&!AllowScalarGraph&&(PrimitiveArrayKind(P)||PayloadKind(P))))||P->GetOwnerStruct()!=U||P->ArrayDim!=1||P->HasSetterOrGetter()||!P->HasAnyPropertyFlags(CPF_Edit)||P->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated|CPF_EditorOnly|CPF_InstancedReference|CPF_ContainsInstancedReference)||P->GetOffset_ForInternal()<0||static_cast<int64>(P->GetOffset_ForInternal())+P->GetSize()>StructBytes)
                return Stop(TEXT("datatable_scalar_uds_field_unsupported"),TEXT("One to32 own editable scalar7/native math fields; existing-table CRUD also admits finite recursive UDS/scalar arrays/Sets/scalar-key Maps through actual reflected ownership/layout. Admitted resolved byte enum fields reuse the composite payload observer; wider FEnumProperty/object/custom operator fields remain incomplete."));
            FScalarField F;F.Property=P;F.Kind=P->GetClass();F.FieldName=P->GetFName();F.Name=P->GetName();F.Offset=P->GetOffset_ForInternal();F.Size=P->GetSize();F.Flags=P->GetPropertyFlags();if(MathKind(P)){auto* Struct=CastField<FStructProperty>(P);F.NativeStruct=Struct->Struct.Get();NativeFieldHolds.Emplace(F.NativeStruct);F.NativeStructPath=F.NativeStruct->GetPathName();NativeMathProfile=true;}if(P->GetClass()==FArrayProperty::StaticClass()&&PrimitiveArrayKind(P)){if(!BindArrayField(F))return false;}else if(!ScalarKind(P)&&!F.NativeStruct)F.Payload=true;F.Layout=F.Name+TEXT(" offset=")+LexToString(F.Offset)+TEXT(" size=")+LexToString(F.Size)+TEXT(" flags=")+LexToString(static_cast<uint64>(F.Flags));ScalarFields.Add(MoveTemp(F));
        }
        if(!BindNativeMath())return false;
        if(ScalarFields.IsEmpty())return Stop(TEXT("datatable_scalar_uds_field_unsupported"),TEXT("At least one own scalar field required."));
        const FString EditorPath=U->EditorData.GetPathName();EditorData=EditorPath.IsEmpty()?nullptr:StaticFindObject(UObject::StaticClass(),nullptr,*EditorPath);
        if(!Context()||!Loaded(U)||U->EditorData!=EditorData||!Loaded(EditorData)||!EditorData->IsIn(U)||!EditorData->GetClass()->HasAnyClassFlags(CLASS_Native)||EditorData->GetClass()->ClassGeneratedBy||!EditorData->GetClass()->GetPathName().Equals(TEXT("/Script/UnrealEd.UserDefinedStructEditorData"),ESearchCase::CaseSensitive))return Stop(TEXT("datatable_scalar_uds_editor_data_unsupported"),TEXT("Existing loaded stock UserDefinedStructEditorData required."));
        SourceOwned.Add(U);TArray<UObject*> Children;GetObjectsWithOuter(U,Children,EGetObjectsFlags::IncludeNestedObjects);for(auto* O:Children)SourceOwned.Add(O);
        for(auto* O:SourceOwned)if((O!=U&&O!=EditorData)||!Loaded(O)||O->GetPackage()!=U->GetPackage()||!O->GetClass()->HasAnyClassFlags(CLASS_Native)||O->GetClass()->ClassGeneratedBy||!Pin(O)||!Pin(O->GetClass()))return Stop(TEXT("datatable_scalar_uds_source_export_unsupported"),TEXT("Only current UDS and stock owned editor data are admitted."));
        if(!Pin(U->GetPackage()))return false;
        for(const auto& F:ScalarFields)if(F.Payload){if(!BindPayload(F.Property))return false;}
        PayloadReady=true;ScalarProfile=true;Active=true;if(!Raw()||!ObserveScalarFields(true))return false;
        if(CompositePayload&&!CapturePayloadDefaults())return false;
        // Route the historical SourceString-only profile back through its
        // original implementation and preserve its output/guard contract.
        if(ScalarFields.Num()==1&&ScalarFields[0].Kind==FStrProperty::StaticClass()&&ScalarFields[0].Authored.Equals(TEXT("SourceString"),ESearchCase::CaseSensitive))
        {ScalarProfile=false;AllowScalarCRUD=false;AllowScalarGraph=false;Active=false;ScalarFields.Reset();SourceOwned.Reset();return BindType(U);}
        DefaultMemory=U->GetDefaultInstance();if(!Raw()||!DefaultMemory)return Stop(TEXT("datatable_scalar_uds_default_unavailable"),TEXT("Existing compiled scalar default memory required; no source compile/recreation."));
        if(!RowValue(DefaultMemory,DefaultValue))return false;if(!ScalarSource(SourceBaseline,true))return false;
        if(AllowScalarGraph)
        {R.Data->SetBoolField(TEXT("scalar_uds_graph_admitted"),true);R.Data->SetStringField(TEXT("scalar_uds_graph_path"),TypePath);R.Data->SetNumberField(TEXT("scalar_uds_field_count"),ScalarFields.Num());R.Data->SetStringField(TEXT("scalar_uds_scope"),TEXT("Graph-only bounded compiled scalar UDS source observation; all authored fields/default/layout retained. No source writes/compile/save, CRUD dispatch, enum/nested/object/container admission or runtime proof."));return true;}
        if(NativeMathProfile){R.Data->SetBoolField(TEXT("native_math_uds_datatable_crud"),true);R.Data->SetStringField(TEXT("math_uds_scope_path"),TypePath);int32 MathCount=0;for(const auto& Field:ScalarFields)MathCount+=Field.NativeStruct!=nullptr;R.Data->SetNumberField(TEXT("math_uds_field_count"),MathCount);R.Data->SetBoolField(TEXT("math_uds_k2_graph_expanded"),false);R.Data->SetBoolField(TEXT("math_uds_runtime_verified"),false);R.Data->SetStringField(TEXT("math_uds_conversion_scope"),TEXT("Original authored-name Any JSON->stock JsonObjectConverter; native math may export as text. No request rewriting or field-specific import. Original save-before-requested-subset failure retained; converter success is not requested-subset or runtime success."));}
        if(ArrayProfile){R.Data->SetBoolField(TEXT("primitive_array_uds_datatable_crud"),true);R.Data->SetBoolField(TEXT("array_uds_k2_graph_expanded"),false);R.Data->SetBoolField(TEXT("array_uds_create_expanded"),true);R.Data->SetStringField(TEXT("array_uds_scope"),TEXT("Ordinary DataTable CRUD/read and fresh empty creation; saved-clean compiled UDS with own int32[]/FString[] mixed with prior scalar/native math fields. Max4096 per array/16384 per row. Actual Array.Inner binding/layout and copied ordered values/current source defaults retained; array buffers/elements are reacquired, never retained across SDK callbacks. Original JSON conversion, nonarray/null/coercion and partial/save-before-subset outcomes remain authoritative. No K2/other nested/container/runtime expansion."));}
        if(CompositePayload){R.Data->SetBoolField(TEXT("composite_uds_datatable_crud"),true);R.Data->SetBoolField(TEXT("composite_uds_create_expanded"),true);R.Data->SetBoolField(TEXT("composite_uds_k2_graph_expanded"),false);R.Data->SetNumberField(TEXT("composite_uds_source_count"),PayloadStructs.Num());R.Data->SetBoolField(TEXT("unordered_typed_slot_bags"),HashedPayload);R.Data->SetBoolField(TEXT("compiled_uds_array_payload"),StructArrayPayload);R.Data->SetStringField(TEXT("composite_uds_scope"),TEXT("Existing ordinary UDS DataTable schema/list/get/add-update/remove share the stock converter and writer. Finite acyclic saved-clean native-editor-data UDS, scalar7-or-UDS arrays, scalar7 Sets and scalar-key Maps to scalar/admitted-array/UDS values use owned current descriptors and freshly copied typed slots. Unordered copied bags retain multiplicity; hash/allocator/index layout is not authored order or uniqueness. Converter false retains actual new/default/partial row then stops without notify/save; converter true keeps notify/requested save before original JSON-subset predicate. Byte enum direct/nested/Array/Set/Map key/value payloads additionally retain exact native byte descriptors, held raw/authored enum source names/values and stored authored UserDefinedEnum DisplayNameMap; stock JSON converter remains authoritative. Fresh empty creation uses the same source/default/reference guards. No K2, unrestricted object/FEnumProperty/custom operator/native-struct-array/direct nested-container expansion, normalization/deduplication, source writes or runtime proof."));if(ArrayProfile)R.Data->SetStringField(TEXT("array_uds_scope"),TEXT("Historical direct int32/FString arrays retain their existing Inner/ordered-value guards; additional mixed fields use the separate concrete composite_uds_scope. Fresh empty creation is admitted; K2 remains unexpanded."));}
        if(ReferencePayload){R.Data->SetBoolField(TEXT("reference_uds_datatable_crud"),true);R.Data->SetStringField(TEXT("reference_uds_scope"),TEXT("Existing ordinary UDS DataTable schema/list/get/add-update/remove hard/soft object fields, Array/nested UDS and existing scalar-key Map values share the original converter/row writer. JSON null remains skip; None string clearing and row-local partial failures stay SDK-owned. Resident saved-clean distinct native Game/Engine assets and prewriter-enrolled readonly root/direct-reference identities/files are protected; copied pointers/soft paths are reacquired. Source loaders/soft Get/source writes/compile/save/runtime instances are not added. Cold/short aliases/generated/instanced/class/Set/reference keys/K2/full authored asset RAM purity and full original/runtime scope remain incomplete or usage validation pending."));}
        if(ByteEnumPayload){R.Data->SetBoolField(TEXT("byte_enum_uds_datatable_crud"),true);R.Data->SetNumberField(TEXT("byte_enum_source_count"),PayloadEnums.Num());R.Data->SetStringField(TEXT("byte_enum_scope"),TEXT("Existing UDS schema/list/get/add-update/remove, including nested/Array/Set/Map byte enums, reuse original authored-name converter/writer/partial/save order. Held exact FByteProperty.Enum sources preserve raw names/values/form/flags and stored authored UserDefinedEnum DisplayNameMap/description source strings. No enum compilation/repair or source writer, no per-slot disk scan, no bitflag-name normalization or enum-membership coercion. FEnumProperty, plain bytes, custom/native-other-module/generated/dirty sources and K2 remain incomplete; fresh ordinary table creation shares this admission. Actual new usage remains validation pending."));}
        R.Data->SetBoolField(TEXT("scalar_uds_crud_admitted"),true);R.Data->SetStringField(TEXT("scalar_uds_path"),TypePath);R.Data->SetNumberField(TEXT("scalar_uds_field_count"),ScalarFields.Num());
        R.Data->SetStringField(TEXT("scalar_uds_scope"),TEXT("Bounded compiled UDS scalar/native math and concrete recursive payload profiles for ordinary table CRUD and fresh empty creation; current source defaults/layout/authored GUIDs and ordered values or unordered typed slot bags retained. No source writes/compile/save, K2 widening, unrestricted enum/object/container/custom admission or runtime proof."));return true;
    }
    bool ImportNamesBounded(const TSharedPtr<FJsonObject>& Requested)
    {
        if(!ScalarProfile)return true;if(!StableSource()||!ArrayImportBounded(Requested))return false;
        for(const auto& F:ScalarFields)if(F.Kind==FNameProperty::StaticClass())
        {
            // This is the installed stock converter's authored-field lookup;
            // no key renaming/case normalization or JSON coercion is added.
            const FString Authored=Type->GetAuthoredNameForField(F.Property);if(!Raw())return false;
            const auto Value=Requested->TryGetField(Authored);if(!Value.IsValid()||Value->Type!=EJson::String)continue;
            const FString NameValue=Value->AsString();bool Nul=false;for(TCHAR C:NameValue)Nul|=C==0;
            if(Nul||NameValue.Len()>=NAME_SIZE)return Fail(R,TEXT("datatable_scalar_name_budget"),TEXT("Selected authored FName input exceeds the safe Engine name bound; no Modify/import/save was dispatched."));
        }
        if(CompositePayload)for(const auto& F:ScalarFields)if(F.Payload)
        {
            const auto Value=Requested->TryGetField(F.Authored);
            if(Value.IsValid()&&!PayloadInputSafe(F.Property,Value))return Stopped?false:Fail(R,TEXT("datatable_payload_input_bound"),TEXT("Selected finite payload exceeds the defined name/numeric/container bound; no Modify/import/save. Null/type mismatches retain stock converter semantics."));
        }
        return StableSource();
    }

    // ExcludeSuper iteration and direct field access do not call the UDS
    // authored-name/EditorData virtual methods. Re-find before dereferencing P.
    bool Raw(bool CheckRows=true)
    {
        if(!Context())return false;
        const auto PointerText=[](const void* P){return FString::Printf(TEXT("%p"),P);};
        for(const auto& I:Held)
        {
            auto* O=I.Object.Get();
            if(!Loaded(O))return IdentityChanged(I,TEXT("object_loaded"),TEXT("true"),TEXT("false"));
            if(!Loaded(I.Class))return IdentityChanged(I,TEXT("captured_class_loaded"),TEXT("true"),TEXT("false"));
            if(I.Outer&&!Loaded(I.Outer))return IdentityChanged(I,TEXT("captured_outer_loaded"),TEXT("true"),TEXT("false"));
            if(!Loaded(I.Package))return IdentityChanged(I,TEXT("captured_package_loaded"),TEXT("true"),TEXT("false"));
            const auto* ActualClass=O->GetClass();if(ActualClass!=I.Class)return IdentityChanged(I,TEXT("class_pointer"),PointerText(I.Class),PointerText(ActualClass));
            const auto* ActualOuter=O->GetOuter();if(ActualOuter!=I.Outer)return IdentityChanged(I,TEXT("outer_pointer"),PointerText(I.Outer),PointerText(ActualOuter));
            const auto* ActualPackage=O->GetPackage();if(ActualPackage!=I.Package)return IdentityChanged(I,TEXT("package_pointer"),PointerText(I.Package),PointerText(ActualPackage));
            const FString ActualPath=O->GetPathName();if(!ActualPath.Equals(I.Path,ESearchCase::CaseSensitive))return IdentityChanged(I,TEXT("object_path"),I.Path,ActualPath);
            const auto ActualFlags=O->GetFlags();if(ActualFlags!=I.Flags)return IdentityChanged(I,TEXT("object_flags"),LexToString(static_cast<uint32>(I.Flags)),LexToString(static_cast<uint32>(ActualFlags)));
            const auto ActualClassFlags=I.Class->GetClassFlags();if(ActualClassFlags!=I.ClassFlags)return IdentityChanged(I,TEXT("class_flags"),LexToString(static_cast<uint32>(I.ClassFlags)),LexToString(static_cast<uint32>(ActualClassFlags)));
            const int32 ActualClassBytes=I.Class->GetPropertiesSize();if(ActualClassBytes!=I.ClassBytes)return IdentityChanged(I,TEXT("class_properties_size"),LexToString(I.ClassBytes),LexToString(ActualClassBytes));
        }
        if(!NativeMathCurrent()||!ReferenceCurrent())return false;
        if((NativeMathProfile||ArrayProfile)&&!Active&&!ScalarFieldsCurrent())return false;
        if(!Active)return true;auto* U=Type.Get();
        if(!Loaded(U)||U->GetClass()!=UUserDefinedStruct::StaticClass()||U->Status!=UDSS_UpToDate||U->Guid!=StructGuid||!U->PrimaryStruct.IsExplicitlyNull()||U->EditorData!=EditorData||U->StructFlags!=StructFlags||U->GetStructureSize()!=StructBytes||U->GetPackage()->IsDirty()||static_cast<uint32>(U->GetPackage()->GetPackageFlags())!=SourcePackageFlags)return Stop(TEXT("datatable_string_uds_source_changed"),TEXT("Exact compiled String1 UDS source role/layout changed."));
        if(ScalarProfile){if(!ScalarFieldsCurrent()||(PayloadReady&&CompositePayload&&!PayloadCurrent()))return false;}
        else
        {
        FProperty* Current=nullptr;int32 Count=0;for(TFieldIterator<FProperty> It(U,EFieldIteratorFlags::ExcludeSuper);It;++It){Current=*It;++Count;}
        if(Count!=1||Current!=Property||!CastField<FStrProperty>(Current)||Current->GetOwnerStruct()!=U||Current->ArrayDim!=1||Current->GetOffset_ForInternal()!=Offset||Current->GetPropertyFlags()!=PropertyFlags||!Current->GetName().Equals(InternalName,ESearchCase::CaseSensitive))return Stop(TEXT("datatable_string_uds_field_changed"),TEXT("Current exact FString field no longer matches the captured owner/name/layout."));
        }
        if(!Bound)return true;auto* T=Table.Get();
        if(!Loaded(T)||T->GetClass()!=UDataTable::StaticClass()||T->RowStruct!=U||!T->GetPathName().Equals(TablePath,ESearchCase::CaseSensitive)||T->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn|PKG_EditorOnly|PKG_PlayInEditor))return Stop(TEXT("datatable_string_uds_table_role_changed"),TEXT("Selected native table/UDS routing changed."));
        TArray<UObject*> CurrentOwned;GetObjectsWithOuter(T,CurrentOwned,EGetObjectsFlags::IncludeNestedObjects);
        if(CurrentOwned.Num()!=Owned.Num())return Stop(TEXT("datatable_string_uds_owned_changed"),TEXT("Selected table owned export set changed."));for(auto* O:Owned)if(!CurrentOwned.Contains(O))return Stop(TEXT("datatable_string_uds_owned_changed"),TEXT("Selected table owned export identity changed."));
        if(TrackModifiedMetadata)
        {
            const FString ActualName=T->RowStructName_DEPRECATED.ToString(),ExpectedName=ModifiedRowStructName.ToString();
            const FString ActualPath=T->RowStructPathName.IsNull()?TEXT("None"):T->RowStructPathName.ToString();
            const FString ExpectedPath=ModifiedRowStructPath.IsNull()?TEXT("None"):ModifiedRowStructPath.ToString();
            for(const auto& I:Held)if(I.Object.Get()==T)
            {
                if(!ActualName.Equals(ExpectedName,ESearchCase::CaseSensitive))return IdentityChanged(I,TEXT("RowStructName"),ExpectedName,ActualName);
                if(!ActualPath.Equals(ExpectedPath,ESearchCase::CaseSensitive))return IdentityChanged(I,TEXT("RowStructPathName"),ExpectedPath,ActualPath);
            }
        }
        if(CheckRows){const auto& Map=T->GetRowMap();if(Map.Num()!=Rows.Num())return Stop(TEXT("datatable_string_uds_rows_changed"),TEXT("Current RowMap count changed outside planned row transition."));int32 I=0;for(const auto& P:Map){if(I>=Rows.Num()||Rows[I].Key!=P.Key||Rows[I].Value!=P.Value||!P.Value)return Stop(TEXT("datatable_string_uds_rows_changed"),TEXT("Current ordered key/storage routing changed; no stale row read."));const auto* V=Values.Find(P.Key);FString Actual;if(!RowValue(P.Value,Actual))return false;if(!V||!Actual.Equals(*V,ESearchCase::CaseSensitive))return ScalarProfile?ScalarRowMismatch(P.Key.ToString(),V?*V:TEXT("captured current row"),Actual):Stop(TEXT("datatable_string_uds_values_changed"),TEXT("Current row value changed outside explicit import."));++I;}}
        return true;
    }
    bool ObserveAuthoredFieldGuid(FGuid& OutGuid,const TCHAR* Phase)
    {
        if(!Raw())return false;auto* U=Type.Get();
        // Runtime PropertyIDs are not the current editor-authored VarGuid authority.
        auto Observation=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("string1_uds_field_guid_observation"),Observation);
        Observation->SetStringField(TEXT("phase"),Phase);Observation->SetStringField(TEXT("property_name"),InternalName);Observation->SetBoolField(TEXT("runtime_property_ids_used_for_admission"),false);
        Observation->SetStringField(TEXT("admitted_guid"),FieldGuid.ToString());Observation->SetBoolField(TEXT("admitted_guid_valid"),FieldGuid.IsValid());
        const FGuid RuntimeGuid=U->FindPropertyGuidFromName(Property->GetFName());
        Observation->SetBoolField(TEXT("runtime_lookup_returned"),true);Observation->SetStringField(TEXT("runtime_property_ids_guid"),RuntimeGuid.ToString());Observation->SetBoolField(TEXT("runtime_property_ids_guid_valid"),RuntimeGuid.IsValid());if(!Raw())return false;
        Observation->SetBoolField(TEXT("authored_lookup_attempted"),true);const FGuid QueriedGuid=FStructureEditorUtils::GetGuidForProperty(Property);
        Observation->SetBoolField(TEXT("authored_lookup_returned"),true);Observation->SetStringField(TEXT("authored_guid"),QueriedGuid.ToString());Observation->SetBoolField(TEXT("authored_guid_valid"),QueriedGuid.IsValid());if(!Raw())return false;
        Observation->SetBoolField(TEXT("const_descriptor_lookup_attempted"),true);
        const auto* ReturnedDescriptions=FStructureEditorUtils::GetVarDescPtr(static_cast<const UUserDefinedStruct*>(U));
        Observation->SetBoolField(TEXT("const_descriptor_lookup_returned"),true);if(!Raw())return false;
        // Compare the returned pointer only. Copy values from the current retained
        // exact native EditorData after Raw, before any next SDK boundary.
        const auto& CurrentDescriptions=static_cast<const UUserDefinedStructEditorData*>(EditorData)->VariablesDescriptions;
        const bool CurrentArrayMatches=ReturnedDescriptions==&CurrentDescriptions;const int32 AuthoredCount=CurrentDescriptions.Num();
        Observation->SetBoolField(TEXT("current_descriptor_array_matches"),CurrentArrayMatches);Observation->SetNumberField(TEXT("authored_descriptor_count"),AuthoredCount);
        if(!CurrentArrayMatches||!QueriedGuid.IsValid()||AuthoredCount!=1)return Stop(TEXT("datatable_string_uds_field_guid_unavailable"),TEXT("One current editor-authored descriptor with a valid VarGuid is required; runtime PropertyIDs are not the authored GUID authority."));
        const FName CurrentVarName=CurrentDescriptions[0].VarName;const FGuid CurrentVarGuid=CurrentDescriptions[0].VarGuid;
        Observation->SetStringField(TEXT("descriptor_var_name"),CurrentVarName.ToString());Observation->SetStringField(TEXT("descriptor_var_guid"),CurrentVarGuid.ToString());
        const bool Matches=CurrentVarName==Property->GetFName()&&CurrentVarGuid==QueriedGuid&&CurrentVarGuid.IsValid();Observation->SetBoolField(TEXT("authored_mapping_matches_current_property"),Matches);
        if(!Matches)return Stop(TEXT("datatable_string_uds_field_guid_unavailable"),TEXT("Current authored descriptor must match the exact retained FString property/name/GUID."));
        OutGuid=CurrentVarGuid;return Raw();
    }
    bool Source(TSharedRef<FJsonObject>& J,bool ReadDisk=false)
    {
        if(ScalarProfile)return ScalarSource(J,ReadDisk);
        if(!Raw())return false;FGuid CurrentGuidBefore;if(!ObserveAuthoredFieldGuid(CurrentGuidBefore,TEXT("source_before")))return false;
        if(CurrentGuidBefore!=FieldGuid)return Stop(TEXT("datatable_string_uds_field_guid_changed"),TEXT("Current authored GUID changed from the admitted field before source observation."));auto* U=Type.Get();
        const auto* Super=U->GetSuperStruct();if(!Raw()||Super)return Stop(TEXT("datatable_string_uds_inherited_unsupported"),TEXT("No inherited UDS fields are admitted."));
        const FString Authored=Property->GetAuthoredName();if(!Raw()||!Authored.Equals(TEXT("SourceString"),ESearchCase::CaseSensitive))return Stop(TEXT("datatable_string_uds_authored_name_changed"),TEXT("Exact authored SourceString field required."));
        const uint8* Memory=U->GetDefaultInstance();if(!Raw()||!Memory||Memory!=DefaultMemory)return Stop(TEXT("datatable_string_uds_default_changed"),TEXT("Existing current source default instance required; no recreation."));
        const FString Default=Property->GetPropertyValue_InContainer(Memory);if(!Default.Equals(DefaultValue,ESearchCase::CaseSensitive))return Stop(TEXT("datatable_string_uds_default_changed"),TEXT("Source FString default changed."));
        J->SetObjectField(TEXT("row_struct"),Identity(U));J->SetObjectField(TEXT("editor_data"),Identity(EditorData));J->SetStringField(TEXT("struct_guid"),StructGuid.ToString());J->SetStringField(TEXT("field_guid"),FieldGuid.ToString());J->SetNumberField(TEXT("row_struct_bytes"),StructBytes);J->SetStringField(TEXT("row_struct_flags"),LexToString(static_cast<uint32>(StructFlags)));J->SetStringField(TEXT("default_value"),Default);
        auto Field=MakeShared<FJsonObject>();Field->SetStringField(TEXT("name"),InternalName);Field->SetStringField(TEXT("authored_name"),Authored);Field->SetStringField(TEXT("cpp_type"),TEXT("FString"));Field->SetStringField(TEXT("class"),TEXT("StrProperty"));Field->SetStringField(TEXT("flags"),LexToString(PropertyFlags));Field->SetNumberField(TEXT("offset"),Offset);J->SetArrayField(TEXT("fields"),{MakeShared<FJsonValueObject>(Field)});
        auto ObjectFields=MakeShared<FJsonObject>();J->SetObjectField(TEXT("source_fields"),ObjectFields);
        for(auto* O:SourceOwned){auto Item=MakeShared<FJsonObject>();ObjectFields->SetObjectField(O->GetPathName(),Item);Item->SetObjectField(TEXT("identity"),Identity(O));auto Props=MakeShared<FJsonObject>();Item->SetObjectField(TEXT("properties"),Props);if(!Properties(O,Props,nullptr,this))return Stopped?false:Stop(TEXT("datatable_string_uds_source_fields_unobservable"),TEXT("Source field export failed or exceeded its finite bound."));}
        auto File=MakeShared<FJsonObject>();if(ReadDisk){if(!Disk(SourcePackage,File)||!File->GetBoolField(TEXT("exists")))return Stop(TEXT("datatable_string_uds_source_disk_unobservable"),TEXT("Saved source file snapshot unavailable."));}else{const auto Stored=SourceBaseline->TryGetField(TEXT("disk"));if(!Stored.IsValid()||Stored->Type!=EJson::Object)return Stop(TEXT("datatable_string_uds_source_disk_baseline_missing"),TEXT("Source disk baseline unavailable."));File=Stored->AsObject().ToSharedRef();}J->SetObjectField(TEXT("disk"),File);FGuid CurrentGuidAfter;if(!ObserveAuthoredFieldGuid(CurrentGuidAfter,TEXT("source_after")))return false;
        if(CurrentGuidAfter!=FieldGuid)return Stop(TEXT("datatable_string_uds_field_guid_changed"),TEXT("Current authored GUID changed from the admitted field during source observation."));return Raw();
    }
    bool StableSource(bool CheckRows=true,bool ReadDisk=false)
    {
        if(!Raw(CheckRows))return false;
        // Source() needs the current admitted row route. Callers accepting a
        // helper/import transition first adopt only the proven current rows.
        auto Now=MakeShared<FJsonObject>();if(!Source(Now,ReadDisk))return false;
        return MirrorReadLiteralEqual(SourceBaseline,Now)||Stop(TEXT("datatable_string_uds_source_fields_changed"),TEXT("Source UDS/default/editor-data/disk changed; no next writer/save."));
    }
    bool BindType(UUserDefinedStruct* U)
    {
        if(AllowScalarCRUD||AllowScalarGraph)return BindScalarType(U);
        if(!Context()||!Loaded(U)||U->GetClass()!=UUserDefinedStruct::StaticClass()||U->GetOuter()!=U->GetPackage()||!AssetPath(U->GetPathName(),true)||U->GetPackage()->IsDirty()||U->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject)||U->Status!=UDSS_UpToDate||(U->StructFlags&STRUCT_Native)||!U->Guid.IsValid()||!U->PrimaryStruct.IsExplicitlyNull()||U->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn|PKG_EditorOnly|PKG_PlayInEditor))return Stop(TEXT("datatable_string_uds_source_unsupported"),TEXT("Loaded saved-clean compiled exact native UUserDefinedStruct asset required."));
        Type.Reset(U);TypePath=U->GetPathName();SourcePackage=U->GetPackage()->GetName();StructGuid=U->Guid;StructFlags=U->StructFlags;StructBytes=U->GetStructureSize();SourcePackageFlags=static_cast<uint32>(U->GetPackage()->GetPackageFlags());
        if(StructBytes<static_cast<int32>(sizeof(FString))||StructBytes>65536)return Stop(TEXT("datatable_string_uds_layout_unsupported"),TEXT("Bounded FString row layout required."));
        int32 Count=0;for(TFieldIterator<FProperty> It(U,EFieldIteratorFlags::ExcludeSuper);It;++It){++Count;Property=CastField<FStrProperty>(*It);}
        if(Count!=1||!Property||Property->GetOwnerStruct()!=U||Property->ArrayDim!=1||!Property->HasAnyPropertyFlags(CPF_Edit)||Property->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated|CPF_EditorOnly|CPF_InstancedReference|CPF_ContainsInstancedReference)||Property->GetOffset_ForInternal()<0||Property->GetOffset_ForInternal()+Property->GetSize()>StructBytes)return Stop(TEXT("datatable_string_uds_field_unsupported"),TEXT("One own editable ArrayDim1 FString field only; nested/object/container variants remain incomplete."));
        InternalName=Property->GetName();Offset=Property->GetOffset_ForInternal();PropertyFlags=static_cast<uint64>(Property->GetPropertyFlags());
        const FString EditorPath=U->EditorData.GetPathName();EditorData=EditorPath.IsEmpty()?nullptr:StaticFindObject(UObject::StaticClass(),nullptr,*EditorPath);
        if(!Context()||!Loaded(U)||U->EditorData!=EditorData||!Loaded(EditorData)||!EditorData->IsIn(U)||!EditorData->GetClass()->HasAnyClassFlags(CLASS_Native)||EditorData->GetClass()->ClassGeneratedBy||!EditorData->GetClass()->GetPathName().Equals(TEXT("/Script/UnrealEd.UserDefinedStructEditorData"),ESearchCase::CaseSensitive))return Stop(TEXT("datatable_string_uds_editor_data_unsupported"),TEXT("Existing loaded native editor data required; no hidden editor data creation."));
        SourceOwned.Add(U);TArray<UObject*> Children;GetObjectsWithOuter(U,Children,EGetObjectsFlags::IncludeNestedObjects);for(auto* O:Children)SourceOwned.Add(O);
        for(auto* O:SourceOwned)if((O!=U&&O!=EditorData)||!Loaded(O)||O->GetPackage()!=U->GetPackage()||!O->GetClass()->HasAnyClassFlags(CLASS_Native)||O->GetClass()->ClassGeneratedBy||!Pin(O)||!Pin(O->GetClass()))return Stop(TEXT("datatable_string_uds_source_export_unsupported"),TEXT("Bounded native same-package UDS owned exports required."));
        if(!Pin(U->GetPackage()))return false;Active=true;if(!Raw())return false;
        DefaultMemory=U->GetDefaultInstance();if(!Raw()||!DefaultMemory)return Stop(TEXT("datatable_string_uds_default_unavailable"),TEXT("Existing compiled default memory required; no compile/recreate."));const FString& DefaultView=Property->GetPropertyValue_InContainer(DefaultMemory);if(DefaultView.Len()>MaxText)return Stop(TEXT("datatable_string_uds_default_text_bound"),TEXT("Source default string exceeds finite text bound."));DefaultValue=DefaultView;
        if(!ObserveAuthoredFieldGuid(FieldGuid,TEXT("bind_type")))return false;
        if(!Source(SourceBaseline,true))return false;
        R.Data->SetBoolField(TEXT("string1_uds_admitted"),true);R.Data->SetStringField(TEXT("string1_uds_path"),TypePath);R.Data->SetStringField(TEXT("string1_uds_scope"),TEXT("Exact saved-clean SourceString:FString1 source; no source writes, schema repair, generic UDS fields, implicit compile or source save. Stock JSON key/coercion/partial-save semantics remain."));return true;
    }
    bool BindTable(UDataTable* T,const FString& RequestedPath,const FString& RequestedPackage)
    {
        if(!StableSource()||!Loaded(T)||T->GetClass()!=UDataTable::StaticClass()||!Loaded(T->GetPackage())||T->GetOuter()!=T->GetPackage()
            ||!T->GetPathName().Equals(RequestedPath,ESearchCase::CaseSensitive)||!T->GetPackage()->GetName().Equals(RequestedPackage,ESearchCase::CaseSensitive)
            ||T->RowStruct!=Type.Get()||T->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject)||!AssetPath(RequestedPath,true)||T->GetRowMap().Num()>MaxRowsObserved||T->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn|PKG_EditorOnly|PKG_PlayInEditor))return Stop(TEXT("datatable_string_uds_table_unsupported"),TEXT("Ordinary native selected /Game DataTable must retain the exact requested path/direct package and admitted String1 UDS."));
        Table.Reset(T);TablePath=RequestedPath;SelectedPackage=RequestedPackage;if(!Pin(T)||!Pin(T->GetPackage())||!Pin(T->GetClass()))return false;
        GetObjectsWithOuter(T,Owned,EGetObjectsFlags::IncludeNestedObjects);for(auto* O:Owned)if(!Loaded(O)||!O->GetClass()->GetPathName().Equals(TEXT("/Script/Engine.AssetImportData"),ESearchCase::CaseSensitive)||O->GetOuter()!=T||!O->IsIn(T)||O->GetPackage()!=T->GetPackage()||!O->GetClass()->HasAnyClassFlags(CLASS_Native)||O->GetClass()->ClassGeneratedBy||!Pin(O))return Stop(TEXT("datatable_string_uds_target_export_unsupported"),TEXT("Finite exact native table-owned exports required."));
        int64 RowChars=0;for(const auto& P:T->GetRowMap()){if(P.Key.IsNone()||!P.Value)return Stop(TEXT("datatable_string_uds_row_invalid"),TEXT("Bounded non-null current row storage required."));FString V;if(!RowValue(P.Value,V))return false;if((RowChars+=V.Len()+P.Key.ToString().Len())>MaxText)return Stop(TEXT("datatable_string_uds_row_text_bound"),TEXT("Current row values exceed finite capture bound."));Rows.Emplace(P.Key,P.Value);Values.Add(P.Key,V);}
        Bound=true;return StableSource();
    }
    bool AcceptRows(FName Selected,bool Remove,const uint8* Returned,bool Imported=false)
    {
        if(!Raw(false))return false;auto* T=Table.Get();const auto& Map=T->GetRowMap();const bool Existed=Values.Contains(Selected);if(Map.Num()!=Rows.Num()+(Imported?0:Remove?-1:1))return Stop(TEXT("datatable_string_uds_transition_count"),TEXT("Only the planned selected row transition may occur."));
        TArray<TPair<FName,const uint8*>> Next;int32 Index=0;FString SelectedValue;
        for(const auto& P:Map){if(!P.Value)return Stop(TEXT("datatable_string_uds_transition_storage"),TEXT("Current row storage is null."));if(Remove&&Index<Rows.Num()&&Rows[Index].Key==Selected)++Index;
            if(P.Key==Selected&&!Remove){if(Returned!=P.Value)return Stop(TEXT("datatable_string_uds_transition_storage"),TEXT("Returned selected storage is no longer current."));if(Imported){if(Index>=Rows.Num()||Rows[Index].Key!=P.Key||Rows[Index].Value!=P.Value)return Stop(TEXT("datatable_string_uds_import_routing"),TEXT("Import replaced selected row storage."));++Index;}else if(Existed||Index!=Rows.Num())return Stop(TEXT("datatable_string_uds_transition_order"),TEXT("New row was not appended once."));if(!RowValue(P.Value,SelectedValue))return false;if(!Imported&&!SelectedValue.Equals(DefaultValue,ESearchCase::CaseSensitive))return Stop(TEXT("datatable_string_uds_new_default_changed"),TEXT("Fresh helper row default differs from the source default."));}
            else{const auto* V=Values.Find(P.Key);if(Index>=Rows.Num()||Rows[Index].Key!=P.Key||Rows[Index].Value!=P.Value||!P.Value||!V)return Stop(TEXT("datatable_string_uds_retained_row_changed"),TEXT("Unselected ordered row pointer/value changed across helper/import."));FString Actual;if(!RowValue(P.Value,Actual))return false;if(!Actual.Equals(*V,ESearchCase::CaseSensitive))return ScalarProfile?ScalarRowMismatch(P.Key.ToString(),*V,Actual):Stop(TEXT("datatable_string_uds_retained_row_changed"),TEXT("Unselected ordered row pointer/value changed across helper/import."));++Index;}Next.Emplace(P.Key,P.Value);}
        if(Remove){while(Index<Rows.Num()&&Rows[Index].Key==Selected)++Index;if(Map.Contains(Selected)||Index!=Rows.Num())return Stop(TEXT("datatable_string_uds_remove_transition"),TEXT("Selected retirement/retained row order incomplete."));Values.Remove(Selected);}else{if(!Map.Contains(Selected)||Index!=Rows.Num())return Stop(TEXT("datatable_string_uds_import_transition"),TEXT("Selected import/append transition incomplete."));Values.Add(Selected,SelectedValue);}Rows=MoveTemp(Next);auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("row_name"),Selected.ToString());Receipt->SetBoolField(TEXT("exists"),!Remove);Receipt->SetBoolField(TEXT("import_observed"),Imported);Receipt->SetNumberField(TEXT("row_count"),Rows.Num());if(!Remove)Receipt->SetStringField(ScalarProfile?TEXT("scalar_values_json"):TEXT("sourceString"),SelectedValue);R.Data->SetObjectField(Imported?TEXT("string_uds_import_prefix"):TEXT("string_uds_row_helper_prefix"),Receipt);return StableSource();
    }
    bool AfterCreateDirty(UPackage* FreshPackage,bool WasDirty,ITransaction* Undo,bool Returned)
    {
        if(!Context())return false;
        for(auto& I:Held)
        {
            auto* O=I.Object.Get();if(O!=FreshPackage||!Loaded(O)||O->GetFlags()==I.Flags)continue;
            const uint32 Before=static_cast<uint32>(I.Flags),After=static_cast<uint32>(O->GetFlags()),Bit=static_cast<uint32>(RF_Transactional);
            const bool ExactFreshRole=R.Operation==TEXT("create_datatable")&&R.Attempted&&Bound&&Loaded(Table.Get())
                &&I.Class==UPackage::StaticClass()&&I.Outer==nullptr&&I.Package==FreshPackage&&I.Path.Equals(SelectedPackage,ESearchCase::CaseSensitive)
                &&O->GetClass()==I.Class&&O->GetOuter()==I.Outer&&O->GetPackage()==I.Package&&O->GetPathName().Equals(I.Path,ESearchCase::CaseSensitive)
                &&Table->GetClass()==UDataTable::StaticClass()&&Table->GetOuter()==FreshPackage&&Table->GetPackage()==FreshPackage
                &&Table->GetPathName().Equals(TablePath,ESearchCase::CaseSensitive)&&Rows.IsEmpty()&&Values.IsEmpty()
                &&!SelectedPackage.Equals(SourcePackage,ESearchCase::CaseSensitive)
                &&!FreshPackage->HasAnyPackageFlags(PKG_CompiledIn|PKG_ContainsScript|PKG_PlayInEditor|PKG_EditorOnly);
            if(!ExactFreshRole||WasDirty||!Returned||!Undo||!TrackUndo||ExpectedUndo!=Undo||GUndo!=Undo||!FreshPackage->IsDirty()
                ||Before!=static_cast<uint32>(RF_Public)||After!=(Before|Bit))return Raw();
            // UPackage::SetDirtyFlag adds only RF_Transactional for this exact
            // fresh public package under the same live transaction. No writer,
            // source role, arbitrary flag delta or earlier unknown is adopted.
            I.Flags=static_cast<EObjectFlags>(After);auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("package"),I.Path);
            J->SetStringField(TEXT("before_flags"),LexToString(Before));J->SetStringField(TEXT("after_flags"),LexToString(After));
            J->SetBoolField(TEXT("was_dirty"),WasDirty);J->SetBoolField(TEXT("mark_returned"),Returned);J->SetBoolField(TEXT("same_transaction"),true);
            R.Data->SetObjectField(TEXT("string_uds_create_package_transactional_transition"),J);
        }
        return StableSource();
    }
    bool AfterModifyDerivedMetadata(FName BeforeName,FTopLevelAssetPath BeforePath,ITransaction* Undo,bool Returned)
    {
        if(!Raw())return false;
        const FName AfterName=Table->RowStructName_DEPRECATED;const FTopLevelAssetPath AfterPath=Table->RowStructPathName;
        const FString BeforeNameText=BeforeName.ToString(),AfterNameText=AfterName.ToString();
        const FString BeforePathText=BeforePath.IsNull()?TEXT("None"):BeforePath.ToString(),AfterPathText=AfterPath.IsNull()?TEXT("None"):AfterPath.ToString();
        const bool NameChanged=!BeforeNameText.Equals(AfterNameText,ESearchCase::CaseSensitive),PathChanged=!BeforePathText.Equals(AfterPathText,ESearchCase::CaseSensitive);
        auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("modify_returned"),Returned);J->SetStringField(TEXT("before_name"),BeforeNameText);J->SetStringField(TEXT("after_name"),AfterNameText);
        J->SetStringField(TEXT("before_path"),BeforePathText);J->SetStringField(TEXT("after_path"),AfterPathText);J->SetBoolField(TEXT("changed"),NameChanged||PathChanged);
        R.Data->SetObjectField(TEXT("string_uds_modify_row_struct_metadata"),J);
        if(NameChanged||PathChanged)
        {
            const FString CurrentName=Type->GetFName().ToString(),CurrentPath=Type->GetStructPathName().ToString();
            const bool Qualified=Bound&&Table->GetClass()==UDataTable::StaticClass()&&Table->RowStruct==Type.Get()
                &&(R.Operation==TEXT("add_or_update_datatable_row")||R.Operation==TEXT("remove_datatable_row"))
                &&Returned&&Undo&&TrackUndo&&ExpectedUndo==Undo&&GUndo==Undo
                &&AfterNameText.Equals(CurrentName,ESearchCase::CaseSensitive)&&AfterPathText.Equals(CurrentPath,ESearchCase::CaseSensitive);
            J->SetBoolField(TEXT("qualified_current_row_struct"),Qualified);
            if(!Qualified)return Stop(TEXT("datatable_string_uds_modify_metadata_unexpected"),TEXT("Actual Modify metadata delta is not the exact current RowStruct serializer name/path transition; prefix retained, no row helper/save."));
            if(NameChanged)ModifyDerivedMetadata.Add(TEXT("/RowStructName/values/0"),TPair<FString,FString>(BeforeNameText,AfterNameText));
            if(PathChanged)ModifyDerivedMetadata.Add(TEXT("/RowStructPathName/values/0"),TPair<FString,FString>(BeforePathText,AfterPathText));
        }
        ModifiedRowStructName=AfterName;ModifiedRowStructPath=AfterPath;TrackModifiedMetadata=true;return StableSource();
    }
    bool AfterModify(bool WasDirty,ITransaction* Undo,bool Returned)
    {
        if(!Context())return false;for(auto& I:Held){auto* O=I.Object.Get();if(O!=Table->GetPackage()||!Loaded(O)||O->GetFlags()==I.Flags)continue;
            const uint32 Before=static_cast<uint32>(I.Flags),After=static_cast<uint32>(O->GetFlags()),Bit=static_cast<uint32>(RF_Transactional);
            if(WasDirty||!Undo||GUndo!=Undo||!Returned||!Table->GetPackage()->IsDirty()||(Before&Bit)||After!=(Before|Bit)||!Table->HasAnyFlags(RF_Transactional))return Raw();
            I.Flags=O->GetFlags();ModifyFlags.Add(I.Path,TPair<uint32,uint32>(Before,After));R.Data->SetBoolField(TEXT("string_uds_modify_package_transactional_adopted"),true);}
        return StableSource();
    }
    bool AfterSave(bool Saved,UPackage* PackageBeforeSave,EObjectFlags FlagsBeforeSave,bool DirtyBeforeSave,ITransaction* UndoBeforeSave)
    {
        if(!Context())return false;if(!Saved)return Raw();for(auto& I:Held){auto* O=I.Object.Get();if(!Loaded(O))return Raw();if(O->GetFlags()==I.Flags)continue;const uint32 A=static_cast<uint32>(I.Flags),B=static_cast<uint32>(O->GetFlags()),Allowed=static_cast<uint32>(RF_WasLoaded|RF_LoadCompleted);
            const bool SelectedPackageRole=Bound&&Loaded(Table.Get())&&Table->GetClass()==UDataTable::StaticClass()&&O==Table->GetPackage()&&Table->GetOuter()==O&&Table->GetPathName().Equals(TablePath,ESearchCase::CaseSensitive)
                &&I.Class==UPackage::StaticClass()&&O->GetClass()==I.Class&&I.Outer==nullptr&&O->GetOuter()==nullptr&&I.Package==O&&O->GetPackage()==O&&I.Path.Equals(SelectedPackage,ESearchCase::CaseSensitive)&&O->GetPathName().Equals(I.Path,ESearchCase::CaseSensitive);
            const bool PackageTransactional=SelectedPackageRole&&O==PackageBeforeSave&&R.SaveAttempted&&R.Saved&&A==static_cast<uint32>(RF_Public)&&static_cast<uint32>(FlagsBeforeSave)==A&&B==(A|static_cast<uint32>(RF_Transactional)|Allowed)
                &&DirtyBeforeSave&&!PackageBeforeSave->IsDirty()&&TrackUndo&&UndoBeforeSave&&UndoBeforeSave==ExpectedUndo&&GUndo==UndoBeforeSave&&!PackageBeforeSave->HasAnyPackageFlags(PKG_PlayInEditor|PKG_ContainsScript|PKG_CompiledIn);
            if(O->GetPackage()!=Table->GetPackage()||(O!=Table.Get()&&!Owned.Contains(O)&&!SelectedPackageRole)||(A&~B)||(((B&~A)&~Allowed)&&!PackageTransactional))return Raw();I.Flags=O->GetFlags();SaveFlags.Add(I.Path,TPair<uint32,uint32>(A,B));
            if(PackageTransactional){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("package"),I.Path);J->SetNumberField(TEXT("before_flags"),A);J->SetNumberField(TEXT("after_flags"),B);J->SetBoolField(TEXT("dirty_before_save"),DirtyBeforeSave);J->SetBoolField(TEXT("dirty_after_save"),false);J->SetBoolField(TEXT("same_transaction"),true);J->SetBoolField(TEXT("save_returned"),Saved);R.Data->SetObjectField(TEXT("string_uds_save_package_transactional_transition"),J);}}return StableSource();
    }
    bool PackageOwned()
    {
        if(!StableSource())return false;TArray<UObject*> Exports;GetObjectsWithPackage(Table->GetPackage(),Exports,EGetObjectsFlags::IncludeNestedObjects);if(!Raw())return false;if(Exports.Num()>MaxObjects)return Stop(TEXT("datatable_string_uds_save_export_bound"),TEXT("Selected save package export bound exceeded."));for(auto* O:Exports)if(O!=Table.Get()&&!Owned.Contains(O))return Stop(TEXT("datatable_string_uds_foreign_save_export"),TEXT("Only selected table/current native owned exports may be saved."));return StableSource(true,true);
    }
};
// UR: native row admission is based on the linked reflected role, never a row name.
// No default scratch instance is constructed by this observer. Native row hooks,
// import/export and destruction stay inside the original public SDK operations.
struct FNativeRowScope
{
    FResult& R;bool Stopped=false,Bound=false,RowsReady=false,SourceBindingsReady=false,SourceMetadataReady=false;TStrongObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;
    TStrongObjectPtr<UScriptStruct> Type;TStrongObjectPtr<UDataTable> Table;TStrongObjectPtr<UObject> ImportData;FString Path,PackagePath,ImportKey,ImportPath;uint8 TableOptions=0;
    struct FObjectRole{TStrongObjectPtr<UObject> Hold;UClass* Kind=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;FString Path;EObjectFlags Flags=RF_NoFlags;uint32 PackageFlags=0;bool PackageDirty=false;};
    struct FFieldRole{FProperty* P=nullptr;FFieldClass* Kind=nullptr;UStruct* Owner=nullptr;FProperty* Parent=nullptr;FName Name;int32 Offset=0,Size=0,Element=0,Dim=0,Align=0;uint64 Flags=0;TMap<FName,FString> Metadata;UScriptStruct* Struct=nullptr;UClass* ObjectClass=nullptr;UClass* SoftMetaClass=nullptr;TObjectPtr<UClass> ObjectClassSlot=nullptr,SoftMetaClassSlot=nullptr;UEnum* Enum=nullptr;FNumericProperty* Underlying=nullptr;FProperty* Inner=nullptr;FProperty* Key=nullptr;FProperty* Value=nullptr;uint8 BoolSize=0,BoolOffset=0,BoolMask=0,BoolFieldMask=0;FScriptSetLayout SetLayout;FScriptMapLayout MapLayout;};
    struct FStructRole{TStrongObjectPtr<UScriptStruct> Hold;UStruct* Super=nullptr;EStructFlags Flags=STRUCT_NoFlags;int32 Size=0,Align=0;TArray<FProperty*> Members;TMap<FName,FString> Metadata;bool Ready=false,CustomJsonConverter=false;};
    struct FEnumRole{TStrongObjectPtr<UEnum> Hold;UEnum::ECppForm Form=UEnum::ECppForm::Regular;TArray<FName> Names;TArray<int64> Values;};
    TArray<FObjectRole> Objects;TArray<TUniquePtr<FStructRole>> Structs;TArray<FFieldRole> Fields;TArray<FEnumRole> Enums;TArray<UScriptStruct*> Active;
    TArray<TPair<FName,const uint8*>> Rows;TMap<FName,TSharedRef<FJsonObject>> Values;TSharedRef<FJsonObject> SourceBaseline=MakeShared<FJsonObject>();
    // WP: readonly parent DAG shares the existing native descriptor/export/value observer.
    // A scope owns one distinct table; edges retain order, nulls and repeated/shared parents.
    FNativeRowScope* CompositeRoot=nullptr;bool CompositeFence=false,ParentEdgesReady=false,CompositeMutable=false;
    // YA: a read-only Composite node may reuse the existing compiled UDS source
    // and copied typed payload observer without binding its ordinary-table writer.
    TUniquePtr<FStringUdsScope> CompositeUdsSource;
    TArray<TUniquePtr<FNativeRowScope>> CompositeOwned;TArray<FNativeRowScope*> CompositeScopes;
    TMap<UDataTable*,FNativeRowScope*> CompositeIndex;TSet<UDataTable*> CompositeActive;
    FArrayProperty* ParentDescriptor=nullptr;FObjectProperty* ParentInner=nullptr;
    int32 ParentOffset=0,ParentSize=0;uint64 ParentFlags=0,ParentInnerFlags=0;
    TArray<UDataTable*> ParentEdges;TMap<FString,TSharedRef<FJsonObject>> CompositeDiskBaseline;
    TMap<FString,TPair<uint32,uint32>> ModifyFlags,SaveFlags;TMap<FString,TPair<FString,FString>> ModifyDerivedMetadata;
    TStrongObjectPtr<UTransBuffer> Transactor;const FTransaction* OwnTransaction=nullptr;FGuid TransactionId,OperationId;FString TransactionContext;int32 TransactionIndex=INDEX_NONE,TransactionDepth=0;bool TransactionReady=false;
    explicit FNativeRowScope(FResult& In):R(In){if(IsInGameThread()&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage()){Editor.Reset(GEditor);World.Reset(GWorld);}}
    bool Bad(const TCHAR* Property,const FString& Expected=TEXT("captured native source/target role"),const FString& Actual=TEXT("changed or unavailable"),bool Unknown=true)
    {
        if(Stopped)return false;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("object"),Path);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);R.Data->SetObjectField(TEXT("native_row_first_mismatch"),J);Stopped=true;return Fail(R,TEXT("datatable_native_row_current_changed"),TEXT("Native row source/current observation failed; retain actual prefix and stop further SDK, save or retry."),Unknown);
    }
    bool Stop(const TCHAR* Code,const TCHAR* Why){if(!Stopped)Bad(Code,Why);R.Code=Code;R.Error=Why;return false;}
    bool StableSource(bool=true,bool=false){return Current(true,true);}
    bool Context()
    {
        if(Stopped||!IsInGameThread()||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||GIsImportingT3D)return false;
        if(!Loaded(Editor.Get())||GEditor!=Editor.Get()||!Loaded(World.Get())||GWorld!=World.Get()||World->WorldType!=EWorldType::Editor||World->bIsTearingDown)return false;
        if(!Editor->PlayWorld.IsResolved()||Editor->PlayWorld.Get()!=nullptr)return false;
        return !IsPlaySessionActive()&&Editor->GetEditorWorldContext().World()==World.Get();
    }
    bool Pin(UObject* O)
    {
        if(!Context()||!Loaded(O)||!CleanPath(O->GetPathName())||!CleanPath(O->GetClass()->GetPathName()))return Bad(TEXT("held_return"));
        for(const auto& E:Objects)if(E.Hold.Get()==O)return true;if(Objects.Num()>=MaxObjects)return Bad(TEXT("held_bound"));
        FObjectRole E;E.Hold.Reset(O);E.Kind=O->GetClass();E.Outer=O->GetOuter();E.Package=O->GetPackage();E.Path=O->GetPathName();E.Flags=O->GetFlags();E.PackageFlags=uint32(E.Package->GetPackageFlags());E.PackageDirty=E.Package->IsDirty();Objects.Add(MoveTemp(E));return true;
    }
    bool RootCurrent()
    {
        if(CompositeRoot&&!CompositeRoot->CompositeFence)return CompositeRoot->CompositeCurrent(false,false);
        if(!Context()||(TransactionReady&&!TransactionCurrent()))return Bad(TEXT("editor_context"));
        if(CompositeUdsSource&&!CompositeUdsSource->Raw(false)){Stopped=true;return false;}
        for(const auto& E:Objects){auto* O=E.Hold.Get();if(!Loaded(O)||!Loaded(E.Kind)||(E.Outer&&!Loaded(E.Outer))||!Loaded(E.Package)||O->GetClass()!=E.Kind||O->GetOuter()!=E.Outer||O->GetPackage()!=E.Package||O->GetFlags()!=E.Flags||!O->GetPathName().Equals(E.Path,ESearchCase::CaseSensitive))return Bad(TEXT("held_identity"),E.Path);if((CompositeRoot||!Bound||E.Package!=Table->GetPackage())&&!(CompositeRoot&&CompositeRoot->CompositeMutable&&E.Package==CompositeRoot->Table->GetPackage())&&(uint32(E.Package->GetPackageFlags())!=E.PackageFlags||E.Package->IsDirty()!=E.PackageDirty))return Bad(TEXT("source_package_state"),E.Path);}
        for(const auto& Entry:Structs){const auto& E=*Entry;auto* S=E.Hold.Get();if(!Loaded(S)||S->GetClass()!=UScriptStruct::StaticClass()||S->StructFlags!=E.Flags||S->GetSuperStruct()!=E.Super||S->GetStructureSize()!=E.Size||S->GetMinAlignment()!=E.Align)return Bad(TEXT("native_struct_layout"));TArray<FProperty*> Now;for(TFieldIterator<FProperty> It(S,EFieldIterationFlags::IncludeDeprecated);It;++It){if(Now.Num()>=MaxFields)return Bad(TEXT("native_field_bound"));Now.Add(*It);}if(Now!=E.Members)return Bad(TEXT("native_member_inventory"));}
        for(const auto& E:Fields){auto* P=E.P;if(E.Parent){const auto* Parent=Fields.FindByPredicate([&](const FFieldRole& X){return X.P==E.Parent;});if(!Parent)return Bad(TEXT("inner_parent"));bool Found=false;if(Parent->Kind==FArrayProperty::StaticClass())Found=static_cast<FArrayProperty*>(E.Parent)->Inner==P;else if(Parent->Kind==FSetProperty::StaticClass())Found=static_cast<FSetProperty*>(E.Parent)->ElementProp==P;else if(Parent->Kind==FMapProperty::StaticClass())Found=static_cast<FMapProperty*>(E.Parent)->KeyProp==P||static_cast<FMapProperty*>(E.Parent)->ValueProp==P;else if(Parent->Kind==FEnumProperty::StaticClass())Found=static_cast<FEnumProperty*>(E.Parent)->GetUnderlyingProperty()==P;if(!Found)return Bad(TEXT("inner_membership"));}if(P->GetClass()!=E.Kind||P->GetOwnerStruct()!=E.Owner||P->GetOwner<FProperty>()!=E.Parent||P->GetFName()!=E.Name||P->GetOffset_ForInternal()!=E.Offset||P->GetSize()!=E.Size||P->ElementSize!=E.Element||P->ArrayDim!=E.Dim||P->GetMinAlignment()!=E.Align||uint64(P->GetPropertyFlags())!=E.Flags||P->HasSetterOrGetter())return Bad(TEXT("native_field_layout"),E.Name.ToString());if(E.Kind==FBoolProperty::StaticClass()){auto* B=static_cast<FBoolProperty*>(P);if(B->GetBoolFieldSize()!=E.BoolSize||B->GetByteOffset()!=E.BoolOffset||B->GetByteMask()!=E.BoolMask||B->GetFieldMask()!=E.BoolFieldMask)return Bad(TEXT("native_bool_layout"));}const auto* M=P->GetMetaDataMap();if((M?M->Num():0)!=E.Metadata.Num())return Bad(TEXT("native_field_metadata"));for(const auto& Pair:E.Metadata){const auto* V=M?M->Find(Pair.Key):nullptr;if(!V||!V->Equals(Pair.Value,ESearchCase::CaseSensitive))return Bad(TEXT("native_field_metadata"));}}
        if(SourceMetadataReady)for(const auto& Entry:Structs){const auto& E=*Entry;const auto* M=FMetaData::GetMapForObject(E.Hold.Get());if((M?M->Num():0)!=E.Metadata.Num())return Bad(TEXT("native_struct_metadata"));for(const auto& Pair:E.Metadata){const auto* V=M?M->Find(Pair.Key):nullptr;if(!V||!V->Equals(Pair.Value,ESearchCase::CaseSensitive))return Bad(TEXT("native_struct_metadata"));}if(FJsonObjectStructInterfaceRegistry::HasStructConverterRegistered(E.Hold.Get())!=E.CustomJsonConverter)return Bad(TEXT("native_json_converter_role"));}
        if(Bound&&(!Loaded(Table.Get())||(Table->GetClass()!=UDataTable::StaticClass()&&(!CompositeRoot||Table->GetClass()!=UCompositeDataTable::StaticClass()))||Table->GetOuter()!=Table->GetPackage()||!Table->RowStruct.IsResolved()||Table->RowStruct.Get()!=Type.Get()||!Table->GetPathName().Equals(Path,ESearchCase::CaseSensitive)||!Table->GetPackage()->GetName().Equals(PackagePath,ESearchCase::CaseSensitive)||FindObject<UDataTable>(nullptr,*Path)!=Table.Get()))return Bad(TEXT("table_role"));if(Bound){const uint8 Options=uint8(Table->bStripFromClientBuilds)|(uint8(Table->bIgnoreExtraFields)<<1)|(uint8(Table->bIgnoreMissingFields)<<2)|(uint8(Table->bPreserveExistingValues)<<3);if(Options!=TableOptions||!Table->ImportKeyField.Equals(ImportKey,ESearchCase::CaseSensitive)||!Table->ImportPath_DEPRECATED.Equals(ImportPath,ESearchCase::CaseSensitive)||!Table->AssetImportData.IsResolved()||static_cast<UObject*>(Table->AssetImportData.Get())!=ImportData.Get())return Bad(TEXT("table_import_settings"));}return true;
    }
    bool CompositeMismatch(const TCHAR* Property,const FString& Expected,const FString& Actual)
    {
        Bad(Property,Expected,Actual);
        if(CompositeRoot&&CompositeRoot!=this)CompositeRoot->Stopped=true;
        return false;
    }
    bool CompositeParents(bool Capture=false)
    {
        if(!Bound||Table->GetClass()!=UCompositeDataTable::StaticClass())return true;
        auto* Descriptor=FindFProperty<FArrayProperty>(UCompositeDataTable::StaticClass(),TEXT("ParentTables"));
        if(!Descriptor||Descriptor->GetOwnerStruct()!=UCompositeDataTable::StaticClass()||Descriptor->ArrayDim!=1||Descriptor->HasSetterOrGetter()
            ||Descriptor->ElementSize!=sizeof(FScriptArray)||Descriptor->ArrayFlags!=EArrayPropertyFlags::None)
            return CompositeMismatch(TEXT("composite.parent_descriptor"),TEXT("linked native ParentTables array"),TEXT("unavailable or changed"));
        auto* Inner=CastField<FObjectProperty>(Descriptor->Inner);
        if(!Inner||Inner->GetClass()!=FObjectProperty::StaticClass()||Inner->GetOwner<FProperty>()!=Descriptor
            ||Inner->GetOwnerStruct()!=UCompositeDataTable::StaticClass()||Inner->ArrayDim!=1||Inner->ElementSize!=sizeof(TObjectPtr<UDataTable>)
            ||!Inner->PropertyClass.IsResolved()||Inner->PropertyClass.Get()!=UDataTable::StaticClass()||Inner->HasSetterOrGetter())
            return CompositeMismatch(TEXT("composite.parent_inner"),TEXT("native TObjectPtr<UDataTable> owned by ParentTables"),TEXT("unavailable or changed"));
        if(!Capture&&(Descriptor!=ParentDescriptor||Inner!=ParentInner||Descriptor->GetOffset_ForInternal()!=ParentOffset
            ||Descriptor->GetSize()!=ParentSize||uint64(Descriptor->GetPropertyFlags())!=ParentFlags||uint64(Inner->GetPropertyFlags())!=ParentInnerFlags))
            return CompositeMismatch(TEXT("composite.parent_layout"),TEXT("captured declaring descriptor/layout"),TEXT("changed"));
        FScriptArrayHelper A(Descriptor,Descriptor->ContainerPtrToValuePtr<void>(Table.Get()));
        if(A.Num()>64||(!Capture&&A.Num()!=ParentEdges.Num()))
            return CompositeMismatch(TEXT("composite.parent_count"),Capture?TEXT("<=64"):LexToString(ParentEdges.Num()),LexToString(A.Num()));
        TArray<UDataTable*> Edges;
        for(int32 I=0;I<A.Num();++I)
        {
            const auto* Slot=reinterpret_cast<const TObjectPtr<UDataTable>*>(A.GetRawPtr(I));
            if(!Slot->IsResolved())return CompositeMismatch(TEXT("composite.parent_resolved"),TEXT("already resolved after original LoadAsset"),TEXT("unresolved; no additional loader"));
            auto* Parent=Slot->Get();
            if(Parent&&(!Loaded(Parent)||!Pin(Parent)||!Pin(Parent->GetPackage())))return CompositeMismatch(TEXT("composite.parent_loaded"),TEXT("loaded held nullable parent"),LexToString(I));
            if(!Capture&&Parent!=ParentEdges[I])return CompositeMismatch(TEXT("composite.parent_order"),FString::Printf(TEXT("index %d pointer:%p"),I,static_cast<void*>(ParentEdges[I])),FString::Printf(TEXT("pointer:%p"),static_cast<void*>(Parent)));
            Edges.Add(Parent);
        }
        if(Capture){ParentDescriptor=Descriptor;ParentInner=Inner;ParentOffset=Descriptor->GetOffset_ForInternal();ParentSize=Descriptor->GetSize();ParentFlags=uint64(Descriptor->GetPropertyFlags());ParentInnerFlags=uint64(Inner->GetPropertyFlags());ParentEdges=MoveTemp(Edges);ParentEdgesReady=true;}
        return true;
    }
    bool CompositeCurrent(bool CheckValues=false,bool CheckRootRows=true)
    {
        if(Stopped)return false;TGuardValue<bool> Guard(CompositeFence,true);
        // XS: only the selected root cache has a writer delta. Parents retain their full
        // membership/value baseline. RootCurrent is a descriptor/context fence, not a
        // stale row-buffer check during the existing helper/converter adoption boundary.
        for(auto* Scope:CompositeScopes)
        {
            const bool RootRows=Scope!=this||!CompositeMutable||CheckRootRows;
            if(!Scope->Current(RootRows,CheckValues&&RootRows)||(Scope->ParentEdgesReady&&!Scope->CompositeParents())){Stopped=true;return false;}
        }
        // Actual effective cache is authoritative, including manual edits; no fresh-merge invariant.
        return true;
    }
    bool BindCompositeNode(UDataTable* T,int32 Depth=0)
    {
        auto& Root=*CompositeRoot;
        if(Depth>16||Root.CompositeActive.Contains(T))return CompositeMismatch(TEXT("composite.parent_dag"),TEXT("acyclic parent DAG depth<=16"),TEXT("cycle or bound exceeded"));
        if(Root.CompositeIndex.Contains(T))return true;
        if(Root.CompositeScopes.Num()>=64||!Loaded(T)||(T->GetClass()!=UDataTable::StaticClass()&&T->GetClass()!=UCompositeDataTable::StaticClass())
            ||!T->RowStruct.IsResolved()||!Loaded(T->RowStruct.Get())
            ||(T->RowStruct.Get()->GetClass()!=UScriptStruct::StaticClass()
                &&(Root.CompositeMutable||T->RowStruct.Get()->GetClass()!=UUserDefinedStruct::StaticClass())))
            return CompositeMismatch(TEXT("composite.source_scope_pending"),TEXT("<=64 exact DataTable/CompositeDataTable with loaded native row struct or readonly admitted compiled UDS"),Loaded(T)?T->GetPathName():TEXT("unavailable"));
        FNativeRowScope* Scope=this;
        if(T!=Root.Table.Get()){auto Owned=MakeUnique<FNativeRowScope>(R);Scope=Owned.Get();Scope->CompositeRoot=&Root;Root.CompositeOwned.Add(MoveTemp(Owned));}
        Root.CompositeIndex.Add(T,Scope);Root.CompositeScopes.Add(Scope);Root.CompositeActive.Add(T);
        if(!Scope->BindType(T->RowStruct.Get(),T,T->GetPathName(),T->GetPackage()->GetName())||!Scope->CompositeParents(true)){Root.Stopped=true;return false;}
        const auto Edges=Scope->ParentEdges;
        for(auto* Parent:Edges)if(Parent)
        {
            if(Root.CompositeActive.Contains(Parent))return Scope->CompositeMismatch(TEXT("composite.parent_dag"),TEXT("acyclic parent DAG"),Parent->GetPathName());
            if(!Root.CompositeIndex.Contains(Parent)&&!Root.BindCompositeNode(Parent,Depth+1))return false;
        }
        Root.CompositeActive.Remove(T);return true;
    }
    bool BindComposite(UDataTable* T,bool Mutable=false)
    {
        CompositeRoot=this;CompositeMutable=Mutable;Table.Reset(T);
        if(!BindCompositeNode(T))return false;
        // Binding completed without rebuilding the effective row cache.
        R.Data->SetBoolField(TEXT("composite_native_read_profile"),true);
        R.Data->SetNumberField(TEXT("composite_explicit_parent_load_calls"),0);
        R.Data->SetStringField(TEXT("composite_read_scope"),TEXT("Original LoadAsset may load/rebuild dependencies. Already-resolved parent DAG/native rows preserve ordered edges and actual effective cached values; no extra load or merge. Readonly compiled UDS sources use the existing bounded source/payload observer; other custom sources and UDS Composite writers remain incomplete. Native row Add/Update/Remove may modify only the selected root cache when explicitly admitted by the original mutable API."));
        if(Mutable)
        {
            R.Data->SetBoolField(TEXT("composite_native_cache_mutation"),true);
            R.Data->SetBoolField(TEXT("composite_parent_writer_dispatched"),false);
            R.Data->SetBoolField(TEXT("composite_cache_values_reload_persistent"),false);
            R.Data->SetStringField(TEXT("composite_mutation_scope"),TEXT("Original public DataTableEditorUtils row helper/converter acts on the selected Composite effective cache, not its parent tables. Original Modify/notify/partial conversion/optional package save/readback remain authoritative. Composite Serialize-on-load and parent updates rebuild this cache from ordered parents; saved=true does not prove these manual cache edits survive reload. No merge, parent writer/save, new loader or authored parent-value replacement."));
        }
        return CompositeCurrent(true);
    }
    bool CompositeSnapshot(const TSharedRef<FJsonObject>& Out)
    {
        if(!CompositeRoot)return true;
        if(!CompositeCurrent(true))return false;
        TArray<TSharedPtr<FJsonValue>> Nodes;
        for(auto* Scope:CompositeScopes)
        {
            auto J=MakeShared<FJsonObject>();
            const bool MutableTarget=CompositeMutable&&Scope==this;
            // The target's identity/properties/rows belong to the existing target state
            // and selected-row delta. Keep only its immutable parent relation here.
            if(MutableTarget)J->SetStringField(TEXT("target_path"),Scope->Table->GetPathName());
            else { J->SetObjectField(TEXT("identity"),Identity(Scope->Table.Get()));J->SetNumberField(TEXT("row_count"),Scope->Rows.Num()); }
            if(Scope!=this&&Scope->CompositeUdsSource)
            {
                auto SourceState=MakeShared<FJsonObject>();if(!Scope->Source(SourceState))return false;
                J->SetObjectField(TEXT("compiled_uds_source"),SourceState);
            }
            J->SetStringField(TEXT("row_struct"),Scope->Type->GetPathName());TArray<TSharedPtr<FJsonValue>> Edges;
            for(auto* Parent:Scope->ParentEdges){if(Parent)Edges.Add(MakeShared<FJsonValueString>(Parent->GetPathName()));else Edges.Add(MakeShared<FJsonValueNull>());}
            J->SetArrayField(TEXT("ordered_parents"),Edges);
            if(Scope!=this)
            {
                auto File=MakeShared<FJsonObject>();if(!Disk(Scope->PackagePath,File)||!CompositeCurrent())return false;
                if(const auto* Before=CompositeDiskBaseline.Find(Scope->PackagePath)){if(!MirrorReadLiteralEqual(*Before,File)){FString X,Y;FJsonSerializer::Serialize(*Before,TJsonWriterFactory<>::Create(&X));FJsonSerializer::Serialize(File,TJsonWriterFactory<>::Create(&Y));return Scope->CompositeMismatch(TEXT("composite.parent_disk"),X,Y);}}
                else CompositeDiskBaseline.Add(Scope->PackagePath,File);
                J->SetObjectField(TEXT("disk"),File);
            }
            // Cache/OldParentTables are deliberately not authored equality conditions.
            if(!MutableTarget){auto Props=MakeShared<FJsonObject>();if(!Properties(Scope->Table.Get(),Props,nullptr,nullptr,Scope))return false;J->SetObjectField(TEXT("properties"),Props);}
            Nodes.Add(MakeShared<FJsonValueObject>(J));
        }
        Out->SetArrayField(TEXT("composite_parent_dag"),Nodes);return CompositeCurrent(true);
    }
    bool BindEnum(UEnum* E)
    {
        if(!Loaded(E)||!Pin(E)||!Pin(E->GetPackage())||!RootCurrent())return false;for(const auto& Old:Enums)if(Old.Hold.Get()==E)return true;
        if(E->GetClass()!=UEnum::StaticClass()||E->NumEnums()>1024)return Bad(TEXT("native_enum_role"));FEnumRole Role;Role.Hold.Reset(E);Role.Form=E->GetCppForm();for(int32 I=0;I<E->NumEnums();++I){Role.Names.Add(E->GetNameByIndex(I));Role.Values.Add(E->GetValueByIndex(I));}Enums.Add(MoveTemp(Role));return RootCurrent();
    }
    static bool SoftReferencePath(const FString& Value)
    {
        if(!CleanPath(Value))return false;
        // Keep the established hard-reference profile unchanged; the new soft
        // family must not serialize either extension or official Editor tooling.
        for(const TCHAR* Token:{TEXT("/Script/ModelContextProtocol"),TEXT("/Script/ToolsetRegistry"),TEXT("/ModelContextProtocol/"),TEXT("/ToolsetRegistry/")})if(Value.Contains(Token,ESearchCase::IgnoreCase))return false;
        return true;
    }
    static bool Kind(FFieldClass* K)
    {return K==FBoolProperty::StaticClass()||K==FInt8Property::StaticClass()||K==FInt16Property::StaticClass()||K==FIntProperty::StaticClass()||K==FInt64Property::StaticClass()||K==FByteProperty::StaticClass()||K==FUInt16Property::StaticClass()||K==FUInt32Property::StaticClass()||K==FUInt64Property::StaticClass()||K==FFloatProperty::StaticClass()||K==FDoubleProperty::StaticClass()||K==FNameProperty::StaticClass()||K==FStrProperty::StaticClass()||K==FTextProperty::StaticClass()||K==FEnumProperty::StaticClass()||K==FStructProperty::StaticClass()||K==FObjectProperty::StaticClass()||K==FSoftObjectProperty::StaticClass()||K==FSoftClassProperty::StaticClass()||K==FArrayProperty::StaticClass()||K==FSetProperty::StaticClass()||K==FMapProperty::StaticClass();}
    bool BindField(FProperty* P,FProperty* Parent=nullptr,int32 Depth=0)
    {
        if(!RootCurrent())return false;if(!P||!Kind(P->GetClass()))return Bad(TEXT("native_field_class_pending"),TEXT("exact native scalar/enum/struct/hard-or-soft object/soft class/array/set/map descriptor"),P?P->GetClass()->GetName():TEXT("null"),false);
        for(const auto& E:Fields)if(E.P==P)return true;
        if(Depth>16||Fields.Num()>=MaxFields||P->ArrayDim<1||P->ArrayDim>256||P->GetSize()<=0||P->GetMinAlignment()<=0||P->HasSetterOrGetter()||P->HasAnyPropertyFlags(CPF_InstancedReference|CPF_ContainsInstancedReference|CPF_PersistentInstance))return Bad(TEXT("native_field_scope_pending"),TEXT("bounded linked field without accessors or instanced object serialization"),P->GetName(),false);
        FFieldRole E;E.P=P;E.Kind=P->GetClass();E.Owner=P->GetOwnerStruct();E.Parent=P->GetOwner<FProperty>();E.Name=P->GetFName();E.Offset=P->GetOffset_ForInternal();E.Size=P->GetSize();E.Element=P->ElementSize;E.Dim=P->ArrayDim;E.Align=P->GetMinAlignment();E.Flags=uint64(P->GetPropertyFlags());if(const auto* M=P->GetMetaDataMap())E.Metadata=*M;
        if(E.Parent!=Parent||!E.Owner||(!Parent&&(E.Offset<0||int64(E.Offset)+E.Size>E.Owner->GetPropertiesSize())))return Bad(TEXT("native_field_owner_bounds"));
        if(E.Kind==FBoolProperty::StaticClass()){auto* B=static_cast<FBoolProperty*>(P);E.BoolSize=B->GetBoolFieldSize();E.BoolOffset=B->GetByteOffset();E.BoolMask=B->GetByteMask();E.BoolFieldMask=B->GetFieldMask();}
        Fields.Add(E);const int32 Index=Fields.Num()-1;
        if(E.Kind==FStructProperty::StaticClass()){auto* S=static_cast<FStructProperty*>(P);if(!S->Struct.IsResolved())return Bad(TEXT("nested_struct_unresolved"));auto* Returned=S->Struct.Get();TStrongObjectPtr<UScriptStruct> Hold(Returned);if(!RootCurrent()||!Loaded(Returned))return Bad(TEXT("nested_struct_return"));Fields[Index].Struct=Returned;if(!BindStruct(Returned,Depth+1))return false;}
        else if(E.Kind==FObjectProperty::StaticClass()||E.Kind==FSoftObjectProperty::StaticClass()||E.Kind==FSoftClassProperty::StaticClass())
        {
            auto* O=static_cast<FObjectPropertyBase*>(P);const TObjectPtr<UClass> CopiedClass=O->PropertyClass;if(!CopiedClass.IsResolved())return Bad(TEXT("object_class_unresolved"));auto* Returned=CopiedClass.Get();TStrongObjectPtr<UClass> Hold(Returned);
            if(!RootCurrent()||!Loaded(Returned)||!Pin(Returned)||!Pin(Returned->GetPackage())||!RootCurrent())return false;
            if(!O->PropertyClass.IsResolved()||O->PropertyClass.GetHandle()!=CopiedClass.GetHandle())return Bad(TEXT("object_class_binding_after_read"));
            if(!Returned->HasAnyClassFlags(CLASS_Native)||Returned->ClassGeneratedBy||Returned->HasAnyClassFlags(CLASS_DefaultToInstanced))return Bad(TEXT("object_class_pending"),TEXT("loaded native noninstanced reference class"),Returned->GetPathName(),false);Fields[Index].ObjectClass=Returned;Fields[Index].ObjectClassSlot=CopiedClass;
            if(E.Kind!=FObjectProperty::StaticClass())
            {
                // ADC: FSoftObjectProperty and FSoftClassProperty own a persistent
                // path, not a TObjectPtr. Its weak cache is not an authored value.
                if(!RootCurrent())return false;
                if(!SoftReferencePath(Returned->GetPathName()))return Bad(TEXT("soft_reference_class_forbidden"),TEXT("runtime-independent native property class"),Returned->GetPathName(),false);
                if(P->ElementSize!=sizeof(FSoftObjectPtr)||P->GetMinAlignment()!=alignof(FSoftObjectPtr))return Bad(TEXT("soft_reference_storage"),TEXT("native FSoftObjectPtr element size/alignment"),P->GetName(),false);
                if(E.Kind==FSoftClassProperty::StaticClass())
                {
                    auto* ClassProperty=static_cast<FSoftClassProperty*>(P);const TObjectPtr<UClass> CopiedMeta=ClassProperty->MetaClass;if(!CopiedMeta.IsResolved())return Bad(TEXT("soft_class_metaclass_unresolved"));UClass* Meta=CopiedMeta.Get();TStrongObjectPtr<UClass> MetaHold(Meta);
                    if(!RootCurrent()||!Loaded(Meta)||!Pin(Meta)||!Pin(Meta->GetPackage())||!RootCurrent())return false;
                    if(!ClassProperty->MetaClass.IsResolved()||ClassProperty->MetaClass.GetHandle()!=CopiedMeta.GetHandle())return Bad(TEXT("soft_class_metaclass_binding_after_read"));
                    if(!SoftReferencePath(Meta->GetPathName()))return Bad(TEXT("soft_class_metaclass_forbidden"),TEXT("runtime-independent native class constraint"),Meta->GetPathName(),false);
                    if(Returned!=UClass::StaticClass()||!Meta->HasAnyClassFlags(CLASS_Native)||!IsObjectHandleNull(Meta->ClassGeneratedBy.GetHandle()))return Bad(TEXT("soft_class_metaclass_pending"),TEXT("resolved native class constraint; no generated source loading"),Meta->GetPathName(),false);
                    Fields[Index].SoftMetaClass=Meta;Fields[Index].SoftMetaClassSlot=CopiedMeta;
                }
            }
        }
        else if(E.Kind==FEnumProperty::StaticClass()){auto* F=static_cast<FEnumProperty*>(P);auto* Inner=F->GetUnderlyingProperty();if(!Inner||!Kind(Inner->GetClass())||!CastField<FNumericProperty>(Inner))return Bad(TEXT("enum_underlying"));Fields[Index].Underlying=Inner;if(!BindField(Inner,P,Depth+1))return false;}
        else if(E.Kind==FByteProperty::StaticClass()){auto* B=static_cast<FByteProperty*>(P);if(!B->Enum.IsResolved())return Bad(TEXT("byte_enum_unresolved"));Fields[Index].Enum=B->Enum.Get();}
        else if(E.Kind==FArrayProperty::StaticClass()){auto* A=static_cast<FArrayProperty*>(P);if(A->ArrayFlags!=EArrayPropertyFlags::None||P->ElementSize!=sizeof(FScriptArray))return Bad(TEXT("array_allocator_pending"),TEXT("native default allocator"),P->GetName(),false);Fields[Index].Inner=A->Inner;if(!BindField(A->Inner,P,Depth+1))return false;}
        else if(E.Kind==FSetProperty::StaticClass()){auto* S=static_cast<FSetProperty*>(P);Fields[Index].Inner=S->ElementProp;Fields[Index].SetLayout=S->SetLayout;if(P->ElementSize!=sizeof(FScriptSet)||!BindField(S->ElementProp,P,Depth+1))return Bad(TEXT("set_layout_pending"));}
        else if(E.Kind==FMapProperty::StaticClass()){auto* M=static_cast<FMapProperty*>(P);if(M->MapFlags!=EMapPropertyFlags::None||P->ElementSize!=sizeof(FScriptMap))return Bad(TEXT("map_allocator_pending"));Fields[Index].Key=M->KeyProp;Fields[Index].Value=M->ValueProp;Fields[Index].MapLayout=M->MapLayout;if(!BindField(M->KeyProp,P,Depth+1)||!BindField(M->ValueProp,P,Depth+1))return false;}
        return RootCurrent();
    }
    bool BindStruct(UScriptStruct* S,int32 Depth=0)
    {
        if(!RootCurrent())return false;if(Active.Contains(S))return Bad(TEXT("native_recursive_struct_pending"),TEXT("finite acyclic reflected value source"),TEXT("active cycle"),false);for(const auto& E:Structs)if(E->Hold.Get()==S)return E->Ready;
        // UR Native2: same admission operands/order; expose the first rejected operand only.
        auto RejectStruct=[&](const TCHAR* Property,const FString& Expected,const FString& Actual,bool IdentitySafe)->bool
        {
            const FString Object=IdentitySafe?S->GetPathName():FString::Printf(TEXT("pointer:%p"),static_cast<const void*>(S));
            const bool Returned=Bad(TEXT("native_struct_scope_pending"),Expected,Actual,false);
            const auto Detail=R.Data->GetObjectField(TEXT("native_row_first_mismatch"));Detail->SetStringField(TEXT("property"),Property);Detail->SetStringField(TEXT("object"),Object);return Returned;
        };
        if(Depth>16)return RejectStruct(TEXT("native_struct.depth"),TEXT("<=16"),LexToString(Depth),false);
        if(Structs.Num()>=128)return RejectStruct(TEXT("native_struct.source_count"),TEXT("<128"),LexToString(Structs.Num()),false);
        if(!Loaded(S))
        {
            const bool Valid=IsValid(S);const uint32 Flags=Valid?uint32(S->GetFlags()):0;
            const uint32 Blocked=Flags&uint32(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);
            return RejectStruct(TEXT("native_struct.loaded"),TEXT("IsValid=true and blocked phase flags=0"),Valid?TEXT("IsValid=true; flags=")+LexToString(Flags)+TEXT("; blocked_phase_flags=")+LexToString(Blocked):TEXT("IsValid=false"),Valid);
        }
        if(S->GetClass()!=UScriptStruct::StaticClass())return RejectStruct(TEXT("native_struct.class"),TEXT("/Script/CoreUObject.ScriptStruct"),Loaded(S->GetClass())?S->GetClass()->GetPathName():TEXT("unavailable"),true);
        if(!(S->StructFlags&STRUCT_Native))return RejectStruct(TEXT("native_struct.flags.STRUCT_Native"),TEXT("set"),TEXT("unset; flags=")+LexToString(uint32(S->StructFlags)),true);
        if(S->GetOuter()!=S->GetPackage())return RejectStruct(TEXT("native_struct.outer_is_package"),FString::Printf(TEXT("pointer:%p"),static_cast<const void*>(S->GetPackage())),FString::Printf(TEXT("pointer:%p"),static_cast<const void*>(S->GetOuter())),true);
        if(!S->GetPackage()->GetName().StartsWith(TEXT("/Script/")))return RejectStruct(TEXT("native_struct.package_name"),TEXT("/Script/*"),S->GetPackage()->GetName(),true);
        if(!S->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn))return RejectStruct(TEXT("native_struct.package_flags.PKG_CompiledIn"),TEXT("set"),TEXT("unset; flags=")+LexToString(uint32(S->GetPackage()->GetPackageFlags())),true);
        // Compiled-in source packages may already be dirty (observed SlateCore); Pin captures it, RootCurrent requires exact preservation.
        if(S->GetStructureSize()<=0)return RejectStruct(TEXT("native_struct.size_positive"),TEXT(">0"),LexToString(S->GetStructureSize()),true);
        if(S->GetStructureSize()>1048576)return RejectStruct(TEXT("native_struct.size_bound"),TEXT("<=1048576"),LexToString(S->GetStructureSize()),true);
        if(S->GetMinAlignment()<=0)return RejectStruct(TEXT("native_struct.alignment"),TEXT(">0"),LexToString(S->GetMinAlignment()),true);
        if(!CleanPath(S->GetPathName()))return RejectStruct(TEXT("native_struct.runtime_independent_path"),TEXT("CleanPath=true"),S->GetPathName(),true);
        if(!Pin(S)||!Pin(S->GetPackage()))return false;auto Entry=MakeUnique<FStructRole>();auto* Role=Entry.Get();Role->Hold.Reset(S);Role->Super=S->GetSuperStruct();Role->Flags=S->StructFlags;Role->Size=S->GetStructureSize();Role->Align=S->GetMinAlignment();for(TFieldIterator<FProperty> It(S,EFieldIterationFlags::IncludeDeprecated);It;++It)Role->Members.Add(*It);Structs.Add(MoveTemp(Entry));Active.Add(S);
        if(Role->Super){auto* Super=Cast<UScriptStruct>(Role->Super);TStrongObjectPtr<UScriptStruct> Hold(Super);if(!RootCurrent()||!Super||!BindStruct(Super,Depth+1))return false;}
        if(Depth>0&&Role->Members.IsEmpty()&&!Role->Super&&S!=FindObject<UScriptStruct>(nullptr,TEXT("/Script/Engine.TableRowBase")))return Bad(TEXT("opaque_nested_native_payload_pending"),TEXT("finite reflected nested members"),S->GetPathName(),false);const auto Members=Role->Members;for(auto* P:Members)if(!BindField(P,nullptr,Depth+1))return false;Active.Remove(S);Role->Ready=true;return RootCurrent();
    }
    bool Current(bool CheckRows=true,bool CheckValues=false)
    {
        if(CompositeRoot&&!CompositeRoot->CompositeFence)return CompositeRoot->CompositeCurrent(CheckValues,CheckRows);
        if(!RootCurrent())return false;
        for(const auto& E:Fields){auto* P=E.P;if(E.Struct){auto* F=static_cast<FStructProperty*>(P);if(!F->Struct.IsResolved()||F->Struct.Get()!=E.Struct)return Bad(TEXT("struct_binding"));}if(E.ObjectClass){auto* F=static_cast<FObjectPropertyBase*>(P);if(!F->PropertyClass.IsResolved()||!E.ObjectClassSlot.IsResolved()||F->PropertyClass.GetHandle()!=E.ObjectClassSlot.GetHandle())return Bad(TEXT("object_class_binding"));}if(E.Kind==FSoftObjectProperty::StaticClass()||E.Kind==FSoftClassProperty::StaticClass()){if(P->ElementSize!=sizeof(FSoftObjectPtr)||P->GetMinAlignment()!=alignof(FSoftObjectPtr))return Bad(TEXT("soft_reference_storage"));if(E.Kind==FSoftClassProperty::StaticClass()){auto* F=static_cast<FSoftClassProperty*>(P);if(!F->MetaClass.IsResolved()||!E.SoftMetaClassSlot.IsResolved()||F->MetaClass.GetHandle()!=E.SoftMetaClassSlot.GetHandle())return Bad(TEXT("soft_class_metaclass_binding"));}}if(SourceBindingsReady&&E.Kind==FEnumProperty::StaticClass()){auto* F=static_cast<FEnumProperty*>(P);auto* Returned=F->GetEnum();TStrongObjectPtr<UEnum> Hold(Returned);if(!RootCurrent())return false;if(Returned!=E.Enum||F->GetUnderlyingProperty()!=E.Underlying)return Bad(TEXT("enum_binding"));}if(E.Kind==FByteProperty::StaticClass()){auto* F=static_cast<FByteProperty*>(P);if(!F->Enum.IsResolved()||F->Enum.Get()!=E.Enum)return Bad(TEXT("byte_enum_binding"));}if(E.Kind==FArrayProperty::StaticClass()&&(static_cast<FArrayProperty*>(P)->Inner!=E.Inner||static_cast<FArrayProperty*>(P)->ArrayFlags!=EArrayPropertyFlags::None))return Bad(TEXT("array_inner"));if(E.Kind==FSetProperty::StaticClass()&&(static_cast<FSetProperty*>(P)->ElementProp!=E.Inner||!FStringUdsScope::SetLayoutSame(static_cast<FSetProperty*>(P)->SetLayout,E.SetLayout)))return Bad(TEXT("set_element"));if(E.Kind==FMapProperty::StaticClass()&&(static_cast<FMapProperty*>(P)->KeyProp!=E.Key||static_cast<FMapProperty*>(P)->ValueProp!=E.Value||static_cast<FMapProperty*>(P)->MapFlags!=EMapPropertyFlags::None||!FStringUdsScope::MapLayoutSame(static_cast<FMapProperty*>(P)->MapLayout,E.MapLayout)))return Bad(TEXT("map_key_value"));}
        for(const auto& E:Enums){auto* U=E.Hold.Get();if(!Loaded(U)||U->GetCppForm()!=E.Form||U->NumEnums()!=E.Names.Num())return Bad(TEXT("enum_inventory"));for(int32 I=0;I<E.Names.Num();++I)if(U->GetNameByIndex(I)!=E.Names[I]||U->GetValueByIndex(I)!=E.Values[I])return Bad(TEXT("enum_value"));}
        if(CheckRows&&RowsReady){const auto& Map=Table->GetRowMap();if(Map.Num()!=Rows.Num())return Bad(TEXT("row_count"));int32 I=0;for(const auto& P:Map){if(P.Key!=Rows[I].Key||P.Value!=Rows[I].Value)return Bad(TEXT("row_membership"));++I;}}if(CheckValues&&RowsReady){const auto Copy=Rows;for(const auto& Row:Copy){const auto* Expected=Values.Find(Row.Key);if(Expected){auto Actual=MakeShared<FJsonObject>();if(!TypedRow(Row.Value,Actual))return false;if(!MirrorReadLiteralEqual(*Expected,Actual)){if(CompositeRoot){FString X,Y;FJsonSerializer::Serialize(*Expected,TJsonWriterFactory<>::Create(&X));FJsonSerializer::Serialize(Actual,TJsonWriterFactory<>::Create(&Y));return CompositeMismatch(*(TEXT("retained_typed_row_value.")+Row.Key.ToString()),X,Y);}return Bad(TEXT("retained_typed_row_value"),Row.Key.ToString());}}}}return true;
    }
    bool BindType(UScriptStruct* S,UDataTable* Existing=nullptr,const FString& InPath=FString(),const FString& InPackage=FString())
    {
        Type.Reset(S);if(!Context()||!Loaded(S))return Bad(TEXT("native_source_return"));
        if(CompositeRoot&&!CompositeRoot->CompositeMutable&&S->GetClass()==UUserDefinedStruct::StaticClass())
        {
            CompositeUdsSource=MakeUnique<FStringUdsScope>(R,true,false,true);
            auto& SourceScope=*CompositeUdsSource;
            if(!SourceScope.Start()||!SourceScope.BindType(static_cast<UUserDefinedStruct*>(S)))
            {Stopped=true;return false;}
            if(!RootCurrent()||!Existing||!BindTable(Existing,InPath,InPackage))return false;
            R.Data->SetBoolField(TEXT("composite_compiled_uds_read_profile"),true);
            R.Data->SetStringField(TEXT("composite_compiled_uds_scope"),TEXT("Schema/list/get reuse the existing saved-clean compiled UDS source/default/layout and copied typed scalar/math/recursive container payload guards for each resolved parent DAG node. Root actual cached values are authoritative; parent rows need not equal them. No UDS writer, parent reconstruction/load, compile/save or reload-persistence claim."));
            return Source(SourceBaseline);
        }
        auto* Base=FindObject<UScriptStruct>(nullptr,TEXT("/Script/Engine.TableRowBase"));TStrongObjectPtr<UScriptStruct> Hold(Base);if(!Loaded(Base)||!S->IsChildOf(Base))return Bad(TEXT("native_row_base_pending"),TEXT("native FTableRowBase-derived row struct"),S->GetPathName(),false);if(!BindStruct(S))return false;
        // All declaring/nested layouts are fixed before observer enum resolution.
        if(Existing&&!BindTable(Existing,InPath,InPackage))return false;
        for(auto& Entry:Structs){if(const auto* M=FMetaData::GetMapForObject(Entry->Hold.Get()))Entry->Metadata=*M;if(!RootCurrent())return false;Entry->CustomJsonConverter=FJsonObjectStructInterfaceRegistry::HasStructConverterRegistered(Entry->Hold.Get());if(!RootCurrent())return false;}SourceMetadataReady=true;if(!Current(true,true))return false;
        for(int32 I=0;I<Fields.Num();++I){const auto FieldKind=Fields[I].Kind;UEnum* E=nullptr;if(FieldKind==FEnumProperty::StaticClass()){E=static_cast<FEnumProperty*>(Fields[I].P)->GetEnum();TStrongObjectPtr<UEnum> Returned(E);if(!RootCurrent())return false;Fields[I].Enum=E;if(!BindEnum(E))return false;}else if(FieldKind==FByteProperty::StaticClass()&&Fields[I].Enum){E=Fields[I].Enum;if(!BindEnum(E))return false;}if(!Current(true,true))return false;}SourceBindingsReady=true;if(!Current(true,true))return false;
        R.Data->SetBoolField(TEXT("generic_native_row_profile"),true);R.Data->SetStringField(TEXT("native_row_scope"),TEXT("Bounded linked native FTableRowBase source with actual declaring descriptors/nested sources/enums and loaded nullable native object refs. Native soft object/class descriptors retain resolved held PropertyClass and separate soft-class MetaClass; logical paths, including pending/null values, are freshly copied through existing nested Array/Set/Map observation. Observer soft resolve/load/Get/CDO creation are absent, and weak resolution caches are not authored invariants. Original stock converter, row helpers, late partial failure and optional save order remain unchanged. Original SDK constructor/destructor/row virtual/import-export hooks are whole-call boundaries, not interleaved/global-purity proof; no observer default instance allocation. Generated declaration classes, custom soft descriptors and full referenced-asset RAM/runtime equivalence remain incomplete."));
        int32 SoftFields=0,SoftClassFields=0;for(const auto& Field:Fields){SoftFields+=Field.Kind==FSoftObjectProperty::StaticClass();SoftClassFields+=Field.Kind==FSoftClassProperty::StaticClass();}
        R.Data->SetNumberField(TEXT("native_soft_object_descriptor_count"),SoftFields);R.Data->SetNumberField(TEXT("native_soft_class_descriptor_count"),SoftClassFields);R.Data->SetBoolField(TEXT("native_soft_referent_resolution_added"),false);return Source(SourceBaseline);
    }
    bool Source(const TSharedRef<FJsonObject>& Out)
    {
        if(!Current())return false;
        if(CompositeUdsSource)
        {
            if(!CompositeUdsSource->StableSource(false,false)){Stopped=true;return false;}
            if(!Current())return false;
            Out->SetObjectField(TEXT("compiled_uds_source"),CompositeUdsSource->SourceBaseline);
        }
        Out->SetObjectField(TEXT("table_class"),Identity(Bound?Table->GetClass():UDataTable::StaticClass()));Out->SetObjectField(TEXT("row_struct"),Identity(Type.Get()));Out->SetStringField(TEXT("row_struct_flags"),LexToString(uint32(Type->StructFlags)));Out->SetNumberField(TEXT("row_struct_bytes"),Type->GetStructureSize());TArray<TSharedPtr<FJsonValue>> Items;
        for(TFieldIterator<FProperty> It(Type.Get());It;++It){auto* P=*It;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),P->GetName());const FString Authored=P->GetAuthoredName();if(!Current())return false;J->SetStringField(TEXT("authored_name"),Authored);const FString Cpp=P->GetCPPType(nullptr,0);if(!Current())return false;J->SetStringField(TEXT("cpp_type"),Cpp);J->SetStringField(TEXT("class"),P->GetClass()->GetName());J->SetStringField(TEXT("flags"),LexToString(uint64(P->GetPropertyFlags())));J->SetStringField(TEXT("declaring_owner"),P->GetOwnerStruct()->GetPathName());Items.Add(MakeShared<FJsonValueObject>(J));}Out->SetArrayField(TEXT("fields"),Items);auto Packages=MakeShared<FJsonObject>();for(const auto& E:Objects)if(E.Hold.Get()==E.Package&&E.Path.StartsWith(TEXT("/Script/"))){auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("initial_dirty"),E.PackageDirty);J->SetStringField(TEXT("initial_package_flags"),LexToString(E.PackageFlags));Packages->SetObjectField(E.Path,J);}Out->SetObjectField(TEXT("source_package_initial_states"),Packages);return Current();
    }
    bool TypedValue(FProperty* P,const void* At,TSharedPtr<FJsonValue>& Out,int32& Cells,int32 Depth=0)
    {
        if(!At||Depth>16||++Cells>65536)return Bad(TEXT("typed_value_bound"));const auto* E=Fields.FindByPredicate([&](const FFieldRole& V){return V.P==P;});if(!E)return Bad(TEXT("typed_descriptor"));
        if(P->ArrayDim>1){TArray<TSharedPtr<FJsonValue>> A;for(int32 I=0;I<P->ArrayDim;++I){TSharedPtr<FJsonValue> V;if(!TypedScalar(*E,static_cast<const uint8*>(At)+I*P->ElementSize,V,Cells,Depth+1))return false;A.Add(V);}Out=MakeShared<FJsonValueArray>(A);return true;}return TypedScalar(*E,At,Out,Cells,Depth);
    }
    bool TypedScalar(const FFieldRole& E,const void* At,TSharedPtr<FJsonValue>& Out,int32& Cells,int32 Depth)
    {
        if(Depth>16||++Cells>65536)return Bad(TEXT("typed_value_bound"));auto* P=E.P;
        if(E.Kind==FBoolProperty::StaticClass())Out=MakeShared<FJsonValueBoolean>(static_cast<FBoolProperty*>(P)->GetPropertyValue(At));
        else if(E.Kind==FNameProperty::StaticClass())Out=MakeShared<FJsonValueString>(static_cast<FNameProperty*>(P)->GetPropertyValue(At).ToString());
        else if(E.Kind==FStrProperty::StaticClass())Out=MakeShared<FJsonValueString>(static_cast<FStrProperty*>(P)->GetPropertyValue(At));
        else if(E.Kind==FTextProperty::StaticClass()){const FText Text=static_cast<FTextProperty*>(P)->GetPropertyValue(At);Out=MakeShared<FJsonValueString>(LexToString(FTextInspector::GetFlags(Text))+TEXT("|")+LexToString(UPTRINT(FTextInspector::GetSharedDataId(Text))));}
        else if(E.Kind==FEnumProperty::StaticClass())Out=MakeShared<FJsonValueString>(LexToString(E.Underlying->GetSignedIntPropertyValue(At)));
        else if(auto* N=CastField<FNumericProperty>(P)){if(N->IsFloatingPoint()){const double V=N->GetFloatingPointPropertyValue(At);if(!FMath::IsFinite(V))return Bad(TEXT("native_nonfinite_value"));Out=MakeShared<FJsonValueNumber>(V);}else{const bool Unsigned=E.Kind==FByteProperty::StaticClass()||E.Kind==FUInt16Property::StaticClass()||E.Kind==FUInt32Property::StaticClass()||E.Kind==FUInt64Property::StaticClass();Out=MakeShared<FJsonValueString>(Unsigned?LexToString(N->GetUnsignedIntPropertyValue(At)):LexToString(N->GetSignedIntPropertyValue(At)));}}
        else if(E.Kind==FObjectProperty::StaticClass()){const TObjectPtr<UObject> Slot=static_cast<FObjectProperty*>(P)->GetObjectPtrPropertyValue(At);if(!Slot.IsResolved())return Bad(TEXT("typed_object_unresolved"));auto* Ref=Slot.Get();TStrongObjectPtr<UObject> Hold(Ref);if(Ref&&(!Pin(Ref)||!Pin(Ref->GetPackage())))return false;Out=Ref?TSharedPtr<FJsonValue>(MakeShared<FJsonValueString>(Ref->GetPathName()+TEXT("|")+LexToString(UPTRINT(Ref)))):TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>());}
        else if(E.Kind==FSoftObjectProperty::StaticClass()||E.Kind==FSoftClassProperty::StaticClass())
        {
            // Copy only logical path storage. Never Get/IsValid/Resolve/Load the
            // soft referent: pending and resident paths have the same value here.
            const FSoftObjectPtr Copied=*static_cast<const FSoftObjectPtr*>(At);const FSoftObjectPath SoftPath=Copied.ToSoftObjectPath();const FString Literal=SoftPath.ToString();
            if(Literal.Len()>MaxText||!SoftReferencePath(Literal)||(!SoftPath.IsNull()&&!SoftPath.IsValid()))return Bad(TEXT("typed_soft_reference_path"),TEXT("bounded tooling-free logical soft object/class path or null"),Literal.Left(4096));
            Out=MakeShared<FJsonValueString>(Literal);
        }
        else if(E.Struct){auto J=MakeShared<FJsonObject>();for(TFieldIterator<FProperty> It(E.Struct);It;++It){if(It->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated))continue;TSharedPtr<FJsonValue> V;if(!TypedValue(*It,It->ContainerPtrToValuePtr<void>(At),V,Cells,Depth+1))return false;J->SetField(It->GetName(),V);}Out=MakeShared<FJsonValueObject>(J);}
        else if(E.Kind==FArrayProperty::StaticClass()){FScriptArrayHelper H(static_cast<FArrayProperty*>(P),At);if(H.Num()<0||H.Num()>4096)return Bad(TEXT("typed_array_bound"));TArray<TSharedPtr<FJsonValue>> A;for(int32 I=0;I<H.Num();++I){TSharedPtr<FJsonValue> V;if(!TypedValue(E.Inner,H.GetRawPtr(I),V,Cells,Depth+1))return false;A.Add(V);}Out=MakeShared<FJsonValueArray>(A);}
        else if(E.Kind==FSetProperty::StaticClass()||E.Kind==FMapProperty::StaticClass()){TArray<FString> Bag;if(E.Kind==FSetProperty::StaticClass()){FScriptSetHelper H(static_cast<FSetProperty*>(P),At);if(H.Num()<0||H.Num()>4096||H.GetMaxIndex()>65536)return Bad(TEXT("typed_set_bound"));for(int32 I=0;I<H.GetMaxIndex();++I)if(H.IsValidIndex(I)){TSharedPtr<FJsonValue> V;if(!TypedValue(E.Inner,H.GetElementPtr(I),V,Cells,Depth+1))return false;FString Text;FJsonSerializer::Serialize(V,TEXT(""),TJsonWriterFactory<>::Create(&Text));Bag.Add(Text);}}else{FScriptMapHelper H(static_cast<FMapProperty*>(P),At);if(H.Num()<0||H.Num()>4096||H.GetMaxIndex()>65536)return Bad(TEXT("typed_map_bound"));for(int32 I=0;I<H.GetMaxIndex();++I)if(H.IsValidIndex(I)){TSharedPtr<FJsonValue> K,V;if(!TypedValue(E.Key,H.GetKeyPtr(I),K,Cells,Depth+1)||!TypedValue(E.Value,H.GetValuePtr(I),V,Cells,Depth+1))return false;FString Text;FJsonSerializer::Serialize(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{K,V}),TEXT(""),TJsonWriterFactory<>::Create(&Text));Bag.Add(Text);}}Bag.Sort();TArray<TSharedPtr<FJsonValue>> A;for(const auto& Text:Bag)A.Add(MakeShared<FJsonValueString>(Text));Out=MakeShared<FJsonValueArray>(A);}
        else return Bad(TEXT("typed_value_kind"));return true;
    }
    bool TypedRow(const uint8* At,const TSharedRef<FJsonObject>& Out)
    {
        if(CompositeUdsSource)
        {
            // Current() finishes all observer-bearing source/descriptor checks.
            // Such callbacks may replace row storage even when the UDS scope
            // itself is source-only (Bound=false). Reacquire the selected route
            // after that boundary, then use only the callback-free value copy.
            if(!Current(true,false))return false;
            const auto* Captured=Rows.FindByPredicate([&](const TPair<FName,const uint8*>& E){return E.Value==At;});
            if(!Captured||!Loaded(Table.Get()))return Bad(TEXT("composite_uds_row_borrow"),TEXT("held selected row and loaded table"),TEXT("unavailable"));
            const auto* Actual=Table->GetRowMap().Find(Captured->Key);
            if(!Actual||*Actual!=At||!At)return Bad(TEXT("composite_uds_row_borrow"),TEXT("captured key and current identical storage"),TEXT("row removed or replaced"));
            FString Value;if(!CompositeUdsSource->RowValue(At,Value,true)){Stopped=true;return false;}
            // The existing UDS observer owns the lossless fingerprint; public row
            // JSON still comes from the original stock converter in Export().
            Out->SetStringField(TEXT("compiled_uds_copied_value"),Value);return RootCurrent();
        }if(!RootCurrent()||!At)return false;int32 Cells=0;for(TFieldIterator<FProperty> It(Type.Get());It;++It){if(It->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated))continue;TSharedPtr<FJsonValue> V;if(!TypedValue(*It,It->ContainerPtrToValuePtr<void>(At),V,Cells))return false;Out->SetField(It->GetName(),V);}return RootCurrent();}
    bool Export(FName Key,const uint8* Expected,const TSharedRef<FJsonObject>& Out,bool CheckAllRows=true)
    {
        if(!Current(CheckAllRows))return false;const auto* Actual=Table->GetRowMap().Find(Key);const uint8* MemoryValue=Actual?*Actual:nullptr;if(!MemoryValue||MemoryValue!=Expected)return Bad(TEXT("fresh_row_memory"));auto Typed=MakeShared<FJsonObject>();if(!TypedRow(MemoryValue,Typed)||!Current(CheckAllRows))return false;const auto* BeforeExport=Table->GetRowMap().Find(Key);if(!BeforeExport||*BeforeExport!=MemoryValue)return Bad(TEXT("fresh_row_before_export"));
        const bool Returned=FJsonObjectConverter::UStructToJsonObject(Type.Get(),MemoryValue,Out,0,0);R.Data->SetBoolField(TEXT("native_row_export_returned"),Returned);if(!Current(CheckAllRows))return false;if(!Returned)return Bad(TEXT("native_row_export_failed"));const auto* Fresh=Table->GetRowMap().Find(Key);return (Fresh&&*Fresh==Expected)||Bad(TEXT("row_memory_after_export"));
    }
    bool BindTable(UDataTable* T,const FString& InPath,const FString& InPackage)
    {
        if(!Current(false)||!Loaded(T)||(T->GetClass()!=UDataTable::StaticClass()&&(!CompositeRoot||T->GetClass()!=UCompositeDataTable::StaticClass()))||T->GetOuter()!=T->GetPackage()||!T->RowStruct.IsResolved()||T->RowStruct.Get()!=Type.Get()||!T->GetPathName().Equals(InPath,ESearchCase::CaseSensitive)||!T->AssetImportData.IsResolved()||T->GetRowMap().Num()>MaxRowsObserved)return Bad(TEXT("native_table_scope"));Table.Reset(T);Path=InPath;PackagePath=InPackage;TableOptions=uint8(T->bStripFromClientBuilds)|(uint8(T->bIgnoreExtraFields)<<1)|(uint8(T->bIgnoreMissingFields)<<2)|(uint8(T->bPreserveExistingValues)<<3);ImportKey=T->ImportKeyField;ImportPath=T->ImportPath_DEPRECATED;ImportData.Reset(T->AssetImportData.Get());Bound=true;if(!Pin(T)||!Pin(T->GetPackage())||!Pin(T->GetClass())||(ImportData.IsValid()&&!Pin(ImportData.Get())))return false;
        for(const auto& P:T->GetRowMap()){if(P.Key.IsNone()||!P.Value)return Bad(TEXT("native_row_memory"));Rows.Emplace(P.Key,P.Value);}RowsReady=true;const auto Copy=Rows;for(const auto& P:Copy){auto V=MakeShared<FJsonObject>();if(!TypedRow(P.Value,V))return false;Values.Add(P.Key,V);}return Current();
    }
    bool Accept(FName Key,bool Remove,const uint8* Returned,bool Import=false)
    {
        if(!Current(false))return false;const bool Existed=Values.Contains(Key);const int32 Delta=Import?0:Remove?-1:1;const auto& Map=Table->GetRowMap();if(Map.Num()!=Rows.Num()+Delta)return Bad(TEXT("native_row_transition_count"));TArray<TPair<FName,const uint8*>> Next;TMap<FName,TSharedRef<FJsonObject>> NextValues;int32 Old=0;
        TArray<TPair<FName,const uint8*>> ActualRows;for(const auto& P:Map)ActualRows.Emplace(P.Key,P.Value);
        for(const auto& P:ActualRows){if(Remove&&Old<Rows.Num()&&Rows[Old].Key==Key)++Old;const bool Selected=P.Key==Key&&!Remove;if(Selected){if(P.Value!=Returned||!P.Value)return Bad(TEXT("native_selected_returned_storage"));if(Import){if(Old>=Rows.Num()||Rows[Old].Key!=P.Key||Rows[Old].Value!=P.Value)return Bad(TEXT("native_import_storage"));++Old;}else if(Existed||Old!=Rows.Num())return Bad(TEXT("native_append_order"));}else if(Old>=Rows.Num()||Rows[Old].Key!=P.Key||Rows[Old].Value!=P.Value)return Bad(TEXT("native_retained_storage"));auto V=MakeShared<FJsonObject>();if(!TypedRow(P.Value,V))return false;if(!Selected){const auto* Before=Values.Find(P.Key);if(!Before||!MirrorReadLiteralEqual(*Before,V))return Bad(TEXT("native_retained_value"),P.Key.ToString());++Old;}Next.Emplace(P.Key,P.Value);NextValues.Add(P.Key,V);}
        if(Remove&&Old<Rows.Num()&&Rows[Old].Key==Key)++Old;if(Old!=Rows.Num()||(Remove&&Map.Contains(Key)))return Bad(TEXT("native_retired_membership"));Rows=MoveTemp(Next);Values=MoveTemp(NextValues);auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("row_name"),Key.ToString());J->SetBoolField(TEXT("exists"),!Remove);J->SetBoolField(TEXT("import_observed"),Import);J->SetNumberField(TEXT("row_count"),Rows.Num());if(!Remove){const auto* V=Values.Find(Key);if(V)J->SetObjectField(TEXT("actual_values"),*V);}R.Data->SetObjectField(Import?TEXT("native_row_import_prefix"):TEXT("native_row_helper_prefix"),J);return Current();
    }
    bool TargetFlagsAfter(bool Save=false)
    {
        if(!Context())return Bad(TEXT("returned_context"));for(auto& E:Objects){auto* O=E.Hold.Get();if(!Loaded(O))return Bad(TEXT("returned_object"));if(O->GetFlags()==E.Flags)continue;const uint32 A=uint32(E.Flags),B=uint32(O->GetFlags()),LoadedBits=uint32(RF_WasLoaded|RF_LoadCompleted);const bool Owned=Bound&&O->GetPackage()==Table->GetPackage();const bool PackageTransaction=Owned&&O==Table->GetPackage()&&GUndo&&O->GetPackage()->IsDirty()&&!(A&uint32(RF_Transactional))&&B==(A|uint32(RF_Transactional));const bool Saved=Save&&Owned&&!(A&~B)&&!((B&~A)&~LoadedBits);if(!PackageTransaction&&!Saved)return Bad(TEXT("unexpected_object_flags"),LexToString(A),LexToString(B));E.Flags=O->GetFlags();}return Current(false);
    }
    bool TransactionCurrent()
    {if(!TransactionReady)return true;if(!Context()||!Loaded(Transactor.Get())||!Editor->Trans.IsResolved()||Editor->Trans.Get()!=Transactor.Get()||Transactor->GetClass()!=UTransBuffer::StaticClass()||Transactor->ActiveCount!=TransactionDepth||Transactor->UndoCount||Transactor->UndoBuffer.IsEmpty()||&Transactor->UndoBuffer.Last().Get()!=OwnTransaction)return false;const auto C=OwnTransaction->GetContext();return C.TransactionId==TransactionId&&C.OperationId==OperationId&&C.Context==TransactionContext;}
    bool BeginTransaction()
    {
        if(!Current())return false;const bool Can=Editor->CanTransact();if(!Current())return false;if(!Can||GIsTransacting)return true;if(!Editor->Trans.IsResolved())return Bad(TEXT("transaction_slot"));auto* T=Editor->Trans.Get();TStrongObjectPtr<UTransactor> Hold(T);if(!Current()||!Loaded(T)||T->GetClass()!=UTransBuffer::StaticClass())return Bad(TEXT("transaction_class_pending"),TEXT("native UTransBuffer"),TEXT("custom or unavailable"),false);Transactor.Reset(static_cast<UTransBuffer*>(T));const int32 Before=Transactor->ActiveCount;if(Before<0||Before==MAX_int32||(Before&&(Transactor->UndoCount||Transactor->UndoBuffer.IsEmpty())))return Bad(TEXT("transaction_depth"));const FTransaction* Prior=Before?&Transactor->UndoBuffer.Last().Get():nullptr;FGuid PriorId,PriorOperation;if(Prior){const auto C=Prior->GetContext();TransactionContext=C.Context;PriorId=C.TransactionId;PriorOperation=C.OperationId;}else TransactionContext=TEXT("CubelessUR_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
        Count(R,TEXT("outer_transaction_calls"));TransactionIndex=Editor->BeginTransaction(*TransactionContext,FText::FromString(TEXT("DataTable native row operation")),Table.Get());R.Data->SetNumberField(TEXT("native_transaction_begin_index"),TransactionIndex);
        if(!Context()||!Editor->Trans.IsResolved()||Editor->Trans.Get()!=Transactor.Get()||!Loaded(Transactor.Get())||TransactionIndex!=Before||Transactor->ActiveCount!=Before+1||Transactor->UndoCount||Transactor->UndoBuffer.IsEmpty())return Bad(TEXT("transaction_begin_return"));OwnTransaction=&Transactor->UndoBuffer.Last().Get();const auto C=OwnTransaction->GetContext();if(C.Context!=TransactionContext||(Prior&&(OwnTransaction!=Prior||C.TransactionId!=PriorId||C.OperationId!=PriorOperation)))return Bad(TEXT("transaction_begin_identity"));TransactionId=C.TransactionId;OperationId=C.OperationId;TransactionDepth=Before+1;TransactionReady=true;return Current();
    }
    bool EndTransaction(bool ReadEndpoint=true)
    {
        if(TransactionIndex==INDEX_NONE)return !Stopped;if(Stopped||!TransactionCurrent()){R.Data->SetBoolField(TEXT("native_transaction_end_skipped_unknown"),true);return Bad(TEXT("transaction_end_current"));}const int32 Before=TransactionIndex;const int32 Returned=Editor->EndTransaction();R.Data->SetNumberField(TEXT("native_transaction_end_depth"),Returned);TransactionIndex=INDEX_NONE;TransactionReady=false;if(!Context()||!Loaded(Transactor.Get())||!Editor->Trans.IsResolved()||Editor->Trans.Get()!=Transactor.Get()||Returned!=Before+1||Transactor->ActiveCount!=Before)return Bad(TEXT("transaction_end_return"));if(Before){if(Transactor->UndoBuffer.IsEmpty()||&Transactor->UndoBuffer.Last().Get()!=OwnTransaction)return Bad(TEXT("transaction_parent_identity"));const auto C=OwnTransaction->GetContext();if(C.TransactionId!=TransactionId||C.OperationId!=OperationId||C.Context!=TransactionContext)return Bad(TEXT("transaction_parent_context"));}return !ReadEndpoint||Current(true,true);
    }
    bool PackageOwned()
    {if(!Current())return false;TArray<UObject*> Exports,Owned;GetObjectsWithPackage(Table->GetPackage(),Exports,EGetObjectsFlags::IncludeNestedObjects);if(!Current()||Exports.Num()>MaxObjects)return false;GetObjectsWithOuter(Table.Get(),Owned,EGetObjectsFlags::IncludeNestedObjects);if(!Current())return false;for(auto* O:Exports)if(O!=Table.Get()&&!Owned.Contains(O))return Bad(TEXT("native_foreign_save_export"));return true;}
    bool AfterModify(bool,ITransaction*,bool)
    {TMap<FString,uint32> Before;for(const auto& E:Objects)Before.Add(E.Path,uint32(E.Flags));if(!TargetFlagsAfter())return false;for(const auto& E:Objects)if(Before.FindRef(E.Path)!=uint32(E.Flags))ModifyFlags.Add(E.Path,{Before.FindRef(E.Path),uint32(E.Flags)});return Current();}
    bool AfterModifyDerivedMetadata(FName BeforeName,FTopLevelAssetPath BeforePath,ITransaction*,bool Returned)
    {
        if(!Current())return false;const FString OldName=BeforeName.ToString(),NewName=Table->RowStructName_DEPRECATED.ToString();const FString OldPath=BeforePath.IsNull()?TEXT("None"):BeforePath.ToString(),NewPath=Table->RowStructPathName.IsNull()?TEXT("None"):Table->RowStructPathName.ToString();if(!OldName.Equals(NewName,ESearchCase::CaseSensitive)||!OldPath.Equals(NewPath,ESearchCase::CaseSensitive)){if(!Returned||!TransactionCurrent()||!NewName.Equals(Type->GetFName().ToString(),ESearchCase::CaseSensitive)||!NewPath.Equals(Type->GetStructPathName().ToString(),ESearchCase::CaseSensitive))return Bad(TEXT("row_struct_serializer_metadata"));if(OldName!=NewName)ModifyDerivedMetadata.Add(TEXT("/RowStructName/values/0"),{OldName,NewName});if(OldPath!=NewPath)ModifyDerivedMetadata.Add(TEXT("/RowStructPathName/values/0"),{OldPath,NewPath});}return Current();
    }
    bool AfterSave(bool Saved,UPackage*,EObjectFlags,bool,ITransaction*)
    {if(!Context())return false;if(!Saved)return Current();TMap<FString,uint32> Before;for(const auto& E:Objects)Before.Add(E.Path,uint32(E.Flags));if(!TargetFlagsAfter(true))return false;for(const auto& E:Objects)if(Before.FindRef(E.Path)!=uint32(E.Flags))SaveFlags.Add(E.Path,{Before.FindRef(E.Path),uint32(E.Flags)});return Current();}
    bool AcceptRows(FName Key,bool Remove,const uint8* Returned,bool Import=false){return Accept(Key,Remove,Returned,Import);}
    bool ImportNamesBounded(const TSharedPtr<FJsonObject>& Requested)
    {
        if(!Current())return false;int32 Cells=0;return ImportFields(Type.Get(),Requested,Cells)&&Current();
    }
    bool ImportFields(UScriptStruct* S,const TSharedPtr<FJsonObject>& Requested,int32& Cells,int32 Depth=0)
    {
        if(!Requested||Depth>16||++Cells>16384)return Bad(TEXT("native_input_bound"));for(TFieldIterator<FProperty> It(S);It;++It){const auto* E=Fields.FindByPredicate([&](const FFieldRole& V){return V.P==*It;});if(!E)return Bad(TEXT("native_input_descriptor"));const FString Name=It->GetAuthoredName();if(!Current())return false;const auto V=Requested->TryGetField(Name);if(!V||V->Type==EJson::Null)continue;if(!ImportValue(*E,V,Cells,Depth+1))return false;}return true;
    }
    bool ImportValue(const FFieldRole& E,const TSharedPtr<FJsonValue>& V,int32& Cells,int32 Depth)
    {
        if(!V||Depth>16||Cells>16384)return Bad(TEXT("native_input_bound"));
        const bool Container=(E.Kind==FArrayProperty::StaticClass()||E.Kind==FSetProperty::StaticClass())&&E.Dim==1;
        if(V->Type==EJson::Array&&!Container){const auto& Items=V->AsArray();const int32 ToRead=FMath::Clamp(Items.Num(),0,E.Dim);for(int32 I=0;I<ToRead;++I)if(!ImportScalar(E,Items[I],Cells,Depth+1))return false;return true;}
        return ImportScalar(E,V,Cells,Depth);
    }
    bool ImportScalar(const FFieldRole& E,const TSharedPtr<FJsonValue>& V,int32& Cells,int32 Depth)
    {
        if(!V||Depth>16||++Cells>16384)return Bad(TEXT("native_input_bound"));if((E.Kind==FNameProperty::StaticClass()||E.Enum)&&V->Type==EJson::String){const FString Text=V->AsString();bool Nul=false;for(TCHAR C:Text)Nul|=C==TCHAR('\0');if(Text.Len()>=NAME_SIZE||Nul)return Bad(TEXT("native_name_input_bound"),TEXT("safe FName/enum text"),TEXT("too long or nul"),false);}if(E.Kind==FObjectProperty::StaticClass()&&V->Type==EJson::String&&!CleanPath(V->AsString()))return Bad(TEXT("native_reference_forbidden"),TEXT("runtime-independent native asset reference"),V->AsString(),false);
        if((E.Kind==FSoftObjectProperty::StaticClass()||E.Kind==FSoftClassProperty::StaticClass())&&V->Type==EJson::String&&!SoftReferencePath(V->AsString()))return Bad(TEXT("native_soft_reference_forbidden"),TEXT("runtime-independent soft asset/class reference before row writer"),V->AsString(),false);
        if(E.Struct&&V->Type==EJson::Object)return ImportFields(E.Struct,V->AsObject(),Cells,Depth+1);if((E.Kind==FArrayProperty::StaticClass()||E.Kind==FSetProperty::StaticClass())&&V->Type==EJson::Array){const auto* Inner=Fields.FindByPredicate([&](const FFieldRole& F){return F.P==E.Inner;});if(!Inner||V->AsArray().Num()>4096)return Bad(TEXT("native_input_array_bound"));for(const auto& Child:V->AsArray())if(!ImportValue(*Inner,Child,Cells,Depth+1))return false;}if(E.Kind==FMapProperty::StaticClass()&&V->Type==EJson::Object){const auto* Key=Fields.FindByPredicate([&](const FFieldRole& F){return F.P==E.Key;});const auto* Value=Fields.FindByPredicate([&](const FFieldRole& F){return F.P==E.Value;});if(!Key||!Value||V->AsObject()->Values.Num()>4096)return Bad(TEXT("native_input_map_bound"));for(const auto& Child:V->AsObject()->Values){if(!ImportValue(*Key,MakeShared<FJsonValueString>(FString(Child.Key.ToView())),Cells,Depth+1)||!ImportValue(*Value,Child.Value,Cells,Depth+1))return false;}}return true;
    }
};
bool NativeRowAdmit(FNativeRowScope* S){return !S||S->Current();}

bool NativeRowCapture(FContext& C,FObservation& O,FResult& R,const TCHAR* Field)
{
    auto& S=*C.Native;O=FObservation{};R.Data->SetObjectField(Field,Observation(O));if(!S.Current(true,true)||!S.Source(O.Source)||!S.CompositeSnapshot(O.Source))return false;auto* T=C.Table.Get();O.State->SetObjectField(TEXT("identity"),Identity(T));O.State->SetStringField(TEXT("row_struct"),C.Type->GetPathName());O.State->SetNumberField(TEXT("row_count"),T->GetRowMap().Num());O.State->SetBoolField(TEXT("package_dirty"),T->GetPackage()->IsDirty());auto Props=MakeShared<FJsonObject>();O.State->SetObjectField(TEXT("properties"),Props);if(!Properties(T,Props,nullptr,nullptr,&S))return false;
    auto RowJson=MakeShared<FJsonObject>();O.State->SetObjectField(TEXT("rows"),RowJson);TArray<TSharedPtr<FJsonValue>> Order;int32 Chars=0;const auto Rows=S.Rows;for(const auto& P:Rows){auto V=MakeShared<FJsonObject>();if(!S.Export(P.Key,P.Value,V,false))return false;FString Text;if(!FJsonSerializer::Serialize(V,TJsonWriterFactory<>::Create(&Text))||(Chars+=Text.Len())>MaxText)return S.Bad(TEXT("row_text_bound"));RowJson->SetObjectField(P.Key.ToString(),V);Order.Add(MakeShared<FJsonValueString>(P.Key.ToString()));}O.State->SetArrayField(TEXT("row_iteration_order"),Order);
    TArray<UObject*> Exports;GetObjectsWithOuter(T,Exports,EGetObjectsFlags::IncludeNestedObjects);if(!S.Current()||Exports.Num()>MaxObjects)return false;Exports.Insert(T,0);auto Owned=MakeShared<FJsonObject>();O.State->SetObjectField(TEXT("owned_exports"),Owned);
    for(auto* E:Exports){if(!S.Pin(E)||!S.Current())return false;if(E!=T){auto J=MakeShared<FJsonObject>();Owned->SetObjectField(E->GetPathName(),J);J->SetObjectField(TEXT("identity"),Identity(E));auto P=MakeShared<FJsonObject>();J->SetObjectField(TEXT("properties"),P);if(!Properties(E,P,nullptr,nullptr,&S))return false;}TArray<UObject*> Refs;FReferenceFinder Finder(Refs,nullptr,false,true,false,true);Finder.FindReferences(E);if(!S.Current())return false;for(auto* Ref:Refs){TStrongObjectPtr<UObject> Hold(Ref);if(!S.Pin(Ref)||!S.Current())return false;O.References->SetObjectField(Ref->GetPathName(),Identity(Ref));}}
    if(!Disk(C.Package,O.DiskState)||!S.Current()||!Dirty(C.Package,O.OtherDirty)||!S.Current(true,true))return false;O.Complete=true;R.Data->SetObjectField(Field,Observation(O));return true;
}


bool StringUdsAdmit(FStringUdsScope* S){return !S||S->Raw();}
bool StringUdsCapture(FContext& C,FObservation& O,FResult& R,const TCHAR* Field)
{
    auto& S=*C.Uds;O=FObservation{};R.Data->SetObjectField(Field,Observation(O));if(!S.StableSource())return false;auto* T=C.Table.Get();
    if(!S.Source(O.Source,true))return false;if(!MirrorReadLiteralEqual(S.SourceBaseline,O.Source))return S.Stop(TEXT("datatable_string_uds_source_snapshot_changed"),TEXT("Source disk/default/editor-data changed since admission; no next reader/writer."));O.State->SetObjectField(TEXT("identity"),Identity(T));O.State->SetStringField(TEXT("row_struct"),S.TypePath);O.State->SetNumberField(TEXT("row_count"),T->GetRowMap().Num());O.State->SetBoolField(TEXT("package_dirty"),T->GetPackage()->IsDirty());
    auto Props=MakeShared<FJsonObject>();O.State->SetObjectField(TEXT("properties"),Props);if(!Properties(T,Props,nullptr,&S))return S.Stopped?false:S.Stop(TEXT("datatable_string_uds_table_fields_unobservable"),TEXT("Selected table field export incomplete."));auto Rows=MakeShared<FJsonObject>();O.State->SetObjectField(TEXT("rows"),Rows);TArray<TSharedPtr<FJsonValue>> Order;int32 Chars=0;
    for(const auto& P:T->GetRowMap()){auto V=MakeShared<FJsonObject>();Rows->SetObjectField(P.Key.ToString(),V);const bool Exported=FJsonObjectConverter::UStructToJsonObject(C.Type,P.Value,V,0,0);if(!S.StableSource())return false;if(!Exported)return S.Stop(TEXT("datatable_row_serialize_failed"),TEXT("Actual bounded UDS row export failed."));FString Text;if(!FJsonSerializer::Serialize(V,TJsonWriterFactory<>::Create(&Text))||(Chars+=Text.Len()+P.Key.ToString().Len())>MaxText)return S.Stop(TEXT("datatable_string_uds_text_bound"),TEXT("Whole table row text exceeds finite bound."));Order.Add(MakeShared<FJsonValueString>(P.Key.ToString()));}O.State->SetArrayField(TEXT("row_iteration_order"),Order);
    auto Owned=MakeShared<FJsonObject>();O.State->SetObjectField(TEXT("owned_exports"),Owned);TArray<UObject*> Exports=S.Owned;Exports.Insert(T,0);int32 Count=0;
    for(auto* E:Exports){if(E!=T){auto Item=MakeShared<FJsonObject>();Owned->SetObjectField(E->GetPathName(),Item);Item->SetObjectField(TEXT("identity"),Identity(E));auto Fields=MakeShared<FJsonObject>();Item->SetObjectField(TEXT("properties"),Fields);if(!Properties(E,Fields,nullptr,&S))return S.Stopped?false:S.Stop(TEXT("datatable_string_uds_owned_fields_unobservable"),TEXT("Selected native owned field export incomplete."));}TArray<UObject*> Refs;FReferenceFinder Finder(Refs,nullptr,false,true,false,true);Finder.FindReferences(E);if(!S.StableSource())return false;for(auto* Ref:Refs){if(++Count>16384||!Loaded(Ref)||!CleanPath(Ref->GetPathName())||!CleanPath(Ref->GetClass()->GetPathName()))return S.Stop(TEXT("datatable_string_uds_reference_unsupported"),TEXT("Loaded safe current asset references only."));O.References->SetObjectField(Ref->GetPathName(),Identity(Ref));}}
    if(!Disk(C.Package,O.DiskState)||!Dirty(C.Package,O.OtherDirty))return S.Stop(TEXT("datatable_string_uds_endpoint_unobservable"),TEXT("Exact target disk/other dirty snapshot incomplete."));if(!S.StableSource())return false;O.Complete=true;R.Data->SetObjectField(Field,Observation(O));return true;
}

bool Capture(FContext& C, FObservation& O, FResult& R, const TCHAR* Field)
{
    if(C.Native)return NativeRowCapture(C,O,R,Field);
    if(C.Uds)return StringUdsCapture(C,O,R,Field);
    O = FObservation{}; R.Data->SetObjectField(Field, Observation(O)); if(!MirrorReadAdmit(C.MirrorRead.Get())) { return false; } UDataTable* T = C.Table.Get();
    if (!Loaded(T) || (C.MirrorRead ? T->GetClass()!=UMirrorDataTable::StaticClass() : T->GetClass()!=UDataTable::StaticClass()) || T->GetOuter() != T->GetOutermost() || T->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject) || T->GetRowStruct() != C.Type || T->GetPathName() != C.Path || FindObject<UDataTable>(nullptr, *C.Path) != T || T->GetRowMap().Num() > MaxRowsObserved) { return false; }
    if (!Sources(C.Type, O.Source) || !MirrorReadAdmit(C.MirrorRead.Get())) { return false; }
    O.State->SetObjectField(TEXT("identity"), Identity(T)); O.State->SetStringField(TEXT("row_struct"), C.Type->GetPathName()); O.State->SetNumberField(TEXT("row_count"), T->GetRowMap().Num()); O.State->SetBoolField(TEXT("package_dirty"), T->GetOutermost()->IsDirty());
    auto Props = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("properties"), Props); if (!Properties(T, Props, C.MirrorRead.Get())) { return false; }
    auto Rows = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("rows"), Rows); TArray<TSharedPtr<FJsonValue>> Order;
    int32 RowChars = 0;
    for (const auto& Pair : T->GetRowMap()) { if (Pair.Key.IsNone() || !Pair.Value) { return false; } auto Values = MakeShared<FJsonObject>(); Rows->SetObjectField(Pair.Key.ToString(), Values); const bool Exported=FJsonObjectConverter::UStructToJsonObject(C.Type, Pair.Value, Values, 0, 0); if (!MirrorReadAdmit(C.MirrorRead.Get()) || !Exported) { return false; } FString RowJson; if (!FJsonSerializer::Serialize(Values, TJsonWriterFactory<>::Create(&RowJson)) || (RowChars += RowJson.Len() + Pair.Key.ToString().Len()) > MaxText) { return false; } Order.Add(MakeShared<FJsonValueString>(Pair.Key.ToString())); }
    O.State->SetArrayField(TEXT("row_iteration_order"), Order);
    TArray<UObject*> Exports; GetObjectsWithOuter(T, Exports, EGetObjectsFlags::IncludeNestedObjects); Exports.Insert(T, 0); if (Exports.Num() > MaxObjects) { return false; } int32 ReferenceCount = 0;
    auto Owned = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("owned_exports"), Owned);
    for (UObject* Export : Exports)
    {
        if (!Loaded(Export) || !CleanPath(Export->GetPathName()) || !CleanPath(Export->GetClass()->GetPathName())) { return false; }
        if (Export != T) { auto Row = MakeShared<FJsonObject>(); Owned->SetObjectField(Export->GetPathName(), Row); Row->SetObjectField(TEXT("identity"), Identity(Export)); auto P = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("properties"), P); if (!Properties(Export, P, C.MirrorRead.Get())) { return false; } }
        TArray<UObject*> Refs; FReferenceFinder Finder(Refs, nullptr, false, true, false, true); Finder.FindReferences(Export); if(!MirrorReadAdmit(C.MirrorRead.Get())) { return false; }
        for (UObject* Ref : Refs) { if (++ReferenceCount > 16384 || !Loaded(Ref) || !CleanPath(Ref->GetPathName()) || !CleanPath(Ref->GetClass()->GetPathName())) { return false; } O.References->SetObjectField(Ref->GetPathName(), Identity(Ref)); }
    }
    if(C.MirrorRead&&!C.MirrorRead->Snapshot(O.State,O.Source)) { return false; }
    if (!Disk(C.Package, O.DiskState) || !Dirty(C.Package, O.OtherDirty) || !MirrorReadAdmit(C.MirrorRead.Get())) { return false; } O.Complete = true; R.Data->SetObjectField(Field, Observation(O)); return true;
}
bool Resolve(const FString& RequestedPath, bool Mutable, FContext& C, FResult& R)
{
    if(StringUdsWriterRunning||NativeRowWriterRunning)return Fail(R,TEXT("datatable_string_uds_writer_busy"),TEXT("String1 UDS writer active; nested DataTable dispatch rejected before load."));
    if (!IsEditorGameThread() || IsPlaySessionActive()) { return Fail(R, TEXT("datatable_editor_context_required"), TEXT("Editor game thread without PIE/SIE required.")); }
    C.Path = Normalize(RequestedPath); C.Package = FPackageName::ObjectPathToPackageName(C.Path); R.Data->SetStringField(TEXT("datatable"), C.Path);
    if (!AssetPath(C.Path, Mutable)) { return Fail(R, TEXT("datatable_path_invalid"), TEXT("Bounded exact native Game/Engine table object path required; writes are Game-only.")); }
    C.MirrorRead=MakeUnique<FMirrorReadScope>(R,Mutable);if(!C.MirrorRead->Start()){return false;}
    R.Stage = TEXT("load_datatable"); auto Receipt = MakeShared<FJsonObject>(); Receipt->SetStringField(TEXT("requested_path"), C.Path); Receipt->SetBoolField(TEXT("attempted"), true); R.Data->SetArrayField(TEXT("asset_load_calls"), { MakeShared<FJsonValueObject>(Receipt) }); Count(R, TEXT("asset_load_count")); UObject* Object = UEditorAssetLibrary::LoadAsset(C.Path); if(!MirrorReadAdmit(C.MirrorRead.Get())){Receipt->SetBoolField(TEXT("returned"),true);return false;} Receipt->SetObjectField(TEXT("returned_object"), Identity(Object)); C.Table.Reset(Cast<UDataTable>(Object));
    UDataTable* T = C.Table.Get(); C.Type = T ? const_cast<UScriptStruct*>(T->GetRowStruct()) : nullptr;
    if(Loaded(T)&&T->GetClass()==UCompositeDataTable::StaticClass())
    {
        C.MirrorRead.Reset();C.Native=MakeUnique<FNativeRowScope>(R);
        if(!C.Native->BindComposite(T,Mutable))return false;
        R.Stage=TEXT("preflight");return true;
    }
    if(Loaded(C.Type)&&C.Type->GetClass()==UUserDefinedStruct::StaticClass())
    {
        if(!MirrorReadAdmit(C.MirrorRead.Get()))return false;
        if(!Loaded(T)||T->GetClass()!=UDataTable::StaticClass()||!Loaded(T->GetPackage())||T->GetOuter()!=T->GetPackage()
            ||!T->GetPathName().Equals(C.Path,ESearchCase::CaseSensitive)||!T->GetPackage()->GetName().Equals(C.Package,ESearchCase::CaseSensitive))
            return Fail(R,TEXT("datatable_string_uds_requested_target_changed"),TEXT("Loaded table no longer matches the exact requested path/direct package; no source binding, target writer or save."),true);
        C.MirrorRead.Reset();C.Uds=MakeUnique<FStringUdsScope>(R,true,false,true);
        if(!C.Uds->Start()||!C.Uds->BindType(static_cast<UUserDefinedStruct*>(C.Type))||!C.Uds->BindTable(T,C.Path,C.Package))return false;
        R.Stage=TEXT("preflight");return true;
    }
    if(Loaded(T)&&T->GetClass()==UDataTable::StaticClass()&&Loaded(C.Type)&&C.Type->GetClass()==UScriptStruct::StaticClass()&&C.Type->GetPathName()!=TagRowPath&&C.Type->GetPathName()!=MirrorRowPath)
    {
        C.MirrorRead.Reset();C.Native=MakeUnique<FNativeRowScope>(R);
        if(!C.Native->BindType(C.Type,T,C.Path,C.Package)||!C.Native->StableSource())return false;
        R.Stage=TEXT("preflight");return true;
    }
    if (!Loaded(T) || (T->GetClass()!=UDataTable::StaticClass()&&T->GetClass()!=UMirrorDataTable::StaticClass()) || T->GetPathName() != C.Path || T->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn | PKG_PlayInEditor | PKG_EditorOnly) || !Loaded(C.Type) || (C.Type->GetPathName() != TagRowPath && C.Type->GetPathName() != MirrorRowPath)) { return Fail(R, TEXT("datatable_type_unsupported"), TEXT("An admitted native-row UDataTable, readonly UCompositeDataTable, or exact MirrorDataTable profile is required on this branch; custom classes and other row/parent profiles remain incomplete.")); }
    if(T->GetClass()==UMirrorDataTable::StaticClass()){if(!C.MirrorRead||!C.MirrorRead->Bind(static_cast<UMirrorDataTable*>(T),C.Type)){return false;}}else{C.MirrorRead.Reset();}
    R.Stage = TEXT("preflight"); return true;
}
bool RowValues(FContext& C, FName Name, TSharedRef<FJsonObject>& J)
{ if(C.Native){if(!C.Native->StableSource())return false;auto* P=C.Table->GetRowMap().Find(Name);const uint8* Memory=P?*P:nullptr;return Memory&&C.Native->Export(Name,Memory,J)&&C.Native->StableSource();} if(C.Uds){auto& S=*C.Uds;if(!S.StableSource())return false;auto P=C.Table->GetRowMap().Find(Name);const bool Exported=P&&*P&&FJsonObjectConverter::UStructToJsonObject(C.Type,*P,J,0,0);return S.StableSource()&&Exported;} if(!MirrorReadAdmit(C.MirrorRead.Get())){return false;} uint8* const* Ptr = C.Table->GetRowMap().Find(Name); const bool Exported=Ptr && *Ptr && FJsonObjectConverter::UStructToJsonObject(C.Type, *Ptr, J, 0, 0); return MirrorReadAdmit(C.MirrorRead.Get())&&Exported; }
TSharedRef<FJsonObject> RowSnapshot(FContext& C, FName Name)
{
    auto J = MakeShared<FJsonObject>(); J->SetNumberField(TEXT("row_count"), C.Table->GetRowMap().Num()); J->SetStringField(TEXT("row_name"), Name.ToString()); const bool Exists = C.Table->GetRowMap().Contains(Name); J->SetBoolField(TEXT("exists"), Exists);
    if (Exists) { auto Values = MakeShared<FJsonObject>(); const bool Serialized = RowValues(C, Name, Values); J->SetBoolField(TEXT("serialized"), Serialized); J->SetObjectField(TEXT("values"), Values); } return J;
}
void ClearFinal(FResult& R) { for (const TCHAR* K : { TEXT("observations_complete"), TEXT("source_preserved"), TEXT("unselected_state_preserved"), TEXT("independent_readback_passed"), TEXT("persistence_verified") }) { R.Data->SetBoolField(K, false); } }
bool Fences(const FObservation& Before, const FObservation& After, FName Selected, bool Remove, FResult& R)
{
    const bool Complete = Before.Complete && After.Complete; R.Data->SetBoolField(TEXT("observations_complete"), Complete); if (!Complete) { return Fail(R, TEXT("datatable_observation_incomplete"), TEXT("Bounded actual target/source prefix incomplete; no next write/save."), R.Attempted); }
    const bool Source = Equal(Before.Source, After.Source) && Equal(Before.References, After.References); const bool Other = Equal(Before.OtherDirty, After.OtherDirty), File = Equal(Before.DiskState, After.DiskState);
    bool State = true; for (const auto& Field : Before.State->Values) { if (Field.Key.ToView() == TEXT("rows") || Field.Key.ToView() == TEXT("row_count") || Field.Key.ToView() == TEXT("row_iteration_order") || Field.Key.ToView() == TEXT("package_dirty")) { continue; } State &= Equal(Field.Value, After.State->TryGetField(Field.Key.ToView())); }
    const auto X = Before.State->GetObjectField(TEXT("rows")), Y = After.State->GetObjectField(TEXT("rows")); int32 ExpectedCount = X->Values.Num(); bool Existed = false;
    for (const auto& Row : X->Values) { if (FName(*FString(Row.Key.ToView())) == Selected && !Selected.IsNone()) { Existed = true; continue; } State &= Equal(Row.Value, Y->TryGetField(Row.Key.ToView())); }
    if (!Selected.IsNone()) { ExpectedCount += Remove ? -1 : (Existed ? 0 : 1); } State &= Y->Values.Num() == ExpectedCount;
    const auto& OldOrder = Before.State->GetArrayField(TEXT("row_iteration_order")); const auto& NewOrder = After.State->GetArrayField(TEXT("row_iteration_order")); int32 Index = 0;
    for (const auto& Name : OldOrder) { if (Remove && FName(*Name->AsString()) == Selected) { continue; } if (Index >= NewOrder.Num() || !Equal(Name, NewOrder[Index++])) { State = false; } }
    if (!Selected.IsNone() && !Remove && !Existed) { State &= NewOrder.Num() == Index + 1 && FName(*NewOrder.Last()->AsString()) == Selected; } else { State &= NewOrder.Num() == Index; }
    R.Data->SetBoolField(TEXT("source_preserved"), Source); R.Data->SetBoolField(TEXT("unselected_state_preserved"), State && Other); R.Data->SetBoolField(TEXT("disk_preserved_before_save"), File);
    return (Source && State && Other && File) || Fail(R, TEXT("datatable_preservation_failed"), TEXT("Observed unrelated rows/order/metadata/references/source/disk/other-dirty changed; actual callback prefix retained, no save."));
}
bool SavedEqual(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B, const FString& Package, const FString& At = FString())
{
    if (!A || !B || A->Type != B->Type) { return false; } if (At == TEXT("/package_dirty")) { return !B->AsBool(); }
    if (A->Type == EJson::Object) { const auto X = A->AsObject(), Y = B->AsObject(); if (X->Values.Num() != Y->Values.Num()) { return false; } FString Path; const bool Owned = X->TryGetStringField(TEXT("path"), Path) && Path.StartsWith(Package + TEXT(".")); for (const auto& Field : X->Values) { const auto Other = Y->TryGetField(Field.Key.ToView()); if (!Other) { return false; } if (Owned && Field.Key.ToView() == TEXT("object_flags")) { if (Field.Value->Type != EJson::String || Other->Type != EJson::String) { return false; } const uint64 Flags = FCString::Strtoui64(*Field.Value->AsString(), nullptr, 10); if (LexToString(Flags) != Field.Value->AsString() || Other->AsString() != LexToString(Flags | static_cast<uint64>(RF_WasLoaded | RF_LoadCompleted))) { return false; } } else if (!SavedEqual(Field.Value, Other, Package, At + TEXT("/") + FString(Field.Key.ToView()))) { return false; } } return true; }
    if (A->Type == EJson::Array) { const auto& X = A->AsArray(); const auto& Y = B->AsArray(); if (X.Num() != Y.Num()) { return false; } for (int32 I = 0; I < X.Num(); ++I) { if (!SavedEqual(X[I], Y[I], Package, At + TEXT("/") + LexToString(I))) { return false; } } return true; } return Equal(A, B);
}
bool Save(FContext& C, const FObservation& Before, FResult& R)
{
    if (!R.SaveRequested) { return true; } const bool ReadbackBeforeSave = R.Data->GetBoolField(TEXT("independent_readback_passed")); R.Data->SetBoolField(TEXT("before_save_independent_readback_passed"), ReadbackBeforeSave); R.Data->SetBoolField(TEXT("before_save_observations_complete"), Before.Complete); R.Data->SetBoolField(TEXT("before_save_source_preserved"), R.Data->GetBoolField(TEXT("source_preserved"))); R.Data->SetBoolField(TEXT("before_save_unselected_state_preserved"), R.Data->GetBoolField(TEXT("unselected_state_preserved"))); ClearFinal(R);
    R.Stage = TEXT("save_package"); FString File; if (!Before.Complete || !FPackageName::TryConvertLongPackageNameToFilename(C.Package, File, FPackageName::GetAssetPackageExtension())) { return Fail(R, TEXT("datatable_save_preflight_failed"), TEXT("Complete admitted state and exact package filename required before save.")); }
    R.Attempted = true; R.Known = false; R.SaveAttempted = true; Count(R, TEXT("save_attempt_count")); auto Receipt = MakeShared<FJsonObject>(); Receipt->SetStringField(TEXT("asset_path"), C.Path); Receipt->SetStringField(TEXT("filename"), File); Receipt->SetBoolField(TEXT("attempted"), true); Receipt->SetBoolField(TEXT("only_if_dirty"), false); Receipt->SetStringField(TEXT("route"), TEXT("UPackage::SavePackage")); R.Data->SetArrayField(TEXT("save_calls"), { MakeShared<FJsonValueObject>(Receipt) });
    FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError; Args.bSlowTask = false; R.Saved = UPackage::SavePackage(C.Table->GetOutermost(), C.Table.Get(), *File, Args); Receipt->SetBoolField(TEXT("saved"), R.Saved); R.Data->SetBoolField(TEXT("saved"), R.Saved);
    if (R.Saved) { Count(R, TEXT("explicit_clear_dirty_calls")); C.Table->GetOutermost()->SetDirtyFlag(false); }
    FObservation After; const bool Complete = Capture(C, After, R, TEXT("after_save")); R.Data->SetBoolField(TEXT("observations_complete"), Complete); if (!R.Saved || !Complete) { return Fail(R, TEXT("datatable_save_unobservable_or_failed"), TEXT("One direct save receipt and actual prefix retained; no retry or rollback."), true); }
    const bool State = SavedEqual(MakeShared<FJsonValueObject>(Before.State), MakeShared<FJsonValueObject>(After.State), C.Package), Source = SavedEqual(MakeShared<FJsonValueObject>(Before.References), MakeShared<FJsonValueObject>(After.References), C.Package) && Equal(Before.Source, After.Source), Other = Equal(Before.OtherDirty, After.OtherDirty);
    R.Known = true; R.Data->SetBoolField(TEXT("source_preserved"), Source); R.Data->SetBoolField(TEXT("unselected_state_preserved"), State && Other); R.Data->SetBoolField(TEXT("save_state_preserved"), State); R.Data->SetBoolField(TEXT("independent_readback_passed"), State && ReadbackBeforeSave); const bool Persisted = After.DiskState->GetBoolField(TEXT("exists")) && After.DiskState->GetNumberField(TEXT("bytes")) > 0 && !C.Table->GetOutermost()->IsDirty(); R.Data->SetBoolField(TEXT("persistence_verified"), Persisted);
    return (State && Source && Other && Persisted) || Fail(R, TEXT("datatable_saved_readback_failed"), TEXT("Actual successful save remains authoritative but observed persisted-state/source fence failed; do not replay."));
}
bool MirrorImportNamesBounded(const FContext& C,const TSharedRef<FJsonObject>& Values,FResult& R);
// JC: exact specialized row mutation; ordinary UDataTable and the existing read3
// retain their own paths. All comparisons below are pure JSON/current plain-row checks.
bool MirrorMutationRunning=false;
bool MirrorMutationEqual(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B,const FMirrorReadScope& Scope,bool Saved=false,const FString& At=FString(),int32 Depth=0)
{
    if(!A||!B||A->Type!=B->Type||Depth>32)return false;
    if(At==TEXT("/package_dirty"))return B->Type==EJson::Boolean&&(Saved?!B->AsBool():B->AsBool());
    if(A->Type==EJson::Object)
    {
        const auto X=A->AsObject(),Y=B->AsObject();if(!X||!Y||X->Values.Num()!=Y->Values.Num())return false;
        TArray<FString> Keys,OtherKeys;for(const auto& P:X->Values)Keys.Add(FString(P.Key.ToView()));for(const auto& P:Y->Values)OtherKeys.Add(FString(P.Key.ToView()));
        const auto Order=[](const FString& L,const FString& Rhs){return L.Compare(Rhs,ESearchCase::CaseSensitive)<0;};Keys.Sort(Order);OtherKeys.Sort(Order);
        FString Path,OtherPath;const bool Identity=X->TryGetStringField(TEXT("path"),Path)&&Y->TryGetStringField(TEXT("path"),OtherPath)&&Path.Equals(OtherPath,ESearchCase::CaseSensitive);
        for(int32 I=0;I<Keys.Num();++I)
        {
            if(!Keys[I].Equals(OtherKeys[I],ESearchCase::CaseSensitive))return false;
            const auto V=X->TryGetField(Keys[I]),W=Y->TryGetField(Keys[I]);
            if(Identity&&Keys[I].Equals(TEXT("object_flags"),ESearchCase::CaseSensitive)&&V&&W&&V->Type==EJson::String&&W->Type==EJson::String)
            {
                if(Scope.ModifyFlagAdopted&&Path.Equals(Scope.SelectedPackagePath,ESearchCase::CaseSensitive)&&V->AsString()==LexToString(Scope.ModifyFlagsBefore)&&W->AsString()==LexToString(Scope.ModifyFlagsAfter))continue;
                const auto* Transition=Saved?Scope.SavedFlagTransitions.Find(Path):nullptr;
                if(Transition&&V->AsString()==LexToString(Transition->Key)&&W->AsString()==LexToString(Transition->Value))continue;
            }
            if(!MirrorMutationEqual(V,W,Scope,Saved,At+TEXT("/")+Keys[I],Depth+1))return false;
        }
        return true;
    }
    if(A->Type==EJson::Array){const auto& X=A->AsArray();const auto& Y=B->AsArray();if(X.Num()!=Y.Num())return false;for(int32 I=0;I<X.Num();++I)if(!MirrorMutationEqual(X[I],Y[I],Scope,Saved,At+TEXT("/")+LexToString(I),Depth+1))return false;return true;}
    return MirrorReadLiteralEqual(A,B,Depth);
}
bool MirrorMutationEqual(const TSharedRef<FJsonObject>& A,const TSharedRef<FJsonObject>& B,const FMirrorReadScope& Scope,bool Saved=false)
{return MirrorMutationEqual(MakeShared<FJsonValueObject>(A),MakeShared<FJsonValueObject>(B),Scope,Saved);}
bool MirrorMutationFences(FContext& C,const FObservation& Before,const FObservation& After,FName Selected,bool Remove,bool CacheChanged,FResult& R)
{
    auto& S=*C.MirrorRead;if(!S.StableSource())return false;
    if(!Before.Complete||!After.Complete)return Fail(R,TEXT("datatable_mirror_mutation_observation_incomplete"),TEXT("Complete source/target observations are required; actual row prefix retained."),true);
    const bool Source=MirrorMutationEqual(Before.Source,After.Source,S)&&MirrorMutationEqual(Before.References,After.References,S);
    const bool Other=MirrorReadLiteralEqual(Before.OtherDirty,After.OtherDirty),File=MirrorReadLiteralEqual(Before.DiskState,After.DiskState);
    bool State=true;for(const auto& P:Before.State->Values)
    {
        if(P.Key.ToView()==TEXT("rows")||P.Key.ToView()==TEXT("row_count")||P.Key.ToView()==TEXT("row_iteration_order")||P.Key.ToView()==TEXT("package_dirty")||(CacheChanged&&P.Key.ToView()==TEXT("mirror_public_derived_cache")))continue;
        State&=MirrorMutationEqual(P.Value,After.State->TryGetField(P.Key.ToView()),S);
    }
    State&=After.State->GetBoolField(TEXT("package_dirty"));
    const auto X=Before.State->GetObjectField(TEXT("rows")),Y=After.State->GetObjectField(TEXT("rows"));bool Existed=false;int32 ExpectedCount=X->Values.Num();
    for(const auto& P:X->Values){if(!Selected.IsNone()&&FName(*FString(P.Key.ToView()))==Selected){Existed=true;continue;}State&=MirrorReadLiteralEqual(P.Value,Y->TryGetField(P.Key.ToView()));}
    if(!Selected.IsNone())ExpectedCount+=Remove?-1:(Existed?0:1);State&=Y->Values.Num()==ExpectedCount;
    const auto& OldOrder=Before.State->GetArrayField(TEXT("row_iteration_order"));const auto& NewOrder=After.State->GetArrayField(TEXT("row_iteration_order"));int32 Index=0;
    for(const auto& Name:OldOrder){if(Remove&&FName(*Name->AsString())==Selected)continue;if(Index>=NewOrder.Num()||!MirrorReadLiteralEqual(Name,NewOrder[Index++]))State=false;}
    if(!Selected.IsNone()&&!Remove&&!Existed)State&=NewOrder.Num()==Index+1&&FName(*NewOrder.Last()->AsString())==Selected;else State&=NewOrder.Num()==Index;
    R.Data->SetBoolField(TEXT("observations_complete"),true);R.Data->SetBoolField(TEXT("source_preserved"),Source);R.Data->SetBoolField(TEXT("unselected_state_preserved"),State&&Other);R.Data->SetBoolField(TEXT("disk_preserved_before_save"),File);
    R.Data->SetBoolField(TEXT("mirror_actual_scoped_state_changed"),!MirrorReadLiteralEqual(Before.State,After.State));
    return (Source&&Other&&File&&State)||Fail(R,TEXT("datatable_mirror_mutation_preservation_failed"),TEXT("Observed nonselected rows/order, authored settings/source/references/disk/other dirty changed; actual callback/row prefix retained, no save."));
}
bool MirrorMutationSave(FContext& C,const FObservation& Before,FResult& R,FObservation& Final)
{
    auto& S=*C.MirrorRead;Final=Before;if(!R.SaveRequested)return true;
    if(!S.StableSource()||!S.PublicCachesMatch()||!S.PackageOwnedForSave())return false;
    const bool Readback=R.Data->GetBoolField(TEXT("independent_readback_passed"));R.Data->SetBoolField(TEXT("before_save_independent_readback_passed"),Readback);
    R.Data->SetBoolField(TEXT("before_save_observations_complete"),Before.Complete);R.Data->SetBoolField(TEXT("before_save_source_preserved"),R.Data->GetBoolField(TEXT("source_preserved")));R.Data->SetBoolField(TEXT("before_save_unselected_state_preserved"),R.Data->GetBoolField(TEXT("unselected_state_preserved")));ClearFinal(R);
    FString File;R.Stage=TEXT("save_package");if(!Before.Complete||!FPackageName::TryConvertLongPackageNameToFilename(C.Package,File,FPackageName::GetAssetPackageExtension()))return Fail(R,TEXT("datatable_save_preflight_failed"),TEXT("Complete exact Mirror target state/package filename required."));
    // Revalidate the entire pre-save observation after observers/export callbacks.
    FObservation Current;if(!Capture(C,Current,R,TEXT("mirror_before_save_fence"))||!S.StableSource()||!MirrorReadLiteralEqual(Before.State,Current.State)||!MirrorReadLiteralEqual(Before.Source,Current.Source)||!MirrorReadLiteralEqual(Before.References,Current.References)||!MirrorReadLiteralEqual(Before.DiskState,Current.DiskState)||!MirrorReadLiteralEqual(Before.OtherDirty,Current.OtherDirty))
        return S.Stop(TEXT("datatable_mirror_mutation_presave_changed"),TEXT("Selected state changed before save; no save dispatch."));
    R.Attempted=true;R.Known=false;R.SaveAttempted=true;Count(R,TEXT("save_attempt_count"));auto Receipt=MakeShared<FJsonObject>();
    Receipt->SetStringField(TEXT("asset_path"),C.Path);Receipt->SetStringField(TEXT("filename"),File);Receipt->SetBoolField(TEXT("attempted"),true);Receipt->SetBoolField(TEXT("only_if_dirty"),false);Receipt->SetStringField(TEXT("route"),TEXT("UPackage::SavePackage"));R.Data->SetArrayField(TEXT("save_calls"),{MakeShared<FJsonValueObject>(Receipt)});
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;Args.bSlowTask=false;
    const bool SavedReturn=UPackage::SavePackage(C.Table->GetOutermost(),C.Table.Get(),*File,Args);
    R.Saved|=SavedReturn;Receipt->SetBoolField(TEXT("saved"),SavedReturn);R.Data->SetBoolField(TEXT("saved"),R.Saved);
    // Save truth is plain prefix, adopted before any callback/context/identity getter.
    if(!S.AfterSave(SavedReturn)||!S.PackageOwnedForSave())return false;
    if(SavedReturn){Count(R,TEXT("explicit_clear_dirty_calls"));C.Table->GetOutermost()->SetDirtyFlag(false);if(!S.StableSource())return false;}
    const bool Observed=Capture(C,Final,R,TEXT("after_save"));if(!Observed||!S.StableSource())return Fail(R,TEXT("datatable_save_unobservable_or_failed"),TEXT("Actual direct save receipt retained; current post-save observation incomplete, no retry."),true);
    if(!S.PublicCachesMatch())return false;
    if(!SavedReturn)return Fail(R,TEXT("datatable_save_unobservable_or_failed"),TEXT("Actual SavePackage returned false; row work retained without retry/rollback."),true);
    const bool State=MirrorMutationEqual(Before.State,Final.State,S,true),Source=MirrorMutationEqual(Before.Source,Final.Source,S,true)&&MirrorMutationEqual(Before.References,Final.References,S,true),Other=MirrorReadLiteralEqual(Before.OtherDirty,Final.OtherDirty);
    const bool Persisted=Final.DiskState->GetBoolField(TEXT("exists"))&&Final.DiskState->GetNumberField(TEXT("bytes"))>0&&!C.Table->GetPackage()->IsDirty();
    R.Known=true;R.Data->SetBoolField(TEXT("observations_complete"),true);R.Data->SetBoolField(TEXT("source_preserved"),Source);R.Data->SetBoolField(TEXT("unselected_state_preserved"),State&&Other);R.Data->SetBoolField(TEXT("save_state_preserved"),State);R.Data->SetBoolField(TEXT("independent_readback_passed"),State&&Readback);R.Data->SetBoolField(TEXT("persistence_verified"),Persisted);
    return (State&&Source&&Other&&Persisted)||Fail(R,TEXT("datatable_saved_readback_failed"),TEXT("Actual SavedTrue retained; selected Mirror state/source/cache/disk fence failed."));
}
FString MirrorRowMutation(FContext& C,const FObservation& Before,FResult& R,const FString& Name,const TSharedPtr<FJsonObject>& Values,bool Remove)
{
    auto& S=*C.MirrorRead;if(MirrorMutationRunning){Fail(R,TEXT("datatable_mirror_mutation_busy"),TEXT("This concrete Mirror writer is already active; no nested row SDK."));return Encode(R);}
    TGuardValue<bool> Busy(MirrorMutationRunning,true);if(!S.StableSource()||(!Remove&&!MirrorImportNamesBounded(C,Values.ToSharedRef(),R)))return Encode(R);
    const FName Key(*Name);const bool Existed=C.Table->GetRowMap().Contains(Key);R.Data->SetStringField(TEXT("row_name"),Name);R.Data->SetObjectField(TEXT("before"),RowSnapshot(C,Key));if(!S.StableSource())return Encode(R);
    if(Remove&&!Existed){FObservation Missing;if(!Capture(C,Missing,R,TEXT("after_observation"))||!S.StableSource())return Encode(R);const bool Same=MirrorReadLiteralEqual(Before.State,Missing.State)&&MirrorReadLiteralEqual(Before.Source,Missing.Source)&&MirrorReadLiteralEqual(Before.References,Missing.References)&&MirrorReadLiteralEqual(Before.DiskState,Missing.DiskState)&&MirrorReadLiteralEqual(Before.OtherDirty,Missing.OtherDirty);if(!Same){S.Stop(TEXT("datatable_mirror_mutation_missing_changed"),TEXT("Missing-row observation changed; no row writer/save."));return Encode(R);}Fail(R,TEXT("datatable_row_not_found"),TEXT("Row not found; no Modify/remove/save."));return Encode(R);}
    auto OldValues=MakeShared<FJsonObject>();const bool Matched=!Remove&&Existed&&RowValues(C,Key,OldValues)&&ContainsExpected(MakeShared<FJsonValueObject>(Values.ToSharedRef()),MakeShared<FJsonValueObject>(OldValues));
    if(!S.StableSource())return Encode(R);if(!Remove)R.Data->SetBoolField(TEXT("already_matched"),Matched);
    FObservation Final;bool HasFinal=false;
    auto Run=[&]()
    {
        R.Stage=Remove?TEXT("remove_row"):TEXT("apply_row");R.Attempted=true;R.Known=false;Count(R,TEXT("outer_transaction_calls"));const FScopedTransaction Transaction(NSLOCTEXT("CubelessDataTable","MirrorRowMutation","Edit Mirror DataTable row"));
        if(!S.StableSource())return;
        FObservation Entry;if(!Capture(C,Entry,R,TEXT("mirror_transaction_entry"))||!S.StableSource())return;
        if(!MirrorReadLiteralEqual(Before.State,Entry.State)||!MirrorReadLiteralEqual(Before.Source,Entry.Source)||!MirrorReadLiteralEqual(Before.References,Entry.References)||!MirrorReadLiteralEqual(Before.DiskState,Entry.DiskState)||!MirrorReadLiteralEqual(Before.OtherDirty,Entry.OtherDirty)){S.Stop(TEXT("datatable_mirror_mutation_transaction_changed"),TEXT("State changed during transaction entry; no target Modify."));return;}
        const bool WasDirty=C.Table->GetPackage()->IsDirty();ITransaction* const Undo=GUndo;const bool ModifyReturned=C.Table->Modify();Count(R,TEXT("explicit_modify_calls"));R.Data->SetBoolField(TEXT("modify_returned"),ModifyReturned);
        if(!S.AfterModify(WasDirty,Undo,ModifyReturned))return;
        FObservation Modified;if(!Capture(C,Modified,R,TEXT("mirror_after_modify"))||!MirrorMutationFences(C,Before,Modified,NAME_None,false,false,R))return;
        bool Removed=false;
        if(Remove)
        {
            Count(R,TEXT("editor_remove_row_calls"));Removed=FDataTableEditorUtils::RemoveRow(C.Table.Get(),Key);R.Data->SetBoolField(TEXT("remove_helper_returned"),Removed);
            if(!Removed){if(!S.StableSource())return;ClearFinal(R);Capture(C,Final,R,TEXT("after_failed_operation"));HasFinal=Final.Complete;Fail(R,TEXT("datatable_remove_postcondition_failed"),TEXT("Actual removal helper returned false; prefix retained, no save."),true);return;}
            // Old selected storage has been destroyed/freed. Only the current map is read.
            if(!S.AcceptRowTransition(Key,true,nullptr)||!S.PublicCachesMatch())return;
        }
        else
        {
            uint8* Memory=nullptr;
            if(Existed){auto Ptr=C.Table->GetRowMap().Find(Key);Memory=Ptr?*Ptr:nullptr;}
            else{Count(R,TEXT("editor_add_row_calls"));Memory=FDataTableEditorUtils::AddRow(C.Table.Get(),Key);if(!Memory){if(!S.Admit())return;ClearFinal(R);Capture(C,Final,R,TEXT("after_failed_operation"));HasFinal=Final.Complete;Fail(R,TEXT("datatable_row_allocation_failed"),TEXT("Actual row-helper prefix retained; no import/save."),true);return;}if(!S.AcceptRowTransition(Key,false,Memory)||!S.PublicCachesMatch())return;}
            R.Data->SetBoolField(TEXT("row_memory_returned"),Memory!=nullptr);
            if(!S.StableSource())return;auto Current=C.Table->GetRowMap().Find(Key);const bool CurrentStorage=Memory&&Current&&*Current==Memory;R.Data->SetBoolField(TEXT("row_memory_current"),CurrentStorage);
            if(!CurrentStorage){S.Stop(TEXT("datatable_row_memory_unobservable"),TEXT("Current exact row storage required before import."));return;}
            FObservation AfterHelper;if(!Capture(C,AfterHelper,R,TEXT("mirror_after_row_helper"))||!MirrorMutationFences(C,Before,AfterHelper,Key,false,!Existed,R))return;
            Count(R,TEXT("json_conversion_calls"));const bool Converted=FJsonObjectConverter::JsonObjectToUStruct(Values.ToSharedRef(),C.Type,Memory,0,0);R.Data->SetBoolField(TEXT("json_conversion_returned"),Converted);
            if(!S.AcceptImportedRow(Key,Memory))return;
            if(!Converted){ClearFinal(R);Capture(C,Final,R,TEXT("after_failed_operation"));HasFinal=Final.Complete;if(!S.Stopped)R.Data->SetObjectField(TEXT("after"),RowSnapshot(C,Key));R.Data->SetStringField(TEXT("mirror_cache_completion"),TEXT("Partial import returned false; original post-change broadcast/save were not dispatched, caches may reflect the prior helper state."));Fail(R,TEXT("datatable_row_conversion_failed"),TEXT("Original converter may have partially applied primitive fields; actual row prefix retained, no broadcast/save/retry."),true);return;}
        }
        if(!S.StableSource())return;C.Table->MarkPackageDirty();Count(R,TEXT("mark_package_dirty_calls"));if(!S.StableSource())return;
        if(!Remove){Count(R,TEXT("row_data_broadcast_calls"));FDataTableEditorUtils::BroadcastPostChange(C.Table.Get(),FDataTableEditorUtils::EDataTableChangeInfo::RowData);if(!S.StableSource())return;}
        if(!S.PublicCachesMatch())return;
        FObservation After;if(!Capture(C,After,R,TEXT("after_observation"))||!S.StableSource())return;Final=After;HasFinal=true;R.Known=true;R.Changed=Remove||!Matched;
        if(!MirrorMutationFences(C,Before,After,Key,Remove,true,R))return;
        R.Data->SetObjectField(TEXT("after"),RowSnapshot(C,Key));if(!S.StableSource())return;
        auto Readback=MakeShared<FJsonObject>();const bool Pass=Remove?(Removed&&!C.Table->GetRowMap().Contains(Key)&&C.Table->GetRowMap().Num()==Before.State->GetIntegerField(TEXT("row_count"))-1):(RowValues(C,Key,Readback)&&ContainsExpected(MakeShared<FJsonValueObject>(Values.ToSharedRef()),MakeShared<FJsonValueObject>(Readback)));
        if(!S.StableSource())return;
        if(Remove){R.Data->SetBoolField(TEXT("removal_observed"),Pass);R.Data->SetBoolField(TEXT("removed"),Pass);}else{R.Data->SetBoolField(TEXT("created"),!Existed);R.Data->SetBoolField(TEXT("updated"),Existed);R.Data->SetObjectField(TEXT("readback_values"),Readback);R.Data->SetBoolField(TEXT("requested_values_match"),Pass);R.Data->SetBoolField(TEXT("legacy_changed"),!Matched);R.Data->SetBoolField(TEXT("actual_selected_row_values_changed"),!Existed||!MirrorReadLiteralEqual(OldValues,Readback));}
        auto Delta=MakeShared<FJsonObject>();Delta->SetNumberField(TEXT("affected_count"),Remove?(Pass?1:0):(Matched?0:1));Delta->SetNumberField(TEXT("created_count"),!Remove&&!Existed?1:0);Delta->SetNumberField(TEXT("updated_count"),!Remove&&Existed&&!Matched?1:0);Delta->SetNumberField(TEXT("removed_count"),Remove&&Pass?1:0);Delta->SetNumberField(TEXT("skipped_count"),!Remove&&Matched?1:0);Delta->SetNumberField(TEXT("failed_count"),Pass?0:1);R.Data->SetObjectField(TEXT("delta"),Delta);
        R.Data->SetBoolField(TEXT("independent_readback_passed"),Pass);
        if(!MirrorMutationSave(C,After,R,Final))return;R.Data->SetBoolField(TEXT("postcondition_verified"),Pass);
        if(!Pass){Fail(R,Remove?TEXT("datatable_remove_postcondition_failed"):TEXT("datatable_row_postcondition_failed"),TEXT("Actual row/save prefix retained; original requested-value/absence/count predicate failed."));return;}Complete(R);
    };
    Run(); // Stock transaction completes/unwinds before final success publication.
    if(HasFinal&&R.Known&&!S.Stopped)
    {
        if(!R.Code.IsEmpty()){auto Prior=MakeShared<FJsonObject>();Prior->SetStringField(TEXT("error_code"),R.Code);Prior->SetStringField(TEXT("error"),R.Error);Prior->SetStringField(TEXT("stage"),R.Stage);R.Data->SetObjectField(TEXT("mirror_pre_transaction_fence_failure"),Prior);}
        FObservation Closed;
        if(!S.StableSource()||!Capture(C,Closed,R,TEXT("mirror_after_transaction"))||!S.StableSource()||!MirrorReadLiteralEqual(Final.State,Closed.State)||!MirrorReadLiteralEqual(Final.Source,Closed.Source)||!MirrorReadLiteralEqual(Final.References,Closed.References)||!MirrorReadLiteralEqual(Final.DiskState,Closed.DiskState)||!MirrorReadLiteralEqual(Final.OtherDirty,Closed.OtherDirty))
        {S.Stop(TEXT("datatable_mirror_mutation_transaction_final_changed"),TEXT("Selected state changed during stock transaction finalization; actual row/save prefix retained."));}
        else R.Data->SetBoolField(TEXT("mirror_transaction_final_fence_passed"),true);
    }
    return Encode(R);
}
bool EndRead(FContext& C,const FObservation& Before,FResult& R);
template<typename TScope>
bool StringUdsEqual(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B,const TScope& S,bool Saved,const FString& At,bool ModifyProperties)
{
    if(!A||!B||A->Type!=B->Type)return false;
    if(ModifyProperties&&A->Type==EJson::String){const auto* Delta=S.ModifyDerivedMetadata.Find(At);if(Delta&&A->AsString().Equals(Delta->Key,ESearchCase::CaseSensitive)&&B->AsString().Equals(Delta->Value,ESearchCase::CaseSensitive))return true;}
    if(A->Type==EJson::Object){const auto X=A->AsObject(),Y=B->AsObject();if(X->Values.Num()!=Y->Values.Num())return false;FString Path;const bool Identity=X->TryGetStringField(TEXT("path"),Path);for(const auto& P:X->Values){const FString Key(P.Key.ToView());const auto Other=Y->TryGetField(Key);if(!Other)return false;if(Identity&&Key.Equals(TEXT("object_flags"),ESearchCase::CaseSensitive)&&P.Value->Type==EJson::String&&Other->Type==EJson::String){const auto* M=S.ModifyFlags.Find(Path);const auto* V=Saved?S.SaveFlags.Find(Path):nullptr;if((M&&P.Value->AsString()==LexToString(M->Key)&&Other->AsString()==LexToString(M->Value))||(V&&P.Value->AsString()==LexToString(V->Key)&&Other->AsString()==LexToString(V->Value)))continue;}if(!StringUdsEqual(P.Value,Other,S,Saved,At+TEXT("/")+Key,ModifyProperties))return false;}return true;}
    if(A->Type==EJson::Array){const auto& X=A->AsArray();const auto& Y=B->AsArray();if(X.Num()!=Y.Num())return false;for(int32 I=0;I<X.Num();++I)if(!StringUdsEqual(X[I],Y[I],S,Saved,At+TEXT("/")+LexToString(I),ModifyProperties))return false;return true;}
    if(Saved&&At.Equals(TEXT("/package_dirty"),ESearchCase::CaseSensitive))return B->Type==EJson::Boolean&&!B->AsBool();return MirrorReadLiteralEqual(A,B);
}
bool StringUdsFences(FContext& C,const FObservation& Before,const FObservation& After,FName Key,bool Remove,FResult& R)
{
    auto& S=*C.Uds;if(!Before.Complete||!After.Complete)return S.Stop(TEXT("datatable_string_uds_observation_incomplete"),TEXT("Current whole-table/source observation incomplete."));
    bool State=true;for(const auto& P:Before.State->Values){const FString Name(P.Key.ToView());if(Name==TEXT("rows")||Name==TEXT("row_count")||Name==TEXT("row_iteration_order")||Name==TEXT("package_dirty"))continue;State&=StringUdsEqual(P.Value,After.State->TryGetField(P.Key.ToView()),S,false,FString(),Name.Equals(TEXT("properties"),ESearchCase::CaseSensitive));}
    const auto X=Before.State->GetObjectField(TEXT("rows")),Y=After.State->GetObjectField(TEXT("rows"));bool Existed=false;for(const auto& P:X->Values){if(!Key.IsNone()&&FName(*FString(P.Key.ToView()))==Key){Existed=true;continue;}State&=MirrorReadLiteralEqual(P.Value,Y->TryGetField(P.Key.ToView()));}
    State&=Y->Values.Num()==X->Values.Num()+(Key.IsNone()?0:Remove?-1:Existed?0:1);
    const auto& Old=Before.State->GetArrayField(TEXT("row_iteration_order"));const auto& Now=After.State->GetArrayField(TEXT("row_iteration_order"));int32 I=0;for(const auto& N:Old){if(Remove&&FName(*N->AsString())==Key)continue;if(I>=Now.Num()||!MirrorReadLiteralEqual(N,Now[I++]))State=false;}
    if(!Key.IsNone()&&!Remove&&!Existed)State&=Now.Num()==I+1&&FName(*Now.Last()->AsString())==Key;else State&=Now.Num()==I;
    const bool Source=MirrorReadLiteralEqual(Before.Source,After.Source)&&(S.ReferencePayload?S.ReferenceProjectionEqual(Before.References,After.References):StringUdsEqual(MakeShared<FJsonValueObject>(Before.References),MakeShared<FJsonValueObject>(After.References),S)),Other=MirrorReadLiteralEqual(Before.OtherDirty,After.OtherDirty),File=MirrorReadLiteralEqual(Before.DiskState,After.DiskState);
    R.Data->SetBoolField(TEXT("observations_complete"),true);R.Data->SetBoolField(TEXT("source_preserved"),Source);R.Data->SetBoolField(TEXT("unselected_state_preserved"),State&&Other);R.Data->SetBoolField(TEXT("disk_preserved_before_save"),File);
    return (State&&Source&&Other&&File)||S.Stop(TEXT("datatable_string_uds_preservation_failed"),TEXT("Unselected rows/order/source/metadata/disk/dirty state changed; actual selected prefix retained, no next save."));
}
bool StringUdsSave(FContext& C,const FObservation& Before,FResult& R,FObservation& Final)
{
    auto& S=*C.Uds;Final=Before;if(!R.SaveRequested)return true;if(!Before.Complete||!S.PackageOwned())return false;
    const bool Readback=R.Data->GetBoolField(TEXT("independent_readback_passed"));R.Data->SetBoolField(TEXT("before_save_independent_readback_passed"),Readback);ClearFinal(R);
    FString File;if(!FPackageName::TryConvertLongPackageNameToFilename(C.Package,File,FPackageName::GetAssetPackageExtension()))return S.Stop(TEXT("datatable_save_preflight_failed"),TEXT("Exact target package filename required."));
    R.Stage=TEXT("save_package");R.Attempted=true;R.Known=false;R.SaveAttempted=true;Count(R,TEXT("save_attempt_count"));auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("asset_path"),C.Path);Receipt->SetStringField(TEXT("filename"),File);Receipt->SetBoolField(TEXT("attempted"),true);Receipt->SetBoolField(TEXT("only_if_dirty"),false);Receipt->SetStringField(TEXT("route"),TEXT("UPackage::SavePackage"));R.Data->SetArrayField(TEXT("save_calls"),{MakeShared<FJsonValueObject>(Receipt)});
    UPackage* const PackageBeforeSave=C.Table->GetPackage();const EObjectFlags FlagsBeforeSave=PackageBeforeSave->GetFlags();const bool DirtyBeforeSave=PackageBeforeSave->IsDirty();ITransaction* const UndoBeforeSave=GUndo;
    Receipt->SetStringField(TEXT("package_before_save"),C.Package);Receipt->SetNumberField(TEXT("package_flags_before_save"),static_cast<uint32>(FlagsBeforeSave));Receipt->SetBoolField(TEXT("package_dirty_before_save"),DirtyBeforeSave);Receipt->SetBoolField(TEXT("active_undo_before_save"),UndoBeforeSave!=nullptr);
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;Args.bSlowTask=false;const bool Saved=UPackage::SavePackage(PackageBeforeSave,C.Table.Get(),*File,Args);
    R.Saved=Saved;Receipt->SetBoolField(TEXT("saved"),Saved);R.Data->SetBoolField(TEXT("saved"),Saved); // Plain returned prefix precedes any new SDK.
    if(!S.AfterSave(Saved,PackageBeforeSave,FlagsBeforeSave,DirtyBeforeSave,UndoBeforeSave)||!S.PackageOwned())return false;
    if(Saved){Count(R,TEXT("explicit_clear_dirty_calls"));C.Table->GetPackage()->SetDirtyFlag(false);if(!S.StableSource())return false;}
    if(!Capture(C,Final,R,TEXT("after_save"))||!S.StableSource())return S.Stop(TEXT("datatable_save_unobservable_or_failed"),TEXT("Actual returned save prefix retained; endpoint incomplete."));
    if(!Saved)return S.Stop(TEXT("datatable_save_unobservable_or_failed"),TEXT("SavePackage returned false; row prefix retained without retry."));
    const bool State=StringUdsEqual(MakeShared<FJsonValueObject>(Before.State),MakeShared<FJsonValueObject>(Final.State),S,true),Source=MirrorReadLiteralEqual(Before.Source,Final.Source)&&StringUdsEqual(MakeShared<FJsonValueObject>(Before.References),MakeShared<FJsonValueObject>(Final.References),S,true),Other=MirrorReadLiteralEqual(Before.OtherDirty,Final.OtherDirty),Persisted=Final.DiskState->GetBoolField(TEXT("exists"))&&Final.DiskState->GetNumberField(TEXT("bytes"))>0&&!C.Table->GetPackage()->IsDirty();
    R.Known=true;R.Data->SetBoolField(TEXT("observations_complete"),true);R.Data->SetBoolField(TEXT("source_preserved"),Source);R.Data->SetBoolField(TEXT("unselected_state_preserved"),State&&Other);R.Data->SetBoolField(TEXT("save_state_preserved"),State);R.Data->SetBoolField(TEXT("independent_readback_passed"),State&&Readback);R.Data->SetBoolField(TEXT("persistence_verified"),Persisted);
    return (State&&Source&&Other&&Persisted)||S.Stop(TEXT("datatable_saved_readback_failed"),TEXT("SavedTrue retained but selected persisted-state/source fence failed."));
}
void StringUdsClose(FContext& C,const FObservation& Final,bool HasFinal,ITransaction* Undo,FResult& R)
{
    auto& S=*C.Uds;S.ExpectedUndo=Undo;S.TrackUndo=true;
    if(S.Stopped||!HasFinal||!R.Known){if(R.Attempted){R.Known=false;R.Success=false;R.Post=false;R.Data->SetBoolField(TEXT("string_uds_endpoint_known"),false);R.Data->SetStringField(TEXT("string_uds_endpoint_scope"),TEXT("Incomplete final observation after transaction destruction; retained raw prefix does not guarantee endpoint known0/knownchanged. No retry."));}return;}
    FObservation Closed;if(!S.StableSource()||!Capture(C,Closed,R,TEXT("string_uds_after_transaction"))||!S.StableSource()||!MirrorReadLiteralEqual(Final.State,Closed.State)||!MirrorReadLiteralEqual(Final.Source,Closed.Source)||!MirrorReadLiteralEqual(Final.References,Closed.References)||!MirrorReadLiteralEqual(Final.DiskState,Closed.DiskState)||!MirrorReadLiteralEqual(Final.OtherDirty,Closed.OtherDirty)){S.Stop(TEXT("datatable_string_uds_transaction_final_changed"),TEXT("Selected/source state changed or became unobservable while transaction closed; prefix retained."));R.Data->SetBoolField(TEXT("string_uds_endpoint_known"),false);return;}
    R.Data->SetBoolField(TEXT("string_uds_transaction_final_fence_passed"),true);R.Data->SetBoolField(TEXT("string_uds_endpoint_known"),true);
}
FString StringUdsRowMutation(FContext& C,const FObservation& Before,FResult& R,const FString& Name,const TSharedPtr<FJsonObject>& Requested,bool Remove)
{
    auto& S=*C.Uds;TGuardValue<bool> Busy(StringUdsWriterRunning,true);if(!S.StableSource()||(!Remove&&!S.ImportNamesBounded(Requested)))return Encode(R);S.ReferenceAdmissionClosed=true;const FName Key(*Name);const bool Existed=C.Table->GetRowMap().Contains(Key);R.Data->SetStringField(TEXT("row_name"),Name);R.Data->SetObjectField(TEXT("before"),RowSnapshot(C,Key));if(!S.StableSource())return Encode(R);
    if(Remove&&!Existed){if(!EndRead(C,Before,R))return Encode(R);Fail(R,TEXT("datatable_row_not_found"),TEXT("Row not found; no Modify/remove/save."));return Encode(R);}
    auto Old=MakeShared<FJsonObject>();const bool Matched=!Remove&&Existed&&RowValues(C,Key,Old)&&ContainsExpected(MakeShared<FJsonValueObject>(Requested.ToSharedRef()),MakeShared<FJsonValueObject>(Old));if(!S.StableSource())return Encode(R);if(!Remove)R.Data->SetBoolField(TEXT("already_matched"),Matched);
    FObservation Final;bool HasFinal=false;ITransaction* const UndoBefore=GUndo;
    auto Run=[&]()
    {
        R.Stage=Remove?TEXT("remove_row"):TEXT("apply_row");R.Attempted=true;R.Known=false;Count(R,TEXT("outer_transaction_calls"));const FScopedTransaction Transaction(NSLOCTEXT("CubelessDataTable","String1RowMutation","Edit String1 DataTable row"));S.ExpectedUndo=GUndo;S.TrackUndo=true;
        if(!S.StableSource())return;FObservation Entry;if(!Capture(C,Entry,R,TEXT("string_uds_transaction_entry"))||!S.StableSource())return;
        if(!MirrorReadLiteralEqual(Before.State,Entry.State)||!MirrorReadLiteralEqual(Before.Source,Entry.Source)||!MirrorReadLiteralEqual(Before.References,Entry.References)||!MirrorReadLiteralEqual(Before.DiskState,Entry.DiskState)||!MirrorReadLiteralEqual(Before.OtherDirty,Entry.OtherDirty)){S.Stop(TEXT("datatable_string_uds_transaction_entry_changed"),TEXT("Transaction entry changed selected/source state; no Modify."));return;}
        const FName RowStructNameBefore=C.Table->RowStructName_DEPRECATED;const FTopLevelAssetPath RowStructPathBefore=C.Table->RowStructPathName;
        const bool WasDirty=C.Table->GetPackage()->IsDirty();ITransaction* const Undo=GUndo;const bool Modified=C.Table->Modify();Count(R,TEXT("explicit_modify_calls"));R.Data->SetBoolField(TEXT("modify_returned"),Modified);
        if(!S.AfterModify(WasDirty,Undo,Modified)||!S.AfterModifyDerivedMetadata(RowStructNameBefore,RowStructPathBefore,Undo,Modified))return;
        FObservation ModifiedState;if(!Capture(C,ModifiedState,R,TEXT("string_uds_after_modify"))||!StringUdsFences(C,Before,ModifiedState,NAME_None,false,R))return;
        bool Removed=false;
        if(Remove){Count(R,TEXT("editor_remove_row_calls"));Removed=FDataTableEditorUtils::RemoveRow(C.Table.Get(),Key);R.Data->SetBoolField(TEXT("remove_helper_returned"),Removed);if(!Removed){S.Stop(TEXT("datatable_remove_postcondition_failed"),TEXT("Actual remove helper false; row prefix unadopted, no save."));return;}if(!S.AcceptRows(Key,true,nullptr))return;R.Data->SetBoolField(TEXT("row_retirement_observed"),true);}
        else
        {
            uint8* Memory=nullptr;if(Existed){auto Found=C.Table->GetRowMap().Find(Key);Memory=Found?*Found:nullptr;}else{Count(R,TEXT("editor_add_row_calls"));Memory=FDataTableEditorUtils::AddRow(C.Table.Get(),Key);R.Data->SetBoolField(TEXT("row_memory_returned"),Memory!=nullptr);if(!Memory){S.Stop(TEXT("datatable_row_allocation_failed"),TEXT("Actual helper prefix retained; null storage, no import/save."));return;}if(!S.AcceptRows(Key,false,Memory))return;R.Data->SetBoolField(TEXT("row_creation_observed"),true);}
            R.Data->SetBoolField(TEXT("row_memory_returned"),Memory!=nullptr);if(!S.StableSource())return;auto Current=C.Table->GetRowMap().Find(Key);const bool Routing=Memory&&Current&&*Current==Memory;R.Data->SetBoolField(TEXT("row_memory_current"),Routing);if(!Routing){S.Stop(TEXT("datatable_row_memory_unobservable"),TEXT("Current exact selected storage required before converter."));return;}
            Count(R,TEXT("json_conversion_calls"));const bool Converted=FJsonObjectConverter::JsonObjectToUStruct(Requested.ToSharedRef(),C.Type,Memory,0,0);R.Data->SetBoolField(TEXT("json_conversion_returned"),Converted);
            if(!S.AcceptRows(Key,false,Memory,true))return;if(!Converted){S.Stop(TEXT("datatable_row_conversion_failed"),TEXT("Original converter false may retain partial fields/default row; no broadcast/save/retry."));return;}
        }
        if(!S.StableSource())return;C.Table->MarkPackageDirty();Count(R,TEXT("mark_package_dirty_calls"));if(!S.StableSource())return;
        if(!Remove){Count(R,TEXT("row_data_broadcast_calls"));FDataTableEditorUtils::BroadcastPostChange(C.Table.Get(),FDataTableEditorUtils::EDataTableChangeInfo::RowData);if(!S.StableSource())return;}
        FObservation After;if(!Capture(C,After,R,TEXT("after_observation"))||!S.StableSource())return;Final=After;HasFinal=true;R.Known=true;R.Changed=Remove||!Matched;
        if(!StringUdsFences(C,Before,After,Key,Remove,R))return;R.Data->SetObjectField(TEXT("after"),RowSnapshot(C,Key));if(!S.StableSource())return;auto Values=MakeShared<FJsonObject>();const bool Pass=Remove?(Removed&&!C.Table->GetRowMap().Contains(Key)&&C.Table->GetRowMap().Num()==Before.State->GetIntegerField(TEXT("row_count"))-1):(RowValues(C,Key,Values)&&ContainsExpected(MakeShared<FJsonValueObject>(Requested.ToSharedRef()),MakeShared<FJsonValueObject>(Values)));if(!S.StableSource())return;
        if(Remove){R.Data->SetBoolField(TEXT("removed"),Pass);R.Data->SetBoolField(TEXT("removal_observed"),Pass);}else{R.Data->SetBoolField(TEXT("created"),!Existed);R.Data->SetBoolField(TEXT("updated"),Existed);R.Data->SetObjectField(TEXT("readback_values"),Values);R.Data->SetBoolField(TEXT("requested_values_match"),Pass);R.Data->SetBoolField(TEXT("legacy_changed"),!Matched);R.Data->SetBoolField(TEXT("actual_selected_row_values_changed"),!Existed||!MirrorReadLiteralEqual(Old,Values));}
        auto Delta=MakeShared<FJsonObject>();Delta->SetNumberField(TEXT("affected_count"),Remove?(Pass?1:0):Matched?0:1);Delta->SetNumberField(TEXT("created_count"),!Remove&&!Existed?1:0);Delta->SetNumberField(TEXT("updated_count"),!Remove&&Existed&&!Matched?1:0);Delta->SetNumberField(TEXT("removed_count"),Remove&&Pass?1:0);Delta->SetNumberField(TEXT("skipped_count"),!Remove&&Matched?1:0);Delta->SetNumberField(TEXT("failed_count"),Pass?0:1);R.Data->SetObjectField(TEXT("delta"),Delta);R.Data->SetBoolField(TEXT("independent_readback_passed"),Pass);
        // Preserve stock save-before-requested-subset postcondition semantics,
        // including ignored lowercamel/unknown input with a real saved row prefix.
        if(!StringUdsSave(C,After,R,Final))return;R.Data->SetBoolField(TEXT("postcondition_verified"),Pass);
        if(!Pass){Fail(R,Remove?TEXT("datatable_remove_postcondition_failed"):TEXT("datatable_row_postcondition_failed"),TEXT("Actual selected row/save prefix retained; requested value/absence/count predicate failed."));return;}Complete(R);
    };
    Run();StringUdsClose(C,Final,HasFinal,UndoBefore,R);return Encode(R);
}

bool JsonBounded(const TSharedPtr<FJsonValue>& V, int32 Depth, int32& Count)
{ if (!V || Depth > 32 || ++Count > 16384) { return false; } if (V->Type == EJson::Number) { return FMath::IsFinite(V->AsNumber()); } if (V->Type == EJson::Object) { for (const auto& Field : V->AsObject()->Values) { if (!JsonBounded(Field.Value, Depth + 1, Count)) { return false; } } } if (V->Type == EJson::Array) { for (const auto& Value : V->AsArray()) { if (!JsonBounded(Value, Depth + 1, Count)) { return false; } } } return V->Type != EJson::None; }
// Keep the stock authored-name lookup/import order; only prevent FName construction past NAME_SIZE.
bool MirrorImportNamesBounded(const FContext& C,const TSharedRef<FJsonObject>& Values,FResult& R)
{
    if(!C.Type->GetPathName().Equals(MirrorRowPath,ESearchCase::CaseSensitive)) { return true; }
    for(const TCHAR* Key:{TEXT("Name"),TEXT("MirroredName"),TEXT("MirrorEntryType")})
    {
        const auto Value=Values->TryGetField(Key);if(!Value.IsValid()||Value->Type!=EJson::String) { continue; }
        const FString TextValue=Value->AsString();bool Nul=false;for(TCHAR Unit:TextValue) { Nul|=Unit==TCHAR('\0'); }
        if(Nul||TextValue.Len()>=NAME_SIZE) { return Fail(R,TEXT("datatable_mirror_name_budget"),TEXT("Selected MirrorTableRow authored FName/enum string exceeds the safe Unreal name budget; no row Modify/import/save.")); }
    }
    return true;
}
FString StringUdsCreate(const FString& RequestedPath,const FString& RequestedStruct,bool RequestedSave)
{
    FResult R;Init(R,TEXT("create_datatable"),RequestedSave);TGuardValue<bool> Busy(StringUdsWriterRunning,true);FContext C;C.Path=Normalize(RequestedPath);C.Package=FPackageName::ObjectPathToPackageName(C.Path);const FString StructPath=Normalize(RequestedStruct);
    R.Data->SetStringField(TEXT("datatable"),C.Path);
    if(!AssetPath(C.Path,true)||FPackageName::ObjectPathToObjectName(C.Path)!=FPackageName::GetShortName(C.Package)||!AssetPath(StructPath,true)){Fail(R,TEXT("datatable_path_invalid"),TEXT("Fresh exact Game table and saved String1 UDS asset paths required."));return Encode(R);}
    C.Uds=MakeUnique<FStringUdsScope>(R,true,false,true);auto& S=*C.Uds;if(!S.Start())return Encode(R);
    Count(R,TEXT("row_struct_load_count"));R.Data->SetStringField(TEXT("row_struct_load_requested"),StructPath);auto* LoadedType=LoadObject<UScriptStruct>(nullptr,*StructPath);R.Data->SetBoolField(TEXT("row_struct_load_returned"),LoadedType!=nullptr);if(!S.Context())return Encode(R);
    if(!Loaded(LoadedType)||LoadedType->GetClass()!=UUserDefinedStruct::StaticClass()){Fail(R,TEXT("datatable_row_struct_unsupported"),TEXT("Exact native UUserDefinedStruct required for the /Game String1 path; broader custom structs remain incomplete."));return Encode(R);}
    C.Type=LoadedType;if(!S.BindType(static_cast<UUserDefinedStruct*>(LoadedType)))return Encode(R);R.Data->SetObjectField(TEXT("source_before"),S.SourceBaseline);
    if(!FDataTableEditorUtils::IsValidTableStruct(C.Type)||!S.StableSource()){if(!S.Stopped)Fail(R,TEXT("datatable_row_struct_unsupported"),TEXT("Public UE58 DataTable row-struct admission failed."));return Encode(R);}
    auto DiskBefore=MakeShared<FJsonObject>(),DirtyBefore=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("disk_before"),DiskBefore);R.Data->SetObjectField(TEXT("other_dirty_before"),DirtyBefore);
    if(!Disk(C.Package,DiskBefore)||!Dirty(C.Package,DirtyBefore)||!S.StableSource()){if(!S.Stopped)Fail(R,TEXT("datatable_preflight_unobservable"),TEXT("Exact target-file absence/other dirty baseline required."));return Encode(R);}
    const bool Exists=FindObject<UObject>(nullptr,*C.Path)||FindPackage(nullptr,*C.Package)||DiskBefore->GetBoolField(TEXT("exists"))||UEditorAssetLibrary::DoesAssetExist(C.Path);if(!S.StableSource())return Encode(R);if(Exists){Fail(R,TEXT("datatable_target_exists"),TEXT("Existing asset/package/file/registry slot is not overwritten or reused."));return Encode(R);}
    auto Before=MakeShared<FJsonObject>();Before->SetBoolField(TEXT("exists"),false);Before->SetStringField(TEXT("asset_path"),C.Path);R.Data->SetObjectField(TEXT("before"),Before);
    // Source/default/reference admission is complete; callbacks may not enroll new reference roles.
    S.ReferenceAdmissionClosed=true;
    FObservation Final;bool HasFinal=false;ITransaction* const UndoBefore=GUndo;TStrongObjectPtr<UPackage> CreatedPackageHold;
    auto Run=[&]()
    {
        R.Attempted=true;R.Known=false;R.Stage=TEXT("create_package");Count(R,TEXT("outer_transaction_calls"));const FScopedTransaction Transaction(NSLOCTEXT("CubelessDataTable","String1Create","Create String1 DataTable"));S.ExpectedUndo=GUndo;S.TrackUndo=true;if(!S.StableSource())return;
        // Transaction construction/source export may run callbacks. Recheck the requested fresh slot before allocation.
        const bool RegistryExists=UEditorAssetLibrary::DoesAssetExist(C.Path);if(!S.StableSource())return;
        auto FreshDisk=MakeShared<FJsonObject>();if(!Disk(C.Package,FreshDisk)){S.Stop(TEXT("datatable_fresh_target_unobservable"),TEXT("Fresh target file absence became unobservable after transaction begin."));return;}
        R.Data->SetBoolField(TEXT("fresh_target_recheck_performed"),true);
        if(FreshDisk->GetBoolField(TEXT("exists"))||RegistryExists||FindObject<UObject>(nullptr,*C.Path)||FindPackage(nullptr,*C.Package)){S.Stop(TEXT("datatable_target_exists"),TEXT("Target object/package/file/registry slot appeared after transaction begin; no package allocation or overwrite."));return;}
        Count(R,TEXT("package_create_attempts"));UPackage* Package=CreatePackage(*C.Package);CreatedPackageHold.Reset(Package);R.Data->SetBoolField(TEXT("created_package_returned"),Package!=nullptr);R.Data->SetBoolField(TEXT("created_package_identity_observed"),false);
        if(!S.Context())return;if(!Loaded(Package)||!S.Pin(Package)){if(!S.Stopped)S.Stop(TEXT("datatable_package_creation_failed"),TEXT("Package allocation prefix retained; no retry."));return;}
        R.Data->SetObjectField(TEXT("created_package"),Identity(Package));R.Data->SetBoolField(TEXT("created_package_identity_observed"),true);if(!S.StableSource())return;
        if(Package->GetClass()!=UPackage::StaticClass()||Package->GetOuter()!=nullptr||Package->GetPackage()!=Package||!Package->GetPathName().Equals(C.Package,ESearchCase::CaseSensitive)||Package->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject))
        {S.Stop(TEXT("datatable_package_creation_failed"),TEXT("Returned package must be the exact fresh requested native package; safe returned prefix retained."));return;}
        const bool RegistryObjectAppeared=UEditorAssetLibrary::DoesAssetExist(C.Path);if(!S.StableSource())return;
        auto ObjectSlotDisk=MakeShared<FJsonObject>();if(!Disk(C.Package,ObjectSlotDisk)){S.Stop(TEXT("datatable_fresh_target_unobservable"),TEXT("Fresh target file absence became unobservable after package allocation."));return;}
        R.Data->SetBoolField(TEXT("fresh_object_slot_recheck_performed"),true);
        if(ObjectSlotDisk->GetBoolField(TEXT("exists"))||RegistryObjectAppeared||FindObject<UObject>(nullptr,*C.Path)||FindPackage(nullptr,*C.Package)!=Package){S.Stop(TEXT("datatable_target_exists"),TEXT("Requested object slot appeared or package route changed before NewObject; no table allocation/replacement."));return;}
        R.Stage=TEXT("create_datatable");Count(R,TEXT("asset_allocation_attempts"));auto* New=NewObject<UDataTable>(Package,FName(*FPackageName::GetShortName(C.Package)),RF_Public|RF_Standalone|RF_Transactional);C.Table.Reset(New);R.Data->SetBoolField(TEXT("allocated_asset_returned"),New!=nullptr);R.Data->SetBoolField(TEXT("allocated_asset_identity_observed"),false);
        if(!S.Context())return;if(!Loaded(New)||!S.Pin(New)){if(!S.Stopped)S.Stop(TEXT("datatable_asset_allocation_failed"),TEXT("Returned table hold/prefix retained; no later source export or retry."));return;}
        R.Data->SetObjectField(TEXT("allocated_asset"),Identity(New));R.Data->SetBoolField(TEXT("allocated_asset_identity_observed"),true);if(!S.StableSource())return;
        if(New->GetClass()!=UDataTable::StaticClass()||New->GetOuter()!=Package||New->GetPackage()!=Package||!New->GetPathName().Equals(C.Path,ESearchCase::CaseSensitive)||!IsObjectHandleNull(New->RowStruct.GetHandle())||!New->GetRowMap().IsEmpty())
        {S.Stop(TEXT("datatable_asset_allocation_failed"),TEXT("Package/table allocation prefix retained but exact fresh role/default row storage changed; no overwrite or rollback."));return;}
        New->RowStruct=C.Type;New->RowStructName_DEPRECATED=C.Type->GetFName();New->RowStructPathName=C.Type->GetStructPathName();if(!S.BindTable(New,C.Path,C.Package))return;
        TMap<FName,const uint8*> Empty;New->CreateTableFromRawData(Empty,C.Type);if(!S.StableSource())return;
        Count(R,TEXT("asset_registry_publication_calls"));FAssetRegistryModule::AssetCreated(New);R.Data->SetBoolField(TEXT("asset_registry_publication_returned"),true);if(!S.StableSource())return;
        TArray<TSharedPtr<FJsonValue>> DirtyCalls;
        const auto MarkCreatedDirty=[&](const UObject* Object,const TCHAR* Role)
        {
            if(!S.StableSource())return false;
            const bool WasDirty=Package->IsDirty();ITransaction* const Undo=GUndo;auto Call=MakeShared<FJsonObject>();
            Call->SetStringField(TEXT("role"),Role);Call->SetStringField(TEXT("package"),C.Package);Call->SetBoolField(TEXT("was_dirty"),WasDirty);Call->SetBoolField(TEXT("attempted"),true);
            DirtyCalls.Add(MakeShared<FJsonValueObject>(Call));R.Data->SetArrayField(TEXT("string_uds_create_dirty_calls"),DirtyCalls);
            const bool Marked=Object->MarkPackageDirty();Count(R,TEXT("mark_package_dirty_calls"));Call->SetBoolField(TEXT("returned"),true);Call->SetBoolField(TEXT("return_value"),Marked);
            if(!S.AfterCreateDirty(Package,WasDirty,Undo,Marked))return false;
            Call->SetBoolField(TEXT("current_fence_passed"),true);Call->SetBoolField(TEXT("package_dirty_after"),Package->IsDirty());return true;
        };
        if(!MarkCreatedDirty(New,TEXT("table_mark_package_dirty"))||!MarkCreatedDirty(Package,TEXT("package_mark_package_dirty")))return;
        FObservation After;if(!Capture(C,After,R,TEXT("after_observation"))||!S.StableSource())return;Final=After;HasFinal=true;
        const bool Source=MirrorReadLiteralEqual(S.SourceBaseline,After.Source),Other=MirrorReadLiteralEqual(DirtyBefore,After.OtherDirty),File=MirrorReadLiteralEqual(DiskBefore,After.DiskState),Pass=New->GetRowMap().IsEmpty()&&New->RowStruct==C.Type;
        R.Known=true;R.Changed=true;R.Data->SetBoolField(TEXT("observations_complete"),true);R.Data->SetBoolField(TEXT("source_preserved"),Source);R.Data->SetBoolField(TEXT("unselected_state_preserved"),Other);R.Data->SetBoolField(TEXT("independent_readback_passed"),Pass);R.Data->SetStringField(TEXT("row_struct"),S.TypePath);
        auto Actual=MakeShared<FJsonObject>();Actual->SetBoolField(TEXT("exists"),true);Actual->SetStringField(TEXT("asset_path"),C.Path);Actual->SetStringField(TEXT("row_struct"),S.TypePath);Actual->SetNumberField(TEXT("row_count"),New->GetRowMap().Num());R.Data->SetObjectField(TEXT("after"),Actual);
        auto Delta=MakeShared<FJsonObject>();Delta->SetNumberField(TEXT("affected_count"),1);Delta->SetNumberField(TEXT("created_count"),1);Delta->SetNumberField(TEXT("updated_count"),0);Delta->SetNumberField(TEXT("removed_count"),0);Delta->SetNumberField(TEXT("skipped_count"),0);Delta->SetNumberField(TEXT("failed_count"),Pass?0:1);R.Data->SetObjectField(TEXT("delta"),Delta);
        if(!Source||!Other||!File||!Pass){S.Stop(TEXT("datatable_create_preservation_failed"),TEXT("Actual fresh table prefix observed but source/disk/dirty/empty-table fence failed; no save."));return;}
        if(!StringUdsSave(C,After,R,Final))return;R.Data->SetBoolField(TEXT("postcondition_verified"),true);Complete(R);
    };
    Run();StringUdsClose(C,Final,HasFinal,UndoBefore,R);return Encode(R);
}

bool NativeRowFences(FContext& C,const FObservation& Before,const FObservation& After,FName Key,bool Remove,FResult& R)
{
    auto& S=*C.Native;if(!Before.Complete||!After.Complete)return S.Stop(TEXT("datatable_native_row_observation_incomplete"),TEXT("Current whole-table/source observation incomplete."));
    bool State=true;for(const auto& P:Before.State->Values){const FString Name(P.Key.ToView());if(Name==TEXT("rows")||Name==TEXT("row_count")||Name==TEXT("row_iteration_order")||Name==TEXT("package_dirty"))continue;State&=StringUdsEqual(P.Value,After.State->TryGetField(P.Key.ToView()),S,false,FString(),Name.Equals(TEXT("properties"),ESearchCase::CaseSensitive));}
    const auto X=Before.State->GetObjectField(TEXT("rows")),Y=After.State->GetObjectField(TEXT("rows"));bool Existed=false;for(const auto& P:X->Values){if(!Key.IsNone()&&FName(*FString(P.Key.ToView()))==Key){Existed=true;continue;}State&=MirrorReadLiteralEqual(P.Value,Y->TryGetField(P.Key.ToView()));}
    State&=Y->Values.Num()==X->Values.Num()+(Key.IsNone()?0:Remove?-1:Existed?0:1);
    const auto& Old=Before.State->GetArrayField(TEXT("row_iteration_order"));const auto& Now=After.State->GetArrayField(TEXT("row_iteration_order"));int32 I=0;for(const auto& N:Old){if(Remove&&FName(*N->AsString())==Key)continue;if(I>=Now.Num()||!MirrorReadLiteralEqual(N,Now[I++]))State=false;}
    if(!Key.IsNone()&&!Remove&&!Existed)State&=Now.Num()==I+1&&FName(*Now.Last()->AsString())==Key;else State&=Now.Num()==I;
    const bool Source=MirrorReadLiteralEqual(Before.Source,After.Source)&&StringUdsEqual(MakeShared<FJsonValueObject>(Before.References),MakeShared<FJsonValueObject>(After.References),S),Other=MirrorReadLiteralEqual(Before.OtherDirty,After.OtherDirty),File=MirrorReadLiteralEqual(Before.DiskState,After.DiskState);
    R.Data->SetBoolField(TEXT("observations_complete"),true);R.Data->SetBoolField(TEXT("source_preserved"),Source);R.Data->SetBoolField(TEXT("unselected_state_preserved"),State&&Other);R.Data->SetBoolField(TEXT("disk_preserved_before_save"),File);
    return (State&&Source&&Other&&File)||S.Stop(TEXT("datatable_native_row_preservation_failed"),TEXT("Unselected rows/order/source/metadata/disk/dirty state changed; actual selected prefix retained, no next save."));
}
bool NativeRowSave(FContext& C,const FObservation& Before,FResult& R,FObservation& Final)
{
    auto& S=*C.Native;Final=Before;if(!R.SaveRequested)return true;if(!Before.Complete||!S.PackageOwned())return false;
    const bool Readback=R.Data->GetBoolField(TEXT("independent_readback_passed"));R.Data->SetBoolField(TEXT("before_save_independent_readback_passed"),Readback);ClearFinal(R);
    FString File;if(!FPackageName::TryConvertLongPackageNameToFilename(C.Package,File,FPackageName::GetAssetPackageExtension()))return S.Stop(TEXT("datatable_save_preflight_failed"),TEXT("Exact target package filename required."));
    R.Stage=TEXT("save_package");R.Attempted=true;R.Known=false;R.SaveAttempted=true;Count(R,TEXT("save_attempt_count"));auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("asset_path"),C.Path);Receipt->SetStringField(TEXT("filename"),File);Receipt->SetBoolField(TEXT("attempted"),true);Receipt->SetBoolField(TEXT("only_if_dirty"),false);Receipt->SetStringField(TEXT("route"),TEXT("UPackage::SavePackage"));R.Data->SetArrayField(TEXT("save_calls"),{MakeShared<FJsonValueObject>(Receipt)});
    UPackage* const PackageBeforeSave=C.Table->GetPackage();const EObjectFlags FlagsBeforeSave=PackageBeforeSave->GetFlags();const bool DirtyBeforeSave=PackageBeforeSave->IsDirty();ITransaction* const UndoBeforeSave=GUndo;
    Receipt->SetStringField(TEXT("package_before_save"),C.Package);Receipt->SetNumberField(TEXT("package_flags_before_save"),static_cast<uint32>(FlagsBeforeSave));Receipt->SetBoolField(TEXT("package_dirty_before_save"),DirtyBeforeSave);Receipt->SetBoolField(TEXT("active_undo_before_save"),UndoBeforeSave!=nullptr);
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;Args.bSlowTask=false;const bool Saved=UPackage::SavePackage(PackageBeforeSave,C.Table.Get(),*File,Args);
    R.Saved=Saved;Receipt->SetBoolField(TEXT("saved"),Saved);R.Data->SetBoolField(TEXT("saved"),Saved); // Plain returned prefix precedes any new SDK.
    if(!Saved){R.Data->SetBoolField(TEXT("native_save_false_asset_getters_suppressed"),true);return Fail(R,TEXT("datatable_save_unobservable_or_failed"),TEXT("Actual SavePackage False retained; no target/source getter, dirty clear, retry or further save. Only sealed own-transaction cleanup may run."),true);}
    if(!S.AfterSave(Saved,PackageBeforeSave,FlagsBeforeSave,DirtyBeforeSave,UndoBeforeSave)||!S.PackageOwned())return false;
    if(Saved){Count(R,TEXT("explicit_clear_dirty_calls"));C.Table->GetPackage()->SetDirtyFlag(false);if(!S.StableSource())return false;}
    if(!Capture(C,Final,R,TEXT("after_save"))||!S.StableSource())return S.Stop(TEXT("datatable_save_unobservable_or_failed"),TEXT("Actual returned save prefix retained; endpoint incomplete."));
    if(!Saved)return S.Stop(TEXT("datatable_save_unobservable_or_failed"),TEXT("SavePackage returned false; row prefix retained without retry."));
    const bool State=StringUdsEqual(MakeShared<FJsonValueObject>(Before.State),MakeShared<FJsonValueObject>(Final.State),S,true),Source=MirrorReadLiteralEqual(Before.Source,Final.Source)&&StringUdsEqual(MakeShared<FJsonValueObject>(Before.References),MakeShared<FJsonValueObject>(Final.References),S,true),Other=MirrorReadLiteralEqual(Before.OtherDirty,Final.OtherDirty),Persisted=Final.DiskState->GetBoolField(TEXT("exists"))&&Final.DiskState->GetNumberField(TEXT("bytes"))>0&&!C.Table->GetPackage()->IsDirty();
    R.Known=true;R.Data->SetBoolField(TEXT("observations_complete"),true);R.Data->SetBoolField(TEXT("source_preserved"),Source);R.Data->SetBoolField(TEXT("unselected_state_preserved"),State&&Other);R.Data->SetBoolField(TEXT("save_state_preserved"),State);R.Data->SetBoolField(TEXT("independent_readback_passed"),State&&Readback);R.Data->SetBoolField(TEXT("persistence_verified"),Persisted);
    return (State&&Source&&Other&&Persisted)||S.Stop(TEXT("datatable_saved_readback_failed"),TEXT("SavedTrue retained but selected persisted-state/source fence failed."));
}
void NativeRowClose(FContext& C,const FObservation& Final,bool HasFinal,FResult& R)
{
    auto& S=*C.Native;if(!S.EndTransaction(HasFinal&&R.Known))return;
    if(S.Stopped||!HasFinal||!R.Known){R.Data->SetBoolField(TEXT("native_row_endpoint_known"),false);return;}
    FObservation Closed;if(!S.StableSource()||!Capture(C,Closed,R,TEXT("native_row_after_transaction"))||!S.StableSource()||!MirrorReadLiteralEqual(Final.State,Closed.State)||!MirrorReadLiteralEqual(Final.Source,Closed.Source)||!MirrorReadLiteralEqual(Final.References,Closed.References)||!MirrorReadLiteralEqual(Final.DiskState,Closed.DiskState)||!MirrorReadLiteralEqual(Final.OtherDirty,Closed.OtherDirty)){S.Stop(TEXT("datatable_native_transaction_final_changed"),TEXT("Selected/source endpoint changed while own transaction closed; no replay."));return;}R.Data->SetBoolField(TEXT("native_row_endpoint_known"),true);
}
FString NativeRowMutation(FContext& C,const FObservation& Before,FResult& R,const FString& Name,const TSharedPtr<FJsonObject>& Requested,bool Remove)

{

    auto& S=*C.Native;TGuardValue<bool> Busy(NativeRowWriterRunning,true);if(!S.StableSource()||(!Remove&&!S.ImportNamesBounded(Requested)))return Encode(R);const FName Key(*Name);const bool Existed=C.Table->GetRowMap().Contains(Key);R.Data->SetStringField(TEXT("row_name"),Name);R.Data->SetObjectField(TEXT("before"),RowSnapshot(C,Key));if(!S.StableSource())return Encode(R);

    if(Remove&&!Existed){if(!EndRead(C,Before,R))return Encode(R);Fail(R,TEXT("datatable_row_not_found"),TEXT("Row not found; no Modify/remove/save."));return Encode(R);}

    auto Old=MakeShared<FJsonObject>();const bool Matched=!Remove&&Existed&&RowValues(C,Key,Old)&&ContainsExpected(MakeShared<FJsonValueObject>(Requested.ToSharedRef()),MakeShared<FJsonValueObject>(Old));if(!S.StableSource())return Encode(R);if(!Remove)R.Data->SetBoolField(TEXT("already_matched"),Matched);

    FObservation Final;bool HasFinal=false;

    auto Run=[&]()

    {

        R.Stage=Remove?TEXT("remove_row"):TEXT("apply_row");R.Attempted=true;R.Known=false;if(!S.BeginTransaction())return;

        if(!S.StableSource())return;FObservation Entry;if(!Capture(C,Entry,R,TEXT("native_row_transaction_entry"))||!S.StableSource())return;

        if(!MirrorReadLiteralEqual(Before.State,Entry.State)||!MirrorReadLiteralEqual(Before.Source,Entry.Source)||!MirrorReadLiteralEqual(Before.References,Entry.References)||!MirrorReadLiteralEqual(Before.DiskState,Entry.DiskState)||!MirrorReadLiteralEqual(Before.OtherDirty,Entry.OtherDirty)){S.Stop(TEXT("datatable_native_row_transaction_entry_changed"),TEXT("Transaction entry changed selected/source state; no Modify."));return;}

        const FName RowStructNameBefore=C.Table->RowStructName_DEPRECATED;const FTopLevelAssetPath RowStructPathBefore=C.Table->RowStructPathName;

        const bool WasDirty=C.Table->GetPackage()->IsDirty();ITransaction* const Undo=GUndo;const bool Modified=C.Table->Modify();Count(R,TEXT("explicit_modify_calls"));R.Data->SetBoolField(TEXT("modify_returned"),Modified);

        if(!S.AfterModify(WasDirty,Undo,Modified)||!S.AfterModifyDerivedMetadata(RowStructNameBefore,RowStructPathBefore,Undo,Modified))return;

        FObservation ModifiedState;if(!Capture(C,ModifiedState,R,TEXT("native_row_after_modify"))||!NativeRowFences(C,Before,ModifiedState,NAME_None,false,R))return;

        bool Removed=false;

        if(Remove){Count(R,TEXT("editor_remove_row_calls"));Removed=FDataTableEditorUtils::RemoveRow(C.Table.Get(),Key);R.Data->SetBoolField(TEXT("remove_helper_returned"),Removed);if(!Removed){S.Stop(TEXT("datatable_remove_postcondition_failed"),TEXT("Actual remove helper false; row prefix unadopted, no save."));return;}if(!S.AcceptRows(Key,true,nullptr))return;R.Data->SetBoolField(TEXT("row_retirement_observed"),true);}

        else

        {

            uint8* Memory=nullptr;if(Existed){auto Found=C.Table->GetRowMap().Find(Key);Memory=Found?*Found:nullptr;}else{Count(R,TEXT("editor_add_row_calls"));Memory=FDataTableEditorUtils::AddRow(C.Table.Get(),Key);R.Data->SetBoolField(TEXT("row_memory_returned"),Memory!=nullptr);if(!Memory){S.Stop(TEXT("datatable_row_allocation_failed"),TEXT("Actual helper prefix retained; null storage, no import/save."));return;}if(!S.AcceptRows(Key,false,Memory))return;R.Data->SetBoolField(TEXT("row_creation_observed"),true);}

            R.Data->SetBoolField(TEXT("row_memory_returned"),Memory!=nullptr);if(!S.StableSource())return;auto Current=C.Table->GetRowMap().Find(Key);const bool Routing=Memory&&Current&&*Current==Memory;R.Data->SetBoolField(TEXT("row_memory_current"),Routing);if(!Routing){S.Stop(TEXT("datatable_row_memory_unobservable"),TEXT("Current exact selected storage required before converter."));return;}

            Count(R,TEXT("json_conversion_calls"));const bool Converted=FJsonObjectConverter::JsonObjectToUStruct(Requested.ToSharedRef(),C.Type,Memory,0,0);R.Data->SetBoolField(TEXT("json_conversion_returned"),Converted);

            if(!S.AcceptRows(Key,false,Memory,true))return;if(!Converted){FObservation Partial;if(!Capture(C,Partial,R,TEXT("after_failed_operation")))return;R.Data->SetObjectField(TEXT("after"),RowSnapshot(C,Key));Fail(R,TEXT("datatable_row_conversion_failed"),TEXT("Original converter false retained actual default/partial row prefix; subsequent RowDataNotify0/save0. AddRow itself may already have RowList/row virtual effects."),true);return;}

        }

        if(!S.StableSource())return;C.Table->MarkPackageDirty();Count(R,TEXT("mark_package_dirty_calls"));if(!S.StableSource())return;

        if(!Remove){Count(R,TEXT("row_data_broadcast_calls"));FDataTableEditorUtils::BroadcastPostChange(C.Table.Get(),FDataTableEditorUtils::EDataTableChangeInfo::RowData);if(!S.StableSource())return;}

        FObservation After;if(!Capture(C,After,R,TEXT("after_observation"))||!S.StableSource())return;Final=After;HasFinal=true;R.Known=true;R.Changed=Remove||!Matched;

        if(!NativeRowFences(C,Before,After,Key,Remove,R))return;R.Data->SetObjectField(TEXT("after"),RowSnapshot(C,Key));if(!S.StableSource())return;auto Values=MakeShared<FJsonObject>();const bool Pass=Remove?(Removed&&!C.Table->GetRowMap().Contains(Key)&&C.Table->GetRowMap().Num()==Before.State->GetIntegerField(TEXT("row_count"))-1):(RowValues(C,Key,Values)&&ContainsExpected(MakeShared<FJsonValueObject>(Requested.ToSharedRef()),MakeShared<FJsonValueObject>(Values)));if(!S.StableSource())return;

        if(Remove){R.Data->SetBoolField(TEXT("removed"),Pass);R.Data->SetBoolField(TEXT("removal_observed"),Pass);}else{R.Data->SetBoolField(TEXT("created"),!Existed);R.Data->SetBoolField(TEXT("updated"),Existed);R.Data->SetObjectField(TEXT("readback_values"),Values);R.Data->SetBoolField(TEXT("requested_values_match"),Pass);R.Data->SetBoolField(TEXT("legacy_changed"),!Matched);R.Data->SetBoolField(TEXT("actual_selected_row_values_changed"),!Existed||!MirrorReadLiteralEqual(Old,Values));}

        auto Delta=MakeShared<FJsonObject>();Delta->SetNumberField(TEXT("affected_count"),Remove?(Pass?1:0):Matched?0:1);Delta->SetNumberField(TEXT("created_count"),!Remove&&!Existed?1:0);Delta->SetNumberField(TEXT("updated_count"),!Remove&&Existed&&!Matched?1:0);Delta->SetNumberField(TEXT("removed_count"),Remove&&Pass?1:0);Delta->SetNumberField(TEXT("skipped_count"),!Remove&&Matched?1:0);Delta->SetNumberField(TEXT("failed_count"),Pass?0:1);R.Data->SetObjectField(TEXT("delta"),Delta);R.Data->SetBoolField(TEXT("independent_readback_passed"),Pass);

        // Preserve stock save-before-requested-subset postcondition semantics,

        // including ignored lowercamel/unknown input with a real saved row prefix.

        if(!NativeRowSave(C,After,R,Final))return;R.Data->SetBoolField(TEXT("postcondition_verified"),Pass);

        if(!Pass){Fail(R,Remove?TEXT("datatable_remove_postcondition_failed"):TEXT("datatable_row_postcondition_failed"),TEXT("Actual selected row/save prefix retained; requested value/absence/count predicate failed."));return;}Complete(R);

    };

    Run();NativeRowClose(C,Final,HasFinal,R);return Encode(R);

}

FString NativeRowList(FContext& C,const FObservation& Before,FResult& R,const FString& Contains,int32 MaxRows,bool IncludeValues)
{
    auto& S=*C.Native;if(!S.StableSource())return Encode(R);const int32 Limit=FMath::Clamp(MaxRows,1,5000);R.Data->SetNumberField(TEXT("max_rows_requested"),MaxRows);R.Data->SetNumberField(TEXT("max_rows_effective"),Limit);R.Data->SetStringField(TEXT("row_iteration_scope"),TEXT("original unsorted GetRowMap order, IgnoreCase substring filter; copied routing across native export boundaries"));TArray<TSharedPtr<FJsonValue>> Items;int32 Matched=0;const auto Rows=S.Rows;
    for(const auto& Pair:Rows){if(!Contains.IsEmpty()&&!Pair.Key.ToString().Contains(Contains))continue;++Matched;R.Data->SetNumberField(TEXT("matched_prefix_count"),Matched);if(Items.Num()>=Limit)continue;auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("row_name"),Pair.Key.ToString());Items.Add(MakeShared<FJsonValueObject>(Row));R.Data->SetArrayField(TEXT("rows"),Items);if(IncludeValues){auto Values=MakeShared<FJsonObject>();Row->SetObjectField(TEXT("values"),Values);if(!S.Export(Pair.Key,Pair.Value,Values,false)){R.Data->SetBoolField(TEXT("list_complete"),false);return Encode(R);}}}
    R.Data->SetBoolField(TEXT("list_complete"),true);R.Data->SetArrayField(TEXT("rows"),Items);R.Data->SetNumberField(TEXT("total_rows"),Rows.Num());R.Data->SetNumberField(TEXT("matched_rows"),Matched);R.Data->SetNumberField(TEXT("returned_rows"),Items.Num());R.Data->SetBoolField(TEXT("truncated"),Matched>Items.Num());if(!S.StableSource()||!EndRead(C,Before,R))return Encode(R);Complete(R);return Encode(R);
}

FString NativeRowCreate(const FString& RequestedPath,const FString& RequestedStruct,bool RequestedSave)
{
    FResult R;Init(R,TEXT("create_datatable"),RequestedSave);if(NativeRowWriterRunning||StringUdsWriterRunning){Fail(R,TEXT("datatable_native_writer_busy"),TEXT("Current DataTable writer is active; no nested load."));return Encode(R);}TGuardValue<bool> Busy(NativeRowWriterRunning,true);FContext C;C.Path=Normalize(RequestedPath);C.Package=FPackageName::ObjectPathToPackageName(C.Path);R.Data->SetStringField(TEXT("datatable"),C.Path);
    if(!AssetPath(C.Path,true)||FPackageName::ObjectPathToObjectName(C.Path)!=FPackageName::GetShortName(C.Package)){Fail(R,TEXT("datatable_path_invalid"),TEXT("Fresh exact Game package/object asset identity required."));return Encode(R);}if(!RequestedStruct.StartsWith(TEXT("/Script/"))||!CleanPath(RequestedStruct)||RequestedStruct.Len()>1024){Fail(R,TEXT("datatable_native_row_struct_pending"),TEXT("Exact compiled-in native row struct object path required; short-name/other source aliases remain incomplete."));return Encode(R);}
    C.Native=MakeUnique<FNativeRowScope>(R);auto& S=*C.Native;if(!S.Context()){S.Bad(TEXT("editor_context"),TEXT("Editor GT outside PIE/SIE"),TEXT("unavailable"),false);return Encode(R);}Count(R,TEXT("row_struct_load_count"));R.Data->SetStringField(TEXT("row_struct_load_requested"),RequestedStruct);UScriptStruct* Returned=LoadObject<UScriptStruct>(nullptr,*RequestedStruct);TStrongObjectPtr<UScriptStruct> HoldType(Returned);R.Data->SetBoolField(TEXT("row_struct_load_returned"),Returned!=nullptr);if(!S.Context()){S.Bad(TEXT("source_load_context"));return Encode(R);}if(!Loaded(Returned)||!Returned->GetPathName().Equals(RequestedStruct,ESearchCase::CaseSensitive)){Fail(R,TEXT("datatable_row_struct_unsupported"),TEXT("Actual loaded native row source must match the requested identity."));return Encode(R);}C.Type=Returned;if(!S.BindType(Returned))return Encode(R);R.Data->SetObjectField(TEXT("source_before"),S.SourceBaseline);
    const bool Valid=FDataTableEditorUtils::IsValidTableStruct(Returned);if(!S.StableSource())return Encode(R);if(!Valid){Fail(R,TEXT("datatable_row_struct_unsupported"),TEXT("Public DataTable native row-struct admission returned false."));return Encode(R);}auto DiskBefore=MakeShared<FJsonObject>(),DirtyBefore=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("disk_before"),DiskBefore);R.Data->SetObjectField(TEXT("other_dirty_before"),DirtyBefore);if(!Disk(C.Package,DiskBefore)||!Dirty(C.Package,DirtyBefore)||!S.StableSource())return Encode(R);
    auto Fresh=[&](){const bool Registry=UEditorAssetLibrary::DoesAssetExist(C.Path);if(!S.StableSource())return false;auto D=MakeShared<FJsonObject>();if(!Disk(C.Package,D)||!S.StableSource())return false;if(Registry||D->GetBoolField(TEXT("exists"))||FindObject<UObject>(nullptr,*C.Path)||FindPackage(nullptr,*C.Package)){Fail(R,TEXT("datatable_target_exists"),TEXT("Existing object/package/file/registry slot is never overwritten or reused."));return false;}return true;};if(!Fresh())return Encode(R);auto Before=MakeShared<FJsonObject>();Before->SetBoolField(TEXT("exists"),false);Before->SetStringField(TEXT("asset_path"),RequestedPath);R.Data->SetObjectField(TEXT("before"),Before);FObservation Final;bool HasFinal=false;TStrongObjectPtr<UPackage> PackageHold;
    auto Run=[&]()
    {
        R.Attempted=true;R.Known=false;R.Stage=TEXT("create_package");if(!S.BeginTransaction()||!Fresh())return;Count(R,TEXT("package_create_attempts"));UPackage* Package=CreatePackage(*C.Package);PackageHold.Reset(Package);R.Data->SetBoolField(TEXT("created_package_returned"),Package!=nullptr);if(!S.StableSource())return;if(!Loaded(Package)||Package->GetClass()!=UPackage::StaticClass()||Package->GetOuter()||Package->GetPackage()!=Package||!Package->GetPathName().Equals(C.Package,ESearchCase::CaseSensitive)||!S.Pin(Package)){S.Stop(TEXT("datatable_package_creation_failed"),TEXT("Returned fresh package role is unavailable; retain allocation prefix."));return;}R.Data->SetObjectField(TEXT("created_package"),Identity(Package));if(!S.StableSource())return;const bool Registry=UEditorAssetLibrary::DoesAssetExist(C.Path);if(!S.StableSource())return;if(Registry||FindObject<UObject>(nullptr,*C.Path)){S.Stop(TEXT("datatable_target_exists"),TEXT("Requested object appeared during package creation; no overwrite."));return;}
        R.Stage=TEXT("create_datatable");Count(R,TEXT("asset_allocation_attempts"));UDataTable* New=NewObject<UDataTable>(Package,FName(*FPackageName::GetShortName(C.Package)),RF_Public|RF_Standalone);C.Table.Reset(New);R.Data->SetBoolField(TEXT("asset_allocation_returned"),New!=nullptr);if(!S.StableSource())return;if(!Loaded(New)||New->GetClass()!=UDataTable::StaticClass()||New->GetOuter()!=Package||New->GetPackage()!=Package||!New->GetPathName().Equals(C.Path,ESearchCase::CaseSensitive)||!New->GetRowMap().IsEmpty()){S.Stop(TEXT("datatable_asset_allocation_failed"),TEXT("Actual fresh native table role is unavailable; retained prefix, no retry."));return;}
        TMap<FName,const uint8*> Empty;New->CreateTableFromRawData(Empty,C.Type);R.Data->SetBoolField(TEXT("create_table_from_raw_data_returned"),true);if(!S.StableSource())return;if(!Loaded(New)||New->GetClass()!=UDataTable::StaticClass()||New->GetOuter()!=Package||New->GetPackage()!=Package||!New->GetPathName().Equals(C.Path,ESearchCase::CaseSensitive)||!New->GetRowMap().IsEmpty()||!New->RowStruct.IsResolved()||New->RowStruct.Get()!=C.Type){S.Stop(TEXT("datatable_fresh_table_current"),TEXT("Fresh empty current table/source must survive native initialization."));return;}
        New->RowStruct=C.Type;New->RowStructName_DEPRECATED=C.Type->GetFName();New->RowStructPathName=C.Type->GetStructPathName();New->SetFlags(RF_Transactional);if(!S.BindTable(New,C.Path,C.Package))return;R.Data->SetObjectField(TEXT("allocated_asset"),Identity(New));Count(R,TEXT("asset_registry_publication_calls"));FAssetRegistryModule::AssetCreated(New);R.Data->SetBoolField(TEXT("asset_registry_publication_returned"),true);if(!S.StableSource())return;
        for(const UObject* Object:{static_cast<const UObject*>(New),static_cast<const UObject*>(Package)}){if(!S.StableSource())return;const bool Marked=Object->MarkPackageDirty();Count(R,TEXT("mark_package_dirty_calls"));R.Data->SetBoolField(TEXT("native_create_last_mark_dirty_return"),Marked);if(!S.TargetFlagsAfter()||!S.StableSource())return;}
        FObservation After;if(!Capture(C,After,R,TEXT("after_observation"))||!S.StableSource())return;Final=After;HasFinal=true;const bool Source=MirrorReadLiteralEqual(S.SourceBaseline,After.Source),Other=MirrorReadLiteralEqual(DirtyBefore,After.OtherDirty),File=MirrorReadLiteralEqual(DiskBefore,After.DiskState),Pass=New->GetRowMap().IsEmpty()&&New->GetRowStruct()==C.Type;R.Known=true;R.Changed=true;R.Data->SetBoolField(TEXT("observations_complete"),true);R.Data->SetBoolField(TEXT("source_preserved"),Source);R.Data->SetBoolField(TEXT("unselected_state_preserved"),Other);R.Data->SetBoolField(TEXT("independent_readback_passed"),Pass);auto Actual=MakeShared<FJsonObject>();Actual->SetBoolField(TEXT("exists"),true);Actual->SetStringField(TEXT("asset_path"),C.Path);Actual->SetStringField(TEXT("row_struct"),C.Type->GetPathName());Actual->SetNumberField(TEXT("row_count"),New->GetRowMap().Num());R.Data->SetObjectField(TEXT("after"),Actual);R.Data->SetStringField(TEXT("row_struct"),C.Type->GetPathName());auto Delta=MakeShared<FJsonObject>();for(const TCHAR* Key:{TEXT("affected_count"),TEXT("created_count")})Delta->SetNumberField(Key,1);for(const TCHAR* Key:{TEXT("updated_count"),TEXT("removed_count"),TEXT("skipped_count")})Delta->SetNumberField(Key,0);Delta->SetNumberField(TEXT("failed_count"),Pass?0:1);R.Data->SetObjectField(TEXT("delta"),Delta);
        if(!Source||!Other||!File||!Pass){S.Stop(TEXT("datatable_create_preservation_failed"),TEXT("Actual native table prefix differs from empty/source/disk/other-dirty plan; no save."));return;}if(!NativeRowSave(C,After,R,Final))return;R.Data->SetBoolField(TEXT("postcondition_verified"),true);Complete(R);
    };
    Run();NativeRowClose(C,Final,HasFinal,R);return Encode(R);
}




bool BeginRead(const FString& Path, FContext& C, FObservation& Before, FResult& R, bool Mutable = false)
{ if(!Resolve(Path,Mutable,C,R)){return false;} if(Capture(C,Before,R,TEXT("before_observation"))){return true;} if((C.MirrorRead&&C.MirrorRead->Stopped)||(C.Uds&&C.Uds->Stopped)||(C.Native&&C.Native->Stopped)){return false;} return Fail(R,TEXT("datatable_preflight_unobservable"),TEXT("Bounded exact native source/whole-table/reference/disk/dirty baseline required.")); }
bool EndRead(FContext& C, const FObservation& Before, FResult& R)
{
    FObservation After; if (!Capture(C, After, R, TEXT("after_observation"))) { if((C.MirrorRead&&C.MirrorRead->Stopped)||(C.Uds&&C.Uds->Stopped)||(C.Native&&C.Native->Stopped)){return false;} return Fail(R, TEXT("datatable_readback_unobservable"), TEXT("Read returned actual prefix but current target preservation is incomplete."), true); }
    const auto ReadEqual=[&](const auto& X,const auto& Y){return (C.MirrorRead||C.Uds||C.Native)?MirrorReadLiteralEqual(X,Y):Equal(X,Y);};
    const bool State = ReadEqual(Before.State, After.State), Source = ReadEqual(Before.Source, After.Source), Refs = ReadEqual(Before.References, After.References), File = ReadEqual(Before.DiskState, After.DiskState), Other = ReadEqual(Before.OtherDirty, After.OtherDirty);
    const bool Same = State && Source && Refs && File && Other; R.Known = true; R.Changed = !Same;
    R.Data->SetBoolField(TEXT("observations_complete"), Before.Complete && After.Complete); R.Data->SetBoolField(TEXT("source_preserved"), Source && Refs); R.Data->SetBoolField(TEXT("unselected_state_preserved"), State && Other); R.Data->SetBoolField(TEXT("independent_readback_passed"), Same);
    R.Data->SetBoolField(TEXT("read_state_preserved"), State); R.Data->SetBoolField(TEXT("read_native_sources_preserved"), Source); R.Data->SetBoolField(TEXT("read_references_preserved"), Refs); R.Data->SetBoolField(TEXT("read_disk_preserved"), File); R.Data->SetBoolField(TEXT("read_other_dirty_preserved"), Other);
    return Same || Fail(R, TEXT("datatable_read_preservation_failed"), TEXT("Fully observed no-authoring read scope changed; known changed=true and individual fences/raw before/after retained."));
}
void Complete(FResult& R) { R.Stage = TEXT("complete"); R.Success = true; R.Post = true; }
}

FString GetSchema(const FString& Path)
{
    FResult R; Init(R, TEXT("get_datatable_schema")); FContext C; FObservation Before; if (!BeginRead(Path, C, Before, R)) { return Encode(R); }
    R.Data->SetStringField(TEXT("row_struct"), C.Type->GetPathName()); R.Data->SetNumberField(TEXT("row_count"), C.Table->GetRowMap().Num()); R.Data->SetArrayField(TEXT("fields"), Before.Source->GetArrayField(TEXT("fields"))); if (!EndRead(C, Before, R)) { return Encode(R); } Complete(R); return Encode(R);
}
FString ListRows(const FString& Path, const FString& Contains, int32 MaxRows, bool IncludeValues)
{
    FResult R; Init(R, TEXT("list_datatable_rows")); bool HasNul = false; for (int32 I = 0; I < Contains.Len(); ++I) { HasNul |= Contains[I] == TCHAR('\0'); } if (Contains.Len() > 1024 || HasNul) { Fail(R, TEXT("datatable_filter_invalid"), TEXT("Bounded row-name substring required.")); return Encode(R); }
    FContext C; FObservation Before; if (!BeginRead(Path, C, Before, R)) { return Encode(R); } if(C.Native)return NativeRowList(C,Before,R,Contains,MaxRows,IncludeValues); const int32 Limit = FMath::Clamp(MaxRows, 1, 5000); R.Data->SetNumberField(TEXT("max_rows_requested"), MaxRows); R.Data->SetNumberField(TEXT("max_rows_effective"), Limit); R.Data->SetStringField(TEXT("row_iteration_scope"), TEXT("original unsorted GetRowMap order, IgnoreCase substring filter")); TArray<TSharedPtr<FJsonValue>> Rows; int32 Matched = 0;
    for (const auto& Pair : C.Table->GetRowMap()) { if (!Contains.IsEmpty() && !Pair.Key.ToString().Contains(Contains)) { continue; } ++Matched; R.Data->SetNumberField(TEXT("matched_prefix_count"), Matched); if (Rows.Num() >= Limit) { continue; } auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("row_name"), Pair.Key.ToString()); Rows.Add(MakeShared<FJsonValueObject>(Row)); R.Data->SetArrayField(TEXT("rows"), Rows); if (IncludeValues) { auto Values = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("values"), Values); const bool Exported=FJsonObjectConverter::UStructToJsonObject(C.Type, Pair.Value, Values, 0, 0); if(C.Uds&&!C.Uds->StableSource())return Encode(R);if(!MirrorReadAdmit(C.MirrorRead.Get())){return Encode(R);} if (!Exported) { R.Data->SetBoolField(TEXT("list_complete"), false); Fail(R, TEXT("datatable_row_serialize_failed"), TEXT("Actual bounded list prefix and partial current row retained; selected row cannot be serialized.")); return Encode(R); } } }
    R.Data->SetBoolField(TEXT("list_complete"), true); R.Data->SetArrayField(TEXT("rows"), Rows); R.Data->SetNumberField(TEXT("total_rows"), C.Table->GetRowMap().Num()); R.Data->SetNumberField(TEXT("matched_rows"), Matched); R.Data->SetNumberField(TEXT("returned_rows"), Rows.Num()); R.Data->SetBoolField(TEXT("truncated"), Matched > Rows.Num()); if (!EndRead(C, Before, R)) { return Encode(R); } Complete(R); return Encode(R);
}
FString GetRow(const FString& Path, const FString& Name)
{
    FResult R; Init(R, TEXT("get_datatable_row")); if (!RowName(Name)) { Fail(R, TEXT("datatable_row_name_invalid"), TEXT("Bounded non-None row name required.")); return Encode(R); } FContext C; FObservation Before; if (!BeginRead(Path, C, Before, R)) { return Encode(R); }
    R.Data->SetStringField(TEXT("row_name"), Name); auto Values = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("values"), Values); if (!C.Table->GetRowMap().Contains(FName(*Name))) { if((C.MirrorRead||C.Uds||C.Native)&&!EndRead(C,Before,R)){return Encode(R);} Fail(R, TEXT("datatable_row_not_found"), TEXT("Requested FName row not found.")); return Encode(R); } if (!RowValues(C, FName(*Name), Values)) { if((C.MirrorRead&&C.MirrorRead->Stopped)||(C.Uds&&C.Uds->Stopped)||(C.Native&&C.Native->Stopped)){return Encode(R);} Fail(R, TEXT("datatable_row_serialize_failed"), TEXT("Actual row export incomplete.")); return Encode(R); } if (!EndRead(C, Before, R)) { return Encode(R); } Complete(R); return Encode(R);
}
FString AddOrUpdateRow(const FString& Path, const FString& Name, const FString& ValuesJson, bool RequestedSave)
{
    FResult R; Init(R, TEXT("add_or_update_datatable_row"), RequestedSave); if(MirrorMutationRunning){Fail(R,TEXT("datatable_mirror_mutation_busy"),TEXT("Owned concrete Mirror writer is active; nested Add/Remove is rejected before load/SDK."));return Encode(R);} TSharedPtr<FJsonObject> Values;
    if (!RowName(Name) || ValuesJson.Len() < 2 || ValuesJson.Len() > MaxText / static_cast<int32>(sizeof(TCHAR)) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ValuesJson), Values) || !Values.IsValid()) { Fail(R, TEXT("datatable_row_values_invalid"), TEXT("Bounded row name and JSON object up to one MiB native TCHAR bytes required before target load/mutation.")); return Encode(R); } int32 JsonCount = 0; if (!JsonBounded(MakeShared<FJsonValueObject>(Values), 0, JsonCount)) { Fail(R, TEXT("datatable_row_values_invalid"), TEXT("Finite JSON depth32/value16384 limit required.")); return Encode(R); }
    R.Data->SetObjectField(TEXT("requested_values"), Values); R.Data->SetStringField(TEXT("json_import_scope"), TEXT("original authored-name JsonObjectToUStruct semantics: unknown/null/lowercase fields are not normalized; conversion/postcondition failure can retain actual write/save prefix")); FContext C; FObservation Before; if (!BeginRead(Path, C, Before, R, true)) { return Encode(R); } if(C.Native)return NativeRowMutation(C,Before,R,Name,Values,false); if(C.Uds)return StringUdsRowMutation(C,Before,R,Name,Values,false); if(C.MirrorRead&&C.MirrorRead->Mutable)return MirrorRowMutation(C,Before,R,Name,Values,false); if(!MirrorImportNamesBounded(C,Values.ToSharedRef(),R)) { return Encode(R); } const FName Key(*Name); const bool Existed = C.Table->GetRowMap().Contains(Key); auto RowBefore = RowSnapshot(C, Key); R.Data->SetObjectField(TEXT("before"), RowBefore); auto OldValues = MakeShared<FJsonObject>(); const bool Matched = Existed && RowValues(C, Key, OldValues) && ContainsExpected(MakeShared<FJsonValueObject>(Values), MakeShared<FJsonValueObject>(OldValues)); R.Data->SetBoolField(TEXT("already_matched"), Matched); R.Data->SetStringField(TEXT("row_name"), Name);
    R.Stage = TEXT("apply_row"); R.Attempted = true; R.Known = false; Count(R, TEXT("outer_transaction_calls")); const FScopedTransaction Transaction(NSLOCTEXT("CubelessDataTable", "AddUpdate", "Add or update DataTable row")); C.Table->Modify(); Count(R, TEXT("explicit_modify_calls")); uint8* Memory = nullptr;
    if (Existed) { uint8* const* Ptr = C.Table->GetRowMap().Find(Key); Memory = Ptr ? *Ptr : nullptr; } else { Count(R, TEXT("editor_add_row_calls")); Memory = FDataTableEditorUtils::AddRow(C.Table.Get(), Key); }
    R.Data->SetBoolField(TEXT("row_memory_returned"), Memory != nullptr); if (!Memory) { FObservation Partial; Capture(C, Partial, R, TEXT("after_failed_operation")); Fail(R, TEXT("datatable_row_allocation_failed"), TEXT("Modify/row-helper prefix retained; no conversion/save."), true); return Encode(R); }
    // Public AddRow delegates may replace/remove a row before returning its pointer.
    // Re-lookup and source validation precede any dereference of that returned storage.
    uint8* const* CurrentMemory = C.Table->GetRowMap().Find(Key); auto CurrentSource = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("source_after_row_helper"), CurrentSource);
    const bool Current = CurrentMemory && *CurrentMemory == Memory && C.Table->GetRowStruct() == C.Type && FindObject<UDataTable>(nullptr, *C.Path) == C.Table.Get() && Sources(C.Type, CurrentSource) && Equal(Before.Source, CurrentSource); R.Data->SetBoolField(TEXT("row_memory_current"), Current);
    if (!Current) { FObservation Partial; Capture(C, Partial, R, TEXT("after_failed_operation")); Fail(R, TEXT("datatable_row_memory_unobservable"), TEXT("Returned helper storage/source is no longer the selected current row; no conversion/save or unsafe dereference."), true); return Encode(R); }
    Count(R, TEXT("json_conversion_calls")); const bool Converted = FJsonObjectConverter::JsonObjectToUStruct(Values.ToSharedRef(), C.Type, Memory, 0, 0); R.Data->SetBoolField(TEXT("json_conversion_returned"), Converted);
    if (!Converted) { FObservation Partial; Capture(C, Partial, R, TEXT("after_failed_operation")); R.Data->SetObjectField(TEXT("after"), RowSnapshot(C, Key)); Fail(R, TEXT("datatable_row_conversion_failed"), TEXT("Original converter may have partially applied fields to allocated/existing row; retained actual prefix, no save/retry."), true); return Encode(R); }
    C.Table->MarkPackageDirty(); Count(R, TEXT("mark_package_dirty_calls")); Count(R, TEXT("row_data_broadcast_calls")); FDataTableEditorUtils::BroadcastPostChange(C.Table.Get(), FDataTableEditorUtils::EDataTableChangeInfo::RowData);
    FObservation After; if (!Capture(C, After, R, TEXT("after_observation"))) { Fail(R, TEXT("datatable_postwrite_unobservable"), TEXT("Row/callback prefix incomplete; save suppressed."), true); return Encode(R); } R.Known = true; R.Changed = !Matched;
    if (!Fences(Before, After, Key, false, R)) { return Encode(R); } R.Data->SetBoolField(TEXT("created"), !Existed); R.Data->SetBoolField(TEXT("updated"), Existed); R.Data->SetObjectField(TEXT("after"), RowSnapshot(C, Key)); auto Readback = MakeShared<FJsonObject>(); const bool ValuesPass = RowValues(C, Key, Readback) && ContainsExpected(MakeShared<FJsonValueObject>(Values), MakeShared<FJsonValueObject>(Readback)); R.Data->SetObjectField(TEXT("readback_values"), Readback); R.Data->SetBoolField(TEXT("requested_values_match"), ValuesPass);
    // Original save occurs before the requested subset postcondition is reported.
    auto Delta = MakeShared<FJsonObject>(); Delta->SetNumberField(TEXT("affected_count"), Matched ? 0 : 1); Delta->SetNumberField(TEXT("created_count"), Existed ? 0 : 1); Delta->SetNumberField(TEXT("updated_count"), Existed && !Matched ? 1 : 0); Delta->SetNumberField(TEXT("removed_count"), 0); Delta->SetNumberField(TEXT("skipped_count"), Matched ? 1 : 0); Delta->SetNumberField(TEXT("failed_count"), ValuesPass ? 0 : 1); R.Data->SetObjectField(TEXT("delta"), Delta);
    R.Data->SetBoolField(TEXT("legacy_changed"), !Matched); R.Data->SetBoolField(TEXT("actual_selected_row_values_changed"), !Existed || !Equal(OldValues, Readback));
    R.Data->SetBoolField(TEXT("independent_readback_passed"), ValuesPass); if (!Save(C, After, R)) { return Encode(R); } R.Data->SetBoolField(TEXT("postcondition_verified"), ValuesPass);
    if (!ValuesPass) { Fail(R, TEXT("datatable_row_postcondition_failed"), TEXT("Converter/save prefix succeeded but requested values do not match; unknown/null/case field quirks are not normalized or replayed.")); return Encode(R); } Complete(R); return Encode(R);
}
FString RemoveRow(const FString& Path, const FString& Name, bool RequestedSave)
{
    FResult R; Init(R, TEXT("remove_datatable_row"), RequestedSave); if(MirrorMutationRunning){Fail(R,TEXT("datatable_mirror_mutation_busy"),TEXT("Owned concrete Mirror writer is active; nested Add/Remove is rejected before load/SDK."));return Encode(R);} if (!RowName(Name)) { Fail(R, TEXT("datatable_row_name_invalid"), TEXT("Bounded non-None row name required.")); return Encode(R); } FContext C; FObservation Before; if (!BeginRead(Path, C, Before, R, true)) { return Encode(R); } if(C.Native)return NativeRowMutation(C,Before,R,Name,nullptr,true); if(C.Uds)return StringUdsRowMutation(C,Before,R,Name,nullptr,true); if(C.MirrorRead&&C.MirrorRead->Mutable)return MirrorRowMutation(C,Before,R,Name,nullptr,true); const FName Key(*Name); R.Data->SetStringField(TEXT("row_name"), Name); R.Data->SetObjectField(TEXT("before"), RowSnapshot(C, Key)); if (!C.Table->GetRowMap().Contains(Key)) { Fail(R, TEXT("datatable_row_not_found"), TEXT("Row not found; no Modify/remove/save.")); return Encode(R); }
    R.Stage = TEXT("remove_row"); R.Attempted = true; R.Known = false; Count(R, TEXT("outer_transaction_calls")); const FScopedTransaction Transaction(NSLOCTEXT("CubelessDataTable", "Remove", "Remove DataTable row")); C.Table->Modify(); Count(R, TEXT("explicit_modify_calls")); Count(R, TEXT("editor_remove_row_calls")); const bool Removed = FDataTableEditorUtils::RemoveRow(C.Table.Get(), Key); R.Data->SetBoolField(TEXT("remove_helper_returned"), Removed); C.Table->MarkPackageDirty(); Count(R, TEXT("mark_package_dirty_calls")); FObservation After;
    if (!Capture(C, After, R, TEXT("after_observation"))) { Fail(R, TEXT("datatable_postwrite_unobservable"), TEXT("Removal/callback prefix incomplete; save suppressed."), true); return Encode(R); } R.Known = true; R.Changed = true; R.Data->SetObjectField(TEXT("after"), RowSnapshot(C, Key)); const bool Pass = Removed && !C.Table->GetRowMap().Contains(Key) && C.Table->GetRowMap().Num() == Before.State->GetIntegerField(TEXT("row_count")) - 1; R.Data->SetBoolField(TEXT("removal_observed"), Pass);
    auto Delta = MakeShared<FJsonObject>(); Delta->SetNumberField(TEXT("affected_count"), Pass ? 1 : 0); Delta->SetNumberField(TEXT("created_count"), 0); Delta->SetNumberField(TEXT("updated_count"), 0); Delta->SetNumberField(TEXT("removed_count"), Pass ? 1 : 0); Delta->SetNumberField(TEXT("skipped_count"), 0); Delta->SetNumberField(TEXT("failed_count"), Pass ? 0 : 1); R.Data->SetObjectField(TEXT("delta"), Delta);
    if (!Fences(Before, After, Key, true, R)) { return Encode(R); } R.Data->SetBoolField(TEXT("removed"), Pass); R.Data->SetBoolField(TEXT("independent_readback_passed"), Pass); if (!Save(C, After, R)) { return Encode(R); } R.Data->SetBoolField(TEXT("postcondition_verified"), Pass); if (!Pass) { Fail(R, TEXT("datatable_remove_postcondition_failed"), TEXT("Actual remove/save prefix retained but row absence/count differs.")); return Encode(R); } Complete(R); return Encode(R);
}
FString Create(const FString& RequestedPath, const FString& RequestedStruct, bool RequestedSave)
{
    if(StringUdsWriterRunning||NativeRowWriterRunning){FResult Busy;Init(Busy,TEXT("create_datatable"),RequestedSave);Fail(Busy,TEXT("datatable_string_uds_writer_busy"),TEXT("String1 UDS writer active; nested create rejected before load."));return Encode(Busy);}
    if(Normalize(RequestedStruct).StartsWith(TEXT("/Game/")))return StringUdsCreate(RequestedPath,RequestedStruct,RequestedSave);
    if(RequestedStruct!=TagRowPath&&RequestedStruct!=MirrorRowPath)return NativeRowCreate(RequestedPath,RequestedStruct,RequestedSave);
    FResult R; Init(R, TEXT("create_datatable"), RequestedSave); if (!IsEditorGameThread() || IsPlaySessionActive()) { Fail(R, TEXT("datatable_editor_context_required"), TEXT("Editor game thread without PIE/SIE required.")); return Encode(R); }
    FContext C; C.Path = Normalize(RequestedPath); C.Package = FPackageName::ObjectPathToPackageName(C.Path); R.Data->SetStringField(TEXT("datatable"), C.Path); if (!AssetPath(C.Path, true) || FPackageName::ObjectPathToObjectName(C.Path) != FPackageName::GetShortName(C.Package)) { Fail(R, TEXT("datatable_path_invalid"), TEXT("Fresh exact Game package/object asset identity required.")); return Encode(R); }
    if (RequestedStruct != TagRowPath && RequestedStruct != MirrorRowPath) { Fail(R, TEXT("datatable_row_struct_unsupported"), TEXT("Concrete native GameplayTagTableRow or MirrorTableRow only; base TableRowBase/custom/arbitrary row structs are explicitly incomplete.")); return Encode(R); }
    Count(R, TEXT("row_struct_load_count")); C.Type = LoadObject<UScriptStruct>(nullptr, *RequestedStruct); auto SourcesBefore = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("source_before"), SourcesBefore); if (!Loaded(C.Type) || C.Type->GetPathName() != RequestedStruct || !Sources(C.Type, SourcesBefore)) { Fail(R, TEXT("datatable_native_source_unobservable"), TEXT("Loaded exact admitted native row layout and clean native source identity/schema required before allocation.")); return Encode(R); }
    auto DiskBefore = MakeShared<FJsonObject>(), DirtyBefore = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("disk_before"), DiskBefore); R.Data->SetObjectField(TEXT("other_dirty_before"), DirtyBefore);
    if (!Disk(C.Package, DiskBefore) || !Dirty(C.Package, DirtyBefore)) { Fail(R, TEXT("datatable_preflight_unobservable"), TEXT("Exact file/dirty absence baseline required.")); return Encode(R); }
    if (FindObject<UObject>(nullptr, *C.Path) || FindPackage(nullptr, *C.Package) || DiskBefore->GetBoolField(TEXT("exists")) || UEditorAssetLibrary::DoesAssetExist(C.Path)) { Fail(R, TEXT("datatable_target_exists"), TEXT("Existing asset/package/file/registry slot is not overwritten or reused.")); return Encode(R); }
    auto Before = MakeShared<FJsonObject>(); Before->SetBoolField(TEXT("exists"), false); Before->SetStringField(TEXT("asset_path"), RequestedPath); R.Data->SetObjectField(TEXT("before"), Before);
    R.Attempted = true; R.Known = false; R.Stage = TEXT("create_package"); Count(R, TEXT("outer_transaction_calls")); const FScopedTransaction Transaction(NSLOCTEXT("CubelessDataTable", "Create", "Create DataTable")); Count(R, TEXT("package_create_attempts")); UPackage* Package = CreatePackage(*C.Package); R.Data->SetObjectField(TEXT("created_package"), Identity(Package)); if (!Loaded(Package)) { Fail(R, TEXT("datatable_package_creation_failed"), TEXT("Package allocation prefix retained; no retry."), true); return Encode(R); }
    R.Stage = TEXT("create_datatable"); Count(R, TEXT("asset_allocation_attempts")); C.Table.Reset(NewObject<UDataTable>(Package, FName(*FPackageName::GetShortName(C.Package)), RF_Public | RF_Standalone)); R.Data->SetObjectField(TEXT("allocated_asset"), Identity(C.Table.Get())); if (!Loaded(C.Table.Get())) { Fail(R, TEXT("datatable_asset_allocation_failed"), TEXT("Package/asset allocation prefix retained; no rollback."), true); return Encode(R); }
    TMap<FName, const uint8*> Empty; C.Table->CreateTableFromRawData(Empty, C.Type); C.Table->RowStruct = C.Type; C.Table->RowStructName_DEPRECATED = C.Type->GetFName(); C.Table->RowStructPathName = C.Type->GetStructPathName(); C.Table->SetFlags(RF_Transactional); Count(R, TEXT("asset_registry_publication_calls")); FAssetRegistryModule::AssetCreated(C.Table.Get()); C.Table->MarkPackageDirty(); Count(R, TEXT("mark_package_dirty_calls")); Package->MarkPackageDirty(); Count(R, TEXT("mark_package_dirty_calls"));
    FObservation After; if (!Capture(C, After, R, TEXT("after_observation"))) { Fail(R, TEXT("datatable_create_postcondition_unobservable"), TEXT("Published native allocation/callback prefix retained; complete observation missing, no save."), true); return Encode(R); } R.Known = true; R.Changed = true;
    const bool Source = Equal(SourcesBefore, After.Source), Other = Equal(DirtyBefore, After.OtherDirty), File = Equal(DiskBefore, After.DiskState); const bool Pass = C.Table->GetRowMap().IsEmpty() && C.Table->GetRowStruct() == C.Type;
    R.Data->SetBoolField(TEXT("observations_complete"), true); R.Data->SetBoolField(TEXT("source_preserved"), Source); R.Data->SetBoolField(TEXT("unselected_state_preserved"), Other); R.Data->SetBoolField(TEXT("independent_readback_passed"), Pass); auto Actual = MakeShared<FJsonObject>(); Actual->SetBoolField(TEXT("exists"), true); Actual->SetStringField(TEXT("asset_path"), C.Path); Actual->SetStringField(TEXT("row_struct"), C.Type->GetPathName()); Actual->SetNumberField(TEXT("row_count"), C.Table->GetRowMap().Num()); R.Data->SetObjectField(TEXT("after"), Actual); R.Data->SetStringField(TEXT("row_struct"), C.Type->GetPathName());
    auto Delta = MakeShared<FJsonObject>(); Delta->SetNumberField(TEXT("affected_count"), 1); Delta->SetNumberField(TEXT("created_count"), 1); Delta->SetNumberField(TEXT("updated_count"), 0); Delta->SetNumberField(TEXT("removed_count"), 0); Delta->SetNumberField(TEXT("skipped_count"), 0); Delta->SetNumberField(TEXT("failed_count"), Pass ? 0 : 1); R.Data->SetObjectField(TEXT("delta"), Delta);
    if (!Source || !Other || !File || !Pass) { Fail(R, TEXT("datatable_create_preservation_failed"), TEXT("Actual new asset prefix observed but empty/native/source/disk/other-dirty fence differs; no save.")); return Encode(R); } if (!Save(C, After, R)) { return Encode(R); } R.Data->SetBoolField(TEXT("postcondition_verified"), true); Complete(R); return Encode(R);
}
// MH: stock Row/Break authoring, not runtime DataTable lookup or a generic UDS policy.
namespace
{
uint32 RowPinFlags(const UEdGraphPin* P)
{
    return (P->bHidden?1U:0U)|(P->bNotConnectable?2U:0U)|(P->bDefaultValueIsReadOnly?4U:0U)|(P->bDefaultValueIsIgnored?8U:0U)|(P->bOrphanedPin?16U:0U)|(P->bAdvancedView?32U:0U)|(P->bDeprecated?64U:0U)|(P->bDisplayAsMutableRef?128U:0U)|(P->bAllowFriendlyName?256U:0U)|(P->ShouldSavePinIfOrphaned()?512U:0U);
}
bool RowTypeSame(const FEdGraphPinType& A,const FEdGraphPinType& B)
{
    return A==B&&A.bIsReference==B.bIsReference&&A.bIsConst==B.bIsConst&&A.bIsWeakPointer==B.bIsWeakPointer&&A.bIsUObjectWrapper==B.bIsUObjectWrapper&&A.bSerializeAsSinglePrecisionFloat==B.bSerializeAsSinglePrecisionFloat&&A.PinValueType.TerminalCategory==B.PinValueType.TerminalCategory&&A.PinValueType.TerminalSubCategory==B.PinValueType.TerminalSubCategory&&A.PinValueType.TerminalSubCategoryObject==B.PinValueType.TerminalSubCategoryObject&&A.PinValueType.bTerminalIsConst==B.PinValueType.bTerminalIsConst&&A.PinValueType.bTerminalIsWeakPointer==B.PinValueType.bTerminalIsWeakPointer&&A.PinValueType.bTerminalIsUObjectWrapper==B.PinValueType.bTerminalIsUObjectWrapper;
}
struct FRowPin
{
    UEdGraphPin* Address=nullptr;FGuid Id,Persistent;FName Name;EEdGraphPinDirection Direction=EGPD_MAX;
    FEdGraphPinType Type;FString Default,Auto;FText Text;TObjectPtr<UObject> Object=nullptr;
    uint32 Flags=0;UEdGraphPin* Parent=nullptr;TArray<UEdGraphPin*> Children,Links;
};
FRowPin CopyRowPin(UEdGraphPin* P)
{
    FRowPin V;V.Address=P;V.Id=P->PinId;V.Persistent=P->PersistentGuid;V.Name=P->PinName;V.Direction=P->Direction;V.Type=P->PinType;V.Default=P->DefaultValue;V.Auto=P->AutogeneratedDefaultValue;V.Text=P->DefaultTextValue;V.Object=P->DefaultObject;V.Flags=RowPinFlags(P);V.Parent=P->ParentPin;V.Children=P->SubPins;V.Links=P->LinkedTo;return V;
}
bool RowPinSame(const FRowPin& V,UEdGraphPin* P,UEdGraphNode* N)
{
    return P==V.Address&&P&&!P->WasTrashed()&&P->GetOwningNodeUnchecked()==N&&P->PinId==V.Id&&P->PinName==V.Name&&P->Direction==V.Direction&&RowTypeSame(P->PinType,V.Type)&&P->DefaultValue.Equals(V.Default,ESearchCase::CaseSensitive)&&P->AutogeneratedDefaultValue.Equals(V.Auto,ESearchCase::CaseSensitive)&&P->DefaultObject==V.Object&&P->DefaultTextValue.IdenticalTo(V.Text)&&RowPinFlags(P)==V.Flags&&P->ParentPin==V.Parent&&P->SubPins==V.Children&&P->LinkedTo==V.Links;
}
struct FRowGraph
{
    FResult& R;FStringUdsScope Source;
    TStrongObjectPtr<UBlueprint> Blueprint;TStrongObjectPtr<UEdGraph> Graph;
    TStrongObjectPtr<UClass> Parent,Generated,Skeleton;TStrongObjectPtr<UEdGraphSchema_K2> Schema;
    TArray<TStrongObjectPtr<UObject>> Holds;
    struct FNative{TStrongObjectPtr<UClass> Class;TStrongObjectPtr<UPackage> Package;FString Path,PackagePath;EClassFlags Flags=CLASS_None;int32 Size=0;uint32 PackageFlags=0;};TArray<FNative> NativeSources;
    TArray<TObjectPtr<UEdGraph>> Functions,Events,Delegates,Macros;
    TArray<TObjectPtr<UEdGraphNode>> Handles;
    struct FNode
    {
        UEdGraphNode* N=nullptr;UClass* Class=nullptr;FString Path;FGuid Guid;int32 X=0,Y=0;EObjectFlags Flags=RF_NoFlags;
        TArray<UEdGraphPin*> Addresses;TArray<FRowPin> Pins;
        FMemberReference Binding;bool HasBinding=false,IsVariable=false,IsTerminator=false;
        TArray<FUserPinInfo> UserPins;TObjectPtr<UScriptStruct> BreakType=nullptr;
    };
    TArray<FNode> Nodes;UK2Node_FunctionEntry* Entry=nullptr;FMemberReference EntryReference;int32 EntryFlags=0;
    TArray<FBPVariableDescription> Locals;FGuid GraphGuid;FString BlueprintPath,GraphPath;
    UPackage* Package=nullptr;uint32 PackageFlags=0;bool Dirty=false,AllowDirty=false,Ready=false;
    EBlueprintStatus Status=BS_Unknown;UK2Node* Added=nullptr;UClass* AddedClass=nullptr;FString AddedPath;FGuid AddedGuid;bool AddedIsBreak=false,GuidReady=false,Attached=false,FreezeAdded=false;FNode AddedSnapshot;
    explicit FRowGraph(FResult& In):R(In),Source(In,false,true){}
    bool Bad(const TCHAR* Operand)
    {
        if(!R.Data->HasField(TEXT("row_graph_first_mismatch")))
        {auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("operand"),Operand);J->SetStringField(TEXT("object_path"),GraphPath.IsEmpty()?BlueprintPath:GraphPath);J->SetStringField(TEXT("comparison"),TEXT("First failed named boolean boundary; no diagnostic SDK or failed-object reread."));J->SetStringField(TEXT("expected"),TEXT("true"));J->SetStringField(TEXT("actual"),TEXT("false"));R.Data->SetObjectField(TEXT("row_graph_first_mismatch"),J);}
        return Fail(R,TEXT("datatable_graph_scope_changed"),TEXT("Exact String1 UDS/PresetLoader graph role changed; actual prefix retained, no next SDK or retry."),R.Attempted);
    }
    // NR: typed descriptors are copied only after the source and exact K2 schema
    // have been bound. Source.Property remains the historical String1 member.
    struct FScalarGraphField
    {
        FName Name;FString Authored;FGuid Guid;FEdGraphPinType Type;
    };
    TArray<FScalarGraphField> ScalarGraphFields;
    bool ScalarBad(const FString& Object,const FString& Property,const FString& Expected,const FString& Actual)
    {
        if(!R.Data->HasField(TEXT("row_graph_scalar_first_mismatch")))
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("object"),Object);
            J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),Expected.Left(4096));J->SetStringField(TEXT("actual"),Actual.Left(4096));
            J->SetBoolField(TEXT("copied_observed_values_only"),true);R.Data->SetObjectField(TEXT("row_graph_scalar_first_mismatch"),J);
        }
        return Bad(TEXT("scalar_field/current_pin_role"));
    }
    static FString ScalarTypeSummary(const FEdGraphPinType& T)
    {
        // Pin type objects are weak references. Get performs a current weak lookup,
        // without loading/resolving an object or reading its properties.
        const auto* Object=T.PinSubCategoryObject.Get();const auto* TerminalObject=T.PinValueType.TerminalSubCategoryObject.Get();
        const bool Resolved=Object!=nullptr,TerminalResolved=TerminalObject!=nullptr;
        return FString::Printf(TEXT("category=%s sub=%s object_current=%d object=%p container=%d ref=%d const=%d weak=%d wrapper=%d single=%d terminal=%s/%s terminal_current=%d terminal_object=%p terminal_const=%d terminal_weak=%d terminal_wrapper=%d"),
            *T.PinCategory.ToString(),*T.PinSubCategory.ToString(),Resolved,Object,int32(T.ContainerType),T.bIsReference,T.bIsConst,T.bIsWeakPointer,T.bIsUObjectWrapper,T.bSerializeAsSinglePrecisionFloat,
            *T.PinValueType.TerminalCategory.ToString(),*T.PinValueType.TerminalSubCategory.ToString(),TerminalResolved,TerminalObject,T.PinValueType.bTerminalIsConst,T.PinValueType.bTerminalIsWeakPointer,T.PinValueType.bTerminalIsUObjectWrapper);
    }
    bool CaptureScalarGraphTypes()
    {
        if(!Source.ScalarProfile)return true;
        if(!Stable()||!Schema.IsValid()||!ScalarGraphFields.IsEmpty()||Source.ScalarFields.IsEmpty())return Bad(TEXT("scalar_graph_type_capture_entry"));
        for(int32 I=0;I<Source.ScalarFields.Num();++I)
        {
            const auto& F=Source.ScalarFields[I];
            if(!F.Property->HasAnyPropertyFlags(CPF_BlueprintVisible))return ScalarBad(Source.TypePath,F.Name+TEXT(".BlueprintVisible"),TEXT("true"),TEXT("false"));
            FScalarGraphField V;V.Name=F.FieldName;V.Authored=F.Authored;V.Guid=F.Guid;
            R.Data->SetNumberField(TEXT("scalar_pin_type_conversion_attempted_prefix"),I+1);
            const bool Converted=Schema->ConvertPropertyToPinType(F.Property,V.Type);
            // No field or pin from before a public SDK call is dereferenced until
            // source/layout/current graph preservation has been re-established.
            if(!Stable())return false;R.Data->SetNumberField(TEXT("scalar_pin_type_conversion_returned_prefix"),I+1);
            if(!Converted)return ScalarBad(Source.TypePath,V.Name.ToString()+TEXT(".pin_type_conversion"),TEXT("true"),TEXT("false"));
            if(V.Type.ContainerType!=EPinContainerType::None||V.Type.PinSubCategoryObject!=nullptr||V.Type.bIsReference||V.Type.bIsConst||V.Type.bIsWeakPointer||V.Type.bIsUObjectWrapper)
                return ScalarBad(Source.TypePath,V.Name.ToString()+TEXT(".scalar_pin_type"),TEXT("plain public scalar value"),V.Type.PinCategory.ToString()+TEXT("/")+V.Type.PinSubCategory.ToString());
            ScalarGraphFields.Add(MoveTemp(V));
        }
        return Stable();
    }
    bool ScalarShape(UK2Node* N,bool Break,bool Initial)
    {
        if(!Raw()||!Loaded(N)||N->GetOuter()!=Graph.Get()||!N->NodeGuid.IsValid()||N->GetClass()!=(Break?UK2Node_BreakStruct::StaticClass():UK2Node_GetDataTableRow::StaticClass()))return Bad(TEXT("scalar_selected_stock_node"));
        if(ScalarGraphFields.Num()!=Source.ScalarFields.Num()||ScalarGraphFields.IsEmpty())return Bad(TEXT("scalar_graph_field_inventory"));
        if(Break&&static_cast<UK2Node_BreakStruct*>(N)->StructType!=Source.Type.Get())return Bad(TEXT("scalar_break_StructType"));
        const int32 ExpectedCount=Break?ScalarGraphFields.Num()+1:6;
        if(N->Pins.Num()!=ExpectedCount)return ScalarBad(N->GetPathName(),TEXT("pin_count"),LexToString(ExpectedCount),LexToString(N->Pins.Num()));
        TSet<FName> Seen;TSet<FGuid> PhysicalIds;TSet<FGuid> AuthoredGuids;
        for(auto* P:N->Pins)
        {
            if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||PhysicalIds.Contains(P->PinId)||Seen.Contains(P->PinName)||P->ParentPin||!P->SubPins.IsEmpty()||P->bOrphanedPin||P->bDeprecated||P->bHidden||P->bNotConnectable||P->DefaultObject!=nullptr||!P->DefaultTextValue.IsEmpty()||P->DefaultValue.Len()>MaxText||P->AutogeneratedDefaultValue.Len()>MaxText||(Initial&&!P->LinkedTo.IsEmpty()))return Bad(TEXT("scalar_selected_whole_pin"));
            PhysicalIds.Add(P->PinId);Seen.Add(P->PinName);FEdGraphPinType Expected;EEdGraphPinDirection Direction=EGPD_Input;
            if(Break)
            {
                if(P->Direction==EGPD_Input&&P->PinName==Source.Type->GetFName())
                {Expected.PinCategory=UEdGraphSchema_K2::PC_Struct;Expected.PinSubCategoryObject=Source.Type.Get();Expected.bIsReference=true;Expected.bIsConst=true;}
                else
                {
                    const auto* Field=ScalarGraphFields.FindByPredicate([&](const FScalarGraphField& F){return F.Name==P->PinName;});
                    if(!Field)return ScalarBad(N->GetPathName(),P->PinName.ToString(),TEXT("current authored output field"),TEXT("not found"));
                    if(!Field->Guid.IsValid()||AuthoredGuids.Contains(Field->Guid)||P->PersistentGuid!=Field->Guid)
                        return ScalarBad(N->GetPathName(),P->PinName.ToString()+TEXT(".PersistentGuid"),Field->Guid.ToString(),P->PersistentGuid.ToString());
                    AuthoredGuids.Add(Field->Guid);Expected=Field->Type;Direction=EGPD_Output;
                }
            }
            else if(P->PinName==UEdGraphSchema_K2::PN_Execute)Expected.PinCategory=UEdGraphSchema_K2::PC_Exec;
            else if(P->PinName==UEdGraphSchema_K2::PN_Then||P->PinName==TEXT("RowNotFound")){Expected.PinCategory=UEdGraphSchema_K2::PC_Exec;Direction=EGPD_Output;}
            else if(P->PinName==TEXT("DataTable")){Expected.PinCategory=UEdGraphSchema_K2::PC_Object;Expected.PinSubCategoryObject=UDataTable::StaticClass();}
            else if(P->PinName==TEXT("RowName"))Expected.PinCategory=UEdGraphSchema_K2::PC_Name;
            else if(P->PinName==UEdGraphSchema_K2::PN_ReturnValue){Expected.PinCategory=P->LinkedTo.IsEmpty()?UEdGraphSchema_K2::PC_Wildcard:UEdGraphSchema_K2::PC_Struct;if(!P->LinkedTo.IsEmpty())Expected.PinSubCategoryObject=Source.Type.Get();Direction=EGPD_Output;}
            else return Bad(TEXT("scalar_row_stock_pin_name"));
            if(P->Direction!=Direction)return ScalarBad(N->GetPathName(),P->PinName.ToString()+TEXT(".direction"),Direction==EGPD_Input?TEXT("input"):TEXT("output"),P->Direction==EGPD_Input?TEXT("input"):TEXT("output"));
            if(!RowTypeSame(P->PinType,Expected))return ScalarBad(N->GetPathName(),P->PinName.ToString()+TEXT(".full_type"),ScalarTypeSummary(Expected),ScalarTypeSummary(P->PinType));
            // The SDK marks later pins Advanced when Pins.Num()>5. Capture that
            // actual physical state; it is neither a hidden nor a missing field.
        }
        if(Break&&AuthoredGuids.Num()!=ScalarGraphFields.Num())return Bad(TEXT("scalar_all_authored_output_guids"));
        return Raw();
    }
    bool ScalarFieldsJson(UK2Node_BreakStruct* Break,TArray<TSharedPtr<FJsonValue>>& Fields)
    {
        if(!Source.ScalarProfile||!Raw())return false;
        if(Break&&!ScalarShape(Break,true,false))return false;
        for(const auto& F:ScalarGraphFields)
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("property_name"),F.Name.ToString());J->SetStringField(TEXT("authored_name"),F.Authored);J->SetStringField(TEXT("field_guid"),F.Guid.ToString());J->SetStringField(TEXT("actual_output_pin_name"),F.Name.ToString());
            auto Expected=MakeShared<FJsonObject>();if(!FullType(F.Type,Expected))return false;J->SetObjectField(TEXT("expected_pin_type"),Expected);
            if(Break)
            {
                if(!Raw()||!Loaded(Break)||!Graph->Nodes.Contains(Break))return Bad(TEXT("scalar_receipt_current_break"));
                auto* P=Break->FindPin(F.Name,EGPD_Output);
                if(!Raw()||!P||!Break->Pins.Contains(P)||P->WasTrashed()||P->GetOwningNodeUnchecked()!=Break)return Bad(TEXT("scalar_receipt_current_output_pin"));
                if(P->PersistentGuid!=F.Guid)return ScalarBad(Break->GetPathName(),F.Name.ToString()+TEXT(".PersistentGuid"),F.Guid.ToString(),P->PersistentGuid.ToString());
                const auto Captured=CopyRowPin(P);auto Output=MakeShared<FJsonObject>();if(!PinJson(Captured,Output))return false;J->SetObjectField(TEXT("output_pin"),Output);
            }
            Fields.Add(MakeShared<FJsonValueObject>(J));
        }
        return Raw();
    }
    bool ScalarReceipt(TSharedRef<FJsonObject>& J,UK2Node_BreakStruct* Break)
    {
        TArray<TSharedPtr<FJsonValue>> Fields;if(!ScalarFieldsJson(Break,Fields))return false;
        J->SetBoolField(TEXT("scalar_uds_graph_profile"),true);J->SetNumberField(TEXT("scalar_uds_field_count"),Fields.Num());J->SetStringField(TEXT("scalar_default_values_json"),Source.DefaultValue);J->SetArrayField(TEXT("scalar_fields"),Fields);return Raw();
    }
    bool ClassRoles() const
    {
        auto* B=Blueprint.Get();return Loaded(B)&&B->ParentClass.GetGCPtr().IsResolved()&&B->ParentClass.GetGCPtr()==Parent.Get()&&B->GeneratedClass.GetGCPtr().IsResolved()&&B->GeneratedClass.GetGCPtr()==Generated.Get()&&B->SkeletonGeneratedClass.GetGCPtr().IsResolved()&&B->SkeletonGeneratedClass.GetGCPtr()==Skeleton.Get();
    }
    bool DescriptionSame(const FBPVariableDescription& A,const FBPVariableDescription& B) const
    {
        if(A.VarName!=B.VarName||A.VarGuid!=B.VarGuid||!RowTypeSame(A.VarType,B.VarType)||A.PropertyFlags!=B.PropertyFlags||!A.FriendlyName.Equals(B.FriendlyName,ESearchCase::CaseSensitive)||!A.DefaultValue.Equals(B.DefaultValue,ESearchCase::CaseSensitive)||!A.Category.IdenticalTo(B.Category)||A.RepNotifyFunc!=B.RepNotifyFunc||A.ReplicationCondition!=B.ReplicationCondition||A.MetaDataArray.Num()!=B.MetaDataArray.Num())return false;
        for(int32 I=0;I<A.MetaDataArray.Num();++I)if(A.MetaDataArray[I].DataKey!=B.MetaDataArray[I].DataKey||!A.MetaDataArray[I].DataValue.Equals(B.MetaDataArray[I].DataValue,ESearchCase::CaseSensitive))return false;return true;
    }
    bool NodeSame(const FNode& V)
    {
        auto* N=V.N;if(!Loaded(N)||!Loaded(V.Class)||N->GetOuter()!=Graph.Get()||N->GetClass()!=V.Class||!N->GetPathName().Equals(V.Path,ESearchCase::CaseSensitive)||N->NodeGuid!=V.Guid||N->NodePosX!=V.X||N->NodePosY!=V.Y||N->GetFlags()!=V.Flags||N->Pins!=V.Addresses)return false;
        if(V.Class==UK2Node_BreakStruct::StaticClass()&&static_cast<UK2Node_BreakStruct*>(N)->StructType!=V.BreakType)return false;
        if(V.HasBinding)
        {const auto& Now=V.IsVariable?static_cast<UK2Node_Variable*>(N)->VariableReference:V.IsTerminator?static_cast<UK2Node_FunctionTerminator*>(N)->FunctionReference:static_cast<UK2Node_CallFunction*>(N)->FunctionReference;if(!Now.IsSameReference(V.Binding))return false;}
        if(V.IsTerminator)
        {const auto& Infos=static_cast<UK2Node_EditablePinBase*>(N)->UserDefinedPins;if(Infos.Num()!=V.UserPins.Num())return false;for(int32 I=0;I<Infos.Num();++I){const auto& A=Infos[I];const auto& B=V.UserPins[I];if(!A||A->PinName!=B.PinName||A->DesiredPinDirection!=B.DesiredPinDirection||!RowTypeSame(A->PinType,B.PinType)||!A->PinDefaultValue.Equals(B.PinDefaultValue,ESearchCase::CaseSensitive))return false;}}
        for(int32 I=0;I<V.Pins.Num();++I)if(!RowPinSame(V.Pins[I],N->Pins[I],N)||(Source.ScalarProfile&&N->Pins[I]->PersistentGuid!=V.Pins[I].Persistent))return false;return true;
    }
    bool Raw()
    {
        if(!Source.Raw())return false;for(const auto& V:NativeSources){auto* C=V.Class.Get();if(!Loaded(C)||!Loaded(V.Package.Get())||C->GetPackage()!=V.Package.Get()||!C->GetPathName().Equals(V.Path,ESearchCase::CaseSensitive)||C->GetClassFlags()!=V.Flags||C->GetPropertiesSize()!=V.Size||C->ClassGeneratedBy!=nullptr||uint32(V.Package->GetPackageFlags())!=V.PackageFlags||V.Package->IsDirty())return Bad(TEXT("native_class_source"));}if(!Blueprint.IsValid())return true;auto* B=Blueprint.Get();
        if(!ClassRoles()||!Loaded(Package)||B->GetPackage()!=Package||B->GetOuter()!=Package||!B->GetPathName().Equals(BlueprintPath,ESearchCase::CaseSensitive)||B->BlueprintType!=BPTYPE_FunctionLibrary||Parent.Get()!=UBlueprintFunctionLibrary::StaticClass()||!B->SimpleConstructionScript.IsResolved()||B->SimpleConstructionScript!=nullptr||!B->NewVariables.IsEmpty()||B->FunctionGraphs!=Functions||B->UbergraphPages!=Events||B->DelegateSignatureGraphs!=Delegates||B->MacroGraphs!=Macros||uint32(Package->GetPackageFlags())!=PackageFlags||(!AllowDirty&&Package->IsDirty()!=Dirty)||B->Status!=Status)return Bad(TEXT("blueprint/current_classes/descriptors"));
        if(!Graph.IsValid())return true;auto* G=Graph.Get();
        if(!Loaded(G)||!G->bEditable||G->GetOuter()!=B||G->GetFName()!=TEXT("PresetLoader")||!G->GetPathName().Equals(GraphPath,ESearchCase::CaseSensitive)||G->GraphGuid!=GraphGuid||!Functions.Contains(G)||!G->Schema.GetGCPtr().IsResolved()||G->Schema.GetGCPtr()!=UEdGraphSchema_K2::StaticClass()||!Schema.IsValid()||Schema->GetClass()!=UEdGraphSchema_K2::StaticClass()||(!UEdGraphSchema_K2::StaticClass()->ClassDefaultObject.IsResolved()||UEdGraphSchema_K2::StaticClass()->ClassDefaultObject!=Schema.Get()))return Bad(TEXT("graph/schema/current_cdo"));
        if(Added&&(!Loaded(Added)||Added->GetClass()!=AddedClass||Added->GetOuter()!=G||!Added->GetPathName().Equals(AddedPath,ESearchCase::CaseSensitive)||(GuidReady&&Added->NodeGuid!=AddedGuid)||(AddedIsBreak&&static_cast<UK2Node_BreakStruct*>(Added)->StructType!=Source.Type.Get())))return Bad(TEXT("allocated_node_current_role"));
        if(!Ready)return true;
        if(G->Nodes.Num()!=Handles.Num()+(Attached?1:0))return Bad(TEXT("node_inventory/count"));for(int32 I=0;I<Handles.Num();++I)if(G->Nodes[I]!=Handles[I])return Bad(TEXT("node_inventory/order"));if(Attached&&G->Nodes.Last()!=Added)return Bad(TEXT("selected_node_membership"));
        for(const auto& V:Nodes)if(!NodeSame(V))return Bad(TEXT("prior_node/full_pin/reference"));if(FreezeAdded&&!NodeSame(AddedSnapshot))return Bad(TEXT("selected_node/full_pin"));
        if(!Loaded(Entry)||!Entry->bIsEditable||Entry->GetExtraFlags()!=EntryFlags||!Entry->FunctionReference.IsSameReference(EntryReference)||Entry->LocalVariables.Num()!=Locals.Num())return Bad(TEXT("entry/reference/static_flags/locals"));for(int32 I=0;I<Locals.Num();++I)if(!DescriptionSame(Entry->LocalVariables[I],Locals[I]))return Bad(TEXT("entry/local_descriptor"));return true;
    }
    bool Stable(bool ReadDisk=false)
    {if(!Raw()||!Source.StableSource(true,ReadDisk))return false;return Raw();}
    bool Keep(UObject* O)
    {if(!O||!Loaded(O))return Bad(TEXT("loaded_return"));Holds.Emplace(O);return true;}
    bool CopyNode(UEdGraphNode* N,FNode& V)
    {
        if(!Raw()||!Loaded(N)||N->GetOuter()!=Graph.Get()||N->Pins.Num()>512)return Bad(TEXT("node_capture"));
        if(!Keep(N))return false;V.N=N;V.Class=N->GetClass();if(!Loaded(V.Class)||!Keep(V.Class))return Bad(TEXT("node_class_hold"));V.Path=N->GetPathName();V.Guid=N->NodeGuid;V.X=N->NodePosX;V.Y=N->NodePosY;V.Flags=N->GetFlags();if(V.Class==UK2Node_BreakStruct::StaticClass())V.BreakType=static_cast<UK2Node_BreakStruct*>(N)->StructType;
        // Classify through the no-track native super iterator before freezing physical pins.
        for(auto C=N->GetClass()->GetSuperStructIterator();C;++C)
        {if(*C==UK2Node_Variable::StaticClass())V.IsVariable=true;if(*C==UK2Node_FunctionTerminator::StaticClass())V.IsTerminator=true;}
        V.HasBinding=V.IsVariable||V.IsTerminator||N->GetClass()==UK2Node_CallFunction::StaticClass();
        if(V.HasBinding)V.Binding=V.IsVariable?static_cast<UK2Node_Variable*>(N)->VariableReference:V.IsTerminator?static_cast<UK2Node_FunctionTerminator*>(N)->FunctionReference:static_cast<UK2Node_CallFunction*>(N)->FunctionReference;
        if(V.IsTerminator)for(const auto& Info:static_cast<UK2Node_EditablePinBase*>(N)->UserDefinedPins){if(!Info)return Bad(TEXT("user_descriptor"));V.UserPins.Add(*Info);}
        V.Addresses=N->Pins;TSet<FGuid> Seen;
        for(auto* P:V.Addresses)
        {if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||Seen.Contains(P->PinId))return Bad(TEXT("whole_pin_capture"));Seen.Add(P->PinId);V.Pins.Add(CopyRowPin(P));}
        return Raw();
    }
    bool ProfileType(FName Name,const FEdGraphPinType& T,bool Output)
    {
        FEdGraphPinType E;
        if(!Output)
        {if(Name==TEXT("PresetName"))E.PinCategory=UEdGraphSchema_K2::PC_String;else if(Name==TEXT("AssetPath")){E.PinCategory=UEdGraphSchema_K2::PC_Name;E.ContainerType=EPinContainerType::Array;}else if(Name==TEXT("AssetTrimmedName"))E.PinCategory=UEdGraphSchema_K2::PC_Name;else if(Name==TEXT("ForcePreferredPreset"))E.PinCategory=UEdGraphSchema_K2::PC_Boolean;else if(Name==TEXT("PreferredPreset")){E.PinCategory=UEdGraphSchema_K2::PC_Object;E.PinSubCategoryObject=UDataTable::StaticClass();}else return false;}
        else
        {if(Name==TEXT("LoadedDataTable")){E.PinCategory=UEdGraphSchema_K2::PC_Object;E.PinSubCategoryObject=UDataTable::StaticClass();}else if(Name==TEXT("LoadedDataTablePath"))E.PinCategory=UEdGraphSchema_K2::PC_String;else if(Name==TEXT("PresetMap")){E.PinCategory=UEdGraphSchema_K2::PC_String;E.ContainerType=EPinContainerType::Map;E.PinValueType.TerminalCategory=UEdGraphSchema_K2::PC_Real;E.PinValueType.TerminalSubCategory=UEdGraphSchema_K2::PC_Double;}else return false;}
        return RowTypeSame(T,E);
    }
    bool Profile()
    {
        if(!Entry||Entry->UserDefinedPins.Num()!=5)return Bad(TEXT("PresetLoader_input5"));TSet<FName> Names;
        for(const auto& I:Entry->UserDefinedPins){if(!I||Names.Contains(I->PinName)||I->DesiredPinDirection!=EGPD_Output||!ProfileType(I->PinName,I->PinType,false))return Bad(TEXT("PresetLoader_input_descriptor"));Names.Add(I->PinName);auto* P=Entry->FindPin(I->PinName,EGPD_Output);if(!P||P->WasTrashed()||!RowTypeSame(P->PinType,I->PinType)||P->bOrphanedPin||P->bDeprecated)return Bad(TEXT("PresetLoader_input_physical_pin"));}
        int32 ResultCount=0;for(const auto& V:Nodes)if(V.Class==UK2Node_FunctionResult::StaticClass())
        {if(++ResultCount>1)return Bad(TEXT("one_optional_Result"));auto* Term=static_cast<UK2Node_FunctionResult*>(V.N);if(!Term->bIsEditable||!Term->FunctionReference.IsSameReference(EntryReference)||Term->UserDefinedPins.Num()>3)return Bad(TEXT("PresetLoader_Result_binding"));Names.Reset();for(const auto& I:Term->UserDefinedPins){if(!I||Names.Contains(I->PinName)||I->DesiredPinDirection!=EGPD_Input||!ProfileType(I->PinName,I->PinType,true))return Bad(TEXT("PresetLoader_Result_descriptor"));Names.Add(I->PinName);auto* P=Term->FindPin(I->PinName,EGPD_Input);if(!P||P->WasTrashed()||!RowTypeSame(P->PinType,I->PinType)||P->bOrphanedPin||P->bDeprecated)return Bad(TEXT("PresetLoader_Result_physical_pin"));}}
        return true;
    }
    bool Bind(UBlueprint* B,UEdGraph* G)
    {
        for(auto* C:{UK2Node_GetDataTableRow::StaticClass(),UK2Node_BreakStruct::StaticClass(),UEdGraphSchema_K2::StaticClass(),UDataTable::StaticClass()}){if(!Source.Raw()||!Loaded(C)||!C->HasAnyClassFlags(CLASS_Native)||C->ClassGeneratedBy!=nullptr||!Loaded(C->GetPackage())||C->GetPackage()->IsDirty())return Bad(TEXT("native_class_admission"));FNative V;V.Class.Reset(C);V.Package.Reset(C->GetPackage());V.Path=C->GetPathName();V.PackagePath=V.Package->GetName();V.Flags=C->GetClassFlags();V.Size=C->GetPropertiesSize();V.PackageFlags=uint32(V.Package->GetPackageFlags());NativeSources.Add(MoveTemp(V));}
        if(!Source.Raw()||!Loaded(B)||B->bBeingCompiled||B->Status==BS_BeingCreated||B->BlueprintType!=BPTYPE_FunctionLibrary||!B->NewVariables.IsEmpty()||!B->SimpleConstructionScript.IsResolved()||B->SimpleConstructionScript!=nullptr||!Loaded(B->GetPackage())||!AssetPath(B->GetPathName(),true)||B->GetOuter()!=B->GetPackage())return Bad(TEXT("exact_member_free_bfl"));
        Blueprint.Reset(B);BlueprintPath=B->GetPathName();Package=B->GetPackage();if(!Keep(Package))return false;PackageFlags=uint32(Package->GetPackageFlags());Dirty=Package->IsDirty();Status=B->Status;Functions=B->FunctionGraphs;Events=B->UbergraphPages;Delegates=B->DelegateSignatureGraphs;Macros=B->MacroGraphs;
        auto ParentHandle=B->ParentClass.GetGCPtr();auto GeneratedHandle=B->GeneratedClass.GetGCPtr();auto SkeletonHandle=B->SkeletonGeneratedClass.GetGCPtr();
        const auto Bootstrap=[&](){return Source.Raw()&&Loaded(B)&&B->ParentClass.GetGCPtr()==ParentHandle&&B->GeneratedClass.GetGCPtr()==GeneratedHandle&&B->SkeletonGeneratedClass.GetGCPtr()==SkeletonHandle&&B->FunctionGraphs==Functions;};
        if(!ParentHandle.IsResolved()||!GeneratedHandle.IsResolved()||!SkeletonHandle.IsResolved())return Bad(TEXT("resolved_class_roles"));
        auto* ParentClassPointer=ParentHandle.Get();if(ParentClassPointer&&Loaded(ParentClassPointer))Parent.Reset(ParentClassPointer);if(!Bootstrap()||ParentClassPointer!=UBlueprintFunctionLibrary::StaticClass())return Bad(TEXT("parent_class"));
        auto* Gen=GeneratedHandle.Get();if(Gen&&Loaded(Gen))Generated.Reset(Gen);if(!Bootstrap()||!Generated.IsValid())return Bad(TEXT("generated_class"));
        auto* Skel=SkeletonHandle.Get();if(Skel&&Loaded(Skel))Skeleton.Reset(Skel);if(!Bootstrap()||!Skeleton.IsValid())return Bad(TEXT("skeleton_class"));
        if(!Loaded(G)||!G->bEditable||G->GetOuter()!=B||G->GetFName()!=TEXT("PresetLoader")||!Functions.Contains(G)||G->Nodes.Num()>2048||!G->GraphGuid.IsValid())return Bad(TEXT("own_PresetLoader_function"));
        auto SchemaHandle=G->Schema.GetGCPtr();if(!SchemaHandle.IsResolved()||SchemaHandle!=UEdGraphSchema_K2::StaticClass())return Bad(TEXT("exact_K2_schema_class"));
        auto SchemaCDO=UEdGraphSchema_K2::StaticClass()->ClassDefaultObject;if(!SchemaCDO.IsResolved()||SchemaCDO==nullptr)return Bad(TEXT("existing_resolved_schema_CDO"));
        auto* ReturnedCDO=SchemaCDO.Get();
        if(ReturnedCDO&&Loaded(ReturnedCDO)&&ReturnedCDO->GetClass()==UEdGraphSchema_K2::StaticClass())Schema.Reset(const_cast<UEdGraphSchema_K2*>(static_cast<const UEdGraphSchema_K2*>(ReturnedCDO)));
        if(!Bootstrap()||UEdGraphSchema_K2::StaticClass()->ClassDefaultObject!=SchemaCDO||!Schema.IsValid()||!Loaded(G)||G->Schema.GetGCPtr()!=SchemaHandle||!Functions.Contains(G))return Bad(TEXT("schema_CDO_return"));
        Graph.Reset(G);GraphPath=G->GetPathName();GraphGuid=G->GraphGuid;Handles=G->Nodes;
        for(const auto& Handle:Handles)
        {auto* N=Handle.Get();if(!Raw()||G->Nodes!=Handles||!Loaded(N))return Bad(TEXT("node_handle_read"));FNode V;if(!CopyNode(N,V))return false;if(N->GetClass()==UK2Node_FunctionEntry::StaticClass()){if(Entry)return Bad(TEXT("one_entry"));Entry=static_cast<UK2Node_FunctionEntry*>(N);}Nodes.Add(MoveTemp(V));}
        if(!Entry)return Bad(TEXT("entry_missing"));EntryReference=Entry->FunctionReference;EntryFlags=Entry->GetExtraFlags();Locals=Entry->LocalVariables;
        FMemberReference Expected;Expected.SetExternalMember(G->GetFName(),nullptr,EntryReference.GetMemberGuid());
        if(!Entry->bIsEditable||!EntryReference.IsSameReference(Expected)||EntryReference.IsDeprecated()||(EntryFlags&(FUNC_Static|FUNC_Public|FUNC_BlueprintCallable))!=(FUNC_Static|FUNC_Public|FUNC_BlueprintCallable)||(EntryFlags&(FUNC_Private|FUNC_Protected|FUNC_EditorOnly|FUNC_BlueprintPure|FUNC_Const))||!Entry->CustomGeneratedFunctionName.IsNone())return Bad(TEXT("own_static_entry_binding"));
        auto* WorldPin=Entry->FindPin(TEXT("__WorldContext"),EGPD_Output);FEdGraphPinType WorldType;WorldType.PinCategory=UEdGraphSchema_K2::PC_Object;WorldType.PinSubCategoryObject=UObject::StaticClass();
        if(!WorldPin||WorldPin->WasTrashed()||!RowTypeSame(WorldPin->PinType,WorldType)||!WorldPin->bHidden||WorldPin->bNotConnectable||WorldPin->bOrphanedPin)return Bad(TEXT("automatic_world_context"));
        Ready=true;if(!Raw()||!Profile())return false;
        TSet<const UEdGraphPin*> Frozen;for(const auto& V:Nodes)for(const auto& P:V.Pins)Frozen.Add(P.Address);
        for(const auto& V:Nodes)for(const auto& P:V.Pins)for(auto* Peer:P.Links)if(!Frozen.Contains(Peer)||Peer==P.Address||Peer->WasTrashed()||!Peer->LinkedTo.Contains(P.Address))return Bad(TEXT("prior_link_current_inventory"));
        if(Source.ScalarProfile&&!CaptureScalarGraphTypes())return false;return Stable();
    }
    TSharedRef<FJsonObject> NativeJson()
    {
        auto J=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;
        for(const auto& V:NativeSources){auto Item=MakeShared<FJsonObject>();Item->SetStringField(TEXT("class_path"),V.Path);Item->SetStringField(TEXT("class_flags_string"),LexToString(uint32(V.Flags)));Item->SetNumberField(TEXT("properties_size"),V.Size);Item->SetStringField(TEXT("package_path"),V.PackagePath);Item->SetStringField(TEXT("package_flags_string"),LexToString(V.PackageFlags));Rows.Add(MakeShared<FJsonValueObject>(Item));}J->SetArrayField(TEXT("classes"),Rows);J->SetBoolField(TEXT("exact_native_class_current_verified"),true);J->SetBoolField(TEXT("schema_cdo_identity_only"),true);J->SetBoolField(TEXT("runtime_function_called"),false);return J;
    }
    bool FullType(const FEdGraphPinType& T,TSharedRef<FJsonObject>& J)
    {
        J->SetStringField(TEXT("category"),T.PinCategory.ToString());J->SetStringField(TEXT("subcategory"),T.PinSubCategory.ToString());
        auto* O=T.PinSubCategoryObject.Get();if(O&&!Loaded(O))return Bad(TEXT("pin_type_loaded_object"));if(O&&!Keep(O))return false;if(!Raw())return false;const FString Path=O?O->GetPathName():FString();J->SetStringField(TEXT("type_object"),Path);
        J->SetNumberField(TEXT("container_type"),int32(T.ContainerType));J->SetBoolField(TEXT("is_reference"),T.bIsReference);J->SetBoolField(TEXT("is_const"),T.bIsConst);J->SetBoolField(TEXT("is_weak_pointer"),T.bIsWeakPointer);J->SetBoolField(TEXT("is_uobject_wrapper"),T.bIsUObjectWrapper);J->SetBoolField(TEXT("serialize_single_precision"),T.bSerializeAsSinglePrecisionFloat);
        auto Terminal=MakeShared<FJsonObject>();auto* TO=T.PinValueType.TerminalSubCategoryObject.Get();if(TO&&!Loaded(TO))return Bad(TEXT("terminal_loaded_object"));if(TO&&!Keep(TO))return false;if(!Raw())return false;Terminal->SetStringField(TEXT("category"),T.PinValueType.TerminalCategory.ToString());Terminal->SetStringField(TEXT("subcategory"),T.PinValueType.TerminalSubCategory.ToString());Terminal->SetStringField(TEXT("type_object"),TO?TO->GetPathName():FString());Terminal->SetBoolField(TEXT("is_const"),T.PinValueType.bTerminalIsConst);Terminal->SetBoolField(TEXT("is_weak_pointer"),T.PinValueType.bTerminalIsWeakPointer);Terminal->SetBoolField(TEXT("is_uobject_wrapper"),T.PinValueType.bTerminalIsUObjectWrapper);J->SetObjectField(TEXT("terminal"),Terminal);return Raw();
    }
    bool PinJson(const FRowPin& V,TSharedRef<FJsonObject>& J)
    {
        if(!Raw())return false;if(Source.ScalarProfile)J->SetStringField(TEXT("persistent_guid"),V.Persistent.ToString(EGuidFormats::Digits));J->SetStringField(TEXT("name"),V.Name.ToString());J->SetStringField(TEXT("direction"),V.Direction==EGPD_Input?TEXT("input"):TEXT("output"));J->SetStringField(TEXT("pin_id"),V.Id.ToString(EGuidFormats::Digits));J->SetNumberField(TEXT("flags"),V.Flags);J->SetStringField(TEXT("raw_default_value"),V.Default);J->SetStringField(TEXT("autogenerated_default_value"),V.Auto);J->SetBoolField(TEXT("default_text_empty"),V.Text.IsEmpty());
        auto* Object=V.Object.Get();if(Object&&!Loaded(Object))return Bad(TEXT("default_loaded_object"));if(Object&&!Keep(Object))return false;if(!Raw())return false;J->SetStringField(TEXT("default_object_path"),Object?Object->GetPathName():FString());
        auto T=MakeShared<FJsonObject>();if(!FullType(V.Type,T))return false;J->SetObjectField(TEXT("pin_type"),T);
        FString TypeText;FEdGraphPinType::StaticStruct()->ExportText(TypeText,&V.Type,&V.Type,nullptr,PPF_None,nullptr);if(!Raw())return false;J->SetStringField(TEXT("pin_type_text"),TypeText);
        const FString Display=UEdGraphSchema_K2::TypeToText(V.Type).ToString();if(!Raw())return false;J->SetStringField(TEXT("pin_type_display"),Display);J->SetStringField(TEXT("projected_category"),V.Type.PinCategory==UEdGraphSchema_K2::PC_Exec?TEXT("exec"):(V.Type.PinCategory==UEdGraphSchema_K2::PC_Name||V.Type.PinCategory==UEdGraphSchema_K2::PC_String||V.Type.PinCategory==UEdGraphSchema_K2::PC_Object)?TEXT("string"):V.Type.PinCategory==UEdGraphSchema_K2::PC_Wildcard?TEXT("integer"):TEXT("object"));if(Source.ScalarProfile){if(V.Type.PinCategory==UEdGraphSchema_K2::PC_Boolean)J->SetStringField(TEXT("projected_category"),TEXT("boolean"));else if(V.Type.PinCategory==UEdGraphSchema_K2::PC_Int||V.Type.PinCategory==UEdGraphSchema_K2::PC_Int64)J->SetStringField(TEXT("projected_category"),TEXT("integer"));else if(V.Type.PinCategory==UEdGraphSchema_K2::PC_Real)J->SetStringField(TEXT("projected_category"),TEXT("number"));}return true;
    }
    bool NodeJson(const FNode& V,TSharedRef<FJsonObject>& J)
    {
        if(!Raw())return false;J->SetStringField(TEXT("object_path"),V.Path);J->SetStringField(TEXT("node_id"),V.Path);J->SetStringField(TEXT("node_guid"),V.Guid.ToString(EGuidFormats::Digits));J->SetStringField(TEXT("node_class"),V.Class->GetPathName());TArray<TSharedPtr<FJsonValue>> Pins;
        for(const auto& P:V.Pins){auto Item=MakeShared<FJsonObject>();if(!PinJson(P,Item))return false;Pins.Add(MakeShared<FJsonValueObject>(Item));}J->SetArrayField(TEXT("pins"),Pins);return Raw();
    }
    bool Snapshot(TSharedRef<FJsonObject>& J)
    {
        if(!Raw())return false;J->SetStringField(TEXT("graph_path"),GraphPath);J->SetStringField(TEXT("graph_guid"),GraphGuid.ToString(EGuidFormats::Digits));J->SetBoolField(TEXT("dirty"),Package->IsDirty());TArray<TSharedPtr<FJsonValue>> Rows;
        for(const auto& V:Nodes){auto Item=MakeShared<FJsonObject>();if(!NodeJson(V,Item))return false;Rows.Add(MakeShared<FJsonValueObject>(Item));}if(FreezeAdded){auto Item=MakeShared<FJsonObject>();if(!NodeJson(AddedSnapshot,Item))return false;Rows.Add(MakeShared<FJsonValueObject>(Item));}J->SetArrayField(TEXT("nodes"),Rows);return Raw();
    }
};
bool GraphNodeRunning=false;
bool RowSource(FRowGraph& S,const FString& Requested)
{
    if(!S.Source.Start())return false;const FString Path=Normalize(Requested);if(!AssetPath(Path,true))return S.Bad(TEXT("row_struct_path"));
    auto* O=StaticFindObject(UObject::StaticClass(),nullptr,*Path);if(O&&Loaded(O))S.Keep(O);if(!S.Source.Raw()||!Loaded(O)||O->GetClass()!=UUserDefinedStruct::StaticClass()||!O->GetPathName().Equals(Path,ESearchCase::CaseSensitive))return S.Bad(TEXT("loaded_String1_UDS"));
    if(!S.Source.BindType(static_cast<UUserDefinedStruct*>(O)))return false;if(S.Source.ScalarProfile){for(const auto& F:S.Source.ScalarFields)if(!F.Property->HasAnyPropertyFlags(CPF_BlueprintVisible))return S.ScalarBad(S.Source.TypePath,F.Name+TEXT(".BlueprintVisible"),TEXT("true"),TEXT("false"));}else if(!S.Source.Property->HasAnyPropertyFlags(CPF_BlueprintVisible)||!S.Source.DefaultValue.IsEmpty())return S.Bad(TEXT("visible_SourceString_empty_default"));S.R.Data->SetObjectField(TEXT("row_struct_source_before"),S.Source.SourceBaseline);return true;
}
bool RowShape(FRowGraph& S,UK2Node* N,bool Break,bool Initial)
{
    if(S.Source.ScalarProfile)return S.ScalarShape(N,Break,Initial);
    if(!S.Raw()||!Loaded(N)||N->GetOuter()!=S.Graph.Get()||!N->NodeGuid.IsValid()||N->GetClass()!=(Break?UK2Node_BreakStruct::StaticClass():UK2Node_GetDataTableRow::StaticClass()))return S.Bad(TEXT("selected_stock_node"));
    if(Break&&static_cast<UK2Node_BreakStruct*>(N)->StructType!=S.Source.Type.Get())return S.Bad(TEXT("break_StructType"));
    if(N->Pins.Num()!=(Break?2:6))return S.Bad(TEXT("stock_pin_count"));TSet<FName> Seen;
    for(auto* P:N->Pins)
    {
        if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||Seen.Contains(P->PinName)||P->ParentPin||!P->SubPins.IsEmpty()||P->bOrphanedPin||P->bDeprecated||P->bHidden||P->bNotConnectable||P->DefaultObject!=nullptr||!P->DefaultTextValue.IsEmpty()||(Initial&&!P->LinkedTo.IsEmpty()))return S.Bad(TEXT("selected_whole_pin"));Seen.Add(P->PinName);
        FEdGraphPinType Expected;EEdGraphPinDirection Direction=EGPD_Input;
        if(Break)
        {if(P->PinName==S.Source.Type->GetFName()){Expected.PinCategory=UEdGraphSchema_K2::PC_Struct;Expected.PinSubCategoryObject=S.Source.Type.Get();Expected.bIsReference=true;Expected.bIsConst=true;}else if(P->PinName==FName(*S.Source.InternalName)){Expected.PinCategory=UEdGraphSchema_K2::PC_String;Direction=EGPD_Output;}else return S.Bad(TEXT("break_actual_property_pin"));}
        else if(P->PinName==UEdGraphSchema_K2::PN_Execute)Expected.PinCategory=UEdGraphSchema_K2::PC_Exec;
        else if(P->PinName==UEdGraphSchema_K2::PN_Then||P->PinName==TEXT("RowNotFound")){Expected.PinCategory=UEdGraphSchema_K2::PC_Exec;Direction=EGPD_Output;}
        else if(P->PinName==TEXT("DataTable")){Expected.PinCategory=UEdGraphSchema_K2::PC_Object;Expected.PinSubCategoryObject=UDataTable::StaticClass();}
        else if(P->PinName==TEXT("RowName"))Expected.PinCategory=UEdGraphSchema_K2::PC_Name;
        else if(P->PinName==UEdGraphSchema_K2::PN_ReturnValue){Expected.PinCategory=P->LinkedTo.IsEmpty()?UEdGraphSchema_K2::PC_Wildcard:UEdGraphSchema_K2::PC_Struct;if(!P->LinkedTo.IsEmpty())Expected.PinSubCategoryObject=S.Source.Type.Get();Direction=EGPD_Output;}
        else return S.Bad(TEXT("row_stock_pin_name"));
        if(P->Direction!=Direction||!RowTypeSame(P->PinType,Expected))return S.Bad(TEXT("stock_pin_full_type"));
    }
    return S.Raw();
}
FString AddRowGraphNode(const FString& BlueprintPath,const FString& StructPath,const FString& Position,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool Create,bool Break)
{
    FResult R;Init(R,Break?TEXT("add_blueprint_break_struct_node"):TEXT("add_blueprint_get_data_table_row_node"));R.Data->SetStringField(TEXT("admission_scope"),TEXT("exact own member-free PresetLoader BFL; saved-clean SourceString FString1 UDS; stock graph nodes only"));R.Data->SetBoolField(TEXT("runtime_row_lookup_verified"),false);R.Data->SetBoolField(TEXT("specialization_pending"),!Break);R.Data->SetBoolField(TEXT("pendingTyped"),!Break);R.Data->SetBoolField(TEXT("producer_connection_verified"),false);R.Data->SetStringField(TEXT("dependency_tier"),TEXT("stock BlueprintGraph Row/Break nodes and public K2 schema; no gameplay plugin function"));R.Data->SetStringField(TEXT("unselected_preservation_scope"),TEXT("Prior target-graph nodes/full pins/links/Entry descriptors and exact KP UDS source; unrelated object values are not sampled."));R.Data->SetStringField(TEXT("change_accounting_scope"),TEXT("One stock graph node appended; allocation/Modify/AddNode/PostPlaced/Allocate/MarkBlueprintAsModified actual return prefix is retained."));R.Data->SetStringField(TEXT("effect_scope"),TEXT("Editor transactions, stock node allocation and Blueprint callbacks may do implicit work; no explicit compile/save/runtime row lookup is dispatched."));
    TArray<TSharedPtr<FJsonValue>> XY;if(Create||(!GraphType.IsEmpty()&&!GraphType.Equals(TEXT("function"),ESearchCase::IgnoreCase))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Position),XY)||XY.Num()!=2||XY[0]->Type!=EJson::Number||XY[1]->Type!=EJson::Number||!FMath::IsFinite(XY[0]->AsNumber())||!FMath::IsFinite(XY[1]->AsNumber())||FMath::Abs(XY[0]->AsNumber())>10000000||FMath::Abs(XY[1]->AsNumber())>10000000||GraphNodeRunning||StringUdsWriterRunning){Fail(R,TEXT("datatable_graph_arguments_invalid"),TEXT("Existing function only, bounded position and no reentrant writer required."));return Encode(R);}
    TGuardValue<bool> Running(GraphNodeRunning,true);FRowGraph S(R);if(!RowSource(S,StructPath))return Encode(R);if(S.Source.ScalarProfile)R.Data->SetStringField(TEXT("admission_scope"),TEXT("exact own member-free PresetLoader BFL; saved-clean bounded scalar UDS; all authored GUID-backed outputs and stock graph nodes"));
    const FString BPPath=Normalize(BlueprintPath);if(!AssetPath(BPPath,true)){Fail(R,TEXT("datatable_graph_blueprint_path_invalid"),TEXT("Exact bounded runtime-safe Blueprint object path required before lookup."));return Encode(R);}auto* O=StaticFindObject(UObject::StaticClass(),nullptr,*BPPath);if(O&&Loaded(O))S.Keep(O);if(!S.Source.Raw()||!Loaded(O)||O->GetClass()!=UBlueprint::StaticClass()||!O->GetPathName().Equals(BPPath,ESearchCase::CaseSensitive)){S.Bad(TEXT("loaded_blueprint"));return Encode(R);}auto* B=static_cast<UBlueprint*>(O);
    UEdGraph* G=nullptr;const auto GraphHandles=B->FunctionGraphs;for(const auto& H:GraphHandles){auto* Candidate=H.Get();if(!S.Source.Raw()||!Loaded(B)||B->FunctionGraphs!=GraphHandles||!Loaded(Candidate)){S.Bad(TEXT("function_graph_lookup"));return Encode(R);}if(Candidate->GetFName()==TEXT("PresetLoader")){if(G){S.Bad(TEXT("ambiguous_graph"));return Encode(R);}G=Candidate;S.Keep(G);}}
    FGuid Parsed;if(!G||(!GraphName.IsEmpty()&&FName(*GraphName)!=G->GetFName())||(!GraphId.IsEmpty()&&(!FGuid::Parse(GraphId,Parsed)||Parsed!=G->GraphGuid))){S.Bad(TEXT("selected_graph_identity"));return Encode(R);}if(!S.Bind(B,G))return Encode(R);
    auto Before=MakeShared<FJsonObject>();if(!S.Snapshot(Before))return Encode(R);R.Data->SetObjectField(TEXT("before"),Before);R.Data->SetStringField(TEXT("blueprint_path"),S.BlueprintPath);R.Data->SetStringField(TEXT("graph_path"),S.GraphPath);R.Data->SetStringField(TEXT("row_struct_path"),S.Source.TypePath);R.Data->SetStringField(TEXT("struct_guid"),S.Source.StructGuid.ToString());if(!S.Source.ScalarProfile){R.Data->SetStringField(TEXT("field_guid"),S.Source.FieldGuid.ToString());R.Data->SetStringField(TEXT("property_name"),S.Source.InternalName);R.Data->SetStringField(TEXT("actual_output_pin_name"),S.Source.InternalName);}R.Data->SetStringField(TEXT("actual_input_pin_name"),Break?S.Source.Type->GetFName().ToString():TEXT("DataTable"));R.Data->SetStringField(TEXT("row_result_pin_name"),TEXT("ReturnValue"));R.Data->SetStringField(TEXT("row_row_name_pin_name"),TEXT("RowName"));
    if(!S.Stable())return Encode(R);R.Attempted=true;R.Known=false;R.Stage=TEXT("transaction");Count(R,TEXT("outer_transaction_calls"));R.Data->SetBoolField(TEXT("transaction_attempted"),true);
    const FScopedTransaction Transaction(NSLOCTEXT("CubelessDataTable","GraphNode","Add stock DataTable graph node"));R.Data->SetBoolField(TEXT("transaction_returned"),true);S.Source.TrackUndo=true;S.Source.ExpectedUndo=GUndo;if(!S.Raw())return Encode(R);
    S.AllowDirty=true;R.Stage=TEXT("modify_blueprint");R.Data->SetBoolField(TEXT("blueprint_modify_attempted"),true);Count(R,TEXT("explicit_modify_calls"));const bool BPModified=B->Modify();R.Data->SetBoolField(TEXT("blueprint_modify_returned"),BPModified);if(!S.Raw())return Encode(R);R.Stage=TEXT("modify_graph");Count(R,TEXT("explicit_modify_calls"));const bool GraphModified=G->Modify();R.Data->SetBoolField(TEXT("graph_modify_returned"),GraphModified);if(!S.Raw())return Encode(R);
    R.Stage=TEXT("allocate_node");R.Data->SetBoolField(TEXT("node_allocation_attempted"),true);UK2Node* Returned=Break?static_cast<UK2Node*>(NewObject<UK2Node_BreakStruct>(G,NAME_None,RF_Transactional)):static_cast<UK2Node*>(NewObject<UK2Node_GetDataTableRow>(G,NAME_None,RF_Transactional));R.Data->SetBoolField(TEXT("node_allocation_returned_nonnull"),Returned!=nullptr);
    // Allocation prefix is independent of broad Editor/world/current roles.
    if(IsInGameThread()&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&!GExitPurge&&!IsEngineExitRequested()&&Loaded(Returned)){S.Keep(Returned);R.Data->SetObjectField(TEXT("returned_node_identity"),Identity(Returned));}else{R.Data->SetBoolField(TEXT("returned_node_identity_unobservable"),true);S.Bad(TEXT("safe_allocation_identity"));return Encode(R);}
    if(!S.Raw())return Encode(R);S.Added=Returned;S.AddedClass=Returned->GetClass();S.AddedPath=Returned->GetPathName();Returned->NodePosX=int32(XY[0]->AsNumber());Returned->NodePosY=int32(XY[1]->AsNumber());
    if(Break){R.Stage=TEXT("set_struct_type");R.Data->SetBoolField(TEXT("struct_type_assignment_attempted"),true);static_cast<UK2Node_BreakStruct*>(Returned)->StructType=S.Source.Type.Get();R.Data->SetBoolField(TEXT("struct_type_assignment_applied"),static_cast<UK2Node_BreakStruct*>(Returned)->StructType==S.Source.Type.Get());S.AddedIsBreak=true;if(!S.Raw())return Encode(R);}
    R.Stage=TEXT("create_node_guid");R.Data->SetBoolField(TEXT("guid_sdk_attempted"),true);Returned->CreateNewGuid();R.Data->SetBoolField(TEXT("guid_sdk_returned"),true);if(!Loaded(Returned)){S.Bad(TEXT("guid_return_loaded_node"));return Encode(R);}S.AddedGuid=Returned->NodeGuid;R.Data->SetStringField(TEXT("created_node_guid"),S.AddedGuid.ToString(EGuidFormats::Digits));S.GuidReady=true;if(!S.Raw())return Encode(R);
    R.Stage=TEXT("add_node");R.Data->SetBoolField(TEXT("add_node_sdk_attempted"),true);G->AddNode(Returned,false,false);R.Data->SetBoolField(TEXT("add_node_sdk_returned"),true);S.Attached=true;if(!S.Raw())return Encode(R);
    R.Stage=TEXT("post_placed");R.Data->SetBoolField(TEXT("post_placed_sdk_attempted"),true);static_cast<UEdGraphNode*>(Returned)->PostPlacedNewNode();R.Data->SetBoolField(TEXT("post_placed_sdk_returned"),true);if(!S.Stable())return Encode(R);
    R.Stage=TEXT("allocate_default_pins");R.Data->SetBoolField(TEXT("allocate_pins_sdk_attempted"),true);static_cast<UEdGraphNode*>(Returned)->AllocateDefaultPins();R.Data->SetBoolField(TEXT("allocate_pins_sdk_returned"),true);if(!S.Stable()||!RowShape(S,Returned,Break,true)||!S.CopyNode(Returned,S.AddedSnapshot))return Encode(R);S.FreezeAdded=true;if(!S.Raw())return Encode(R);
    R.Stage=TEXT("mark_modified");R.Data->SetBoolField(TEXT("mark_modified_attempted"),true);FBlueprintEditorUtils::MarkBlueprintAsModified(B);R.Data->SetBoolField(TEXT("mark_modified_returned"),true);S.Status=BS_Dirty;if(!S.Stable(true))return Encode(R);
    auto Node=MakeShared<FJsonObject>(),After=MakeShared<FJsonObject>(),SourceAfter=MakeShared<FJsonObject>();if(!S.NodeJson(S.AddedSnapshot,Node)||!S.Snapshot(After)||(S.Source.ScalarProfile&&!S.ScalarReceipt(R.Data,Break?static_cast<UK2Node_BreakStruct*>(Returned):nullptr))||!S.Source.Source(SourceAfter,true)||!S.Raw())return Encode(R);
    R.Data->SetObjectField(TEXT("node"),Node);R.Data->SetStringField(TEXT("node_id"),S.AddedSnapshot.Path);R.Data->SetStringField(TEXT("object_path"),S.AddedSnapshot.Path);R.Data->SetStringField(TEXT("node_guid"),S.AddedSnapshot.Guid.ToString(EGuidFormats::Digits));R.Data->SetBoolField(TEXT("dirty_before"),S.Dirty);R.Data->SetBoolField(TEXT("dirty_after"),S.Package->IsDirty());R.Data->SetObjectField(TEXT("after"),After);R.Data->SetObjectField(TEXT("row_struct_source_after"),SourceAfter);const bool Preserved=MirrorReadLiteralEqual(S.Source.SourceBaseline,SourceAfter);R.Data->SetBoolField(TEXT("row_struct_source_unchanged"),Preserved);if(!Preserved){S.Bad(TEXT("source_before_after"));return Encode(R);}
    R.Data->SetObjectField(TEXT("native_graph_sources"),S.NativeJson());if(S.Source.ScalarProfile)R.Data->SetBoolField(TEXT("native_scalar_uds_graph_node"),true);else R.Data->SetBoolField(TEXT("native_string1_graph_node"),true);R.Data->SetBoolField(TEXT("observations_complete"),true);R.Data->SetBoolField(TEXT("source_preserved"),true);R.Data->SetBoolField(TEXT("unselected_state_preserved"),true);R.Data->SetBoolField(TEXT("independent_readback_passed"),true);R.Known=true;R.Changed=true;R.Success=true;R.Post=true;R.Stage=TEXT("complete");return Encode(R);
}
}
FString AddGraphRowNode(const FString& B,const FString& U,const FString& P,const FString& N,const FString& I,const FString& T,bool C)
{return AddRowGraphNode(B,U,P,N,I,T,C,false);}
FString AddGraphStringBreakNode(const FString& B,const FString& U,const FString& P,const FString& N,const FString& I,const FString& T,bool C)
{return AddRowGraphNode(B,U,P,N,I,T,C,true);}

TSharedRef<FJsonObject> ReadStringRowConnection(const FString& RequestedGraph,const FString& SourcePath,const FString& SourcePin,const FString& TargetPath,const FString& TargetPin)
{
    auto Result=MakeShared<FJsonObject>();Result->SetBoolField(TEXT("success"),false);Result->SetBoolField(TEXT("mutation_attempted"),false);Result->SetBoolField(TEXT("compiled"),false);Result->SetBoolField(TEXT("saved"),false);Result->SetBoolField(TEXT("loaded_during_read"),false);Result->SetStringField(TEXT("stage"),TEXT("preflight"));Result->SetStringField(TEXT("connection_scope"),TEXT("string1_datatable_row"));auto O=MakeShared<FJsonObject>();Result->SetObjectField(TEXT("row_connection_observation"),O);O->SetBoolField(TEXT("eligible"),false);
    FResult R;Init(R,TEXT("read_string1_row_connection"));FRowGraph S(R);
    const auto Failed=[&](){Result->SetStringField(TEXT("error_code"),R.Code.IsEmpty()?TEXT("graph_string1_row_scope_invalid"):*R.Code);Result->SetStringField(TEXT("error"),R.Error.IsEmpty()?TEXT("Exact current Row to String1 Break role required."):*R.Error);Result->SetObjectField(TEXT("native_observation"),R.Data);return Result;};
    if(!S.Source.Start())return Failed();auto* G=FindObject<UEdGraph>(nullptr,*RequestedGraph);if(G&&Loaded(G))S.Keep(G);if(!S.Source.Raw())return Failed();
    auto* From=FindObject<UEdGraphNode>(nullptr,*SourcePath);if(From&&Loaded(From))S.Keep(From);if(!S.Source.Raw())return Failed();
    auto* To=FindObject<UEdGraphNode>(nullptr,*TargetPath);if(To&&Loaded(To))S.Keep(To);if(!S.Source.Raw())return Failed();
    if(!S.Source.Raw()||!Loaded(G)||!Loaded(From)||!Loaded(To)||From->GetOuter()!=G||To->GetOuter()!=G||!G->Nodes.Contains(From)||!G->Nodes.Contains(To)){S.Bad(TEXT("same_graph_current_nodes"));return Failed();}
    const bool TableInput=To->GetClass()==UK2Node_GetDataTableRow::StaticClass()&&FName(*TargetPin)==TEXT("DataTable");
    UK2Node_GetDataTableRow* Row=TableInput?static_cast<UK2Node_GetDataTableRow*>(To):From->GetClass()==UK2Node_GetDataTableRow::StaticClass()?static_cast<UK2Node_GetDataTableRow*>(From):nullptr;
    UK2Node_BreakStruct* Break=!TableInput&&To->GetClass()==UK2Node_BreakStruct::StaticClass()?static_cast<UK2Node_BreakStruct*>(To):nullptr;
    if(!Row||(!TableInput&&FName(*SourcePin)!=UEdGraphSchema_K2::PN_ReturnValue)){S.Bad(TEXT("exact_row_break_role"));return Failed();}
    if(TableInput)
    {
        // Locate the actual typed downstream node by opaque addresses first. A
        // peer is never dereferenced before membership in a current pin array.
        auto* Output=Row->FindPin(UEdGraphSchema_K2::PN_ReturnValue,EGPD_Output);if(!Output||Output->WasTrashed()||Output->LinkedTo.Num()!=1){S.Bad(TEXT("connect_row_to_break_before_table"));return Failed();}
        auto* Peer=Output->LinkedTo[0];const auto Inventory=G->Nodes;
        for(const auto& Handle:Inventory){auto* N=Handle.Get();if(N&&Loaded(N))S.Keep(N);if(!S.Source.Raw()||!Loaded(G)||G->Nodes!=Inventory||!Loaded(Row)||!G->Nodes.Contains(Row)||Row->FindPin(UEdGraphSchema_K2::PN_ReturnValue,EGPD_Output)!=Output||Output->WasTrashed()||Output->LinkedTo.Num()!=1||Output->LinkedTo[0]!=Peer||!Loaded(N)){S.Bad(TEXT("downstream_node_lookup"));return Failed();}if(N->GetClass()==UK2Node_BreakStruct::StaticClass()&&N->Pins.Contains(Peer)){if(Break){S.Bad(TEXT("one_typed_break"));return Failed();}Break=static_cast<UK2Node_BreakStruct*>(N);}}
    }
    if(!Loaded(Break)||!G->Nodes.Contains(Break)||Break->GetOuter()!=G||!Break->StructType.IsResolved()){S.Bad(TEXT("resolved_break_type"));return Failed();}
    auto* U=Break->StructType.Get();if(U&&Loaded(U))S.Keep(U);if(!S.Source.Raw()||!Loaded(Break)||Break->StructType!=U||!Loaded(U)||U->GetClass()!=UUserDefinedStruct::StaticClass()){S.Bad(TEXT("current_break_type"));return Failed();}
    if(!S.Source.BindType(static_cast<UUserDefinedStruct*>(U)))return Failed();if(S.Source.ScalarProfile){for(const auto& F:S.Source.ScalarFields)if(!F.Property->HasAnyPropertyFlags(CPF_BlueprintVisible)){S.ScalarBad(S.Source.TypePath,F.Name+TEXT(".BlueprintVisible"),TEXT("true"),TEXT("false"));return Failed();}Result->SetStringField(TEXT("connection_scope"),TEXT("scalar_uds_datatable_row"));}else if(!S.Source.Property->HasAnyPropertyFlags(CPF_BlueprintVisible)||!S.Source.DefaultValue.IsEmpty()){S.Bad(TEXT("visible_SourceString_empty_default"));return Failed();}
    auto* B=G->GetTypedOuter<UBlueprint>();if(B&&Loaded(B))S.Keep(B);if(!S.Source.Raw()||!Loaded(B)||!S.Bind(B,G)||(!TableInput&&FName(*TargetPin)!=U->GetFName())||!RowShape(S,Row,false,false)||!RowShape(S,Break,true,false))return Failed();
    auto* RowOutput=Row->FindPin(UEdGraphSchema_K2::PN_ReturnValue,EGPD_Output);auto* BreakInput=Break->FindPin(U->GetFName(),EGPD_Input);auto* Table=Row->FindPin(TEXT("DataTable"),EGPD_Input);auto* Out=From->FindPin(FName(*SourcePin),EGPD_Output);auto* In=To->FindPin(FName(*TargetPin),EGPD_Input);
    if(!RowOutput||!BreakInput||!Table||!Out||!In||Out->WasTrashed()||In->WasTrashed()||Table->DefaultObject!=nullptr||RowOutput->LinkedTo.Num()>1||BreakInput->LinkedTo.Num()>1){S.Bad(TEXT("wire_only_table_and_direct_struct_link"));return Failed();}
    const bool Pending=RowOutput->LinkedTo.IsEmpty();const bool Linked=RowOutput->LinkedTo.Num()==1&&RowOutput->LinkedTo[0]==BreakInput&&BreakInput->LinkedTo.Num()==1&&BreakInput->LinkedTo[0]==RowOutput;
    if((Pending&&!BreakInput->LinkedTo.IsEmpty())||(!Pending&&!Linked)||(TableInput&&!Linked)){S.Bad(TEXT("reciprocal_initial_or_typed_link"));return Failed();}
    if(TableInput)
    {FEdGraphPinType Expected;Expected.PinCategory=UEdGraphSchema_K2::PC_Object;Expected.PinSubCategoryObject=UDataTable::StaticClass();Expected.bIsConst=Out->PinType.bIsConst;if(Out->PinType!=Expected||Out->PinType.bIsUObjectWrapper||Out->PinType.bSerializeAsSinglePrecisionFloat||Out->bOrphanedPin||Out->bDeprecated||Out->bNotConnectable||Out->ParentPin||!Out->SubPins.IsEmpty()||In!=Table||Table->LinkedTo.Num()>1||(Table->LinkedTo.Num()==1&&Table->LinkedTo[0]!=Out)){S.Bad(TEXT("exact_DataTable_scalar_producer"));return Failed();}}
    const auto Decision=S.Schema->CanCreateConnection(Out,In);if(!S.Stable())return Failed();
    // Same public virtual/locale as UBlueprintEditorLibrary::GetNodeTitle.
    const FString RowTitle=static_cast<UEdGraphNode*>(Row)->GetNodeTitle(ENodeTitleType::ListView).ToString();if(!S.Raw())return Failed();
    O->SetStringField(TEXT("connection_role"),TableInput?TEXT("datatable_input"):TEXT("row_result_to_break"));O->SetStringField(TEXT("row_node_title"),RowTitle);O->SetBoolField(TEXT("specialization_pending"),Pending);O->SetBoolField(TEXT("first_connection"),TableInput?Table->LinkedTo.IsEmpty():Pending);O->SetBoolField(TEXT("actual_link_verified"),Out->LinkedTo.Contains(In)&&In->LinkedTo.Contains(Out));O->SetBoolField(TEXT("row_break_link_verified"),Linked);O->SetBoolField(TEXT("native_source_current_verified"),true);O->SetStringField(TEXT("row_node_path"),Row->GetPathName());O->SetStringField(TEXT("row_node_guid"),Row->NodeGuid.ToString(EGuidFormats::Digits));O->SetStringField(TEXT("break_node_path"),Break->GetPathName());O->SetStringField(TEXT("break_node_guid"),Break->NodeGuid.ToString(EGuidFormats::Digits));O->SetStringField(TEXT("source_node_path"),From->GetPathName());O->SetStringField(TEXT("source_node_guid"),From->NodeGuid.ToString(EGuidFormats::Digits));O->SetStringField(TEXT("target_node_path"),To->GetPathName());O->SetStringField(TEXT("target_node_guid"),To->NodeGuid.ToString(EGuidFormats::Digits));O->SetStringField(TEXT("row_struct_path"),S.Source.TypePath);O->SetStringField(TEXT("struct_guid"),S.Source.StructGuid.ToString());if(S.Source.ScalarProfile){if(!S.ScalarReceipt(O,Break))return Failed();}else{O->SetStringField(TEXT("field_guid"),S.Source.FieldGuid.ToString());O->SetStringField(TEXT("property_name"),S.Source.InternalName);O->SetStringField(TEXT("actual_output_pin_name"),S.Source.InternalName);}O->SetStringField(TEXT("actual_input_pin_name"),BreakInput->PinName.ToString());O->SetObjectField(TEXT("row_struct_source"),S.Source.SourceBaseline);O->SetObjectField(TEXT("native_graph_sources"),S.NativeJson());
    for(const auto& V:S.Nodes)
    {TArray<TSharedPtr<FJsonValue>> Pins;for(const auto& P:V.Pins){if(V.N!=Row&&V.N!=Break&&P.Address!=Out&&P.Address!=In)continue;auto J=MakeShared<FJsonObject>();if(!S.PinJson(P,J))return Failed();Pins.Add(MakeShared<FJsonValueObject>(J));if(P.Address==RowOutput)O->SetObjectField(TEXT("row_result_pin"),J);if(P.Address==BreakInput)O->SetObjectField(TEXT("break_input_pin"),J);if(P.Address==Out)O->SetObjectField(TEXT("source_pin"),J);if(P.Address==In)O->SetObjectField(TEXT("target_pin"),J);}if(V.N==Row||V.N==Break)O->SetArrayField(V.N==Row?TEXT("row_pins"):TEXT("break_pins"),Pins);}
    auto OutType=MakeShared<FJsonObject>(),InType=MakeShared<FJsonObject>();if(!S.FullType(Out->PinType,OutType)||!S.FullType(In->PinType,InType))return Failed();Result->SetObjectField(TEXT("source_pin_type"),OutType);Result->SetObjectField(TEXT("target_pin_type"),InType);
    auto D=MakeShared<FJsonObject>();D->SetStringField(TEXT("raw_default_value"),In->DefaultValue);D->SetStringField(TEXT("autogenerated_default_value"),In->AutogeneratedDefaultValue);D->SetStringField(TEXT("default_object_path"),FString());D->SetBoolField(TEXT("default_text_empty"),In->DefaultTextValue.IsEmpty());D->SetBoolField(TEXT("is_orphaned"),In->bOrphanedPin);D->SetBoolField(TEXT("default_value_is_ignored"),In->bDefaultValueIsIgnored);D->SetBoolField(TEXT("default_value_is_read_only"),In->bDefaultValueIsReadOnly);Result->SetObjectField(TEXT("target_pin_default_state"),D);
    Result->SetBoolField(TEXT("direct_connection_allowed"),Decision.Response==CONNECT_RESPONSE_MAKE);Result->SetNumberField(TEXT("schema_response_code"),int32(Decision.Response));Result->SetStringField(TEXT("schema_response_name"),Connection::SchemaResponseName(Decision.Response));Result->SetStringField(TEXT("schema_message"),Decision.Message.ToString());Result->SetBoolField(TEXT("exact_standard_k2_schema"),true);Result->SetBoolField(TEXT("already_linked"),Out->LinkedTo.Contains(In)&&In->LinkedTo.Contains(Out));Result->SetNumberField(TEXT("source_link_count"),Out->LinkedTo.Num());Result->SetNumberField(TEXT("target_link_count"),In->LinkedTo.Num());Result->SetBoolField(TEXT("type_exactly_equal"),Out->PinType==In->PinType);Result->SetStringField(TEXT("graph_path"),S.GraphPath);Result->SetStringField(TEXT("blueprint_path"),S.BlueprintPath);Result->SetBoolField(TEXT("dirty_before"),S.Dirty);
    if(!S.Stable(true))return Failed();Result->SetBoolField(TEXT("dirty_after"),S.Package->IsDirty());O->SetBoolField(TEXT("eligible"),true);O->SetBoolField(TEXT("runtime_row_lookup_verified"),false);Result->SetBoolField(TEXT("success"),true);Result->SetStringField(TEXT("stage"),TEXT("complete"));return Result;
}

}
