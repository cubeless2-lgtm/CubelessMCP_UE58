#include "Operations/CubelessControlRigPresetRecipes.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace Cubeless::Operations::ControlRigPresetRecipes
{
namespace
{
constexpr int32 SupportedSchemaVersion = 1;

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
		TEXT("CubelessControlRigToolsEditor")
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

FString JsonObjectToString(const TSharedRef<FJsonObject>& Object)
{
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Object, Writer);
	return Json;
}

TSharedRef<FJsonObject> MakeVectorJson(const FVector& Value)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("x"), Value.X);
	Object->SetNumberField(TEXT("y"), Value.Y);
	Object->SetNumberField(TEXT("z"), Value.Z);
	return Object;
}

TSharedRef<FJsonObject> MakeRotatorJson(const FRotator& Value)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("pitch"), Value.Pitch);
	Object->SetNumberField(TEXT("yaw"), Value.Yaw);
	Object->SetNumberField(TEXT("roll"), Value.Roll);
	return Object;
}

TSharedRef<FJsonObject> MakeTransformJson(const FVector& Location, const FRotator& Rotation, const FVector& Scale)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetObjectField(TEXT("location"), MakeVectorJson(Location));
	Object->SetObjectField(TEXT("rotation"), MakeRotatorJson(Rotation));
	Object->SetObjectField(TEXT("scale"), MakeVectorJson(Scale));
	return Object;
}

TArray<TSharedPtr<FJsonValue>> MakeColorArray(const FLinearColor& Color)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	Values.Add(MakeShared<FJsonValueNumber>(Color.R));
	Values.Add(MakeShared<FJsonValueNumber>(Color.G));
	Values.Add(MakeShared<FJsonValueNumber>(Color.B));
	Values.Add(MakeShared<FJsonValueNumber>(Color.A));
	return Values;
}

TSharedRef<FJsonObject> MakeControlSpecJson(
	const FString& Name,
	const FString& Parent,
	const FString& ValueType,
	double FloatValue,
	const FVector& Location,
	const FRotator& Rotation,
	const FLinearColor& ShapeColor)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("name"), Name);
	if (!Parent.TrimStartAndEnd().IsEmpty())
	{
		Object->SetStringField(TEXT("parent"), Parent);
	}
	Object->SetStringField(TEXT("value_type"), ValueType);
	if (ValueType == TEXT("euler_transform"))
	{
		Object->SetObjectField(TEXT("value"), MakeTransformJson(Location, Rotation, FVector::OneVector));
		Object->SetObjectField(TEXT("shape_transform"), MakeTransformJson(FVector::ZeroVector, FRotator::ZeroRotator, FVector(0.6, 0.6, 0.6)));
	}
	else
	{
		Object->SetNumberField(TEXT("value"), FloatValue);
	}
	Object->SetArrayField(TEXT("shape_color"), MakeColorArray(ShapeColor));
	return Object;
}

TSharedRef<FJsonObject> MakeCurveSpecJson(const FString& Name, double Value)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("name"), Name);
	Object->SetNumberField(TEXT("value"), Value);
	return Object;
}

TSharedRef<FJsonObject> MakeNullSpecJson(
	const FString& Name,
	const FString& Parent,
	const FVector& Location,
	const FRotator& Rotation)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("name"), Name);
	if (!Parent.TrimStartAndEnd().IsEmpty())
	{
		Object->SetStringField(TEXT("parent"), Parent);
	}
	Object->SetObjectField(TEXT("transform"), MakeTransformJson(Location, Rotation, FVector::OneVector));
	return Object;
}

TSharedRef<FJsonObject> MakeUnitJson(
	const FString& StructName,
	const FString& NodeName,
	double X,
	double Y)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("struct_name"), StructName);
	Object->SetStringField(TEXT("node_name"), NodeName);

	TSharedRef<FJsonObject> Position = MakeShared<FJsonObject>();
	Position->SetNumberField(TEXT("x"), X);
	Position->SetNumberField(TEXT("y"), Y);
	Object->SetObjectField(TEXT("position"), Position);
	return Object;
}

FString SanitizePresetNamePart(const FString& RawName, const FString& Fallback)
{
	FString Result = RawName.TrimStartAndEnd();
	if (Result.IsEmpty())
	{
		Result = Fallback;
	}

	for (int32 Index = 0; Index < Result.Len(); ++Index)
	{
		const TCHAR Character = Result[Index];
		if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
		{
			Result[Index] = TEXT('_');
		}
	}
	return Result;
}

void AddPresetIssue(
	TArray<TSharedPtr<FJsonValue>>& Issues,
	const FString& Code,
	const FString& Message,
	const FString& FieldPath)
{
	TSharedRef<FJsonObject> Issue = MakeShared<FJsonObject>();
	Issue->SetStringField(TEXT("severity"), TEXT("error"));
	Issue->SetStringField(TEXT("code"), Code);
	Issue->SetStringField(TEXT("message"), Message);
	Issue->SetStringField(TEXT("field_path"), FieldPath);
	Issues.Add(MakeShared<FJsonValueObject>(Issue));
}

FString MakePresetResponseJson(
	bool bSuccess,
	const FString& PresetName,
	const FString& TargetControlRigPath,
	const TSharedPtr<FJsonObject>& Recipe,
	const TArray<TSharedPtr<FJsonValue>>& Issues,
	const FString& Summary)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetBoolField(TEXT("success"), bSuccess);
	Object->SetBoolField(TEXT("plugin_unavailable"), false);
	Object->SetBoolField(TEXT("dry_run"), true);
	Object->SetBoolField(TEXT("sample_only"), true);
	Object->SetStringField(TEXT("preset_name"), PresetName);
	Object->SetStringField(TEXT("target_control_rig_path"), TargetControlRigPath);
	Object->SetArrayField(TEXT("issues"), Issues);
	Object->SetArrayField(TEXT("warnings"), TArray<TSharedPtr<FJsonValue>>());
	Object->SetStringField(TEXT("readback_summary"), Summary);

	if (Recipe.IsValid())
	{
		Object->SetObjectField(TEXT("recipe"), Recipe);
		Object->SetStringField(TEXT("recipe_json"), JsonObjectToString(Recipe.ToSharedRef()));
	}
	else
	{
		Object->SetStringField(TEXT("recipe_json"), TEXT(""));
	}

	return JsonObjectToString(Object);
}

TSharedRef<FJsonObject> MakeBasePresetRecipe(const TSharedRef<FJsonObject>& Request)
{
	FString TargetControlRigPath;
	FString SourceSkeletalMesh;
	FString PreviewSkeletalMesh;
	ReadStringField(Request, TEXT("target_control_rig_path"), TEXT("TargetControlRigPath"), TargetControlRigPath);
	ReadStringField(Request, TEXT("source_skeletal_mesh"), TEXT("SourceSkeletalMesh"), SourceSkeletalMesh);
	ReadStringField(Request, TEXT("preview_skeletal_mesh"), TEXT("PreviewSkeletalMesh"), PreviewSkeletalMesh);

	bool bImportPreviewBones = false;
	ReadBoolField(Request, TEXT("import_preview_bones"), TEXT("bImportPreviewBones"), bImportPreviewBones);

	int32 MaxImportBones = 0;
	ReadIntField(Request, TEXT("max_import_bones"), TEXT("MaxImportBones"), MaxImportBones);
	if (MaxImportBones <= 0)
	{
		MaxImportBones = bImportPreviewBones ? 128 : 1;
	}

	bool bCompile = true;
	bool bSave = false;
	bool bInspectTopology = true;
	bool bDirectSolve = true;
	bool bDependencyAudit = true;
	ReadBoolField(Request, TEXT("compile"), TEXT("bCompile"), bCompile);
	ReadBoolField(Request, TEXT("save"), TEXT("bSave"), bSave);
	ReadBoolField(Request, TEXT("inspect_topology"), TEXT("bInspectTopology"), bInspectTopology);
	ReadBoolField(Request, TEXT("direct_solve"), TEXT("bDirectSolve"), bDirectSolve);
	ReadBoolField(Request, TEXT("dependency_audit"), TEXT("bDependencyAudit"), bDependencyAudit);

	TSharedRef<FJsonObject> Recipe = MakeShared<FJsonObject>();
	Recipe->SetNumberField(TEXT("schema_version"), SupportedSchemaVersion);
	Recipe->SetStringField(TEXT("target_control_rig_path"), TargetControlRigPath);
	Recipe->SetStringField(TEXT("source_skeletal_mesh"), SourceSkeletalMesh);
	Recipe->SetStringField(TEXT("preview_skeletal_mesh"), PreviewSkeletalMesh);

	TSharedRef<FJsonObject> Hierarchy = MakeShared<FJsonObject>();
	Hierarchy->SetBoolField(TEXT("import_preview_bones"), bImportPreviewBones);
	Hierarchy->SetNumberField(TEXT("max_import_bones"), MaxImportBones);
	Hierarchy->SetArrayField(TEXT("nulls"), TArray<TSharedPtr<FJsonValue>>());
	Hierarchy->SetArrayField(TEXT("controls"), TArray<TSharedPtr<FJsonValue>>());
	Hierarchy->SetArrayField(TEXT("curves"), TArray<TSharedPtr<FJsonValue>>());
	Hierarchy->SetArrayField(TEXT("null_specs"), TArray<TSharedPtr<FJsonValue>>());
	Hierarchy->SetArrayField(TEXT("control_specs"), TArray<TSharedPtr<FJsonValue>>());
	Hierarchy->SetArrayField(TEXT("curve_specs"), TArray<TSharedPtr<FJsonValue>>());
	Recipe->SetObjectField(TEXT("hierarchy"), Hierarchy);

	TSharedRef<FJsonObject> RigVM = MakeShared<FJsonObject>();
	RigVM->SetArrayField(TEXT("units"), TArray<TSharedPtr<FJsonValue>>());
	RigVM->SetArrayField(TEXT("variables"), TArray<TSharedPtr<FJsonValue>>());
	RigVM->SetArrayField(TEXT("template_nodes"), TArray<TSharedPtr<FJsonValue>>());
	RigVM->SetArrayField(TEXT("function_references"), TArray<TSharedPtr<FJsonValue>>());
	RigVM->SetArrayField(TEXT("pin_defaults"), TArray<TSharedPtr<FJsonValue>>());
	RigVM->SetArrayField(TEXT("links"), TArray<TSharedPtr<FJsonValue>>());
	RigVM->SetArrayField(TEXT("exposed_pins"), TArray<TSharedPtr<FJsonValue>>());
	Recipe->SetObjectField(TEXT("rigvm"), RigVM);

	TSharedRef<FJsonObject> AnimBPBinding = MakeShared<FJsonObject>();
	AnimBPBinding->SetBoolField(TEXT("enabled"), false);
	AnimBPBinding->SetStringField(TEXT("target_anim_bp_path"), TEXT(""));
	AnimBPBinding->SetStringField(TEXT("source_anim_bp_path"), TEXT(""));
	Recipe->SetObjectField(TEXT("animbp_binding"), AnimBPBinding);

	TSharedRef<FJsonObject> Verification = MakeShared<FJsonObject>();
	Verification->SetBoolField(TEXT("compile"), bCompile);
	Verification->SetBoolField(TEXT("save"), bSave);
	Verification->SetBoolField(TEXT("inspect_topology"), bInspectTopology);
	Verification->SetBoolField(TEXT("direct_solve"), bDirectSolve);
	Verification->SetBoolField(TEXT("dependency_audit"), bDependencyAudit);
	Recipe->SetObjectField(TEXT("verification"), Verification);

	return Recipe;
}

