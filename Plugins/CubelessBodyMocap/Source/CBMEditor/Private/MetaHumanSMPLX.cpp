// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanSMPLX.h"

#include "Logging/LogMacros.h"
DEFINE_LOG_CATEGORY_STATIC(LogMetaHumanBodyTracker, Log, All);

#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "SkeletalMeshAttributes.h"
#include "Rendering/SkeletalMeshModel.h"
#include "ReferenceSkeleton.h"
#include "Animation/Skeleton.h"
#include "SkeletalRenderPublic.h"
#include "StaticMeshOperations.h"
#include "Engine/StaticMesh.h"



THIRD_PARTY_INCLUDES_START
#include "Eigen/Dense"
THIRD_PARTY_INCLUDES_END



class FMetaHumanSMPLXImpl
{
public:

	// This class, its variables, and the maths behind it, assumes you are already familiar with the SMPLX model.
	// See https://github.com/vchoutas/smplx for more detail of how the SMPLX model is defined and controlled.

	bool Init(TUniquePtr<FCBMBodyData> InData);
	bool IsInitialized() const;
	const FCBMBodyData* GetSmplxData() const { return SmplxData.Get(); }

	TUniquePtr<FCBMBodyData> SmplxData;

	bool SetShape(const TArray<float>& InShapeData);
	bool SetPose(const TArray<float>& InPoseData);

	bool SetAccountForHeight(bool bInAccountForHeight);
	bool GetAccountForHeight() const;

	void Calculate();

	const TArray<FVector>& GetSkinnedVerticesSMPL();
	const TArray<FVector>& GetSkinnedVerticesUE();
	const TArray<TTuple<uint32, uint32, uint32>>& GetFaces() const;
	const TArray<TTuple<FString, int32, FTransform>>& GetBoneHierarchyUE();
	float GetHeightOffset() const;

	const TArray<FString> JointNamesSMPL =
	{
		"pelvis",
		"left_hip",
		"right_hip",
		"spine1",
		"left_knee",
		"right_knee",
		"spine2",
		"left_ankle",
		"right_ankle",
		"spine3",
		"left_foot",
		"right_foot",
		"neck",
		"left_collar",
		"right_collar",
		"head",
		"left_shoulder",
		"right_shoulder",
		"left_elbow",
		"right_elbow",
		"left_wrist",
		"right_wrist",
		"jaw",
		"left_eye_smplhf",
		"right_eye_smplhf",
		"left_index1",
		"left_index2",
		"left_index3",
		"left_middle1",
		"left_middle2",
		"left_middle3",
		"left_pinky1",
		"left_pinky2",
		"left_pinky3",
		"left_ring1",
		"left_ring2",
		"left_ring3",
		"left_thumb1",
		"left_thumb2",
		"left_thumb3",
		"right_index1",
		"right_index2",
		"right_index3",
		"right_middle1",
		"right_middle2",
		"right_middle3",
		"right_pinky1",
		"right_pinky2",
		"right_pinky3",
		"right_ring1",
		"right_ring2",
		"right_ring3",
		"right_thumb1",
		"right_thumb2",
		"right_thumb3",
	};

	const TArray<FString> JointNamesUE =
	{
		"pelvis",
		"thigh_l",
		"thigh_r",
		"spine_01",
		"calf_l",
		"calf_r",
		"spine_02",
		"foot_l",
		"foot_r",
		"spine_03",
		"ball_l",
		"ball_r",
		"neck_01",
		"clavicle_l",
		"clavicle_r",
		"head",
		"upperarm_l",
		"upperarm_r",
		"lowerarm_l",
		"lowerarm_r",
		"hand_l",
		"hand_r",
		"jaw",
		"eye_l",
		"eye_r",
		"index_01_l",
		"index_02_l",
		"index_03_l",
		"middle_01_l",
		"middle_02_l",
		"middle_03_l",
		"pinky_01_l",
		"pinky_02_l",
		"pinky_03_l",
		"ring_01_l",
		"ring_02_l",
		"ring_03_l",
		"thumb_01_l",
		"thumb_02_l",
		"thumb_03_l",
		"index_01_r",
		"index_02_r",
		"index_03_r",
		"middle_01_r",
		"middle_02_r",
		"middle_03_r",
		"pinky_01_r",
		"pinky_02_r",
		"pinky_03_r",
		"ring_01_r",
		"ring_02_r",
		"ring_03_r",
		"thumb_01_r",
		"thumb_02_r",
		"thumb_03_r"
	};

