// HH original pure DTO/parser/validation/report work; no UObject worker or server.
#include "Operations/CubelessControlRigRecipeAuthoringOperations.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
namespace Cubeless::Operations::ControlRigRecipeAuthoring
{
constexpr int32 SupportedSchemaVersion = 1;

const TCHAR* const TempRoot = TEXT("/Game/_MCP_Temp/");
const TCHAR* const SampleRoot = TEXT("/Game/_MCP_Sample/");

const TSet<FString>& AllowedUnitStructs()
{
	static const TSet<FString> Allowed =
	{
		TEXT("FRigUnit_BeginExecution"),
		TEXT("FRigUnit_PreBeginExecution"),
		TEXT("FRigUnit_PostBeginExecution"),
		TEXT("FRigUnit_PrepareForExecution"),
		TEXT("FRigUnit_PostPrepareForExecution"),
		TEXT("FRigUnit_InverseExecution"),
		TEXT("FRigUnit_SetControlFloat")
	};
	return Allowed;
}

FString MakeObjectPathFromPackagePath(const FString& PackagePath)
{
	const FString AssetName = FPackageName::GetLongPackageAssetName(PackagePath);
	return FString::Printf(TEXT("%s.%s"), *PackagePath, *AssetName);
}

FString NormalizeUnitStructName(FString StructName)
{
	StructName.TrimStartAndEndInline();

	FString RightSide;
	if (StructName.Split(TEXT("."), nullptr, &RightSide, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
	{
		StructName = RightSide;
	}

	if (StructName.StartsWith(TEXT("RigUnit_")))
	{
		StructName = FString::Printf(TEXT("F%s"), *StructName);
	}

	return StructName;
}

FString DefaultUnitNodeName(const FString& NormalizedStructName)
{
	if (NormalizedStructName == TEXT("FRigUnit_PreBeginExecution"))
	{
		return TEXT("CB_PreBeginExecution");
	}
	if (NormalizedStructName == TEXT("FRigUnit_PostBeginExecution"))
	{
		return TEXT("CB_PostBeginExecution");
	}
	if (NormalizedStructName == TEXT("FRigUnit_PrepareForExecution"))
	{
		return TEXT("CB_PrepareForExecution");
	}
	if (NormalizedStructName == TEXT("FRigUnit_PostPrepareForExecution"))
	{
		return TEXT("CB_PostPrepareForExecution");
	}
	if (NormalizedStructName == TEXT("FRigUnit_InverseExecution"))
	{
		return TEXT("CB_InverseExecution");
	}
	if (NormalizedStructName == TEXT("FRigUnit_SetControlFloat"))
	{
		return TEXT("CB_SetControlFloat");
	}
	return TEXT("CB_BeginExecution");
}





bool ContainsForbiddenMcpReference(const FString& Value)
{
	static const FString ForbiddenTokens[] =
	{
		TEXT("/Script/UnrealMCP"),
		TEXT("/UnrealMCP"),
		TEXT("UnrealMCP"),
		TEXT("MCPUnreal"),
		TEXT("mcp_unreal"),
		TEXT("/Script/CubelessControlRigTools"),
		TEXT("CubelessControlRigTools"),
		TEXT("CubelessControlRigToolsEditor"), TEXT("CubelessMCP"), TEXT("cubeless_mcp"), TEXT("ToolsetRegistry"), TEXT("ModelContextProtocol"), TEXT("/Engine/PythonTypes")
	};

	for (const FString& Token : ForbiddenTokens)
	{
		if (Value.Contains(Token, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

void AddIssue(
	FHHControlRigBuildReport& Report,
	EHHControlRigIssueSeverity Severity,
	const FString& Code,
	const FString& Message,
	const FString& FieldPath)
{
	FHHControlRigValidationIssue Issue;
	Issue.Severity = Severity;
	Issue.Code = Code;
	Issue.Message = Message;
	Issue.FieldPath = FieldPath;
	Report.Issues.Add(Issue);

	if (Severity == EHHControlRigIssueSeverity::Warning)
	{
		Report.Warnings.Add(Message);
	}
}

bool HasError(const FHHControlRigBuildReport& Report)
{
	for (const FHHControlRigValidationIssue& Issue : Report.Issues)
	{
		if (Issue.Severity == EHHControlRigIssueSeverity::Error)
		{
			return true;
		}
	}

	return false;
}

void AddReportEntry(TArray<FString>& Entries, const FString& Entry)
{
	const FString TrimmedEntry = Entry.TrimStartAndEnd();
	if (!TrimmedEntry.IsEmpty())
	{
		Entries.AddUnique(TrimmedEntry);
	}
}

FString FormatReportEntry(const FString& Verb, const FString& Subject)
{
	return Subject.IsEmpty()
		? Verb
		: FString::Printf(TEXT("%s:%s"), *Verb, *Subject);
}

void AddTargetCleanupGuidance(FHHControlRigBuildReport& Report)
{
	if (Report.TargetControlRigPath.IsEmpty())
	{
		return;
	}

	AddReportEntry(
		Report.CleanupSuggestions,
		FormatReportEntry(TEXT("inspect_sample_temp_target"), Report.TargetControlRigPath));
	AddReportEntry(
		Report.CleanupSuggestions,
		FormatReportEntry(TEXT("delete_or_recreate_sample_temp_target_after_review"), Report.TargetControlRigPath));
	AddReportEntry(
		Report.CleanupSuggestions,
		TEXT("rerun_validate_controlrig_recipe_before_retry"));
}

void MarkNoMutationStop(
	FHHControlRigBuildReport& Report,
	const FString& Summary,
	const FString& NextAction)
{
	Report.bTargetModified = false;
	Report.bPartialTargetModified = false;
	Report.bSuccess = false;
	Report.ReadbackSummary = Summary;
	Report.NextAction = NextAction;
	AddReportEntry(Report.SkippedWrites, TEXT("asset_mutation_not_started"));
}

void MarkBuildFailure(
	FHHControlRigBuildReport& Report,
	const FString& Summary)
{
	Report.bPartialTargetModified = Report.bTargetModified;
	Report.bSuccess = false;
	Report.ReadbackSummary = Summary;

	if (Report.bPartialTargetModified)
	{
		Report.NextAction = TEXT("inspect_then_cleanup_or_retry_sample_temp_target");
		AddTargetCleanupGuidance(Report);
	}
	else
	{
		Report.NextAction = TEXT("fix_recipe_and_rerun_build_controlrig_recipe");
		AddReportEntry(Report.SkippedWrites, TEXT("asset_mutation_not_started"));
	}
}

bool IsAllowedTargetPath(const FString& Path, const FHHControlRigBuildOptions& Options)
{
	return (Options.bAllowTempPath && Path.StartsWith(TempRoot)) ||
		(Options.bAllowSamplePath && Path.StartsWith(SampleRoot));
}

void ValidateAssetPath(
	FHHControlRigBuildReport& Report,
	const FString& Path,
	const FString& FieldPath,
	bool bRequired)
{
	if (Path.IsEmpty())
	{
		if (bRequired)
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("missing_asset_path"),
				FString::Printf(TEXT("%s is required."), *FieldPath),
				FieldPath);
		}
		return;
	}

	FText ObjectPathFailure;
	FText PackagePathFailure;
	if (!FPackageName::IsValidObjectPath(Path, &ObjectPathFailure) &&
		!FPackageName::IsValidLongPackageName(Path, false, &PackagePathFailure))
	{
		AddIssue(
			Report,
			EHHControlRigIssueSeverity::Error,
			TEXT("invalid_asset_path"),
			FString::Printf(TEXT("%s is not a valid Unreal object or package path."), *Path),
			FieldPath);
	}

	if (ContainsForbiddenMcpReference(Path))
	{
		AddIssue(
			Report,
			EHHControlRigIssueSeverity::Error,
			TEXT("forbidden_mcp_reference"),
			FString::Printf(TEXT("%s contains an UnrealMCP-only reference."), *Path),
			FieldPath);
	}
}

void ValidateNameArray(
	FHHControlRigBuildReport& Report,
	const TArray<FString>& Names,
	const FString& FieldPath)
{
	TSet<FString> Seen;
	for (int32 Index = 0; Index < Names.Num(); ++Index)
	{
		const FString ItemPath = FString::Printf(TEXT("%s[%d]"), *FieldPath, Index);
		const FString Name = Names[Index].TrimStartAndEnd();
		if (Name.IsEmpty())
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("empty_name"),
				FString::Printf(TEXT("%s must not be empty."), *ItemPath),
				ItemPath);
			continue;
		}

		if (Seen.Contains(Name))
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("duplicate_name"),
				FString::Printf(TEXT("%s is duplicated."), *Name),
				ItemPath);
		}
		Seen.Add(Name);

		if (ContainsForbiddenMcpReference(Name))
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("forbidden_mcp_reference"),
				FString::Printf(TEXT("%s contains an UnrealMCP-only reference."), *Name),
				ItemPath);
		}
	}
}

void ValidateSpecName(
	FHHControlRigBuildReport& Report,
	const FString& Name,
	const FString& FieldPath,
	TSet<FString>& Seen)
{
	const FString TrimmedName = Name.TrimStartAndEnd();
	if (TrimmedName.IsEmpty())
	{
		AddIssue(
			Report,
			EHHControlRigIssueSeverity::Error,
			TEXT("empty_name"),
			FString::Printf(TEXT("%s must not be empty."), *FieldPath),
			FieldPath);
		return;
	}

	if (Seen.Contains(TrimmedName))
	{
		AddIssue(
			Report,
			EHHControlRigIssueSeverity::Error,
			TEXT("duplicate_name"),
			FString::Printf(TEXT("%s is duplicated."), *TrimmedName),
			FieldPath);
	}
	Seen.Add(TrimmedName);

	if (ContainsForbiddenMcpReference(TrimmedName))
	{
		AddIssue(
			Report,
			EHHControlRigIssueSeverity::Error,
			TEXT("forbidden_mcp_reference"),
			FString::Printf(TEXT("%s contains an UnrealMCP-only reference."), *TrimmedName),
			FieldPath);
	}
}

