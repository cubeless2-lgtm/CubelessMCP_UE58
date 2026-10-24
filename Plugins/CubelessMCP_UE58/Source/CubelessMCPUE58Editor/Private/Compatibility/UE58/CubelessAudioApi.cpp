// Original focused audio inspection and one-Wave-player SoundCue creation.
#include "Compatibility/UE58/CubelessAudioApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Factories/SoundCueFactoryNew.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "IAssetTools.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNode.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundWave.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::Audio
{
namespace
{
constexpr int32 MaxProperties = 256, MaxText = 4 * 1024 * 1024, MaxNodes = 128, MaxPins = 1024, MaxPackages = 4096;
constexpr int64 MaxDiskBytes = 512 * 1024 * 1024;
struct FResult
{
    FString Operation, Stage = TEXT("preflight"), Code, Error;
    bool bSuccess = false, bAttempted = false, bKnown = true, bChanged = false, bPost = false;
    bool bSaveRequested = false, bSaveAttempted = false, bSaved = false;
    TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
};
FString Text(const TSharedRef<FJsonObject>& J)
{ FString S; FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&S)); return S; }
bool Fail(FResult& R, const TCHAR* Code, const FString& Error)
{ R.Code = Code; R.Error = Error; R.bSuccess = false; return false; }
FString Encode(const FResult& R)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("operation"), R.Operation); J->SetStringField(TEXT("stage"), R.Stage);
    J->SetBoolField(TEXT("success"), R.bSuccess); J->SetStringField(TEXT("error_code"), R.Code); J->SetStringField(TEXT("error"), R.Error);
    J->SetBoolField(TEXT("mutation_attempted"), R.bAttempted); J->SetBoolField(TEXT("change_state_known"), R.bKnown);
    if (R.bKnown) { J->SetBoolField(TEXT("changed"), R.bChanged); } else { J->SetField(TEXT("changed"), MakeShared<FJsonValueNull>()); }
    J->SetBoolField(TEXT("outcome_unknown"), !R.bKnown); J->SetBoolField(TEXT("postcondition_passed"), R.bPost);
    J->SetBoolField(TEXT("compile_requested"), false); J->SetBoolField(TEXT("compile_attempted"), false); J->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    J->SetBoolField(TEXT("save_requested"), R.bSaveRequested); J->SetBoolField(TEXT("save_attempted"), R.bSaveAttempted); J->SetBoolField(TEXT("saved"), R.bSaved);
    J->SetBoolField(TEXT("atomic_rollback_available"), false); J->SetBoolField(TEXT("auto_retry_safe"), false); J->SetObjectField(TEXT("data"), R.Data);
    return Text(J);
}
void Init(FResult& R, const TCHAR* Operation)
{
    R.Operation = Operation;
    for (const TCHAR* Key : { TEXT("asset_load_call_count"), TEXT("asset_factory_call_count"), TEXT("wave_player_creation_count"), TEXT("link_graph_call_count"),
        TEXT("post_edit_change_call_count"), TEXT("duration_getter_call_count"), TEXT("save_attempt_count"), TEXT("asset_registry_refresh_calls"), TEXT("asset_registry_refresh_file_count") }) { R.Data->SetNumberField(Key, 0); }
    R.Data->SetArrayField(TEXT("asset_load_calls"), {}); R.Data->SetArrayField(TEXT("save_calls"), {});
    R.Data->SetBoolField(TEXT("observations_complete"), false); R.Data->SetBoolField(TEXT("playback_verified"), false);
    R.Data->SetBoolField(TEXT("metasound_builder_implemented"), false); R.Data->SetBoolField(TEXT("automatic_cleanup_attempted"), false);
    R.Data->SetStringField(TEXT("dependency_tier"), TEXT("EngineAPI: public Engine/AssetRegistry/AssetTools/AudioEditor/EditorAssetLibrary; thin hidden Editor wrapper, no official Audio Toolset delegate or ServerRPC."));
    R.Data->SetStringField(TEXT("effect_scope"), TEXT("Initial public asset resolution/duration getter may run PostLoad and cache work. Fresh SoundCue public factory/graph linking/PostEditChange can update native caches/selection/transactions. Explicit save receipts are distinct; no audio playback, runtime helper, rollback or implicit-work instrumentation."));
}
bool Loaded(const UObject* O)
{ return IsValid(O) && !O->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_BeginDestroyed | RF_FinishDestroyed); }
bool Native(UClass* C, const FString& Package, bool bAbstract = false)
{ return Loaded(C) && C->HasAnyClassFlags(CLASS_Native) && !C->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists)
    && (bAbstract || !C->HasAnyClassFlags(CLASS_Abstract)) && C->GetOutermost()->GetName() == Package
    && C->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn) && !C->GetOutermost()->IsDirty(); }
TSharedRef<FJsonObject> Identity(const UObject* O)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("path"), O ? O->GetPathName() : FString()); J->SetStringField(TEXT("class"), O ? O->GetClass()->GetPathName() : FString());
    J->SetStringField(TEXT("outer"), O && O->GetOuter() ? O->GetOuter()->GetPathName() : FString());
    J->SetStringField(TEXT("object_flags"), O ? FString::Printf(TEXT("%llu"), static_cast<uint64>(O->GetFlags())) : TEXT("0")); return J;
}
bool Properties(UObject* O, TSharedRef<FJsonObject>& J,const TFunction<bool()>& Current={})
{
    if(Current)
    {
        if(!Current()||!Loaded(O))return false;TStrongObjectPtr<UObject> Owner(O);TStrongObjectPtr<UClass> Class(O->GetClass());
        struct FField{FProperty* P=nullptr;FName Name;FFieldClass* Kind=nullptr;UStruct* Declaring=nullptr;int32 Offset=0,Size=0,Dim=0;uint64 Flags=0;};
        TArray<FField> Fields;
        for(TFieldIterator<FProperty> It(Class.Get(),EFieldIteratorFlags::IncludeSuper);It;++It)
        {auto* P=*It;if(Fields.Num()>=MaxProperties||P->ArrayDim<1||P->ArrayDim>64||P->HasSetterOrGetter())return false;FField F;F.P=P;F.Name=P->GetFName();F.Kind=P->GetClass();F.Declaring=P->GetOwnerStruct();F.Offset=P->GetOffset_ForInternal();F.Size=P->GetSize();F.Dim=P->ArrayDim;F.Flags=uint64(P->GetPropertyFlags());Fields.Add(F);}
        const auto FieldsCurrent=[&]()->bool
        {
            if(!Current()||!Loaded(Owner.Get())||Owner->GetClass()!=Class.Get())return false;
            TArray<FProperty*> Now;for(TFieldIterator<FProperty> It(Class.Get(),EFieldIteratorFlags::IncludeSuper);It;++It){if(Now.Num()>=MaxProperties)return false;Now.Add(*It);}
            if(Now.Num()!=Fields.Num())return false;
            for(int32 I=0;I<Fields.Num();++I){const auto& F=Fields[I];if(Now[I]!=F.P)return false;auto* P=Now[I];if(P->GetFName()!=F.Name||P->GetClass()!=F.Kind||P->GetOwnerStruct()!=F.Declaring||P->GetOffset_ForInternal()!=F.Offset||P->GetSize()!=F.Size||P->ArrayDim!=F.Dim||uint64(P->GetPropertyFlags())!=F.Flags||P->HasSetterOrGetter())return false;}
            return true;
        };
        int32 Chars=0;
        for(const auto& F:Fields)
        {
            TArray<TSharedPtr<FJsonValue>> Values;
            for(int32 I=0;I<F.Dim;++I){if(!FieldsCurrent())return false;FString V;const void* At=F.P->ContainerPtrToValuePtr<void>(Owner.Get(),I);const bool Exported=F.P->ExportText_Direct(V,At,At,Owner.Get(),PPF_None);if(!FieldsCurrent()||!Exported||(Chars+=V.Len())>MaxText)return false;Values.Add(MakeShared<FJsonValueString>(V));}
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("owner"),F.Declaring->GetPathName());Row->SetStringField(TEXT("class"),F.Kind->GetName());Row->SetStringField(TEXT("flags"),LexToString(F.Flags));Row->SetNumberField(TEXT("array_dim"),F.Dim);Row->SetArrayField(TEXT("values"),Values);J->SetObjectField(F.Name.ToString(),Row);
        }
        return FieldsCurrent();
    }
    if (!Loaded(O)) { return false; } int32 Count = 0, Chars = 0;
    for (TFieldIterator<FProperty> It(O->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
    {
        FProperty* P = *It; if (++Count > MaxProperties || P->ArrayDim < 1 || P->ArrayDim > 64) { return false; }
        TArray<TSharedPtr<FJsonValue>> Values;
        for (int32 Index = 0; Index < P->ArrayDim; ++Index)
        { FString Value; const void* Address = P->ContainerPtrToValuePtr<void>(O, Index);
            if (!P->ExportText_Direct(Value, Address, Address, O, PPF_None) || (Chars += Value.Len()) > MaxText) { return false; } Values.Add(MakeShared<FJsonValueString>(Value)); }
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("owner"), P->GetOwnerStruct()->GetPathName()); Row->SetStringField(TEXT("class"), P->GetClass()->GetName());
        Row->SetStringField(TEXT("flags"), FString::Printf(TEXT("%llu"), static_cast<uint64>(P->GetPropertyFlags()))); Row->SetNumberField(TEXT("array_dim"), P->ArrayDim);
        Row->SetArrayField(TEXT("values"), Values); J->SetObjectField(P->GetName(), Row);
    } return true;
}
bool Disk(const FString& Package, TSharedRef<FJsonObject>& J)
{
    FString Filename; if (!FPackageName::TryConvertLongPackageNameToFilename(Package, Filename, FPackageName::GetAssetPackageExtension())) { return false; }
    Filename = FPaths::ConvertRelativePathToFull(Filename); const int64 Size = IFileManager::Get().FileSize(*Filename);
    J->SetStringField(TEXT("path"), Filename); J->SetBoolField(TEXT("exists"), Size >= 0); J->SetNumberField(TEXT("bytes"), FMath::Max<int64>(0, Size));
    if (Size < 0) { J->SetStringField(TEXT("sha1"), TEXT("")); return true; }
    TArray<uint8> Bytes; if (Size > MaxDiskBytes || !FFileHelper::LoadFileToArray(Bytes, *Filename) || Bytes.Num() != Size) { return false; }
    uint8 Digest[FSHA1::DigestSize]; FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Digest); J->SetStringField(TEXT("sha1"), BytesToHex(Digest, FSHA1::DigestSize)); return true;
}
bool Dirty(const FString& Excluded, TSharedRef<FJsonObject>& J)
{ int32 Count = 0; for (TObjectIterator<UPackage> It; It; ++It)
    { if (!Loaded(*It) || It->GetName() == Excluded) { continue; } if (++Count > MaxPackages) { return false; } J->SetBoolField(It->GetPathName(), It->IsDirty()); } return true; }
