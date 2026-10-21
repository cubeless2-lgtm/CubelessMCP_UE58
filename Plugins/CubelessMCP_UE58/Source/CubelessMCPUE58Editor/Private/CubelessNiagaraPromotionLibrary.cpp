#include "CubelessNiagaraPromotionLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraPromotionApi.h"

FString UCubelessNiagaraPromotionLibrary::AuditNiagaraPromotionCandidate(const FString& ParamsJson){return Cubeless::UE58::NiagaraPromotion::AuditCandidate(ParamsJson);}
FString UCubelessNiagaraPromotionLibrary::PromoteNiagaraSystemCandidate(const FString& ParamsJson){return Cubeless::UE58::NiagaraPromotion::PromoteCandidate(ParamsJson);}