void BuildSimpleFkPreset(const TSharedRef<FJsonObject>& Request, const TSharedRef<FJsonObject>& Recipe)
{
	FString RootParent;
	FString NamePrefix = TEXT("FK");
	ReadStringField(Request, TEXT("root_parent"), TEXT("RootParent"), RootParent);
	ReadStringField(Request, TEXT("name_prefix"), TEXT("NamePrefix"), NamePrefix);
	NamePrefix = SanitizePresetNamePart(NamePrefix, TEXT("FK"));

	FLinearColor ShapeColor(0.15f, 0.55f, 1.0f, 1.0f);
	ReadLinearColorField(Request, TEXT("shape_color"), TEXT("ShapeColor"), ShapeColor);

	const TSharedPtr<FJsonObject> Hierarchy = ReadObjectField(Recipe, TEXT("hierarchy"), TEXT("Hierarchy"));
	const TSharedPtr<FJsonObject> RigVM = ReadObjectField(Recipe, TEXT("rigvm"), TEXT("RigVM"));
	check(Hierarchy.IsValid());
	check(RigVM.IsValid());

	TArray<TSharedPtr<FJsonValue>> ControlSpecs;
	const TArray<TSharedPtr<FJsonValue>>* ChainValues = nullptr;
	if (!Request->TryGetArrayField(TEXT("chain"), ChainValues) &&
		!Request->TryGetArrayField(TEXT("controls"), ChainValues))
	{
		static const TCHAR* Defaults[] = { TEXT("Root"), TEXT("Mid"), TEXT("End") };
		FString Parent = RootParent;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Defaults); ++Index)
		{
			const FString ControlName = FString::Printf(TEXT("%s_%s"), *NamePrefix, Defaults[Index]);
			ControlSpecs.Add(MakeShared<FJsonValueObject>(
				MakeControlSpecJson(ControlName, Parent, TEXT("euler_transform"), 0.0, FVector::ZeroVector, FRotator::ZeroRotator, ShapeColor)));
			Parent = ControlName;
		}
	}
	else
	{
		FString Parent = RootParent;
		for (int32 Index = 0; Index < ChainValues->Num(); ++Index)
		{
			const TSharedPtr<FJsonValue>& Value = (*ChainValues)[Index];
			FString RawName;
			FString ExplicitParent;
			FVector Location = FVector::ZeroVector;
			FRotator Rotation = FRotator::ZeroRotator;
			FLinearColor EntryColor = ShapeColor;

			if (Value.IsValid() && Value->TryGetString(RawName))
			{
				RawName = SanitizePresetNamePart(RawName, FString::Printf(TEXT("%02d"), Index + 1));
				RawName = RawName.StartsWith(NamePrefix + TEXT("_")) ? RawName : FString::Printf(TEXT("%s_%s"), *NamePrefix, *RawName);
			}
			else
			{
				const TSharedPtr<FJsonObject>* EntryObject = nullptr;
				if (Value.IsValid() && Value->TryGetObject(EntryObject))
				{
					ReadStringField(EntryObject->ToSharedRef(), TEXT("name"), TEXT("Name"), RawName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("control_name"), TEXT("ControlName"), RawName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("parent"), TEXT("Parent"), ExplicitParent);
					ReadVectorField(EntryObject->ToSharedRef(), TEXT("location"), TEXT("Location"), Location);
					ReadRotatorField(EntryObject->ToSharedRef(), TEXT("rotation"), TEXT("Rotation"), Rotation);
					ReadLinearColorField(EntryObject->ToSharedRef(), TEXT("shape_color"), TEXT("ShapeColor"), EntryColor);
				}

				RawName = SanitizePresetNamePart(RawName, FString::Printf(TEXT("%s_%02d"), *NamePrefix, Index + 1));
				if (!RawName.StartsWith(NamePrefix + TEXT("_")))
				{
					RawName = FString::Printf(TEXT("%s_%s"), *NamePrefix, *RawName);
				}
			}

			const FString ControlParent = ExplicitParent.TrimStartAndEnd().IsEmpty() ? Parent : ExplicitParent;
			ControlSpecs.Add(MakeShared<FJsonValueObject>(
				MakeControlSpecJson(RawName, ControlParent, TEXT("euler_transform"), 0.0, Location, Rotation, EntryColor)));
			Parent = RawName;
		}
	}

	Hierarchy->SetArrayField(TEXT("control_specs"), ControlSpecs);

	TArray<TSharedPtr<FJsonValue>> Units;
	Units.Add(MakeShared<FJsonValueObject>(MakeUnitJson(TEXT("FRigUnit_BeginExecution"), TEXT("Preset_BeginExecution"), 0.0, 0.0)));
	RigVM->SetArrayField(TEXT("units"), Units);
}

void BuildCurveGatePreset(const TSharedRef<FJsonObject>& Request, const TSharedRef<FJsonObject>& Recipe)
{
	FString NamePrefix = TEXT("Gate");
	FString Parent;
	ReadStringField(Request, TEXT("name_prefix"), TEXT("NamePrefix"), NamePrefix);
	ReadStringField(Request, TEXT("parent"), TEXT("Parent"), Parent);
	NamePrefix = SanitizePresetNamePart(NamePrefix, TEXT("Gate"));

	FLinearColor ShapeColor(0.95f, 0.65f, 0.12f, 1.0f);
	ReadLinearColorField(Request, TEXT("shape_color"), TEXT("ShapeColor"), ShapeColor);

	const TSharedPtr<FJsonObject> Hierarchy = ReadObjectField(Recipe, TEXT("hierarchy"), TEXT("Hierarchy"));
	const TSharedPtr<FJsonObject> RigVM = ReadObjectField(Recipe, TEXT("rigvm"), TEXT("RigVM"));
	check(Hierarchy.IsValid());
	check(RigVM.IsValid());

	TArray<TSharedPtr<FJsonValue>> ControlSpecs;
	TArray<TSharedPtr<FJsonValue>> CurveSpecs;
	TArray<TSharedPtr<FJsonValue>> Units;
	TArray<TSharedPtr<FJsonValue>> PinDefaults;
	TArray<TSharedPtr<FJsonValue>> Links;
	Units.Add(MakeShared<FJsonValueObject>(MakeUnitJson(TEXT("FRigUnit_BeginExecution"), TEXT("Preset_BeginExecution"), 0.0, 0.0)));

	const TArray<TSharedPtr<FJsonValue>>* GateValues = nullptr;
	if (!Request->TryGetArrayField(TEXT("gates"), GateValues))
	{
		static const TCHAR* Defaults[] = { TEXT("Open"), TEXT("Weight") };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Defaults); ++Index)
		{
			const FString GateName = FString::Printf(TEXT("%s_%s"), *NamePrefix, Defaults[Index]);
			const FString ControlName = GateName + TEXT("_Control");
			const FString CurveName = GateName + TEXT("_Curve");
			const FString DefaultValue = Index == 0 ? TEXT("1.0") : TEXT("0.5");
			const double NumericDefault = FCString::Atod(*DefaultValue);

			ControlSpecs.Add(MakeShared<FJsonValueObject>(
				MakeControlSpecJson(ControlName, Parent, TEXT("float"), NumericDefault, FVector::ZeroVector, FRotator::ZeroRotator, ShapeColor)));
			CurveSpecs.Add(MakeShared<FJsonValueObject>(MakeCurveSpecJson(CurveName, NumericDefault)));
		}
	}
	else
	{
		for (int32 Index = 0; Index < GateValues->Num(); ++Index)
		{
			const TSharedPtr<FJsonValue>& Value = (*GateValues)[Index];
			FString GateName;
			FString ControlName;
			FString CurveName;
			FString EntryParent = Parent;
			FLinearColor EntryColor = ShapeColor;
			double NumericDefault = 1.0;

			if (Value.IsValid() && Value->TryGetString(GateName))
			{
				GateName = SanitizePresetNamePart(GateName, FString::Printf(TEXT("%02d"), Index + 1));
			}
			else
			{
				const TSharedPtr<FJsonObject>* EntryObject = nullptr;
				if (Value.IsValid() && Value->TryGetObject(EntryObject))
				{
					ReadStringField(EntryObject->ToSharedRef(), TEXT("name"), TEXT("Name"), GateName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("control_name"), TEXT("ControlName"), ControlName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("curve_name"), TEXT("CurveName"), CurveName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("parent"), TEXT("Parent"), EntryParent);
					EntryObject->ToSharedRef()->TryGetNumberField(TEXT("default_value"), NumericDefault);
					EntryObject->ToSharedRef()->TryGetNumberField(TEXT("value"), NumericDefault);
					ReadLinearColorField(EntryObject->ToSharedRef(), TEXT("shape_color"), TEXT("ShapeColor"), EntryColor);
				}
				GateName = SanitizePresetNamePart(GateName, FString::Printf(TEXT("%02d"), Index + 1));
			}

			if (!GateName.StartsWith(NamePrefix + TEXT("_")))
			{
				GateName = FString::Printf(TEXT("%s_%s"), *NamePrefix, *GateName);
			}
			ControlName = SanitizePresetNamePart(ControlName, GateName + TEXT("_Control"));
			CurveName = SanitizePresetNamePart(CurveName, GateName + TEXT("_Curve"));

			ControlSpecs.Add(MakeShared<FJsonValueObject>(
				MakeControlSpecJson(ControlName, EntryParent, TEXT("float"), NumericDefault, FVector::ZeroVector, FRotator::ZeroRotator, EntryColor)));
			CurveSpecs.Add(MakeShared<FJsonValueObject>(MakeCurveSpecJson(CurveName, NumericDefault)));
		}
	}

	Hierarchy->SetArrayField(TEXT("control_specs"), ControlSpecs);
	Hierarchy->SetArrayField(TEXT("curve_specs"), CurveSpecs);
	RigVM->SetArrayField(TEXT("units"), Units);
	RigVM->SetArrayField(TEXT("pin_defaults"), PinDefaults);
	RigVM->SetArrayField(TEXT("links"), Links);
}

