#include "Compatibility/UE58/CubelessMaterialArrayConverter.h"

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "ToolsetRegistry/ToolsetJsonConverter.h"

namespace
{
    enum class EMaterialArray { Unhandled, NodePosition, ParameterNames };

    EMaterialArray MatchParameter(const FProperty* Property)
    {
        const FArrayProperty* Array = CastField<FArrayProperty>(Property);
        const UFunction* Function = Property ? Cast<UFunction>(Property->GetOwnerUObject()) : nullptr;
        const UClass* Owner = Function ? Function->GetOwnerClass() : nullptr;
        if (!Array || Array->ArrayDim != 1 || !Array->HasAllPropertyFlags(CPF_Parm)
            || Array->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm | CPF_ReferenceParm)
            || !Owner || Owner->GetPathName() != TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/material_toolset_PY.CubelessMaterialTools"))
        {
            return EMaterialArray::Unhandled;
        }
        if (Function->GetName() == TEXT("add_material_node") && Array->GetName() == TEXT("node_position")
            && CastField<FIntProperty>(Array->Inner))
        {
            return EMaterialArray::NodePosition;
        }
        if (Function->GetName() == TEXT("get_material_parameter_collection_values") && Array->GetName() == TEXT("parameter_names")
            && CastField<FStrProperty>(Array->Inner))
        {
            return EMaterialArray::ParameterNames;
        }
        return EMaterialArray::Unhandled;
    }

    bool ValidateJson(const TSharedPtr<FJsonValue>& Value, EMaterialArray Kind)
    {
        if (Kind == EMaterialArray::Unhandled || !Value.IsValid() || Value->Type != EJson::Array)
        {
            return false;
        }
        const TArray<TSharedPtr<FJsonValue>>& Items = Value->AsArray();
        if (Kind == EMaterialArray::NodePosition && Items.Num() != 2)
        {
            return false;
        }
        for (const TSharedPtr<FJsonValue>& Item : Items)
        {
            if (!Item.IsValid()) { return false; }
            if (Kind == EMaterialArray::ParameterNames)
            {
                if (Item->Type != EJson::String) { return false; }
            }
            else
            {
                if (Item->Type != EJson::Number) { return false; }
                const double Number = Item->AsNumber();
                if (!FMath::IsFinite(Number) || Number < MIN_int32 || Number > MAX_int32
                    || Number != FMath::TruncToDouble(Number))
                {
                    return false;
                }
            }
        }
        return true;
    }

    TSharedPtr<FJsonValue> ExportArray(const FArrayProperty* Array, const void* Address)
    {
        FScriptArrayHelper Helper(Array, Address);
        TArray<TSharedPtr<FJsonValue>> Items;
        Items.Reserve(Helper.Num());
        for (int32 Index = 0; Index < Helper.Num(); ++Index)
        {
            if (const FIntProperty* Integer = CastField<FIntProperty>(Array->Inner))
            {
                Items.Add(MakeShared<FJsonValueNumber>(Integer->GetPropertyValue(Helper.GetRawPtr(Index))));
            }
            else if (const FStrProperty* String = CastField<FStrProperty>(Array->Inner))
            {
                Items.Add(MakeShared<FJsonValueString>(String->GetPropertyValue(Helper.GetRawPtr(Index))));
            }
            else { return nullptr; }
        }
        return MakeShared<FJsonValueArray>(Items);
    }

    TSharedPtr<FJsonValue> ParseDefault(const FProperty* Property, const FString& Text)
    {
        const FArrayProperty* Array = CastField<FArrayProperty>(Property);
        const EMaterialArray Kind = MatchParameter(Property);
        if (!Array || Kind == EMaterialArray::Unhandled) { return nullptr; }
        FDefaultConstructedPropertyElement Scratch(Array);
        // Empty arrays can export an empty metadata value. Presence of the metadata
        // is checked by the caller; empty text means an initialized empty array.
        if (!Text.IsEmpty())
        {
            const TCHAR* End = Array->ImportText_Direct(*Text, Scratch.GetObjAddress(), nullptr, PPF_None);
            if (!End || !FString(End).TrimStartAndEnd().IsEmpty()) { return nullptr; }
        }
        TSharedPtr<FJsonValue> Value = ExportArray(Array, Scratch.GetObjAddress());
        return ValidateJson(Value, Kind) ? Value : nullptr;
    }