// YG source preparation uses the existing public resolved-path reader pattern.
template<typename T>
UObject* CreationSourcePointer(const TObjectPtr<T>& Ref)
{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
    if(!IsEditorGameThread()||IsPlaySessionActive()||!Ref.IsResolved()||Ref==nullptr)return nullptr;
    const FString Path=Ref.GetPathName();UObject* Found=StaticFindObject(nullptr,nullptr,*Path);
    return Loaded(Found)&&Ref==Found?Found:nullptr;
#else
    return nullptr;
#endif
}
bool CreationSourceSuper(UClass* Class,UClass*& Out)
{
    Out=nullptr;if(!Loaded(Class))return false;
    static FObjectProperty* const P=FindFProperty<FObjectProperty>(UStruct::StaticClass(),TEXT("SuperStruct"));
    if(!P||P->GetClass()!=FObjectProperty::StaticClass()||P->GetOwnerStruct()!=UStruct::StaticClass()
        ||P->GetOwnerProperty()!=P||P->ArrayDim!=1||P->ElementSize!=sizeof(TObjectPtr<UStruct>)
        ||P->GetSize()!=sizeof(TObjectPtr<UStruct>)||P->GetMinAlignment()!=alignof(TObjectPtr<UStruct>)
        ||P->GetOffset_ForInternal()<0||int64(P->GetOffset_ForInternal())+P->GetSize()>UStruct::StaticClass()->GetPropertiesSize()
        ||P->HasSetterOrGetter()||!P->PropertyClass.IsResolved()||P->PropertyClass!=UStruct::StaticClass())return false;
    const TObjectPtr<UObject> Slot=P->GetObjectPtrPropertyValue(P->ContainerPtrToValuePtr<void>(Class));
    if(!Slot.IsResolved())return false;if(Slot==nullptr)return true;
    Out=Cast<UClass>(CreationSourcePointer(Slot));return Out!=nullptr;
}
bool Source(UClass* C, TSharedRef<FJsonObject>& J,const TFunction<bool()>& Current={})
{
    if ((Current&&!Current())||!Loaded(C) || !C->HasAnyClassFlags(CLASS_Native) || C->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists)
        || !C->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn) || (!Current&&C->GetOutermost()->IsDirty())) { return false; }
    const auto ID = Identity(C); for (const auto& Pair : ID->Values) { J->SetField(Pair.Key, Pair.Value); }
    J->SetStringField(TEXT("class_flags"), FString::Printf(TEXT("%llu"), static_cast<uint64>(C->GetClassFlags())));
    UObject* CDO = C->GetDefaultObject(false); if(Current&&!Current())return false; J->SetBoolField(TEXT("cdo_exists"), CDO != nullptr);
    if (CDO) { if (!Loaded(CDO) || CDO->GetClass() != C || CDO->GetOuter() != C->GetOuter() || !CDO->HasAnyFlags(RF_ClassDefaultObject)) { return false; }
        auto P = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("existing_cdo"), Identity(CDO)); J->SetObjectField(TEXT("existing_cdo_properties"), P); if (!Properties(CDO, P,Current)) { return false; } }
    return true;
}
bool Forbidden(const FString& S)
{ for (const TCHAR* K : { TEXT("UnrealMCP"), TEXT("MCPUnreal"), TEXT("mcp_unreal"), TEXT("CubelessControlRigTools"), TEXT("/CubelessMCP_UE58/"), TEXT("/Script/CubelessMCPUE58Editor"),
    TEXT("/ModelContextProtocol/"), TEXT("/ToolsetRegistry/"), TEXT("/PythonScriptPlugin/"), TEXT("/EditorScriptingUtilities/"),
    TEXT("/Script/ModelContextProtocol"), TEXT("/Script/ToolsetRegistry"), TEXT("/Script/PythonScriptPlugin"), TEXT("/Script/EditorScriptingUtilities") })
    { if (S.Contains(K, ESearchCase::IgnoreCase)) { return true; } } return false; }
