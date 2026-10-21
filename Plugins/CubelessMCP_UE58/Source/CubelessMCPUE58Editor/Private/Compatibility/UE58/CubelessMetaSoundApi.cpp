// Original finite continuous MetaSound graph through public Editor builder APIs.
#include "Compatibility/UE58/CubelessMetaSoundApi.h"
#include "Compatibility/UE58/CubelessAudioApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "HAL/FileManager.h"
#include "Interfaces/MetasoundFrontendSourceInterface.h"
#include "Interfaces/MetasoundOutputFormatInterfaces.h"
#include "ISourceControlModule.h"
#include "MetasoundBuilderBase.h"
#include "MetasoundBuilderSubsystem.h"
#include "MetasoundEditorSubsystem.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendLiteral.h"
#include "MetasoundSource.h"
#include "Misc/PackageName.h"
#include "NodeTemplates/MetasoundFrontendNodeTemplateInput.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace Cubeless::UE58::MetaSound
{
namespace
{
struct FResult
{
    FString Stage = TEXT("preflight"), Code, Error;
    bool bSuccess = false, bAttempted = false, bKnown = true, bChanged = false, bPost = false, bSaveRequested = false, bSaveAttempted = false, bSaved = false;
    TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
};
FString Text(const TSharedRef<FJsonObject>& J) { FString S; FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&S)); return S; }
bool SetFailure(FResult& R, const TCHAR* Code, const FString& Error) { R.Code = Code; R.Error = Error; return false; }
FString Encode(const FResult& R)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("operation"), TEXT("create_basic_metasound_source")); J->SetStringField(TEXT("stage"), R.Stage); J->SetStringField(TEXT("error_code"), R.Code); J->SetStringField(TEXT("error"), R.Error);
    J->SetBoolField(TEXT("success"), R.bSuccess); J->SetBoolField(TEXT("mutation_attempted"), R.bAttempted); J->SetBoolField(TEXT("change_state_known"), R.bKnown);
    if (R.bKnown) { J->SetBoolField(TEXT("changed"), R.bChanged); } else { J->SetField(TEXT("changed"), MakeShared<FJsonValueNull>()); }
    J->SetBoolField(TEXT("outcome_unknown"), !R.bKnown); J->SetBoolField(TEXT("postcondition_passed"), R.bPost); J->SetBoolField(TEXT("compile_requested"), false); J->SetBoolField(TEXT("compile_attempted"), false); J->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    J->SetBoolField(TEXT("save_requested"), R.bSaveRequested); J->SetBoolField(TEXT("save_attempted"), R.bSaveAttempted); J->SetBoolField(TEXT("saved"), R.bSaved); J->SetBoolField(TEXT("atomic_rollback_available"), false); J->SetBoolField(TEXT("auto_retry_safe"), false); J->SetObjectField(TEXT("data"), R.Data); return Text(J);
}
bool ContainsMCPToken(const FString& Value)
{ for (const TCHAR* Token : { TEXT("UnrealMCP"), TEXT("MCPUnreal"), TEXT("mcp_unreal"), TEXT("CubelessControlRigTools"), TEXT("/CubelessMCP_UE58/"), TEXT("/Script/CubelessMCPUE58Editor") }) { if (Value.Contains(Token, ESearchCase::IgnoreCase)) { return true; } } return false; }
bool Loaded(const UObject* O) { return IsValid(O) && !O->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_BeginDestroyed | RF_FinishDestroyed); }
bool NativeSource(const UMetaSoundSource* O)
{ return Loaded(O) && O->GetClass() == UMetaSoundSource::StaticClass() && O->GetClass()->HasAnyClassFlags(CLASS_Native)
    && !O->GetClass()->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Abstract) && O->GetClass()->GetOutermost()->GetName() == TEXT("/Script/MetasoundEngine")
    && O->GetClass()->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn) && !O->GetClass()->GetOutermost()->IsDirty()
    && O->GetOuter() == O->GetOutermost() && !O->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject) && !O->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor | PKG_EditorOnly | PKG_CompiledIn); }

struct FBasicMetaSoundReadback
{
    int32 GraphNodeCount = 0;
    int32 GeneratorNodeCount = 0;
    int32 AudioOutputCount = 0;
    int32 InterfaceCount = 0;
    float ObservedFrequency = 0.0f;
    bool bFrequencyVerified = false;
    FString Failure;
};