void BuildHandFootTargetPreset(const TSharedRef<FJsonObject>& Request, const TSharedRef<FJsonObject>& Recipe)
{
	FString NamePrefix = TEXT("IK");
	FString Parent;
	ReadStringField(Request, TEXT("name_prefix"), TEXT("NamePrefix"), NamePrefix);
	ReadStringField(Request, TEXT("parent"), TEXT("Parent"), Parent);
	NamePrefix = SanitizePresetNamePart(NamePrefix, TEXT("IK"));

	bool bIncludeSpaces = true;
	bool bIncludePoles = true;
	ReadBoolField(Request, TEXT("include_spaces"), TEXT("bIncludeSpaces"), bIncludeSpaces);
	ReadBoolField(Request, TEXT("include_poles"), TEXT("bIncludePoles"), bIncludePoles);

	FLinearColor TargetColor(0.25f, 0.85f, 0.45f, 1.0f);
	FLinearColor PoleColor(0.9f, 0.4f, 0.2f, 1.0f);
	ReadLinearColorField(Request, TEXT("target_shape_color"), TEXT("TargetShapeColor"), TargetColor);
	ReadLinearColorField(Request, TEXT("pole_shape_color"), TEXT("PoleShapeColor"), PoleColor);

	const TSharedPtr<FJsonObject> Hierarchy = ReadObjectField(Recipe, TEXT("hierarchy"), TEXT("Hierarchy"));
	const TSharedPtr<FJsonObject> RigVM = ReadObjectField(Recipe, TEXT("rigvm"), TEXT("RigVM"));
	check(Hierarchy.IsValid());
	check(RigVM.IsValid());

	TArray<TSharedPtr<FJsonValue>> NullSpecs;
	TArray<TSharedPtr<FJsonValue>> ControlSpecs;

	struct FDefaultTarget
	{
		const TCHAR* Name;
		FVector TargetLocation;
		FVector PoleLocation;
	};

	static const FDefaultTarget Defaults[] =
	{
		{ TEXT("LeftHand"), FVector(25.0, -35.0, 110.0), FVector(25.0, -70.0, 85.0) },
		{ TEXT("RightHand"), FVector(25.0, 35.0, 110.0), FVector(25.0, 70.0, 85.0) },
		{ TEXT("LeftFoot"), FVector(5.0, -18.0, 10.0), FVector(35.0, -18.0, 55.0) },
		{ TEXT("RightFoot"), FVector(5.0, 18.0, 10.0), FVector(35.0, 18.0, 55.0) },
	};

	auto AddTargetSet = [&](
		FString RawName,
		FString TargetControlName,
		FString PoleControlName,
		FString SpaceName,
		FString EntryParent,
		FVector TargetLocation,
		FVector PoleLocation,
		FRotator TargetRotation,
		bool bEntryIncludeSpace,
		bool bEntryIncludePole,
		FLinearColor EntryTargetColor,
		FLinearColor EntryPoleColor)
	{
		RawName = SanitizePresetNamePart(RawName, TEXT("Target"));
		const FString BaseName = RawName.StartsWith(NamePrefix + TEXT("_"))
			? RawName
			: FString::Printf(TEXT("%s_%s"), *NamePrefix, *RawName);
		TargetControlName = SanitizePresetNamePart(TargetControlName, BaseName + TEXT("_Target"));
		PoleControlName = SanitizePresetNamePart(PoleControlName, BaseName + TEXT("_Pole"));
		SpaceName = SanitizePresetNamePart(SpaceName, BaseName + TEXT("_Space"));

		FString ControlParent = EntryParent;
		if (bEntryIncludeSpace)
		{
			NullSpecs.Add(MakeShared<FJsonValueObject>(MakeNullSpecJson(SpaceName, EntryParent, TargetLocation, TargetRotation)));
			ControlParent = SpaceName;
		}

		ControlSpecs.Add(MakeShared<FJsonValueObject>(
			MakeControlSpecJson(TargetControlName, ControlParent, TEXT("euler_transform"), 0.0, TargetLocation, TargetRotation, EntryTargetColor)));

		if (bEntryIncludePole)
		{
			ControlSpecs.Add(MakeShared<FJsonValueObject>(
				MakeControlSpecJson(PoleControlName, ControlParent, TEXT("euler_transform"), 0.0, PoleLocation, FRotator::ZeroRotator, EntryPoleColor)));
		}
	};

	const TArray<TSharedPtr<FJsonValue>>* TargetValues = nullptr;
	if (!Request->TryGetArrayField(TEXT("targets"), TargetValues) || TargetValues->IsEmpty())
	{
		for (const FDefaultTarget& DefaultTarget : Defaults)
		{
			AddTargetSet(
				DefaultTarget.Name,
				TEXT(""),
				TEXT(""),
				TEXT(""),
				Parent,
				DefaultTarget.TargetLocation,
				DefaultTarget.PoleLocation,
				FRotator::ZeroRotator,
				bIncludeSpaces,
				bIncludePoles,
				TargetColor,
				PoleColor);
		}
	}
	else
	{
		for (int32 Index = 0; Index < TargetValues->Num(); ++Index)
		{
			const TSharedPtr<FJsonValue>& Value = (*TargetValues)[Index];
			FString RawName;
			FString TargetControlName;
			FString PoleControlName;
			FString SpaceName;
			FString EntryParent = Parent;
			FVector TargetLocation = FVector::ZeroVector;
			FVector PoleLocation(35.0, 0.0, 55.0);
			FRotator TargetRotation = FRotator::ZeroRotator;
			bool bEntryIncludeSpace = bIncludeSpaces;
			bool bEntryIncludePole = bIncludePoles;
			FLinearColor EntryTargetColor = TargetColor;
			FLinearColor EntryPoleColor = PoleColor;

			if (Value.IsValid() && Value->TryGetString(RawName))
			{
				RawName = SanitizePresetNamePart(RawName, FString::Printf(TEXT("Target_%02d"), Index + 1));
			}
			else
			{
				const TSharedPtr<FJsonObject>* EntryObject = nullptr;
				if (Value.IsValid() && Value->TryGetObject(EntryObject))
				{
					ReadStringField(EntryObject->ToSharedRef(), TEXT("name"), TEXT("Name"), RawName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("target_control_name"), TEXT("TargetControlName"), TargetControlName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("control_name"), TEXT("ControlName"), TargetControlName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("pole_control_name"), TEXT("PoleControlName"), PoleControlName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("space_name"), TEXT("SpaceName"), SpaceName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("parent"), TEXT("Parent"), EntryParent);
					ReadVectorField(EntryObject->ToSharedRef(), TEXT("target_location"), TEXT("TargetLocation"), TargetLocation);
					ReadVectorField(EntryObject->ToSharedRef(), TEXT("location"), TEXT("Location"), TargetLocation);
					ReadVectorField(EntryObject->ToSharedRef(), TEXT("pole_location"), TEXT("PoleLocation"), PoleLocation);
					ReadRotatorField(EntryObject->ToSharedRef(), TEXT("target_rotation"), TEXT("TargetRotation"), TargetRotation);
					ReadRotatorField(EntryObject->ToSharedRef(), TEXT("rotation"), TEXT("Rotation"), TargetRotation);
					ReadBoolField(EntryObject->ToSharedRef(), TEXT("include_space"), TEXT("bIncludeSpace"), bEntryIncludeSpace);
					ReadBoolField(EntryObject->ToSharedRef(), TEXT("include_pole"), TEXT("bIncludePole"), bEntryIncludePole);
					ReadLinearColorField(EntryObject->ToSharedRef(), TEXT("target_shape_color"), TEXT("TargetShapeColor"), EntryTargetColor);
					ReadLinearColorField(EntryObject->ToSharedRef(), TEXT("pole_shape_color"), TEXT("PoleShapeColor"), EntryPoleColor);
				}
				RawName = SanitizePresetNamePart(RawName, FString::Printf(TEXT("Target_%02d"), Index + 1));
			}

			AddTargetSet(
				RawName,
				TargetControlName,
				PoleControlName,
				SpaceName,
				EntryParent,
				TargetLocation,
				PoleLocation,
				TargetRotation,
				bEntryIncludeSpace,
				bEntryIncludePole,
				EntryTargetColor,
				EntryPoleColor);
		}
	}

	Hierarchy->SetArrayField(TEXT("null_specs"), NullSpecs);
	Hierarchy->SetArrayField(TEXT("control_specs"), ControlSpecs);

	TArray<TSharedPtr<FJsonValue>> Units;
	Units.Add(MakeShared<FJsonValueObject>(MakeUnitJson(TEXT("FRigUnit_BeginExecution"), TEXT("Preset_BeginExecution"), 0.0, 0.0)));
	RigVM->SetArrayField(TEXT("units"), Units);
}