bool Path(const FString& Input, FString& Out)
{
    Out = FPackageName::ExportTextPathToObjectPath(Input).TrimStartAndEnd(); Out.TrimQuotesInline(); Out.ReplaceInline(TEXT("\\"), TEXT("/"));
    if (Out.StartsWith(TEXT("/")) && !Out.Contains(TEXT("."))) { Out += TEXT(".") + FPackageName::GetLongPackageAssetName(Out); }
    FString Package, Asset; return Out.Len() < NAME_SIZE && !Forbidden(Out) && FPackageName::IsValidObjectPath(Out)
        && Out.Split(TEXT("."), &Package, &Asset, ESearchCase::CaseSensitive, ESearchDir::FromEnd)
        && Asset == FPackageName::GetLongPackageAssetName(Package) && (Package.StartsWith(TEXT("/Game/")) || Package.StartsWith(TEXT("/Engine/")));
}
UObject* Resolve(const FString& Path, FResult& R)
{
    UObject* Before = FindObject<UObject>(nullptr, *Path); UObject* O = UEditorAssetLibrary::LoadAsset(Path);
    auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("path"), Path); Row->SetBoolField(TEXT("already_loaded"), Before != nullptr); Row->SetObjectField(TEXT("returned_object"), Identity(O));
    auto Rows = R.Data->GetArrayField(TEXT("asset_load_calls")); Rows.Add(MakeShared<FJsonValueObject>(Row)); R.Data->SetArrayField(TEXT("asset_load_calls"), Rows); R.Data->SetNumberField(TEXT("asset_load_call_count"), Rows.Num());
    return Loaded(O) && O->GetPathName() == Path && O->GetOuter() == O->GetOutermost() && !O->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject)
        && !O->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn | PKG_EditorOnly | PKG_PlayInEditor) ? O : nullptr;
}
bool Graph(USoundCue* Cue, TSharedRef<FJsonObject>& J,const TFunction<bool()>& Current={})
{
    if(Current&&(!Current()||!Cue->SoundCueGraph.IsResolved()))return false;
    UEdGraph* G = Cue->SoundCueGraph.Get();TStrongObjectPtr<UEdGraph> KeepGraph(Current?G:nullptr);
    TArray<TStrongObjectPtr<UEdGraphNode>> KeepNodes;TArray<UEdGraphNode*> ExpectedNodes;
    TStrongObjectPtr<UClass> GraphClass,SchemaClass;TStrongObjectPtr<UPackage> GraphPackage;FString GraphPath;
    UObject* SchemaOuter=nullptr;UPackage* SchemaPackage=nullptr;FString SchemaPath;UClass* SchemaKind=nullptr;
    if(Current&&G)
    {
        if(!Loaded(G)||G->GetOuter()!=Cue||!Native(G->GetClass(),TEXT("/Script/AudioEditor"))||G->Nodes.Num()>MaxNodes||G->SubGraphs.Num()!=0||!G->Schema.GetGCPtr().IsResolved())return false;
        GraphClass.Reset(G->GetClass());GraphPackage.Reset(G->GetPackage());GraphPath=G->GetPathName();SchemaClass.Reset(G->Schema.GetGCPtr().Get());
        if(SchemaClass.IsValid()){if(!Loaded(SchemaClass.Get())||!Loaded(SchemaClass->GetPackage()))return false;SchemaOuter=SchemaClass->GetOuter();SchemaPackage=SchemaClass->GetPackage();SchemaPath=SchemaClass->GetPathName();SchemaKind=SchemaClass->GetClass();}
        for(const auto& Slot:G->Nodes){if(!Slot.IsResolved()||!Loaded(Slot.Get()))return false;KeepNodes.Emplace(Slot.Get());ExpectedNodes.Add(Slot.Get());}
    }
    const TFunction<bool()> GraphCurrent=Current?TFunction<bool()>([&]()->bool
    {
        if(!Current()||!Loaded(G)||!Loaded(GraphClass.Get())||!Loaded(GraphPackage.Get())||!Cue->SoundCueGraph.IsResolved()||Cue->SoundCueGraph.Get()!=G
            ||G->GetClass()!=GraphClass.Get()||G->GetOuter()!=Cue||G->GetPackage()!=GraphPackage.Get()||G->GetPathName()!=GraphPath||!Native(GraphClass.Get(),TEXT("/Script/AudioEditor"))
            ||!G->Schema.GetGCPtr().IsResolved()||G->Schema.GetGCPtr().Get()!=SchemaClass.Get()||G->SubGraphs.Num()!=0||G->Nodes.Num()!=ExpectedNodes.Num())return false;
        if(SchemaClass.IsValid()&&(!Loaded(SchemaClass.Get())||!Loaded(SchemaPackage)||(SchemaOuter&&!Loaded(SchemaOuter))||SchemaClass->GetClass()!=SchemaKind||SchemaClass->GetOuter()!=SchemaOuter||SchemaClass->GetPackage()!=SchemaPackage||SchemaClass->GetPathName()!=SchemaPath))return false;
        for(int32 I=0;I<ExpectedNodes.Num();++I)if(!G->Nodes[I].IsResolved()||G->Nodes[I].Get()!=ExpectedNodes[I]||!Loaded(ExpectedNodes[I])||ExpectedNodes[I]->GetOuter()!=G)return false;
        return true;
    }):TFunction<bool()>();
    J->SetBoolField(TEXT("exists"), G != nullptr); if (!G) { return true; }
    if (!Loaded(G) || G->GetOuter() != Cue || !Native(G->GetClass(), TEXT("/Script/AudioEditor")) || G->Nodes.Num() > MaxNodes || G->SubGraphs.Num() != 0) { return false; }
    J->SetObjectField(TEXT("identity"), Identity(G)); J->SetStringField(TEXT("schema"), G->Schema ? G->Schema->GetPathName() : FString());
    TSet<const UEdGraphPin*> Pins; TSet<const UEdGraphNode*> Nodes; TSet<FGuid> Guids;
    for (UEdGraphNode* N : G->Nodes)
    { if (!Loaded(N) || N->GetOuter() != G || !Native(N->GetClass(), TEXT("/Script/AudioEditor")) || Nodes.Contains(N) || !N->NodeGuid.IsValid() || Guids.Contains(N->NodeGuid)) { return false; }
        Nodes.Add(N); Guids.Add(N->NodeGuid); for (const UEdGraphPin* P : N->Pins)
        { if (!P || P->GetOwningNodeUnchecked() != N || Pins.Contains(P) || !P->PinId.IsValid() || P->bOrphanedPin || Pins.Num() >= MaxPins) { return false; } Pins.Add(P); } }
    TArray<TSharedPtr<FJsonValue>> Rows; int32 Links = 0;
    for (UEdGraphNode* N : G->Nodes)
    {
        auto Row = Identity(N); Row->SetStringField(TEXT("guid"), N->NodeGuid.ToString()); auto P = MakeShared<FJsonObject>(); if (!Properties(N, P,GraphCurrent)) { return false; } Row->SetObjectField(TEXT("properties"), P);
        if(Current)
        {
            // ExportText can reconstruct pin allocations. Rebuild the current set
            // before dereferencing links, without carrying old pin pointers across it.
            Pins.Reset();
            for(auto* CurrentNode:ExpectedNodes)for(auto* Port:CurrentNode->Pins){if(!Port||Port->GetOwningNodeUnchecked()!=CurrentNode||Pins.Contains(Port)||Pins.Num()>=MaxPins||Port->WasTrashed())return false;Pins.Add(Port);}
        }
        TArray<TSharedPtr<FJsonValue>> PortRows;
        for (const UEdGraphPin* Pin : N->Pins)
        {
            auto Port = MakeShared<FJsonObject>(); Port->SetStringField(TEXT("id"), Pin->PinId.ToString()); Port->SetStringField(TEXT("name"), Pin->PinName.ToString()); Port->SetNumberField(TEXT("direction"), static_cast<int32>(Pin->Direction));
            TArray<TSharedPtr<FJsonValue>> Neighbors;
            for (const UEdGraphPin* Other : Pin->LinkedTo)
            { if (!Other || !Pins.Contains(Other) || Other->Direction == Pin->Direction || !Other->LinkedTo.Contains(const_cast<UEdGraphPin*>(Pin))) { return false; }
                Neighbors.Add(MakeShared<FJsonValueString>(Other->GetOwningNodeUnchecked()->GetPathName() + TEXT(":") + Other->PinId.ToString())); ++Links; }
            Port->SetArrayField(TEXT("links"), Neighbors); PortRows.Add(MakeShared<FJsonValueObject>(Port));
        }
        Row->SetArrayField(TEXT("pins"), PortRows); Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    J->SetArrayField(TEXT("nodes"), Rows); J->SetNumberField(TEXT("node_count"), Rows.Num()); J->SetNumberField(TEXT("physical_link_count"), Links / 2); return true;
}
struct FObservation
{
    TSharedRef<FJsonObject> State = MakeShared<FJsonObject>(), DiskState = MakeShared<FJsonObject>(), OtherDirty = MakeShared<FJsonObject>();
};
TSharedRef<FJsonObject> Observation(const FObservation& O)
{ auto J = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("state"), O.State); J->SetObjectField(TEXT("disk"), O.DiskState); J->SetObjectField(TEXT("other_package_dirty"), O.OtherDirty); return J; }
bool Capture(UObject* O, FObservation& Out,const TFunction<bool()>& Current={},const TArray<UClass*>* Chain=nullptr)
{
    if ((Current&&!Current())||!Loaded(O)) { return false; } const auto ID = Identity(O); for (const auto& Pair : ID->Values) { Out.State->SetField(Pair.Key, Pair.Value); }
    Out.State->SetBoolField(TEXT("package_dirty"), O->GetOutermost()->IsDirty());
    auto P = MakeShared<FJsonObject>(); Out.State->SetObjectField(TEXT("properties"), P);
    if (!Properties(O, P,Current) || !Disk(O->GetOutermost()->GetName(), Out.DiskState) || !Dirty(O->GetOutermost()->GetName(), Out.OtherDirty)||(Current&&!Current())) { return false; }
    auto NativeSource = MakeShared<FJsonObject>(); Out.State->SetObjectField(TEXT("native_class_source"), NativeSource); if (!Source(O->GetClass(), NativeSource,Current)) { return false; }
    if(Chain){auto Sources=MakeShared<FJsonObject>();Out.State->SetObjectField(TEXT("native_class_chain_sources"),Sources);for(auto* Class:*Chain){if(Current&&!Current())return false;auto Row=MakeShared<FJsonObject>();if(!Source(Class,Row,Current))return false;Sources->SetObjectField(Class->GetPathName(),Row);}}
    if (USoundCue* Cue = Cast<USoundCue>(O))
    {
        if (Cue->AllNodes.Num() > MaxNodes) { return false; } TArray<TSharedPtr<FJsonValue>> Nodes; TSet<const USoundNode*> Seen;
        Out.State->SetArrayField(TEXT("sound_nodes"), Nodes);
        for (USoundNode* Node : Cue->AllNodes)
        { if (!Loaded(Node) || Node->GetOuter() != Cue || !Native(Node->GetClass(), TEXT("/Script/Engine")) || Seen.Contains(Node)) { return false; } Seen.Add(Node);
            auto Row = Identity(Node); auto Props = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("properties"), Props); Nodes.Add(MakeShared<FJsonValueObject>(Row));
            Out.State->SetArrayField(TEXT("sound_nodes"), Nodes); if (!Properties(Node, Props,Current)) { return false; } }
        Out.State->SetArrayField(TEXT("sound_nodes"), Nodes); Out.State->SetStringField(TEXT("first_node"), Cue->FirstNode ? Cue->FirstNode->GetPathName() : FString());
        auto G = MakeShared<FJsonObject>(); Out.State->SetObjectField(TEXT("graph"), G); if (!Graph(Cue, G,Current)) { return false; }
    }
    return true;
}
bool Same(const FObservation& A, const FObservation& B)
{ return Text(Observation(A)) == Text(Observation(B)); }
bool OtherDirtyPreserved(const FObservation& A, const FObservation& B, const FString& TargetPackage)
{
    auto Before = MakeShared<FJsonObject>(), After = MakeShared<FJsonObject>();
    for (const auto& Pair : A.OtherDirty->Values) { if (FString(*Pair.Key) != TargetPackage) { Before->SetField(Pair.Key, Pair.Value); } }
    for (const auto& Pair : B.OtherDirty->Values) { if (FString(*Pair.Key) != TargetPackage) { After->SetField(Pair.Key, Pair.Value); } }
    return Text(Before) == Text(After);
}
bool MemoryDependencies(UObject* Asset, FResult& R, const TCHAR* Label)
{
    auto Audit = MakeShared<FJsonObject>(); R.Data->SetObjectField(Label, Audit); Audit->SetBoolField(TEXT("passed"), false);
    Audit->SetStringField(TEXT("scope"), TEXT("bounded loaded package serialized hard/class and typed soft references; external saved registry dependencies; no rescan/load/cleanup/runtime execution"));
    TArray<UObject*> Exports; GetObjectsWithPackage(Asset->GetOutermost(), Exports, EGetObjectsFlags::IncludeNestedObjects, RF_Transient, EInternalObjectFlags::Garbage);
    if (Exports.Num() > 4096) { return Fail(R, TEXT("audio_live_dependency_audit_incomplete"), TEXT("Loaded audio exports exceed the4096 bound.")); }
    TArray<FString> External; int32 Count = 0, Values = 0, SoftCount = 0; TArray<TSharedPtr<FJsonValue>> Rows;
    const auto Token = [&](const FString& PathValue)
    {
        if (Forbidden(PathValue)) { return false; }
        const FString PackageName = FPackageName::ObjectPathToPackageName(PathValue);
        if (PackageName != Asset->GetOutermost()->GetName() && (PackageName.StartsWith(TEXT("/Game/")) || PackageName.StartsWith(TEXT("/Engine/")))) { External.AddUnique(PackageName); }
        return true;
    };
    for (UObject* Export : Exports)
    {
        if (!Loaded(Export) || !Token(Export->GetPathName()) || !Token(Export->GetClass()->GetPathName()))
        { return Fail(R, TEXT("audio_live_dependency_audit_failed"), TEXT("An observed audio export/class contains a forbidden reference or is unreadable.")); }
        TArray<UObject*> References; FReferenceFinder Finder(References, nullptr, false, true, false, true); Finder.FindReferences(Export);
        Count += References.Num(); Audit->SetNumberField(TEXT("hard_reference_count"), Count);
        if (Count > 16384) { return Fail(R, TEXT("audio_live_dependency_audit_incomplete"), TEXT("Serialized audio references exceed16384.")); }
        for (UObject* Reference : References)
        {
            if (!Reference) { continue; } auto Row = Identity(Reference); Rows.Add(MakeShared<FJsonValueObject>(Row)); Audit->SetArrayField(TEXT("references"), Rows);
            if (!Loaded(Reference) || !Token(Reference->GetPathName()) || !Token(Reference->GetClass()->GetPathName()))
            { return Fail(R, TEXT("audio_live_dependency_audit_failed"), TEXT("A serialized audio reference contains an Editor bridge token or is unreadable.")); }
        }
        for (TPropertyValueIterator<const FProperty> It(Export->GetClass(), Export, EPropertyValueIteratorFlags::FullRecursion, EFieldIteratorFlags::IncludeDeprecated); It; ++It)
        {
            if (++Values > 65536) { return Fail(R, TEXT("audio_live_dependency_audit_incomplete"), TEXT("Serialized audio property values exceed65536.")); }
            const FProperty* Property = It.Key();
            if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient | CPF_SkipSerialization)) { It.SkipRecursiveProperty(); continue; }
            const FSoftObjectProperty* P = CastField<FSoftObjectProperty>(Property); if (!P || !It.Value()) { continue; }
            if (++SoftCount > 16384) { return Fail(R, TEXT("audio_live_dependency_audit_incomplete"), TEXT("Typed soft audio references exceed16384.")); }
            const FString SoftPath = P->GetPropertyValue(It.Value()).ToSoftObjectPath().ToString();
            auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("owner"), Export->GetPathName()); Row->SetStringField(TEXT("property"), P->GetName()); Row->SetStringField(TEXT("soft_path"), SoftPath);
            Rows.Add(MakeShared<FJsonValueObject>(Row)); Audit->SetArrayField(TEXT("references"), Rows);
            if (!Token(SoftPath)) { return Fail(R, TEXT("audio_live_dependency_audit_failed"), TEXT("A serialized soft audio reference contains an Editor bridge token.")); }
        }
    }
    Audit->SetNumberField(TEXT("export_count"), Exports.Num()); Audit->SetNumberField(TEXT("property_values_visited"), Values); Audit->SetNumberField(TEXT("soft_reference_count"), SoftCount);
    if (!External.IsEmpty()) { const auto Registry = ReadPackageDependencies(External, true, 4096); Audit->SetObjectField(TEXT("external_saved_registry"), Registry);
        bool Passed = false; if (!Registry->TryGetBoolField(TEXT("passed"), Passed) || !Passed)
        { return Fail(R, TEXT("audio_live_dependency_audit_incomplete"), TEXT("External saved dependency registry observation is incomplete or forbidden.")); } }
    Audit->SetBoolField(TEXT("passed"), true); return true;
}
bool RequirePersisted(UObject* O, FResult& R, const TCHAR* Label)
{
    auto D = MakeShared<FJsonObject>(); if (!Disk(O->GetOutermost()->GetName(), D) || !D->GetBoolField(TEXT("exists")))
    { return Fail(R, TEXT("audio_source_not_persisted"), TEXT("The requested dependency must have an observable saved asset file.")); }
    const auto Audit = ReadPackageDependencies({ O->GetOutermost()->GetName() }, true, 4096); R.Data->SetObjectField(Label, Audit);
    bool Passed = false; if (!Audit->TryGetBoolField(TEXT("passed"), Passed) || !Passed)
    { return Fail(R, TEXT("audio_mcp_dependency_audit_failed"), TEXT("Current persisted dependency observation is incomplete or forbidden; inspect the retained actual audit before retrying.")); }
    return MemoryDependencies(O, R, *FString::Printf(TEXT("%s_live"), Label));
}
bool Verify(USoundCue* Cue, USoundWave* Wave, const TFunction<bool()>& Current={})
{
    if(Current)
    {
        if(!Current()||!Loaded(Cue)||Cue->GetClass()!=USoundCue::StaticClass())return false;
        UObject* Root=CreationSourcePointer(Cue->FirstNode);
        if(!Loaded(Root)||Root->GetClass()!=USoundNodeWavePlayer::StaticClass()||Root->GetOuter()!=Cue)return false;
        auto* Player=static_cast<USoundNodeWavePlayer*>(Root);TStrongObjectPtr<USoundNodeWavePlayer> Hold(Player);
        if(!Current()||Cue->FirstNode!=Player||!Loaded(Player)||Player->GetOuter()!=Cue)return false;
        USoundWave* Actual=Player->GetSoundWave();
        if(!Current()||Cue->FirstNode!=Player||!Loaded(Player)||Player->GetClass()!=USoundNodeWavePlayer::StaticClass()||Player->GetOuter()!=Cue)return false;
        return Actual==Wave&&Cue->AllNodes.Num()==1&&Cue->AllNodes[0]==Player&&Player->ChildNodes.IsEmpty();
    }
    if (!Cue || Cue->GetClass() != USoundCue::StaticClass() || !Loaded(Cue)) { return false; }
    const auto* Player = Cast<USoundNodeWavePlayer>(Cue->FirstNode.Get());
    return Loaded(Player) && Player->GetClass() == USoundNodeWavePlayer::StaticClass() && Player->GetOuter() == Cue
        && Player->GetSoundWave() == Wave && Cue->AllNodes.Num() == 1 && Cue->AllNodes[0] == Player && Player->ChildNodes.IsEmpty();
}
bool CueData(USoundCue* Cue, USoundWave* Wave, bool bCreated, FResult& R, const TFunction<bool()>& Current={})
{
    TStrongObjectPtr<USoundNodeWavePlayer> RootHold;
    if(Current)
    {
        if(!Current()||!Loaded(Cue)||Cue->GetClass()!=USoundCue::StaticClass())return false;
        UObject* Root=CreationSourcePointer(Cue->FirstNode);
        if(!Loaded(Root)||Root->GetClass()!=USoundNodeWavePlayer::StaticClass()||Root->GetOuter()!=Cue)return false;
        RootHold.Reset(static_cast<USoundNodeWavePlayer*>(Root));
        if(!Current()||Cue->FirstNode!=RootHold.Get()||!Loaded(RootHold.Get())||RootHold->GetOuter()!=Cue||!Cue->AllNodes.Contains(RootHold.Get()))return false;
    }
    R.Data->SetStringField(TEXT("asset_path"), Cue->GetPathName()); R.Data->SetStringField(TEXT("class"), Cue->GetClass()->GetPathName()); R.Data->SetStringField(TEXT("sound_wave_path"), Wave->GetPathName());
    R.Data->SetStringField(TEXT("root_node"), (Current?RootHold->GetName():Cue->FirstNode->GetName())); R.Data->SetNumberField(TEXT("node_count"), Cue->AllNodes.Num());
    R.Data->SetBoolField(TEXT("created"), bCreated); R.Data->SetBoolField(TEXT("already_existed"), !bCreated); R.Data->SetBoolField(TEXT("saved"), true);
    R.Data->SetBoolField(TEXT("readback_verified"), true); R.Data->SetBoolField(TEXT("uses_unrealmcp_classes"), false);
    R.Data->SetBoolField(TEXT("mutation_attempted"), bCreated); R.Data->SetBoolField(TEXT("mutation_performed"), bCreated);
    R.Data->SetBoolField(TEXT("mutation_state_unknown"), false); R.Data->SetBoolField(TEXT("changed"), bCreated);
    auto Audit = MakeShared<FJsonObject>(); Audit->SetStringField(TEXT("status"), TEXT("passed")); Audit->SetStringField(TEXT("mode"), TEXT("require_persisted"));
    Audit->SetNumberField(TEXT("checked_asset_count"), 2); Audit->SetArrayField(TEXT("checked_assets"), { MakeShared<FJsonValueString>(Wave->GetPathName()), MakeShared<FJsonValueString>(Cue->GetPathName()) });
    R.Data->SetObjectField(TEXT("dependency_audit"), Audit);
    auto Verification = MakeShared<FJsonObject>(); Verification->SetStringField(TEXT("status"), TEXT("passed")); Verification->SetStringField(TEXT("method"), TEXT("ue_5_8_native_single_wave_sound_cue_saved_readback")); R.Data->SetObjectField(TEXT("verification"), Verification);
    return !Current||Current();
}
bool IdentityAfterSave(const TSharedPtr<FJsonObject>& Before, const TSharedPtr<FJsonObject>& After)
{
    if (!Before.IsValid() || !After.IsValid()) { return false; }
    for (const TCHAR* Key : { TEXT("path"), TEXT("class"), TEXT("outer") }) { if (Before->GetStringField(Key) != After->GetStringField(Key)) { return false; } }
    const FString Flags = Before->GetStringField(TEXT("object_flags")); const uint64 Old = FCString::Strtoui64(*Flags, nullptr, 10);
    return Flags == FString::Printf(TEXT("%llu"), Old) && After->GetStringField(TEXT("object_flags")) == FString::Printf(TEXT("%llu"), Old | static_cast<uint64>(RF_WasLoaded | RF_LoadCompleted));
}
bool SavedState(const FObservation& Before, const FObservation& After)
{
    auto Expected = MakeShared<FJsonObject>(); for (const auto& Pair : Before.State->Values) { Expected->SetField(Pair.Key, Pair.Value); }
    if (!IdentityAfterSave(Before.State, After.State)) { return false; }
    Expected->SetStringField(TEXT("object_flags"), After.State->GetStringField(TEXT("object_flags"))); Expected->SetBoolField(TEXT("package_dirty"), false);
    // Only actual target-owned node/graph identities receive Engine save-load flags.
    TArray<TSharedPtr<FJsonValue>> NewNodes;
    const auto& OldNodes = Before.State->GetArrayField(TEXT("sound_nodes")); const auto& AfterNodes = After.State->GetArrayField(TEXT("sound_nodes")); if (OldNodes.Num() != AfterNodes.Num()) { return false; }
    for (int32 Index = 0; Index < OldNodes.Num(); ++Index)
    { const auto A = OldNodes[Index]->AsObject(), B = AfterNodes[Index]->AsObject(); if (!IdentityAfterSave(A, B)) { return false; }
        auto Row = MakeShared<FJsonObject>(); for (const auto& Pair : A->Values) { Row->SetField(Pair.Key, Pair.Value); } Row->SetStringField(TEXT("object_flags"), B->GetStringField(TEXT("object_flags"))); NewNodes.Add(MakeShared<FJsonValueObject>(Row)); }
    Expected->SetArrayField(TEXT("sound_nodes"), NewNodes);
    const auto OldGraph = Before.State->GetObjectField(TEXT("graph")), NewGraph = After.State->GetObjectField(TEXT("graph"));
    auto G = MakeShared<FJsonObject>(); for (const auto& Pair : OldGraph->Values) { G->SetField(Pair.Key, Pair.Value); }
    if (OldGraph->GetBoolField(TEXT("exists")))
    {
        if (!NewGraph->GetBoolField(TEXT("exists")) || !IdentityAfterSave(OldGraph->GetObjectField(TEXT("identity")), NewGraph->GetObjectField(TEXT("identity")))) { return false; }
        G->SetObjectField(TEXT("identity"), NewGraph->GetObjectField(TEXT("identity")));
        const auto& A = OldGraph->GetArrayField(TEXT("nodes")); const auto& B = NewGraph->GetArrayField(TEXT("nodes")); if (A.Num() != B.Num()) { return false; }
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (int32 Index = 0; Index < A.Num(); ++Index)
        { if (!IdentityAfterSave(A[Index]->AsObject(), B[Index]->AsObject())) { return false; } auto Row = MakeShared<FJsonObject>(); for (const auto& Pair : A[Index]->AsObject()->Values) { Row->SetField(Pair.Key, Pair.Value); }
            Row->SetStringField(TEXT("object_flags"), B[Index]->AsObject()->GetStringField(TEXT("object_flags"))); Rows.Add(MakeShared<FJsonValueObject>(Row)); }
        G->SetArrayField(TEXT("nodes"), Rows);
    }
    Expected->SetObjectField(TEXT("graph"), G); return Text(Expected) == Text(After.State) && Text(Before.OtherDirty) == Text(After.OtherDirty);
}
}

