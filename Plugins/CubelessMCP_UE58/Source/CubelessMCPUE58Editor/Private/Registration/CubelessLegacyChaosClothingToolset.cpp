#include "Registration/CubelessLegacyChaosClothingToolset.h"
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
constexpr const TCHAR* CanonicalName = TEXT("cubeless_mcp_ue58.registration.legacy_chaos_clothing_toolset.CubelessLegacyChaosClothingTools");
constexpr const TCHAR* CanonicalClass = TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/legacy_chaos_clothing_toolset_PY.CubelessLegacyChaosClothingTools");
constexpr const TCHAR* Methods[]={TEXT("create_legacy_chaos_clothing_from_skeletal_sections")};
constexpr const TCHAR* Fields0[]={TEXT("source_skeletal_mesh_path"),TEXT("output_skeletal_mesh_path"),TEXT("section_configs"),TEXT("dry_run"),TEXT("save_assets"),TEXT("use_source_physics_asset"),TEXT("create_vroid_lower_body_cloth_physics_asset")};
struct FLayout{const TCHAR* const* Fields;int32 Count;};
constexpr FLayout Layouts[]={{Fields0,7}};
constexpr int32 MaxInputChars=8*1024*1024,MaxCarrierChars=1024*1024,MaxRawDepth=65,MaxValueSlots=262144;
int32 MethodIndex(const FString& Method){for(int32 I=0;I<1;++I){if(Method.Equals(Methods[I],ESearchCase::CaseSensitive)){return I;}}return INDEX_NONE;}
constexpr const TCHAR* LogicalInputSchemas[]={TEXT("{\"properties\":{\"source_skeletal_mesh_path\":{\"title\":\"Source Skeletal Mesh Path\",\"type\":\"string\"},\"output_skeletal_mesh_path\":{\"title\":\"Output Skeletal Mesh Path\",\"type\":\"string\"},\"section_configs\":{\"items\":{\"type\":\"object\"},\"title\":\"Section Configs\",\"type\":\"array\"},\"dry_run\":{\"default\":true,\"title\":\"Dry Run\",\"type\":\"boolean\"},\"save_assets\":{\"default\":true,\"title\":\"Save Assets\",\"type\":\"boolean\"},\"use_source_physics_asset\":{\"default\":true,\"title\":\"Use Source Physics Asset\",\"type\":\"boolean\"},\"create_vroid_lower_body_cloth_physics_asset\":{\"default\":false,\"title\":\"Create Vroid Lower Body Cloth Physics Asset\",\"type\":\"boolean\"}},\"required\":[\"source_skeletal_mesh_path\",\"output_skeletal_mesh_path\",\"section_configs\"],\"title\":\"create_legacy_chaos_clothing_from_skeletal_sectionsArguments\",\"type\":\"object\"}")};
bool BoundText(const FString& Text,int32 Maximum)
{if(Text.Len()>Maximum){return false;}for(TCHAR C:Text){if(C==0){return false;}}return true;}
// Wrapper parsing and Native carrier parsing have separate bounds; neither permits arbitrary nested payloads.
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
    R->SetStringField(TEXT("stage"), TEXT("legacy_chaos_clothing_raw_pre_dispatch"));
    R->SetStringField(TEXT("error_code"), Code);
    R->SetStringField(TEXT("error"), Message);
    R->SetStringField(TEXT("changes_json"), TEXT("[]"));
    auto Compile = MakeShared<FJsonObject>();
    Compile->SetBoolField(TEXT("requested"), false); Compile->SetBoolField(TEXT("attempted"), false);
    Compile->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    Compile->SetStringField(TEXT("status"), TEXT("legacy_chaos_clothing_raw_not_dispatched"));
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

