#include "Compatibility/UE58/CubelessPCGApi.h"

#include "Editor.h"
#include "Engine/World.h"
#include "UObject/GarbageCollection.h"
#include "Misc/StringOutputDevice.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/PropertyAccessUtil.h"
#include "ToolsetRegistry/ToolsetLibrary.h"
#include "PCGPoint.h"
#include "PCGGraph.h"
#include "PCGInputOutputSettings.h"
#include "PCGEdge.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGSettings.h"
#include "Elements/PCGProjectionElement.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Data/Registry/PCGDataTypeCommon.h"

bool CubelessPCGUE58::IsDensityFilterEditorDebugProperty(const UClass* Class, FProperty* Property)
{
    const auto* Bool = CastField<FBoolProperty>(Property);
    return Class && Class->GetPathName().Equals(TEXT("/Script/PCG.PCGDensityFilterSettings"), ESearchCase::CaseSensitive) &&
        Class->HasAllClassFlags(CLASS_Native) && !Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) &&
        Bool && Bool->IsNativeBool() && Property->GetOwnerStruct() == Class && Property->GetFName() == TEXT("bKeepZeroDensityPoints") &&
        Property->ArrayDim == 1 && Property->GetSize() == sizeof(bool) && !Property->HasSetterOrGetter() &&
        Property->HasAllPropertyFlags(CPF_Edit | CPF_BlueprintVisible | CPF_Transient | CPF_EditorOnly) &&
        !Property->HasAnyPropertyFlags(CPF_EditConst | CPF_Deprecated);
}

namespace
{
    using FJson = TSharedPtr<FJsonObject>;

    FString Encode(const FJson& Value)
    {
        FString Text;
        FJsonSerializer::Serialize(Value.ToSharedRef(), TJsonWriterFactory<>::Create(&Text));
        return Text;
    }

    FJson Outcome(bool bSuccess, const FString& Code = FString(), const FString& Error = FString())
    {
        FJson Value = MakeShared<FJsonObject>();
        Value->SetBoolField(TEXT("success"), bSuccess);
        Value->SetStringField(TEXT("error_code"), Code);
        Value->SetStringField(TEXT("error"), Error);
        Value->SetBoolField(TEXT("mutation_attempted"), false);
        Value->SetBoolField(TEXT("mutation_performed"), false);
        Value->SetBoolField(TEXT("outcome_unknown"), false);
        Value->SetBoolField(TEXT("asset_load_attempted"), false);
        Value->SetBoolField(TEXT("save_attempted"), false);
        return Value;
    }

    bool Bounded(const FString& Value, bool bAllowEmpty = false)
    {
        if ((!bAllowEmpty && Value.IsEmpty()) || Value.Len() >= 1024) return false;
        for (TCHAR Ch : Value) if (Ch < 32 || Ch == 127) return false;
        return true;
    }

    FString Token(FString Value)
    {
        Value.TrimStartAndEndInline();
        Value.ToLowerInline();
        Value.ReplaceInline(TEXT("_"), TEXT(""));
        Value.ReplaceInline(TEXT("-"), TEXT(""));
        Value.ReplaceInline(TEXT(" "), TEXT(""));
        return Value;
    }

