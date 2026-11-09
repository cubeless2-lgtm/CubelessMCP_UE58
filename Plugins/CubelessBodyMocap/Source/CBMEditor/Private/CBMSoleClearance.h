#pragma once
#include "CoreMinimal.h"

namespace CBMSole {
// Reduce excess clearance inside the already-safe trajectory. The constraints
// refer to temporal differences, not anatomical joint angles. Both time orders
// are averaged so the offline solve cannot introduce a directional lag.
inline TArray<double> ReduceHover(const TArray<double>& Need, const TArray<double>& Upper, double FPS) {
    const int N=Need.Num();if(N<4)return Upper;
    const double Coeff[3][4]={{-1,1,0,0},{1,-2,1,0},{-1,3,-3,1}};
    double Bound[3]={0,0,0};
    for(int D=1;D<=3;++D)for(int K=0;K<N-D;++K){double V=0;for(int J=0;J<=D;++J)V+=Coeff[D-1][J]*Upper[K+J];Bound[D-1]=FMath::Max(Bound[D-1],FMath::Abs(V));}
    auto Solve=[&](bool Reverse) {
        TArray<double> X=Upper;const double Lambda=FMath::Pow(FPS*.05,4.);
        for(int Iter=0;Iter<160;++Iter){double Change=0;
            for(int Pass=0;Pass<2;++Pass)for(int Index=0;Index<N;++Index){
                const int I=((Pass==0)!=Reverse)?Index:N-1-Index;
                double Lo=Need[I]+.01,Hi=Upper[I],AA=1,BB=-Need[I];
                for(int K=FMath::Max(0,I-2);K<=FMath::Min(I,N-3);++K){
                    const double A=Coeff[1][I-K];double B=0;for(int J=0;J<3;++J)B+=Coeff[1][J]*X[K+J];B-=A*X[I];AA+=Lambda*A*A;BB+=Lambda*A*B;
                }
                for(int D=1;D<=3;++D)for(int K=FMath::Max(0,I-D);K<=FMath::Min(I,N-D-1);++K){
                    const double A=Coeff[D-1][I-K];double B=0;for(int J=0;J<=D;++J)B+=Coeff[D-1][J]*X[K+J];B-=A*X[I];
                    const double L=(-Bound[D-1]-B)/A,H=(Bound[D-1]-B)/A;Lo=FMath::Max(Lo,FMath::Min(L,H));Hi=FMath::Min(Hi,FMath::Max(L,H));
                }
                if(Lo<=Hi){const double V=FMath::Clamp(-BB/AA,Lo,Hi);Change=FMath::Max(Change,FMath::Abs(V-X[I]));X[I]=V;}
            }
            if(Change<1.e-6)break;
        }
        return X;
    };
    auto A=Solve(false);const auto B=Solve(true);for(int I=0;I<N;++I)A[I]=(A[I]+B[I])*.5;
    // Fail closed to the original clearance if floating-point error violates a
    // geometric or temporal bound. No partially accepted trajectory is emitted.
    for(int I=0;I<N;++I)if(!FMath::IsFinite(A[I])||A[I]<Need[I]+.01-1.e-8||A[I]>Upper[I]+1.e-8)return Upper;
    for(int D=1;D<=3;++D)for(int K=0;K<N-D;++K){double V=0;for(int J=0;J<=D;++J)V+=Coeff[D-1][J]*A[K+J];if(FMath::Abs(V)>Bound[D-1]+1.e-8)return Upper;}
    return A;
}
// Offline, symmetric envelope: anticipates/releases penetration correction without
// delaying the animation. Units cm/seconds. It changes vertical root position only.
inline TArray<double> Lift(const TArray<double>& MinimumZ, double FPS) {
    if (FPS <= 0 || MinimumZ.IsEmpty()) return {};
    TArray<double> Need, Envelope;
    double Maximum=0;
    for (double Z:MinimumZ) {
        if (!FMath::IsFinite(Z)) return {};
        const double V=FMath::Max(0.,-Z);Need.Add(V);Maximum=FMath::Max(Maximum,V);
    }
    // Large coordinate/floor errors need calibration, not an automatic body lift.
    if (Maximum>5.) return {};
    if (Maximum<=.01) {Need.Init(0.,MinimumZ.Num());return Need;}
    Envelope=Need;const double Step=10./FPS;
    for (int I=1;I<Envelope.Num();++I) Envelope[I]=FMath::Max(Envelope[I],Envelope[I-1]-Step);
    for (int I=Envelope.Num()-2;I>=0;--I) Envelope[I]=FMath::Max(Envelope[I],Envelope[I+1]-Step);
    const double Sigma=FMath::Max(.5,FPS*.05);const int Radius=FMath::CeilToInt(4*Sigma);
    TArray<double> Result;Result.SetNum(Need.Num());double Deficit=0;
    for (int I=0;I<Need.Num();++I) {
        double Sum=0,Weight=0;
        for(int J=-Radius;J<=Radius;++J) {const double W=FMath::Exp(-.5*J*J/(Sigma*Sigma));Sum+=Envelope[FMath::Clamp(I+J,0,Need.Num()-1)]*W;Weight+=W;}
        Result[I]=Sum/Weight;Deficit=FMath::Max(Deficit,Need[I]-Result[I]);
    }
    // Constant deficit repair preserves the smoothed derivatives and the lower bound.
    for(double& V:Result)V+=Deficit+.01;
    return ReduceHover(Need,Result,FPS);
}
}