// Public JSON FString carriers include scalars. Close() requires a completed object/array,
// so use the installed public one-element array overload and remove only its outer brackets.
// StoreNumbersAsStrings + PreferStringRepresentation preserve original numeric lexemes.
bool SerializePublicValue(const TSharedPtr<FJsonValue>& Value,FString& Carrier,FString& Error)
{
    if(!Value.IsValid()){Error=TEXT("Public JSON value is invalid");return false;}
    const TArray<TSharedPtr<FJsonValue>> Envelope{Value};FString EnvelopeJson;
    auto Writer=TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&EnvelopeJson);
    if(!FJsonSerializer::Serialize(Envelope,Writer)||EnvelopeJson.Len()<2||EnvelopeJson[0]!=TEXT('[')||EnvelopeJson[EnvelopeJson.Len()-1]!=TEXT(']')){Error=TEXT("Public JSON value carrier serialization failed");return false;}
    Carrier=EnvelopeJson.Mid(1,EnvelopeJson.Len()-2);TArray<TSharedPtr<FJsonValue>> Back;
    if(!BoundText(Carrier,MaxCarrierChars)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(EnvelopeJson),Back,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Back.Num()!=1||!Back[0].IsValid()||Back[0]->Type!=Value->Type){Error=TEXT("Public JSON carrier bound/readback failed");return false;}
    return true;
}