void ValidateOptionalParent(
	FHHControlRigBuildReport& Report,
	const FString& Parent,
	const FString& FieldPath)
{
	const FString TrimmedParent = Parent.TrimStartAndEnd();
	if (!TrimmedParent.IsEmpty() && ContainsForbiddenMcpReference(TrimmedParent))
	{
		AddIssue(
			Report,
			EHHControlRigIssueSeverity::Error,
			TEXT("forbidden_mcp_reference"),
			FString::Printf(TEXT("%s contains an UnrealMCP-only reference."), *TrimmedParent),
			FieldPath);
	}
}

void ValidateCurveSpecs(
	FHHControlRigBuildReport& Report,
	const TArray<FString>& LegacyNames,
	const TArray<FHHControlRigCurveSpec>& Specs)
{
	TSet<FString> Seen;
	for (const FString& LegacyName : LegacyNames)
	{
		const FString TrimmedName = LegacyName.TrimStartAndEnd();
		if (!TrimmedName.IsEmpty())
		{
			Seen.Add(TrimmedName);
		}
	}

	for (int32 Index = 0; Index < Specs.Num(); ++Index)
	{
		ValidateSpecName(
			Report,
			Specs[Index].Name,
			FString::Printf(TEXT("hierarchy.curve_specs[%d].name"), Index),
			Seen);
	}
}

void ValidateNullSpecs(
	FHHControlRigBuildReport& Report,
	const TArray<FString>& LegacyNames,
	const TArray<FHHControlRigNullSpec>& Specs)
{
	TSet<FString> Seen;
	for (const FString& LegacyName : LegacyNames)
	{
		const FString TrimmedName = LegacyName.TrimStartAndEnd();
		if (!TrimmedName.IsEmpty())
		{
			Seen.Add(TrimmedName);
		}
	}

	for (int32 Index = 0; Index < Specs.Num(); ++Index)
	{
		const FString SpecPath = FString::Printf(TEXT("hierarchy.null_specs[%d]"), Index);
		ValidateSpecName(Report, Specs[Index].Name, SpecPath + TEXT(".name"), Seen);
		ValidateOptionalParent(Report, Specs[Index].Parent, SpecPath + TEXT(".parent"));
	}
}

void ValidateControlSpecs(
	FHHControlRigBuildReport& Report,
	const TArray<FString>& LegacyNames,
	const TArray<FHHControlRigControlSpec>& Specs)
{
	TSet<FString> Seen;
	for (const FString& LegacyName : LegacyNames)
	{
		const FString TrimmedName = LegacyName.TrimStartAndEnd();
		if (!TrimmedName.IsEmpty())
		{
			Seen.Add(TrimmedName);
		}
	}

	for (int32 Index = 0; Index < Specs.Num(); ++Index)
	{
		const FString SpecPath = FString::Printf(TEXT("hierarchy.control_specs[%d]"), Index);
		ValidateSpecName(Report, Specs[Index].Name, SpecPath + TEXT(".name"), Seen);
		ValidateOptionalParent(Report, Specs[Index].Parent, SpecPath + TEXT(".parent"));
		if (ContainsForbiddenMcpReference(Specs[Index].ShapeName))
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("forbidden_mcp_reference"),
				TEXT("Control shape_name contains an UnrealMCP-only reference."),
				SpecPath + TEXT(".shape_name"));
		}
	}
}

void ValidateAnimBPBinding(
	FHHControlRigBuildReport& Report,
	const FHHControlRigRecipe& Recipe,
	const FHHControlRigBuildOptions& Options)
{
	if (!Recipe.AnimBPBinding.bEnabled)
	{
		return;
	}

	ValidateAssetPath(Report, Recipe.AnimBPBinding.SourceAnimBPPath, TEXT("animbp_binding.source_anim_bp_path"), true);

	if (Recipe.AnimBPBinding.TargetAnimBPPath.TrimStartAndEnd().IsEmpty())
	{
		AddIssue(
			Report,
			EHHControlRigIssueSeverity::Error,
			TEXT("missing_target_anim_bp_path"),
			TEXT("animbp_binding.target_anim_bp_path is required when AnimBP binding is enabled."),
			TEXT("animbp_binding.target_anim_bp_path"));
	}
	else
	{
		FText TargetPathFailure;
		if (!FPackageName::IsValidLongPackageName(Recipe.AnimBPBinding.TargetAnimBPPath, false, &TargetPathFailure))
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("invalid_target_anim_bp_path"),
				TEXT("animbp_binding.target_anim_bp_path must be a long package path under /Game/_MCP_Temp or /Game/_MCP_Sample."),
				TEXT("animbp_binding.target_anim_bp_path"));
		}
		else if (!IsAllowedTargetPath(Recipe.AnimBPBinding.TargetAnimBPPath, Options))
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("target_anim_bp_path_not_sample_or_temp"),
				TEXT("AnimBP binding targets must stay under /Game/_MCP_Temp or /Game/_MCP_Sample."),
				TEXT("animbp_binding.target_anim_bp_path"));
		}
	}

	if (!Recipe.AnimBPBinding.SourceAnimBPPath.TrimStartAndEnd().IsEmpty() &&
		Recipe.AnimBPBinding.SourceAnimBPPath == Recipe.AnimBPBinding.TargetAnimBPPath)
	{
		AddIssue(
			Report,
			EHHControlRigIssueSeverity::Error,
			TEXT("source_target_anim_bp_same"),
			TEXT("source_anim_bp_path and target_anim_bp_path must be different."),
			TEXT("animbp_binding"));
	}

	const FString InsertMode = Recipe.AnimBPBinding.InsertMode.TrimStartAndEnd();
	if (!InsertMode.IsEmpty() && InsertMode != TEXT("input_pose_to_controlrig_to_root"))
	{
		AddIssue(
			Report,
			EHHControlRigIssueSeverity::Error,
			TEXT("unsupported_animbp_insert_mode"),
			TEXT("Only input_pose_to_controlrig_to_root is supported for recipe-planned AnimBP binding."),
			TEXT("animbp_binding.insert_mode"));
	}

	if (ContainsForbiddenMcpReference(Recipe.AnimBPBinding.SourceAnimBPPath) ||
		ContainsForbiddenMcpReference(Recipe.AnimBPBinding.TargetAnimBPPath) ||
		ContainsForbiddenMcpReference(Recipe.AnimBPBinding.InsertMode))
	{
		AddIssue(
			Report,
			EHHControlRigIssueSeverity::Error,
			TEXT("forbidden_mcp_reference"),
			TEXT("AnimBP binding contains an UnrealMCP-only reference."),
			TEXT("animbp_binding"));
	}

	if (Recipe.AnimBPBinding.ExposeInputPins.Num() > 32)
	{
		AddIssue(
			Report,
			EHHControlRigIssueSeverity::Error,
			TEXT("too_many_animbp_input_pins"),
			TEXT("animbp_binding.expose_input_pins is limited to 32 names."),
			TEXT("animbp_binding.expose_input_pins"));
	}

	ValidateNameArray(Report, Recipe.AnimBPBinding.ExposeInputPins, TEXT("animbp_binding.expose_input_pins"));

	if (Recipe.AnimBPBinding.InputDefaults.Num() > 32)
	{
		AddIssue(
			Report,
			EHHControlRigIssueSeverity::Error,
			TEXT("too_many_animbp_input_defaults"),
			TEXT("animbp_binding.input_defaults is limited to 32 entries."),
			TEXT("animbp_binding.input_defaults"));
	}

	TSet<FString> SeenInputDefaults;
	for (int32 Index = 0; Index < Recipe.AnimBPBinding.InputDefaults.Num(); ++Index)
	{
		const FHHControlRigAnimBPInputDefaultSpec& InputDefault = Recipe.AnimBPBinding.InputDefaults[Index];
		const FString FieldPath = FString::Printf(TEXT("animbp_binding.input_defaults[%d]"), Index);
		ValidateSpecName(Report, InputDefault.Name, FieldPath + TEXT(".name"), SeenInputDefaults);
		if (InputDefault.ValueJson == TEXT("<unsupported>"))
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("unsupported_animbp_input_default_value"),
				TEXT("AnimBP binding input defaults currently support only strings, numbers, booleans, and simple arrays."),
				FieldPath + TEXT(".value"));
		}
		if (ContainsForbiddenMcpReference(InputDefault.ValueJson))
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("forbidden_mcp_reference"),
				TEXT("AnimBP binding input default contains an UnrealMCP-only reference."),
				FieldPath + TEXT(".value"));
		}
	}

	AddIssue(
		Report,
		EHHControlRigIssueSeverity::Info,
		TEXT("animbp_binding_validated_for_orchestration"),
		TEXT("AnimBP binding was validated for external orchestration through ensure_anim_graph_controlrig_node_sample and set_anim_graph_controlrig_input_defaults; BuildRecipe does not mutate AnimBPs yet."),
		TEXT("animbp_binding"));
}