    UPCGGraph* Graph(const FString& Path, FJson& Error, bool bAssetOnly = false)
    {
        if (!IsInGameThread() || !GEditor || GEditor->PlayWorld)
        {
            Error = Outcome(false, TEXT("pcg_editor_not_idle"), TEXT("Requires the editor game thread without PIE/SIE"));
            return nullptr;
        }
        if (!Bounded(Path) || (!Path.StartsWith(TEXT("/Game/")) && (bAssetOnly || !Path.StartsWith(TEXT("/Engine/Transient.")))))
        {
            Error = Outcome(false, TEXT("pcg_graph_path_invalid"), TEXT("Use the exact loaded /Game graph object path; read-only previews also admit an exact transient PCGGraph"));
            return nullptr;
        }
        UPCGGraph* Value = FindObject<UPCGGraph>(nullptr, *Path);
        if (!IsValid(Value) || Value->GetClass() != UPCGGraph::StaticClass() || Value->GetPathName() != Path ||
            Value->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed | RF_FinishDestroyed) ||
            !IsValid(Value->GetPackage()) || Value->GetNodes().Num() > 256)
        {
            Error = Outcome(false, TEXT("pcg_graph_not_loaded"), TEXT("Exact, ordinary, bounded PCGGraph was not already loaded; no load or fallback executed"));
            return nullptr;
        }
        return Value;
    }

    UPCGNode* Node(UPCGGraph* Owner, const FString& Id, FJson& Error)
    {
        if (!Bounded(Id))
        {
            Error = Outcome(false, TEXT("pcg_node_id_invalid"), TEXT("Node identifier must be bounded"));
            return nullptr;
        }
        TArray<UPCGNode*> Candidates = Owner->GetNodes();
        Candidates.Add(Owner->GetInputNode());
        Candidates.Add(Owner->GetOutputNode());
        UPCGNode* Found = nullptr;
        for (UPCGNode* Candidate : Candidates)
        {
            if (!IsValid(Candidate) || Candidate->GetOuter() != Owner) continue;
            const bool bMatches = Candidate->GetPathName() == Id || Candidate->GetName() == Id || Candidate->NodeTitle.ToString() == Id;
            if (!bMatches) continue;
            if (Found && Found != Candidate)
            {
                Error = Outcome(false, TEXT("pcg_node_ambiguous"), TEXT("Node name/title is ambiguous; use the exact object path"));
                return nullptr;
            }
            Found = Candidate;
        }
        if (!Found) Error = Outcome(false, TEXT("pcg_node_not_found"), TEXT("No exact member node in the loaded graph"));
        return Found;
    }

    FJson SelectorState(const FPCGAttributePropertySelector& Value)
    {
        FJson Row = MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("selection_value"), static_cast<int64>(Value.GetSelection()));
        Row->SetStringField(TEXT("selection"), StaticEnum<EPCGAttributePropertySelection>()->GetNameStringByValue(static_cast<int64>(Value.GetSelection())));
        Row->SetStringField(TEXT("attribute_name"), Value.GetAttributeName().ToString());
        Row->SetStringField(TEXT("selected_property_name"), Value.GetPropertyName().ToString());
        Row->SetStringField(TEXT("domain_name"), Value.GetDomainName().ToString());
        Row->SetStringField(TEXT("point_property"), StaticEnum<EPCGPointProperties>()->GetNameStringByValue(static_cast<int64>(Value.GetPointProperty())));
        Row->SetStringField(TEXT("extra_property"), StaticEnum<EPCGExtraProperties>()->GetNameStringByValue(static_cast<int64>(Value.GetExtraProperty())));
        Row->SetStringField(TEXT("name"), Value.GetName().ToString());
        Row->SetStringField(TEXT("display"), Value.ToString());
        Row->SetBoolField(TEXT("valid"), Value.IsValid());
        TArray<TSharedPtr<FJsonValue>> Extra;
        for (const FString& Item : Value.GetExtraNames()) Extra.Add(MakeShared<FJsonValueString>(Item));
        Row->SetArrayField(TEXT("extra_names"), Extra);
        return Row;
    }

    struct FSelectorContext
    {
        UPCGGraph* Owner = nullptr;
        UPCGNode* Member = nullptr;
        UPCGSettings* Settings = nullptr;
        FStructProperty* Property = nullptr;
        FPCGAttributePropertySelector* Selector = nullptr;
        bool bDirty = false;
    };

    bool Selector(const FString& GraphPath, const FString& NodeId, const FString& PropertyName, FSelectorContext& Context, FJson& Error)
    {
        Context.Owner = Graph(GraphPath, Error);
        if (!Context.Owner) return false;
        Context.Member = Node(Context.Owner, NodeId, Error);
        if (!Context.Member) return false;
        Context.Settings = Context.Member->GetSettings();
        if (!Bounded(PropertyName) || !IsValid(Context.Settings) || Context.Settings->GetOuter() != Context.Member ||
            !Context.Settings->GetClass()->GetPathName().StartsWith(TEXT("/Script/PCG.")))
        {
            Error = Outcome(false, TEXT("pcg_selector_settings_invalid"), TEXT("Selector requires the exact native runtime PCG settings owned by this node"));
            return false;
        }
        for (TFieldIterator<FProperty> It(Context.Settings->GetClass()); It; ++It)
        {
            if (Token(It->GetName()) != Token(PropertyName)) continue;
            if (Context.Property)
            {
                Error = Outcome(false, TEXT("pcg_selector_property_ambiguous"), TEXT("Selector property normalization is ambiguous"));
                return false;
            }
            Context.Property = CastField<FStructProperty>(*It);
            if (!Context.Property) break;
        }
        if (!Context.Property || Context.Property->ArrayDim != 1 ||
            !Context.Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible) || Context.Property->HasAnyPropertyFlags(CPF_EditConst) ||
            !Context.Property->Struct->IsChildOf(FPCGAttributePropertySelector::StaticStruct()))
        {
            Error = Outcome(false, TEXT("pcg_selector_property_invalid"), TEXT("Property must be one editable FPCGAttributePropertySelector-derived scalar; no container/private-field fallback"));
            return false;
        }
        Context.Selector = Context.Property->ContainerPtrToValuePtr<FPCGAttributePropertySelector>(Context.Settings);
        Context.bDirty = Context.Owner->GetPackage()->IsDirty();
        return Context.Selector != nullptr;
    }

    FJson Identity(const FSelectorContext& Context)
    {
        FJson Value = Outcome(true);
        Value->SetStringField(TEXT("graph"), Context.Owner->GetPathName());
        Value->SetStringField(TEXT("node_id"), Context.Member->GetPathName());
        Value->SetStringField(TEXT("settings_path"), Context.Settings->GetPathName());
        Value->SetStringField(TEXT("settings_class"), Context.Settings->GetClass()->GetPathName());
        Value->SetStringField(TEXT("selector_property_name"), Context.Property->GetName());
        Value->SetStringField(TEXT("selector_property_path"), Context.Property->GetPathName());
        Value->SetStringField(TEXT("selector_struct_path"), Context.Property->Struct->GetPathName());
        Value->SetBoolField(TEXT("package_dirty_before"), Context.bDirty);
        Value->SetBoolField(TEXT("package_dirty_after"), Context.Owner->GetPackage()->IsDirty());
        Value->SetBoolField(TEXT("read_only"), true);
        return Value;
    }

    bool StringField(const FJson& Request, const TCHAR* Key, FString& Value, FJson& Error, bool bRequired = false)
    {
        if (!Request->HasField(Key))
        {
            if (!bRequired) return true;
            Error = Outcome(false, TEXT("pcg_selector_request_invalid"), FString(Key) + TEXT(" is required"));
            return false;
        }
        if (!Request->TryGetStringField(Key, Value) || !Bounded(Value, true))
        {
            Error = Outcome(false, TEXT("pcg_selector_request_invalid"), FString(Key) + TEXT(" must be a bounded string"));
            return false;
        }
        return true;
    }

    bool EnumValue(const UEnum* Enum, const FString& Text, int64& Value)
    {
        FString Name = Text;
        int32 Dot;
        if (Name.FindLastChar(TEXT('.'), Dot)) Name = Name.Mid(Dot + 1);
        for (int32 I = 0; I < Enum->NumEnums() - 1; ++I)
        {
            if (Token(Enum->GetNameStringByIndex(I)) == Token(Name))
            {
                Value = Enum->GetValueByIndex(I);
                return true;
            }
        }
        return false;
    }

    bool ObservedObject(const UObject* Value)
    {
        return IsValid(Value) && !Value->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject |
            RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_BeginDestroyed | RF_FinishDestroyed);
    }

    // Keep the existing Python snapshot's JSON identities/order, without
    // exporting nested pin/edge/AllowedTypes wrappers to CPython. This avoids
    // the observed fatal traversal; it does not establish its root cause.
    bool CaptureGraphSnapshot(UPCGGraph* Owner, FJson& Snapshot, FJson& Error, TFunctionRef<bool()>* Current = nullptr)
    {
        const TArray<UPCGNode*> Authored = Owner->GetNodes();
        TArray<UPCGNode*> AllNodes = Authored;
        AllNodes.Add(Owner->GetInputNode());
        AllNodes.Add(Owner->GetOutputNode());
        TSet<const UPCGNode*> Nodes;
        TSet<const UPCGPin*> Inputs, Outputs;
        TMap<const UPCGEdge*, int32> EdgeReferences;
        TArray<TSharedPtr<FJsonValue>> NodeRows;
        auto Reject = [&Error](const TCHAR* Code, const TCHAR* Message)
        {
            Error = Outcome(false, Code, Message);
            Error->SetBoolField(TEXT("read_only"), true);
            Error->SetBoolField(TEXT("observation_complete"), false);
            return false;
        };
        for (const UPCGNode* Member : AllNodes)
        {
            if (!ObservedObject(Member) || Member->GetOuter() != Owner || Nodes.Contains(Member))
                return Reject(TEXT("pcg_node_owner_mismatch"), TEXT("Graph contains a missing, duplicate, incomplete or foreign factory/member node"));
            Nodes.Add(Member);
            UPCGSettings* Settings = Member->GetSettings();
            if (Current && !(*Current)()) return false;
            const bool bFactoryInput = Member == Owner->GetInputNode();
            const bool bFactoryOutput = Member == Owner->GetOutputNode();
            const bool bFactoryIO = bFactoryInput || bFactoryOutput;
            const UPCGGraphInputOutputSettings* IO = bFactoryIO && ObservedObject(Settings)
                && Settings->GetClass() == UPCGGraphInputOutputSettings::StaticClass()
                ? Cast<UPCGGraphInputOutputSettings>(Settings) : nullptr;
            // PCGGraph's constructor owns both IO settings directly, unlike
            // authored settings. Only the exact factory pointers/type/direction
            // may use that graph outer; ordinary members remain node-owned.
            const UObject* ExpectedSettingsOuter = bFactoryIO ? static_cast<const UObject*>(Owner) : static_cast<const UObject*>(Member);
            const bool bFactoryIdentity = !bFactoryIO || (bFactoryInput != bFactoryOutput
                && Member->GetClass() == UPCGNode::StaticClass() && IO && IO->IsInput() == bFactoryInput);
            if (!ObservedObject(Settings) || Settings->GetOuter() != ExpectedSettingsOuter || !bFactoryIdentity ||
                !Settings->GetClass()->HasAllClassFlags(CLASS_Native) ||
                Settings->GetClass()->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists) ||
                !Settings->GetClass()->GetPathName().StartsWith(TEXT("/Script/PCG.")))
            {
                Reject(TEXT("pcg_snapshot_settings_invalid"), TEXT("Authored settings must be node-owned; only exact factory Input/Output node pointers with matching native PCGGraphInputOutputSettings direction may have this graph outer"));
                Error->SetStringField(TEXT("node_path"), Member->GetPathName().Left(1024));
                Error->SetStringField(TEXT("node_outer_path"), Member->GetOuter() ? Member->GetOuter()->GetPathName().Left(1024) : FString());
                Error->SetStringField(TEXT("node_class"), Member->GetClass()->GetPathName().Left(1024));
                Error->SetStringField(TEXT("node_class_flags_string"), LexToString(static_cast<uint32>(Member->GetClass()->GetClassFlags())));
                Error->SetStringField(TEXT("settings_path"), IsValid(Settings) ? Settings->GetPathName().Left(1024) : FString());
                Error->SetStringField(TEXT("settings_outer_path"), IsValid(Settings) && Settings->GetOuter() ? Settings->GetOuter()->GetPathName().Left(1024) : FString());
                Error->SetStringField(TEXT("settings_class"), IsValid(Settings) ? Settings->GetClass()->GetPathName().Left(1024) : FString());
                Error->SetStringField(TEXT("settings_class_flags_string"), IsValid(Settings) ? LexToString(static_cast<uint32>(Settings->GetClass()->GetClassFlags())) : FString());
                Error->SetStringField(TEXT("expected_settings_outer_path"), ExpectedSettingsOuter->GetPathName().Left(1024));
                Error->SetBoolField(TEXT("factory_input_node"), bFactoryInput); Error->SetBoolField(TEXT("factory_output_node"), bFactoryOutput);
                Error->SetBoolField(TEXT("factory_settings_identity_verified"), bFactoryIdentity);
                Error->SetStringField(TEXT("identity_text_scope"), TEXT("paths/class names capped at 1024 characters; native class flags serialized as strings"));
                if (IO) { Error->SetBoolField(TEXT("settings_is_input"), IO->IsInput()); }
                return false;
            }
            FJson Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("node_path"), Member->GetPathName());
            Row->SetStringField(TEXT("node_name"), Member->GetName());
            Row->SetStringField(TEXT("title"), Member->GetAuthoredTitleName().ToString());
            int32 X, Y; Member->GetNodePosition(X, Y);
            Row->SetArrayField(TEXT("position"), {MakeShared<FJsonValueNumber>(X), MakeShared<FJsonValueNumber>(Y)});
            Row->SetStringField(TEXT("settings_path"), Settings->GetPathName());
            Row->SetStringField(TEXT("settings_class"), Settings->GetClass()->GetPathName());
            auto Pins = [&](const TArray<TObjectPtr<UPCGPin>>& Supplied, bool bInput, TArray<TSharedPtr<FJsonValue>>& Rows)
            {
                const TArray<TObjectPtr<UPCGPin>> Stable = Current ? Supplied : TArray<TObjectPtr<UPCGPin>>();
                const TArray<TObjectPtr<UPCGPin>>& Values = Current ? Stable : Supplied;
                if (Values.Num() > 256 || Inputs.Num() + Outputs.Num() + Values.Num() > 4096)
                    return Reject(TEXT("pcg_observation_bound_exceeded"), TEXT("Pin observation exceeds 256 per side or 4096 per graph"));
                for (const UPCGPin* Pin : Values)
                {
                    if (Current && !(*Current)()) return false;
                    if (!ObservedObject(Pin) || Pin->GetOuter() != Member || Pin->Node != Member ||
                        Inputs.Contains(Pin) || Outputs.Contains(Pin) || Pin->Edges.Num() > 2048)
                        return Reject(TEXT("pcg_snapshot_pin_invalid"), TEXT("Pin owner/identity, loaded state or bounded edge list is invalid"));
                    (bInput ? Inputs : Outputs).Add(Pin);
                    FJson PinRow = MakeShared<FJsonObject>();
                    PinRow->SetStringField(TEXT("path"), Pin->GetPathName());
                    PinRow->SetStringField(TEXT("label"), Pin->Properties.Label.ToString());
                    // Match PyWrapperStruct::ExportToText exactly: public
                    // UScriptStruct serializer, same value/default and flags.
                    // FPCGDataTypeIdentifier's private Ids are never accessed.
                    const FPCGDataTypeIdentifier& Allowed = Pin->Properties.AllowedTypes;
                    FString AllowedText;
                    FPCGDataTypeIdentifier::StaticStruct()->ExportText(AllowedText, &Allowed, &Allowed, nullptr, PPF_None, nullptr);
                    if (Current && !(*Current)()) return false;
                    if (AllowedText.Len() > 65536)
                        return Reject(TEXT("pcg_observation_bound_exceeded"), TEXT("AllowedTypes serialized text exceeds 65536 characters"));
                    FJson AllowedRow = MakeShared<FJsonObject>();
                    AllowedRow->SetStringField(TEXT("struct_type"), TEXT("PCGDataTypeIdentifier"));
                    AllowedRow->SetStringField(TEXT("export_text"), AllowedText);
                    PinRow->SetObjectField(TEXT("allowed_types"), AllowedRow);
                    TArray<FString> Paths; TSet<const UPCGEdge*> PinEdges;
                    for (const UPCGEdge* Edge : Pin->Edges)
                    {
                        if (!ObservedObject(Edge) || PinEdges.Contains(Edge))
                            return Reject(TEXT("pcg_edge_observation_invalid"), TEXT("Pin edge list contains an incomplete or duplicate edge"));
                        PinEdges.Add(Edge); Paths.Add(Edge->GetPathName());
                        const int32 References = ++EdgeReferences.FindOrAdd(Edge);
                        if (References > 2 || EdgeReferences.Num() > 2048)
                            return Reject(TEXT("pcg_edge_observation_invalid"), TEXT("Physical edges exceed 2048 or occur in more than two pin lists"));
                    }
                    Paths.Sort(); TArray<TSharedPtr<FJsonValue>> EdgePaths;
                    for (const FString& Path : Paths) EdgePaths.Add(MakeShared<FJsonValueString>(Path));
                    PinRow->SetArrayField(TEXT("edges"), EdgePaths);
                    Rows.Add(MakeShared<FJsonValueObject>(PinRow));
                }
                return true;
            };
            TArray<TSharedPtr<FJsonValue>> InputRows, OutputRows;
            if (!Pins(Member->GetInputPins(), true, InputRows) || !Pins(Member->GetOutputPins(), false, OutputRows)) return false;
            Row->SetArrayField(TEXT("input_pins"), InputRows);
            Row->SetArrayField(TEXT("output_pins"), OutputRows);
            NodeRows.Add(MakeShared<FJsonValueObject>(Row));
        }
        if (EdgeReferences.Num() > 2048)
            return Reject(TEXT("pcg_edge_observation_invalid"), TEXT("Physical edge inventory exceeds 2048"));
        for (const auto& Pair : EdgeReferences)
        {
            const UPCGEdge* Edge = Pair.Key;
            const UPCGPin* Source = Edge->InputPin.Get();
            const UPCGPin* Target = Edge->OutputPin.Get();
            if (Pair.Value != 2 || !Outputs.Contains(Source) || !Inputs.Contains(Target) ||
                (Edge->GetOuter() != Source && Edge->GetOuter() != Target) ||
                !Source->Edges.Contains(Edge) || !Target->Edges.Contains(Edge))
                return Reject(TEXT("pcg_edge_observation_invalid"), TEXT("Physical edge does not belong exactly to one owned output/input pin pair"));
        }
        // GetAllEdges reads these already validated native input pin arrays;
        // its native traversal order matches the previous Python method.
        const TArray<UPCGEdge*> Physical = Owner->GetAllEdges();
        TSet<const UPCGEdge*> UniqueEdges; TArray<TSharedPtr<FJsonValue>> EdgeRows;
        if (Physical.Num() != EdgeReferences.Num())
            return Reject(TEXT("pcg_edge_observation_invalid"), TEXT("Graph physical inventory disagrees with the two-sided pin inventory"));
        for (const UPCGEdge* Edge : Physical)
        {
            if (!EdgeReferences.Contains(Edge) || UniqueEdges.Contains(Edge))
                return Reject(TEXT("pcg_edge_observation_invalid"), TEXT("Graph inventory contains a repeated or foreign physical edge"));
            UniqueEdges.Add(Edge); FJson Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("edge_path"), Edge->GetPathName());
            Row->SetStringField(TEXT("from_node"), Edge->InputPin->Node->GetPathName());
            Row->SetStringField(TEXT("from_pin"), Edge->InputPin->Properties.Label.ToString());
            Row->SetStringField(TEXT("to_node"), Edge->OutputPin->Node->GetPathName());
            Row->SetStringField(TEXT("to_pin"), Edge->OutputPin->Properties.Label.ToString());
            EdgeRows.Add(MakeShared<FJsonValueObject>(Row));
        }
        Snapshot = MakeShared<FJsonObject>();
        Snapshot->SetStringField(TEXT("graph"), Owner->GetPathName());
        Snapshot->SetStringField(TEXT("package"), Owner->GetPackage()->GetPathName());
        Snapshot->SetNumberField(TEXT("authored_node_count"), Authored.Num());
        Snapshot->SetNumberField(TEXT("node_count_including_factory"), AllNodes.Num());
        Snapshot->SetNumberField(TEXT("edge_count"), Physical.Num());
        Snapshot->SetArrayField(TEXT("nodes"), NodeRows);
        Snapshot->SetArrayField(TEXT("edges"), EdgeRows);
        return true;
    }

    // Finite fields of the original nine-node CPU falloff family. This is not
    // a generic property writer; selectors retain their separate named API.
    bool SamplingEditorDebugProperty(const UClass* Class,const FProperty* Property)
    {return Class&&Property&&Class->GetPathName()==TEXT("/Script/PCG.PCGSurfaceSamplerSettings")&&Property->GetOwnerStruct()==Class&&Property->GetFName()==TEXT("bKeepZeroDensityPoints")&&Property->GetClass()==FBoolProperty::StaticClass()&&Property->HasAnyPropertyFlags(CPF_Transient);}
    const TMap<FString, TMap<FString, FString>>& SettingFields()
    {
        static const TMap<FString, TMap<FString, FString>> Fields = {
            {TEXT("PCGDataFromActorSettings"), {{TEXT("mode"),TEXT("Mode")},{TEXT("ignorepcggeneratedcomponents"),TEXT("bIgnorePCGGeneratedComponents")},{TEXT("alsooutputsinglepointdata"),TEXT("bAlsoOutputSinglePointData")},{TEXT("componentsmustoverlapself"),TEXT("bComponentsMustOverlapSelf")},{TEXT("getdataonallgrids"),TEXT("bGetDataOnAllGrids")},{TEXT("allowedgrids"),TEXT("AllowedGrids")},{TEXT("mergesinglepointdata"),TEXT("bMergeSinglePointData")},{TEXT("propertyname"),TEXT("PropertyName")},{TEXT("alwaysrequeryactors"),TEXT("bAlwaysRequeryActors")},{TEXT("silencesanitizedattributenamewarnings"),TEXT("bSilenceSanitizedAttributeNameWarnings")},{TEXT("silencereservedattributenamewarnings"),TEXT("bSilenceReservedAttributeNameWarnings")},{TEXT("trackactorsonlywithinbounds"),TEXT("bTrackActorsOnlyWithinBounds")}}},
            {TEXT("PCGGetLandscapeSettings"), {{TEXT("mode"),TEXT("Mode")},{TEXT("ignorepcggeneratedcomponents"),TEXT("bIgnorePCGGeneratedComponents")},{TEXT("alsooutputsinglepointdata"),TEXT("bAlsoOutputSinglePointData")},{TEXT("componentsmustoverlapself"),TEXT("bComponentsMustOverlapSelf")},{TEXT("getdataonallgrids"),TEXT("bGetDataOnAllGrids")},{TEXT("allowedgrids"),TEXT("AllowedGrids")},{TEXT("mergesinglepointdata"),TEXT("bMergeSinglePointData")},{TEXT("propertyname"),TEXT("PropertyName")},{TEXT("alwaysrequeryactors"),TEXT("bAlwaysRequeryActors")},{TEXT("silencesanitizedattributenamewarnings"),TEXT("bSilenceSanitizedAttributeNameWarnings")},{TEXT("silencereservedattributenamewarnings"),TEXT("bSilenceReservedAttributeNameWarnings")},{TEXT("trackactorsonlywithinbounds"),TEXT("bTrackActorsOnlyWithinBounds")},{TEXT("unbounded"),TEXT("bUnbounded")}}},
            {TEXT("PCGSurfaceSamplerSettings"), {{TEXT("pointspersquaredmeter"),TEXT("PointsPerSquaredMeter")},{TEXT("pointextents"),TEXT("PointExtents")},
                {TEXT("looseness"),TEXT("Looseness")},{TEXT("unbounded"),TEXT("bUnbounded")},{TEXT("applydensitytopoints"),TEXT("bApplyDensityToPoints")},
                {TEXT("pointsteepness"),TEXT("PointSteepness")},{TEXT("keepzerodensitypoints"),TEXT("bKeepZeroDensityPoints")},{TEXT("uselegacygridcreationmethod"),TEXT("bUseLegacyGridCreationMethod")}}},
            // ADP: graph-spec partial ProjectionParams dictionaries use typed native fields.
            {TEXT("PCGProjectionSettings"), {{TEXT("projectionparams"),TEXT("ProjectionParams")},{TEXT("forcecollapsetopoint"),TEXT("bForceCollapseToPoint")},{TEXT("keepzerodensitypoints"),TEXT("bKeepZeroDensityPoints")}}},
            // ADL: exact public own spatial-composition enum/bool fields; no execution/dynamic-pin writer.
            {TEXT("PCGUnionSettings"), {{TEXT("type"),TEXT("Type")},{TEXT("densityfunction"),TEXT("DensityFunction")}}},
            {TEXT("PCGInnerIntersectionSettings"), {{TEXT("densityfunction"),TEXT("DensityFunction")},{TEXT("keepzerodensitypoints"),TEXT("bKeepZeroDensityPoints")}}},
            {TEXT("PCGOuterIntersectionSettings"), {{TEXT("densityfunction"),TEXT("DensityFunction")},{TEXT("ignorepinswithnoinput"),TEXT("bIgnorePinsWithNoInput")},{TEXT("keepzerodensitypoints"),TEXT("bKeepZeroDensityPoints")}}},
            {TEXT("PCGDifferenceSettings"), {{TEXT("densityfunction"),TEXT("DensityFunction")},{TEXT("mode"),TEXT("Mode")},
                {TEXT("diffmetadata"),TEXT("bDiffMetadata")},{TEXT("keepzerodensitypoints"),TEXT("bKeepZeroDensityPoints")}}},
            {TEXT("PCGTransformPointsSettings"), {{TEXT("applytoattribute"),TEXT("bApplyToAttribute")},{TEXT("attributename"),TEXT("AttributeName")},
                {TEXT("offsetmin"),TEXT("OffsetMin")},{TEXT("offsetmax"),TEXT("OffsetMax")},{TEXT("absoluteoffset"),TEXT("bAbsoluteOffset")},
                {TEXT("rotationmin"),TEXT("RotationMin")},{TEXT("rotationmax"),TEXT("RotationMax")},{TEXT("absoluterotation"),TEXT("bAbsoluteRotation")},
                {TEXT("scalemin"),TEXT("ScaleMin")},{TEXT("scalemax"),TEXT("ScaleMax")},{TEXT("absolutescale"),TEXT("bAbsoluteScale")},
                {TEXT("uniformscale"),TEXT("bUniformScale")},{TEXT("recomputeseed"),TEXT("bRecomputeSeed")}}},
            {TEXT("PCGPointExtentsModifierSettings"), {{TEXT("extents"),TEXT("Extents")},{TEXT("mode"),TEXT("Mode")}}},
            {TEXT("PCGBoundsModifierSettings"), {{TEXT("mode"),TEXT("Mode")},{TEXT("boundsmin"),TEXT("BoundsMin")},
                {TEXT("boundsmax"),TEXT("BoundsMax")},{TEXT("affectsteepness"),TEXT("bAffectSteepness")},{TEXT("steepness"),TEXT("Steepness")}}},
            {TEXT("PCGCreatePointsGridSettings"), {{TEXT("gridextents"),TEXT("GridExtents")},{TEXT("cellsize"),TEXT("CellSize")},
                {TEXT("coordinatespace"),TEXT("CoordinateSpace")},{TEXT("pointposition"),TEXT("PointPosition")},
                {TEXT("pointsteepness"),TEXT("PointSteepness")},{TEXT("setpointsbounds"),TEXT("bSetPointsBounds")},
                {TEXT("cullpointsoutsidevolume"),TEXT("bCullPointsOutsideVolume")}}},
            {TEXT("PCGCreatePointsSettings"), {{TEXT("coordinatespace"),TEXT("CoordinateSpace")},
                {TEXT("pointstocreate"),TEXT("PointsToCreate")},{TEXT("cullpointsoutsidevolume"),TEXT("bCullPointsOutsideVolume")}}},
            {TEXT("PCGDistanceSettings"), {{TEXT("setdensity"),TEXT("bSetDensity")},{TEXT("maximumdistance"),TEXT("MaximumDistance")},
                {TEXT("outputtoattribute"),TEXT("bOutputToAttribute")},{TEXT("outputdistancevector"),TEXT("bOutputDistanceVector")},
                {TEXT("sourceshape"),TEXT("SourceShape")},{TEXT("targetshape"),TEXT("TargetShape")},
                {TEXT("checksourceagainstrespectivetarget"),TEXT("bCheckSourceAgainstRespectiveTarget")}}},
            {TEXT("PCGDensityRemapSettings"), {{TEXT("rangemin"),TEXT("InRangeMin")},{TEXT("rangemax"),TEXT("InRangeMax")},
                {TEXT("outrangemin"),TEXT("OutRangeMin")},{TEXT("outrangemax"),TEXT("OutRangeMax")},
                {TEXT("excludevaluesoutsideinputrange"),TEXT("bExcludeValuesOutsideInputRange")}}},
            {TEXT("PCGDensityFilterSettings"), {{TEXT("lowerbound"),TEXT("LowerBound")},{TEXT("upperbound"),TEXT("UpperBound")},
                {TEXT("invertfilter"),TEXT("bInvertFilter")},{TEXT("keepzerodensitypoints"),TEXT("bKeepZeroDensityPoints")}}},
            {TEXT("PCGAttributeNoiseSettings"), {{TEXT("mode"),TEXT("Mode")},{TEXT("noisemin"),TEXT("NoiseMin")},
                {TEXT("noisemax"),TEXT("NoiseMax")},{TEXT("clampresult"),TEXT("bClampResult")},
                {TEXT("invertsource"),TEXT("bInvertSource")},{TEXT("hascustomseedsource"),TEXT("bHasCustomSeedSource")}}},
            {TEXT("PCGAttributeFilteringSettings"), {{TEXT("operator"),TEXT("Operator")},
                {TEXT("useconstantthreshold"),TEXT("bUseConstantThreshold")},{TEXT("usespatialquery"),TEXT("bUseSpatialQuery")},
                {TEXT("warnondatamissingattribute"),TEXT("bWarnOnDataMissingAttribute")}}},
            {TEXT("PCGGetActorPropertySettings"), {{TEXT("propertyname"),TEXT("PropertyName")},
                {TEXT("alwaysrequeryactors"),TEXT("bAlwaysRequeryActors")}}},
            {TEXT("PCGRandomChoiceSettings"), {{TEXT("fixedmode"),TEXT("bFixedMode")},{TEXT("fixednumber"),TEXT("FixedNumber")},
                {TEXT("outputdiscardedentries"),TEXT("bOutputDiscardedEntries")},{TEXT("seed"),TEXT("Seed")}}}
        };
        return Fields;
    }

    struct FSettingContext
    {
        UPCGGraph* Owner = nullptr;
        UPCGNode* Member = nullptr;
        UPCGSettings* Settings = nullptr;
        FProperty* Property = nullptr;
    };

    bool Setting(const FString& GraphPath, const FString& NodeId, const FString& PropertyName, FSettingContext& C, FJson& Error)
    {
        C.Owner = Graph(GraphPath, Error); if (!C.Owner) return false;
        C.Member = Node(C.Owner, NodeId, Error); if (!C.Member) return false;
        C.Settings = C.Member->GetSettings();
        if (!ObservedObject(C.Settings) || C.Settings->GetOuter() != C.Member || !Bounded(PropertyName))
        { Error = Outcome(false, TEXT("pcg_setting_owner_invalid"), TEXT("Requires exact loaded settings owned by this graph's node")); return false; }
        const UClass* Class = C.Settings->GetClass();
        const auto* Fields = SettingFields().Find(Class->GetName());
        const FString* NativeName = Fields ? Fields->Find(Token(PropertyName)) : nullptr;
        if (!NativeName || Class->GetPathName() != TEXT("/Script/PCG.") + Class->GetName() ||
            !Class->HasAllClassFlags(CLASS_Native) || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
        { Error = Outcome(false, TEXT("pcg_setting_contract_unsupported"), TEXT("Requires an exact finite native field from the existing PCG family, PointExtents/Bounds, TransformPoints13, or SurfaceSampler8/Difference4 and Union2/InnerIntersection2/OuterIntersection3; deprecated/custom/inherited extras remain incomplete")); return false; }
        C.Property = FindFProperty<FProperty>(Class, FName(**NativeName));
        if (!C.Property || C.Property->ArrayDim != 1 || !C.Property->HasAnyPropertyFlags(CPF_BlueprintVisible | CPF_Edit) ||
            (C.Property->HasAnyPropertyFlags(CPF_EditConst | CPF_Deprecated) || (C.Property->HasAnyPropertyFlags(CPF_Transient) && !CubelessPCGUE58::IsDensityFilterEditorDebugProperty(Class, C.Property)&&!SamplingEditorDebugProperty(Class,C.Property))) ||
            EnumHasAnyFlags(PropertyAccessUtil::CanGetPropertyValue(C.Property), EPropertyAccessResultFlags::PermissionDenied) ||
            EnumHasAnyFlags(PropertyAccessUtil::CanSetPropertyValue(C.Property, PropertyAccessUtil::EditorReadOnlyFlags,
                PropertyAccessUtil::IsObjectTemplate(C.Settings)), EPropertyAccessResultFlags::PermissionDenied))
        { Error = Outcome(false, TEXT("pcg_setting_property_inaccessible"), TEXT("Pinned property is not a public readable/writable scalar or finite point array")); return false; }
        return true;
    }

    FJson VectorJson(const FVector& Value)
    {
        FJson Row = MakeShared<FJsonObject>(); Row->SetNumberField(TEXT("x"), Value.X);
        Row->SetNumberField(TEXT("y"), Value.Y); Row->SetNumberField(TEXT("z"), Value.Z); return Row;
    }
    TSharedPtr<FJsonValue> VectorValue(const FVector& Value)
    { return MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
        MakeShared<FJsonValueNumber>(Value.X), MakeShared<FJsonValueNumber>(Value.Y), MakeShared<FJsonValueNumber>(Value.Z)}); }
    FJson RotatorJson(const FRotator& Value)
    {FJson Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("pitch"),Value.Pitch);Row->SetNumberField(TEXT("yaw"),Value.Yaw);Row->SetNumberField(TEXT("roll"),Value.Roll);return Row;}
    TSharedPtr<FJsonValue> RotatorValue(const FRotator& Value)
    {return MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(Value.Pitch),MakeShared<FJsonValueNumber>(Value.Yaw),MakeShared<FJsonValueNumber>(Value.Roll)});}
    TSharedPtr<FJsonValue> StructValue(UScriptStruct* Struct, const void* Value)
    {
        FString Text; Struct->ExportText(Text, Value, Value, nullptr, PPF_None, nullptr);
        if (Text.Len() > 65536) return nullptr;
        FJson Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("struct_type"), Struct->GetName());
        Row->SetStringField(TEXT("export_text"), Text); return MakeShared<FJsonValueObject>(Row);
    }
    FString EnumRepr(const UEnum* Enum, int64 Value)
    {
        FString Type = Enum->GetName(); if (Type.StartsWith(TEXT("EPCG"))) Type.RightChopInline(1);
        const FString Name = Enum->GetNameStringByValue(Value); FString PythonName;
        for (int32 I = 0; I < Name.Len(); ++I)
        {
            if (I && FChar::IsUpper(Name[I]) && (FChar::IsLower(Name[I-1]) ||
                (I + 1 < Name.Len() && FChar::IsUpper(Name[I-1]) && FChar::IsLower(Name[I+1])))) PythonName += TEXT('_');
            PythonName += FChar::ToUpper(Name[I]);
        }
        return FString::Printf(TEXT("<%s.%s: %lld>"), *Type, *PythonName, Value);
    }
    UEnum* PropertyEnum(FProperty* Property, FNumericProperty*& Storage)
    {
        if (auto* Enum = CastField<FEnumProperty>(Property)) { Storage = Enum->GetUnderlyingProperty(); return Enum->GetEnum(); }
        Storage = CastField<FNumericProperty>(Property); return Storage ? Storage->GetIntPropertyEnum() : nullptr;
    }
    bool PointArray(FProperty* Property, FArrayProperty*& Array, FStructProperty*& Inner,bool Fresh=false)
    {
        Array = CastField<FArrayProperty>(Property); Inner = Array ? CastField<FStructProperty>(Array->Inner) : nullptr;
        return Inner && Property->GetName() == TEXT("PointsToCreate") && (Fresh ? (Inner->Struct.IsResolved() && Inner->Struct==FPCGPoint::StaticStruct() && FPCGPoint::StaticStruct()->GetStructureSize()==sizeof(FPCGPoint)) : (Inner->Struct->GetPathName() == TEXT("/Script/PCG.PCGPoint") && Inner->Struct->GetStructureSize() == sizeof(FPCGPoint)));
    }
    // Installed PCGProjectionParams.h: eight nondeprecated authored members.
    // This is the graph-spec dictionary contract, not native raw ImportText.
    const TMap<FString,FString>& ProjectionFields()
    {
        static const TMap<FString,FString> Fields={{TEXT("projectpositions"),TEXT("bProjectPositions")},{TEXT("projectrotations"),TEXT("bProjectRotations")},{TEXT("projectscales"),TEXT("bProjectScales")},{TEXT("colorblendmode"),TEXT("ColorBlendMode")},{TEXT("attributelist"),TEXT("AttributeList")},{TEXT("attributemode"),TEXT("AttributeMode")},{TEXT("attributemergeoperation"),TEXT("AttributeMergeOperation")},{TEXT("tagmergeoperation"),TEXT("TagMergeOperation")}};return Fields;
    }
    bool ProjectionProperty(FProperty* P)
    {
        auto* S=CastField<FStructProperty>(P);return S&&P->GetFName()==TEXT("ProjectionParams")&&P->GetOwnerStruct()&&P->GetOwnerStruct()->GetPathName().Equals(TEXT("/Script/PCG.PCGProjectionSettings"),ESearchCase::CaseSensitive)&&S->Struct.IsResolved()&&S->Struct==FPCGProjectionParams::StaticStruct()&&P->GetSize()==sizeof(FPCGProjectionParams)&&P->ArrayDim==1;
    }
    bool ProjectionString(const FString& S){if(S.Len()>65536)return false;for(TCHAR C:S)if(C==0)return false;return true;}
    FProperty* ProjectionField(const FString& Name)
    {
        const auto& Fields=ProjectionFields();const FString T=Token(Name);const FString* Native=Fields.Find(T);
        if(!Native)for(const auto& E:Fields)if(Name.Equals(E.Value,ESearchCase::IgnoreCase)){Native=&E.Value;break;}
        if(!Native)return nullptr;auto* Owner=FPCGProjectionParams::StaticStruct();FProperty* P=FindFProperty<FProperty>(Owner,FName(**Native));
        if(!P||P->GetOwnerStruct()!=Owner||P->ArrayDim!=1||P->HasSetterOrGetter()||!P->HasAllPropertyFlags(CPF_Edit|CPF_BlueprintVisible)||P->HasAnyPropertyFlags(CPF_EditConst|CPF_Deprecated|CPF_Transient))return nullptr;
        if(P->GetFName()==TEXT("AttributeList"))return P->GetClass()==FStrProperty::StaticClass()&&P->GetSize()==sizeof(FString)?P:nullptr;
        if(P->GetFName()==TEXT("bProjectPositions")||P->GetFName()==TEXT("bProjectRotations")||P->GetFName()==TEXT("bProjectScales")){auto* B=CastField<FBoolProperty>(P);return B&&B->IsNativeBool()&&P->GetSize()==sizeof(bool)?P:nullptr;}
        auto* E=CastField<FEnumProperty>(P);auto* U=E?E->GetUnderlyingProperty():nullptr;
        return E&&U&&U->GetOwner<FProperty>()==P&&U->GetClass()==FByteProperty::StaticClass()&&U->ArrayDim==1&&P->GetSize()==sizeof(uint8)&&U->GetSize()==sizeof(uint8)?P:nullptr;
    }
    bool SameProjection(const FPCGProjectionParams& A,const FPCGProjectionParams& B)
    {return A.bProjectPositions==B.bProjectPositions&&A.bProjectRotations==B.bProjectRotations&&A.bProjectScales==B.bProjectScales&&A.ColorBlendMode==B.ColorBlendMode&&A.AttributeList.Equals(B.AttributeList,ESearchCase::CaseSensitive)&&A.AttributeMode==B.AttributeMode&&A.AttributeMergeOperation==B.AttributeMergeOperation&&A.TagMergeOperation==B.TagMergeOperation;}
    bool ReadSetting(FProperty* Property, const void* Value, TSharedPtr<FJsonValue>& Out,TFunctionRef<bool()>* FreshCurrent=nullptr)
    {
        if(ProjectionProperty(Property))
        {
            if(FreshCurrent&&!(*FreshCurrent)())return false;const FPCGProjectionParams Copy=*static_cast<const FPCGProjectionParams*>(Value);FJson Row=MakeShared<FJsonObject>();
            for(const auto& Field:ProjectionFields()){FProperty* P=ProjectionField(Field.Value);if(!P)return false;TSharedPtr<FJsonValue> V;auto* Observer=CastField<FEnumProperty>(P)?FreshCurrent:nullptr;if(!ReadSetting(P,P->ContainerPtrToValuePtr<void>(&Copy),V,Observer))return false;if(Observer&&!(*Observer)())return false;if(ProjectionField(Field.Value)!=P)return false;Row->SetField(Field.Value,V);}
            if(FreshCurrent&&!(*FreshCurrent)())return false;if(!SameProjection(Copy,*static_cast<const FPCGProjectionParams*>(Value)))return false;Out=MakeShared<FJsonValueObject>(Row);return true;
        }
        if(auto* String=CastField<FStrProperty>(Property)){const FString Copy=String->GetPropertyValue(Value);if(!ProjectionString(Copy))return false;Out=MakeShared<FJsonValueString>(Copy);return true;}
        FNumericProperty* Storage = nullptr;
        UEnum* NativeEnum=PropertyEnum(Property,Storage);TStrongObjectPtr<UEnum> EnumHold(nullptr);if(FreshCurrent){if(!(*FreshCurrent)())return false;if(NativeEnum){if(!ObservedObject(NativeEnum))return false;EnumHold.Reset(NativeEnum);}}
        if (UEnum* Enum = NativeEnum)
        {
            const int64 Number = Storage->GetSignedIntPropertyValue(Value);
            if (!Enum->GetPathName().StartsWith(TEXT("/Script/PCG.")) || !Enum->IsValidEnumValue(Number)) return false;
            Out = MakeShared<FJsonValueString>(EnumRepr(Enum, Number)); return true;
        }
        if (auto* Bool = CastField<FBoolProperty>(Property)) { Out = MakeShared<FJsonValueBoolean>(Bool->GetPropertyValue(Value)); return true; }
        if (auto* Int = CastField<FIntProperty>(Property)) { Out = MakeShared<FJsonValueNumber>(Int->GetPropertyValue(Value)); return true; }
        if (Storage && Storage->IsFloatingPoint())
        {
            const double Number = Storage->GetFloatingPointPropertyValue(Value); if (!FMath::IsFinite(Number)) return false;
            Out = MakeShared<FJsonValueNumber>(Number); return true;
        }
        if (auto* Name = CastField<FNameProperty>(Property)) { Out = MakeShared<FJsonValueString>(Name->GetPropertyValue(Value).ToString()); return true; }
        if (auto* Struct = CastField<FStructProperty>(Property); Struct && (FreshCurrent ? (Struct->Struct.IsResolved() && Struct->Struct==TBaseStructure<FVector>::Get() && TBaseStructure<FVector>::Get()->GetStructureSize()==sizeof(FVector)) : (Struct->Struct->GetPathName() == TEXT("/Script/CoreUObject.Vector") && Struct->Struct->GetStructureSize() == sizeof(FVector))))
        { const FVector& Vector = *static_cast<const FVector*>(Value); if (Vector.ContainsNaN()) return false; Out = VectorValue(Vector); return true; }
        if(auto* Struct=CastField<FStructProperty>(Property);Struct&&Struct->Struct.IsResolved()&&Struct->Struct==TBaseStructure<FRotator>::Get()&&Struct->Struct->GetStructureSize()==sizeof(FRotator))
        {const auto& R=*static_cast<const FRotator*>(Value);if(!FMath::IsFinite(R.Pitch)||!FMath::IsFinite(R.Yaw)||!FMath::IsFinite(R.Roll))return false;Out=RotatorValue(R);return true;}
        FArrayProperty* Array = nullptr; FStructProperty* Inner = nullptr;
        if (!PointArray(Property, Array, Inner,FreshCurrent!=nullptr)) return false;
        FScriptArrayHelper Helper(Array, Value); if (Helper.Num() > 128) return false;
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (int32 I = 0; I < Helper.Num(); ++I)
        { auto Row = StructValue(Inner->Struct, Helper.GetRawPtr(I)); if (!Row.IsValid()) return false; Rows.Add(Row); }
        Out = MakeShared<FJsonValueArray>(Rows); return true;
    }
    bool Number(const TSharedPtr<FJsonValue>& Value, double& Out, bool bFloat = false)
    { return Value.IsValid() && Value->Type == EJson::Number && Value->TryGetNumber(Out) && FMath::IsFinite(Out)
        && (!bFloat || FMath::Abs(Out) <= TNumericLimits<float>::Max()); }
    bool Int32(const TSharedPtr<FJsonValue>& Value, int32& Out)
    { double N; if (!Number(Value, N) || N < MIN_int32 || N > MAX_int32 || N != FMath::TruncToDouble(N)) return false; Out = static_cast<int32>(N); return true; }
    bool Vector(const TSharedPtr<FJsonValue>& Value, FVector& Out)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values; double X, Y, Z;
        if (!Value.IsValid() || !Value->TryGetArray(Values) || Values->Num() != 3 || !Number((*Values)[0],X) ||
            !Number((*Values)[1],Y) || !Number((*Values)[2],Z)) return false;
        Out = FVector(X,Y,Z); return true;
    }
    // Legacy dict values are partial struct updates. Preflight validates only the supplied components;
    // the writer merges against the actual constructed settings, never assumed CDO/literal defaults.
    bool MathComponents(const TSharedPtr<FJsonValue>& Input,const TCHAR* A,const TCHAR* B,const TCHAR* C,double& X,double& Y,double& Z,bool& Partial)
    {
        if(!Input)return false;Partial=Input->Type==EJson::Object;
        if(!Partial){FVector V;if(!Vector(Input,V))return false;X=V.X;Y=V.Y;Z=V.Z;return true;}
        const auto O=Input->AsObject();if(!O||O->Values.Num()>3)return false;
        for(const auto& P:O->Values){double N=0;if(!Number(P.Value,N))return false;const FString K=Token(FString(P.Key));if(K==A)X=N;else if(K==B)Y=N;else if(K==C)Z=N;else return false;}return true;
    }
    bool PreparePoints(const TSharedPtr<FJsonValue>& Input, FStructProperty* Inner, TSharedPtr<FJsonValue>& Payload, TSharedPtr<FJsonValue>& Expected, TArray<FPCGPoint>& ExpectedPoints,TFunctionRef<bool()>* FreshCurrent=nullptr)
    {
        const TArray<TSharedPtr<FJsonValue>>* Rows;
        if (!Input.IsValid() || !Input->TryGetArray(Rows) || Rows->Num() > 128) return false;
        TArray<TSharedPtr<FJsonValue>> JsonRows, ExpectedRows;
        for (const auto& InputRow : *Rows)
        {
            if (!InputRow.IsValid() || InputRow->Type != EJson::Object) return false;
            FPCGPoint Point;
            for (const auto& Pair : InputRow->AsObject()->Values)
            {
                const FString Key = Token(FString(Pair.Key));
                if (Key == TEXT("transform"))
                {
                    if (Pair.Value->Type != EJson::Object) return false;
                    for (const auto& Part : Pair.Value->AsObject()->Values)
                    {
                        const FString Name = Token(FString(Part.Key)); FVector V;
                        if (Name == TEXT("translation")) { if (!Vector(Part.Value,V)) return false; Point.Transform.SetTranslation(V); }
                        else if (Name == TEXT("scale3d")) { if (!Vector(Part.Value,V)) return false; Point.Transform.SetScale3D(V); }
                        else if (Name == TEXT("rotation"))
                        {
                            const TArray<TSharedPtr<FJsonValue>>* Q; double X,Y,Z,W;
                            if (!Part.Value->TryGetArray(Q) || Q->Num()!=4 || !Number((*Q)[0],X) || !Number((*Q)[1],Y) || !Number((*Q)[2],Z) || !Number((*Q)[3],W)) return false;
                            const FQuat Rotation(X,Y,Z,W); if (!Rotation.IsNormalized()) return false; Point.Transform.SetRotation(Rotation);
                        }
                        else return false;
                    }
                }
                else if (Key == TEXT("boundsmin")) { if (!Vector(Pair.Value,Point.BoundsMin)) return false; }
                else if (Key == TEXT("boundsmax")) { if (!Vector(Pair.Value,Point.BoundsMax)) return false; }
                else if (Key == TEXT("density") || Key == TEXT("steepness"))
                { double N; if (!Number(Pair.Value,N,true)) return false; if (Key == TEXT("density")) Point.Density=N; else Point.Steepness=N; }
                else if (Key == TEXT("seed")) { if (!Int32(Pair.Value,Point.Seed)) return false; }
                else if (Key == TEXT("color"))
                {
                    const TArray<TSharedPtr<FJsonValue>>* C; double X,Y,Z,W;
                    if (!Pair.Value->TryGetArray(C) || C->Num()!=4 || !Number((*C)[0],X) || !Number((*C)[1],Y) || !Number((*C)[2],Z) || !Number((*C)[3],W)) return false;
                    Point.Color=FVector4(X,Y,Z,W);
                }
                else return false;
            }
            FJson Row = MakeShared<FJsonObject>(), Transform = MakeShared<FJsonObject>(), Rotation = MakeShared<FJsonObject>();
            const FRotator R = Point.Transform.Rotator(); Rotation->SetNumberField(TEXT("pitch"),R.Pitch);
            Rotation->SetNumberField(TEXT("yaw"),R.Yaw); Rotation->SetNumberField(TEXT("roll"),R.Roll);
            Transform->SetObjectField(TEXT("location"),VectorJson(Point.Transform.GetTranslation()));
            Transform->SetObjectField(TEXT("rotation"),Rotation); Transform->SetObjectField(TEXT("scale"),VectorJson(Point.Transform.GetScale3D()));
            Row->SetObjectField(TEXT("transform"),Transform); Row->SetObjectField(TEXT("boundsMin"),VectorJson(Point.BoundsMin));
            Row->SetObjectField(TEXT("boundsMax"),VectorJson(Point.BoundsMax)); Row->SetNumberField(TEXT("density"),Point.Density);
            Row->SetNumberField(TEXT("steepness"),Point.Steepness); Row->SetNumberField(TEXT("seed"),Point.Seed);
            FJson Color = MakeShared<FJsonObject>(); Color->SetNumberField(TEXT("x"),Point.Color.X); Color->SetNumberField(TEXT("y"),Point.Color.Y);
            Color->SetNumberField(TEXT("z"),Point.Color.Z); Color->SetNumberField(TEXT("w"),Point.Color.W); Row->SetObjectField(TEXT("color"),Color);
            JsonRows.Add(MakeShared<FJsonValueObject>(Row));
            auto NativeValue = StructValue(FreshCurrent?FPCGPoint::StaticStruct():Inner->Struct.Get(),&Point); if (FreshCurrent&&!(*FreshCurrent)())return false;if (!NativeValue.IsValid()) return false; ExpectedRows.Add(NativeValue);
            ExpectedPoints.Add(Point);
        }
        Payload=MakeShared<FJsonValueArray>(JsonRows); Expected=MakeShared<FJsonValueArray>(ExpectedRows); return true;
    }
    bool PrepareSetting(FProperty* Property, const TSharedPtr<FJsonValue>& Input, TSharedPtr<FJsonValue>& Payload, TSharedPtr<FJsonValue>& Expected, TArray<FPCGPoint>& ExpectedPoints,TFunctionRef<bool()>* FreshCurrent=nullptr,const void* ExistingValue=nullptr)
    {
        if(ProjectionProperty(Property))
        {
            if(!Input||Input->Type!=EJson::Object||Input->AsObject()->Values.Num()>8)return false;if(FreshCurrent&&!(*FreshCurrent)())return false;
            // Own the FString-bearing value before any reflected Enum callback.
            const FPCGProjectionParams Copy=ExistingValue?*static_cast<const FPCGProjectionParams*>(ExistingValue):FPCGProjectionParams();
            FJson Request=MakeShared<FJsonObject>(),Whole=MakeShared<FJsonObject>();
            if(ExistingValue){TSharedPtr<FJsonValue> Prior;if(!ReadSetting(Property,&Copy,Prior,FreshCurrent))return false;Whole=MakeShared<FJsonObject>(*Prior->AsObject());}
            TSet<FName> Seen;
            for(const auto& Pair:Input->AsObject()->Values)
            {
                FProperty* Child=ProjectionField(FString(Pair.Key));if(!Child||Seen.Contains(Child->GetFName()))return false;Seen.Add(Child->GetFName());
                TSharedPtr<FJsonValue> Native,Wanted;TArray<FPCGPoint> Ignored;
                auto* Observer=CastField<FEnumProperty>(Child)?FreshCurrent:nullptr;
                if(!PrepareSetting(Child,Pair.Value,Native,Wanted,Ignored,Observer,ExistingValue?Child->ContainerPtrToValuePtr<void>(&Copy):nullptr))return false;
                if(Observer&&!(*Observer)())return false;if(ProjectionField(FString(Pair.Key))!=Child)return false;Request->SetField(Child->GetName(),Native);Whole->SetField(Child->GetName(),Wanted);
            }
            if(FreshCurrent&&!(*FreshCurrent)())return false;if(ExistingValue&&!SameProjection(Copy,*static_cast<const FPCGProjectionParams*>(ExistingValue)))return false;
            Payload=MakeShared<FJsonValueObject>(Request);Expected=MakeShared<FJsonValueObject>(Whole);return true;
        }
        if(CastField<FStrProperty>(Property)){FString V;if(!Input||Input->Type!=EJson::String||!Input->TryGetString(V)||!ProjectionString(V))return false;Payload=Expected=MakeShared<FJsonValueString>(V);return true;}
        FNumericProperty* Storage = nullptr;
        UEnum* NativeEnum=PropertyEnum(Property,Storage);TStrongObjectPtr<UEnum> EnumHold(nullptr);if(FreshCurrent){if(!(*FreshCurrent)())return false;if(NativeEnum){if(!ObservedObject(NativeEnum))return false;EnumHold.Reset(NativeEnum);}}
        if (UEnum* Enum = NativeEnum)
        {
            FString Name; int64 N;
            if (Input->Type!=EJson::String || !Input->TryGetString(Name) || !EnumValue(Enum,Name,N)) return false;
            Payload=MakeShared<FJsonValueString>(Enum->GetNameStringByValue(N)); Expected=MakeShared<FJsonValueString>(EnumRepr(Enum,N)); return true;
        }
        if (CastField<FBoolProperty>(Property)) { bool B; if (Input->Type!=EJson::Boolean || !Input->TryGetBool(B)) return false; Payload=Expected=MakeShared<FJsonValueBoolean>(B); return true; }
        if (CastField<FIntProperty>(Property)) { int32 N; if (!Int32(Input,N)) return false; Payload=Expected=MakeShared<FJsonValueNumber>(N); return true; }
        if (Storage && Storage->IsFloatingPoint())
        { double N; if (!Number(Input,N,CastField<FFloatProperty>(Property)!=nullptr)) return false; if (CastField<FFloatProperty>(Property)) N=static_cast<float>(N); Payload=Expected=MakeShared<FJsonValueNumber>(N); return true; }
        if (CastField<FNameProperty>(Property))
        { FString N; if (Input->Type!=EJson::String || !Input->TryGetString(N) || !Bounded(N,true)) return false;
            Payload=MakeShared<FJsonValueString>(N); Expected=MakeShared<FJsonValueString>(FName(*N).ToString()); return true; }
        if (auto* Struct=CastField<FStructProperty>(Property); Struct && (FreshCurrent ? (Struct->Struct.IsResolved() && Struct->Struct==TBaseStructure<FVector>::Get() && TBaseStructure<FVector>::Get()->GetStructureSize()==sizeof(FVector)) : (Struct->Struct->GetPathName()==TEXT("/Script/CoreUObject.Vector") && Struct->Struct->GetStructureSize()==sizeof(FVector))))
        {if(Input&&Input->Type==EJson::Object&&Property->GetOwnerStruct()->GetPathName()!=TEXT("/Script/PCG.PCGTransformPointsSettings")&&Property->GetOwnerStruct()->GetPathName()!=TEXT("/Script/PCG.PCGSurfaceSamplerSettings"))return false;FVector V=ExistingValue?*static_cast<const FVector*>(ExistingValue):FVector::ZeroVector;bool Partial=false;if(!MathComponents(Input,TEXT("x"),TEXT("y"),TEXT("z"),V.X,V.Y,V.Z,Partial))return false;
            if(Partial&&!ExistingValue){Payload=Expected=Input;}else{Payload=MakeShared<FJsonValueObject>(VectorJson(V));Expected=VectorValue(V);}return true;}
        if(auto* Struct=CastField<FStructProperty>(Property);Struct&&Struct->Struct.IsResolved()&&Struct->Struct==TBaseStructure<FRotator>::Get()&&Struct->Struct->GetStructureSize()==sizeof(FRotator))
        {FRotator V=ExistingValue?*static_cast<const FRotator*>(ExistingValue):FRotator::ZeroRotator;bool Partial=false;if(!MathComponents(Input,TEXT("pitch"),TEXT("yaw"),TEXT("roll"),V.Pitch,V.Yaw,V.Roll,Partial))return false;
            if(Partial&&!ExistingValue){Payload=Expected=Input;}else{Payload=MakeShared<FJsonValueObject>(RotatorJson(V));Expected=RotatorValue(V);}return true;}
        FArrayProperty* Array=nullptr; FStructProperty* Inner=nullptr;
        return PointArray(Property,Array,Inner,FreshCurrent!=nullptr) && PreparePoints(Input,Inner,Payload,Expected,ExpectedPoints,FreshCurrent);
    }
    FString JsonValueText(const TSharedPtr<FJsonValue>& Value)
    { FString Out; FJsonSerializer::Serialize(TArray<TSharedPtr<FJsonValue>>{Value},TJsonWriterFactory<>::Create(&Out)); return Out; }

    FJson SettingIdentity(const FSettingContext& C)
    {
        FJson Value=Outcome(true); Value->SetStringField(TEXT("graph"),C.Owner->GetPathName());
        Value->SetStringField(TEXT("node_id"),C.Member->GetPathName()); Value->SetStringField(TEXT("settings_path"),C.Settings->GetPathName());
        Value->SetStringField(TEXT("settings_class"),C.Settings->GetClass()->GetPathName());
        Value->SetStringField(TEXT("native_property_name"),C.Property->GetName()); Value->SetStringField(TEXT("native_property_path"),C.Property->GetPathName());
        Value->SetStringField(TEXT("native_property_flags_string"),FString::Printf(TEXT("%llu"),static_cast<uint64>(C.Property->GetPropertyFlags())));
        Value->SetBoolField(TEXT("python_nested_struct_traversal"),false); return Value;
    }

    bool CaptureSettingFields(const FSettingContext& C, FJson& Values)
    {
        Values=MakeShared<FJsonObject>();
        const auto* Fields=SettingFields().Find(C.Settings->GetClass()->GetName()); if (!Fields) return false;
        for (const auto& Pair : *Fields)
        {
            FProperty* Property=FindFProperty<FProperty>(C.Settings->GetClass(),FName(*Pair.Value));
            if (!Property || Property->ArrayDim!=1 || EnumHasAnyFlags(PropertyAccessUtil::CanGetPropertyValue(Property),EPropertyAccessResultFlags::PermissionDenied)) return false;
            TSharedPtr<FJsonValue> Value; if (!ReadSetting(Property,Property->ContainerPtrToValuePtr<void>(C.Settings),Value)) return false;
            Values->SetField(Pair.Key,Value);
        }
        return true;
    }
    bool UnrelatedSettingFieldsPreserved(const FJson& Before, const FJson& After, const FString& Except)
    {
        if (Before->Values.Num()!=After->Values.Num()) return false;
        for (const auto& Pair : Before->Values)
        {
            if (FString(Pair.Key)==Except) continue;
            const auto Value=After->TryGetField(Pair.Key);
            if (!Value.IsValid() || JsonValueText(Pair.Value)!=JsonValueText(Value)) return false;
        }
        return true;
    }
    bool GraphTopologyPreserved(const FJson& Before, const FJson& After)
    {
        if (Before->GetStringField(TEXT("graph"))!=After->GetStringField(TEXT("graph")) ||
            Before->GetIntegerField(TEXT("authored_node_count"))!=After->GetIntegerField(TEXT("authored_node_count")) ||
            Before->GetIntegerField(TEXT("node_count_including_factory"))!=After->GetIntegerField(TEXT("node_count_including_factory"))) return false;
        const auto& A=Before->GetArrayField(TEXT("nodes")); const auto& B=After->GetArrayField(TEXT("nodes"));
        if (A.Num()!=B.Num()) return false;
        for (int32 I=0; I<A.Num(); ++I)
        {
            const auto Left=A[I]->AsObject(), Right=B[I]->AsObject();
            for (const TCHAR* Key : {TEXT("node_path"),TEXT("node_name"),TEXT("title"),TEXT("settings_path"),TEXT("settings_class")})
                if (Left->GetStringField(Key)!=Right->GetStringField(Key)) return false;
            if (JsonValueText(Left->TryGetField(TEXT("position")))!=JsonValueText(Right->TryGetField(TEXT("position")))) return false;
        }
        return JsonValueText(Before->TryGetField(TEXT("edges")))==JsonValueText(After->TryGetField(TEXT("edges")));
    }
}

