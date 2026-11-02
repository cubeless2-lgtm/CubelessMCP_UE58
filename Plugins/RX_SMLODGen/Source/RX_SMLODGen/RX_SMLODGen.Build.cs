using UnrealBuildTool;

public class RX_SMLODGen : ModuleRules
{
    public RX_SMLODGen(ReadOnlyTargetRules Target) : base(Target)
    {
        // Editor-only LOD workspace, stable preview ticking and local Korean tooltips.
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "UnrealEd", "Slate", "SlateCore", "InputCore",
            "ToolMenus", "ContentBrowser", "AssetRegistry", "StaticMeshEditor",
            "MeshDescription", "StaticMeshDescription", "MeshReductionInterface", "MeshUtilitiesCommon",
            "RenderCore", "RHI", "Json", "JsonUtilities", "PropertyEditor", "DesktopPlatform"
        });
    }
}
