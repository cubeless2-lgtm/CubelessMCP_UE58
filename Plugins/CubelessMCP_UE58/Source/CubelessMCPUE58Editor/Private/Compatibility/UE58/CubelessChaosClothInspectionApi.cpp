// Dedicated UE5.8.3 Chaos Cloth observers. No server, bridge or generic executor.
#include "Compatibility/UE58/CubelessChaosClothInspectionApi.h"
#include "Compatibility/UE58/CubelessChaosClothAuthoringApi.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothAssetBase.h"
#include "ChaosClothAsset/ClothSimulationModel.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/WeightMapNode.h"
#include "Dataflow/DataflowGraph.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Dataflow/DataflowNode.h"
#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowObjectInterface.h"
#include "Dataflow/DataflowToolNode.h"
#include "Dataflow/DataflowVertexAttributeEditableNode.h"
#include "Editor.h"
#include "Misc/Crc.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::ChaosClothInspection
{
namespace
{
using Variable::FResult;
constexpr int32 MaxJsonChars=1024*1024,MaxDepth=16,MaxSlots=262144,MaxGraphNodes=16384,MaxModels=1024,MaxLods=1024,MaxMeshElements=32000000,MaxSnapshotBytes=64*1024*1024,MaxProjectionChars=4*1024*1024;
bool Running=false;
bool Live(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
FString Json(const TSharedRef<FJsonObject>& J) { FString S;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&S));return S; }
bool Text(const FString& S,int32 Max=MaxJsonChars) { if(S.Len()>Max) { return false; }for(TCHAR C:S) { if(C==0) { return false; } }return true; }
struct FExactKeyFuncs:DefaultKeyFuncs<FString>
{ static bool Matches(const FString& A,const FString& B) { return A.Equals(B,ESearchCase::CaseSensitive); }static uint32 GetKeyHash(const FString& V) { return FCrc::StrCrc32(*V); } };
bool Parse(const FString& S,TSharedPtr<FJsonObject>& Out)
{
    if(S.IsEmpty()||!Text(S)) { return false; }auto Reader=TJsonReaderFactory<>::Create(S);TArray<bool> Objects;TArray<TSet<FString,FExactKeyFuncs>> Keys;EJsonNotation Token;int32 Slots=0;
    while(Reader->ReadNext(Token))
    {
        if(Token==EJsonNotation::Error||++Slots>MaxSlots) { return false; }
        if(Token!=EJsonNotation::ObjectEnd&&Token!=EJsonNotation::ArrayEnd&&!Objects.IsEmpty()&&Objects.Last()) { FString K(Reader->GetIdentifier());if(Keys.Last().Contains(K)) { return false; }Keys.Last().Add(K); }
        if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart) { Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();if(Objects.Num()>MaxDepth) { return false; } }
        else if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd) { if(Objects.IsEmpty()) { return false; }Objects.Pop();Keys.Pop(); }
    }
    return Objects.IsEmpty()&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S),Out)&&Out.IsValid();
}
bool Dependency(const FString& Input)
{
    FString S=FPackageName::ExportTextPathToObjectPath(Input).TrimStartAndEnd();S.TrimQuotesInline();S.ReplaceInline(TEXT("\\"),TEXT("/"));
    for(const TCHAR* K:{TEXT("/Script/UnrealMCP"),TEXT("/UnrealMCP"),TEXT("UnrealMCP"),TEXT("MCPUnreal"),TEXT("mcp_unreal"),TEXT("CubelessControlRigTools"),TEXT("CubelessMCP_UE58"),TEXT("CubelessMCPUE58")}) { if(S.Contains(K,ESearchCase::IgnoreCase)) { return true; } }return false;
}
TArray<TSharedPtr<FJsonValue>> StringsToJson(const TArray<FString>& Values) { TArray<TSharedPtr<FJsonValue>> A;for(const auto& S:Values) { A.Add(MakeShared<FJsonValueString>(S)); }return A; }
TArray<TSharedPtr<FJsonValue>> Vector3fToJson(const FVector3f& V) { return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)}; }
bool FiniteJson(const TSharedPtr<FJsonValue>& V,int32 Depth=0)
{
    if(!V.IsValid()||Depth>MaxDepth) { return false; }if(V->Type==EJson::Number) { return FMath::IsFinite(V->AsNumber()); }if(V->Type==EJson::String) { return Text(V->AsString()); }if(V->Type==EJson::Array) { for(const auto& Row:V->AsArray()) { if(!FiniteJson(Row,Depth+1)) { return false; } } }else if(V->Type==EJson::Object) { for(const auto& Row:V->AsObject()->Values) { if(!Text(FString(Row.Key))||!FiniteJson(Row.Value,Depth+1)) { return false; } } }return true;
}
bool FiniteObject(const TSharedRef<FJsonObject>& V) { return FiniteJson(MakeShared<FJsonValueObject>(V)); }
struct FWork
{
    FResult R;FString Command,FailedStage;TSharedPtr<FJsonObject> Params;
    TSharedRef<FJsonObject> Legacy=MakeShared<FJsonObject>(),Calls=MakeShared<FJsonObject>(),Checks=MakeShared<FJsonObject>(),Prefix=MakeShared<FJsonObject>();
    TStrongObjectPtr<UChaosClothAssetBase> Asset{nullptr};TSharedPtr<FJsonObject> Before;int64 ProjectionChars=0;
    explicit FWork(const TCHAR* Name):Command(Name)
    {
        R.Compiled=false;Legacy->SetBoolField(TEXT("success"),false);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.Data->SetObjectField(TEXT("native_call_counts"),Calls);R.Data->SetObjectField(TEXT("postconditions"),Checks);R.Data->SetObjectField(TEXT("actual_observation_prefix"),Prefix);
        R.Data->SetBoolField(TEXT("direct_observer_API"),true);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("explicit_wrapper_wait_performed"),false);R.Data->SetBoolField(TEXT("public_SDK_getters_may_synchronously_wait"),true);R.Data->SetBoolField(TEXT("Dataflow_upstream_evaluation_may_update_cache"),true);R.Data->SetBoolField(TEXT("whole_external_pipeline_complete"),false);R.Data->SetBoolField(TEXT("supported_for_mcp_write_means_original_type_eligibility_not_installed_writer"),true);R.Data->SetBoolField(TEXT("snapshot_hash_durable_roundtrip_verified"),false);R.Data->SetBoolField(TEXT("effective_runtime_simulation_verified"),false);
        R.Data->SetStringField(TEXT("synchronous_limit"),TEXT("LoadObject/PostLoad and const cloth async-property getters may wait internally; upstream Dataflow GetValue evaluates graph/cache. This wrapper adds no forced compile/flush/wait and cannot promise nonblocking execution or global SDK purity."));
        R.Data->SetStringField(TEXT("state_58_contract"),TEXT("Legacy serialized VertexWeights, current snapshot-backed editable state, effective evaluated weights and compiled terminal weights are reported separately. UE58 snapshot state hash is versioned; no old-array-only equality claim."));
        for(const TCHAR* K:{TEXT("load_calls"),TEXT("load_returns"),TEXT("model_getter_calls"),TEXT("model_getter_returns"),TEXT("collection_getter_calls"),TEXT("collection_getter_returns"),TEXT("Dataflow_input_evaluation_calls"),TEXT("Dataflow_input_evaluation_returns"),TEXT("current_weight_evaluation_calls"),TEXT("current_weight_evaluation_returns"),TEXT("snapshot_serialization_calls"),TEXT("snapshot_serialization_returns"),TEXT("collection_mesh_build_calls"),TEXT("collection_mesh_build_returns"),TEXT("property_export_calls"),TEXT("property_export_returns"),TEXT("explicit_modify_calls"),TEXT("explicit_notify_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls"),TEXT("explicit_wrapper_wait_calls")}) { Calls->SetNumberField(K,0); }
        auto Limits=MakeShared<FJsonObject>();Limits->SetNumberField(TEXT("JSON_UTF16_chars"),MaxJsonChars);Limits->SetNumberField(TEXT("JSON_depth"),MaxDepth);Limits->SetNumberField(TEXT("graph_nodes"),MaxGraphNodes);Limits->SetNumberField(TEXT("models"),MaxModels);Limits->SetNumberField(TEXT("lods_per_model"),MaxLods);Limits->SetNumberField(TEXT("full_mesh_elements"),MaxMeshElements);Limits->SetNumberField(TEXT("snapshot_serialization_bytes"),MaxSnapshotBytes);Limits->SetNumberField(TEXT("projection_UTF16_char_budget"),MaxProjectionChars);R.Data->SetObjectField(TEXT("health_limits"),Limits);
    }
    void Count(const TCHAR* K) { double N=0;Calls->TryGetNumberField(K,N);Calls->SetNumberField(K,N+1); }
    bool Fail(const TCHAR* Code,const FString& Error) { if(R.Error.IsEmpty()) { R.ErrorCode=TEXT("chaos_cloth_inspection_")+FString(Code);R.Error=Error;FailedStage=R.Stage;R.Data->SetStringField(TEXT("failed_stage"),FailedStage); }R.bSuccess=false;Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("error"),R.Error);return false; }
    bool Budget(int64 N) { ProjectionChars+=N;return ProjectionChars<=MaxProjectionChars||Fail(TEXT("projection_budget"),TEXT("Bounded response projection budget exceeded; completed observation prefix retained")); }
    bool Enter(const FString& S)
    {
        R.Stage=TEXT("parse_parameters");if(!Parse(S,Params)) { return Fail(TEXT("params_invalid"),TEXT("Bounded unambiguous JSON object required")); }
        if(!IsInGameThread()||!GEditor||Running) { return Fail(TEXT("editor_required"),TEXT("Non-reentrant Editor game-thread inspection required")); }return true;
    }
    void SDK(const TCHAR* Stage) { R.Stage=Stage;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true); }
    TSharedRef<FJsonObject> Projection() const
    {
        auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("asset_live"),Live(Asset.Get()));if(Live(Asset.Get())) { J->SetStringField(TEXT("path"),Asset->GetPathName());J->SetStringField(TEXT("class"),Asset->GetClass()->GetPathName());J->SetBoolField(TEXT("package_dirty"),Asset->GetPackage()->IsDirty());J->SetStringField(TEXT("package"),Asset->GetPackage()->GetName()); }return J;
    }
    bool Admit() { return Live(Asset.Get())||Fail(TEXT("asset_lost"),TEXT("Captured cloth asset is no longer a live loaded target")); }
    bool Load(const FString& Path)
    {
        if(Path.IsEmpty()) { return Fail(TEXT("asset_path_required"),TEXT("cloth_asset_path is required")); }if(Dependency(Path)) { return Fail(TEXT("dependency_reference"),TEXT("MCP-only dependency reference rejected in cloth_asset_path")); }
        SDK(TEXT("load_cloth_asset"));Count(TEXT("load_calls"));Asset.Reset(LoadObject<UChaosClothAssetBase>(nullptr,*Path));Count(TEXT("load_returns"));Prefix->SetBoolField(TEXT("asset_load_returned"),true);if(!Live(Asset.Get())) { return Fail(TEXT("asset_load_failed"),FString::Printf(TEXT("Unable to load a UChaosClothAssetBase at '%s'"),*Path)); }Before=Projection();R.Data->SetObjectField(TEXT("target_before"),Before.ToSharedRef());return true;
    }
    FString Finish()
    {
        if(Asset.Get()) { auto After=Projection();R.Data->SetObjectField(TEXT("target_after"),After);const bool Changed=Before.IsValid()&&!Json(Before.ToSharedRef()).Equals(Json(After),ESearchCase::CaseSensitive);R.bChanged=Changed;Checks->SetBoolField(TEXT("captured_asset_projection_unchanged"),!Changed);if(Changed&&R.Error.IsEmpty()) { Fail(TEXT("observer_projection_changed"),TEXT("Captured asset identity/dirty projection changed during observation")); } }
        R.bPostconditionPassed=R.Error.IsEmpty()&&Legacy->GetBoolField(TEXT("success"));R.bSuccess=R.bPostconditionPassed;
        Checks->SetBoolField(TEXT("explicit_writer_compile_save_calls_zero"),!R.bMutationAttempted&&!R.bCompileAttempted&&!R.bSaveAttempted);R.Data->SetBoolField(TEXT("observed_projection_changed"),R.bChanged);R.Data->SetBoolField(TEXT("global_change_state_unknown"),!R.bChangeStateKnown);Prefix->SetNumberField(TEXT("projection_char_budget_used"),ProjectionChars);if(R.bSuccess) { R.Stage=TEXT("complete"); }else if(!FailedStage.IsEmpty()) { R.Stage=FailedStage; }
        return Cubeless::Operations::EncodeVariableResult(R,*Command);
    }
};
struct FJsonParamParseState
{
    void AddError(const FString& Error)
    {
        Errors.Add(Error);
    }

