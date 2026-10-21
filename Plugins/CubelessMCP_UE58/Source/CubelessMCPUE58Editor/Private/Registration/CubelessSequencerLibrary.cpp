#include "Registration/CubelessSequencerLibrary.h"
#include "Compatibility/UE58/CubelessSequencerApi.h"
#include "Compatibility/UE58/CubelessSequencerFloatDefaultsConverter.h"
#include "Compatibility/UE58/CubelessSequencerMaintenanceDefaultsConverter.h"
#include "Compatibility/UE58/CubelessSequencerCameraDefaultsConverter.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "ToolsetRegistry/ToolsetJsonConverter.h"
#include "ToolsetRegistry/ToolsetRegistrySubsystem.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UObject/UnrealType.h"

ULevelSequence* UCubelessSequencerLibrary::ResolveLevelSequenceAsset(const FString& SequencePath,FString& OutReceiptJson)
{return Cubeless::UE58::Sequencer::ResolveAsset(SequencePath,OutReceiptJson);}
FString UCubelessSequencerLibrary::ApplyLevelSequencePlaybackRange(const FString& SequencePath,
    double PlaybackStartFrame, double PlaybackEndFrame)
{
    return Cubeless::UE58::Sequencer::ApplyPlaybackRange(SequencePath, PlaybackStartFrame, PlaybackEndFrame);
}
FString UCubelessSequencerLibrary::AddLevelSequenceTransformKeys(const FString& SequencePath,
    const FString& KeysJson, const FString& BindingId, const FString& ActorLabel)
{
    return Cubeless::UE58::Sequencer::AddTransformKeys(SequencePath, KeysJson, BindingId, ActorLabel);
}

FString UCubelessSequencerLibrary::AddLevelSequenceFloatPropertyKeys(const FString& Path, const FString& Property,
    const FString& Keys, const FString& Id, const FString& Label)
{ return Cubeless::UE58::Sequencer::AddFloatPropertyKeys(Path, Property, Keys, Id, Label); }

FString UCubelessSequencerLibrary::PrepareSequencerFloatToolset(UClass* ToolsetClass)
{
    auto Report = MakeShared<FJsonObject>(); Report->SetBoolField(TEXT("success"), false); Report->SetBoolField(TEXT("postcondition_passed"), false); Report->SetStringField(TEXT("class_path"), ToolsetClass ? ToolsetClass->GetPathName() : FString());
    for (const TCHAR* Field : { TEXT("exact_converter_active"), TEXT("default_verified"), TEXT("mutation_attempted"), TEXT("changed"), TEXT("asset_mutation_attempted"), TEXT("compile_requested"), TEXT("compile_attempted"), TEXT("save_requested"), TEXT("save_attempted"), TEXT("saved") }) { Report->SetBoolField(Field, false); }
    Report->SetBoolField(TEXT("change_state_known"), true); Report->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>()); Report->SetNumberField(TEXT("metadata_write_attempt_count"), 0); Report->SetNumberField(TEXT("metadata_changed_count"), 0);
    Report->SetStringField(TEXT("scope"), TEXT("Only appended Float method's exact binding_id/actor_label empty metadata; old Sequencer defaults, Python bodies/assets/compile/save and global string conversion remain unchanged.")); FString Error;
    if (!IsInGameThread()) { Error = TEXT("Sequencer Float default preparation requires the game thread."); }
    else if (!ToolsetClass || ToolsetClass->GetPathName() != TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/sequencer_toolset_PY.CubelessSequencerTools")) { Error = TEXT("Exact mounted current CubelessSequencerTools class required."); }
    else if (UToolsetRegistry::IsToolsetClassRegistered(ToolsetClass)) { Error = TEXT("Invalidate the exact cached Sequencer class before preparation/registration."); }
    else
    {
        auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Cubeless Sequencer Float empty default preparation"));
        if (!Subsystem.HasValue()) { Error = TEXT("Live official registry subsystem required."); }
        else
        {
            UFunction* Function = ToolsetClass->FindFunctionByName(TEXT("add_level_sequence_float_property_keys")); bool Active = true;
            for (const FName Name : { FName(TEXT("binding_id")), FName(TEXT("actor_label")) })
            {
                FProperty* Property = Function ? FindFProperty<FProperty>(Function, Name) : nullptr; auto Converter = Property ? Subsystem.GetValue()->ToolsetRegistry.GetConverterForProperty(Property) : nullptr;
                Active &= Converter.IsValid() && Converter->GetName() == TEXT("CubelessMCP_UE58.SequencerFloatEmptyStringDefaults.v1");
            }
            Report->SetBoolField(TEXT("exact_converter_active"), Active);
            if (!Active) { Error = TEXT("Both exact appended Float FString selector inputs must use the registered finite converter."); }
            else if (Cubeless::UE58::PrepareSequencerFloatDefaults(ToolsetClass, Report, Error)) { Report->SetBoolField(TEXT("success"), true); Report->SetBoolField(TEXT("postcondition_passed"), true); }
        }
    }
    Report->SetStringField(TEXT("stage"), Report->GetBoolField(TEXT("mutation_attempted")) ? TEXT("metadata_preparation") : TEXT("schema_preflight")); Report->SetStringField(TEXT("error_code"), Error.IsEmpty() ? FString() : TEXT("sequencer_float_default_preparation_failed")); Report->SetStringField(TEXT("error"), Error); FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json)); return Json;
}