void BuildLookAtAimTargetPreset(const TSharedRef<FJsonObject>& Request, const TSharedRef<FJsonObject>& Recipe)
{
	FString NamePrefix = TEXT("Aim");
	FString Parent;
	ReadStringField(Request, TEXT("name_prefix"), TEXT("NamePrefix"), NamePrefix);
	ReadStringField(Request, TEXT("parent"), TEXT("Parent"), Parent);
	NamePrefix = SanitizePresetNamePart(NamePrefix, TEXT("Aim"));

	bool bIncludeSpaces = true;
	bool bIncludeUpVectors = true;
	bool bIncludeWeightCurves = true;
	ReadBoolField(Request, TEXT("include_spaces"), TEXT("bIncludeSpaces"), bIncludeSpaces);
	ReadBoolField(Request, TEXT("include_up_vectors"), TEXT("bIncludeUpVectors"), bIncludeUpVectors);
	ReadBoolField(Request, TEXT("include_weight_curves"), TEXT("bIncludeWeightCurves"), bIncludeWeightCurves);

	FLinearColor TargetColor(0.35f, 0.65f, 1.0f, 1.0f);
	FLinearColor UpVectorColor(0.85f, 0.45f, 1.0f, 1.0f);
	ReadLinearColorField(Request, TEXT("target_shape_color"), TEXT("TargetShapeColor"), TargetColor);
	ReadLinearColorField(Request, TEXT("up_vector_shape_color"), TEXT("UpVectorShapeColor"), UpVectorColor);

	const TSharedPtr<FJsonObject> Hierarchy = ReadObjectField(Recipe, TEXT("hierarchy"), TEXT("Hierarchy"));
	const TSharedPtr<FJsonObject> RigVM = ReadObjectField(Recipe, TEXT("rigvm"), TEXT("RigVM"));
	check(Hierarchy.IsValid());
	check(RigVM.IsValid());

	TArray<TSharedPtr<FJsonValue>> NullSpecs;
	TArray<TSharedPtr<FJsonValue>> ControlSpecs;
	TArray<TSharedPtr<FJsonValue>> CurveSpecs;

	struct FDefaultAimTarget
	{
		const TCHAR* Name;
		FVector TargetLocation;
		FVector UpVectorLocation;
		float Weight;
	};

	static const FDefaultAimTarget Defaults[] =
	{
		{ TEXT("HeadLook"), FVector(120.0, 0.0, 155.0), FVector(120.0, 0.0, 210.0), 1.0f },
		{ TEXT("ChestAim"), FVector(105.0, 0.0, 115.0), FVector(105.0, 0.0, 170.0), 0.75f },
	};

	auto AddAimTargetSet = [&](
		FString RawName,
		FString TargetControlName,
		FString UpVectorControlName,
		FString SpaceName,
		FString WeightCurveName,
		FString EntryParent,
		FVector TargetLocation,
		FVector UpVectorLocation,
		FRotator TargetRotation,
		bool bEntryIncludeSpace,
		bool bEntryIncludeUpVector,
		bool bEntryIncludeWeightCurve,
		float Weight,
		FLinearColor EntryTargetColor,
		FLinearColor EntryUpVectorColor)
	{
		RawName = SanitizePresetNamePart(RawName, TEXT("Target"));
		const FString BaseName = RawName.StartsWith(NamePrefix + TEXT("_"))
			? RawName
			: FString::Printf(TEXT("%s_%s"), *NamePrefix, *RawName);
		TargetControlName = SanitizePresetNamePart(TargetControlName, BaseName + TEXT("_Target"));
		UpVectorControlName = SanitizePresetNamePart(UpVectorControlName, BaseName + TEXT("_UpVector"));
		SpaceName = SanitizePresetNamePart(SpaceName, BaseName + TEXT("_Space"));
		WeightCurveName = SanitizePresetNamePart(WeightCurveName, BaseName + TEXT("_Weight"));

		FString ControlParent = EntryParent;
		if (bEntryIncludeSpace)
		{
			NullSpecs.Add(MakeShared<FJsonValueObject>(MakeNullSpecJson(SpaceName, EntryParent, TargetLocation, TargetRotation)));
			ControlParent = SpaceName;
		}

		ControlSpecs.Add(MakeShared<FJsonValueObject>(
			MakeControlSpecJson(TargetControlName, ControlParent, TEXT("euler_transform"), 0.0, TargetLocation, TargetRotation, EntryTargetColor)));

		if (bEntryIncludeUpVector)
		{
			ControlSpecs.Add(MakeShared<FJsonValueObject>(
				MakeControlSpecJson(UpVectorControlName, ControlParent, TEXT("euler_transform"), 0.0, UpVectorLocation, FRotator::ZeroRotator, EntryUpVectorColor)));
		}

		if (bEntryIncludeWeightCurve)
		{
			CurveSpecs.Add(MakeShared<FJsonValueObject>(MakeCurveSpecJson(WeightCurveName, Weight)));
		}
	};

	const TArray<TSharedPtr<FJsonValue>>* TargetValues = nullptr;
	if (!Request->TryGetArrayField(TEXT("targets"), TargetValues) || TargetValues->IsEmpty())
	{
		for (const FDefaultAimTarget& DefaultTarget : Defaults)
		{
			AddAimTargetSet(
				DefaultTarget.Name,
				TEXT(""),
				TEXT(""),
				TEXT(""),
				TEXT(""),
				Parent,
				DefaultTarget.TargetLocation,
				DefaultTarget.UpVectorLocation,
				FRotator::ZeroRotator,
				bIncludeSpaces,
				bIncludeUpVectors,
				bIncludeWeightCurves,
				DefaultTarget.Weight,
				TargetColor,
				UpVectorColor);
		}
	}
	else
	{
		for (int32 Index = 0; Index < TargetValues->Num(); ++Index)
		{
			const TSharedPtr<FJsonValue>& Value = (*TargetValues)[Index];
			FString RawName;
			FString TargetControlName;
			FString UpVectorControlName;
			FString SpaceName;
			FString WeightCurveName;
			FString EntryParent = Parent;
			FVector TargetLocation(120.0, 0.0, 155.0);
			FVector UpVectorLocation(120.0, 0.0, 210.0);
			FRotator TargetRotation = FRotator::ZeroRotator;
			bool bEntryIncludeSpace = bIncludeSpaces;
			bool bEntryIncludeUpVector = bIncludeUpVectors;
			bool bEntryIncludeWeightCurve = bIncludeWeightCurves;
			float Weight = 1.0f;
			FLinearColor EntryTargetColor = TargetColor;
			FLinearColor EntryUpVectorColor = UpVectorColor;

			if (Value.IsValid() && Value->TryGetString(RawName))
			{
				RawName = SanitizePresetNamePart(RawName, FString::Printf(TEXT("Target_%02d"), Index + 1));
			}
			else
			{
				const TSharedPtr<FJsonObject>* EntryObject = nullptr;
				if (Value.IsValid() && Value->TryGetObject(EntryObject))
				{
					ReadStringField(EntryObject->ToSharedRef(), TEXT("name"), TEXT("Name"), RawName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("target_control_name"), TEXT("TargetControlName"), TargetControlName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("control_name"), TEXT("ControlName"), TargetControlName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("up_vector_control_name"), TEXT("UpVectorControlName"), UpVectorControlName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("space_name"), TEXT("SpaceName"), SpaceName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("weight_curve_name"), TEXT("WeightCurveName"), WeightCurveName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("parent"), TEXT("Parent"), EntryParent);
					ReadVectorField(EntryObject->ToSharedRef(), TEXT("target_location"), TEXT("TargetLocation"), TargetLocation);
					ReadVectorField(EntryObject->ToSharedRef(), TEXT("location"), TEXT("Location"), TargetLocation);
					ReadVectorField(EntryObject->ToSharedRef(), TEXT("up_vector_location"), TEXT("UpVectorLocation"), UpVectorLocation);
					ReadRotatorField(EntryObject->ToSharedRef(), TEXT("target_rotation"), TEXT("TargetRotation"), TargetRotation);
					ReadRotatorField(EntryObject->ToSharedRef(), TEXT("rotation"), TEXT("Rotation"), TargetRotation);
					ReadBoolField(EntryObject->ToSharedRef(), TEXT("include_space"), TEXT("bIncludeSpace"), bEntryIncludeSpace);
					ReadBoolField(EntryObject->ToSharedRef(), TEXT("include_up_vector"), TEXT("bIncludeUpVector"), bEntryIncludeUpVector);
					ReadBoolField(EntryObject->ToSharedRef(), TEXT("include_weight_curve"), TEXT("bIncludeWeightCurve"), bEntryIncludeWeightCurve);
					ReadFloatField(EntryObject->ToSharedRef(), TEXT("weight"), TEXT("Weight"), Weight);
					ReadFloatField(EntryObject->ToSharedRef(), TEXT("default_weight"), TEXT("DefaultWeight"), Weight);
					ReadLinearColorField(EntryObject->ToSharedRef(), TEXT("target_shape_color"), TEXT("TargetShapeColor"), EntryTargetColor);
					ReadLinearColorField(EntryObject->ToSharedRef(), TEXT("up_vector_shape_color"), TEXT("UpVectorShapeColor"), EntryUpVectorColor);
				}
				RawName = SanitizePresetNamePart(RawName, FString::Printf(TEXT("Target_%02d"), Index + 1));
			}

			AddAimTargetSet(
				RawName,
				TargetControlName,
				UpVectorControlName,
				SpaceName,
				WeightCurveName,
				EntryParent,
				TargetLocation,
				UpVectorLocation,
				TargetRotation,
				bEntryIncludeSpace,
				bEntryIncludeUpVector,
				bEntryIncludeWeightCurve,
				Weight,
				EntryTargetColor,
				EntryUpVectorColor);
		}
	}

	Hierarchy->SetArrayField(TEXT("null_specs"), NullSpecs);
	Hierarchy->SetArrayField(TEXT("control_specs"), ControlSpecs);
	Hierarchy->SetArrayField(TEXT("curve_specs"), CurveSpecs);

	TArray<TSharedPtr<FJsonValue>> Units;
	Units.Add(MakeShared<FJsonValueObject>(MakeUnitJson(TEXT("FRigUnit_BeginExecution"), TEXT("Preset_BeginExecution"), 0.0, 0.0)));
	RigVM->SetArrayField(TEXT("units"), Units);
}

