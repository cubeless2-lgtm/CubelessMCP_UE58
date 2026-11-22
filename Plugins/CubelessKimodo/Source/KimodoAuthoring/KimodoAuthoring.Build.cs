using UnrealBuildTool;
public class KimodoAuthoring : ModuleRules
{
    public KimodoAuthoring(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new[] {"Core", "CoreUObject", "Engine"});
        PrivateDependencyModuleNames.AddRange(new[] {"AnimGraph","Slate","SlateCore","PropertyEditor","UnrealEd",
            "ToolMenus","PythonScriptPlugin","WorkspaceMenuStructure"});
    }
}

