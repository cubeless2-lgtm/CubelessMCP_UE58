#include "Compatibility/UE58/CubelessPhysicsAssetInspectionApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "BodySetupEnums.h"
#include "Interfaces/Interface_PreviewMeshProvider.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "EditorAssetLibrary.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
namespace Cubeless::UE58::PhysicsAssetInspection
{
namespace
{
using Variable::FResult;
constexpr int32 InspectionArrayBudget=4096,InspectionObjectBudget=16384,InspectionWorkBudget=65536,InspectionJsonBudget=1024*1024;
FResult* Current=nullptr;TArray<TStrongObjectPtr<UObject>> Held;TSet<const UObject*> HeldObjects;
bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
bool InputTextSafe(const FString& S,int32 Limit) { if(S.Len()>Limit) { return false; }for(int32 I=0;I<S.Len();++I) { if(S[I]==0) { return false; } }return true; }
void Count(const TCHAR* Key,int32 N=1)
{ if(Current) { double V=0;auto C=Current->Data->GetObjectField(TEXT("native_call_counts"));C->TryGetNumberField(Key,V);C->SetNumberField(Key,V+N); } }
void Unsafe(const FString& Error)
{ if(Current) { Current->Data->SetBoolField(TEXT("observation_incomplete"),true);Current->ErrorCode=TEXT("physics_asset_observation_incomplete");Current->Error=Error; } }
bool Incomplete() { bool V=false;return Current&&Current->Data->TryGetBoolField(TEXT("observation_incomplete"),V)&&V; }
bool Hold(const UObject* O)
{
    if(!O) { return true; }if(!Loaded(O)||!Loaded(O->GetClass())||!InputTextSafe(O->GetPathName(),65536)) { Unsafe(TEXT("Referenced current UObject/class unavailable or outside bounded path scope"));return false; }
    if(!HeldObjects.Contains(O)) { if(HeldObjects.Num()>=InspectionObjectBudget) { Unsafe(TEXT("Strong reference inventory exceeds16384"));return false; }HeldObjects.Add(O);Held.Emplace(const_cast<UObject*>(O)); }return true;
}
bool Small(int32 N) { if(N>InspectionArrayBudget) { Unsafe(TEXT("Selected source array exceeds4096"));return false; }return true; }
TSharedPtr<FJsonObject> LegacyError(const FString& Error)
{ auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),false);J->SetStringField(TEXT("error"),Error);return J; }
bool JsonSafe(const TSharedPtr<FJsonValue>& V,int32 Depth,int32& Items,int64& Chars)
{
    if(!V.IsValid()||Depth>32||++Items>100000) { Unsafe(TEXT("Native JSON tree exceeds bounded source scope"));return false; }
    if(V->Type==EJson::Number&&!FMath::IsFinite(V->AsNumber())) { Unsafe(TEXT("Selected authored numeric value is nonfinite"));return false; }
    if(V->Type==EJson::String) { const auto S=V->AsString();Chars+=S.Len();if(!InputTextSafe(S,65536)||Chars>262144) { Unsafe(TEXT("Selected metadata text exceeds finite bound"));return false; } }
    else if(V->Type==EJson::Array) { for(const auto& A:V->AsArray()) { if(!JsonSafe(A,Depth+1,Items,Chars)) { return false; } } }
    else if(V->Type==EJson::Object) { for(const auto& Pair:V->AsObject()->Values) { if(!JsonSafe(Pair.Value,Depth+1,Items,Chars)) { return false; } } }
    return true;
}
FString Serialize(const TSharedPtr<FJsonObject>& J)
{
    int32 Items=0;int64 Chars=0;if(!J.IsValid()||!JsonSafe(MakeShared<FJsonValueObject>(J),0,Items,Chars)) { return FString(); }
    FString S;FJsonSerializer::Serialize(J.ToSharedRef(),TJsonWriterFactory<>::Create(&S));if(FTCHARToUTF8(*S).Length()>InspectionJsonBudget) { Unsafe(TEXT("Selected native JSON exceeds1MiB"));return FString(); }return S;
}

FString NormalizeAssetObjectPathForLoad(const FString& AssetPath)
{
    FString NormalizedPath = FPackageName::ExportTextPathToObjectPath(AssetPath).TrimStartAndEnd();
    NormalizedPath.TrimQuotesInline();

    if ((NormalizedPath.StartsWith(TEXT("/Game/")) || NormalizedPath.StartsWith(TEXT("/Engine/"))) && !NormalizedPath.Contains(TEXT(".")))
    {
        const FString AssetName = FPackageName::GetShortName(NormalizedPath);
        NormalizedPath = FString::Printf(TEXT("%s.%s"), *NormalizedPath, *AssetName);
    }

    return NormalizedPath;
}

TArray<TSharedPtr<FJsonValue>> NameArrayToJson(const TArray<FName>& Names)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    for (const FName& Name : Names)
    {
        Values.Add(MakeShared<FJsonValueString>(Name.ToString()));
    }
    return Values;
}

TArray<TSharedPtr<FJsonValue>> VectorToJsonArray(const FVector& Vector)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    Values.Add(MakeShared<FJsonValueNumber>(Vector.X));
    Values.Add(MakeShared<FJsonValueNumber>(Vector.Y));
    Values.Add(MakeShared<FJsonValueNumber>(Vector.Z));
    return Values;
}

void AddJsonString(TArray<TSharedPtr<FJsonValue>>& Values, const FString& Text)
{
    Values.Add(MakeShared<FJsonValueString>(Text));
}

FString PhysicsAssetSolverTypeToString(EPhysicsAssetSolverType SolverType)
{
    switch (SolverType)
    {
    case EPhysicsAssetSolverType::RBAN:
        return TEXT("RBAN");
    case EPhysicsAssetSolverType::World:
        return TEXT("World");
    default:
        return TEXT("Unknown");
    }
}

FString LinearConstraintMotionToString(ELinearConstraintMotion Motion)
{
    switch (Motion)
    {
    case LCM_Free:
        return TEXT("Free");
    case LCM_Limited:
        return TEXT("Limited");
    case LCM_Locked:
        return TEXT("Locked");
    default:
        return TEXT("Unknown");
    }
}

FString AngularConstraintMotionToString(EAngularConstraintMotion Motion)
{
    switch (Motion)
    {
    case ACM_Free:
        return TEXT("Free");
    case ACM_Limited:
        return TEXT("Limited");
    case ACM_Locked:
        return TEXT("Locked");
    default:
        return TEXT("Unknown");
    }
}

FString ConstraintPlasticityTypeToString(EConstraintPlasticityType PlasticityType)
{
    switch (PlasticityType)
    {
    case CCPT_Free:
        return TEXT("Free");
    case CCPT_Shrink:
        return TEXT("Shrink");
    case CCPT_Grow:
        return TEXT("Grow");
    default:
        return TEXT("Unknown");
    }
}

FString CollisionEnabledToString(ECollisionEnabled::Type CollisionEnabled)
{
    switch (CollisionEnabled)
    {
    case ECollisionEnabled::NoCollision:
        return TEXT("NoCollision");
    case ECollisionEnabled::QueryOnly:
        return TEXT("QueryOnly");
    case ECollisionEnabled::PhysicsOnly:
        return TEXT("PhysicsOnly");
    case ECollisionEnabled::QueryAndPhysics:
        return TEXT("QueryAndPhysics");
    default:
        return TEXT("Unknown");
    }
}

void AddRotatorFields(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, const FRotator& Rotator)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    Values.Add(MakeShared<FJsonValueNumber>(Rotator.Pitch));
    Values.Add(MakeShared<FJsonValueNumber>(Rotator.Yaw));
    Values.Add(MakeShared<FJsonValueNumber>(Rotator.Roll));
    Object->SetArrayField(FieldName, Values);
}

