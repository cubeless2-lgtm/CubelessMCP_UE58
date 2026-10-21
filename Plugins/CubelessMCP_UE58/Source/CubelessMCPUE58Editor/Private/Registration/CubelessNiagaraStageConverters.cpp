#include "Registration/CubelessNiagaraStageConverters.h"
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Kismet/KismetSystemLibrary.h"
#include "ToolsetRegistry/ToolsetJsonConverter.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

namespace
{
constexpr const TCHAR* ClassPath = TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/niagara_simulation_stages_toolset_PY.CubelessNiagaraSimulationStageTools");
const FName DefaultKey(TEXT("CPP_Default_element_count"));
bool Match(const FProperty* Property)
{
    const auto* Array = CastField<FArrayProperty>(Property); const auto* Function = Property ? Cast<UFunction>(Property->GetOwnerUObject()) : nullptr;
    const auto* Owner = Function ? Function->GetOwnerClass() : nullptr;
    return Array && Array->ArrayDim == 1 && CastField<FIntProperty>(Array->Inner) && Array->Inner->ArrayDim == 1 &&
        Array->HasAllPropertyFlags(CPF_Parm) && !Array->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm | CPF_ReferenceParm) &&
        Owner && Owner->GetPathName() == ClassPath && Array->GetName() == TEXT("element_count") &&
        (Function->GetName() == TEXT("add_niagara_grid_simulation_stage") || Function->GetName() == TEXT("set_niagara_simulation_stage_settings"));
}
bool EmptyDeclaredDefault(const FProperty* Property, const FString& Value)
{
    const auto* Array = CastField<FArrayProperty>(Property);
    if (!Match(Property) || (!Value.IsEmpty() && Value != TEXT("()"))) { return false; }
    FDefaultConstructedPropertyElement Scratch(Array);
    if (!Value.IsEmpty()) { const TCHAR* End = Array->ImportText_Direct(*Value, Scratch.GetObjAddress(), nullptr, PPF_None);
        if (!End || !FString(End).TrimStartAndEnd().IsEmpty()) { return false; } }
    return FScriptArrayHelper(Array, Scratch.GetObjAddress()).Num() == 0;
}
bool Valid(const TSharedPtr<FJsonValue>& Value)
{
    if (!Value.IsValid() || Value->Type != EJson::Array) { return false; }
    const auto& Items = Value->AsArray(); if (Items.Num() < 1 || Items.Num() > 3) { return false; }
    for (const auto& Item : Items)
    { if (!Item.IsValid() || Item->Type != EJson::Number) { return false; } const double N = Item->AsNumber();
        if (!FMath::IsFinite(N) || N < 0 || N > MAX_int32 || N != FMath::TruncToDouble(N)) { return false; } }
    return true;
}
class FStageArrayConverter final : public UE::ToolsetRegistry::FToolsetJsonConverter
{
public:
    virtual FString GetName() const override { return TEXT("CubelessMCP_UE58.NiagaraStageElementCount.v1"); }
    virtual bool CanConvertProperty(TNotNull<const FProperty*> Property) override { return Match(Property); }
    virtual TSharedPtr<FJsonObject> PropertyToJsonSchema(TNotNull<const FProperty*> Property) override
    {
        auto Schema = MakeShared<FJsonObject>(); if (!Match(Property)) { return Schema; }
        Schema->SetArrayField(TEXT("type"), {MakeShared<FJsonValueString>(TEXT("array")), MakeShared<FJsonValueString>(TEXT("null"))});
        auto Item = MakeShared<FJsonObject>(); Item->SetStringField(TEXT("type"), TEXT("integer")); Item->SetNumberField(TEXT("minimum"), 0); Item->SetNumberField(TEXT("maximum"), MAX_int32);
        Schema->SetObjectField(TEXT("items"), Item); Schema->SetNumberField(TEXT("minItems"), 1); Schema->SetNumberField(TEXT("maxItems"), 3);
        const auto* Function = CastChecked<UFunction>(Property->GetOwnerUObject());
        if (Function->HasMetaData(DefaultKey) && EmptyDeclaredDefault(Property, Function->GetMetaData(DefaultKey)))
        { Schema->SetField(TEXT("default"), MakeShared<FJsonValueNull>()); }
        return Schema;
    }
    virtual TSharedPtr<FJsonValue> PropertyToDefault(TNotNull<const FProperty*> Property, const FString& DefaultString) override
    {
        if (EmptyDeclaredDefault(Property, DefaultString)) { return MakeShared<FJsonValueNull>(); }
        UKismetSystemLibrary::RaiseScriptError(TEXT("Stage element_count default must be the exact internal empty-array bridge; registration refused."));
        return nullptr;
    }
    virtual TSharedPtr<FJsonValue> PropertyToJsonData(TNotNull<FProperty*> Property, const void* Value) override
    {
        if (!Match(Property) || !Value) { return nullptr; } auto* Array = CastFieldChecked<FArrayProperty>(Property); FScriptArrayHelper Helper(Array, Value);
        if (Helper.Num() == 0) { return MakeShared<FJsonValueNull>(); }
        TArray<TSharedPtr<FJsonValue>> Items; for (int32 N = 0; N < Helper.Num(); ++N)
        { Items.Add(MakeShared<FJsonValueNumber>(CastFieldChecked<FIntProperty>(Array->Inner)->GetPropertyValue(Helper.GetRawPtr(N)))); }
        TSharedPtr<FJsonValue> Json = MakeShared<FJsonValueArray>(Items); return Valid(Json) ? Json : nullptr;
    }
    virtual bool JsonDataToProperty(const TSharedPtr<FJsonValue>& JsonValue, TNotNull<FProperty*> Property, void* OutValue, UObject* Outer) override
    {
        // UE58 omits null before the custom converter. A direct converter null
        // also initializes only the omission bridge; explicit [] stays invalid.
        if (!Match(Property) || !OutValue || !JsonValue.IsValid()) { return false; }
        if (JsonValue->Type != EJson::Null && !Valid(JsonValue))
        { UKismetSystemLibrary::RaiseScriptError(TEXT("Stage element_count requires null or one through three nonnegative integral int32 JSON numbers; explicit [] and coercions are rejected before the operation.")); return false; }
        auto* Array = CastFieldChecked<FArrayProperty>(Property); FDefaultConstructedPropertyElement Scratch(Array); FScriptArrayHelper Helper(Array, Scratch.GetObjAddress());
        if (JsonValue->Type == EJson::Array)
        { const auto& Items = JsonValue->AsArray(); Helper.AddValues(Items.Num()); for (int32 N = 0; N < Items.Num(); ++N)
            { CastFieldChecked<FIntProperty>(Array->Inner)->SetPropertyValue(Helper.GetRawPtr(N), static_cast<int32>(Items[N]->AsNumber())); } }
        Array->CopyCompleteValue(OutValue, Scratch.GetObjAddress()); return true;
    }
};
}
TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> Cubeless::Registration::MakeNiagaraStageElementCountConverter()
{ return MakeShared<FStageArrayConverter>(); }
bool Cubeless::Registration::PrepareNiagaraStageArrayDefaults(UClass* ToolsetClass, FString& Error, bool& bNormalized)
{
    bNormalized = false;
    if (!IsInGameThread() || !ToolsetClass || ToolsetClass->GetPathName() != ClassPath)
    { Error = TEXT("Require exact mounted Stage Python generated class on the editor game thread."); return false; }
    TArray<UFunction*> Functions;
    for (const FName Name : {FName(TEXT("add_niagara_grid_simulation_stage")), FName(TEXT("set_niagara_simulation_stage_settings"))})
    { auto* Function = ToolsetClass->FindFunctionByName(Name); auto* Property = Function ? FindFProperty<FProperty>(Function, TEXT("element_count")) : nullptr;
        if (!Match(Property) || !Function->HasMetaData(DefaultKey) || !EmptyDeclaredDefault(Property, Function->GetMetaData(DefaultKey)))
        { Error = TEXT("Both exact Stage plain FArray<int32> inputs must declare the empty-array bridge; optional-array or other defaults are unsupported."); return false; }
        Functions.Add(Function);
    }
    // All properties/defaults are admitted before changing only our generated
    // UFunction metadata. () reaches the official default-converter callback.
    for (auto* Function : Functions) { if (Function->GetMetaData(DefaultKey).IsEmpty()) { Function->SetMetaData(DefaultKey, TEXT("()")); bNormalized = true; } }
    return true;
}
