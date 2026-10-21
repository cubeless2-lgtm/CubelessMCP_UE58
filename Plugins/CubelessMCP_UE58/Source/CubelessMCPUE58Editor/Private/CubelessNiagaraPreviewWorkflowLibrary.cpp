#include "CubelessNiagaraPreviewWorkflowLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraPreviewWorkflowApi.h"
#include "CubelessNiagaraPreviewWorkflowJob.h"
using namespace Cubeless::UE58;
FString UCubelessNiagaraPreviewWorkflowLibrary::OpenNiagaraPreviewPlayer(const FString& S){return NiagaraPreviewWorkflows::OpenPlayer(S);}
FString UCubelessNiagaraPreviewWorkflowLibrary::GetNiagaraPreviewPlayerState(const FString& S){return NiagaraPreviewWorkflows::GetPlayerState(S);}
FString UCubelessNiagaraPreviewWorkflowLibrary::PreviewNiagaraComponentAssetSwap(const FString& S){return NiagaraPreviewWorkflows::PreviewComponentSwap(S);}
FString UCubelessNiagaraPreviewWorkflowLibrary::SampleNiagaraSystemInPreviewLab(const FString& S){return NiagaraPreviewWorkflows::SampleLab(S);}
UCubelessNiagaraPreviewWorkflowJob* UCubelessNiagaraPreviewWorkflowLibrary::StartOpenPlayerJob(const FString& S){return NiagaraPreviewWorkflows::StartOpenPlayer(S);}
UCubelessNiagaraPreviewWorkflowJob* UCubelessNiagaraPreviewWorkflowLibrary::StartPreviewComponentSwapJob(const FString& S){return NiagaraPreviewWorkflows::StartComponentSwap(S);}
UCubelessNiagaraPreviewWorkflowJob* UCubelessNiagaraPreviewWorkflowLibrary::StartSampleLabJob(const FString& S){return NiagaraPreviewWorkflows::StartSampleLab(S);}
UCubelessNiagaraPreviewWorkflowJob* UCubelessNiagaraPreviewWorkflowLibrary::StartPreviewLabJob(const FString& S){return NiagaraPreviewWorkflows::StartPreviewLab(S);}
