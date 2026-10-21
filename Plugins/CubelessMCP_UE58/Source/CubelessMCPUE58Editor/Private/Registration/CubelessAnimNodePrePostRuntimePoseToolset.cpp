#include "Registration/CubelessAnimNodePrePostRuntimePoseToolset.h"
#include "Registration/CubelessOwnedToolsetIdentity.h"
#include "Async/Future.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/CoreMisc.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Templates/Atomic.h"
#include "ToolsetRegistry/Toolset.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "ToolsetRegistry/ToolsetRegistrySubsystem.h"
#include "UObject/FieldIterator.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

namespace Cubeless::Registration
{
namespace
{
using FToolset = UE::ToolsetRegistry::FToolset;
using FOutput = TValueOrError<FString, FString>;
constexpr const TCHAR* CanonicalName = TEXT("cubeless_mcp_ue58.registration.anim_node_pre_post_runtime_pose_toolset.CubelessAnimNodePrePostRuntimePoseTools");
constexpr const TCHAR* CanonicalClass = TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/anim_node_pre_post_runtime_pose_toolset_PY.CubelessAnimNodePrePostRuntimePoseTools");
constexpr const TCHAR* AnimNodePrePostRuntimePoseMethod = TEXT("sample_anim_node_pre_post_runtime_pose");
constexpr int32 MaxInputChars=8*1024*1024,MaxStringChars=1024*1024,MaxCarrierChars=1024*1024;

struct FAttachmentState
{
    explicit FAttachmentState(UClass* InClass) : Class(InClass) {}
    TStrongObjectPtr<UClass> Class;
    TWeakObjectPtr<UToolsetRegistrySubsystem> Subsystem;
    TSharedPtr<FToolset> Original;
    TSharedPtr<FToolset> Wrapper;
    FString ReflectedSchema;
    FString FullSchema;
    FString Version;
    FString Description;
    TAtomic<bool> Active { false };
    TAtomic<int32> InFlight { 0 };
    TAtomic<bool> AnimNodePrePostRuntimePoseDelegation { false };
    bool OriginalDetached = false;
};
// One Editor module instance owns these fields. Wrapper/state cycle is broken by removal.
TSharedPtr<FAttachmentState> Owned;
bool OperationBusy = false;
struct FBusyScope
{
    FBusyScope() { OperationBusy = true; }
    ~FBusyScope() { OperationBusy = false; }
};

FString Json(const TSharedRef<FJsonObject>& Object)
{
    FString Out;
    FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Out));
    return Out;
}

FString Rejection(const FString& Code, const FString& Message, const FString& Method)
{
    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), false);
    // This receipt is not an operation journal and cannot be replayed as one.
    R->SetStringField(TEXT("operation_id"), TEXT(""));
    R->SetStringField(TEXT("stage"), TEXT("anim_node_pre_post_runtime_pose_raw_pre_dispatch"));
    R->SetStringField(TEXT("error_code"), Code);
    R->SetStringField(TEXT("error"), Message);
    R->SetStringField(TEXT("changes_json"), TEXT("[]"));
    auto Compile = MakeShared<FJsonObject>();
    Compile->SetBoolField(TEXT("requested"), false); Compile->SetBoolField(TEXT("attempted"), false);
    Compile->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    Compile->SetStringField(TEXT("status"), TEXT("anim_node_pre_post_runtime_pose_raw_not_dispatched"));
    R->SetStringField(TEXT("compile_json"), Json(Compile));
    auto Save = MakeShared<FJsonObject>();
    Save->SetBoolField(TEXT("requested"), false); Save->SetBoolField(TEXT("attempted"), false); Save->SetBoolField(TEXT("saved"), false);
    R->SetStringField(TEXT("save_json"), Json(Save));
    auto Validation = MakeShared<FJsonObject>();
    Validation->SetStringField(TEXT("status"), TEXT("raw_input_rejected_before_struct_packing"));
    Validation->SetStringField(TEXT("disk_reload"), TEXT("not_run")); Validation->SetStringField(TEXT("packaged_runtime"), TEXT("not_run"));
    R->SetStringField(TEXT("validation_json"), Json(Validation));
    auto Retry = MakeShared<FJsonObject>();
    Retry->SetBoolField(TEXT("automatic_retry"), false); Retry->SetBoolField(TEXT("replayed"), false); Retry->SetBoolField(TEXT("atomic_rollback"), false);
    Retry->SetStringField(TEXT("duplicate_risk"), TEXT("none_observed"));
    R->SetStringField(TEXT("retry_json"), Json(Retry));
    auto Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("operation"), Method);
    Data->SetBoolField(TEXT("native_dispatched"), false); Data->SetBoolField(TEXT("python_dispatched"), false);
    Data->SetBoolField(TEXT("mutation_attempted"), false); Data->SetBoolField(TEXT("mutation_performed"), false);
    Data->SetBoolField(TEXT("mutation_state_unknown"), false); Data->SetBoolField(TEXT("change_state_known"), true); Data->SetBoolField(TEXT("changed"), false);
    Data->SetBoolField(TEXT("journal_created"), false); Data->SetBoolField(TEXT("automatic_action_retry"), false);
    Data->SetNumberField(TEXT("explicit_compile_calls"), 0); Data->SetNumberField(TEXT("explicit_save_calls"), 0);
    Data->SetBoolField(TEXT("unique_external_pipeline_complete"), false); Data->SetBoolField(TEXT("packaged_runtime_verified"), false);
    R->SetStringField(TEXT("data_json"), Json(Data));
    auto Outer = MakeShared<FJsonObject>(); Outer->SetObjectField(TEXT("returnValue"), R);
    return Json(Outer);
}

// Keep Number tokens exact before the official reflected handler converts JSON values.
// Public integer5 remains typed; private decimal FString5 avoids Python int32 reflection.
bool IntegralDecimal(const TSharedPtr<FJsonValue>& Value, FString& Decimal)
{
    FString Token;
    if (!Value.IsValid() || Value->Type != EJson::Number || !Value->TryGetString(Token) || Token.IsEmpty()) { return false; }
    const bool Negative = Token[0] == TEXT('-'); int32 At = Negative ? 1 : 0;
    FString Digits; int32 FractionDigits = 0; bool Fraction = false;
    while (At < Token.Len() && Token[At] != TEXT('e') && Token[At] != TEXT('E'))
    {
        const TCHAR C = Token[At++];
        if (C == TEXT('.') && !Fraction) { Fraction = true; continue; }
        if (C < TEXT('0') || C > TEXT('9')) { return false; }
        Digits.AppendChar(C); if (Fraction) { ++FractionDigits; }
    }
    int32 Exponent = 0;
    if (At < Token.Len())
    {
        ++At; bool ExponentNegative = false;
        if (At < Token.Len() && (Token[At] == TEXT('+') || Token[At] == TEXT('-')))
        { ExponentNegative = Token[At++] == TEXT('-'); }
        if (At == Token.Len()) { return false; }
        // Larger magnitudes cannot make a nonzero bounded input fit int64.
        while (At < Token.Len())
        {
            const TCHAR C = Token[At++]; if (C < TEXT('0') || C > TEXT('9')) { return false; }
            Exponent = FMath::Min(Exponent * 10 + static_cast<int32>(C - TEXT('0')), MaxInputChars + 32);
        }
        if (ExponentNegative) { Exponent = -Exponent; }
    }
    int32 Leading = 0; while (Leading < Digits.Len() && Digits[Leading] == TEXT('0')) { ++Leading; }
    if (Leading == Digits.Len()) { Decimal = TEXT("0"); return true; }
    Digits = Digits.Mid(Leading);
    int32 Shift = Exponent - FractionDigits;
    if (Shift < 0)
    {
        if (-Shift > Digits.Len()) { return false; }
        for (int32 I = Digits.Len() + Shift; I < Digits.Len(); ++I) { if (Digits[I] != TEXT('0')) { return false; } }
        Digits.LeftChopInline(-Shift); Shift = 0;
    }
    if (static_cast<int64>(Digits.Len()) + Shift > 19) { return false; }
    while (Shift-- > 0) { Digits.AppendChar(TEXT('0')); }
    const uint64 Limit = static_cast<uint64>(MAX_int64) + (Negative ? 1u : 0u); uint64 Magnitude = 0;
    for (const TCHAR C : Digits)
    {
        const uint64 Digit = static_cast<uint64>(C - TEXT('0'));
        if (Magnitude > (Limit - Digit) / 10u) { return false; }
        Magnitude = Magnitude * 10u + Digit;
    }
    Decimal = Negative ? TEXT("-") + Digits : Digits; return true;
}