	const TArray<int32> KinematicTree = { -1, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9, 9, 12, 13, 14, 16, 17, 18, 19, 15, 15, 15, 20, 25, 26, 20, 28, 29, 20, 31, 32, 20, 34, 35, 20, 37, 38, 21, 40, 41, 21, 43, 44, 21, 46, 47, 21, 49, 50, 21, 52, 53 };
	Eigen::Matrix<float, -1, -1> Verts;
	TArray<TTuple<uint32, uint32, uint32>> Faces;
	Eigen::Matrix<float, -1, -1> JointReg;
	Eigen::Matrix<float, -1, -1> Weights;
	Eigen::Matrix<float, -1, -1> ShapeBlendShapes;
	Eigen::Matrix<float, -1, -1> PoseBlendShapes;

	Eigen::Matrix<float, -1, 1> Shape;
	Eigen::Matrix<float, -1, 1> Pose;

	static constexpr int32 NumVerts = 10475;
	static constexpr int32 NumFaces = 20908;

	static constexpr int32 NumJoints = 55;
	static constexpr int32 NumShapes = 400;

	bool bIsInitialized = false;
	bool bIsUpToDate = false;
	bool bAccountForHeight = true;
	float HeightOffset = 0;
	TArray<float> ShapeCached;

	TArray<FVector> SkinnedVertsSMPL;
	TArray<FVector> SkinnedVertsUE;
	TArray<TTuple<FString, int32, FTransform>> BoneHierarchyUE;
};

bool FMetaHumanSMPLXImpl::Init(TUniquePtr<FCBMBodyData> InData)
{
	if (!InData)
	{
		UE_LOGF(LogMetaHumanBodyTracker, Warning, "No SMPLX data asset");
		return false;
	}

	if (InData->Verts.Num() == NumVerts * 3 &&
		InData->Faces.Num() == NumFaces * 3 &&
		InData->JointReg.Num() == NumJoints * NumVerts &&
		InData->Weights.Num() == NumVerts * NumJoints &&
		InData->BlendShapes.Num() == NumVerts * 3 * (NumShapes + 9 * (NumJoints - 1)))
	{
		Verts.resize(NumVerts, 3);
		FMemory::Memcpy(Verts.data(), InData->Verts.GetData(), Verts.size() * sizeof(float));

		Eigen::Matrix<uint32, -1, -1> FacesMatrix;
		FacesMatrix.resize(NumFaces, 3);
		FMemory::Memcpy(FacesMatrix.data(), InData->Faces.GetData(), FacesMatrix.size() * sizeof(uint32));

		Faces.SetNumUninitialized(NumFaces);
		for (int32 Face = 0; Face < NumFaces; ++Face)
		{
			Faces[Face] = TTuple<uint32, uint32, uint32>(FacesMatrix(Face, 0), FacesMatrix(Face, 1), FacesMatrix(Face, 2));
		}

		JointReg.resize(NumJoints, NumVerts);
		FMemory::Memcpy(JointReg.data(), InData->JointReg.GetData(), JointReg.size() * sizeof(float));

		Weights.resize(NumVerts, NumJoints);
		FMemory::Memcpy(Weights.data(), InData->Weights.GetData(), Weights.size() * sizeof(float));

		Eigen::Matrix<float, -1, -1> BlendShapes;
		BlendShapes.resize(NumVerts * 3, NumShapes + 9 * (NumJoints - 1));
		FMemory::Memcpy(BlendShapes.data(), InData->BlendShapes.GetData(), BlendShapes.size() * sizeof(float));

		ShapeBlendShapes = BlendShapes.block(0, 0, BlendShapes.rows(), NumShapes);
		PoseBlendShapes = BlendShapes.block(0, NumShapes, BlendShapes.rows(), 9 * (NumJoints - 1));

		Shape.resize(NumShapes, 1);
		Shape.setZero();

		ShapeCached = FMetaHumanSMPLX::InvalidShape(); // An impossible shape to ensure no accidental cache hits

		Pose.resize(NumJoints * 3, 1);
		Pose.setZero();

		SmplxData = MoveTemp(InData);
		bIsInitialized = true;
		bIsUpToDate = false;
	}
	else
	{
		UE_LOGF(LogMetaHumanBodyTracker, Warning, "Incorrect SMPLX data asset content");
	}

	return bIsInitialized;
}

