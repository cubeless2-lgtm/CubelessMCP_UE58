#pragma once
#include "CoreMinimal.h"
#include "Physics/Experimental/ChaosEventType.h"
#include "CubelessChaosPerformanceBreakListener.generated.h"

/** Editor module only. The listener is created in TransientPackage with RF_Transient. */
UCLASS(Transient, NotBlueprintable)
class UCubelessChaosPerformanceBreakListener final : public UObject
{
    GENERATED_BODY()
public:
    TFunction<void(const FChaosBreakEvent&)> Callback;
    bool bAcceptEvents = true;
    UFUNCTION()
    void HandleBreak(const FChaosBreakEvent& Event);
};