bool ValidateNativeAssetReferences(UObject* Asset, TSharedRef<FJsonObject>& Out, FString& Error)
{
    Out->SetBoolField(TEXT("passed"), false);
    if (!IsEditorGameThread() || IsPlaySessionActive() || !Loaded(Asset) || Asset->GetOuter() != Asset->GetOutermost() || Asset->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject)
        || Asset->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn | PKG_EditorOnly | PKG_PlayInEditor)
        || !((Asset->GetClass() == USoundWave::StaticClass() || Asset->GetClass() == USoundCue::StaticClass()) && Native(Asset->GetClass(), TEXT("/Script/Engine")))
            && !(Asset->GetClass()->GetPathName() == TEXT("/Script/MetasoundEngine.MetaSoundSource") && Native(Asset->GetClass(), TEXT("/Script/MetasoundEngine"))))
    { Error = TEXT("Internal reference audit accepts only exact ordinary loaded native Wave/Cue/MetaSoundSource assets."); return false; }
    FResult R; Init(R, TEXT("internal_audio_reference_observation"));
    const bool Passed = MemoryDependencies(Asset, R, TEXT("reference_observation"));
    Out = R.Data->GetObjectField(TEXT("reference_observation")).ToSharedRef(); Error = R.Error; return Passed;
}

