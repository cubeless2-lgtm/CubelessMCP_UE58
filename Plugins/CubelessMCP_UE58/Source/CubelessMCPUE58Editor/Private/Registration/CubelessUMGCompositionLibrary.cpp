#include "Registration/CubelessUMGCompositionLibrary.h"
#include "Operations/CubelessUMGCompositionOperations.h"
#include "Compatibility/UE58/CubelessUMGCompositionApi.h"
FString UCubelessUMGCompositionLibrary::Compose(const FString& P,const FString& J) { return Cubeless::Operations::UMGComposition::Compose(P,J); }
FString UCubelessUMGCompositionLibrary::Animate(const FString& P,const FString& N,double D,const FString& J) { return Cubeless::Operations::UMGComposition::Animate(P,N,D,J); }
FString UCubelessUMGCompositionLibrary::Inspect(const FString& P) { return Cubeless::Operations::UMGComposition::Inspect(P); }
FString UCubelessUMGCompositionLibrary::Finalize(const FString& P,bool S) { return Cubeless::Operations::UMGComposition::Finalize(P,S); }
UWidget* UCubelessUMGCompositionLibrary::SourceWidget(const FString& P,const FString& N) { return Cubeless::UE58::UMGComposition::SourceWidget(P,N); }