    bool IsValid() const
    {
        return Errors.IsEmpty();
    }

    FString GetErrorMessage() const
    {
        return FString::Printf(TEXT("Invalid Chaos tuning parameters: %s"), *FString::Join(Errors, TEXT("; ")));
    }

private:
    TArray<FString> Errors;
};

FString GetStringParam(
    const TSharedPtr<FJsonObject>& Params,
    const TCHAR* Name,
    FJsonParamParseState& ParseState,
    const FString& Default = FString())
{
    if (!Params.IsValid())
    {
        return Default;
    }
    const TSharedPtr<FJsonValue>* JsonValue = Params->Values.Find(Name);
    if (!JsonValue)
    {
        return Default;
    }
    FString Value;
    if (!JsonValue->IsValid() || (*JsonValue)->Type != EJson::String || !(*JsonValue)->TryGetString(Value))
    {
        ParseState.AddError(FString::Printf(TEXT("%s must be a JSON string"), Name));
        return Default;
    }
    return Value;
}

bool GetBoolParam(
    const TSharedPtr<FJsonObject>& Params,
    const TCHAR* Name,
    bool bDefault,
    FJsonParamParseState& ParseState)
{
    if (!Params.IsValid())
    {
        return bDefault;
    }
    const TSharedPtr<FJsonValue>* JsonValue = Params->Values.Find(Name);
    if (!JsonValue)
    {
        return bDefault;
    }
    bool bValue = bDefault;
    if (!JsonValue->IsValid() || (*JsonValue)->Type != EJson::Boolean || !(*JsonValue)->TryGetBool(bValue))
    {
        ParseState.AddError(FString::Printf(TEXT("%s must be a JSON boolean"), Name));
        return bDefault;
    }
    return bValue;
}

int32 GetIntParam(
    const TSharedPtr<FJsonObject>& Params,
    const TCHAR* Name,
    int32 Default,
    FJsonParamParseState& ParseState)
{
    if (!Params.IsValid())
    {
        return Default;
    }
    const TSharedPtr<FJsonValue>* JsonValue = Params->Values.Find(Name);
    if (!JsonValue)
    {
        return Default;
    }
    double Value = 0.0;
    if (!JsonValue->IsValid() || (*JsonValue)->Type != EJson::Number ||
        !(*JsonValue)->TryGetNumber(Value) || !FMath::IsFinite(Value) ||
        Value != FMath::TruncToDouble(Value) ||
        Value < static_cast<double>(TNumericLimits<int32>::Min()) ||
        Value > static_cast<double>(TNumericLimits<int32>::Max()))
    {
        ParseState.AddError(FString::Printf(
            TEXT("%s must be a JSON number containing a 32-bit signed integer"),
            Name));
        return Default;
    }
    return static_cast<int32>(Value);
}

TSharedPtr<FJsonObject> MakePropertyFlagsSnapshot(
    const Chaos::Softs::FCollectionPropertyConstFacade& Facade,
    int32 PropertyIndex)
{
    TSharedPtr<FJsonObject> Flags = MakeShared<FJsonObject>();
    Flags->SetNumberField(TEXT("raw"), static_cast<uint8>(Facade.GetFlags(PropertyIndex)));
    Flags->SetBoolField(TEXT("enabled"), Facade.IsEnabled(PropertyIndex));
    Flags->SetBoolField(TEXT("animatable"), Facade.IsAnimatable(PropertyIndex));
    Flags->SetBoolField(TEXT("legacy"), Facade.IsLegacy(PropertyIndex));
    Flags->SetBoolField(TEXT("interpolable"), Facade.IsInterpolable(PropertyIndex));
    Flags->SetBoolField(TEXT("intrinsic"), Facade.IsIntrinsic(PropertyIndex));
    Flags->SetBoolField(TEXT("dirty"), Facade.IsDirty(PropertyIndex));
    Flags->SetBoolField(TEXT("string_dirty"), Facade.IsStringDirty(PropertyIndex));
    return Flags;
}