void BuildTwoBoneIkLimbPreset(const TSharedRef<FJsonObject>& Request, const TSharedRef<FJsonObject>& Recipe)
{
	FString NamePrefix = TEXT("TwoBoneIK");
	FString Parent;
	ReadStringField(Request, TEXT("name_prefix"), TEXT("NamePrefix"), NamePrefix);
	ReadStringField(Request, TEXT("parent"), TEXT("Parent"), Parent);
	NamePrefix = SanitizePresetNamePart(NamePrefix, TEXT("TwoBoneIK"));

	bool bIncludeSpaces = true;
	bool bIncludePoles = true;
	bool bIncludeJointControls = true;
	bool bIncludeWeightCurves = true;
	ReadBoolField(Request, TEXT("include_spaces"), TEXT("bIncludeSpaces"), bIncludeSpaces);
	ReadBoolField(Request, TEXT("include_poles"), TEXT("bIncludePoles"), bIncludePoles);
	ReadBoolField(Request, TEXT("include_joint_controls"), TEXT("bIncludeJointControls"), bIncludeJointControls);
	ReadBoolField(Request, TEXT("include_joints"), TEXT("bIncludeJoints"), bIncludeJointControls);
	ReadBoolField(Request, TEXT("include_weight_curves"), TEXT("bIncludeWeightCurves"), bIncludeWeightCurves);

	FLinearColor TargetColor(0.2f, 0.85f, 0.55f, 1.0f);
	FLinearColor PoleColor(0.95f, 0.45f, 0.2f, 1.0f);
	FLinearColor JointColor(0.35f, 0.7f, 1.0f, 1.0f);
	ReadLinearColorField(Request, TEXT("target_shape_color"), TEXT("TargetShapeColor"), TargetColor);
	ReadLinearColorField(Request, TEXT("pole_shape_color"), TEXT("PoleShapeColor"), PoleColor);
	ReadLinearColorField(Request, TEXT("joint_shape_color"), TEXT("JointShapeColor"), JointColor);

	const TSharedPtr<FJsonObject> Hierarchy = ReadObjectField(Recipe, TEXT("hierarchy"), TEXT("Hierarchy"));
	const TSharedPtr<FJsonObject> RigVM = ReadObjectField(Recipe, TEXT("rigvm"), TEXT("RigVM"));
	check(Hierarchy.IsValid());
	check(RigVM.IsValid());

	TArray<TSharedPtr<FJsonValue>> NullSpecs;
	TArray<TSharedPtr<FJsonValue>> ControlSpecs;
	TArray<TSharedPtr<FJsonValue>> CurveSpecs;

	struct FDefaultLimb
	{
		const TCHAR* Name;
		FVector RootLocation;
		FVector MidLocation;
		FVector TargetLocation;
		FVector PoleLocation;
		float Weight;
	};

	static const FDefaultLimb Defaults[] =
	{
		{ TEXT("LeftArm"), FVector(0.0, -18.0, 135.0), FVector(35.0, -32.0, 110.0), FVector(70.0, -45.0, 105.0), FVector(35.0, -78.0, 115.0), 1.0f },
		{ TEXT("RightArm"), FVector(0.0, 18.0, 135.0), FVector(35.0, 32.0, 110.0), FVector(70.0, 45.0, 105.0), FVector(35.0, 78.0, 115.0), 1.0f },
		{ TEXT("LeftLeg"), FVector(0.0, -10.0, 90.0), FVector(10.0, -12.0, 45.0), FVector(5.0, -14.0, 5.0), FVector(45.0, -24.0, 55.0), 1.0f },
		{ TEXT("RightLeg"), FVector(0.0, 10.0, 90.0), FVector(10.0, 12.0, 45.0), FVector(5.0, 14.0, 5.0), FVector(45.0, 24.0, 55.0), 1.0f },
	};

	auto AddLimbSet = [&](
		FString RawName,
		FString RootControlName,
		FString MidControlName,
		FString TargetControlName,
		FString PoleControlName,
		FString SpaceName,
		FString WeightCurveName,
		FString EntryParent,
		FVector RootLocation,
		FVector MidLocation,
		FVector TargetLocation,
		FVector PoleLocation,
		FRotator RootRotation,
		FRotator MidRotation,
		FRotator TargetRotation,
		bool bEntryIncludeSpace,
		bool bEntryIncludePole,
		bool bEntryIncludeJointControls,
		bool bEntryIncludeWeightCurve,
		float Weight,
		FLinearColor EntryTargetColor,
		FLinearColor EntryPoleColor,
		FLinearColor EntryJointColor)
	{
		RawName = SanitizePresetNamePart(RawName, TEXT("Limb"));
		const FString BaseName = RawName.StartsWith(NamePrefix + TEXT("_"))
			? RawName
			: FString::Printf(TEXT("%s_%s"), *NamePrefix, *RawName);
		RootControlName = SanitizePresetNamePart(RootControlName, BaseName + TEXT("_Root"));
		MidControlName = SanitizePresetNamePart(MidControlName, BaseName + TEXT("_Mid"));
		TargetControlName = SanitizePresetNamePart(TargetControlName, BaseName + TEXT("_Target"));
		PoleControlName = SanitizePresetNamePart(PoleControlName, BaseName + TEXT("_Pole"));
		SpaceName = SanitizePresetNamePart(SpaceName, BaseName + TEXT("_Space"));
		WeightCurveName = SanitizePresetNamePart(WeightCurveName, BaseName + TEXT("_Weight"));

		if (bEntryIncludeJointControls)
		{
			ControlSpecs.Add(MakeShared<FJsonValueObject>(
				MakeControlSpecJson(RootControlName, EntryParent, TEXT("euler_transform"), 0.0, RootLocation, RootRotation, EntryJointColor)));
			ControlSpecs.Add(MakeShared<FJsonValueObject>(
				MakeControlSpecJson(MidControlName, RootControlName, TEXT("euler_transform"), 0.0, MidLocation, MidRotation, EntryJointColor)));
		}

		FString TargetParent = EntryParent;
		if (bEntryIncludeSpace)
		{
			NullSpecs.Add(MakeShared<FJsonValueObject>(MakeNullSpecJson(SpaceName, EntryParent, TargetLocation, TargetRotation)));
			TargetParent = SpaceName;
		}

		ControlSpecs.Add(MakeShared<FJsonValueObject>(
			MakeControlSpecJson(TargetControlName, TargetParent, TEXT("euler_transform"), 0.0, TargetLocation, TargetRotation, EntryTargetColor)));

		if (bEntryIncludePole)
		{
			ControlSpecs.Add(MakeShared<FJsonValueObject>(
				MakeControlSpecJson(PoleControlName, TargetParent, TEXT("euler_transform"), 0.0, PoleLocation, FRotator::ZeroRotator, EntryPoleColor)));
		}

		if (bEntryIncludeWeightCurve)
		{
			CurveSpecs.Add(MakeShared<FJsonValueObject>(MakeCurveSpecJson(WeightCurveName, Weight)));
		}
	};

	const TArray<TSharedPtr<FJsonValue>>* LimbValues = nullptr;
	if ((!Request->TryGetArrayField(TEXT("limbs"), LimbValues) &&
		!Request->TryGetArrayField(TEXT("targets"), LimbValues)) ||
		LimbValues->IsEmpty())
	{
		for (const FDefaultLimb& DefaultLimb : Defaults)
		{
			AddLimbSet(
				DefaultLimb.Name,
				TEXT(""),
				TEXT(""),
				TEXT(""),
				TEXT(""),
				TEXT(""),
				TEXT(""),
				Parent,
				DefaultLimb.RootLocation,
				DefaultLimb.MidLocation,
				DefaultLimb.TargetLocation,
				DefaultLimb.PoleLocation,
				FRotator::ZeroRotator,
				FRotator::ZeroRotator,
				FRotator::ZeroRotator,
				bIncludeSpaces,
				bIncludePoles,
				bIncludeJointControls,
				bIncludeWeightCurves,
				DefaultLimb.Weight,
				TargetColor,
				PoleColor,
				JointColor);
		}
	}
	else
	{
		for (int32 Index = 0; Index < LimbValues->Num(); ++Index)
		{
			const TSharedPtr<FJsonValue>& Value = (*LimbValues)[Index];
			FString RawName;
			FString RootControlName;
			FString MidControlName;
			FString TargetControlName;
			FString PoleControlName;
			FString SpaceName;
			FString WeightCurveName;
			FString EntryParent = Parent;
			FVector RootLocation(0.0, 0.0, 100.0);
			FVector MidLocation(35.0, 0.0, 75.0);
			FVector TargetLocation(70.0, 0.0, 75.0);
			FVector PoleLocation(35.0, -45.0, 85.0);
			FRotator RootRotation = FRotator::ZeroRotator;
			FRotator MidRotation = FRotator::ZeroRotator;
			FRotator TargetRotation = FRotator::ZeroRotator;
			bool bEntryIncludeSpace = bIncludeSpaces;
			bool bEntryIncludePole = bIncludePoles;
			bool bEntryIncludeJointControls = bIncludeJointControls;
			bool bEntryIncludeWeightCurve = bIncludeWeightCurves;
			float Weight = 1.0f;
			FLinearColor EntryTargetColor = TargetColor;
			FLinearColor EntryPoleColor = PoleColor;
			FLinearColor EntryJointColor = JointColor;

			if (Value.IsValid() && Value->TryGetString(RawName))
			{
				RawName = SanitizePresetNamePart(RawName, FString::Printf(TEXT("Limb_%02d"), Index + 1));
			}
			else
			{
				const TSharedPtr<FJsonObject>* EntryObject = nullptr;
				if (Value.IsValid() && Value->TryGetObject(EntryObject))
				{
					ReadStringField(EntryObject->ToSharedRef(), TEXT("name"), TEXT("Name"), RawName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("root_control_name"), TEXT("RootControlName"), RootControlName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("mid_control_name"), TEXT("MidControlName"), MidControlName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("target_control_name"), TEXT("TargetControlName"), TargetControlName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("control_name"), TEXT("ControlName"), TargetControlName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("pole_control_name"), TEXT("PoleControlName"), PoleControlName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("space_name"), TEXT("SpaceName"), SpaceName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("weight_curve_name"), TEXT("WeightCurveName"), WeightCurveName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("parent"), TEXT("Parent"), EntryParent);
					ReadVectorField(EntryObject->ToSharedRef(), TEXT("root_location"), TEXT("RootLocation"), RootLocation);
					ReadVectorField(EntryObject->ToSharedRef(), TEXT("mid_location"), TEXT("MidLocation"), MidLocation);
					ReadVectorField(EntryObject->ToSharedRef(), TEXT("joint_location"), TEXT("JointLocation"), MidLocation);
					ReadVectorField(EntryObject->ToSharedRef(), TEXT("target_location"), TEXT("TargetLocation"), TargetLocation);
					ReadVectorField(EntryObject->ToSharedRef(), TEXT("end_location"), TEXT("EndLocation"), TargetLocation);
					ReadVectorField(EntryObject->ToSharedRef(), TEXT("location"), TEXT("Location"), TargetLocation);
					ReadVectorField(EntryObject->ToSharedRef(), TEXT("pole_location"), TEXT("PoleLocation"), PoleLocation);
					ReadRotatorField(EntryObject->ToSharedRef(), TEXT("root_rotation"), TEXT("RootRotation"), RootRotation);
					ReadRotatorField(EntryObject->ToSharedRef(), TEXT("mid_rotation"), TEXT("MidRotation"), MidRotation);
					ReadRotatorField(EntryObject->ToSharedRef(), TEXT("target_rotation"), TEXT("TargetRotation"), TargetRotation);
					ReadRotatorField(EntryObject->ToSharedRef(), TEXT("rotation"), TEXT("Rotation"), TargetRotation);
					ReadBoolField(EntryObject->ToSharedRef(), TEXT("include_space"), TEXT("bIncludeSpace"), bEntryIncludeSpace);
					ReadBoolField(EntryObject->ToSharedRef(), TEXT("include_pole"), TEXT("bIncludePole"), bEntryIncludePole);
					ReadBoolField(EntryObject->ToSharedRef(), TEXT("include_joint_controls"), TEXT("bIncludeJointControls"), bEntryIncludeJointControls);
					ReadBoolField(EntryObject->ToSharedRef(), TEXT("include_joints"), TEXT("bIncludeJoints"), bEntryIncludeJointControls);
					ReadBoolField(EntryObject->ToSharedRef(), TEXT("include_weight_curve"), TEXT("bIncludeWeightCurve"), bEntryIncludeWeightCurve);
					ReadFloatField(EntryObject->ToSharedRef(), TEXT("weight"), TEXT("Weight"), Weight);
					ReadFloatField(EntryObject->ToSharedRef(), TEXT("default_weight"), TEXT("DefaultWeight"), Weight);
					ReadLinearColorField(EntryObject->ToSharedRef(), TEXT("target_shape_color"), TEXT("TargetShapeColor"), EntryTargetColor);
					ReadLinearColorField(EntryObject->ToSharedRef(), TEXT("pole_shape_color"), TEXT("PoleShapeColor"), EntryPoleColor);
					ReadLinearColorField(EntryObject->ToSharedRef(), TEXT("joint_shape_color"), TEXT("JointShapeColor"), EntryJointColor);
				}
				RawName = SanitizePresetNamePart(RawName, FString::Printf(TEXT("Limb_%02d"), Index + 1));
			}

			AddLimbSet(
				RawName,
				RootControlName,
				MidControlName,
				TargetControlName,
				PoleControlName,
				SpaceName,
				WeightCurveName,
				EntryParent,
				RootLocation,
				MidLocation,
				TargetLocation,
				PoleLocation,
				RootRotation,
				MidRotation,
				TargetRotation,
				bEntryIncludeSpace,
				bEntryIncludePole,
				bEntryIncludeJointControls,
				bEntryIncludeWeightCurve,
				Weight,
				EntryTargetColor,
				EntryPoleColor,
				EntryJointColor);
		}
	}

	Hierarchy->SetArrayField(TEXT("null_specs"), NullSpecs);
	Hierarchy->SetArrayField(TEXT("control_specs"), ControlSpecs);
	Hierarchy->SetArrayField(TEXT("curve_specs"), CurveSpecs);

	TArray<TSharedPtr<FJsonValue>> Units;
	Units.Add(MakeShared<FJsonValueObject>(MakeUnitJson(TEXT("FRigUnit_BeginExecution"), TEXT("Preset_BeginExecution"), 0.0, 0.0)));
	RigVM->SetArrayField(TEXT("units"), Units);
}