bool RawTransform(const FString& Input,FString& PackedInput,FString& Error)
{
    if(Input.Len()>MaxInputChars){Error=TEXT("AnimGraph node pre/post raw input exceeds 8Mi UTF16");return false;}
    int32 Depth=0;bool Quoted=false,Escaped=false;
    for(TCHAR Ch:Input){if(Ch==0){Error=TEXT("Raw NUL is refused");return false;}if(Quoted){if(Escaped){Escaped=false;}else if(Ch==TCHAR('\\')){Escaped=true;}else if(Ch==TCHAR('"')){Quoted=false;}continue;}if(Ch==TCHAR('"')){Quoted=true;}else if(Ch==TCHAR('[')||Ch==TCHAR('{')){if(++Depth>65){Error=TEXT("Raw JSON exceeds outer wrapper plus carrier depth64");return false;}}else if(Ch==TCHAR(']')||Ch==TCHAR('}')){if(--Depth<0){Error=TEXT("Malformed raw JSON");return false;}}}
    if(Depth||Quoted){Error=TEXT("Malformed raw JSON");return false;}
    TSharedPtr<FJsonObject> Args;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input),Args,FJsonSerializer::EFlags::StoreNumbersAsStrings)||!Args.IsValid()){Error=TEXT("A raw JSON object is required");return false;}
    const TSet<FString> Allowed{TEXT("blueprint_name"),TEXT("anim_blueprint"),TEXT("graph_name"),TEXT("graph_id"),TEXT("graph_type"),TEXT("node_id"),TEXT("node_type"),TEXT("title_contains"),TEXT("sample_bones"),TEXT("sample_sockets"),TEXT("mode"),TEXT("skeletal_mesh"),TEXT("temp_root"),TEXT("cleanup"),TEXT("overwrite_existing_temp_assets"),TEXT("run_id"),TEXT("actor_label"),TEXT("actor_name"),TEXT("actor_path"),TEXT("component_name"),TEXT("anim_instance_source"),TEXT("prefer_pie_world"),TEXT("require_pie_world"),TEXT("tick_count"),TEXT("tick_delta_time"),TEXT("settle_tick_count"),TEXT("refresh_bone_transforms"),TEXT("allow_missing_bones"),TEXT("pose_link_max_depth"),TEXT("input_pose_mode"),TEXT("input_pose_field_path"),TEXT("input_pose_index"),TEXT("include_pins"),TEXT("max_depth"),TEXT("dry_run")};
    for(const auto& Pair:Args->Values){if(!Allowed.Contains(FString(Pair.Key.ToView()))){Error=TEXT("Unexpected public35 input; raw aliases remain Batch-only");return false;}}
    {const TCHAR* Key=TEXT("blueprint_name");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT(""));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("anim_blueprint");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT(""));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("graph_name");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT("AnimGraph"));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("graph_id");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT(""));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("graph_type");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT("function"));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("node_id");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT(""));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("node_type");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT(""));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("title_contains");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT(""));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("mode");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT("active_component_tick_delta"));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("skeletal_mesh");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT(""));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("temp_root");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT("/Game/_MCP_Temp/AnimNodePrePost"));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("run_id");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT(""));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("actor_label");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT(""));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("actor_name");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT(""));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("actor_path");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT(""));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("component_name");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT(""));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("anim_instance_source");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT("main"));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("input_pose_mode");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT("first"));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("input_pose_field_path");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT(""));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("cleanup");auto V=Args->TryGetField(Key);if(V.IsValid()&&V->Type!=EJson::Boolean){Error=TEXT("Exact public JSON boolean required");return false;}if(!V.IsValid()){Args->SetBoolField(Key,true);}}
    {const TCHAR* Key=TEXT("overwrite_existing_temp_assets");auto V=Args->TryGetField(Key);if(V.IsValid()&&V->Type!=EJson::Boolean){Error=TEXT("Exact public JSON boolean required");return false;}if(!V.IsValid()){Args->SetBoolField(Key,true);}}
    {const TCHAR* Key=TEXT("prefer_pie_world");auto V=Args->TryGetField(Key);if(V.IsValid()&&V->Type!=EJson::Boolean){Error=TEXT("Exact public JSON boolean required");return false;}if(!V.IsValid()){Args->SetBoolField(Key,true);}}
    {const TCHAR* Key=TEXT("require_pie_world");auto V=Args->TryGetField(Key);if(V.IsValid()&&V->Type!=EJson::Boolean){Error=TEXT("Exact public JSON boolean required");return false;}if(!V.IsValid()){Args->SetBoolField(Key,false);}}
    {const TCHAR* Key=TEXT("refresh_bone_transforms");auto V=Args->TryGetField(Key);if(V.IsValid()&&V->Type!=EJson::Boolean){Error=TEXT("Exact public JSON boolean required");return false;}if(!V.IsValid()){Args->SetBoolField(Key,true);}}
    {const TCHAR* Key=TEXT("allow_missing_bones");auto V=Args->TryGetField(Key);if(V.IsValid()&&V->Type!=EJson::Boolean){Error=TEXT("Exact public JSON boolean required");return false;}if(!V.IsValid()){Args->SetBoolField(Key,false);}}
    {const TCHAR* Key=TEXT("include_pins");auto V=Args->TryGetField(Key);if(V.IsValid()&&V->Type!=EJson::Boolean){Error=TEXT("Exact public JSON boolean required");return false;}if(!V.IsValid()){Args->SetBoolField(Key,true);}}
    {const TCHAR* Key=TEXT("dry_run");auto V=Args->TryGetField(Key);if(V.IsValid()&&V->Type!=EJson::Boolean){Error=TEXT("Exact public JSON boolean required");return false;}if(!V.IsValid()){Args->SetBoolField(Key,true);}}
    {const TCHAR* Key=TEXT("tick_count");auto V=Args->TryGetField(Key);FString Decimal=TEXT("1");if(V.IsValid()&&!IntegralDecimal(V,Decimal)){Error=TEXT("Exact int64 JSON integer required");return false;}Args->SetStringField(Key,Decimal);}
    {const TCHAR* Key=TEXT("settle_tick_count");auto V=Args->TryGetField(Key);FString Decimal=TEXT("0");if(V.IsValid()&&!IntegralDecimal(V,Decimal)){Error=TEXT("Exact int64 JSON integer required");return false;}Args->SetStringField(Key,Decimal);}
    {const TCHAR* Key=TEXT("pose_link_max_depth");auto V=Args->TryGetField(Key);FString Decimal=TEXT("4");if(V.IsValid()&&!IntegralDecimal(V,Decimal)){Error=TEXT("Exact int64 JSON integer required");return false;}Args->SetStringField(Key,Decimal);}
    {const TCHAR* Key=TEXT("input_pose_index");auto V=Args->TryGetField(Key);FString Decimal=TEXT("-1");if(V.IsValid()&&!IntegralDecimal(V,Decimal)){Error=TEXT("Exact int64 JSON integer required");return false;}Args->SetStringField(Key,Decimal);}
    {const TCHAR* Key=TEXT("max_depth");auto V=Args->TryGetField(Key);FString Decimal=TEXT("3");if(V.IsValid()&&!IntegralDecimal(V,Decimal)){Error=TEXT("Exact int64 JSON integer required");return false;}Args->SetStringField(Key,Decimal);}
    {const TCHAR* Key=TEXT("tick_delta_time");auto V=Args->TryGetField(Key);FString Decimal=TEXT("0.03333333333333333");if(V.IsValid()){double Number=0;if(V->Type!=EJson::Number||!V->TryGetNumber(Number)||!FMath::IsFinite(Number)||!V->TryGetString(Decimal)||Decimal.Len()>MaxCarrierChars){Error=TEXT("Finite double JSON number required");return false;}}Args->SetStringField(Key,Decimal);}
    for(const TCHAR* Key:{TEXT("sample_bones"),TEXT("sample_sockets")})
    {
        auto Value=Args->TryGetField(Key);if(!Value.IsValid()){Value=MakeShared<FJsonValueNull>();}
        if(Value->Type!=EJson::Null&&Value->Type!=EJson::Array){Error=TEXT("Public samples remain nullable List[str]");return false;}
        if(Value->Type==EJson::Array)
        {
            if(Value->AsArray().Num()>65535){Error=TEXT("Name-list JSON value-slot bound exceeded");return false;}
            for(const auto& Item:Value->AsArray())
            {if(!Item.IsValid()||Item->Type!=EJson::String){Error=TEXT("Named samples require string rows; raw fallbacks remain Batch-only");return false;}for(TCHAR Ch:Item->AsString()){if(Ch==0){Error=TEXT("Decoded name NUL is refused");return false;}}}
        }
        FString Carrier;auto Writer=TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Carrier);
        if(!FJsonSerializer::Serialize(Value.ToSharedRef(),TEXT(""),Writer,false)){Error=TEXT("Nullable name serialization failed");return false;}
        const bool Closed=Writer->Close();const bool Scalar=Value->Type!=EJson::Array;TArray<TSharedPtr<FJsonValue>> Back;
        if((!Closed&&!Scalar)||Carrier.Len()>MaxCarrierChars||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+Carrier+TEXT("]")),Back,FJsonSerializer::EFlags::None)||Back.Num()!=1||!Back[0].IsValid()||Back[0]->Type!=Value->Type){Error=TEXT("Nullable name carrier readback/type/bound failed");return false;}
        Args->SetStringField(Key,Carrier);
    }
    PackedInput=Json(Args.ToSharedRef());return true;
}