TSharedPtr<FJsonObject> PhysicsSolverSettingsToJson(const UPhysicsAsset* PhysicsAsset)
{
    Count(TEXT("PhysicsSolverSettingsToJson_calls"));
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    if (!PhysicsAsset)
    {
        Object->SetBoolField(TEXT("valid"), false);
        return Object;
    }

    const FPhysicsAssetSolverSettings& Settings = PhysicsAsset->SolverSettings;
    Object->SetBoolField(TEXT("valid"), true);
    Object->SetStringField(TEXT("solver_type"), PhysicsAssetSolverTypeToString(PhysicsAsset->SolverType));
    Object->SetNumberField(TEXT("solver_type_value"), static_cast<int32>(PhysicsAsset->SolverType));
    Object->SetNumberField(TEXT("position_iterations"), Settings.PositionIterations);
    Object->SetNumberField(TEXT("velocity_iterations"), Settings.VelocityIterations);
    Object->SetNumberField(TEXT("projection_iterations"), Settings.ProjectionIterations);
    Object->SetNumberField(TEXT("cull_distance"), Settings.CullDistance);
    Object->SetNumberField(TEXT("max_depenetration_velocity"), Settings.MaxDepenetrationVelocity);
    Object->SetNumberField(TEXT("fixed_time_step"), Settings.FixedTimeStep);
    Object->SetBoolField(TEXT("use_linear_joint_solver"), Settings.bUseLinearJointSolver);
    Object->SetBoolField(TEXT("use_manifolds"), Settings.bUseManifolds);
    Object->SetBoolField(TEXT("not_for_dedicated_server"), PhysicsAsset->bNotForDedicatedServer != 0);
    return Object;
}

TSharedPtr<FJsonObject> PhysicalAnimationDataToJson(const FPhysicalAnimationData& Data)
{
    Count(TEXT("PhysicalAnimationDataToJson_calls"));
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("body_name"), Data.BodyName.ToString());
    Object->SetBoolField(TEXT("is_local_simulation"), Data.bIsLocalSimulation != 0);
    Object->SetNumberField(TEXT("orientation_strength"), Data.OrientationStrength);
    Object->SetNumberField(TEXT("angular_velocity_strength"), Data.AngularVelocityStrength);
    Object->SetNumberField(TEXT("position_strength"), Data.PositionStrength);
    Object->SetNumberField(TEXT("velocity_strength"), Data.VelocityStrength);
    Object->SetNumberField(TEXT("max_linear_force"), Data.MaxLinearForce);
    Object->SetNumberField(TEXT("max_angular_force"), Data.MaxAngularForce);
    return Object;
}

TArray<TSharedPtr<FJsonValue>> PhysicalAnimationProfilesToJson(
    const USkeletalBodySetup* BodySetup,
    int32 MaxProfiles,
    bool& bOutTruncated)
{
    Count(TEXT("PhysicalAnimationProfilesToJson_calls"));
    TArray<TSharedPtr<FJsonValue>> Values;
    bOutTruncated = false;
    if (!BodySetup)
    {
        return Values;
    }

    Count(TEXT("body_profile_getter_calls"));
    const TArray<FPhysicalAnimationProfile>& Profiles = BodySetup->GetPhysicalAnimationProfiles();
    for (int32 ProfileIndex = 0; ProfileIndex < Profiles.Num(); ++ProfileIndex)
    {
        if (Values.Num() >= MaxProfiles)
        {
            bOutTruncated = true;
            continue;
        }

        const FPhysicalAnimationProfile& Profile = Profiles[ProfileIndex];
        TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetNumberField(TEXT("index"), ProfileIndex);
        Object->SetStringField(TEXT("profile_name"), Profile.ProfileName.ToString());
        Object->SetObjectField(TEXT("data"), PhysicalAnimationDataToJson(Profile.PhysicalAnimationData));
        Values.Add(MakeShared<FJsonValueObject>(Object));
    }
    return Values;
}

TSharedPtr<FJsonObject> CollisionShapeCountsToJson(const FKAggregateGeom& AggGeom)
{
    Count(TEXT("CollisionShapeCountsToJson_calls"));
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("total"), AggGeom.GetElementCount());
    Object->SetNumberField(TEXT("spheres"), AggGeom.SphereElems.Num());
    Object->SetNumberField(TEXT("boxes"), AggGeom.BoxElems.Num());
    Object->SetNumberField(TEXT("capsules"), AggGeom.SphylElems.Num());
    Object->SetNumberField(TEXT("convex"), AggGeom.ConvexElems.Num());
    Object->SetNumberField(TEXT("tapered_capsules"), AggGeom.TaperedCapsuleElems.Num());
    Object->SetNumberField(TEXT("level_sets"), AggGeom.LevelSetElems.Num());
    Object->SetNumberField(TEXT("skinned_level_sets"), AggGeom.SkinnedLevelSetElems.Num());
    Object->SetNumberField(TEXT("ml_level_sets"), AggGeom.MLLevelSetElems.Num());
    Object->SetNumberField(TEXT("skinned_triangle_meshes"), AggGeom.SkinnedTriangleMeshElems.Num());
    return Object;
}

void AddSphereShapeSummary(TArray<TSharedPtr<FJsonValue>>& Values, const FKSphereElem& Shape, int32 Index)
{
    Count(TEXT("AddSphereShapeSummary_calls"));
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("shape_kind"), TEXT("sphere"));
    Object->SetNumberField(TEXT("index"), Index);
    Object->SetArrayField(TEXT("center"), VectorToJsonArray(Shape.Center));
    Object->SetNumberField(TEXT("radius"), Shape.Radius);
    Values.Add(MakeShared<FJsonValueObject>(Object));
}

void AddBoxShapeSummary(TArray<TSharedPtr<FJsonValue>>& Values, const FKBoxElem& Shape, int32 Index)
{
    Count(TEXT("AddBoxShapeSummary_calls"));
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("shape_kind"), TEXT("box"));
    Object->SetNumberField(TEXT("index"), Index);
    Object->SetArrayField(TEXT("center"), VectorToJsonArray(Shape.Center));
    AddRotatorFields(Object, TEXT("rotation"), Shape.Rotation);
    Object->SetNumberField(TEXT("x"), Shape.X);
    Object->SetNumberField(TEXT("y"), Shape.Y);
    Object->SetNumberField(TEXT("z"), Shape.Z);
    Values.Add(MakeShared<FJsonValueObject>(Object));
}

void AddSphylShapeSummary(TArray<TSharedPtr<FJsonValue>>& Values, const FKSphylElem& Shape, int32 Index)
{
    Count(TEXT("AddSphylShapeSummary_calls"));
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("shape_kind"), TEXT("capsule"));
    Object->SetNumberField(TEXT("index"), Index);
    Object->SetArrayField(TEXT("center"), VectorToJsonArray(Shape.Center));
    AddRotatorFields(Object, TEXT("rotation"), Shape.Rotation);
    Object->SetNumberField(TEXT("radius"), Shape.Radius);
    Object->SetNumberField(TEXT("length"), Shape.Length);
    Values.Add(MakeShared<FJsonValueObject>(Object));
}

void AddTaperedCapsuleShapeSummary(TArray<TSharedPtr<FJsonValue>>& Values, const FKTaperedCapsuleElem& Shape, int32 Index)
{
    Count(TEXT("AddTaperedCapsuleShapeSummary_calls"));
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("shape_kind"), TEXT("tapered_capsule"));
    Object->SetNumberField(TEXT("index"), Index);
    Object->SetArrayField(TEXT("center"), VectorToJsonArray(Shape.Center));
    AddRotatorFields(Object, TEXT("rotation"), Shape.Rotation);
    Object->SetNumberField(TEXT("radius0"), Shape.Radius0);
    Object->SetNumberField(TEXT("radius1"), Shape.Radius1);
    Object->SetNumberField(TEXT("length"), Shape.Length);
    Object->SetBoolField(TEXT("one_sided_collision"), Shape.bOneSidedCollision);
    Values.Add(MakeShared<FJsonValueObject>(Object));
}