void BuildSpineSplineControlChainPreset(const TSharedRef<FJsonObject>& Request, const TSharedRef<FJsonObject>& Recipe)
{
	FString NamePrefix = TEXT("Spine");
	FString Parent;
	ReadStringField(Request, TEXT("name_prefix"), TEXT("NamePrefix"), NamePrefix);
	ReadStringField(Request, TEXT("parent"), TEXT("Parent"), Parent);
	NamePrefix = SanitizePresetNamePart(NamePrefix, TEXT("Spine"));

	bool bIncludeSpaces = true;
	bool bIncludeTangentControls = true;
	bool bIncludeWeightCurves = true;
	bool bIncludeStretchCurve = true;
	bool bChainParentControls = false;
	ReadBoolField(Request, TEXT("include_spaces"), TEXT("bIncludeSpaces"), bIncludeSpaces);
	ReadBoolField(Request, TEXT("include_tangent_controls"), TEXT("bIncludeTangentControls"), bIncludeTangentControls);
	ReadBoolField(Request, TEXT("include_tangents"), TEXT("bIncludeTangents"), bIncludeTangentControls);
	ReadBoolField(Request, TEXT("include_weight_curves"), TEXT("bIncludeWeightCurves"), bIncludeWeightCurves);
	ReadBoolField(Request, TEXT("include_stretch_curve"), TEXT("bIncludeStretchCurve"), bIncludeStretchCurve);
	ReadBoolField(Request, TEXT("chain_parent_controls"), TEXT("bChainParentControls"), bChainParentControls);

	float BendWeight = 1.0f;
	float Stretch = 0.0f;
	ReadFloatField(Request, TEXT("default_bend_weight"), TEXT("DefaultBendWeight"), BendWeight);
	ReadFloatField(Request, TEXT("default_stretch"), TEXT("DefaultStretch"), Stretch);

	FString BendWeightCurveName = NamePrefix + TEXT("_BendWeight");
	FString StretchCurveName = NamePrefix + TEXT("_Stretch");
	ReadStringField(Request, TEXT("bend_weight_curve_name"), TEXT("BendWeightCurveName"), BendWeightCurveName);
	ReadStringField(Request, TEXT("stretch_curve_name"), TEXT("StretchCurveName"), StretchCurveName);

	FLinearColor ControlColor(0.35f, 0.75f, 1.0f, 1.0f);
	FLinearColor TangentColor(0.85f, 0.55f, 1.0f, 1.0f);
	ReadLinearColorField(Request, TEXT("control_shape_color"), TEXT("ControlShapeColor"), ControlColor);
	ReadLinearColorField(Request, TEXT("tangent_shape_color"), TEXT("TangentShapeColor"), TangentColor);

	const TSharedPtr<FJsonObject> Hierarchy = ReadObjectField(Recipe, TEXT("hierarchy"), TEXT("Hierarchy"));
	const TSharedPtr<FJsonObject> RigVM = ReadObjectField(Recipe, TEXT("rigvm"), TEXT("RigVM"));
	check(Hierarchy.IsValid());
	check(RigVM.IsValid());

	TArray<TSharedPtr<FJsonValue>> NullSpecs;
	TArray<TSharedPtr<FJsonValue>> ControlSpecs;
	TArray<TSharedPtr<FJsonValue>> CurveSpecs;

	struct FDefaultSpinePoint
	{
		const TCHAR* Name;
		FVector Location;
		FVector TangentLocation;
	};

	static const FDefaultSpinePoint Defaults[] =
	{
		{ TEXT("Pelvis"), FVector(0.0, 0.0, 90.0), FVector(35.0, 0.0, 90.0) },
		{ TEXT("Chest"), FVector(0.0, 0.0, 125.0), FVector(35.0, 0.0, 125.0) },
		{ TEXT("Head"), FVector(0.0, 0.0, 155.0), FVector(35.0, 0.0, 155.0) },
	};

	auto AddSpinePoint = [&](
		FString RawName,
		FString ControlName,
		FString TangentControlName,
		FString SpaceName,
		FString EntryParent,
		FVector Location,
		FVector TangentLocation,
		FRotator Rotation,
		FRotator TangentRotation,
		bool bEntryIncludeSpace,
		bool bEntryIncludeTangentControl,
		FLinearColor EntryControlColor,
		FLinearColor EntryTangentColor) -> FString
	{
		RawName = SanitizePresetNamePart(RawName, TEXT("Point"));
		const FString BaseName = RawName.StartsWith(NamePrefix + TEXT("_"))
			? RawName
			: FString::Printf(TEXT("%s_%s"), *NamePrefix, *RawName);
		ControlName = SanitizePresetNamePart(ControlName, BaseName + TEXT("_Control"));
		TangentControlName = SanitizePresetNamePart(TangentControlName, BaseName + TEXT("_Tangent"));
		SpaceName = SanitizePresetNamePart(SpaceName, BaseName + TEXT("_Space"));

		FString ControlParent = EntryParent;
		if (bEntryIncludeSpace)
		{
			NullSpecs.Add(MakeShared<FJsonValueObject>(MakeNullSpecJson(SpaceName, EntryParent, Location, Rotation)));
			ControlParent = SpaceName;
		}

		ControlSpecs.Add(MakeShared<FJsonValueObject>(
			MakeControlSpecJson(ControlName, ControlParent, TEXT("euler_transform"), 0.0, Location, Rotation, EntryControlColor)));

		if (bEntryIncludeTangentControl)
		{
			ControlSpecs.Add(MakeShared<FJsonValueObject>(
				MakeControlSpecJson(TangentControlName, ControlName, TEXT("euler_transform"), 0.0, TangentLocation, TangentRotation, EntryTangentColor)));
		}

		return ControlName;
	};

	const TArray<TSharedPtr<FJsonValue>>* PointValues = nullptr;
	if ((!Request->TryGetArrayField(TEXT("points"), PointValues) &&
		!Request->TryGetArrayField(TEXT("controls"), PointValues)) ||
		PointValues->IsEmpty())
	{
		FString ChainParent = Parent;
		for (const FDefaultSpinePoint& DefaultPoint : Defaults)
		{
			const FString AuthoredControl = AddSpinePoint(
				DefaultPoint.Name,
				TEXT(""),
				TEXT(""),
				TEXT(""),
				ChainParent,
				DefaultPoint.Location,
				DefaultPoint.TangentLocation,
				FRotator::ZeroRotator,
				FRotator::ZeroRotator,
				bIncludeSpaces,
				bIncludeTangentControls,
				ControlColor,
				TangentColor);
			if (bChainParentControls)
			{
				ChainParent = AuthoredControl;
			}
		}
	}
	else
	{
		FString ChainParent = Parent;
		for (int32 Index = 0; Index < PointValues->Num(); ++Index)
		{
			const FDefaultSpinePoint& DefaultPoint = Defaults[FMath::Min(Index, static_cast<int32>(UE_ARRAY_COUNT(Defaults)) - 1)];
			const TSharedPtr<FJsonValue>& Value = (*PointValues)[Index];
			FString RawName;
			FString ControlName;
			FString TangentControlName;
			FString SpaceName;
			FString EntryParent = ChainParent;
			FVector Location = DefaultPoint.Location;
			FVector TangentLocation = DefaultPoint.TangentLocation;
			FRotator Rotation = FRotator::ZeroRotator;
			FRotator TangentRotation = FRotator::ZeroRotator;
			bool bEntryIncludeSpace = bIncludeSpaces;
			bool bEntryIncludeTangentControl = bIncludeTangentControls;
			FLinearColor EntryControlColor = ControlColor;
			FLinearColor EntryTangentColor = TangentColor;

			if (Value.IsValid() && Value->TryGetString(RawName))
			{
				RawName = SanitizePresetNamePart(RawName, FString::Printf(TEXT("Point_%02d"), Index + 1));
			}
			else
			{
				const TSharedPtr<FJsonObject>* EntryObject = nullptr;
				if (Value.IsValid() && Value->TryGetObject(EntryObject))
				{
					ReadStringField(EntryObject->ToSharedRef(), TEXT("name"), TEXT("Name"), RawName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("control_name"), TEXT("ControlName"), ControlName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("tangent_control_name"), TEXT("TangentControlName"), TangentControlName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("space_name"), TEXT("SpaceName"), SpaceName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("parent"), TEXT("Parent"), EntryParent);
					ReadVectorField(EntryObject->ToSharedRef(), TEXT("location"), TEXT("Location"), Location);
					ReadVectorField(EntryObject->ToSharedRef(), TEXT("target_location"), TEXT("TargetLocation"), Location);
					ReadVectorField(EntryObject->ToSharedRef(), TEXT("tangent_location"), TEXT("TangentLocation"), TangentLocation);
					ReadRotatorField(EntryObject->ToSharedRef(), TEXT("rotation"), TEXT("Rotation"), Rotation);
					ReadRotatorField(EntryObject->ToSharedRef(), TEXT("tangent_rotation"), TEXT("TangentRotation"), TangentRotation);
					ReadBoolField(EntryObject->ToSharedRef(), TEXT("include_space"), TEXT("bIncludeSpace"), bEntryIncludeSpace);
					ReadBoolField(EntryObject->ToSharedRef(), TEXT("include_tangent_control"), TEXT("bIncludeTangentControl"), bEntryIncludeTangentControl);
					ReadBoolField(EntryObject->ToSharedRef(), TEXT("include_tangent"), TEXT("bIncludeTangent"), bEntryIncludeTangentControl);
					ReadLinearColorField(EntryObject->ToSharedRef(), TEXT("control_shape_color"), TEXT("ControlShapeColor"), EntryControlColor);
					ReadLinearColorField(EntryObject->ToSharedRef(), TEXT("tangent_shape_color"), TEXT("TangentShapeColor"), EntryTangentColor);
				}
				RawName = SanitizePresetNamePart(RawName, FString::Printf(TEXT("Point_%02d"), Index + 1));
			}

			const FString AuthoredControl = AddSpinePoint(
				RawName,
				ControlName,
				TangentControlName,
				SpaceName,
				EntryParent,
				Location,
				TangentLocation,
				Rotation,
				TangentRotation,
				bEntryIncludeSpace,
				bEntryIncludeTangentControl,
				EntryControlColor,
				EntryTangentColor);
			if (bChainParentControls)
			{
				ChainParent = AuthoredControl;
			}
		}
	}

	if (bIncludeWeightCurves)
	{
		CurveSpecs.Add(MakeShared<FJsonValueObject>(MakeCurveSpecJson(
			SanitizePresetNamePart(BendWeightCurveName, NamePrefix + TEXT("_BendWeight")),
			BendWeight)));
	}
	if (bIncludeStretchCurve)
	{
		CurveSpecs.Add(MakeShared<FJsonValueObject>(MakeCurveSpecJson(
			SanitizePresetNamePart(StretchCurveName, NamePrefix + TEXT("_Stretch")),
			Stretch)));
	}

	Hierarchy->SetArrayField(TEXT("null_specs"), NullSpecs);
	Hierarchy->SetArrayField(TEXT("control_specs"), ControlSpecs);
	Hierarchy->SetArrayField(TEXT("curve_specs"), CurveSpecs);

	TArray<TSharedPtr<FJsonValue>> Units;
	Units.Add(MakeShared<FJsonValueObject>(MakeUnitJson(TEXT("FRigUnit_BeginExecution"), TEXT("Preset_BeginExecution"), 0.0, 0.0)));
	RigVM->SetArrayField(TEXT("units"), Units);
}