bool Metadata(UClass* Class,FString& Error)
{
    if(!Class||!IsValid(Class)||Class->HasAnyClassFlags(CLASS_NewerVersionExists)||!Class->GetPathName().Equals(CanonicalClass,ESearchCase::CaseSensitive)||!Class->IsChildOf(UToolsetDefinition::StaticClass())){Error=TEXT("Exact live canonical AnimGraph node pre/post class required");return false;}
    UFunction* Function=Class->FindFunctionByName(FName(AnimNodePrePostRuntimePoseMethod));
    if(!Function||Function->GetOuter()!=Class||!Function->GetName().Equals(AnimNodePrePostRuntimePoseMethod,ESearchCase::CaseSensitive)){Error=TEXT("Exact owned AnimGraph node pre/post method required");return false;}
    auto Callable=UToolsetDefinition::IsFunctionAICallable(TObjectPtr<const UFunction>(Function));if(!Callable.HasValue()||!Callable.GetValue()){Error=TEXT("Officially callable method required");return false;}
    const TCHAR* Order[]={TEXT("blueprint_name"),TEXT("anim_blueprint"),TEXT("graph_name"),TEXT("graph_id"),TEXT("graph_type"),TEXT("node_id"),TEXT("node_type"),TEXT("title_contains"),TEXT("sample_bones"),TEXT("sample_sockets"),TEXT("mode"),TEXT("skeletal_mesh"),TEXT("temp_root"),TEXT("cleanup"),TEXT("overwrite_existing_temp_assets"),TEXT("run_id"),TEXT("actor_label"),TEXT("actor_name"),TEXT("actor_path"),TEXT("component_name"),TEXT("anim_instance_source"),TEXT("prefer_pie_world"),TEXT("require_pie_world"),TEXT("tick_count"),TEXT("tick_delta_time"),TEXT("settle_tick_count"),TEXT("refresh_bone_transforms"),TEXT("allow_missing_bones"),TEXT("pose_link_max_depth"),TEXT("input_pose_mode"),TEXT("input_pose_field_path"),TEXT("input_pose_index"),TEXT("include_pins"),TEXT("max_depth"),TEXT("dry_run")};const TSet<int32> BooleanIndices{13,14,21,22,26,27,32,34};int32 Index=0;
    for(TFieldIterator<FProperty> It(Function,EFieldIteratorFlags::ExcludeSuper);It;++It){if(!It->HasAnyPropertyFlags(CPF_Parm)||It->HasAnyPropertyFlags(CPF_ReturnParm)){continue;}if(Index>=35||!It->GetName().Equals(Order[Index],ESearchCase::CaseSensitive)||It->GetOwnerStruct()!=Function||It->ArrayDim!=1||It->HasAnyPropertyFlags(CPF_OutParm|CPF_ReferenceParm)||(BooleanIndices.Contains(Index)?!CastField<FBoolProperty>(*It):!CastField<FStrProperty>(*It))){Error=TEXT("Exact physical35 FString27/bool8 field order required");return false;}++Index;}
    if(Index!=35){Error=TEXT("Exact physical35 arity required");return false;}
    {const FName Key(TEXT("CPP_Default_blueprint_name"));if(Function->HasMetaData(Key)&&!Function->GetMetaData(Key).IsEmpty()){Error=TEXT("Original/private carrier default differs: blueprint_name");return false;}}
    {const FName Key(TEXT("CPP_Default_anim_blueprint"));if(Function->HasMetaData(Key)&&!Function->GetMetaData(Key).IsEmpty()){Error=TEXT("Original/private carrier default differs: anim_blueprint");return false;}}
    {const FName Key(TEXT("CPP_Default_graph_name"));if(!Function->HasMetaData(Key)||Function->GetMetaData(Key)!=TEXT("AnimGraph")){Error=TEXT("Original/private carrier default differs: graph_name");return false;}}
    {const FName Key(TEXT("CPP_Default_graph_id"));if(Function->HasMetaData(Key)&&!Function->GetMetaData(Key).IsEmpty()){Error=TEXT("Original/private carrier default differs: graph_id");return false;}}
    {const FName Key(TEXT("CPP_Default_graph_type"));if(!Function->HasMetaData(Key)||Function->GetMetaData(Key)!=TEXT("function")){Error=TEXT("Original/private carrier default differs: graph_type");return false;}}
    {const FName Key(TEXT("CPP_Default_node_id"));if(Function->HasMetaData(Key)&&!Function->GetMetaData(Key).IsEmpty()){Error=TEXT("Original/private carrier default differs: node_id");return false;}}
    {const FName Key(TEXT("CPP_Default_node_type"));if(Function->HasMetaData(Key)&&!Function->GetMetaData(Key).IsEmpty()){Error=TEXT("Original/private carrier default differs: node_type");return false;}}
    {const FName Key(TEXT("CPP_Default_title_contains"));if(Function->HasMetaData(Key)&&!Function->GetMetaData(Key).IsEmpty()){Error=TEXT("Original/private carrier default differs: title_contains");return false;}}
    {const FName Key(TEXT("CPP_Default_sample_bones"));if(!Function->HasMetaData(Key)||Function->GetMetaData(Key)!=TEXT("null")){Error=TEXT("Original/private carrier default differs: sample_bones");return false;}}
    {const FName Key(TEXT("CPP_Default_sample_sockets"));if(!Function->HasMetaData(Key)||Function->GetMetaData(Key)!=TEXT("null")){Error=TEXT("Original/private carrier default differs: sample_sockets");return false;}}
    {const FName Key(TEXT("CPP_Default_mode"));if(!Function->HasMetaData(Key)||Function->GetMetaData(Key)!=TEXT("active_component_tick_delta")){Error=TEXT("Original/private carrier default differs: mode");return false;}}
    {const FName Key(TEXT("CPP_Default_skeletal_mesh"));if(Function->HasMetaData(Key)&&!Function->GetMetaData(Key).IsEmpty()){Error=TEXT("Original/private carrier default differs: skeletal_mesh");return false;}}
    {const FName Key(TEXT("CPP_Default_temp_root"));if(!Function->HasMetaData(Key)||Function->GetMetaData(Key)!=TEXT("/Game/_MCP_Temp/AnimNodePrePost")){Error=TEXT("Original/private carrier default differs: temp_root");return false;}}
    {const FName Key(TEXT("CPP_Default_run_id"));if(Function->HasMetaData(Key)&&!Function->GetMetaData(Key).IsEmpty()){Error=TEXT("Original/private carrier default differs: run_id");return false;}}
    {const FName Key(TEXT("CPP_Default_actor_label"));if(Function->HasMetaData(Key)&&!Function->GetMetaData(Key).IsEmpty()){Error=TEXT("Original/private carrier default differs: actor_label");return false;}}
    {const FName Key(TEXT("CPP_Default_actor_name"));if(Function->HasMetaData(Key)&&!Function->GetMetaData(Key).IsEmpty()){Error=TEXT("Original/private carrier default differs: actor_name");return false;}}
    {const FName Key(TEXT("CPP_Default_actor_path"));if(Function->HasMetaData(Key)&&!Function->GetMetaData(Key).IsEmpty()){Error=TEXT("Original/private carrier default differs: actor_path");return false;}}
    {const FName Key(TEXT("CPP_Default_component_name"));if(Function->HasMetaData(Key)&&!Function->GetMetaData(Key).IsEmpty()){Error=TEXT("Original/private carrier default differs: component_name");return false;}}
    {const FName Key(TEXT("CPP_Default_anim_instance_source"));if(!Function->HasMetaData(Key)||Function->GetMetaData(Key)!=TEXT("main")){Error=TEXT("Original/private carrier default differs: anim_instance_source");return false;}}
    {const FName Key(TEXT("CPP_Default_tick_count"));if(!Function->HasMetaData(Key)||Function->GetMetaData(Key)!=TEXT("1")){Error=TEXT("Original/private carrier default differs: tick_count");return false;}}
    {const FName Key(TEXT("CPP_Default_tick_delta_time"));if(!Function->HasMetaData(Key)||Function->GetMetaData(Key)!=TEXT("0.03333333333333333")){Error=TEXT("Original/private carrier default differs: tick_delta_time");return false;}}
    {const FName Key(TEXT("CPP_Default_settle_tick_count"));if(!Function->HasMetaData(Key)||Function->GetMetaData(Key)!=TEXT("0")){Error=TEXT("Original/private carrier default differs: settle_tick_count");return false;}}
    {const FName Key(TEXT("CPP_Default_pose_link_max_depth"));if(!Function->HasMetaData(Key)||Function->GetMetaData(Key)!=TEXT("4")){Error=TEXT("Original/private carrier default differs: pose_link_max_depth");return false;}}
    {const FName Key(TEXT("CPP_Default_input_pose_mode"));if(!Function->HasMetaData(Key)||Function->GetMetaData(Key)!=TEXT("first")){Error=TEXT("Original/private carrier default differs: input_pose_mode");return false;}}
    {const FName Key(TEXT("CPP_Default_input_pose_field_path"));if(Function->HasMetaData(Key)&&!Function->GetMetaData(Key).IsEmpty()){Error=TEXT("Original/private carrier default differs: input_pose_field_path");return false;}}
    {const FName Key(TEXT("CPP_Default_input_pose_index"));if(!Function->HasMetaData(Key)||Function->GetMetaData(Key)!=TEXT("-1")){Error=TEXT("Original/private carrier default differs: input_pose_index");return false;}}
    {const FName Key(TEXT("CPP_Default_max_depth"));if(!Function->HasMetaData(Key)||Function->GetMetaData(Key)!=TEXT("3")){Error=TEXT("Original/private carrier default differs: max_depth");return false;}}
    return true;
}