TSharedPtr<FJsonObject> MakeCollectionPropertySnapshot(
    const Chaos::Softs::FCollectionPropertyConstFacade& Facade,
    int32 PropertyIndex,
    bool bIncludeValues,
    bool bIncludeFlags)
{
    TSharedPtr<FJsonObject> Property = MakeShared<FJsonObject>();
    Property->SetStringField(TEXT("name"), Facade.GetKeyName(PropertyIndex).ToString());
    if (bIncludeValues)
    {
        Property->SetArrayField(
            TEXT("low_raw_vector"),
            Vector3fToJson(Facade.GetLowValue<FVector3f>(PropertyIndex)));
        Property->SetArrayField(
            TEXT("high_raw_vector"),
            Vector3fToJson(Facade.GetHighValue<FVector3f>(PropertyIndex)));
        Property->SetStringField(TEXT("string_value"), Facade.GetStringValue(PropertyIndex));
        Property->SetStringField(
            TEXT("value_type_contract"),
            TEXT("UE 5.8 collection properties store numeric values as raw FVector3f; the semantic type is defined by the consuming solver key"));
    }
    if (bIncludeFlags)
    {
        Property->SetObjectField(TEXT("flags"), MakePropertyFlagsSnapshot(Facade, PropertyIndex));
    }
    return Property;
}

bool IsSupportedDataflowProperty(const FProperty* Property)
{
    if (!Property ||
        Property->GetOwnerStruct() == FDataflowNode::StaticStruct() ||
        !Property->HasAnyPropertyFlags(CPF_Edit) ||
        Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated | CPF_DisableEditOnInstance))
    {
        return false;
    }

    return CastField<FBoolProperty>(Property) ||
        CastField<FNumericProperty>(Property) ||
        CastField<FEnumProperty>(Property) ||
        CastField<FByteProperty>(Property) ||
        CastField<FStrProperty>(Property) ||
        CastField<FNameProperty>(Property) ||
        CastField<FStructProperty>(Property);
}

FString ExportDataflowProperty(
    FWork& W, const FDataflowNode& Node,
    const FProperty& Property)
{
    FString Exported;
    const void* ValuePtr = Property.ContainerPtrToValuePtr<void>(&Node);
    W.Count(TEXT("property_export_calls"));Property.ExportTextItem_Direct(Exported, ValuePtr, nullptr, nullptr, PPF_None);W.Count(TEXT("property_export_returns"));
    return Exported;
}

bool IsClothDataflowNode(const FDataflowNode& Node)
{
    const UScriptStruct* Struct = Node.TypedScriptStruct();
    return Struct &&
        (Struct->HasMetaData(TEXT("DataflowCloth")) ||
         Struct->GetName().StartsWith(TEXT("ChaosClothAsset")) ||
         Node.GetCategory().ToString().Contains(TEXT("Cloth"), ESearchCase::IgnoreCase));
}

TArray<FString> GetConnectedDataflowInputPaths(
    const FDataflowNode& Node,
    const FProperty& TopLevelProperty)
{
    TArray<FString> Result;
    const FString TopLevelName = TopLevelProperty.GetName();
    const FString StructDescendantPrefix = TopLevelName + TEXT(".");
    const FString ArrayDescendantPrefix = TopLevelName + TEXT("[");
    for (const FDataflowInput* Input : Node.GetInputs())
    {
        if (!Input || !Input->HasAnyConnections())
        {
            continue;
        }
        const FString InputPath = Input->GetName().ToString();
        if (InputPath.Equals(TopLevelName, ESearchCase::IgnoreCase) ||
            InputPath.StartsWith(StructDescendantPrefix, ESearchCase::IgnoreCase) ||
            InputPath.StartsWith(ArrayDescendantPrefix, ESearchCase::IgnoreCase))
        {
            Result.AddUnique(InputPath);
        }
    }
    Result.Sort();
    return Result;
}

TSharedPtr<FJsonObject> MakeDataflowPropertySnapshot(
    FWork& W, const FDataflowNode& Node,
    const FProperty& Property)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), Property.GetName());
    Result->SetStringField(TEXT("cpp_type"), Property.GetCPPType());
    Result->SetStringField(TEXT("display_name"), Property.GetDisplayNameText().ToString());
    Result->SetStringField(TEXT("category"), Property.GetMetaData(TEXT("Category")));
    Result->SetStringField(TEXT("tooltip"), Property.GetToolTipText().ToString());
    Result->SetStringField(TEXT("export_text"), ExportDataflowProperty(W, Node, Property));
    Result->SetBoolField(TEXT("supported_for_mcp_write"), IsSupportedDataflowProperty(&Property));
    const TArray<FString> ConnectedInputPaths =
        GetConnectedDataflowInputPaths(Node, Property);
    Result->SetBoolField(TEXT("input_connected"), !ConnectedInputPaths.IsEmpty());
    Result->SetArrayField(
        TEXT("connected_input_paths"),
        StringsToJson(ConnectedInputPaths));

    TSharedPtr<FJsonObject> Metadata = MakeShared<FJsonObject>();
    static const TCHAR* MetadataNames[] = {
        TEXT("ClampMin"), TEXT("ClampMax"), TEXT("UIMin"), TEXT("UIMax"),
        TEXT("EditCondition"), TEXT("InteractorName"), TEXT("Units")};
    for (const TCHAR* MetadataName : MetadataNames)
    {
        const FString Value = Property.GetMetaData(MetadataName);
        if (!Value.IsEmpty())
        {
            Metadata->SetStringField(MetadataName, Value);
        }
    }
    Result->SetObjectField(TEXT("metadata"), Metadata);
    return Result;
}

FString HashFloatValues(const TConstArrayView<float> Values)
{
    FMD5 Md5;
    const int32 Count = Values.Num();
    Md5.Update(reinterpret_cast<const uint8*>(&Count), sizeof(Count));
    if (!Values.IsEmpty())
    {
        Md5.Update(
            reinterpret_cast<const uint8*>(Values.GetData()),
            static_cast<uint32>(Values.Num() * sizeof(float)));
    }
    uint8 Digest[16];
    Md5.Final(Digest);
    return BytesToHex(Digest, UE_ARRAY_COUNT(Digest)).ToLower();
}

void UpdateMd5WithString(FMD5& Md5, const FString& Value)
{
    const FTCHARToUTF8 Utf8(*Value);
    const int32 ByteCount = Utf8.Length();
    Md5.Update(reinterpret_cast<const uint8*>(&ByteCount), sizeof(ByteCount));
    if (ByteCount > 0)
    {
        Md5.Update(reinterpret_cast<const uint8*>(Utf8.Get()), ByteCount);
    }
}

FString HashSimulationTopology(
    const TConstArrayView<FVector3f> Positions,
    const TConstArrayView<uint32> Indices)
{
    FMD5 Md5;
    const int32 PositionCount = Positions.Num();
    const int32 IndexCount = Indices.Num();
    Md5.Update(reinterpret_cast<const uint8*>(&PositionCount), sizeof(PositionCount));
    if (!Positions.IsEmpty())
    {
        Md5.Update(
            reinterpret_cast<const uint8*>(Positions.GetData()),
            static_cast<uint32>(Positions.Num() * sizeof(FVector3f)));
    }
    Md5.Update(reinterpret_cast<const uint8*>(&IndexCount), sizeof(IndexCount));
    if (!Indices.IsEmpty())
    {
        Md5.Update(
            reinterpret_cast<const uint8*>(Indices.GetData()),
            static_cast<uint32>(Indices.Num() * sizeof(uint32)));
    }
    uint8 Digest[16];
    Md5.Final(Digest);
    return BytesToHex(Digest, UE_ARRAY_COUNT(Digest)).ToLower();
}

bool ReadFloatArrayProperty(
    FWork& W, const FDataflowNode& Node,
    const FName PropertyName,
    TArray<float>& OutValues)
{
    const UScriptStruct* const Struct = Node.TypedScriptStruct();
    const FArrayProperty* const ArrayProperty = Struct
        ? FindFProperty<FArrayProperty>(Struct, PropertyName)
        : nullptr;
    const FNumericProperty* const InnerProperty = ArrayProperty
        ? CastField<FNumericProperty>(ArrayProperty->Inner)
        : nullptr;
    if (!ArrayProperty || !InnerProperty || !InnerProperty->IsFloatingPoint())
    {
        return false;
    }

    const void* const ArrayPtr = ArrayProperty->ContainerPtrToValuePtr<void>(&Node);
    FScriptArrayHelper ArrayHelper(
        ArrayProperty,
        ArrayPtr);
    if(ArrayHelper.Num()>MaxMeshElements) { return W.Fail(TEXT("weight_limit"),TEXT("Serialized node weights exceed inspection bound before copying")); }
    OutValues.SetNumUninitialized(ArrayHelper.Num());
    for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
    {
        OutValues[Index] = static_cast<float>(
            InnerProperty->GetFloatingPointPropertyValue(ArrayHelper.GetRawPtr(Index)));
    }
    return true;
}