void BuildInteractionTargetPreset(const TSharedRef<FJsonObject>& Request, const TSharedRef<FJsonObject>& Recipe)
{
	FString NamePrefix = TEXT("Interaction");
	FString Parent;
	ReadStringField(Request, TEXT("name_prefix"), TEXT("NamePrefix"), NamePrefix);
	ReadStringField(Request, TEXT("parent"), TEXT("Parent"), Parent);
	NamePrefix = SanitizePresetNamePart(NamePrefix, TEXT("Interaction"));

	bool bIncludeSpaces = true;
	bool bIncludeOffsets = true;
	bool bIncludeReachControls = true;
	bool bIncludeWeightCurves = true;
	bool bIncludeContactCurves = true;
	ReadBoolField(Request, TEXT("include_spaces"), TEXT("bIncludeSpaces"), bIncludeSpaces);
	ReadBoolField(Request, TEXT("include_offsets"), TEXT("bIncludeOffsets"), bIncludeOffsets);
	ReadBoolField(Request, TEXT("include_offset_controls"), TEXT("bIncludeOffsetControls"), bIncludeOffsets);
	ReadBoolField(Request, TEXT("include_reach_controls"), TEXT("bIncludeReachControls"), bIncludeReachControls);
	ReadBoolField(Request, TEXT("include_weight_curves"), TEXT("bIncludeWeightCurves"), bIncludeWeightCurves);
	ReadBoolField(Request, TEXT("include_contact_curves"), TEXT("bIncludeContactCurves"), bIncludeContactCurves);

	FLinearColor TargetColor(0.15f, 0.8f, 0.7f, 1.0f);
	FLinearColor OffsetColor(0.75f, 0.55f, 1.0f, 1.0f);
	FLinearColor ReachColor(1.0f, 0.7f, 0.25f, 1.0f);
	ReadLinearColorField(Request, TEXT("target_shape_color"), TEXT("TargetShapeColor"), TargetColor);
	ReadLinearColorField(Request, TEXT("offset_shape_color"), TEXT("OffsetShapeColor"), OffsetColor);
	ReadLinearColorField(Request, TEXT("reach_shape_color"), TEXT("ReachShapeColor"), ReachColor);

	const TSharedPtr<FJsonObject> Hierarchy = ReadObjectField(Recipe, TEXT("hierarchy"), TEXT("Hierarchy"));
	const TSharedPtr<FJsonObject> RigVM = ReadObjectField(Recipe, TEXT("rigvm"), TEXT("RigVM"));
	check(Hierarchy.IsValid());
	check(RigVM.IsValid());

	TArray<TSharedPtr<FJsonValue>> NullSpecs;
	TArray<TSharedPtr<FJsonValue>> ControlSpecs;
	TArray<TSharedPtr<FJsonValue>> CurveSpecs;

	struct FDefaultInteractionTarget
	{
		const TCHAR* Name;
		FVector TargetLocation;
		FVector OffsetLocation;
		float Reach;
		float Weight;
		float Contact;
	};

	static const FDefaultInteractionTarget Defaults[] =
	{
		{ TEXT("LeftHand"), FVector(55.0, -35.0, 100.0), FVector(10.0, -8.0, 0.0), 1.0f, 1.0f, 0.0f },
		{ TEXT("RightHand"), FVector(55.0, 35.0, 100.0), FVector(10.0, 8.0, 0.0), 1.0f, 1.0f, 0.0f },
		{ TEXT("Focus"), FVector(95.0, 0.0, 130.0), FVector(0.0, 0.0, 20.0), 0.75f, 0.75f, 0.0f },
	};

	auto AddInteractionTargetSet = [&](
		FString RawName,
		FString TargetControlName,
		FString OffsetControlName,
		FString ReachControlName,
		FString SpaceName,
		FString WeightCurveName,
		FString ContactCurveName,
		FString EntryParent,
		FVector TargetLocation,
		FVector OffsetLocation,
		FRotator TargetRotation,
		FRotator OffsetRotation,
		bool bEntryIncludeSpace,
		bool bEntryIncludeOffset,
		bool bEntryIncludeReachControl,
		bool bEntryIncludeWeightCurve,
		bool bEntryIncludeContactCurve,
		float Reach,
		float Weight,
		float Contact,
		FLinearColor EntryTargetColor,
		FLinearColor EntryOffsetColor,
		FLinearColor EntryReachColor)
	{
		RawName = SanitizePresetNamePart(RawName, TEXT("Target"));
		const FString BaseName = RawName.StartsWith(NamePrefix + TEXT("_"))
			? RawName
			: FString::Printf(TEXT("%s_%s"), *NamePrefix, *RawName);
		TargetControlName = SanitizePresetNamePart(TargetControlName, BaseName + TEXT("_Target"));
		OffsetControlName = SanitizePresetNamePart(OffsetControlName, BaseName + TEXT("_Offset"));
		ReachControlName = SanitizePresetNamePart(ReachControlName, BaseName + TEXT("_Reach"));
		SpaceName = SanitizePresetNamePart(SpaceName, BaseName + TEXT("_Space"));
		WeightCurveName = SanitizePresetNamePart(WeightCurveName, BaseName + TEXT("_Weight"));
		ContactCurveName = SanitizePresetNamePart(ContactCurveName, BaseName + TEXT("_Contact"));

		FString ControlParent = EntryParent;
		if (bEntryIncludeSpace)
		{
			NullSpecs.Add(MakeShared<FJsonValueObject>(MakeNullSpecJson(SpaceName, EntryParent, TargetLocation, TargetRotation)));
			ControlParent = SpaceName;
		}

		ControlSpecs.Add(MakeShared<FJsonValueObject>(
			MakeControlSpecJson(TargetControlName, ControlParent, TEXT("euler_transform"), 0.0, TargetLocation, TargetRotation, EntryTargetColor)));

		if (bEntryIncludeOffset)
		{
			ControlSpecs.Add(MakeShared<FJsonValueObject>(
				MakeControlSpecJson(OffsetControlName, TargetControlName, TEXT("euler_transform"), 0.0, OffsetLocation, OffsetRotation, EntryOffsetColor)));
		}

		if (bEntryIncludeReachControl)
		{
			ControlSpecs.Add(MakeShared<FJsonValueObject>(
				MakeControlSpecJson(ReachControlName, ControlParent, TEXT("float"), Reach, FVector::ZeroVector, FRotator::ZeroRotator, EntryReachColor)));
		}

		if (bEntryIncludeWeightCurve)
		{
			CurveSpecs.Add(MakeShared<FJsonValueObject>(MakeCurveSpecJson(WeightCurveName, Weight)));
		}

		if (bEntryIncludeContactCurve)
		{
			CurveSpecs.Add(MakeShared<FJsonValueObject>(MakeCurveSpecJson(ContactCurveName, Contact)));
		}
	};

	const TArray<TSharedPtr<FJsonValue>>* TargetValues = nullptr;
	if ((!Request->TryGetArrayField(TEXT("targets"), TargetValues) &&
		!Request->TryGetArrayField(TEXT("interactions"), TargetValues)) ||
		TargetValues->IsEmpty())
	{
		for (const FDefaultInteractionTarget& DefaultTarget : Defaults)
		{
			AddInteractionTargetSet(
				DefaultTarget.Name,
				TEXT(""),
				TEXT(""),
				TEXT(""),
				TEXT(""),
				TEXT(""),
				TEXT(""),
				Parent,
				DefaultTarget.TargetLocation,
				DefaultTarget.OffsetLocation,
				FRotator::ZeroRotator,
				FRotator::ZeroRotator,
				bIncludeSpaces,
				bIncludeOffsets,
				bIncludeReachControls,
				bIncludeWeightCurves,
				bIncludeContactCurves,
				DefaultTarget.Reach,
				DefaultTarget.Weight,
				DefaultTarget.Contact,
				TargetColor,
				OffsetColor,
				ReachColor);
		}
	}
	else
	{
		for (int32 Index = 0; Index < TargetValues->Num(); ++Index)
		{
			const TSharedPtr<FJsonValue>& Value = (*TargetValues)[Index];
			FString RawName;
			FString TargetControlName;
			FString OffsetControlName;
			FString ReachControlName;
			FString SpaceName;
			FString WeightCurveName;
			FString ContactCurveName;
			FString EntryParent = Parent;
			FVector TargetLocation(55.0, 0.0, 100.0);
			FVector OffsetLocation(10.0, 0.0, 0.0);
			FRotator TargetRotation = FRotator::ZeroRotator;
			FRotator OffsetRotation = FRotator::ZeroRotator;
			bool bEntryIncludeSpace = bIncludeSpaces;
			bool bEntryIncludeOffset = bIncludeOffsets;
			bool bEntryIncludeReachControl = bIncludeReachControls;
			bool bEntryIncludeWeightCurve = bIncludeWeightCurves;
			bool bEntryIncludeContactCurve = bIncludeContactCurves;
			float Reach = 1.0f;
			float Weight = 1.0f;
			float Contact = 0.0f;
			FLinearColor EntryTargetColor = TargetColor;
			FLinearColor EntryOffsetColor = OffsetColor;
			FLinearColor EntryReachColor = ReachColor;

			if (Value.IsValid() && Value->TryGetString(RawName))
			{
				RawName = SanitizePresetNamePart(RawName, FString::Printf(TEXT("Target_%02d"), Index + 1));
			}
			else
			{
				const TSharedPtr<FJsonObject>* EntryObject = nullptr;
				if (Value.IsValid() && Value->TryGetObject(EntryObject))
				{
					ReadStringField(EntryObject->ToSharedRef(), TEXT("name"), TEXT("Name"), RawName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("target_control_name"), TEXT("TargetControlName"), TargetControlName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("control_name"), TEXT("ControlName"), TargetControlName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("offset_control_name"), TEXT("OffsetControlName"), OffsetControlName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("reach_control_name"), TEXT("ReachControlName"), ReachControlName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("space_name"), TEXT("SpaceName"), SpaceName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("weight_curve_name"), TEXT("WeightCurveName"), WeightCurveName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("contact_curve_name"), TEXT("ContactCurveName"), ContactCurveName);
					ReadStringField(EntryObject->ToSharedRef(), TEXT("parent"), TEXT("Parent"), EntryParent);
					ReadVectorField(EntryObject->ToSharedRef(), TEXT("target_location"), TEXT("TargetLocation"), TargetLocation);
					ReadVectorField(EntryObject->ToSharedRef(), TEXT("location"), TEXT("Location"), TargetLocation);
					ReadVectorField(EntryObject->ToSharedRef(), TEXT("offset_location"), TEXT("OffsetLocation"), OffsetLocation);
					ReadRotatorField(EntryObject->ToSharedRef(), TEXT("target_rotation"), TEXT("TargetRotation"), TargetRotation);
					ReadRotatorField(EntryObject->ToSharedRef(), TEXT("rotation"), TEXT("Rotation"), TargetRotation);
					ReadRotatorField(EntryObject->ToSharedRef(), TEXT("offset_rotation"), TEXT("OffsetRotation"), OffsetRotation);
					ReadBoolField(EntryObject->ToSharedRef(), TEXT("include_space"), TEXT("bIncludeSpace"), bEntryIncludeSpace);
					ReadBoolField(EntryObject->ToSharedRef(), TEXT("include_offset"), TEXT("bIncludeOffset"), bEntryIncludeOffset);
					ReadBoolField(EntryObject->ToSharedRef(), TEXT("include_offset_control"), TEXT("bIncludeOffsetControl"), bEntryIncludeOffset);
					ReadBoolField(EntryObject->ToSharedRef(), TEXT("include_reach_control"), TEXT("bIncludeReachControl"), bEntryIncludeReachControl);
					ReadBoolField(EntryObject->ToSharedRef(), TEXT("include_weight_curve"), TEXT("bIncludeWeightCurve"), bEntryIncludeWeightCurve);
					ReadBoolField(EntryObject->ToSharedRef(), TEXT("include_contact_curve"), TEXT("bIncludeContactCurve"), bEntryIncludeContactCurve);
					ReadFloatField(EntryObject->ToSharedRef(), TEXT("reach"), TEXT("Reach"), Reach);
					ReadFloatField(EntryObject->ToSharedRef(), TEXT("default_reach"), TEXT("DefaultReach"), Reach);
					ReadFloatField(EntryObject->ToSharedRef(), TEXT("weight"), TEXT("Weight"), Weight);
					ReadFloatField(EntryObject->ToSharedRef(), TEXT("default_weight"), TEXT("DefaultWeight"), Weight);
					ReadFloatField(EntryObject->ToSharedRef(), TEXT("contact"), TEXT("Contact"), Contact);
					ReadFloatField(EntryObject->ToSharedRef(), TEXT("default_contact"), TEXT("DefaultContact"), Contact);
					ReadLinearColorField(EntryObject->ToSharedRef(), TEXT("target_shape_color"), TEXT("TargetShapeColor"), EntryTargetColor);
					ReadLinearColorField(EntryObject->ToSharedRef(), TEXT("offset_shape_color"), TEXT("OffsetShapeColor"), EntryOffsetColor);
					ReadLinearColorField(EntryObject->ToSharedRef(), TEXT("reach_shape_color"), TEXT("ReachShapeColor"), EntryReachColor);
				}
				RawName = SanitizePresetNamePart(RawName, FString::Printf(TEXT("Target_%02d"), Index + 1));
			}

			AddInteractionTargetSet(
				RawName,
				TargetControlName,
				OffsetControlName,
				ReachControlName,
				SpaceName,
				WeightCurveName,
				ContactCurveName,
				EntryParent,
				TargetLocation,
				OffsetLocation,
				TargetRotation,
				OffsetRotation,
				bEntryIncludeSpace,
				bEntryIncludeOffset,
				bEntryIncludeReachControl,
				bEntryIncludeWeightCurve,
				bEntryIncludeContactCurve,
				Reach,
				Weight,
				Contact,
				EntryTargetColor,
				EntryOffsetColor,
				EntryReachColor);
		}
	}

	Hierarchy->SetArrayField(TEXT("null_specs"), NullSpecs);
	Hierarchy->SetArrayField(TEXT("control_specs"), ControlSpecs);
	Hierarchy->SetArrayField(TEXT("curve_specs"), CurveSpecs);

	TArray<TSharedPtr<FJsonValue>> Units;
	Units.Add(MakeShared<FJsonValueObject>(MakeUnitJson(TEXT("FRigUnit_BeginExecution"), TEXT("Preset_BeginExecution"), 0.0, 0.0)));
	RigVM->SetArrayField(TEXT("units"), Units);
}

} // namespace

