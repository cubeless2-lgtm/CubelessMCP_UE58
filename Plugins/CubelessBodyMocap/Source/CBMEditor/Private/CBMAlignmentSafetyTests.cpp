#include "CBMAlignmentSafety.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBMAlignmentTrustTest,"Cubeless.BodyMocap.Alignment.TemporalEvidence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCBMAlignmentTrustTest::RunTest(const FString&){
 TArray<CBMAlignment::FObservation> O;O.SetNum(31);
 for(int I=0;I<O.Num();++I){O[I].Relative=FVector2D(I*.15,I*.07);O[I].Supported=true;}
 auto Q=CBMAlignment::TemporalReliability(O,30,1);
 for(float V:Q)TestTrue(TEXT("Coherent fast motion including boundaries retains trust"),V>.999f);
 O[15].Relative+=FVector2D(2,-2);Q=CBMAlignment::TemporalReliability(O,30,1);
 TestTrue(TEXT("High-score isolated outlier loses trust"),Q[15]<.01f);
 TestTrue(TEXT("Trust fades beside outlier"),Q[14]<.8f);
 TestTrue(TEXT("Distant motion remains trusted"),Q[5]>.999f);
 O[0].Relative+=FVector2D(3,3);Q=CBMAlignment::TemporalReliability(O,30,1);
 TestTrue(TEXT("One-sided boundary evidence catches first-frame outlier"),Q[0]<.01f);
 for(auto& V:O)V.Supported=false;Q=CBMAlignment::TemporalReliability(O,30,1);
 for(float V:Q)TestEqual(TEXT("Missing evidence is conservative"),V,.25f);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBMAlignmentGuardTest,"Cubeless.BodyMocap.Alignment.AddedMotionGuard",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCBMAlignmentGuardTest::RunTest(const FString&){
 using namespace CBMAlignment;
 TArray<FMotionSample> S;S.SetNum(61);TArray<float> A;A.Init(.2f,S.Num());
 for(int I=0;I<S.Num();++I){const FQuat Q(FVector::UpVector,FMath::DegreesToRadians(I*35.));S[I].Axis=FVector::UpVector;S[I].U=Q.RotateVector(FVector(12,0,0));S[I].V=FVector::CrossProduct(S[I].Axis,S[I].U);S[I].Root=Q;S[I].Middle=Q;}
 FGuardStats Normal;
 TestTrue(TEXT("Fast original motion is valid"),AttenuateMotionRegression(A,S,30,Normal,[]{return false;},1));
 TestEqual(TEXT("Coherent correction retains full strength"),Normal.Attenuated,0);
 for(float V:A)TestEqual(TEXT("No original speed or angle clamp"),V,.2f);
 A.Init(0,S.Num());A[30]=1.2f;FGuardStats Spike;
 TestTrue(TEXT("Added spike can be attenuated"),AttenuateMotionRegression(A,S,30,Spike,[]{return false;},1));
 TestTrue(TEXT("Problematic correction reduced"),A[30]<.1f);
 TestTrue(TEXT("Added rotation step bounded against original"),Spike.MaxStepAfter<=1.00001);
 TestTrue(TEXT("Input pose retained"),S[30].Root.Equals(FQuat(FVector::UpVector,FMath::DegreesToRadians(30*35.)),1.e-9));
 TArray<float> Cancel=A;FGuardStats Stop;
 TestFalse(TEXT("Cancellation returns before committing angles"),AttenuateMotionRegression(Cancel,S,30,Stop,[]{return true;},1));
 TestTrue(TEXT("Cancelled output unchanged"),Cancel==A);
 A.Init(0,S.Num());FGuardStats Zero;
 TestTrue(TEXT("Unmodified fast baseline passes"),AttenuateMotionRegression(A,S,30,Zero,[]{return false;},1));
 TestEqual(TEXT("Zero correction stays zero"),Zero.Attenuated,0);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBMAlignmentTurnTest,"Cubeless.BodyMocap.Alignment.CurvedMotionEvidence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCBMAlignmentTurnTest::RunTest(const FString&){
 for(int FPS:{24,30,60}){
  TArray<CBMAlignment::FObservation> O;O.SetNum(4*FPS+1);
  for(int I=0;I<O.Num();++I){O[I].Relative=FVector2D(.25*FMath::Cos(4*UE_PI*I/FPS),.15*FMath::Sin(4*UE_PI*I/FPS));O[I].Supported=true;}
  auto Q=CBMAlignment::TemporalReliability(O,FPS,1);
  TestTrue(TEXT("Accurate 2Hz turning point retains high reliability"),Q[2*FPS]>.95f);
  O[2*FPS].Relative+=FVector2D(2,-2);Q=CBMAlignment::TemporalReliability(O,FPS,1);
  TestTrue(TEXT("Curvature support does not accept isolated detection spike"),Q[2*FPS]<.01f);
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBMAlignmentAngularTest,"Cubeless.BodyMocap.Alignment.AngularAcceleration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCBMAlignmentAngularTest::RunTest(const FString&){
 using namespace CBMAlignment;
 TArray<FMotionSample> S;S.SetNum(61);TArray<float> A;A.SetNum(S.Num());
 for(int I=0;I<S.Num();++I){const FQuat Q(FVector::UpVector,FMath::DegreesToRadians(double(I)));S[I].Axis=FVector::UpVector;S[I].U=Q.RotateVector(FVector(3,0,0));S[I].V=FVector::CrossProduct(S[I].Axis,S[I].U);S[I].Root=Q;S[I].Middle=I%2?Q*-1.:Q;A[I]=FMath::DegreesToRadians(I%2?.2f:-.2f);}
 FGuardStats G;
 TestTrue(TEXT("Direction changes are attenuated"),AttenuateMotionRegression(A,S,30,G,[]{return false;},1));
 TestTrue(TEXT("Regression passed the old rotation-step budget"),G.MaxStepBefore<1);
 TestTrue(TEXT("Regression passed the old position-acceleration budget"),G.MaxAccelerationBefore<.25);
 TestTrue(TEXT("Angular acceleration detects the missing regression"),G.MaxAngularAccelerationBefore>.79);
 TestTrue(TEXT("Added angular acceleration is bounded"),G.MaxAngularAccelerationAfter<=.50001);
 TestTrue(TEXT("Correction was reduced"),G.Attenuated>0);
 A.Init(.2f,S.Num());FGuardStats Sign;
 TestTrue(TEXT("Quaternion sign and constant correction are invariant"),AttenuateMotionRegression(A,S,30,Sign,[]{return false;},1));
 TestEqual(TEXT("No attenuation of constant correction on normal motion"),Sign.Attenuated,0);
 return true;
}
#endif