bool VerifyBasicMetaSound(
    const UMetaSoundSource* Source,
    const FMetasoundFrontendClassName& ExpectedGeneratorClass,
    bool bExpectFrequency,
    float ExpectedFrequency,
    EMetaSoundOutputAudioFormat ExpectedFormat,
    FBasicMetaSoundReadback& Out)
{
    auto Fail = [&Out](const FString& Message)
    {
        Out.Failure = Message;
        return false;
    };

    if (!Source || Source->GetClass() != UMetaSoundSource::StaticClass())
    {
        return Fail(TEXT("asset is not an exact MetaSoundSource class"));
    }
    if (Source->OutputFormat != ExpectedFormat)
    {
        return Fail(TEXT("saved MetaSound output format does not match the request"));
    }

    const FMetasoundFrontendDocument& Document = Source->GetConstDocument();
    const FMetasoundFrontendVersion& ExpectedOutputInterface =
        ExpectedFormat == EMetaSoundOutputAudioFormat::Stereo
        ? Metasound::Engine::OutputFormatStereoInterface::GetVersion()
        : Metasound::Engine::OutputFormatMonoInterface::GetVersion();
    Out.InterfaceCount = Document.Interfaces.Num();
    if (Out.InterfaceCount != 2 ||
        !Document.Interfaces.Contains(Metasound::Frontend::SourceInterface::GetVersion()) ||
        !Document.Interfaces.Contains(ExpectedOutputInterface) ||
        Document.Interfaces.Contains(Metasound::Frontend::SourceOneShotInterface::GetVersion()))
    {
        return Fail(TEXT("saved MetaSound interfaces do not describe the requested continuous source"));
    }
    if (Document.RootGraph.PresetOptions.bIsPreset || !Document.Subgraphs.IsEmpty())
    {
        return Fail(TEXT("saved MetaSound is a preset or contains a subgraph"));
    }
    if (ContainsMCPToken(Source->GetClass()->GetPathName()) ||
        ContainsMCPToken(Document.RootGraph.Metadata.GetClassName().ToString()) ||
        ContainsMCPToken(Document.RootGraph.Metadata.GetAuthor()))
    {
        return Fail(TEXT("saved MetaSound metadata contains an MCP-only dependency token"));
    }

    const FMetasoundFrontendClassInterface& RootInterface = Document.RootGraph.GetDefaultInterface();
    if (RootInterface.Inputs.Num() != 1 ||
        RootInterface.Inputs[0].Name != Metasound::Frontend::SourceInterface::Inputs::OnPlay)
    {
        return Fail(TEXT("saved MetaSound has unexpected graph inputs"));
    }

    TSet<FName> ExpectedOutputNames;
    if (ExpectedFormat == EMetaSoundOutputAudioFormat::Stereo)
    {
        ExpectedOutputNames.Add(Metasound::Engine::OutputFormatStereoInterface::Outputs::LeftOut);
        ExpectedOutputNames.Add(Metasound::Engine::OutputFormatStereoInterface::Outputs::RightOut);
    }
    else
    {
        ExpectedOutputNames.Add(Metasound::Engine::OutputFormatMonoInterface::Outputs::MonoOut);
    }
    if (RootInterface.Outputs.Num() != ExpectedOutputNames.Num())
    {
        return Fail(TEXT("saved MetaSound graph output count does not match the request"));
    }
    for (const FMetasoundFrontendClassOutput& Output : RootInterface.Outputs)
    {
        if (!ExpectedOutputNames.Remove(Output.Name) || Output.TypeName != FName(TEXT("Audio")))
        {
            return Fail(TEXT("saved MetaSound graph outputs do not match the requested audio channels"));
        }
    }
    if (!ExpectedOutputNames.IsEmpty())
    {
        return Fail(TEXT("saved MetaSound is missing a requested audio channel"));
    }

    if (Document.RootGraph.GetConstGraphPages().Num() != 1)
    {
        return Fail(TEXT("saved MetaSound has an unexpected graph page count"));
    }
    const FMetasoundFrontendGraph* Graph =
        Document.RootGraph.FindConstGraph(Metasound::Frontend::DefaultPageID);
    if (!Graph || !Graph->Variables.IsEmpty())
    {
        return Fail(TEXT("saved MetaSound default graph is missing or contains variables"));
    }
    Out.GraphNodeCount = Graph->Nodes.Num();

    TSet<FGuid> GeneratorClassIDs;
    TSet<FGuid> TemplateClassIDs;
    for (const FMetasoundFrontendClass& Dependency : Document.Dependencies)
    {
        const FMetasoundFrontendClassMetadata& Metadata = Dependency.Metadata;
        if (ContainsMCPToken(Metadata.GetClassName().ToString()) ||
            ContainsMCPToken(Metadata.GetAuthor()))
        {
            return Fail(TEXT("saved MetaSound dependency metadata contains an MCP-only token"));
        }
        switch (Metadata.GetType())
        {
        case EMetasoundFrontendClassType::External:
            if (Metadata.GetClassName() != ExpectedGeneratorClass || Metadata.GetVersion().Major != 1)
            {
                return Fail(TEXT("saved MetaSound contains an unexpected external node class"));
            }
            GeneratorClassIDs.Add(Dependency.ID);
            break;
        case EMetasoundFrontendClassType::Template:
            if (Metadata.GetClassName() != Metasound::Frontend::FInputNodeTemplate::ClassName)
            {
                return Fail(TEXT("saved MetaSound contains an unexpected template node class"));
            }
            TemplateClassIDs.Add(Dependency.ID);
            break;
        case EMetasoundFrontendClassType::Input:
        case EMetasoundFrontendClassType::Output:
            break;
        default:
            return Fail(TEXT("saved MetaSound contains an unexpected dependency type"));
        }
    }
    if (GeneratorClassIDs.Num() != 1 || TemplateClassIDs.Num() != 1)
    {
        return Fail(TEXT("saved MetaSound dependency set does not match the requested basic graph"));
    }

    const FGuid InputNodeID = RootInterface.Inputs[0].NodeID;
    TSet<FGuid> OutputNodeIDs;
    for (const FMetasoundFrontendClassOutput& Output : RootInterface.Outputs)
    {
        OutputNodeIDs.Add(Output.NodeID);
    }

    const FMetasoundFrontendNode* GeneratorNode = nullptr;
    const FMetasoundFrontendNode* TemplateNode = nullptr;
    for (const FMetasoundFrontendNode& Node : Graph->Nodes)
    {
        if (ContainsMCPToken(Node.Name.ToString()))
        {
            return Fail(TEXT("saved MetaSound node name contains an MCP-only token"));
        }
        if (GeneratorClassIDs.Contains(Node.ClassID))
        {
            if (GeneratorNode)
            {
                return Fail(TEXT("saved MetaSound contains more than one generator node"));
            }
            GeneratorNode = &Node;
            ++Out.GeneratorNodeCount;
        }
        else if (TemplateClassIDs.Contains(Node.ClassID))
        {
            if (TemplateNode)
            {
                return Fail(TEXT("saved MetaSound contains more than one input template node"));
            }
            TemplateNode = &Node;
        }
        else if (Node.GetID() != InputNodeID && !OutputNodeIDs.Contains(Node.GetID()))
        {
            return Fail(TEXT("saved MetaSound contains a node outside the requested basic graph"));
        }
    }
    const int32 ExpectedNodeCount = 3 + RootInterface.Outputs.Num();
    if (!GeneratorNode || !TemplateNode || Out.GraphNodeCount != ExpectedNodeCount)
    {
        return Fail(TEXT("saved MetaSound node count does not match the requested basic graph"));
    }

    const FMetasoundFrontendNode* InputNode = Graph->Nodes.FindByPredicate(
        [InputNodeID](const FMetasoundFrontendNode& Node)
        {
            return Node.GetID() == InputNodeID;
        });
    if (!InputNode || InputNode->Interface.Outputs.Num() != 1 || TemplateNode->Interface.Inputs.Num() != 1)
    {
        return Fail(TEXT("saved MetaSound input-template vertices are missing or ambiguous"));
    }

    const FMetasoundFrontendVertex* GeneratorAudio = GeneratorNode->Interface.Outputs.FindByPredicate(
        [](const FMetasoundFrontendVertex& Vertex)
        {
            return Vertex.Name == FName(TEXT("Audio")) && Vertex.TypeName == FName(TEXT("Audio"));
        });
    if (!GeneratorAudio || GeneratorNode->Interface.Outputs.Num() != 1)
    {
        return Fail(TEXT("saved generator Audio output is missing or ambiguous"));
    }

    if (bExpectFrequency)
    {
        const FMetasoundFrontendVertex* FrequencyInput = GeneratorNode->Interface.Inputs.FindByPredicate(
            [](const FMetasoundFrontendVertex& Vertex)
            {
                return Vertex.Name == FName(TEXT("Frequency"));
            });
        if (!FrequencyInput || GeneratorNode->InputLiterals.Num() != 1)
        {
            return Fail(TEXT("saved generator frequency literal is missing or ambiguous"));
        }
        const FMetasoundFrontendVertexLiteral* Literal = GeneratorNode->InputLiterals.FindByPredicate(
            [FrequencyInput](const FMetasoundFrontendVertexLiteral& Candidate)
            {
                return Candidate.VertexID == FrequencyInput->VertexID;
            });
        if (!Literal || !Literal->Value.TryGet(Out.ObservedFrequency) ||
            !FMath::IsNearlyEqual(Out.ObservedFrequency, ExpectedFrequency, 0.001f))
        {
            return Fail(TEXT("saved generator frequency does not match the request"));
        }
        Out.bFrequencyVerified = true;
    }
    else if (!GeneratorNode->InputLiterals.IsEmpty())
    {
        return Fail(TEXT("saved noise generator has unexpected input overrides"));
    }

    TSet<FGuid> ConnectedOutputNodeIDs;
    bool bTemplateInputEdgeFound = false;
    for (const FMetasoundFrontendEdge& Edge : Graph->Edges)
    {
        if (Edge.FromNodeID == GeneratorNode->GetID())
        {
            const FMetasoundFrontendNode* OutputNode = Graph->Nodes.FindByPredicate(
                [&Edge](const FMetasoundFrontendNode& Node)
                {
                    return Node.GetID() == Edge.ToNodeID;
                });
            if (Edge.FromVertexID != GeneratorAudio->VertexID ||
                !OutputNodeIDs.Contains(Edge.ToNodeID) ||
                !OutputNode || OutputNode->Interface.Inputs.Num() != 1 ||
                OutputNode->Interface.Inputs[0].VertexID != Edge.ToVertexID ||
                ConnectedOutputNodeIDs.Contains(Edge.ToNodeID))
            {
                return Fail(TEXT("saved generator has an unexpected or duplicate output connection"));
            }
            ConnectedOutputNodeIDs.Add(Edge.ToNodeID);
            ++Out.AudioOutputCount;
        }
        else if (Edge.FromNodeID == InputNodeID && Edge.ToNodeID == TemplateNode->GetID())
        {
            if (bTemplateInputEdgeFound ||
                Edge.FromVertexID != InputNode->Interface.Outputs[0].VertexID ||
                Edge.ToVertexID != TemplateNode->Interface.Inputs[0].VertexID)
            {
                return Fail(TEXT("saved MetaSound has an unexpected input-template connection"));
            }
            bTemplateInputEdgeFound = true;
        }
        else
        {
            return Fail(TEXT("saved MetaSound contains an unexpected graph connection"));
        }
    }
    if (!bTemplateInputEdgeFound ||
        ConnectedOutputNodeIDs.Num() != RootInterface.Outputs.Num() ||
        Graph->Edges.Num() != RootInterface.Outputs.Num() + 1)
    {
        return Fail(TEXT("saved MetaSound output or input-template connections are incomplete"));
    }
    return true;
}