// This only adjusts the detached retained handler's local filters. Registry user lists are untouched.
void SyncOriginalFilters(const TSharedRef<FAttachmentState>& State)
{
    if (UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get())
    { State->Original->SetNameFilters(Subsystem->ToolsetRegistry.GetBlockedNames(), Subsystem->ToolsetRegistry.GetAllowedNames()); }
}

FString UnfilteredOriginalSchema(const TSharedRef<FAttachmentState>& State)
{
    // Public GetJsonSchema is filtered. Obtain all tools without editing any registry settings.
    State->Original->SetNameFilters({}, {});
    const FString Full = State->Original->GetJsonSchema();
    SyncOriginalFilters(State);
    // Retained unguarded handler is disabled between authorized wrapper delegations.
    State->Original->SetEnabled(false);
    return Full;
}

bool Required(const TSharedPtr<FJsonObject>& Schema, const TSet<FString>& Expected)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Schema.IsValid() || !Schema->TryGetArrayField(TEXT("required"), Values) || !Values || Values->Num() != Expected.Num()) { return false; }
    TSet<FString> Remaining = Expected;
    for (const auto& Value : *Values)
    { if (!Value.IsValid() || Value->Type != EJson::String || Remaining.Remove(Value->AsString()) != 1) { return false; } }
    return Remaining.IsEmpty();
}

bool Type(const TSharedPtr<FJsonObject>& Object, const TCHAR* Expected)
{
    FString Value; return Object.IsValid() && Object->TryGetStringField(TEXT("type"), Value) && Value == Expected;
}

