#pragma once
#include "CoreMinimal.h"

class UCubelessNiagaraPreviewWorkflowJob;

namespace Cubeless::UE58::NiagaraPreviewWorkflows
{
    FString OpenPlayer(const FString& ParamsJson);
    FString GetPlayerState(const FString& ParamsJson);
    FString PreviewComponentSwap(const FString& ParamsJson);
    FString SampleLab(const FString& ParamsJson);

    // Domain-only GT/ticker jobs. Snapshot/cancel live on the transient Job UObject.
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraPreviewWorkflowJob* StartOpenPlayer(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraPreviewWorkflowJob* StartComponentSwap(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraPreviewWorkflowJob* StartSampleLab(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraPreviewWorkflowJob* StartPreviewLab(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API bool Cancel(UCubelessNiagaraPreviewWorkflowJob* Job, const FString& Reason);
    // Root integrates this before registration/converter teardown. No forced save/flush/undo.
    CUBELESSMCPUE58EDITOR_API void Shutdown(const FString& Reason);
}