void AddConvexShapeSummary(TArray<TSharedPtr<FJsonValue>>& Values, const FKConvexElem& Shape, int32 Index)
{
    Count(TEXT("AddConvexShapeSummary_calls"));
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("shape_kind"), TEXT("convex"));
    Object->SetNumberField(TEXT("index"), Index);
    Object->SetNumberField(TEXT("vertex_count"), Shape.VertexData.Num());
    Object->SetNumberField(TEXT("index_count"), Shape.IndexData.Num());
    Object->SetArrayField(TEXT("bounds_min"), VectorToJsonArray(Shape.ElemBox.Min));
    Object->SetArrayField(TEXT("bounds_max"), VectorToJsonArray(Shape.ElemBox.Max));
    Values.Add(MakeShared<FJsonValueObject>(Object));
}

TArray<TSharedPtr<FJsonValue>> CollisionShapesToJson(
    const FKAggregateGeom& AggGeom,
    int32 MaxShapes,
    bool& bOutTruncated)
{
    Count(TEXT("CollisionShapesToJson_calls"));
    TArray<TSharedPtr<FJsonValue>> Values;
    bOutTruncated = false;
    auto CanAddShape = [&Values, MaxShapes, &bOutTruncated]() -> bool
    {
        if (Values.Num() >= MaxShapes)
        {
            bOutTruncated = true;
            return false;
        }
        return true;
    };

    for (int32 Index = 0; Index < AggGeom.SphereElems.Num(); ++Index)
    {
        if (CanAddShape()) { AddSphereShapeSummary(Values, AggGeom.SphereElems[Index], Index); }
    }
    for (int32 Index = 0; Index < AggGeom.BoxElems.Num(); ++Index)
    {
        if (CanAddShape()) { AddBoxShapeSummary(Values, AggGeom.BoxElems[Index], Index); }
    }
    for (int32 Index = 0; Index < AggGeom.SphylElems.Num(); ++Index)
    {
        if (CanAddShape()) { AddSphylShapeSummary(Values, AggGeom.SphylElems[Index], Index); }
    }
    for (int32 Index = 0; Index < AggGeom.TaperedCapsuleElems.Num(); ++Index)
    {
        if (CanAddShape()) { AddTaperedCapsuleShapeSummary(Values, AggGeom.TaperedCapsuleElems[Index], Index); }
    }
    for (int32 Index = 0; Index < AggGeom.ConvexElems.Num(); ++Index)
    {
        if (CanAddShape()) { AddConvexShapeSummary(Values, AggGeom.ConvexElems[Index], Index); }
    }
    return Values;
}

TSharedPtr<FJsonObject> ConstraintProfilePropertiesToJson(const FConstraintProfileProperties& Profile)
{
    Count(TEXT("ConstraintProfilePropertiesToJson_calls"));
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();

    TSharedPtr<FJsonObject> LinearObject = MakeShared<FJsonObject>();
    LinearObject->SetStringField(TEXT("x_motion"), LinearConstraintMotionToString(Profile.LinearLimit.XMotion));
    LinearObject->SetStringField(TEXT("y_motion"), LinearConstraintMotionToString(Profile.LinearLimit.YMotion));
    LinearObject->SetStringField(TEXT("z_motion"), LinearConstraintMotionToString(Profile.LinearLimit.ZMotion));
    LinearObject->SetNumberField(TEXT("limit"), Profile.LinearLimit.Limit);
    LinearObject->SetBoolField(TEXT("soft"), Profile.LinearLimit.bSoftConstraint != 0);
    LinearObject->SetNumberField(TEXT("stiffness"), Profile.LinearLimit.Stiffness);
    LinearObject->SetNumberField(TEXT("damping"), Profile.LinearLimit.Damping);
    LinearObject->SetNumberField(TEXT("restitution"), Profile.LinearLimit.Restitution);
    LinearObject->SetNumberField(TEXT("contact_distance"), Profile.LinearLimit.ContactDistance);
    Object->SetObjectField(TEXT("linear"), LinearObject);

    TSharedPtr<FJsonObject> AngularObject = MakeShared<FJsonObject>();
    AngularObject->SetStringField(TEXT("swing1_motion"), AngularConstraintMotionToString(Profile.ConeLimit.Swing1Motion));
    AngularObject->SetNumberField(TEXT("swing1_limit"), Profile.ConeLimit.Swing1LimitDegrees);
    AngularObject->SetStringField(TEXT("swing2_motion"), AngularConstraintMotionToString(Profile.ConeLimit.Swing2Motion));
    AngularObject->SetNumberField(TEXT("swing2_limit"), Profile.ConeLimit.Swing2LimitDegrees);
    AngularObject->SetStringField(TEXT("twist_motion"), AngularConstraintMotionToString(Profile.TwistLimit.TwistMotion));
    AngularObject->SetNumberField(TEXT("twist_limit"), Profile.TwistLimit.TwistLimitDegrees);
    AngularObject->SetBoolField(TEXT("swing_soft"), Profile.ConeLimit.bSoftConstraint != 0);
    AngularObject->SetBoolField(TEXT("twist_soft"), Profile.TwistLimit.bSoftConstraint != 0);
    AngularObject->SetNumberField(TEXT("swing_stiffness"), Profile.ConeLimit.Stiffness);
    AngularObject->SetNumberField(TEXT("swing_damping"), Profile.ConeLimit.Damping);
    AngularObject->SetNumberField(TEXT("twist_stiffness"), Profile.TwistLimit.Stiffness);
    AngularObject->SetNumberField(TEXT("twist_damping"), Profile.TwistLimit.Damping);
    Object->SetObjectField(TEXT("angular"), AngularObject);

    TSharedPtr<FJsonObject> ProjectionObject = MakeShared<FJsonObject>();
    ProjectionObject->SetBoolField(TEXT("enabled"), Profile.bEnableProjection != 0);
    ProjectionObject->SetNumberField(TEXT("linear_tolerance"), Profile.ProjectionLinearTolerance);
    ProjectionObject->SetNumberField(TEXT("angular_tolerance"), Profile.ProjectionAngularTolerance);
    ProjectionObject->SetNumberField(TEXT("linear_alpha"), Profile.ProjectionLinearAlpha);
    ProjectionObject->SetNumberField(TEXT("angular_alpha"), Profile.ProjectionAngularAlpha);
    ProjectionObject->SetNumberField(TEXT("shock_propagation_alpha"), Profile.ShockPropagationAlpha);
    Object->SetObjectField(TEXT("projection"), ProjectionObject);

    TSharedPtr<FJsonObject> BreakObject = MakeShared<FJsonObject>();
    BreakObject->SetBoolField(TEXT("linear_breakable"), Profile.bLinearBreakable != 0);
    BreakObject->SetNumberField(TEXT("linear_break_threshold"), Profile.LinearBreakThreshold);
    BreakObject->SetBoolField(TEXT("angular_breakable"), Profile.bAngularBreakable != 0);
    BreakObject->SetNumberField(TEXT("angular_break_threshold"), Profile.AngularBreakThreshold);
    Object->SetObjectField(TEXT("breakability"), BreakObject);

    TSharedPtr<FJsonObject> PlasticityObject = MakeShared<FJsonObject>();
    PlasticityObject->SetBoolField(TEXT("linear_plasticity"), Profile.bLinearPlasticity != 0);
    PlasticityObject->SetNumberField(TEXT("linear_plasticity_threshold"), Profile.LinearPlasticityThreshold);
    PlasticityObject->SetStringField(TEXT("linear_plasticity_type"), ConstraintPlasticityTypeToString(Profile.LinearPlasticityType));
    PlasticityObject->SetBoolField(TEXT("angular_plasticity"), Profile.bAngularPlasticity != 0);
    PlasticityObject->SetNumberField(TEXT("angular_plasticity_threshold"), Profile.AngularPlasticityThreshold);
    Object->SetObjectField(TEXT("plasticity"), PlasticityObject);

    TSharedPtr<FJsonObject> FlagsObject = MakeShared<FJsonObject>();
    FlagsObject->SetBoolField(TEXT("disable_collision"), Profile.bDisableCollision != 0);
    FlagsObject->SetBoolField(TEXT("parent_dominates"), Profile.bParentDominates != 0);
    FlagsObject->SetBoolField(TEXT("mass_conditioning"), Profile.bEnableMassConditioning != 0);
    Object->SetObjectField(TEXT("flags"), FlagsObject);

    TSharedPtr<FJsonObject> DriveObject = MakeShared<FJsonObject>();
    DriveObject->SetBoolField(TEXT("linear_position_drive"), Profile.LinearDrive.IsPositionDriveEnabled());
    DriveObject->SetBoolField(TEXT("linear_velocity_drive"), Profile.LinearDrive.IsVelocityDriveEnabled());
    DriveObject->SetBoolField(TEXT("angular_orientation_drive"), Profile.AngularDrive.IsOrientationDriveEnabled());
    DriveObject->SetBoolField(TEXT("angular_velocity_drive"), Profile.AngularDrive.IsVelocityDriveEnabled());
    DriveObject->SetArrayField(TEXT("linear_position_target"), VectorToJsonArray(Profile.LinearDrive.PositionTarget));
    DriveObject->SetArrayField(TEXT("linear_velocity_target"), VectorToJsonArray(Profile.LinearDrive.VelocityTarget));
    AddRotatorFields(DriveObject, TEXT("angular_orientation_target"), Profile.AngularDrive.OrientationTarget);
    DriveObject->SetArrayField(TEXT("angular_velocity_target"), VectorToJsonArray(Profile.AngularDrive.AngularVelocityTarget));
    Object->SetObjectField(TEXT("drive"), DriveObject);

    return Object;
}