bool AnimNodePrePostRuntimePoseSchema(const FString& Reflected,FString& Logical,FString& Error)
{
    TSharedPtr<FJsonObject> Root;const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;FString Name;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Reflected),Root)||!Root.IsValid()||!Root->TryGetStringField(TEXT("name"),Name)||Name!=CanonicalName||!Root->TryGetArrayField(TEXT("tools"),Rows)||!Rows||Rows->Num()!=1){Error=TEXT("Exactly one canonical AnimGraph node pre/post Toolset required");return false;}
    const TSharedPtr<FJsonObject>* Tool=nullptr;const TSharedPtr<FJsonObject>* Input=nullptr;const TSharedPtr<FJsonObject>* Inputs=nullptr;const TSharedPtr<FJsonObject>* Output=nullptr;const TSharedPtr<FJsonObject>* Outer=nullptr;const TSharedPtr<FJsonObject>* Result=nullptr;const TSharedPtr<FJsonObject>* Fields=nullptr;
    const TSet<FString> ResultKeys{TEXT("success"),TEXT("operation_id"),TEXT("stage"),TEXT("error_code"),TEXT("error"),TEXT("changes_json"),TEXT("compile_json"),TEXT("save_json"),TEXT("validation_json"),TEXT("retry_json"),TEXT("data_json")};
    if(!(*Rows)[0].IsValid()||!(*Rows)[0]->TryGetObject(Tool)||!Tool||!Tool->IsValid()||!(*Tool)->TryGetStringField(TEXT("name"),Name)||Name!=FString(CanonicalName)+TEXT(".")+AnimNodePrePostRuntimePoseMethod||!(*Tool)->TryGetObjectField(TEXT("inputSchema"),Input)||!Type(*Input,TEXT("object"))||!(*Input)->TryGetObjectField(TEXT("properties"),Inputs)||(*Inputs)->Values.Num()!=35||!Required(*Input,{TEXT("blueprint_name"),TEXT("anim_blueprint"),TEXT("graph_id"),TEXT("node_id"),TEXT("node_type"),TEXT("title_contains"),TEXT("skeletal_mesh"),TEXT("run_id"),TEXT("actor_label"),TEXT("actor_name"),TEXT("actor_path"),TEXT("component_name"),TEXT("input_pose_field_path")})){Error=TEXT("Exact physical35 and SDK required empty13 fields required");return false;}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("blueprint_name"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: blueprint_name");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Physical empty default must be absent: blueprint_name");return false;}(*Field)->SetStringField(TEXT("default"),TEXT(""));}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("anim_blueprint"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: anim_blueprint");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Physical empty default must be absent: anim_blueprint");return false;}(*Field)->SetStringField(TEXT("default"),TEXT(""));}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("graph_name"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: graph_name");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=TEXT("AnimGraph")){Error=TEXT("Original/private carrier default differs: graph_name");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("graph_id"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: graph_id");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Physical empty default must be absent: graph_id");return false;}(*Field)->SetStringField(TEXT("default"),TEXT(""));}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("graph_type"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: graph_type");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=TEXT("function")){Error=TEXT("Original/private carrier default differs: graph_type");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("node_id"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: node_id");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Physical empty default must be absent: node_id");return false;}(*Field)->SetStringField(TEXT("default"),TEXT(""));}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("node_type"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: node_type");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Physical empty default must be absent: node_type");return false;}(*Field)->SetStringField(TEXT("default"),TEXT(""));}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("title_contains"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: title_contains");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Physical empty default must be absent: title_contains");return false;}(*Field)->SetStringField(TEXT("default"),TEXT(""));}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("sample_bones"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: sample_bones");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=TEXT("null")){Error=TEXT("Original/private carrier default differs: sample_bones");return false;}(*Field)->SetArrayField(TEXT("type"),{MakeShared<FJsonValueString>(TEXT("array")),MakeShared<FJsonValueString>(TEXT("null"))});(*Field)->SetField(TEXT("default"),MakeShared<FJsonValueNull>());auto Item=MakeShared<FJsonObject>();Item->SetStringField(TEXT("type"),TEXT("string"));(*Field)->SetObjectField(TEXT("items"),Item);}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("sample_sockets"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: sample_sockets");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=TEXT("null")){Error=TEXT("Original/private carrier default differs: sample_sockets");return false;}(*Field)->SetArrayField(TEXT("type"),{MakeShared<FJsonValueString>(TEXT("array")),MakeShared<FJsonValueString>(TEXT("null"))});(*Field)->SetField(TEXT("default"),MakeShared<FJsonValueNull>());auto Item=MakeShared<FJsonObject>();Item->SetStringField(TEXT("type"),TEXT("string"));(*Field)->SetObjectField(TEXT("items"),Item);}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("mode"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: mode");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=TEXT("active_component_tick_delta")){Error=TEXT("Original/private carrier default differs: mode");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("skeletal_mesh"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: skeletal_mesh");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Physical empty default must be absent: skeletal_mesh");return false;}(*Field)->SetStringField(TEXT("default"),TEXT(""));}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("temp_root"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: temp_root");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=TEXT("/Game/_MCP_Temp/AnimNodePrePost")){Error=TEXT("Original/private carrier default differs: temp_root");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("cleanup"),Field)||!Type(*Field,TEXT("boolean"))){Error=TEXT("Physical field type differs: cleanup");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()!=true){Error=TEXT("Original boolean default differs: cleanup");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("overwrite_existing_temp_assets"),Field)||!Type(*Field,TEXT("boolean"))){Error=TEXT("Physical field type differs: overwrite_existing_temp_assets");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()!=true){Error=TEXT("Original boolean default differs: overwrite_existing_temp_assets");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("run_id"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: run_id");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Physical empty default must be absent: run_id");return false;}(*Field)->SetStringField(TEXT("default"),TEXT(""));}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("actor_label"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: actor_label");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Physical empty default must be absent: actor_label");return false;}(*Field)->SetStringField(TEXT("default"),TEXT(""));}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("actor_name"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: actor_name");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Physical empty default must be absent: actor_name");return false;}(*Field)->SetStringField(TEXT("default"),TEXT(""));}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("actor_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: actor_path");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Physical empty default must be absent: actor_path");return false;}(*Field)->SetStringField(TEXT("default"),TEXT(""));}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("component_name"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: component_name");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Physical empty default must be absent: component_name");return false;}(*Field)->SetStringField(TEXT("default"),TEXT(""));}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("anim_instance_source"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: anim_instance_source");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=TEXT("main")){Error=TEXT("Original/private carrier default differs: anim_instance_source");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("prefer_pie_world"),Field)||!Type(*Field,TEXT("boolean"))){Error=TEXT("Physical field type differs: prefer_pie_world");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()!=true){Error=TEXT("Original boolean default differs: prefer_pie_world");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("require_pie_world"),Field)||!Type(*Field,TEXT("boolean"))){Error=TEXT("Physical field type differs: require_pie_world");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()!=false){Error=TEXT("Original boolean default differs: require_pie_world");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("tick_count"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: tick_count");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=TEXT("1")){Error=TEXT("Original/private carrier default differs: tick_count");return false;}(*Field)->SetStringField(TEXT("type"),TEXT("integer"));(*Field)->SetNumberField(TEXT("default"),1);}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("tick_delta_time"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: tick_delta_time");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=TEXT("0.03333333333333333")){Error=TEXT("Original/private carrier default differs: tick_delta_time");return false;}(*Field)->SetStringField(TEXT("type"),TEXT("number"));(*Field)->SetNumberField(TEXT("default"),0.03333333333333333);}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("settle_tick_count"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: settle_tick_count");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=TEXT("0")){Error=TEXT("Original/private carrier default differs: settle_tick_count");return false;}(*Field)->SetStringField(TEXT("type"),TEXT("integer"));(*Field)->SetNumberField(TEXT("default"),0);}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("refresh_bone_transforms"),Field)||!Type(*Field,TEXT("boolean"))){Error=TEXT("Physical field type differs: refresh_bone_transforms");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()!=true){Error=TEXT("Original boolean default differs: refresh_bone_transforms");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("allow_missing_bones"),Field)||!Type(*Field,TEXT("boolean"))){Error=TEXT("Physical field type differs: allow_missing_bones");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()!=false){Error=TEXT("Original boolean default differs: allow_missing_bones");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("pose_link_max_depth"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: pose_link_max_depth");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=TEXT("4")){Error=TEXT("Original/private carrier default differs: pose_link_max_depth");return false;}(*Field)->SetStringField(TEXT("type"),TEXT("integer"));(*Field)->SetNumberField(TEXT("default"),4);}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("input_pose_mode"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: input_pose_mode");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=TEXT("first")){Error=TEXT("Original/private carrier default differs: input_pose_mode");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("input_pose_field_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: input_pose_field_path");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Physical empty default must be absent: input_pose_field_path");return false;}(*Field)->SetStringField(TEXT("default"),TEXT(""));}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("input_pose_index"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: input_pose_index");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=TEXT("-1")){Error=TEXT("Original/private carrier default differs: input_pose_index");return false;}(*Field)->SetStringField(TEXT("type"),TEXT("integer"));(*Field)->SetNumberField(TEXT("default"),-1);}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("include_pins"),Field)||!Type(*Field,TEXT("boolean"))){Error=TEXT("Physical field type differs: include_pins");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()!=true){Error=TEXT("Original boolean default differs: include_pins");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("max_depth"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: max_depth");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=TEXT("3")){Error=TEXT("Original/private carrier default differs: max_depth");return false;}(*Field)->SetStringField(TEXT("type"),TEXT("integer"));(*Field)->SetNumberField(TEXT("default"),3);}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("dry_run"),Field)||!Type(*Field,TEXT("boolean"))){Error=TEXT("Physical field type differs: dry_run");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()!=true){Error=TEXT("Original boolean default differs: dry_run");return false;}}
    (*Input)->SetArrayField(TEXT("required"),{});(*Input)->SetBoolField(TEXT("additionalProperties"),false);
    if (!(*Tool)->TryGetObjectField(TEXT("outputSchema"), Output) || !Type(*Output, TEXT("object")) || !Required(*Output, {TEXT("returnValue")})
        || !(*Output)->TryGetObjectField(TEXT("properties"), Outer) || (*Outer)->Values.Num() != 1 || !(*Outer)->TryGetObjectField(TEXT("returnValue"), Result)
        || !Type(*Result, TEXT("object")) || !Required(*Result, ResultKeys) || !(*Result)->TryGetObjectField(TEXT("properties"), Fields) || (*Fields)->Values.Num() != 11)
    { Error = TEXT("Original synchronous standard11 output envelope required"); return false; }
    for (const auto& Key : ResultKeys)
    { const TSharedPtr<FJsonObject>* Field = nullptr; if (!(*Fields)->TryGetObjectField(Key, Field) || !Type(*Field, Key == TEXT("success") ? TEXT("boolean") : TEXT("string")))
      { Error = TEXT("Original standard11 field types required"); return false; } }
    Logical = Json(Root.ToSharedRef()); return true;
}


