using UnrealBuildTool;

public class RX_SkeletalVolumeLOD : ModuleRules
{
    public RX_SkeletalVolumeLOD(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "UnrealEd", "AssetRegistry", "ContentBrowser", "ToolMenus",
            "Slate", "SlateCore", "RenderCore", "RHI", "AnimationCore", "MeshDescription",
            "StaticMeshDescription", "SkeletalMeshDescription", "MeshConversion", "GeometryCore",
            "DynamicMesh", "GeometryAlgorithms", "ModelingOperatorsEditorOnly", "MaterialBaking",
            "Json", "JsonUtilities"
        });
    }
}