FHHControlRigBuildReport MakeBaseReport(
	const FHHControlRigRecipe& Recipe,
	const FHHControlRigBuildOptions& Options)
{
	FHHControlRigBuildReport Report;
	Report.bDryRun = Options.bDryRun;
	Report.bSampleOnly = true;
	Report.TargetControlRigPath = Recipe.TargetControlRigPath;
	Report.bCompileRequested = Recipe.Verification.bCompile;
	Report.bSaveRequested = Recipe.Verification.bSave;
	Report.bDependencyAuditPassed = Recipe.Verification.bDependencyAudit;
	Report.NextAction = Options.bDryRun
		? TEXT("fix_recipe_or_build_sample_temp_after_validation")
		: TEXT("complete_controlrig_recipe_transaction");

	AddReportEntry(
		Report.PlannedWrites,
		FormatReportEntry(TEXT("create_or_reuse_control_rig"), Recipe.TargetControlRigPath));

	if (!Recipe.PreviewSkeletalMesh.IsEmpty())
	{
		AddReportEntry(
			Report.PlannedWrites,
			FormatReportEntry(TEXT("set_preview_mesh"), Recipe.PreviewSkeletalMesh));
	}
	if (Recipe.Hierarchy.bImportPreviewBones)
	{
		AddReportEntry(
			Report.PlannedWrites,
			FString::Printf(TEXT("import_preview_bones:max=%d"), Recipe.Hierarchy.MaxImportBones));
	}
	for (const FString& CurveName : Recipe.Hierarchy.Curves)
	{
		AddReportEntry(Report.PlannedWrites, FormatReportEntry(TEXT("add_curve"), CurveName.TrimStartAndEnd()));
	}
	for (const FHHControlRigCurveSpec& CurveSpec : Recipe.Hierarchy.CurveSpecs)
	{
		AddReportEntry(Report.PlannedWrites, FormatReportEntry(TEXT("add_curve"), CurveSpec.Name.TrimStartAndEnd()));
	}
	for (const FString& NullName : Recipe.Hierarchy.Nulls)
	{
		AddReportEntry(Report.PlannedWrites, FormatReportEntry(TEXT("add_null"), NullName.TrimStartAndEnd()));
	}
	for (const FHHControlRigNullSpec& NullSpec : Recipe.Hierarchy.NullSpecs)
	{
		AddReportEntry(Report.PlannedWrites, FormatReportEntry(TEXT("add_null"), NullSpec.Name.TrimStartAndEnd()));
	}
	for (const FString& ControlName : Recipe.Hierarchy.Controls)
	{
		AddReportEntry(Report.PlannedWrites, FormatReportEntry(TEXT("add_control"), ControlName.TrimStartAndEnd()));
	}
	for (const FHHControlRigControlSpec& ControlSpec : Recipe.Hierarchy.ControlSpecs)
	{
		AddReportEntry(Report.PlannedWrites, FormatReportEntry(TEXT("add_control"), ControlSpec.Name.TrimStartAndEnd()));
	}
	for (const FHHControlRigUnitSpec& Unit : Recipe.RigVM.Units)
	{
		const FString NormalizedStructName = NormalizeUnitStructName(Unit.StructName);
		const FString NodeName = Unit.NodeName.TrimStartAndEnd().IsEmpty()
			? DefaultUnitNodeName(NormalizedStructName)
			: Unit.NodeName.TrimStartAndEnd();
		AddReportEntry(Report.PlannedWrites, FormatReportEntry(TEXT("add_rigvm_unit"), NodeName));
	}
	for (const FHHControlRigPinDefaultSpec& PinDefault : Recipe.RigVM.PinDefaults)
	{
		AddReportEntry(Report.PlannedWrites, FormatReportEntry(TEXT("set_pin_default"), PinDefault.PinPath));
	}
	for (const FHHControlRigLinkSpec& Link : Recipe.RigVM.Links)
	{
		AddReportEntry(
			Report.PlannedWrites,
			FString::Printf(TEXT("add_link:%s->%s"), *Link.SourcePinPath, *Link.TargetPinPath));
	}
	if (Recipe.Verification.bCompile)
	{
		AddReportEntry(
			Report.PlannedWrites,
			FormatReportEntry(TEXT("compile_control_rig"), Recipe.TargetControlRigPath));
	}
	if (Recipe.Verification.bDependencyAudit)
	{
		AddReportEntry(
			Report.PlannedWrites,
			FormatReportEntry(TEXT("dependency_audit"), Recipe.TargetControlRigPath));
	}
	if (Recipe.Verification.bSave)
	{
		AddReportEntry(
			Report.PlannedWrites,
			FormatReportEntry(TEXT("save_control_rig_package"), Recipe.TargetControlRigPath));
	}
	if (Recipe.AnimBPBinding.bEnabled)
	{
		AddReportEntry(
			Report.PlannedWrites,
			FormatReportEntry(TEXT("external_sample_anim_bp_binding"), Recipe.AnimBPBinding.TargetAnimBPPath));
	}

	return Report;
}

void ReadStringField(
	const TSharedRef<FJsonObject>& Object,
	const TCHAR* SnakeKey,
	const TCHAR* PascalKey,
	FString& OutValue)
{
	if (Object->TryGetStringField(SnakeKey, OutValue))
	{
		return;
	}
	Object->TryGetStringField(PascalKey, OutValue);
}

void ReadBoolField(
	const TSharedRef<FJsonObject>& Object,
	const TCHAR* SnakeKey,
	const TCHAR* PascalKey,
	bool& OutValue)
{
	bool Value = OutValue;
	if (Object->TryGetBoolField(SnakeKey, Value) || Object->TryGetBoolField(PascalKey, Value))
	{
		OutValue = Value;
	}
}

void ReadIntField(
	const TSharedRef<FJsonObject>& Object,
	const TCHAR* SnakeKey,
	const TCHAR* PascalKey,
	int32& OutValue)
{
	int32 Value = OutValue;
	if (Object->TryGetNumberField(SnakeKey, Value) || Object->TryGetNumberField(PascalKey, Value))
	{
		OutValue = Value;
	}
}

void ReadFloatField(
	const TSharedRef<FJsonObject>& Object,
	const TCHAR* SnakeKey,
	const TCHAR* PascalKey,
	float& OutValue)
{
	double Value = OutValue;
	if (Object->TryGetNumberField(SnakeKey, Value) || Object->TryGetNumberField(PascalKey, Value))
	{
		OutValue = static_cast<float>(Value);
	}
}

void ReadStringArrayField(
	const TSharedRef<FJsonObject>& Object,
	const TCHAR* SnakeKey,
	const TCHAR* PascalKey,
	TArray<FString>& OutValues)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object->TryGetArrayField(SnakeKey, Values) && !Object->TryGetArrayField(PascalKey, Values))
	{
		return;
	}

	OutValues.Reset();
	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		FString StringValue;
		if (Value.IsValid() && Value->TryGetString(StringValue))
		{
			OutValues.Add(StringValue);
		}
	}
}

FString JsonValueToSimpleString(const TSharedPtr<FJsonValue>& Value)
{
	if (!Value.IsValid() || Value->IsNull())
	{
		return TEXT("<unsupported>");
	}

	FString StringValue;
	if (Value->TryGetString(StringValue))
	{
		return StringValue;
	}

	double NumberValue = 0.0;
	if (Value->TryGetNumber(NumberValue))
	{
		return FString::SanitizeFloat(NumberValue);
	}

	bool BoolValue = false;
	if (Value->TryGetBool(BoolValue))
	{
		return BoolValue ? TEXT("true") : TEXT("false");
	}

	const TArray<TSharedPtr<FJsonValue>>* ArrayValues = nullptr;
	if (Value->TryGetArray(ArrayValues))
	{
		TArray<FString> Parts;
		for (const TSharedPtr<FJsonValue>& ArrayValue : *ArrayValues)
		{
			const FString Part = JsonValueToSimpleString(ArrayValue);
			if (Part == TEXT("<unsupported>"))
			{
				return TEXT("<unsupported>");
			}
			Parts.Add(Part);
		}
		return FString::Printf(TEXT("[%s]"), *FString::Join(Parts, TEXT(",")));
	}

	return TEXT("<unsupported>");
}

bool ReadVectorFromArray(const TArray<TSharedPtr<FJsonValue>>& Values, FVector& OutValue)
{
	if (Values.Num() < 3)
	{
		return false;
	}

	double X = OutValue.X;
	double Y = OutValue.Y;
	double Z = OutValue.Z;
	if (!Values[0].IsValid() || !Values[0]->TryGetNumber(X) ||
		!Values[1].IsValid() || !Values[1]->TryGetNumber(Y) ||
		!Values[2].IsValid() || !Values[2]->TryGetNumber(Z))
	{
		return false;
	}

	OutValue = FVector(X, Y, Z);
	return true;
}

bool ReadVectorFromObject(const TSharedRef<FJsonObject>& Object, FVector& OutValue)
{
	double X = OutValue.X;
	double Y = OutValue.Y;
	double Z = OutValue.Z;
	bool bRead = false;
	bRead |= Object->TryGetNumberField(TEXT("x"), X) || Object->TryGetNumberField(TEXT("X"), X);
	bRead |= Object->TryGetNumberField(TEXT("y"), Y) || Object->TryGetNumberField(TEXT("Y"), Y);
	bRead |= Object->TryGetNumberField(TEXT("z"), Z) || Object->TryGetNumberField(TEXT("Z"), Z);
	if (bRead)
	{
		OutValue = FVector(X, Y, Z);
	}
	return bRead;
}

bool ReadVectorField(
	const TSharedRef<FJsonObject>& Object,
	const TCHAR* SnakeKey,
	const TCHAR* PascalKey,
	FVector& OutValue)
{
	const TSharedPtr<FJsonObject>* VectorObject = nullptr;
	if (Object->TryGetObjectField(SnakeKey, VectorObject) || Object->TryGetObjectField(PascalKey, VectorObject))
	{
		return ReadVectorFromObject(VectorObject->ToSharedRef(), OutValue);
	}

	const TArray<TSharedPtr<FJsonValue>>* VectorArray = nullptr;
	if (Object->TryGetArrayField(SnakeKey, VectorArray) || Object->TryGetArrayField(PascalKey, VectorArray))
	{
		return ReadVectorFromArray(*VectorArray, OutValue);
	}

	return false;
}

