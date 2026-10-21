#include "Operations/CubelessMaterialCollectionMutationOperations.h"
#include "Compatibility/UE58/CubelessMaterialCollectionMutationApi.h"
#include "Operations/CubelessFunctionOperations.h"
#include "Serialization/JsonSerializer.h"

namespace Cubeless::Operations
{
namespace
{
FString Encode(const UE58::Function::FResult& R, const TCHAR* Operation)
{
    const FString Base = EncodeFunctionResult(R, Operation);
    TSharedPtr<FJsonObject> J;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Base), J) || !J.IsValid()) { return Base; }
    for (const TCHAR* Key : {TEXT("save_requested"), TEXT("save_attempted"), TEXT("saved")})
    {
        bool Value = false; if (R.Data->TryGetBoolField(Key, Value)) { J->SetBoolField(Key, Value); }
    }
    FString Text; FJsonSerializer::Serialize(J.ToSharedRef(), TJsonWriterFactory<>::Create(&Text)); return Text;
}
}
FString SetMaterialParameterCollectionValues(const FString& CollectionPath, const FString& ScalarsJson, const FString& VectorsJson,
    bool bSetAssetDefaults, bool bSetRuntime, bool bSave)
{ return Encode(UE58::MaterialCollectionMutation::SetValues(CollectionPath, ScalarsJson, VectorsJson,
    bSetAssetDefaults, bSetRuntime, bSave), TEXT("set_material_parameter_collection_values")); }
FString MirrorMaterialParameterCollection(const FString& SourcePath, const FString& TargetPath, const FString& ParameterNamesJson,
    bool bCopyDefaults, bool bPreserveIds, bool bSave)
{ return Encode(UE58::MaterialCollectionMutation::Mirror(SourcePath, TargetPath, ParameterNamesJson,
    bCopyDefaults, bPreserveIds, bSave), TEXT("mirror_material_parameter_collection")); }
}