bool ReadWeightMapNodeOutputName(
    const FDataflowNode& Node,
    FString& OutWeightMapName)
{
    const UScriptStruct* const Struct = Node.TypedScriptStruct();
    if (!Struct)
    {
        return false;
    }

    if (const FStructProperty* const OutputNameProperty =
            FindFProperty<FStructProperty>(Struct, TEXT("OutputName")))
    {
        const FStrProperty* const StringValueProperty =
            FindFProperty<FStrProperty>(OutputNameProperty->Struct, TEXT("StringValue"));
        if (!StringValueProperty)
        {
            return false;
        }
        const void* const OutputNamePtr =
            OutputNameProperty->ContainerPtrToValuePtr<void>(&Node);
        const void* const StringValuePtr =
            StringValueProperty->ContainerPtrToValuePtr<void>(OutputNamePtr);
        OutWeightMapName = StringValueProperty->GetPropertyValue(StringValuePtr);
        return true;
    }

    if (const FStrProperty* const LegacyNameProperty =
            FindFProperty<FStrProperty>(Struct, TEXT("Name")))
    {
        OutWeightMapName = LegacyNameProperty->GetPropertyValue_InContainer(&Node);
        return true;
    }
    return false;
}


void HashText(FMD5& MD5,const FString& S) { UpdateMd5WithString(MD5,S); }
FString Digest(FMD5& MD5) { uint8 D[16];MD5.Final(D);return BytesToHex(D,UE_ARRAY_COUNT(D)).ToLower(); }
bool FiniteVector(const FVector3f& V) { return FMath::IsFinite(V.X)&&FMath::IsFinite(V.Y)&&FMath::IsFinite(V.Z); }
bool FiniteMesh(FWork& W,TConstArrayView<FVector3f> P,TConstArrayView<uint32> I)
{
    if(P.Num()>MaxMeshElements||I.Num()>MaxMeshElements) { return W.Fail(TEXT("mesh_limit"),TEXT("Full mesh exceeds documented bounded observation limit")); }
    for(const auto& V:P) { if(!FiniteVector(V)) { return W.Fail(TEXT("nonfinite_mesh"),TEXT("Nonfinite compiled or evaluated mesh coordinates cannot be serialized as JSON")); } }
    for(uint32 Index:I) { if(Index>=static_cast<uint32>(P.Num())) { return W.Fail(TEXT("index_invalid"),TEXT("Triangle index is outside the captured position buffer")); } }return true;
}
bool InputTopology(FWork& W,const TSharedPtr<FDataflowNode>& Node,UObject* EvaluationAsset,int32& Count,FString& Hash)
{
    Count=0;Hash.Reset();if(!Node.IsValid()||!Live(EvaluationAsset)) { return false; }const FDataflowInput* Input=Node->FindInput(FName(TEXT("Collection")));if(!Input||Input->GetType()!=FName(TEXT("FManagedArrayCollection"))) { return false; }
    W.SDK(TEXT("evaluate_weightmap_input"));W.Count(TEXT("Dataflow_input_evaluation_calls"));UE::Dataflow::FEngineContext Context(EvaluationAsset);const FManagedArrayCollection Empty;const FManagedArrayCollection Collection=Input->GetValue<FManagedArrayCollection>(Context,Empty);W.Count(TEXT("Dataflow_input_evaluation_returns"));if(!W.Admit()) { return false; }
    auto Shared=MakeShared<FManagedArrayCollection>(Collection);const UE::Chaos::ClothAsset::FCollectionClothConstFacade Cloth(Shared);if(!Cloth.IsValid()) { return false; }
    TArray<FVector3f> P,N;TArray<uint32> I,PatternIndices,PatternWelding;TArray<FVector2f> PP;W.Count(TEXT("collection_mesh_build_calls"));Cloth.BuildSimulationMesh(P,N,I,PP,PatternIndices,PatternWelding);W.Count(TEXT("collection_mesh_build_returns"));if(!FiniteMesh(W,P,I)) { return false; }Count=P.Num();Hash=HashSimulationTopology(P,I);return true;
}
TSharedPtr<FJsonObject> WeightMapNodeState(FWork& W,const TSharedPtr<FDataflowNode>& Node,UObject* Owner,bool bEvaluateCurrent)
{
    if(!Node.IsValid()||!Node->TypedScriptStruct()) { return nullptr; }const auto* Struct=Node->TypedScriptStruct();const FString TypeName=Struct->GetName();
    if(!TypeName.EndsWith(TEXT("ChaosClothAssetWeightMapNode"))&&!TypeName.EndsWith(TEXT("ChaosClothAssetAddWeightMapNode"))) { return nullptr; }
    FString MapName;if(!ReadWeightMapNodeOutputName(*Node,MapName)) { return nullptr; }TArray<float> LegacyWeights;if(!ReadFloatArrayProperty(W,*Node,TEXT("VertexWeights"),LegacyWeights)) { return nullptr; }
    if(LegacyWeights.Num()>MaxMeshElements) { W.Fail(TEXT("weight_limit"),TEXT("Legacy node weights exceed bounded observation limit"));return nullptr; }
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("node_name"),Node->GetName().ToString());J->SetStringField(TEXT("node_type"),Node->GetType().ToString());J->SetStringField(TEXT("node_struct"),TypeName);J->SetStringField(TEXT("weight_map_name"),MapName);
    FString MeshTarget,Override;for(const auto& Entry:{TPair<FName,FString*>(FName(TEXT("MeshTarget")),&MeshTarget),TPair<FName,FString*>(FName(TEXT("MapOverrideType")),&Override)}) { if(const FProperty* P=FindFProperty<FProperty>(Struct,Entry.Key)) { *Entry.Value=ExportDataflowProperty(W,*Node,*P); } }
    J->SetStringField(TEXT("mesh_target"),MeshTarget);J->SetStringField(TEXT("map_override_type"),Override);J->SetNumberField(TEXT("stored_weight_count"),LegacyWeights.Num());J->SetStringField(TEXT("stored_weights_md5"),HashFloatValues(LegacyWeights));
    J->SetStringField(TEXT("stored_weight_representation"),TEXT("serialized_legacy_VertexWeights_compatibility_field"));J->SetBoolField(TEXT("legacy_stored_weights_are_current_snapshot_state"),false);
    FMD5 State;HashText(State,Node->GetName().ToString());HashText(State,Node->GetType().ToString());HashText(State,TypeName);HashText(State,MapName);HashText(State,MeshTarget);HashText(State,Override);HashText(State,HashFloatValues(LegacyWeights));
    bool CurrentStateComplete=false;int32 SnapshotCount=0;int32 ActiveIndex=INDEX_NONE;
    TArray<TSharedPtr<FJsonValue>> SnapshotRows;
    if(Struct->IsChildOf(FDataflowToolNode::StaticStruct()))
    {
        // Public base read methods; no private derived accessor, mutation or const_cast.
        const auto* Tool=static_cast<const FDataflowToolNode*>(Node.Get());const auto Snapshots=Tool->GetSnapshots();const auto* Active=Tool->GetActiveSnapshot();SnapshotCount=Snapshots.Num();if(SnapshotCount>256) { W.Fail(TEXT("snapshot_limit"),TEXT("Snapshot count exceeds bounded inspection limit"));return nullptr; }
        int64 TotalBytes=0;for(int32 Index=0;Index<Snapshots.Num();++Index)
        {
            const auto& S=Snapshots[Index];if(&S==Active) { ActiveIndex=Index; }if(S.Data.GetAllocatedSize()>static_cast<SIZE_T>(MaxSnapshotBytes)) { W.Fail(TEXT("snapshot_limit"),TEXT("Snapshot collection exceeds byte bound before copy"));return nullptr; }
            FManagedArrayCollection Copy(S.Data);TArray<uint8> Bytes;FMemoryWriter Memory(Bytes,true);FObjectAndNameAsStringProxyArchive Archive(Memory,false);W.SDK(TEXT("serialize_snapshot_copy"));W.Count(TEXT("snapshot_serialization_calls"));const bool Serialized=Copy.Serialize(Archive);W.Count(TEXT("snapshot_serialization_returns"));TotalBytes+=Bytes.Num();if(!Serialized||Archive.IsError()||TotalBytes>MaxSnapshotBytes) { W.Fail(TEXT("snapshot_serialization_failed"),TEXT("Snapshot copy serialization failed or exceeded aggregate byte budget"));return nullptr; }
            FMD5 SnapshotHash;if(!Bytes.IsEmpty()) { SnapshotHash.Update(Bytes.GetData(),static_cast<uint32>(Bytes.Num())); }const FString H=Digest(SnapshotHash);HashText(State,S.Name);HashText(State,S.Description);HashText(State,S.GetDate().ToIso8601());HashText(State,S.IsLocked()?TEXT("locked"):TEXT("unlocked"));HashText(State,H);
            auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("index"),Index);Row->SetStringField(TEXT("name"),S.Name);Row->SetStringField(TEXT("description"),S.Description);Row->SetStringField(TEXT("date"),S.GetDate().ToIso8601());Row->SetBoolField(TEXT("locked"),S.IsLocked());Row->SetStringField(TEXT("collection_serialized_md5"),H);Row->SetNumberField(TEXT("serialized_bytes"),Bytes.Num());SnapshotRows.Add(MakeShared<FJsonValueObject>(Row));
        }
        State.Update(reinterpret_cast<const uint8*>(&ActiveIndex),sizeof(ActiveIndex));CurrentStateComplete=true;
    }
    J->SetNumberField(TEXT("snapshot_count"),SnapshotCount);J->SetNumberField(TEXT("active_snapshot_index"),ActiveIndex);J->SetArrayField(TEXT("snapshots"),SnapshotRows);J->SetBoolField(TEXT("current_snapshot_projection_complete"),CurrentStateComplete);J->SetStringField(TEXT("current_snapshot_projection_scope"),TEXT("snapshot_metadata_activeindex_and_serialized_collection_copy_only_not_all_node_settings_or_effective_state"));J->SetBoolField(TEXT("node_state_hash_covers_all_current_settings"),false);
    if(bEvaluateCurrent&&Struct->IsChildOf(FDataflowVertexAttributeEditableNode::StaticStruct())&&TypeName.Equals(TEXT("ChaosClothAssetWeightMapNode"),ESearchCase::CaseSensitive))
    {
        const auto* Editable=static_cast<const FDataflowVertexAttributeEditableNode*>(Node.Get());UE::Dataflow::FEngineContext Context(Owner);TArray<float> Values;W.SDK(TEXT("evaluate_current_weight_values"));W.Count(TEXT("current_weight_evaluation_calls"));Editable->GetVertexAttributeValues(Context,Values);W.Count(TEXT("current_weight_evaluation_returns"));if(!W.Admit()) { return nullptr; }if(Values.Num()>MaxMeshElements) { W.Fail(TEXT("weight_limit"),TEXT("Current evaluated node weights exceed bounded observation limit"));return nullptr; }
        for(float V:Values) { if(!FMath::IsFinite(V)) { W.Fail(TEXT("nonfinite_weights"),TEXT("Current evaluated node weights contain nonfinite values"));return nullptr; } }
        J->SetNumberField(TEXT("current_evaluated_weight_count"),Values.Num());J->SetStringField(TEXT("current_evaluated_weights_md5"),HashFloatValues(Values));J->SetStringField(TEXT("current_value_projection"),TEXT("public_EditableNode_GetVertexAttributeValues_absolute_values_not_serialized_weights"));
    }
    J->SetStringField(TEXT("node_state_md5"),Digest(State));J->SetStringField(TEXT("node_state_hash_contract"),TEXT("UE58_snapshot_copy_archive_names_metadata_activeindex_and_original_node_fields_v1"));J->SetBoolField(TEXT("node_state_hash_cross_version_compatible"),false);
    J->SetBoolField(TEXT("supported_for_weight_map_write"),false);J->SetStringField(TEXT("weight_map_write_support_error"),TEXT("No validated current writer token is available for this node"));
    // Preserve every original inspection group and legacy field separately. The
    // old opaque limited hash must never be accepted as the v2 writer precondition.
    J->SetStringField(TEXT("legacy_node_state_md5"),J->GetStringField(TEXT("node_state_md5")));J->SetStringField(TEXT("legacy_node_state_hash_contract"),J->GetStringField(TEXT("node_state_hash_contract")));
    J->SetStringField(TEXT("legacy_stored_weights_md5"),J->GetStringField(TEXT("stored_weights_md5")));J->SetNumberField(TEXT("legacy_stored_weight_count"),J->GetNumberField(TEXT("stored_weight_count")));
    if(Struct==FChaosClothAssetWeightMapNode::StaticStruct())
    {
        TSharedPtr<FJsonObject> Current;FString Error;W.SDK(TEXT("shared_current_authoring_state"));W.Count(TEXT("current_authoring_state_calls"));
        const bool Described=Cubeless::UE58::ChaosClothAuthoring::DescribeWeightMapAuthoringState(Node,Owner,Current,Error);W.Count(TEXT("current_authoring_state_returns"));if(!W.Admit()) { return nullptr; }
        J->SetBoolField(TEXT("current_authoring_token_available"),Described&&Current.IsValid());
        if(Described&&Current.IsValid())
        {
            J->SetObjectField(TEXT("current_authoring_state"),Current);for(const TCHAR* K:{TEXT("node_state_md5"),TEXT("node_state_hash_contract"),TEXT("node_state_hash_version"),TEXT("node_state_hash_scope"),TEXT("node_state_hash_excluded_scope"),TEXT("weight_map_writer_additional_gates")}) { J->SetStringField(K,Current->GetStringField(K)); }
            J->SetBoolField(TEXT("node_state_hash_covers_reviewed_writer_preconditions"),true);J->SetBoolField(TEXT("supported_for_weight_map_write"),true);J->SetStringField(TEXT("weight_map_write_support_error"),TEXT(""));
        }
        else
        {
            J->SetStringField(TEXT("current_authoring_state_error"),Error);J->SetStringField(TEXT("weight_map_write_support_error"),Error);if(Error.StartsWith(TEXT("Captured authoring"),ESearchCase::CaseSensitive)) { W.Fail(TEXT("authoring_target_changed"),Error);return nullptr; }
        }
    }return J;
}