TArray<TSharedPtr<FJsonValue>> ConstraintProfileHandlesToJson(
    const UPhysicsConstraintTemplate* ConstraintTemplate,
    int32 MaxProfiles,
    bool& bOutTruncated)
{
    Count(TEXT("ConstraintProfileHandlesToJson_calls"));
    TArray<TSharedPtr<FJsonValue>> Values;
    bOutTruncated = false;
    if (!ConstraintTemplate)
    {
        return Values;
    }

    for (int32 ProfileIndex = 0; ProfileIndex < ConstraintTemplate->ProfileHandles.Num(); ++ProfileIndex)
    {
        if (Values.Num() >= MaxProfiles)
        {
            bOutTruncated = true;
            continue;
        }

        const FPhysicsConstraintProfileHandle& Handle = ConstraintTemplate->ProfileHandles[ProfileIndex];
        TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetNumberField(TEXT("index"), ProfileIndex);
        Object->SetStringField(TEXT("profile_name"), Handle.ProfileName.ToString());
        Object->SetObjectField(TEXT("properties"), ConstraintProfilePropertiesToJson(Handle.ProfileProperties));
        Values.Add(MakeShared<FJsonValueObject>(Object));
    }
    return Values;
}

TSharedPtr<FJsonObject> BodySetupToJson(
    const UPhysicsAsset* PhysicsAsset,
    const USkeletalBodySetup* BodySetup,
    int32 BodyIndex,
    bool bIncludeProfiles,
    bool bIncludeCollisionSummary,
    int32 MaxProfilesPerItem,
    int32 MaxShapesPerBody,
    bool& bOutProfilesTruncated,
    bool& bOutShapesTruncated)
{
    Count(TEXT("BodySetupToJson_calls"));
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("index"), BodyIndex);
    if (!BodySetup)
    {
        Object->SetBoolField(TEXT("valid"), false);
        return Object;
    }

    if(!Hold(BodySetup)||!Hold(BodySetup->PhysMaterial.Get())) { return Object; }
    Count(TEXT("body_collision_trace_getter_calls"));
    Count(TEXT("body_collision_enabled_getter_calls"));
    Count(TEXT("body_collision_profile_getter_calls"));
    const FBodyInstance& BodyInstance = BodySetup->DefaultInstance;
    Object->SetBoolField(TEXT("valid"), true);
    Object->SetStringField(TEXT("name"), BodySetup->GetName());
    Object->SetStringField(TEXT("path"), BodySetup->GetPathName());
    Object->SetStringField(TEXT("bone_name"), BodySetup->BoneName.ToString());
    Object->SetBoolField(TEXT("consider_for_bounds"), BodySetup->bConsiderForBounds != 0);
    Object->SetBoolField(TEXT("is_bounds_body"), PhysicsAsset && PhysicsAsset->BoundsBodies.Contains(BodyIndex));
    Object->SetStringField(TEXT("physics_type"), FString(LexToString(BodySetup->PhysicsType.GetValue())));
    Object->SetStringField(TEXT("collision_trace_flag"), FString(LexToString(BodySetup->GetCollisionTraceFlag().GetValue())));
    Object->SetStringField(TEXT("collision_response"), FString(LexToString(BodySetup->CollisionReponse.GetValue())));
    Object->SetBoolField(TEXT("skip_scale_from_animation"), BodySetup->bSkipScaleFromAnimation);
    Object->SetStringField(TEXT("simple_physical_material"), BodySetup->PhysMaterial ? BodySetup->PhysMaterial->GetPathName() : FString());

    TSharedPtr<FJsonObject> BodyInstanceObject = MakeShared<FJsonObject>();
    BodyInstanceObject->SetStringField(TEXT("collision_enabled"), CollisionEnabledToString(BodyInstance.GetCollisionEnabled(false)));
    BodyInstanceObject->SetNumberField(TEXT("object_type"), static_cast<int32>(BodyInstance.GetObjectType()));
    BodyInstanceObject->SetStringField(TEXT("collision_profile_name"), BodyInstance.GetCollisionProfileName().ToString());
    BodyInstanceObject->SetNumberField(TEXT("position_solver_iteration_count"), BodyInstance.PositionSolverIterationCount);
    BodyInstanceObject->SetNumberField(TEXT("velocity_solver_iteration_count"), BodyInstance.VelocitySolverIterationCount);
    BodyInstanceObject->SetNumberField(TEXT("projection_solver_iteration_count"), BodyInstance.ProjectionSolverIterationCount);
    BodyInstanceObject->SetNumberField(TEXT("mass_override"), BodyInstance.GetMassOverride());
    BodyInstanceObject->SetNumberField(TEXT("linear_damping"), BodyInstance.LinearDamping);
    BodyInstanceObject->SetNumberField(TEXT("angular_damping"), BodyInstance.AngularDamping);
    BodyInstanceObject->SetBoolField(TEXT("use_ccd"), BodyInstance.bUseCCD != 0);
    BodyInstanceObject->SetBoolField(TEXT("notify_rigid_body_collision"), BodyInstance.bNotifyRigidBodyCollision != 0);
    Object->SetObjectField(TEXT("body_instance"), BodyInstanceObject);

    if (bIncludeCollisionSummary)
    {
        Object->SetObjectField(TEXT("shape_counts"), CollisionShapeCountsToJson(BodySetup->AggGeom));
        Object->SetArrayField(TEXT("shapes"), CollisionShapesToJson(BodySetup->AggGeom, MaxShapesPerBody, bOutShapesTruncated));
    }

    if (bIncludeProfiles)
    {
        Object->SetArrayField(TEXT("physical_animation_profiles"), PhysicalAnimationProfilesToJson(BodySetup, MaxProfilesPerItem, bOutProfilesTruncated));
    }
    return Object;
}

