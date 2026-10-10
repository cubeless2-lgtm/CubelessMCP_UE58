#include "Registration/CubelessRegistrationLibrary.h"

#include "Compatibility/UE58/CubelessMaterialArrayConverter.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "ToolsetRegistry/ToolsetJsonConverter.h"
#include "ToolsetRegistry/ToolsetRegistrySubsystem.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogCubelessRegistration, Log, All);

FString UCubelessRegistrationLibrary::PrepareMaterialToolset(UClass* ToolsetClass)
{
    TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("class_path"), ToolsetClass ? ToolsetClass->GetPathName() : FString());
    Report->SetBoolField(TEXT("success"), false);
    Report->SetBoolField(TEXT("normalized_empty_array_default"), false);
    FString Error;
    bool bNormalized = false;
    auto Subsystem = UToolsetRegistrySubsystem::Get();
    if (!IsInGameThread() || !Subsystem.HasValue())
    {
        Error = TEXT("Material registration preparation requires the game thread and live official registry.");
    }
    else if (!ToolsetClass || ToolsetClass->GetPathName() != TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/material_toolset_PY.CubelessMaterialTools"))
    {
        Error = TEXT("Only the exact mounted Cubeless Material Python generated class can be prepared.");
    }
    else if (UToolsetRegistry::IsToolsetClassRegistered(ToolsetClass))
    {
        Error = TEXT("Material toolset already has a cached schema; preparation must precede registration. Restart the editor.");
    }
    else
    {
        bool bBothConverters = true;
        for (const TPair<FName, FName>& Names : {
            TPair<FName, FName>(TEXT("add_material_node"), TEXT("node_position")),
            TPair<FName, FName>(TEXT("get_material_parameter_collection_values"), TEXT("parameter_names")) })
        {
            UFunction* Function = ToolsetClass->FindFunctionByName(Names.Key);
            FProperty* Property = Function ? FindFProperty<FProperty>(Function, Names.Value) : nullptr;
            TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> Converter = Property
                ? Subsystem.GetValue()->ToolsetRegistry.GetConverterForProperty(Property) : nullptr;
            bBothConverters &= Converter.IsValid() && Converter->GetName() == TEXT("CubelessMCP_UE58.MaterialToolArrayParameters.v1");
        }
        Report->SetBoolField(TEXT("both_exact_converters_active"), bBothConverters);
        if (!bBothConverters)
        {
            Error = TEXT("Exact Material array converter is missing or another converter claimed an input. Registration refused.");
        }
        else if (Cubeless::UE58::PrepareMaterialArrayDefaults(ToolsetClass, Error, bNormalized))
        {
            Report->SetBoolField(TEXT("success"), true);
            Report->SetBoolField(TEXT("normalized_empty_array_default"), bNormalized);
            Report->SetArrayField(TEXT("node_position_default"), {MakeShared<FJsonValueNumber>(0), MakeShared<FJsonValueNumber>(0)});
            Report->SetArrayField(TEXT("parameter_names_default"), {});
            Report->SetStringField(TEXT("node_position_property_type"), TEXT("FArrayProperty<FIntProperty>"));
            Report->SetStringField(TEXT("parameter_names_property_type"), TEXT("FArrayProperty<FStrProperty>"));
            Report->SetStringField(TEXT("parameter_names_metadata_after"), TEXT("()"));
        }
    }
    Report->SetStringField(TEXT("error"), Error);
    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    if (Error.IsEmpty()) { UE_LOG(LogCubelessRegistration, Display, TEXT("Material array schema preparation: %s"), *Json); }
    else { UE_LOG(LogCubelessRegistration, Error, TEXT("Material array schema preparation: %s"), *Json); }
    return Json;
}