bool FMetaHumanSMPLXImpl::IsInitialized() const
{
	return bIsInitialized;
}

bool FMetaHumanSMPLXImpl::SetShape(const TArray<float>& InShapeData)
{
	if (!bIsInitialized)
	{
		UE_LOGF(LogMetaHumanBodyTracker, Warning, "Not initialized");
		return false;
	}

	if (InShapeData.Num() > Shape.rows()) // Only specifying a sub-set of the shape is ok
	{
		UE_LOGF(LogMetaHumanBodyTracker, Warning, "Too many shape data items, got %i expected at most %lli", InShapeData.Num(), Shape.rows());
		return false;
	}

	if (InShapeData == ShapeCached)
	{
		return true;
	}

	ShapeCached = InShapeData;

	Shape.setZero();
	for (int32 Index = 0; Index < InShapeData.Num(); ++Index)
	{
		Shape(Index, 0) = InShapeData[Index];
	}

	const Eigen::Matrix<float, -1, -1> PoseBackup = Pose;
	SetPose(TArray<float>());

	bIsUpToDate = false;

	HeightOffset = 0.0;
	GetSkinnedVerticesSMPL();

	float YMin = SkinnedVertsSMPL[0].Y;
	for (const FVector& Vert : SkinnedVertsSMPL)
	{
		if (Vert.Y < YMin)
		{
			YMin = Vert.Y;
		}
	}

	HeightOffset = -YMin;

	Pose = PoseBackup;

	bIsUpToDate = false;

	return true;
}

bool FMetaHumanSMPLXImpl::SetPose(const TArray<float>& InPoseData)
{
	if (!bIsInitialized)
	{
		UE_LOGF(LogMetaHumanBodyTracker, Warning, "Not initialized");
		return false;
	}

	if (InPoseData.Num() > Pose.rows()) // Only specifying a sub-set of the pose is ok
	{
		UE_LOGF(LogMetaHumanBodyTracker, Warning, "Too many pose data items, got %i expected at most %lli", InPoseData.Num(), Pose.rows());
		return false;
	}

	Pose.setZero();
	for (int32 Index = 0; Index < InPoseData.Num(); ++Index)
	{
		Pose(Index, 0) = InPoseData[Index];
	}

	bIsUpToDate = false;

	return true;
}

bool FMetaHumanSMPLXImpl::SetAccountForHeight(bool bInAccountForHeight)
{
	bAccountForHeight = bInAccountForHeight;

	bIsUpToDate = false;

	return true;
}

bool FMetaHumanSMPLXImpl::GetAccountForHeight() const
{
	return bAccountForHeight;
}