bool ReadRotatorFromArray(const TArray<TSharedPtr<FJsonValue>>& Values, FRotator& OutValue)
{
	if (Values.Num() < 3)
	{
		return false;
	}

	double Pitch = OutValue.Pitch;
	double Yaw = OutValue.Yaw;
	double Roll = OutValue.Roll;
	if (!Values[0].IsValid() || !Values[0]->TryGetNumber(Pitch) ||
		!Values[1].IsValid() || !Values[1]->TryGetNumber(Yaw) ||
		!Values[2].IsValid() || !Values[2]->TryGetNumber(Roll))
	{
		return false;
	}

	OutValue = FRotator(Pitch, Yaw, Roll);
	return true;
}

bool ReadRotatorFromObject(const TSharedRef<FJsonObject>& Object, FRotator& OutValue)
{
	double Pitch = OutValue.Pitch;
	double Yaw = OutValue.Yaw;
	double Roll = OutValue.Roll;
	bool bRead = false;
	bRead |= Object->TryGetNumberField(TEXT("pitch"), Pitch) || Object->TryGetNumberField(TEXT("Pitch"), Pitch) ||
		Object->TryGetNumberField(TEXT("x"), Pitch) || Object->TryGetNumberField(TEXT("X"), Pitch);
	bRead |= Object->TryGetNumberField(TEXT("yaw"), Yaw) || Object->TryGetNumberField(TEXT("Yaw"), Yaw) ||
		Object->TryGetNumberField(TEXT("y"), Yaw) || Object->TryGetNumberField(TEXT("Y"), Yaw);
	bRead |= Object->TryGetNumberField(TEXT("roll"), Roll) || Object->TryGetNumberField(TEXT("Roll"), Roll) ||
		Object->TryGetNumberField(TEXT("z"), Roll) || Object->TryGetNumberField(TEXT("Z"), Roll);
	if (bRead)
	{
		OutValue = FRotator(Pitch, Yaw, Roll);
	}
	return bRead;
}

bool ReadRotatorField(
	const TSharedRef<FJsonObject>& Object,
	const TCHAR* SnakeKey,
	const TCHAR* PascalKey,
	FRotator& OutValue)
{
	const TSharedPtr<FJsonObject>* RotatorObject = nullptr;
	if (Object->TryGetObjectField(SnakeKey, RotatorObject) || Object->TryGetObjectField(PascalKey, RotatorObject))
	{
		return ReadRotatorFromObject(RotatorObject->ToSharedRef(), OutValue);
	}

	const TArray<TSharedPtr<FJsonValue>>* RotatorArray = nullptr;
	if (Object->TryGetArrayField(SnakeKey, RotatorArray) || Object->TryGetArrayField(PascalKey, RotatorArray))
	{
		return ReadRotatorFromArray(*RotatorArray, OutValue);
	}

	return false;
}

bool ReadLinearColorFromArray(const TArray<TSharedPtr<FJsonValue>>& Values, FLinearColor& OutValue)
{
	if (Values.Num() < 3)
	{
		return false;
	}

	double R = OutValue.R;
	double G = OutValue.G;
	double B = OutValue.B;
	double A = OutValue.A;
	if (!Values[0].IsValid() || !Values[0]->TryGetNumber(R) ||
		!Values[1].IsValid() || !Values[1]->TryGetNumber(G) ||
		!Values[2].IsValid() || !Values[2]->TryGetNumber(B))
	{
		return false;
	}
	if (Values.Num() >= 4 && Values[3].IsValid())
	{
		Values[3]->TryGetNumber(A);
	}

	OutValue = FLinearColor(R, G, B, A);
	return true;
}

bool ReadLinearColorFromObject(const TSharedRef<FJsonObject>& Object, FLinearColor& OutValue)
{
	double R = OutValue.R;
	double G = OutValue.G;
	double B = OutValue.B;
	double A = OutValue.A;
	bool bRead = false;
	bRead |= Object->TryGetNumberField(TEXT("r"), R) || Object->TryGetNumberField(TEXT("R"), R);
	bRead |= Object->TryGetNumberField(TEXT("g"), G) || Object->TryGetNumberField(TEXT("G"), G);
	bRead |= Object->TryGetNumberField(TEXT("b"), B) || Object->TryGetNumberField(TEXT("B"), B);
	bRead |= Object->TryGetNumberField(TEXT("a"), A) || Object->TryGetNumberField(TEXT("A"), A);
	if (bRead)
	{
		OutValue = FLinearColor(R, G, B, A);
	}
	return bRead;
}

bool ReadLinearColorField(
	const TSharedRef<FJsonObject>& Object,
	const TCHAR* SnakeKey,
	const TCHAR* PascalKey,
	FLinearColor& OutValue)
{
	const TSharedPtr<FJsonObject>* ColorObject = nullptr;
	if (Object->TryGetObjectField(SnakeKey, ColorObject) || Object->TryGetObjectField(PascalKey, ColorObject))
	{
		return ReadLinearColorFromObject(ColorObject->ToSharedRef(), OutValue);
	}

	const TArray<TSharedPtr<FJsonValue>>* ColorArray = nullptr;
	if (Object->TryGetArrayField(SnakeKey, ColorArray) || Object->TryGetArrayField(PascalKey, ColorArray))
	{
		return ReadLinearColorFromArray(*ColorArray, OutValue);
	}

	return false;
}

void ReadTransformSpecObject(
	const TSharedRef<FJsonObject>& Object,
	FHHControlRigTransformSpec& OutTransform)
{
	ReadVectorField(Object, TEXT("location"), TEXT("Location"), OutTransform.Location);
	ReadVectorField(Object, TEXT("translation"), TEXT("Translation"), OutTransform.Location);
	ReadRotatorField(Object, TEXT("rotation"), TEXT("Rotation"), OutTransform.Rotation);
	ReadVectorField(Object, TEXT("scale"), TEXT("Scale"), OutTransform.Scale);
	ReadBoolField(Object, TEXT("transform_in_global"), TEXT("bTransformInGlobal"), OutTransform.bTransformInGlobal);
}

bool ReadTransformSpecField(
	const TSharedRef<FJsonObject>& Object,
	const TCHAR* SnakeKey,
	const TCHAR* PascalKey,
	FHHControlRigTransformSpec& OutTransform)
{
	const TSharedPtr<FJsonObject>* TransformObject = nullptr;
	if (Object->TryGetObjectField(SnakeKey, TransformObject) || Object->TryGetObjectField(PascalKey, TransformObject))
	{
		ReadTransformSpecObject(TransformObject->ToSharedRef(), OutTransform);
		return true;
	}
	return false;
}

TSharedPtr<FJsonObject> ReadObjectField(
	const TSharedRef<FJsonObject>& Object,
	const TCHAR* SnakeKey,
	const TCHAR* PascalKey)
{
	const TSharedPtr<FJsonObject>* Child = nullptr;
	if (Object->TryGetObjectField(SnakeKey, Child) || Object->TryGetObjectField(PascalKey, Child))
	{
		return *Child;
	}
	return nullptr;
}

