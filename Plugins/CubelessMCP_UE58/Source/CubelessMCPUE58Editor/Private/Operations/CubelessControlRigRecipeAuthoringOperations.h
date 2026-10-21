#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Rigs/RigHierarchyElements.h"
namespace Cubeless::Operations::ControlRigRecipeAuthoring
{

enum class EHHControlRigIssueSeverity : uint8
{
	Info,
	Warning,
	Error
};
struct FHHControlRigValidationIssue
{
	EHHControlRigIssueSeverity Severity = EHHControlRigIssueSeverity::Error;
	FString Code;
	FString Message;
	FString FieldPath;
};
struct FHHControlRigBuildOptions
{
	bool bDryRun = true;
	bool bAllowTempPath = true;
	bool bAllowSamplePath = true;
	bool bAllowSave = false;
	bool bAllowArrayResize = false;
	bool bAllowLinkedPinPropagation = false;
	bool bAllowCastNodeCreation = false;
	int32 MaxImportBones = 256;
};
enum class EHHControlRigControlValueType : uint8
{
	Float,
	Bool,
	EulerTransform
};
struct FHHControlRigTransformSpec
{
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	FVector Scale = FVector::OneVector;
	bool bTransformInGlobal = true;
};
struct FHHControlRigCurveSpec
{
	FString Name;
	float Value = 0.0f;
};
struct FHHControlRigNullSpec
{
	FString Name;
	FString Parent;
	FHHControlRigTransformSpec Transform;
};
struct FHHControlRigControlSpec
{
	FString Name;
	FString Parent;
	EHHControlRigControlValueType ValueType = EHHControlRigControlValueType::Float;
	float FloatValue = 0.0f;
	bool bBoolValue = false;
	FHHControlRigTransformSpec EulerTransformValue;
	FHHControlRigTransformSpec OffsetTransform;
	FHHControlRigTransformSpec ShapeTransform;
	FString ShapeName;
	FLinearColor ShapeColor = FLinearColor::White;
	bool bShapeVisible = true;
};
struct FHHControlRigHierarchySpec
{
	bool bImportPreviewBones = true;
	int32 MaxImportBones = 256;
	TArray<FString> Nulls;
	TArray<FString> Controls;
	TArray<FString> Curves;
	TArray<FHHControlRigNullSpec> NullSpecs;
	TArray<FHHControlRigControlSpec> ControlSpecs;
	TArray<FHHControlRigCurveSpec> CurveSpecs;
};
struct FHHControlRigUnitSpec
{
	FString StructName;
	FString NodeName;
	FVector2D Position = FVector2D::ZeroVector;
};
struct FHHControlRigPinDefaultSpec
{
	FString NodeName;
	FString PinPath;
	FString DefaultValue;
	bool bAllowArrayResize = false;
	bool bAllowLinkedPinPropagation = false;
};
struct FHHControlRigLinkSpec
{
	FString SourcePinPath;
	FString TargetPinPath;
	bool bAllowCastNodeCreation = false;
};
struct FHHControlRigRigVMSpec
{
	TArray<FHHControlRigUnitSpec> Units;
	TArray<FString> Variables;
	TArray<FString> TemplateNodes;
	TArray<FString> FunctionReferences;
	TArray<FHHControlRigPinDefaultSpec> PinDefaults;
	TArray<FHHControlRigLinkSpec> Links;
	TArray<FString> ExposedPins;
};
struct FHHControlRigAnimBPInputDefaultSpec
{
	FString Name;
	FString ValueJson;
};
struct FHHControlRigAnimBPBindingSpec
{
	bool bEnabled = false;
	FString TargetAnimBPPath;
	FString SourceAnimBPPath;
	FString InsertMode = TEXT("input_pose_to_controlrig_to_root");
	TArray<FString> ExposeInputPins;
	TArray<FHHControlRigAnimBPInputDefaultSpec> InputDefaults;
	bool bDisconnectExistingInputLinks = false;
	bool bAllowExistingTargetReuse = true;
	bool bOverwriteExisting = false;
	bool bReplaceExistingLinks = true;
};
struct FHHControlRigVerificationSpec
{
	bool bCompile = true;
	bool bSave = false;
	bool bInspectTopology = true;
	bool bDirectSolve = true;
	bool bDependencyAudit = true;
};
struct FHHControlRigRecipe
{
	int32 SchemaVersion = 1;
	FString TargetControlRigPath;
	FString SourceSkeletalMesh;
	FString PreviewSkeletalMesh;
	FHHControlRigHierarchySpec Hierarchy;
	FHHControlRigRigVMSpec RigVM;
	FHHControlRigAnimBPBindingSpec AnimBPBinding;
	FHHControlRigVerificationSpec Verification;
};
struct FHHControlRigBuildReport
{
	bool bSuccess = false;
	bool bPluginUnavailable = false;
	bool bDryRun = true;
	bool bSampleOnly = true;
	FString TargetControlRigPath;
	bool bCreated = false;
	bool bReused = false;
	bool bTargetModified = false;
	bool bPartialTargetModified = false;
	bool bCompileRequested = false;
	bool bCompileSucceeded = false;
	bool bSaveRequested = false;
	bool bSaveSucceeded = false;
	bool bDependencyAuditPassed = false;
	TArray<FHHControlRigValidationIssue> Issues;
	TArray<FString> Warnings;
	TArray<FString> PlannedWrites;
	TArray<FString> PerformedWrites;
	TArray<FString> SkippedWrites;
	TArray<FString> ReusedItems;
	TArray<FString> CleanupSuggestions;
	int32 CreatedHierarchyElements = 0;
	int32 CreatedRigVMNodes = 0;
	int32 AppliedPinDefaults = 0;
	int32 AppliedLinks = 0;
	FString ReadbackSummary;
	FString NextAction;
};


FString NormalizeUnitStructName(FString StructName);
FString DefaultUnitNodeName(const FString& NormalizedStructName);
FString MakeObjectPathFromPackagePath(const FString& PackagePath);
bool ContainsForbiddenMcpReference(const FString& Value);
void AddIssue(FHHControlRigBuildReport& Report,EHHControlRigIssueSeverity Severity,const FString& Code,const FString& Message,const FString& FieldPath);
bool HasError(const FHHControlRigBuildReport& Report);
void AddReportEntry(TArray<FString>& Entries,const FString& Entry);
FString FormatReportEntry(const FString& Verb,const FString& Subject);
void AddTargetCleanupGuidance(FHHControlRigBuildReport& Report);
void MarkNoMutationStop(FHHControlRigBuildReport& Report,const FString& Summary,const FString& NextAction);
void MarkBuildFailure(FHHControlRigBuildReport& Report,const FString& Summary);
FHHControlRigBuildReport MakeBaseReport(const FHHControlRigRecipe& Recipe,const FHHControlRigBuildOptions& Options);
bool ParseRecipeJson(const FString& RecipeJson,FHHControlRigRecipe& OutRecipe,FString& OutError);
bool ParseBuildOptionsJson(const FString& OptionsJson,FHHControlRigBuildOptions& OutOptions,FString& OutError);
FTransform ToTransform(const FHHControlRigTransformSpec& TransformSpec);
FEulerTransform ToEulerTransform(const FHHControlRigTransformSpec& TransformSpec);
bool ResolveAuthoredParentKey(const FString& ParentName,const TMap<FString,FRigElementKey>& AuthoredParentKeys,FRigElementKey& OutParentKey,FHHControlRigBuildReport& Report,const FString& FieldPath);
FRigControlSettings MakeControlSettings(const FString& ControlName,const FHHControlRigControlSpec& Spec);
FRigControlValue MakeControlValue(const FHHControlRigControlSpec& Spec);
TSharedRef<FJsonObject> ReportToObject(const FHHControlRigBuildReport& Report);
FHHControlRigBuildReport ValidateRecipe(const FHHControlRigRecipe& Recipe,const FHHControlRigBuildOptions& Options);
FHHControlRigBuildReport DryRunRecipe(const FHHControlRigRecipe& Recipe,const FHHControlRigBuildOptions& Options);
}
