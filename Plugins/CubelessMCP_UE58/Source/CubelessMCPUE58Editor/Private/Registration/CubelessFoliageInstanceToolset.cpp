#include "Registration/CubelessFoliageInstanceToolset.h"
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
constexpr const TCHAR* CanonicalName = TEXT("cubeless_mcp_ue58.registration.foliage_instances_toolset.CubelessFoliageInstanceTools");
constexpr const TCHAR* CanonicalClass = TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/foliage_instances_toolset_PY.CubelessFoliageInstanceTools");
constexpr const TCHAR* Methods[]={TEXT("list_foliage_types"),TEXT("add_foliage_instances"),TEXT("remove_foliage_instances")};
constexpr const TCHAR* AddFields[]={TEXT("foliage_type_path"),TEXT("transforms")};
constexpr const TCHAR* RemoveFields[]={TEXT("foliage_type_path"),TEXT("instance_indices"),TEXT("remove_all"),TEXT("max_remove_count")};
struct FLayout{const TCHAR* const* Fields;int32 Count;};
constexpr FLayout Layouts[]={{nullptr,0},{AddFields,2},{RemoveFields,4}};
constexpr int32 MaxInputChars=8*1024*1024,MaxCarrierChars=1024*1024,MaxRawDepth=65,MaxValueSlots=262144;
int32 MethodIndex(const FString& Method){for(int32 I=0;I<3;++I){if(Method.Equals(Methods[I],ESearchCase::CaseSensitive)){return I;}}return INDEX_NONE;}
bool BoundText(const FString& Text,int32 Maximum)
{if(Text.Len()>Maximum){return false;}for(TCHAR C:Text){if(C==0){return false;}}return true;}
// Wrapper parsing depth bound65 is separate from the Native carrier bound16.
bool RawDepth(const FString& Text)
{
    int32 Depth=0;bool Quoted=false,Escaped=false;
    for(TCHAR C:Text)
    {
        if(Quoted){if(Escaped){Escaped=false;}else if(C==TEXT('\\')){Escaped=true;}else if(C==TEXT('"')){Quoted=false;}continue;}
        if(C==TEXT('"')){Quoted=true;}
        else if(C==TEXT('[')||C==TEXT('{')){if(++Depth>MaxRawDepth){return false;}}
        else if(C==TEXT(']')||C==TEXT('}')){if(--Depth<0){return false;}}
    }
    return !Quoted&&Depth==0;
}
bool FiniteValues(const TSharedRef<FJsonObject>& Args)
{
    TArray<TSharedPtr<FJsonValue>> Pending;for(const auto& Pair:Args->Values){Pending.Add(Pair.Value);}
    int32 Slots=0;
    while(!Pending.IsEmpty())
    {
        if(++Slots>MaxValueSlots){return false;}const auto Value=Pending.Pop();if(!Value.IsValid()){return false;}
        if(Value->Type==EJson::Number&&!FMath::IsFinite(Value->AsNumber())){return false;}
        if(Value->Type==EJson::Array){Pending.Append(Value->AsArray());}
        else if(Value->Type==EJson::Object){for(const auto& Pair:Value->AsObject()->Values){Pending.Add(Pair.Value);}}
    }
    return true;
}
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
    TAtomic<int32> DelegatedMethodIndex { INDEX_NONE };
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
    R->SetStringField(TEXT("stage"), TEXT("foliage_instances_raw_pre_dispatch"));
    R->SetStringField(TEXT("error_code"), Code);
    R->SetStringField(TEXT("error"), Message);
    R->SetStringField(TEXT("changes_json"), TEXT("[]"));
    auto Compile = MakeShared<FJsonObject>();
    Compile->SetBoolField(TEXT("requested"), false); Compile->SetBoolField(TEXT("attempted"), false);
    Compile->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    Compile->SetStringField(TEXT("status"), TEXT("foliage_instances_raw_not_dispatched"));
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