TSharedPtr<FJsonObject> MakeMetaSoundData(
    const UMetaSoundSource* Source,
    const FString& Oscillator,
    double RequestedFrequency,
    const FString& OutputFormat,
    const FBasicMetaSoundReadback& Readback,
    bool bCreated)
{
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("asset_path"), Source->GetPathName());
    Data->SetStringField(TEXT("class"), Source->GetClass()->GetPathName());
    Data->SetStringField(TEXT("oscillator"), Oscillator);
    Data->SetNumberField(TEXT("frequency"), RequestedFrequency);
    Data->SetBoolField(TEXT("frequency_applied"), Readback.bFrequencyVerified);
    if (Readback.bFrequencyVerified)
    {
        Data->SetNumberField(TEXT("observed_frequency"), Readback.ObservedFrequency);
    }
    Data->SetStringField(TEXT("output_format"), OutputFormat);
    Data->SetNumberField(TEXT("audio_output_count"), Readback.AudioOutputCount);
    Data->SetNumberField(TEXT("graph_node_count"), Readback.GraphNodeCount);
    Data->SetNumberField(TEXT("generator_node_count"), Readback.GeneratorNodeCount);
    Data->SetNumberField(TEXT("interface_count"), Readback.InterfaceCount);
    Data->SetBoolField(TEXT("one_shot"), false);
    Data->SetBoolField(TEXT("created"), bCreated);
    Data->SetBoolField(TEXT("already_existed"), !bCreated);
    Data->SetBoolField(TEXT("saved"), true);
    Data->SetBoolField(TEXT("document_verified"), true);
    Data->SetBoolField(TEXT("mutation_attempted"), bCreated);
    Data->SetBoolField(TEXT("mutation_performed"), bCreated);
    Data->SetBoolField(TEXT("mutation_state_unknown"), false);
    Data->SetBoolField(TEXT("changed"), bCreated);
    TSharedPtr<FJsonObject> Verification = MakeShared<FJsonObject>();
    Verification->SetStringField(TEXT("status"), TEXT("passed"));
    Verification->SetStringField(TEXT("method"), TEXT("ue_5_8_metasound_document_saved_readback"));
    Data->SetObjectField(TEXT("verification"), Verification);
    return Data;
}



