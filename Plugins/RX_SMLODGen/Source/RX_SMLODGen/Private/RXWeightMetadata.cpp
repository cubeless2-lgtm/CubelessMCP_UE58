#include "RXWeightMetadata.h"
#include "RXImpostorDirections.h"
#include "RXSilhouetteSettings.h"

#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

const TCHAR* RXWeightMetadata::Key() { return TEXT("RX_SMLODGen_Weights_v1"); }

bool RXWeightMetadata::Encode(const FString& SourceId, const TMap<int32, FRXStoredLODWeights>& LODs,
    int32 ImpostorLOD, int32 ImpostorMaterialSlot, FString& OutJson, FString& Error, int32 ImpostorDirections)
{
    if (SourceId.IsEmpty() || !RXImpostorDirections::IsValid(ImpostorDirections)) { Error = TEXT("Missing LOD 0 identity or invalid impostor directions (2 or 4 required)."); return false; }
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("version"), 5);
    Root->SetNumberField(TEXT("impostor_directions"), ImpostorDirections);
    Root->SetStringField(TEXT("source_id"), SourceId);
    if (ImpostorLOD < INDEX_NONE || ImpostorMaterialSlot < INDEX_NONE || (ImpostorLOD < 0) != (ImpostorMaterialSlot < 0) || ImpostorLOD == 0)
    { Error = TEXT("Invalid impostor LOD or material slot."); return false; }
    Root->SetNumberField(TEXT("impostor_lod"), ImpostorLOD);
    Root->SetNumberField(TEXT("impostor_material_slot"), ImpostorMaterialSlot);
    TArray<TSharedPtr<FJsonValue>> Entries;
    TArray<int32> Keys;
    LODs.GetKeys(Keys);
    Keys.Sort();
    for (int32 Index : Keys)
    {
        const FRXStoredLODWeights& LOD = LODs[Index];
        FString SettingsJson;
        if (Index < 1 || LOD.Sections.IsEmpty() || !FJsonObjectConverter::UStructToJsonObjectString(LOD.Settings, SettingsJson))
        { Error = TEXT("Invalid LOD weights or reduction settings."); return false; }
        TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetNumberField(TEXT("lod"), Index);
        Entry->SetStringField(TEXT("settings"), SettingsJson);
        TArray<TSharedPtr<FJsonValue>> Sections;
        TSet<int32> Seen;
        for (const FRXSectionWeight& Section : LOD.Sections)
        {
            const int32 Group = Section.Group.GetValue();
            if (Group < 0 || Seen.Contains(Group) || !FMath::IsFinite(Section.Weight) || Section.Weight < 0 || Section.Weight > 1)
            { Error = TEXT("Invalid or duplicate section weight."); return false; }
            Seen.Add(Group);
            TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetNumberField(TEXT("group"), Group);
            Row->SetNumberField(TEXT("weight"), Section.Weight);
            Row->SetBoolField(TEXT("excluded"), Section.bExcluded);
            RXSilhouetteSettings::Write(*Row, Section);
            Sections.Add(MakeShared<FJsonValueObject>(Row));
        }
        Entry->SetArrayField(TEXT("sections"), Sections);
        Entries.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Root->SetArrayField(TEXT("lods"), Entries);
    FString Encoded;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Encoded);
    if (!FJsonSerializer::Serialize(Root, Writer)) { Error = TEXT("Could not encode weight metadata."); return false; }
    OutJson = MoveTemp(Encoded);
    Error.Reset();
    return true;
}