void FMetaHumanSMPLXImpl::Calculate()
{
	// Shaped vertices
	Eigen::Matrix<float, -1, -1> VertDeltas = ShapeBlendShapes * Shape; // ((NumVerts * 3) x 1) col vector 
	const Eigen::Map<Eigen::Matrix<float, -1, 3, Eigen::RowMajor>> VertDeltasReshaped(VertDeltas.data(), Verts.rows(), Verts.cols()); // (NumVerts x 3)
	Eigen::Matrix<float, -1, -1> ShapedVerts = Verts + VertDeltasReshaped;

	// Joint regression
	const Eigen::Matrix<float, -1, -1> Joints = JointReg * ShapedVerts;

	// Forward Kinematics

	// - Local Transforms
	TArray<Eigen::Matrix<float, 4, 4>> LocalTransforms;
	LocalTransforms.SetNumUninitialized(NumJoints);

	Eigen::Matrix<float, -1, 1> PoseBasisWeights;
	PoseBasisWeights.resize(9 * (NumJoints - 1), 1);
	int32 PoseBasisWeightsRow = 0;

	for (int32 Joint = 0; Joint < NumJoints; ++Joint)
	{
		Eigen::Vector3f Axis(Pose(Joint * 3), Pose(Joint * 3 + 1), Pose(Joint * 3 + 2));
		const float Angle = Axis.norm();

		Eigen::Matrix<float, 3, 3> R;

		if (Angle > 1e-4)
		{
			Axis.normalize();

			const Eigen::AngleAxisf AxisAngle(Angle, Axis);

			R = AxisAngle.toRotationMatrix();
		}
		else
		{
			R.setIdentity();
		}

		if (Joint > 0)
		{
			Eigen::Matrix<float, 3, 3> RMinusI = R - Eigen::Matrix<float, 3, 3>::Identity();

			for (int32 Row = 0; Row < 3; ++Row)
			{
				for (int32 Col = 0; Col < 3; ++Col)
				{
					PoseBasisWeights(PoseBasisWeightsRow++, 0) = RMinusI(Row, Col);
				}
			}
		}

		Eigen::Matrix<float, 4, 4> T;
		T.setIdentity();

		T.block<3, 3>(0, 0) = R; //-V521

		if (KinematicTree[Joint] == -1)
		{
			T(0, 3) = Joints(Joint, 0);
			T(1, 3) = Joints(Joint, 1);
			T(2, 3) = Joints(Joint, 2);
		}
		else
		{
			const int32 Parent = KinematicTree[Joint];
			T(0, 3) = Joints(Joint, 0) - Joints(Parent, 0);
			T(1, 3) = Joints(Joint, 1) - Joints(Parent, 1);
			T(2, 3) = Joints(Joint, 2) - Joints(Parent, 2);
		}

		LocalTransforms[Joint] = T;
	}

	// - World Transforms
	TArray<Eigen::Matrix<float, 4, 4>> WorldTransforms;
	WorldTransforms.SetNumUninitialized(NumJoints);

	for (int32 Joint = 0; Joint < NumJoints; ++Joint)
	{
		const int32 Parent = KinematicTree[Joint];

		if (Parent == -1)
		{
			WorldTransforms[Joint] = LocalTransforms[Joint];
		}
		else
		{
			WorldTransforms[Joint] = WorldTransforms[Parent] * LocalTransforms[Joint];
		}
	}

	// - For skinning, we need transformations relative to rest pose
	TArray<Eigen::Matrix<float, 4, 4>> SkinningMatrix;
	SkinningMatrix.SetNumUninitialized(NumJoints);

	const Eigen::Matrix<float, -1, -1> RestJoints = JointReg * ShapedVerts;

	for (int32 Joint = 0; Joint < NumJoints; ++Joint)
	{
		Eigen::Matrix<float, 4, 4> RestTransformInv;
		RestTransformInv.setIdentity();

		RestTransformInv(0, 3) = -RestJoints(Joint, 0);
		RestTransformInv(1, 3) = -RestJoints(Joint, 1);
		RestTransformInv(2, 3) = -RestJoints(Joint, 2);

		SkinningMatrix[Joint] = WorldTransforms[Joint] * RestTransformInv;
	}

	Eigen::Matrix<float, -1, -1> PoseDeltas = PoseBlendShapes * PoseBasisWeights;
	const Eigen::Map<Eigen::Matrix<float, -1, -1, Eigen::RowMajor>> PoseDeltasReshaped(PoseDeltas.data(), NumVerts, 3);
	ShapedVerts += PoseDeltasReshaped;

	// Skinned verts
	SkinnedVertsSMPL.SetNumUninitialized(NumVerts);
	SkinnedVertsUE.SetNumUninitialized(NumVerts);

	for (int32 Vert = 0; Vert < NumVerts; ++Vert)
	{
		const Eigen::Vector4f InitialVertPos(ShapedVerts(Vert, 0), ShapedVerts(Vert, 1), ShapedVerts(Vert, 2), 1.0);
		Eigen::Vector4f FinalVertPos(0, 0, 0, 0);

		for (int32 Joint = 0; Joint < NumJoints; ++Joint)
		{
			const float Weight = Weights(Vert, Joint);
			if (Weight > 0)
			{
				FinalVertPos += Weight * (SkinningMatrix[Joint] * InitialVertPos);
			}
		}

		SkinnedVertsSMPL[Vert] = FVector(FinalVertPos(0), FinalVertPos(1), FinalVertPos(2));
		if (bAccountForHeight)
		{
			SkinnedVertsSMPL[Vert].Y += HeightOffset;
		}
		SkinnedVertsUE[Vert] = FMetaHumanSMPLX::SMPL2UE(SkinnedVertsSMPL[Vert]);
	}

	// Bone hierarchy
	BoneHierarchyUE.Reset(NumJoints + 1);
	BoneHierarchyUE.Add(TTuple<FString, int32, FTransform>("root", -1, FTransform::Identity));

	Eigen::Matrix<float, 4, 4> SMPL2UE; // convert SMPL (x, y, z) to UE (x, z, y). Meters to cm accounted for later.
	SMPL2UE.setZero();
	SMPL2UE(0, 0) = 1;
	SMPL2UE(1, 2) = 1;
	SMPL2UE(2, 1) = 1;
	SMPL2UE(3, 3) = 1;

	const Eigen::Matrix<float, 4, 4> SMPL2UETranspose = SMPL2UE.transpose();

	for (int32 Joint = 0; Joint < NumJoints; ++Joint)
	{
		const Eigen::Matrix<float, 4, 4> UE = SMPL2UE * LocalTransforms[Joint] * SMPL2UETranspose;

		FMatrix Matrix;
		for (int32 Row = 0; Row < 4; ++Row)
		{
			for (int32 Col = 0; Col < 4; ++Col)
			{
				Matrix.M[Col][Row] = UE(Row, Col);
			}
		}

		Matrix.M[3][0] *= 100;
		Matrix.M[3][1] *= 100;
		Matrix.M[3][2] *= 100;

		if (Joint == 0 && bAccountForHeight)
		{
			Matrix.M[3][2] += (HeightOffset * 100);
		}

		BoneHierarchyUE.Add(TTuple<FString, int32, FTransform>(JointNamesUE[Joint], KinematicTree[Joint] + 1, FTransform(Matrix)));
	}

	bIsUpToDate = true;
}

