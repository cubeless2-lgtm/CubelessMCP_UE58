#include "CBMFootSupport.h"
#include "Misc/AutomationTest.h"
#include <limits>
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBMFootSupportTest,"Cubeless.BodyMocap.Contact.SupportAnchor",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCBMFootSupportTest::RunTest(const FString&){
 using namespace CBMFootSupport;FInput I;I.Parent={-1,0,1,2,3};I.Thigh[0]=1;I.Calf[0]=2;I.Foot[0]=3;I.Ball[0]=4;
 TArray<FTransform> Old;Old.Init(FTransform::Identity,5);Old[1].SetTranslation(FVector(0,0,100));Old[2].SetTranslation(FVector(0,20,60));Old[3].SetTranslation(FVector(0,0,20));Old[4].SetTranslation(FVector(0,15,12));
 TArray<FTransform> New;const FVector Target(0,1,18);TestTrue(TEXT("Reachable ankle uses original knee plane"),LegIK(I,Old,0,Target,FQuat::Identity,New));
 TestTrue(TEXT("Hip fixed"),New[1].GetTranslation().Equals(Old[1].GetTranslation(),1.e-8));TestTrue(TEXT("Ankle reaches target"),New[3].GetTranslation().Equals(Target,1.e-8));
 TestTrue(TEXT("Thigh length preserved"),FMath::Abs((New[2].GetTranslation()-New[1].GetTranslation()).Size()-(Old[2].GetTranslation()-Old[1].GetTranslation()).Size())<1.e-8);
 TestTrue(TEXT("Calf length preserved"),FMath::Abs((New[3].GetTranslation()-New[2].GetTranslation()).Size()-(Old[3].GetTranslation()-Old[2].GetTranslation()).Size())<1.e-8);
 TestFalse(TEXT("No stretching to unreachable foot"),LegIK(I,Old,0,FVector(0,0,-100),FQuat::Identity,New));
 TestTrue(TEXT("Equal stability passes"),NoWorse({1.,2.},{1.,2.}));TestFalse(TEXT("Slip regression rejected"),NoWorse({1.,2.1},{1.,2.}));
 const double NaN=std::numeric_limits<double>::quiet_NaN();
 TestFalse(TEXT("Invalid baseline cannot pass stability gate"),NoWorse({1.},{NaN}));
 TestFalse(TEXT("Invalid candidate rejected"),NoWorse({NaN},{1.}));
 TestFalse(TEXT("Invalid tolerance rejected"),NoWorse({1.},{1.},NaN));
 TestFalse(TEXT("Nonfinite ankle rejected"),LegIK(I,Old,0,FVector(NaN,0,0),FQuat::Identity,New));
 const FString Run=TEXT("36B51CBE4427D68AB06833B30CCC94BB");
 TestTrue(TEXT("Derived source retains matching run"),MatchesEvidenceSource(TEXT("Body_")+Run+TEXT("_HandFlow_Soles"),Run));
 TestFalse(TEXT("Same-length foreign run rejected"),MatchesEvidenceSource(TEXT("Body_")+Run+TEXT("_HandFlow"),TEXT("5E96DD54482F5AF29DDDD3A583E5C598")));
 TestFalse(TEXT("Missing provenance rejected"),MatchesEvidenceSource(TEXT("RenamedAnimation"),Run));
 TestFalse(TEXT("Prefix collision rejected"),MatchesEvidenceSource(TEXT("Body_")+Run+TEXT("EXTRA"),Run));
 return true;
}
#endif