FString UCubelessSequencerLibrary::SetLevelSequencePlaybackRange(const FString& Path, double Start, double End, bool bSave)
{ return Cubeless::UE58::Sequencer::SetPlaybackRange(Path, Start, End, bSave); }
FString UCubelessSequencerLibrary::RemoveLevelSequenceBinding(const FString& Path, const FString& Id, const FString& Label, bool bSave)
{ return Cubeless::UE58::Sequencer::RemoveBinding(Path, Id, Label, bSave); }

FString UCubelessSequencerLibrary::PrepareSequencerMaintenanceToolset(UClass* ToolsetClass)
{
    auto Report = MakeShared<FJsonObject>(); Report->SetBoolField(TEXT("success"), false); Report->SetBoolField(TEXT("postcondition_passed"), false); Report->SetStringField(TEXT("class_path"), ToolsetClass ? ToolsetClass->GetPathName() : FString());
    for (const TCHAR* Field : { TEXT("exact_converter_active"), TEXT("default_verified"), TEXT("mutation_attempted"), TEXT("changed"), TEXT("asset_mutation_attempted"), TEXT("compile_requested"), TEXT("compile_attempted"), TEXT("save_requested"), TEXT("save_attempted"), TEXT("saved") }) { Report->SetBoolField(Field, false); }
    Report->SetBoolField(TEXT("change_state_known"), true); Report->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>()); Report->SetNumberField(TEXT("metadata_write_attempt_count"), 0); Report->SetNumberField(TEXT("metadata_changed_count"), 0);
    Report->SetStringField(TEXT("scope"), TEXT("Only appended Remove method's exact binding_id/actor_label empty metadata; old Sequencer defaults, Python bodies/assets/compile/save and global string conversion remain unchanged.")); FString Error;
    if (!IsInGameThread()) { Error = TEXT("Sequencer Maintenance default preparation requires the game thread."); }
    else if (!ToolsetClass || ToolsetClass->GetPathName() != TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/sequencer_toolset_PY.CubelessSequencerTools")) { Error = TEXT("Exact mounted current CubelessSequencerTools class required."); }
    else if (UToolsetRegistry::IsToolsetClassRegistered(ToolsetClass)) { Error = TEXT("Invalidate the exact cached Sequencer class before preparation/registration."); }
    else
    {
        auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Cubeless Sequencer Maintenance empty default preparation"));
        if (!Subsystem.HasValue()) { Error = TEXT("Live official registry subsystem required."); }
        else
        {
            UFunction* Function = ToolsetClass->FindFunctionByName(TEXT("remove_level_sequence_binding")); bool Active = true;
            for (const FName Name : { FName(TEXT("binding_id")), FName(TEXT("actor_label")) })
            {
                FProperty* Property = Function ? FindFProperty<FProperty>(Function, Name) : nullptr; auto Converter = Property ? Subsystem.GetValue()->ToolsetRegistry.GetConverterForProperty(Property) : nullptr;
                Active &= Converter.IsValid() && Converter->GetName() == TEXT("CubelessMCP_UE58.SequencerMaintenanceEmptyStringDefaults.v1");
            }
            Report->SetBoolField(TEXT("exact_converter_active"), Active);
            if (!Active) { Error = TEXT("Both exact appended Remove FString selector inputs must use the registered finite converter."); }
            else if (Cubeless::UE58::PrepareSequencerMaintenanceDefaults(ToolsetClass, Report, Error)) { Report->SetBoolField(TEXT("success"), true); Report->SetBoolField(TEXT("postcondition_passed"), true); }
        }
    }
    Report->SetStringField(TEXT("stage"), Report->GetBoolField(TEXT("mutation_attempted")) ? TEXT("metadata_preparation") : TEXT("schema_preflight")); Report->SetStringField(TEXT("error_code"), Error.IsEmpty() ? FString() : TEXT("sequencer_maintenance_default_preparation_failed")); Report->SetStringField(TEXT("error"), Error); FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json)); return Json;
}

