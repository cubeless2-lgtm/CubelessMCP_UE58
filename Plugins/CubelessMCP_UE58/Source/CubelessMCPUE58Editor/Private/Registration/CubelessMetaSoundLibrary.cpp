#include "Registration/CubelessMetaSoundLibrary.h"
#include "Compatibility/UE58/CubelessMetaSoundApi.h"
FString UCubelessMetaSoundLibrary::CreateBasicMetaSoundSource(const FString& Name, const FString& PackagePath, const FString& Oscillator, double Frequency, const FString& OutputFormat)
{ return Cubeless::UE58::MetaSound::CreateBasicSource(Name, PackagePath, Oscillator, Frequency, OutputFormat); }