void ReadPositionField(const TSharedRef<FJsonObject>& Object, FVector2D& OutPosition)
{
	const TSharedPtr<FJsonObject>* PositionObject = nullptr;
	if (Object->TryGetObjectField(TEXT("position"), PositionObject) || Object->TryGetObjectField(TEXT("Position"), PositionObject))
	{
		double X = OutPosition.X;
		double Y = OutPosition.Y;
		(*PositionObject)->TryGetNumberField(TEXT("x"), X);
		(*PositionObject)->TryGetNumberField(TEXT("X"), X);
		(*PositionObject)->TryGetNumberField(TEXT("y"), Y);
		(*PositionObject)->TryGetNumberField(TEXT("Y"), Y);
		OutPosition = FVector2D(X, Y);
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* PositionArray = nullptr;
	if ((Object->TryGetArrayField(TEXT("position"), PositionArray) || Object->TryGetArrayField(TEXT("Position"), PositionArray)) &&
		PositionArray->Num() >= 2)
	{
		double X = OutPosition.X;
		double Y = OutPosition.Y;
		(*PositionArray)[0]->TryGetNumber(X);
		(*PositionArray)[1]->TryGetNumber(Y);
		OutPosition = FVector2D(X, Y);
	}
}

void ReadControlValueTypeField(
	const TSharedRef<FJsonObject>& Object,
	EHHControlRigControlValueType& OutValueType)
{
	FString ValueType;
	ReadStringField(Object, TEXT("value_type"), TEXT("ValueType"), ValueType);
	ValueType.TrimStartAndEndInline();
	ValueType = ValueType.ToLower();
	if (ValueType.IsEmpty())
	{
		return;
	}

	if (ValueType == TEXT("bool") || ValueType == TEXT("boolean"))
	{
		OutValueType = EHHControlRigControlValueType::Bool;
	}
	else if (ValueType == TEXT("transform") || ValueType == TEXT("euler_transform") || ValueType == TEXT("eulertransform"))
	{
		OutValueType = EHHControlRigControlValueType::EulerTransform;
	}
	else
	{
		OutValueType = EHHControlRigControlValueType::Float;
	}
}

void ReadCurveSpecObject(const TSharedRef<FJsonObject>& Object, FHHControlRigCurveSpec& OutSpec)
{
	ReadStringField(Object, TEXT("name"), TEXT("Name"), OutSpec.Name);
	ReadFloatField(Object, TEXT("value"), TEXT("Value"), OutSpec.Value);
}

void ReadNullSpecObject(const TSharedRef<FJsonObject>& Object, FHHControlRigNullSpec& OutSpec)
{
	ReadStringField(Object, TEXT("name"), TEXT("Name"), OutSpec.Name);
	ReadStringField(Object, TEXT("parent"), TEXT("Parent"), OutSpec.Parent);
	ReadTransformSpecObject(Object, OutSpec.Transform);
	ReadTransformSpecField(Object, TEXT("transform"), TEXT("Transform"), OutSpec.Transform);
}

void ReadControlSpecObject(const TSharedRef<FJsonObject>& Object, FHHControlRigControlSpec& OutSpec)
{
	ReadStringField(Object, TEXT("name"), TEXT("Name"), OutSpec.Name);
	ReadStringField(Object, TEXT("parent"), TEXT("Parent"), OutSpec.Parent);
	ReadControlValueTypeField(Object, OutSpec.ValueType);
	ReadFloatField(Object, TEXT("float_value"), TEXT("FloatValue"), OutSpec.FloatValue);
	ReadFloatField(Object, TEXT("value"), TEXT("Value"), OutSpec.FloatValue);
	ReadBoolField(Object, TEXT("bool_value"), TEXT("bBoolValue"), OutSpec.bBoolValue);
	ReadBoolField(Object, TEXT("value"), TEXT("Value"), OutSpec.bBoolValue);
	ReadTransformSpecObject(Object, OutSpec.EulerTransformValue);
	ReadTransformSpecField(Object, TEXT("euler_transform_value"), TEXT("EulerTransformValue"), OutSpec.EulerTransformValue);
	ReadTransformSpecField(Object, TEXT("transform_value"), TEXT("TransformValue"), OutSpec.EulerTransformValue);
	ReadTransformSpecField(Object, TEXT("value"), TEXT("Value"), OutSpec.EulerTransformValue);
	ReadTransformSpecField(Object, TEXT("offset_transform"), TEXT("OffsetTransform"), OutSpec.OffsetTransform);
	ReadTransformSpecField(Object, TEXT("shape_transform"), TEXT("ShapeTransform"), OutSpec.ShapeTransform);
	ReadStringField(Object, TEXT("shape_name"), TEXT("ShapeName"), OutSpec.ShapeName);
	ReadLinearColorField(Object, TEXT("shape_color"), TEXT("ShapeColor"), OutSpec.ShapeColor);
	ReadBoolField(Object, TEXT("shape_visible"), TEXT("bShapeVisible"), OutSpec.bShapeVisible);
}

void ReadCurveArrayField(
	const TSharedRef<FJsonObject>& Object,
	const TCHAR* SnakeKey,
	const TCHAR* PascalKey,
	TArray<FString>& OutNames,
	TArray<FHHControlRigCurveSpec>& OutSpecs,
	bool bReset)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object->TryGetArrayField(SnakeKey, Values) && !Object->TryGetArrayField(PascalKey, Values))
	{
		return;
	}

	if (bReset)
	{
		OutNames.Reset();
		OutSpecs.Reset();
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		FString StringValue;
		if (Value.IsValid() && Value->TryGetString(StringValue))
		{
			OutNames.Add(StringValue);
			continue;
		}

		const TSharedPtr<FJsonObject>* SpecObject = nullptr;
		if (Value.IsValid() && Value->TryGetObject(SpecObject))
		{
			FHHControlRigCurveSpec Spec;
			ReadCurveSpecObject(SpecObject->ToSharedRef(), Spec);
			OutSpecs.Add(Spec);
		}
	}
}

void ReadNullArrayField(
	const TSharedRef<FJsonObject>& Object,
	const TCHAR* SnakeKey,
	const TCHAR* PascalKey,
	TArray<FString>& OutNames,
	TArray<FHHControlRigNullSpec>& OutSpecs,
	bool bReset)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object->TryGetArrayField(SnakeKey, Values) && !Object->TryGetArrayField(PascalKey, Values))
	{
		return;
	}

	if (bReset)
	{
		OutNames.Reset();
		OutSpecs.Reset();
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		FString StringValue;
		if (Value.IsValid() && Value->TryGetString(StringValue))
		{
			OutNames.Add(StringValue);
			continue;
		}

		const TSharedPtr<FJsonObject>* SpecObject = nullptr;
		if (Value.IsValid() && Value->TryGetObject(SpecObject))
		{
			FHHControlRigNullSpec Spec;
			ReadNullSpecObject(SpecObject->ToSharedRef(), Spec);
			OutSpecs.Add(Spec);
		}
	}
}

void ReadControlArrayField(
	const TSharedRef<FJsonObject>& Object,
	const TCHAR* SnakeKey,
	const TCHAR* PascalKey,
	TArray<FString>& OutNames,
	TArray<FHHControlRigControlSpec>& OutSpecs,
	bool bReset)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object->TryGetArrayField(SnakeKey, Values) && !Object->TryGetArrayField(PascalKey, Values))
	{
		return;
	}

	if (bReset)
	{
		OutNames.Reset();
		OutSpecs.Reset();
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		FString StringValue;
		if (Value.IsValid() && Value->TryGetString(StringValue))
		{
			OutNames.Add(StringValue);
			continue;
		}

		const TSharedPtr<FJsonObject>* SpecObject = nullptr;
		if (Value.IsValid() && Value->TryGetObject(SpecObject))
		{
			FHHControlRigControlSpec Spec;
			ReadControlSpecObject(SpecObject->ToSharedRef(), Spec);
			OutSpecs.Add(Spec);
		}
	}
}

void ReadUnits(const TSharedRef<FJsonObject>& RigVMObject, FHHControlRigRigVMSpec& OutRigVM)
{
	const TArray<TSharedPtr<FJsonValue>>* Units = nullptr;
	if (!RigVMObject->TryGetArrayField(TEXT("units"), Units) &&
		!RigVMObject->TryGetArrayField(TEXT("Units"), Units))
	{
		return;
	}

	OutRigVM.Units.Reset();
	for (const TSharedPtr<FJsonValue>& Value : *Units)
	{
		const TSharedPtr<FJsonObject>* UnitObject = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(UnitObject))
		{
			continue;
		}

		FHHControlRigUnitSpec Unit;
		ReadStringField(UnitObject->ToSharedRef(), TEXT("struct_name"), TEXT("StructName"), Unit.StructName);
		ReadStringField(UnitObject->ToSharedRef(), TEXT("node_name"), TEXT("NodeName"), Unit.NodeName);
		ReadPositionField(UnitObject->ToSharedRef(), Unit.Position);
		OutRigVM.Units.Add(Unit);
	}
}

void ReadPinDefaults(const TSharedRef<FJsonObject>& RigVMObject, FHHControlRigRigVMSpec& OutRigVM)
{
	const TArray<TSharedPtr<FJsonValue>>* PinDefaults = nullptr;
	if (!RigVMObject->TryGetArrayField(TEXT("pin_defaults"), PinDefaults) &&
		!RigVMObject->TryGetArrayField(TEXT("PinDefaults"), PinDefaults))
	{
		return;
	}

	OutRigVM.PinDefaults.Reset();
	for (const TSharedPtr<FJsonValue>& Value : *PinDefaults)
	{
		const TSharedPtr<FJsonObject>* PinObject = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(PinObject))
		{
			continue;
		}

		FHHControlRigPinDefaultSpec PinDefault;
		ReadStringField(PinObject->ToSharedRef(), TEXT("node_name"), TEXT("NodeName"), PinDefault.NodeName);
		ReadStringField(PinObject->ToSharedRef(), TEXT("pin_path"), TEXT("PinPath"), PinDefault.PinPath);
		ReadStringField(PinObject->ToSharedRef(), TEXT("default_value"), TEXT("DefaultValue"), PinDefault.DefaultValue);
		ReadBoolField(PinObject->ToSharedRef(), TEXT("allow_array_resize"), TEXT("bAllowArrayResize"), PinDefault.bAllowArrayResize);
		ReadBoolField(PinObject->ToSharedRef(), TEXT("allow_linked_pin_propagation"), TEXT("bAllowLinkedPinPropagation"), PinDefault.bAllowLinkedPinPropagation);
		OutRigVM.PinDefaults.Add(PinDefault);
	}
}

void ReadLinks(const TSharedRef<FJsonObject>& RigVMObject, FHHControlRigRigVMSpec& OutRigVM)
{
	const TArray<TSharedPtr<FJsonValue>>* Links = nullptr;
	if (!RigVMObject->TryGetArrayField(TEXT("links"), Links) &&
		!RigVMObject->TryGetArrayField(TEXT("Links"), Links))
	{
		return;
	}

	OutRigVM.Links.Reset();
	for (const TSharedPtr<FJsonValue>& Value : *Links)
	{
		const TSharedPtr<FJsonObject>* LinkObject = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(LinkObject))
		{
			continue;
		}

		FHHControlRigLinkSpec Link;
		ReadStringField(LinkObject->ToSharedRef(), TEXT("source_pin_path"), TEXT("SourcePinPath"), Link.SourcePinPath);
		ReadStringField(LinkObject->ToSharedRef(), TEXT("target_pin_path"), TEXT("TargetPinPath"), Link.TargetPinPath);
		ReadBoolField(LinkObject->ToSharedRef(), TEXT("allow_cast_node_creation"), TEXT("bAllowCastNodeCreation"), Link.bAllowCastNodeCreation);
		OutRigVM.Links.Add(Link);
	}
}