bool RawTransform(int32 Which,const FString& Input,FString& PackedInput,FString& Error)
{
    TSharedPtr<FJsonObject> Args;
    if(Which<0||Which>=3||!BoundText(Input,MaxInputChars)||!RawDepth(Input)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input),Args,FJsonSerializer::EFlags::StoreNumbersAsStrings)||!Args.IsValid())
    {Error=TEXT("Foliage requires bounded raw JSON object");return false;}
    const auto& Layout=Layouts[Which];
    for(const auto& Pair:Args->Values){const FString Key(Pair.Key.ToView());bool Allowed=false;for(int32 I=0;I<Layout.Count;++I){if(Key.Equals(Layout.Fields[I],ESearchCase::CaseSensitive)){Allowed=true;break;}}if(!Allowed){Error=TEXT("Unexpected public foliage input; raw command context is Batch-only");return false;}}
    if(!FiniteValues(Args.ToSharedRef())){Error=TEXT("Finite bounded JSON values required");return false;}
    if(Which>0){auto Path=Args->TryGetField(TEXT("foliage_type_path"));if(!Path.IsValid()||Path->Type!=EJson::String||!BoundText(Path->AsString(),MaxCarrierChars)){Error=TEXT("Required exact bounded foliage_type_path string");return false;}}
    if(Which==1||Which==2){
        const TCHAR* Key=Which==1?TEXT("transforms"):TEXT("instance_indices");auto Value=Args->TryGetField(Key);
        if(!Value.IsValid()&&Which==2){Value=MakeShared<FJsonValueNull>();}
        if(!Value.IsValid()||(Value->Type!=EJson::Array&&!(Which==2&&Value->Type==EJson::Null))){Error=TEXT("Original transforms array or nullable indices array required");return false;}
        if(Which==2&&Value->Type==EJson::Array){for(const auto& Index:Value->AsArray()){FString Decimal;if(!IntegralDecimal(Index,Decimal)){Error=TEXT("Public instance_indices requires exact integers, not bool");return false;}}}
        FString Carrier;auto Writer=TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Carrier);
        if(!FJsonSerializer::Serialize(Value.ToSharedRef(),TEXT(""),Writer,false)){Error=TEXT("Foliage JSON carrier serialization failed");return false;}
        const bool Closed=Writer->Close();TArray<TSharedPtr<FJsonValue>> Back;
        if((!Closed&&Value->Type!=EJson::Null)||!BoundText(Carrier,MaxCarrierChars)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+Carrier+TEXT("]")),Back)||Back.Num()!=1||Back[0]->Type!=Value->Type){Error=TEXT("Foliage carrier readback/bound failed");return false;}
        Args->SetStringField(Key,Carrier);
    }
    if(Which==2){
        auto All=Args->TryGetField(TEXT("remove_all"));if(All.IsValid()&&All->Type!=EJson::Boolean){Error=TEXT("Public remove_all requires exact JSON Boolean");return false;}if(!All.IsValid()){Args->SetBoolField(TEXT("remove_all"),false);}
        auto Max=Args->TryGetField(TEXT("max_remove_count"));FString Decimal=TEXT("5000");if(Max.IsValid()&&!IntegralDecimal(Max,Decimal)){Error=TEXT("Public max_remove_count requires exact int64 integer");return false;}Args->SetStringField(TEXT("max_remove_count"),Decimal);
    }
    PackedInput=Json(Args.ToSharedRef());return true;
}

bool Metadata(UClass* Class,FString& Error)
{
    if(!IsValid(Class)||Class->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed)||Class->HasAnyClassFlags(CLASS_NewerVersionExists)||Class->GetPathName()!=CanonicalClass||!Class->IsChildOf(UToolsetDefinition::StaticClass())){Error=TEXT("Exact live canonical Foliage class required");return false;}
    for(int32 Which=0;Which<3;++Which){
        UFunction* Function=Class->FindFunctionByName(FName(Methods[Which]));if(!Function||Function->GetOuter()!=Class||Function->GetName()!=Methods[Which]){Error=TEXT("Exact owned foliage method required");return false;}
        auto Callable=UToolsetDefinition::IsFunctionAICallable(TObjectPtr<const UFunction>(Function));if(!Callable.HasValue()||!Callable.GetValue()){Error=TEXT("Officially callable foliage method required");return false;}
        const auto& Layout=Layouts[Which];int32 Index=0;
        for(TFieldIterator<FProperty> It(Function,EFieldIteratorFlags::ExcludeSuper);It;++It){if(!It->HasAnyPropertyFlags(CPF_Parm)||It->HasAnyPropertyFlags(CPF_ReturnParm)){continue;}
            const bool Boolean=Which==2&&Index==2;
            if(Index>=Layout.Count||It->GetName()!=Layout.Fields[Index]||It->GetOwnerStruct()!=Function||It->ArrayDim!=1||It->HasAnyPropertyFlags(CPF_OutParm|CPF_ReferenceParm)||(Boolean?!CastField<FBoolProperty>(*It):!CastField<FStrProperty>(*It))){Error=TEXT("Exact foliage physical0/2/4 ordered fields required");return false;}++Index;}
        if(Index!=Layout.Count){Error=TEXT("Exact foliage physical0/2/4 arity required");return false;}
        for(int32 I=0;I<(Which==1?2:Which==2?1:0);++I){if(Function->HasMetaData(FName(*(FString(TEXT("CPP_Default_"))+Layout.Fields[I])))){Error=TEXT("Required foliage input has reflected default");return false;}}
        if(Which==2){if(Function->GetMetaData(TEXT("CPP_Default_instance_indices"))!=TEXT("null")||Function->GetMetaData(TEXT("CPP_Default_max_remove_count"))!=TEXT("5000")){Error=TEXT("Exact private nullable/int64 defaults required");return false;}}
    }
    return true;
}

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