    class FMaterialArrayConverter final : public UE::ToolsetRegistry::FToolsetJsonConverter
    {
    public:
        virtual FString GetName() const override { return TEXT("CubelessMCP_UE58.MaterialToolArrayParameters.v1"); }

        virtual bool CanConvertProperty(TNotNull<const FProperty*> Property) override
        {
            return MatchParameter(Property) != EMaterialArray::Unhandled;
        }

        virtual TSharedPtr<FJsonObject> PropertyToJsonSchema(TNotNull<const FProperty*> Property) override
        {
            const EMaterialArray Kind = MatchParameter(Property);
            TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
            if (Kind == EMaterialArray::Unhandled) { return Schema; }
            Schema->SetStringField(TEXT("type"), TEXT("array"));
            TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("type"), Kind == EMaterialArray::NodePosition ? TEXT("integer") : TEXT("string"));
            if (Kind == EMaterialArray::NodePosition)
            {
                Item->SetNumberField(TEXT("minimum"), MIN_int32);
                Item->SetNumberField(TEXT("maximum"), MAX_int32);
                Schema->SetNumberField(TEXT("minItems"), 2);
                Schema->SetNumberField(TEXT("maxItems"), 2);
            }
            Schema->SetObjectField(TEXT("items"), Item);

            const UFunction* Function = CastChecked<UFunction>(Property->GetOwnerUObject());
            const FName Key(*FString::Printf(TEXT("CPP_Default_%s"), *Property->GetName()));
            // Full schema must carry the default, including empty-array metadata:
            // UE58 only invokes PropertyToDefault when its default string is nonempty.
            if (Function->HasMetaData(Key))
            {
                if (TSharedPtr<FJsonValue> Default = ParseDefault(Property, Function->GetMetaData(Key)))
                {
                    Schema->SetField(TEXT("default"), Default);
                }
            }
            return Schema;
        }

        virtual TSharedPtr<FJsonValue> PropertyToDefault(TNotNull<const FProperty*> Property, const FString& DefaultString) override
        {
            if (TSharedPtr<FJsonValue> Default = ParseDefault(Property, DefaultString)) { return Default; }
            UKismetSystemLibrary::RaiseScriptError(TEXT("Cubeless Material array default metadata is invalid; omitted calls cannot be accepted."));
            // The registration preparation rejects invalid metadata before a toolset
            // can cache its schema. This fallback is not a valid declared default.
            return MakeShared<FJsonValueNull>();
        }

        virtual TSharedPtr<FJsonValue> PropertyToJsonData(TNotNull<FProperty*> Property, const void* Value) override
        {
            if (MatchParameter(Property) == EMaterialArray::Unhandled || !Value) { return nullptr; }
            TSharedPtr<FJsonValue> Result = ExportArray(CastFieldChecked<FArrayProperty>(Property), Value);
            return ValidateJson(Result, MatchParameter(Property)) ? Result : nullptr;
        }

