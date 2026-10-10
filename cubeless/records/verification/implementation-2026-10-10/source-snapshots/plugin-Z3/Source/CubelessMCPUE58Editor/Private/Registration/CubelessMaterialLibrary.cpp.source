#include "Registration/CubelessMaterialLibrary.h"
#include "Operations/CubelessMaterialOperations.h"
#include "Dom/JsonValue.h"

namespace
{
TSharedPtr<FJsonObject> GraphArgs(const FString& MaterialPath, const FString& GraphType)
{
    TSharedPtr<FJsonObject> Args = MakeShared<FJsonObject>();
    Args->SetStringField(TEXT("material_path"), MaterialPath);
    Args->SetStringField(TEXT("graph_type"), GraphType);
    return Args;
}
}

using Cubeless::UE58::Material::EOperation;
using Cubeless::Operations::RunMaterialOperation;

FString UCubelessMaterialLibrary::ResolveMaterialGraph(const FString& MaterialPath, const FString& GraphType, int32 MaxGraphNodes)
{
    return RunMaterialOperation(EOperation::ResolveGraph, GraphArgs(MaterialPath, GraphType), MaxGraphNodes);
}

FString UCubelessMaterialLibrary::ListMaterialNodes(const FString& MaterialPath, const FString& GraphType, const FString& NodeType, const FString& DescContains, bool bIncludePins, int32 MaxGraphNodes)
{
    auto Args = GraphArgs(MaterialPath, GraphType);
    Args->SetStringField(TEXT("node_type"), NodeType);
    Args->SetStringField(TEXT("desc_contains"), DescContains);
    Args->SetBoolField(TEXT("include_pins"), bIncludePins);
    return RunMaterialOperation(EOperation::ListNodes, Args, MaxGraphNodes);
}

FString UCubelessMaterialLibrary::AnalyzeMaterialGraph(const FString& MaterialPath, const FString& GraphType, bool bIncludeNodes, bool bIncludeUsage, int32 MaxReferencers, int32 MaxGraphNodes)
{
    auto Args = GraphArgs(MaterialPath, GraphType);
    Args->SetBoolField(TEXT("include_nodes"), bIncludeNodes);
    Args->SetBoolField(TEXT("include_usage"), bIncludeUsage);
    Args->SetNumberField(TEXT("max_referencers"), MaxReferencers);
    return RunMaterialOperation(EOperation::AnalyzeGraph, Args, MaxGraphNodes);
}

FString UCubelessMaterialLibrary::ListMaterialCollectionParameterNodes(const FString& MaterialPath, const FString& GraphType, int32 MaxGraphNodes)
{
    return RunMaterialOperation(EOperation::ListCollectionNodes, GraphArgs(MaterialPath, GraphType), MaxGraphNodes);
}

FString UCubelessMaterialLibrary::GetMaterialParameterCollectionValues(const FString& CollectionPath, const TArray<FString>& ParameterNames, bool bIncludeAssetDefaults, bool bIncludeRuntime)
{
    auto Args = MakeShared<FJsonObject>();
    Args->SetStringField(TEXT("collection_path"), CollectionPath);
    TArray<TSharedPtr<FJsonValue>> Names;
    for (const FString& Name : ParameterNames) { Names.Add(MakeShared<FJsonValueString>(Name)); }
    Args->SetArrayField(TEXT("parameter_names"), Names);
    Args->SetBoolField(TEXT("include_asset_defaults"), bIncludeAssetDefaults);
    Args->SetBoolField(TEXT("include_runtime"), bIncludeRuntime);
    return RunMaterialOperation(EOperation::GetCollectionValues, Args, 2048);
}

FString UCubelessMaterialLibrary::ConnectMaterialNodes(const FString& MaterialPath, const FString& FromNodeId, const FString& ToNodeId, const FString& ToInput, const FString& FromOutput, const FString& GraphType, int32 MaxGraphNodes)
{
    auto Args = GraphArgs(MaterialPath, GraphType);
    Args->SetStringField(TEXT("from_node_id"), FromNodeId);
    Args->SetStringField(TEXT("to_node_id"), ToNodeId);
    Args->SetStringField(TEXT("to_input"), ToInput);
    Args->SetStringField(TEXT("from_output"), FromOutput);
    return RunMaterialOperation(EOperation::ConnectNodes, Args, MaxGraphNodes);
}