TSharedRef<FJsonObject> NodeSnapshot(FWork& W,const TSharedPtr<FDataflowNode>& Node,int32 MaxProperties)
{
    auto J=MakeShared<FJsonObject>();if(!Node.IsValid()) { return J; }const UScriptStruct* Struct=Node->TypedScriptStruct();J->SetStringField(TEXT("name"),Node->GetName().ToString());J->SetStringField(TEXT("guid"),Node->GetGuid().ToString(EGuidFormats::DigitsWithHyphens));J->SetStringField(TEXT("type"),Node->GetType().ToString());J->SetStringField(TEXT("struct"),Struct?Struct->GetName():FString());J->SetStringField(TEXT("display_name"),Node->GetDisplayName().ToString());J->SetStringField(TEXT("category"),Node->GetCategory().ToString());J->SetStringField(TEXT("tags"),Node->GetTags());J->SetBoolField(TEXT("cloth_node"),IsClothDataflowNode(*Node));
    auto State=WeightMapNodeState(W,Node,W.Asset.Get(),false);J->SetBoolField(TEXT("weight_map_node"),State.IsValid());if(State) { J->SetObjectField(TEXT("weight_map_state"),State); }if(!W.R.Error.IsEmpty()) { return J; }
    TArray<TSharedPtr<FJsonValue>> Props;int32 Total=0;if(Struct) { for(TFieldIterator<FProperty> It(Struct,EFieldIteratorFlags::IncludeSuper);It;++It)
    {
        FProperty* P=*It;if(!P||!P->HasAnyPropertyFlags(CPF_Edit)) { continue; }++Total;if(Props.Num()>=MaxProperties) { continue; }FString Export=ExportDataflowProperty(W,*Node,*P);if(Export.Len()>4096) { continue; }auto Row=MakeDataflowPropertySnapshot(W,*Node,*P);if(!W.Budget(Json(Row.ToSharedRef()).Len())) { break; }Props.Add(MakeShared<FJsonValueObject>(Row));
    } }
    J->SetNumberField(TEXT("editable_property_count"),Total);J->SetBoolField(TEXT("properties_truncated"),Total>Props.Num());J->SetArrayField(TEXT("editable_properties"),Props);return J;
}
TSharedRef<FJsonObject> AssetSnapshot(FWork& W,int32 RequestedLod,const FString& Filter,bool Values,bool Flags,bool DataflowNodes,int32 MaxProperties,int32 MaxNodes)
{
    auto D=MakeShared<FJsonObject>();auto* Asset=W.Asset.Get();D->SetStringField(TEXT("asset_path"),Asset->GetPathName());D->SetStringField(TEXT("asset_class"),Asset->GetClass()->GetPathName());W.SDK(TEXT("read_asset_models"));W.Count(TEXT("model_getter_calls"));const bool Valid=Asset->HasValidClothSimulationModels();const int32 ModelCount=Asset->GetNumClothSimulationModels();W.Count(TEXT("model_getter_returns"));if(!W.Admit()) { return D; }D->SetBoolField(TEXT("has_valid_simulation_models"),Valid);D->SetNumberField(TEXT("simulation_model_count"),ModelCount);if(ModelCount<0||ModelCount>MaxModels) { W.Fail(TEXT("model_limit"),TEXT("Simulation model count exceeds inspection bound"));return D; }
    TArray<TSharedPtr<FJsonValue>> Models;int32 Returned=0,Total=0;D->SetArrayField(TEXT("models"),Models);
    for(int32 ModelIndex=0;ModelIndex<ModelCount&&W.R.Error.IsEmpty();++ModelIndex)
    {
        auto M=MakeShared<FJsonObject>();M->SetNumberField(TEXT("model_index"),ModelIndex);M->SetStringField(TEXT("model_name"),Asset->GetClothSimulationModelName(ModelIndex).ToString());M->SetStringField(TEXT("asset_guid"),Asset->GetAssetGuid(ModelIndex).ToString(EGuidFormats::DigitsWithHyphens));W.Count(TEXT("collection_getter_calls"));const TArray<TSharedRef<const FManagedArrayCollection>> Collections=Asset->GetCollections(ModelIndex);W.Count(TEXT("collection_getter_returns"));if(!W.Admit()) { break; }if(Collections.Num()>MaxLods) { W.Fail(TEXT("lod_limit"),TEXT("LOD collection count exceeds inspection bound"));break; }M->SetNumberField(TEXT("lod_count"),Collections.Num());TArray<TSharedPtr<FJsonValue>> Lods;
        for(int32 L=0;L<Collections.Num()&&W.R.Error.IsEmpty();++L)
        {
            if(RequestedLod>=0&&L!=RequestedLod) { continue; }auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("lod_index"),L);const Chaos::Softs::FCollectionPropertyConstFacade Facade(Collections[L]);Row->SetBoolField(TEXT("property_facade_valid"),Facade.IsValid());Row->SetNumberField(TEXT("property_count"),Facade.IsValid()?Facade.Num():0);TArray<TSharedPtr<FJsonValue>> Props;
            if(Facade.IsValid()) { for(int32 I=0;I<Facade.Num();++I) { const FString Name=Facade.GetKeyName(I).ToString();if(!Filter.IsEmpty()&&!Name.Contains(Filter,ESearchCase::IgnoreCase)) { continue; }++Total;if(Returned>=MaxProperties) { continue; }auto P=MakeCollectionPropertySnapshot(Facade,I,Values,Flags);if(Values&&(!FiniteVector(Facade.GetLowValue<FVector3f>(I))||!FiniteVector(Facade.GetHighValue<FVector3f>(I)))) { W.Fail(TEXT("nonfinite_property"),TEXT("Cloth property raw vector contains nonfinite values"));break; }if(!W.Budget(Json(P.ToSharedRef()).Len())) { break; }Props.Add(MakeShared<FJsonValueObject>(P));++Returned; } }
            Row->SetArrayField(TEXT("properties"),Props);Lods.Add(MakeShared<FJsonValueObject>(Row));
        }
        M->SetArrayField(TEXT("lods"),Lods);Models.Add(MakeShared<FJsonValueObject>(M));D->SetArrayField(TEXT("models"),Models);
    }
    D->SetNumberField(TEXT("matching_property_count"),Total);D->SetNumberField(TEXT("returned_property_count"),Returned);D->SetBoolField(TEXT("properties_truncated"),Total>Returned);if(!W.R.Error.IsEmpty()) { return D; }
    W.SDK(TEXT("read_asset_dataflow"));const FDataflowInstance& Instance=Asset->GetDataflowInstance();TStrongObjectPtr<UDataflow> Dataflow(Instance.GetDataflowAsset());D->SetStringField(TEXT("dataflow_asset_path"),Live(Dataflow.Get())?Dataflow->GetPathName():FString());D->SetStringField(TEXT("dataflow_terminal"),Instance.GetDataflowTerminal().ToString());D->SetBoolField(TEXT("dataflow_present"),Dataflow.Get()!=nullptr);D->SetBoolField(TEXT("dataflow_editor_only"),Live(Dataflow.Get())&&Dataflow->IsEditorOnly());if(!W.Admit()) { return D; }
    if(DataflowNodes&&Live(Dataflow.Get()))
    {
        const auto Graph=static_cast<const UDataflow*>(Dataflow.Get())->GetDataflow();TArray<TSharedPtr<FJsonValue>> Nodes;int32 Count=0,ReturnedProps=0,EditableProps=0;if(Graph.IsValid())
        {
            const auto GraphNodes=Graph->GetNodes();if(GraphNodes.Num()>MaxGraphNodes) { W.Fail(TEXT("graph_limit"),TEXT("Dataflow graph node count exceeds bounded inspection limit"));return D; }
            for(const auto& Node:GraphNodes)
            {
                if(!Node.IsValid()||!IsClothDataflowNode(*Node)) { continue; }++Count;if(Nodes.Num()>=MaxNodes) { continue; }auto Row=NodeSnapshot(W,Node,FMath::Max(0,MaxProperties-ReturnedProps));const TArray<TSharedPtr<FJsonValue>>* Props=nullptr;if(Row->TryGetArrayField(TEXT("editable_properties"),Props)&&Props) { ReturnedProps+=Props->Num(); }double N=0;if(Row->TryGetNumberField(TEXT("editable_property_count"),N)) { EditableProps+=FMath::Max(0,FMath::RoundToInt(N)); }if(!W.Budget(Json(Row).Len())) { break; }Nodes.Add(MakeShared<FJsonValueObject>(Row));if(!W.R.Error.IsEmpty()) { break; }
            }
        }
        D->SetNumberField(TEXT("cloth_dataflow_node_count"),Count);D->SetNumberField(TEXT("returned_dataflow_property_count"),ReturnedProps);D->SetNumberField(TEXT("dataflow_editable_property_count"),EditableProps);D->SetBoolField(TEXT("dataflow_nodes_truncated"),Count>Nodes.Num());D->SetBoolField(TEXT("dataflow_properties_truncated"),EditableProps>ReturnedProps);D->SetArrayField(TEXT("cloth_dataflow_nodes"),Nodes);
    }
    D->SetArrayField(TEXT("ue_5_6_exposure_notes"),StringsToJson({TEXT("Collection numeric storage is raw FVector3f; consuming keys define semantic type."),TEXT("Intrinsic changes require cloth proxy reconstruction."),TEXT("Connected Dataflow inputs override reflected local values."),TEXT("Direct generated-collection writes are intentionally not exposed because Dataflow regeneration replaces them.")}));D->SetArrayField(TEXT("ue_5_8_compatibility_notes"),StringsToJson({TEXT("Const getters may synchronously wait for async properties."),TEXT("Snapshot-backed node state and legacy VertexWeights are distinct; current/effective/compiled weights are not interchangeable."),TEXT("Durable snapshot-based weight-map writing is outside these observation APIs.")}));return D;
}
TSharedPtr<FJsonObject> MakeWeightMapValueSummary(
    const FName WeightMapName,
    const TConstArrayView<float> Values,
    const TArray<float>* CompiledValues,
    const Chaos::Softs::FCollectionPropertyConstFacade& Properties)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), WeightMapName.ToString());
    Result->SetNumberField(TEXT("value_count"), Values.Num());
    Result->SetStringField(TEXT("values_md5"), HashFloatValues(Values));
    const bool bCompiled = CompiledValues != nullptr;
    Result->SetBoolField(TEXT("compiled_for_solver"), bCompiled);
    Result->SetNumberField(
        TEXT("compiled_value_count"),
        CompiledValues ? CompiledValues->Num() : 0);
    Result->SetStringField(
        TEXT("compiled_values_md5"),
        CompiledValues ? HashFloatValues(*CompiledValues) : FString());
    const bool bCompiledValuesMatchCollection =
        CompiledValues && CompiledValues->Num() == Values.Num() &&
        HashFloatValues(*CompiledValues) == HashFloatValues(Values);
    Result->SetBoolField(
        TEXT("compiled_values_match_collection"),
        bCompiledValuesMatchCollection);

    double Sum = 0.0;
    float Minimum = 0.0f;
    float Maximum = 0.0f;
    int32 ZeroCount = 0;
    int32 OneCount = 0;
    bool bAllFinite = true;
    if (!Values.IsEmpty())
    {
        Minimum = Values[0];
        Maximum = Values[0];
        for (const float Value : Values)
        {
            bAllFinite &= FMath::IsFinite(Value);
            Minimum = FMath::Min(Minimum, Value);
            Maximum = FMath::Max(Maximum, Value);
            Sum += Value;
            ZeroCount += FMath::IsNearlyZero(Value) ? 1 : 0;
            OneCount += FMath::IsNearlyEqual(Value, 1.0f) ? 1 : 0;
        }
    }
    Result->SetNumberField(TEXT("minimum"), Minimum);
    Result->SetNumberField(TEXT("maximum"), Maximum);
    Result->SetNumberField(
        TEXT("average"),
        Values.IsEmpty() ? 0.0 : Sum / static_cast<double>(Values.Num()));
    Result->SetNumberField(TEXT("zero_count"), ZeroCount);
    Result->SetNumberField(TEXT("one_count"), OneCount);
    Result->SetBoolField(TEXT("all_finite"), bAllFinite);

    TArray<TSharedPtr<FJsonValue>> Consumers;
    if (Properties.IsValid())
    {
        for (int32 PropertyIndex = 0; PropertyIndex < Properties.Num(); ++PropertyIndex)
        {
            if (Properties.GetStringValue(PropertyIndex) != WeightMapName.ToString())
            {
                continue;
            }
            TSharedPtr<FJsonObject> Consumer = MakeShared<FJsonObject>();
            Consumer->SetStringField(TEXT("property_name"), Properties.GetKeyName(PropertyIndex).ToString());
            Consumer->SetArrayField(
                TEXT("low_raw_vector"),
                Vector3fToJson(Properties.GetLowValue<FVector3f>(PropertyIndex)));
            Consumer->SetArrayField(
                TEXT("high_raw_vector"),
                Vector3fToJson(Properties.GetHighValue<FVector3f>(PropertyIndex)));
            Consumer->SetBoolField(TEXT("intrinsic"), Properties.IsIntrinsic(PropertyIndex));
            Consumers.Add(MakeShared<FJsonValueObject>(Consumer));
        }
    }
    Result->SetNumberField(TEXT("consumer_property_count"), Consumers.Num());
    Result->SetArrayField(TEXT("consumer_properties"), Consumers);
    Result->SetBoolField(
        TEXT("runtime_bound"),
        !Consumers.IsEmpty() && bCompiledValuesMatchCollection);
    return Result;
}


