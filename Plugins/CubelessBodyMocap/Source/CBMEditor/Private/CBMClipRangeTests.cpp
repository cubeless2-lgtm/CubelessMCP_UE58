#include "CBMClipRange.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBMClipRangeTest,"Cubeless.BodyMocap.Preview.ClipRange",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCBMClipRangeTest::RunTest(const FString&){
 auto A=FCBMClipRange::Normalize(3,10,0,15,30);TestEqual(TEXT("Input interval start"),A.X,3.);TestEqual(TEXT("Exclusive input end"),A.Y,10.);
 A=FCBMClipRange::Normalize(14,10,3,15,30);TestTrue(TEXT("Start cannot cross end"),A.Y-A.X>=.1-1.e-9);
 A=FCBMClipRange::Normalize(3,2,3,15,30);TestTrue(TEXT("End cannot cross start"),A.Y-A.X>=.1-1.e-9);
 A=FCBMClipRange::Normalize(0,900,0,900,30);TestEqual(TEXT("Pipeline maximum duration preserved"),A.Y-A.X,120.);
 A=FCBMClipRange::Normalize(-10,7,3,15,30);TestEqual(TEXT("No negative start"),A.X,0.);
 A=FCBMClipRange::Normalize(3,99,3,15,30);TestEqual(TEXT("End does not exceed video"),A.Y,15.);
 A=FCBMClipRange::Normalize(3.021,10.021,0,15,24);TestTrue(TEXT("24fps selection grid"),FMath::IsNearlyEqual(A.X*24.,FMath::RoundToDouble(A.X*24.),1.e-9));
 A=FCBMClipRange::Normalize(0,1./24.,0,15,24);TestEqual(TEXT("24fps minimum uses three whole frames"),A.Y,3./24.);
 A=FCBMClipRange::Normalize(15,10,3,15,24);TestEqual(TEXT("Crossing start remains on frame grid"),A.X,10.-3./24.);
 A=FCBMClipRange::Normalize(3,99,3,15.02,24);TestEqual(TEXT("Partial trailing frame excluded"),A.Y,15.);
 A=FCBMClipRange::Normalize(0,1,0,.05,30);TestEqual(TEXT("Too short source rejected"),A,FVector2D::ZeroVector);
 return true;
}
#endif