bool RefreshSavedAssetRegistry(UObject* Asset, TSharedRef<FJsonObject>& Out, FString& Error)
{
    Out->SetNumberField(TEXT("scan_call_count"), 0); Out->SetNumberField(TEXT("file_count"), 0);
    Out->SetBoolField(TEXT("scan_returned"), false); Out->SetBoolField(TEXT("asset_state_change_known"), false); Out->SetBoolField(TEXT("asset_state_preserved"), false);
    Out->SetBoolField(TEXT("registry_cache_mutation_attempted"), false); Out->SetBoolField(TEXT("registry_cache_change_state_known"), false); Out->SetField(TEXT("registry_cache_changed"), MakeShared<FJsonValueNull>());
    Out->SetBoolField(TEXT("implicit_delegate_and_background_effects_measured"), false);
    Out->SetStringField(TEXT("api"), TEXT("IAssetRegistry::ScanSynchronous(empty_dirs, one_saved_file, ForceRescan)"));
    Out->SetBoolField(TEXT("directory_scan_requested"), false); Out->SetBoolField(TEXT("wait_for_in_memory_objects_requested"), false); Out->SetBoolField(TEXT("global_wait_requested"), false);
    Out->SetStringField(TEXT("scope"), TEXT("one supplied persisted Game audio file; public synchronous gather may advance existing gather/premade work and broadcast registry/telemetry events; no total-global-cache mutation or elapsed-time bound is asserted"));
    IAssetRegistry* Registry = IAssetRegistry::Get();
    if (!IsEditorGameThread() || IsPlaySessionActive() || !Registry || Registry->IsLoadingAssets() || !Loaded(Asset) || Asset->GetOuter() != Asset->GetOutermost()
        || Asset->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject) || Asset->GetOutermost()->IsDirty() || !Asset->GetOutermost()->GetName().StartsWith(TEXT("/Game/"))
        || Asset->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn | PKG_EditorOnly | PKG_PlayInEditor)
        || !(((Asset->GetClass() == USoundCue::StaticClass() || Asset->GetClass() == USoundWave::StaticClass()) && Native(Asset->GetClass(), TEXT("/Script/Engine")))
            || (Asset->GetClass()->GetPathName() == TEXT("/Script/MetasoundEngine.MetaSoundSource") && Native(Asset->GetClass(), TEXT("/Script/MetasoundEngine")))))
    { Error = TEXT("Exact ordinary loaded clean persisted Game native audio asset and idle existing registry required; no scan dispatched."); return false; }
    FObservation Before; const bool BeforeComplete = Capture(Asset, Before); Out->SetObjectField(TEXT("before"), Observation(Before)); Out->SetBoolField(TEXT("before_complete"), BeforeComplete);
    if (!BeforeComplete || !Before.DiskState->GetBoolField(TEXT("exists")) || Before.DiskState->GetNumberField(TEXT("bytes")) <= 0)
    { Error = TEXT("Complete saved audio/source/disk/other-dirty baseline required before exact-file registry refresh."); return false; }
    const FString Filename = Before.DiskState->GetStringField(TEXT("path")); FString ExpectedFilename;
    if (!FPackageName::TryConvertLongPackageNameToFilename(Asset->GetOutermost()->GetName(), ExpectedFilename, FPackageName::GetAssetPackageExtension())
        || Filename != FPaths::ConvertRelativePathToFull(ExpectedFilename) || FPaths::GetExtension(Filename, true) != FPackageName::GetAssetPackageExtension())
    { Error = TEXT("One selected exact saved .uasset file must agree with the mounted Game package identity; no scan dispatched."); return false; }
    Out->SetStringField(TEXT("asset_path"), Asset->GetPathName()); Out->SetStringField(TEXT("package_name"), Asset->GetOutermost()->GetName()); Out->SetArrayField(TEXT("files"), { MakeShared<FJsonValueString>(Filename) });
    Out->SetNumberField(TEXT("file_count"), 1); Out->SetNumberField(TEXT("scan_call_count"), 1); Out->SetBoolField(TEXT("registry_cache_mutation_attempted"), true);
    const double Started = FPlatformTime::Seconds();
    // ScanFilesSynchronous implicitly adds WaitForInMemoryObjects (all loaded tag updates).
    // The public general scan with no directories and one file omits that unrelated work.
    Registry->ScanSynchronous(TArray<FString>(), { Filename }, UE::AssetRegistry::EScanFlags::ForceRescan);
    Out->SetBoolField(TEXT("scan_returned"), true); Out->SetNumberField(TEXT("elapsed_seconds"), FPlatformTime::Seconds() - Started);
    FObservation After; const bool Complete = Capture(Asset, After); Out->SetObjectField(TEXT("after"), Observation(After)); Out->SetBoolField(TEXT("after_complete"), Complete);
    Out->SetBoolField(TEXT("asset_state_change_known"), Complete); const bool Preserved = Complete && Same(Before, After); Out->SetBoolField(TEXT("asset_state_preserved"), Preserved);
    if (!Complete || !Preserved) { Error = TEXT("Exact-file registry call returned; raw asset/source/disk/other-dirty prefix retained but preservation is incomplete or failed. No further save or scan retry."); return false; }
    TArray<FName> Dependencies; const bool Available = Registry->GetDependencies(FName(*Asset->GetOutermost()->GetName()), Dependencies, UE::AssetRegistry::EDependencyCategory::Package);
    Dependencies.Sort(FNameLexicalLess()); TArray<TSharedPtr<FJsonValue>> Rows; for (FName Dependency : Dependencies) { Rows.Add(MakeShared<FJsonValueString>(Dependency.ToString())); }
    Out->SetBoolField(TEXT("selected_dependency_data_available"), Available); Out->SetArrayField(TEXT("selected_package_dependencies"), Rows);
    // Complete recursively persisted dependency audit remains the caller's existing gate.
    return true;
}