TArray<TSharedPtr<FJsonValue>> WeightNodes(FWork& W,UDataflow* Dataflow,int32 TerminalCount,const FString& TerminalHash,const FString& Filter)
{
    TArray<TSharedPtr<FJsonValue>> Rows;const auto Graph=Dataflow?static_cast<const UDataflow*>(Dataflow)->GetDataflow():nullptr;if(!Graph.IsValid()) { return Rows; }const auto Nodes=Graph->GetNodes();if(Nodes.Num()>MaxGraphNodes) { W.Fail(TEXT("graph_limit"),TEXT("Dataflow graph exceeds bounded inspection limit"));return Rows; }
    for(const auto& Node:Nodes)
    {
        if(!Filter.IsEmpty()) { FString Selected;if(!Node.IsValid()||!ReadWeightMapNodeOutputName(*Node,Selected)||Selected!=Filter) { continue; } }auto S=WeightMapNodeState(W,Node,W.Asset.Get(),true);if(!W.R.Error.IsEmpty()) { break; }if(!S.IsValid()||(!Filter.IsEmpty()&&S->GetStringField(TEXT("weight_map_name"))!=Filter)) { continue; }int32 Count=0;FString Hash;const bool Valid=InputTopology(W,Node,W.Asset.Get(),Count,Hash);S->SetBoolField(TEXT("input_collection_valid"),Valid);S->SetNumberField(TEXT("input_sim_vertex_count"),Count);S->SetStringField(TEXT("input_topology_md5"),Hash);S->SetBoolField(TEXT("terminal_topology_matches"),Valid&&Count==TerminalCount&&Hash==TerminalHash);if(!W.Budget(Json(S.ToSharedRef()).Len())) { break; }Rows.Add(MakeShared<FJsonValueObject>(S));if(!W.R.Error.IsEmpty()) { break; }
    }return Rows;
}
} // namespace