FString CubelessPCGUE58::InspectSettingsClass(const FString& ClassPath)
{
    if (!IsInGameThread() || !GEditor || GEditor->PlayWorld)
        return Encode(Outcome(false, TEXT("pcg_editor_not_idle"), TEXT("Requires the editor game thread without PIE/SIE")));
    if (!Bounded(ClassPath) || !ClassPath.StartsWith(TEXT("/Script/PCG.")))
        return Encode(Outcome(false, TEXT("pcg_settings_class_unsupported"), TEXT("Use an exact already-loaded native runtime /Script/PCG settings class")));

    UPackage* Package = FindObject<UPackage>(nullptr, TEXT("/Script/PCG"));
    UClass* Base = FindObject<UClass>(nullptr, TEXT("/Script/PCG.PCGSettings"));
    UClass* Class = FindObject<UClass>(nullptr, *ClassPath);
    if (!IsValid(Package) || Package->IsDirty() || !Package->HasAnyPackageFlags(PKG_CompiledIn)
        || Package->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly)
        || !IsValid(Base) || Base->GetOuter() != Package || Base != UPCGSettings::StaticClass()
        || !IsValid(Class) || Class->GetPathName() != ClassPath || Class->GetOuter() != Package
        || !Class->IsChildOf(Base) || !Class->HasAllClassFlags(CLASS_Native)
        || Class->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
        return Encode(Outcome(false, TEXT("pcg_settings_class_unsupported"), TEXT("Exact loaded native PCGSettings ancestry/runtime package identity was not verified; no load or construction executed")));

    const EClassFlags Flags = Class->GetClassFlags();
    const bool bAbstract = Class->HasAnyClassFlags(CLASS_Abstract);
    const bool bDeprecated = Class->HasAnyClassFlags(CLASS_Deprecated);
    const bool bNewerVersionExists = Class->HasAnyClassFlags(CLASS_NewerVersionExists);
    const bool bAdmitted = !bAbstract && !bDeprecated && !bNewerVersionExists;
    FJson Value = Outcome(bAdmitted,
        bAdmitted ? FString() : FString(TEXT("pcg_settings_class_unconstructible")),
        bAdmitted ? FString() : FString(TEXT("Abstract/deprecated/replaced PCGSettings classes are rejected before transient graph/node construction")));
    Value->SetStringField(TEXT("class_path"), Class->GetPathName());
    Value->SetStringField(TEXT("class_flags_string"), FString::Printf(TEXT("%llu"), static_cast<uint64>(Flags)));
    Value->SetStringField(TEXT("base_class_path"), Base->GetPathName());
    Value->SetStringField(TEXT("package_path"), Package->GetPathName());
    Value->SetBoolField(TEXT("native_runtime_settings_verified"), true);
    Value->SetBoolField(TEXT("abstract"), bAbstract);
    Value->SetBoolField(TEXT("deprecated"), bDeprecated);
    Value->SetBoolField(TEXT("newer_version_exists"), bNewerVersionExists);
    Value->SetBoolField(TEXT("constructible"), bAdmitted);
    Value->SetBoolField(TEXT("package_dirty_before"), false);
    Value->SetBoolField(TEXT("package_dirty_after"), Package->IsDirty());
    Value->SetBoolField(TEXT("state_preserved"), Flags == Class->GetClassFlags() && !Package->IsDirty()
        && FindObject<UClass>(nullptr, *ClassPath) == Class && Class->GetOuter() == Package);
    Value->SetBoolField(TEXT("read_only"), true);
    Value->SetBoolField(TEXT("class_default_object_observed"), false);
    Value->SetBoolField(TEXT("object_construction_attempted"), false);
    return Encode(Value);
}

