#include "CubelessNiagaraPreviewLabLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraPreviewLabApi.h"
FString UCubelessNiagaraPreviewLabLibrary::GetNiagaraPreviewLabState(const FString& P){return Cubeless::UE58::NiagaraPreviewLab::GetState(P);}
FString UCubelessNiagaraPreviewLabLibrary::CleanupNiagaraPreviewLab(const FString& P){return Cubeless::UE58::NiagaraPreviewLab::Cleanup(P);}
FString UCubelessNiagaraPreviewLabLibrary::CaptureNiagaraPreviewLabView(const FString& P){return Cubeless::UE58::NiagaraPreviewLab::CaptureView(P);}
FString UCubelessNiagaraPreviewLabLibrary::PreviewNiagaraSystemInPreviewLab(const FString& P){return Cubeless::UE58::NiagaraPreviewLab::PreviewSystem(P);}