bool ReadSnapshot(UMetaSoundSource* Source, const TCHAR* Label, FResult& R, TSharedPtr<FJsonObject>& Snapshot)
{
    const FString ReceiptText = Audio::InspectAsset(Source->GetPathName()); TSharedPtr<FJsonObject> Receipt;
    if (ReceiptText.Len() > 16 * 1024 * 1024 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ReceiptText), Receipt) || !Receipt.IsValid())
    { R.bKnown = false; return SetFailure(R, TEXT("audio_metasound_observation_unobservable"), TEXT("Bounded shared audio observer receipt could not be decoded.")); }
    R.Data->SetObjectField(Label, Receipt);
    bool Success = false, Known = false, Changed = false, Post = false;
    if (!Receipt->TryGetBoolField(TEXT("success"), Success) || !Receipt->TryGetBoolField(TEXT("change_state_known"), Known) || !Receipt->TryGetBoolField(TEXT("changed"), Changed)
        || !Receipt->TryGetBoolField(TEXT("postcondition_passed"), Post) || !Success || !Known || Changed || !Post)
    { R.bKnown = false; return SetFailure(R, TEXT("audio_metasound_observation_unobservable"), TEXT("Actual shared audio observation failed or does not prove stable state after its initial getter; no writer replay.")); }
    const auto DataValue = Receipt->TryGetField(TEXT("data")); if (!DataValue.IsValid() || DataValue->Type != EJson::Object) { R.bKnown = false; return SetFailure(R, TEXT("audio_metasound_observation_unobservable"), TEXT("Shared observer data is absent.")); }
    const auto After = DataValue->AsObject()->TryGetField(TEXT("after")); if (!After.IsValid() || After->Type != EJson::Object) { R.bKnown = false; return SetFailure(R, TEXT("audio_metasound_observation_unobservable"), TEXT("Shared observer snapshot is absent.")); }
    Snapshot = After->AsObject(); return true;
}
bool Persisted(UMetaSoundSource* Source, const TSharedPtr<FJsonObject>& Snapshot, FResult& R, const TCHAR* Label)
{
    if (!Snapshot->GetObjectField(TEXT("disk"))->GetBoolField(TEXT("exists"))) { return SetFailure(R, TEXT("audio_metasound_not_persisted"), TEXT("RequirePersisted MetaSound file is absent.")); }
    auto References = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("live_reference_observation"), References); FString ReferenceError;
    if (!Audio::ValidateNativeAssetReferences(Source, References, ReferenceError))
    { R.Data->SetObjectField(TEXT("live_reference_observation"), References); return SetFailure(R, TEXT("audio_metasound_live_dependency_audit_failed"), ReferenceError); }
    R.Data->SetObjectField(TEXT("live_reference_observation"), References);
    const auto Audit = ReadPackageDependencies({ Source->GetOutermost()->GetName() }, true, 4096); R.Data->SetObjectField(Label, Audit);
    bool Passed = false; return (Audit->TryGetBoolField(TEXT("passed"), Passed) && Passed) || SetFailure(R, TEXT("audio_metasound_dependency_audit_failed"), TEXT("Current persisted dependency observation is incomplete/forbidden; no scan, cleanup or replay."));
}
void PublishLegacy(UMetaSoundSource* Source, const FString& Oscillator, double Frequency, const FString& OutputFormat, const FBasicMetaSoundReadback& Proof, bool bCreated, FResult& R)
{
    const auto Legacy = MakeMetaSoundData(Source, Oscillator, Frequency, OutputFormat, Proof, bCreated); for (const auto& Field : Legacy->Values) { R.Data->SetField(Field.Key, Field.Value); }
    auto Audit = MakeShared<FJsonObject>(); Audit->SetStringField(TEXT("status"), TEXT("passed")); Audit->SetStringField(TEXT("mode"), TEXT("require_persisted")); Audit->SetNumberField(TEXT("checked_asset_count"), 1); Audit->SetArrayField(TEXT("checked_assets"), { MakeShared<FJsonValueString>(Source->GetPathName()) }); R.Data->SetObjectField(TEXT("dependency_audit"), Audit); R.Data->SetBoolField(TEXT("uses_unrealmcp_classes"), false);
}
bool SavedObservation(const TSharedPtr<FJsonObject>& Before, const TSharedPtr<FJsonObject>& After)
{
    const auto A = Before->GetObjectField(TEXT("state")), B = After->GetObjectField(TEXT("state")); auto Expected = MakeShared<FJsonObject>(); for (const auto& Field : A->Values) { Expected->SetField(Field.Key, Field.Value); }
    for (const TCHAR* Key : { TEXT("path"), TEXT("class"), TEXT("outer") }) { if (A->GetStringField(Key) != B->GetStringField(Key)) { return false; } }
    const FString FlagText = A->GetStringField(TEXT("object_flags")); const uint64 Flags = FCString::Strtoui64(*FlagText, nullptr, 10);
    if (FlagText != FString::Printf(TEXT("%llu"), Flags) || B->GetStringField(TEXT("object_flags")) != FString::Printf(TEXT("%llu"), Flags | static_cast<uint64>(RF_WasLoaded | RF_LoadCompleted))) { return false; }
    Expected->SetStringField(TEXT("object_flags"), B->GetStringField(TEXT("object_flags"))); Expected->SetBoolField(TEXT("package_dirty"), false);
    return Text(Expected) == Text(B.ToSharedRef()) && Text(Before->GetObjectField(TEXT("other_package_dirty")).ToSharedRef()) == Text(After->GetObjectField(TEXT("other_package_dirty")).ToSharedRef());
}
}