const TArray<FVector>& FMetaHumanSMPLXImpl::GetSkinnedVerticesSMPL()
{
	if (!bIsInitialized)
	{
		UE_LOGF(LogMetaHumanBodyTracker, Warning, "Not initialized");
	}

	if (!bIsUpToDate)
	{
		Calculate();
	}

	return SkinnedVertsSMPL;
}

const TArray<FVector>& FMetaHumanSMPLXImpl::GetSkinnedVerticesUE()
{
	if (!bIsInitialized)
	{
		UE_LOGF(LogMetaHumanBodyTracker, Warning, "Not initialized");
	}

	if (!bIsUpToDate)
	{
		Calculate();
	}

	return SkinnedVertsUE;
}

const TArray<TTuple<uint32, uint32, uint32>>& FMetaHumanSMPLXImpl::GetFaces() const
{
	if (!bIsInitialized)
	{
		UE_LOGF(LogMetaHumanBodyTracker, Warning, "Not initialized");
	}

	return Faces;
}

const TArray<TTuple<FString, int32, FTransform>>& FMetaHumanSMPLXImpl::GetBoneHierarchyUE()
{
	if (!bIsInitialized)
	{
		UE_LOGF(LogMetaHumanBodyTracker, Warning, "Not initialized");
	}

	if (!bIsUpToDate)
	{
		Calculate();
	}

	return BoneHierarchyUE;
}