        virtual bool JsonDataToProperty(const TSharedPtr<FJsonValue>& JsonValue, TNotNull<FProperty*> Property,
            void* OutValue, UObject* Outer) override
        {
            const EMaterialArray Kind = MatchParameter(Property);
            // Do not touch caller storage until every element has passed validation.
            if (!OutValue || !ValidateJson(JsonValue, Kind))
            {
                UKismetSystemLibrary::RaiseScriptError(FString::Printf(
                    TEXT("Cubeless %s requires %s; coercions and malformed non-null arrays are rejected before the operation."),
                    *Property->GetName(), Kind == EMaterialArray::NodePosition
                        ? TEXT("exactly two finite int32 JSON numbers") : TEXT("an array of JSON strings")));
                return false;
            }
            FArrayProperty* Array = CastFieldChecked<FArrayProperty>(Property);
            FDefaultConstructedPropertyElement Scratch(Array);
            FScriptArrayHelper Helper(Array, Scratch.GetObjAddress());
            const TArray<TSharedPtr<FJsonValue>>& Items = JsonValue->AsArray();
            Helper.AddValues(Items.Num());
            for (int32 Index = 0; Index < Items.Num(); ++Index)
            {
                if (Kind == EMaterialArray::NodePosition)
                {
                    CastFieldChecked<FIntProperty>(Array->Inner)->SetPropertyValue(Helper.GetRawPtr(Index), static_cast<int32>(Items[Index]->AsNumber()));
                }
                else
                {
                    CastFieldChecked<FStrProperty>(Array->Inner)->SetPropertyValue(Helper.GetRawPtr(Index), Items[Index]->AsString());
                }
            }
            Array->CopyCompleteValue(OutValue, Scratch.GetObjAddress());
            return true;
        }
    };
}

TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> Cubeless::UE58::MakeMaterialArrayConverter()
{
    return MakeShared<FMaterialArrayConverter>();
}

bool Cubeless::UE58::PrepareMaterialArrayDefaults(UClass* ToolsetClass, FString& Error, bool& bNormalized)
{
    bNormalized = false;
    if (!IsInGameThread() || !ToolsetClass || ToolsetClass->GetPathName() != TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/material_toolset_PY.CubelessMaterialTools"))
    {
        Error = TEXT("Expected the exact mounted Cubeless Material Python generated class on the game thread.");
        return false;
    }
    UFunction* Add = ToolsetClass->FindFunctionByName(TEXT("add_material_node"));
    UFunction* Get = ToolsetClass->FindFunctionByName(TEXT("get_material_parameter_collection_values"));
    FProperty* Position = Add ? FindFProperty<FProperty>(Add, TEXT("node_position")) : nullptr;
    FProperty* Names = Get ? FindFProperty<FProperty>(Get, TEXT("parameter_names")) : nullptr;
    const FName PositionKey(TEXT("CPP_Default_node_position"));
    const FName NamesKey(TEXT("CPP_Default_parameter_names"));
    if (MatchParameter(Position) != EMaterialArray::NodePosition || MatchParameter(Names) != EMaterialArray::ParameterNames
        || !Add->HasMetaData(PositionKey) || !Get->HasMetaData(NamesKey))
    {
        Error = TEXT("Expected both exact array input properties and declared CPP_Default metadata.");
        return false;
    }
    const FString NamesDefault = Get->GetMetaData(NamesKey);
    TSharedPtr<FJsonValue> PositionValue = ParseDefault(Position, Add->GetMetaData(PositionKey));
    TSharedPtr<FJsonValue> NamesValue = ParseDefault(Names, NamesDefault);
    if (!PositionValue.IsValid() || PositionValue->AsArray()[0]->AsNumber() != 0.0 || PositionValue->AsArray()[1]->AsNumber() != 0.0
        || !NamesValue.IsValid() || !NamesValue->AsArray().IsEmpty()
        || (!NamesDefault.IsEmpty() && NamesDefault != TEXT("()")))
    {
        Error = TEXT("Declared defaults differ from the supported [0,0] and [] contract; registration refused without changing metadata.");
        return false;
    }
    // UE58's schema editor skips the default callback for empty exported text.
    // () imports to the identical empty FArrayProperty but reaches that callback.
    // Only our in-memory generated UFunction metadata is changed; no engine/asset writes.
    if (NamesDefault.IsEmpty())
    {
        Get->SetMetaData(NamesKey, TEXT("()"));
        bNormalized = true;
    }
    return true;
}