FString UCubelessMaterialLibrary::ConnectMaterialProperty(const FString& MaterialPath, const FString& FromNodeId, const FString& MaterialProperty, const FString& FromOutput, int32 MaxGraphNodes)
{
    auto Args = GraphArgs(MaterialPath, TEXT("material"));
    Args->SetStringField(TEXT("from_node_id"), FromNodeId);
    Args->SetStringField(TEXT("property"), MaterialProperty);
    Args->SetStringField(TEXT("from_output"), FromOutput);
    return RunMaterialOperation(EOperation::ConnectProperty, Args, MaxGraphNodes);
}

FString UCubelessMaterialLibrary::DeleteMaterialNode(const FString& MaterialPath, const FString& NodeId, const FString& GraphType, int32 MaxGraphNodes)
{
    auto Args = GraphArgs(MaterialPath, GraphType);
    Args->SetStringField(TEXT("node_id"), NodeId);
    return RunMaterialOperation(EOperation::DeleteNode, Args, MaxGraphNodes);
}

FString UCubelessMaterialLibrary::LayoutMaterialNodes(const FString& MaterialPath, const FString& GraphType, int32 MaxGraphNodes)
{
    return RunMaterialOperation(EOperation::LayoutNodes, GraphArgs(MaterialPath, GraphType), MaxGraphNodes);
}

FString UCubelessMaterialLibrary::AddMaterialNode(const FString& MaterialPath, const FString& ExpressionClass, const FString& GraphType, int32 NodeX, int32 NodeY, const FString& SelectedAsset, const FString& PropertiesJson, int32 MaxGraphNodes)
{
    auto Args = GraphArgs(MaterialPath, GraphType);
    Args->SetStringField(TEXT("expression_class"), ExpressionClass);
    Args->SetNumberField(TEXT("node_x"), NodeX);
    Args->SetNumberField(TEXT("node_y"), NodeY);
    Args->SetStringField(TEXT("selected_asset"), SelectedAsset);
    Args->SetStringField(TEXT("properties_json"), PropertiesJson);
    return RunMaterialOperation(EOperation::AddNode, Args, MaxGraphNodes);
}

FString UCubelessMaterialLibrary::SetMaterialNodeProperty(const FString& MaterialPath, const FString& NodeId, const FString& PropertyName, const FString& ValueJson, const FString& GraphType, int32 MaxGraphNodes)
{
    auto Args = GraphArgs(MaterialPath, GraphType);
    Args->SetStringField(TEXT("node_id"), NodeId);
    Args->SetStringField(TEXT("property_name"), PropertyName);
    Args->SetStringField(TEXT("value_json"), ValueJson);
    return RunMaterialOperation(EOperation::SetNodeProperty, Args, MaxGraphNodes);
}

FString UCubelessMaterialLibrary::AddCustomMaterialNode(const FString& MaterialPath, const FString& Code, const FString& OutputType, const FString& InputsJson, const FString& Description, const FString& GraphType, int32 NodeX, int32 NodeY, const FString& AdditionalDefinesJson, const FString& IncludeFilePathsJson, int32 MaxGraphNodes)
{
    auto Args = GraphArgs(MaterialPath, GraphType);
    Args->SetStringField(TEXT("code"), Code);
    Args->SetStringField(TEXT("output_type"), OutputType);
    Args->SetStringField(TEXT("inputs_json"), InputsJson);
    Args->SetStringField(TEXT("description"), Description);
    Args->SetNumberField(TEXT("node_x"), NodeX);
    Args->SetNumberField(TEXT("node_y"), NodeY);
    Args->SetStringField(TEXT("additional_defines_json"), AdditionalDefinesJson);
    Args->SetStringField(TEXT("include_file_paths_json"), IncludeFilePathsJson);
    return RunMaterialOperation(EOperation::AddCustomNode, Args, MaxGraphNodes);
}
