#include "Compatibility/UE58/DomainExpansion/CubelessDomainCommon.h"

namespace Cubeless::UE58::DomainExpansion
{
FObject Execute(const FObject& P)
{
	if(P->HasField(TEXT("dry_run")) && !P->HasTypedField<EJson::Boolean>(TEXT("dry_run"))) { return Error(TEXT("dry_run must be a JSON boolean")); }
	const FString Op=String(P,TEXT("operation"));
#if !UE_VERSION_OLDER_THAN(5,8,0)
	if(Op.StartsWith(TEXT("authoring.physics."))) { return Physics(P); }
	if(Op.StartsWith(TEXT("authoring.cloth."))) { return Cloth(P); }
	if(Op.StartsWith(TEXT("authoring.pcg."))) { return PCG(P); }
	if(Op.StartsWith(TEXT("authoring.niagara."))) { return Niagara(P); }
	if(Op.StartsWith(TEXT("authoring.animbp."))) { return Animation(P); }
#endif
	return Error(TEXT("Unsupported authoring operation"));
}
}