FString CubelessPCGUE58::InspectGraphSnapshot(const FString& GraphPath)
{
    FJson Error;
    UPCGGraph* Owner = Graph(GraphPath, Error);
    if (!Owner) return Encode(Error);
    if (!ObservedObject(Owner))
        return Encode(Outcome(false, TEXT("pcg_graph_not_loaded"), TEXT("Snapshot requires an ordinary fully loaded graph; no load or repair executed")));
    TStrongObjectPtr<UPCGGraph> Keep(Owner);
    const bool bDirty = Owner->GetPackage()->IsDirty();
    FJson Before, After;
    if (!CaptureGraphSnapshot(Owner, Before, Error) || !CaptureGraphSnapshot(Owner, After, Error)) return Encode(Error);
    const bool bPreserved = Encode(Before) == Encode(After) && bDirty == Owner->GetPackage()->IsDirty()
        && FindObject<UPCGGraph>(nullptr, *GraphPath) == Owner;
    FJson Value = Outcome(bPreserved,
        bPreserved ? FString() : FString(TEXT("pcg_snapshot_not_preserved")),
        bPreserved ? FString() : FString(TEXT("Read-only graph identities/topology or package state changed across observation")));
    Value->SetObjectField(TEXT("snapshot"), After);
    Value->SetBoolField(TEXT("package_dirty_before"), bDirty);
    Value->SetBoolField(TEXT("package_dirty_after"), Owner->GetPackage()->IsDirty());
    Value->SetBoolField(TEXT("observation_complete"), true);
    Value->SetBoolField(TEXT("state_preserved"), bPreserved);
    Value->SetBoolField(TEXT("read_only"), true);
    Value->SetBoolField(TEXT("python_nested_pin_struct_traversal"), false);
    Value->SetBoolField(TEXT("gc_root_cause_verified"), false);
    Value->SetBoolField(TEXT("object_construction_attempted"), false);
    Value->SetBoolField(TEXT("compile_attempted"), false);
    return Encode(Value);
}

FString CubelessPCGUE58::InspectSetting(const FString& GraphPath, const FString& NodeId, const FString& PropertyName)
{
    FSettingContext C; FJson Error; if (!Setting(GraphPath,NodeId,PropertyName,C,Error)) return Encode(Error);
    TStrongObjectPtr<UPCGGraph> Keep(C.Owner); const bool bDirty=C.Owner->GetPackage()->IsDirty();
    FJson Before,After; const bool bObserved=CaptureSettingFields(C,Before)&&CaptureSettingFields(C,After);
    const bool bPreserved=bObserved && Encode(Before)==Encode(After) && bDirty==C.Owner->GetPackage()->IsDirty();
    FJson Value=SettingIdentity(C); Value->SetBoolField(TEXT("success"),bPreserved);
    if (bObserved) { Value->SetField(TEXT("actual"),After->TryGetField(Token(PropertyName))); Value->SetObjectField(TEXT("settings_contract"),After); }
    else { Value->SetStringField(TEXT("error_code"),TEXT("pcg_setting_observation_incomplete")); Value->SetStringField(TEXT("error"),TEXT("Finite typed field observation is incomplete; no write dispatched")); }
    if (bObserved&&!bPreserved) { Value->SetStringField(TEXT("error_code"),TEXT("pcg_setting_not_preserved")); Value->SetStringField(TEXT("error"),TEXT("Read-only settings or package state changed")); }
    Value->SetBoolField(TEXT("state_preserved"),bPreserved); Value->SetBoolField(TEXT("observation_complete"),bObserved);
    Value->SetBoolField(TEXT("package_dirty_before"),bDirty); Value->SetBoolField(TEXT("package_dirty_after"),C.Owner->GetPackage()->IsDirty());
    Value->SetBoolField(TEXT("read_only"),true); return Encode(Value);
}

FString CubelessPCGUE58::ApplySetting(const FString& GraphPath, const FString& NodeId, const FString& PropertyName, const FString& ValueJson)
{
    FSettingContext C; FJson Error; if (!Setting(GraphPath,NodeId,PropertyName,C,Error)) return Encode(Error);
    if(C.Settings->GetClass()->GetPathName().Equals(TEXT("/Script/PCG.PCGProjectionSettings"),ESearchCase::CaseSensitive))return Encode(Outcome(false,TEXT("pcg_projection_native_raw_pending"),TEXT("Projection nested dictionaries are enrolled in graph-spec only; the original native raw ImportText/Structural contract remains incomplete.")));
    // The domain admits persistent target creation/ownership before this leaf;
    // it stays fail-closed until the new native route is actually verified.
    TStrongObjectPtr<UPCGGraph> Keep(C.Owner); const bool bDirty=C.Owner->GetPackage()->IsDirty();
    TArray<TSharedPtr<FJsonValue>> Parsed;
    if (ValueJson.Len()>1048576 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+ValueJson+TEXT("]")),Parsed) || Parsed.Num()!=1 || !Parsed[0].IsValid())
        return Encode(Outcome(false,TEXT("pcg_setting_value_json_invalid"),TEXT("Expected one bounded JSON value; no dispatch")));
    TSharedPtr<FJsonValue> Payload,Expected;
    TArray<FPCGPoint> ExpectedPoints;
    if (!PrepareSetting(C.Property,Parsed[0],Payload,Expected,ExpectedPoints,nullptr,C.Property->ContainerPtrToValuePtr<void>(C.Settings)))
        return Encode(Outcome(false,TEXT("pcg_setting_type_invalid"),TEXT("Pinned native property requires an exact finite primitive/enum/vector or bounded public PCGPoint payload; no conversion/write fallback")));
    FJson Before,GraphBefore;
    if (!CaptureSettingFields(C,Before) || !CaptureGraphSnapshot(C.Owner,GraphBefore,Error))
        return Encode(Error.IsValid()?Error:Outcome(false,TEXT("pcg_setting_before_unobservable"),TEXT("Finite setting values are not observable; no dispatch")));
    FJson Value=SettingIdentity(C); Value->SetField(TEXT("requested"),Parsed[0]); Value->SetField(TEXT("expected"),Expected);
    Value->SetField(TEXT("before"),Before->TryGetField(Token(PropertyName))); Value->SetObjectField(TEXT("settings_contract_before"),Before);
    Value->SetObjectField(TEXT("graph_before"),GraphBefore); Value->SetBoolField(TEXT("package_dirty_before"),bDirty);
    Value->SetStringField(TEXT("api_dependency"),TEXT("public EngineAPI observation + ToolsetAPI UToolsetLibrary::SetObjectProperties; no ServerRPC in native/domain leaf"));
    Value->SetBoolField(TEXT("native_toolset_converter_module_load_possible"),true);
    Value->SetStringField(TEXT("mutation_scope"),TEXT("finite family setting values, graph identities/topology and package dirty flag; callback caches/editor metadata are not independently observed"));
    Value->SetBoolField(TEXT("compile_attempted"),false); Value->SetBoolField(TEXT("automatic_retry"),false);
    const bool bEqual=JsonValueText(Before->TryGetField(Token(PropertyName)))==JsonValueText(Expected);
    bool bReturned=true;
    TArray<FName> SetNames;
    if (!bEqual)
    {
        FJson Request=MakeShared<FJsonObject>(); Request->SetField(C.Property->GetName(),Payload);
        Value->SetBoolField(TEXT("mutation_attempted"),true);
        // One fully prevalidated public field per call. False may still mean
        // partial nested writes; successful names are never that inventory.
        bReturned=UToolsetLibrary::SetObjectProperties(C.Settings,Encode(Request),SetNames,EBypassContainerCheck::No);
    }
    Value->SetBoolField(TEXT("write_returned"),bReturned); Value->SetBoolField(TEXT("property_write_skipped_equal_value"),bEqual);
    TArray<TSharedPtr<FJsonValue>> Names; for (FName Name : SetNames) Names.Add(MakeShared<FJsonValueString>(Name.ToString()));
    Value->SetArrayField(TEXT("toolset_successful_property_names"),Names);
    Value->SetStringField(TEXT("successful_names_scope"),TEXT("Supplemental; false can omit partially changed nested fields. Independent typed readback determines observed state."));
    FJson After,GraphAfter; const bool bObserved=CaptureSettingFields(C,After)&&CaptureGraphSnapshot(C.Owner,GraphAfter,Error);
    Value->SetBoolField(TEXT("observation_complete"),bObserved); Value->SetBoolField(TEXT("outcome_unknown"),!bObserved);
    Value->SetBoolField(TEXT("package_dirty_after"),C.Owner->GetPackage()->IsDirty());
    if (!bObserved)
    {
        Value->SetField(TEXT("mutation_performed"),MakeShared<FJsonValueNull>());
        Value->SetBoolField(TEXT("success"),false); Value->SetStringField(TEXT("error_code"),TEXT("pcg_setting_after_unobservable"));
        Value->SetStringField(TEXT("error"),TEXT("Write completed but after-state is incomplete; retain prior changes and do not retry"));
        return Encode(Value);
    }
    const auto Actual=After->TryGetField(Token(PropertyName));
    FArrayProperty* PointProperty=nullptr; FStructProperty* PointInner=nullptr;
    // Keep the raw export-text evidence, but compare this exact admitted array
    // as native values. The public Transform converter round-trips Euler angles
    // to quaternion components; +0 and -0 have identical typed numeric values.
    const bool bVerified=PointArray(C.Property,PointProperty,PointInner)
        ? PointProperty->Identical(C.Property->ContainerPtrToValuePtr<void>(C.Settings),&ExpectedPoints,PPF_None)
        : JsonValueText(Actual)==JsonValueText(Expected);
    const bool bOtherFields=UnrelatedSettingFieldsPreserved(Before,After,Token(PropertyName));
    const bool bTopology=GraphTopologyPreserved(GraphBefore,GraphAfter);
    Value->SetField(TEXT("actual"),Actual); Value->SetObjectField(TEXT("settings_contract_after"),After); Value->SetObjectField(TEXT("graph_after"),GraphAfter);
    Value->SetBoolField(TEXT("requested_value_verified"),bVerified); Value->SetBoolField(TEXT("unrelated_contract_fields_preserved"),bOtherFields);
    Value->SetBoolField(TEXT("graph_topology_preserved"),bTopology);
    Value->SetStringField(TEXT("preservation_scope"),TEXT("Exact nine-class family fields and existing node identities/positions/physical edges; native caches and other settings fields are not an all-property preservation claim."));
    Value->SetBoolField(TEXT("mutation_performed"),Encode(Before)!=Encode(After)||Encode(GraphBefore)!=Encode(GraphAfter)||bDirty!=C.Owner->GetPackage()->IsDirty());
    Value->SetBoolField(TEXT("success"),bReturned&&bVerified&&bOtherFields&&bTopology);
    if (!Value->GetBoolField(TEXT("success")))
    { Value->SetStringField(TEXT("error_code"),TEXT("pcg_setting_postcondition_failed")); Value->SetStringField(TEXT("error"),TEXT("Public write returned false or typed value/finite-field/topology readback differs; actual partial state retained, no rollback/retry")); }
    return Encode(Value);
}

namespace
{
    bool FreshInterfaceMatches(UPCGNode* Member,UPCGSettings* Settings)
    {
        FProperty* Found=nullptr;for(auto Super=Member->GetClass()->GetSuperStructIterator();Super&&!Found;++Super)for(TFieldIterator<FProperty> It(*Super,EFieldIterationFlags::None);It;++It)if(It->GetFName()==TEXT("SettingsInterface")){Found=*It;break;}
        auto* P=CastField<FObjectProperty>(Found);if(!P)return false;auto* H=P->GetObjectPtrPropertyValuePtr(P->ContainerPtrToValuePtr<void>(Member));return H->IsResolved()&&*H==Settings;
    }
    // Reuses the existing finite9 field names and typed conversion. No loader,
    // generic reflection fallback, selector or generation capability is added.
    FProperty* FreshProperty(UClass* Class, const FString& Name,FString* CanonicalAlias=nullptr)
    {
        const auto* Names=Class?SettingFields().Find(Class->GetName()):nullptr;
        FString Alias=Token(Name);const auto* NativeName=Names?Names->Find(Alias):nullptr;
        // The finite table is still authoritative. Native FName spellings map
        // back to its existing alias; no generic reflection fallback is used.
        if(!NativeName&&Names)for(const auto& Pair:*Names)if(Name.Equals(Pair.Value,ESearchCase::IgnoreCase)){Alias=Pair.Key;NativeName=&Pair.Value;break;}
        if(CanonicalAlias)*CanonicalAlias=Alias;
        if(!NativeName||Class->GetPathName()!=TEXT("/Script/PCG.")+Class->GetName()||
            !Class->HasAllClassFlags(CLASS_Native)||Class->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists))return nullptr;
        FProperty* P=nullptr;for(auto Super=Class->GetSuperStructIterator();Super&&!P;++Super)for(TFieldIterator<FProperty> It(*Super,EFieldIterationFlags::None);It;++It)if(It->GetFName()==FName(**NativeName)){P=*It;break;}
        return P&&P->ArrayDim==1&&P->HasAnyPropertyFlags(CPF_Edit|CPF_BlueprintVisible)&&
            !P->HasAnyPropertyFlags(CPF_EditConst|CPF_Deprecated)&&(!P->HasAnyPropertyFlags(CPF_Transient)||CubelessPCGUE58::IsDensityFilterEditorDebugProperty(Class,P)||SamplingEditorDebugProperty(Class,P))&&!P->HasSetterOrGetter()?P:nullptr;
    }
    bool FreshFields(const FSettingContext& C,FJson& Values,TFunctionRef<bool()> Current)
    {
        Values=MakeShared<FJsonObject>();const auto* Names=SettingFields().Find(C.Settings->GetClass()->GetName());if(!Names)return false;
        for(const auto& Pair:*Names)
        {
            if(!Current())return false;FProperty* P=FreshProperty(C.Settings->GetClass(),Pair.Key);if(!P)return false;
            TSharedPtr<FJsonValue> V;FArrayProperty* Array=nullptr;FStructProperty* Inner=nullptr;
            if(PointArray(P,Array,Inner,true))
            {
                FScriptArrayHelper H(Array,P->ContainerPtrToValuePtr<void>(C.Settings));if(H.Num()>128)return false;
                TArray<FPCGPoint> Copies;for(int32 I=0;I<H.Num();++I)Copies.Add(*reinterpret_cast<const FPCGPoint*>(H.GetRawPtr(I)));
                TStrongObjectPtr<UScriptStruct> Struct(FPCGPoint::StaticStruct());TArray<TSharedPtr<FJsonValue>> Rows;
                for(const FPCGPoint& Copy:Copies){auto Row=StructValue(Struct.Get(),&Copy);if(!Current()||!Row)return false;Rows.Add(Row);}
                V=MakeShared<FJsonValueArray>(Rows);
            }
            else if(!ReadSetting(P,P->ContainerPtrToValuePtr<void>(C.Settings),V,&Current))return false;
            if(!Current()||FreshProperty(C.Settings->GetClass(),Pair.Key)!=P)return false;
            Values->SetField(Pair.Key,V);
        }
        return Current();
    }
}

