using UnrealBuildTool;

public class CubelessMCPUE58Editor : ModuleRules
{
    public CubelessMCPUE58Editor(ReadOnlyTargetRules Target) : base(Target)
    {
        if (Target.Type != TargetType.Editor)
        {
            throw new BuildException("CubelessMCPUE58Editor is an editor-only module.");
        }
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "UnrealEd", "AssetRegistry",
            "Projects", "Json", "ToolsetRegistry", "EditorScriptingUtilities",
            "MaterialEditor", "RHI", "EditorFramework", "BlueprintGraph", "KismetCompiler", "JsonUtilities"
        });
    }
}