FString InspectAsset(const FString& ParamsJson)
{
    FWork W(TEXT("inspect_chaos_cloth_asset"));if(!W.Enter(ParamsJson)) { return W.Finish(); }TGuardValue<bool> Guard(Running,true);FJsonParamParseState P;const FString Path=GetStringParam(W.Params,TEXT("cloth_asset_path"),P);const int32 Lod=GetIntParam(W.Params,TEXT("lod_index"),-1,P);const FString Filter=GetStringParam(W.Params,TEXT("property_name_filter"),P);const bool Values=GetBoolParam(W.Params,TEXT("include_property_values"),true,P),Flags=GetBoolParam(W.Params,TEXT("include_property_flags"),true,P),Nodes=GetBoolParam(W.Params,TEXT("include_dataflow_nodes"),true,P);const int32 MaxProperties=FMath::Clamp(GetIntParam(W.Params,TEXT("max_properties"),512,P),1,4096),MaxNodes=FMath::Clamp(GetIntParam(W.Params,TEXT("max_nodes"),256,P),1,2048);
    if(!P.IsValid()) { W.Fail(TEXT("parameters_invalid"),P.GetErrorMessage());return W.Finish(); }if(!W.Load(Path)) { return W.Finish(); }if(Lod<-1) { W.Fail(TEXT("lod_invalid"),TEXT("lod_index must be -1 or non-negative"));return W.Finish(); }
    auto D=AssetSnapshot(W,Lod,Filter,Values,Flags,Nodes,MaxProperties,MaxNodes);W.Legacy->SetObjectField(TEXT("data"),D);if(W.R.Error.IsEmpty()) { W.Legacy->SetBoolField(TEXT("success"),true);W.Checks->SetBoolField(TEXT("original_model_collection_Dataflow_projection_observed"),true); }return W.Finish();
}

