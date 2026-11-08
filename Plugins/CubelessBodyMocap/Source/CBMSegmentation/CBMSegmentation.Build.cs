// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CBMSegmentation : ModuleRules
{
	public CBMSegmentation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"ImageCore",
				"NNE"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
			}
		);
	}
}
