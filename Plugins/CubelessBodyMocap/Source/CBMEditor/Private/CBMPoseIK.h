#pragma once
#include "CubelessMocapSettings.h"

// Editor-only pose layer. Outputs are ordinary animation keys, without a rig dependency.
struct FCBMIKHandle { FName Name; FVector Position; FQuat Rotation; bool Finger=false; };
struct FCBMIKPose { TArray<FTransform> Local,Global; TArray<FCBMIKHandle> Handles; bool Changed=false; };
bool CBMEvaluateIK(UCubelessMocapSettings* Settings,double Seconds,FCBMIKPose& Pose);
double CBMIKWeight(const FCBMIKEdit& Edit,double Seconds,double Duration);
void CBMIKBegin(UCubelessMocapSettings* Settings);
void CBMIKEnd(UCubelessMocapSettings* Settings);
bool CBMIKUndo(UCubelessMocapSettings* Settings,bool Redo,FString& Result);

bool CBMIKBusy();

bool CBMIKActive(UCubelessMocapSettings* Settings);
void CBMIKHistoryCounts(UCubelessMocapSettings* Settings,int& Undo,int& Redo,bool& Gesture);
void CBMIKShutdown();
