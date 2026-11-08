// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CBMInference : ModuleRules
{
	public CBMInference(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "ImageCore",
                "CoreUObject",
                
                
                "CBMSegmentation"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "AssetRegistry",
                "Engine",
                "Json",
                "NNE",
                "RenderCore",
                "RHI",
                "ImageCore"
            }
        );
    }
}
