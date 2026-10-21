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
            "InterchangeEngine", "MaterialEditor", "RHI", "RenderCore", "EditorFramework", "BlueprintGraph", "BlueprintEditorLibrary", "KismetCompiler", "Kismet", "Slate", "SlateCore", "Merge", "PropertyEditor", "JsonUtilities", "GameplayTags", "InputCore", "PCG", "Niagara", "NiagaraCore", "NiagaraShader", "NiagaraEditor", "LevelSequence", "UniversalObjectLocator", "MovieScene", "MovieSceneTracks", "AIModule", "BehaviorTreeEditor", "AIGraph", "AssetTools", "AudioEditor", "MetasoundEngine", "MetasoundEditor", "MetasoundFrontend", "MetasoundStandardNodes", "SourceControl", "PythonScriptPlugin", "PhysicsUtilities", "UMG", "UMGEditor", "PropertyPath", "AnimGraph", "PhysicsCore", "AnimGraphRuntime", "ControlRigDeveloper", "ControlRig", "ControlRigEditor", "RigVMDeveloper", "RigVM", "StateTreeModule", "StateTreeEditorModule", "GameplayStateTreeModule", "PropertyBindingUtils", "Foliage", "Landscape", "LevelEditor", "Chaos", "ChaosSolverEngine", "ChaosClothAsset", "ChaosClothAssetEngine", "ChaosClothAssetDataflowNodes", "DataflowCore", "DataflowEngine", "DataflowNodes", "DataflowSimulation", "ClothingSystemRuntimeInterface", "InteractiveToolsFramework", "ChaosClothAssetTools", "ChaosCloth", "ClothingSystemRuntimeCommon", "ClothingSystemEditorInterface", "ChaosCaching", "GeometryCollectionEngine", "FieldSystemEngine", "FractureEngine", "MeshDescription"
        });
        DynamicallyLoadedModuleNames.Add("ClothingSystemEditor");
    }
}