FString CreateBasicSource(const FString& InputName, const FString& InputPackage, const FString& InputOscillator, double Frequency, const FString& InputFormat)
{
    FResult R; FString Name = InputName.TrimStartAndEnd(), Package = InputPackage.TrimStartAndEnd(), Oscillator = InputOscillator.TrimStartAndEnd().ToLower(), OutputFormat = InputFormat.TrimStartAndEnd().ToLower();
    Package.TrimQuotesInline(); Package.ReplaceInline(TEXT("\\"), TEXT("/")); while (Package.Len() > 1 && Package.EndsWith(TEXT("/"))) { Package.LeftChopInline(1); }
    const FString AssetPackage = Package + TEXT("/") + Name, ObjectPath = AssetPackage + TEXT(".") + Name; R.Data->SetStringField(TEXT("asset_path"), ObjectPath);
    R.Data->SetBoolField(TEXT("metasound_builder_implemented"), true); R.Data->SetBoolField(TEXT("audible_runtime_verified"), false); R.Data->SetBoolField(TEXT("implicit_build_work_measured"), false); R.Data->SetBoolField(TEXT("automatic_cleanup_attempted"), false);
    for (const TCHAR* Key : { TEXT("transient_builder_create_calls"), TEXT("generator_add_calls"), TEXT("frequency_set_calls"), TEXT("output_connect_calls"), TEXT("build_to_asset_calls"), TEXT("save_attempt_count"), TEXT("asset_registry_refresh_calls"), TEXT("asset_registry_refresh_file_count") }) { R.Data->SetNumberField(Key, 0); }
    R.Data->SetArrayField(TEXT("save_calls"), {}); R.Data->SetBoolField(TEXT("observations_complete"), false);
    R.Data->SetStringField(TEXT("dependency_tier"), TEXT("EngineAPI: public MetaSound Engine/Frontend/StandardNodes/Editor builder, existing Audio observer, EditorAssetLibrary/AssetRegistry; no ServerRPC/officialToolset or Cubeless runtime classes."));
    R.Data->SetStringField(TEXT("effect_scope"), TEXT("Transient fixed source builder then public BuildToAsset factory/document registration/input-template injection. SourceControl-enabled branch refuses before construction. Initial source/read getter/CDO and build/cache/world effects are not fully instrumented; explicit forced-save count is not a total Engine save count."));
    const TMap<FString, FString> Names = { {TEXT("sine"),TEXT("Sine")}, {TEXT("saw"),TEXT("Saw")}, {TEXT("square"),TEXT("Square")}, {TEXT("triangle"),TEXT("Triangle")}, {TEXT("noise"),TEXT("Noise")} };
    const FString* NodeName = Names.Find(Oscillator); FText Invalid;
    if (!IsEditorGameThread() || IsPlaySessionActive() || !GEditor || Name.IsEmpty() || Name.Len() > 64 || FName(*Name).IsNone() || Name.Contains(TEXT("/")) || Name.Contains(TEXT("\\")) || Name.Contains(TEXT("."))
        || !FName::IsValidXName(Name, INVALID_OBJECTNAME_CHARACTERS, &Invalid) || ContainsMCPToken(Name) || ContainsMCPToken(Package) || !(Package == TEXT("/Game") || Package.StartsWith(TEXT("/Game/")))
        || !FPackageName::IsValidLongPackageName(Package, false) || ObjectPath.Len() >= NAME_SIZE || !FPackageName::IsValidObjectPath(ObjectPath) || !NodeName || (OutputFormat != TEXT("mono") && OutputFormat != TEXT("stereo"))
        || !FMath::IsFinite(Frequency) || Frequency < 1.0 || Frequency > 20000.0)
    { SetFailure(R, TEXT("audio_metasound_invalid_preflight"), TEXT("Requires idle Editor, valid /Game identity, original five oscillator names/mono-stereo and finite frequency1..20000 (including noise).")); return Encode(R); }
    const EMetaSoundOutputAudioFormat Format = OutputFormat == TEXT("stereo") ? EMetaSoundOutputAudioFormat::Stereo : EMetaSoundOutputAudioFormat::Mono;
    const FMetasoundFrontendClassName GeneratorClass(FName(TEXT("UE")), FName(**NodeName), FName(TEXT("Audio"))); const bool HasFrequency = Oscillator != TEXT("noise");
    if (UEditorAssetLibrary::DoesAssetExist(AssetPackage) || FindObject<UObject>(nullptr, *ObjectPath))
    {
        R.Stage = TEXT("existing_target"); UMetaSoundSource* Existing = Cast<UMetaSoundSource>(UEditorAssetLibrary::LoadAsset(ObjectPath)); TStrongObjectPtr<UMetaSoundSource> Keep(Existing); FBasicMetaSoundReadback Proof;
        if (!NativeSource(Existing) || Existing->GetPathName() != ObjectPath || Existing->GetOutermost()->IsDirty() || !VerifyBasicMetaSound(Existing, GeneratorClass, HasFrequency, static_cast<float>(Frequency), Format, Proof))
        { SetFailure(R, TEXT("audio_asset_conflict"), TEXT("Existing MetaSound was not overwritten: require exact clean persisted requested native document shape.")); R.Data->SetStringField(TEXT("conflict_reason"), Proof.Failure); return Encode(R); }
        TSharedPtr<FJsonObject> Before, After;
        if (!ReadSnapshot(Existing, TEXT("before_native_audio_observer"), R, Before) || !Persisted(Existing, Before, R, TEXT("existing_dependency_audit")) || !ReadSnapshot(Existing, TEXT("after_native_audio_observer"), R, After)) { return Encode(R); }
        if (Text(Before.ToSharedRef()) != Text(After.ToSharedRef())) { R.bKnown = true; R.bChanged = true; SetFailure(R, TEXT("audio_metasound_existing_state_changed"), TEXT("Existing observed state changed during read/audit; no writer invoked.")); return Encode(R); }
        PublishLegacy(Existing, Oscillator, Frequency, OutputFormat, Proof, false, R); R.Data->SetBoolField(TEXT("observations_complete"), true); R.bSuccess = true; R.bPost = true; R.Stage = TEXT("complete"); return Encode(R);
    }
    FString Filename; if (!FPackageName::TryConvertLongPackageNameToFilename(AssetPackage, Filename, FPackageName::GetAssetPackageExtension()) || IFileManager::Get().FileExists(*Filename))
    { SetFailure(R, TEXT("audio_asset_conflict"), TEXT("An on-disk target exists or absence cannot be observed; no replacement.")); return Encode(R); }
    R.Data->SetBoolField(TEXT("source_control_enabled"), ISourceControlModule::Get().IsEnabled());
    if (R.Data->GetBoolField(TEXT("source_control_enabled"))) { SetFailure(R, TEXT("audio_metasound_source_control_path_unsupported"), TEXT("Public BuildToAsset may prompt/save with source control enabled; this bounded leaf refuses before builder/factory instead of changing settings or invoking that path.")); return Encode(R); }
    UMetaSoundBuilderSubsystem* Subsystem = UMetaSoundBuilderSubsystem::Get(); UMetaSoundEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMetaSoundEditorSubsystem>();
    if (!Loaded(Subsystem) || !Loaded(EditorSubsystem)) { SetFailure(R, TEXT("audio_metasound_builder_unavailable"), TEXT("Public native builder/editor subsystem is unavailable.")); return Encode(R); }
    FMetaSoundBuilderNodeOutputHandle OnPlay; FMetaSoundBuilderNodeInputHandle OnFinished; TArray<FMetaSoundBuilderNodeInputHandle> Outputs; EMetaSoundBuilderResult Result = EMetaSoundBuilderResult::Failed;
    R.Stage = TEXT("create_transient_builder"); R.Data->SetNumberField(TEXT("transient_builder_create_calls"), 1);
    UMetaSoundSourceBuilder* Builder = Subsystem->CreateSourceBuilder(FName(*FString::Printf(TEXT("AudioBuilder_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits))), OnPlay, OnFinished, Outputs, Result, Format, false); TStrongObjectPtr<UMetaSoundSourceBuilder> KeepBuilder(Builder);
    if (!Loaded(Builder) || Result != EMetaSoundBuilderResult::Succeeded || Outputs.Num() != (Format == EMetaSoundOutputAudioFormat::Stereo ? 2 : 1)) { SetFailure(R, TEXT("audio_metasound_builder_create_failed"), TEXT("Public transient source builder/output admission failed; no game asset factory called.")); return Encode(R); }
    R.Data->SetStringField(TEXT("transient_builder_path"), Builder->GetPathName()); R.Stage = TEXT("add_generator"); R.Data->SetNumberField(TEXT("generator_add_calls"), 1);
    const FMetaSoundNodeHandle Generator = Builder->AddNodeByClassName(GeneratorClass, Result, 1);
    if (Result != EMetaSoundBuilderResult::Succeeded || !Generator.IsSet()) { SetFailure(R, TEXT("audio_metasound_generator_unavailable"), TEXT("The exact original UE/Audio major1 generator was not found; no alternative class/version is substituted.")); return Encode(R); }
    if (HasFrequency)
    {
        R.Stage = TEXT("set_frequency"); const FMetaSoundBuilderNodeInputHandle Input = Builder->FindNodeInputByName(Generator, TEXT("Frequency"), Result);
        if (Result != EMetaSoundBuilderResult::Succeeded || !Input.IsSet()) { SetFailure(R, TEXT("audio_metasound_frequency_input_missing"), TEXT("Exact generator Frequency input is absent.")); return Encode(R); }
        FMetasoundFrontendLiteral Literal; Literal.Set(static_cast<float>(Frequency)); R.Data->SetNumberField(TEXT("frequency_set_calls"), 1); Builder->SetNodeInputDefault(Input, Literal, Result);
        if (Result != EMetaSoundBuilderResult::Succeeded) { SetFailure(R, TEXT("audio_metasound_frequency_set_failed"), TEXT("Transient frequency write failed; no game asset created.")); return Encode(R); }
    }
    R.Stage = TEXT("connect_outputs"); const FMetaSoundBuilderNodeOutputHandle AudioOutput = Builder->FindNodeOutputByName(Generator, TEXT("Audio"), Result);
    if (Result != EMetaSoundBuilderResult::Succeeded || !AudioOutput.IsSet()) { SetFailure(R, TEXT("audio_metasound_audio_output_missing"), TEXT("Exact generator Audio output is absent.")); return Encode(R); }
    for (const auto& Output : Outputs) { R.Data->SetNumberField(TEXT("output_connect_calls"), R.Data->GetNumberField(TEXT("output_connect_calls")) + 1); Builder->ConnectNodes(AudioOutput, Output, Result);
        if (Result != EMetaSoundBuilderResult::Succeeded) { SetFailure(R, TEXT("audio_metasound_connect_failed"), TEXT("Transient output connection failed; no game asset created.")); return Encode(R); } }
    if (ISourceControlModule::Get().IsEnabled()) { SetFailure(R, TEXT("audio_metasound_source_control_path_unsupported"), TEXT("Source control changed before public asset build; construction remains refused.")); return Encode(R); }
    R.Stage = TEXT("build_to_asset"); R.bAttempted = true; R.bKnown = false; R.bSaveRequested = true; R.Data->SetNumberField(TEXT("build_to_asset_calls"), 1);
    const TScriptInterface<IMetaSoundDocumentInterface> Built = EditorSubsystem->BuildToAsset(Builder, TEXT("Audio Authoring"), Name, Package, Result, nullptr); UMetaSoundSource* Source = Cast<UMetaSoundSource>(Built.GetObject()); TStrongObjectPtr<UMetaSoundSource> KeepSource(Source);
    if (Result != EMetaSoundBuilderResult::Succeeded || !NativeSource(Source) || Source->GetPathName() != ObjectPath) { SetFailure(R, TEXT("audio_metasound_build_to_asset_failed"), TEXT("Public build returned no exact native target; actual factory/document prefix remains unknown, no cleanup/retry.")); return Encode(R); }
    R.bChanged = true; R.Data->SetStringField(TEXT("built_source_path"), Source->GetPathName()); Source->Modify(); Source->MarkPackageDirty(); FBasicMetaSoundReadback BeforeProof;
    if (!VerifyBasicMetaSound(Source, GeneratorClass, HasFrequency, static_cast<float>(Frequency), Format, BeforeProof)) { R.Data->SetStringField(TEXT("document_failure"), BeforeProof.Failure); SetFailure(R, TEXT("audio_metasound_pre_save_document_failed"), TEXT("Retained constructed document does not match original finite shape; no exact save attempted.")); return Encode(R); }
    TSharedPtr<FJsonObject> Before, After; if (!ReadSnapshot(Source, TEXT("before_save_native_audio_observer"), R, Before)) { return Encode(R); } R.bKnown = true;
    auto References = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("pre_save_reference_observation"), References); FString ReferenceError;
    if (!Audio::ValidateNativeAssetReferences(Source, References, ReferenceError))
    { R.Data->SetObjectField(TEXT("pre_save_reference_observation"), References); SetFailure(R, TEXT("audio_metasound_pre_save_dependency_audit_failed"), ReferenceError); return Encode(R); }
    R.Data->SetObjectField(TEXT("pre_save_reference_observation"), References);
    R.Stage = TEXT("save_asset"); R.bKnown = false; R.bSaveAttempted = true; R.Data->SetNumberField(TEXT("save_attempt_count"), 1);
    auto Save = MakeShared<FJsonObject>(); Save->SetStringField(TEXT("asset_path"), ObjectPath); Save->SetBoolField(TEXT("attempted"), true); Save->SetBoolField(TEXT("success"), false); R.Data->SetArrayField(TEXT("save_calls"), {MakeShared<FJsonValueObject>(Save)});
    R.bSaved = UEditorAssetLibrary::SaveLoadedAsset(Source, false); Save->SetBoolField(TEXT("success"), R.bSaved);
    if (!ReadSnapshot(Source, TEXT("after_save_native_audio_observer"), R, After)) { return Encode(R); }
    if (!R.bSaved) { SetFailure(R, TEXT("audio_metasound_save_failed"), TEXT("Exact save returned false; observed receipt retained, disk outcome unknown, no retry.")); return Encode(R); }
    R.bKnown = true; const bool StatePreserved = SavedObservation(Before, After); R.Data->SetBoolField(TEXT("saved_state_preserved"), StatePreserved); FBasicMetaSoundReadback AfterProof;
    if (!StatePreserved || Source->GetOutermost()->IsDirty() || !VerifyBasicMetaSound(Source, GeneratorClass, HasFrequency, static_cast<float>(Frequency), Format, AfterProof)) { R.Data->SetStringField(TEXT("document_failure"), AfterProof.Failure); SetFailure(R, TEXT("audio_metasound_saved_readback_failed"), TEXT("Actual saved prefix retained; owned flags/document/unrelated dirty readback failed without retry.")); return Encode(R); }
    R.Data->SetBoolField(TEXT("saved_state_preserved_before_registry_refresh"), R.Data->GetBoolField(TEXT("saved_state_preserved"))); R.Data->SetBoolField(TEXT("saved_state_preserved"), false);
    R.Stage = TEXT("refresh_saved_asset_registry"); auto Refresh = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("saved_asset_registry_refresh"), Refresh); FString RefreshError;
    const bool Refreshed = Audio::RefreshSavedAssetRegistry(Source, Refresh, RefreshError);
    R.Data->SetNumberField(TEXT("asset_registry_refresh_calls"), Refresh->GetNumberField(TEXT("scan_call_count"))); R.Data->SetNumberField(TEXT("asset_registry_refresh_file_count"), Refresh->GetNumberField(TEXT("file_count")));
    if (!Refreshed) { R.bKnown = Refresh->GetBoolField(TEXT("asset_state_change_known")); SetFailure(R, TEXT("audio_metasound_saved_registry_refresh_failed"), RefreshError); return Encode(R); }
    R.Data->SetBoolField(TEXT("saved_state_preserved"), true);
    FBasicMetaSoundReadback RefreshedProof;
    if (!VerifyBasicMetaSound(Source, GeneratorClass, HasFrequency, static_cast<float>(Frequency), Format, RefreshedProof)) { R.Data->SetStringField(TEXT("document_failure"), RefreshedProof.Failure); SetFailure(R, TEXT("audio_metasound_registry_refresh_document_failed"), TEXT("Exact saved-file registry call returned but final finite document proof failed; no save or scan retry.")); return Encode(R); }
    if (!Persisted(Source, After, R, TEXT("saved_dependency_audit"))) { return Encode(R); }
    PublishLegacy(Source, Oscillator, Frequency, OutputFormat, RefreshedProof, true, R); R.Data->SetBoolField(TEXT("observations_complete"), true); R.bSuccess = true; R.bPost = true; R.Stage = TEXT("complete"); return Encode(R);
}
}