class FAnimNodePrePostRuntimePoseToolset final : public FToolset
{
public:
    explicit FAnimNodePrePostRuntimePoseToolset(const TSharedRef<FAttachmentState>& InState) : State(InState) {}
    virtual FString GetToolsetName() const override { return CanonicalName; }
    virtual FString GetToolsetVersion() const override { return State->Version; }
    virtual FString GetToolsetDescription() const override { return State->Description; }
    virtual UClass* GetToolsetClass() const override { return State->Class.Get(); }
protected:
    // Immutable full projected logical schema is visible at the registration event, even while dispatch is inactive.
    // Base FToolset applies current wrapper filters. No UObjects or filter changes on schema reads.
    virtual FString GetJsonSchemaInternal() const override { return State->FullSchema; }
    virtual TFuture<FOutput> ExecuteToolInternal(const FString& Method, const FString& Input) override
    {
        auto Reject = [&Method](const TCHAR* Code, const FString& Error)
        { return MakeFulfilledPromise<FOutput>(MakeValue(Rejection(Code, Error, Method))).GetFuture(); };
        if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy || !State->Active.Load() || State->InFlight.Load() != 0)
        { return Reject(TEXT("anim_node_pre_post_runtime_pose_raw_guard_unavailable"), TEXT("A live active game-thread AnimNodePrePostRuntimePose guard is required; no delegate dispatched")); }
        UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get();
        if (!Subsystem || Subsystem->ToolsetRegistry.Find(CanonicalName).Get() != this)
        { return Reject(TEXT("anim_node_pre_post_runtime_pose_raw_guard_ownership_lost"), TEXT("Exact original-subsystem wrapper identity is unavailable; no delegate dispatched")); }
        FString Error;
        if (!Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
        { State->Active.Store(false); State->Original->SetEnabled(false); return Reject(TEXT("anim_node_pre_post_runtime_pose_raw_guard_definition_changed"), Error); }
        if (!Method.Equals(AnimNodePrePostRuntimePoseMethod, ESearchCase::CaseSensitive))
        { return Reject(TEXT("anim_node_pre_post_runtime_pose_method_refused"), TEXT("Only the exact original anim_node_pre_post_runtime_pose adapter method is guarded")); }
        FString PackedInput;
        if (!RawTransform(Input, PackedInput, Error))
        { return Reject(TEXT("anim_node_pre_post_runtime_pose_raw_arguments_invalid"), Error); }
        const TSharedRef<FAttachmentState> CallState = State;
        const TSharedPtr<FToolset> OriginalLifetime = State->Original;
        const TSharedPtr<FToolset> WrapperLifetime = State->Wrapper;
        State->AnimNodePrePostRuntimePoseDelegation.Store(false);
        // Public SetNameFilters generates schema and can invoke converter callbacks.
        // Capture lifetimes and reject nested delegation before entering that path.
        State->InFlight.Store(1);
        SyncOriginalFilters(State);
        auto CurrentSubsystem = UToolsetRegistrySubsystem::Get();
        if (!State->Active.Load() || !CurrentSubsystem.HasValue()
            || CurrentSubsystem.GetValue().Get() != State->Subsystem.Get()
            || Subsystem->ToolsetRegistry.Find(CanonicalName) != WrapperLifetime
            || !WrapperLifetime->IsToolEnabled(FString(CanonicalName) + TEXT(".") + Method)
            || !Metadata(State->Class.Get(), Error) || OriginalLifetime->GetToolsetClass() != State->Class.Get())
        {
            State->Active.Store(false); OriginalLifetime->SetEnabled(false);
            State->AnimNodePrePostRuntimePoseDelegation.Store(false); State->InFlight.Store(0);
            if (Error.IsEmpty()) { Error = TEXT("AnimNodePrePostRuntimePose ownership/filter/class changed during local filter callbacks; no delegate dispatched"); }
            return Reject(TEXT("anim_node_pre_post_runtime_pose_raw_guard_changed_before_dispatch"), Error);
        }
        // Only the actual guarded Named method delegation may ask Verify for its exact admission marker.
        State->AnimNodePrePostRuntimePoseDelegation.Store(Method.Equals(AnimNodePrePostRuntimePoseMethod, ESearchCase::CaseSensitive));
        // Direct public delegation avoids recursion through the same canonical Registry name.
        return OriginalLifetime->ExecuteTool(Method, PackedInput).Next(
            [CallState, OriginalLifetime, WrapperLifetime](FOutput&& Output) -> FOutput
            {
                // Official reflected handler completions run on the game thread; no UObject work here.
                if (IsInGameThread()) { OriginalLifetime->SetEnabled(false); }
                else { CallState->Active.Store(false); }
                CallState->AnimNodePrePostRuntimePoseDelegation.Store(false); CallState->InFlight.Store(0);
                return MoveTemp(Output);
            });
    }
private:
    TSharedRef<FAttachmentState> State;
};