TSharedPtr<FJsonObject> ConstraintTemplateToJson(
    const UPhysicsConstraintTemplate* ConstraintTemplate,
    int32 ConstraintIndex,
    bool bIncludeProfiles,
    int32 MaxProfilesPerItem,
    bool& bOutProfilesTruncated)
{
    Count(TEXT("ConstraintTemplateToJson_calls"));
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("index"), ConstraintIndex);
    if (!ConstraintTemplate)
    {
        Object->SetBoolField(TEXT("valid"), false);
        return Object;
    }

    if(!Hold(ConstraintTemplate)) { return Object; }
    const FConstraintInstance& Constraint = ConstraintTemplate->DefaultInstance;
    Object->SetBoolField(TEXT("valid"), true);
    Object->SetStringField(TEXT("name"), ConstraintTemplate->GetName());
    Object->SetStringField(TEXT("path"), ConstraintTemplate->GetPathName());
    Object->SetStringField(TEXT("joint_name"), Constraint.JointName.ToString());
    Object->SetStringField(TEXT("child_bone"), Constraint.GetChildBoneName().ToString());
    Object->SetStringField(TEXT("parent_bone"), Constraint.GetParentBoneName().ToString());
    Object->SetArrayField(TEXT("pos1"), VectorToJsonArray(Constraint.Pos1));
    Object->SetArrayField(TEXT("pos2"), VectorToJsonArray(Constraint.Pos2));
    Object->SetArrayField(TEXT("pri_axis1"), VectorToJsonArray(Constraint.PriAxis1));
    Object->SetArrayField(TEXT("pri_axis2"), VectorToJsonArray(Constraint.PriAxis2));
    Object->SetArrayField(TEXT("sec_axis1"), VectorToJsonArray(Constraint.SecAxis1));
    Object->SetArrayField(TEXT("sec_axis2"), VectorToJsonArray(Constraint.SecAxis2));
    AddRotatorFields(Object, TEXT("angular_rotation_offset"), Constraint.AngularRotationOffset);
    Object->SetBoolField(TEXT("scale_linear_limits"), Constraint.bScaleLinearLimits != 0);
    Object->SetObjectField(TEXT("default_profile"), ConstraintProfilePropertiesToJson(Constraint.ProfileInstance));

    if (bIncludeProfiles)
    {
        Object->SetArrayField(TEXT("profile_handles"), ConstraintProfileHandlesToJson(ConstraintTemplate, MaxProfilesPerItem, bOutProfilesTruncated));
    }
    return Object;
}

struct FOptions
{
    bool bIncludeBodies;
    bool bIncludeConstraints;
    bool bIncludeProfiles;
    bool bIncludeSolverSettings;
    bool bIncludeCollisionSummary;
    bool bRequireSupportedClass;
    int32 MaxBodies;
    int32 MaxConstraints;
    int32 MaxProfilesPerItem;
    int32 MaxShapesPerBody;
};

bool ReferencesSafe(UPhysicsAsset* Asset,const FOptions& Options)
{
    Count(TEXT("reference_preflight_calls"));if(!Asset) { return true; }
    if(!Hold(Asset)||!Hold(Asset->GetOutermost())||!Small(Asset->SkeletalBodySetups.Num())||!Small(Asset->ConstraintSetup.Num())||!Small(Asset->BoundsBodies.Num())) { return false; }
    const FString PreviewPath=Asset->PreviewSkeletalMesh.ToSoftObjectPath().ToString();
    if(!InputTextSafe(PreviewPath,NAME_SIZE-1)) { Unsafe(TEXT("Preview mesh soft path exceeds safe FName/load bound"));return false; }
    int64 Work=64,SourceItems=0;
    auto Account=[&](int64 Source,int64 Selected)
    { SourceItems+=Source;Work+=Selected;if(SourceItems>InspectionWorkBudget||Work>InspectionWorkBudget) { Unsafe(TEXT("Selected authored inspection exceeds65536 source/work items"));return false; }return true; };
    if(Options.bIncludeProfiles)
    {
        Count(TEXT("asset_profile_name_getter_calls"),2);
        const auto& Constraints=Asset->GetConstraintProfileNames();const auto& Physical=Asset->GetPhysicalAnimationProfileNames();
        if(!Small(Constraints.Num())||!Small(Physical.Num())||!Account(Constraints.Num()+Physical.Num(),Constraints.Num()+Physical.Num())) { return false; }
    }
    if(Options.bIncludeBodies)
    {
        for(int32 I=0;I<FMath::Min(Options.MaxBodies,Asset->SkeletalBodySetups.Num());++I)
        {
            Count(TEXT("body_reference_rows_checked"));const auto* B=Asset->SkeletalBodySetups[I].Get();if(!Hold(B)) { return false; }if(!Account(1,64)) { return false; }if(!B) { continue; }
            if(!Hold(B->PhysMaterial.Get())) { return false; }
            if(Options.bIncludeProfiles)
            {
                Count(TEXT("body_profile_getter_calls"));const auto& Profiles=B->GetPhysicalAnimationProfiles();
                if(!Small(Profiles.Num())||!Account(Profiles.Num(),int64(FMath::Min(Profiles.Num(),Options.MaxProfilesPerItem))*16)) { return false; }
            }
            if(Options.bIncludeCollisionSummary)
            {
                const auto& G=B->AggGeom;int64 ShapeCount=0;
                for(int32 N:{G.SphereElems.Num(),G.BoxElems.Num(),G.SphylElems.Num(),G.ConvexElems.Num(),G.TaperedCapsuleElems.Num(),G.LevelSetElems.Num(),G.SkinnedLevelSetElems.Num(),G.MLLevelSetElems.Num(),G.SkinnedTriangleMeshElems.Num()})
                { if(!Small(N)) { return false; }ShapeCount+=N; }
                const int64 Detailed=int64(G.SphereElems.Num())+G.BoxElems.Num()+G.SphylElems.Num()+G.TaperedCapsuleElems.Num()+G.ConvexElems.Num();
                if(!Account(ShapeCount,int64(FMath::Min<int64>(Detailed,Options.MaxShapesPerBody))*16)) { return false; }
            }
        }
    }
    if(Options.bIncludeConstraints)
    {
        for(int32 I=0;I<FMath::Min(Options.MaxConstraints,Asset->ConstraintSetup.Num());++I)
        {
            Count(TEXT("constraint_reference_rows_checked"));const auto* C=Asset->ConstraintSetup[I].Get();if(!Hold(C)||!Account(1,160)) { return false; }if(!C) { continue; }
            if(Options.bIncludeProfiles&&(!Small(C->ProfileHandles.Num())||!Account(C->ProfileHandles.Num(),int64(FMath::Min(C->ProfileHandles.Num(),Options.MaxProfilesPerItem))*128))) { return false; }
        }
    }
    return true;
}
const USkeletalMesh* PreviewForObservation(const UPhysicsAsset* Asset)
{
    Count(TEXT("const_preview_mesh_virtual_calls"));const auto* Provider=static_cast<const IInterface_PreviewMeshProvider*>(Asset);const auto* Mesh=Provider->GetPreviewMesh();
    if(!Hold(Mesh)) { return nullptr; }return Mesh;
}
UObject* LoadForInspection(const FString& AssetPath)
{
    const FString Normalized=NormalizeAssetObjectPathForLoad(AssetPath);
    if(!InputTextSafe(Normalized,NAME_SIZE-1)) { Unsafe(TEXT("Normalized asset object path exceeds safe FName/load bound"));return nullptr; }
    if(Normalized.IsEmpty()) { return nullptr; }
    auto Calls=Current->Data->GetArrayField(TEXT("asset_load_calls"));auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("requested_path"),AssetPath);J->SetStringField(TEXT("normalized_path"),Normalized);J->SetBoolField(TEXT("editor_load_attempted"),true);J->SetBoolField(TEXT("editor_load_returned"),false);J->SetBoolField(TEXT("static_load_attempted"),false);J->SetBoolField(TEXT("static_load_returned"),false);Calls.Add(MakeShared<FJsonValueObject>(J));Current->Data->SetArrayField(TEXT("asset_load_calls"),Calls);
    Count(TEXT("editor_asset_load_calls"));UObject* O=UEditorAssetLibrary::LoadAsset(Normalized);Count(TEXT("editor_asset_load_returns"));J->SetBoolField(TEXT("editor_load_returned"),true);
    if(!O) { Count(TEXT("static_load_calls"));J->SetBoolField(TEXT("static_load_attempted"),true);O=StaticLoadObject(UObject::StaticClass(),nullptr,*Normalized);Count(TEXT("static_load_returns"));J->SetBoolField(TEXT("static_load_returned"),true); }
    J->SetStringField(TEXT("returned_object_path"),O?O->GetPathName():FString());if(!Hold(O)) { return nullptr; }return O;
}