void ReadAnimBPInputDefaults(
	const TSharedRef<FJsonObject>& AnimBPObject,
	FHHControlRigAnimBPBindingSpec& OutAnimBPBinding)
{
	TSharedPtr<FJsonValue> InputDefaultsValue = AnimBPObject->TryGetField(TEXT("input_defaults"));
	if (!InputDefaultsValue.IsValid())
	{
		InputDefaultsValue = AnimBPObject->TryGetField(TEXT("InputDefaults"));
	}
	if (!InputDefaultsValue.IsValid())
	{
		return;
	}

	OutAnimBPBinding.InputDefaults.Reset();

	const TSharedPtr<FJsonObject>* DefaultsObject = nullptr;
	if (InputDefaultsValue->TryGetObject(DefaultsObject))
	{
		for (const auto& Pair : DefaultsObject->ToSharedRef()->Values)
		{
			FHHControlRigAnimBPInputDefaultSpec Spec;
			Spec.Name = FString(Pair.Key);
			Spec.ValueJson = JsonValueToSimpleString(Pair.Value);
			OutAnimBPBinding.InputDefaults.Add(Spec);
		}
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* DefaultsArray = nullptr;
	if (InputDefaultsValue->TryGetArray(DefaultsArray))
	{
		for (const TSharedPtr<FJsonValue>& Value : *DefaultsArray)
		{
			const TSharedPtr<FJsonObject>* DefaultObject = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(DefaultObject))
			{
				continue;
			}

			FHHControlRigAnimBPInputDefaultSpec Spec;
			ReadStringField(DefaultObject->ToSharedRef(), TEXT("name"), TEXT("Name"), Spec.Name);
			ReadStringField(DefaultObject->ToSharedRef(), TEXT("input_name"), TEXT("InputName"), Spec.Name);
			ReadStringField(DefaultObject->ToSharedRef(), TEXT("control"), TEXT("Control"), Spec.Name);

			TSharedPtr<FJsonValue> ValueField = DefaultObject->ToSharedRef()->TryGetField(TEXT("value"));
			if (!ValueField.IsValid())
			{
				ValueField = DefaultObject->ToSharedRef()->TryGetField(TEXT("Value"));
			}
			if (ValueField.IsValid())
			{
				Spec.ValueJson = JsonValueToSimpleString(ValueField);
			}

			OutAnimBPBinding.InputDefaults.Add(Spec);
		}
	}
}

bool ParseRecipeJson(
	const FString& RecipeJson,
	FHHControlRigRecipe& OutRecipe,
	FString& OutError)
{
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RecipeJson);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Recipe JSON could not be parsed as an object.");
		return false;
	}

	const TSharedRef<FJsonObject> Root = RootObject.ToSharedRef();
	ReadIntField(Root, TEXT("schema_version"), TEXT("SchemaVersion"), OutRecipe.SchemaVersion);
	ReadStringField(Root, TEXT("target_control_rig_path"), TEXT("TargetControlRigPath"), OutRecipe.TargetControlRigPath);
	ReadStringField(Root, TEXT("source_skeletal_mesh"), TEXT("SourceSkeletalMesh"), OutRecipe.SourceSkeletalMesh);
	ReadStringField(Root, TEXT("preview_skeletal_mesh"), TEXT("PreviewSkeletalMesh"), OutRecipe.PreviewSkeletalMesh);

	if (const TSharedPtr<FJsonObject> Hierarchy = ReadObjectField(Root, TEXT("hierarchy"), TEXT("Hierarchy")))
	{
		ReadBoolField(Hierarchy.ToSharedRef(), TEXT("import_preview_bones"), TEXT("bImportPreviewBones"), OutRecipe.Hierarchy.bImportPreviewBones);
		ReadIntField(Hierarchy.ToSharedRef(), TEXT("max_import_bones"), TEXT("MaxImportBones"), OutRecipe.Hierarchy.MaxImportBones);
		ReadNullArrayField(Hierarchy.ToSharedRef(), TEXT("nulls"), TEXT("Nulls"), OutRecipe.Hierarchy.Nulls, OutRecipe.Hierarchy.NullSpecs, true);
		ReadControlArrayField(Hierarchy.ToSharedRef(), TEXT("controls"), TEXT("Controls"), OutRecipe.Hierarchy.Controls, OutRecipe.Hierarchy.ControlSpecs, true);
		ReadCurveArrayField(Hierarchy.ToSharedRef(), TEXT("curves"), TEXT("Curves"), OutRecipe.Hierarchy.Curves, OutRecipe.Hierarchy.CurveSpecs, true);
		ReadNullArrayField(Hierarchy.ToSharedRef(), TEXT("null_specs"), TEXT("NullSpecs"), OutRecipe.Hierarchy.Nulls, OutRecipe.Hierarchy.NullSpecs, false);
		ReadControlArrayField(Hierarchy.ToSharedRef(), TEXT("control_specs"), TEXT("ControlSpecs"), OutRecipe.Hierarchy.Controls, OutRecipe.Hierarchy.ControlSpecs, false);
		ReadCurveArrayField(Hierarchy.ToSharedRef(), TEXT("curve_specs"), TEXT("CurveSpecs"), OutRecipe.Hierarchy.Curves, OutRecipe.Hierarchy.CurveSpecs, false);
	}

	if (const TSharedPtr<FJsonObject> RigVM = ReadObjectField(Root, TEXT("rigvm"), TEXT("RigVM")))
	{
		ReadUnits(RigVM.ToSharedRef(), OutRecipe.RigVM);
		ReadStringArrayField(RigVM.ToSharedRef(), TEXT("variables"), TEXT("Variables"), OutRecipe.RigVM.Variables);
		ReadStringArrayField(RigVM.ToSharedRef(), TEXT("template_nodes"), TEXT("TemplateNodes"), OutRecipe.RigVM.TemplateNodes);
		ReadStringArrayField(RigVM.ToSharedRef(), TEXT("function_references"), TEXT("FunctionReferences"), OutRecipe.RigVM.FunctionReferences);
		ReadPinDefaults(RigVM.ToSharedRef(), OutRecipe.RigVM);
		ReadLinks(RigVM.ToSharedRef(), OutRecipe.RigVM);
		ReadStringArrayField(RigVM.ToSharedRef(), TEXT("exposed_pins"), TEXT("ExposedPins"), OutRecipe.RigVM.ExposedPins);
	}

	if (const TSharedPtr<FJsonObject> AnimBP = ReadObjectField(Root, TEXT("animbp_binding"), TEXT("AnimBPBinding")))
	{
		ReadBoolField(AnimBP.ToSharedRef(), TEXT("enabled"), TEXT("bEnabled"), OutRecipe.AnimBPBinding.bEnabled);
		ReadStringField(AnimBP.ToSharedRef(), TEXT("target_anim_bp_path"), TEXT("TargetAnimBPPath"), OutRecipe.AnimBPBinding.TargetAnimBPPath);
		ReadStringField(AnimBP.ToSharedRef(), TEXT("source_anim_bp_path"), TEXT("SourceAnimBPPath"), OutRecipe.AnimBPBinding.SourceAnimBPPath);
		ReadStringField(AnimBP.ToSharedRef(), TEXT("insert_mode"), TEXT("InsertMode"), OutRecipe.AnimBPBinding.InsertMode);
		ReadStringArrayField(AnimBP.ToSharedRef(), TEXT("expose_input_pins"), TEXT("ExposeInputPins"), OutRecipe.AnimBPBinding.ExposeInputPins);
		ReadBoolField(AnimBP.ToSharedRef(), TEXT("disconnect_existing_input_links"), TEXT("bDisconnectExistingInputLinks"), OutRecipe.AnimBPBinding.bDisconnectExistingInputLinks);
		ReadBoolField(AnimBP.ToSharedRef(), TEXT("allow_existing_target_reuse"), TEXT("bAllowExistingTargetReuse"), OutRecipe.AnimBPBinding.bAllowExistingTargetReuse);
		ReadBoolField(AnimBP.ToSharedRef(), TEXT("overwrite_existing"), TEXT("bOverwriteExisting"), OutRecipe.AnimBPBinding.bOverwriteExisting);
		ReadBoolField(AnimBP.ToSharedRef(), TEXT("replace_existing_links"), TEXT("bReplaceExistingLinks"), OutRecipe.AnimBPBinding.bReplaceExistingLinks);
		ReadAnimBPInputDefaults(AnimBP.ToSharedRef(), OutRecipe.AnimBPBinding);
	}

	if (const TSharedPtr<FJsonObject> Verification = ReadObjectField(Root, TEXT("verification"), TEXT("Verification")))
	{
		ReadBoolField(Verification.ToSharedRef(), TEXT("compile"), TEXT("bCompile"), OutRecipe.Verification.bCompile);
		ReadBoolField(Verification.ToSharedRef(), TEXT("save"), TEXT("bSave"), OutRecipe.Verification.bSave);
		ReadBoolField(Verification.ToSharedRef(), TEXT("inspect_topology"), TEXT("bInspectTopology"), OutRecipe.Verification.bInspectTopology);
		ReadBoolField(Verification.ToSharedRef(), TEXT("direct_solve"), TEXT("bDirectSolve"), OutRecipe.Verification.bDirectSolve);
		ReadBoolField(Verification.ToSharedRef(), TEXT("dependency_audit"), TEXT("bDependencyAudit"), OutRecipe.Verification.bDependencyAudit);
	}

	return true;
}

bool ParseBuildOptionsJson(
	const FString& OptionsJson,
	FHHControlRigBuildOptions& OutOptions,
	FString& OutError)
{
	if (OptionsJson.TrimStartAndEnd().IsEmpty())
	{
		return true;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(OptionsJson);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Options JSON could not be parsed as an object.");
		return false;
	}

	const TSharedRef<FJsonObject> Root = RootObject.ToSharedRef();
	ReadBoolField(Root, TEXT("dry_run"), TEXT("bDryRun"), OutOptions.bDryRun);
	ReadBoolField(Root, TEXT("allow_temp_path"), TEXT("bAllowTempPath"), OutOptions.bAllowTempPath);
	ReadBoolField(Root, TEXT("allow_sample_path"), TEXT("bAllowSamplePath"), OutOptions.bAllowSamplePath);
	ReadBoolField(Root, TEXT("allow_save"), TEXT("bAllowSave"), OutOptions.bAllowSave);
	ReadBoolField(Root, TEXT("allow_array_resize"), TEXT("bAllowArrayResize"), OutOptions.bAllowArrayResize);
	ReadBoolField(Root, TEXT("allow_linked_pin_propagation"), TEXT("bAllowLinkedPinPropagation"), OutOptions.bAllowLinkedPinPropagation);
	ReadBoolField(Root, TEXT("allow_cast_node_creation"), TEXT("bAllowCastNodeCreation"), OutOptions.bAllowCastNodeCreation);
	ReadIntField(Root, TEXT("max_import_bones"), TEXT("MaxImportBones"), OutOptions.MaxImportBones);
	return true;
}