float FMetaHumanSMPLXImpl::GetHeightOffset() const
{
	return HeightOffset;
}



FMetaHumanSMPLX::FMetaHumanSMPLX()
{
	Impl = MakeUnique<FMetaHumanSMPLXImpl>();
}

FMetaHumanSMPLX::~FMetaHumanSMPLX()
{
	Impl.Reset();
}

bool FMetaHumanSMPLX::Init(UMetaHumanSMPLXData* InData)
{
	if (!InData)
	{
		InData = LoadObject<UMetaHumanSMPLXData>(GetTransientPackage(), TEXT("/" UE_PLUGIN_NAME "/SMPLX_NEUTRAL_2020_locked_head_array_f32.SMPLX_NEUTRAL_2020_locked_head_array_f32"));
	}

	if(!InData)return false;
 auto Copy=MakeUnique<FCBMBodyData>();Copy->Verts=InData->Verts;Copy->Faces=InData->Faces;Copy->JointReg=InData->JointReg;Copy->Weights=InData->Weights;Copy->BlendShapes=InData->BlendShapes;
 return InitData(MoveTemp(Copy));
}

bool FMetaHumanSMPLX::IsInitialized() const
{
	return Impl->IsInitialized();
}

const FCBMBodyData* FMetaHumanSMPLX::GetSmplxData() const
{
	return Impl->GetSmplxData();
}

bool FMetaHumanSMPLX::SetShape(const TArray<float>& InShapeData)
{
	return Impl->SetShape(InShapeData);
}

bool FMetaHumanSMPLX::SetPose(const TArray<float>& InPoseData)
{
	return Impl->SetPose(InPoseData);
}

bool FMetaHumanSMPLX::SetAccountForHeight(bool bInAccountForHeight)
{
	return Impl->SetAccountForHeight(bInAccountForHeight);
}

const TArray<FVector>& FMetaHumanSMPLX::GetSkinnedVerticesSMPL()
{
	return Impl->GetSkinnedVerticesSMPL();
}

const TArray<FVector>& FMetaHumanSMPLX::GetSkinnedVerticesUE()
{
	return Impl->GetSkinnedVerticesUE();
}

const TArray<TTuple<uint32, uint32, uint32>>& FMetaHumanSMPLX::GetFaces() const
{
	return Impl->GetFaces();
}

const TArray<TTuple<FString, int32, FTransform>>& FMetaHumanSMPLX::GetBoneHierarchyUE()
{
	return Impl->GetBoneHierarchyUE();
}

float FMetaHumanSMPLX::GetHeightOffset() const
{
	return Impl->GetHeightOffset();
}

FVector FMetaHumanSMPLX::SMPL2UE(const FVector& InVector)
{
	FVector Out = FVector(InVector.X, InVector.Z, InVector.Y); // Account for orientation and left/right coord system
	Out *= 100; // Meters to cm

	return Out;
}

FVector FMetaHumanSMPLX::UE2SMPL(const FVector& InVector)
{
	FVector Out = FVector(InVector.X, InVector.Z, InVector.Y); // Account for orientation and left/right coord system
	Out /= 100; // cm to meters

	return Out;
}

TArray<float> FMetaHumanSMPLX::InvalidShape()
{
	TArray<float> Shape;

	Shape.SetNumZeroed(FMetaHumanSMPLXImpl::NumShapes + 1);

	return Shape;
}


bool FMetaHumanSMPLX::InitData(TUniquePtr<FCBMBodyData> Data) { return Impl->Init(MoveTemp(Data)); }