void BaseReport(const TSharedRef<FJsonObject>& Report)
{
    Report->SetBoolField(TEXT("ready"), false); Report->SetBoolField(TEXT("verified"), false);
    Report->SetBoolField(TEXT("asset_mutation"), false); Report->SetBoolField(TEXT("metadata_mutation"), false);
    Report->SetBoolField(TEXT("actual_named_wire_verified"), false); Report->SetBoolField(TEXT("registry_changed"), false);
    Report->SetBoolField(TEXT("guard_attached"), false); Report->SetBoolField(TEXT("original_handler_detached"), false);
    Report->SetBoolField(TEXT("cleanup_required"), Owned.IsValid());
    Report->SetBoolField(TEXT("class_unregister_allowed"), !Owned.IsValid());
    Report->SetBoolField(TEXT("schema_adapter_ready_is_wire_proof"), false);
    Report->SetBoolField(TEXT("delegated_call_admitted"), false);
    Report->SetBoolField(TEXT("delegated_admission_only"), false);
    Report->SetStringField(TEXT("delegated_method"), TEXT(""));
    Report->SetStringField(TEXT("logical_schema_json"), TEXT(""));
    Report->SetStringField(TEXT("logical_schema_source"), TEXT(""));
    Report->SetBoolField(TEXT("logical_schema_registry_identity_verified"), false);
    Report->SetBoolField(TEXT("original_handler_restored"), false);
    Report->SetStringField(TEXT("guard_scope"), TEXT("Resolve an AnimGraph node and preserve the original public35/defaults and four mode meanings: compiled mapping, same-instance PoseWatch input/output capture, active final-component tick delta, or isolated source-bypass/selected-node clone comparison. Native owns node/instance selection, sample fallbacks, ordered tick/refresh, debug attachments, temporary clones, compiler/cleanup and partial or unknown SDK effects. Isolated mode supports the original RigidBody/Trail MVP and temporary-root policy; asset duplication is refused during PIE/SIE by the existing public SDK route. No atomic rollback or automatic retry is provided. Official Named HTTP numbers may already be rounded before this adapter."));
    for (const TCHAR* Key : { TEXT("active"), TEXT("raw_guard_installed"), TEXT("default_verified"), TEXT("reflected_carrier_schema_verified"), TEXT("logical_schema_verified"), TEXT("output11_schema_preserved"), TEXT("ordinary_string19_schema_verified"), TEXT("nullable2_schema_verified"), TEXT("boolean8_schema_verified"), TEXT("integer5_schema_verified"), TEXT("number1_schema_verified"), TEXT("node_prepost35_schema_verified"), TEXT("empty_string_defaults_verified") })
    { Report->SetBoolField(Key, false); }
    Report->SetNumberField(TEXT("delegated_calls_in_flight"), Owned.IsValid() ? Owned->InFlight.Load() : 0);
}

bool Ready(const TSharedRef<FAttachmentState>& State, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get();
    if (!Subsystem || !State->Wrapper.IsValid() || !State->Original.IsValid())
    { Error = TEXT("Captured AnimNodePrePostRuntimePose guard owner/subsystem unavailable"); return false; }
    // Temporarily hide dispatch while probing a retained disabled object without changing global filters.
    const bool WasActive = State->Active.Load(); State->Active.Store(false);
    const bool Identical = HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper);
    State->Active.Store(WasActive && Identical);
    Report->SetBoolField(TEXT("guard_attached"), Identical);
    Report->SetBoolField(TEXT("original_handler_detached"), State->OriginalDetached);
    Report->SetBoolField(TEXT("class_unregister_allowed"), false);
    Report->SetBoolField(TEXT("cleanup_required"), true);
    if (!Identical) { State->Original->SetEnabled(false); Error = TEXT("Captured AnimNodePrePostRuntimePose wrapper no longer owns its canonical registry entry; foreign entry preserved"); return false; }
    if (!Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get()) { State->Active.Store(false); State->Original->SetEnabled(false); return false; }
    const FString CurrentSchema = UnfilteredOriginalSchema(State);
    FString CurrentLogical;
    Report->SetStringField(TEXT("reflected_schema_json"), CurrentSchema);
    if (!AnimNodePrePostRuntimePoseSchema(CurrentSchema, CurrentLogical, Error) || CurrentSchema != State->ReflectedSchema || CurrentLogical != State->FullSchema)
    { State->Active.Store(false); State->Original->SetEnabled(false); if (Error.IsEmpty()) { Error = TEXT("Original AnimNodePrePostRuntimePose schema changed; no automatic replacement or dispatch"); } return false; }
    auto CurrentSubsystem = UToolsetRegistrySubsystem::Get();
    if (!CurrentSubsystem.HasValue() || CurrentSubsystem.GetValue().Get() != State->Subsystem.Get()
        || !HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper)
        || !Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
    { State->Active.Store(false); State->Original->SetEnabled(false); if (Error.IsEmpty()) { Error = TEXT("AnimNodePrePostRuntimePose wrapper owner/class changed during reflected schema callbacks"); } return false; }
    Report->SetBoolField(TEXT("reflected_carrier_schema_verified"), true); Report->SetBoolField(TEXT("logical_schema_verified"), true);
    Report->SetBoolField(TEXT("default_verified"), true); Report->SetBoolField(TEXT("raw_guard_installed"), true);
    for (const TCHAR* Key : { TEXT("output11_schema_preserved"), TEXT("ordinary_string19_schema_verified"), TEXT("nullable2_schema_verified"), TEXT("boolean8_schema_verified"), TEXT("integer5_schema_verified"), TEXT("number1_schema_verified"), TEXT("node_prepost35_schema_verified"), TEXT("empty_string_defaults_verified") })
    { Report->SetBoolField(Key, true); }
    Report->SetBoolField(TEXT("toolset_enabled"), State->Wrapper->IsEnabled());
    if (!State->Wrapper->IsEnabled()) { Error = TEXT("AnimNodePrePostRuntimePose guard is installed but disabled by existing toolset state or user filters"); return false; }
    // The public Python class getter constructs a new physical FunctionLibraryToolset.
    // Read the actual registered wrapper instead, retaining current user filters.
    const TSharedPtr<FToolset> Registered = Subsystem->ToolsetRegistry.Find(CanonicalName);
    if (Registered != State->Wrapper)
    { State->Active.Store(false); Error = TEXT("Current registered AnimNodePrePostRuntimePose wrapper identity is unavailable for logical schema receipt"); return false; }
    const FString RegisteredLogicalSchema = Registered->GetJsonSchema();
    auto AfterSchemaSubsystem = UToolsetRegistrySubsystem::Get();
    if (RegisteredLogicalSchema.IsEmpty() || !AfterSchemaSubsystem.HasValue()
        || AfterSchemaSubsystem.GetValue().Get() != State->Subsystem.Get()
        || Subsystem->ToolsetRegistry.Find(CanonicalName) != Registered
        || !Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
    { State->Active.Store(false); State->Original->SetEnabled(false);
      if (Error.IsEmpty()) { Error = TEXT("Current registered AnimNodePrePostRuntimePose logical schema/owner/class receipt could not be confirmed"); }
      return false; }
    Report->SetStringField(TEXT("logical_schema_json"), RegisteredLogicalSchema);
    Report->SetStringField(TEXT("logical_schema_source"), TEXT("actual_registered_wrapper_GetJsonSchema"));
    Report->SetBoolField(TEXT("logical_schema_registry_identity_verified"), true);
    return true;
}

bool Cleanup(const TSharedRef<FJsonObject>& Report, FString& Error)
{
    if (!Owned.IsValid()) { Report->SetBoolField(TEXT("cleanup_required"), false); return true; }
    const TSharedRef<FAttachmentState> State = Owned.ToSharedRef(); State->Active.Store(false);
    if (State->Original.IsValid()) { State->Original->SetEnabled(false); }
    UToolsetRegistrySubsystem* Subsystem = UObjectInitialized() ? State->Subsystem.Get() : nullptr;
    bool Changed = false;
    if (Subsystem)
    {
        // SDK removal is name-only. Probe and remove only retained identities, including filtered owners.
        if (State->Wrapper.IsValid() && HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper))
        { Changed |= Subsystem->ToolsetRegistry.UnregisterToolset(State->Wrapper); }
        // Failed installation can leave the captured, disabled original registered. Never restore it.
        if (State->Original.IsValid() && HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Original))
        { Changed |= Subsystem->ToolsetRegistry.UnregisterToolset(State->Original); }
        const bool Remains = (State->Wrapper.IsValid() && HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper))
            || (State->Original.IsValid() && HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Original));
        if (Remains) { Report->SetBoolField(TEXT("cleanup_required"), true); Report->SetBoolField(TEXT("registry_changed"), Changed); Error = TEXT("Owned AnimNodePrePostRuntimePose guard cleanup was not confirmed; inactive retained owner preserved"); return false; }
    }
    // Detached/cached wrappers are inactive, so they cannot dispatch after this cycle is broken.
    State->Wrapper.Reset(); State->Original.Reset(); Owned.Reset();
    Report->SetBoolField(TEXT("registry_changed"), Changed); Report->SetBoolField(TEXT("cleanup_required"), false);
    Report->SetBoolField(TEXT("class_unregister_allowed"), false); Report->SetBoolField(TEXT("owned_handler_removed"), true);
    Report->SetBoolField(TEXT("foreign_handler_preserved"), true);
    return true;
}
}