FTransform ToTransform(const FHHControlRigTransformSpec& TransformSpec)
{
	return FTransform(TransformSpec.Rotation, TransformSpec.Location, TransformSpec.Scale);
}

FEulerTransform ToEulerTransform(const FHHControlRigTransformSpec& TransformSpec)
{
	return FEulerTransform(TransformSpec.Location, TransformSpec.Rotation, TransformSpec.Scale);
}

bool ResolveAuthoredParentKey(
	const FString& ParentName,
	const TMap<FString, FRigElementKey>& AuthoredParentKeys,
	FRigElementKey& OutParentKey,
	FHHControlRigBuildReport& Report,
	const FString& FieldPath)
{
	const FString TrimmedParentName = ParentName.TrimStartAndEnd();
	if (TrimmedParentName.IsEmpty())
	{
		OutParentKey = FRigElementKey();
		return true;
	}

	const FRigElementKey* FoundParentKey = AuthoredParentKeys.Find(TrimmedParentName);
	if (!FoundParentKey || !FoundParentKey->IsValid())
	{
		AddIssue(
			Report,
			EHHControlRigIssueSeverity::Error,
			TEXT("parent_not_found"),
			FString::Printf(TEXT("Parent must be an imported bone or earlier authored null/control in V1: %s."), *TrimmedParentName),
			FieldPath);
		return false;
	}

	OutParentKey = *FoundParentKey;
	return true;
}

FRigControlSettings MakeControlSettings(const FString& ControlName, const FHHControlRigControlSpec& Spec)
{
	FRigControlSettings Settings;
	Settings.AnimationType = ERigControlAnimationType::AnimationControl;
	Settings.DisplayName = FName(*ControlName);
	Settings.bIsCurve = false;
	Settings.bIsTransientControl = false;
	Settings.bShapeVisible = Spec.bShapeVisible;
	Settings.ShapeColor = Spec.ShapeColor;
	if (!Spec.ShapeName.TrimStartAndEnd().IsEmpty())
	{
		Settings.ShapeName = FName(*Spec.ShapeName.TrimStartAndEnd());
	}

	switch (Spec.ValueType)
	{
	case EHHControlRigControlValueType::Bool:
		Settings.ControlType = ERigControlType::Bool;
		break;
	case EHHControlRigControlValueType::EulerTransform:
		Settings.ControlType = ERigControlType::EulerTransform;
		break;
	case EHHControlRigControlValueType::Float:
	default:
		Settings.ControlType = ERigControlType::Float;
		break;
	}

	Settings.SetupLimitArrayForType();
	return Settings;
}

FRigControlValue MakeControlValue(const FHHControlRigControlSpec& Spec)
{
	switch (Spec.ValueType)
	{
	case EHHControlRigControlValueType::Bool:
		return FRigControlValue::Make<bool>(Spec.bBoolValue);
	case EHHControlRigControlValueType::EulerTransform:
		return FRigControlValue::Make<FEulerTransform>(ToEulerTransform(Spec.EulerTransformValue));
	case EHHControlRigControlValueType::Float:
	default:
		return FRigControlValue::Make<float>(Spec.FloatValue);
	}
}

FString SeverityToString(EHHControlRigIssueSeverity Severity)
{
	switch (Severity)
	{
	case EHHControlRigIssueSeverity::Info:
		return TEXT("info");
	case EHHControlRigIssueSeverity::Warning:
		return TEXT("warning");
	case EHHControlRigIssueSeverity::Error:
	default:
		return TEXT("error");
	}
}

TSharedRef<FJsonObject> IssueToJson(const FHHControlRigValidationIssue& Issue)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("severity"), SeverityToString(Issue.Severity));
	Object->SetStringField(TEXT("code"), Issue.Code);
	Object->SetStringField(TEXT("message"), Issue.Message);
	Object->SetStringField(TEXT("field_path"), Issue.FieldPath);
	return Object;
}

FString JsonObjectToString(const TSharedRef<FJsonObject>& Object)
{
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Object, Writer);
	return Json;
}

void AddStringArrayJsonField(
	const TSharedRef<FJsonObject>& Object,
	const TCHAR* FieldName,
	const TArray<FString>& Values)
{
	TArray<TSharedPtr<FJsonValue>> JsonValues;
	for (const FString& Value : Values)
	{
		JsonValues.Add(MakeShared<FJsonValueString>(Value));
	}
	Object->SetArrayField(FieldName, JsonValues);
}

TSharedRef<FJsonObject> ReportToObject(const FHHControlRigBuildReport& Report)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetBoolField(TEXT("success"), Report.bSuccess);
	Object->SetBoolField(TEXT("plugin_unavailable"), Report.bPluginUnavailable);
	Object->SetBoolField(TEXT("dry_run"), Report.bDryRun);
	Object->SetBoolField(TEXT("sample_only"), Report.bSampleOnly);
	Object->SetStringField(TEXT("target_control_rig_path"), Report.TargetControlRigPath);
	Object->SetBoolField(TEXT("created"), Report.bCreated);
	Object->SetBoolField(TEXT("reused"), Report.bReused);
	Object->SetBoolField(TEXT("target_modified"), Report.bTargetModified);
	Object->SetBoolField(TEXT("partial_target_modified"), Report.bPartialTargetModified);
	Object->SetBoolField(TEXT("compile_requested"), Report.bCompileRequested);
	Object->SetBoolField(TEXT("compile_succeeded"), Report.bCompileSucceeded);
	Object->SetBoolField(TEXT("save_requested"), Report.bSaveRequested);
	Object->SetBoolField(TEXT("save_succeeded"), Report.bSaveSucceeded);
	Object->SetBoolField(TEXT("dependency_audit_passed"), Report.bDependencyAuditPassed);
	Object->SetNumberField(TEXT("created_hierarchy_elements"), Report.CreatedHierarchyElements);
	Object->SetNumberField(TEXT("created_rigvm_nodes"), Report.CreatedRigVMNodes);
	Object->SetNumberField(TEXT("applied_pin_defaults"), Report.AppliedPinDefaults);
	Object->SetNumberField(TEXT("applied_links"), Report.AppliedLinks);
	Object->SetStringField(TEXT("readback_summary"), Report.ReadbackSummary);
	Object->SetStringField(TEXT("next_action"), Report.NextAction);

	TArray<TSharedPtr<FJsonValue>> Issues;
	for (const FHHControlRigValidationIssue& Issue : Report.Issues)
	{
		Issues.Add(MakeShared<FJsonValueObject>(IssueToJson(Issue)));
	}
	Object->SetArrayField(TEXT("issues"), Issues);

	TArray<TSharedPtr<FJsonValue>> Warnings;
	for (const FString& Warning : Report.Warnings)
	{
		Warnings.Add(MakeShared<FJsonValueString>(Warning));
	}
	Object->SetArrayField(TEXT("warnings"), Warnings);
	AddStringArrayJsonField(Object, TEXT("planned_writes"), Report.PlannedWrites);
	AddStringArrayJsonField(Object, TEXT("performed_writes"), Report.PerformedWrites);
	AddStringArrayJsonField(Object, TEXT("skipped_writes"), Report.SkippedWrites);
	AddStringArrayJsonField(Object, TEXT("reused_items"), Report.ReusedItems);
	AddStringArrayJsonField(Object, TEXT("cleanup_suggestions"), Report.CleanupSuggestions);

	return Object;
}

