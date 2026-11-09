#include "CBMLegTwist.h"
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBMLegTwistTransportTest,"Cubeless.BodyMocap.Alignment.LegTwistTransport",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCBMLegTwistTransportTest::RunTest(const FString&)
{
 using namespace CBMLegTwist;
 const FVector LocalAxis=FVector(1,.04,-.03).GetSafeNormal();
 const FQuat Source=FRotator(12,38,-25).Quaternion();
 const FVector A=Source.RotateVector(LocalAxis);
 const FVector B=FQuat(FVector(.2,.5,.7).GetSafeNormal(),.35).RotateVector(A);
 const FQuat Swing=FQuat::FindBetweenVectors(A,B),Transported=(Swing*Source).GetNormalized();
 double Angle=99.;
 TestTrue(TEXT("Unmodified source orientation remains valid"),MeasureAddedTwist(Source,A,Source,A,Angle));
 TestTrue(TEXT("Existing source pose is not penalized"),FMath::Abs(Angle)<1.e-9);
 TestTrue(TEXT("Pure shortest swing remains valid"),MeasureAddedTwist(Source,A,Transported,B,Angle));
 TestTrue(TEXT("Swing is not mistaken for axial twist"),FMath::Abs(Angle)<1.e-9);
 for(double Expected:{-.61,.37}){
  const FQuat Candidate=(FQuat(B,Expected)*Transported).GetNormalized();
  TestTrue(TEXT("Known added roll is measurable"),MeasureAddedTwist(Source,A*43.,Candidate,B*39.,Angle));
  TestTrue(TEXT("Signed roll and non-unit bone length are correct"),FMath::Abs(Angle-Expected)<1.e-9);
  TestTrue(TEXT("Finite non-unit quaternion magnitudes are normalized"),MeasureAddedTwist(Source*1.e-5,A,Candidate*3.,B,Angle));
  TestTrue(TEXT("Quaternion magnitude leaves measured roll unchanged"),FMath::Abs(Angle-Expected)<1.e-9);
  for(int SourceSign:{-1,1})for(int CandidateSign:{-1,1}){
   TestTrue(TEXT("Both quaternion signs are valid"),MeasureAddedTwist(Source*double(SourceSign),A,Candidate*double(CandidateSign),B,Angle));
   TestTrue(TEXT("Quaternion sign leaves signed roll unchanged"),FMath::Abs(Angle-Expected)<1.e-9);
  }
  const FQuat World=FRotator(-37,121,64).Quaternion();
  TestTrue(TEXT("Common world rotation remains valid"),MeasureAddedTwist(World*Source,World.RotateVector(A),World*Candidate,World.RotateVector(B),Angle));
  TestTrue(TEXT("Roll is independent of world orientation"),FMath::Abs(Angle-Expected)<1.e-9);
  auto MirrorVector=[](const FVector& V){return FVector(V.X,-V.Y,V.Z);};
  auto MirrorQuat=[](const FQuat& Q){return FQuat(-Q.X,Q.Y,-Q.Z,Q.W);};
  TestTrue(TEXT("Reflected left/right poses remain valid"),MeasureAddedTwist(MirrorQuat(Source),MirrorVector(A),MirrorQuat(Candidate),MirrorVector(B),Angle));
  TestTrue(TEXT("Reflection reverses signed roll without changing magnitude"),FMath::Abs(Angle+Expected)<1.e-9);
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBMLegTwistInvalidTest,"Cubeless.BodyMocap.Alignment.LegTwistInvalidInput",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCBMLegTwistInvalidTest::RunTest(const FString&)
{
 using namespace CBMLegTwist;
 const FQuat I=FQuat::Identity;const FVector A=FVector::ForwardVector;double Angle=99.;
 TestFalse(TEXT("Zero source axis is rejected"),MeasureAddedTwist(I,FVector::ZeroVector,I,A,Angle));
 TestEqual(TEXT("Invalid result has a deterministic zero output"),Angle,0.);
 TestFalse(TEXT("Zero candidate axis is rejected"),MeasureAddedTwist(I,A,I,FVector::ZeroVector,Angle));
 TestFalse(TEXT("Zero quaternion is rejected"),MeasureAddedTwist(FQuat(0,0,0,0),A,I,A,Angle));
 const double NaN=std::numeric_limits<double>::quiet_NaN();
 FVector BadAxis=FVector::ZeroVector;BadAxis.X=NaN;FQuat BadRotation=I;BadRotation.Y=NaN;
 TestFalse(TEXT("Non-finite axis is rejected"),MeasureAddedTwist(I,BadAxis,I,A,Angle));
 TestFalse(TEXT("Non-finite rotation is rejected"),MeasureAddedTwist(I,A,BadRotation,A,Angle));
 const FQuat Opposite(FVector::UpVector,UE_PI),NearOpposite(FVector::UpVector,UE_PI-1.e-4);
 TestFalse(TEXT("Antipodal transport is rejected"),MeasureAddedTwist(I,A,Opposite,-A,Angle));
 TestFalse(TEXT("Nearly antipodal transport does not pick an unstable axis"),MeasureAddedTwist(I,A,NearOpposite,NearOpposite.RotateVector(A),Angle));
 TestFalse(TEXT("Inconsistent rotation and bone basis are rejected"),MeasureAddedTwist(I,A,I,FVector::RightVector,Angle));
 TestFalse(TEXT("Signed half-turn roll is ambiguous"),MeasureAddedTwist(I,A,FQuat(A,UE_PI),A,Angle));
 TestFalse(TEXT("Negative float half-turn is equally ambiguous"),MeasureAddedTwist(I,A,FQuat(A,-double(float(UE_DOUBLE_PI))),A,Angle));
 TestTrue(TEXT("Unambiguous roll near a half-turn is not an anatomical clamp"),MeasureAddedTwist(I,A,FQuat(A,UE_DOUBLE_PI-.001),A,Angle));
 TestTrue(TEXT("Large valid roll retains its actual magnitude"),FMath::Abs(Angle-(UE_DOUBLE_PI-.001))<1.e-8);
 const FQuat LargeSwing(FVector::UpVector,UE_PI-.01);
 TestTrue(TEXT("Large but unambiguous swing remains supported"),MeasureAddedTwist(I,A,LargeSwing,LargeSwing.RotateVector(A),Angle));
 TestTrue(TEXT("Large swing has no artificial roll"),FMath::Abs(Angle)<1.e-8);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBMLegTwistChainTest,"Cubeless.BodyMocap.Alignment.LegTwistWholeChain",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCBMLegTwistChainTest::RunTest(const FString&)
{
 const FVector H(1,2,90),K(11,3,49),E(3,1,8),Axis=(E-H).GetSafeNormal();
 const FQuat Thigh=FQuat::FindBetweenVectors(FVector::ForwardVector,K-H);
 const FQuat Calf=(FQuat((E-K).GetSafeNormal(),.21)*FQuat::FindBetweenVectors(FVector::ForwardVector,E-K)).GetNormalized();
 const FQuat Foot=FRotator(-8,14,5).Quaternion();
 const FVector KneeLocal=Thigh.UnrotateVector(K-H),FootLocal=Calf.UnrotateVector(E-K);
 const FQuat OriginalKnee=(Thigh.Inverse()*Calf).GetNormalized();
 for(double Radians:{-.2,.3}){
  const FQuat Q(Axis,Radians),NewThigh=(Q*Thigh).GetNormalized(),NewCalf=(Q*Calf).GetNormalized();
  const FVector NewK=H+NewThigh.RotateVector(KneeLocal);
  const FVector NewE=NewK+NewCalf.RotateVector(FootLocal);
  const FQuat NewKnee=(NewThigh.Inverse()*NewCalf).GetNormalized();
  const FQuat NewAnkle=(NewCalf.Inverse()*Foot).GetNormalized();
  TestTrue(TEXT("Rotating the whole leg plane preserves knee-local orientation"),OriginalKnee.Equals(NewKnee,1.e-9));
  TestTrue(TEXT("Fixed endpoint remains anchored"),NewE.Equals(E,1.e-9));
  TestTrue(TEXT("Relocalizing the ankle preserves world foot orientation"),(NewCalf*NewAnkle).GetNormalized().Equals(Foot,1.e-9));
  TestTrue(TEXT("Thigh length is unchanged"),FMath::Abs((NewK-H).Size()-(K-H).Size())<1.e-9);
  TestTrue(TEXT("Calf length is unchanged"),FMath::Abs((NewE-NewK).Size()-(E-K).Size())<1.e-9);
  const FQuat UnwoundCalf=(FQuat::FindBetweenVectors(E-K,NewE-NewK)*Calf).GetNormalized();
  const FQuat UnwoundKnee=(NewThigh.Inverse()*UnwoundCalf).GetNormalized();
  TestTrue(TEXT("Calf-only unwind transfers roll into the knee and is not equivalent"),FMath::RadiansToDegrees(OriginalKnee.AngularDistance(UnwoundKnee))>5.);
 }
 return true;
}
#endif