FString InspectAsset(const FString& AssetPath)
{
    FResult R; Init(R, TEXT("inspect_audio_asset")); FString ObjectPath;
    if (!IsEditorGameThread() || IsPlaySessionActive() || !Path(AssetPath, ObjectPath))
    { Fail(R, TEXT("audio_invalid_inspection_preflight"), TEXT("Requires the Editor game thread outside PIE/SIE and one exact supported /Game or /Engine audio asset path.")); return Encode(R); }
    R.Stage = TEXT("resolve_asset"); UObject* O = Resolve(ObjectPath, R); TStrongObjectPtr<UObject> Keep(O); USoundBase* Sound = Cast<USoundBase>(O);
    FString Kind;TArray<UClass*> Chain;TArray<TStrongObjectPtr<UClass>> KeepClasses;TArray<TStrongObjectPtr<UObject>> KeepDefaults;
    struct FClassRole{UClass* Class=nullptr;UClass* Super=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;FString Path;uint32 Flags=0,PackageFlags=0;bool Dirty=false;UObject* Default=nullptr;};
    TArray<FClassRole> Roles;
    bool Metasound=false;
    for(auto* C=Sound?Sound->GetClass():nullptr;C;C=C->GetSuperClass())
    {
        if(Chain.Num()>=64||Chain.Contains(C)||!Loaded(C)||!C->HasAnyClassFlags(CLASS_Native)||C->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists|CLASS_CompiledFromBlueprint)||C->IsEditorOnly()
            ||!Loaded(C->GetPackage())||!C->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||Forbidden(C->GetPathName())||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy.Get())
        {Fail(R,TEXT("audio_native_class_hierarchy_pending"),TEXT("Loaded finite native runtime class hierarchy required; generated/custom runtime source profiles remain incomplete."));return Encode(R);}
        Chain.Add(C);KeepClasses.Emplace(C);FClassRole Role;Role.Class=C;Role.Super=C->GetSuperClass();Role.Outer=C->GetOuter();Role.Package=C->GetPackage();Role.Path=C->GetPathName();Role.Flags=uint32(C->GetClassFlags());Role.PackageFlags=uint32(C->GetPackage()->GetPackageFlags());Role.Dirty=C->GetPackage()->IsDirty();Roles.Add(Role);
        Metasound|=C->GetPathName()==TEXT("/Script/MetasoundEngine.MetaSoundSource");
    }
    // Same classification priority as UE56: MetaSoundSource chain, then Cue, then Wave.
    if(Metasound)Kind=TEXT("MetaSoundSource");else if(Cast<USoundCue>(Sound))Kind=TEXT("SoundCue");else if(Cast<USoundWave>(Sound))Kind=TEXT("SoundWave");
    if(Kind.IsEmpty()){Fail(R,TEXT("audio_asset_not_found"),TEXT("Supported loaded SoundWave/SoundCue/MetaSoundSource native hierarchy not found; generated and other original-valid custom sources remain incomplete."));return Encode(R);}
    const bool Inherited=O->GetClass()!=USoundWave::StaticClass()&&O->GetClass()!=USoundCue::StaticClass()&&O->GetClass()->GetPathName()!=TEXT("/Script/MetasoundEngine.MetaSoundSource");
    TStrongObjectPtr<UEditorEngine> Editor(GEditor);TStrongObjectPtr<UWorld> World(Loaded(GEditor)?GEditor->GetEditorWorldContext().World():nullptr);
    UClass* AssetClass=O->GetClass();UObject* AssetOuter=O->GetOuter();UPackage* AssetPackage=O->GetPackage();
    bool ObserverReady=false,CurrentLost=false;TStrongObjectPtr<UEdGraph> CueGraph;TStrongObjectPtr<USoundNode> CueRoot;TArray<TStrongObjectPtr<USoundNode>> CueNodes;
    const auto Current=[&]()->bool
    {
        if(CurrentLost)return false;
        const auto Bad=[&](const FString& Object,const TCHAR* Property,const FString& Expected,const FString& Actual)->bool
        {if(!R.Data->HasField(TEXT("audio_first_mismatch"))){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("object"),Object);J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);R.Data->SetObjectField(TEXT("audio_first_mismatch"),J);}CurrentLost=true;R.bKnown=false;return Fail(R,TEXT("audio_native_current_changed"),TEXT("Duration/observer returned outside captured native source/target/Editor roles; retain actual prefix, no next SDK or automatic retry."));};
        if(!IsEditorGameThread()||IsPlaySessionActive()||IsGarbageCollecting()||GExitPurge||IsEngineExitRequested()||UE::IsSavingPackage()||!Loaded(Editor.Get())||GEditor!=Editor.Get()||!Loaded(World.Get())||GEditor->GetEditorWorldContext().World()!=World.Get()||World->bIsTearingDown)
            return Bad(TEXT("Editor"),TEXT("context"),TEXT("same idle Editor/world"),TEXT("changed"));
        if(!Loaded(Keep.Get())||O->GetClass()!=AssetClass||O->GetOuter()!=AssetOuter||O->GetPackage()!=AssetPackage||O->GetPathName()!=ObjectPath||FindObject<UObject>(nullptr,*ObjectPath)!=O)
            return Bad(ObjectPath,TEXT("target_identity"),TEXT("captured loaded object/class/owner/package"),TEXT("changed"));
        for(const auto& Role:Roles)
        {
            auto* C=Role.Class;if(!Loaded(C)||!Loaded(Role.Package)||(Role.Outer&&!Loaded(Role.Outer))||C->GetSuperClass()!=Role.Super||C->GetOuter()!=Role.Outer||C->GetPackage()!=Role.Package||C->GetPathName()!=Role.Path||uint32(C->GetClassFlags())!=Role.Flags||uint32(Role.Package->GetPackageFlags())!=Role.PackageFlags||Role.Package->IsDirty()!=Role.Dirty||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy.Get())
                return Bad(Role.Path,TEXT("native_class_source"),TEXT("captured native class/super/package role"),TEXT("changed"));
            if(ObserverReady&&C->GetDefaultObject(false)!=Role.Default)return Bad(Role.Path,TEXT("existing_cdo"),TEXT("captured post-duration nullable CDO; no creation"),TEXT("changed"));
        }
        if(ObserverReady)if(auto* Cue=Cast<USoundCue>(O))
        {
            if(!Cue->SoundCueGraph.IsResolved()||Cue->SoundCueGraph.Get()!=CueGraph.Get()||!Cue->FirstNode.IsResolved()||Cue->FirstNode.Get()!=CueRoot.Get()||Cue->AllNodes.Num()!=CueNodes.Num())return Bad(ObjectPath,TEXT("cue_source_roots"),TEXT("post-duration graph/root/node set"),TEXT("changed"));
            for(int32 I=0;I<CueNodes.Num();++I)if(!Cue->AllNodes[I].IsResolved()||Cue->AllNodes[I].Get()!=CueNodes[I].Get()||!Loaded(CueNodes[I].Get())||CueNodes[I]->GetOuter()!=Cue)return Bad(ObjectPath,TEXT("cue_node_membership"),LexToString(I),TEXT("changed"));
        }
        return true;
    };
    if(!Current())return Encode(R);
    R.Stage = TEXT("duration_read");R.Data->SetNumberField(TEXT("duration_getter_call_count"),1);
    const float Duration=Sound->GetDuration();R.Data->SetBoolField(TEXT("duration_getter_returned"),true);if(FMath::IsFinite(Duration))R.Data->SetNumberField(TEXT("duration_getter_actual_return"),Duration);
    R.Data->SetBoolField(TEXT("duration_getter_aggregate_cache_refresh_possible"),Kind==TEXT("SoundCue"));
    R.Data->SetBoolField(TEXT("duration_getter_side_effects_unknown"),true);R.Data->SetBoolField(TEXT("virtual_getduration_purity_verified"),false);
    if(!Current())return Encode(R);
    if(!FMath::IsFinite(Duration)){R.bKnown=false;Fail(R,TEXT("audio_duration_unobservable"),TEXT("Duration getter returned nonfinite; actual getter/cache prefix remains unknown and is not retried."));return Encode(R);}
    for(auto& Role:Roles){Role.Default=Role.Class->GetDefaultObject(false);if(Role.Default){if(!Loaded(Role.Default)){R.bKnown=false;Fail(R,TEXT("audio_native_default_unavailable"),TEXT("Post-duration existing CDO is unavailable; no source observer or retry."));return Encode(R);}KeepDefaults.Emplace(Role.Default);}}
    if(auto* Cue=Cast<USoundCue>(O))
    {
        if(!Cue->SoundCueGraph.IsResolved()||!Cue->FirstNode.IsResolved()||Cue->AllNodes.Num()>MaxNodes){R.bKnown=false;Fail(R,TEXT("audio_cue_source_pending"),TEXT("Post-duration Cue source must already be resolved/bounded."));return Encode(R);}
        CueGraph.Reset(Cue->SoundCueGraph.Get());CueRoot.Reset(Cue->FirstNode.Get());
        for(const auto& Slot:Cue->AllNodes){if(!Slot.IsResolved()||!Loaded(Slot.Get())){R.bKnown=false;Fail(R,TEXT("audio_cue_node_pending"),TEXT("Post-duration Cue nodes must already be loaded."));return Encode(R);}CueNodes.Emplace(Slot.Get());}
    }
    ObserverReady=true;if(!Current())return Encode(R);
    const TFunction<bool()> InspectorCurrent=Inherited?TFunction<bool()>(Current):TFunction<bool()>();
    R.Data->SetBoolField(TEXT("native_inherited_audio_profile"),Inherited);
    R.Data->SetStringField(TEXT("inspection_preservation_scope"),TEXT("Existing native hierarchy input range; original single GetDuration may execute arbitrary native virtual/cache work. Current source roles are checked after return; preservation evidence covers two post-duration observer snapshots only, not virtual purity or global side effects. Generated sources and nonstandard Cue graph/node export profiles remain incomplete."));
    R.Stage=TEXT("observe_before");FObservation Before, After; R.Data->SetObjectField(TEXT("before"), Observation(Before));
    if (!Capture(O, Before,InspectorCurrent,Inherited?&Chain:nullptr)) { R.bKnown = false; if(R.Code.IsEmpty())Fail(R, TEXT("audio_inspection_unobservable"), TEXT("Bounded complete native audio observation after the initial load/duration getter failed.")); return Encode(R); }
    R.Data->SetObjectField(TEXT("before"), Observation(Before)); R.Data->SetObjectField(TEXT("after"), Observation(After));
    R.Stage=TEXT("observe_after");
    if (!Capture(O, After,InspectorCurrent,Inherited?&Chain:nullptr)) { R.bKnown = false; if(R.Code.IsEmpty())Fail(R, TEXT("audio_inspection_unobservable"), TEXT("Retained audio readback is incomplete; initial complete prefix remains.")); return Encode(R); }
    R.Data->SetObjectField(TEXT("after"), Observation(After)); if (!Same(Before, After)) { R.bChanged = true; Fail(R, TEXT("audio_inspection_state_changed"), TEXT("Repeated observed asset/source/disk/dirty state differs after initial getter work.")); return Encode(R); }
    R.Data->SetStringField(TEXT("asset_path"), O->GetPathName()); R.Data->SetStringField(TEXT("name"), O->GetName()); R.Data->SetStringField(TEXT("class"), O->GetClass()->GetPathName()); R.Data->SetStringField(TEXT("kind"), Kind); R.Data->SetNumberField(TEXT("duration_seconds"), Duration);
    if (const USoundWave* Wave = Cast<USoundWave>(O)) { R.Data->SetNumberField(TEXT("channel_count"), Wave->NumChannels); R.Data->SetBoolField(TEXT("looping"), Wave->bLooping); }
    if (const USoundCue* Cue = Cast<USoundCue>(O)) { R.Data->SetStringField(TEXT("root_node"), Cue->FirstNode ? Cue->FirstNode->GetName() : FString()); R.Data->SetNumberField(TEXT("node_count"), Cue->AllNodes.Num()); }
    R.Data->SetBoolField(TEXT("observations_complete"), true); R.Data->SetBoolField(TEXT("read_only_after_initial_load_and_duration_getter"), true);
    R.bSuccess = true; R.bPost = true; R.Stage = TEXT("complete"); return Encode(R);
}

