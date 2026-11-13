using UnrealBuildTool;
public class CBMEditor : ModuleRules {
 public CBMEditor(ReadOnlyTargetRules Target) : base(Target) {
 PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
 PublicDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine","IKRig"});
 PrivateDependencyModuleNames.AddRange(new[]{"InputCore","DesktopPlatform","MediaAssets","AdvancedPreviewScene","RenderCore","RHI"});
 PrivateDependencyModuleNames.AddRange(new[]{"MainFrame","CBMInference","CBMSegmentation","CBMOptimizer","NNE","ImageCore","ImageWrapper","Eigen","UnrealEd","Slate","SlateCore","ToolMenus","PropertyEditor","AssetRegistry","IKRigEditor","Projects","Json","JsonUtilities"});
 }
}