bool CubelessPCGUE58::PrepareFreshSettings(UClass* Class,const FJson& Input,FJson& Plans,TFunctionRef<bool()> Current,bool bPreparePointImport)
{
    Plans=MakeShared<FJsonObject>();if(!Current()||!Input||!Class||!SettingFields().Contains(Class->GetName()))return false;
    TSet<FName> Seen;
    for(const auto& Pair:Input->Values)
    {
        Plans->SetStringField(TEXT("preflight_input_field"),FString(Pair.Key));
        if(!Current())return false;FProperty* P=FreshProperty(Class,FString(Pair.Key));if(!P||Seen.Contains(P->GetFName()))return false;Seen.Add(P->GetFName());
        TSharedPtr<FJsonValue> Payload,Expected;TArray<FPCGPoint> Points;
        const bool Accepted=PrepareSetting(P,Pair.Value,Payload,Expected,Points,&Current);
        if(!Current()||FreshProperty(Class,FString(Pair.Key))!=P||!Accepted)return false;
        FJson Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("native_property"),P->GetName());
        if(bPreparePointImport&&Class->GetPathName().Equals(TEXT("/Script/PCG.PCGCreatePointsSettings"),ESearchCase::CaseSensitive)&&P->GetFName()==TEXT("PointsToCreate"))
        {
            FArrayProperty* A=nullptr;FStructProperty* I=nullptr;if(!PointArray(P,A,I,true)||I->GetOwner<FProperty>()!=P||I->ArrayDim!=1||I->GetSize()!=sizeof(FPCGPoint))return false;
            const auto InnerHandle=I->Struct;const uint64 InnerFlags=I->GetPropertyFlags();const int32 InnerOffset=I->GetOffset_ForInternal();TStrongObjectPtr<UScriptStruct> Hold(FPCGPoint::StaticStruct());if(!Current()||!Hold.IsValid()||InnerHandle!=Hold.Get())return false;
            struct FPointField{FProperty* P;FFieldClass* Kind;UStruct* Owner;uint64 Flags;int32 Offset,Size,Dim;FStructProperty* Child;TObjectPtr<UScriptStruct> ChildType;};
            struct FPointType{UScriptStruct* S;UPackage* Package;FString Path,PackagePath;uint32 PackageFlags;EObjectFlags ObjectFlags;EStructFlags Flags;int32 Size,Alignment;TArray<FProperty*> Inventory;TArray<FPointField> Fields;};
            TArray<FPointType> Types;TArray<TStrongObjectPtr<UScriptStruct>> TypeHolds;TArray<TStrongObjectPtr<UPackage>> PackageHolds;TArray<UScriptStruct*> Pending{Hold.Get()};TSet<UScriptStruct*> PointTypesSeen;
            while(!Pending.IsEmpty())
            {
                UScriptStruct* T=Pending.Pop();if(PointTypesSeen.Contains(T))continue;PointTypesSeen.Add(T);if(!ObservedObject(T))return false;TypeHolds.Emplace(T);const FString Path=T->GetPathName();int32 Size=0;
                if(Path.Equals(TEXT("/Script/PCG.PCGPoint"),ESearchCase::CaseSensitive))Size=sizeof(FPCGPoint);else if(Path.Equals(TEXT("/Script/CoreUObject.Transform"),ESearchCase::CaseSensitive))Size=sizeof(FTransform);else if(Path.Equals(TEXT("/Script/CoreUObject.Vector"),ESearchCase::CaseSensitive))Size=sizeof(FVector);else if(Path.Equals(TEXT("/Script/CoreUObject.Vector4"),ESearchCase::CaseSensitive))Size=sizeof(FVector4);else if(Path.Equals(TEXT("/Script/CoreUObject.Quat"),ESearchCase::CaseSensitive))Size=sizeof(FQuat);
                if(!Size||!(T->StructFlags&STRUCT_Native)||T->GetSuperStruct()!=nullptr||T->GetStructureSize()!=Size||!T->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||T->GetPackage()->IsDirty())return false;
                FPointType L;L.S=T;L.Path=Path;L.Package=T->GetPackage();L.PackagePath=L.Package->GetPathName();PackageHolds.Emplace(L.Package);L.PackageFlags=L.Package->GetPackageFlags();L.ObjectFlags=T->GetFlags();L.Flags=T->StructFlags;L.Size=T->GetStructureSize();L.Alignment=T->GetMinAlignment();
                for(TFieldIterator<FProperty> It(T);It;++It){FProperty* Q=*It;if(Q->HasSetterOrGetter())return false;auto* Child=CastField<FStructProperty>(Q);if(Child&&!Child->Struct.IsResolved())return false;L.Inventory.Add(Q);L.Fields.Add({Q,Q->GetClass(),Q->GetOwnerStruct(),Q->GetPropertyFlags(),Q->GetOffset_ForInternal(),Q->GetSize(),Q->ArrayDim,Child,Child?Child->Struct:TObjectPtr<UScriptStruct>()});if(Child)Pending.Add(Child->Struct.Get());}Types.Add(MoveTemp(L));
            }
            auto SourceCurrent=[&]()
            {
                if(!Current()||FreshProperty(Class,FString(Pair.Key))!=P||A->Inner!=I||I->GetOwner<FProperty>()!=P||I->ArrayDim!=1||I->GetSize()!=sizeof(FPCGPoint)||I->GetPropertyFlags()!=InnerFlags||I->GetOffset_ForInternal()!=InnerOffset||!I->Struct.IsResolved()||I->Struct!=InnerHandle)return false;
                for(const auto& L:Types){if(!ObservedObject(L.S)||L.S->GetPackage()!=L.Package||!L.S->GetPathName().Equals(L.Path,ESearchCase::CaseSensitive)||!ObservedObject(L.Package)||!L.Package->GetPathName().Equals(L.PackagePath,ESearchCase::CaseSensitive)||L.Package->IsDirty()||L.Package->GetPackageFlags()!=L.PackageFlags||L.S->StructFlags!=L.Flags||L.S->GetSuperStruct()!=nullptr||L.S->GetFlags()!=L.ObjectFlags||L.S->GetStructureSize()!=L.Size||L.S->GetMinAlignment()!=L.Alignment)return false;TArray<FProperty*> Now;for(TFieldIterator<FProperty> It(L.S);It;++It)Now.Add(*It);if(Now!=L.Inventory)return false;for(const auto& F:L.Fields){if(F.P->HasSetterOrGetter()||F.P->GetClass()!=F.Kind||F.P->GetOwnerStruct()!=F.Owner||F.P->GetPropertyFlags()!=F.Flags||F.P->GetOffset_ForInternal()!=F.Offset||F.P->GetSize()!=F.Size||F.P->ArrayDim!=F.Dim)return false;if(F.Child&&(!F.Child->Struct.IsResolved()||F.Child->Struct!=F.ChildType))return false;}}return true;
            };
            if(!SourceCurrent())return false;FString Text;P->ExportTextItem_Direct(Text,&Points,nullptr,nullptr,PPF_None);if(!SourceCurrent())return false;
            TArray<FPCGPoint> Scratch;FStringOutputDevice Errors;const TCHAR* End=P->ImportText_Direct(*Text,&Scratch,nullptr,PPF_None,&Errors);const bool Returned=End!=nullptr;FString Tail=End?FString(End):FString();Tail.TrimStartAndEndInline();if(!SourceCurrent())return false;
            const bool Consumed=Returned&&Tail.IsEmpty()&&Errors.IsEmpty();bool Equal=false;if(Consumed){Equal=P->Identical(&Points,&Scratch,PPF_None);if(!SourceCurrent())return false;}
            auto Proof=MakeShared<FJsonObject>();Proof->SetStringField(TEXT("property_path"),P->GetPathName());Proof->SetStringField(TEXT("condition"),!Returned?TEXT("scratch_import_returned"):(!Tail.IsEmpty()?TEXT("scratch_full_consumption"):(!Errors.IsEmpty()?TEXT("scratch_import_error_free"):TEXT("requested_point_values_identical"))));Proof->SetBoolField(TEXT("expected"),true);Proof->SetBoolField(TEXT("actual"),Consumed&&Equal);Proof->SetStringField(TEXT("import_diagnostics"),Errors);Proof->SetBoolField(TEXT("target_mutation_attempted"),false);Proof->SetBoolField(TEXT("scratch_import_returned"),Returned);Proof->SetBoolField(TEXT("scratch_full_consumption"),Consumed);Proof->SetBoolField(TEXT("requested_point_values_identical"),Equal);Proof->SetNumberField(TEXT("requested_count"),Points.Num());Proof->SetNumberField(TEXT("scratch_count"),Scratch.Num());Proof->SetStringField(TEXT("scope"),TEXT("Prepared point-array scratch only; no target Modify/notify/compile/save. Text precision is not assumed."));
            if(!Consumed||!Equal){Plans->SetObjectField(TEXT("point_import_preflight"),Proof);return false;}
            Row->SetStringField(TEXT("prepared_point_import_text"),Text);Row->SetBoolField(TEXT("point_import_roundtrip_verified"),true);
        }
        // PA standalone enum writers consume this already-validated canonical
        // native payload; no factory construction admission is implied.
        if(Class->GetPathName().Equals(TEXT("/Script/PCG.PCGPointExtentsModifierSettings"),ESearchCase::CaseSensitive)||Class->GetPathName().Equals(TEXT("/Script/PCG.PCGBoundsModifierSettings"),ESearchCase::CaseSensitive))Row->SetField(TEXT("payload"),Payload);
        Row->SetBoolField(TEXT("transient_editor_state"),SamplingEditorDebugProperty(Class,P));if(SamplingEditorDebugProperty(Class,P))Row->SetBoolField(TEXT("serialized_persistence_verified"),false);Row->SetBoolField(TEXT("partial_struct_request"),CastField<FStructProperty>(P)!=nullptr&&Pair.Value->Type==EJson::Object);Row->SetField(TEXT("requested"),Pair.Value);Row->SetField(TEXT("expected"),Expected);Plans->SetObjectField(FString(Pair.Key),Row);Plans->RemoveField(TEXT("preflight_input_field"));
    }
    return Current();
}

bool CubelessPCGUE58::ApplyFreshSetting(UPCGGraph* Owner,UPCGNode* Member,const FString& Name,
    const TSharedPtr<FJsonValue>& Input,FJson& Receipt,TFunctionRef<bool()> Current)
{
    Receipt=Outcome(false);if(!Current()||!Owner||!Member||!Owner->GetNodes().Contains(Member)||Member->GetOuter()!=Owner)return false;
    UPCGSettings* S=Member->GetSettings();if(!Current()||!S||!FreshInterfaceMatches(Member,S)||S->GetOuter()!=Member||S->GetPackage()!=Owner->GetPackage())return false;
    TStrongObjectPtr<UPCGSettings> Hold(S);FSettingContext C;C.Owner=Owner;C.Member=Member;C.Settings=S;FString CanonicalAlias;C.Property=FreshProperty(S->GetClass(),Name,&CanonicalAlias);if(!C.Property)return false;
    TSharedPtr<FJsonValue> Payload,Expected;TArray<FPCGPoint> Points;FProperty* P=C.Property;
    const bool Prepared=PrepareSetting(P,Input,Payload,Expected,Points,&Current,P->ContainerPtrToValuePtr<void>(S));
    if(!Current()||FreshProperty(S->GetClass(),Name)!=P||!Prepared)return false;
    FJson Before;if(!FreshFields(C,Before,Current))return false;
    Receipt->SetStringField(TEXT("property"),Name);Receipt->SetStringField(TEXT("native_property"),P->GetName());Receipt->SetField(TEXT("expected"),Expected);
    Receipt->SetObjectField(TEXT("settings_contract_before"),Before);
    Receipt->SetBoolField(TEXT("transient_editor_state"),SamplingEditorDebugProperty(S->GetClass(),P));if(SamplingEditorDebugProperty(S->GetClass(),P))Receipt->SetBoolField(TEXT("serialized_persistence_verified"),false);Receipt->SetBoolField(TEXT("partial_struct_merged_from_actual"),CastField<FStructProperty>(P)!=nullptr&&Input->Type==EJson::Object);
    const bool Equal=JsonValueText(Before->TryGetField(CanonicalAlias)).Equals(JsonValueText(Expected),ProjectionProperty(P)?ESearchCase::CaseSensitive:ESearchCase::IgnoreCase);TArray<FName> Names;bool Returned=true;
    Receipt->SetBoolField(TEXT("write_attempted"),!Equal);
    if(!Equal)
    {
        if(!Current())return false;FJson Request=MakeShared<FJsonObject>();Request->SetField(P->GetName(),Payload);
        FArrayProperty* PointField=nullptr;FStructProperty* PointElement=nullptr;
        if(PointArray(P,PointField,PointElement,true))
        {
            // This request replaces an entire preplanned native point array. It is
            // not an insertion whose index can be inferred from changed JSON elements.
            // Keep initialized owned point storage across the public editor callback.
            const uint64 InnerFlags=PointElement->GetPropertyFlags();const int32 InnerOffset=PointElement->GetOffset_ForInternal();
            const auto InnerType=PointElement->Struct;TStrongObjectPtr<UScriptStruct> HeldType(FPCGPoint::StaticStruct());
            if(!Current()||!HeldType.IsValid()||InnerType!=HeldType.Get())return false;
            const TArray<FPCGPoint> Replacement=Points;
            Receipt->SetStringField(TEXT("write_route"),TEXT("public_editor_typed_point_array_replacement"));
            Receipt->SetNumberField(TEXT("replacement_count"),Replacement.Num());
            const auto Access=PropertyAccessUtil::SetPropertyValue_Object(P,S,P,&Replacement,INDEX_NONE,
                PropertyAccessUtil::EditorReadOnlyFlags,EPropertyAccessChangeNotifyMode::Default);
            Returned=Access==EPropertyAccessResultFlags::Success;
            Receipt->SetBoolField(TEXT("write_returned"),Returned);Receipt->SetBoolField(TEXT("write_call_returned"),true);
            // Pointer membership is checked before reading a descriptor after callback.
            if(!Current()||FreshProperty(S->GetClass(),Name)!=P||PointField->Inner!=PointElement)return false;
            if(PointElement->GetOwner<FProperty>()!=P||PointElement->ArrayDim!=1||PointElement->GetSize()!=sizeof(FPCGPoint)||
                PointElement->GetPropertyFlags()!=InnerFlags||PointElement->GetOffset_ForInternal()!=InnerOffset||
                !PointElement->Struct.IsResolved()||PointElement->Struct!=InnerType)return false;
        }
        else
        {
            Returned=UToolsetLibrary::SetObjectProperties(S,Encode(Request),Names,EBypassContainerCheck::No);
            Receipt->SetBoolField(TEXT("write_returned"),Returned);Receipt->SetBoolField(TEXT("write_call_returned"),true);
        }
        if(!Current())return false;
    }
    Receipt->SetBoolField(TEXT("property_write_skipped_equal_value"),Equal);
    if(FreshProperty(S->GetClass(),Name)!=P||!FreshInterfaceMatches(Member,S))return false;
    FJson After;if(!FreshFields(C,After,Current))return false;
    FArrayProperty* A=nullptr;FStructProperty* Inner=nullptr;
    const bool Match=PointArray(P,A,Inner,true)?P->Identical(P->ContainerPtrToValuePtr<void>(S),&Points,PPF_None):JsonValueText(After->TryGetField(CanonicalAlias)).Equals(JsonValueText(Expected),ProjectionProperty(P)?ESearchCase::CaseSensitive:ESearchCase::IgnoreCase);
    if(!Current())return false;
    const bool Other=UnrelatedSettingFieldsPreserved(Before,After,CanonicalAlias);
    Receipt->SetObjectField(TEXT("settings_contract_after"),After);Receipt->SetField(TEXT("actual"),After->TryGetField(CanonicalAlias));
    Receipt->SetBoolField(TEXT("requested_value_verified"),Match);Receipt->SetBoolField(TEXT("unrelated_contract_fields_preserved"),Other);
    Receipt->SetBoolField(TEXT("mutation_performed"),Encode(Before)!=Encode(After));Receipt->SetBoolField(TEXT("observation_complete"),true);
    Receipt->SetBoolField(TEXT("success"),Returned&&Match&&Other);return Returned&&Match&&Other;
}

bool CubelessPCGUE58::VerifyFreshSettings(UPCGGraph* Owner,UPCGNode* Member,const FJson& Input,FJson& Receipt,TFunctionRef<bool()> Current)
{
    Receipt=MakeShared<FJsonObject>();if(!Current()||!Owner||!Member||!Owner->GetNodes().Contains(Member)||Member->GetOuter()!=Owner)return false;
    UPCGSettings* S=Member->GetSettings();if(!Current()||!S||!FreshInterfaceMatches(Member,S)||S->GetOuter()!=Member)return false;
    TStrongObjectPtr<UPCGSettings> Hold(S);FSettingContext C;C.Owner=Owner;C.Member=Member;C.Settings=S;FJson Actual;
    if(!FreshFields(C,Actual,Current))return false;
    for(const auto& Pair:Input->Values)
    {
        FString CanonicalAlias;FProperty* P=FreshProperty(S->GetClass(),FString(Pair.Key),&CanonicalAlias);if(!P)return false;TSharedPtr<FJsonValue> Payload,Expected;TArray<FPCGPoint> Points;
        const bool Prepared=PrepareSetting(P,Pair.Value,Payload,Expected,Points,&Current,P->ContainerPtrToValuePtr<void>(S));if(!Current()||FreshProperty(S->GetClass(),FString(Pair.Key))!=P||!Prepared)return false;
        FArrayProperty* Array=nullptr;FStructProperty* Inner=nullptr;
        const bool Match=PointArray(P,Array,Inner,true)?P->Identical(P->ContainerPtrToValuePtr<void>(S),&Points,PPF_None):JsonValueText(Actual->TryGetField(CanonicalAlias)).Equals(JsonValueText(Expected),ProjectionProperty(P)?ESearchCase::CaseSensitive:ESearchCase::IgnoreCase);
        if(!Current())return false;FJson Row=MakeShared<FJsonObject>();Row->SetField(TEXT("expected"),Expected);Row->SetField(TEXT("actual"),Actual->TryGetField(CanonicalAlias));Row->SetBoolField(TEXT("verified"),Match);Receipt->SetObjectField(FString(Pair.Key),Row);if(!Match)return false;
    }
    return Current();
}

FString CubelessPCGUE58::InspectAttributeSelector(const FString& GraphPath, const FString& NodeId, const FString& PropertyName)
{
    FSelectorContext Context;
    FJson Error;
    if (!Selector(GraphPath, NodeId, PropertyName, Context, Error)) return Encode(Error);
    FJson Value = Identity(Context);
    Value->SetObjectField(TEXT("current_selector"), SelectorState(*Context.Selector));
    Value->SetBoolField(TEXT("state_preserved"), Context.bDirty == Context.Owner->GetPackage()->IsDirty());
    return Encode(Value);
}

FString CubelessPCGUE58::PreviewAttributeSelector(const FString& GraphPath, const FString& NodeId, const FString& PropertyName, const FString& RequestJson)
{
    FSelectorContext Context;
    FJson Error;
    if (!Selector(GraphPath, NodeId, PropertyName, Context, Error)) return Encode(Error);
    FJson Request;
    if (RequestJson.Len() > 65536 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(RequestJson), Request) || !Request.IsValid())
        return Encode(Outcome(false, TEXT("pcg_selector_request_invalid"), TEXT("Expected bounded JSON object")));
    const TSet<FString> Allowed = {TEXT("selector_type"), TEXT("attribute_name"), TEXT("selected_property_name"), TEXT("domain_name"), TEXT("point_property"), TEXT("extra_property"), TEXT("extra_names"), TEXT("reset_extra_names")};
    for (const auto& Pair : Request->Values) if (!Allowed.Contains(FString(Pair.Key)))
        return Encode(Outcome(false, TEXT("pcg_selector_request_invalid"), TEXT("Unknown selector request field")));
    bool bReset = true;
    if (Request->HasField(TEXT("reset_extra_names")) && !Request->TryGetBoolField(TEXT("reset_extra_names"), bReset))
        return Encode(Outcome(false, TEXT("pcg_selector_request_invalid"), TEXT("reset_extra_names must be boolean")));
    FString Type, Attribute, Property, Domain, Point, Extra;
    if (!StringField(Request, TEXT("selector_type"), Type, Error, true) ||
        !StringField(Request, TEXT("attribute_name"), Attribute, Error) ||
        !StringField(Request, TEXT("selected_property_name"), Property, Error) ||
        !StringField(Request, TEXT("domain_name"), Domain, Error) ||
        !StringField(Request, TEXT("point_property"), Point, Error) ||
        !StringField(Request, TEXT("extra_property"), Extra, Error)) return Encode(Error);
    const FPCGAttributePropertySelector Before = *Context.Selector;
    FPCGAttributePropertySelector Preview = Before;
    if (Request->HasField(TEXT("domain_name"))) Preview.SetDomainName(FName(*Domain), false);
    const FString Kind = Token(Type);
    if (Kind == TEXT("attribute"))
    {
        if (!Request->HasField(TEXT("attribute_name"))) return Encode(Outcome(false, TEXT("pcg_selector_request_invalid"), TEXT("attribute_name is required")));
        Preview.SetAttributeName(FName(*Attribute), bReset);
    }
    else if (Kind == TEXT("property"))
    {
        if (!Request->HasField(TEXT("selected_property_name"))) return Encode(Outcome(false, TEXT("pcg_selector_request_invalid"), TEXT("selected_property_name is required")));
        Preview.SetPropertyName(FName(*Property), bReset);
    }
    else if (Kind == TEXT("pointproperty") || Kind == TEXT("point"))
    {
        int64 Enum;
        if (!EnumValue(StaticEnum<EPCGPointProperties>(), Point, Enum)) return Encode(Outcome(false, TEXT("pcg_selector_enum_invalid"), TEXT("Unknown EPCGPointProperties value")));
        Preview.SetPointProperty(static_cast<EPCGPointProperties>(Enum), bReset);
    }
    else if (Kind == TEXT("extraproperty") || Kind == TEXT("extra"))
    {
        int64 Enum;
        if (!EnumValue(StaticEnum<EPCGExtraProperties>(), Extra, Enum)) return Encode(Outcome(false, TEXT("pcg_selector_enum_invalid"), TEXT("Unknown EPCGExtraProperties value")));
        Preview.SetExtraProperty(static_cast<EPCGExtraProperties>(Enum), bReset);
    }
    else return Encode(Outcome(false, TEXT("pcg_selector_type_invalid"), TEXT("Expected attribute, property, point_property or extra_property")));
    if (Request->HasField(TEXT("extra_names")))
    {
        const TArray<TSharedPtr<FJsonValue>>* Values;
        if (!Request->TryGetArrayField(TEXT("extra_names"), Values) || Values->Num() > 64) return Encode(Outcome(false, TEXT("pcg_selector_request_invalid"), TEXT("extra_names must be an array of at most 64 strings")));
        TArray<FString> Names;
        for (const auto& Item : *Values)
        {
            FString Name;
            if (!Item->TryGetString(Name) || !Bounded(Name)) return Encode(Outcome(false, TEXT("pcg_selector_request_invalid"), TEXT("Extra names must be bounded, nonempty strings")));
            Names.Add(Name);
        }
        Preview.GetExtraNamesMutable() = MoveTemp(Names);
    }
    if (!Preview.IsValid()) return Encode(Outcome(false, TEXT("pcg_selector_invalid"), TEXT("Native copied selector IsValid() rejected the requested state; asset unchanged")));
    FJson Value = Identity(Context);
    Value->SetObjectField(TEXT("current_selector"), SelectorState(Before));
    Value->SetObjectField(TEXT("preview_selector"), SelectorState(Preview));
    Value->SetBoolField(TEXT("changed"), !Before.IsSame(Preview));
    Value->SetBoolField(TEXT("state_preserved"), Before.IsSame(*Context.Selector) && Context.bDirty == Context.Owner->GetPackage()->IsDirty());
    Value->SetBoolField(TEXT("native_copy_preview"), true);
    return Encode(Value);
}