FHHControlRigBuildReport ValidateRecipe(
	const FHHControlRigRecipe& Recipe,
	const FHHControlRigBuildOptions& Options)
{

	FHHControlRigBuildReport Report = MakeBaseReport(Recipe, Options);

	if (Recipe.SchemaVersion != SupportedSchemaVersion)
	{
		AddIssue(
			Report,
			EHHControlRigIssueSeverity::Error,
			TEXT("unsupported_schema_version"),
			FString::Printf(TEXT("schema_version %d is not supported."), Recipe.SchemaVersion),
			TEXT("schema_version"));
	}

	if (Recipe.TargetControlRigPath.IsEmpty())
	{
		AddIssue(
			Report,
			EHHControlRigIssueSeverity::Error,
			TEXT("missing_target_control_rig_path"),
			TEXT("target_control_rig_path is required."),
			TEXT("target_control_rig_path"));
	}
	else
	{
		FText PathFailure;
		if (!FPackageName::IsValidLongPackageName(Recipe.TargetControlRigPath, false, &PathFailure))
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("invalid_target_control_rig_path"),
				FString::Printf(TEXT("%s must be a valid long package path like /Game/_MCP_Temp/Folder/CR_Name."), *Recipe.TargetControlRigPath),
				TEXT("target_control_rig_path"));
		}

		if (!IsAllowedTargetPath(Recipe.TargetControlRigPath, Options))
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("target_outside_sample_temp_scope"),
				TEXT("target_control_rig_path must be under /Game/_MCP_Temp/ or /Game/_MCP_Sample/ in V1."),
				TEXT("target_control_rig_path"));
		}

		if (ContainsForbiddenMcpReference(Recipe.TargetControlRigPath))
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("forbidden_mcp_reference"),
				TEXT("target_control_rig_path contains an UnrealMCP-only reference."),
				TEXT("target_control_rig_path"));
		}
	}

	ValidateAssetPath(Report, Recipe.SourceSkeletalMesh, TEXT("source_skeletal_mesh"), false);
	ValidateAssetPath(Report, Recipe.PreviewSkeletalMesh, TEXT("preview_skeletal_mesh"), Recipe.Hierarchy.bImportPreviewBones);

	if (Recipe.Hierarchy.MaxImportBones <= 0)
	{
		AddIssue(
			Report,
			EHHControlRigIssueSeverity::Error,
			TEXT("invalid_max_import_bones"),
			TEXT("hierarchy.max_import_bones must be greater than zero."),
			TEXT("hierarchy.max_import_bones"));
	}
	else if (Recipe.Hierarchy.MaxImportBones > Options.MaxImportBones)
	{
		AddIssue(
			Report,
			EHHControlRigIssueSeverity::Error,
			TEXT("max_import_bones_exceeds_limit"),
			FString::Printf(TEXT("hierarchy.max_import_bones exceeds option limit %d."), Options.MaxImportBones),
			TEXT("hierarchy.max_import_bones"));
	}

	ValidateNameArray(Report, Recipe.Hierarchy.Nulls, TEXT("hierarchy.nulls"));
	ValidateNameArray(Report, Recipe.Hierarchy.Controls, TEXT("hierarchy.controls"));
	ValidateNameArray(Report, Recipe.Hierarchy.Curves, TEXT("hierarchy.curves"));
	ValidateNullSpecs(Report, Recipe.Hierarchy.Nulls, Recipe.Hierarchy.NullSpecs);
	ValidateControlSpecs(Report, Recipe.Hierarchy.Controls, Recipe.Hierarchy.ControlSpecs);
	ValidateCurveSpecs(Report, Recipe.Hierarchy.Curves, Recipe.Hierarchy.CurveSpecs);

	TSet<FString> UnitNodeNames;
	for (int32 Index = 0; Index < Recipe.RigVM.Units.Num(); ++Index)
	{
		const FHHControlRigUnitSpec& Unit = Recipe.RigVM.Units[Index];
		const FString UnitPath = FString::Printf(TEXT("rigvm.units[%d]"), Index);
		const FString NormalizedStructName = NormalizeUnitStructName(Unit.StructName);

		if (NormalizedStructName.IsEmpty())
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("missing_unit_struct"),
				TEXT("RigVM unit struct_name is required."),
				UnitPath + TEXT(".struct_name"));
		}
		else if (!AllowedUnitStructs().Contains(NormalizedStructName))
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("unit_struct_not_allowlisted"),
				FString::Printf(TEXT("%s is not in the V1 ControlRig unit allowlist."), *Unit.StructName),
				UnitPath + TEXT(".struct_name"));
		}

		if (ContainsForbiddenMcpReference(Unit.StructName) || ContainsForbiddenMcpReference(Unit.NodeName))
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("forbidden_mcp_reference"),
				TEXT("RigVM unit contains an UnrealMCP-only reference."),
				UnitPath);
		}

		const FString NodeName = Unit.NodeName.TrimStartAndEnd();
		if (!NodeName.IsEmpty())
		{
			if (UnitNodeNames.Contains(NodeName))
			{
				AddIssue(
					Report,
					EHHControlRigIssueSeverity::Error,
					TEXT("duplicate_unit_node_name"),
					FString::Printf(TEXT("%s is duplicated."), *NodeName),
					UnitPath + TEXT(".node_name"));
			}
			UnitNodeNames.Add(NodeName);
		}
	}

	if (!Recipe.RigVM.Variables.IsEmpty())
	{
		AddIssue(Report, EHHControlRigIssueSeverity::Error, TEXT("variables_deferred"), TEXT("RigVM variables are deferred beyond V1."), TEXT("rigvm.variables"));
	}
	if (!Recipe.RigVM.TemplateNodes.IsEmpty())
	{
		AddIssue(Report, EHHControlRigIssueSeverity::Error, TEXT("template_nodes_deferred"), TEXT("RigVM template nodes are deferred beyond V1."), TEXT("rigvm.template_nodes"));
	}
	if (!Recipe.RigVM.FunctionReferences.IsEmpty())
	{
		AddIssue(Report, EHHControlRigIssueSeverity::Error, TEXT("function_references_deferred"), TEXT("RigVM function references are deferred beyond V1."), TEXT("rigvm.function_references"));
	}
	if (!Recipe.RigVM.ExposedPins.IsEmpty())
	{
		AddIssue(Report, EHHControlRigIssueSeverity::Error, TEXT("exposed_pins_deferred"), TEXT("RigVM exposed pins are deferred beyond V1."), TEXT("rigvm.exposed_pins"));
	}

	for (int32 Index = 0; Index < Recipe.RigVM.PinDefaults.Num(); ++Index)
	{
		const FHHControlRigPinDefaultSpec& PinDefault = Recipe.RigVM.PinDefaults[Index];
		const FString PinPath = FString::Printf(TEXT("rigvm.pin_defaults[%d]"), Index);
		if (PinDefault.PinPath.TrimStartAndEnd().IsEmpty())
		{
			AddIssue(Report, EHHControlRigIssueSeverity::Error, TEXT("missing_pin_path"), TEXT("pin_path is required."), PinPath + TEXT(".pin_path"));
		}
		if (PinDefault.bAllowArrayResize && !Options.bAllowArrayResize)
		{
			AddIssue(Report, EHHControlRigIssueSeverity::Error, TEXT("array_resize_not_allowed"), TEXT("Array resizing is disabled by default."), PinPath + TEXT(".allow_array_resize"));
		}
		if (PinDefault.bAllowLinkedPinPropagation && !Options.bAllowLinkedPinPropagation)
		{
			AddIssue(Report, EHHControlRigIssueSeverity::Error, TEXT("linked_pin_propagation_not_allowed"), TEXT("Linked-pin propagation is disabled by default."), PinPath + TEXT(".allow_linked_pin_propagation"));
		}
		if (ContainsForbiddenMcpReference(PinDefault.NodeName) || ContainsForbiddenMcpReference(PinDefault.PinPath) || ContainsForbiddenMcpReference(PinDefault.DefaultValue))
		{
			AddIssue(Report, EHHControlRigIssueSeverity::Error, TEXT("forbidden_mcp_reference"), TEXT("Pin default contains an UnrealMCP-only reference."), PinPath);
		}
	}

	for (int32 Index = 0; Index < Recipe.RigVM.Links.Num(); ++Index)
	{
		const FHHControlRigLinkSpec& Link = Recipe.RigVM.Links[Index];
		const FString LinkPath = FString::Printf(TEXT("rigvm.links[%d]"), Index);
		if (Link.SourcePinPath.TrimStartAndEnd().IsEmpty() || Link.TargetPinPath.TrimStartAndEnd().IsEmpty())
		{
			AddIssue(Report, EHHControlRigIssueSeverity::Error, TEXT("missing_link_pin"), TEXT("Both source_pin_path and target_pin_path are required."), LinkPath);
		}
		if (Link.bAllowCastNodeCreation && !Options.bAllowCastNodeCreation)
		{
			AddIssue(Report, EHHControlRigIssueSeverity::Error, TEXT("cast_node_creation_not_allowed"), TEXT("Cast-node creation is disabled by default."), LinkPath + TEXT(".allow_cast_node_creation"));
		}
		if (ContainsForbiddenMcpReference(Link.SourcePinPath) || ContainsForbiddenMcpReference(Link.TargetPinPath))
		{
			AddIssue(Report, EHHControlRigIssueSeverity::Error, TEXT("forbidden_mcp_reference"), TEXT("RigVM link contains an UnrealMCP-only reference."), LinkPath);
		}
	}

	ValidateAnimBPBinding(Report, Recipe, Options);

	if (Recipe.Verification.bSave && !Options.bAllowSave)
	{
		AddIssue(
			Report,
			EHHControlRigIssueSeverity::Error,
			TEXT("save_not_allowed"),
			TEXT("Saving is disabled in Phase 1 dry-run validation."),
			TEXT("verification.save"));
	}

	if (!Recipe.Verification.bDependencyAudit)
	{
		AddIssue(
			Report,
			EHHControlRigIssueSeverity::Error,
			TEXT("dependency_audit_required"),
			TEXT("dependency_audit must stay enabled before any authored ControlRig can be promoted."),
			TEXT("verification.dependency_audit"));
	}

	if (Recipe.Verification.bCompile && Options.bDryRun)
	{
		AddIssue(
			Report,
			EHHControlRigIssueSeverity::Info,
			TEXT("compile_not_run_in_phase1"),
			TEXT("Compile verification is requested but not executed during dry-run validation."),
			TEXT("verification.compile"));
	}

	Report.bDependencyAuditPassed = Report.bDependencyAuditPassed && !HasError(Report);
	Report.bSuccess = !HasError(Report);
	if (Options.bDryRun)
	{
		AddReportEntry(Report.SkippedWrites, TEXT("dry_run_no_asset_mutation"));
	}
	Report.NextAction = Report.bSuccess
		? (Recipe.AnimBPBinding.bEnabled
			? TEXT("run_build_controlrig_recipe_with_animbp_binding_for_sample_temp_target")
			: TEXT("run_build_controlrig_recipe_for_sample_temp_target"))
		: TEXT("fix_recipe_and_rerun_validate_controlrig_recipe");
	Report.ReadbackSummary = Report.bSuccess
		? TEXT("Recipe validation passed. No assets were created, modified, compiled, or saved.")
		: TEXT("Recipe validation failed. No assets were created, modified, compiled, or saved.");
	return Report;
}

FHHControlRigBuildReport DryRunRecipe(
	const FHHControlRigRecipe& Recipe,
	const FHHControlRigBuildOptions& Options)
{
	FHHControlRigBuildOptions DryRunOptions = Options;
	DryRunOptions.bDryRun = true;
	return ValidateRecipe(Recipe, DryRunOptions);
}
}