FString MakePresetRecipeJson(const FString& PresetJson)
{
	TArray<TSharedPtr<FJsonValue>> Issues;
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PresetJson);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		AddPresetIssue(
			Issues,
			TEXT("invalid_json"),
			TEXT("Preset JSON could not be parsed as an object."),
			TEXT("preset_json"));
		return MakePresetResponseJson(
			false,
			TEXT(""),
			TEXT(""),
			nullptr,
			Issues,
			TEXT("Preset JSON parse failed. No assets were created, modified, compiled, or saved."));
	}

	const TSharedRef<FJsonObject> Root = RootObject.ToSharedRef();

	int32 SchemaVersion = SupportedSchemaVersion;
	ReadIntField(Root, TEXT("schema_version"), TEXT("SchemaVersion"), SchemaVersion);
	if (SchemaVersion != SupportedSchemaVersion)
	{
		AddPresetIssue(
			Issues,
			TEXT("unsupported_schema_version"),
			FString::Printf(TEXT("schema_version %d is not supported."), SchemaVersion),
			TEXT("schema_version"));
	}

	FString PresetName;
	ReadStringField(Root, TEXT("preset"), TEXT("Preset"), PresetName);
	ReadStringField(Root, TEXT("preset_name"), TEXT("PresetName"), PresetName);
	ReadStringField(Root, TEXT("type"), TEXT("Type"), PresetName);
	PresetName.TrimStartAndEndInline();
	PresetName = PresetName.ToLower();

	FString TargetControlRigPath;
	ReadStringField(Root, TEXT("target_control_rig_path"), TEXT("TargetControlRigPath"), TargetControlRigPath);
	if (TargetControlRigPath.TrimStartAndEnd().IsEmpty())
	{
		AddPresetIssue(
			Issues,
			TEXT("missing_target_control_rig_path"),
			TEXT("target_control_rig_path is required."),
			TEXT("target_control_rig_path"));
	}

	if (ContainsForbiddenMcpReference(PresetJson))
	{
		AddPresetIssue(
			Issues,
			TEXT("forbidden_mcp_reference"),
			TEXT("Preset request contains an UnrealMCP-only reference."),
			TEXT("preset_json"));
	}

	if (PresetName.IsEmpty())
	{
		AddPresetIssue(
			Issues,
			TEXT("missing_preset"),
			TEXT("preset is required. Supported values: simple_fk_control_chain, curve_gate_controls, hand_foot_target_controls, look_at_aim_target_controls, two_bone_ik_limb_controls, spine_spline_control_chain, interaction_target_controls."),
			TEXT("preset"));
	}
	else if (
		PresetName != TEXT("simple_fk_control_chain") &&
		PresetName != TEXT("curve_gate_controls") &&
		PresetName != TEXT("hand_foot_target_controls") &&
		PresetName != TEXT("look_at_aim_target_controls") &&
		PresetName != TEXT("two_bone_ik_limb_controls") &&
		PresetName != TEXT("spine_spline_control_chain") &&
		PresetName != TEXT("interaction_target_controls"))
	{
		AddPresetIssue(
			Issues,
			TEXT("unsupported_preset"),
			FString::Printf(TEXT("Unsupported preset: %s."), *PresetName),
			TEXT("preset"));
	}

	if (!Issues.IsEmpty())
	{
		return MakePresetResponseJson(
			false,
			PresetName,
			TargetControlRigPath,
			nullptr,
			Issues,
			TEXT("Preset recipe generation failed. No assets were created, modified, compiled, or saved."));
	}

	TSharedRef<FJsonObject> Recipe = MakeBasePresetRecipe(Root);
	if (PresetName == TEXT("simple_fk_control_chain"))
	{
		BuildSimpleFkPreset(Root, Recipe);
	}
	else if (PresetName == TEXT("curve_gate_controls"))
	{
		BuildCurveGatePreset(Root, Recipe);
	}
	else if (PresetName == TEXT("hand_foot_target_controls"))
	{
		BuildHandFootTargetPreset(Root, Recipe);
	}
	else if (PresetName == TEXT("look_at_aim_target_controls"))
	{
		BuildLookAtAimTargetPreset(Root, Recipe);
	}
	else if (PresetName == TEXT("two_bone_ik_limb_controls"))
	{
		BuildTwoBoneIkLimbPreset(Root, Recipe);
	}
	else if (PresetName == TEXT("spine_spline_control_chain"))
	{
		BuildSpineSplineControlChainPreset(Root, Recipe);
	}
	else if (PresetName == TEXT("interaction_target_controls"))
	{
		BuildInteractionTargetPreset(Root, Recipe);
	}

	return MakePresetResponseJson(
		true,
		PresetName,
		TargetControlRigPath,
		Recipe,
		Issues,
		TEXT("Preset recipe generated. No assets were created, modified, compiled, or saved."));
}

} // namespace Cubeless::Operations::ControlRigPresetRecipes
