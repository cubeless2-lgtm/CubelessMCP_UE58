#include "CBMSoleClearance.h"
#include "Misc/AutomationTest.h"
#include <limits>
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBMSoleClearanceTest,"Cubeless.BodyMocap.Contact.SoleClearance",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCBMSoleClearanceTest::RunTest(const FString&) {
 TArray<double> Air;Air.Init(12.,100);auto A=CBMSole::Lift(Air,30.);
 TestEqual(TEXT("Airborne clip is not pulled to ground"),A.Num(),100);for(double V:A)TestEqual(TEXT("Airborne correction is zero"),V,0.);
 TArray<double> Z;Z.Init(0.,100);Z[50]=-3.;auto L=CBMSole::Lift(Z,30.);
 TestEqual(TEXT("Frame count preserved"),L.Num(),Z.Num());
 for(int I=0;I<L.Num();++I){TestTrue(TEXT("Surface stays above floor"),Z[I]+L[I]>=0.);if(I)TestTrue(TEXT("Lift has bounded speed"),FMath::Abs(L[I]-L[I-1])*30.<=10.0001);}
 for(int I=1;I<49;++I)TestTrue(TEXT("No temporal delay"),FMath::Abs(L[50-I]-L[50+I])<1.e-9);
 TArray<double> Constant;Constant.Init(-2.,20);auto C=CBMSole::Lift(Constant,60.);for(double V:C)TestTrue(TEXT("Static stance has static clearance"),FMath::Abs(V-2.01)<1.e-9);
 Constant[2]=-9.;TestTrue(TEXT("Large floor errors are rejected"),CBMSole::Lift(Constant,30.).IsEmpty());
 Constant[2]=std::numeric_limits<double>::quiet_NaN();TestTrue(TEXT("Nonfinite geometry rejected"),CBMSole::Lift(Constant,30.).IsEmpty());
 TestTrue(TEXT("Invalid frame rate rejected"),CBMSole::Lift(Air,0.).IsEmpty());
 TArray<double> Need,Upper;Need.Init(0.,120);Upper.Init(.4,120);
 for(int I=40;I<80;++I){Need[I]=.3;Upper[I]=.7;}
 const auto Reduced=CBMSole::ReduceHover(Need,Upper,30.);double Saved=0;
 for(int I=0;I<Need.Num();++I){TestTrue(TEXT("Local clearance stays feasible"),Reduced[I]>=Need[I]+.01-1.e-8&&Reduced[I]<=Upper[I]+1.e-8);Saved+=Upper[I]-Reduced[I];}
 TestTrue(TEXT("Excess global height is removed"),Saved>1.);
 const double Coeff[3][4]={{-1,1,0,0},{1,-2,1,0},{-1,3,-3,1}};
 for(int D=1;D<=3;++D){double Before=0,After=0;for(int K=0;K<Upper.Num()-D;++K){double V=0,W=0;for(int J=0;J<=D;++J){V+=Coeff[D-1][J]*Upper[K+J];W+=Coeff[D-1][J]*Reduced[K+J];}Before=FMath::Max(Before,FMath::Abs(V));After=FMath::Max(After,FMath::Abs(W));}TestTrue(TEXT("Speed acceleration and jerk cannot increase"),After<=Before+1.e-8);}
 return true;
}
#endif
