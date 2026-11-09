#pragma once
#include "CoreMinimal.h"

// Measures added alignment roll, not an anatomical joint angle or a joint limit.
// The axes are world-space calf-to-foot vectors from the same target skeleton.
namespace CBMLegTwist {
inline bool MeasureAddedTwist(const FQuat& SourceWorldRotation,const FVector& SourceBoneAxis,
 const FQuat& CandidateWorldRotation,const FVector& CandidateBoneAxis,double& OutRadians)
{
 OutRadians=0.;
 auto FiniteVector=[](const FVector& V){return FMath::IsFinite(V.X)&&FMath::IsFinite(V.Y)&&FMath::IsFinite(V.Z);};
 auto FiniteQuat=[](const FQuat& Q){return FMath::IsFinite(Q.X)&&FMath::IsFinite(Q.Y)&&FMath::IsFinite(Q.Z)&&FMath::IsFinite(Q.W);};
 if(!FiniteVector(SourceBoneAxis)||!FiniteVector(CandidateBoneAxis)||!FiniteQuat(SourceWorldRotation)||!FiniteQuat(CandidateWorldRotation))return false;
 const double AL=SourceBoneAxis.SizeSquared(),BL=CandidateBoneAxis.SizeSquared();
 const double SQ=SourceWorldRotation.SizeSquared(),CQ=CandidateWorldRotation.SizeSquared();
 if(!FMath::IsFinite(AL)||!FMath::IsFinite(BL)||!FMath::IsFinite(SQ)||!FMath::IsFinite(CQ)||AL<=1.e-12||BL<=1.e-12||SQ<=1.e-12||CQ<=1.e-12)return false;
 const FVector A=SourceBoneAxis/FMath::Sqrt(AL),B=CandidateBoneAxis/FMath::Sqrt(BL);
 const FQuat Source=SourceWorldRotation*(1./FMath::Sqrt(SQ)),Candidate=CandidateWorldRotation*(1./FMath::Sqrt(CQ));
 const double Dot=FMath::Clamp(FVector::DotProduct(A,B),-1.,1.);
 // Near a half-turn the shortest transport axis is ambiguous. Keep the existing
 // pose rather than choose an arbitrary roll axis or manufacture a discontinuity.
 if(Dot<=-1.+1.e-6)return false;
 // A mismatched bone axis/basis is not a measurable correction on the same bone.
 if(FVector::DotProduct(Source.UnrotateVector(A),Candidate.UnrotateVector(B))<1.-1.e-6)return false;
 const FVector Cross=FVector::CrossProduct(A,B);
 const FQuat Swing=FQuat(Cross.X,Cross.Y,Cross.Z,1.+Dot).GetNormalized();
 const FQuat Transported=(Swing*Source).GetNormalized();
 FQuat Residual=(Candidate*Transported.Inverse()).GetNormalized();
 // Quaternion double-cover must not change the signed result.
 if(Residual.W<0.)Residual=Residual*-1.;
 const double Axial=FVector::DotProduct(FVector(Residual.X,Residual.Y,Residual.Z),B);
 const double Norm=Axial*Axial+Residual.W*Residual.W;
 // Animation tracks and UE_PI may have float precision before conversion to
 // double. A nominal half-turn can therefore retain W around 4.4e-8.
 if(!FMath::IsFinite(Norm)||Norm<=1.e-12||FMath::Abs(Residual.W)<1.e-6)return false;
 const double Angle=2.*FMath::Atan2(Axial,Residual.W);
 if(!FMath::IsFinite(Angle))return false;
 OutRadians=Angle;
 return true;
}
}
