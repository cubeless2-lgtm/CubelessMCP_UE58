// Adapted from Epic Games MetaHumanBodyTracker. Copyright Epic Games, Inc. All Rights Reserved.
static bool Ground(FSolveData& Data, FMetaHumanSMPLX& Model, const TArray<float>& AverageShape, float InCameraPitchRadians, float InCameraRollRadians, float& OutGlobalMaxY)
{
	const int32 HpsNumFrames = Data.Shape.Num() / 10;
	if (HpsNumFrames == 0
		|| (Data.Pose.Num() / 165) != HpsNumFrames
		|| (Data.Translation.Num() / 3) != HpsNumFrames
		|| Data.ValidFrame.Num() != HpsNumFrames)
	{
		UE_LOGFMT(LogCubelessMocap, Error, "ApplyHps: buffer size mismatch");
		return false;
	}
	
	Model.SetShape(AverageShape);

	// Calculate position of pelvis (the center for rotation)
	FVector PelvisVector = Model.GetBoneHierarchyUE()[1].Get<2>().GetLocation(); // Bone 1 = pelvis
	PelvisVector = FMetaHumanSMPLX::UE2SMPL(PelvisVector);
	const Eigen::Vector3f Pelvis(PelvisVector.X, PelvisVector.Y, PelvisVector.Z);

	// Account for camera orientation and find global min Y position of all skinned vertex across all frames.
	// The anim is then translated so that the global min Y = 0 thus locating the anim in the 3D world at the groundplane.
	// However, this bit of code is still working in smpl-camera coords, where +Y is down; so its the max Y we actually need.
	// The conversion to smpl-world coords where +Y is up (so flipping Y and Z) comes later.

	const Eigen::Matrix<float, 3, 3> CameraPitch = Eigen::AngleAxisf(-InCameraPitchRadians, Eigen::Vector3f::UnitX()).toRotationMatrix();
	const Eigen::Matrix<float, 3, 3> CameraRoll = Eigen::AngleAxisf(InCameraRollRadians, Eigen::Vector3f::UnitY()).toRotationMatrix();
	const Eigen::Matrix<float, 3, 3> CameraRotation = CameraPitch * CameraRoll;
	float GlobalMaxY = 0;

	// Pass 1: Apply camera rotation and find ground contact
	for (int32 Frame = 0; Frame < HpsNumFrames; ++Frame)
	{
        if(Data.ShouldCancel && Data.ShouldCancel())return false;
		if (!Data.ValidFrame[Frame])
		{
			continue;
		}

		// Pelvis pose
		Eigen::Vector3f Axis(Data.Pose[Frame * 165 + 0], Data.Pose[Frame * 165 + 1], Data.Pose[Frame * 165 + 2]);
		const float Angle = Axis.norm();

		Eigen::Matrix<float, 3, 3> Pose;

		if (Angle > 1e-4)
		{
			Axis.normalize();

			const Eigen::AngleAxisf AxisAngle(Angle, Axis);

			Pose = AxisAngle.toRotationMatrix();
		}
		else
		{
			Pose.setIdentity();
		}

		Pose = CameraRotation * Pose;

		Eigen::Vector3f Translation(Data.Translation[Frame * 3 + 0], Data.Translation[Frame * 3 + 1], Data.Translation[Frame * 3 + 2]);

		Translation = CameraRotation * (Pelvis + Translation) - Pelvis;

		// Store modified values

		const Eigen::AngleAxisf AxisAngle = Eigen::AngleAxisf(Pose);
		const Eigen::Vector3f CompactRodrigues = AxisAngle.axis() * AxisAngle.angle();

		Data.Pose[Frame * 165 + 0] = CompactRodrigues[0];
		Data.Pose[Frame * 165 + 1] = CompactRodrigues[1];
		Data.Pose[Frame * 165 + 2] = CompactRodrigues[2];

		Data.Translation[Frame * 3 + 0] = Translation(0);
		Data.Translation[Frame * 3 + 1] = Translation(1);
		Data.Translation[Frame * 3 + 2] = Translation(2);

		// Find min (actually max!) skinned vertex position of the pose

		TArray<float> FramePose;
		FramePose.SetNumUninitialized(165);
		for (int32 Index = 0; Index < 165; ++Index)
		{
			FramePose[Index] = Data.Pose[Frame * 165 + Index];
		}

		Model.SetPose(FramePose);

		const TArray<FVector>& SkinnedVerts = Model.GetSkinnedVerticesSMPL();
		float MaxY = SkinnedVerts[0].Y;
		for (const FVector& Vert : SkinnedVerts)
		{
			if (Vert.Y > MaxY)
			{
				MaxY = Vert.Y;
			}
		}
		MaxY += Translation(1);

		if (Frame == 0 || MaxY > GlobalMaxY)
		{
			GlobalMaxY = MaxY;
		}
	}

	// Pass 2: Transform from camera coord to SMPLX's Y-up world coord. Negate Y and Z axis.
	// Also place animation on groundplane by translating by GlobalMaxY.

	Eigen::Matrix<float, 3, 3> Flip;
	Flip << 1, 0, 0,  0, -1, 0,  0, 0, -1;

	for (int32 Frame = 0; Frame < HpsNumFrames; ++Frame)
	{
        if(Data.ShouldCancel && Data.ShouldCancel())return false;
		if (!Data.ValidFrame[Frame])
		{
			continue;
		}

		// Pelvis pose
		Eigen::Vector3f Axis(Data.Pose[Frame * 165 + 0], Data.Pose[Frame * 165 + 1], Data.Pose[Frame * 165 + 2]);
		const float Angle = Axis.norm();

		Eigen::Matrix<float, 3, 3> Pose;

		if (Angle > 1e-4)
		{
			Axis.normalize();

			const Eigen::AngleAxisf AxisAngle(Angle, Axis);

			Pose = AxisAngle.toRotationMatrix();
		}
		else
		{
			Pose.setIdentity();
		}

		Pose = Flip * Pose;

		Eigen::Vector3f Translation(Data.Translation[Frame * 3 + 0], Data.Translation[Frame * 3 + 1], Data.Translation[Frame * 3 + 2]);

		Translation = Flip * (Pelvis + Translation) + Eigen::Vector3f(0, GlobalMaxY, 0) - Pelvis;

		// Store modified values

		const Eigen::AngleAxisf AxisAngle = Eigen::AngleAxisf(Pose);
		const Eigen::Vector3f CompactRodrigues = AxisAngle.axis() * AxisAngle.angle();

		Data.Pose[Frame * 165 + 0] = CompactRodrigues[0];
		Data.Pose[Frame * 165 + 1] = CompactRodrigues[1];
		Data.Pose[Frame * 165 + 2] = CompactRodrigues[2];

		Data.Translation[Frame * 3 + 0] = Translation(0);
		Data.Translation[Frame * 3 + 1] = Translation(1);
		Data.Translation[Frame * 3 + 2] = Translation(2);
	}

	OutGlobalMaxY = GlobalMaxY;

	return true;
}