TSharedPtr<FJsonObject> Observe(UObject* LoadedObject,USkeletalMesh* SourceSkeletalMesh,UPhysicsAsset* PhysicsAsset,const FString& PhysicsAssetPath,const FString& SkeletalMeshPath,const FOptions& Options)
{
    Count(TEXT("observation_calls"));
    const bool bIncludeBodies=Options.bIncludeBodies;
    const bool bIncludeConstraints=Options.bIncludeConstraints;
    const bool bIncludeProfiles=Options.bIncludeProfiles;
    const bool bIncludeSolverSettings=Options.bIncludeSolverSettings;
    const bool bIncludeCollisionSummary=Options.bIncludeCollisionSummary;
    const bool bRequireSupportedClass=Options.bRequireSupportedClass;
    const int32 MaxBodies=Options.MaxBodies;
    const int32 MaxConstraints=Options.MaxConstraints;
    const int32 MaxProfilesPerItem=Options.MaxProfilesPerItem;
    const int32 MaxShapesPerBody=Options.MaxShapesPerBody;
    TArray<TSharedPtr<FJsonValue>> WarningValues;
    TArray<TSharedPtr<FJsonValue>> ErrorValues;
    TArray<TSharedPtr<FJsonValue>> UnavailableValues;
    const bool bSupportedClass = PhysicsAsset != nullptr;

    TSharedPtr<FJsonObject> AssetObject = MakeShared<FJsonObject>();
    AssetObject->SetStringField(TEXT("input_physics_asset_path"), PhysicsAssetPath);
    AssetObject->SetStringField(TEXT("input_skeletal_mesh_path"), SkeletalMeshPath);
    AssetObject->SetStringField(TEXT("loaded_object_path"), LoadedObject ? LoadedObject->GetPathName() : FString());
    AssetObject->SetStringField(TEXT("loaded_object_class"), LoadedObject && LoadedObject->GetClass() ? LoadedObject->GetClass()->GetPathName() : FString());
    AssetObject->SetBoolField(TEXT("supported_class"), bSupportedClass);

    if (!bSupportedClass)
    {
        AddJsonString(ErrorValues, TEXT("Loaded object is not a UPhysicsAsset."));
        if (bRequireSupportedClass)
        {
            TSharedPtr<FJsonObject> ErrorObj = LegacyError(TEXT("Unsupported asset class for inspect_physics_asset_constraints_guarded"));
            ErrorObj->SetBoolField(TEXT("unsupported_class"), true);
            ErrorObj->SetObjectField(TEXT("asset"), AssetObject);
            return ErrorObj;
        }
    }

    UPackage* Package = PhysicsAsset ? PhysicsAsset->GetOutermost() : (LoadedObject ? LoadedObject->GetOutermost() : nullptr);
    const bool bPackageDirtyBefore = Package && Package->IsDirty();

    if (PhysicsAsset)
    {
        const USkeletalMesh* PreviewMesh = PreviewForObservation(PhysicsAsset);
        AssetObject->SetStringField(TEXT("physics_asset_path"), PhysicsAsset->GetPathName());
        AssetObject->SetStringField(TEXT("physics_asset_name"), PhysicsAsset->GetName());
        AssetObject->SetStringField(TEXT("package"), Package ? Package->GetPathName() : FString());
        AssetObject->SetStringField(TEXT("source_skeletal_mesh_path"), SourceSkeletalMesh ? SourceSkeletalMesh->GetPathName() : FString());
        AssetObject->SetStringField(TEXT("preview_skeletal_mesh"), PreviewMesh ? PreviewMesh->GetPathName() : FString());
    }

    if(Incomplete()||!ReferencesSafe(PhysicsAsset,Options))
    { Current->Data->SetObjectField(TEXT("partial_observed_asset"),AssetObject);return LegacyError(TEXT("Current asset/reference state unavailable after preview observation")); }

    TArray<TSharedPtr<FJsonValue>> BodyValues;
    bool bBodiesTruncated = false;
    bool bBodyProfilesTruncated = false;
    bool bBodyShapesTruncated = false;
    if (bIncludeBodies && PhysicsAsset)
    {
        for (int32 BodyIndex = 0; BodyIndex < PhysicsAsset->SkeletalBodySetups.Num(); ++BodyIndex)
        {
            if (BodyValues.Num() >= MaxBodies)
            {
                bBodiesTruncated = true;
                continue;
            }

            const USkeletalBodySetup* BodySetup = PhysicsAsset->SkeletalBodySetups[BodyIndex].Get();
            if (!BodySetup)
            {
                AddJsonString(WarningValues, FString::Printf(TEXT("Null SkeletalBodySetup at index %d"), BodyIndex));
            }

            bool bThisBodyProfilesTruncated = false;
            bool bThisBodyShapesTruncated = false;
            BodyValues.Add(MakeShared<FJsonValueObject>(BodySetupToJson(
                PhysicsAsset,
                BodySetup,
                BodyIndex,
                bIncludeProfiles,
                bIncludeCollisionSummary,
                MaxProfilesPerItem,
                MaxShapesPerBody,
                bThisBodyProfilesTruncated,
                bThisBodyShapesTruncated)));
            bBodyProfilesTruncated = bBodyProfilesTruncated || bThisBodyProfilesTruncated;
            bBodyShapesTruncated = bBodyShapesTruncated || bThisBodyShapesTruncated;
        }
    }

    TArray<TSharedPtr<FJsonValue>> ConstraintValues;
    bool bConstraintsTruncated = false;
    bool bConstraintProfilesTruncated = false;
    if (bIncludeConstraints && PhysicsAsset)
    {
        for (int32 ConstraintIndex = 0; ConstraintIndex < PhysicsAsset->ConstraintSetup.Num(); ++ConstraintIndex)
        {
            if (ConstraintValues.Num() >= MaxConstraints)
            {
                bConstraintsTruncated = true;
                continue;
            }

            const UPhysicsConstraintTemplate* ConstraintTemplate = PhysicsAsset->ConstraintSetup[ConstraintIndex].Get();
            if (!ConstraintTemplate)
            {
                AddJsonString(WarningValues, FString::Printf(TEXT("Null PhysicsConstraintTemplate at index %d"), ConstraintIndex));
            }

            bool bThisConstraintProfilesTruncated = false;
            ConstraintValues.Add(MakeShared<FJsonValueObject>(ConstraintTemplateToJson(
                ConstraintTemplate,
                ConstraintIndex,
                bIncludeProfiles,
                MaxProfilesPerItem,
                bThisConstraintProfilesTruncated)));
            bConstraintProfilesTruncated = bConstraintProfilesTruncated || bThisConstraintProfilesTruncated;
        }
    }

    TSharedPtr<FJsonObject> ProfilesObject = MakeShared<FJsonObject>();
    ProfilesObject->SetBoolField(TEXT("included"), bIncludeProfiles && PhysicsAsset != nullptr);
    if (bIncludeProfiles && PhysicsAsset)
    {
#if WITH_EDITOR
        Count(TEXT("asset_profile_name_getter_calls"),2);
        ProfilesObject->SetArrayField(TEXT("constraint_profile_names"), NameArrayToJson(PhysicsAsset->GetConstraintProfileNames()));
        ProfilesObject->SetArrayField(TEXT("physical_animation_profile_names"), NameArrayToJson(PhysicsAsset->GetPhysicalAnimationProfileNames()));
#else
        AddJsonString(UnavailableValues, TEXT("asset_level_profile_names"));
        ProfilesObject->SetArrayField(TEXT("constraint_profile_names"), TArray<TSharedPtr<FJsonValue>>());
        ProfilesObject->SetArrayField(TEXT("physical_animation_profile_names"), TArray<TSharedPtr<FJsonValue>>());
#endif
    }

    TSharedPtr<FJsonObject> LimitsObject = MakeShared<FJsonObject>();
    LimitsObject->SetBoolField(TEXT("include_bodies"), bIncludeBodies);
    LimitsObject->SetBoolField(TEXT("include_constraints"), bIncludeConstraints);
    LimitsObject->SetBoolField(TEXT("include_profiles"), bIncludeProfiles);
    LimitsObject->SetBoolField(TEXT("include_solver_settings"), bIncludeSolverSettings);
    LimitsObject->SetBoolField(TEXT("include_collision_summary"), bIncludeCollisionSummary);
    LimitsObject->SetBoolField(TEXT("require_supported_class"), bRequireSupportedClass);
    LimitsObject->SetNumberField(TEXT("max_bodies"), MaxBodies);
    LimitsObject->SetNumberField(TEXT("max_constraints"), MaxConstraints);
    LimitsObject->SetNumberField(TEXT("max_profiles_per_item"), MaxProfilesPerItem);
    LimitsObject->SetNumberField(TEXT("max_shapes_per_body"), MaxShapesPerBody);

    TSharedPtr<FJsonObject> TruncatedObject = MakeShared<FJsonObject>();
    TruncatedObject->SetBoolField(TEXT("bodies"), bBodiesTruncated);
    TruncatedObject->SetBoolField(TEXT("constraints"), bConstraintsTruncated);
    TruncatedObject->SetBoolField(TEXT("body_profiles"), bBodyProfilesTruncated);
    TruncatedObject->SetBoolField(TEXT("constraint_profiles"), bConstraintProfilesTruncated);
    TruncatedObject->SetBoolField(TEXT("body_shapes"), bBodyShapesTruncated);

    TSharedPtr<FJsonObject> CountsObject = MakeShared<FJsonObject>();
    CountsObject->SetNumberField(TEXT("body_count"), PhysicsAsset ? PhysicsAsset->SkeletalBodySetups.Num() : 0);
    CountsObject->SetNumberField(TEXT("body_included_count"), BodyValues.Num());
    CountsObject->SetNumberField(TEXT("constraint_count"), PhysicsAsset ? PhysicsAsset->ConstraintSetup.Num() : 0);
    CountsObject->SetNumberField(TEXT("constraint_included_count"), ConstraintValues.Num());
    CountsObject->SetNumberField(TEXT("bounds_body_count"), PhysicsAsset ? PhysicsAsset->BoundsBodies.Num() : 0);

    const bool bPackageDirtyAfter = Package && Package->IsDirty();
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("command"), TEXT("inspect_physics_asset_constraints_guarded"));
    ResultObj->SetBoolField(TEXT("read_only"), true);
    ResultObj->SetBoolField(TEXT("asset_modified"), bPackageDirtyAfter != bPackageDirtyBefore);
    ResultObj->SetBoolField(TEXT("saves_assets"), false);
    ResultObj->SetBoolField(TEXT("unsupported_class"), !bSupportedClass);
    ResultObj->SetBoolField(TEXT("package_dirty_before"), bPackageDirtyBefore);
    ResultObj->SetBoolField(TEXT("package_dirty_after"), bPackageDirtyAfter);
    ResultObj->SetObjectField(TEXT("asset"), AssetObject);
    ResultObj->SetObjectField(TEXT("solver_settings"), bIncludeSolverSettings ? PhysicsSolverSettingsToJson(PhysicsAsset) : MakeShared<FJsonObject>());
    ResultObj->SetObjectField(TEXT("profiles"), ProfilesObject);
    ResultObj->SetArrayField(TEXT("bodies"), BodyValues);
    ResultObj->SetArrayField(TEXT("constraints"), ConstraintValues);
    ResultObj->SetObjectField(TEXT("limits"), LimitsObject);
    ResultObj->SetObjectField(TEXT("truncated"), TruncatedObject);
    ResultObj->SetObjectField(TEXT("counts"), CountsObject);
    ResultObj->SetArrayField(TEXT("warnings"), WarningValues);
    ResultObj->SetArrayField(TEXT("errors"), ErrorValues);
    ResultObj->SetArrayField(TEXT("unavailable_fields"), UnavailableValues);
    ResultObj->SetStringField(TEXT("scope_note"), TEXT("Editor-only read-only PhysicsAsset summary. This command does not simulate physics, apply profiles, cook collision, compile, save, or mutate assets."));
    return ResultObj;
}