bool RXWeightMetadata::Decode(const FString& Json, FString& OutSourceId, TMap<int32, FRXStoredLODWeights>& OutLODs,
    int32& OutImpostorLOD, int32& OutImpostorMaterialSlot, FString& Error, int32* OutImpostorDirections)
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) { Error = TEXT("Invalid weight metadata JSON."); return false; }
    int32 Version = 0;
    FString SourceId;
    const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
    if (!Root->TryGetNumberField(TEXT("version"), Version) || (Version < 1 || Version > 5) ||
        !Root->TryGetStringField(TEXT("source_id"), SourceId) || SourceId.IsEmpty() ||
        !Root->TryGetArrayField(TEXT("lods"), Entries) || Entries->Num() > 32)
    { Error = TEXT("Unsupported or incomplete weight metadata."); return false; }
    int32 Directions = 4;
    if (!RXImpostorDirections::Read(*Root, Version, Directions)) { Error = TEXT("Impostor directions must be 2 or 4."); return false; }
    TMap<int32, FRXStoredLODWeights> LODs;
    int32 ImpostorLOD = INDEX_NONE;
    int32 ImpostorMaterialSlot = INDEX_NONE;
    const bool bHasLOD = Root->TryGetNumberField(TEXT("impostor_lod"), ImpostorLOD);
    const bool bHasSlot = Root->TryGetNumberField(TEXT("impostor_material_slot"), ImpostorMaterialSlot);
    if (bHasLOD != bHasSlot || ImpostorLOD < INDEX_NONE || ImpostorMaterialSlot < INDEX_NONE || (ImpostorLOD < 0) != (ImpostorMaterialSlot < 0) || ImpostorLOD == 0)
    { Error = TEXT("Invalid impostor metadata."); return false; }
    for (const TSharedPtr<FJsonValue>& Value : *Entries)
    {
        const TSharedPtr<FJsonObject>* Entry = nullptr;
        int32 Index = 0;
        FString SettingsJson;
        const TArray<TSharedPtr<FJsonValue>>* Sections = nullptr;
        if (!Value.IsValid() || !Value->TryGetObject(Entry) || !Entry || !Entry->IsValid() ||
            !(*Entry)->TryGetNumberField(TEXT("lod"), Index) || Index < 1 || Index >= 32 || LODs.Contains(Index) ||
            !(*Entry)->TryGetStringField(TEXT("settings"), SettingsJson) ||
            !(*Entry)->TryGetArrayField(TEXT("sections"), Sections) || Sections->IsEmpty() || Sections->Num() > 4096)
        { Error = TEXT("Invalid LOD entry in weight metadata."); return false; }
        FRXStoredLODWeights LOD;
        if (!FJsonObjectConverter::JsonObjectStringToUStruct(SettingsJson, &LOD.Settings))
        { Error = TEXT("Invalid reduction settings in metadata."); return false; }
        TSet<int32> Seen;
        for (const TSharedPtr<FJsonValue>& SectionValue : *Sections)
        {
            const TSharedPtr<FJsonObject>* Row = nullptr;
            int32 Group = -1;
            double Weight = -1;
            bool bExcluded = false;
            if (!SectionValue.IsValid() || !SectionValue->TryGetObject(Row) || !Row || !Row->IsValid() ||
                !(*Row)->TryGetNumberField(TEXT("group"), Group) || Group < 0 || Seen.Contains(Group) ||
                !(*Row)->TryGetNumberField(TEXT("weight"), Weight) || !FMath::IsFinite(Weight) || Weight < 0 || Weight > 1 ||
                !(*Row)->TryGetBoolField(TEXT("excluded"), bExcluded))
            { Error = TEXT("Invalid section entry in weight metadata."); return false; }
            FRXSectionWeight Settings{ FPolygonGroupID(Group), Version == 1 ? 1.0f - float(Weight) : float(Weight), bExcluded };
            if (!RXSilhouetteSettings::Read(**Row, Version, Settings))
            { Error = TEXT("Invalid silhouette preservation flags."); return false; }
            Seen.Add(Group); LOD.Sections.Add(Settings);
        }
        LODs.Add(Index, MoveTemp(LOD));
    }
    OutSourceId = MoveTemp(SourceId);
    OutLODs = MoveTemp(LODs);
    OutImpostorLOD = ImpostorLOD;
    OutImpostorMaterialSlot = ImpostorMaterialSlot;
    if (OutImpostorDirections) { *OutImpostorDirections = Directions; }
    Error.Reset();
    return true;
}