FString CubelessPCGUE58::InspectConnection(const FString& GraphPath, const FString& FromNode, const FString& FromPin, const FString& ToNode, const FString& ToPin)
{
    FJson Error;
    UPCGGraph* Owner = Graph(GraphPath, Error);
    if (!Owner) return Encode(Error);
    UPCGNode* From = Node(Owner, FromNode, Error);
    if (!From) return Encode(Error);
    UPCGNode* To = Node(Owner, ToNode, Error);
    if (!To) return Encode(Error);
    if (!Bounded(FromPin) || !Bounded(ToPin)) return Encode(Outcome(false, TEXT("pcg_pin_label_invalid"), TEXT("Pin labels must be bounded")));
    UPCGPin* Source = From->GetOutputPin(FName(*FromPin));
    UPCGPin* Target = To->GetInputPin(FName(*ToPin));
    if (!IsValid(Source) || !IsValid(Target) || Source->Node != From || Target->Node != To)
        return Encode(Outcome(false, TEXT("pcg_pin_not_found"), TEXT("Exact output/input pin pair was not found")));
    const bool bDirty = Owner->GetPackage()->IsDirty();
    FText Message;
    const EPCGDataTypeCompatibilityResult Compatibility = Target->GetCompatibilityWithOtherPin(Source, &Message);
    FJson Value = Outcome(true);
    Value->SetStringField(TEXT("graph"), Owner->GetPathName());
    Value->SetStringField(TEXT("from_node"), From->GetPathName());
    Value->SetStringField(TEXT("to_node"), To->GetPathName());
    Value->SetStringField(TEXT("from_pin"), Source->Properties.Label.ToString());
    Value->SetStringField(TEXT("to_pin"), Target->Properties.Label.ToString());
    Value->SetNumberField(TEXT("compatibility_value"), static_cast<int32>(Compatibility));
    Value->SetStringField(TEXT("compatibility_message"), Message.ToString());
    Value->SetBoolField(TEXT("direct_compatible"), Compatibility == EPCGDataTypeCompatibilityResult::Compatible);
    Value->SetBoolField(TEXT("can_connect"), Target->CanConnect(Source));
    Value->SetNumberField(TEXT("source_edge_count"), Source->Edges.Num());
    Value->SetNumberField(TEXT("target_edge_count"), Target->Edges.Num());
    Value->SetBoolField(TEXT("target_allows_multiple_connections"), Target->AllowsMultipleConnections());
    Value->SetBoolField(TEXT("package_dirty_before"), bDirty);
    Value->SetBoolField(TEXT("package_dirty_after"), Owner->GetPackage()->IsDirty());
    Value->SetBoolField(TEXT("state_preserved"), bDirty == Owner->GetPackage()->IsDirty());
    Value->SetBoolField(TEXT("read_only"), true);
    return Encode(Value);
}

FString CubelessPCGUE58::NotifyAndRecompile(const FString& GraphPath, bool bAllowDirty)
{
    FJson Error;
    UPCGGraph* Owner = Graph(GraphPath, Error, true);
    if (!Owner) return Encode(Error);
    const bool bDirty = Owner->GetPackage()->IsDirty();
    if (bDirty && !bAllowDirty) return Encode(Outcome(false, TEXT("pcg_graph_dirty"), TEXT("Graph is dirty and this explicit request did not admit dirty state")));
    FJson Value = Outcome(true);
    Value->SetStringField(TEXT("graph"), Owner->GetPathName());
    Value->SetBoolField(TEXT("package_dirty_before"), bDirty);
    Value->SetBoolField(TEXT("mutation_attempted"), true);
    Owner->ForceNotificationForEditor(EPCGChangeType::Structural);
    const bool bCacheChanged = Owner->Recompile();
    Value->SetBoolField(TEXT("notified"), true);
    Value->SetBoolField(TEXT("recompiled"), bCacheChanged);
    Value->SetStringField(TEXT("recompiled_semantics"), TEXT("Exact UPCGGraph::Recompile return: graph compiler cache-change result; false is not a compile failure, true also occurs when no active graph compiler exists"));
    Value->SetBoolField(TEXT("mutation_performed"), true);
    Value->SetBoolField(TEXT("package_dirty_after"), Owner->GetPackage()->IsDirty());
    Value->SetNumberField(TEXT("explicit_notify_requests"), 1);
    Value->SetNumberField(TEXT("explicit_recompile_requests"), 1);
    return Encode(Value);
}

// NL: exact four-role selector assignment. Draft checkpoint; not a product include.
namespace
{
    bool SelectorNoNul(const FString& S)
    { for (int32 I = 0; I < S.Len(); ++I) if (S[I] == 0) return false; return true; }
    bool SelectorSafe()
    { return IsInGameThread() && !IsEngineExitRequested() && !GExitPurge && !IsGarbageCollecting() && !UE::IsSavingPackage() && IsValid(GEditor); }
    FString SelectorAddress(const void* P) { return FString::Printf(TEXT("%p"), P); }
    FJson SelectorPlainValue(const FPCGAttributePropertySelector& V)
    {
        FJson J = MakeShared<FJsonObject>(); J->SetNumberField(TEXT("selection_value"), static_cast<int64>(V.GetSelection())); J->SetStringField(TEXT("attribute_name"), V.GetAttributeName().ToString()); J->SetStringField(TEXT("selected_property_name"), V.GetPropertyName().ToString()); J->SetStringField(TEXT("domain_name"), V.GetDomainName().ToString()); J->SetNumberField(TEXT("extra_property_value"), static_cast<int64>(V.GetExtraProperty()));
        TArray<TSharedPtr<FJsonValue>> Extra; for (const auto& S : V.GetExtraNames()) Extra.Add(MakeShared<FJsonValueString>(S)); J->SetArrayField(TEXT("extra_names"), Extra); return J;
    }
    bool SelectorFullEqual(const FPCGAttributePropertySelector& A, const FPCGAttributePropertySelector& B)
    {
        auto SameName = [](FName X, FName Y) { return X.ToString().Equals(Y.ToString(), ESearchCase::CaseSensitive); };
        if (A.GetSelection() != B.GetSelection() || !SameName(A.GetAttributeName(), B.GetAttributeName()) || !SameName(A.GetPropertyName(), B.GetPropertyName()) || !SameName(A.GetDomainName(), B.GetDomainName()) || A.GetExtraProperty() != B.GetExtraProperty() || A.GetExtraNames().Num() != B.GetExtraNames().Num()) return false;
        for (int32 I = 0; I < A.GetExtraNames().Num(); ++I) if (!A.GetExtraNames()[I].Equals(B.GetExtraNames()[I], ESearchCase::CaseSensitive)) return false;
        return true;
    }
    bool SelectorObjectSlot(UObject* O, const TCHAR* Name, UObject* Expected)
    {
        FObjectProperty* P = FindFProperty<FObjectProperty>(O->GetClass(), Name); if (!P || P->ArrayDim != 1 || P->HasSetterOrGetter()) return false;
        const auto* H = P->GetObjectPtrPropertyValuePtr(P->ContainerPtrToValuePtr<void>(O)); return H && H->IsResolved() && *H == Expected;
    }
    UObject* SelectorObjectValue(UObject* O, const TCHAR* Name)
    {
        FObjectProperty* P = FindFProperty<FObjectProperty>(O->GetClass(), Name); if (!P || P->ArrayDim != 1 || P->HasSetterOrGetter()) return nullptr;
        const auto* H = P->GetObjectPtrPropertyValuePtr(P->ContainerPtrToValuePtr<void>(O)); return H && H->IsResolved() ? H->Get() : nullptr;
    }
    bool SelectorJsonEqual(const FJson& A, const FJson& B)
    {
        return A && B && FJsonValue::CompareEqual(FJsonValueObject(A), FJsonValueObject(B));
    }