bool InstallAnimNodePrePostRuntimePoseToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { Error = TEXT("AnimNodePrePostRuntimePose guard installation requires an idle live editor game thread; no attachment retried"); return false; }
    FBusyScope Busy;
    if (!Metadata(ToolsetClass, Error)) { return false; }
    auto Current = UToolsetRegistrySubsystem::Get();
    if (!Current.HasValue()) { Error = TEXT("Original official Toolset Registry subsystem unavailable"); return false; }
    if (Owned.IsValid())
    {
        if (Owned->Subsystem.Get() != Current.GetValue().Get() || Owned->Class.Get() != ToolsetClass)
        { Error = TEXT("Different retained AnimNodePrePostRuntimePose owner/subsystem/class exists; explicit cleanup required, no automatic reattachment"); return false; }
        const bool Ok = Ready(Owned.ToSharedRef(), Report, Error) && Owned->Active.Load();
        Report->SetBoolField(TEXT("ready"), Ok); Report->SetBoolField(TEXT("verified"), Ok); Report->SetBoolField(TEXT("active"), Owned->Active.Load()); return Ok;
    }
    TSharedPtr<FToolset> Original = Current.GetValue()->ToolsetRegistry.Find(CanonicalName);
    if (!Original.IsValid() || Original->GetToolsetClass() != ToolsetClass || !Original->GetToolsetName().Equals(CanonicalName, ESearchCase::CaseSensitive))
    { Error = TEXT("Existing enabled exact canonical class handler required after Root-owned class registration; foreign/disabled entries not replaced"); return false; }
    const TSharedRef<FAttachmentState> State = MakeShared<FAttachmentState>(ToolsetClass);
    State->Subsystem = Current.GetValue().Get(); State->Original = Original;
    State->Version = Original->GetToolsetVersion(); State->Description = Original->GetToolsetDescription();
    // Ownership is captured before any local filtering, including a schema-verification failure.
    Owned = State;
    Report->SetBoolField(TEXT("cleanup_required"), true); Report->SetBoolField(TEXT("class_unregister_allowed"), false);
    State->ReflectedSchema = UnfilteredOriginalSchema(State);
    Report->SetStringField(TEXT("reflected_schema_json"), State->ReflectedSchema);
    if (!AnimNodePrePostRuntimePoseSchema(State->ReflectedSchema, State->FullSchema, Error)) { Original->SetEnabled(false); return false; }
    State->Wrapper = MakeShared<FAnimNodePrePostRuntimePoseToolset>(State);
    // Exact identity is checked immediately before SDK's name-only removal, with dispatch inactive.
    if (!HasRetainedToolsetIdentity(Current.GetValue()->ToolsetRegistry, Original)
        || !Current.GetValue()->ToolsetRegistry.UnregisterToolset(Original))
    { Error = TEXT("Original AnimNodePrePostRuntimePose handler detach failed or identity changed; captured owner remains inactive for explicit cleanup"); return false; }
    State->OriginalDetached = true; Report->SetBoolField(TEXT("registry_changed"), true); Report->SetBoolField(TEXT("original_handler_detached"), true);
    // Register broadcasts callbacks. Reentry is busy-rejected and wrapper dispatch remains inactive.
    if (!Current.GetValue()->ToolsetRegistry.RegisterToolset(State->Wrapper))
    { Error = TEXT("AnimNodePrePostRuntimePose raw wrapper registration failed or collided; no unguarded handler restored, inspect actual registry before cleanup"); return false; }
    if (!Ready(State, Report, Error)) { return false; }
    State->Active.Store(true); State->Original->SetEnabled(false);
    Report->SetBoolField(TEXT("ready"), true); Report->SetBoolField(TEXT("verified"), true);
    Report->SetBoolField(TEXT("active"), true);
    // The registration event already exposed the immutable complete schema; activation changes no catalogue.
    Report->SetBoolField(TEXT("extra_catalogue_broadcast_required"), false);
    return true;
}

bool VerifyAnimNodePrePostRuntimePoseToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy) { Error = TEXT("AnimNodePrePostRuntimePose guard verification requires an idle live editor game thread"); return false; }
    if (Owned.IsValid() && Owned->InFlight.Load() != 0)
    {
        auto Current = UToolsetRegistrySubsystem::Get();
        const bool Admitted = Owned->InFlight.Load() == 1 && Owned->AnimNodePrePostRuntimePoseDelegation.Load() && Owned->Active.Load()
            && Owned->Class.Get() == ToolsetClass && Current.HasValue()
            && Current.GetValue().Get() == Owned->Subsystem.Get() && Owned->Wrapper.IsValid()
            && Current.GetValue()->ToolsetRegistry.Find(CanonicalName) == Owned->Wrapper;
        Report->SetBoolField(TEXT("delegated_call_admitted"), Admitted);
        Report->SetBoolField(TEXT("delegated_admission_only"), true);
        Report->SetStringField(TEXT("delegated_method"), Admitted ? AnimNodePrePostRuntimePoseMethod : TEXT(""));
        Report->SetBoolField(TEXT("active"), Admitted); Report->SetBoolField(TEXT("guard_attached"), Admitted);
        // This is only the current wrapper's admission check. No schema/metadata callbacks,
        // install/remove/asset work, or general readiness proof is performed here.
        // False intentionally keeps Native helper ready/verified=False even when admitted.
        if (!Admitted) { Error = TEXT("Only the current exact guarded Named anim_node_pre_post_runtime_pose adapter delegation is admitted"); }
        return false;
    }
    FBusyScope Busy;
    if (!Owned.IsValid() || Owned->Class.Get() != ToolsetClass) { Error = TEXT("Exact installed AnimNodePrePostRuntimePose raw guard required; verification never installs one"); return false; }
    auto Current = UToolsetRegistrySubsystem::Get();
    if (!Current.HasValue() || Current.GetValue().Get() != Owned->Subsystem.Get()) { Error = TEXT("Original AnimNodePrePostRuntimePose registry subsystem identity changed; no retargeting"); return false; }
    const bool Ok = Ready(Owned.ToSharedRef(), Report, Error) && Owned->Active.Load();
    Report->SetBoolField(TEXT("ready"), Ok); Report->SetBoolField(TEXT("verified"), Ok); Report->SetBoolField(TEXT("active"), Owned->Active.Load());
    return Ok;
}

bool RemoveAnimNodePrePostRuntimePoseToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { Error = TEXT("AnimNodePrePostRuntimePose guard removal requires an idle editor game thread; no foreign entry removed"); return false; }
    FBusyScope Busy;
    if (Owned.IsValid() && Owned->Class.Get() != ToolsetClass) { Error = TEXT("Exact retained AnimNodePrePostRuntimePose class owner required for removal"); return false; }
    const bool Ok = Cleanup(Report, Error); Report->SetBoolField(TEXT("removed"), Ok); return Ok;
}

void ShutdownAnimNodePrePostRuntimePoseToolset()
{
    if (Owned.IsValid()) { Owned->Active.Store(false); }
    if (!IsInGameThread() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { return; }
    FBusyScope Busy; auto Report = MakeShared<FJsonObject>(); FString Error;
    Cleanup(Report, Error);
}
}