FString InspectSimulationMesh(const FString& ParamsJson)
{
    FWork W(TEXT("inspect_chaos_cloth_simulation_mesh"));if(!W.Enter(ParamsJson)) { return W.Finish(); }TGuardValue<bool> Guard(Running,true);FJsonParamParseState P;const FString Path=GetStringParam(W.Params,TEXT("cloth_asset_path"),P);const int32 Lod=GetIntParam(W.Params,TEXT("lod_index"),0,P);const FString MapName=GetStringParam(W.Params,TEXT("weight_map_name"),P);const int32 VertexStart=GetIntParam(W.Params,TEXT("vertex_start"),0,P),RequestedV=GetIntParam(W.Params,TEXT("max_vertices"),4096,P),FaceStart=GetIntParam(W.Params,TEXT("face_start"),0,P),RequestedF=GetIntParam(W.Params,TEXT("max_faces"),4096,P);const bool PositionsIncluded=GetBoolParam(W.Params,TEXT("include_positions"),true,P),NormalsIncluded=GetBoolParam(W.Params,TEXT("include_normals"),false,P),IndicesIncluded=GetBoolParam(W.Params,TEXT("include_indices"),true,P),WeightsIncluded=GetBoolParam(W.Params,TEXT("include_weight_values"),true,P);
    if(!P.IsValid()) { W.Fail(TEXT("parameters_invalid"),P.GetErrorMessage());return W.Finish(); }if(Lod<0||VertexStart<0||FaceStart<0||RequestedV<1||RequestedF<1) { W.Fail(TEXT("range_invalid"),TEXT("lod_index, vertex_start, and face_start must be non-negative; max_vertices and max_faces must be positive"));return W.Finish(); }const int32 MaxV=FMath::Clamp(RequestedV,1,16384),MaxF=FMath::Clamp(RequestedF,1,16384);
    if(!W.Load(Path)) { return W.Finish(); }const UChaosClothAsset* Asset=Cast<UChaosClothAsset>(W.Asset.Get());if(!Asset) { W.Fail(TEXT("class_unsupported"),TEXT("This route requires UChaosClothAsset and does not reinterpret other cloth asset classes"));return W.Finish(); }
    W.SDK(TEXT("read_simulation_model"));W.Count(TEXT("collection_getter_calls"));const TArray<TSharedRef<const FManagedArrayCollection>> Collections=Asset->GetClothCollections();W.Count(TEXT("collection_getter_returns"));if(!W.Admit()) { return W.Finish(); }if(!Collections.IsValidIndex(Lod)) { W.Fail(TEXT("lod_out_of_range"),FString::Printf(TEXT("lod_index %d is out of range for %d cloth collections"),Lod,Collections.Num()));return W.Finish(); }const UE::Chaos::ClothAsset::FCollectionClothConstFacade Cloth(Collections[Lod]);if(!Cloth.IsValid()) { W.Fail(TEXT("collection_schema_invalid"),TEXT("The selected LOD does not contain a valid UE5.8 cloth collection schema"));return W.Finish(); }
    W.Count(TEXT("model_getter_calls"));const TSharedPtr<const FChaosClothSimulationModel> Model=Asset->GetClothSimulationModel();W.Count(TEXT("model_getter_returns"));if(!W.Admit()) { return W.Finish(); }if(!Model.IsValid()||!Model->IsValidLodIndex(Lod)) { W.Fail(TEXT("simulation_model_invalid"),TEXT("The selected cloth asset has no valid compiled simulation model for this LOD"));return W.Finish(); }
    const auto Positions=Model->GetPositions(Lod),Normals=Model->GetNormals(Lod);const auto Indices=Model->GetIndices(Lod);if(Indices.Num()%3!=0) { W.Fail(TEXT("triangle_alignment"),TEXT("The compiled simulation index buffer is not triangle aligned"));return W.Finish(); }if(!FiniteMesh(W,Positions,Indices)) { return W.Finish(); }
    const auto& Compiled=Model->ClothSimulationLodModels[Lod];const int32 VertexEnd=static_cast<int32>(FMath::Min<int64>(Positions.Num(),static_cast<int64>(VertexStart)+MaxV));const int32 FaceCount=Indices.Num()/3,FaceEnd=static_cast<int32>(FMath::Min<int64>(FaceCount,static_cast<int64>(FaceStart)+MaxF));auto D=MakeShared<FJsonObject>();W.Legacy->SetObjectField(TEXT("data"),D);D->SetStringField(TEXT("asset_path"),Asset->GetPathName());D->SetStringField(TEXT("asset_class"),Asset->GetClass()->GetPathName());D->SetStringField(TEXT("asset_guid"),Asset->GetAssetGuid(0).ToString(EGuidFormats::DigitsWithHyphens));D->SetNumberField(TEXT("lod_index"),Lod);D->SetNumberField(TEXT("lod_count"),Model->GetNumLods());D->SetNumberField(TEXT("sim_vertex_count"),Positions.Num());D->SetNumberField(TEXT("sim_face_count"),FaceCount);D->SetNumberField(TEXT("collection_sim_face_count"),Cloth.GetNumSimFaces());D->SetNumberField(TEXT("sim_pattern_count"),Cloth.GetNumSimPatterns());const FString Topology=HashSimulationTopology(Positions,Indices);D->SetStringField(TEXT("topology_md5"),Topology);D->SetStringField(TEXT("coordinate_space"),TEXT("cloth_local_rest_pose"));D->SetStringField(TEXT("coordinate_units"),TEXT("Unreal centimeters"));D->SetStringField(TEXT("vertex_index_contract"),TEXT("compiled positions and collection SimVertices3D weight values use the same global vertex indices"));W.Checks->SetBoolField(TEXT("collection_compiled_vertex_count_equal"),Cloth.GetNumSimVertices3D()==Positions.Num());
    D->SetNumberField(TEXT("vertex_start"),VertexStart);D->SetNumberField(TEXT("returned_vertex_count"),FMath::Max(0,VertexEnd-VertexStart));D->SetNumberField(TEXT("next_vertex_start"),VertexEnd<Positions.Num()?VertexEnd:-1);D->SetBoolField(TEXT("vertices_truncated"),VertexEnd<Positions.Num());
    if(PositionsIncluded) { TArray<TSharedPtr<FJsonValue>> A;for(int32 I=VertexStart;I<VertexEnd;++I) { A.Add(MakeShared<FJsonValueArray>(Vector3fToJson(Positions[I]))); }D->SetArrayField(TEXT("positions"),A); }
    if(NormalsIncluded) { TArray<TSharedPtr<FJsonValue>> A;for(int32 I=VertexStart;I<VertexEnd;++I) { const FVector3f N=Normals.IsValidIndex(I)?Normals[I]:FVector3f::ZeroVector;if(!FiniteVector(N)) { W.Fail(TEXT("nonfinite_normal"),TEXT("Nonfinite normal cannot be serialized"));return W.Finish(); }A.Add(MakeShared<FJsonValueArray>(Vector3fToJson(N))); }D->SetArrayField(TEXT("normals"),A); }
    D->SetNumberField(TEXT("face_start"),FaceStart);D->SetNumberField(TEXT("returned_face_count"),FMath::Max(0,FaceEnd-FaceStart));D->SetNumberField(TEXT("next_face_start"),FaceEnd<FaceCount?FaceEnd:-1);D->SetBoolField(TEXT("faces_truncated"),FaceEnd<FaceCount);
    if(IndicesIncluded) { TArray<TSharedPtr<FJsonValue>> A;for(int32 I=FaceStart;I<FaceEnd;++I) { const int32 O=I*3;A.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(Indices[O]),MakeShared<FJsonValueNumber>(Indices[O+1]),MakeShared<FJsonValueNumber>(Indices[O+2])})); }D->SetArrayField(TEXT("indices"),A); }
    const Chaos::Softs::FCollectionPropertyConstFacade Properties(Collections[Lod]);TArray<FName> Names=Cloth.GetWeightMapNames();Names.Sort(FNameLexicalLess());TArray<TSharedPtr<FJsonValue>> Maps;for(FName Name:Names) { auto Values=Cloth.GetWeightMap(Name);if(Values.Num()>MaxMeshElements) { W.Fail(TEXT("weight_limit"),TEXT("Weight map exceeds full observation bound"));break; }auto S=MakeWeightMapValueSummary(Name,Values,Compiled.WeightMaps.Find(Name),Properties);if(!S->GetBoolField(TEXT("all_finite"))||!FiniteObject(S.ToSharedRef())) { W.Fail(TEXT("nonfinite_weight"),TEXT("Collection weight map contains nonfinite values"));break; }if(!W.Budget(Json(S.ToSharedRef()).Len())) { break; }Maps.Add(MakeShared<FJsonValueObject>(S)); }D->SetNumberField(TEXT("weight_map_count"),Names.Num());D->SetArrayField(TEXT("weight_maps"),Maps);if(!W.R.Error.IsEmpty()) { return W.Finish(); }
    if(!MapName.IsEmpty()) { const FName Name(*MapName);if(!Cloth.HasWeightMap(Name)) { W.Fail(TEXT("weight_map_missing"),FString::Printf(TEXT("Weight map '%s' is not present on LOD %d"),*MapName,Lod));return W.Finish(); }const auto Values=Cloth.GetWeightMap(Name);D->SetStringField(TEXT("selected_weight_map_name"),MapName);D->SetStringField(TEXT("selected_weight_map_md5"),HashFloatValues(Values));D->SetBoolField(TEXT("selected_weight_map_compiled_for_solver"),Compiled.WeightMaps.Contains(Name));if(WeightsIncluded) { TArray<TSharedPtr<FJsonValue>> A;for(int32 I=VertexStart;I<VertexEnd&&Values.IsValidIndex(I);++I) { A.Add(MakeShared<FJsonValueNumber>(Values[I])); }D->SetArrayField(TEXT("selected_weight_values"),A); } }
    W.SDK(TEXT("read_weightmap_Dataflow"));TStrongObjectPtr<UDataflow> Dataflow(Asset->GetDataflowInstance().GetDataflowAsset());D->SetBoolField(TEXT("dataflow_present"),Dataflow.Get()!=nullptr);D->SetStringField(TEXT("dataflow_asset_path"),Live(Dataflow.Get())?Dataflow->GetPathName():FString());auto Nodes=WeightNodes(W,Live(Dataflow.Get())?Dataflow.Get():nullptr,Positions.Num(),Topology,MapName);D->SetNumberField(TEXT("matching_weight_map_node_count"),Nodes.Num());D->SetArrayField(TEXT("weight_map_dataflow_nodes"),Nodes);D->SetArrayField(TEXT("authoring_notes"),StringsToJson({TEXT("Weight values are normalized masks, normally [0,1]; MaxDistance centimeters come from consuming Low/High."),TEXT("Use a durable snapshot-aware writer only after fresh topology/map/node-state and dry-run preconditions. Legacy VertexWeights is not the current snapshot state.")}));
    if(!W.R.Error.IsEmpty()) { return W.Finish(); }if(!W.Budget(Json(D).Len())) { return W.Finish(); }W.Legacy->SetBoolField(TEXT("success"),true);W.Checks->SetBoolField(TEXT("compiled_mesh_weightmap_node_topology_projection_observed"),true);return W.Finish();
}
} // namespace Cubeless::UE58::ChaosClothInspection