bool RawTransform(int32 Which,const FString& Input,FString& PackedInput,FString& Error)
{
    TSharedPtr<FJsonObject> Args;
    if(Which<0||Which>=1||!BoundText(Input,MaxInputChars)||!RawDepth(Input)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input),Args,FJsonSerializer::EFlags::StoreNumbersAsStrings)||!Args.IsValid()||!FiniteValues(Args.ToSharedRef())){Error=TEXT("Legacy clothing requires bounded finite raw object");return false;}
    auto Packed=MakeShared<FJsonObject>();
    // Original public FastMCP arg_model ignores extra fields; no hidden public carrier fields are added.
    // Select known fields with exact JsonObject key lookup; legacy raw bypasses this wrapper.
    if(Which==0){
        {auto Value=Args->TryGetField(TEXT("source_skeletal_mesh_path"));if(!Value.IsValid()){Error=TEXT("Required public field missing");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("source_skeletal_mesh_path"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("output_skeletal_mesh_path"));if(!Value.IsValid()){Error=TEXT("Required public field missing");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("output_skeletal_mesh_path"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("section_configs"));if(!Value.IsValid()){Error=TEXT("Required public field missing");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("section_configs"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("dry_run"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[true]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("dry_run"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("save_assets"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[true]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("save_assets"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("use_source_physics_asset"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[true]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("use_source_physics_asset"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("create_vroid_lower_body_cloth_physics_asset"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[false]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("create_vroid_lower_body_cloth_physics_asset"),Carrier);}
    }
    PackedInput=Json(Packed);return true;
}

bool Metadata(UClass* Class,FString& Error)
{
    if(!IsValid(Class)||Class->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed)||Class->HasAnyClassFlags(CLASS_NewerVersionExists)||!Class->GetPathName().Equals(CanonicalClass,ESearchCase::CaseSensitive)||!Class->IsChildOf(UToolsetDefinition::StaticClass())){Error=TEXT("Exact live canonical LegacyChaosClothing class required");return false;}
    for(int32 Which=0;Which<1;++Which){
        UFunction* Function=Class->FindFunctionByName(FName(Methods[Which]));if(!Function||Function->GetOuter()!=Class||!Function->GetName().Equals(Methods[Which],ESearchCase::CaseSensitive)){Error=TEXT("Exact owned LegacyChaosClothing method required");return false;}
        auto Callable=UToolsetDefinition::IsFunctionAICallable(TObjectPtr<const UFunction>(Function));if(!Callable.HasValue()||!Callable.GetValue()){Error=TEXT("Officially callable LegacyChaosClothing method required");return false;}
        const auto& Layout=Layouts[Which];int32 Index=0;
        for(TFieldIterator<FProperty> It(Function,EFieldIteratorFlags::ExcludeSuper);It;++It){if(!It->HasAnyPropertyFlags(CPF_Parm)||It->HasAnyPropertyFlags(CPF_ReturnParm)){continue;}if(Index>=Layout.Count||!It->GetName().Equals(Layout.Fields[Index],ESearchCase::CaseSensitive)||It->GetOwnerStruct()!=Function||It->ArrayDim!=1||It->HasAnyPropertyFlags(CPF_OutParm|CPF_ReferenceParm)||!CastField<FStrProperty>(*It)){Error=TEXT("Exact physical public JSON FString fields7 required");return false;}++Index;}
        if(Index!=Layout.Count){Error=TEXT("Exact physical arity required");return false;}
        if(Which==0){
            if(Function->HasMetaData(TEXT("CPP_Default_source_skeletal_mesh_path"))){Error=TEXT("Required public field has reflected default");return false;}
            if(Function->HasMetaData(TEXT("CPP_Default_output_skeletal_mesh_path"))){Error=TEXT("Required public field has reflected default");return false;}
            if(Function->HasMetaData(TEXT("CPP_Default_section_configs"))){Error=TEXT("Required public field has reflected default");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_dry_run"))||!Function->GetMetaData(TEXT("CPP_Default_dry_run")).Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_save_assets"))||!Function->GetMetaData(TEXT("CPP_Default_save_assets")).Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_use_source_physics_asset"))||!Function->GetMetaData(TEXT("CPP_Default_use_source_physics_asset")).Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_create_vroid_lower_body_cloth_physics_asset"))||!Function->GetMetaData(TEXT("CPP_Default_create_vroid_lower_body_cloth_physics_asset")).Equals(TEXT("false"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
        }
    }return true;
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
bool RequiredOrAbsentEmpty(const TSharedPtr<FJsonObject>& Input,const TSet<FString>& Expected)
{return Expected.IsEmpty()&&!Input->HasField(TEXT("required"))?true:Required(Input,Expected);}

bool LegacyChaosClothingSchema(const FString& Physical,FString& Logical,FString& Error)
{
    TSharedPtr<FJsonObject> Root;const TArray<TSharedPtr<FJsonValue>>* Tools=nullptr;FString Name;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Physical),Root)||!Root.IsValid()||!Root->TryGetStringField(TEXT("name"),Name)||!Name.Equals(CanonicalName,ESearchCase::CaseSensitive)||!Root->TryGetArrayField(TEXT("tools"),Tools)||!Tools||Tools->Num()!=1){Error=TEXT("Exact canonical LegacyChaosClothing Toolset1 required");return false;}
    TSet<int32> Seen;
    for(const auto& Value:*Tools){
        if(!Value.IsValid()||Value->Type!=EJson::Object){return false;}auto Tool=Value->AsObject();if(!Tool->TryGetStringField(TEXT("name"),Name)){return false;}
        int32 Which=INDEX_NONE;for(int32 I=0;I<1;++I){if(Name.Equals(FString(CanonicalName)+TEXT(".")+Methods[I],ESearchCase::CaseSensitive)){Which=I;}}
        if(Which==INDEX_NONE||Seen.Contains(Which)){Error=TEXT("Exactly one distinct legacy clothing method required");return false;}Seen.Add(Which);
        const TSharedPtr<FJsonObject>* Input=nullptr;const TSharedPtr<FJsonObject>* Props=nullptr;
        if(!Tool->TryGetObjectField(TEXT("inputSchema"),Input)||!Type(*Input,TEXT("object"))){return false;}
        if(!(*Input)->TryGetObjectField(TEXT("properties"),Props)||(*Props)->Values.Num()!=Layouts[Which].Count){Error=TEXT("Exact physical legacy clothing fields required");return false;}
        if(Which==0){if(!RequiredOrAbsentEmpty(*Input,{TEXT("source_skeletal_mesh_path"),TEXT("output_skeletal_mesh_path"),TEXT("section_configs")})){Error=TEXT("Physical required3 differs");return false;}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("source_skeletal_mesh_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("output_skeletal_mesh_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("section_configs"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("dry_run"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("save_assets"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("use_source_physics_asset"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("create_vroid_lower_body_cloth_physics_asset"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("false"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
        }
        TSharedPtr<FJsonObject> OriginalInput;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(LogicalInputSchemas[Which]),OriginalInput)||!OriginalInput.IsValid()){Error=TEXT("Frozen original input schema invalid");return false;}
        Tool->SetObjectField(TEXT("inputSchema"),OriginalInput.ToSharedRef());
        if(!Output11(Tool)){Error=TEXT("Exact standard11 output required for legacy clothing1");return false;}
    }Logical=Json(Root.ToSharedRef());return true;
}

class FLegacyChaosClothingToolset final : public FToolset
{
public:
    explicit FLegacyChaosClothingToolset(const TSharedRef<FAttachmentState>& InState) : State(InState) {}
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
        { return Reject(TEXT("legacy_chaos_clothing_raw_guard_unavailable"), TEXT("A live active game-thread LegacyChaosClothing guard is required; no delegate dispatched")); }
        UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get();
        if (!Subsystem || Subsystem->ToolsetRegistry.Find(CanonicalName).Get() != this)
        { return Reject(TEXT("legacy_chaos_clothing_raw_guard_ownership_lost"), TEXT("Exact original-subsystem wrapper identity is unavailable; no delegate dispatched")); }
        FString Error;
        if (!Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
        { State->Active.Store(false); State->Original->SetEnabled(false); return Reject(TEXT("legacy_chaos_clothing_raw_guard_definition_changed"), Error); }
        const int32 Which=MethodIndex(Method);
        if (Which==INDEX_NONE)
        { return Reject(TEXT("legacy_chaos_clothing_method_refused"), TEXT("Only the four exact original LegacyChaosClothing methods are guarded")); }
        FString PackedInput;
        if (!RawTransform(Which, Input, PackedInput, Error))
        { return Reject(TEXT("legacy_chaos_clothing_raw_arguments_invalid"), Error); }
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
            if (Error.IsEmpty()) { Error = TEXT("LegacyChaosClothing ownership/filter/class changed during local filter callbacks; no delegate dispatched"); }
            return Reject(TEXT("legacy_chaos_clothing_raw_guard_changed_before_dispatch"), Error);
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
    Report->SetStringField(TEXT("guard_scope"),TEXT("Create owned SkeletalMesh legacy Chaos clothing from1..8 sections after complete preflight. Preserve exact material slots, original binary PinGuide/local-Z fallback, optional source or VRoid PhysicsAsset, native factory/config/paint/bind/post-edit/readback, actual SDK save versus vacuous legacy save flags and partial owned cleanup. The source mesh is independently observed. No atomic rollback or uncertain retry; no Cubeless/MCP runtime class dependency. SDK synchronous build/cook/cache/cleanup modal limits remain explicit."));
    for(const TCHAR* Key : {TEXT("active"),TEXT("raw_guard_installed"),TEXT("default_verified"),TEXT("reflected_carrier_schema_verified"),TEXT("logical_schema_verified"),TEXT("output11_schema_preserved"),TEXT("ordinary_string2_schema_verified"),TEXT("nested_section_object_array1_schema_verified"),TEXT("boolean4_schema_verified"),TEXT("legacy_chaos_clothing_public7_schema_verified"),TEXT("required3_default4_verified"),TEXT("json_value_carrier7_schema_verified")}){Report->SetBoolField(Key,false);}
    Report->SetNumberField(TEXT("delegated_calls_in_flight"), Owned.IsValid() ? Owned->InFlight.Load() : 0);
}

bool Ready(const TSharedRef<FAttachmentState>& State, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get();
    if (!Subsystem || !State->Wrapper.IsValid() || !State->Original.IsValid())
    { Error = TEXT("Captured LegacyChaosClothing guard owner/subsystem unavailable"); return false; }
    // Temporarily hide dispatch while probing a retained disabled object without changing global filters.
    const bool WasActive = State->Active.Load(); State->Active.Store(false);
    const bool Identical = HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper);
    State->Active.Store(WasActive && Identical);
    Report->SetBoolField(TEXT("guard_attached"), Identical);
    Report->SetBoolField(TEXT("original_handler_detached"), State->OriginalDetached);
    Report->SetBoolField(TEXT("class_unregister_allowed"), false);
    Report->SetBoolField(TEXT("cleanup_required"), true);
    if (!Identical) { State->Original->SetEnabled(false); Error = TEXT("Captured LegacyChaosClothing wrapper no longer owns its canonical registry entry; foreign entry preserved"); return false; }
    if (!Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get()) { State->Active.Store(false); State->Original->SetEnabled(false); return false; }
    const FString CurrentSchema = UnfilteredOriginalSchema(State);
    FString CurrentLogical;
    Report->SetStringField(TEXT("reflected_schema_json"), CurrentSchema);
    if (!LegacyChaosClothingSchema(CurrentSchema, CurrentLogical, Error) || CurrentSchema != State->ReflectedSchema || CurrentLogical != State->FullSchema)
    { State->Active.Store(false); State->Original->SetEnabled(false); if (Error.IsEmpty()) { Error = TEXT("Original LegacyChaosClothing schema changed; no automatic replacement or dispatch"); } return false; }
    auto CurrentSubsystem = UToolsetRegistrySubsystem::Get();
    if (!CurrentSubsystem.HasValue() || CurrentSubsystem.GetValue().Get() != State->Subsystem.Get()
        || !HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper)
        || !Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
    { State->Active.Store(false); State->Original->SetEnabled(false); if (Error.IsEmpty()) { Error = TEXT("LegacyChaosClothing wrapper owner/class changed during reflected schema callbacks"); } return false; }
    for(const TCHAR* Key : {TEXT("raw_guard_installed"),TEXT("default_verified"),TEXT("reflected_carrier_schema_verified"),TEXT("logical_schema_verified"),TEXT("output11_schema_preserved"),TEXT("ordinary_string2_schema_verified"),TEXT("nested_section_object_array1_schema_verified"),TEXT("boolean4_schema_verified"),TEXT("legacy_chaos_clothing_public7_schema_verified"),TEXT("required3_default4_verified"),TEXT("json_value_carrier7_schema_verified")}){Report->SetBoolField(Key,true);}
    Report->SetBoolField(TEXT("toolset_enabled"), State->Wrapper->IsEnabled());
    if (!State->Wrapper->IsEnabled()) { Error = TEXT("LegacyChaosClothing guard is installed but disabled by existing toolset state or user filters"); return false; }
    // The public Python class getter constructs a new physical FunctionLibraryToolset.
    // Read the actual registered wrapper instead, retaining current user filters.
    const TSharedPtr<FToolset> Registered = Subsystem->ToolsetRegistry.Find(CanonicalName);
    if (Registered != State->Wrapper)
    { State->Active.Store(false); Error = TEXT("Current registered LegacyChaosClothing wrapper identity is unavailable for logical schema receipt"); return false; }
    const FString RegisteredLogicalSchema = Registered->GetJsonSchema();
    auto AfterSchemaSubsystem = UToolsetRegistrySubsystem::Get();
    if (RegisteredLogicalSchema.IsEmpty() || !AfterSchemaSubsystem.HasValue()
        || AfterSchemaSubsystem.GetValue().Get() != State->Subsystem.Get()
        || Subsystem->ToolsetRegistry.Find(CanonicalName) != Registered
        || !Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
    { State->Active.Store(false); State->Original->SetEnabled(false);
      if (Error.IsEmpty()) { Error = TEXT("Current registered LegacyChaosClothing logical schema/owner/class receipt could not be confirmed"); }
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
        if (Remains) { Report->SetBoolField(TEXT("cleanup_required"), true); Report->SetBoolField(TEXT("registry_changed"), Changed); Error = TEXT("Owned LegacyChaosClothing guard cleanup was not confirmed; inactive retained owner preserved"); return false; }
    }
    // Detached/cached wrappers are inactive, so they cannot dispatch after this cycle is broken.
    State->Wrapper.Reset(); State->Original.Reset(); Owned.Reset();
    Report->SetBoolField(TEXT("registry_changed"), Changed); Report->SetBoolField(TEXT("cleanup_required"), false);
    Report->SetBoolField(TEXT("class_unregister_allowed"), false); Report->SetBoolField(TEXT("owned_handler_removed"), true);
    Report->SetBoolField(TEXT("foreign_handler_preserved"), true);
    return true;
}
}

bool InstallLegacyChaosClothingToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { Error = TEXT("LegacyChaosClothing guard installation requires an idle live editor game thread; no attachment retried"); return false; }
    FBusyScope Busy;
    if (!Metadata(ToolsetClass, Error)) { return false; }
    auto Current = UToolsetRegistrySubsystem::Get();
    if (!Current.HasValue()) { Error = TEXT("Original official Toolset Registry subsystem unavailable"); return false; }
    if (Owned.IsValid())
    {
        if (Owned->Subsystem.Get() != Current.GetValue().Get() || Owned->Class.Get() != ToolsetClass)
        { Error = TEXT("Different retained LegacyChaosClothing owner/subsystem/class exists; explicit cleanup required, no automatic reattachment"); return false; }
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
    if (!LegacyChaosClothingSchema(State->ReflectedSchema, State->FullSchema, Error)) { Original->SetEnabled(false); return false; }
    State->Wrapper = MakeShared<FLegacyChaosClothingToolset>(State);
    // Exact identity is checked immediately before SDK's name-only removal, with dispatch inactive.
    if (!HasRetainedToolsetIdentity(Current.GetValue()->ToolsetRegistry, Original)
        || !Current.GetValue()->ToolsetRegistry.UnregisterToolset(Original))
    { Error = TEXT("Original LegacyChaosClothing handler detach failed or identity changed; captured owner remains inactive for explicit cleanup"); return false; }
    State->OriginalDetached = true; Report->SetBoolField(TEXT("registry_changed"), true); Report->SetBoolField(TEXT("original_handler_detached"), true);
    // Register broadcasts callbacks. Reentry is busy-rejected and wrapper dispatch remains inactive.
    if (!Current.GetValue()->ToolsetRegistry.RegisterToolset(State->Wrapper))
    { Error = TEXT("LegacyChaosClothing raw wrapper registration failed or collided; no unguarded handler restored, inspect actual registry before cleanup"); return false; }
    if (!Ready(State, Report, Error)) { return false; }
    State->Active.Store(true); State->Original->SetEnabled(false);
    Report->SetBoolField(TEXT("ready"), true); Report->SetBoolField(TEXT("verified"), true);
    Report->SetBoolField(TEXT("active"), true);
    // The registration event already exposed the immutable complete schema; activation changes no catalogue.
    Report->SetBoolField(TEXT("extra_catalogue_broadcast_required"), false);
    return true;
}

bool VerifyLegacyChaosClothingToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy) { Error = TEXT("LegacyChaosClothing guard verification requires an idle live editor game thread"); return false; }
    if (Owned.IsValid() && Owned->InFlight.Load() != 0)
    {
        auto Current = UToolsetRegistrySubsystem::Get();
        const int32 DelegatedIndex=Owned->DelegatedMethodIndex.Load();
        const bool Admitted = Owned->InFlight.Load() == 1 && DelegatedIndex>=0 && DelegatedIndex<1 && Owned->Active.Load()
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
        if (!Admitted) { Error = TEXT("Only the current exact guarded Named LegacyChaosClothing delegation is admitted"); }
        return false;
    }
    FBusyScope Busy;
    if (!Owned.IsValid() || Owned->Class.Get() != ToolsetClass) { Error = TEXT("Exact installed LegacyChaosClothing raw guard required; verification never installs one"); return false; }
    auto Current = UToolsetRegistrySubsystem::Get();
    if (!Current.HasValue() || Current.GetValue().Get() != Owned->Subsystem.Get()) { Error = TEXT("Original LegacyChaosClothing registry subsystem identity changed; no retargeting"); return false; }
    const bool Ok = Ready(Owned.ToSharedRef(), Report, Error) && Owned->Active.Load();
    Report->SetBoolField(TEXT("ready"), Ok); Report->SetBoolField(TEXT("verified"), Ok); Report->SetBoolField(TEXT("active"), Owned->Active.Load());
    return Ok;
}

bool RemoveLegacyChaosClothingToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { Error = TEXT("LegacyChaosClothing guard removal requires an idle editor game thread; no foreign entry removed"); return false; }
    FBusyScope Busy;
    if (Owned.IsValid() && Owned->Class.Get() != ToolsetClass) { Error = TEXT("Exact retained LegacyChaosClothing class owner required for removal"); return false; }
    const bool Ok = Cleanup(Report, Error); Report->SetBoolField(TEXT("removed"), Ok); return Ok;
}

void ShutdownLegacyChaosClothingToolset()
{
    if (Owned.IsValid()) { Owned->Active.Store(false); }
    if (!IsInGameThread() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { return; }
    FBusyScope Busy; auto Report = MakeShared<FJsonObject>(); FString Error;
    Cleanup(Report, Error);
}
}