FString UCubelessSequencerLibrary::AddLevelSequenceSubsequence(const FString& Path, const FString& ChildPath, double Start, double Duration, bool bSave)
{ return Cubeless::UE58::Sequencer::AddSubsequence(Path, ChildPath, Start, Duration, bSave); }

FString UCubelessSequencerLibrary::AddLevelSequenceCameraCut(const FString& Path, double Start, double End, const FString& Id, const FString& Label, bool bSave)
{ return Cubeless::UE58::Sequencer::AddCameraCut(Path, Start, End, Id, Label, bSave); }

FString UCubelessSequencerLibrary::PrepareSequencerCameraToolset(UClass* ToolsetClass)
{
    auto Report = MakeShared<FJsonObject>(); Report->SetBoolField(TEXT("success"), false); Report->SetBoolField(TEXT("postcondition_passed"), false); Report->SetStringField(TEXT("class_path"), ToolsetClass ? ToolsetClass->GetPathName() : FString());
    for (const TCHAR* Field : { TEXT("exact_converter_active"), TEXT("default_verified"), TEXT("mutation_attempted"), TEXT("changed"), TEXT("asset_mutation_attempted"), TEXT("compile_requested"), TEXT("compile_attempted"), TEXT("save_requested"), TEXT("save_attempted"), TEXT("saved") }) { Report->SetBoolField(Field, false); }
    Report->SetBoolField(TEXT("change_state_known"), true); Report->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>()); Report->SetNumberField(TEXT("metadata_write_attempt_count"), 0); Report->SetNumberField(TEXT("metadata_changed_count"), 0);
    Report->SetStringField(TEXT("scope"), TEXT("Only appended CameraCut method's exact binding_id/actor_label empty metadata; old Sequencer defaults, Python bodies/assets/compile/save and global string conversion remain unchanged.")); FString Error;
    if (!IsInGameThread()) { Error = TEXT("Sequencer Camera default preparation requires the game thread."); }
    else if (!ToolsetClass || ToolsetClass->GetPathName() != TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/sequencer_toolset_PY.CubelessSequencerTools")) { Error = TEXT("Exact mounted current CubelessSequencerTools class required."); }
    else if (UToolsetRegistry::IsToolsetClassRegistered(ToolsetClass)) { Error = TEXT("Invalidate the exact cached Sequencer class before preparation/registration."); }
    else
    {
        auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Cubeless Sequencer Camera empty default preparation"));
        if (!Subsystem.HasValue()) { Error = TEXT("Live official registry subsystem required."); }
        else
        {
            UFunction* Function = ToolsetClass->FindFunctionByName(TEXT("add_level_sequence_camera_cut")); bool Active = true;
            for (const FName Name : { FName(TEXT("binding_id")), FName(TEXT("actor_label")) })
            {
                FProperty* Property = Function ? FindFProperty<FProperty>(Function, Name) : nullptr; auto Converter = Property ? Subsystem.GetValue()->ToolsetRegistry.GetConverterForProperty(Property) : nullptr;
                Active &= Converter.IsValid() && Converter->GetName() == TEXT("CubelessMCP_UE58.SequencerCameraEmptyStringDefaults.v1");
            }
            Report->SetBoolField(TEXT("exact_converter_active"), Active);
            if (!Active) { Error = TEXT("Both exact appended CameraCut FString selector inputs must use the registered finite converter."); }
            else if (Cubeless::UE58::PrepareSequencerCameraDefaults(ToolsetClass, Report, Error)) { Report->SetBoolField(TEXT("success"), true); Report->SetBoolField(TEXT("postcondition_passed"), true); }
        }
    }
    Report->SetStringField(TEXT("stage"), Report->GetBoolField(TEXT("mutation_attempted")) ? TEXT("metadata_preparation") : TEXT("schema_preflight")); Report->SetStringField(TEXT("error_code"), Error.IsEmpty() ? FString() : TEXT("sequencer_camera_default_preparation_failed")); Report->SetStringField(TEXT("error"), Error); FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json)); return Json;
}

FString UCubelessSequencerLibrary::OpenLevelSequenceInEditor(const FString& SequencePath)
{ return Cubeless::UE58::Sequencer::OpenInEditor(SequencePath); }