bool Output11(const TSharedPtr<FJsonObject>& Tool)
{
    const TSharedPtr<FJsonObject>* Output=nullptr;const TSharedPtr<FJsonObject>* Props=nullptr;const TSharedPtr<FJsonObject>* Return=nullptr;const TSharedPtr<FJsonObject>* Fields=nullptr;
    const TSet<FString> Names {TEXT("success"),TEXT("operation_id"),TEXT("stage"),TEXT("error_code"),TEXT("error"),TEXT("changes_json"),TEXT("compile_json"),TEXT("save_json"),TEXT("validation_json"),TEXT("retry_json"),TEXT("data_json")};
    if(!Tool->TryGetObjectField(TEXT("outputSchema"),Output)||!Type(*Output,TEXT("object"))||!Required(*Output,{TEXT("returnValue")})
        ||!(*Output)->TryGetObjectField(TEXT("properties"),Props)||(*Props)->Values.Num()!=1||!(*Props)->TryGetObjectField(TEXT("returnValue"),Return)
        ||!Type(*Return,TEXT("object"))||!Required(*Return,Names)||!(*Return)->TryGetObjectField(TEXT("properties"),Fields)||(*Fields)->Values.Num()!=Names.Num()){return false;}
    for(const FString& Name:Names){const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Fields)->TryGetObjectField(Name,Field)||!Type(*Field,Name==TEXT("success")?TEXT("boolean"):TEXT("string"))){return false;}}
    return true;
}
bool FoliageInstanceSchema(const FString& Physical,FString& Logical,FString& Error)
{
    TSharedPtr<FJsonObject> Root;const TArray<TSharedPtr<FJsonValue>>* Tools=nullptr;FString Name;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Physical),Root)||!Root.IsValid()||!Root->TryGetStringField(TEXT("name"),Name)||Name!=CanonicalName||!Root->TryGetArrayField(TEXT("tools"),Tools)||!Tools||Tools->Num()!=3){Error=TEXT("Exact canonical foliage Toolset3 schema required");return false;}
    TSet<int32> Seen;
    for(const auto& Value:*Tools){
        if(!Value.IsValid()||Value->Type!=EJson::Object){return false;}const auto Tool=Value->AsObject();if(!Tool->TryGetStringField(TEXT("name"),Name)){return false;}
        int32 Which=INDEX_NONE;for(int32 I=0;I<3;++I){if(Name==FString(CanonicalName)+TEXT(".")+Methods[I]){Which=I;}}
        if(Which==INDEX_NONE||Seen.Contains(Which)){Error=TEXT("Only three exact foliage methods admitted");return false;}Seen.Add(Which);
        const auto& Layout=Layouts[Which];const TSharedPtr<FJsonObject>* Inputs=nullptr;const TSharedPtr<FJsonObject>* Props=nullptr;
        if(!Tool->TryGetObjectField(TEXT("inputSchema"),Inputs)||!Type(*Inputs,TEXT("object"))){Error=TEXT("Exact foliage object input required");return false;}
        if(Which==0){
            // Installed JsonSchemaGenerator334-348 omits properties/required for zero input.
            const TArray<TSharedPtr<FJsonValue>>* RequiredValues=nullptr;
            if(((*Inputs)->HasField(TEXT("properties"))&&(!(*Inputs)->TryGetObjectField(TEXT("properties"),Props)||(*Props)->Values.Num()!=0))||((*Inputs)->HasField(TEXT("required"))&&(!(*Inputs)->TryGetArrayField(TEXT("required"),RequiredValues)||RequiredValues->Num()!=0))){Error=TEXT("Zero-input observer schema has inputs");return false;}
            (*Inputs)->SetObjectField(TEXT("properties"),MakeShared<FJsonObject>());(*Inputs)->SetArrayField(TEXT("required"),{});
            (*Inputs)->TryGetObjectField(TEXT("properties"),Props);
        }else if(!(*Inputs)->TryGetObjectField(TEXT("properties"),Props)||(*Props)->Values.Num()!=Layout.Count||!(Which==1?Required(*Inputs,{TEXT("foliage_type_path"),TEXT("transforms")}):Required(*Inputs,{TEXT("foliage_type_path")}))){Error=TEXT("Exact physical2/4 and required2/1 fields required");return false;}
        for(int32 I=0;I<Layout.Count;++I){
            const TCHAR* Key=Layout.Fields[I];const TSharedPtr<FJsonObject>* Field=nullptr;const bool Boolean=Which==2&&I==2;
            if(!(*Props)->TryGetObjectField(Key,Field)||!Type(*Field,Boolean?TEXT("boolean"):TEXT("string"))){Error=TEXT("Physical foliage field type differs");return false;}
            auto Default=(*Field)->TryGetField(TEXT("default"));const bool RequiredField=Which==1||I==0;
            if(RequiredField){if(Default.IsValid()){Error=TEXT("Required foliage field has default");return false;}}
            else if(I==1){if(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=TEXT("null")){Error=TEXT("Nullable indices private default differs");return false;}}
            else if(I==2){if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()){Error=TEXT("remove_all false default differs");return false;}}
            else if(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=TEXT("5000")){Error=TEXT("max_remove_count private decimal default differs");return false;}
            if((Which==1&&I==1)||(Which==2&&I==1)){
                auto Item=MakeShared<FJsonObject>();Item->SetStringField(TEXT("type"),Which==1?TEXT("object"):TEXT("integer"));(*Field)->SetObjectField(TEXT("items"),Item);
                if(Which==1){(*Field)->SetStringField(TEXT("type"),TEXT("array"));}
                else{(*Field)->SetArrayField(TEXT("type"),{MakeShared<FJsonValueString>(TEXT("array")),MakeShared<FJsonValueString>(TEXT("null"))});(*Field)->SetField(TEXT("default"),MakeShared<FJsonValueNull>());}
            }
            if(Which==2&&I==3){(*Field)->SetStringField(TEXT("type"),TEXT("integer"));(*Field)->SetNumberField(TEXT("default"),5000);}
        }
        (*Inputs)->SetBoolField(TEXT("additionalProperties"),false);
        if(!Output11(Tool)){Error=TEXT("Exact standard11 output required for all foliage methods");return false;}
    }
    Logical=Json(Root.ToSharedRef());return true;
}