void Init(FResult& R)
{
    R.Data->SetStringField(TEXT("operation"),TEXT("inspect_physics_asset_constraints_guarded"));R.Data->SetBoolField(TEXT("read_only"),true);R.Data->SetBoolField(TEXT("observation_incomplete"),false);R.Data->SetBoolField(TEXT("observed_selected_metadata_changed"),false);R.Data->SetBoolField(TEXT("load_cache_effects_unverified"),true);
    R.Data->SetStringField(TEXT("change_scope"),TEXT("Selected authored metadata and package dirty state after initial asset load; direct mutation flag counts only adapter writer calls"));R.Data->SetStringField(TEXT("counter_scope"),TEXT("Explicit adapter/helper call-sites; SDK internal loading/cache/profile work is not an aggregate count"));
    auto C=MakeShared<FJsonObject>();for(const TCHAR* K:{TEXT("editor_asset_load_calls"),TEXT("editor_asset_load_returns"),TEXT("static_load_calls"),TEXT("static_load_returns"),TEXT("observation_calls"),TEXT("reference_preflight_calls"),TEXT("body_reference_rows_checked"),TEXT("constraint_reference_rows_checked"),TEXT("asset_profile_name_getter_calls"),TEXT("body_profile_getter_calls"),TEXT("source_physics_asset_getter_calls"),TEXT("const_preview_mesh_virtual_calls"),TEXT("body_collision_trace_getter_calls"),TEXT("body_collision_enabled_getter_calls"),TEXT("body_collision_profile_getter_calls"),TEXT("PhysicsSolverSettingsToJson_calls"),TEXT("PhysicalAnimationDataToJson_calls"),TEXT("PhysicalAnimationProfilesToJson_calls"),TEXT("CollisionShapeCountsToJson_calls"),TEXT("AddSphereShapeSummary_calls"),TEXT("AddBoxShapeSummary_calls"),TEXT("AddSphylShapeSummary_calls"),TEXT("AddTaperedCapsuleShapeSummary_calls"),TEXT("AddConvexShapeSummary_calls"),TEXT("CollisionShapesToJson_calls"),TEXT("ConstraintProfilePropertiesToJson_calls"),TEXT("ConstraintProfileHandlesToJson_calls"),TEXT("BodySetupToJson_calls"),TEXT("ConstraintTemplateToJson_calls"),TEXT("explicit_modify_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls"),TEXT("physics_simulation_calls"),TEXT("profile_application_calls"),TEXT("collision_cook_calls")}) { C->SetNumberField(K,0); }R.Data->SetObjectField(TEXT("native_call_counts"),C);R.Data->SetArrayField(TEXT("asset_load_calls"),TArray<TSharedPtr<FJsonValue>>{});
}
}
Variable::FResult Inspect(const FString& PhysicsAssetPath,const FString& SkeletalMeshPath,bool bIncludeBodies,bool bIncludeConstraints,bool bIncludeProfiles,bool bIncludeSolverSettings,bool bIncludeCollisionSummary,int64 MaxBodies,int64 MaxConstraints,int64 MaxProfilesPerItem,int64 MaxShapesPerBody,bool bRequireSupportedClass)
{
    FResult R;Init(R);if(!IsEditorGameThread()||Current) { R.ErrorCode=TEXT("physics_asset_editor_state_invalid");R.Error=TEXT("Nonreentrant Editor game thread required");return R; }
    struct FScope { FScope(FResult& Value) { Current=&Value;Held.Reset();HeldObjects.Reset(); }~FScope() { Held.Reset();HeldObjects.Reset();Current=nullptr; } } Scope(R);
    R.Data->SetStringField(TEXT("requested_physics_asset_path"),PhysicsAssetPath);R.Data->SetStringField(TEXT("requested_skeletal_mesh_path"),SkeletalMeshPath);
    if(!InputTextSafe(PhysicsAssetPath,NAME_SIZE-1)||!InputTextSafe(SkeletalMeshPath,NAME_SIZE-1)) { R.ErrorCode=TEXT("physics_asset_input_invalid");R.Error=TEXT("Bounded paths without interior NUL required before load/find");return R; }
    auto Failed=[&](const TSharedPtr<FJsonObject>& J,const TCHAR* Code)
    { R.Data->SetObjectField(TEXT("legacy_result"),J);if(Incomplete()) { R.bChangeStateKnown=false; }else { R.ErrorCode=Code;J->TryGetStringField(TEXT("error"),R.Error); }return R; };
    if(PhysicsAssetPath.IsEmpty()&&SkeletalMeshPath.IsEmpty()) { return Failed(LegacyError(TEXT("Missing 'physics_asset_path' or 'skeletal_mesh_path' parameter")),TEXT("physics_asset_path_missing")); }
    const FString SelectedPath=PhysicsAssetPath.IsEmpty()?SkeletalMeshPath:PhysicsAssetPath;R.Stage=TEXT("load_physics_asset_source");UObject* Object=LoadForInspection(SelectedPath);
    if(!Object) { const auto J=LegacyError((PhysicsAssetPath.IsEmpty()?TEXT("SkeletalMesh not found or failed to load: "):TEXT("PhysicsAsset not found or failed to load: "))+SelectedPath);return Failed(J,Incomplete()?TEXT("physics_asset_observation_incomplete"):TEXT("physics_asset_source_not_found")); }
    USkeletalMesh* SourceMesh=nullptr;UPhysicsAsset* Asset=nullptr;
    if(!PhysicsAssetPath.IsEmpty()) { Asset=Cast<UPhysicsAsset>(Object); }
    else
    {
        SourceMesh=Cast<USkeletalMesh>(Object);
        if(!SourceMesh)
        { auto J=LegacyError(TEXT("Loaded skeletal_mesh_path object is not a USkeletalMesh"));J->SetBoolField(TEXT("unsupported_class"),true);J->SetStringField(TEXT("loaded_object_path"),Object->GetPathName());J->SetStringField(TEXT("loaded_object_class"),Object->GetClass()->GetPathName());return Failed(J,TEXT("skeletal_mesh_class_unsupported")); }
        Count(TEXT("source_physics_asset_getter_calls"));Asset=SourceMesh->GetPhysicsAsset();if(!Hold(Asset)) { R.bChangeStateKnown=false;return R; }
        if(!Asset) { return Failed(LegacyError(TEXT("SkeletalMesh has no PhysicsAsset: ")+SkeletalMeshPath),TEXT("skeletal_mesh_physics_asset_missing")); }
    }
    FOptions Options;Options.bIncludeBodies=bIncludeBodies;Options.bIncludeConstraints=bIncludeConstraints;Options.bIncludeProfiles=bIncludeProfiles;Options.bIncludeSolverSettings=bIncludeSolverSettings;Options.bIncludeCollisionSummary=bIncludeCollisionSummary;Options.bRequireSupportedClass=bRequireSupportedClass;
    Options.MaxBodies=static_cast<int32>(FMath::Clamp<int64>(MaxBodies,0,4096));Options.MaxConstraints=static_cast<int32>(FMath::Clamp<int64>(MaxConstraints,0,4096));Options.MaxProfilesPerItem=static_cast<int32>(FMath::Clamp<int64>(MaxProfilesPerItem,0,256));Options.MaxShapesPerBody=static_cast<int32>(FMath::Clamp<int64>(MaxShapesPerBody,0,512));
    if(bRequireSupportedClass&&!Asset)
    { auto J=Observe(Object,SourceMesh,Asset,PhysicsAssetPath,SkeletalMeshPath,Options);return Failed(J,TEXT("physics_asset_class_unsupported")); }
    R.Stage=TEXT("physics_asset_reference_preflight");if(!ReferencesSafe(Asset,Options)) { R.bChangeStateKnown=false;return R; }
    UPackage* Package=Asset?Asset->GetOutermost():Object->GetOutermost();if(!Hold(Package)) { R.bChangeStateKnown=false;return R; }const bool DirtyBefore=Package&&Package->IsDirty();
    R.Stage=TEXT("observe_physics_asset");const auto Report=Observe(Object,SourceMesh,Asset,PhysicsAssetPath,SkeletalMeshPath,Options);const FString First=Serialize(Report);
    const bool DirtyAfterFirst=Package&&Package->IsDirty();R.bChanged=DirtyBefore!=DirtyAfterFirst;R.Data->SetBoolField(TEXT("package_dirty_before"),DirtyBefore);R.Data->SetBoolField(TEXT("package_dirty_after"),DirtyAfterFirst);
    if(!First.IsEmpty()) { R.Data->SetObjectField(TEXT("legacy_result"),Report); }else { R.Data->SetBoolField(TEXT("legacy_result_omitted_for_native_budget"),true); }
    if(Incomplete()) { R.bChangeStateKnown=false;return R; }
    R.Stage=TEXT("verify_current_physics_metadata");
    if(!Hold(Object)||!Hold(SourceMesh)||!ReferencesSafe(Asset,Options)) { R.bChangeStateKnown=false;return R; }
    if(SourceMesh) { Count(TEXT("source_physics_asset_getter_calls"));auto* Assigned=SourceMesh->GetPhysicsAsset();if(!Hold(Assigned)) { R.bChangeStateKnown=false;return R; }if(Assigned!=Asset) { R.bChanged=true;R.Data->SetBoolField(TEXT("assigned_physics_asset_changed"),true);R.ErrorCode=TEXT("physics_asset_current_readback_mismatch");R.Error=TEXT("Source mesh assigned PhysicsAsset changed between selected observations");return R; } }
    const auto Readback=Observe(Object,SourceMesh,Asset,PhysicsAssetPath,SkeletalMeshPath,Options);const FString Second=Serialize(Readback);const bool DirtyAfter=Package&&Package->IsDirty();R.bChanged|=DirtyBefore!=DirtyAfter;R.Data->SetBoolField(TEXT("package_dirty_after"),DirtyAfter);
    const bool SelectedChanged=!First.IsEmpty()&&!Second.IsEmpty()&&!First.Equals(Second,ESearchCase::CaseSensitive);R.bChanged|=SelectedChanged;R.Data->SetBoolField(TEXT("observed_selected_metadata_changed"),SelectedChanged);
    if(SelectedChanged) { R.Data->SetObjectField(TEXT("verification_report"),Readback);if(Serialize(R.Data).IsEmpty()) { R.Data->RemoveField(TEXT("verification_report"));R.Data->SetBoolField(TEXT("verification_report_omitted_for_native_budget"),true); } }
    R.Data->SetBoolField(TEXT("selected_metadata_unchanged"),!First.IsEmpty()&&First.Equals(Second,ESearchCase::CaseSensitive));
    if(Incomplete()) { R.bChangeStateKnown=false;return R; }
    R.bPostconditionPassed=!R.bChanged&&!First.IsEmpty()&&First.Equals(Second,ESearchCase::CaseSensitive);
    if(!R.bPostconditionPassed) { R.ErrorCode=TEXT("physics_asset_current_readback_mismatch");R.Error=TEXT("Selected metadata/assigned asset/package dirty scope changed; current prefix retained without replay");return R; }
    R.bSuccess=true;R.Stage=TEXT("complete");if(Serialize(R.Data).IsEmpty()) { R.Data->RemoveField(TEXT("legacy_result"));R.Data->SetBoolField(TEXT("legacy_result_omitted_for_native_budget"),true);R.bSuccess=false;R.bPostconditionPassed=false;R.bChangeStateKnown=false; }return R;
}
}