FString CreateSoundCueFromWave(const FString& InputName, const FString& SoundWavePath, const FString& InputPackagePath)
{
    FResult R; Init(R, TEXT("create_sound_cue_from_wave")); FString Name = InputName.TrimStartAndEnd(), Package = InputPackagePath.TrimStartAndEnd(), WavePath;
    Package.TrimQuotesInline(); Package.ReplaceInline(TEXT("\\"), TEXT("/")); while (Package.Len() > 1 && Package.EndsWith(TEXT("/"))) { Package.LeftChopInline(1); }
    FText Invalid; const FString AssetPackage = Package + TEXT("/") + Name, ObjectPath = AssetPackage + TEXT(".") + Name;
    R.Data->SetStringField(TEXT("asset_path"), ObjectPath);
    if (!IsEditorGameThread() || IsPlaySessionActive() || Name.IsEmpty() || Name.Len() > 64 || FName(*Name).IsNone()
        || !FName::IsValidXName(Name, INVALID_OBJECTNAME_CHARACTERS, &Invalid) || Name.Contains(TEXT(".")) || Name.Contains(TEXT("/")) || Name.Contains(TEXT("\\"))
        || Forbidden(Name) || Forbidden(Package) || !(Package == TEXT("/Game") || Package.StartsWith(TEXT("/Game/"))) || !FPackageName::IsValidLongPackageName(Package, false)
        || ObjectPath.Len() >= NAME_SIZE || !FPackageName::IsValidObjectPath(ObjectPath) || !Path(SoundWavePath, WavePath))
    { Fail(R, TEXT("audio_invalid_creation_preflight"), TEXT("Requires a fresh /Game SoundCue identity of1–64 valid name characters, exact /Game or /Engine Wave and idle Editor game thread.")); return Encode(R); }
    // Required public Editor modules resolve before the source preservation baseline.
    FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")); FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("AudioEditor"));
    R.Stage = TEXT("resolve_source"); USoundWave* Wave = Cast<USoundWave>(Resolve(WavePath, R)); TStrongObjectPtr<USoundWave> KeepWave(Wave);
    if (!Loaded(Wave)) { Fail(R, TEXT("audio_sound_wave_not_found"), TEXT("A loaded SoundWave source or admitted native SoundWave subclass is required.")); return Encode(R); }
    const bool InheritedSource=Wave->GetClass()!=USoundWave::StaticClass();
    if(!InheritedSource&&!Native(Wave->GetClass(),TEXT("/Script/Engine"))){Fail(R,TEXT("audio_sound_wave_not_found"),TEXT("Exact SoundWave native identity is unavailable."));return Encode(R);}
    TArray<UClass*> SourceChain;
    struct FSourceRole
    {
        TStrongObjectPtr<UClass> Class,Super;TStrongObjectPtr<UObject> CDO;TStrongObjectPtr<UPackage> Package;
        UObject* Outer=nullptr;FString Path;EClassFlags Flags=CLASS_None;uint32 PackageFlags=0;bool Dirty=false;
        UObject* CDOOuter=nullptr;UPackage* CDOPackage=nullptr;EObjectFlags CDOFlags=RF_NoFlags;
    };
    TArray<TUniquePtr<FSourceRole>> SourceRoles;
    TStrongObjectPtr<UEditorEngine> SourceEditor(InheritedSource?GEditor:nullptr);
    TStrongObjectPtr<UWorld> SourceWorld(InheritedSource?GEditor->GetEditorWorldContext().World():nullptr);
    TStrongObjectPtr<USoundCue> TargetRole;TStrongObjectPtr<UPackage> TargetPackageRole;TStrongObjectPtr<USoundNodeWavePlayer> PlayerRole;bool PlayerRootBound=false;
    UClass* WaveClass=Wave->GetClass();UObject* WaveOuter=Wave->GetOuter();UPackage* WavePackage=Wave->GetPackage();
    TFunction<bool()> SourceCurrent;
    if(InheritedSource)
    {
        // Sticky loss is stored in the result, not a borrowed lambda-local.
        SourceCurrent=[&]()->bool
        {
            if(R.Data->HasField(TEXT("audio_creation_source_first_mismatch")))return false;
            const auto Bad=[&](const FString& Object,const TCHAR* Property)->bool
            {auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("object"),Object);J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),TEXT("captured live native source/receiver role"));J->SetStringField(TEXT("actual"),TEXT("changed or unavailable"));R.Data->SetObjectField(TEXT("audio_creation_source_first_mismatch"),J);R.bKnown=false;R.Data->SetBoolField(TEXT("mutation_state_unknown"),true);R.Data->SetField(TEXT("changed"),MakeShared<FJsonValueNull>());R.Data->SetBoolField(TEXT("readback_verified"),false);return Fail(R,TEXT("audio_creation_source_current_changed"),TEXT("Retain actual factory/node/save prefix; source/receiver context lost, no next SDK or automatic retry."));};
            if(!IsEditorGameThread()||IsPlaySessionActive()||IsGarbageCollecting()||GExitPurge||IsEngineExitRequested()||UE::IsSavingPackage()
                ||!Loaded(SourceEditor.Get())||GEditor!=SourceEditor.Get()||!Loaded(SourceWorld.Get())||SourceWorld->bIsTearingDown
                ||GEditor->GetEditorWorldContext().World()!=SourceWorld.Get())return Bad(TEXT("Editor"),TEXT("context"));
            if(!Loaded(Wave)||Wave->GetClass()!=WaveClass||Wave->GetOuter()!=WaveOuter||Wave->GetPackage()!=WavePackage
                ||Wave->GetPathName()!=WavePath||StaticFindObject(nullptr,nullptr,*WavePath)!=Wave)return Bad(WavePath,TEXT("source_identity"));
            for(const auto& Entry:SourceRoles)
            {
                const auto& E=*Entry;auto* C=E.Class.Get();UClass* Super=nullptr;
                if(!CreationSourceSuper(C,Super)||Super!=E.Super.Get()||!Loaded(E.Package.Get())||C->GetPackage()!=E.Package.Get()
                    ||C->GetOuter()!=E.Outer||C->GetPathName()!=E.Path||C->GetClassFlags()!=E.Flags
                    ||uint32(E.Package->GetPackageFlags())!=E.PackageFlags||E.Package->IsDirty()!=E.Dirty
                    ||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy!=nullptr||!C->ClassDefaultObject.IsResolved()
                    ||C->ClassDefaultObject!=E.CDO.Get())return Bad(E.Path,TEXT("native_class_source"));
                if(E.CDO.IsValid()&&(!Loaded(E.CDO.Get())||E.CDO->GetClass()!=C||E.CDO->GetOuter()!=E.CDOOuter
                    ||E.CDO->GetPackage()!=E.CDOPackage||E.CDO->GetFlags()!=E.CDOFlags))return Bad(E.Path,TEXT("existing_native_cdo"));
            }
            if(TargetRole.IsValid()&&(!Loaded(TargetRole.Get())||TargetRole->GetClass()!=USoundCue::StaticClass()
                ||!Loaded(TargetPackageRole.Get())||TargetRole->GetPathName()!=ObjectPath||TargetRole->GetPackage()!=TargetPackageRole.Get()||TargetRole->GetOuter()!=TargetPackageRole.Get()))return Bad(ObjectPath,TEXT("target_receiver"));
            if(PlayerRole.IsValid()&&(!Loaded(PlayerRole.Get())||PlayerRole->GetClass()!=USoundNodeWavePlayer::StaticClass()
                ||PlayerRole->GetOuter()!=TargetRole.Get()||!TargetRole->AllNodes.Contains(PlayerRole.Get())||(PlayerRootBound&&TargetRole->FirstNode!=PlayerRole.Get())))return Bad(ObjectPath,TEXT("wave_player_receiver"));
            return true;
        };
        for(UClass* C=WaveClass;C;)
        {
            if(SourceChain.Num()>=64||SourceChain.Contains(C)||!Loaded(C)||!Loaded(C->GetPackage())
                ||!C->HasAnyClassFlags(CLASS_Native)||C->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists|CLASS_CompiledFromBlueprint)
                ||!C->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||C->GetPackage()->HasAnyPackageFlags(PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly)
                ||Forbidden(C->GetPathName())||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy!=nullptr||!C->ClassDefaultObject.IsResolved())
            {Fail(R,TEXT("audio_creation_source_hierarchy_pending"),TEXT("Finite already-loaded native runtime SoundWave ancestry required; generated/editor/custom unobservable sources remain incomplete."));return Encode(R);}
            UClass* Super=nullptr;if(!CreationSourceSuper(C,Super)){Fail(R,TEXT("audio_creation_source_hierarchy_pending"),TEXT("Native superclass observation failed, not an observed null terminator."));return Encode(R);}
            auto E=MakeUnique<FSourceRole>();E->Class.Reset(C);E->Super.Reset(Super);E->Package.Reset(C->GetPackage());E->Outer=C->GetOuter();E->Path=C->GetPathName();E->Flags=C->GetClassFlags();E->PackageFlags=uint32(E->Package->GetPackageFlags());E->Dirty=E->Package->IsDirty();
            E->CDO.Reset(CreationSourcePointer(C->ClassDefaultObject));if(C->ClassDefaultObject!=E->CDO.Get()){Fail(R,TEXT("audio_creation_source_hierarchy_pending"),TEXT("Existing native CDO slot must resolve without a load/default creation."));return Encode(R);}
            if(E->CDO.IsValid()){E->CDOOuter=E->CDO->GetOuter();E->CDOPackage=E->CDO->GetPackage();E->CDOFlags=E->CDO->GetFlags();}
            SourceChain.Add(C);SourceRoles.Add(MoveTemp(E));if(!SourceCurrent())return Encode(R);C=Super;
        }
        R.Data->SetBoolField(TEXT("inherited_native_wave_source"),true);R.Data->SetBoolField(TEXT("SDK_global_effects_verified_zero"),false);
        R.Data->SetStringField(TEXT("inherited_wave_scope"),TEXT("Existing native runtime SoundWave hierarchy as readonly creation source; shared WS property/native-default/file observations and explicit SDK return fences. Factory/virtual/serialization callbacks are whole-call boundaries, not global purity or playback proof. Exact target Cue/one-player/save/idempotency contract unchanged; generated sources and other unobservable custom profiles remain incomplete."));
    }
    const auto Fence=[&](){return !SourceCurrent||SourceCurrent();};
    if(!Fence())return Encode(R);
    if (!RequirePersisted(Wave, R, TEXT("source_dependency_audit"))) { return Encode(R); }
    if(!Fence())return Encode(R);
    FObservation SourceBefore; R.Data->SetObjectField(TEXT("source_before"), Observation(SourceBefore));
    if (!Capture(Wave, SourceBefore,SourceCurrent,InheritedSource?&SourceChain:nullptr)) { Fail(R, TEXT("audio_source_unobservable"), TEXT("Source Wave/file/native defaults/other dirty state are not completely observable before creation.")); return Encode(R); }
    R.Data->SetObjectField(TEXT("source_before"), Observation(SourceBefore));
    if (UEditorAssetLibrary::DoesAssetExist(AssetPackage) || FindObject<UObject>(nullptr, *ObjectPath))
    {
        if(!Fence())return Encode(R);
        R.Stage = TEXT("existing_target"); USoundCue* Existing = Cast<USoundCue>(Resolve(ObjectPath, R)); TStrongObjectPtr<USoundCue> KeepExisting(Existing);
        if(!Fence())return Encode(R);if(InheritedSource&&Loaded(Existing)&&Existing->GetClass()==USoundCue::StaticClass()&&Existing->GetPathName()==ObjectPath&&Existing->GetOuter()==Existing->GetPackage()){TargetRole.Reset(Existing);TargetPackageRole.Reset(Existing->GetPackage());}if(!Fence())return Encode(R);
        if(InheritedSource&&TargetRole.IsValid())
        {
            UObject* Root=CreationSourcePointer(Existing->FirstNode);
            if(Loaded(Root)&&Root->GetClass()==USoundNodeWavePlayer::StaticClass()&&Root->GetOuter()==Existing&&Existing->AllNodes.Contains(static_cast<USoundNodeWavePlayer*>(Root)))
            {PlayerRole.Reset(static_cast<USoundNodeWavePlayer*>(Root));PlayerRootBound=true;}
            if(!Fence())return Encode(R);
        }
        if (!Existing || Existing->GetOutermost()->IsDirty() || !Verify(Existing, Wave,SourceCurrent)) { Fail(R, TEXT("audio_asset_conflict"), TEXT("Existing target was not overwritten: require a clean exact saved SoundCue with one exact requested Wave player.")); return Encode(R); }
        if (!RequirePersisted(Existing, R, TEXT("existing_dependency_audit"))) { return Encode(R); }
        FObservation Before, After, SourceAfter; R.Data->SetObjectField(TEXT("before"), Observation(Before));
        if (!Capture(Existing, Before,SourceCurrent)) { Fail(R, TEXT("audio_existing_unobservable"), TEXT("Existing target observation is incomplete; no creation or save occurred.")); return Encode(R); }
        R.Data->SetObjectField(TEXT("before"), Observation(Before)); R.Data->SetObjectField(TEXT("after"), Observation(After));
        R.Data->SetObjectField(TEXT("source_after"), Observation(SourceAfter));
        if (!Capture(Existing, After,SourceCurrent) || !Capture(Wave, SourceAfter,SourceCurrent,InheritedSource?&SourceChain:nullptr)) { R.bKnown = false; Fail(R, TEXT("audio_existing_unobservable"), TEXT("Existing target/source retained readback is incomplete.")); return Encode(R); }
        R.Data->SetObjectField(TEXT("after"), Observation(After)); R.Data->SetObjectField(TEXT("source_after"), Observation(SourceAfter));
        if (!Same(Before, After) || !Same(SourceBefore, SourceAfter)) { R.bChanged = true; Fail(R, TEXT("audio_existing_state_changed"), TEXT("Observed state changed during existing-target resolution/audit; no writer was invoked.")); return Encode(R); }
        if(!Fence())return Encode(R);
        if(!CueData(Existing, Wave, false, R,SourceCurrent)||!Fence()){R.bKnown=false;Fail(R,TEXT("audio_cue_data_current_changed"),TEXT("Root player diagnostic lease lost; no next SDK or retry."));return Encode(R);} R.Data->SetBoolField(TEXT("observations_complete"), true); R.bSuccess = true; R.bPost = true; R.Stage = TEXT("complete"); return Encode(R);
    }
    auto ExistingDisk = MakeShared<FJsonObject>(); if (!Disk(AssetPackage, ExistingDisk) || ExistingDisk->GetBoolField(TEXT("exists")))
    { Fail(R, TEXT("audio_asset_conflict"), TEXT("Target file exists or cannot be observed; do not overwrite or infer an absent destination from stale registry metadata.")); return Encode(R); }
    if (!USoundCue::GetSoundCueAudioEditor().IsValid() || !Native(USoundCue::StaticClass(), TEXT("/Script/Engine"))
        || !Native(USoundNodeWavePlayer::StaticClass(), TEXT("/Script/Engine")) || !Native(USoundCueFactoryNew::StaticClass(), TEXT("/Script/AudioEditor")))
    { Fail(R, TEXT("audio_public_factory_unavailable"), TEXT("The public native SoundCue factory/editor interface and classes must be ready before assert-bearing construction.")); return Encode(R); }
    if(!Fence())return Encode(R);
    R.Stage = TEXT("create_asset"); R.bAttempted = true; R.bKnown = false; R.bSaveRequested = true;
    USoundCueFactoryNew* Factory = NewObject<USoundCueFactoryNew>(); TStrongObjectPtr<USoundCueFactoryNew> KeepFactory(Factory);
    if(!Fence())return Encode(R);
    R.Data->SetNumberField(TEXT("asset_factory_call_count"), 1);
    USoundCue* Cue = Cast<USoundCue>(FAssetToolsModule::GetModule().Get().CreateAsset(Name, Package, USoundCue::StaticClass(), Factory)); TStrongObjectPtr<USoundCue> KeepCue(Cue);
    if(!Fence())return Encode(R);
    if (!Loaded(Cue) || Cue->GetClass() != USoundCue::StaticClass() || Cue->GetPathName() != ObjectPath || Cue->GetOuter() != Cue->GetOutermost())
    { Fail(R, TEXT("audio_sound_cue_create_failed"), TEXT("Public factory returned no exact owned target; its creation prefix is unknown and was not cleaned up or retried.")); return Encode(R); }
    if(InheritedSource){TargetRole.Reset(Cue);TargetPackageRole.Reset(Cue->GetPackage());}if(!Fence())return Encode(R);
    R.Data->SetObjectField(TEXT("created_asset_identity"), Identity(Cue)); R.bChanged = true;
    Cue->Modify(); if(!Fence())return Encode(R); R.Stage = TEXT("create_wave_player"); R.Data->SetNumberField(TEXT("wave_player_creation_count"), 1);
    USoundNodeWavePlayer* Player = Cue->ConstructSoundNode<USoundNodeWavePlayer>();
    if(!Fence())return Encode(R);
    if (!Loaded(Player) || Player->GetClass() != USoundNodeWavePlayer::StaticClass() || Player->GetOuter() != Cue)
    { Fail(R, TEXT("audio_sound_cue_node_failed"), TEXT("The returned Wave player is unavailable; retained factory/node prefix is unknown.")); return Encode(R); }
    if(InheritedSource)PlayerRole.Reset(Player);if(!Fence())return Encode(R);
    R.Data->SetObjectField(TEXT("created_wave_player_identity"), Identity(Player)); Player->Modify(); if(!Fence())return Encode(R); Player->SetSoundWave(Wave); if(!Fence())return Encode(R); Cue->FirstNode = Player;if(InheritedSource)PlayerRootBound=true;
    R.Stage = TEXT("link_graph"); R.Data->SetNumberField(TEXT("link_graph_call_count"), 1); Cue->LinkGraphNodesFromSoundNodes(); if(!Fence())return Encode(R);
    R.Data->SetNumberField(TEXT("post_edit_change_call_count"), 1); Cue->PostEditChange(); if(!Fence())return Encode(R); Cue->MarkPackageDirty(); if(!Fence())return Encode(R);
    FObservation BeforeSave, SourceAfter; R.Data->SetObjectField(TEXT("after_creation"), Observation(BeforeSave));
    R.Data->SetObjectField(TEXT("source_after_creation"), Observation(SourceAfter));
    if (!Capture(Cue, BeforeSave,SourceCurrent) || !Capture(Wave, SourceAfter,SourceCurrent,InheritedSource?&SourceChain:nullptr)) { Fail(R, TEXT("audio_creation_unobservable"), TEXT("The retained constructed graph/source prefix is incomplete; no exact save was attempted.")); return Encode(R); }
    R.Data->SetObjectField(TEXT("after_creation"), Observation(BeforeSave)); R.Data->SetObjectField(TEXT("source_after_creation"), Observation(SourceAfter)); R.bKnown = true;
    // Source's other-dirty snapshot may now legitimately contain the new target.
    const bool SourceStatePreserved = Text(SourceBefore.State) == Text(SourceAfter.State) && Text(SourceBefore.DiskState) == Text(SourceAfter.DiskState)
        && OtherDirtyPreserved(SourceBefore, SourceAfter, AssetPackage);
    R.Data->SetBoolField(TEXT("source_state_preserved_after_creation"), SourceStatePreserved);
    if (!Verify(Cue, Wave,SourceCurrent) || !SourceStatePreserved) { Fail(R, TEXT("audio_creation_postcondition_failed"), TEXT("The observed single-wave graph/source does not match; no save or cleanup occurred.")); return Encode(R); }
    if (!MemoryDependencies(Cue, R, TEXT("pre_save_dependency_audit"))) { return Encode(R); }
    if(!Fence())return Encode(R);
    R.Stage = TEXT("save_asset"); R.bSaveAttempted = true; R.Data->SetNumberField(TEXT("save_attempt_count"), 1); R.bKnown = false;
    auto SaveRow = MakeShared<FJsonObject>(); SaveRow->SetStringField(TEXT("asset_path"), ObjectPath); SaveRow->SetBoolField(TEXT("attempted"), true); SaveRow->SetBoolField(TEXT("success"), false);
    R.Data->SetArrayField(TEXT("save_calls"), { MakeShared<FJsonValueObject>(SaveRow) });
    const bool Saved = UEditorAssetLibrary::SaveLoadedAsset(Cue, false); SaveRow->SetBoolField(TEXT("success"), Saved); R.bSaved = Saved;
    if(!Fence())return Encode(R);
    FObservation AfterSave, FinalSource; R.Data->SetObjectField(TEXT("after_save"), Observation(AfterSave));
    const bool Complete = Capture(Cue, AfterSave,SourceCurrent) && Capture(Wave, FinalSource,SourceCurrent,InheritedSource?&SourceChain:nullptr); R.Data->SetObjectField(TEXT("after_save"), Observation(AfterSave)); R.Data->SetObjectField(TEXT("source_after_save"), Observation(FinalSource));
    if (!Complete) { Fail(R, TEXT("audio_sound_cue_save_unobservable"), TEXT("The actual save return is retained but final observations are incomplete; do not resend the writer.")); return Encode(R); }
    if (!Saved) { Fail(R, TEXT("audio_sound_cue_save_failed"), TEXT("The exact save returned false; no retry or cleanup, disk outcome remains unknown.")); return Encode(R); }
    R.bKnown = true;
    const bool FinalSourcePreserved = Text(SourceBefore.State) == Text(FinalSource.State) && Text(SourceBefore.DiskState) == Text(FinalSource.DiskState)
        && OtherDirtyPreserved(SourceBefore, FinalSource, AssetPackage);
    R.Data->SetBoolField(TEXT("source_state_preserved_after_save"), FinalSourcePreserved); R.Data->SetBoolField(TEXT("saved_state_preserved"), SavedState(BeforeSave, AfterSave));
    if (!Verify(Cue, Wave,SourceCurrent) || !AfterSave.DiskState->GetBoolField(TEXT("exists")) || Cue->GetOutermost()->IsDirty() || !FinalSourcePreserved || !R.Data->GetBoolField(TEXT("saved_state_preserved")))
    { Fail(R, TEXT("audio_sound_cue_readback_failed"), TEXT("Actual successful save retained, but exact target/source/owned-save-flags readback failed; no writer retry.")); return Encode(R); }
    R.Data->SetBoolField(TEXT("saved_state_preserved_before_registry_refresh"), R.Data->GetBoolField(TEXT("saved_state_preserved"))); R.Data->SetBoolField(TEXT("saved_state_preserved"), false);
    R.Stage = TEXT("refresh_saved_asset_registry"); auto Refresh = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("saved_asset_registry_refresh"), Refresh); FString RefreshError;
    const bool Refreshed = RefreshSavedAssetRegistry(Cue, Refresh, RefreshError);
    R.Data->SetNumberField(TEXT("asset_registry_refresh_calls"), Refresh->GetNumberField(TEXT("scan_call_count"))); R.Data->SetNumberField(TEXT("asset_registry_refresh_file_count"), Refresh->GetNumberField(TEXT("file_count")));
    if(!Fence())return Encode(R);
    if (!Refreshed) { R.bKnown = Refresh->GetBoolField(TEXT("asset_state_change_known")); Fail(R, TEXT("audio_saved_registry_refresh_failed"), RefreshError); return Encode(R); }
    R.Data->SetBoolField(TEXT("saved_state_preserved"), true); FObservation RefreshedSource; R.Data->SetObjectField(TEXT("source_after_registry_refresh"), Observation(RefreshedSource));
    if (!Capture(Wave, RefreshedSource,SourceCurrent,InheritedSource?&SourceChain:nullptr)) { R.Data->SetObjectField(TEXT("source_after_registry_refresh"), Observation(RefreshedSource)); R.bKnown = false; Fail(R, TEXT("audio_registry_refresh_source_unobservable"), TEXT("Registry refresh returned; final source Wave prefix incomplete, successful save receipt retained without retry.")); return Encode(R); }
    R.Data->SetObjectField(TEXT("source_after_registry_refresh"), Observation(RefreshedSource));
    const bool RefreshedSourcePreserved = Text(SourceBefore.State) == Text(RefreshedSource.State) && Text(SourceBefore.DiskState) == Text(RefreshedSource.DiskState) && OtherDirtyPreserved(SourceBefore, RefreshedSource, AssetPackage);
    R.Data->SetBoolField(TEXT("source_state_preserved_after_registry_refresh"), RefreshedSourcePreserved);
    if (!RefreshedSourcePreserved) { Fail(R, TEXT("audio_registry_refresh_source_preservation_failed"), TEXT("Complete source Wave/source-disk/other-dirty observation differs after exact-file registry callbacks; saved receipt retained.")); return Encode(R); }
    if (!RequirePersisted(Cue, R, TEXT("saved_dependency_audit"))) { return Encode(R); }
    if(!Fence())return Encode(R);
    if(!CueData(Cue, Wave, true, R,SourceCurrent)||!Fence()){R.bKnown=false;Fail(R,TEXT("audio_cue_data_current_changed"),TEXT("Root player diagnostic lease lost; actual save receipt retained, no retry."));return Encode(R);} R.Data->SetBoolField(TEXT("observations_complete"), true); R.bSuccess = true; R.bPost = true; R.Stage = TEXT("complete"); return Encode(R);
}
}