class FFoliageInstanceToolset final : public FToolset
{
public:
    explicit FFoliageInstanceToolset(const TSharedRef<FAttachmentState>& InState) : State(InState) {}
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
        { return Reject(TEXT("foliage_instances_raw_guard_unavailable"), TEXT("A live active game-thread FoliageInstance guard is required; no delegate dispatched")); }
        UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get();
        if (!Subsystem || Subsystem->ToolsetRegistry.Find(CanonicalName).Get() != this)
        { return Reject(TEXT("foliage_instances_raw_guard_ownership_lost"), TEXT("Exact original-subsystem wrapper identity is unavailable; no delegate dispatched")); }
        FString Error;
        if (!Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
        { State->Active.Store(false); State->Original->SetEnabled(false); return Reject(TEXT("foliage_instances_raw_guard_definition_changed"), Error); }
        const int32 Which=MethodIndex(Method);
        if (Which==INDEX_NONE)
        { return Reject(TEXT("foliage_instances_method_refused"), TEXT("Only the three exact original foliage methods are guarded")); }
        FString PackedInput;
        if (!RawTransform(Which, Input, PackedInput, Error))
        { return Reject(TEXT("foliage_instances_raw_arguments_invalid"), Error); }
        const TSharedRef<FAttachmentState> CallState = State;
        const TSharedPtr<FToolset> OriginalLifetime = State->Original;
        const TSharedPtr<FToolset> WrapperLifetime = State->Wrapper;
        State->DelegatedMethodIndex.Store(INDEX_NONE);
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
            State->DelegatedMethodIndex.Store(INDEX_NONE); State->InFlight.Store(0);
            if (Error.IsEmpty()) { Error = TEXT("FoliageInstance ownership/filter/class changed during local filter callbacks; no delegate dispatched"); }
            return Reject(TEXT("foliage_instances_raw_guard_changed_before_dispatch"), Error);
        }
        // Only the actual guarded Named method delegation may ask Verify for its exact admission marker.
        State->DelegatedMethodIndex.Store(Which);
        // Direct public delegation avoids recursion through the same canonical Registry name.
        return OriginalLifetime->ExecuteTool(Method, PackedInput).Next(
            [CallState, OriginalLifetime, WrapperLifetime](FOutput&& Output) -> FOutput
            {
                // Official reflected handler completions run on the game thread; no UObject work here.
                if (IsInGameThread()) { OriginalLifetime->SetEnabled(false); }
                else { CallState->Active.Store(false); }
                CallState->DelegatedMethodIndex.Store(INDEX_NONE); CallState->InFlight.Store(0);
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
    Report->SetStringField(TEXT("guard_scope"),TEXT("Current Editor world/current-level IFA foliage list/add/remove original public0/2/4. List creates no actor. Add retains per-item parse/write order and actual partial registration/instance prefix. Remove preserves no-op/dedup/range/cap behavior and swapped-index risk. Canonical Named normalization is distinct from raw legacy remove; actual Native Json conversion preserves locale and raw presence through an internal compatibility context. Exact finite coordinate guards, LevelPartition-only scope and safe int32 conversion limits are explicit. Native refresh/remove may start async HISM work; no game-thread render wait, explicit compile/save, uncertain retry or atomic rollback."));
    for(const TCHAR* Key : {TEXT("active"),TEXT("raw_guard_installed"),TEXT("default_verified"),TEXT("reflected_carrier_schema_verified"),TEXT("logical_schema_verified"),TEXT("output11_schema_preserved"),TEXT("ordinary_path2_schema_verified"),TEXT("array2_schema_verified"),TEXT("nullable_indices_schema_verified"),TEXT("boolean1_schema_verified"),TEXT("integer1_schema_verified"),TEXT("foliage_public0_2_4_schema_verified"),TEXT("required0_2_1_default0_0_3_verified")}){Report->SetBoolField(Key,false);}
    Report->SetNumberField(TEXT("delegated_calls_in_flight"), Owned.IsValid() ? Owned->InFlight.Load() : 0);
}

bool Ready(const TSharedRef<FAttachmentState>& State, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get();
    if (!Subsystem || !State->Wrapper.IsValid() || !State->Original.IsValid())
    { Error = TEXT("Captured FoliageInstance guard owner/subsystem unavailable"); return false; }
    // Temporarily hide dispatch while probing a retained disabled object without changing global filters.
    const bool WasActive = State->Active.Load(); State->Active.Store(false);
    const bool Identical = HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper);
    State->Active.Store(WasActive && Identical);
    Report->SetBoolField(TEXT("guard_attached"), Identical);
    Report->SetBoolField(TEXT("original_handler_detached"), State->OriginalDetached);
    Report->SetBoolField(TEXT("class_unregister_allowed"), false);
    Report->SetBoolField(TEXT("cleanup_required"), true);
    if (!Identical) { State->Original->SetEnabled(false); Error = TEXT("Captured FoliageInstance wrapper no longer owns its canonical registry entry; foreign entry preserved"); return false; }
    if (!Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get()) { State->Active.Store(false); State->Original->SetEnabled(false); return false; }
    const FString CurrentSchema = UnfilteredOriginalSchema(State);
    FString CurrentLogical;
    Report->SetStringField(TEXT("reflected_schema_json"), CurrentSchema);
    if (!FoliageInstanceSchema(CurrentSchema, CurrentLogical, Error) || CurrentSchema != State->ReflectedSchema || CurrentLogical != State->FullSchema)
    { State->Active.Store(false); State->Original->SetEnabled(false); if (Error.IsEmpty()) { Error = TEXT("Original FoliageInstance schema changed; no automatic replacement or dispatch"); } return false; }
    auto CurrentSubsystem = UToolsetRegistrySubsystem::Get();
    if (!CurrentSubsystem.HasValue() || CurrentSubsystem.GetValue().Get() != State->Subsystem.Get()
        || !HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper)
        || !Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
    { State->Active.Store(false); State->Original->SetEnabled(false); if (Error.IsEmpty()) { Error = TEXT("FoliageInstance wrapper owner/class changed during reflected schema callbacks"); } return false; }
    for(const TCHAR* Key : {TEXT("raw_guard_installed"),TEXT("default_verified"),TEXT("reflected_carrier_schema_verified"),TEXT("logical_schema_verified"),TEXT("output11_schema_preserved"),TEXT("ordinary_path2_schema_verified"),TEXT("array2_schema_verified"),TEXT("nullable_indices_schema_verified"),TEXT("boolean1_schema_verified"),TEXT("integer1_schema_verified"),TEXT("foliage_public0_2_4_schema_verified"),TEXT("required0_2_1_default0_0_3_verified")}){Report->SetBoolField(Key,true);}
    Report->SetBoolField(TEXT("toolset_enabled"), State->Wrapper->IsEnabled());
    if (!State->Wrapper->IsEnabled()) { Error = TEXT("FoliageInstance guard is installed but disabled by existing toolset state or user filters"); return false; }
    // The public Python class getter constructs a new physical FunctionLibraryToolset.
    // Read the actual registered wrapper instead, retaining current user filters.
    const TSharedPtr<FToolset> Registered = Subsystem->ToolsetRegistry.Find(CanonicalName);
    if (Registered != State->Wrapper)
    { State->Active.Store(false); Error = TEXT("Current registered FoliageInstance wrapper identity is unavailable for logical schema receipt"); return false; }
    const FString RegisteredLogicalSchema = Registered->GetJsonSchema();
    auto AfterSchemaSubsystem = UToolsetRegistrySubsystem::Get();
    if (RegisteredLogicalSchema.IsEmpty() || !AfterSchemaSubsystem.HasValue()
        || AfterSchemaSubsystem.GetValue().Get() != State->Subsystem.Get()
        || Subsystem->ToolsetRegistry.Find(CanonicalName) != Registered
        || !Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
    { State->Active.Store(false); State->Original->SetEnabled(false);
      if (Error.IsEmpty()) { Error = TEXT("Current registered FoliageInstance logical schema/owner/class receipt could not be confirmed"); }
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
        if (Remains) { Report->SetBoolField(TEXT("cleanup_required"), true); Report->SetBoolField(TEXT("registry_changed"), Changed); Error = TEXT("Owned FoliageInstance guard cleanup was not confirmed; inactive retained owner preserved"); return false; }
    }
    // Detached/cached wrappers are inactive, so they cannot dispatch after this cycle is broken.
    State->Wrapper.Reset(); State->Original.Reset(); Owned.Reset();
    Report->SetBoolField(TEXT("registry_changed"), Changed); Report->SetBoolField(TEXT("cleanup_required"), false);
    Report->SetBoolField(TEXT("class_unregister_allowed"), false); Report->SetBoolField(TEXT("owned_handler_removed"), true);
    Report->SetBoolField(TEXT("foreign_handler_preserved"), true);
    return true;
}
}

bool InstallFoliageInstanceToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { Error = TEXT("FoliageInstance guard installation requires an idle live editor game thread; no attachment retried"); return false; }
    FBusyScope Busy;
    if (!Metadata(ToolsetClass, Error)) { return false; }
    auto Current = UToolsetRegistrySubsystem::Get();
    if (!Current.HasValue()) { Error = TEXT("Original official Toolset Registry subsystem unavailable"); return false; }
    if (Owned.IsValid())
    {
        if (Owned->Subsystem.Get() != Current.GetValue().Get() || Owned->Class.Get() != ToolsetClass)
        { Error = TEXT("Different retained FoliageInstance owner/subsystem/class exists; explicit cleanup required, no automatic reattachment"); return false; }
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
    if (!FoliageInstanceSchema(State->ReflectedSchema, State->FullSchema, Error)) { Original->SetEnabled(false); return false; }
    State->Wrapper = MakeShared<FFoliageInstanceToolset>(State);
    // Exact identity is checked immediately before SDK's name-only removal, with dispatch inactive.
    if (!HasRetainedToolsetIdentity(Current.GetValue()->ToolsetRegistry, Original)
        || !Current.GetValue()->ToolsetRegistry.UnregisterToolset(Original))
    { Error = TEXT("Original FoliageInstance handler detach failed or identity changed; captured owner remains inactive for explicit cleanup"); return false; }
    State->OriginalDetached = true; Report->SetBoolField(TEXT("registry_changed"), true); Report->SetBoolField(TEXT("original_handler_detached"), true);
    // Register broadcasts callbacks. Reentry is busy-rejected and wrapper dispatch remains inactive.
    if (!Current.GetValue()->ToolsetRegistry.RegisterToolset(State->Wrapper))
    { Error = TEXT("FoliageInstance raw wrapper registration failed or collided; no unguarded handler restored, inspect actual registry before cleanup"); return false; }
    if (!Ready(State, Report, Error)) { return false; }
    State->Active.Store(true); State->Original->SetEnabled(false);
    Report->SetBoolField(TEXT("ready"), true); Report->SetBoolField(TEXT("verified"), true);
    Report->SetBoolField(TEXT("active"), true);
    // The registration event already exposed the immutable complete schema; activation changes no catalogue.
    Report->SetBoolField(TEXT("extra_catalogue_broadcast_required"), false);
    return true;
}

bool VerifyFoliageInstanceToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy) { Error = TEXT("FoliageInstance guard verification requires an idle live editor game thread"); return false; }
    if (Owned.IsValid() && Owned->InFlight.Load() != 0)
    {
        auto Current = UToolsetRegistrySubsystem::Get();
        const int32 DelegatedIndex=Owned->DelegatedMethodIndex.Load();
        const bool Admitted = Owned->InFlight.Load() == 1 && DelegatedIndex>=0 && DelegatedIndex<3 && Owned->Active.Load()
            && Owned->Class.Get() == ToolsetClass && Current.HasValue()
            && Current.GetValue().Get() == Owned->Subsystem.Get() && Owned->Wrapper.IsValid()
            && Current.GetValue()->ToolsetRegistry.Find(CanonicalName) == Owned->Wrapper;
        Report->SetBoolField(TEXT("delegated_call_admitted"), Admitted);
        Report->SetBoolField(TEXT("delegated_admission_only"), true);
        Report->SetStringField(TEXT("delegated_method"), Admitted ? Methods[DelegatedIndex] : TEXT(""));
        Report->SetBoolField(TEXT("active"), Admitted); Report->SetBoolField(TEXT("guard_attached"), Admitted);
        // This is only the current wrapper's admission check. No schema/metadata callbacks,
        // install/remove/asset work, or general readiness proof is performed here.
        // False intentionally keeps Native helper ready/verified=False even when admitted.
        if (!Admitted) { Error = TEXT("Only the current exact guarded Named Foliage instance delegation is admitted"); }
        return false;
    }
    FBusyScope Busy;
    if (!Owned.IsValid() || Owned->Class.Get() != ToolsetClass) { Error = TEXT("Exact installed FoliageInstance raw guard required; verification never installs one"); return false; }
    auto Current = UToolsetRegistrySubsystem::Get();
    if (!Current.HasValue() || Current.GetValue().Get() != Owned->Subsystem.Get()) { Error = TEXT("Original FoliageInstance registry subsystem identity changed; no retargeting"); return false; }
    const bool Ok = Ready(Owned.ToSharedRef(), Report, Error) && Owned->Active.Load();
    Report->SetBoolField(TEXT("ready"), Ok); Report->SetBoolField(TEXT("verified"), Ok); Report->SetBoolField(TEXT("active"), Owned->Active.Load());
    return Ok;
}

bool RemoveFoliageInstanceToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { Error = TEXT("FoliageInstance guard removal requires an idle editor game thread; no foreign entry removed"); return false; }
    FBusyScope Busy;
    if (Owned.IsValid() && Owned->Class.Get() != ToolsetClass) { Error = TEXT("Exact retained FoliageInstance class owner required for removal"); return false; }
    const bool Ok = Cleanup(Report, Error); Report->SetBoolField(TEXT("removed"), Ok); return Ok;
}

void ShutdownFoliageInstanceToolset()
{
    if (Owned.IsValid()) { Owned->Active.Store(false); }
    if (!IsInGameThread() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { return; }
    FBusyScope Busy; auto Report = MakeShared<FJsonObject>(); FString Error;
    Cleanup(Report, Error);
}
}