    bool SelectorCarrier(const FString& Text, FJson& Out)
    {
        if (Text.IsEmpty() || Text.Len() > 262144 || !SelectorNoNul(Text)) return false;
        // The ordinary JSON object reader overwrites duplicate keys. Check its
        // token stream first, including nested expected-preview/request objects.
        auto Reader = TJsonReaderFactory<>::Create(TEXT("[") + Text + TEXT("]"));
        TArray<TArray<FString>> Keys; TArray<bool> Objects;
        EJsonNotation N; int32 Count = 0;
        while (Reader->ReadNext(N))
        {
            if (N == EJsonNotation::Error || ++Count > 8192) return false;
            if (N != EJsonNotation::ObjectEnd && N != EJsonNotation::ArrayEnd && !Objects.IsEmpty() && Objects.Last())
            {
                const FString Key = Reader->GetIdentifier();
                if (!SelectorNoNul(Key) || Keys.Last().ContainsByPredicate([&](const FString& K) { return K.Equals(Key, ESearchCase::IgnoreCase); })) return false;
                Keys.Last().Add(Key);
            }
            if (N == EJsonNotation::Number && !FMath::IsFinite(Reader->GetValueAsNumber())) return false;
            if (N == EJsonNotation::String && !SelectorNoNul(Reader->GetValueAsString())) return false;
            if (N == EJsonNotation::ObjectStart || N == EJsonNotation::ArrayStart)
            { if (Objects.Num() >= 12) return false; Objects.Add(N == EJsonNotation::ObjectStart); Keys.AddDefaulted(); }
            else if (N == EJsonNotation::ObjectEnd || N == EJsonNotation::ArrayEnd)
            { if (Objects.IsEmpty()) return false; Objects.Pop(); Keys.Pop(); }
        }
        if (!Reader->GetErrorMessage().IsEmpty() || !Objects.IsEmpty() || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Out) || !Out) return false;
        const TArray<FString> Allowed{TEXT("graph_path"), TEXT("node_id"), TEXT("selector_property_name"), TEXT("request"), TEXT("expected_preview")};
        if (Out->Values.Num() != Allowed.Num()) return false;
        for (const auto& P : Out->Values) if (!Allowed.ContainsByPredicate([&](const FString& K) { return K.Equals(FString(P.Key), ESearchCase::CaseSensitive); })) return false;
        return true;
    }

    // Own complete declared input/output values, never a byte image or a
    // borrowed pointer into the settings. Only public selector methods mutate.
    struct FSelectorValue
    {
        FPCGAttributePropertyInputSelector Input;
        FPCGAttributePropertyOutputSelector Output;
        bool bOutput = false;
        FPCGAttributePropertySelector& Base() { return bOutput ? static_cast<FPCGAttributePropertySelector&>(Output) : static_cast<FPCGAttributePropertySelector&>(Input); }
        const FPCGAttributePropertySelector& Base() const { return bOutput ? static_cast<const FPCGAttributePropertySelector&>(Output) : static_cast<const FPCGAttributePropertySelector&>(Input); }
        const void* Address() const { return bOutput ? static_cast<const void*>(&Output) : static_cast<const void*>(&Input); }
        void Copy(const void* SourceAddress)
        { if (bOutput) Output = *static_cast<const FPCGAttributePropertyOutputSelector*>(SourceAddress); else Input = *static_cast<const FPCGAttributePropertyInputSelector*>(SourceAddress); }
    };

    struct FSelectorAssignmentScope
    {
        struct FObjectRow { UObject* Object; UObject* Outer; UClass* Class; FName Name; FString Path; };
        struct FFieldRow { FProperty* P; FString Path; FFieldClass* Kind; FFieldVariant Owner; uint64 Flags; int32 Offset, Size, Dim; FStructProperty* Struct; TObjectPtr<UScriptStruct> StructType; FArrayProperty* Array; FProperty* Inner; };
        struct FTypeRow { UStruct* Type; FString Path; UStruct* Super; UPackage* Package; uint32 PackageFlags; EObjectFlags ObjectFlags; int32 Size, Align; uint64 Flags; TArray<FProperty*> Inventory; TArray<FFieldRow> Fields; };
        struct FEnumRow { UEnum* E; UPackage* Package; uint32 PackageFlags; EObjectFlags Flags; UEnum::ECppForm Form; TArray<TPair<FName, int64>> Names; };
        struct FNodeRow { UPCGNode* N; UPCGSettings* S; FName Title; int32 X, Y; TArray<UPCGPin*> In, Out; FJson Finite; };
        struct FPinRow { UPCGPin* P; UPCGNode* Node; FPCGPinProperties Properties; TArray<UPCGEdge*> Edges; };
        struct FEdgeRow { UPCGEdge* E; UPCGPin* A; UPCGPin* B; };
        struct FSelectorRow { UPCGSettings* S; FStructProperty* P; FSelectorValue Before, Expected; };

        FSelectorContext C;
        FJson Result = Outcome(false), GraphBefore;
        FSelectorAssignmentScope* SourceScope = nullptr;
        TArray<TStrongObjectPtr<UObject>> Holds;
        TArray<FObjectRow> Objects;
        TArray<FTypeRow> Types;
        TArray<FEnumRow> Enums;
        TArray<FNodeRow> Nodes;
        TArray<FPinRow> Pins;
        TArray<FEdgeRow> Edges;
        TArray<FSelectorRow> Selectors;
        TArray<UPCGNode*> Authored;
        UPCGNode* Input = nullptr; UPCGNode* Output = nullptr;
        UPackage* Package = nullptr; uint32 PackageFlags = 0;
        UObject* Editor = nullptr; UWorld* World = nullptr; bool bWorldCaptured = false;
        bool bExpectedDirty = false, bFailed = false, bReady = false, bUnsafe = false;
        int32 Selected = INDEX_NONE;
        FString Stage = TEXT("bind");
        FString CapturedGraphPath;
        FJson Counts = MakeShared<FJsonObject>();

        FSelectorAssignmentScope()
        {
            for (const TCHAR* K : {TEXT("target_modify_attempts"), TEXT("target_modify_returns"), TEXT("graph_modify_attempts"), TEXT("graph_modify_returns"), TEXT("pre_edit_attempts"), TEXT("pre_edit_returns"), TEXT("assignment_attempts"), TEXT("assignment_returns"), TEXT("post_edit_attempts"), TEXT("post_edit_returns")}) Counts->SetNumberField(K, 0);
        }
        void Count(const TCHAR* K) { Counts->SetNumberField(K, Counts->GetIntegerField(K) + 1); }
        bool Fail(const TCHAR* Code, const TCHAR* Message, const FString& Object = FString(), const FString& Property = FString())
        {
            if (!bFailed)
            {
                bFailed = true; Result->SetBoolField(TEXT("success"), false); Result->SetStringField(TEXT("error_code"), Code); Result->SetStringField(TEXT("error"), Message);
                FJson First = MakeShared<FJsonObject>(); First->SetStringField(TEXT("stage"), Stage); First->SetStringField(TEXT("object"), Object.IsEmpty() ? (CapturedGraphPath.IsEmpty() ? TEXT("unbound selector request") : CapturedGraphPath) : Object); First->SetStringField(TEXT("property"), Property.IsEmpty() ? FString(Code) : Property);
                Result->SetObjectField(TEXT("first_failure"), First);
                First->SetBoolField(TEXT("expected_actual_observed"), false); First->SetStringField(TEXT("expected"), TEXT("scoped contract satisfied")); First->SetStringField(TEXT("actual"), TEXT("not observed; no post-failure SDK query"));
            }
            return false;
        }
        bool Mismatch(const TCHAR* Code, const TCHAR* Message, const FString& Object, const FString& Property, const TSharedPtr<FJsonValue>& Expected, const TSharedPtr<FJsonValue>& Actual)
        {
            const bool First = !bFailed; Fail(Code, Message, Object, Property);
            if (First) { const FJson J = Result->GetObjectField(TEXT("first_failure")); J->SetField(TEXT("expected"), Expected); J->SetField(TEXT("actual"), Actual); J->SetBoolField(TEXT("expected_actual_observed"), true); }
            return false;
        }
        bool Hold(UObject* O)
        {
            if (!ObservedObject(O)) return Fail(TEXT("pcg_selector_native_source_invalid"), TEXT("Required loaded ordinary object is unavailable"));
            if (Objects.ContainsByPredicate([&](const FObjectRow& R) { return R.Object == O; })) return true;
            Holds.Emplace(O); Objects.Add({O, O->GetOuter(), O->GetClass(), O->GetFName(), O->GetPathName()}); return true;
        }
        FString PathOf(const UObject* O) const
        { const auto* R = Objects.FindByPredicate([&](const FObjectRow& V) { return V.Object == O; }); return R ? R->Path : TEXT("unobserved object identity"); }
        bool NativeType(UStruct* T)
        {
            if (!T || Types.ContainsByPredicate([&](const FTypeRow& R) { return R.Type == T; })) return T != nullptr;
            if (!Hold(T) || !Hold(T->GetPackage()) || T->GetPackage()->IsDirty() || !T->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)) return false;
            UClass* Class = Cast<UClass>(T); UScriptStruct* Struct = Cast<UScriptStruct>(T);
            if ((!Class || !Class->HasAllClassFlags(CLASS_Native) || Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists)) && (!Struct || !(Struct->StructFlags & STRUCT_Native))) return false;
            FTypeRow R; R.Type = T; R.Path = T->GetPathName(); R.Super = T->GetSuperStruct(); R.Package = T->GetPackage(); R.PackageFlags = R.Package->GetPackageFlags(); R.ObjectFlags = T->GetFlags(); R.Size = T->GetStructureSize(); R.Align = T->GetMinAlignment(); R.Flags = Class ? static_cast<uint64>(Class->GetClassFlags()) : static_cast<uint64>(Struct->StructFlags);
            for (TFieldIterator<FProperty> It(T); It; ++It)
            {
                FProperty* P = *It; R.Inventory.Add(P);
                auto* S = CastField<FStructProperty>(P); auto* A = CastField<FArrayProperty>(P);
                R.Fields.Add({P, P->GetPathName(), P->GetClass(), P->GetOwnerVariant(), P->GetPropertyFlags(), P->GetOffset_ForInternal(), P->GetSize(), P->ArrayDim, S, S ? S->Struct : TObjectPtr<UScriptStruct>(), A, A ? A->Inner : nullptr});
                if (A && A->Inner) { FProperty* I = A->Inner; auto* IS = CastField<FStructProperty>(I); R.Fields.Add({I, I->GetPathName(), I->GetClass(), I->GetOwnerVariant(), I->GetPropertyFlags(), I->GetOffset_ForInternal(), I->GetSize(), I->ArrayDim, IS, IS ? IS->Struct : TObjectPtr<UScriptStruct>(), nullptr, nullptr}); }
                if (!Hold(P->GetOwnerStruct())) return false;
            }
            Types.Add(MoveTemp(R));
            return !T->GetSuperStruct() || NativeType(T->GetSuperStruct());
        }
        bool NativeEnum(UEnum* E)
        {
            if (!E || Enums.ContainsByPredicate([&](const FEnumRow& R) { return R.E == E; })) return E != nullptr;
            if (!Hold(E) || !Hold(E->GetPackage()) || E->GetPackage()->IsDirty() || !E->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)) return false;
            FEnumRow R; R.E = E; R.Package = E->GetPackage(); R.PackageFlags = R.Package->GetPackageFlags(); R.Flags = E->GetFlags(); R.Form = E->GetCppForm();
            if (E->NumEnums() < 1 || E->NumEnums() > 256) return false;
            for (int32 I = 0; I < E->NumEnums(); ++I) R.Names.Add({E->GetNameByIndex(I), E->GetValueByIndex(I)});
            Enums.Add(MoveTemp(R)); return true;
        }
        bool Raw()
        {
            if (bFailed) return false;
            if (SourceScope && !SourceScope->Raw()) { bUnsafe = true; return Fail(TEXT("pcg_selector_parent_source_changed"), TEXT("Original source current failed during transient assignment; no further SDK observation")); }
            struct FGuard { bool& Unsafe; bool Complete = false; ~FGuard() { if (!Complete) Unsafe = true; } } Guard{bUnsafe};
            if (!SelectorSafe() || (bWorldCaptured && GEditor != Editor)) return Fail(TEXT("pcg_selector_editor_changed"), TEXT("Unsafe GC/save/exit or captured Editor context changed; no further SDK observation"));
            UWorld* Now = GEditor->GetEditorWorldContext().World();
            if (!SelectorSafe() || (bWorldCaptured && (GEditor != Editor || Now != World || Now != GWorld)) || (Now && (!IsValid(Now) || Now->bIsTearingDown)) || GEditor->PlayWorld || GEditor->GetPIEWorldContext()) return Fail(TEXT("pcg_selector_world_changed"), TEXT("Captured Editor world/no-PIE role changed"));
            for (const auto& R : Objects)
            {
                if (!ObservedObject(R.Object)) return Mismatch(TEXT("pcg_selector_identity_changed"), TEXT("Held object liveness/ordinary role changed"), R.Path, TEXT("ordinary_loaded_object"), MakeShared<FJsonValueBoolean>(true), MakeShared<FJsonValueBoolean>(false));
                UObject* Outer = R.Object->GetOuter(); if (Outer != R.Outer) return Mismatch(TEXT("pcg_selector_identity_changed"), TEXT("Held object outer changed"), R.Path, TEXT("outer_pointer"), MakeShared<FJsonValueString>(SelectorAddress(R.Outer)), MakeShared<FJsonValueString>(SelectorAddress(Outer)));
                UClass* Class = R.Object->GetClass(); if (Class != R.Class) return Mismatch(TEXT("pcg_selector_identity_changed"), TEXT("Held object class changed"), R.Path, TEXT("class_pointer"), MakeShared<FJsonValueString>(SelectorAddress(R.Class)), MakeShared<FJsonValueString>(SelectorAddress(Class)));
                const FString Name = R.Object->GetFName().ToString(); if (!Name.Equals(R.Name.ToString(), ESearchCase::CaseSensitive)) return Mismatch(TEXT("pcg_selector_identity_changed"), TEXT("Held object name changed"), R.Path, TEXT("name"), MakeShared<FJsonValueString>(R.Name.ToString()), MakeShared<FJsonValueString>(Name));
            }
            for (const auto& T : Types)
            {
                const auto* Class = Cast<UClass>(T.Type); const auto* Struct = Cast<UScriptStruct>(T.Type);
                const uint64 Flags = Class ? static_cast<uint64>(Class->GetClassFlags()) : static_cast<uint64>(Struct->StructFlags);
                const int32 Size = T.Type->GetStructureSize(), Align = T.Type->GetMinAlignment();
                if (T.Type->GetSuperStruct() != T.Super || Size != T.Size || Align != T.Align || Flags != T.Flags || T.Type->GetFlags() != T.ObjectFlags || T.Type->GetPackage() != T.Package || T.Package->GetPackageFlags() != T.PackageFlags || T.Package->IsDirty())
                { FJson E = MakeShared<FJsonObject>(), A = MakeShared<FJsonObject>(); E->SetNumberField(TEXT("size"), T.Size); A->SetNumberField(TEXT("size"), Size); E->SetNumberField(TEXT("alignment"), T.Align); A->SetNumberField(TEXT("alignment"), Align); E->SetStringField(TEXT("native_flags"), LexToString(T.Flags)); A->SetStringField(TEXT("native_flags"), LexToString(Flags)); return Mismatch(TEXT("pcg_selector_native_layout_changed"), TEXT("Captured native type/layout changed"), T.Path, TEXT("native_type_layout"), MakeShared<FJsonValueObject>(E), MakeShared<FJsonValueObject>(A)); }
                TArray<FProperty*> CurrentFields; for (TFieldIterator<FProperty> It(T.Type); It; ++It) CurrentFields.Add(*It);
                if (CurrentFields != T.Inventory) return Mismatch(TEXT("pcg_selector_native_fields_changed"), TEXT("Captured full reflected inventory changed"), T.Path, TEXT("field_inventory_count_or_identity"), MakeShared<FJsonValueNumber>(T.Inventory.Num()), MakeShared<FJsonValueNumber>(CurrentFields.Num()));
                for (const auto& F : T.Fields)
                {
                    const uint64 PF = F.P->GetPropertyFlags(); const int32 PO = F.P->GetOffset_ForInternal(), PS = F.P->GetSize(), PD = F.P->ArrayDim;
                    const bool Kind = F.P->GetClass() == F.Kind, Owner = F.P->GetOwnerVariant() == F.Owner, Slot = !F.Array || F.Array->Inner == F.Inner, Type = !F.Struct || F.Struct->Struct == F.StructType;
                    if (!Kind || !Owner || !Slot || !Type || PF != F.Flags || PO != F.Offset || PS != F.Size || PD != F.Dim)
                    { FJson E = MakeShared<FJsonObject>(), A = MakeShared<FJsonObject>(); E->SetStringField(TEXT("flags"), LexToString(F.Flags)); A->SetStringField(TEXT("flags"), LexToString(PF)); E->SetNumberField(TEXT("offset"), F.Offset); A->SetNumberField(TEXT("offset"), PO); E->SetNumberField(TEXT("size"), F.Size); A->SetNumberField(TEXT("size"), PS); E->SetNumberField(TEXT("array_dim"), F.Dim); A->SetNumberField(TEXT("array_dim"), PD); E->SetBoolField(TEXT("kind_owner_child_type_equal"), true); A->SetBoolField(TEXT("kind_owner_child_type_equal"), Kind && Owner && Slot && Type); return Mismatch(TEXT("pcg_selector_native_field_changed"), TEXT("Captured property owner/type/flags/slot/layout changed"), T.Path, F.Path, MakeShared<FJsonValueObject>(E), MakeShared<FJsonValueObject>(A)); }
                }
            }
            for (const auto& E : Enums)
            {
                if (E.E->GetPackage() != E.Package || E.Package->IsDirty() || E.Package->GetPackageFlags() != E.PackageFlags || E.E->GetFlags() != E.Flags || E.E->GetCppForm() != E.Form || E.E->NumEnums() != E.Names.Num()) return Fail(TEXT("pcg_selector_native_enum_changed"), TEXT("Native enum source/package/form/count changed"));
                for (int32 I = 0; I < E.Names.Num(); ++I) if (E.E->GetNameByIndex(I) != E.Names[I].Key || E.E->GetValueByIndex(I) != E.Names[I].Value) return Fail(TEXT("pcg_selector_native_enum_changed"), TEXT("Native enum name/value inventory changed"));
            }
            if (C.Owner)
            {
                if (C.Owner->GetNodes() != Authored) { TArray<TSharedPtr<FJsonValue>> A, B; for (const auto* N : Authored) A.Add(MakeShared<FJsonValueString>(SelectorAddress(N))); for (const auto* N : C.Owner->GetNodes()) B.Add(MakeShared<FJsonValueString>(SelectorAddress(N))); return Mismatch(TEXT("pcg_selector_graph_changed"), TEXT("Exact authored node membership/order changed"), CapturedGraphPath, TEXT("Nodes"), MakeShared<FJsonValueArray>(A), MakeShared<FJsonValueArray>(B)); }
                if (!SelectorObjectSlot(C.Owner, TEXT("InputNode"), Input) || !SelectorObjectSlot(C.Owner, TEXT("OutputNode"), Output) || C.Owner->GetPackage() != Package || Package->GetPackageFlags() != PackageFlags) return Fail(TEXT("pcg_selector_graph_changed"), TEXT("Exact graph IO/package role changed"), CapturedGraphPath, TEXT("IO/package_identity"));
                const bool Dirty = Package->IsDirty(); if (Dirty != bExpectedDirty) return Mismatch(TEXT("pcg_selector_graph_changed"), TEXT("Exact planned package dirty state differs"), CapturedGraphPath, TEXT("package_dirty"), MakeShared<FJsonValueBoolean>(bExpectedDirty), MakeShared<FJsonValueBoolean>(Dirty));
            }
            for (const auto& N : Nodes)
            {
                UObject* Interface = SelectorObjectValue(N.N, TEXT("SettingsInterface"));
                if (!FreshInterfaceMatches(N.N, N.S)) return Mismatch(TEXT("pcg_selector_node_changed"), TEXT("Captured settings interface changed"), PathOf(N.N), TEXT("SettingsInterface"), MakeShared<FJsonValueString>(SelectorAddress(N.S)), MakeShared<FJsonValueString>(Interface ? SelectorAddress(Interface) : TEXT("null or unresolved")));
                const FString Title = N.N->GetAuthoredTitleName().ToString(); if (!Title.Equals(N.Title.ToString(), ESearchCase::CaseSensitive)) return Mismatch(TEXT("pcg_selector_node_changed"), TEXT("Authored title changed"), PathOf(N.N), TEXT("NodeTitle"), MakeShared<FJsonValueString>(N.Title.ToString()), MakeShared<FJsonValueString>(Title));
                if (N.N->PositionX != N.X) return Mismatch(TEXT("pcg_selector_node_changed"), TEXT("Authored position changed"), PathOf(N.N), TEXT("PositionX"), MakeShared<FJsonValueNumber>(N.X), MakeShared<FJsonValueNumber>(N.N->PositionX));
                if (N.N->PositionY != N.Y) return Mismatch(TEXT("pcg_selector_node_changed"), TEXT("Authored position changed"), PathOf(N.N), TEXT("PositionY"), MakeShared<FJsonValueNumber>(N.Y), MakeShared<FJsonValueNumber>(N.N->PositionY));
                auto Same = [&](const TArray<TObjectPtr<UPCGPin>>& A, const TArray<UPCGPin*>& B, const TCHAR* Key)
                { if (A.Num() != B.Num()) return Mismatch(TEXT("pcg_selector_pins_changed"), TEXT("Physical pin count changed"), PathOf(N.N), Key, MakeShared<FJsonValueNumber>(B.Num()), MakeShared<FJsonValueNumber>(A.Num())); for (int32 I = 0; I < A.Num(); ++I) if (!A[I].IsResolved() || A[I] != B[I]) return Mismatch(TEXT("pcg_selector_pins_changed"), TEXT("Physical pin membership/order changed"), PathOf(N.N), FString::Printf(TEXT("%s[%d]"), Key, I), MakeShared<FJsonValueString>(SelectorAddress(B[I])), MakeShared<FJsonValueString>(A[I].IsResolved() ? SelectorAddress(A[I].Get()) : TEXT("unresolved handle"))); return true; };
                if (!Same(N.N->GetInputPins(), N.In, TEXT("InputPins")) || !Same(N.N->GetOutputPins(), N.Out, TEXT("OutputPins"))) return false;
            }
            for (const auto& P : Pins)
            {
                if (!P.P->Node.IsResolved() || P.P->Node != P.Node) return Mismatch(TEXT("pcg_selector_pin_changed"), TEXT("Physical pin node changed"), PathOf(P.P), TEXT("Node"), MakeShared<FJsonValueString>(SelectorAddress(P.Node)), MakeShared<FJsonValueString>(P.P->Node.IsResolved() ? SelectorAddress(P.P->Node.Get()) : TEXT("unresolved handle")));
                if (!(P.P->Properties == P.Properties))
                { FJson E = MakeShared<FJsonObject>(), A = MakeShared<FJsonObject>(); E->SetStringField(TEXT("label"), P.Properties.Label.ToString()); A->SetStringField(TEXT("label"), P.P->Properties.Label.ToString()); E->SetBoolField(TEXT("allowed_types_equal"), true); A->SetBoolField(TEXT("allowed_types_equal"), P.P->Properties.AllowedTypes == P.Properties.AllowedTypes); E->SetBoolField(TEXT("multiple_data"), P.Properties.bAllowMultipleData); A->SetBoolField(TEXT("multiple_data"), P.P->Properties.bAllowMultipleData); E->SetBoolField(TEXT("multiple_connections"), P.Properties.AllowsMultipleConnections()); A->SetBoolField(TEXT("multiple_connections"), P.P->Properties.AllowsMultipleConnections()); E->SetNumberField(TEXT("usage"), static_cast<int32>(P.Properties.Usage)); A->SetNumberField(TEXT("usage"), static_cast<int32>(P.P->Properties.Usage)); E->SetNumberField(TEXT("status"), static_cast<int32>(P.Properties.PinStatus)); A->SetNumberField(TEXT("status"), static_cast<int32>(P.P->Properties.PinStatus)); E->SetBoolField(TEXT("invisible"), P.Properties.bInvisiblePin); A->SetBoolField(TEXT("invisible"), P.P->Properties.bInvisiblePin); return Mismatch(TEXT("pcg_selector_pin_changed"), TEXT("Physical pin semantic properties changed"), PathOf(P.P), TEXT("Properties"), MakeShared<FJsonValueObject>(E), MakeShared<FJsonValueObject>(A)); }
                if (P.P->Edges.Num() != P.Edges.Num()) return Mismatch(TEXT("pcg_selector_links_changed"), TEXT("Existing physical edge count changed"), PathOf(P.P), TEXT("Edges.Num"), MakeShared<FJsonValueNumber>(P.Edges.Num()), MakeShared<FJsonValueNumber>(P.P->Edges.Num()));
                for (int32 I = 0; I < P.Edges.Num(); ++I) if (!P.P->Edges[I].IsResolved() || P.P->Edges[I] != P.Edges[I]) return Mismatch(TEXT("pcg_selector_links_changed"), TEXT("Existing physical edge order/identity changed"), PathOf(P.P), FString::Printf(TEXT("Edges[%d]"), I), MakeShared<FJsonValueString>(SelectorAddress(P.Edges[I])), MakeShared<FJsonValueString>(P.P->Edges[I].IsResolved() ? SelectorAddress(P.P->Edges[I].Get()) : TEXT("unresolved handle")));
            }
            for (const auto& E : Edges)
            { if (!E.E->InputPin.IsResolved() || E.E->InputPin != E.A) return Mismatch(TEXT("pcg_selector_edge_changed"), TEXT("Reciprocal edge source changed"), PathOf(E.E), TEXT("InputPin"), MakeShared<FJsonValueString>(SelectorAddress(E.A)), MakeShared<FJsonValueString>(E.E->InputPin.IsResolved() ? SelectorAddress(E.E->InputPin.Get()) : TEXT("unresolved handle"))); if (!E.E->OutputPin.IsResolved() || E.E->OutputPin != E.B) return Mismatch(TEXT("pcg_selector_edge_changed"), TEXT("Reciprocal edge target changed"), PathOf(E.E), TEXT("OutputPin"), MakeShared<FJsonValueString>(SelectorAddress(E.B)), MakeShared<FJsonValueString>(E.E->OutputPin.IsResolved() ? SelectorAddress(E.E->OutputPin.Get()) : TEXT("unresolved handle"))); }
            if (bReady) for (const auto& S : Selectors)
            {
                const void* Address = S.P->ContainerPtrToValuePtr<void>(S.S);
                FSelectorValue Actual; Actual.bOutput = S.Expected.bOutput; Actual.Copy(Address);
                if (!SelectorFullEqual(Actual.Base(), S.Expected.Base())) return Mismatch(TEXT("pcg_selector_value_changed"), TEXT("Selected or sibling selector differs from its complete planned public value"), Objects.FindByPredicate([&](const FObjectRow& R) { return R.Object == S.S; })->Path, S.P->GetFName().ToString(), MakeShared<FJsonValueObject>(SelectorPlainValue(S.Expected.Base())), MakeShared<FJsonValueObject>(SelectorPlainValue(Actual.Base())));
            }
            Guard.Complete = true; return true;
        }
        bool Values()
        {
            if (!Raw()) return false;
            for (const auto& N : Nodes)
            {
                if (!N.Finite) continue;
                FSettingContext V; V.Owner = C.Owner; V.Member = N.N; V.Settings = N.S;
                FJson Now; if (!FreshFields(V, Now, [this] { return Raw(); })) { bUnsafe = true; return Fail(TEXT("pcg_selector_fields_unobservable"), TEXT("Finite settings readback unavailable")); }
                if (!SelectorJsonEqual(N.Finite, Now))
                { bUnsafe = true; for (const auto& E : N.Finite->Values) { const auto Actual = Now->TryGetField(FString(E.Key)); if (!Actual || !FJsonValue::CompareEqual(*E.Value, *Actual)) return Mismatch(TEXT("pcg_selector_other_fields_changed"), TEXT("Existing finite settings field changed"), PathOf(N.S), FString(E.Key), E.Value, Actual ? Actual : MakeShared<FJsonValueNull>()); } return Mismatch(TEXT("pcg_selector_other_fields_changed"), TEXT("Finite settings key count changed"), PathOf(N.S), TEXT("finite_field_count"), MakeShared<FJsonValueNumber>(N.Finite->Values.Num()), MakeShared<FJsonValueNumber>(Now->Values.Num())); }
            }
            return Raw();
        }
        bool State(const FSelectorValue& V, FJson& Out)
        {
            // The owned copy is safe across public display/enum operations;
            // the graph/type fence is still required after each SDK return.
            if (!Raw()) return false; Out = MakeShared<FJsonObject>(); const auto& B = V.Base();
            Out->SetNumberField(TEXT("selection_value"), static_cast<int64>(B.GetSelection()));
            auto EnumName = [&](UEnum* E, int64 Number, const TCHAR* K) { const FString S = E->GetNameStringByValue(Number); if (!Raw()) return false; Out->SetStringField(K, S); return true; };
            if (!EnumName(StaticEnum<EPCGAttributePropertySelection>(), static_cast<int64>(B.GetSelection()), TEXT("selection"))) return false;
            Out->SetStringField(TEXT("attribute_name"), B.GetAttributeName().ToString()); Out->SetStringField(TEXT("selected_property_name"), B.GetPropertyName().ToString()); Out->SetStringField(TEXT("domain_name"), B.GetDomainName().ToString());
            const auto Point = B.GetPointProperty(); if (!Raw() || !EnumName(StaticEnum<EPCGPointProperties>(), static_cast<int64>(Point), TEXT("point_property")) || !EnumName(StaticEnum<EPCGExtraProperties>(), static_cast<int64>(B.GetExtraProperty()), TEXT("extra_property"))) return false;
            const FName Name = B.GetName(); if (!Raw()) return false; Out->SetStringField(TEXT("name"), Name.ToString());
            const FString Display = B.ToString(); if (!Raw()) return false; Out->SetStringField(TEXT("display"), Display);
            const bool Valid = B.IsValid(); if (!Raw()) return false; Out->SetBoolField(TEXT("valid"), Valid);
            TArray<TSharedPtr<FJsonValue>> Extra; for (const FString& S : B.GetExtraNames()) Extra.Add(MakeShared<FJsonValueString>(S)); Out->SetArrayField(TEXT("extra_names"), Extra); return Raw();
        }
        FJson IdentityValue()
        {
            FJson J = Identity(C); J->SetBoolField(TEXT("state_preserved"), true); return J;
        }
        bool Bind(const FString& Path, const FString& Id, const FString& Name)
        {
            if (SourceScope && !SourceScope->Raw()) return Fail(TEXT("pcg_selector_parent_source_changed"), TEXT("Original source current failed before transient bind"));
            if (!SelectorSafe()) return Fail(TEXT("pcg_selector_context_unavailable"), TEXT("Requires safe editor game-thread outside GC/save/exit"));
            Editor = GEditor; if (!Hold(Editor)) return false; World = GEditor->GetEditorWorldContext().World();
            if (!SelectorSafe() || GEditor != Editor || World != GWorld || (World && (!Hold(World) || World->bIsTearingDown)) || GEditor->PlayWorld || GEditor->GetPIEWorldContext()) return Fail(TEXT("pcg_selector_context_unavailable"), TEXT("Editor world is not stable/no-PIE")); bWorldCaptured = true;
            C.Owner = Graph(Path, Result); CapturedGraphPath = Path; if (!C.Owner || !Hold(C.Owner) || !NativeType(C.Owner->GetClass())) return false;
            Package = C.Owner->GetPackage(); if (!Hold(Package)) return false; PackageFlags = Package->GetPackageFlags(); bExpectedDirty = C.bDirty = Package->IsDirty();
            Authored = C.Owner->GetNodes(); Input = C.Owner->GetInputNode(); if (!Hold(Input)) return false; Output = C.Owner->GetOutputNode(); if (!Hold(Output)) return false;
            TArray<UPCGNode*> All = Authored; All.Add(Input); All.Add(Output); TSet<UPCGNode*> Seen;
            for (UPCGNode* N : All) { if (!Hold(N) || Seen.Contains(N) || N->GetOuter() != C.Owner) return false; Seen.Add(N); }
            C.Member = Node(C.Owner, Id, Result); if (!C.Member) return false;
            // Capture all native memberships before the first virtual settings
            // getter. A returned value never becomes a new baseline.
            for (UPCGNode* N : All)
            {
                UPCGSettings* S = Cast<UPCGSettings>(SelectorObjectValue(N, TEXT("SettingsInterface"))); if (!Hold(S)) return false;
                if (C.Owner->GetNodes() != Authored || !FreshInterfaceMatches(N, S) || !NativeType(N->GetClass()) || !NativeType(S->GetClass())) return Fail(TEXT("pcg_selector_settings_current_invalid"), TEXT("Node-owned native settings changed during bind"));
                FNodeRow R; R.N = N; R.S = S; R.Title = N->GetAuthoredTitleName(); R.X = N->PositionX; R.Y = N->PositionY;
                auto AddPins = [&](const TArray<TObjectPtr<UPCGPin>>& Values, TArray<UPCGPin*>& Dest)
                {
                    if (Values.Num() > 256 || Pins.Num() + Values.Num() > 4096) return false;
                    for (const auto& H : Values) { if (!H.IsResolved()) return false; UPCGPin* P = H.Get(); if (!Hold(P) || !NativeType(P->GetClass()) || P->GetOuter() != N || !P->Node.IsResolved() || P->Node != N || Pins.ContainsByPredicate([&](const FPinRow& X) { return X.P == P; }) || P->Edges.Num() > 2048) return false; FPinRow PR{P, N, P->Properties, {}}; for (const auto& EH : P->Edges) { if (!EH.IsResolved() || !Hold(EH.Get()) || !NativeType(EH->GetClass())) return false; PR.Edges.Add(EH.Get()); } Pins.Add(MoveTemp(PR)); Dest.Add(P); } return true;
                };
                if (!AddPins(N->GetInputPins(), R.In) || !AddPins(N->GetOutputPins(), R.Out)) return false; Nodes.Add(MoveTemp(R));
                if (N == C.Member) C.Settings = S;
            }
            if (!Raw()) return false;
            for (const auto& N : Nodes)
            {
                UPCGSettings* Returned = N.N->GetSettings(); TStrongObjectPtr<UPCGSettings> KeepReturned(Returned);
                if (!Raw() || Returned != N.S) return Fail(TEXT("pcg_selector_settings_getter_changed"), TEXT("Virtual settings return differs from its pre-captured exact owned interface"));
            }
            for (const auto& P : Pins) for (UPCGEdge* E : P.Edges)
            {
                if (Edges.ContainsByPredicate([&](const FEdgeRow& R) { return R.E == E; })) continue;
                if (!E->InputPin.IsResolved() || !E->OutputPin.IsResolved()) return false; UPCGPin* A = E->InputPin.Get(); UPCGPin* B = E->OutputPin.Get();
                if (!Pins.ContainsByPredicate([&](const FPinRow& R) { return R.P == A; }) || !Pins.ContainsByPredicate([&](const FPinRow& R) { return R.P == B; }) || !A->Edges.Contains(E) || !B->Edges.Contains(E)) return false; Edges.Add({E, A, B});
            }
            UClass* SelectedClass=C.Settings->GetClass();
            if(SelectedClass->GetPathName()!=TEXT("/Script/PCG.")+SelectedClass->GetName()
                ||!SelectedClass->IsChildOf(UPCGSettings::StaticClass())||!SelectedClass->HasAllClassFlags(CLASS_Native)
                ||SelectedClass->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)||SelectedClass->IsEditorOnly())
                return Fail(TEXT("pcg_selector_assignment_unsupported"),TEXT("Requires an exact constructible native runtime /Script/PCG settings class; generated/custom settings remain incomplete"));
            for(const auto& N:Nodes)
            {
                UClass* Class=N.S->GetClass();
                if(Class->GetPathName()!=TEXT("/Script/PCG.")+Class->GetName())continue;
                // Discover concrete scalar selector roles from native reflection;
                // inherited fields keep their actual declaring class, not the
                // selected derived settings class as an invented owner.
                for(TFieldIterator<FProperty> It(Class);It;++It)
                {
                    FStructProperty* P=CastField<FStructProperty>(*It);
                    if(!P||!P->Struct.IsResolved())continue;
                    UScriptStruct* Type=P->Struct.Get();
                    const bool IsOutput=Type==FPCGAttributePropertyOutputSelector::StaticStruct();
                    if(!IsOutput&&Type!=FPCGAttributePropertyInputSelector::StaticStruct())continue;
                    UClass* Declaring=Cast<UClass>(P->GetOwnerStruct());
                    if(!Declaring||!Class->IsChildOf(Declaring)||Declaring->GetPathName()!=TEXT("/Script/PCG.")+Declaring->GetName()
                        ||P->ArrayDim!=1||P->HasSetterOrGetter()||P->HasAnyPropertyFlags(CPF_EditConst|CPF_Deprecated|CPF_Transient)
                        ||!P->HasAllPropertyFlags(CPF_Edit|CPF_BlueprintVisible)
                        ||P->GetSize()!=(IsOutput?sizeof(FPCGAttributePropertyOutputSelector):sizeof(FPCGAttributePropertyInputSelector)))
                    {
                        if(N.N==C.Member&&Token(P->GetName())==Token(Name))
                            return Fail(TEXT("pcg_selector_assignment_property_invalid"),TEXT("Selected scalar selector native declaration/editability/layout was not verified"));
                        continue;
                    }
                    if(!NativeType(Declaring)||!NativeType(Type))return false;
                    FSelectorRow R;R.S=N.S;R.P=P;R.Before.bOutput=R.Expected.bOutput=IsOutput;
                    R.Before.Copy(P->ContainerPtrToValuePtr<void>(N.S));R.Expected=R.Before;
                    if(N.N==C.Member&&Token(P->GetName())==Token(Name))
                    {if(Selected!=INDEX_NONE)return Fail(TEXT("pcg_selector_assignment_property_ambiguous"),TEXT("Selector property token matches multiple admitted scalar fields"));Selected=Selectors.Num();C.Property=P;C.Selector=P->ContainerPtrToValuePtr<FPCGAttributePropertySelector>(N.S);}
                    Selectors.Add(MoveTemp(R));
                }
            }
            if(Selected==INDEX_NONE||!NativeEnum(StaticEnum<EPCGAttributePropertySelection>())||!NativeEnum(StaticEnum<EPCGPointProperties>())||!NativeEnum(StaticEnum<EPCGExtraProperties>())||!NativeType(FPCGDataTypeIdentifier::StaticStruct()))
                return Fail(TEXT("pcg_selector_assignment_unsupported"),TEXT("Requires an editable native scalar InputSelector/OutputSelector property. Custom selector structs and selectors inside containers remain incomplete"));
            Result->SetStringField(TEXT("selector_assignment_scope"),TEXT("Reflected scalar native /Script/PCG InputSelector/OutputSelector roles, including actual native declaring ancestors; selected and admitted sibling selectors, graph identities/topology and existing finite settings fields are preserved. Arbitrary other settings values, custom/generated settings or selector structs, container selectors and runtime generation remain incomplete."));
            bReady = true; if (!Raw()) return false;
            for (auto& N : Nodes) if (SettingFields().Contains(N.S->GetClass()->GetName()))
            { FSettingContext V; V.Owner = C.Owner; V.Member = N.N; V.Settings = N.S; if (!FreshFields(V, N.Finite, [this] { return Raw(); })) return false; }
            auto CurrentLambda = [this] { return Raw(); }; TFunctionRef<bool()> Current(CurrentLambda);
            if (!CaptureGraphSnapshot(C.Owner, GraphBefore, Result, &Current) || !Values()) return false;
            return true;
        }
        bool Prepare(const FJson& Request, FSelectorValue& Prepared)
        {
            Prepared = Selectors[Selected].Before;
            const TArray<FString> Allowed{TEXT("selector_type"), TEXT("attribute_name"), TEXT("selected_property_name"), TEXT("domain_name"), TEXT("point_property"), TEXT("extra_property"), TEXT("extra_names"), TEXT("reset_extra_names")};
            if (!Request || Encode(Request).Len() > 65536) return Fail(TEXT("pcg_selector_request_invalid"), TEXT("Expected bounded selector request object"));
            for (const auto& P : Request->Values) if (!Allowed.ContainsByPredicate([&](const FString& K) { return K.Equals(FString(P.Key), ESearchCase::CaseSensitive); })) return Fail(TEXT("pcg_selector_request_invalid"), TEXT("Unknown selector request field"));
            bool Reset = true; if (Request->HasField(TEXT("reset_extra_names")) && !Request->TryGetBoolField(TEXT("reset_extra_names"), Reset)) return Fail(TEXT("pcg_selector_request_invalid"), TEXT("reset_extra_names must be boolean"));
            FString Kind, Attribute, Property, Domain, Point, Extra; FJson Error;
            if (!StringField(Request, TEXT("selector_type"), Kind, Error, true) || !StringField(Request, TEXT("attribute_name"), Attribute, Error) || !StringField(Request, TEXT("selected_property_name"), Property, Error) || !StringField(Request, TEXT("domain_name"), Domain, Error) || !StringField(Request, TEXT("point_property"), Point, Error) || !StringField(Request, TEXT("extra_property"), Extra, Error)) return Fail(TEXT("pcg_selector_request_invalid"), TEXT("Invalid selector string field"));
            if (!Values()) return false; auto& V = Prepared.Base();
            if (Request->HasField(TEXT("domain_name"))) { V.SetDomainName(FName(*Domain), false); if (!Values()) return false; }
            const FString K = Token(Kind);
            if (K == TEXT("attribute") && Request->HasField(TEXT("attribute_name"))) V.SetAttributeName(FName(*Attribute), Reset);
            else if (K == TEXT("property") && Request->HasField(TEXT("selected_property_name"))) V.SetPropertyName(FName(*Property), Reset);
            else if (K == TEXT("pointproperty") || K == TEXT("point")) { int64 E; if (!EnumValue(StaticEnum<EPCGPointProperties>(), Point, E) || !Values()) return Fail(TEXT("pcg_selector_enum_invalid"), TEXT("Unknown point property")); V.SetPointProperty(static_cast<EPCGPointProperties>(E), Reset); }
            else if (K == TEXT("extraproperty") || K == TEXT("extra")) { int64 E; if (!EnumValue(StaticEnum<EPCGExtraProperties>(), Extra, E) || !Values()) return Fail(TEXT("pcg_selector_enum_invalid"), TEXT("Unknown extra property")); V.SetExtraProperty(static_cast<EPCGExtraProperties>(E), Reset); }
            else return Fail(TEXT("pcg_selector_request_invalid"), TEXT("Selector type/required field is invalid"));
            if (!Values()) return false;
            if (Request->HasField(TEXT("extra_names")))
            { const TArray<TSharedPtr<FJsonValue>>* Items; if (!Request->TryGetArrayField(TEXT("extra_names"), Items) || Items->Num() > 64) return Fail(TEXT("pcg_selector_request_invalid"), TEXT("extra_names must contain at most 64 strings")); TArray<FString> Names; for (const auto& I : *Items) { FString S; if (!I->TryGetString(S) || !Bounded(S)) return Fail(TEXT("pcg_selector_request_invalid"), TEXT("Extra names must be nonempty bounded strings")); Names.Add(S); } V.GetExtraNamesMutable() = MoveTemp(Names); }
            const bool Valid = V.IsValid(); if (!Values()) return false; return Valid || Fail(TEXT("pcg_selector_invalid"), TEXT("Native public selector IsValid rejected the requested state"));
        }
        bool Assign(const FSelectorValue& Prepared)
        {
            if (!Values()) return false;
            Stage = TEXT("settings_modify"); Count(TEXT("target_modify_attempts")); const bool SM = C.Settings->Modify(); Count(TEXT("target_modify_returns")); Counts->SetBoolField(TEXT("settings_modify_engine_return"), SM);
            // UObjectBaseUtility.cpp:236-245: a Transient outer chain never
            // marks its package dirty. The persistent target follows Modify.
            bExpectedDirty = bExpectedDirty || !C.Owner->HasAnyFlags(RF_Transient); if (!Values()) return false;
            Stage = TEXT("graph_modify"); Count(TEXT("graph_modify_attempts")); const bool GM = C.Owner->Modify(); Count(TEXT("graph_modify_returns")); Counts->SetBoolField(TEXT("graph_modify_engine_return"), GM); if (!Values()) return false;
            Stage = TEXT("pre_edit_change"); Count(TEXT("pre_edit_attempts")); C.Settings->PreEditChange(C.Property); Count(TEXT("pre_edit_returns")); if (!Values()) return false;
            Stage = TEXT("selector_copy_complete_value"); Count(TEXT("assignment_attempts")); Selectors[Selected].Expected = Prepared;
            C.Property->CopyCompleteValue(C.Property->ContainerPtrToValuePtr<void>(C.Settings), Prepared.Address()); Count(TEXT("assignment_returns")); if (!Values()) return false;
            Stage = TEXT("post_edit_change_property"); Count(TEXT("post_edit_attempts")); FPropertyChangedEvent Event(C.Property, EPropertyChangeType::ValueSet); static_cast<UObject*>(C.Settings)->PostEditChangeProperty(Event); Count(TEXT("post_edit_returns"));
            return Values();
        }
    };

    bool SelectorAssignmentActive = false;
    FString AssignSelector(const FString& ParamsJson, bool Preflight)
    {
        FSelectorAssignmentScope W; FJson Carrier; const TCHAR* Op = Preflight ? TEXT("preflight_attribute_selector_assignment") : TEXT("commit_attribute_selector_assignment");
        auto Finish = [&](bool Success, bool Known)
        {
            FString ExistingCode; W.Result->TryGetStringField(TEXT("error_code"), ExistingCode);
            if (!Success && ExistingCode.IsEmpty()) W.Fail(TEXT("pcg_selector_assignment_incomplete"), TEXT("Native selector stage did not complete; retain exact prefix and do not retry"));
            if (!Success && !W.Result->HasField(TEXT("first_failure"))) { FJson First = MakeShared<FJsonObject>(); First->SetStringField(TEXT("stage"), W.Stage); First->SetStringField(TEXT("object"), W.CapturedGraphPath); First->SetStringField(TEXT("property"), ExistingCode); First->SetBoolField(TEXT("expected_actual_observed"), false); First->SetStringField(TEXT("expected"), TEXT("scoped admission")); First->SetStringField(TEXT("actual"), TEXT("not observed; original error retained without post-failure SDK")); W.Result->SetObjectField(TEXT("first_failure"), First); }
            W.Result->SetBoolField(TEXT("success"), Success); W.Result->SetStringField(TEXT("operation"), Op); W.Result->SetStringField(TEXT("native_route"), TEXT("finite_selector_public_copy_assignment_v1"));
            const bool Attempted = !Preflight && W.Counts->GetIntegerField(TEXT("target_modify_attempts")) > 0;
            W.Result->SetBoolField(TEXT("mutation_attempted"), Attempted); W.Result->SetBoolField(TEXT("change_state_known"), Known); W.Result->SetBoolField(TEXT("outcome_unknown"), !Known);
            if (!Known) W.Result->SetField(TEXT("mutation_performed"), MakeShared<FJsonValueNull>()); else W.Result->SetBoolField(TEXT("mutation_performed"), Attempted);
            W.Result->SetBoolField(TEXT("operation_complete"), Success); W.Result->SetBoolField(TEXT("postcondition_passed"), Success); W.Result->SetBoolField(TEXT("observation_complete"), Known);
            W.Result->SetBoolField(TEXT("compile_attempted"), false); W.Result->SetBoolField(TEXT("compiled"), false); W.Result->SetBoolField(TEXT("saved"), false);
            W.Result->SetStringField(TEXT("stage"), Success ? TEXT("complete") : *W.Stage);
            W.Result->SetBoolField(TEXT("object_construction_attempted"), Preflight && W.Result->HasField(TEXT("transient_graph_construction_attempted")));
            if (Preflight) { W.Result->SetObjectField(TEXT("transient_assignment_prefix"), W.Counts); for (const TCHAR* K : {TEXT("target_modify_attempts"), TEXT("target_modify_returns"), TEXT("graph_modify_attempts"), TEXT("graph_modify_returns"), TEXT("pre_edit_attempts"), TEXT("pre_edit_returns"), TEXT("assignment_attempts"), TEXT("assignment_returns"), TEXT("post_edit_attempts"), TEXT("post_edit_returns")}) W.Result->SetNumberField(K, 0); }
            else for (const auto& P : W.Counts->Values) W.Result->SetField(FString(P.Key), P.Value);
            return Encode(W.Result);
        };
        if (!SelectorCarrier(ParamsJson, Carrier)) { W.Fail(TEXT("pcg_selector_carrier_invalid"), TEXT("Expected exact bounded carrier without duplicate keys, NUL or nonfinite values")); return Finish(false, true); }
        if (!SelectorSafe() || GEditor->PlayWorld || SelectorAssignmentActive) { W.Fail(TEXT("pcg_selector_assignment_busy"), TEXT("Requires safe idle editor game thread and no reentrant selector native assignment")); return Finish(false, true); }
        struct FActive { FActive() { SelectorAssignmentActive = true; } ~FActive() { SelectorAssignmentActive = false; } } Active;
        FString Path, NodeId, Name; const FJson* Request = nullptr; const FJson* Expected = nullptr;
        if (!Carrier->TryGetStringField(TEXT("graph_path"), Path) || !Bounded(Path) || !Carrier->TryGetStringField(TEXT("node_id"), NodeId) || !Bounded(NodeId) || !Carrier->TryGetStringField(TEXT("selector_property_name"), Name) || !Bounded(Name) || !Carrier->TryGetObjectField(TEXT("request"), Request) || !Carrier->TryGetObjectField(TEXT("expected_preview"), Expected)) { W.Fail(TEXT("pcg_selector_carrier_invalid"), TEXT("Carrier has invalid value kinds/bounds")); return Finish(false, true); }
        if (!W.Bind(Path, NodeId, Name)) return Finish(false, !W.bFailed && !W.C.Owner);
        FSelectorValue Prepared; if (!W.Prepare(*Request, Prepared)) return Finish(false, !W.bUnsafe);
        FJson BeforeState, PreviewState; if (!W.State(W.Selectors[W.Selected].Before, BeforeState) || !W.State(Prepared, PreviewState)) return Finish(false, false);
        FJson Preview = W.IdentityValue(); Preview->SetObjectField(TEXT("current_selector"), BeforeState); Preview->SetObjectField(TEXT("preview_selector"), PreviewState); Preview->SetBoolField(TEXT("changed"), !W.Selectors[W.Selected].Before.Base().IsSame(Prepared.Base())); Preview->SetBoolField(TEXT("native_copy_preview"), true);
        if (!W.Values()) return Finish(false, false);
        if (!SelectorJsonEqual(*Expected, Preview)) { W.Fail(TEXT("pcg_selector_preview_stale"), TEXT("Caller expectation differs from independent current native preview")); return Finish(false, true); }
        W.Result->SetObjectField(TEXT("before"), W.IdentityValue()); W.Result->GetObjectField(TEXT("before"))->SetObjectField(TEXT("current_selector"), BeforeState);
        if (!Preflight)
        {
            const bool Changed = Preview->GetBoolField(TEXT("changed")); if (Changed && !W.Assign(Prepared)) return Finish(false, false);
            FJson GraphAfter; auto CurrentLambda = [&W] { return W.Raw(); }; TFunctionRef<bool()> Current(CurrentLambda);
            if (!CaptureGraphSnapshot(W.C.Owner, GraphAfter, W.Result, &Current) || !W.Values()) return Finish(false, false);
            if (!SelectorJsonEqual(W.GraphBefore, GraphAfter)) { W.Fail(TEXT("pcg_selector_graph_postcondition_failed"), TEXT("Public selector notify changed existing graph snapshot; actual state retained")); return Finish(false, true); }
            FJson Actual; if (!W.State(W.Selectors[W.Selected].Expected, Actual)) return Finish(false, false);
            FJson After = W.IdentityValue(); After->SetObjectField(TEXT("current_selector"), Actual); W.Result->SetObjectField(TEXT("after"), After);
            W.Result->SetBoolField(TEXT("public_selector_copy_assignment"), true); W.Result->SetBoolField(TEXT("toolset_native_converter_dependency"), false); W.Result->SetBoolField(TEXT("toolset_execute_tool_dependency"), false); W.Result->SetBoolField(TEXT("source_unchanged_except_selected_selector_and_dirty"), true);
            return Finish(true, true);
        }
        W.Stage = TEXT("transient_graph_new"); if (!W.Values()) return Finish(false, false);
        W.Result->SetBoolField(TEXT("transient_graph_construction_attempted"), true);
        W.Result->SetNumberField(TEXT("transient_graph_constructor_attempts"), 1);
        TStrongObjectPtr<UPCGGraph> Scratch(NewObject<UPCGGraph>(GetTransientPackage(), NAME_None, RF_Transient));
        W.Result->SetNumberField(TEXT("transient_graph_constructor_returns"), 1);
        if (!W.Values()) return Finish(false, false);
        if (!ObservedObject(Scratch.Get()) || Scratch->GetOuter() != GetTransientPackage() || Scratch->GetClass() != UPCGGraph::StaticClass() || !Scratch->HasAnyFlags(RF_Transient)) { W.Fail(TEXT("pcg_selector_scratch_graph_invalid"), TEXT("Transient PCGGraph construction returned an invalid role")); return Finish(false, false); }
        W.Stage = TEXT("transient_node_add"); UPCGSettings* ScratchSettings = nullptr;
        W.Result->SetNumberField(TEXT("transient_node_constructor_attempts"), 1);
        UPCGNode* ScratchNode = Scratch->AddNodeOfType(W.C.Settings->GetClass(), ScratchSettings); TStrongObjectPtr<UPCGNode> KeepNode(ScratchNode); TStrongObjectPtr<UPCGSettings> KeepSettings(ScratchSettings);
        W.Result->SetNumberField(TEXT("transient_node_constructor_returns"), 1);
        if (!W.Values()) return Finish(false, false);
        if (!ObservedObject(ScratchNode) || !ObservedObject(ScratchSettings) || !Scratch->GetNodes().Contains(ScratchNode) || ScratchNode->GetOuter() != Scratch.Get() || ScratchSettings->GetOuter() != ScratchNode || ScratchSettings->GetClass() != W.C.Settings->GetClass() || !FreshInterfaceMatches(ScratchNode, ScratchSettings)) { W.Fail(TEXT("pcg_selector_scratch_node_invalid"), TEXT("Scratch node/settings do not match the exact admitted source class")); return Finish(false, false); }
        FStructProperty* ScratchProperty = FindFProperty<FStructProperty>(ScratchSettings->GetClass(), W.C.Property->GetFName());
        if (ScratchProperty != W.C.Property || !W.Values()) return Finish(false, false);
        // Seed by the complete concrete struct, then bind an independent scope.
        W.Result->SetNumberField(TEXT("transient_seed_copy_attempts"), 1); ScratchProperty->CopyCompleteValue(ScratchProperty->ContainerPtrToValuePtr<void>(ScratchSettings), W.Selectors[W.Selected].Before.Address()); W.Result->SetNumberField(TEXT("transient_seed_copy_returns"), 1);
        if (!W.Values()) return Finish(false, false);
        FSelectorAssignmentScope SW; SW.SourceScope = &W; if (!SW.Bind(Scratch->GetPathName(), ScratchNode->GetPathName(), Name)) { SW.Result->SetObjectField(TEXT("assignment_prefix"), SW.Counts); W.Result->SetObjectField(TEXT("transient_failure"), SW.Result); W.Fail(TEXT("pcg_selector_scratch_bind_failed"), TEXT("Transient same-class selector scope could not be verified")); return Finish(false, false); }
        FSelectorValue ScratchPrepared; if (!SW.Prepare(*Request, ScratchPrepared) || !SW.Assign(ScratchPrepared)) { SW.Result->SetObjectField(TEXT("assignment_prefix"), SW.Counts); W.Result->SetObjectField(TEXT("transient_failure"), SW.Result); W.Counts = SW.Counts; W.Fail(TEXT("pcg_selector_scratch_assignment_failed"), TEXT("Actual transient public assignment did not complete; scratch prefix retained")); return Finish(false, false); }
        W.Counts = SW.Counts; if (!W.Values() || !SW.Values()) return Finish(false, false);
        FJson Actual; if (!SW.State(SW.Selectors[SW.Selected].Expected, Actual) || !W.Values()) return Finish(false, false);
        if (!SelectorJsonEqual(Actual, PreviewState)) { W.Fail(TEXT("pcg_selector_scratch_postcondition_failed"), TEXT("Actual public scratch assignment differs from independent preview")); return Finish(false, true); }
        W.Result->SetBoolField(TEXT("transient_public_assignment_verified"), true); W.Result->SetBoolField(TEXT("converter_dependency"), false); W.Result->SetObjectField(TEXT("preview_state"), Actual); W.Result->SetBoolField(TEXT("source_unchanged"), true);
        FJson Admission = Outcome(true); Admission->SetStringField(TEXT("class_path"), W.C.Settings->GetClass()->GetPathName()); Admission->SetBoolField(TEXT("constructible"), true); Admission->SetBoolField(TEXT("native_runtime_settings_verified"), true); W.Result->SetObjectField(TEXT("settings_class_admission"), Admission);
        return Finish(true, true);
    }
}

FString CubelessPCGUE58::PreflightAttributeSelectorAssignment(const FString& ParamsJson)
{ return AssignSelector(ParamsJson, true); }
FString CubelessPCGUE58::CommitAttributeSelectorAssignment(const FString& ParamsJson)
{ return AssignSelector(ParamsJson, false); }
